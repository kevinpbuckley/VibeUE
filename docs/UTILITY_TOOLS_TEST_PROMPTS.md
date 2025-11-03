# Utility Tools Test Prompts

This document provides comprehensive test prompts for VibeUE utility tools: `check_unreal_connection` and `get_help`.

## Overview

These are the foundational diagnostic tools for the VibeUE MCP system. They should be tested first when setting up a new environment or troubleshooting issues.

---

## Tool 1: check_unreal_connection

**Purpose**: Connection diagnostics and plugin status verification

### Test Scenario 1: Normal Connection Test

**Prompt:**
```
Test the connection to Unreal Engine using check_unreal_connection
```

**Expected Response:**
```json
{
  "success": true,
  "connection_status": "Connected successfully",
  "plugin_status": "UnrealMCP plugin is responding",
  "test_response": {...},
  "port": "55557",
  "host": "127.0.0.1"
}
```

**Validation Checklist:**
- [ ] `success` is `true`
- [ ] `connection_status` indicates successful connection
- [ ] `plugin_status` confirms plugin is responding
- [ ] Port and host information is correct

---

### Test Scenario 2: Verify Plugin Status

**Prompt:**
```
Check if the UnrealMCP plugin is properly loaded and responding
```

**Expected Behavior:**
- Tool should execute `check_unreal_connection()`
- Should return plugin status information
- Should confirm TCP connection on port 55557

**Validation Checklist:**
- [ ] Connection is established
- [ ] Plugin responds to test commands
- [ ] No connection errors reported

---

### Test Scenario 3: Troubleshooting Connection Failure

**Prompt:**
```
I'm having trouble connecting to Unreal Engine. Help me diagnose the issue.
```

**Expected Response (if Unreal is not running):**
```json
{
  "success": false,
  "connection_status": "Failed to establish connection",
  "troubleshooting": [
    "1. Verify Unreal Engine 5.6 is running",
    "2. Check that UnrealMCP plugin is loaded and enabled",
    "3. Ensure port 55557 is available",
    "4. Verify project has UnrealMCP plugin in Plugins folder"
  ]
}
```

**Validation Checklist:**
- [ ] `success` is `false`
- [ ] Troubleshooting steps are provided
- [ ] Error message is clear and actionable

---

### Test Scenario 4: Connection During Complex Operations

**Prompt:**
```
Before creating a new Blueprint, verify that Unreal Engine is connected and ready
```

**Expected Workflow:**
1. Execute `check_unreal_connection()`
2. If successful, proceed with Blueprint creation
3. If failed, display troubleshooting steps

**Validation Checklist:**
- [ ] Connection check completes before other operations
- [ ] Clear feedback on connection status
- [ ] Prevents wasted operations if connection is down

---

## Tool 2: get_help

**Purpose**: Documentation access for VibeUE MCP tools and workflows

### Test Scenario 1: Get Overview Help (Default)

**Prompt:**
```
Show me an overview of the VibeUE MCP tools
```

**Expected Response:**
```json
{
  "success": true,
  "topic": "overview",
  "content": "...",
  "source_file": "overview.md",
  "content_type": "markdown",
  "help_system": "topic-based",
  "usage": "get_help(topic='overview')"
}
```

**Validation Checklist:**
- [ ] `success` is `true`
- [ ] Topic is "overview"
- [ ] Content contains markdown documentation
- [ ] Source file path is provided

---

### Test Scenario 2: Blueprint Workflow Guide

**Prompt:**
```
I need help with the Blueprint development workflow. What's the recommended approach?
```

**Expected Tool Call:**
```python
get_help(topic="blueprint-workflow")
```

**Validation Checklist:**
- [ ] Returns blueprint-workflow.md content
- [ ] Contains workflow steps and dependencies
- [ ] Includes examples and best practices
- [ ] Explains proper tool execution order

---

### Test Scenario 3: Node Tools Reference

**Prompt:**
```
How do I create and connect Blueprint nodes? I need documentation on node tools.
```

**Expected Tool Call:**
```python
get_help(topic="node-tools")
```

**Validation Checklist:**
- [ ] Returns node-tools.md content
- [ ] Contains node creation patterns
- [ ] Explains spawner_key usage
- [ ] Covers node connections and positioning

---

### Test Scenario 4: Multi-Action Tools Reference

**Prompt:**
```
What are the multi-action tools and how do I use them?
```

**Expected Tool Call:**
```python
get_help(topic="multi-action-tools")
```

**Validation Checklist:**
- [ ] Returns multi-action-tools.md content
- [ ] Documents manage_blueprint_function
- [ ] Documents manage_blueprint_variable
- [ ] Documents manage_blueprint_component
- [ ] Documents manage_blueprint_node
- [ ] Includes action parameter examples

---

### Test Scenario 5: UMG Widget Development Guide

**Prompt:**
```
I want to create UMG widgets. Show me the UMG development guide.
```

**Expected Tool Call:**
```python
get_help(topic="umg-guide")
```

**Validation Checklist:**
- [ ] Returns umg-guide.md content
- [ ] Contains widget creation workflows
- [ ] Explains styling and hierarchy
- [ ] Covers container-specific requirements
- [ ] Includes property setting patterns

---

### Test Scenario 6: Asset Discovery and Management

**Prompt:**
```
How do I search for and manage assets in my project?
```

**Expected Tool Call:**
```python
get_help(topic="asset-discovery")
```

**Validation Checklist:**
- [ ] Returns asset-discovery.md content
- [ ] Explains search_items usage
- [ ] Covers asset path best practices
- [ ] Includes performance optimization tips
- [ ] Documents import/export workflows

---

### Test Scenario 7: Node Positioning Best Practices

**Prompt:**
```
What are the best practices for positioning Blueprint nodes? I want to create clean, readable graphs.
```

**Expected Tool Call:**
```python
get_help(topic="node-positioning")
```

**Validation Checklist:**
- [ ] Returns node-positioning.md content
- [ ] Explains left-to-right flow principles
- [ ] Covers consistent spacing standards
- [ ] Documents branch layout patterns
- [ ] Includes position calculation helpers
- [ ] Shows common anti-patterns to avoid

---

### Test Scenario 8: Component Properties Guide

**Prompt:**
```
I need to set component properties like colors and vectors. Show me the properties guide.
```

**Expected Tool Call:**
```python
get_help(topic="properties")
```

**Validation Checklist:**
- [ ] Returns properties.md content
- [ ] Explains color property formats (FColor, FLinearColor)
- [ ] Includes common color values reference
- [ ] Covers Vector/Rotator property formats
- [ ] Shows light-specific properties
- [ ] Documents common mistakes and solutions

---

### Test Scenario 9: Troubleshooting Guide

**Prompt:**
```
I'm encountering errors with Blueprint nodes. What troubleshooting steps should I follow?
```

**Expected Tool Call:**
```python
get_help(topic="troubleshooting")
```

**Validation Checklist:**
- [ ] Returns troubleshooting.md content
- [ ] Contains common error scenarios
- [ ] Provides diagnostic steps
- [ ] Explains connection issues
- [ ] Covers Blueprint/Node/UMG specific problems

---

### Test Scenario 10: List All Available Topics

**Prompt:**
```
What help topics are available?
```

**Expected Tool Call:**
```python
get_help(topic="topics")
```

**Validation Checklist:**
- [ ] Returns topics.md content
- [ ] Lists all available topics
- [ ] Includes topic descriptions
- [ ] Shows usage examples for each topic

---

### Test Scenario 11: Invalid Topic Request

**Prompt:**
```
Show me help on "nonexistent-topic"
```

**Expected Response:**
```json
{
  "success": false,
  "error": "Topic 'nonexistent-topic' not found",
  "requested_topic": "nonexistent-topic",
  "available_topics": [
    "asset-discovery",
    "blueprint-workflow", 
    "multi-action-tools",
    "node-positioning",
    "node-tools",
    "overview",
    "properties",
    "topics",
    "troubleshooting",
    "umg-guide"
  ],
  "suggestion": "Use get_help(topic='topics') to see all available topics"
}
```

**Validation Checklist:**
- [ ] `success` is `false`
- [ ] Error message is clear
- [ ] Lists available topics
- [ ] Provides helpful suggestion

---

## Integration Test Scenarios

### Integration 1: Pre-Blueprint Creation Workflow

**Prompt:**
```
I want to create a new Blueprint. First verify the connection, then show me the Blueprint workflow guide.
```

**Expected Workflow:**
1. Execute `check_unreal_connection()`
2. If successful, execute `get_help(topic="blueprint-workflow")`
3. Display workflow steps

**Validation Checklist:**
- [ ] Connection verified before accessing help
- [ ] Workflow guide displayed
- [ ] User can proceed with confidence

---

### Integration 2: Node Creation with Help

**Prompt:**
```
Before I create Blueprint nodes, show me the node tools documentation so I understand the spawner_key patterns.
```

**Expected Tool Call:**
```python
get_help(topic="node-tools")
```

**Validation Checklist:**
- [ ] Help accessed before attempting node creation
- [ ] User gets spawner_key guidance
- [ ] Prevents common node creation errors

---

### Integration 3: UMG Styling Preparation

**Prompt:**
```
I need to style a UMG widget. Show me the UMG guide first so I understand container-specific requirements.
```

**Expected Tool Call:**
```python
get_help(topic="umg-guide")
```

**Validation Checklist:**
- [ ] UMG guide accessed before styling
- [ ] Container requirements explained
- [ ] Slot vs widget properties clarified

---

### Integration 4: Troubleshooting Workflow

**Prompt:**
```
My tools are failing. First check the connection, then show me troubleshooting steps.
```

**Expected Workflow:**
1. Execute `check_unreal_connection()`
2. If connection fails, show troubleshooting from connection check
3. If connection succeeds, execute `get_help(topic="troubleshooting")`

**Validation Checklist:**
- [ ] Connection checked first
- [ ] Appropriate troubleshooting guidance provided
- [ ] Clear diagnostic path for user

---

### Integration 5: Asset Management with Discovery Guide

**Prompt:**
```
Before searching for assets, show me the asset discovery best practices.
```

**Expected Tool Call:**
```python
get_help(topic="asset-discovery")
```

**Validation Checklist:**
- [ ] Asset discovery guide accessed
- [ ] Performance tips understood
- [ ] User can make efficient asset queries

---

## Complete Test Checklist

### check_unreal_connection Tests
- [ ] Normal connection test passes
- [ ] Plugin status verification works
- [ ] Connection failure provides troubleshooting
- [ ] Connection check integrates with other operations

### get_help Tests
- [ ] Overview (default) topic works
- [ ] blueprint-workflow topic loads
- [ ] node-tools topic loads
- [ ] multi-action-tools topic loads
- [ ] umg-guide topic loads
- [ ] asset-discovery topic loads
- [ ] node-positioning topic loads
- [ ] properties topic loads
- [ ] troubleshooting topic loads
- [ ] topics topic loads (list all topics)
- [ ] Invalid topic request handled gracefully
- [ ] All topics return markdown content
- [ ] File paths are correct

### Integration Tests
- [ ] Connection check before complex operations
- [ ] Help accessed before specific tool usage
- [ ] Troubleshooting workflow combines both tools
- [ ] Documentation guides efficient tool usage

---

## Notes for Test Engineers

### Testing Environment Requirements
- Unreal Engine 5.6+ must be running
- VibeUE plugin must be enabled
- MCP server must be connected
- Port 55557 must be available

### Testing Best Practices
1. **Always start with connection test** - Verify environment before other tests
2. **Test help topics in order** - Start with overview, then specific topics
3. **Validate content quality** - Ensure markdown is properly formatted
4. **Check error handling** - Test invalid inputs and failure scenarios
5. **Integration testing** - Verify tools work together in real workflows

### Common Issues During Testing
1. **Connection failures** - Ensure Unreal is running and plugin is enabled
2. **Topic not found** - Check that .md files exist in resources/topics/
3. **Path resolution** - Help system searches up directory tree for topics
4. **Encoding issues** - All topic files should be UTF-8 encoded

### Success Criteria
- ✅ All test scenarios pass
- ✅ Connection diagnostics work reliably
- ✅ All 10 help topics are accessible
- ✅ Error handling is graceful and helpful
- ✅ Integration with other tools is seamless
- ✅ Documentation is accurate and complete

---

## Appendix: Available Help Topics

| Topic | Description | Use Case |
|-------|-------------|----------|
| `overview` | General VibeUE overview | First-time users, quick reference |
| `blueprint-workflow` | Blueprint development guide | Creating/modifying Blueprints |
| `node-tools` | Node creation reference | Working with Blueprint nodes |
| `node-positioning` | Node positioning best practices | Creating clean, readable graphs |
| `multi-action-tools` | Multi-action tool patterns | Using complex tools with actions |
| `properties` | Component property setting guide | Setting colors, vectors, rotators |
| `umg-guide` | UMG widget development | Creating/styling widgets |
| `asset-discovery` | Asset management | Finding and managing assets |
| `troubleshooting` | Problem solving | Diagnosing and fixing issues |
| `topics` | List all topics | Discovery of available help |

---

## Version History

- **v1.0** (2025-11-03): Initial test prompts for utility tools
  - check_unreal_connection test scenarios
  - get_help test scenarios for all 10 topics
  - Integration test workflows
  - Complete test checklist
