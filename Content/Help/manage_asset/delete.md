# delete

Delete an asset from the Unreal Engine project.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| AssetPath | string | Yes | Content path to the asset to delete |
| Force | boolean | No | Force deletion even if asset has references (default: false) |

## Examples

### Delete Single Asset
```json
{
  "Action": "delete",
  "ParamsJson": "{\"AssetPath\": \"/Game/Blueprints/BP_OldCharacter\"}"
}
```

### Force Delete
```json
{
  "Action": "delete",
  "ParamsJson": "{\"AssetPath\": \"/Game/Materials/M_Deprecated\", \"Force\": true}"
}
```

## Returns

### Success
```json
{
  "Success": true,
  "Message": "Asset deleted successfully"
}
```

### Failure (Has References)
```json
{
  "Success": false,
  "Message": "Cannot delete asset: 3 other assets reference it",
  "References": [
    "/Game/Maps/Level_01",
    "/Game/Blueprints/BP_Player",
    "/Game/Materials/MI_Character"
  ]
}
```

## Tips

- Always use `list_references` before deleting to check for dependencies
- Force deletion can cause broken references in other assets
- Deleted assets are moved to the system trash and can potentially be recovered
- Consider using redirectors when renaming rather than deleting and recreating
- Save the project after deletion to persist changes
