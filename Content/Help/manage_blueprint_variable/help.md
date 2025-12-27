# manage_blueprint_variable

Manage variables within Blueprints - create, delete, modify, and query Blueprint variables.

## Summary

The `manage_blueprint_variable` tool provides comprehensive variable management for Blueprint assets. Variables store data that can be accessed and modified throughout the Blueprint. This tool lets you create new variables, configure their properties, and manage existing ones.

## Actions

| Action | Description |
|--------|-------------|
| help | Get help information about variable management |
| search_types | Search for available variable types |
| create | Create a new variable in a Blueprint |
| delete | Delete a variable from a Blueprint |
| get_info | Get detailed information about a variable |
| list | List all variables in a Blueprint |
| modify | Modify variable properties (type, default value, etc.) |

## Usage

### Create a Variable
```json
{
  "Action": "create",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"VariableName\": \"Health\", \"VariableType\": \"Float\", \"DefaultValue\": 100}"
}
```

### List Variables
```json
{
  "Action": "list",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\"}"
}
```

### Search Variable Types
```json
{
  "Action": "search_types",
  "ParamsJson": "{\"SearchTerm\": \"Vector\"}"
}
```

## Notes

- Variable names must be unique within a Blueprint
- Compile after variable changes
- Variables can be exposed to the editor for instance customization
- Use categories to organize variables in complex Blueprints
