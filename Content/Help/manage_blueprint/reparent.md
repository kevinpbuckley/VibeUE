# reparent

Change the parent class of an existing Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint to reparent |
| NewParentClass | string | Yes | New parent class name |

## Examples

### Change to Character Parent
```json
{
  "Action": "reparent",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_NPC\", \"NewParentClass\": \"Character\"}"
}
```

### Change to Custom Parent
```json
{
  "Action": "reparent",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Enemies/BP_Zombie\", \"NewParentClass\": \"/Game/Blueprints/BP_BaseEnemy\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_NPC",
  "OldParent": "Pawn",
  "NewParent": "Character",
  "Message": "Blueprint reparented successfully"
}
```

## Tips

- Reparenting may invalidate components from the old parent
- Custom variables and functions are preserved during reparenting
- Event overrides may become invalid if they don't exist in the new parent
- Always compile and test after reparenting
- You can reparent to another Blueprint class using its full path
- Consider the inheritance hierarchy carefully before reparenting
