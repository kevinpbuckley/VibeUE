# duplicate

Create a copy of an existing asset in the project.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| SourcePath | string | Yes | Content path to the asset to duplicate |
| DestinationPath | string | Yes | Content path for the new duplicated asset |
| NewName | string | No | Name for the duplicated asset (derived from path if not provided) |

## Examples

### Duplicate to Same Folder
```json
{
  "Action": "duplicate",
  "ParamsJson": "{\"SourcePath\": \"/Game/Blueprints/BP_Enemy\", \"DestinationPath\": \"/Game/Blueprints/BP_Enemy_Copy\"}"
}
```

### Duplicate to Different Folder
```json
{
  "Action": "duplicate",
  "ParamsJson": "{\"SourcePath\": \"/Game/Materials/M_Metal\", \"DestinationPath\": \"/Game/Materials/Variants/M_Metal_Rusty\"}"
}
```

### Duplicate with Custom Name
```json
{
  "Action": "duplicate",
  "ParamsJson": "{\"SourcePath\": \"/Game/Meshes/SM_Crate\", \"DestinationPath\": \"/Game/Meshes/Props\", \"NewName\": \"SM_Crate_Large\"}"
}
```

## Returns

```json
{
  "Success": true,
  "NewAssetPath": "/Game/Blueprints/BP_Enemy_Copy",
  "Message": "Asset duplicated successfully"
}
```

## Tips

- Duplicated assets are independent copies - changes to one don't affect the other
- For Blueprints, the duplicate will have the same parent class and components
- Material instances duplicated will retain their parameter overrides
- Consider using this to create variations of existing assets
- The destination folder will be created if it doesn't exist
