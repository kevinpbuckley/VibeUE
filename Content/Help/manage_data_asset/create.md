# create

Create a new data asset instance of a specified type.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `class_name` | string | Yes | The UDataAsset subclass name (use `search_types` to find) |
| `asset_path` | string | No | Destination folder path (default: `/Game/Data`) |
| `asset_name` | string | Yes* | Name for the new asset (*can be included in asset_path) |
| `properties` | object | No | Initial property values to set |

## Returns

```json
{
  "success": true,
  "message": "Created data asset: /Game/Data/Items/DA_Sword",
  "asset_path": "/Game/Data/Items/DA_Sword.DA_Sword",
  "asset_name": "DA_Sword",
  "class_name": "MyItemDataAsset"
}
```

## Examples

```python
# Basic creation
manage_data_asset(
    action="create",
    class_name="MyItemDataAsset",
    asset_path="/Game/Data/Items",
    asset_name="DA_Sword"
)

# With path including name
manage_data_asset(
    action="create",
    class_name="MyItemDataAsset",
    asset_path="/Game/Data/Items/DA_Sword"
)

# With initial properties
manage_data_asset(
    action="create",
    class_name="MyItemDataAsset",
    asset_path="/Game/Data/Items",
    asset_name="DA_Sword",
    properties={
        "ItemName": "Iron Sword",
        "Damage": 25,
        "Weight": 3.5
    }
)
```

## Notes

- Use `search_types` first to discover valid class names
- The asset is marked dirty but not saved - call `save` or `manage_asset(action="save_all")` afterwards
- Initial properties are optional - you can set them later with `set_property` or `set_properties`
