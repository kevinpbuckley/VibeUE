# Test Prompts Directory

This directory contains comprehensive test prompts for VibeUE MCP tools.

## Purpose

Test prompts provide:
- **Complete action coverage** - Every action in multi-action tools tested
- **Realistic workflows** - Real-world usage patterns demonstrated
- **Best practices** - Critical patterns and anti-patterns highlighted
- **Type discovery** - Proper workflows for finding correct type paths
- **Validation examples** - Expected results documented

## Available Test Prompts

### manage_blueprint_variable.md
Complete test coverage for Blueprint variable management tool.

**Actions covered:**
1. `search_types` - Discover available variable types
2. `create` - Create new variables with proper typing
3. `list` - List and filter variables
4. `get_info` - Get detailed variable information
5. `get_property` - Get specific property values
6. `set_property` - Modify variable properties
7. `delete` - Remove variables with reference checking

**Key workflows demonstrated:**
- Type path discovery pattern (CRITICAL)
- Creating primitive variables (float, int, bool)
- Creating object variables (UserWidget, SoundBase)
- Variable metadata manipulation
- Category-based organization

## How to Use Test Prompts

1. **Read through the complete workflow** - Understand the pattern before testing
2. **Follow the order** - Tests build on each other (e.g., create before list)
3. **Check expected results** - Verify each action produces expected output
4. **Adapt to your needs** - Use as templates for real workflows

## Test Prompt Format

Each test prompt file includes:

```markdown
## Action X: action_name - Description

### Test Xa: Specific Test Case

\`\`\`python
# Code example with comments
manage_blueprint_variable(
    blueprint_name="BP_Test",
    action="action_name",
    ...
)
\`\`\`

**Expected Result**:
- What should happen
- Verification steps
```

## Contributing

When adding new test prompts:
1. Cover ALL actions for the tool
2. Include realistic workflows
3. Document expected results
4. Highlight critical patterns with ✅/❌
5. Include both positive and edge cases

## Related Documentation

- `../topics/multi-action-tools.md` - Multi-action tool reference
- `../topics/blueprint-workflow.md` - Blueprint workflow patterns
- Tool source code in `../../tools/`
