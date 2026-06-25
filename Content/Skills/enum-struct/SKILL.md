---
name: enum-struct
display_name: Enums and Structs
description: Create, modify, and introspect UserDefinedEnums and UserDefinedStructs (EnumStructService). Use when the user asks to create a Blueprint enum or struct, add enum values or struct members, or inspect an enum/struct's fields. Useful for defining a DataTable row struct.
vibeue_classes:
  - EnumStructService
  - BlueprintTypeParser
unreal_classes:
  - UserDefinedEnum
  - UserDefinedStruct
  - EditorAssetLibrary
keywords:
  - enum
  - struct
  - userdefined
  - enumeration
  - structure
  - type
  - property
  - datatable
---

# Enum and Struct Skill

## Critical Rules

### Read-Only Native Types

Native C++ enums and structs are **read-only**. Only UserDefinedEnums and UserDefinedStructs can be modified:

```python
import unreal

# Search for user-defined enums only (editable)
enums = unreal.EnumStructService.search_enums("Weapon", True)  # bUserDefinedOnly=True

# Check if an enum is user-defined before modifying
# (returns a single EnumInfo or None — NOT a (success, info) tuple)
info = unreal.EnumStructService.get_enum_info("EMyEnum")
if info and info.is_user_defined:          # field is is_user_defined, NOT b_is_user_defined
    unreal.EnumStructService.add_enum_value("EMyEnum", "NewValue")
```

### 🚨 Pass the EXACT asset name you want — prefixing is dumb

`create_enum` / `create_struct` only **prepend** `E`/`F` when the name doesn't already start
with that letter; they never insert underscores or reformat. So:

- want `E_TestState` → pass `"E_TestState"` (passing `"TestState"` creates `ETestState`)
- want `EWeaponType` → `"WeaponType"` or `"EWeaponType"` both work

Creation **fails with an empty string** if the asset already exists — delete it first
(`delete_enum` / `delete_struct`), there is no overwrite.

### 🚨 Enum values: you are always working with DISPLAY names

UserDefinedEnum entries have immutable internal names (`NewEnumerator0..N`). Every
`EnumStructService` method speaks **display names**: the name you pass to
`add_enum_value` becomes the display name, `get_enum_values` returns display names,
and `rename_enum_value` / `remove_enum_value` accept display names (rename changes the
display name only). For internal+display pairs use `get_enum_info(...).values`
(each entry has `name` = internal, `display_name`, `value`, `index`).

---

## Workflows

### Create UserDefinedEnum

```python
import unreal

# Create a new enum — pass the exact final name (here E prefix gets prepended)
enum_path = unreal.EnumStructService.create_enum("/Game/Data/Enums", "WeaponType")
print(f"Created: {enum_path}")  # /Game/Data/Enums/EWeaponType.EWeaponType
if not enum_path:
    # creation failed — most likely the asset already exists; delete_enum first
    ...

# Add values
unreal.EnumStructService.add_enum_value(enum_path, "Sword", "Melee Sword")
unreal.EnumStructService.add_enum_value(enum_path, "Bow", "Ranged Bow")
unreal.EnumStructService.add_enum_value(enum_path, "Staff", "Magic Staff")

# Verify
values = unreal.EnumStructService.get_enum_values(enum_path)
print(f"Values: {values}")  # ['Sword', 'Bow', 'Staff']
```

### Create UserDefinedStruct

```python
import unreal

# Create a new struct (F prefix added automatically)
struct_path = unreal.EnumStructService.create_struct("/Game/Data/Structs", "WeaponData")
print(f"Created: {struct_path}")

# Add properties
unreal.EnumStructService.add_struct_property(struct_path, "WeaponName", "FString", "Unnamed")
unreal.EnumStructService.add_struct_property(struct_path, "Damage", "float", "10.0")
unreal.EnumStructService.add_struct_property(struct_path, "AttackSpeed", "float", "1.0")
unreal.EnumStructService.add_struct_property(struct_path, "WeaponType", "EWeaponType")  # Use our enum

# Add array property
unreal.EnumStructService.add_struct_property(struct_path, "SpecialEffects", "FName", "", "Array")

# Verify — get_struct_info returns a single StructInfo (or None), NOT a (success, info) tuple
info = unreal.EnumStructService.get_struct_info(struct_path)
if info:
    print(f"Properties: {info.property_count}")
    for prop in info.properties:
        print(f"  {prop.name}: {prop.type} (default: {prop.default_value}, guid: {prop.guid})")
```

Notes:

- `prop.name` is the **authored name** ("Damage") and all mutation methods
  (`rename_struct_property`, `set_struct_property_default`, `remove_struct_property`,
  `change_struct_property_type`) accept authored names. GUIDs live per property
  (`StructPropertyInfo.guid`) — `StructInfo` itself has no `guid` field.
- New structs are seeded with a placeholder bool (`MemberVar_0`); the first
  `add_struct_property` call removes it automatically. If you ever see a stray
  `MemberVar_*` bool in an older struct, drop it with
  `remove_struct_property(path, "MemberVar_0")`.

### Discover Enums and Structs

```python
import unreal

# Search all enums containing "Weapon"
enums = unreal.EnumStructService.search_enums("Weapon")
for e in enums:
    print(f"{e.name} ({e.value_count} values) - UserDefined: {e.is_user_defined}")

# Search user-defined structs only
structs = unreal.EnumStructService.search_structs("Data", True)  # bUserDefinedOnly=True
for s in structs:
    print(f"{s.name} ({s.property_count} properties)")

# Get detailed enum info — returns a single EnumInfo (or None), NOT a (success, info) tuple
enum_info = unreal.EnumStructService.get_enum_info("EWeaponType")
if enum_info:
    for v in enum_info.values:
        print(f"  {v.name} = {v.value} ({v.display_name})")
```

### Modify Existing Enum

```python
import unreal

enum_path = "/Game/Data/Enums/EWeaponType.EWeaponType"

# Rename a value
unreal.EnumStructService.rename_enum_value(enum_path, "Sword", "LongSword")

# Change display name
unreal.EnumStructService.set_enum_value_display_name(enum_path, "Bow", "Composite Bow")

# Remove a value
unreal.EnumStructService.remove_enum_value(enum_path, "Staff")

# Add new value
unreal.EnumStructService.add_enum_value(enum_path, "Crossbow", "Heavy Crossbow")
```

### Modify Existing Struct

```python
import unreal

struct_path = "/Game/Data/Structs/FWeaponData.FWeaponData"

# Rename a property
unreal.EnumStructService.rename_struct_property(struct_path, "Damage", "BaseDamage")

# Change property type — ⚠️ this RESETS the default value to the new type's zero;
# re-set the default afterwards if you need one
unreal.EnumStructService.change_struct_property_type(struct_path, "BaseDamage", "int32")

# Set default value
unreal.EnumStructService.set_struct_property_default(struct_path, "BaseDamage", "15")

# Remove a property
unreal.EnumStructService.remove_struct_property(struct_path, "SpecialEffects")

# Add new property
unreal.EnumStructService.add_struct_property(struct_path, "CriticalMultiplier", "float", "2.0")
```

### Use Struct in a DataTable

After creating a UserDefinedStruct, it becomes available as a DataTable row type. The struct must
contain at least one property before it can be used as a row type.

```python
import unreal

# Create the row struct with EnumStructService
struct_path = unreal.EnumStructService.create_struct("/Game/Data", "ItemRow")
unreal.EnumStructService.add_struct_property(struct_path, "ItemName", "FString")
unreal.EnumStructService.add_struct_property(struct_path, "Value", "int32", "0")
unreal.EnumStructService.add_struct_property(struct_path, "Icon", "UTexture2D")
```

DataTable creation itself is no longer a VibeUE service — it is owned by the engine's native asset
toolset. Create the DataTable with the engine **`AssetTools`** toolset via `call_tool` (pass the
`FItemRow` struct as the row structure); run `describe_toolset` on `AssetTools` for the exact action
name and parameters. You can also create one in raw Python via `unreal.AssetToolsHelpers` +
`unreal.DataTableFactory` if you prefer `execute_python_code`.

---

## Available Property Types

When adding struct properties, use these type strings:

### Basic Types
- `bool`, `int32`, `int64`, `float`, `double`
- `FString`, `FName`, `FText`, `uint8`, `byte`

### Common Structs
- `FVector`, `FVector2D`, `FRotator`, `FTransform`
- `FColor`, `FLinearColor`
- `FGameplayTag`, `FGameplayTagContainer`

### Object References
- `AActor`, `APawn`, `ACharacter`
- `UTexture2D`, `UMaterial`, `UStaticMesh`
- `USoundBase`, `UAnimSequence`

### Containers
Use the `ContainerType` parameter:
```python
# Array of strings
unreal.EnumStructService.add_struct_property(path, "Tags", "FString", "", "Array")

# Set of names
unreal.EnumStructService.add_struct_property(path, "UniqueNames", "FName", "", "Set")

# Map of string to int
unreal.EnumStructService.add_struct_property(path, "Scores", "int32", "", "Map")
```

---

## Troubleshooting

### "Enum not found" Error
- Check if using full path vs just name
- Use `search_enums()` to find the correct path
- Ensure the asset is saved

### "Cannot modify native enum/struct"
- Only UserDefinedEnum/UserDefinedStruct assets can be modified
- Native C++ types from engine/plugins are read-only
- Check `is_user_defined` property before modifying

### Property Type Not Recognized
- Use `BlueprintService.search_variable_types()` to find valid type names
- Ensure enum/struct types exist before referencing them

## Sample scripts (run via `execute_python_code`)

- **`scripts/create_enum_struct.txt`** — create a UserDefinedEnum (+values) and a UserDefinedStruct.
