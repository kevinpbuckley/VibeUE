---
name: data-assets
display_name: Data Assets
description: Create and modify Primary Data Assets and set their properties (DataAssetService). Use when the user asks to create a Data Asset / PrimaryDataAsset (DA_), populate its fields, or read/write Data Asset properties.
vibeue_classes:
  - DataAssetService
unreal_classes:
  - EditorAssetLibrary
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "data-driven-design"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Data Assets Skill

## Critical Rules

### ⚠️ Property Values Are ALWAYS Strings

```python
# WRONG
unreal.DataAssetService.set_property(path, "Damage", 75)

# CORRECT
unreal.DataAssetService.set_property(path, "Damage", "75")
unreal.DataAssetService.set_property(path, "IsActive", "true")
```

### ⚠️ Complex Properties Use Unreal String Format, NOT JSON

```python
# WRONG - JSON format won't work
keys_json = '[{"EntryName": "Key1"}]'

# CORRECT - Unreal string format
keys_str = '((EntryName="Key1"),(EntryName="Key2"))'
```

### ⚠️ Asset Reference Format — full object path, EVERYWHERE

This applies to top-level properties AND object paths nested inside struct/array
values. A bare package path either fails to resolve or gets stored as a broken
soft reference that will never load.

```python
# WRONG
"/Game/Meshes/Cube"

# CORRECT - repeat asset name
"/Game/Meshes/Cube.Cube"

# Blueprint classes add _C suffix
"/Game/Blueprints/BP_Enemy.BP_Enemy_C"

# Object refs inside a struct value - same rule, quoted:
unreal.DataAssetService.set_property(
    nsc_path, "SystemCollection",
    '(Systems=("/Game/Fx/NS_Leaf.NS_Leaf","/Game/Fx/NS_Dust.NS_Dust"))')
```

### ⚠️ Read Back After Setting Complex Values

`set_property` returns `False` if the value string can't be fully parsed, but a
*parseable-yet-wrong* value (e.g. a soft-ref path missing `.AssetName`) still
applies and returns `True`.
After any struct/array set, `get_property` it back and confirm:

```python
unreal.DataAssetService.set_property(path, "SystemCollection", value_str)
print(unreal.DataAssetService.get_property(path, "SystemCollection"))  # verify!
```

### ⚠️ Return Types Are Structs, Not Dicts — and field names differ per struct

```python
# WRONG
info = unreal.DataAssetService.get_info(path)
print(info["name"])      # ERROR - not a dict
print(t.parent_name)     # ERROR - no such field on any result struct

# CORRECT
# search_types() returns DataAssetTypeInfo:   .name .path .module .is_native .parent_class   (singular string)
# get_class_info() returns DataAssetClassInfo: .name .path .is_abstract .is_native .parent_classes (array) .properties
# properties entries are DataAssetPropertyInfo: .name .type .category .description .defined_in .read_only .is_array
for t in unreal.DataAssetService.search_types("Input"):
    print(t.name, t.parent_class)

info = unreal.DataAssetService.get_class_info("InputAction", True)
if not info.name:
    print("class not found")          # empty struct = lookup failed
for p in info.properties:
    print(p.name, p.type, p.read_only)
```

### ⚠️ Always Save After Modifications

```python
unreal.DataAssetService.set_property(path, "Damage", "100")
unreal.EditorAssetLibrary.save_asset(path)  # REQUIRED
```

---

## What Is (and Is NOT) a DataAsset

`DataAssetService` only handles `UDataAsset` subclasses. Common types that look
like data assets but are NOT — create/edit these via other tools:

| Type | Actually inherits | How to create instead |
|---|---|---|
| DataTable / CompositeDataTable | UObject | data-tables skill (`DataTableService`) |
| CurveFloat / CurveVector / CurveLinearColor | UCurveBase | AssetTools + curve factory |
| BehaviorTree | UObject | AssetTools + `unreal.BehaviorTreeFactory` |
| PhysicalMaterial | UObject | AssetTools + `unreal.PhysicalMaterialFactoryNew` |
| SoundAttenuation / SoundConcurrency | UObject | AssetTools + `unreal.SoundAttenuationFactory` / `unreal.SoundConcurrencyFactory` (NOT `...FactoryNew`) |
| MaterialParameterCollection | UObject | materials skill |

Generic AssetTools pattern (verify the factory exists first — not every build
exposes every factory):

```python
import unreal
factory_name = "PhysicalMaterialFactoryNew"
if hasattr(unreal, factory_name):
    factory = getattr(unreal, factory_name)()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    asset = tools.create_asset("PM_Ice", "/Game/Data/Physics", None, factory)
else:
    print(f"{factory_name} not available in this build")
```

Inspecting a NON-DataAsset asset: `DataAssetService.get_info/list_properties`
won't work, and `UClass` has no `.properties` attribute in Python. Load it and
use `get_editor_property`, with `discover_python_class` for the property list:

```python
pm = unreal.EditorAssetLibrary.load_asset("/Game/Data/Physics/PM_Ice")
print(pm.get_editor_property("friction"))
pm.set_editor_property("friction", 0.05)
unreal.EditorAssetLibrary.save_asset("/Game/Data/Physics/PM_Ice")
```

---

## Discovery Notes

- `search_types(filter)` is a **case-insensitive substring** match on the class
  name. Short filters are noisy: `"AI"` also matches `ConcertAssetCont`**`AI`**`ner`
  and `PaperTerr`**`AI`**`nMaterial`. Prefer full words (`"Blackboard"`,
  `"EnvQuery"`, `"StateTree"`) and run several precise searches.
- `search_types` only returns **concrete, creatable** classes — abstract bases
  (PrimaryDataAsset, DataAsset) never appear. `get_class_info` DOES work on
  abstract classes if you need their schema.
- `SKEL_*_C` entries are Blueprint skeleton classes — never instantiate them;
  use the matching `*_C` class (without `SKEL_`) instead.

---

## Property Formats

### Simple Types

```python
unreal.DataAssetService.set_property(path, "Level", "50")        # Integer
unreal.DataAssetService.set_property(path, "Weight", "5.5")      # Float
unreal.DataAssetService.set_property(path, "IsActive", "true")   # Boolean
unreal.DataAssetService.set_property(path, "Name", "Iron Sword") # String
```

### Structs (Unreal Text Format)

```python
# FVector
unreal.DataAssetService.set_property(path, "Location", "(X=100.0,Y=200.0,Z=50.0)")

# FLinearColor
unreal.DataAssetService.set_property(path, "Color", "(R=1.0,G=0.5,B=0.0,A=1.0)")

# Custom struct
unreal.DataAssetService.set_property(path, "Stats", "(Attack=75,Defense=50)")
```

### Deeply Nested Structs — set the WHOLE root property

There is no dotted-path syntax (`Settings.General.Delay` won't work). Read the
full value, edit the text, write the full value back:

```python
cur = unreal.DataAssetService.get_property(path, "Settings")
new = cur.replace("EventSchedulingMinDelaySeconds=0.300000",
                  "EventSchedulingMinDelaySeconds=0.500000")
unreal.DataAssetService.set_property(path, "Settings", new)
```

### Arrays of Structs — include existing entries when appending

Setting an array property REPLACES it. To append, read current value first and
keep existing entries (note: instanced sub-objects like blackboard `KeyType`
survive as quoted object paths — keep them verbatim; escape any `'` inside an
entry string as `\'`):

```python
cur = unreal.DataAssetService.get_property(bb_path, "Keys")  # ((EntryName="SelfActor",KeyType="..."))
entry = '(EntryName="TargetLocation",EntryCategory="Combat",EntryDescription="The location we\\\'re moving to")'
new = cur[:-1] + "," + entry + ")"
unreal.DataAssetService.set_property(bb_path, "Keys", new)
```

Limitation: you cannot CREATE new instanced sub-objects (e.g. a blackboard
key's `KeyType`) from a string — entries added this way have no key type until
set up in the editor.

---

## Workflows

### Create → Configure → Save (idempotent)

```python
import unreal

full = "/Game/Data/DA_NewItem"
if unreal.EditorAssetLibrary.does_asset_exist(full):
    unreal.EditorAssetLibrary.delete_asset(full)   # create fails if asset exists

path = unreal.DataAssetService.create_data_asset("InputAction", "/Game/Data", "DA_NewItem")
if not path:                                        # empty string = failure (see Output Log)
    raise RuntimeError("create failed")
unreal.DataAssetService.set_property(path, "bConsumeInput", "true")
unreal.EditorAssetLibrary.save_asset(path)
```

### Discover Properties First

```python
import unreal

# Find available DataAsset classes
types = unreal.DataAssetService.search_types("Item")
for t in types:
    print(f"{t.name}: {t.parent_class}")

# Get class schema (works for abstract classes too)
info = unreal.DataAssetService.get_class_info("InputAction", True)
for p in info.properties:
    print(f"  {p.name}: {p.type}")
```

### Inspect Existing Asset

```python
import unreal

path = "/Game/Data/DA_Item"
props = unreal.DataAssetService.list_properties(path)
for p in props:
    value = unreal.DataAssetService.get_property(path, p.name)
    print(f"{p.name}: {value}")
```

### Set Multiple Properties

```python
import unreal
import json

properties = {"Name": "Iron Sword", "Damage": "50", "Weight": "5.5"}
result = unreal.DataAssetService.set_properties(path, json.dumps(properties))
# result.success_properties - list of properties set
# result.failed_properties - list that failed
```

## Sample scripts (run via `execute_python_code`)

- **`scripts/create_data_asset.pyx`** — create a Primary Data Asset and set properties.
