# manage_material_node

Material graph node (expression) management tool for Unreal Engine.

## Summary

Create, connect, and configure material expression nodes in the material graph. Supports discovering expression types, connecting nodes to material outputs, promoting constants to parameters, and managing node properties.

## Actions

| Action | Description |
|--------|-------------|
| help | Show help for this tool or a specific action |
| discover_types | Discover available material expression types |
| get_categories | Get expression type categories |
| create | Create a new expression node |
| delete | Delete an expression node |
| move | Move a node to a new position |
| list | List all nodes in the material graph |
| get_details | Get detailed information about a node |
| get_pins | Get pin information for a node |
| connect | Connect two expression pins |
| disconnect | Disconnect an expression pin |
| connect_to_output | Connect an expression to a material output |
| disconnect_output | Disconnect a material output |
| list_connections | List all connections in the material |
| get_property | Get a node property value |
| set_property | Set a node property value |
| list_properties | List all editable properties for a node |
| promote_to_parameter | Promote an expression pin to a material parameter |
| create_parameter | Create a new material parameter |
| set_parameter_metadata | Set parameter metadata |
| get_output_properties | Get material output properties |
| get_output_connections | Get material output connections |

## Usage

```json
{"action": "help"}
{"action": "help", "help_action": "create"}
```

## Notes

- Use `discover_types` with `search_term` to find expression classes
- Expression IDs are returned when creating nodes - save them for connecting
- Use `connect_to_output` to wire expressions to material outputs (BaseColor, Roughness, etc.)
- Always compile the material after making changes using `manage_material(action="compile")`

## Expression Property Names

Different material expressions have different property names:
- For `MaterialExpressionConstant`, the value property is `R` (not `Value` or `ConstantValue`)
- For `MaterialExpressionConstant3Vector`, use `Constant` to set the color

When unsure, use `manage_material_node(action="list_properties", ...)` to discover available properties.

## Common Mistakes to Avoid

**Listing vs Getting Properties:**
- ❌ WRONG: `manage_material_node(action="get_properties", ...)` - use singular form!
- ✅ CORRECT: `manage_material_node(action="list_properties", ...)` to list all properties
- ✅ CORRECT: `manage_material_node(action="get_property", property_name="R", ...)` to get one property

**Category Filters in discover_types:**
- ❌ WRONG: `manage_material_node(action="discover_types", category="All")` - "All" is not a valid category
- ✅ CORRECT: `manage_material_node(action="discover_types", search_term="Constant")` - use search_term instead
- ✅ CORRECT: `manage_material_node(action="get_categories")` - to list valid categories first

## Workflow Tips

1. Use `get_property` first to see the current value and format
2. Use the EXACT same format structure when calling `set_property`
3. If still failing, use `action="help"` for the specific action
