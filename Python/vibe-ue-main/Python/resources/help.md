# VibeUE MCP Tools Complete Reference Guide

This guide provides comprehensive documentation for all VibeUE MCP tools, their actions, parameters, and usage patterns.

## 🆘 AI ASSISTANTS: WHEN TO USE THIS GUIDE

**CALL `get_help()` IMMEDIATELY WHEN:**
- ❓ You can't find the right tool for a task
- 🔧 You don't know which parameters a tool requires  
- ❌ You're getting errors and need troubleshooting help
- 📝 You need examples for multi-action tools (manage_blueprint_function, manage_blueprint_node)
- 🔄 You want to understand proper workflow patterns
- ⚡ You need performance tips or best practices
- 🚫 A tool seems to be missing or not working as expected

**DON'T ASK USERS FOR TOOL HELP - CHECK THIS GUIDE FIRST!**

This documentation contains everything you need to use the VibeUE MCP system effectively.

## 🚀 Quick Start

**Essential First Steps:**
1. Use `check_unreal_connection()` to verify Unreal Engine connection
2. Use `search_items()` to find assets before modifying them
3. Always check the `success` field in responses
4. Use `get_umg_guide()` for UMG-specific workflow guidance

---

## 📋 Tool Categories

### System & Diagnostics
- `check_unreal_connection` - Test connection and plugin status
- `get_help` - This comprehensive tool guide

### Asset Discovery & Management
- `search_items` - Universal asset search (widgets, textures, materials, etc.)
- `import_texture_asset` - Import textures with validation and optimization
- `export_texture_for_analysis` - Export textures for AI visual analysis
- `convert_svg_to_png` - Convert SVG files to PNG format
- `open_asset_in_editor` - Open any asset in appropriate Unreal editor

### Blueprint Management
- `create_blueprint` - Create new Blueprint classes
- `get_blueprint_info` - Get comprehensive Blueprint information (variables, components, functions)
- `compile_blueprint` - Compile Blueprint after changes
- `reparent_blueprint` - Change Blueprint parent class

### Blueprint Variables
- `add_blueprint_variable` - Add variables to Blueprint
- `get_blueprint_variable` - Get variable information
- `get_blueprint_variable_info` - Get detailed variable metadata
- `delete_blueprint_variable` - Remove variables from Blueprint
- `get_available_blueprint_variable_types` - List all available variable types
- `get_variable_property` - Get nested variable properties
- `set_variable_property` - Set nested variable properties

### Blueprint Components
- `get_available_components` - Discover all component types via reflection
- `add_component` - Add components using reflection-based creation
- `add_component_to_blueprint` - Add components to Blueprint
- `get_component_info` - Get complete component information
- `get_component_hierarchy` - Get Blueprint component hierarchy
- `get_property_metadata` - Get detailed property information
- `set_component_property` - Set component properties with reflection
- `remove_component` - Remove components with hierarchy handling
- `reorder_components` - Reorder component hierarchy

### Blueprint Functions & Nodes
- `manage_blueprint_function` - **MULTI-ACTION TOOL** for function management (includes listing functions)
- `manage_blueprint_node` - **MULTI-ACTION TOOL** for node operations
- `list_custom_events` - List custom events on Event Graph
- `get_available_blueprint_nodes` - Discover available Blueprint nodes
- `get_node_details` - Get detailed node information
- `summarize_event_graph` - Get readable Event Graph overview

### UMG Widget System
- `get_umg_guide` - **ESSENTIAL** comprehensive UMG workflow guide
- `create_umg_widget_blueprint` - Create new Widget Blueprints
- `delete_widget_blueprint` - Delete Widget Blueprint assets
- `get_widget_blueprint_info` - Get complete widget information
- `validate_widget_hierarchy` - Validate widget structure

### UMG Widget Components
- `get_available_widgets` - Discover all widget types via reflection
- `get_available_widget_types` - Get widget types with categories
- `list_widget_components` - List all widget components with hierarchy
- `add_widget_component` - **UNIVERSAL** widget component creation
- `remove_umg_component` - Universal widget component removal
- `list_widget_properties` - List available properties for components
- `get_widget_component_properties` - Get all component properties
- `get_widget_property` - Get specific widget property value
- `set_widget_property` - **UNIVERSAL** widget property setter

### UMG Events & Binding
- `get_available_events` - Get available events for components
- `bind_input_events` - Bind multiple input events at once

---

## 🛠️ Multi-Action Tools Reference

These tools use an `action` parameter to perform different operations. Each action has specific required parameters.

### `manage_blueprint_function` - Function Management

**Purpose:** Complete function lifecycle management with local variable support.

**Note:** This tool replaces the old `list_blueprint_functions` tool - use `list_functions` action instead.

#### Actions Available:

##### `list_functions`
List all functions in a Blueprint.
```python
manage_blueprint_function(
    blueprint_name="BP_Player",
    action="list_functions",
    ctx={},
    kwargs={"include_overrides": True}  # Optional: include inherited functions
)
```

##### `create_function`
Create a new function in the Blueprint.
```python
manage_blueprint_function(
    blueprint_name="BP_Player", 
    action="create_function",
    function_name="CalculateDamage",
    ctx={},
    kwargs={"return_type": "float"}
)
```

##### `delete_function`
Remove a function from the Blueprint.
```python
manage_blueprint_function(
    blueprint_name="BP_Player",
    action="delete_function", 
    function_name="OldFunction",
    ctx={},
    kwargs={}
)
```

##### `add_parameter`
Add input/output parameter to function.
```python
manage_blueprint_function(
    blueprint_name="BP_Player",
    action="add_parameter",
    function_name="CalculateDamage",
    param_name="BaseDamage",
    direction="input",  # or "output"
    type="float",
    ctx={},
    kwargs={}
)
```

##### `remove_parameter` 
Remove parameter from function.
```python
manage_blueprint_function(
    blueprint_name="BP_Player",
    action="remove_parameter",
    function_name="CalculateDamage", 
    param_name="OldParam",
    ctx={},
    kwargs={}
)
```

##### `rename_parameter`
Rename function parameter.
```python
manage_blueprint_function(
    blueprint_name="BP_Player",
    action="rename_parameter",
    function_name="CalculateDamage",
    param_name="OldName",
    new_name="NewName", 
    ctx={},
    kwargs={}
)
```

##### `list_locals` 
List all local variables in function.
```python
manage_blueprint_function(
    blueprint_name="BP_Player",
    action="list_locals",
    function_name="CalculateDamage",
    ctx={},
    kwargs={}
)
```

##### `add_local`
Add local variable to function.
```python
manage_blueprint_function(
    blueprint_name="BP_Player", 
    action="add_local",
    function_name="CalculateDamage",
    param_name="TempResult",
    type="float", 
    ctx={},
    kwargs={}
)
```

##### `remove_local`
Remove local variable from function.
```python
manage_blueprint_function(
    blueprint_name="BP_Player",
    action="remove_local", 
    function_name="CalculateDamage",
    param_name="TempResult",
    ctx={},
    kwargs={}
)
```

##### `update_local`
Update local variable type.
```python
manage_blueprint_function(
    blueprint_name="BP_Player",
    action="update_local",
    function_name="CalculateDamage", 
    param_name="TempResult",
    new_type="int",
    ctx={},
    kwargs={}
)
```

### `manage_blueprint_node` - Node Operations

**Purpose:** Complete node lifecycle and connection management for Blueprint graphs.

#### Actions Available:

##### `list_nodes`
List all nodes in Event Graph or specific function.
```python
manage_blueprint_node(
    blueprint_name="BP_Player",
    action="list_nodes", 
    graph_scope="event",  # or "function"
    function_name="MyFunction",  # if graph_scope="function"
    ctx={},
    kwargs={}
)
```

##### `create_node`
Create a new node in the graph.
```python
manage_blueprint_node(
    blueprint_name="BP_Player",
    action="create_node",
    node_type="K2Node_CallFunction",
    node_config={
        "function_name": "Print String",
        "position": [100, 200]
    },
    ctx={},
    kwargs={}
)
```

##### `delete_node`
Remove a node from the graph.
```python
manage_blueprint_node(
    blueprint_name="BP_Player", 
    action="delete_node",
    node_id="12345",
    ctx={},
    kwargs={}
)
```

##### `connect_pins`
Connect output pin to input pin between nodes.
```python
manage_blueprint_node(
    blueprint_name="BP_Player",
    action="connect_pins",
    source_node_id="12345",
    source_pin="output",
    target_node_id="67890", 
    target_pin="input",
    ctx={},
    kwargs={}
)
```

##### `disconnect_pins`
Disconnect pins between nodes.
```python
manage_blueprint_node(
    blueprint_name="BP_Player",
    action="disconnect_pins", 
    source_node_id="12345",
    source_pin="output",
    target_node_id="67890",
    target_pin="input",
    ctx={},
    kwargs={}
)
```

##### `set_node_property`
Set property value on a node.
```python
manage_blueprint_node(
    blueprint_name="BP_Player",
    action="set_node_property",
    node_id="12345",
    property_name="InString", 
    property_value="Hello World",
    ctx={},
    kwargs={}
)
```

##### `move_node`
Change node position in graph.
```python
manage_blueprint_node(
    blueprint_name="BP_Player",
    action="move_node",
    node_id="12345",
    node_position=[300, 400],
    ctx={},
    kwargs={}
)
```

---

## 📚 Common Usage Patterns

### Widget Creation Workflow
```python
# 1. Search for existing widgets
search_result = search_items(search_term="Inventory", asset_type="Widget")

# 2. Create new widget if needed
create_umg_widget_blueprint(name="WBP_Inventory", path="/Game/UI")

# 3. Get widget structure
widget_info = get_widget_blueprint_info("WBP_Inventory")

# 4. Add components
add_widget_component("WBP_Inventory", "CanvasPanel", "MainPanel")
add_widget_component("WBP_Inventory", "Button", "CloseButton", parent_name="MainPanel")

# 5. Style components
set_widget_property("WBP_Inventory", "CloseButton", "Text", "Close")
set_widget_property("WBP_Inventory", "CloseButton", "ColorAndOpacity", [1.0, 0.0, 0.0, 1.0])
```

### Blueprint Function with Local Variables
```python
# 1. Create function
manage_blueprint_function("BP_Player", "create_function", function_name="ProcessInput", ctx={}, kwargs={})

# 2. Add parameters
manage_blueprint_function("BP_Player", "add_parameter", function_name="ProcessInput", 
                         param_name="InputValue", direction="input", type="float", ctx={}, kwargs={})

# 3. Add local variables  
manage_blueprint_function("BP_Player", "add_local", function_name="ProcessInput",
                         param_name="ProcessedValue", type="float", ctx={}, kwargs={})

# 4. List all variables to verify
locals_list = manage_blueprint_function("BP_Player", "list_locals", function_name="ProcessInput", ctx={}, kwargs={})
```

### Asset Import and Application
```python
# 1. Import texture
import_result = import_texture_asset("/path/to/texture.png", "/Game/Textures")

# 2. Find widgets to apply texture to
widgets = search_items(asset_type="Widget")

# 3. Apply texture to widget component
set_widget_property("WBP_Menu", "Background", "Brush.Texture", "/Game/Textures/MyTexture")
```

---

## ⚠️ Important Guidelines

### Always Check Success
Every tool returns a `success` field. Always check it:
```python
result = some_tool(parameters...)
if not result.get("success", False):
    # Handle error
    print(f"Error: {result.get('error', 'Unknown error')}")
```

### Asset Search First
Before modifying any asset, search for it first:
```python
# ❌ Wrong - assuming asset exists
set_widget_property("WBP_Unknown", "Button1", "Text", "Click Me")

# ✅ Correct - search first
widgets = search_items(search_term="Menu", asset_type="Widget")
if widgets["success"] and widgets["items"]:
    widget_name = widgets["items"][0]["name"]
    set_widget_property(widget_name, "Button1", "Text", "Click Me")
```

### UMG Guide First
Before any UMG work, get the comprehensive guide:
```python
# Always start with this for UMG workflows
guide = get_umg_guide()
# Review guide content for proper patterns and best practices
```

### Multi-Action Tool Pattern
For manage tools, always provide required context:
```python
# All manage_blueprint_function calls need ctx and kwargs
manage_blueprint_function(
    blueprint_name="BP_Target",
    action="list_locals", 
    function_name="MyFunction",
    ctx={},  # Always provide
    kwargs={}  # Always provide
)
```

### Component Type Discovery
Use reflection tools to discover available types:
```python
# Get all available widget types
widgets = get_available_widgets()

# Get all available component types  
components = get_available_components()

# Get all Blueprint variable types
var_types = get_available_blueprint_variable_types()
```

---

## 🔧 Troubleshooting

### Connection Issues
- `check_unreal_connection()` - First diagnostic step
- Ensure Unreal Engine 5.6 is running
- Verify VibeUE plugin is loaded and enabled
- Check port 55557 is available

### Tool Failures
- Always check `success` field in responses
- Use exact names from search results
- Don't assume assets exist - search first
- Check logs for detailed error information
- **If still stuck: Re-read this help documentation or specific tool sections**

### Widget Issues
- Use `validate_widget_hierarchy()` for structure problems
- Get `get_widget_blueprint_info()` before modifications
- List components with `list_widget_components()` for exact names
- Use `get_umg_guide()` for workflow guidance
- **If confused about UMG: Review the UMG workflow patterns in this guide**

### Blueprint Issues  
- Always `compile_blueprint()` after changes
- Use `get_blueprint_info()` to understand structure
- Check variable types with reflection tools
- Test functions in isolation
- **If manage_blueprint_* tools confusing: Check Multi-Action Tools section above**

---

## 💡 Pro Tips

1. **Performance**: Use full asset paths from search results for faster operations
2. **Reliability**: Check `success` field on every tool response 
3. **Discovery**: Use reflection tools (`get_available_*`) to find valid types
4. **Workflow**: Follow UMG Guide patterns for professional results
5. **Debugging**: Use info tools (`get_*_info`) to understand current state
6. **Safety**: Use validation tools before making destructive changes
7. **When Lost**: Call `get_help()` to access this complete reference guide

---

## 🎯 Summary

The VibeUE MCP toolset provides comprehensive Unreal Engine automation through:

- **Universal Search**: `search_items()` for any asset type
- **Reflection-Based Discovery**: `get_available_*()` tools for type discovery  
- **Multi-Action Management**: `manage_blueprint_*()` for complex operations
- **Universal Property System**: `set_widget_property()` and `set_component_property()`
- **Comprehensive Guidance**: `get_umg_guide()` and `get_help()` for workflow support

**Remember**: Always start with connection testing, asset discovery, and appropriate guide consultation before making modifications.

---

## 🆘 FINAL REMINDER FOR AI ASSISTANTS

**This help tool (`get_help()`) is your complete reference for:**
- Finding the right tool for any task
- Understanding multi-action tool parameters  
- Getting troubleshooting guidance
- Learning workflow patterns and best practices
- Resolving errors and performance issues

**Use `get_help()` BEFORE asking users for clarification on tool usage!**

**The help is comprehensive - everything you need is documented here.**
