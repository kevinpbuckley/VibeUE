# get_hierarchy

Get the full component hierarchy tree of a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |

## Examples

### Get Hierarchy
```json
{
  "Action": "get_hierarchy",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Vehicle\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Vehicle",
  "Hierarchy": {
    "Name": "DefaultSceneRoot",
    "Class": "SceneComponent",
    "Children": [
      {
        "Name": "VehicleMesh",
        "Class": "SkeletalMeshComponent",
        "Children": [
          {
            "Name": "DriverSeat",
            "Class": "SceneComponent",
            "Children": []
          },
          {
            "Name": "Headlights",
            "Class": "SpotLightComponent",
            "Children": []
          }
        ]
      },
      {
        "Name": "VehicleMovement",
        "Class": "WheeledVehicleMovementComponent",
        "Children": []
      }
    ]
  }
}
```

## Tips

- The hierarchy shows parent-child relationships between components
- Root component is typically DefaultSceneRoot or inherited from parent class
- Child components inherit transforms from their parents
- Use this to understand the Blueprint structure before making modifications
- Inherited components are included in the hierarchy
