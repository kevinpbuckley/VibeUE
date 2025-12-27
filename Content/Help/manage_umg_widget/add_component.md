# add_component

Add a widget component to a Widget Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| widget_name | string | Yes | Content path to the Widget Blueprint |
| component_type | string | Yes | Type of widget to add (e.g., "Button", "TextBlock", "Image") |
| component_name | string | Yes | Name for the new component |
| parent_name | string | No | Name of parent container (omit for root level) |

## Examples

### Add Button to Root
```python
manage_umg_widget(action="add_component", widget_name="/Game/UI/WBP_MainMenu",
                  component_type="Button", component_name="PlayButton")
```

### Add TextBlock to Container
```python
manage_umg_widget(action="add_component", widget_name="/Game/UI/WBP_MainMenu",
                  component_type="TextBlock", component_name="TitleText",
                  parent_name="MenuContainer")
```

### Build Widget Hierarchy
```python
# 1. Add root container
manage_umg_widget(action="add_component", widget_name="/Game/UI/MyWidget",
                  component_type="CanvasPanel", component_name="RootCanvas")

# 2. Add child components to the container
manage_umg_widget(action="add_component", widget_name="/Game/UI/MyWidget",
                  component_type="VerticalBox", component_name="MenuBox",
                  parent_name="RootCanvas")

# 3. Add buttons to the vertical box
manage_umg_widget(action="add_component", widget_name="/Game/UI/MyWidget",
                  component_type="Button", component_name="PlayButton",
                  parent_name="MenuBox")
```

## Returns

```json
{
  "success": true,
  "widget_name": "/Game/UI/WBP_MainMenu",
  "component_name": "PlayButton",
  "component_type": "Button",
  "parent_name": "(root)",
  "is_variable": false,
  "note": "Created new Button widget 'PlayButton'"
}
```

## Common Widget Types

| Type | Description |
|------|-------------|
| Button | Clickable button |
| TextBlock | Text display |
| Image | Image display |
| CanvasPanel | Absolute positioning container |
| VerticalBox | Vertical layout container |
| HorizontalBox | Horizontal layout container |
| Overlay | Stacking container |
| ScrollBox | Scrollable container |
| CheckBox | Check box control |
| Slider | Slider control |
| ProgressBar | Progress indicator |
| EditableText | Single-line text input |
| EditableTextBox | Multi-line text input |

## Tips

- Use `search_types` action to discover available widget types
- Empty Widget Blueprints have no root - add a container first
- Parent containers must exist before adding children to them
- Compile the Widget Blueprint after adding components
