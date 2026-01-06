<div align="center">
# VibeUE - AI-Powered Unreal Engine Editor with MCP Integration

https://www.vibeue.com/

<span style="color: #555555">Vibe UE</span>

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7%2B-orange)](https://www.unrealengine.com)
[![MCP](https://img.shields.io/badge/MCP-2025--11--25-blue)](https://modelcontextprotocol.io)

</div>

**VibeUE brings AI directly into Unreal Engine** with a powerful In-Editor Chat Client that can manipulate Blueprints, UMG widgets, materials, and assets through natural language. No external tools required!

Use AI to create UI widgets, modify Blueprints, manage materials, and control level actors - all from within the Unreal Editor. Extend the AI's capabilities by connecting external MCP servers, or expose VibeUE's tools to your favorite AI IDE.


## ✨ Key Features

- **In-Editor AI Chat** - Chat with AI directly inside Unreal Engine's editor
- **27 Built-in Tools** - Comprehensive control over Blueprints, Materials, Widgets, Actors, Python, and Filesystem
- **Custom Instructions** - Add project-specific context via the Instructions folder
- **Extend via MCP** - Connect external MCP servers to add more AI capabilities
- **Expose to External IDEs** - Let VS Code, Claude Desktop, Cursor, and Windsurf control Unreal via MCP

## 🚀 Installation & Quick Start

### Prerequisites
- Unreal Engine 5.7+
- Git (for manual installation)

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

### 4. Open AI Chat & Get Your API Key

1. **Open the AI Chat**: Go to `Window > VibeUE > AI Chat`
2. **Click the ⚙️ gear icon** to open Settings
3. **Get a free API key** at [vibeue.com](https://vibeue.com) (or use [openrouter.ai](https://openrouter.ai))
4. **Paste your API key** and click Save
5. **Start chatting!** Ask the AI to create a widget, modify a Blueprint, or search your assets

### 5. Test the Installation

Try these commands in the AI Chat:
- "Search for all widgets in my project"
- "Create a new Actor Blueprint called BP_TestActor"
- "List all actors in the current level"

**Success!** You're now using AI-powered development directly in Unreal Engine.

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

VibeUE provides comprehensive AI-powered control over Unreal Engine through **27 individual tools** (14 multi-action tool groups + Python tools + Filesystem tools) exposing **200+ total actions** organized into these major categories:

## 🔧 Canonical Tools Reference (27 tools, 200+ actions)

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

### 5. `manage_blueprint_function` (11 actions)
Blueprint function lifecycle, parameter management, and local variables.

| Action | Purpose |
|--------|---------|
| `list` | List all functions in Blueprint |
| `get_info` | Get detailed function information including parameters and local variables |
| `create` | Create new custom function |
| `delete` | Remove function from Blueprint |
| `add_input` | Add input parameter |
| `add_output` | Add output parameter |
| `remove_param` | Remove parameter from function |
| `add_local_variable` | Add local variable to function |
| `remove_local_variable` | Remove local variable from function |
| `update_local_variable` | Update properties of existing local variable |
| `list_local_variables` | List all local variables in function |
| `get_available_local_types` | Get list of available types for local variables |

### 6. `manage_blueprint_variable` (7 actions)
Variable creation, inspection, and management.

| Action | Purpose |
|--------|---------|
| `search_types` | Discover available variable types |
| `create` | Create new Blueprint variable |
| `delete` | Remove variable with reference check |
| `list` | List all variables in Blueprint |
| `get_info` | Get detailed variable information |
| `get_property_options` | Discover valid values for a specific property (e.g., replication_condition) |
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

### 15. `manage_python_execution` (8 actions)
Python code execution with runtime API discovery and introspection.

| Action | Purpose |
|--------|---------|
| `discover_module` | Introspect Python module contents and available classes/functions |
| `discover_class` | Get detailed class information - methods, properties, inheritance |
| `discover_function` | Get function signature, parameters, return type, and docstring |
| `list_subsystems` | List all available Unreal Engine editor subsystems |
| `execute_code` | Execute Python code in Unreal Engine context with output capture |
| `evaluate_expression` | Evaluate Python expression and return result |
| `get_examples` | Get curated Python code examples for common operations |
| `get_help` | Get comprehensive help documentation for Python tools |

### 16. File System Operations (5 actions)
Comprehensive file reading, searching, and navigation capabilities.

| Action | Purpose |
|--------|---------|
| `read_file` | Read file contents with line range support (like VSCode) |
| `list_dir` | List directory contents - files and subdirectories |
| `file_search` | Find files matching glob patterns across workspace |
| `grep_search` | Search for text/regex patterns in files with context |
| `get_directories` | Get important project directories (game, plugin, Python paths) |

---

All capabilities are accessible through natural language commands via AI assistants, enabling rapid prototyping, automated UI generation, intelligent asset management, and Python-powered automation workflows.

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
- **Python code execution** with runtime API discovery
- **Filesystem operations** - read, search, and navigate project files
- Robust error handling and connection management

### MCP Server (Built-in)
- Exposes all 27 VibeUE tools to external AI clients
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

## 🌐 Expose VibeUE to External AI IDEs

Want to use VS Code, Claude Desktop, Cursor, or Windsurf to control Unreal? VibeUE includes a built-in MCP server that exposes all 27 tools to external clients.

### Enable the MCP Server

Configure in **Project Settings > Plugins > VibeUE** (or ⚙️ in the Chat window):

| Setting | Default | Description |
|---------|---------|-------------|
| **Enable MCP Server** | Enabled | Toggle the HTTP MCP server |
| **Port** | 8088 | HTTP port for MCP connections |
| **API Key** | (empty) | Bearer token for authentication |

When enabled, the server runs at `http://127.0.0.1:<port>/mcp` using Streamable HTTP transport (MCP 2025-11-25 spec).

### Configure Your AI IDE

#### VS Code
1. Install the **"Model Context Protocol" extension**
2. Create `.vscode/mcp.json` in your project:
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
3. Make sure Unreal Editor is running
4. Reload VS Code - it will connect automatically

#### Claude Desktop, Cursor, Windsurf
Use this configuration:

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

> **Note:** Replace `YOUR_API_KEY` with the API key from VibeUE settings. If no API key is set, omit the `headers` section.

### MCP Configuration File Locations

| MCP Client | Configuration File Location |
|------------|------------------------------|
| **VS Code** | `.vscode/mcp.json` (in project root) |
| **Claude Desktop** | `%USERPROFILE%\.config\claude-desktop\mcp.json` |
| **Cursor** | `.cursor/mcp.json` (in project root) |
| **Windsurf** | `%USERPROFILE%\.config\windsurf\mcp.json` |

**Important:** The MCP server only runs when Unreal Editor is open with VibeUE enabled.


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

