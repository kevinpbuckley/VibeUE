# VibeUE Test Prompts

## Purpose

This directory contains standardized test prompts for validating VibeUE MCP tools. These prompts are designed to be used with AI assistants (VS Code with MCP extension, Claude Desktop, Cursor, Windsurf, etc.) to systematically test and verify tool functionality.

## How to Use Test Prompts

### Prerequisites

Before running any test prompts, ensure:

1. **Unreal Engine is running** with your project loaded
2. **VibeUE plugin is enabled** in your Unreal Engine project (Edit > Plugins > VibeUE)
3. **MCP client is configured** (VS Code, Claude Desktop, Cursor, or Windsurf)
4. **MCP server is connected** to Unreal Engine (verify with `check_unreal_connection`)

### Running Test Prompts

1. **Select a test prompt** from one of the category folders:
   - `blueprint/` - Blueprint manipulation tools
   - `umg/` - UMG widget tools
   - `assets/` - Asset management tools
   - `utilities/` - Utility and diagnostic tools

2. **Open the test prompt file** in your preferred text editor

3. **Copy the test steps** from the "Test Steps" section

4. **Paste into your AI assistant** and follow the instructions

5. **Compare results** against the "Expected Outcomes" section

6. **Document any discrepancies** between expected and actual results

### Test Prompt Format Conventions

Each test prompt follows a standardized format:

```markdown
# Tool Name - Test Prompt

## Purpose
Brief description of what this test validates

## Prerequisites
- Specific requirements for this test
- Required Unreal Engine setup
- Any pre-existing assets or blueprints needed

## Test Steps
1. Step-by-step instructions
2. Commands to execute
3. Actions to perform

## Expected Outcomes
- What should happen
- Expected return values
- Visual or behavioral results
```

### Tracking Results

When testing, track:

- ✅ **Pass**: Tool behaves as expected
- ⚠️ **Partial**: Tool works but with unexpected behavior or warnings
- ❌ **Fail**: Tool does not work or produces errors
- 📝 **Notes**: Any additional observations or edge cases discovered

## Test Categories

### Blueprint Tools (`blueprint/`)

Test prompts for Blueprint lifecycle, components, functions, variables, and nodes:
- `manage_blueprint.md` - Blueprint creation, compilation, and property management
- `manage_blueprint_component.md` - Component addition, removal, and hierarchy
- `manage_blueprint_function.md` - Function creation and manipulation
- `manage_blueprint_variable.md` - Variable management
- `manage_blueprint_node.md` - Node creation and graph manipulation

### UMG Tools (`umg/`)

Test prompts for UMG widget creation and manipulation:
- `manage_umg_widget.md` - Widget blueprint creation, component addition, and styling

### Asset Tools (`assets/`)

Test prompts for asset discovery and management:
- `manage_asset.md` - Asset search, import, export, and manipulation

### Utility Tools (`utilities/`)

Test prompts for system diagnostics and help:
- `check_unreal_connection.md` - Connection verification
- `get_help.md` - Help system and documentation access

## Best Practices

### Before Testing
- Start with `check_unreal_connection` to verify setup
- Use `get_help` to understand tool parameters
- Review tool documentation in `Docs/` directory

### During Testing
- Test one tool at a time
- Follow test steps exactly as written
- Note any deviations from expected behavior
- Test with both valid and invalid inputs (error handling)

### After Testing
- Document results with specific details
- Report bugs with reproducible steps
- Suggest improvements to test prompts or tools
- Update test prompts if expected behavior changes

## Contributing Test Prompts

When adding new test prompts:

1. **Follow the standard template** structure
2. **Be specific** about prerequisites and setup
3. **Include clear steps** that are easy to follow
4. **Define measurable outcomes** that can be verified
5. **Test your test prompt** before committing

## Integration with Development Workflow

Test prompts serve multiple purposes:

- **Validation**: Verify new features work as intended
- **Regression Testing**: Ensure updates don't break existing functionality
- **Documentation**: Provide usage examples for developers and users
- **Onboarding**: Help new users understand tool capabilities

## Additional Resources

- **Main Documentation**: `/Docs/README.md`
- **Tool Documentation**: `/Docs/Tools/README.md`
- **Python Server**: `/Python/vibe-ue-main/Python/README.md`
- **Topic Guides**: `/Python/vibe-ue-main/Python/resources/topics/`

## Support

For questions or issues with test prompts:
- Review the main VibeUE documentation
- Check the Discord community: https://discord.gg/hZs73ST59a
- Open an issue on GitHub with the `testing` label
