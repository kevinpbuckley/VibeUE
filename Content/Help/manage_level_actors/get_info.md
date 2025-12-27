# get_info

Get detailed information about a specific actor in the level.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ActorName | string | Yes | Name of the actor to inspect |

## Examples

### Get Actor Info
```json
{
  "Action": "get_info",
  "ParamsJson": "{\"ActorName\": \"BP_Player_C_0\"}"
}
```

## Returns

```json
{
  "Success": true,
  "Actor": {
    "Name": "BP_Player_C_0",
    "Class": "BP_Player_C",
    "Label": "Player Character",
    "Transform": {
      "Location": {"X": 100, "Y": 0, "Z": 100},
      "Rotation": {"Pitch": 0, "Yaw": 90, "Roll": 0},
      "Scale": {"X": 1, "Y": 1, "Z": 1}
    },
    "Folder": "Gameplay",
    "Tags": ["Player", "Pawn"],
    "IsHidden": false,
    "IsSpatiallyLoaded": true,
    "Parent": null,
    "Children": [],
    "Components": [
      "CapsuleComponent",
      "CharacterMovement",
      "Mesh",
      "Camera"
    ]
  }
}
```

## Tips

- Shows complete transform information
- Lists attached children and parent
- Shows component names for the actor
- Tags can be used for gameplay queries
- Use this to understand an actor before modifying it
