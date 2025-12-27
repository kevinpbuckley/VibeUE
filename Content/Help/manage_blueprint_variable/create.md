# create

Create a new variable in a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| VariableName | string | Yes | Name for the new variable |
| VariableType | string | Yes | Type of the variable (e.g., "Float", "Integer", "Boolean", "Vector") |
| DefaultValue | any | No | Default value for the variable |
| Category | string | No | Category for organizing in the variable list |
| IsInstanceEditable | boolean | No | Whether the variable can be edited per instance (default: false) |
| IsExposeOnSpawn | boolean | No | Whether the variable is exposed when spawning (default: false) |
| ReplicationCondition | string | No | Replication setting for multiplayer ("None", "Replicated", "RepNotify") |

## Examples

### Create Float Variable
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"VariableName\": \"Health\", \"VariableType\": \"Float\", \"DefaultValue\": 100}"
}
```

### Create Instance Editable Variable
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Enemy\", \"VariableName\": \"MaxHealth\", \"VariableType\": \"Float\", \"DefaultValue\": 50, \"IsInstanceEditable\": true, \"Category\": \"Stats\"}"
}
```

### Create Object Reference Variable
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Weapon\", \"VariableName\": \"Owner\", \"VariableType\": \"Actor\"}"
}
```

### Create Replicated Variable
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"VariableName\": \"Score\", \"VariableType\": \"Integer\", \"DefaultValue\": 0, \"ReplicationCondition\": \"Replicated\"}"
}
```

### Create Array Variable
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Inventory\", \"VariableName\": \"Items\", \"VariableType\": \"Array<Object>\"}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "VariableName": "Health",
  "VariableType": "Float",
  "DefaultValue": 100,
  "Message": "Variable created successfully"
}
```

## Tips

- Variable names should use PascalCase convention
- Boolean variables conventionally start with 'b' prefix (e.g., bIsAlive)
- Instance editable variables show in the Details panel when selecting actors
- Replicated variables sync across network in multiplayer
- RepNotify triggers a function when the value changes on clients
- Array syntax: Array<ElementType> (e.g., Array<Integer>, Array<Actor>)
