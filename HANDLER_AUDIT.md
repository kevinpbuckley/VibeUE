# Handler Audit - Active vs Dead Code

This document maps MCP tools to C++ handlers to identify what code is actually used vs what can be deleted.

## Methodology

1. Analyzed all Python MCP tool files in `Python/tools/`
2. Extracted all `send_command()` calls
3. Mapped to C++ handler routing in `HandleCommand()` methods
4. Identified handlers not called by any tool

## Active MCP Tools (Keep All Handlers)

### Tool: manage_blueprint
**File**: `manage_blueprint.py`
**C++ Class**: `FBlueprintCommands`
**Commands**:
- ✅ `create_blueprint`
- ✅ `compile_blueprint`
- ✅ `get_blueprint_info`
- ✅ `get_blueprint_property`
- ✅ `set_blueprint_property`
- ✅ `reparent_blueprint`
- ✅ `list_custom_events`

**Note**: `summarize_event_graph` calls `manage_blueprint_node` internally

---

### Tool: manage_blueprint_node
**File**: `manage_blueprint_node.py`
**C++ Class**: `FBlueprintNodeCommands`
**Commands**:
- ✅ `manage_blueprint_node` (multi-action tool)
  - Actions: discover, create, connect, connect_pins, disconnect, disconnect_pins, delete, move, list, describe, get_details, configure, split, split_pins, recombine, recombine_pins, unsplit, reset_pin_defaults, refresh_node, refresh_nodes

**Also calls**:
- ✅ `get_node_details` (for summarize_event_graph)

---

### Tool: manage_blueprint_variable
**File**: `manage_blueprint_variable.py`
**C++ Class**: `FBlueprintCommands` (variable handlers)
**Commands**:
- ✅ `manage_blueprint_variable` (multi-action tool)
  - Actions: create, delete, get_info, get_property, set_property, search_types, list, modify

---

### Tool: manage_blueprint_function
**File**: `manage_blueprint_function.py`
**C++ Class**: `FBlueprintCommands` (function handlers)
**Commands**:
- ✅ `manage_blueprint_function` (multi-action tool)
  - Actions: list, get, list_params, create, delete, add_param, remove_param, update_param, list_locals, add_local, remove_local, update_local, update_properties

---

### Tool: manage_blueprint_component
**File**: `manage_blueprint_component.py`
**C++ Class**: `FBlueprintCommands` or `BlueprintComponentReflection`
**Commands**:
- ✅ `get_available_components` (search_types action)
- ✅ `get_component_info` (get_info action)
- ✅ `get_property_metadata` (get_property_metadata action)
- ✅ `get_component_hierarchy` (internal?)
- ✅ `add_component` (create action)
- ✅ `set_component_property` (set_property action)
- ✅ `get_component_property` (get_property action)
- ✅ `get_all_component_properties` (get_all_properties action)
- ✅ `compare_component_properties` (compare_properties action)
- ✅ `remove_component` (delete action)
- ✅ `reorder_components` (reorder action)
- ✅ `reparent_component` (reparent action)

---

### Tool: manage_umg_widget
**File**: `manage_umg_widget.py`
**C++ Class**: `FUMGCommands`
**Commands**:
- ✅ `list_widget_components`
- ✅ `add_widget_component`
- ✅ `remove_umg_component`
- ✅ `validate_widget_hierarchy`
- ✅ `get_available_widgets` (search_types action)
- ✅ `get_widget_component_properties`
- ✅ `get_widget_property`
- ✅ `set_widget_property`
- ✅ `list_widget_properties`
- ✅ `get_available_events`
- ✅ `bind_input_events`

---

### Tool: manage_asset
**File**: `manage_asset.py`
**C++ Class**: `FAssetCommands`
**Commands**:
- ✅ `search_items`
- ✅ `import_texture_asset`
- ✅ `export_texture_for_analysis`
- ✅ `delete_asset`
- ✅ `OpenAssetInEditor`
- ✅ `duplicate_asset`

---

### Tool: system (check_unreal_connection)
**File**: `system.py`
**C++ Class**: Various
**Commands**:
- ✅ `check_connection`

---

## Summary of Active Commands

Total active commands: **41 unique C++ handlers**

```
add_component
add_widget_component  
bind_input_events
check_connection
compare_component_properties
compile_blueprint
create_blueprint
delete_asset
duplicate_asset
export_texture_for_analysis
get_all_component_properties
get_available_components
get_available_events
get_available_widgets
get_blueprint_info
get_blueprint_property
get_component_hierarchy
get_component_info
get_component_property
get_node_details
get_property_metadata
get_widget_component_properties
get_widget_property
import_texture_asset
list_custom_events
list_widget_components
list_widget_properties
manage_blueprint_function (multi-action)
manage_blueprint_node (multi-action)
manage_blueprint_variable (multi-action)
OpenAssetInEditor
remove_component
remove_umg_component
reorder_components
reparent_blueprint
reparent_component
search_items
set_blueprint_property
set_component_property
set_widget_property
validate_widget_hierarchy
```

---

## Next Step: Identify Dead Code

Now we need to:
1. List ALL handlers in each C++ command file
2. Compare against this active list
3. Identify handlers NOT in active list
4. Create deletion issue with specific handlers to remove
