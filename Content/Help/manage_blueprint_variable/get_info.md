# get_info

Get complete information about a specific variable in a Blueprint. This action returns ALL properties that can be modified using the `modify` action.

**Use this action first** to discover what properties are available and their current values before making modifications.

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

- **Discovery pattern:** Always use `get_info` before `modify` to see all available properties
- Shows ALL configuration options including: replication_condition, is_blueprint_read_only, is_editable_in_details, is_private, is_expose_on_spawn, is_expose_to_cinematics, tooltip, category, and metadata
- Any field returned in the Variable object can be changed using the `modify` action
- Useful for understanding variable setup before making changes
- Check replication_condition for multiplayer Blueprints (None/Replicated/RepNotify)
