# manage_blueprint_variable

Manage variables within Blueprints - create, delete, modify, and query Blueprint variables.

## Summary

The `manage_blueprint_variable` tool provides comprehensive variable management for Blueprint assets. Variables store data that can be accessed and modified throughout the Blueprint. This tool lets you create new variables, configure their properties, and manage existing ones.

**Important workflow:** Use `get_info` to discover all available properties and their current values, then use `modify` to change any of those properties. The `get_info` action returns a complete Variable object with all modifiable fields including replication settings, instance editability, tooltips, categories, and more.

## Actions

| Action | Description |
|--------|-------------|
| help | Get help information about variable management |
| search_types | Search for available variable types |
| create | Create a new variable in a Blueprint |
| delete | Delete a variable from a Blueprint |
| get_info | Get complete information about a variable - returns all properties that can be modified |
| get_property_options | Discover available options for a specific property (e.g., replication_condition values) |
| list | List all variables in a Blueprint |
| modify | Modify any variable property returned by get_info (replication, default value, tooltip, etc.) |

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
