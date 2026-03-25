---
name: enum-struct
display_name: Enums and Structs
description: Create, modify, and introspect UserDefinedEnums and UserDefinedStructs
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
success, info = unreal.EnumStructService.get_enum_info("EMyEnum")
if info.b_is_user_defined:
    unreal.EnumStructService.add_enum_value("EMyEnum", "NewValue")
```

### Naming Conventions

- Enums should start with `E` prefix (e.g., `EWeaponType`)
- Structs should start with `F` prefix (e.g., `FWeaponData`)
- The service auto-adds prefixes if missing

---

## Workflows

### Create UserDefinedEnum

```python
import unreal

# Create a new enum (E prefix added automatically)
enum_path = unreal.EnumStructService.create_enum("/Game/Data/Enums", "WeaponType")
print(f"Created: {enum_path}")  # /Game/Data/Enums/EWeaponType.EWeaponType

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

# Verify
success, info = unreal.EnumStructService.get_struct_info(struct_path)
print(f"Properties: {info.property_count}")
for prop in info.properties:
    print(f"  {prop.name}: {prop.type}")
```

### Discover Enums and Structs

```python
import unreal

# Search all enums containing "Weapon"
enums = unreal.EnumStructService.search_enums("Weapon")
for e in enums:
    print(f"{e.name} ({e.value_count} values) - UserDefined: {e.b_is_user_defined}")

# Search user-defined structs only
structs = unreal.EnumStructService.search_structs("Data", True)  # bUserDefinedOnly=True
for s in structs:
    print(f"{s.name} ({s.property_count} properties)")

# Get detailed enum info
success, enum_info = unreal.EnumStructService.get_enum_info("EWeaponType")
if success:
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

# Change property type
unreal.EnumStructService.change_struct_property_type(struct_path, "BaseDamage", "int32")

# Set default value
unreal.EnumStructService.set_struct_property_default(struct_path, "BaseDamage", "15")

# Remove a property
unreal.EnumStructService.remove_struct_property(struct_path, "SpecialEffects")

# Add new property
unreal.EnumStructService.add_struct_property(struct_path, "CriticalMultiplier", "float", "2.0")
```

### Use Struct in DataTable

After creating a struct, you can use it as a row type for DataTables:

```python
import unreal

# First create the struct
struct_path = unreal.EnumStructService.create_struct("/Game/Data", "ItemRow")
unreal.EnumStructService.add_struct_property(struct_path, "ItemName", "FString")
unreal.EnumStructService.add_struct_property(struct_path, "Value", "int32", "0")
unreal.EnumStructService.add_struct_property(struct_path, "Icon", "UTexture2D")

# Search for it as a row type
row_types = unreal.DataTableService.search_row_types("ItemRow")
if row_types:
    print(f"Found row type: {row_types[0].name}")

    # Create a DataTable using this struct
    dt_path = unreal.DataTableService.create_data_table("FItemRow", "/Game/Data", "DT_Items")
    print(f"Created DataTable: {dt_path}")
```

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
- Check `b_is_user_defined` property before modifying

### Property Type Not Recognized
- Use `BlueprintService.search_variable_types()` to find valid type names
- Ensure enum/struct types exist before referencing them
