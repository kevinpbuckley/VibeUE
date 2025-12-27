# help

Show help information for this tool or a specific action.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| help_action | string | No | Specific action to get detailed help for |

## Examples

Get tool overview:
```json
{"action": "help"}
```

Get help for a specific action:
```json
{"action": "help", "help_action": "create"}
```

## Returns

- `tool`: Tool name
- `summary`: Tool description
- `actions`: Array of available actions with descriptions
- `total_actions`: Number of available actions

When `help_action` is specified:
- `action`: Action name
- `description`: What the action does
- `parameters`: Array of parameter definitions
- `example`: Example JSON call
