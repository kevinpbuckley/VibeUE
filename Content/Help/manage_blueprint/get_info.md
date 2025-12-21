# get_info

Get detailed information about a Blueprint asset.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |

## Examples

### Get Blueprint Info
```json
{
  "Action": "get_info",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "ParentClass": "Character",
  "GeneratedClass": "BP_Player_C",
  "IsCompiled": true,
  "Components": [
    {
      "Name": "CapsuleComponent",
      "Class": "CapsuleComponent"
    },
    {
      "Name": "CharacterMovement",
      "Class": "CharacterMovementComponent"
    },
    {
      "Name": "Mesh",
      "Class": "SkeletalMeshComponent"
    }
  ],
  "Variables": [
    {
      "Name": "Health",
      "Type": "Float"
    },
    {
      "Name": "MaxHealth",
      "Type": "Float"
    }
  ],
  "Functions": [
    "BeginPlay",
    "Tick",
    "TakeDamage"
  ]
}
```

## Tips

- Use this to discover the structure of existing Blueprints
- Components list includes both inherited and added components
- Variables show custom variables defined in the Blueprint
- Functions list includes event overrides and custom functions
- Check `IsCompiled` status before relying on generated class
