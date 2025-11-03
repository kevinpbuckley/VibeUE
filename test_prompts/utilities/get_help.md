# get_help - Test Prompt

## Purpose

This test validates the help system that provides documentation, usage examples, and guidance for VibeUE MCP tools. The help system is crucial for understanding tool capabilities and proper usage patterns.

## Prerequisites

- VibeUE plugin is installed
- MCP client is connected to the VibeUE server
- Connection verified with `check_unreal_connection` (Unreal Engine should be running)

## Test Steps

### Test 1: Get General Help

1. Ask your AI assistant: "Get help for VibeUE tools"

2. Review the general help information returned

### Test 2: Get Help for Specific Tool

3. Ask your AI assistant: "Get help for the 'create_blueprint' tool"

4. Review the tool-specific documentation

### Test 3: Get Help with Topic - UMG Guide

5. Ask your AI assistant: "Get help with topic 'umg-guide'"

6. Review the UMG styling and workflow guidance

### Test 4: Get Help with Topic - Blueprint Workflow

7. Ask your AI assistant: "Get help with topic 'blueprint-workflow'"

8. Review the Blueprint creation workflow guidance

### Test 5: Get Help with Topic - Node Tools

9. Ask your AI assistant: "Get help with topic 'node-tools'"

10. Review the node manipulation guidance

### Test 6: Get Help with Topic - Node Positioning

11. Ask your AI assistant: "Get help with topic 'node-positioning'"

12. Review the node positioning guidance

### Test 7: List Available Topics

13. Ask your AI assistant: "What help topics are available in VibeUE?"

14. Review the list of available help topics

### Test 8: Get Help with Topic - Troubleshooting

15. Ask your AI assistant: "Get help with topic 'troubleshooting'"

16. Review the troubleshooting guidance

### Test 9: Get Help with Topic - Properties

17. Ask your AI assistant: "Get help with topic 'properties'"

18. Review the property setting guidance

### Test 10: Get Help for Multiple Tools

19. Ask your AI assistant: "Get help for 'add_component' and 'set_component_property'"

20. Review if multiple tool help is provided

### Test 11: Error Handling - Invalid Topic

21. Ask your AI assistant: "Get help with topic 'nonexistent-topic'"

22. Verify appropriate error handling or suggestion

### Test 12: Error Handling - Invalid Tool Name

23. Ask your AI assistant: "Get help for 'invalid_tool_name'"

24. Verify appropriate error handling

## Expected Outcomes

### Test 1 - General Help
- ✅ Returns overview of VibeUE capabilities
- ✅ Lists available tools by category
- ✅ Provides quick start information
- ✅ May include links to additional documentation
- ✅ Formatted clearly and readable

### Test 2 - Tool-Specific Help
- ✅ Returns detailed information about 'create_blueprint'
- ✅ Shows tool parameters and their types
- ✅ Provides usage examples
- ✅ Explains return values
- ✅ Includes common use cases
- ✅ May include warnings or important notes

### Test 3 - UMG Guide Topic
- ✅ Returns comprehensive UMG styling guide
- ✅ Includes DO's and DON'Ts for widget styling
- ✅ Provides property setting syntax examples
- ✅ Explains alignment enum values
- ✅ Covers container-specific requirements
- ✅ Includes compilation requirements

### Test 4 - Blueprint Workflow Topic
- ✅ Returns Blueprint creation workflow guidance
- ✅ Explains step-by-step process
- ✅ Provides best practices
- ✅ May include common patterns

### Test 5 - Node Tools Topic
- ✅ Returns node manipulation guidance
- ✅ Explains node creation and connection
- ✅ Provides examples of common node operations
- ✅ Includes node type information

### Test 6 - Node Positioning Topic
- ✅ Returns node positioning guidance
- ✅ Explains coordinate system
- ✅ Provides positioning strategies
- ✅ May include layout best practices

### Test 7 - List Topics
- ✅ Returns list of available help topics
- ✅ Topics include: umg-guide, blueprint-workflow, node-tools, node-positioning, properties, troubleshooting
- ✅ May include brief description of each topic
- ✅ Formatted as a clear list

### Test 8 - Troubleshooting Topic
- ✅ Returns troubleshooting guidance
- ✅ Covers common issues
- ✅ Provides solutions or workarounds
- ✅ Includes diagnostic steps

### Test 9 - Properties Topic
- ✅ Returns property setting guidance
- ✅ Explains property types and values
- ✅ Provides syntax examples
- ✅ Covers type conversion and validation

### Test 10 - Multiple Tools
- ✅ Returns help for both tools (if supported)
- ✅ Each tool's help is clearly separated
- ✅ Information is complete for both
- ✅ Or suggests requesting help for one tool at a time

### Test 11 - Invalid Topic
- ✅ Clear error message indicating topic not found
- ✅ Lists available topics or suggests using topic list
- ✅ No crash or unexpected behavior
- ✅ Helpful suggestion for finding the right topic

### Test 12 - Invalid Tool Name
- ✅ Clear error message indicating tool not found
- ✅ May suggest similar tool names
- ✅ May suggest using general help to see all tools
- ✅ No crash or unexpected behavior

## Notes

- The help system is available even when Unreal Engine is not running
- Help topics provide workflow guidance beyond individual tool documentation
- The `get_help` tool is one of the most important diagnostic tools
- Topic names are case-sensitive in some implementations
- Available topics include:
  - **umg-guide**: Comprehensive UMG widget styling guide
  - **blueprint-workflow**: Blueprint creation and manipulation workflows
  - **node-tools**: Node creation and graph manipulation
  - **node-positioning**: Node positioning strategies
  - **properties**: Property setting syntax and types
  - **troubleshooting**: Common issues and solutions
  - **asset-discovery**: Asset search and management
  - **multi-action-tools**: Tools that perform multiple operations
  - **overview**: General overview and capabilities
  - **topics**: List of all available topics
- Help content is stored in markdown files in the `Python/vibe-ue-main/Python/resources/topics/` directory
- Help is designed for AI assistants but is human-readable

## Best Practices

- **Always start** with `get_help` when learning a new tool
- **Use topic help** for understanding workflows, not just individual tools
- **Reference help** when you encounter errors with a tool
- **Check troubleshooting topic** when things aren't working as expected
- **Use umg-guide** before any UMG widget styling work
- **Review properties topic** when setting properties to understand value formats

## Success Criteria

You should be able to:
- ✅ Get general help about VibeUE
- ✅ Get specific help for individual tools
- ✅ Access workflow guidance through topics
- ✅ Find troubleshooting information
- ✅ List all available help topics
- ✅ Understand tool parameters and usage from help text
