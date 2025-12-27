# manage_enhanced_input

Manage the Enhanced Input system in Unreal Engine - create input actions, mapping contexts, key bindings, modifiers, and triggers.

## Summary

The `manage_enhanced_input` tool provides comprehensive management of Unreal Engine's Enhanced Input system. It allows you to create input actions, set up mapping contexts, configure key bindings with modifiers and triggers, and discover available input types.

## Actions

| Action | Description |
|--------|-------------|
| action_create | Create a new Input Action asset |
| action_list | List all Input Actions in the project |
| action_get_properties | Get properties of an Input Action |
| action_configure | Configure Input Action settings |
| mapping_create_context | Create a new Input Mapping Context |
| mapping_list_contexts | List all Input Mapping Contexts |
| mapping_add_key_mapping | Add a key mapping to a context |
| mapping_get_mappings | Get all mappings in a context |
| mapping_remove_mapping | Remove a key mapping from a context |
| mapping_add_modifier | Add a modifier to a key mapping |
| mapping_remove_modifier | Remove a modifier from a key mapping |
| mapping_get_modifiers | Get modifiers on a key mapping |
| mapping_add_trigger | Add a trigger to a key mapping |
| mapping_remove_trigger | Remove a trigger from a key mapping |
| mapping_get_triggers | Get triggers on a key mapping |
| mapping_get_available_keys | Get list of available input keys |
| mapping_get_available_modifier_types | Get available modifier types |
| mapping_get_available_trigger_types | Get available trigger types |
| reflection_discover_types | Discover available Enhanced Input types |

## Usage

### Create Input Action
```json
{
  "Action": "action_create",
  "ParamsJson": "{\"Path\": \"/Game/Input/IA_Move\", \"ValueType\": \"Axis2D\"}"
}
```

### Create Mapping Context
```json
{
  "Action": "mapping_create_context",
  "ParamsJson": "{\"Path\": \"/Game/Input/IMC_Default\"}"
}
```

### Add Key Binding
```json
{
  "Action": "mapping_add_key_mapping",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\", \"ActionPath\": \"/Game/Input/IA_Jump\", \"Key\": \"SpaceBar\"}"
}
```

## Efficient Workflow

### Best Practice: Use Returned Asset Paths
When you create an asset, the response includes the exact path to use in subsequent operations. **Use it directly - don't search for it!**

```python
# Step 1: Create the action - response includes full asset_path
result = manage_enhanced_input(action="action_create", action_name="IA_Sprint", 
                               asset_path="/Game/Input", value_type="Digital")
# Response: {"asset_path": "/Game/Input/IA_Sprint.IA_Sprint", ...}

# Step 2: Use the returned path directly in the next call
manage_enhanced_input(action="mapping_add_key_mapping",
                     context_path="/Game/Input/IMC_Default.IMC_Default",
                     action_path="/Game/Input/IA_Sprint.IA_Sprint",  # From response above
                     key="LeftShift")
```

### Common Mistake: Unnecessary Searches
❌ **WRONG**: Creating an action, then searching for it, then binding it (3+ calls)
✅ **RIGHT**: Creating an action and immediately using the returned path to bind it (2 calls)

### Workflow for Binding to Existing Actions
If binding to an existing action (not one you just created), call `action_list` first to get exact paths:
```python
# First, get the list of existing actions
manage_enhanced_input(action="action_list")
# Then use the paths from the response for binding
```

### Example: Create DoubleJump bound to LeftShift (2 tool calls, NOT 15)
```python
# Step 1: Create the action
result = manage_enhanced_input(action="action_create", action_name="IA_DoubleJump", 
                               asset_path="/Game/Input/Actions", value_type="Digital")
# Response includes: asset_path="/Game/Input/Actions/IA_DoubleJump.IA_DoubleJump"

# Step 2: Bind to existing context (use the asset_path from step 1)
manage_enhanced_input(action="mapping_add_key_mapping",
                     context_path="/Game/Variant_Horror/Input/IMC_Horror.IMC_Horror",
                     action_path="/Game/Input/Actions/IA_DoubleJump.IA_DoubleJump",
                     key="LeftShift")
```

## Notes

- Enhanced Input replaces the legacy input system in UE5
- Input Actions define the type of input (button, axis, etc.)
- Mapping Contexts group related input mappings
- Modifiers transform input values (negate, scale, etc.)
- Triggers determine when input events fire
