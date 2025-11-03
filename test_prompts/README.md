# VibeUE MCP Test Prompts - Quick Validation

## Overview
Quick validation prompts to verify VibeUE MCP tools are working. Each file has minimal tests for fast smoke testing.

## Purpose
- **Quick Validation**: Verify tools work after code changes
- **Smoke Tests**: Basic functionality checks
- **AI Testing**: Simple prompts for AI assistants to execute

## How to Use

### Prerequisites
1. ✅ Unreal Engine 5.6+ running
2. ✅ VibeUE plugin loaded
3. ✅ MCP connection active

### Running Tests
1. Open test prompt file
2. Copy the validation command
3. Paste into AI chat
4. Verify it works (no errors)
6. **Document failures** if actual results differ

### Test Prompt Structure
Each test prompt follows this format:

```markdown
## Test N: [Test Name]
**Purpose**: [What this test validates]

### Prerequisites
- [Specific requirements for this test]

### Steps
1. [Step 1 with MCP tool call]
2. [Step 2 with MCP tool call]
...

### Expected Outcomes
- ✅ [Expected result 1]
- ✅ [Expected result 2]
...

### Cleanup
- [Steps to undo test changes]
```

## ⚡ Quick Start

**Want to validate all tools in 30 seconds?**  
See [QUICK_START.md](./QUICK_START.md) for one-liner tests!

---

## Test Prompt Categories

### Blueprint Tools
Test prompts for Blueprint manipulation and introspection:

| Tool | File | Actions Tested |
|------|------|----------------|
| `manage_blueprint` | [blueprint/manage_blueprint.md](blueprint/manage_blueprint.md) | create, get_info, get_property, set_property, compile, reparent, list_custom_events, summarize_event_graph (8 actions) |
| `manage_blueprint_component` | [blueprint/manage_blueprint_component.md](blueprint/manage_blueprint_component.md) | search_types, get_info, get_property_metadata, list, create, get/set_property, get_all_properties, compare_properties, reorder, reparent, delete (12 actions) |
| `manage_blueprint_function` | [blueprint/manage_blueprint_function.md](blueprint/manage_blueprint_function.md) | list, get, list_params, create, add_param, update_param, remove_param, list_locals, add_local, update_local, remove_local, update_properties, delete (13 actions) |
| `manage_blueprint_variable` | [blueprint/manage_blueprint_variable.md](blueprint/manage_blueprint_variable.md) | search_types, get_info, create, get_property, set_property, list, delete (7 actions) |
| `manage_blueprint_node` | [blueprint/manage_blueprint_node.md](blueprint/manage_blueprint_node.md) | discover, create, connect_pins, disconnect_pins, delete, move, list, describe, get_details, configure, split_pins, recombine_pins, refresh_node, refresh_nodes, reset_pin_defaults (15 actions) |

### UMG Widget Tools
Test prompts for UMG widget development:

| Tool | File | Actions Tested |
|------|------|----------------|
| `manage_umg_widget` | [umg/manage_umg_widget.md](umg/manage_umg_widget.md) | list_components, add_component, remove_component, validate, search_types, get_component_properties, get_property, set_property, list_properties, get_available_events, bind_events (11 actions) |

### Asset Management Tools
Test prompts for asset operations:

| Tool | File | Actions Tested |
|------|------|----------------|
| `manage_asset` | [assets/manage_asset.md](assets/manage_asset.md) | search, import_texture, export_texture, open_in_editor, svg_to_png (5 actions) |

### Utility Tools
Test prompts for connection and help tools:

| Tool | File | Actions Tested |
|------|------|----------------|
| `check_unreal_connection` | [utilities/check_unreal_connection.md](utilities/check_unreal_connection.md) | Connection test, plugin status, troubleshooting (4 scenarios) |
| `get_help` | [utilities/get_help.md](utilities/get_help.md) | overview, blueprint-workflow, node-tools, multi-action-tools, umg-guide, asset-discovery, troubleshooting, topics (11 scenarios) |

## Testing Workflows

### Complete Tool Validation
To fully validate VibeUE tools:
1. Start with utilities (connection + help)
2. Test Blueprint tools in order (manage_blueprint → components → functions → variables → nodes)
3. Test UMG widget tools
4. Test asset management tools

### Integration Testing
Some workflows test multiple tools together:
- Blueprint + Components + Functions + Nodes = Complete Blueprint development
- UMG Widgets + Assets (textures) = UI development with styling
- get_help + any tool = Documentation-driven development

## Expected Outcomes vs Actual Results

### Validation Checklist
For each test, verify:
- ✅ Tool executes without errors
- ✅ Return values match expected types
- ✅ Unreal Engine state changes as expected
- ✅ Error handling works correctly (test invalid inputs)
- ✅ Cleanup steps restore original state

### Documenting Failures
If a test fails:
1. **Capture error message** from MCP tool response
2. **Note Unreal Engine version** and VibeUE plugin version
3. **Document reproduction steps** clearly
4. **Create GitHub issue** with test prompt reference
5. **Tag issue** with `testing` and `bug` labels

## Troubleshooting

### Connection Issues
If tools fail with "Failed to connect":
1. Check Unreal Engine is running
2. Verify VibeUE plugin is loaded (check Plugin window)
3. Test with `check_unreal_connection` tool
4. Restart Unreal Engine if needed
5. Check MCP server logs

### Tool-Specific Issues
If a specific tool fails:
1. Use `get_help(topic="troubleshooting")` for guidance
2. Check prerequisites in test prompt
3. Verify asset names are exact (case-sensitive)
4. Try simpler test case first
5. Consult tool documentation in `get_help(topic="[relevant-topic]")`

### Test Environment Issues
If tests behave inconsistently:
1. Use a clean test project (not production)
2. Restart Unreal Engine between test runs
3. Delete test Blueprints/Widgets after each run
4. Check for Blueprint compilation errors
5. Verify plugin updates are installed

## Contributing

### Adding New Test Prompts
When adding tests for new tools:
1. Follow the test prompt structure above
2. Test all tool actions comprehensively
3. Include cleanup steps
4. Add to appropriate category directory
5. Update this README's test table

### Improving Existing Tests
To improve test coverage:
1. Add edge case scenarios
2. Test error handling paths
3. Add integration tests with other tools
4. Clarify expected outcomes
5. Submit PR with improved tests

## References
- **VibeUE Documentation**: `Plugins/VibeUE/docs/`
- **MCP Tool Reference**: Use `get_help(topic="topics")` to list all topics
- **GitHub Issues**: [kevinpbuckley/VibeUE/issues](https://github.com/kevinpbuckley/VibeUE/issues)
- **Phase 5 Testing**: Issues #69-77

---

**Last Updated**: November 3, 2025  
**Phase**: Phase 5 - Testing & Validation  
**Status**: Complete test prompt structure ready for validation
