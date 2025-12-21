# get_info

Get detailed information about a specific variable in a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| VariableName | string | Yes | Name of the variable to inspect |

## Examples

### Get Variable Info
```json
{
  "Action": "get_info",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"VariableName\": \"Health\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "Variable": {
    "Name": "Health",
    "Type": "Float",
    "DefaultValue": 100,
    "Category": "Stats",
    "IsInstanceEditable": true,
    "IsExposeOnSpawn": false,
    "IsPrivate": false,
    "ReplicationCondition": "None",
    "Tooltip": "Current health of the player",
    "IsArray": false,
    "IsSet": false,
    "IsMap": false
  }
}
```

## Tips

- Shows all configuration options for the variable
- Use this to understand variable setup before modifying
- Category helps organize variables in the Blueprint editor
- Check ReplicationCondition for multiplayer Blueprints
