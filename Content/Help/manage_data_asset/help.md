# manage_data_asset

## Overview

The `manage_data_asset` tool provides comprehensive management for UDataAsset instances in your Unreal Engine project. It uses reflection to discover available DataAsset classes and read/write properties on any DataAsset type, making it perfect for working with game configuration data, tables, and custom data structures.

## Available Actions

| Action | Description | Required Params |
|--------|-------------|-----------------|
| `help` | Get help for this tool or a specific action | - |
| `search_types` | Find available UDataAsset subclasses | - |
| `list` | List data asset instances | - |
| `create` | Create a new data asset | `class_name` |
| `get_info` | Get complete data asset info with all properties | `asset_path` |
| `list_properties` | List available properties on an asset or class | `asset_path` or `class_name` |
| `get_property` | Get a specific property value | `asset_path`, `property_name` |
| `set_property` | Set a property value | `asset_path`, `property_name`, `property_value` |
| `set_properties` | Set multiple properties at once | `asset_path`, `properties` |
| `get_class_info` | Get class definition info | `class_name` |

**Note:** For delete, duplicate, and save operations, use `manage_asset` tool instead - it works with all asset types including data assets.

## Quick Start

### 1. Discover Available Data Asset Types
```python
# Find all data asset classes in your project
manage_data_asset(action="search_types")

# Search for specific types
manage_data_asset(action="search_types", search_filter="Item")
```

### 2. Create a Data Asset
```python
# Create using discovered class name
manage_data_asset(
    action="create",
    class_name="UMyItemDataAsset",
    asset_path="/Game/Data/Items",
    asset_name="DA_Sword"
)
```

### 3. Read and Modify Properties
```python
# Get all properties and values
manage_data_asset(action="get_info", asset_path="/Game/Data/Items/DA_Sword")

# Get a specific property
manage_data_asset(
    action="get_property",
    asset_path="/Game/Data/Items/DA_Sword",
    property_name="ItemName"
)

# Set a property
manage_data_asset(
    action="set_property",
    asset_path="/Game/Data/Items/DA_Sword",
    property_name="ItemName",
    property_value="Legendary Sword"
)

# Set multiple properties at once
manage_data_asset(
    action="set_properties",
    asset_path="/Game/Data/Items/DA_Sword",
    properties={"ItemName": "Legendary Sword", "Damage": 50, "Weight": 5.5}
)
```

### 4. Save Changes
```python
# Use manage_asset to save data assets
manage_asset(action="save", asset_path="/Game/Data/Items/DA_Sword")

# Or save all dirty assets at once
manage_asset(action="save_all")
```

## Common Workflows

### Creating Game Configuration Data
```python
# 1. Find your custom data asset class
manage_data_asset(action="search_types", search_filter="GameConfig")

# 2. Create the asset
manage_data_asset(
    action="create",
    class_name="UGameConfigDataAsset",
    asset_path="/Game/Data",
    asset_name="DA_GameSettings"
)

# 3. Configure it
manage_data_asset(
    action="set_properties",
    asset_path="/Game/Data/DA_GameSettings",
    properties={
        "MaxPlayers": 4,
        "RoundDuration": 300.0,
        "GameModeName": "Team Deathmatch"
    }
)

# 4. Save
manage_data_asset(action="save", asset_path="/Game/Data/DA_GameSettings")
```

### Duplicating and Modifying Data
```python
# Use manage_asset to duplicate any asset (including data assets)
manage_asset(
    action="duplicate",
    asset_path="/Game/Data/Items/DA_Sword",
    destination_path="/Game/Data/Items",
    new_name="DA_FireSword"
)

# Then use manage_data_asset to modify the copy's properties
manage_data_asset(
    action="set_properties",
    asset_path="/Game/Data/Items/DA_FireSword",
    properties={
        "ItemName": "Fire Sword",
        "ElementType": "Fire",
        "BonusDamage": 25
    }
)
```

## Supported Property Types

The tool supports reading and writing:

- **Primitives**: `int32`, `int64`, `float`, `double`, `bool`, `uint8`
- **Strings**: `FString`, `FName`, `FText`
- **Enums**: Both enum values by name or numeric index
- **Object References**: Asset paths for `UObject*` properties
- **Soft References**: `TSoftObjectPtr<>` paths
- **Arrays**: `TArray<>` of supported types
- **Structs**: Nested structures (via JSON objects)
- **Maps**: `TMap<>` with string keys

## Tips and Best Practices

1. **Use search_types first**: Always discover what DataAsset classes exist before trying to create instances
2. **Check list_properties**: Before setting values, check what properties exist and their types
3. **Use set_properties for batch updates**: More efficient than multiple set_property calls
4. **Save after modifications**: Changes are not persisted until you save
5. **Full paths**: Always use full package paths like `/Game/Data/MyAsset`

## Error Handling

Common errors and solutions:

| Error | Solution |
|-------|----------|
| "Could not find data asset class" | Use `search_types` to find available classes |
| "Property not found" | Check spelling with `list_properties` |
| "Could not find data asset" | Verify path, use `list` to find assets |
| "Property is not editable" | Some properties are read-only or private |

## Related Tools

- `manage_asset`: For general asset operations (search, open, save_all)
- `manage_blueprint`: For Blueprint creation and modification
