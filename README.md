<div align="center">
# VibeUE - Model Context Protocol for Unreal Engine with In Editor AI Agentic Chat

https://www.vibeue.com/

<span style="color: #555555">Vibe UE</span>

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7%2B-orange)](https://www.unrealengine.com)
[![MCP](https://img.shields.io/badge/MCP-2025--11--25-blue)](https://modelcontextprotocol.io)

</div>

This project enables AI assistant clients like **VS Code**, Cursor, Windsurf and Claude Desktop to control Unreal Engine through natural language using the Model Context Protocol (MCP). With seamless VS Code integration, you can manipulate Blueprints, UMG widgets, and Unreal Engine assets directly from your code editor.

**NEW:** VibeUE now includes a built-in **In-Editor AI Chat Client** that runs directly inside Unreal Engine - no external tools required!

It's not perfect but it's a glimpse of a vision of how to better deal with No-Code solutions.  It's also kind of fun to play with.

## ✨ Key Features

- **In-Editor AI Chat** - Chat with AI directly inside Unreal Engine's editor
- **14 Built-in Tools** - Comprehensive control over Blueprints, Materials, Widgets, Actors, and more
- **Custom Instructions** - Add project-specific context via the Instructions folder
- **External MCP Servers** - Connect additional MCP tools via the plugin's config
- **MCP Server** - Expose VibeUE tools to external AI clients (VS Code, Claude Desktop, etc.)

## 🚀 Installation & Quick Start

### Prerequisites
- Unreal Engine 5.7+
- Git
- MCP Client (VS Code with MCP extension, Claude Desktop, Cursor, Windsurf) - *optional, for external AI access*

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

VibeUE exposes its tools via a built-in **Streamable HTTP MCP server**.

#### Configure the MCP Server in Unreal

1. Open **Project Settings > Plugins > VibeUE** (or click the gear icon in the AI Chat window)
2. Enable **"Enable MCP Server"**
3. Set your **Port** (default: 8088)
4. Set an **API Key** for authentication (optional but recommended)
5. The server starts automatically when Unreal Editor launches

#### VS Code (Recommended)
1. **Install the "Model Context Protocol" extension** from the VS Code marketplace
2. **Create or update `.vscode/mcp.json`** in your project root:
   ```json
   {
     "servers": {
       "VibeUE": {
         "type": "http",
         "url": "http://127.0.0.1:8088/mcp",
         "headers": {
           "Authorization": "Bearer YOUR_API_KEY"
         }
       }
     }
   }
   ```
3. **Reload VS Code** - It will connect to the running Unreal Editor

#### Other MCP Clients
Use this configuration for Claude Desktop, Cursor, or Windsurf:

```json
{
  "mcpServers": {
    "VibeUE": {
      "type": "http",
      "url": "http://127.0.0.1:8088/mcp",
      "headers": {
        "Authorization": "Bearer YOUR_API_KEY"
      }
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

## 💬 In-Editor AI Chat Client

VibeUE includes a powerful **built-in AI chat interface** that runs directly inside Unreal Engine. No external tools required!

### Opening the Chat Window

1. **Menu Bar**: Go to `Window > VibeUE > AI Chat`
2. **Or use the toolbar button** if available

### Features

- **Direct AI Interaction** - Chat with AI models without leaving the editor
- **Tool Integration** - All 14 VibeUE tools are available to the AI
- **Tool Manager** - Enable/disable specific tools per conversation
- **Conversation History** - Maintains context throughout your session
- **External MCP Tools** - Connect additional MCP servers for extended capabilities

### Configuration

Configure the chat client in **Project Settings > Plugins > VibeUE** (or click the ⚙️ gear icon in the Chat window):

#### LLM Provider

| Setting | Description |
|---------|-------------|
| **LLM Provider** | Select your AI provider: `VibeUE` or `OpenRouter` |
| **VibeUE API Key** | Free API key from [vibeue.com](https://vibeue.com) |
| **OpenRouter API Key** | API key from [openrouter.ai](https://openrouter.ai) |
| **Debug Mode** | Enable verbose logging for troubleshooting |

> **💡 Free API Key:** Get a free VibeUE API key at [vibeue.com](https://vibeue.com) to start using the In-Editor Chat immediately!

#### LLM Generation Parameters (VibeUE only)

| Setting | Default | Description |
|---------|---------|-------------|
| **Temperature** | 0.2 | Controls randomness (0.0 = deterministic, 1.0 = creative) |
| **Top P** | 0.9 | Nucleus sampling threshold |
| **Max Tokens** | 16384 | Maximum response length |
| **Max Tool Iterations** | 100 | Maximum tool calls per conversation turn |
| **Parallel Tool Calls** | Enabled | Allow AI to call multiple tools simultaneously |

## 📝 Custom Instructions

Add project-specific context to help the AI understand your codebase and conventions.

### Instructions Folder

Place markdown (`.md`) files in the plugin's `Config/Instructions/` folder:

```
Plugins/VibeUE/Config/Instructions/
├── project-overview.md      # Your project description
├── coding-standards.md      # Your naming conventions
├── blueprint-patterns.md    # Common patterns in your project
└── ...
```

### How It Works

- All `.md` files in the Instructions folder are automatically loaded
- Content is included as system context for the AI
- Helps the AI understand your project's specific requirements
- Updates take effect on the next conversation

### Example Instructions File

```markdown
# Project: My Awesome Game

## Overview
This is a top-down action RPG built in Unreal Engine 5.7.

## Naming Conventions
- Blueprints: BP_<Type>_<Name> (e.g., BP_Actor_Enemy)
- Widgets: WBP_<Name> (e.g., WBP_MainMenu)
- Materials: M_<Surface>_<Variant> (e.g., M_Metal_Rusty)

## Common Patterns
- All UI widgets inherit from WBP_BaseWidget
- Enemy AI uses Behavior Trees in /Content/AI/BehaviorTrees/
```

## 🔌 External MCP Servers

Connect additional MCP servers to extend the AI's capabilities beyond the built-in tools.

### Configuration File

Edit `Plugins/VibeUE/Config/vibeue.mcp.json`:

```json
{
  "servers": {
    "my-server-name": {
      "type": "stdio",
      "command": "path/to/executable",
      "args": ["--some-arg"],
      "env": {
        "MY_VAR": "value"
      },
      "cwd": "C:\\path\\to\\server"
    },
    "http-server": {
      "type": "http",
      "url": "http://127.0.0.1:8080/mcp",
      "headers": {
        "Authorization": "Bearer your-token-here"
      }
    }
  }
}
```

### Server Types

| Type | Description |
|------|-------------|
| `stdio` | Launch a local process and communicate via stdin/stdout |
| `http` | Connect to an HTTP MCP server (supports streamable HTTP transport) |

### stdio Server Options

| Field | Required | Description |
|-------|----------|-------------|
| `command` | Yes | Executable to run |
| `args` | No | Array of command-line arguments |
| `env` | No | Environment variables |
| `cwd` | No | Working directory |

### HTTP Server Options

| Field | Required | Description |
|-------|----------|-------------|
| `url` | Yes | Full URL to the MCP endpoint |
| `headers` | No | HTTP headers (e.g., Authorization) |

### Managing MCP Tools

1. Open the **Tool Manager** in the chat window
2. View all available tools (Internal + MCP)
3. Enable/disable tools as needed
4. MCP tools show their source server name

## 🌟 Overview

VibeUE provides comprehensive AI-powered control over Unreal Engine through **14 multi-action tools** exposing **173 total actions** organized into these major categories:

## 🛠️ Canonical Tools Reference (14 tools, 173 actions)

The running MCP server exposes multi-action tools that consolidate related operations. For full parameter documentation and examples, call `get_help()`.

### 1. `manage_asset` (9 actions)
Asset import, export, search, and management operations.

| Action | Purpose |
|--------|---------|
| `search` | Search for assets (widgets, textures, blueprints, materials) |
| `import_texture` | Import texture from file system |
| `export_texture` | Export texture for AI analysis |
| `delete` | Delete asset with safety checks |
| `open` | Open asset in appropriate editor |
| `duplicate` | Duplicate asset to new location |
| `save` | Save single asset to disk |
| `save_all` | Save all modified assets |
| `list_references` | List asset references/dependencies |

### 2. `manage_blueprint` (7 actions)
Blueprint lifecycle, compilation, and property management.

| Action | Purpose |
|--------|---------|
| `create` | Create new Blueprint (Actor, Widget, Component, etc.) |
| `compile` | Compile Blueprint |
| `get_info` | Get comprehensive Blueprint information |
| `get_property` | Get class default property value |
| `set_property` | Set class default property value |
| `reparent` | Change Blueprint parent class |
| `diff` | Compare two Blueprints |

### 3. `manage_blueprint_component` (9 actions)
Component discovery, creation, and property management.

| Action | Purpose |
|--------|---------|
| `get_available` | Discover available component types |
| `get_info` | Get component type information |
| `get_hierarchy` | List all components in Blueprint with hierarchy |
| `add` | Add new component to Blueprint |
| `remove` | Remove component from Blueprint |
| `get_property` | Get single property value |
| `set_property` | Set component property value |
| `get_all_properties` | Get all property values |
| `reparent` | Change component parent attachment |

### 4. `manage_blueprint_node` (12 actions)
Node graph operations for Blueprint visual scripting.

| Action | Purpose |
|--------|---------|
| `discover` | Discover available node types with spawner_key |
| `create` | Create new node using spawner_key |
| `connect` | Connect pins between nodes |
| `disconnect` | Disconnect specific pins |
| `delete` | Remove node from graph |
| `list` | List all nodes in graph |
| `details` | Get detailed node information |
| `configure` | Set pin defaults and node config |
| `set_property` | Set node property value |
| `split` | Split struct pins into sub-pins |
| `recombine` | Collapse split pins back |
| `refresh_node` | Reconstruct single node |

### 5. `manage_blueprint_function` (7 actions)
Blueprint function lifecycle and parameter management.

| Action | Purpose |
|--------|---------|
| `list` | List all functions in Blueprint |
| `get_info` | Get detailed function information |
| `create` | Create new custom function |
| `delete` | Remove function from Blueprint |
| `add_input` | Add input parameter |
| `add_output` | Add output parameter |
| `remove_param` | Remove parameter from function |

### 6. `manage_blueprint_variable` (6 actions)
Variable creation, inspection, and management.

| Action | Purpose |
|--------|---------|
| `search_types` | Discover available variable types |
| `create` | Create new Blueprint variable |
| `delete` | Remove variable with reference check |
| `list` | List all variables in Blueprint |
| `get_info` | Get detailed variable information |
| `modify` | Modify variable properties |

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

### 8. `manage_enhanced_input` (19 actions)
Complete Enhanced Input system control.

| Action | Purpose |
|--------|---------|
| `reflection_discover_types` | Discover modifier/trigger types |
| `action_create` | Create Input Action |
| `action_list` | List all Input Actions |
| `action_get_properties` | Get action properties |
| `action_configure` | Modify action properties |
| `mapping_create_context` | Create Mapping Context |
| `mapping_list_contexts` | List all contexts |
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

### 9. `manage_level_actors` (21 actions)
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
| `attach` | Attach actor to parent |
| `detach` | Detach actor from parent |
| `select` | Select actor in editor |

### 10. `manage_material` (26 actions)
Complete material and material instance management.

| Action | Purpose |
|--------|---------|
| `create` | Create new material asset |
| `create_instance` | Create Material Instance Constant (MIC) |
| `get_info` | Get comprehensive material information |
| `summarize` | Get material summary overview |
| `open` | Open material in editor |
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

### 12. `manage_data_asset` (9 actions)
UDataAsset management with reflection-based property access.

| Action | Purpose |
|--------|---------|
| `search_types` | Find UDataAsset subclasses |
| `list` | List data assets in project |
| `create` | Create new data asset |
| `get_info` | Get data asset information |
| `list_properties` | List all properties on asset |
| `get_property` | Get property value |
| `set_property` | Set property value |
| `set_properties` | Set multiple properties at once |
| `get_class_info` | Get class-level property information |

### 13. `manage_data_table` (15 actions)
DataTable management for structured game data.

| Action | Purpose |
|--------|---------|
| `search_row_types` | Discover available row struct types |
| `list` | List all data tables in project |
| `create` | Create new data table |
| `get_info` | Get data table information |
| `get_row_struct` | Get row struct definition |
| `list_rows` | List all rows in table |
| `get_row` | Get single row data |
| `add_row` | Add new row to table |
| `add_rows` | Add multiple rows at once |
| `update_row` | Update existing row |
| `remove_row` | Remove row from table |
| `rename_row` | Rename row key |
| `clear_rows` | Remove all rows |
| `import_json` | Import rows from JSON |
| `export_json` | Export rows to JSON |

### 14. `check_unreal_connection` (1 action)
Test connection to Unreal Engine and verify plugin status.

---

All capabilities are accessible through natural language commands via AI assistants, enabling rapid prototyping, automated UI generation, and intelligent asset management workflows.

## 🧩 Components

### Unreal Engine Plugin (VibeUE) `Plugins/VibeUE`
- **Native C++ implementation** - runs entirely within Unreal Editor
- **Built-in MCP Server** using Streamable HTTP transport (MCP 2025-11-25 spec)
- **In-Editor AI Chat** with tool integration
- Deep integration with Unreal Editor subsystems and APIs
- Comprehensive Blueprint manipulation and UMG widget control
- Asset management with import/export capabilities
- Real-time property inspection and modification
- Advanced node graph manipulation and analysis
- Robust error handling and connection management

### MCP Server (Built-in)
- Exposes all 14 VibeUE tools to external AI clients
- **Streamable HTTP transport** - connects via HTTP POST to `/mcp` endpoint
- Configurable port (default: 8088) and API key authentication
- Compatible with VS Code, Claude Desktop, Cursor, Windsurf, and other MCP clients
- Runs automatically when Unreal Editor starts (if enabled)

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
  - **Config/** - Plugin configuration
    - **vibeue.mcp.json** - External MCP server configuration
    - **Instructions/** - Custom instruction files (`.md`)
  - **VibeUE.uplugin** - Plugin definition

- **Docs/** - Comprehensive documentation
  - See [Docs/README.md](Docs/README.md) for documentation index

## 🛠️ Complete Setup Guide

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


### MCP Server Configuration

The built-in MCP server can be configured in **Project Settings > Plugins > VibeUE**:

| Setting | Default | Description |
|---------|---------|-------------|
| **Enable MCP Server** | Enabled | Toggle the HTTP MCP server |
| **Port** | 8088 | HTTP port for MCP connections |
| **API Key** | (empty) | Bearer token for authentication |

When enabled, the server runs at `http://127.0.0.1:<port>/mcp` and accepts JSON-RPC requests following the MCP 2025-11-25 specification with Streamable HTTP transport.

### Configuring your MCP Client

#### VS Code (Recommended)
1. Install the "Model Context Protocol" extension
2. Create `.vscode/mcp.json` with the HTTP configuration (see Quick Start above)
3. Make sure Unreal Editor is running with VibeUE enabled
4. VS Code will connect to the running MCP server

#### Other MCP Clients
Use the following JSON for your MCP configuration:

```json
{
  "mcpServers": {
    "VibeUE": {
      "type": "http",
      "url": "http://127.0.0.1:8088/mcp",
      "headers": {
        "Authorization": "Bearer YOUR_API_KEY"
      }
    }
  }
}
```

> **Note:** Replace `YOUR_API_KEY` with the API key configured in Unreal's VibeUE settings. If no API key is set, you can omit the `headers` section.

#### MCP Configuration Locations

| MCP Client | Configuration File Location | Notes |
|------------|------------------------------|-------|
| **VS Code** | `.vscode/mcp.json` | Located in your project root directory |
| Claude Desktop | `~/.config/claude-desktop/mcp.json` | On Windows: `%USERPROFILE%\.config\claude-desktop\mcp.json` |
| Cursor | `.cursor/mcp.json` | Located in your project root directory |
| Windsurf | `~/.config/windsurf/mcp.json` | On Windows: `%USERPROFILE%\.config\windsurf\mcp.json` |

All clients use the same HTTP configuration format. The MCP server must be running (Unreal Editor open with VibeUE enabled) for clients to connect.


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

