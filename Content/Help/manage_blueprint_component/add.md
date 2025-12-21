# add

Add a new component to a Blueprint actor.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| ComponentClass | string | Yes | Class type of the component to add |
| ComponentName | string | Yes | Unique name for the new component |
| ParentComponent | string | No | Name of parent component (attaches to root if not specified) |

## Examples

### Add Static Mesh Component
```json
{
  "Action": "add",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Pickup\", \"ComponentClass\": \"StaticMeshComponent\", \"ComponentName\": \"PickupMesh\"}"
}
```

### Add Component with Parent
```json
{
  "Action": "add",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Weapon\", \"ComponentClass\": \"PointLightComponent\", \"ComponentName\": \"MuzzleLight\", \"ParentComponent\": \"WeaponMesh\"}"
}
```

### Add Box Collision
```json
{
  "Action": "add",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Trigger\", \"ComponentClass\": \"BoxComponent\", \"ComponentName\": \"TriggerBox\"}"
}
```

### Add Audio Component
```json
{
  "Action": "add",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Ambient\", \"ComponentClass\": \"AudioComponent\", \"ComponentName\": \"AmbientSound\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Pickup",
  "ComponentName": "PickupMesh",
  "ComponentClass": "StaticMeshComponent",
  "Message": "Component added successfully"
}
```

## Tips

- Common component classes: StaticMeshComponent, SkeletalMeshComponent, BoxComponent, SphereComponent, CapsuleComponent, PointLightComponent, SpotLightComponent, AudioComponent, ParticleSystemComponent, CameraComponent
- Use `get_available` action to see all available component types
- Component names must be unique within the Blueprint
- Child components inherit transform from their parent
- Compile the Blueprint after adding components
