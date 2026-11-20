# Copyright Buckley Builds LLC 2026 All Rights Reserved.
"""Agent-ergonomics helpers for Unreal's ToolsetRegistry (issues #545, #547, #548).

Usage from execute_python:

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
- python_globals_holding() / release_globals() — find and drop the execute_python script
  globals that root an asset (a GCObjectReferencer root), so delete_asset_unattended can proceed.
- safe_load_level(level_path, *, allow_resident_target=False, discard_temp=True) —
  LevelEditorSubsystem.load_level behind a resident-map check: a map loaded as an ASSET (Save-As, a
  Level Instance, load_asset on a map) stays resident, and the next load_level then FATALS the editor
  ("World Memory Leaks"). Raises LevelLoadRefused instead of loading while any map other than the
  open level is resident, or when the target does not exist. Discards a dirty never-saved /Temp/
  open level (LevelEditorUnsavedChangesPolicy.DISCARD), never a dirty real map. Returns load_level's
  bool.
- pie_session() / capture_game() / game_world() — `async with vibeue.pie_session(1280, 720) as s:`
  sets throttling, the PIE window size (in both places it must go), starts PIE, waits for the player
  controller and hides screen messages, and restores all of it on exit; `await
  vibeue.capture_game(path)` writes CaptureGameViewport to a PNG and returns the path.
- wait_for() / wait_until() / wait_for_players() / waits() / cancel_all_waits() — non-blocking
  waits for gate harnesses (a Slate post-tick callback with a wall-clock deadline; never sleep on
  the game thread), and player_ids() for the PIE server's PlayerState ids.
- set_prop_pie() / get_prop() / set_array_prop() — property writes that do not rebuild a live PIE
  actor's components, reads that do not raise, and array writes that really mark the package dirty.
- dirty_snapshot() / dirty_delta() — bracket an audit to see which packages it dirtied.
- viewport_state() / gate_session() — context managers that restore the viewport camera, FOV and
  exposure, and the background-throttling and PIE net-mode settings (gate_session also has
  enter()/exit() for a gate that spans several calls).
- mapping_context_priority() / canvas_slot() / set_canvas_slot() — small API-trap wrappers.
"""

import asyncio
import base64
import contextlib
import inspect
import json
import math
import os
import sys
import time
from typing import Any, Callable, Optional

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


async def exec_tool_await(toolset_name, tool_name, args=None, unwrap=True):
    """Call a ToolsetRegistry tool from a script that uses top-level await, and wait for its result.

    The editor ticks while this waits, so a genuinely-async tool (e.g.
    EditorAppToolset.CaptureAssetImage) finishes here, in one execute_python call:

        import vibeue
        image = await vibeue.exec_tool_await("EditorToolset.EditorAppToolset", "CaptureAssetImage", {...})

    Args are schema-filled and the result decoded exactly like exec_tool(). Raises RuntimeError on a
    tool error.
    """
    args = dict(args or {})
    tool_schema = _find_tool_schema(get_toolset_schema(toolset_name), tool_name)
    if tool_schema is not None:
        _fill_args_from_schema(tool_schema, args)
    res = unreal.ToolsetRegistry.execute_tool(toolset_name, tool_name, json.dumps(args))
    while not res.is_complete and not res.error:
        await asyncio.sleep(0)
    if res.error:
        raise RuntimeError("{}::{} failed: {}".format(toolset_name, tool_name, res.error))
    out = _decode_stable(res.get_value_as_json_string())
    if unwrap and isinstance(out, dict) and "returnValue" in out:
        return _decode_stable(out["returnValue"])
    return out


def exec_tool_async(toolset_name, tool_name, args=None, key=None):
    """Fire a genuinely-async ToolsetRegistry tool and stash its pending result for a later collect.

    Some engine tools (e.g. EditorAppToolset.CaptureAssetImage) never reach is_complete inside a
    script without an await — the editor does not tick mid-script. Fire on one call with this, then
    collect_tool_result(key) on the NEXT call, or await exec_tool_await() in one call instead. Args are schema-filled exactly like exec_tool(). The
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
# Namespace note: inside execute_python, __name__ == "__main__" but globals() is NOT
# sys.modules["__main__"].__dict__ — the script runs in a SEPARATE dict. UE 5.9's execute_python
# gives each call a fresh one, so a script global roots an asset only until that call ends (and
# across an `await` inside it); a global in an imported module, or in __main__, lives on. So
# `import __main__` never sees the script's own globals. These helpers resolve the CALLER's globals
# via the call stack AND scan __main__ as a fallback.
#
# Self-test (one execute_python call):
#   import unreal, vibeue
#   held = unreal.load_asset("/Game/UI/W_Prompt")            # a script global now roots the asset
#   found = vibeue.python_globals_holding("/Game/UI/W_Prompt")   # -> ['held']
#   [found, vibeue.release_globals(found)]                        # -> [['held'], ['held']]

def _script_namespaces():
    """The namespace dicts an execute_python global might live in, most-relevant first.

    Returns the CALLER's globals (found by walking the stack back past vibeue's own frames — the
    execute_python script namespace, a dict that is not sys.modules['__main__'])
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
    """Names of execute_python script globals that reference the given asset (a GC root).

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
    holds it (the caller's execute_python script namespace and __main__; see
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


# --- Guarded level load ------------------------------------------------------------------------
#
# A map that is loaded as an ASSET (EditorLoadingAndSavingUtils.save_map Save-As, placed Level
# Instances, unreal.load_asset on a map path) stays resident, and the engine's stale-world check on
# the NEXT level load is a FATAL, not a warning ("World Memory Leaks", EditorServer.cpp).
# collect_garbage alone leaves such a map; unload_packages releases it once no Python variable holds
# it (python_globals_holding, release_globals). So the safe move is to not load a level while one is
# resident: release it first, or restart the editor. An editor whose host releases such maps before
# every load (unreal.PythonTools.host_releases_resident_maps(), set through the console variable
# VibeUE.HostReleasesResidentMaps) makes that load safe, so the check defers to it. (#613, #685)

class LevelLoadRefused(RuntimeError):
    """safe_load_level() refused to load: a map is resident, or the target does not exist."""


def _map_package_name(path):
    """'/Game/Maps/Foo.Foo' or '/Game/Maps/Foo' -> '/Game/Maps/Foo' (the long package name)."""
    path = str(path).strip()
    leaf = path.rsplit("/", 1)[-1]
    if "." in leaf:
        path = path[: len(path) - len(leaf)] + leaf.split(".", 1)[0]
    return path


def _open_level_package():
    """Package name of the editor world ('/Temp/Untitled_1', '/Game/Maps/Foo'), or '' if none."""
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    except Exception:
        return ""
    if world is None:
        return ""
    try:
        return str(world.get_outermost().get_name())
    except Exception:
        return _map_package_name(world.get_path_name())


def _dirty_map_package_names():
    try:
        return [str(p.get_name()) for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    except Exception:
        return []


def _unsaved_changes_policy(open_pkg, discard_temp):
    """(policy or None, reason) for load_level. None = the engine default (FAIL).

    DISCARD only when the open level is a /Temp/ level (a never-saved scratch world, e.g. a
    turnaround or preview render) AND every dirty map package is under /Temp/ too: discarding is
    then losing nothing anyone saved. A dirty real map keeps the engine's refusal (FAIL).
    """
    policy_enum = getattr(unreal, "LevelEditorUnsavedChangesPolicy", None)   # new in 5.9
    if not discard_temp or policy_enum is None or not open_pkg.startswith("/Temp/"):
        return None, ""
    real_dirty = [p for p in _dirty_map_package_names() if not p.startswith("/Temp/")]
    if real_dirty:
        raise LevelLoadRefused(
            "safe_load_level: the open level {} is a temporary level, but real maps also have unsaved "
            "changes ({}). Save them (unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)) "
            "or load with load_level(path, unreal.LevelEditorUnsavedChangesPolicy.DISCARD) if losing "
            "them is intended.".format(open_pkg, ", ".join(real_dirty)))
    return policy_enum.DISCARD, "discarded the temporary level {}".format(open_pkg)


def safe_load_level(level_path, *, allow_resident_target=False, discard_temp=True):
    """Load a level with LevelEditorSubsystem.load_level, refusing when that would crash the editor.

    Reads unreal.PythonTools.get_resident_maps() (map worlds resident besides the open level: the
    resident_maps reply list minus streaming sublevels of the open level, which a level change unloads
    with it) and raises LevelLoadRefused, loading
    nothing, when:
      - any resident map is NOT the target (that is the fatal "World Memory Leaks" shape); the
        message lists them and how to release them;
      - the TARGET itself is resident, unless allow_resident_target=True (issue #613 shape: loading
        a map that is already resident as an asset);
      - level_path is empty, or the asset registry has no map at that package path;
      - the open level is a /Temp/ level but a real map also has unsaved changes (see below).
    With unreal.PythonTools.host_releases_resident_maps() true, the two resident checks are skipped:
    the host releases those maps before the load, and when it cannot, load_level returns False and the
    editor log says why. Without it, release a resident map with
    unreal.EditorLoadingAndSavingUtils.unload_packages once no Python variable holds it.

    Unsaved changes (issue #680): load_level refuses while the open level has unsaved changes
    ("LoadLevel: There are unsaved level changes in /Temp/... Save them before loading a level, or
    load it with UnsavedChangesPolicy"). With discard_temp=True (default), when the open level is a
    never-saved /Temp/ level (a scratch world from a turnaround, preview or new_level) and no real map
    is dirty, it loads with unreal.LevelEditorUnsavedChangesPolicy.DISCARD (5.9). A dirty REAL map
    is never discarded: the engine still refuses, and the exception names the packages. Save first,
    or call load_level(path, unreal.LevelEditorUnsavedChangesPolicy.SAVE / DISCARD) yourself.
    Otherwise calls load_level and returns its bool.

        import unreal, vibeue
        vibeue.safe_load_level("/Game/Maps/RunMap_01")
    """
    if not isinstance(level_path, str) or not level_path.strip():
        raise LevelLoadRefused("safe_load_level: level_path must be a non-empty package path such as /Game/Maps/Foo")
    target_pkg = _map_package_name(level_path)
    if not target_pkg.startswith("/"):
        raise LevelLoadRefused("safe_load_level: '{}' is not a long package path (expected /Game/...)".format(level_path))

    host_releases = unreal.PythonTools.host_releases_resident_maps()
    resident = [] if host_releases else [str(m) for m in unreal.PythonTools.get_resident_maps()]
    others = [m for m in resident if _map_package_name(m) != target_pkg]
    # each refusal says how to release the map, not only to restart
    release = ("Release a map first: drop the Python variables that hold it (vibeue.python_globals_holding, "
               "vibeue.release_globals), then unreal.EditorLoadingAndSavingUtils.unload_packages("
               "[unreal.find_package(path)]); or restart the editor.")
    if others:
        raise LevelLoadRefused(
            "safe_load_level: refusing to load '{}': {} map(s) other than the open level are resident "
            "({}). Loading any level now FATALS the editor (World Memory Leaks). {}".format(
                target_pkg, len(others), ", ".join(others), release))
    if resident and not allow_resident_target:
        raise LevelLoadRefused(
            "safe_load_level: refusing to load '{}': the target map is itself resident as an asset ({}). "
            "{} Or pass allow_resident_target=True if you know this load is safe.".format(
                target_pkg, ", ".join(resident), release))

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    # A bool UFUNCTION with an out-param comes back as the list on success and None on failure;
    # unwrap a tuple too, in case a binding ever returns (bool, list).
    found = registry.get_assets_by_package_name(target_pkg)
    if isinstance(found, tuple):
        found = next((x for x in reversed(found) if not isinstance(x, bool)), None)
    is_map = False
    for asset in found or []:
        try:
            if str(asset.asset_class_path.asset_name) == "World":
                is_map = True
                break
        except Exception:
            pass
    if not is_map:
        raise LevelLoadRefused(
            "safe_load_level: no map at '{}' in the asset registry; nothing was loaded".format(target_pkg))

    policy, note = _unsaved_changes_policy(_open_level_package(), discard_temp)
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if policy is None:
        return subsystem.load_level(level_path)
    unreal.log("vibeue.safe_load_level: {}".format(note))
    return subsystem.load_level(level_path, policy)


# --- Non-blocking waits (gate harnesses) -----------------------------------------------------
#
# execute_python runs on the GAME THREAD. time.sleep() there freezes the editor, the PIE
# server and every PIE client together (a 20 s sleep logged "Very long time between ticks" on all
# three net drivers and corrupted the replication it was waiting for). And the editor does not tick
# while a script runs, so nothing a script waits on can change during that call anyway.
#
# wait_for() instead registers a Slate post-tick callback that re-evaluates a predicate between
# calls and stops itself on success or at a WALL-CLOCK deadline (time.monotonic(); world time
# restarts on every PIE session, so a world-time deadline can deadlock). Every pending wait is kept
# in a registry that survives a reload of this module, so a wait whose Wait object the caller lost
# (overwritten global, reloaded script) can still be found and cancelled: cancel_all_waits().
# A finished wait has already unregistered its handle (the engine removes it at once, even from
# inside the callback's own broadcast).
#
# No Python-shutdown hook is needed: PySlate's ShutdownModule clears every Python Slate tick handle
# when the Python plugin shuts down.

_RECENT_WAITS_KEPT = 32


def _wait_registry():
    """The live {wait_id: Wait} registry and the recent-finished list, created on first use.

    Stored on the unreal module (like init_unreal.py's skill-class cache) so an
    importlib.reload(vibeue) does not orphan handles that are still registered with Slate.
    """
    reg = getattr(unreal, "_vibeue_wait_registry", None)
    if reg is None:
        reg = {"active": {}, "recent": [], "next_id": 1}
        unreal._vibeue_wait_registry = reg
    return reg


def _unregister_handle(handle) -> bool:
    if handle is None:
        return True
    try:
        unreal.unregister_slate_post_tick_callback(handle)
        return True
    except Exception:
        return False


def _slate_running() -> bool:
    """Whether FSlateApplication is initialized, i.e. whether post-tick callbacks can ever run.

    Python has no direct check. unregister_slate_post_tick_callback() rejects a non-handle argument
    with TypeError only when Slate is initialized; without Slate it returns None and does nothing
    (PySlate.cpp UnregisterSlatePostTickCallback). Passing a plain object() is therefore a
    side-effect-free probe.
    """
    try:
        unreal.unregister_slate_post_tick_callback(object())
    except TypeError:
        return True
    except Exception:
        return False
    return False


class Wait(object):
    """A pending wait_for() — poll it in a LATER execute_python call, or chain .then().

    Attributes:
        id          -- registry id (int)
        description -- the caller's label
        done        -- True once it succeeded, timed out, raised, or was cancelled
        ok          -- True only when the predicate returned a truthy value
        result      -- that truthy value (None otherwise)
        timed_out   -- True when the wall-clock deadline passed first
        cancelled   -- True when cancel() / cancel_all_waits() ended it
        error       -- the predicate's exception text, or "" (a raising predicate ends the wait)
        ticks       -- how many times the predicate was evaluated
        registered  -- True only when Slate is running and the callback was added; False when
                       registration raised (then done is True) or Slate is not initialized
                       (headless: the engine hands back an empty handle that never ticks)
        elapsed_s   -- wall-clock seconds since start (frozen once done)

    Example:
        w = vibeue.wait_for(lambda: pawn_is_seated(), timeout_s=8, description="seated")
        # ... next call:
        print(w.done, w.ok, w.elapsed_s)
    """

    def __init__(self, predicate, timeout_s: float, poll_s: float, description: str = ""):
        self.id = 0
        self.description = description
        self.done = False
        self.ok = False
        self.result = None
        self.timed_out = False
        self.cancelled = False
        self.error = ""
        self.ticks = 0
        self.registered = False
        self.callback_errors = []
        self._predicate = predicate
        self._timeout_s = max(0.0, float(timeout_s))
        self._poll_s = max(0.0, float(poll_s))
        self._started = time.monotonic()
        self._finished = None
        self._last_poll = None
        self._handle = None
        self._callbacks = []

    @property
    def elapsed_s(self) -> float:
        end = self._finished if self._finished is not None else time.monotonic()
        return end - self._started

    def then(self, callback: Callable[["Wait"], Any]) -> "Wait":
        """Run callback(wait) on the game thread when this wait finishes (any outcome).

        If it has already finished, the callback runs immediately. Exceptions are caught, logged
        as a warning, and kept in .callback_errors. Returns self so calls chain:
            vibeue.wait_for_players(3).then(lambda w: print("players:", w.ok))
        """
        if self.done:
            self._run_callback(callback)
        else:
            self._callbacks.append(callback)
        return self

    def cancel(self) -> None:
        """End the wait now (ok stays False, cancelled becomes True) and unregister its handle."""
        if not self.done:
            self.cancelled = True
            self._finish()

    def _run_callback(self, callback) -> None:
        try:
            callback(self)
        except Exception as exc:
            self.callback_errors.append(str(exc))
            unreal.log_warning("vibeue.wait_for({!r}) .then callback raised: {}".format(
                self.description, exc))

    def _tick(self, _delta_seconds) -> None:
        if self.done:
            return
        now = time.monotonic()
        if self._last_poll is None or self._poll_s <= 0.0 or now - self._last_poll >= self._poll_s:
            self._last_poll = now
            self.ticks += 1
            try:
                value = self._predicate()
            except Exception as exc:
                self.error = "{}: {}".format(type(exc).__name__, exc)
                self._finish()
                return
            if value:
                self.ok = True
                self.result = value
                self._finish()
                return
        if now - self._started >= self._timeout_s:
            self.timed_out = True
            self._finish()

    def _finish(self) -> None:
        if self._finished is None:
            self._finished = time.monotonic()
        self.done = True
        self._predicate = None   # drop the closure and anything it captured
        _unregister_handle(self._handle)
        self._handle = None
        reg = _wait_registry()
        reg["active"].pop(self.id, None)
        if self not in reg["recent"]:
            reg["recent"].append(self)
            del reg["recent"][:-_RECENT_WAITS_KEPT]
        callbacks, self._callbacks = self._callbacks, []
        for callback in callbacks:
            self._run_callback(callback)

    def __repr__(self) -> str:
        state = ("ok" if self.ok else "cancelled" if self.cancelled else "timed_out" if self.timed_out
                 else "error" if self.error else "done" if self.done else "pending")
        return "<vibeue.Wait #{} {!r} {} after {:.2f}s, {} ticks>".format(
            self.id, self.description, state, self.elapsed_s, self.ticks)


def wait_for(predicate: Callable[[], Any], timeout_s: float = 10.0, poll_s: float = 0.1, *,
             description: str = "") -> Wait:
    """Wait for predicate() to return a truthy value WITHOUT blocking the game thread.

    Registers a Slate post-tick callback that calls predicate() at most every poll_s seconds and
    stops at the first truthy value or when timeout_s of WALL-CLOCK time (time.monotonic) has
    passed. Never time.sleep() on the game thread instead: it freezes every PIE net driver. Returns
    a Wait at once; it unregisters its own handle when it finishes, and cancel_all_waits() ends any
    pending wait whose Wait object you no longer hold. timeout_s must be finite.

    The editor does not tick while execute_python runs, so a wait CANNOT finish inside the call
    that starts it (the first evaluation happens on the next tick). Two usage patterns:

        # 1) start in one call ...
        import vibeue
        w = vibeue.wait_for(lambda: unreal.GameplayStatics.get_player_pawn(world, 0),
                            timeout_s=5, description="pawn")
        # ... and read it back in a LATER call. Script globals do not survive the call in 5.9,
        # so find it again through the registry:
        vibeue.waits()                       # [<vibeue.Wait #1 'pawn' ok after 0.40s, 4 ticks>]
        # Inside ONE call, top-level `await asyncio.sleep(...)` lets the editor tick instead.

        # 2) chain the follow-up so it runs on the game thread when the wait completes:
        vibeue.wait_for(lambda: door.is_open()).then(lambda w: print("door", w.ok, w.elapsed_s))

    PIE objects: at PIE end the Python plugin purges wrapper references to each PIE world and the
    objects inside it, so a pending predicate that touches one raises on its next tick and the wait
    ends with .error set. The purge does NOT reach PIE objects outside the world (LocalPlayer,
    GameInstance, EnhancedInputLocalPlayerSubsystem): a finished wait's .result holding one stays
    in the recent list and keeps it alive. Call cancel_all_waits() before StopPIE; it ends pending
    waits and clears the finished list. A finished wait drops its predicate itself.

    A predicate that raises ends the wait with .error set. Without Slate (a headless process) the
    engine hands back an empty handle that never ticks: .registered is False and the wait only
    ends through cancel()/cancel_all_waits().

    While cancel_all_waits() runs (it fires each cancelled wait's .then callbacks), wait_for()
    returns an already-cancelled Wait, so a chained phase cannot re-arm itself mid-cancel.
    """
    if not math.isfinite(float(timeout_s)):
        raise ValueError("wait_for: timeout_s must be finite, got {!r}".format(timeout_s))
    reg = _wait_registry()
    wait = Wait(predicate, timeout_s, poll_s, description)
    wait.id = reg["next_id"]
    reg["next_id"] += 1
    reg["active"][wait.id] = wait
    if reg.get("cancelling"):
        wait.cancelled = True
        wait._finish()
        return wait
    try:
        wait._handle = unreal.register_slate_post_tick_callback(wait._tick)
        wait.registered = _slate_running()
    except Exception as exc:
        wait.error = "register_slate_post_tick_callback failed: {}".format(exc)
        wait._finish()
    return wait


wait_until = wait_for


def waits() -> list:
    """The active waits followed by the most recently finished ones (up to 32), oldest first.

    Example:
        for w in vibeue.waits():
            print(w)          # <vibeue.Wait #3 'seated' ok after 1.20s, 12 ticks>
    """
    reg = _wait_registry()
    return list(reg["recent"]) + [w for _, w in sorted(reg["active"].items())]


def cancel_all_waits() -> int:
    """Cancel every active wait, unregister every handle, and clear the finished list.

    Returns how many active waits were cancelled. Run it at the end of a gate, or when a callback
    seems to have leaked (output keeps appearing after a wait should have stopped).

    Example:
        print(vibeue.cancel_all_waits())   # -> 0 when nothing was pending
    """
    reg = _wait_registry()
    cancelled = 0
    reg["cancelling"] = True
    try:
        # A fresh snapshot each pass, since a .then callback may touch the registry. wait_for()
        # hands back already-cancelled waits while "cancelling" is set, so this converges; the
        # pass cap is only a backstop.
        for _ in range(100):
            if not reg["active"]:
                break
            for wait in list(reg["active"].values()):
                if not wait.done:
                    cancelled += 1
                wait.cancel()
                reg["active"].pop(wait.id, None)
        for wait in list(reg["active"].values()):
            # Reached only if the cap was hit: unregister each handle instead of dropping it live.
            _unregister_handle(wait._handle)
            wait._handle = None
            wait.done = wait.cancelled = True
        reg["active"].clear()
    finally:
        reg["cancelling"] = False
    del reg["recent"][:]
    return cancelled


def _active_wait_count() -> int:
    """How many waits are registered and not finished (for the automation test)."""
    return len(_wait_registry()["active"])


def _server_pie_world(world=None):
    """The PIE server/host world: the given one, else pie_worlds()['server'], else the editor game
    world when its path is a real /Game/...UEDPIE_0_ world (not a stale /Memory/ shell)."""
    if world is not None:
        return world
    found = pie_worlds().get("server")
    if found is not None:
        return found
    try:
        local = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    except Exception:
        local = None
    if local is not None:
        try:
            path = local.get_path_name()
        except Exception:
            path = ""
        if path.startswith("/Game/") and "UEDPIE_0_" in path:
            return local
    return None


def player_ids(world=None) -> list:
    """Sorted distinct PlayerState player ids of the PlayerControllers in the PIE server world.

    Reads `get_editor_property("player_id")` because APlayerState.get_player_id() is not exposed
    to Python. Returns [] when no PIE server world exists; any other read failure raises (inside
    wait_for_players that ends the wait with .error set).

    Example:
        print(vibeue.player_ids())   # -> [256, 257, 258] in a 3-player listen session
    """
    world = _server_pie_world(world)
    if world is None:
        return []
    ids = set()
    controllers = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerController)
    for pc in controllers or []:
        ps = get_prop(pc, "player_state", strict=True)
        if ps is None:
            continue    # not assigned yet
        pid = get_prop(ps, "player_id", strict=True)
        if pid is not None:
            ids.add(int(pid))
    return sorted(ids)


def wait_for_players(n: int, timeout_s: float = 15.0, world=None) -> Wait:
    """Wait until the PIE server world has at least n distinct PlayerController player ids.

    Built on wait_for(), so it never blocks: start it right after StartPIE (StartPIE is async too)
    and read it in a later call. The world is re-resolved every poll unless one is passed, because
    the PIE worlds do not exist yet in the call that starts PIE. Result is the id list.

    Example:
        vibeue.exec_tool("EditorToolset.EditorAppToolset", "StartPIE")
        w = vibeue.wait_for_players(3, timeout_s=20)
        # ... next call:
        print(w.ok, w.result)        # True [256, 257, 258]
    """
    wanted = int(n)

    def _enough():
        ids = player_ids(world)
        return ids if len(ids) >= wanted else None

    return wait_for(_enough, timeout_s=timeout_s, poll_s=0.25,
                    description="{} PIE players".format(wanted))


# --- Property access in PIE and on assets ----------------------------------------------------

def set_prop_pie(obj, name: str, value) -> bool:
    """set_editor_property with notify_mode NEVER — the safe way to write a LIVE PIE object.

    The default notify mode runs PostEditChange, and on a component of a Blueprint actor that calls
    RerunConstructionScripts() even in PIE (GIsEditor is true): every BP-added component of that
    actor is destroyed and recreated LOCALLY. On a client that leaves fresh empty components the
    server never resends; on the server the clients lose the pawn's components
    ("ReadContentBlockPayload failed ..."). NEVER skips the Pre/PostEditChange notifications, so no
    rebuild, no undo entry and no dirty mark (right for PIE, where nothing should be saved).

    Example:
        arm = pawn.get_component_by_class(unreal.SpringArmComponent)
        vibeue.set_prop_pie(arm, "target_arm_length", 0.0)
    """
    obj.set_editor_property(name, value, unreal.PropertyAccessChangeNotifyMode.NEVER)
    return True


_PROTECTED_READ = "is protected and cannot be read"


def get_prop(obj, name: str, default=None, *, strict: bool = False):
    """get_editor_property that returns `default` for a None obj or a PROTECTED property.

    Only the protected-property error ("Property 'X' for attribute 'x' on 'Y' is protected and
    cannot be read") is swallowed; anything else, such as a misspelled name ("Failed to find
    property ..."), still raises so a typo cannot pass as "no value". With strict=True only the
    None obj returns `default`.

    Example:
        ps = vibeue.get_prop(pc, "player_state")
        pid = vibeue.get_prop(ps, "player_id", -1)
    """
    if obj is None:
        return default
    try:
        return obj.get_editor_property(name)
    except Exception as exc:
        if not strict and _PROTECTED_READ in str(exc):
            return default
        raise


def dirty_snapshot() -> set:
    """Names of every dirty content AND map package right now (a set of '/Game/...' strings).

    Bracket a read-only audit with it so the audit's own dirt can be told apart from the user's
    unsaved work (and pass auto_save=false on those calls):

        before = vibeue.dirty_snapshot()
        # ... the audit ...
        print(vibeue.dirty_delta(before))   # {'dirtied': [], 'cleaned': []}
    """
    names = set()
    utils = unreal.EditorLoadingAndSavingUtils
    for getter in (utils.get_dirty_content_packages, utils.get_dirty_map_packages):
        try:
            packages = getter()
        except Exception:
            continue
        for package in packages or []:
            try:
                names.add(package.get_name())
            except Exception:
                pass
    return names


def dirty_delta(before: set) -> dict:
    """{'dirtied': [...], 'cleaned': [...]} between a dirty_snapshot() and now, sorted.

    Example:
        before = vibeue.dirty_snapshot()
        unreal.BlueprintService.get_timelines("/Game/BP_Player")
        delta = vibeue.dirty_delta(before)
        assert not delta["dirtied"], delta
    """
    now = dirty_snapshot()
    before = set(before or ())
    return {"dirtied": sorted(now - before), "cleaned": sorted(before - now)}


def _package_name(obj):
    try:
        return obj.get_outermost().get_name()
    except Exception:
        return None


def set_array_prop(obj, name: str, values, *, force_dirty: bool = True) -> dict:
    """Replace an array property so the change is REAL: notified, undoable and marked dirty.

    get_editor_property() returns a LIVE reference to an array, so the usual
    `arr = obj.get_editor_property(n); arr.append(x); obj.set_editor_property(n, arr)` edits the
    asset with no notification, then the set compares the array with itself, finds it identical and
    skips PreEditChange/PostEditChange: no Modify(), no dirty mark, no undo entry, and any
    "save if dirty" path silently skips the edit. This helper copies `values` into a fresh list,
    sets it with PropertyAccessChangeNotifyMode.ALWAYS (Pre/PostEditChange run even when the value
    reads identical) and, with force_dirty, calls obj.modify(True), which marks the package dirty
    (UPackage.mark_package_dirty is not exposed to Python in 5.8).

    Build `values` from a COPY taken before touching the live array, never from the live array you
    already mutated (the undo snapshot would then hold the mutated state too). Still save with
    `unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)` and confirm the file
    changed on disk.

    Returns {"applied": bool, "dirty": bool|None, "error": str}; dirty is whether the outermost
    package is in dirty_snapshot() afterwards (None if the package cannot be named).

    Example:
        frags = list(da.get_editor_property("fragments"))      # a COPY
        s = unreal.InstancedStruct()
        s.import_text("/Script/MyGame.WeightFragment(WeightPerUnit=0.02)")
        print(vibeue.set_array_prop(da, "fragments", frags + [s]))
        unreal.EditorAssetLibrary.save_asset(da.get_path_name(), only_if_is_dirty=False)
    """
    out = {"applied": False, "dirty": None, "error": ""}
    try:
        obj.set_editor_property(name, list(values), unreal.PropertyAccessChangeNotifyMode.ALWAYS)
        out["applied"] = True
    except Exception as exc:
        out["error"] = str(exc)
        return out
    if force_dirty:
        try:
            obj.modify(True)
        except Exception as exc:
            out["error"] = "modify failed: {}".format(exc)
    package = _package_name(obj)
    if package:
        out["dirty"] = package in dirty_snapshot()
    return out


# --- Editor-state brackets -------------------------------------------------------------------

@contextlib.contextmanager
def viewport_state():
    """Snapshot the level viewport's camera, FOV and exposure, and put them back on exit.

    ViewportService.set_exposure / set_fov PERSIST into the user's
    Saved/Config/.../EditorPerProjectUserSettings.ini, so a capture script that leaves a fixed
    exposure behind leaves the user a black Lit viewport on every later launch (2026-10-19: a
    script stepped exposure and quit at fixed EV100 12 with FOV 73.8). Wrap every capture that
    changes the camera, FOV or exposure. Restores with ViewportService.restore_viewport_info(info)
    when the plugin has it, else with the engine's EditorAppToolset.SetCameraTransform plus
    ViewportService.set_fov + set_exposure(fixed, ev100), which puts both exposure fields back. Yields the snapshot (a ViewportInfo, or None when
    there is no level viewport, in which case nothing is restored).

    Example:
        with vibeue.viewport_state() as info:
            unreal.ViewportService.set_exposure(True, 11.5)
            vibeue.exec_tool("EditorToolset.EditorAppToolset", "SetCameraTransform",
                             {"translation": {"x": 0, "y": 0, "z": 500},
                              "rotation": {"pitch": -30, "yaw": 0, "roll": 0}})
            # ... capture ...
        # camera, FOV and exposure are back as they were
    """
    info = None
    try:
        info = unreal.ViewportService.get_viewport_info()
        if not get_prop(info, "viewport_type", ""):
            info = None     # no active level viewport: GetViewportInfo returned the defaults
    except Exception:
        info = None
    try:
        yield info
    finally:
        if info is not None:
            _restore_viewport(info)


def _restore_viewport(info) -> None:
    restore = getattr(unreal.ViewportService, "restore_viewport_info", None)
    if restore is not None:
        try:
            restore(info)
            return
        except Exception as exc:
            unreal.log_warning("vibeue.viewport_state: restore_viewport_info failed ({}); "
                               "restoring field by field".format(exc))
    try:
        # ActorService.set_viewport_camera was removed on the 5-9 branch; the engine's
        # EditorAppToolset.SetCameraTransform is AICallable-only, so it goes through exec_tool.
        exec_tool("EditorToolset.EditorAppToolset", "SetCameraTransform", {
            "translation": {"x": info.location.x, "y": info.location.y, "z": info.location.z},
            "rotation": {"pitch": info.rotation.pitch, "yaw": info.rotation.yaw,
                         "roll": info.rotation.roll},
        })
    except Exception as exc:
        unreal.log_warning("vibeue.viewport_state: camera restore failed: {}".format(exc))
    try:
        unreal.ViewportService.set_fov(info.fov)
    except Exception as exc:
        unreal.log_warning("vibeue.viewport_state: FOV restore failed: {}".format(exc))
    try:
        # One call writes both fields. Do NOT substitute set_exposure(False, 1.0) here: that
        # resets the saved FixedEV100 to 1.0 when the user was on Game Settings.
        unreal.ViewportService.set_exposure(bool(info.exposure_fixed), info.exposure_ev100)
    except Exception as exc:
        unreal.log_warning("vibeue.viewport_state: exposure restore failed: {}".format(exc))


def _decode_json_reply(raw) -> dict:
    try:
        value = json.loads(raw) if isinstance(raw, str) else {}
    except ValueError:
        value = {}
    return value if isinstance(value, dict) else {}


class GateSession(object):
    """Editor settings for a gate run, applied by enter() and put back by exit().

    Made by gate_session(); see there. A plain object, not a generator, so a session entered in
    one execute_python call and exited in a later one cannot run its restore early when an
    intermediate reference is garbage-collected. exit() is idempotent.

    enter() is transactional: it reads and validates every setting first, then applies the changes
    one by one. If an apply step raises, every change already made is put back in reverse order and
    the exception propagates; the session stays un-entered, so exit() is then a no-op.

    perf / settings default to unreal.PerformanceService / unreal.EngineSettingsService; tests pass
    fakes with the same methods.
    """

    def __init__(self, clients: Optional[int] = None, net_mode: Optional[str] = None,
                 throttle: bool = False, perf=None, settings=None):
        self.clients = clients
        self.net_mode = net_mode
        self.throttle = bool(throttle)
        self.state = {"throttle_before": None, "pie_before": None, "pie_applied": None,
                      "throttle_applied": None, "notes": []}
        self.entered = False
        self.exited = False
        self._changed_pie = False
        self._changed_throttle = False
        self._perf = perf
        self._settings = settings

    def _services(self):
        perf = self._perf if self._perf is not None else unreal.PerformanceService
        settings = self._settings if self._settings is not None else unreal.EngineSettingsService
        return perf, settings

    def _reset_state(self):
        state = self.state
        for key in ("throttle_before", "pie_before", "pie_applied", "throttle_applied"):
            state[key] = None
        state["notes"] = []
        self._changed_pie = False
        self._changed_throttle = False
        return state

    def _restore_pie(self):
        _, settings = self._services()
        prior = self.state["pie_before"]
        if not settings.set_pie_settings(
                prior["net_mode"], max(1, prior["num_clients"]), prior["run_under_one_process"]):
            unreal.log_warning("vibeue.gate_session: PIE settings restore did not read back")

    def _restore_throttle(self):
        perf, _ = self._services()
        perf.set_background_throttling(self.state["throttle_before"])

    def enter(self) -> dict:
        """Record the current settings and apply the requested ones. Returns self.state."""
        if self.entered:
            return self.state
        perf, settings = self._services()
        state = self._reset_state()

        # Phase 1: read and validate everything. Nothing has changed yet, so a raise here leaves
        # the editor exactly as it was.
        before = _decode_json_reply(perf.get_background_throttling())
        if before.get("success"):
            state["throttle_before"] = bool(before.get("throttling_enabled", True))
        else:
            # Nothing could restore it, so leave it alone.
            state["notes"].append("get_background_throttling failed ({}); throttling left unchanged"
                                  .format(before.get("error", "?")))

        pie = settings.get_pie_settings()
        if get_prop(pie, "success", False):
            state["pie_before"] = {"net_mode": pie.net_mode,
                                   "num_clients": int(pie.num_clients),
                                   "run_under_one_process": bool(pie.run_under_one_process)}
        else:
            state["notes"].append("get_pie_settings failed: {}".format(get_prop(pie, "error_message", "?")))

        pie_target = None
        if self.clients is not None or self.net_mode is not None:
            if state["pie_before"] is None:
                state["notes"].append("PIE settings left unchanged: the current ones could not be read")
            else:
                prior = state["pie_before"]
                mode = self.net_mode if self.net_mode is not None else prior["net_mode"]
                count = int(self.clients) if self.clients is not None else max(1, prior["num_clients"])
                pie_target = (mode, count)

        # Phase 2: apply one change at a time. Each restore is recorded BEFORE its setter runs,
        # because a setter that raises may still have written part of its change; restoring the
        # recorded prior value is harmless when it wrote nothing.
        undo = []
        try:
            if state["throttle_before"] is not None:
                undo.append(self._restore_throttle)
                perf.set_background_throttling(self.throttle)
                self._changed_throttle = True
                state["throttle_applied"] = self.throttle
            if pie_target is not None:
                mode, count = pie_target
                undo.append(self._restore_pie)
                ok = settings.set_pie_settings(mode, count, True)
                self._changed_pie = True
                state["pie_applied"] = {"net_mode": mode, "num_clients": count,
                                        "run_under_one_process": True, "ok": bool(ok)}
        except BaseException:
            for restore in reversed(undo):
                try:
                    restore()
                except Exception as exc:
                    unreal.log_warning("vibeue.gate_session: rollback step failed: {}".format(exc))
            self._changed_pie = False
            self._changed_throttle = False
            state["throttle_applied"] = None
            state["pie_applied"] = None
            state["notes"].append("enter() failed; every applied change was rolled back")
            raise
        self.entered = True
        return state

    def exit(self) -> dict:
        """Put back whatever enter() changed. Safe to call twice. Returns self.state."""
        if self.exited or not self.entered:
            return self.state
        self.exited = True
        state = self.state
        try:
            if self._changed_pie:
                self._restore_pie()
        finally:
            if self._changed_throttle:
                self._restore_throttle()
        return state

    def __enter__(self) -> dict:
        return self.enter()

    def __exit__(self, exc_type, exc, tb) -> bool:
        self.exit()
        return False


def gate_session(*, clients: Optional[int] = None, net_mode: Optional[str] = None,
                 throttle: bool = False) -> GateSession:
    """Set up the editor for a gate run and restore it afterwards.

    On enter: records PerformanceService.get_background_throttling() and
    EngineSettingsService.get_pie_settings(), then calls set_background_throttling(throttle)
    (False = full frame rate while the editor is unfocused; an unfocused editor otherwise drops to
    ~3 FPS and stalls PIE), and, when clients or net_mode is given,
    set_pie_settings(net_mode or current, clients or current, True). A setting whose current value
    could not be read is left unchanged (noted in "notes"), since nothing could restore it. On exit:
    puts back what it changed. set_pie_settings saves to EditorPerProjectUserSettings.ini (it
    survives a restart), so the restore matters. The state dict is {"throttle_before",
    "throttle_applied", "pie_before", "pie_applied", "notes"}.

    Start PIE inside the block, in a later call than the one that entered it if you need the new
    net mode to apply (StartPIE is async). A `with` block must stay inside ONE execute_python
    call; for a gate spanning calls, keep the session in a script global and call enter()/exit():

        gs = vibeue.gate_session(clients=3, net_mode="ListenServer")
        state = gs.enter()                   # call 1
        # ... calls 2..N run the gate ...
        gs.exit()                            # last call: restore (idempotent)

    Example (one call):
        with vibeue.gate_session(clients=2, net_mode="ListenServer") as s:
            print(s["pie_before"], s["pie_applied"])
    """
    return GateSession(clients=clients, net_mode=net_mode, throttle=throttle)


# --- PIE session and game capture (issue #679) -----------------------------------------------
#
# Every PIE check in the 2026-11 jam re-implemented the same setup: throttling off, the floating
# window size written to BOTH the LevelEditorSubsystem play settings and the LevelEditorPlaySettings
# CDO (with only one set, the window comes back at the size it was last closed at), StartPIE, a
# wait for the local player controller, DisableAllScreenMessages, and base64-to-PNG saving of a
# CaptureGameViewport result. pie_session() and capture_game() are that code, written once.
# Both are coroutines: use them from an execute_python script with top-level await.

_EDITOR_APP = "EditorToolset.EditorAppToolset"
_SCENE_CAPTURE = "SceneCaptureToolset.SceneCaptureToolset"
_WINDOW_FIELDS = ("new_window_width", "new_window_height", "new_window_position")


def game_world():
    """The PIE (or Simulate) world from UnrealEditorSubsystem.get_game_world(), or None."""
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    except Exception:
        return None


def _play_settings_objects() -> list:
    """The subsystem's play settings object and the LevelEditorPlaySettings CDO (different objects)."""
    objects = []
    try:
        objects.append(unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_play_settings())
    except Exception:
        pass
    try:
        cdo = unreal.LevelEditorPlaySettings.get_default_object()
        if not any(cdo == o for o in objects):
            objects.append(cdo)
    except Exception:
        pass
    return [o for o in objects if o is not None]


def _pie_running() -> bool:
    try:
        return bool(exec_tool(_EDITOR_APP, "IsPIERunning"))
    except Exception:
        return game_world() is not None


async def _await_until(predicate, timeout_s: float, step_s: float = 0.1):
    """Poll predicate() with await asyncio.sleep(step_s) until it is truthy or the wall-clock deadline
    passes; returns the truthy value or None. The editor ticks during each await."""
    deadline = time.monotonic() + max(0.0, float(timeout_s))
    while True:
        value = predicate()
        if value:
            return value
        if time.monotonic() >= deadline:
            return None
        await asyncio.sleep(step_s)


class PieSession(object):
    """A PIE run with its editor settings, made by pie_session(); see there.

    start() / stop() are the coroutines `async with` runs. stop() is idempotent and puts back
    everything start() changed, also after a start() that failed half-way.
    """

    def __init__(self, width: int = 1280, height: int = 720, hide_screen_messages: bool = True, *,
                 position=(0, 0), play_mode: str = "PlayMode_InEditorFloating",
                 warmup_s: float = 2.0, timeout_s: float = 30.0, clients: Optional[int] = None,
                 net_mode: Optional[str] = None, stop_on_exit: bool = True):
        self.width = int(width)
        self.height = int(height)
        self.hide_screen_messages = bool(hide_screen_messages)
        self.position = position
        self.play_mode = play_mode
        self.warmup_s = float(warmup_s)
        self.timeout_s = float(timeout_s)
        self.stop_on_exit = bool(stop_on_exit)
        self.state = {"world": None, "player_controller": None, "started_pie": False,
                      "window": (self.width, self.height), "gate": None, "notes": []}
        self._gate = GateSession(clients=clients, net_mode=net_mode, throttle=False)
        self._window_before = []      # [(settings object, {field: value}), ...]
        self._hid_messages = False
        self._requested_pie = False   # StartPIE was sent (it may have raised after PIE came up)
        self._started = False
        self._stopped = False

    def _apply_window(self):
        position = self.position
        if position is not None and not isinstance(position, unreal.IntPoint):
            position = unreal.IntPoint(int(position[0]), int(position[1]))
        for settings in _play_settings_objects():
            before = {}
            for field in _WINDOW_FIELDS:
                try:
                    value = settings.get_editor_property(field)
                except Exception:
                    continue
                if isinstance(value, unreal.IntPoint):
                    # A copy: restoring the read-back wrapper itself did not put the position back.
                    value = unreal.IntPoint(value.x, value.y)
                before[field] = value
            self._window_before.append((settings, before))
            settings.set_editor_property("new_window_width", self.width)
            settings.set_editor_property("new_window_height", self.height)
            if position is not None:
                settings.set_editor_property("new_window_position", position)

    def _restore_window(self):
        for settings, before in reversed(self._window_before):
            for field, value in before.items():
                try:
                    settings.set_editor_property(field, value)
                except Exception as exc:
                    unreal.log_warning("vibeue.pie_session: could not restore {}: {}".format(field, exc))
        self._window_before = []

    async def start(self) -> dict:
        """Throttling off, window size, StartPIE, wait for player 0's controller, screen messages off."""
        if self._started:
            return self.state
        if _pie_running():
            raise RuntimeError("vibeue.pie_session: a PIE session is already running; stop it first "
                               "(EditorToolset.EditorAppToolset.StopPIE)")
        self._started = True
        try:
            self.state["gate"] = self._gate.enter()
            self._apply_window()
            self._requested_pie = True
            await exec_tool_await(_EDITOR_APP, "StartPIE", {
                "options": {"playMode": self.play_mode, "warmupSeconds": self.warmup_s}})
            self.state["started_pie"] = True

            def _player():
                world = game_world()
                if world is None:
                    return None
                return unreal.GameplayStatics.get_player_controller(world, 0)

            pc = await _await_until(_player, self.timeout_s)
            if pc is None:
                raise TimeoutError("vibeue.pie_session: no player controller after {:.0f} s".format(
                    self.timeout_s))
            world = game_world()
            if self.hide_screen_messages:
                unreal.SystemLibrary.execute_console_command(world, "DisableAllScreenMessages")
                self._hid_messages = True
            self.state["world"] = world
            self.state["player_controller"] = pc
            return self.state
        except BaseException:
            await self.stop()
            raise

    async def stop(self) -> dict:
        """StopPIE (if this session started it), then put back messages, window size and settings."""
        if self._stopped or not self._started:
            return self.state
        self._stopped = True
        # Drop PIE object references first: one kept past StopPIE asserts on PIE teardown.
        self.state["world"] = None
        self.state["player_controller"] = None
        try:
            if self._hid_messages:
                unreal.SystemLibrary.execute_console_command(None, "EnableAllScreenMessages")
                self._hid_messages = False
            # PIE was not running when start() began, so one that runs now is this session's, also
            # when StartPIE itself raised (its 30 s limit) after the session came up.
            if (self.state["started_pie"] or self._requested_pie) and self.stop_on_exit and _pie_running():
                await exec_tool_await(_EDITOR_APP, "StopPIE")
                if await _await_until(lambda: game_world() is None, 10.0) is None:
                    self.state["notes"].append("the PIE world was still alive 10 s after StopPIE")
        finally:
            self._restore_window()
            self._gate.exit()
        return self.state

    async def __aenter__(self) -> dict:
        return await self.start()

    async def __aexit__(self, exc_type, exc, tb) -> bool:
        await self.stop()
        return False


def pie_session(width: int = 1280, height: int = 720, hide_screen_messages: bool = True, *,
                position=(0, 0), play_mode: str = "PlayMode_InEditorFloating", warmup_s: float = 2.0,
                timeout_s: float = 30.0, clients: Optional[int] = None,
                net_mode: Optional[str] = None, stop_on_exit: bool = True) -> PieSession:
    """Start a PIE session set up for an agent, and put the editor back afterwards.

    An async context manager, for an execute_python script with top-level await:

        import unreal, vibeue
        async with vibeue.pie_session(1280, 720) as s:
            pc = s["player_controller"]
            # ... drive the game: await call_tool(...), await asyncio.sleep(...) ...
            shot = await vibeue.capture_game("C:/Proj/Saved/Shots/menu.png")
        shot

    On enter, in order: background throttling off (gate_session; with clients / net_mode it also
    sets the PIE net mode); new_window_width / new_window_height / new_window_position written to
    BOTH the LevelEditorSubsystem play settings and the LevelEditorPlaySettings CDO; StartPIE with
    play_mode (floating window by default, so the size applies) and warmup_s; a wait, with awaits
    (never time.sleep), for player 0's controller up to timeout_s (TimeoutError after); then
    DisableAllScreenMessages when hide_screen_messages. Yields a dict {"world", "player_controller",
    "started_pie", "window", "gate", "notes"}.

    On exit, on every path (exceptions included): EnableAllScreenMessages, StopPIE and a wait until
    the PIE world is gone (skipped with stop_on_exit=False), the window settings and the throttling /
    net-mode settings put back. Raises RuntimeError at once if PIE is already running. A failure
    half-way through enter undoes what was done.

    When PIE ends, the engine itself writes the floating window's last size and position to the
    LevelEditorPlaySettings CDO and saves EditorPerProjectUserSettings.ini (PlayLevel.cpp). The
    in-memory settings are put back after that, so the next PIE opens at the old size, but the ini
    file keeps this session's size until the editor next saves its play settings.

    The whole block runs inside one execute_python call, so keep it short (a client times out after
    roughly a minute while the script keeps running). The yielded world and controller are dropped
    on exit; do not keep PIE objects in a module past the block.
    """
    return PieSession(width, height, hide_screen_messages, position=position, play_mode=play_mode,
                      warmup_s=warmup_s, timeout_s=timeout_s, clients=clients, net_mode=net_mode,
                      stop_on_exit=stop_on_exit)


async def capture_game(path: str, max_res: Optional[int] = None, world=None) -> str:
    """Capture the running game viewport, UMG included, to a PNG file and return its path.

    Calls SceneCaptureToolset.SceneCaptureToolset.CaptureGameViewport (as Slate draws it: scene plus
    every widget over the viewport) and writes the decoded image to `path` (folders are created).
    world defaults to game_world(); pass one of pie_worlds()["clients"] for a client window.
    max_res caps the longest edge; None uses the viewport's own longest edge (a 1:1 shot; the tool's
    default would be 1024), capped by the toolset's configured maximum. Raises RuntimeError when
    there is no game world or the capture comes back empty. Return the path, not the image, from
    your script, then open the PNG with your file-reading tool.

        path = await vibeue.capture_game("C:/Proj/Saved/Shots/hud.png")
    """
    if world is None:
        world = game_world()
    if world is None:
        raise RuntimeError("vibeue.capture_game: no PIE or Game world; start PIE first")
    if max_res is None:
        try:
            size = unreal.WidgetLayoutLibrary.get_viewport_size(world)
            max_res = int(round(max(size.x, size.y)))
        except Exception:
            max_res = 0
        if max_res <= 0:
            max_res = 1024
    result = await exec_tool_await(_SCENE_CAPTURE, "CaptureGameViewport",
                                   {"world": world.get_path_name(), "maxResolution": int(max_res)})
    image = result.get("image") if isinstance(result, dict) else None
    data = image.get("data") if isinstance(image, dict) else image
    if not data:
        warnings = result.get("warnings") if isinstance(result, dict) else result
        raise RuntimeError("vibeue.capture_game: the capture came back empty ({})".format(warnings))
    folder = os.path.dirname(os.path.abspath(path))
    if folder:
        os.makedirs(folder, exist_ok=True)
    with open(path, "wb") as handle:
        handle.write(base64.b64decode(data))
    return path


# --- Small API traps -------------------------------------------------------------------------

def mapping_context_priority(subsystem, ctx) -> Optional[int]:
    """The priority an Input Mapping Context is applied at, or None when it is not applied.

    EnhancedInputLocalPlayerSubsystem.has_mapping_context(ctx) in 5.8 Python returns the found
    PRIORITY (an int) or None: the C++ bool return becomes None-or-out-param. Priority 0 is common
    (IMC_Default), so `if subsystem.has_mapping_context(ctx):` reads an applied context as absent.
    Also tolerates a (bool, priority) tuple or a bare bool from other builds (True -> 0, priority
    unknown).

    Example:
        prio = vibeue.mapping_context_priority(eis, imc)
        print("applied" if prio is not None else "absent", prio)
    """
    if subsystem is None or ctx is None:
        return None
    try:
        found = subsystem.has_mapping_context(ctx)
    except Exception:
        return None
    if found is None or found is False:
        return None
    if found is True:
        return 0
    if isinstance(found, tuple):
        if not found or not found[0]:
            return None
        return int(found[1]) if len(found) > 1 else 0
    try:
        return int(found)
    except (TypeError, ValueError):
        return None


def _split_asset_path(path: str):
    """'/Game/UI/W_X' or '/Game/UI/W_X.W_X' -> ('/Game/UI/W_X', 'W_X')."""
    package, _, asset = path.partition(".")
    if not asset:
        asset = package.rsplit("/", 1)[-1]
    return package, asset


def canvas_slot(widget_bp_path: str, widget_name: str):
    """The DESIGN-TIME CanvasPanelSlot of a widget in a Widget Blueprint (or None).

    Loads `<pkg>.<Asset>:WidgetTree.<widget_name>`, the editable source tree. The asset also holds
    `<Asset>_C:WidgetTree`, the generated copy compile rebuilds: never edit that one.
    WidgetBlueprint has no Python widget_tree property, hence the object path.

    Example:
        slot = vibeue.canvas_slot("/Game/UI/W_PlayerHUD", "AmmoText")
        print(slot.get_anchors(), slot.get_offsets())
    """
    package, asset = _split_asset_path(widget_bp_path)
    widget = unreal.load_object(None, "{}.{}:WidgetTree.{}".format(package, asset, widget_name))
    if widget is None:
        return None
    slot = get_prop(widget, "slot")
    if slot is None:
        slot = getattr(widget, "slot", None)
    if slot is None or not isinstance(slot, unreal.CanvasPanelSlot):
        return None
    return slot


def _vec2(value):
    if isinstance(value, unreal.Vector2D):
        return value
    x, y = value
    return unreal.Vector2D(float(x), float(y))


def set_canvas_slot(widget_bp_path: str, widget_name: str, *, anchor_min=None, anchor_max=None,
                    offsets=None, alignment=None, auto_size: Optional[bool] = None) -> dict:
    """Edit a widget's design-time CanvasPanelSlot, then compile and save the Widget Blueprint.

    Only the given arguments change: anchor_min / anchor_max / alignment are (x, y) or Vector2D
    (the other anchor is kept), offsets is (left, top, right, bottom) or a Margin, auto_size a
    bool. Then BlueprintEditorLibrary.compile_blueprint(bp) and EditorAssetLibrary.save_asset(path,
    only_if_is_dirty=False). Returns {"applied": [...], "compiled": bool, "saved": bool, "error"}.

    For a single scalar, go through the engine: ObjectToolset.SetProperties on the slot object
    (WidgetService.set_property and its "Position X/Y" / "Anchor Min X/Y" slot aliases were removed
    on the 5-9 branch). Use this helper for whole anchors/offsets at once. It edits `<Asset>:WidgetTree` (the editable source),
    not `<Asset>_C:WidgetTree` (the generated copy).

    Example:
        print(vibeue.set_canvas_slot("/Game/UI/W_PlayerHUD", "AmmoText",
                                     anchor_min=(1, 1), anchor_max=(1, 1), alignment=(1, 1),
                                     offsets=(-40, -40, 200, 48)))
    """
    out = {"applied": [], "compiled": False, "saved": False, "error": ""}
    slot = canvas_slot(widget_bp_path, widget_name)
    if slot is None:
        out["error"] = "no CanvasPanelSlot for '{}' in {}".format(widget_name, widget_bp_path)
        return out
    try:
        slot.modify(True)
    except Exception:
        pass
    try:
        if anchor_min is not None or anchor_max is not None:
            anchors = slot.get_anchors()
            minimum = _vec2(anchor_min) if anchor_min is not None else anchors.minimum
            maximum = _vec2(anchor_max) if anchor_max is not None else anchors.maximum
            slot.set_anchors(unreal.Anchors(minimum=minimum, maximum=maximum))
            out["applied"].append("anchors")
        if offsets is not None:
            if not isinstance(offsets, unreal.Margin):
                left, top, right, bottom = offsets
                offsets = unreal.Margin(left=float(left), top=float(top),
                                        right=float(right), bottom=float(bottom))
            slot.set_offsets(offsets)
            out["applied"].append("offsets")
        if alignment is not None:
            slot.set_alignment(_vec2(alignment))
            out["applied"].append("alignment")
        if auto_size is not None:
            slot.set_auto_size(bool(auto_size))
            out["applied"].append("auto_size")
    except Exception as exc:
        out["error"] = str(exc)
        return out

    package, _ = _split_asset_path(widget_bp_path)
    bp = unreal.EditorAssetLibrary.load_asset(package)
    if bp is None:
        out["error"] = "could not load {}".format(package)
        return out
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        out["compiled"] = True
    except Exception as exc:
        out["error"] = "compile failed: {}".format(exc)
        return out
    out["saved"] = bool(unreal.EditorAssetLibrary.save_asset(package, only_if_is_dirty=False))
    return out
