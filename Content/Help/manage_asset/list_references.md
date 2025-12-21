# list_references

List all assets that reference or are referenced by a specific asset.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| AssetPath | string | Yes | Content path to the asset to check references for |
| Direction | string | No | "Referencers" (what uses this), "Dependencies" (what this uses), or "Both" (default: "Both") |

## Examples

### Get All References
```json
{
  "Action": "list_references",
  "ParamsJson": "{\"AssetPath\": \"/Game/Materials/M_Character\"}"
}
```

### Get Only Referencers
```json
{
  "Action": "list_references",
  "ParamsJson": "{\"AssetPath\": \"/Game/Textures/T_Wood_Diffuse\", \"Direction\": \"Referencers\"}"
}
```

### Get Only Dependencies
```json
{
  "Action": "list_references",
  "ParamsJson": "{\"AssetPath\": \"/Game/Blueprints/BP_Weapon\", \"Direction\": \"Dependencies\"}"
}
```

## Returns

```json
{
  "Success": true,
  "AssetPath": "/Game/Materials/M_Character",
  "Referencers": [
    "/Game/Characters/SK_Hero",
    "/Game/Blueprints/BP_Player"
  ],
  "Dependencies": [
    "/Game/Textures/T_Character_Diffuse",
    "/Game/Textures/T_Character_Normal",
    "/Game/Textures/T_Character_Roughness"
  ]
}
```

## Tips

- Check references before deleting assets to avoid broken dependencies
- Useful for understanding asset relationships in large projects
- Referencers are assets that depend on this asset
- Dependencies are assets that this asset depends on
- Circular references can exist between assets
