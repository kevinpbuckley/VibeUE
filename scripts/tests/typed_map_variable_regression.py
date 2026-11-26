"""Regression checks for typed Blueprint Map variables on 5-9 (the 5-9 counterpart of PR #715).

On 5-9 Blueprint variables are Epic's BlueprintToolset.AddVariable, whose MAP container always has an
FString key. A Map with any other key goes through the engine's Python library: build the pin type with
BlueprintEditorLibrary.get_map_type(key, value) and add it with add_member_variable. This script proves
that route (create, compile, save, reload, read the key and value types back), that AddVariable's MAP
is FString-keyed, and that an unhashable key silently becomes a plain int, so the skill keeps saying so.

Run from an execute_python script (top-level await) in a DISPOSABLE editor session (any RHI). It writes
/Game/VibeUETests/TypedMap_<id> and deletes it at the end:

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import typed_map_variable_regression as t
    await t.run(call_tool)

Returns {"passed": [...], "failed": [...], "details": {...}}.
"""

import json
import uuid

import unreal

ROOT = "/Game/VibeUETests"
BT = "BlueprintToolsets.BlueprintToolset"


def _check(results, name, ok, detail=None):
    (results["passed"] if ok else results["failed"]).append(name)
    if detail is not None:
        results["details"][name] = detail
    return ok


def _new_blueprint(folder, name):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Blueprint, factory)


def _key_kind(cdo, prop, sample_key, sample_value):
    """The Python type a key comes back as once inserted into a COPY of the CDO's map (Name vs str shows
    an FName key from an FString key; the property schema reports both as plain strings)."""
    m = cdo.get_editor_property(prop)
    m[sample_key] = sample_value
    return type(list(m.keys())[0])


async def _schema(call_tool, cdo):
    """ObjectToolset.GetPropertySchema of the CDO: per map, propertyNames (key) and additionalProperties (value)."""
    raw = await call_tool("EditorToolset.ObjectToolset.GetPropertySchema", instance={"refPath": cdo.get_path_name()},
                          maxDepth=1)
    schema = json.loads(raw) if isinstance(raw, str) else raw
    out = {}
    for name, entry in schema.items():
        if isinstance(entry, dict) and "additionalProperties" in entry:
            keys = dict(entry.get("propertyNames", {}))
            keys.pop("enum", None)
            out[name] = {"key": keys, "value": entry.get("additionalProperties")}
    return out


def _describe(cdo, schema):
    return {
        "Weights": (_key_kind(cdo, "Weights", "a", 1.0).__name__, schema.get("weights", {}).get("value")),
        "Materials": (_key_kind(cdo, "Materials", "a", None).__name__, schema.get("materials", {}).get("value")),
        "Channels": (schema.get("channels", {}).get("key"), schema.get("channels", {}).get("value")),
    }


async def run(call_tool):
    lib = unreal.BlueprintEditorLibrary
    results = {"passed": [], "failed": [], "details": {}}
    folder = f"{ROOT}/TypedMap_{uuid.uuid4().hex[:12]}"
    path = folder + "/BP_TypedMap"
    try:
        bp = _new_blueprint(folder, "BP_TypedMap")
        if not _check(results, "fixture Blueprint", bp is not None):
            return results

        name_key = lib.get_basic_type_by_name("name")
        weights = lib.get_map_type(name_key, lib.get_basic_type_by_name("real"))
        materials = lib.get_map_type(name_key, lib.get_object_reference_type(unreal.MaterialInterface))
        channels = lib.get_map_type(lib.get_enum_type(unreal.CollisionChannel.static_enum()),
                                    lib.get_struct_type(unreal.Vector.static_struct()))
        added = [lib.add_member_variable(bp, n, t) for n, t in
                 (("Weights", weights), ("Materials", materials), ("Channels", channels))]
        _check(results, "three typed Map members added", all(added), added)
        await call_tool(f"{BT}.CompileBlueprint", blueprint={"refPath": bp.get_path_name()})
        _check(results, "compiled cleanly", bp.get_editor_property("status") == unreal.BlueprintStatus.BS_UP_TO_DATE,
               str(bp.get_editor_property("status")))

        cdo = unreal.get_default_object(bp.generated_class())
        before = _describe(cdo, await _schema(call_tool, cdo))
        _check(results, "Weights is Map<Name, float>", before["Weights"] == ("Name", {"type": "number"}), before["Weights"])
        _check(results, "Materials is Map<Name, MaterialInterface>", before["Materials"][0] == "Name"
               and (before["Materials"][1] or {}).get("title") == "/Script/Engine.MaterialInterface", before["Materials"])
        _check(results, "Channels is Map<ECollisionChannel, Vector>",
               (before["Channels"][0] or {}).get("title") == "ECollisionChannel"
               and (before["Channels"][1] or {}).get("title") == "Vector", before["Channels"])

        # Survives save and reload.
        unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
        unreal.EditorLoadingAndSavingUtils.reload_packages([bp.get_package()])
        bp = unreal.load_asset(path)
        cdo = unreal.get_default_object(bp.generated_class())
        after = _describe(cdo, await _schema(call_tool, cdo))
        _check(results, "all three Map types identical after save and reload", before == after,
               {"before": before, "after": after})

        # Epic's AddVariable: a MAP container is FString-keyed (what the skill says).
        await call_tool(f"{BT}.AddVariable", blueprint={"refPath": bp.get_path_name()}, name="ByString",
                        typeName="float", options={"containerType": "MAP"})
        await call_tool(f"{BT}.CompileBlueprint", blueprint={"refPath": bp.get_path_name()})
        kind = _key_kind(unreal.get_default_object(bp.generated_class()), "ByString", "a", 1.0)
        _check(results, "BlueprintToolset.AddVariable MAP has an FString key", kind is str, kind.__name__)

        # An unhashable key: get_map_type falls back to a plain int and the add still reports success
        # (what the skill warns about).
        bool_map = lib.get_map_type(lib.get_basic_type_by_name("bool"), lib.get_basic_type_by_name("int"))
        added_bool = lib.add_member_variable(bp, "BoolKeyed", bool_map)
        await call_tool(f"{BT}.CompileBlueprint", blueprint={"refPath": bp.get_path_name()})
        value = unreal.get_default_object(bp.generated_class()).get_editor_property("BoolKeyed")
        _check(results, "bool key: get_map_type gives a plain int, add still succeeds",
               added_bool and isinstance(value, int) and not isinstance(value, unreal.Map), type(value).__name__)
    finally:
        if unreal.EditorAssetLibrary.does_directory_exist(folder):
            unreal.EditorAssetLibrary.delete_directory(folder)
    return results
