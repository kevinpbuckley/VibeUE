# remove

Remove a component from a Blueprint actor.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| ComponentName | string | Yes | Name of the component to remove |

## Examples

### Remove Component
```json
{
  "Action": "remove",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"ComponentName\": \"OldMeshComponent\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "ComponentName": "OldMeshComponent",
  "Message": "Component removed successfully"
}
```

## Tips

- Cannot remove inherited components from parent classes
- Removing a parent component will also remove all child components
- Blueprint nodes referencing the removed component will become invalid
- Compile after removing components to validate the Blueprint
- Consider the impact on existing Blueprint logic before removing
