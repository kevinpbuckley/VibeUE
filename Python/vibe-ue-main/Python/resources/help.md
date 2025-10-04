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

## 🏗️ Blueprint Development Workflow

**CRITICAL DEPENDENCY ORDER for Blueprint Function Development:**

### Phase 1: Foundation
1. **Create Blueprint** (`create_blueprint`)
2. **Create Blueprint Variables** (`manage_blueprint_variables`)
3. **Create Components** (`add_component`)

### Phase 2: Function Structure
4. **Create Functions** (`manage_blueprint_function` with `create_function`)
5. **Add Function Parameters** (`manage_blueprint_function` with `add_parameter`)
6. **Add Local Variables** (`manage_blueprint_function` with `add_local_variable`)

### Phase 3: Function Implementation  
7. **Create Nodes** (`manage_blueprint_node` with `create_node`)
8. **Connect Nodes** (`manage_blueprint_node` with `connect_pins`)
9. **Test Compilation** (`compile_blueprint`)

### Phase 4: Event Graph
10. **Create Event Graph Nodes** (using Event Graph context)
11. **Final Compilation and Testing**

**⚠️ NEVER add nodes to functions before completing phases 1-2. This causes "ERROR!" states in Blueprint graphs.**

**✅ Success Pattern:**
```
Dependencies Ready → Structure Complete → Implementation → Testing
```

**❌ Common Failure:**
```  
Create Function → Add Nodes Immediately (Missing dependencies!) → Broken Blueprint
```

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
- `manage_blueprint_variables` - **UNIFIED TOOL** for all variable operations (create, delete, search_types, etc.)
- `add_blueprint_variable` - (Legacy) Add variables to Blueprint
- `get_blueprint_variable` - Get variable information
- `get_blueprint_variable_info` - Get detailed variable metadata
- `delete_blueprint_variable` - (Legacy) Remove variables from Blueprint
- `get_available_blueprint_variable_types` - List all available variable types
- `get_variable_property` - Get nested variable properties
- `set_variable_property` - Set nested variable properties

**⚠️ CRITICAL: Creating Blueprint Variables - REQUIRED `type_path` Parameter**

❌ **COMMON MISTAKE**: Using `"type": "UserWidget"` or `"type": "float"` in variable_config
✅ **CORRECT USAGE**: You MUST use `"type_path": "/Script/UMG.UserWidget"` or `"type_path": "/Script/CoreUObject.FloatProperty"`

When using `manage_blueprint_variables` with `action="create"`, the `variable_config` dictionary **REQUIRES** a `type_path` field with the full canonical path. The field name is `type_path`, NOT `type`. Here are the correct type paths:

**Quick Reference - Most Common Types:**

| What You Want | Use This `type_path` |
|--------------|---------------------|
| Float number | `"/Script/CoreUObject.FloatProperty"` |
| Integer number | `"/Script/CoreUObject.IntProperty"` |
| True/False | `"/Script/CoreUObject.BoolProperty"` |
| Text string | `"/Script/CoreUObject.StrProperty"` |
| Widget (UI) | `"/Script/UMG.UserWidget"` |
| Particle Effect | `"/Script/Niagara.NiagaraSystem"` |
| Sound/Audio | `"/Script/Engine.SoundBase"` |
| Actor reference | `"/Script/Engine.Actor"` |
| Blueprint class | `"/Game/Path/To/BP_Name.BP_Name_C"` |

**Primitive Types (Most Common):**
- **float**: `"/Script/CoreUObject.FloatProperty"`
- **int**: `"/Script/CoreUObject.IntProperty"`
- **bool**: `"/Script/CoreUObject.BoolProperty"`
- **double**: `"/Script/CoreUObject.DoubleProperty"`
- **string**: `"/Script/CoreUObject.StrProperty"`
- **name**: `"/Script/CoreUObject.NameProperty"`
- **byte**: `"/Script/CoreUObject.ByteProperty"`

**Object Types:**
- **UserWidget**: `"/Script/UMG.UserWidget"`
- **NiagaraSystem**: `"/Script/Niagara.NiagaraSystem"`
- **SoundBase**: `"/Script/Engine.SoundBase"`
- **Actor**: `"/Script/Engine.Actor"`
- **Character**: `"/Script/Engine.Character"`
- **StaticMesh**: `"/Script/Engine.StaticMesh"`
- **Material**: `"/Script/Engine.Material"`
- **Texture2D**: `"/Script/Engine.Texture2D"`

**Blueprint Classes:**
- Use full asset path: `"/Game/Path/To/BP_ClassName.BP_ClassName_C"`
- Example: `"/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C"`

**Container Types:**
- Arrays, Maps, Sets: Specify base type path + container configuration

**Example: Creating Variables with Correct Type Paths:**
```python
# ❌ WRONG - This will FAIL with "type_path required" error:
manage_blueprint_variables(
    blueprint_name="BP_Player",
    action="create",
    variable_name="Health",
    variable_config={
        "type": "float",  # ❌ WRONG FIELD NAME!
        "category": "Stats"
    }
)

# ✅ CORRECT - Use type_path, not type:
# Float variable
manage_blueprint_variables(
    blueprint_name="BP_Player",
    action="create",
    variable_name="Health",
    variable_config={
        "type_path": "/Script/CoreUObject.FloatProperty",  # ✅ CORRECT!
        "category": "Stats",
        "is_editable": True
    }
)

# Widget variable
manage_blueprint_variables(
    blueprint_name="BP_Player",
    action="create",
    variable_name="HUDWidget",
    variable_config={
        "type_path": "/Script/UMG.UserWidget",
        "category": "UI",
        "is_editable": True
    }
)

# Blueprint class reference
manage_blueprint_variables(
    blueprint_name="BP_Player",
    action="create",
    variable_name="CustomHUD",
    variable_config={
        "type_path": "/Game/Blueprints/HUD/BP_MicrosubHUD.BP_MicrosubHUD_C",
        "category": "UI",
        "is_editable": True
    }
)
```

### Blueprint Components
- `manage_blueprint_components` - **UNIFIED MULTI-ACTION TOOL** for complete component management (12 actions)
  - **Discovery**: search_types, get_info, get_property_metadata, list
  - **Lifecycle**: create, delete, reparent, reorder
  - **Properties**: get_property, set_property, get_all_properties, compare_properties
  - **Status**: ✅ FULLY TESTED AND WORKING (11/12 actions validated)
  - **📖 See**: `manage_blueprint_components_guide.md` for complete action reference, examples, and best practices
- `get_available_components` - (Legacy - use manage_blueprint_components with search_types)
- `add_component` - (Legacy - use manage_blueprint_components with create)
- `add_component_to_blueprint` - Add components to Blueprint
- `get_component_info` - (Legacy - use manage_blueprint_components with get_info)
- `get_component_hierarchy` - (Legacy - use manage_blueprint_components with list)
- `get_property_metadata` - (Legacy - use manage_blueprint_components with get_property_metadata)
- `set_component_property` - (Legacy - use manage_blueprint_components with set_property)
- `remove_component` - (Legacy - use manage_blueprint_components with delete)
- `reorder_components` - (Legacy - use manage_blueprint_components with reorder)

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

#### 🚨 CRITICAL: Function Building Dependencies

**ALWAYS ensure these dependencies are met BEFORE adding nodes to a function:**

1. **Dependent Functions**: Any functions that your function will call must already exist with their input and output parameters properly defined.
   ```python
   # Example: If CalculateHealth calls ValidateInput function
   # First create ValidateInput with proper parameters
   manage_blueprint_function(
       blueprint_name="BP_Player",
       action="create_function",
       function_name="ValidateInput",
       ctx={}
   )
   # Add parameters to ValidateInput...
   # THEN create CalculateHealth that calls it
   ```

2. **Blueprint Variables**: All Blueprint-level variables that the function will reference must already exist.
   ```python
   # Example: If function uses Health variable
   # First create the Blueprint variable
   manage_blueprint_variables(
       blueprint_name="BP_Player",
       action="create",
       variable_name="Health",
       variable_config={"type_path": "/Script/CoreUObject.FloatProperty"}
   )
   # THEN create the function that references Health
   ```

3. **Local Function Variables**: Any local variables needed within the function must be created before adding nodes.
   ```python
   # Example: Create local variable in function first
   manage_blueprint_function(
       blueprint_name="BP_Player",
       action="add_local_variable",
       function_name="CalculateHealth",
       variable_name="TempModifier",
       variable_type="float"
   )
   # THEN add nodes that use TempModifier
   ```

**Dependency Order Workflow:**
```
1. Create Blueprint Variables → 2. Create Dependent Functions → 3. Add Function Parameters → 
4. Add Local Variables → 5. Add Nodes to Function → 6. Connect Nodes
```

**Why This Matters:** Nodes that reference non-existent variables or functions will cause compilation errors and broken Blueprint graphs. Following this dependency order prevents the "ERROR!" states in functions.

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

> ✅ **Update (Sept 2025):** The reflection layer now resolves external targets when
> you supply class hints. Include `node_params.function_name` with
> `node_params.function_class` (or `FunctionReference.MemberParent`) to spawn fully
> wired static/global calls such as `GameplayStatics::GetPlayerController`. Provide
> `node_params.cast_target` (soft class path or Blueprint class name) to configure
> `Cast To <Class>` nodes automatically. Pins populate immediately, eliminating the
> manual cleanup required by earlier builds.

#### ⚠️ DEPENDENCY REQUIREMENTS

**Before adding nodes to any function, verify:**

1. **All referenced variables exist** (Blueprint variables, function parameters, local variables)
2. **All called functions exist** with proper input/output parameters defined
3. **Function signature is complete** (parameters added via `manage_blueprint_function`)
4. **Dependencies are satisfied** (see manage_blueprint_function section for full workflow)

**Common Node Creation Failures:**
- `Variable Get/Set nodes`: Variable doesn't exist → Create variable first
- `Function Call nodes`: Target function missing parameters → Add parameters first  
- `Cast nodes`: Target class unknown → Verify class name and availability
- `Connection failures`: Pin names don't match → Use `get_node_details` to verify pin names

**Recommended Workflow:**
```
Dependencies Ready → Create Nodes → Connect Pins → Test Compilation
```

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
- Use `get_blueprint_info()` to verify Blueprint structure

### Function Development Issues
**"ERROR!" nodes in Blueprint graphs:**
- ❌ **Root Cause**: Dependencies missing when nodes were created
- ✅ **Solution**: Delete broken functions, recreate with proper dependency order

**Function Creation Failures:**
```python
# ❌ WRONG: Create function and add nodes immediately
manage_blueprint_function("BP_Player", "create_function", function_name="Test")
manage_blueprint_node("BP_Player", "create_node", node_type="Get Health")  # FAILS!

# ✅ RIGHT: Follow dependency order
# 1. Create variable first
manage_blueprint_variables("BP_Player", "create", variable_name="Health", 
                          variable_config={"type_path": "/Script/CoreUObject.FloatProperty"})
# 2. Create function
manage_blueprint_function("BP_Player", "create_function", function_name="Test")
# 3. Now create nodes that reference existing dependencies
manage_blueprint_node("BP_Player", "create_node", node_type="Get Health")  # SUCCESS!
```

**Function Compilation Errors:**
- **Node reference failures**: Missing variables/functions → Create dependencies first
- **Parameter mismatches**: Function signature incomplete → Add all parameters before nodes
- **Connection failures**: Pin names don't exist → Use `get_node_details()` to verify pins
- **Recovery**: Delete function with `delete_function`, recreate with proper workflow
- Use `get_blueprint_info()` to understand structure
- Check variable types with reflection tools
- Test functions in isolation
- **If manage_blueprint_* tools confusing: Check Multi-Action Tools section above**

---

## � Function Building Quick Reference

**Essential Checklist Before Adding Nodes:**

```python
# ✅ REQUIRED DEPENDENCIES CHECKLIST:

# 1. Blueprint Variables Created?
manage_blueprint_variables("BP_Player", "create", variable_name="Health", 
                          variable_config={"type_path": "/Script/CoreUObject.FloatProperty"})

# 2. Dependent Functions Created with Parameters?  
manage_blueprint_function("BP_Player", "create_function", function_name="ValidateInput")
manage_blueprint_function("BP_Player", "add_parameter", function_name="ValidateInput", 
                         param_name="InputValue", direction="input", type="float")

# 3. Target Function Created with Complete Signature?
manage_blueprint_function("BP_Player", "create_function", function_name="ProcessData") 
manage_blueprint_function("BP_Player", "add_parameter", function_name="ProcessData",
                         param_name="Data", direction="input", type="float")
manage_blueprint_function("BP_Player", "add_parameter", function_name="ProcessData", 
                         param_name="Result", direction="output", type="bool")

# 4. Local Variables Added?
manage_blueprint_function("BP_Player", "add_local_variable", function_name="ProcessData",
                         variable_name="TempValue", variable_type="float")

# ✅ NOW SAFE TO ADD NODES:
manage_blueprint_node("BP_Player", "create_node", node_type="Get Health",
                     graph_scope="function", function_name="ProcessData")
```

**Recovery from Broken Functions:**
```python
# Delete all broken functions
manage_blueprint_function("BP_Player", "delete_function", function_name="BrokenFunction")
# Compile to clear errors
compile_blueprint("BP_Player") 
# Recreate with proper workflow (dependencies first!)
```

---

## �💡 Pro Tips

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
