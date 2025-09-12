<div align="center">

# Model Context Protocol for Unreal Engine
<span style="color: #555555">Vibe UE</span>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.6%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.12%2B-yellow)](https://www.python.org)
[![Status](https://img.shields.io/badge/Status-Experimental-red)](https://github.com/chongdashu/unreal-mcp)

</div>

This project enables AI assistant clients like **VS Code**, Cursor, Windsurf and Claude Desktop to control Unreal Engine through natural language using the Model Context Protocol (MCP). With seamless VS Code integration, you can manipulate Blueprints, UMG widgets, and Unreal Engine assets directly from your code editor.


## Credits

This project has forked off of https://github.com/chongdashu/unreal-mcp

This implementation is focused on Blueprints, UMG Widgets, and seamless integration with VS Code for enhanced development workflows.

## ⚠️ Experimental Status

This project is currently in an **EXPERIMENTAL** state. The API, functionality, and implementation details are subject to significant changes. While we encourage testing and feedback, please be aware that:

- Breaking changes may occur without notice
- Features may be incomplete or unstable
- Documentation may be outdated or missing
- Production use is not recommended at this time

## 🌟 Overview

The Unreal MCP integration provides comprehensive tools for controlling Unreal Engine through natural language:

| Category | Capabilities |
|----------|-------------|
| **Actor Management** | • Create and delete actors (cubes, spheres, lights, cameras, etc.)<br>• Set actor transforms (position, rotation, scale)<br>• Query actor properties and find actors by name<br>• List all actors in the current level |
| **Blueprint Development** | • Create new Blueprint classes with custom components<br>• Add and configure components (mesh, camera, light, etc.)<br>• Set component properties and physics settings<br>• Compile Blueprints and spawn Blueprint actors<br>• Create input mappings for player controls |
| **Blueprint Node Graph** | • Add event nodes (BeginPlay, Tick, etc.)<br>• Create function call nodes and connect them<br>• Add variables with custom types and default values<br>• Create component and self references<br>• Find and manage nodes in the graph |
| **Editor Control** | • Focus viewport on specific actors or locations<br>• Control viewport camera orientation and distance |

All these capabilities are accessible through natural language commands via AI assistants, making it easy to automate and control Unreal Engine workflows.

## 🧩 Components

### Unreal Engine Plugin (VibeUE) `Plugins/VibeUE`
- Native TCP server for MCP communication
- Integrates with Unreal Editor subsystems
- Implements actor manipulation tools
- Handles command execution and response handling

### Python MCP Server `Plugins/VibeUE/Python/vibe-ue-main/Python/vibe_ue_server.py`
- Implemented in `vibe_ue_server.py`
- Manages TCP socket connections to the C++ plugin (port 55557)
- Handles command serialization and response parsing
- Provides error handling and connection management
- Loads and registers tool modules from the `tools` directory
- Uses the FastMCP library to implement the Model Context Protocol

## 📂 Directory Structure

- **Plugins/VibeUE/** - C++ plugin source
  - **Source/VibeUE/** - Plugin source code
  - **VibeUE.uplugin** - Plugin definition
  - **Plugins/VibeUE/Python/vibe-ue-main/** - Python MCP server
    - **Python/** - Python server and tools
      - **tools/** - Tool modules for actor, editor, and blueprint operations
      - **scripts/** - Example scripts and demos
      - **vibe_ue_server.py** - Main MCP server script

- **Docs/** - Comprehensive documentation
  - See [Docs/README.md](Docs/README.md) for documentation index

## 🚀 Quick Start Guide

### Prerequisites
- Unreal Engine 5.6+
- Python 3.12+
- MCP Client (VS Code with MCP extension, Claude Desktop, Cursor, Windsurf)

### VS Code Setup

1. **Install VS Code Extensions**
   - Install the "Model Context Protocol" extension from the VS Code marketplace
   - This enables MCP server integration within VS Code

2. **Configure MCP in VS Code**
   - Copy the `mcp.json` file from this project to your VS Code workspace
   - Or create `.vscode/mcp.json` in your project root with the following content:
   ```json
   {
     "servers": {
       "VibeUE": {
         "type": "stdio",
         "command": "python",
         "args": ["Plugins\\VibeUE\\Python\\vibe-ue-main\\Python\\vibe_ue_server.py"],
         "env": {},
         "cwd": "${workspaceFolder}"
       }
     },
     "inputs": []
   }
   ```

3. **Start the MCP Server**
   - The MCP server will automatically start when VS Code loads
   - Check the VS Code output panel for MCP server status messages

### Plugin Setup

1. **Copy the plugin to your project**
   - Copy `Plugins/VibeUE` to your Unreal project's Plugins folder
   - Or use the existing VibeUE plugin if already installed

2. **Enable the plugin**
   - Edit > Plugins
   - Find "VibeUE" in Editor category
   - Enable the plugin
   - Restart editor when prompted

3. **Build the plugin**
   - Right-click your .uproject file
   - Generate Visual Studio project files
   - Open solution (`.sln`)
   - Build with your target platform and output settings

### Python Server Setup

See [Python/README.md](Python/README.md) for detailed Python setup instructions, including:
- Setting up your Python environment
- Running the MCP server
- Using direct or server-based connections

### Configuring your MCP Client

#### VS Code (Recommended)
1. Install the "Model Context Protocol" extension
2. Use the provided `.vscode/mcp.json` configuration file
3. The server will start automatically when VS Code loads

#### Other MCP Clients
Use the following JSON for your MCP configuration:

```json
{
  "mcpServers": {
    "VibeUE": {
      "command": "python",
      "args": [
        "Plugins\\VibeUE\\Python\\vibe-ue-main\\Python\\vibe_ue_server.py"
      ],
      "cwd": "<path/to/your/unreal/project>"
    }
  }
}
```

#### MCP Configuration Locations

| MCP Client | Configuration File Location | Notes |
|------------|------------------------------|-------|
| **VS Code** | `.vscode/mcp.json` | Located in your project root directory |
| Claude Desktop | `~/.config/claude-desktop/mcp.json` | On Windows: `%USERPROFILE%\.config\claude-desktop\mcp.json` |
| Cursor | `.cursor/mcp.json` | Located in your project root directory |
| Windsurf | `~/.config/windsurf/mcp.json` | On Windows: `%USERPROFILE%\.config\windsurf\mcp.json` |

Each client uses the same JSON format as shown in the example above. 
Simply place the configuration in the appropriate location for your MCP client.


## License
MIT

## Questions

For questions, you can reach me on X/Twitter: [@chongdashu](https://www.x.com/chongdashu)