# Utility Tools Test Prompts

## Overview
This document combines test prompts for both utility tools: `check_unreal_connection` and `get_help`.

---

# check_unreal_connection Tests

## Prerequisites
- ✅ MCP server running
- ✅ Unreal Engine installed (may or may not be running for different tests)

## Test 1: Normal Connection Test

**Purpose**: Verify connection when everything is working

### Steps

1. **Start Unreal Engine**
   ```
   Launch Unreal Engine 5.6+ with project that has VibeUE plugin
   ```

2. **Load VibeUE Plugin**
   ```
   Ensure plugin is enabled and active
   ```

3. **Check Connection**
   ```
   Use check_unreal_connection tool
   ```

### Expected Outcomes
- ✅ success: true
- ✅ connection_status: "Connected"
- ✅ plugin_status: "Active" or "Loaded"
- ✅ No error messages
- ✅ Unreal Engine version shown
- ✅ Plugin version shown

---

## Test 2: Plugin Status Verification

**Purpose**: Get detailed plugin status information

### Steps

1. **Check Connection with Plugin Running**
   ```
   Call check_unreal_connection
   ```

2. **Examine Response**
   ```
   Look for plugin_status details:
   - Plugin loaded
   - Plugin version
   - Connection port
   ```

3. **Verify MCP Server Info**
   ```
   Check for MCP server status in response
   ```

### Expected Outcomes
- ✅ plugin_status shows detailed state
- ✅ Version numbers present
- ✅ Connection details included
- ✅ No warnings or errors

---

## Test 3: Connection Failure Troubleshooting

**Purpose**: Test behavior when Unreal is not running

### Steps

1. **Close Unreal Engine**
   ```
   Ensure Unreal Editor is completely closed
   ```

2. **Check Connection**
   ```
   Call check_unreal_connection
   ```

3. **Examine Error Details**
   ```
   Review troubleshooting suggestions in response
   ```

### Expected Outcomes
- ✅ success: false
- ✅ connection_status: "Failed" or "Not Connected"
- ✅ error message: "Failed to connect" or similar
- ✅ troubleshooting: Contains helpful suggestions:
  - "Check if Unreal Engine is running"
  - "Verify VibeUE plugin is loaded"
  - "Check MCP server logs"
- ✅ Clear actionable steps provided

---

## Test 4: Integration with Other Tools

**Purpose**: Use connection check in typical workflow

### Steps

1. **Check Connection First**
   ```
   Always call check_unreal_connection before other operations
   ```

2. **If Connected, Proceed**
   ```
   Call manage_blueprint or other tools
   ```

3. **If Not Connected, Troubleshoot**
   ```
   Follow troubleshooting steps from connection check
   ```

4. **Retry After Fix**
   ```
   Check connection again after fixing issues
   ```

### Expected Outcomes
- ✅ Connection check prevents tool failures
- ✅ Clear workflow: check → fix → proceed
- ✅ Saves time vs trying tools that will fail
- ✅ Better error messages

---

# get_help Tests

## Prerequisites
- ✅ MCP server running
- ✅ Connection to VibeUE established (optional for help)

## Test 5: Default Help (Overview)

**Purpose**: Get general VibeUE overview

### Steps

1. **Call get_help Without Parameters**
   ```
   Call get_help() with no topic parameter
   ```

2. **Examine Overview Content**
   ```
   Should return overview topic by default
   ```

### Expected Outcomes
- ✅ Returns overview documentation
- ✅ Contains VibeUE introduction
- ✅ Lists available topics
- ✅ Quick start guidance included

---

## Test 6: blueprint-workflow Topic

**Purpose**: Get Blueprint development workflow guide

### Steps

1. **Get Blueprint Workflow Help**
   ```
   Call get_help(topic="blueprint-workflow")
   ```

2. **Review Workflow Steps**
   ```
   Check for:
   - Dependency order (Variables → Components → Functions → Nodes)
   - Creation patterns
   - Common workflows
   ```

### Expected Outcomes
- ✅ CRITICAL dependency order documented
- ✅ Step-by-step workflow examples
- ✅ Best practices included
- ✅ Common pitfalls highlighted

---

## Test 7: node-tools Topic

**Purpose**: Get node creation and connection reference

### Steps

1. **Get Node Tools Help**
   ```
   Call get_help(topic="node-tools")
   ```

2. **Review Node Patterns**
   ```
   Check for:
   - discover → create workflow
   - spawner_key usage
   - Pin connection patterns
   - node_params examples
   ```

### Expected Outcomes
- ✅ Discover workflow explained
- ✅ spawner_key pattern documented
- ✅ Connection format examples
- ✅ node_params patterns for variables, casts

---

## Test 8: multi-action-tools Topic

**Purpose**: Get reference for manage_blueprint_function/variable/component

### Steps

1. **Get Multi-Action Help**
   ```
   Call get_help(topic="multi-action-tools")
   ```

2. **Review Action Patterns**
   ```
   Check for comprehensive action documentation
   ```

### Expected Outcomes
- ✅ All actions listed per tool
- ✅ Parameter requirements documented
- ✅ Usage examples provided
- ✅ Return value structures shown

---

## Test 9: umg-guide Topic

**Purpose**: Get UMG widget development guide

### Steps

1. **Get UMG Help**
   ```
   Call get_help(topic="umg-guide")
   ```

2. **Review UMG Patterns**
   ```
   Check for:
   - Container-specific patterns
   - Slot property usage
   - Event binding workflows
   ```

### Expected Outcomes
- ✅ Container hierarchy rules documented
- ✅ Slot properties explained (HAlign_Fill, etc.)
- ✅ Property value formats provided
- ✅ Modern color palettes included

---

## Test 10: asset-discovery Topic

**Purpose**: Get asset management and search guidance

### Steps

1. **Get Asset Discovery Help**
   ```
   Call get_help(topic="asset-discovery")
   ```

2. **Review Search Patterns**
   ```
   Check for:
   - Search best practices
   - Performance tips
   - Asset type filters
   ```

### Expected Outcomes
- ✅ Search patterns documented
- ✅ Performance optimization tips
- ✅ Asset type reference
- ✅ Path usage explained

---

## Test 11: node-positioning Topic

**Purpose**: Get node layout and positioning guidance

### Steps

1. **Get Positioning Help**
   ```
   Call get_help(topic="node-positioning")
   ```

2. **Review Layout Rules**
   ```
   Check for coordinate system and spacing guidance
   ```

### Expected Outcomes
- ✅ Coordinate system explained (X right, Y down)
- ✅ Spacing recommendations provided
- ✅ Left-to-right flow emphasized
- ✅ Common mistakes highlighted

---

## Test 12: properties Topic

**Purpose**: Get property value format reference

### Steps

1. **Get Properties Help**
   ```
   Call get_help(topic="properties")
   ```

2. **Review Value Formats**
   ```
   Check for color, padding, font formats
   ```

### Expected Outcomes
- ✅ Color format: [R, G, B, A]
- ✅ Padding format documented
- ✅ Font structure explained
- ✅ Type-specific examples

---

## Test 13: troubleshooting Topic

**Purpose**: Get problem-solving guidance

### Steps

1. **Get Troubleshooting Help**
   ```
   Call get_help(topic="troubleshooting")
   ```

2. **Review Common Issues**
   ```
   Check for:
   - Connection issues
   - Blueprint problems
   - Node errors
   - UMG issues
   ```

### Expected Outcomes
- ✅ Common issues documented
- ✅ Solutions provided
- ✅ Diagnostic steps included
- ✅ Error message interpretations

---

## Test 14: topics Topic (List All)

**Purpose**: Get list of all available help topics

### Steps

1. **List Topics**
   ```
   Call get_help(topic="topics")
   ```

2. **Examine Topic List**
   ```
   Should show all available topics with descriptions
   ```

### Expected Outcomes
- ✅ Complete topic list returned
- ✅ Each topic has description
- ✅ Topic categories shown
- ✅ Navigation guidance included

---

## Test 15: Invalid Topic Error Handling

**Purpose**: Test behavior with non-existent topic

### Steps

1. **Request Invalid Topic**
   ```
   Call get_help(topic="nonexistent-topic")
   ```

2. **Examine Error Response**
   ```
   Should provide helpful error message
   ```

### Expected Outcomes
- ✅ Error indicates topic not found
- ✅ Suggests using topic="topics" to list all
- ✅ Graceful error handling
- ✅ No crash or exception

---

## Integration Test: Help-Driven Development

**Purpose**: Use get_help before complex operations

### Workflow

1. **Before Creating Blueprint**
   ```
   Call get_help(topic="blueprint-workflow")
   Review dependency order
   ```

2. **Before Creating Nodes**
   ```
   Call get_help(topic="node-tools")
   Review discover → create pattern
   ```

3. **Before Styling UMG**
   ```
   Call get_help(topic="umg-guide")
   Review slot property patterns
   ```

4. **If Encountering Errors**
   ```
   Call get_help(topic="troubleshooting")
   Review relevant solution
   ```

5. **Proceed with Informed Approach**
   ```
   Apply patterns from documentation
   ```

### Expected Outcomes
- ✅ Help provides necessary context
- ✅ Patterns prevent common mistakes
- ✅ Troubleshooting speeds problem resolution
- ✅ Overall workflow more efficient

---

## Reference: All Help Topics

| Topic | Purpose |
|-------|---------|
| **overview** | General VibeUE introduction and quick reference |
| **blueprint-workflow** | Complete Blueprint development workflow |
| **node-tools** | Node discovery, creation, connections |
| **multi-action-tools** | manage_blueprint_function/variable/component reference |
| **umg-guide** | UMG widget development, styling, hierarchy |
| **asset-discovery** | Asset search, management, performance |
| **node-positioning** | Node layout and coordinate system |
| **properties** | Property value formats and types |
| **troubleshooting** | Common issues and solutions |
| **topics** | List all available topics |

---

## Reference: Connection Check Response

### Successful Connection
```json
{
  "success": true,
  "connection_status": "Connected",
  "plugin_status": "Active",
  "unreal_version": "5.6",
  "plugin_version": "1.0.0"
}
```

### Failed Connection
```json
{
  "success": false,
  "connection_status": "Failed",
  "error": "Failed to connect to Unreal Engine",
  "troubleshooting": [
    "Check if Unreal Engine is running",
    "Verify VibeUE plugin is loaded",
    "Check MCP server connection"
  ]
}
```

---

**Test Coverage**:  
- ✅ check_unreal_connection: 4 scenarios tested
- ✅ get_help: 11 topics + error handling tested
- ✅ Integration workflows documented

**Last Updated**: November 3, 2025  
**Related Issues**: #69, #77
