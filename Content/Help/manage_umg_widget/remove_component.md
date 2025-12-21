# remove_component

Remove a widget component from a Widget Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| widget_name | string | Yes | Content path to the Widget Blueprint |
| component_name | string | Yes | Name of the component to remove |

## Examples

### Remove a Button
```python
manage_umg_widget(action="remove_component", widget_name="/Game/UI/WBP_MainMenu",
                  component_name="PlayButton")
```

### Remove a Container (and all children)
```python
# This will also remove all children of the container
manage_umg_widget(action="remove_component", widget_name="/Game/UI/WBP_MainMenu",
                  component_name="MenuContainer")
```

## Returns

```json
{
  "success": true,
  "widget_name": "/Game/UI/WBP_MainMenu",
  "component_name": "PlayButton",
  "note": "Component removed successfully"
}
```

## Tips

- Removing a container also removes all its children
- Use `list_components` first to verify the component exists
- Compile the Widget Blueprint after removing components
- Save the Widget Blueprint after making changes
