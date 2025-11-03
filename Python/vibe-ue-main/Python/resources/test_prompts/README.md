# Test Prompts for VibeUE Tools

This directory contains comprehensive test prompts for validating VibeUE MCP tools functionality. Each file provides example usage patterns, expected behaviors, and multi-step workflows.

## Purpose

Test prompts serve multiple purposes:
- **Documentation**: Show realistic usage patterns for each tool action
- **Testing**: Provide structured scenarios for manual and automated testing
- **Training**: Help AI assistants understand proper tool usage
- **Validation**: Verify all tool actions work as expected

## Structure

Each test prompt file follows this format:

```
# Tool Name Test Prompts

## Test N: [Test Description]
# Test: What this test validates
# Action: The specific action being tested
# Expected: Expected result/behavior

[Natural language prompt or code example]
```

## Files

- **manage_asset_test_prompts.txt** - Tests for all 5 manage_asset actions:
  - `search` - Asset discovery across multiple types
  - `import_texture` - Import with various optimization settings
  - `export_texture` - Export at different resolutions
  - `open_in_editor` - Open assets in Unreal editors
  - `svg_to_png` - SVG conversion with size/background options

## Usage

### For Manual Testing

1. Open the test prompt file
2. Copy a test scenario
3. Execute in your MCP client (Claude Desktop, etc.)
4. Verify the expected behavior

### For Automated Testing

Test prompts can be parsed and executed programmatically:

```python
# Parse test prompts
with open('manage_asset_test_prompts.txt') as f:
    tests = parse_test_prompts(f.read())

# Execute each test
for test in tests:
    result = execute_test(test)
    validate_result(result, test.expected)
```

### For AI Training

Include test prompts in AI context to demonstrate proper tool usage:

```
Here are example usage patterns for manage_asset:
[Include relevant test prompts]
```

## Test Categories

### Individual Action Tests
Test each action independently with various parameter combinations.

### Workflow Tests
Multi-step scenarios that combine multiple actions:
- Search → Open in Editor
- Search → Export for Analysis
- SVG Convert → Import Texture

### Edge Cases
- Empty search results
- Invalid asset paths
- Missing file paths
- Unsupported formats

## Adding New Test Prompts

When adding a new test prompt file:

1. **Follow the naming convention**: `{tool_name}_test_prompts.txt`
2. **Include comprehensive coverage**: Test all actions/parameters
3. **Add workflow scenarios**: Show realistic multi-step usage
4. **Document expectations**: Clearly state expected behavior
5. **Update this README**: Add the new file to the Files section

## Acceptance Criteria Template

Each test prompt file should end with a summary:

```
## Summary
# Total Tests: X
# Actions Covered: [list of actions]
# Workflow Tests: [number of multi-step scenarios]
# 
# Acceptance Criteria Met:
# ✅ [Criterion 1]
# ✅ [Criterion 2]
# ...
```

## Dependencies

Test prompts reference Issue #69 which established the test_prompts structure pattern.

## Related Resources

- [Multi-Action Tools Guide](../topics/multi-action-tools.md) - Complete reference for all multi-action tools
- [Asset Discovery Guide](../topics/asset-discovery.md) - Asset search patterns and best practices
- [Help Documentation](../help.md) - General VibeUE tool usage

## Contributing

When creating test prompts:
- Use realistic file paths and asset names
- Include both success and edge case scenarios
- Provide clear expected outcomes
- Test all parameter variations
- Add multi-tool workflow examples
