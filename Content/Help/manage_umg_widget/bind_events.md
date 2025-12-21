# bind_events

Bind widget events to Blueprint functions.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| widget_name | string | Yes | Content path to the Widget Blueprint |
| input_mappings | array | Yes | Array of event-to-function mappings |

### input_mappings Array Format

Each item in `input_mappings` is an object with:

| Name | Type | Required | Description |
|------|------|----------|-------------|
| event_name | string | Yes | Name of the event to bind (e.g., "OnClicked", "OnHovered") |
| function_name | string | Yes | Name of the function to call |

## Examples

### Bind Button Click
```json
{
  "Action": "bind_events",
  "ParamsJson": "{\"widget_name\": \"/Game/UI/WBP_MainMenu\", \"input_mappings\": [{\"event_name\": \"OnClicked\", \"function_name\": \"HandlePlayButtonClicked\"}]}"
}
```

### Bind Multiple Events
```json
{
  "Action": "bind_events",
  "ParamsJson": "{\"widget_name\": \"/Game/UI/WBP_MainMenu\", \"input_mappings\": [{\"event_name\": \"OnClicked\", \"function_name\": \"HandlePlayClicked\"}, {\"event_name\": \"OnHovered\", \"function_name\": \"HandlePlayHovered\"}]}"
}
```

### Python Example
```python
# Bind single event
manage_umg_widget(action="bind_events", 
                  widget_name="/Game/Blueprints/MyWidget",
                  input_mappings=[{"event_name": "OnClicked", "function_name": "HandleClick"}])

# Bind multiple events at once
manage_umg_widget(action="bind_events",
                  widget_name="/Game/Blueprints/MyWidget", 
                  input_mappings=[
                      {"event_name": "OnClicked", "function_name": "HandleClick"},
                      {"event_name": "OnHovered", "function_name": "HandleHover"}
                  ])
```

## Returns

```json
{
  "success": true,
  "widget_name": "/Game/UI/WBP_MainMenu",
  "input_mappings": [...],
  "bindings_count": 2,
  "note": "Input events bound to widget functions successfully"
}
```

## Tips

- The function must exist in the Widget Blueprint before binding
- Create functions first using `manage_blueprint_function(action="create")`
- Function signature must match event parameters
- OnClicked has no parameters
- OnValueChanged passes the new value
- Save and compile the Widget Blueprint after binding

## Common Events

| Event | Widget Type | Parameters |
|-------|-------------|------------|
| OnClicked | Button | None |
| OnPressed | Button | None |
| OnReleased | Button | None |
| OnHovered | Button | None |
| OnUnhovered | Button | None |
| OnValueChanged | Slider, CheckBox | Value |
| OnTextChanged | EditableText | Text |
| OnTextCommitted | EditableText | Text, CommitMethod |
