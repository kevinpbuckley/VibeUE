# set_property

Set a property value on a widget component.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| widget_name | string | Yes | Content path to the Widget Blueprint |
| component_name | string | Yes | Name of the component to modify |
| property_name | string | Yes | Name of the property to set |
| property_value | any | Yes | New value for the property |

## Examples

### Set Text
```python
manage_umg_widget(action="set_property", widget_name="/Game/UI/WBP_MainMenu", 
                  component_name="TitleText", property_name="Text", 
                  property_value="My Awesome Game")
```

### Set Visibility
```python
manage_umg_widget(action="set_property", widget_name="/Game/UI/WBP_HUD", 
                  component_name="PauseMenu", property_name="Visibility", 
                  property_value="Collapsed")
```

### Set Color (use dict format, NOT escaped JSON string!)
```python
# ✅ CORRECT - property_value as dict object (will be auto-serialized)
manage_umg_widget(action="set_property", widget_name="/Game/UI/WBP_MainMenu", 
                  component_name="TitleText", property_name="ColorAndOpacity", 
                  property_value={"R": 1.0, "G": 0.8, "B": 0.0, "A": 1.0})

# ❌ WRONG - Double-escaped JSON string causes parse errors
property_value="{\"R\": 1.0, \"G\": 0.8}"  # WRONG! Don't escape JSON inside JSON
```

### Set Progress
```python
manage_umg_widget(action="set_property", widget_name="/Game/UI/WBP_HUD", 
                  component_name="HealthBar", property_name="Percent", 
                  property_value=0.75)
```

### Set Image Brush
```python
manage_umg_widget(action="set_property", widget_name="/Game/UI/WBP_MainMenu", 
                  component_name="BackgroundImage", property_name="Brush", 
                  property_value="/Game/UI/Textures/T_Background")
```

## JSON Encoding Warning

**CRITICAL: property_value must be a proper JSON value, NOT a string containing escaped JSON!**

When passing complex values (objects, arrays):
- ✅ Pass the actual object: `property_value={"R": 1.0, "G": 0.8}` 
- ❌ Don't pass escaped string: `property_value="{\"R\": 1.0, \"G\": 0.8}"` 

The MCP layer handles JSON serialization - you just pass native values.

## Returns

```json
{
  "success": true,
  "widget_name": "/Game/UI/WBP_MainMenu",
  "component_name": "TitleText",
  "property_name": "Text",
  "property_value": "My Awesome Game",
  "note": "Property set successfully"
}
```

## Tips

- Visibility values: Visible, Hidden, Collapsed, HitTestInvisible, SelfHitTestInvisible
- Colors use 0-1 range for R, G, B, A (NOT 0-255!)
- Progress bar Percent is 0.0 to 1.0
- Image brushes use texture content paths
- Save the Widget Blueprint after property changes
