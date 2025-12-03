<div align="center">
# VibeUE - Model Context Protocol for Unreal Engine

https://www.vibeue.com/

<span style="color: #555555">Vibe UE</span>

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.12%2B-yellow)](https://www.python.org)

</div>

This project enables AI assistant clients like **VS Code**, Cursor, Windsurf and Claude Desktop to control Unreal Engine through natural language using the Model Context Protocol (MCP). With seamless VS Code integration, you can manipulate Blueprints, UMG widgets, and Unreal Engine assets directly from your code editor.

It's not perfect but it's a glimpse of a vision of how to better deal with No-Code solutions.  It's also kind of fun to play with.

## 🚀 Installation & Quick Start

### Prerequisites
- Unreal Engine 5.7+
- Python 3.12+
- Git
- MCP Client (VS Code with MCP extension, Claude Desktop, Cursor, Windsurf)

### 1. Clone the Repository (manual installs only)

> **Marketplace users:** If you already installed VibeUE from the Unreal Marketplace, Unreal copied the plugin into your project automatically. Skip to [Step 3](#3-enable-the-plugin-in-unreal-engine) to enable it in the editor.

**Manual install:** Clone directly into your Unreal Engine project's `Plugins` folder.

```bash
cd /path/to/your/unreal/project/Plugins
git clone https://github.com/kevinpbuckley/VibeUE.git
```

**Example for Windows:**
```cmd
cd C:\MyProject\Plugins
git clone https://github.com/kevinpbuckley/VibeUE.git
```

This will create the plugin structure at `YourProject/Plugins/VibeUE/`

### 2. Build the Plugin (Required for Manual Installation)

**Why build?** VibeUE is a C++ plugin that needs to be compiled for your specific Unreal Engine version. If you skip this step and try to launch Unreal, you'll see a "Missing Modules" error.

**Quick Build - Double-click this file:**
```
Plugins/VibeUE/BuildPlugin.bat
```

The script automatically finds your Unreal Engine installation and project file, then builds the plugin.

**Having issues?** See the [detailed build guide](docs/BUILD_PLUGIN.md) for troubleshooting and advanced options.

> **Note:** If you install VibeUE from the Unreal Marketplace in the future, this build step won't be necessary - Epic compiles plugins automatically.

### 3. Enable the Plugin in Unreal Engine

1. **Open your Unreal Engine project**
2. **Go to Edit > Plugins**
3. **Find "VibeUE" in the Editor category**
4. **Check the box to enable it**
5. **Restart the editor when prompted**

### 4. Configure MCP Client

#### VS Code (Recommended)
1. **Install the "Model Context Protocol" extension** from the VS Code marketplace
2. **Create or update `.vscode/mcp.json`** in your project root:
   ```json
   {
     "servers": {
        "VibeUE": {

      "type": "stdio",
      "command": "uv",
      "args": [
        "--directory",
        "Plugins\\VibeUE\\Content\\Python",
        "run",
        "vibe_ue_server.py"
      ],
      "env": {},
      "cwd": "${workspaceFolder}",
    },
     "inputs": []
   }
   ```
3. **Reload VS Code** - The MCP server will start automatically

#### Other MCP Clients
Use this configuration for Claude Desktop, Cursor, or Windsurf:

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory",
        "[PATH TO YOUR UNREAL PROJECT]\\Plugins\\VibeUE\\Python\\vibe-ue-main\\Python",
        "run",
        "vibe_ue_server.py"
      ]
    }
  }
}

```

### 5. Test the Installation

1. **Open Unreal Engine** with your project
2. **Open your MCP client** (VS Code, Claude Desktop, etc.)
3. **Ask your AI assistant**: "Search for widgets in my project"
4. **Success!** If it returns widget information, VibeUE is working

For detailed setup instructions, see the [Complete Setup Guide](#complete-setup-guide) below.

## 🌟 Overview

VibeUE provides comprehensive AI-powered control over Unreal Engine through **13 multi-action tools** exposing **167 total actions** organized into these major categories:

## 🛠️ Canonical Tools Reference (13 tools, 167 actions)

The running MCP server exposes multi-action tools that consolidate related operations. For full parameter documentation and examples, call `get_help()`.

### 1. `manage_asset` (10 actions)
Asset import, export, search, and management operations.

| Action | Purpose |
|--------|---------|
| `search` | Search for assets (widgets, textures, blueprints, materials) |
| `import_texture` | Import texture from file system |
| `export_texture` | Export texture for AI analysis |
| `delete` | Delete asset with safety checks |
| `open_in_editor` | Open asset in appropriate editor |
| `svg_to_png` | Convert SVG to PNG |
| `duplicate` | Duplicate asset to new location |
| `save` | Save single asset to disk |
| `save_all` | Save all modified assets |
| `list_references` | List asset references/dependencies |

### 2. `manage_blueprint` (8 actions)
Blueprint lifecycle, compilation, and property management.

| Action | Purpose |
|--------|---------|
| `create` | Create new Blueprint (Actor, Widget, Component, etc.) |
| `compile` | Compile Blueprint |
| `get_info` | Get comprehensive Blueprint information |
| `get_property` | Get class default property value |
| `set_property` | Set class default property value |
| `reparent` | Change Blueprint parent class |
| `list_custom_events` | List custom events in Blueprint |
| `summarize_event_graph` | Get event graph summary |

### 3. `manage_blueprint_component` (12 actions)
Component discovery, creation, and property management.

| Action | Purpose |
|--------|---------|
| `search_types` | Discover available component types |
| `get_info` | Get component type information |
| `get_property_metadata` | Get detailed property metadata |
| `list` | List all components in Blueprint |
| `create` | Add new component to Blueprint |
| `delete` | Remove component from Blueprint |
| `get_property` | Get single property value |
| `set_property` | Set component property value |
| `get_all_properties` | Get all property values |
| `compare_properties` | Compare properties between Blueprints |
| `reorder` | Change component order |
| `reparent` | Change component parent attachment |

### 4. `manage_blueprint_node` (17 actions)
Node graph operations for Blueprint visual scripting.

| Action | Purpose |
|--------|---------|
| `discover` | Discover available node types with spawner_key |
| `create` | Create new node using spawner_key |
| `connect` | Connect pins between nodes |
| `connect_pins` | Batch connect with validation |
| `disconnect` | Disconnect specific pins |
| `disconnect_pins` | Break links or clear pins |
| `delete` | Remove node from graph |
| `move` | Reposition node |
| `list` | List all nodes in graph |
| `describe` | Get rich node + pin metadata |
| `reset_pin_defaults` | Restore pin default values |
| `get_details` | Get detailed node information |
| `configure` | Set pin defaults and node config |
| `split` | Split struct pins into sub-pins |
| `recombine` | Collapse split pins back |
| `refresh_node` | Reconstruct single node |
| `refresh_nodes` | Refresh all nodes in Blueprint |

### 5. `manage_blueprint_function` (13 actions)
Blueprint function lifecycle and parameter management.

| Action | Purpose |
|--------|---------|
| `list` | List all functions in Blueprint |
| `get` | Get detailed function information |
| `list_params` | List function parameters (inputs/outputs) |
| `create` | Create new custom function |
| `delete` | Remove function from Blueprint |
| `add_param` | Add input/output parameter |
| `remove_param` | Remove parameter from function |
| `update_param` | Update parameter type or name |
| `list_locals` | List local variables in function |
| `add_local` | Add local variable to function |
| `remove_local` | Remove local variable |
| `update_local` | Update local variable type |
| `update_properties` | Update function metadata (pure, category) |

### 6. `manage_blueprint_variable` (7 actions)
Variable creation, inspection, and management.

| Action | Purpose |
|--------|---------|
| `create` | Create new Blueprint variable |
| `delete` | Remove variable with reference check |
| `list` | List all variables in Blueprint |
| `get_info` | Get detailed variable information |
| `get_property` | Get nested property value |
| `set_property` | Set nested property value |
| `search_types` | Discover available variable types |

### 7. `manage_umg_widget` (11 actions)
UMG Widget Blueprint operations.

| Action | Purpose |
|--------|---------|
| `list_components` | List components with hierarchy |
| `add_component` | Add widget component |
| `remove_component` | Remove widget component |
| `validate` | Validate widget hierarchy |
| `search_types` | Discover widget component types |
| `get_component_properties` | Get all component properties |
| `get_property` | Get single property value |
| `set_property` | Set widget property |
| `list_properties` | List available properties |
| `get_available_events` | Get bindable events |
| `bind_events` | Bind input events |

### 8. `manage_enhanced_input` (24 actions)
Complete Enhanced Input system control.

| Action | Purpose |
|--------|---------|
| `reflection_discover_types` | Discover modifier/trigger types |
| `reflection_get_metadata` | Get type metadata |
| `action_create` | Create Input Action |
| `action_list` | List all Input Actions |
| `action_get_properties` | Get action properties |
| `action_configure` | Modify action properties |
| `mapping_create_context` | Create Mapping Context |
| `mapping_list_contexts` | List all contexts |
| `mapping_get_properties` | Get context properties |
| `mapping_get_property` | Get single property |
| `mapping_update_context` | Update context |
| `mapping_validate_context` | Validate context |
| `mapping_get_mappings` | List key mappings |
| `mapping_add_key_mapping` | Add key binding |
| `mapping_remove_mapping` | Remove key mapping |
| `mapping_get_available_keys` | List bindable keys |
| `mapping_add_modifier` | Add modifier |
| `mapping_remove_modifier` | Remove modifier |
| `mapping_get_modifiers` | List modifiers |
| `mapping_get_available_modifier_types` | List modifier types |
| `mapping_add_trigger` | Add trigger |
| `mapping_remove_trigger` | Remove trigger |
| `mapping_get_triggers` | List triggers |
| `mapping_get_available_trigger_types` | List trigger types |

### 9. `manage_level_actors` (18 actions)
Complete level actor management for runtime level manipulation.

| Action | Purpose |
|--------|--------|
| `list` | List all actors in current level |
| `find` | Find actors by name pattern or class |
| `get_info` | Get detailed actor information |
| `add` | Spawn new actor in level |
| `remove` | Delete actor from level |
| `get_transform` | Get actor transform (location, rotation, scale) |
| `set_transform` | Set complete actor transform |
| `set_location` | Set actor world location |
| `set_rotation` | Set actor world rotation |
| `set_scale` | Set actor 3D scale |
| `focus` | Focus viewport on actor |
| `move_to_view` | Move actor to current camera view |
| `refresh_viewport` | Force viewport refresh |
| `get_property` | Get actor property value |
| `set_property` | Set actor property value |
| `get_all_properties` | List all actor properties |
| `set_folder` | Set actor's folder path in World Outliner |
| `rename` | Rename actor label |

### 10. `manage_material` (24 actions)
Complete material and material instance management.

| Action | Purpose |
|--------|---------|
| `create` | Create new material asset |
| `create_instance` | Create Material Instance Constant (MIC) |
| `get_info` | Get comprehensive material information |
| `list_properties` | List all editable properties |
| `get_property` | Get property value |
| `get_property_info` | Get detailed property metadata |
| `set_property` | Set property value |
| `set_properties` | Set multiple properties at once |
| `list_parameters` | List material parameters |
| `get_parameter` | Get specific parameter info |
| `set_parameter_default` | Set parameter default value |
| `save` | Save material to disk |
| `compile` | Recompile material shaders |
| `refresh_editor` | Refresh open Material Editor |
| `get_instance_info` | Get material instance information |
| `list_instance_properties` | List instance editable properties |
| `get_instance_property` | Get instance property value |
| `set_instance_property` | Set instance property (e.g., PhysMaterial) |
| `list_instance_parameters` | List instance parameter overrides |
| `set_instance_scalar_parameter` | Set scalar parameter override |
| `set_instance_vector_parameter` | Set vector/color parameter override |
| `set_instance_texture_parameter` | Set texture parameter override |
| `clear_instance_parameter_override` | Remove parameter override |
| `save_instance` | Save material instance to disk |

### 11. `manage_material_node` (21 actions)
Material graph node (expression) operations.

| Action | Purpose |
|--------|--------|
| `discover_types` | Discover available expression types |
| `get_categories` | Get expression categories |
| `create` | Create new expression node |
| `delete` | Remove expression |
| `move` | Reposition expression |
| `list` | List all expressions in material |
| `get_details` | Get detailed expression info |
| `get_pins` | Get all pins for expression |
| `connect` | Connect two expressions |
| `disconnect` | Disconnect input |
| `list_connections` | List all connections |
| `connect_to_output` | Connect to material output |
| `disconnect_output` | Disconnect material output |
| `get_property` | Get expression property |
| `set_property` | Set expression property |
| `list_properties` | List editable properties |
| `create_parameter` | Create parameter expression |
| `promote_to_parameter` | Convert constant to parameter |
| `set_parameter_metadata` | Set parameter group/priority |
| `get_output_properties` | List material outputs |
| `get_output_connections` | Get output connections |

### 12. `check_unreal_connection` (1 action)
Test connection to Unreal Engine and verify plugin status.

### 13. `get_help` (1 action)
Get comprehensive help documentation by topic.

**Available Topics**: `overview`, `blueprint-workflow`, `node-tools`, `multi-action-tools`, `umg-guide`, `enhanced-input`, `level-actors`, `material-management`, `material-node-tools`, `asset-discovery`, `troubleshooting`, `topics`

---

All capabilities are accessible through natural language commands via AI assistants, enabling rapid prototyping, automated UI generation, and intelligent asset management workflows.

## 🧩 Components

### Unreal Engine Plugin (VibeUE) `Plugins/VibeUE`
- Native C++ implementation with TCP server for MCP communication
- Deep integration with Unreal Editor subsystems and APIs
- Comprehensive Blueprint manipulation and UMG widget control
- Asset management with import/export capabilities
- Real-time property inspection and modification
- Advanced node graph manipulation and analysis
- Robust error handling and connection management

### Python MCP Server `Plugins/VibeUE/Python/vibe-ue-main/Python/vibe_ue_server.py`
- Implemented in `vibe_ue_server.py`
- Manages TCP socket connections to the C++ plugin (port 55557)
- Handles command serialization and response parsing
- Provides error handling and connection management
- Loads and registers tool modules from the `tools` directory
- Uses the FastMCP library to implement the Model Context Protocol

## 💡 Best Practices & AI Integration

### Performance Optimization
- **Always use full asset paths** from `search_items()` results for instant loading
- **Avoid partial names** when possible - they trigger expensive Asset Registry searches
- **Use `get_help(topic="umg-guide")`** before styling to understand container-specific requirements
- **Batch property changes** when modifying multiple components

### Error Prevention
- **Start with discovery**: Use `search_items()` to find exact asset names
- **Validate hierarchies**: Use `list_widget_components()` to see available components
- **Check connections**: Use `check_unreal_connection()` when tools fail
- **Use exact names**: Copy exact names from discovery tools to avoid typos

### Intelligent Workflow Design
- **Plan before creating**: Use analysis tools to understand existing structures
- **Incremental development**: Build widgets step-by-step with validation
- **Template-driven**: Use `get_help(topic="umg-guide")` for styling patterns and best practices
- **Visual feedback**: Export textures for AI analysis with `export_texture_for_analysis()`

### AI Assistant Integration
The tools are specifically designed for AI assistants with:
- **Comprehensive help text** with examples and usage patterns
- **Type detection** for automatic property value conversion  
- **Error messages** that guide towards correct usage
- **Workflow suggestions** built into tool responses

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

## �️ Complete Setup Guide

### Advanced Plugin Configuration

For developers who need more detailed setup instructions or want to customize the installation:

#### Manual Build (Optional)
If you prefer to build the plugin manually:

1. **Generate Visual Studio project files**
   - Right-click your `.uproject` file
   - Select "Generate Visual Studio project files"

2. **Build in Visual Studio**
   - Open the generated `.sln` file
   - Build with your target platform (Win64, Development/DebugGame)


### Python Environment Setup

For detailed Python setup instructions, see [Python/vibe-ue-main/Python/README.md](Python/vibe-ue-main/Python/README.md), including:
- Setting up your Python environment
- Installing dependencies  
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


## Questions and Contributions

For questions and contributions, you can reach me on Discord: https://discord.gg/hZs73ST59a

## Open to Opportunities

I'm actively seeking game development opportunities, particularly roles involving:
- Unreal Engine development
- AI-assisted game development workflows
- Blueprint and UMG systems
- Technical tooling and automation

If you're interested in collaborating or have opportunities available, feel free to reach out via Discord or GitHub.

## Thank You

Thank you to everyone who helped teach me coding, Unreal Engine and AI.
Thank you to everyone who tries this project and contributes to making it better.

