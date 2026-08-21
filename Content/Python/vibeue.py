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
"""

import json

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
