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


# --- Persisted Python results (B2) -------------------------------------------------
# execute_python_code persists every run to Saved/VibeUE/Signals/python-<pid>-last.json (latest)
# and python-<pid>-runs.jsonl (history). A client whose call timed out (~30s / 300s) while the
# script kept running in the editor reads these on its NEXT call instead of re-running the script
# and double-executing a mutation. The recovery call runs in the SAME editor process, so os.getpid()
# resolves the same files the C++ wrote.


def _signals_dir():
    saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
    return os.path.join(saved, "VibeUE", "Signals")


def _last_result_path(pid=None):
    return os.path.join(_signals_dir(), "python-{}-last.json".format(pid or os.getpid()))


def _runs_path(pid=None):
    return os.path.join(_signals_dir(), "python-{}-runs.jsonl".format(pid or os.getpid()))


def last_python_result(pid=None):
    """The persisted result of the most recent execute_python_code run in THIS editor process, as a
    dict (keys: runId, pid, success, label, output, error, result, execution_time_ms, startedUtc,
    finishedUtc), or None if nothing has been recorded.

    Use after a call timed out: `execute_python_code("import vibeue; print(vibeue.last_python_result())")`
    returns the lost run because the read happens before this call's own result is persisted. Note
    that a SECOND such read reflects the first read, not the original run — use its runId with
    python_run() for a stable handle, or read it once and keep it.
    """
    try:
        with open(_last_result_path(pid), "r", encoding="utf-8") as handle:
            return json.load(handle)
    except (OSError, ValueError):
        return None


def python_run(run_id, pid=None):
    """The persisted record for a specific runId from python-<pid>-runs.jsonl (a dict), or None if it
    is not present (it may have been trimmed — the history keeps the last ~200 runs / ~2 MB)."""
    try:
        with open(_runs_path(pid), "r", encoding="utf-8") as handle:
            lines = handle.readlines()
    except OSError:
        return None
    for line in reversed(lines):
        line = line.strip()
        if not line:
            continue
        try:
            record = json.loads(line)
        except ValueError:
            continue
        if record.get("runId") == run_id:
            return record
    return None


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


# --- Non-blocking waits (gate harnesses) -----------------------------------------------------
#
# execute_python_code runs on the GAME THREAD. time.sleep() there freezes the editor, the PIE
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
    """A pending wait_for() — poll it in a LATER execute_python_code call, or chain .then().

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

    The editor does not tick while execute_python_code runs, so a wait CANNOT finish inside the call
    that starts it (the first evaluation happens on the next tick). Two usage patterns:

        # 1) start in one call ...
        import vibeue
        w = vibeue.wait_for(lambda: unreal.GameplayStatics.get_player_pawn(world, 0),
                            timeout_s=5, description="pawn")
        # ... and read it back in a LATER call (script globals persist between calls):
        print(w.done, w.ok, w.elapsed_s)     # or: print(vibeue.waits())

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
        unreal.BlueprintService.get_graph_summary("/Game/BP_Player", "EventGraph")
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
    when the plugin has it, else with ActorService.set_viewport_camera + ViewportService.set_fov +
    set_exposure(fixed, ev100), which puts both exposure fields back. Yields the snapshot (a ViewportInfo, or None when
    there is no level viewport, in which case nothing is restored).

    Example:
        with vibeue.viewport_state() as info:
            unreal.ViewportService.set_exposure(True, 11.5)
            unreal.ActorService.set_viewport_camera(unreal.Vector(0, 0, 500),
                                                    unreal.Rotator(roll=0, pitch=-30, yaw=0))
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
        unreal.ActorService.set_viewport_camera(info.location, info.rotation)
    except Exception as exc:
        unreal.log_warning("vibeue.viewport_state: camera restore failed: {}".format(exc))
    try:
        unreal.ViewportService.set_fov(info.fov)
    except Exception as exc:
        unreal.log_warning("vibeue.viewport_state: FOV restore failed: {}".format(exc))
    try:
        # One call writes both fields. set_exposure_game_settings() is set_exposure(False, 1.0):
        # it would reset the saved FixedEV100 to 1.0 when the user was on Game Settings.
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
    one execute_python_code call and exited in a later one cannot run its restore early when an
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
    net mode to apply (StartPIE is async). A `with` block must stay inside ONE execute_python_code
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

    For a single scalar, WidgetService.set_property already takes the slot aliases "Position X/Y",
    "Size X/Y", "Anchor Min X/Y", "Anchor Max X/Y", "Alignment X/Y" and any "Slot.<Prop>"; use this
    helper for whole anchors/offsets at once. It edits `<Asset>:WidgetTree` (the editable source),
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
