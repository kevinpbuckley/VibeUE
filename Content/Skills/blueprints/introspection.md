---
name: blueprints/introspection
description: What Blueprint introspection works and what doesn't in UE 5.7 Python — getting level actors, finding BP assets, reading components via CDO, parent class via asset tags, listing BPs by class, and material parameters.
---

# Blueprint Introspection — What Works and What Doesn't (UE 5.7)

## Contents
- Getting level actors
- Finding Blueprint assets
- Inspecting Blueprint components (CDO pattern)
- Getting the parent class
- Listing Blueprints by class
- Material parameters

## Getting level actors

```python
# CORRECT
actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = actor_subsys.get_all_level_actors()

# WRONG — deprecated, blocked by VibeUE in UE 5.7
unreal.EditorLevelLibrary.get_all_level_actors()
```

## Finding Blueprint assets

```python
ar = unreal.AssetRegistryHelpers.get_asset_registry()
assets = ar.get_assets_by_path('/Game', recursive=True)
print(assets[0].package_name)   # /Game/Blueprints/BP_MyActor
print(assets[0].package_path)   # /Game/Blueprints
# WRONG — object_path does not exist on AssetData in UE 5.7
```

## Inspecting Blueprint components (CDO pattern)

Direct introspection APIs are largely missing/protected in UE 5.7. The reliable read path is the CDO:

```python
import unreal

bp = unreal.EditorAssetLibrary.load_asset("/Game/Blueprints/BP_MyActor")
# unreal.load_asset() returns None on a fresh session; use EditorAssetLibrary.load_asset()
gc = bp.generated_class()
cdo = unreal.get_default_object(gc)   # read-only is allowed

for comp in cdo.get_components_by_class(unreal.ActorComponent):
    print(f"{comp.get_class().get_name()}: {comp.get_name()}")
```

CDO modification is blocked — `set_editor_property(...)` on the CDO raises `PYTHON_UNSAFE_CODE`.

These do **not** work: `bp.get_editor_property('simple_construction_script')` (protected),
`scs.get_all_nodes()` (crash), `BlueprintEditorLibrary.get_blueprint_component_names()` (absent),
`bp.parent_class` / `bp.get_editor_property('parent_class')` (not exposed).

## Getting the parent class

```python
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for a in ar.get_assets_by_path("/Game/Blueprints", False):
    if "BP_MyActor" in str(a.asset_name):
        print(a.get_tag_value("ParentClass"))        # e.g. "/Script/Engine.Actor"
        print(a.get_tag_value("NativeParentClass"))  # native C++ parent
        break
```

## Listing Blueprints by class

```python
# ARFilter properties can't be set after construction; asset_class_names kwarg doesn't exist.
ar = unreal.AssetRegistryHelpers.get_asset_registry()
bp_class = unreal.TopLevelAssetPath("/Script/Engine", "Blueprint")
assets = ar.get_assets_by_class(bp_class, True)   # True = include derived
for a in assets:
    print(f"{a.package_path}/{a.asset_name}")
```

## Function & override introspection field names

Bool fields drop the C++ `b` prefix in Python (`bIsPure` → `is_pure`). Don't guess `b_*`.

`list_functions(bp)` → **BlueprintFunctionInfo**: `function_name`, `is_override`, `is_pure`,
`parameters`, `return_type`.

`list_overridable_functions(bp)` → **OverridableFunctionInfo**: `function_name`, `owner_class`,
`already_overridden`, `is_event_style`, `is_native_event`, `parameters`, `return_type`.

Function parameters → **BlueprintFunctionParameterInfo**: `parameter_name`, `parameter_type`,
`is_output` (NOT `b_is_output`).

```python
for f in unreal.BlueprintService.list_functions(bp):
    print(f.function_name, "pure" if f.is_pure else "impure", "override" if f.is_override else "")
for o in unreal.BlueprintService.list_overridable_functions(bp):
    print(o.function_name, o.owner_class, "(already overridden)" if o.already_overridden else "")
```

## Material parameters

```python
import unreal
mat = unreal.load_asset('/Game/Materials/M_MyMaterial')
print(unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat))
```
