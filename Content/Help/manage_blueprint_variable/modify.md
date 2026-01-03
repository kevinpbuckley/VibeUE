# modify

Modify properties of an existing variable in a Blueprint. All fields returned by the `get_info` action can be modified.

**Recommended workflow:** Use `get_info` first to discover available properties and current values, then use `modify` to change them.

## Parameters

All parameters are optional except BlueprintPath and VariableName. Use `get_info` to see current values.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| VariableName | string | Yes | Name of the variable to modify |
| NewName | string | No | New name for the variable |
| DefaultValue | any | No | New default value |
| Category | string | No | New category for organization |
| Tooltip | string | No | Set tooltip description |
| replication_condition | string | No | Replication setting: "None", "Replicated", or "RepNotify" |
| is_blueprint_read_only | boolean | No | Make variable read-only in Blueprints |
| is_editable_in_details | boolean | No | Allow editing in Details panel (instance editable) |
| is_private | boolean | No | Mark variable as private |
| is_expose_on_spawn | boolean | No | Expose variable when spawning actor |
| is_expose_to_cinematics | boolean | No | Allow Sequencer/Cinematics to animate this variable |

## Examples

### Change Default Value
```json
{
  "Action": "modify",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"VariableName\": \"Health\", \"DefaultValue\": 150}"
}
```

### Rename Variable
```json
{
  "Action": "modify",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"VariableName\": \"HP\", \"NewName\": \"Health\"}"
}
```

### Make Instance Editable
```json
{
  "Action": "modify",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Enemy\", \"VariableName\": \"Damage\", \"IsInstanceEditable\": true, \"Category\": \"Combat\"}"
}
```

### Add Tooltip
```json
{
  "Action": "modify",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Weapon\", \"VariableName\": \"FireRate\", \"Tooltip\": \"Rounds per minute\"}"
}
```

### Enable Replication
```json
{
  "Action": "modify",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"VariableName\": \"Score\", \"ReplicationCondition\": \"Replicated\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "VariableName": "Health",
  "ModifiedProperties": ["DefaultValue"],
  "Message": "Variable modified successfully"
}
```

## Tips

- **Always use `get_info` first** to see all available properties and their current values
- Any field in the Variable object from `get_info` can be modified here
- Renaming updates all Get/Set nodes referencing the variable
- Changing the type is not supported - delete and recreate instead
- is_editable_in_details makes the variable show in the Details panel for level instances
- is_expose_on_spawn allows setting the value when using SpawnActor
- replication_condition values: "None" (no replication), "Replicated" (always replicate), "RepNotify" (replicate with notification callback)
- Compile after modifications to validate changes
- Tooltips appear when hovering over the variable in the Blueprint editor
