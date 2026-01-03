# get_property_options

Discover available options for a specific blueprint variable property. Use this to find valid values before modifying properties.

**Use this when you need to know:** What values are valid for a property (e.g., what replication conditions exist?)

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| property_name | string | Yes | Name of the property to get options for (e.g., "replication_condition") |

## Examples

### Get Replication Options
```json
{
  "Action": "get_property_options",
  "ParamsJson": "{\"property_name\": \"replication_condition\"}"
}
```

### Get Boolean Property Options
```json
{
  "Action": "get_property_options",
  "ParamsJson": "{\"property_name\": \"is_blueprint_read_only\"}"
}
```

## Returns

```json
{
  "success": true,
  "data": {
    "property_name": "replication_condition",
    "options": ["None", "Replicated", "RepNotify"],
    "description": "Replication condition controls how the variable is synchronized in multiplayer. None = not replicated, Replicated = always synchronized, RepNotify = synchronized with notification callback."
  }
}
```

## Supported Properties

- **replication_condition**: Returns ["None", "Replicated", "RepNotify"]
- **is_blueprint_read_only**: Returns [true, false]
- **is_editable_in_details**: Returns [true, false]
- **is_private**: Returns [true, false]
- **is_expose_on_spawn**: Returns [true, false]
- **is_expose_to_cinematics**: Returns [true, false]

## Tips

- Use this action when you need to set a property but don't know the valid values
- Replication options are especially important for multiplayer games
- Boolean properties (is_*) always return [true, false]
- This eliminates guessing - discover the exact enum values the system uses
