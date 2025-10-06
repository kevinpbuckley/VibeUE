# Troubleshooting Guide

## Connection Issues

### "Failed to connect to Unreal Engine"

**Causes:**
- Unreal Engine not running
- VibeUE plugin not loaded
- Firewall blocking port 55557
- Another application using the port

**Solutions:**
1. Verify Unreal Engine 5.6 is running
2. Check VibeUE plugin is enabled in Edit → Plugins
3. Reload plugin: Disable → Enable → Restart Editor
4. Check Windows Firewall settings for port 55557
5. Verify no other app is using port 55557

### "Connection timeout" or "No response"

**Causes:**
- Unreal Engine frozen or crashed
- Command too complex
- Plugin communication issue

**Solutions:**
1. Check Unreal Editor is responsive
2. Restart Unreal Engine
3. Break complex operations into smaller steps
4. Check vibe_ue.log for errors

## Blueprint Issues

### "Blueprint not found"

**Causes:**
- Using short name instead of full package path
- Incorrect path format
- Blueprint doesn't exist

**Solutions:**
1. ✅ Use `search_items()` to find exact path
2. ✅ Use package_path format: `/Game/Blueprints/BP_Player2`
3. ❌ Avoid short names: `BP_Player2`
4. ❌ Avoid duplicated paths: `/Game/Blueprints/BP_Player2.BP_Player2`

### "Compilation failed"

**Causes:**
- Broken node connections
- Wrong variable types
- Missing dependencies
- Invalid property values

**Solutions:**
1. Use `get_errors()` to see specific errors
2. Verify all node connections are valid
3. Check variable types match expectations
4. Ensure dependencies created before use
5. Use `describe` to verify node structure

### "Variable not found" or "Component not found"

**Causes:**
- Dependency order violation
- Typo in name
- Wrong Blueprint

**Solutions:**
1. **Create variables BEFORE nodes** that reference them
2. Use exact names from `list_widget_components()` or `manage_blueprint_variables(action="list")`
3. Verify you're in the correct Blueprint

## Node Issues

### "Node type not found" or "Unknown node type"

**Causes:**
- Fuzzy search matched wrong node
- Node type doesn't exist
- Wrong graph context

**Solutions:**
1. ✅ **Use `get_available_blueprint_nodes()` first**
2. ✅ **Use exact `spawner_key` from discovery**
3. Check `graph_scope` matches node type (event vs function)
4. Verify node exists in Unreal version

### "Wrong number of pins" or "Missing pins"

**Causes:**
- Missing `node_params` for variable/cast nodes
- Wrong node variant created
- Broken node_params format

**Solutions:**
1. **Variable Set**: Include `node_params={"variable_name": "VarName"}`
2. **Variable Get**: Include `node_params={"variable_name": "VarName"}`
3. **Cast Nodes**: Include `node_params={"cast_target": "/Full/Path/BP_Class.BP_Class_C"}`
4. Use `describe` to verify pin count before connecting

### "Invalid direction" error

**Causes:**
- Using "output" instead of "out"
- Typo in direction parameter

**Solutions:**
1. ✅ Use `"input"` for input parameters
2. ✅ Use `"out"` for output parameters (NOT "output"!)
3. ✅ Use `"return"` as alternative to "out"

## UMG Widget Issues

### "Widget not found"

**Causes:**
- Using duplicated object path
- Incomplete package path
- Widget doesn't exist

**Solutions:**
1. ✅ Use `search_items(asset_type="Widget")` first
2. ✅ Use `package_path` field from results (NOT `path`)
3. ✅ Format: `/Game/UI/WBP_Widget` (package path)
4. ❌ Avoid: `/Game/UI/WBP_Widget.WBP_Widget` (causes timeouts)

### "Component not found"

**Causes:**
- Typo in component name
- Component doesn't exist
- Wrong widget

**Solutions:**
1. Use `list_widget_components()` to get exact names
2. Use exact component name from list
3. Verify you're in correct widget

### "Property not found" or "Invalid property"

**Causes:**
- Property doesn't exist on widget type
- Typo in property name
- Wrong property path

**Solutions:**
1. Use `list_widget_properties()` to see available properties
2. Check component type supports the property
3. Use exact property names from list

## Common Error Patterns

| Error Message | Typical Cause | Solution |
|---------------|---------------|----------|
| "Failed to connect" | Unreal not running | Start Unreal Engine |
| "Blueprint 'BP_X' not found" | Using short name | Use full package path |
| "Invalid direction 'output'" | Wrong parameter | Use "out" not "output" |
| "Node type not found" | Fuzzy search fail | Use spawner_key from discovery |
| "Variable 'X' not found" | Created node before variable | Create variables first |
| "Property 'X' not found" | Typo or wrong component | Use list_widget_properties() |
| "Timeout receiving response" | Complex operation | Break into smaller steps |

## Diagnostic Commands

```python
# Test basic connection
check_unreal_connection()

# Find exact asset paths
search_items(search_term="BP_Player", asset_type="Blueprint")

# Get Blueprint errors
get_errors(filePaths=["/Game/Blueprints/BP_Player2.BP_Player2"])

# Inspect Blueprint structure
get_blueprint_info("/Game/Blueprints/BP_Player2")

# Verify node structure
manage_blueprint_node(
    blueprint_name="/Game/Blueprints/BP_Player",
    action="describe",
    graph_scope="function",
    function_name="MyFunction"
)

# List widget components
list_widget_components("WBP_Inventory")

# Check available properties
list_widget_properties("WBP_Inventory", "Button_Start")
```

## When All Else Fails

1. **Restart Unreal Engine** - Solves many plugin communication issues
2. **Check vibe_ue.log** - Contains detailed error messages
3. **Use get_help(topic="...")** - Get topic-specific guidance
4. **Break down the operation** - Test each step individually
5. **Verify prerequisites** - Check dependency order
