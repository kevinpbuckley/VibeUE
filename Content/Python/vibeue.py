# Copyright Buckley Builds LLC 2026 All Rights Reserved.
"""Agent-ergonomics helpers for Unreal's ToolsetRegistry (issues #545, #547, #548).

Usage from execute_python_code:

    import unreal, vibeue

    schema = vibeue.get_toolset_schema("EditorToolset.EditorAppToolset")   # by NAME (issue #547)
    out = vibeue.exec_tool("EditorToolset.EditorAppToolset", "StartPIE")   # bare call just works
    print(out)                                                             # fully decoded (issue #548)

Why this exists:
- unreal.ToolsetRegistry.get_toolset_json_schema() takes a ToolsetDefinition CLASS, not the
  namespaced name string every other API uses (issue #547) — get_toolset_schema() bridges that.
- The engine's arg validation aborts on the FIRST optional param that lacks a schema default
  ("input param X needs a default value"), one param per attempt (issue #545) — exec_tool()
  pre-fills every missing optional param from the schema (or a type-appropriate zero) and reports
  ALL missing required params in one error.
- execute_tool results are inconsistently double-encoded ("returnValue" is sometimes a JSON string,
  issue #548) — exec_tool() decodes until stable and returns real Python values.

Also provides (agent helpers for PIE, async tools, and asset GC):
- pie_worlds() / role() — the live PIE worlds keyed by net role (server, clients, all, local),
  skipping the ~100 stale /Memory/UEDPIE_* shells that a name match would grab.
- exec_tool_async() / collect_tool_result() — fire a genuinely-async engine tool now (the editor
  does not tick mid-script, so it never completes in one call), collect its decoded value next call.
  exec_tool_collect() fires and collects in ONE call: the value if the tool completed synchronously,
  else the pending key to collect_tool_result() later.
- python_globals_holding() / release_globals() — find and drop the execute_python_code script
  globals that root an asset (a GCObjectReferencer root), so delete_asset_unattended can proceed.
"""

import inspect
import json
import os
import sys

import unreal

_ZERO_BY_TYPE = {
    "string": "",
    "number": 0,
    "integer": 0,
    "boolean": False,
    "object": {},
    "array": [],
}


def _parse_schema_entry(raw):
    if isinstance(raw, str):
        try:
            raw = json.loads(raw)
        except ValueError:
            return None
    if isinstance(raw, dict):
        return raw
    return None


def _all_schema_entries():
    """Toolset schema dicts from get_all_toolset_json_schemas(), which returns ONE JSON string
    holding a list of {name, version, description, tools} entries (not a list of strings)."""
    raw = unreal.ToolsetRegistry.get_all_toolset_json_schemas()
    if isinstance(raw, (str, bytes)) or not hasattr(raw, "__iter__"):
        try:
            doc = json.loads(str(raw))
        except ValueError:
            return []
        items = doc if isinstance(doc, list) else [doc]
    else:
        items = list(raw)
    return [e for e in (_parse_schema_entry(item) for item in items) if e]


def list_toolset_names():
    """Names of every registered toolset, parsed from get_all_toolset_json_schemas()."""
    names = []
    for parsed in _all_schema_entries():
        name = parsed.get("name") or parsed.get("toolsetName") or parsed.get("toolset_name")
        if name:
            names.append(name)
    return names


def get_toolset_schema(toolset_name):
    """Full JSON schema (as a dict) for one toolset, looked up by its namespaced NAME string.

    Accepts 'EditorToolset.EditorAppToolset' style names (the same string execute_tool takes);
    falls back to a case-insensitive suffix match so 'EditorAppToolset' also resolves.
    """
    fallback = None
    wanted = toolset_name.lower()
    for parsed in _all_schema_entries():
        name = parsed.get("name") or parsed.get("toolsetName") or parsed.get("toolset_name") or ""
        if name.lower() == wanted:
            return parsed
        if name.lower().endswith(wanted) and fallback is None:
            fallback = parsed
    if fallback is not None:
        return fallback
    raise KeyError(
        "Toolset '{}' not found. Registered toolsets: {}".format(
            toolset_name, ", ".join(sorted(list_toolset_names()))))


def _find_tool_schema(toolset_schema, tool_name):
    tools = None
    for key in ("tools", "functions", "toolSchemas"):
        candidate = toolset_schema.get(key)
        if isinstance(candidate, list):
            tools = candidate
            break
    if tools is None:
        return None
    wanted = tool_name.lower()
    for entry in tools:
        entry = _parse_schema_entry(entry)
        if not entry:
            continue
        name = entry.get("name") or entry.get("toolName") or ""
        # Tool names come fully qualified ("EditorToolset.EditorAppToolset.CaptureViewport");
        # match the bare tool name execute_tool takes as well.
        if name.lower() == wanted or name.rsplit(".", 1)[-1].lower() == wanted:
            return entry
    return None


def _synth_value(prop):
    """Best-effort neutral value for a schema property: default > first enum value > recursive
    object build > typed zero."""
    prop = _parse_schema_entry(prop) or {}
    if "default" in prop:
        return prop["default"]
    enum = prop.get("enum")
    if isinstance(enum, list) and enum:
        return enum[0]
    ptype = prop.get("type", "string")
    if ptype == "object":
        return {name: _synth_value(sub) for name, sub in (prop.get("properties") or {}).items()}
    return _ZERO_BY_TYPE.get(ptype, "")


def _fill_args_from_schema(tool_schema, args):
    """Fill missing params so bare calls work (issue #545): optional params always; required params
    when a neutral value is synthesizable (default, enum, object, bool/number/array — the classes
    the engine toolsets over-declare as required). Required STRINGS with no default stay caller's
    responsibility — but they are reported in ONE error naming all of them, with the full schema,
    instead of the engine's one-param-per-attempt loop."""
    input_schema = (tool_schema.get("inputSchema") or tool_schema.get("input_schema")
                    or tool_schema.get("parameters") or {})
    input_schema = _parse_schema_entry(input_schema) or {}
    properties = input_schema.get("properties") or {}
    required = set(input_schema.get("required") or [])

    missing_required = []
    for name in sorted(required):
        if name in args:
            continue
        prop = _parse_schema_entry(properties.get(name)) or {}
        if "default" in prop or prop.get("enum") or prop.get("type") in (
                "object", "boolean", "number", "integer", "array"):
            args[name] = _synth_value(prop)
        else:
            missing_required.append(name)
    if missing_required:
        raise ValueError(
            "Missing required param(s) {} for tool '{}'. Input schema: {}".format(
                missing_required, tool_schema.get("name", "?"), json.dumps(input_schema)))

    for name, prop in properties.items():
        if name in args:
            continue
        args[name] = _synth_value(prop)
    return args


def _decode_stable(value):
    """json.loads until the value stops being a JSON string (issue #548)."""
    for _ in range(4):
        if not isinstance(value, str):
            return value
        stripped = value.strip()
        if not stripped or stripped[0] not in "[{\"" :
            return value
        try:
            value = json.loads(stripped)
        except ValueError:
            return value
    return value


def exec_tool(toolset_name, tool_name, args=None, unwrap=True):
    """Execute a ToolsetRegistry tool with schema-aware arg filling and normalized decoding.

    Returns the tool's decoded returnValue (or the whole decoded result dict if unwrap=False /
    there is no returnValue key). Raises RuntimeError on tool errors.
    """
    args = dict(args or {})
    try:
        tool_schema = _find_tool_schema(get_toolset_schema(toolset_name), tool_name)
    except KeyError:
        raise
    if tool_schema is not None:
        _fill_args_from_schema(tool_schema, args)

    res = unreal.ToolsetRegistry.execute_tool(toolset_name, tool_name, json.dumps(args))
    if res.error:
        raise RuntimeError("{}::{} failed: {}".format(toolset_name, tool_name, res.error))
    if not res.is_complete:
        # The editor toolsets complete synchronously; a pending result here means a genuinely
        # async tool — hand the raw result back rather than blocking the game thread.
        return res

    out = _decode_stable(res.get_value_as_json_string())
    if unwrap and isinstance(out, dict) and "returnValue" in out:
        return _decode_stable(out["returnValue"])
    return out


# --- PIE worlds ------------------------------------------------------------------------------

def role(actor):
    """The net role of an actor as a string (e.g. 'NetRole.ROLE_AUTHORITY').

    A 2-client PIE run is Standalone by default: BOTH worlds' actors read ROLE_AUTHORITY. Check
    this before claiming a networked (server/client) result. Returns "" if the actor has no
    readable role property.
    """
    try:
        return str(actor.get_editor_property("role"))
    except Exception:
        return ""


def pie_worlds():
    """The live Play-In-Editor worlds, keyed by net role.

    Returns {"server": UWorld|None, "clients": [UWorld, ...], "all": [UWorld, ...],
    "local": UWorld|None}. The real PIE worlds have get_path_name() paths starting '/Game/' with
    'UEDPIE_<N>_' in them: N=0 is the server (dedicated) or the listen host, N>=1 are clients,
    ordered by N. Matching PIE worlds by name grabs one of the ~100 stale /Memory/UEDPIE_* shells
    (0 actors) that linger in a session, so this filters on the /Game/ path instead. "local" is the
    editor's own game world from UnrealEditorSubsystem.get_game_world().
    """
    marker = "UEDPIE_"
    indexed = []
    for world in unreal.ObjectIterator(unreal.World):
        try:
            path = world.get_path_name()
        except Exception:
            continue
        if not path.startswith("/Game/") or marker not in path:
            continue
        rest = path[path.find(marker) + len(marker):]
        digits = ""
        for ch in rest:
            if ch.isdigit():
                digits += ch
            else:
                break
        if not digits:
            continue
        indexed.append((int(digits), world))
    indexed.sort(key=lambda pair: pair[0])

    server = None
    clients = []
    for n, world in indexed:
        if n == 0 and server is None:
            server = world
        elif n >= 1:
            clients.append(world)

    local = None
    try:
        local = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    except Exception:
        local = None

    return {"server": server, "clients": clients, "all": [w for _, w in indexed], "local": local}


# --- Async engine tools ----------------------------------------------------------------------

_PENDING = {}


def exec_tool_async(toolset_name, tool_name, args=None, key=None):
    """Fire a genuinely-async ToolsetRegistry tool and stash its pending result for a later collect.

    Some engine tools (e.g. EditorAppToolset.CaptureAssetImage) never reach is_complete inside one
    execute_python_code call — the editor does not tick mid-script. Fire on one call with this, then
    collect_tool_result(key) on the NEXT call. Args are schema-filled exactly like exec_tool(). The
    raw result object is stored in the module-level _PENDING dict (which persists across calls) under
    `key`, defaulting to "<toolset>.<tool>". Returns the key.
    """
    args = dict(args or {})
    if key is None:
        key = "{}.{}".format(toolset_name, tool_name)
    tool_schema = _find_tool_schema(get_toolset_schema(toolset_name), tool_name)
    if tool_schema is not None:
        _fill_args_from_schema(tool_schema, args)
    _PENDING[key] = unreal.ToolsetRegistry.execute_tool(toolset_name, tool_name, json.dumps(args))
    return key


def collect_tool_result(key, unwrap=True):
    """Decoded value of an exec_tool_async() fire stashed under `key`.

    Returns None while the result is still pending (is_complete False) — poll again next call. Once
    complete, returns the decoded returnValue (or the whole decoded dict if unwrap=False / there is
    no returnValue key), using the same stable-decode/unwrap logic as exec_tool(). Raises
    RuntimeError with the engine error string on failure, and KeyError if nothing was fired under
    `key`. A completed or failed result is dropped from _PENDING; a pending one is kept.
    """
    if key not in _PENDING:
        raise KeyError(
            "No pending async result under key '{}'. Fire exec_tool_async() first.".format(key))
    res = _PENDING[key]
    if res.error:
        del _PENDING[key]
        raise RuntimeError("async tool under '{}' failed: {}".format(key, res.error))
    if not res.is_complete:
        return None
    out = _decode_stable(res.get_value_as_json_string())
    del _PENDING[key]
    if unwrap and isinstance(out, dict) and "returnValue" in out:
        return _decode_stable(out["returnValue"])
    return out


def exec_tool_collect(toolset_name, tool_name, args=None, key=None):
    """Fire an engine tool and try to collect its result in the SAME call.

    A one-call convenience over exec_tool_async()/collect_tool_result(): fires the tool, then
    collects immediately if it already completed (a synchronous editor tool) and returns the decoded
    value; otherwise returns the pending key to pass to collect_tool_result() on a later call (a
    genuinely-async tool cannot finish here — the editor does not tick mid-script). Raises like
    collect_tool_result() on a tool error.
    """
    fired_key = exec_tool_async(toolset_name, tool_name, args=args, key=key)
    res = _PENDING[fired_key]
    if res.error or res.is_complete:
        return collect_tool_result(fired_key, unwrap=True)
    return fired_key


# --- Asset GC roots held by Python globals ---------------------------------------------------
#
# Namespace note: inside execute_python_code, __name__ == "__main__" but globals() is NOT
# sys.modules["__main__"].__dict__ — the script runs in a SEPARATE dict that persists across calls
# (which is exactly why a global there keeps rooting an asset). So `import __main__` never sees it.
# These helpers resolve the CALLER's globals via the call stack AND scan __main__ as a fallback.
#
# Self-test (run the two lines below in ONE execute_python_code call, then a THIRD call):
#   import unreal, vibeue
#   held = unreal.load_asset("/Game/UI/W_Prompt")            # a script global now roots the asset
#   print(vibeue.python_globals_holding("/Game/UI/W_Prompt"))  # -> ['held']
#   # ...next call:
#   print(vibeue.release_globals(vibeue.python_globals_holding("/Game/UI/W_Prompt")))  # -> ['held']

def _script_namespaces():
    """The namespace dicts an execute_python_code global might live in, most-relevant first.

    Returns the CALLER's globals (found by walking the stack back past vibeue's own frames — the
    execute_python_code script namespace, a persistent dict that is not sys.modules['__main__'])
    followed by sys.modules['__main__'].__dict__ as a fallback, deduplicated by dict identity.
    """
    module_ns = globals()
    namespaces = []
    seen = set()

    def _add(namespace):
        if isinstance(namespace, dict) and id(namespace) not in seen:
            seen.add(id(namespace))
            namespaces.append(namespace)

    frame = inspect.currentframe()
    walker = frame.f_back if frame is not None else None
    try:
        while walker is not None:
            if walker.f_globals is not module_ns:
                _add(walker.f_globals)
                break
            walker = walker.f_back
    finally:
        del frame
        del walker
    try:
        _add(sys.modules["__main__"].__dict__)
    except Exception:
        pass
    return namespaces


def python_globals_holding(asset_or_path):
    """Names of execute_python_code script globals that reference the given asset (a GC root).

    Accepts a loaded unreal.Object or an asset path (either the package form '/Game/Path/Name' or
    the object form '/Game/Path/Name.Name'). Scans the caller's script namespace and __main__ (see
    _script_namespaces) and returns the names whose value is that object, or is any unreal.Object
    whose get_path_name() matches the given path — deduplicated across namespaces.
    delete_asset_unattended refuses an asset still held by such a global; feed this list to
    release_globals() to free it.
    """
    wanted_obj = asset_or_path if isinstance(asset_or_path, unreal.Object) else None
    wanted_paths = set()
    if isinstance(asset_or_path, str):
        wanted_paths.add(asset_or_path)
        leaf = asset_or_path.rsplit("/", 1)[-1]
        if "." not in leaf:
            wanted_paths.add("{}.{}".format(asset_or_path, leaf))
    elif wanted_obj is not None:
        try:
            wanted_paths.add(wanted_obj.get_path_name())
        except Exception:
            pass

    hits = []
    seen_names = set()
    for namespace in _script_namespaces():
        for name, value in list(namespace.items()):
            if name.startswith("__") or name in seen_names:
                continue
            if wanted_obj is not None and value is wanted_obj:
                hits.append(name)
                seen_names.add(name)
                continue
            if isinstance(value, unreal.Object):
                try:
                    if value.get_path_name() in wanted_paths:
                        hits.append(name)
                        seen_names.add(name)
                except Exception:
                    pass
    return hits


def release_globals(names, namespace=None):
    """del the named script globals and collect_garbage(), freeing the GC roots they held.

    Pass the names python_globals_holding() returned. Deletes each name from every namespace that
    holds it (the caller's execute_python_code script namespace and __main__; see
    _script_namespaces), so a name defined in both is fully released. Pass an explicit `namespace`
    dict to delete from only that dict instead. Returns the names actually deleted (a name present
    in none is skipped). Run before retrying delete_asset_unattended on an asset it refused as
    rooted.
    """
    namespaces = [namespace] if isinstance(namespace, dict) else _script_namespaces()
    released = []
    for name in names:
        for ns in namespaces:
            if name in ns:
                del ns[name]
                if name not in released:
                    released.append(name)
    unreal.SystemLibrary.collect_garbage()
    return released
