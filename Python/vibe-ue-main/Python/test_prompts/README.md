# Test Prompts

This directory contains comprehensive test prompts for VibeUE MCP tools.

## Purpose

Test prompts serve as:
- **Documentation**: Clear examples of how to use each tool action
- **Testing Guide**: Step-by-step workflows to validate tool functionality
- **Training Material**: Reference for AI assistants and developers
- **Validation Suite**: Acceptance criteria for tool implementations

## Structure

Each test prompt file follows this structure:

1. **Overview**: Description of the tool and test coverage
2. **Prerequisites**: Setup requirements before testing
3. **Test Workflow**: Numbered steps with prompts and expected results
4. **Additional Test Cases**: Edge cases and advanced scenarios
5. **Summary**: Verification of coverage and acceptance criteria

## Available Test Prompts

- **manage_blueprint_function_test_prompts.md**: Tests all 13 actions for Blueprint function management
  - Function lifecycle (create, delete, get, list)
  - Parameter management (add, update, remove, list)
  - Local variable management (add, update, remove, list)
  - Function property updates

## How to Use

### For Manual Testing

1. Open Unreal Engine with your test project
2. Ensure the VibeUE plugin is enabled
3. Connect your MCP client (e.g., Claude Desktop)
4. Follow the prompts in sequence
5. Verify expected results match actual outputs

### For AI Assistants

When testing a tool:
1. Reference the appropriate test prompt file
2. Follow the workflow steps in order
3. Use the exact tool calls provided
4. Validate results against expected outputs
5. Report any discrepancies

### For Developers

When adding a new tool or action:
1. Create a corresponding test prompt file
2. Cover all actions/variations
3. Include edge cases
4. Document expected results
5. Update this README

## Test Prompt Format

Each test step includes:

```markdown
### Step N: Action Description (Action: action_name)

**Prompt:**
Natural language description of what to do

**Tool Call:**
```python
tool_name(
    param1="value1",
    param2="value2"
)
```

**Expected Result:**
```json
{
    "success": true,
    "field": "value"
}
```

**Validation:**
- Check point 1
- Check point 2
```

## Coverage Goals

Each test prompt file should:
- ✅ Cover all available actions
- ✅ Test both success and failure cases
- ✅ Include parameter validation
- ✅ Demonstrate common workflows
- ✅ Show integration patterns
- ✅ Document critical notes and gotchas

## Contributing

When adding test prompts:
1. Follow the established format
2. Use realistic examples
3. Include complete workflows
4. Document expected behaviors
5. Add validation checkpoints
6. Keep prompts clear and concise

## Related Documentation

- [Main README](../README.md) - MCP server overview
- [Tools Documentation](../tools/) - Individual tool implementations
- [Resources](../resources/) - Additional guides and topics
