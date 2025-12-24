# search_types

Discover available UDataAsset subclasses in the project. Use this action first to find what data asset types are available before creating instances.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `search_filter` | string | No | Filter results by name (case-insensitive) |

## Returns

```json
{
  "success": true,
  "types": [
    {
      "name": "MyItemDataAsset",
      "path": "/Script/MyGame.MyItemDataAsset",
      "module": "/Script/MyGame",
      "is_native": true,
      "parent_class": "UPrimaryDataAsset"
    }
  ],
  "count": 1
}
```

## Examples

```python
# Find all data asset types
manage_data_asset(action="search_types")

# Search for item-related types
manage_data_asset(action="search_types", search_filter="Item")

# Search for config types  
manage_data_asset(action="search_types", search_filter="Config")
```

## Notes

- Returns both C++ and Blueprint-defined DataAsset classes
- Excludes abstract and deprecated classes
- Use the `name` field when creating assets with `create` action
