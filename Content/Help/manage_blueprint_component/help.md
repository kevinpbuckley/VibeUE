# manage_blueprint_component

Manage components within Blueprint actors - add, remove, configure, and organize components in the component hierarchy.

## Summary

The `manage_blueprint_component` tool provides comprehensive component management for Blueprint actors. It allows you to add and remove components, configure their properties, navigate the component hierarchy, and reparent components within the tree structure.

## Actions

| Action | Description |
|--------|-------------|
| add | Add a new component to a Blueprint |
| remove | Remove a component from a Blueprint |
| get_hierarchy | Get the full component hierarchy tree |
| set_property | Set a property value on a component |
| get_property | Get a property value from a component |
| get_all_properties | Get all properties of a component |
| reparent | Change a component's parent in the hierarchy |
| get_available | List available component types that can be added |
| get_info | Get detailed information about a specific component |

## Usage

### Add a Component
```json
{
  "Action": "add",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"ComponentClass\": \"StaticMeshComponent\", \"ComponentName\": \"WeaponMesh\"}"
}
```

### Get Component Hierarchy
```json
{
  "Action": "get_hierarchy",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Vehicle\"}"
}
```

### Set Component Property
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Light\", \"ComponentName\": \"PointLight\", \"PropertyName\": \"Intensity\", \"Value\": 5000}"
}
```

## Notes

- Component names must be unique within a Blueprint
- Some components can only be added to specific parent classes
- Use `get_available` to discover valid component types
- Compile the Blueprint after adding or modifying components

## Color Property Formats (FColor)

**Blueprint components use FColor with 0-255 byte values:**
```python
# ✅ CORRECT - JSON object format (0-255 byte values, RGB order)
manage_blueprint_component(action="set_property", 
    blueprint_name="/Game/Blueprints/BP_Light",
    component_name="SpotLight",
    property_name="LightColor",
    property_value={"R": 0, "G": 255, "B": 0, "A": 255})  # Green

# ✅ CORRECT - Array format (0-255 byte values, RGB order)
manage_blueprint_component(action="set_property",
    blueprint_name="/Game/Blueprints/BP_Light",
    component_name="SpotLight",
    property_name="LightColor",
    property_value=[255, 0, 0, 255])  # Red (RGB order)

# ❌ WRONG - These formats will fail:
# property_value="[0, 255, 0]"      # String array - WRONG
# property_value="0.0, 1.0, 0.0"    # Float format - WRONG  
# property_value="FColor(0,255,0)"  # Constructor format - WRONG

# ⚠️ NOTE: FColor uses 0-255 byte values (NOT 0-1 normalized!)
```

## Vector Property Formats (FVector)

```python
# CORRECT
property_value="(X=100,Y=200,Z=300)"

# WRONG
# property_value="[100, 200, 300]"
```

## Workflow Tips

1. Use `get_property` first to see the current value and format
2. Use the EXACT same format structure when calling `set_property`
3. If still failing, use `action="help"` for the specific action
