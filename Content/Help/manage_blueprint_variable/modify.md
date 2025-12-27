# modify

Modify properties of an existing variable in a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| VariableName | string | Yes | Name of the variable to modify |
| NewName | string | No | New name for the variable |
| DefaultValue | any | No | New default value |
| Category | string | No | New category |
| IsInstanceEditable | boolean | No | Change instance editable setting |
| IsExposeOnSpawn | boolean | No | Change expose on spawn setting |
| ReplicationCondition | string | No | Change replication setting |
| Tooltip | string | No | Set tooltip description |

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

- Renaming updates all Get/Set nodes referencing the variable
- Changing the type is not supported - delete and recreate instead
- Instance editable makes the variable show in the Details panel
- Expose on spawn allows setting the value when using SpawnActor
- Compile after modifications to validate changes
- Tooltips appear when hovering over the variable in the editor
