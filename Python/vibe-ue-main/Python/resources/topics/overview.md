# VibeUE MCP Overview

## What is VibeUE?

VibeUE is a Model Context Protocol (MCP) server that enables AI assistants to control Unreal Engine 5.6+ through natural language commands.

## Core Capabilities

- **Blueprint Development**: Create and modify Blueprint classes, functions, and event graphs
- **UMG Widget System**: Build and style user interfaces with complete widget hierarchy control
- **Asset Management**: Search, import, export, and analyze project assets
- **Node-Based Programming**: Manipulate Blueprint node graphs with precision
- **Real-Time Property Editing**: Modify component and widget properties on-the-fly

## Connection Architecture

- **Protocol**: TCP Socket communication on localhost:55557
- **Pattern**: Connection-per-command (Unreal closes after each operation)
- **Timeout**: 5-second timeout prevents hanging
- **Error Recovery**: Automatic reconnection and comprehensive error messages

## Essential First Steps

1. **Verify Connection**: Use `check_unreal_connection()` to ensure Unreal is running
2. **Search Before Modify**: Use `search_items()` to find exact asset names
3. **Check Success Field**: Always verify `success` field in responses
4. **Read Topic Guides**: Use `get_help(topic="...")` for specific guidance

## Quick Reference

- **Stuck?** → `get_help(topic="troubleshooting")`
- **Blueprint work?** → `get_help(topic="blueprint-workflow")`
- **UMG styling?** → `get_help(topic="umg-guide")`
- **Node connections?** → `get_help(topic="node-tools")`
- **Node positioning?** → `get_help(topic="node-positioning")`
- **Asset management?** → `get_help(topic="asset-discovery")`

## Available Topics

Call `get_help(topic="topics")` to see all available help topics.
