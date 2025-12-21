# manage_material

Material and Material Instance management tool for Unreal Engine.

## Summary

Create, modify, compile, and manage materials and material instances. Supports property editing, parameter configuration, and material instance creation.

## Actions

| Action | Description |
|--------|-------------|
| help | Show help for this tool or a specific action |
| create | Create a new material asset |
| create_instance | Create a material instance from a parent material |
| save | Save material to disk |
| compile | Recompile material shaders |
| refresh_editor | Refresh open Material Editor |
| get_info | Get comprehensive material information |
| summarize | Get a brief summary of material |
| list_properties | List all editable properties |
| get_property | Get a property value |
| get_property_info | Get detailed property metadata |
| set_property | Set a property value |
| set_properties | Set multiple properties at once |
| list_parameters | List all material parameters |
| get_parameter | Get a specific parameter |
| set_parameter_default | Set a parameter's default value |
| get_instance_info | Get comprehensive info about a material instance |
| list_instance_properties | List all editable properties on a material instance |
| get_instance_property | Get a single property value from instance |
| set_instance_property | Set a property on material instance |
| list_instance_parameters | List all parameters with current/default values |
| set_instance_scalar_parameter | Set a scalar parameter override |
| set_instance_vector_parameter | Set a vector/color parameter override |
| set_instance_texture_parameter | Set a texture parameter override |
| clear_instance_parameter_override | Remove parameter override, revert to parent |
| save_instance | Save material instance to disk |

## Usage

```json
{"action": "help"}
{"action": "help", "help_action": "create"}
```

## Notes

- Base material actions use `material_path` parameter
- Instance actions use `instance_path` parameter
- Property setting accepts both `value` and `property_value` parameter names
- Always compile after making changes for them to take effect

## Common Patterns

### Creating Materials
```python
# Create a material
manage_material(action="create", material_name="M_Red", destination_path="/Game/Materials")

# Create a material instance
manage_material(action="create_instance", instance_name="MI_RedBright",
               parent_material_path="/Game/Materials/M_Red",
               destination_path="/Game/Materials")

# Set a color parameter on an instance
manage_material(action="set_instance_vector_parameter",
               instance_path="/Game/Materials/MI_RedBright",
               parameter_name="BaseColor", r=1.0, g=0.0, b=0.0, a=1.0)
```

## Common Mistakes to Avoid

**Opening Materials in Editor:**
- ❌ WRONG: `manage_material(action="open_editor", ...)` - this action doesn't exist!
- ✅ CORRECT: `manage_asset(action="open_in_editor", asset_path="/Game/Materials/M_MyMaterial")` - use manage_asset for opening ANY asset

**Creating Parameters:**
- ❌ WRONG: `manage_material(action="create_scalar_parameter", ...)` - this action doesn't exist!
- ✅ CORRECT: `manage_material_node(action="create_parameter", parameter_type="Scalar", parameter_name="Roughness", ...)` - use manage_material_node for graph editing

**Material Properties vs Graph Nodes:**
- `manage_material` = Material asset properties, instances, and parameters (NOT graph editing)
- `manage_material_node` = Material graph expressions, node connections, and visual graph editing
