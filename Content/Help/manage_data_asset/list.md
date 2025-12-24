# list

List existing data asset instances in the project.

## Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `class_name` | string | No | Filter by specific data asset type |
| `path` | string | No | Filter by path prefix (default: `/Game`) |

## Returns

```json
{
  "success": true,
  "assets": [
    {
      "name": "DA_Sword",
      "path": "/Game/Data/Items/DA_Sword.DA_Sword",
      "class": "MyItemDataAsset"
    }
  ],
  "count": 1
}
```

## Examples

```python
# List all data assets
manage_data_asset(action="list")

# List only items
manage_data_asset(action="list", class_name="MyItemDataAsset")

# List in specific folder
manage_data_asset(action="list", path="/Game/Data/Items")

# Combined filter
manage_data_asset(
    action="list",
    class_name="MyItemDataAsset",
    path="/Game/Data/Weapons"
)
```

## Notes

- Returns all UDataAsset subclass instances by default
- Use `class_name` to narrow down to a specific type
- The `path` field in response contains the full object path
