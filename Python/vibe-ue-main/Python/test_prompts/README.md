# Test Prompts Directory

## Purpose

This directory contains comprehensive test prompts for VibeUE MCP tools. Test prompts serve as:

1. **AI Assistant Testing Guides**: Step-by-step instructions for testing tool functionality
2. **Documentation Examples**: Real-world usage patterns for each tool action
3. **Validation Workflows**: Complete test sequences to verify tool correctness
4. **Learning Resources**: Educational material for understanding tool capabilities

## Structure

Each test prompt file follows this format:

```
tool_name_test_prompts.md
```

### Test Prompt File Contents

Each test prompt document includes:

- **Overview**: Purpose and scope of tests
- **Test Workflow Summary**: High-level test sequence
- **Test Prompts by Action**: Detailed prompts for each tool action
- **Complete Workflow Example**: End-to-end integration test
- **Advanced Test Cases**: Complex scenarios and edge cases
- **Acceptance Criteria**: Success criteria checklist
- **Testing Results Template**: Format for recording test outcomes
- **Common Issues and Solutions**: Troubleshooting guide
- **Best Practices**: Recommended testing approaches

## Available Test Prompts

### manage_umg_widget_test_prompts.md
**Tool**: `manage_umg_widget`  
**Actions Tested**: 11  
**Status**: ✅ Complete

Tests all UMG widget management actions:
- Component lifecycle (list, add, remove, validate)
- Type discovery (search_types, get_component_properties)
- Property management (get, set, list)
- Event management (get_available_events, bind_events)

**Special Focus**:
- Widget vs. slot property differentiation
- Event binding workflows
- Hierarchy validation
- Complete integration testing

## How to Use Test Prompts

### For AI Assistants

1. **Read the test prompt document** for the tool you want to test
2. **Follow the workflow sequence** provided in the document
3. **Execute each test prompt** as described
4. **Record results** using the testing results template
5. **Compare against expected results** for each action

### For Developers

1. **Reference test prompts** when writing tool documentation
2. **Use workflows** to validate tool changes
3. **Add new test cases** as new scenarios are discovered
4. **Update acceptance criteria** when tool features change

### Example Usage

To test the `manage_umg_widget` tool:

```markdown
1. Open manage_umg_widget_test_prompts.md
2. Start with "Action: search_types" section
3. Copy the test prompt
4. Execute the prompt with your AI assistant
5. Verify the expected result matches actual result
6. Mark the action as ✅ Pass or ❌ Fail in the results table
7. Continue with the next action
```

## Test Prompt Development Guidelines

When creating new test prompt files:

### Required Sections

1. **Overview** - Purpose, date, tool name, total actions
2. **Test Workflow Summary** - High-level sequence
3. **Test Prompts by Action** - One section per action with:
   - Purpose
   - Test prompt (natural language)
   - Expected MCP call (code)
   - Expected result (description)
4. **Complete Workflow Example** - Full integration test
5. **Acceptance Criteria Checklist** - Success criteria

### Optional Sections

- Advanced test cases
- Common issues and solutions
- Best practices
- Related documentation
- Success metrics

### Formatting Standards

- Use clear markdown headers (##, ###)
- Include code blocks for MCP calls
- Use checklists for acceptance criteria
- Add emojis for visual organization (🎯, ✅, ❌, etc.)
- Include test results templates

## Testing Philosophy

### Comprehensive Coverage
- Test **all** actions for multi-action tools
- Include **edge cases** and error scenarios
- Cover **common workflows** and use patterns

### Clear Documentation
- Write prompts in **natural language** for AI assistants
- Provide **expected results** for validation
- Include **troubleshooting** for common issues

### Practical Examples
- Use **realistic** widget/component names
- Follow **best practices** from tool documentation
- Demonstrate **real-world** workflows

## Contributing Test Prompts

To add a new test prompt file:

1. **Create a new markdown file** named `{tool_name}_test_prompts.md`
2. **Follow the standard structure** outlined above
3. **Test all actions** for the tool
4. **Include acceptance criteria** based on requirements
5. **Add entry** to this README's "Available Test Prompts" section
6. **Submit for review**

## Related Documentation

- **Python/tools/**: Tool implementation source code
- **Python/resources/help.md**: Complete tool reference
- **Python/resources/topics/**: Topic-specific guides
- **Docs/**: Project-wide documentation

## Version History

- **v1.0** (November 3, 2025): Initial structure with manage_umg_widget test prompts

---

**Note**: This directory is part of Phase 5, Task 7 of the VibeUE development roadmap. Test prompts help ensure tool quality and provide valuable documentation for AI-assisted development workflows.
