# reparent

Change a component's parent in the hierarchy, moving it to be a child of another component.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| ComponentName | string | Yes | Name of the component to move |
| NewParent | string | Yes | Name of the new parent component |

## Examples

### Reparent to Different Component
```json
{
  "Action": "reparent",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Vehicle\", \"ComponentName\": \"Headlight\", \"NewParent\": \"VehicleMesh\"}"
}
```

### Move to Root
```json
{
  "Action": "reparent",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Actor\", \"ComponentName\": \"FloatingWidget\", \"NewParent\": \"DefaultSceneRoot\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Vehicle",
  "ComponentName": "Headlight",
  "OldParent": "DefaultSceneRoot",
  "NewParent": "VehicleMesh",
  "Message": "Component reparented successfully"
}
```

## Tips

- Child components inherit transform from their parent
- Reparenting preserves the component's relative transform
- Cannot create circular parent-child relationships
- All children of the moved component move with it
- Use `get_hierarchy` to understand the current structure before reparenting
