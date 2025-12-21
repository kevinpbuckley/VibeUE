# get_info

Get detailed information about a specific component within a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| ComponentName | string | Yes | Name of the component to inspect |

## Examples

### Get Component Info
```json
{
  "Action": "get_info",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"ComponentName\": \"CharacterMesh\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "Component": {
    "Name": "CharacterMesh",
    "Class": "SkeletalMeshComponent",
    "Parent": "CapsuleComponent",
    "IsInherited": false,
    "RelativeLocation": {"X": 0, "Y": 0, "Z": -90},
    "RelativeRotation": {"Pitch": 0, "Yaw": -90, "Roll": 0},
    "RelativeScale": {"X": 1, "Y": 1, "Z": 1},
    "ChildCount": 2,
    "Children": ["WeaponAttachPoint", "CameraArm"]
  }
}
```

## Tips

- Use this to get detailed information about a specific component
- Shows whether the component is inherited from a parent class
- Includes transform information and child component list
- For full property listing, use `get_all_properties` instead
