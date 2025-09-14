<div align="center">

# VibeUE - Model Context Protocol for Unreal Engine
<span style="color: #555555">Vibe UE</span>

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.6%2B-orange)](https://www.unrealengine.com)
[![Python](https://img.shields.io/badge/Python-3.12%2B-yellow)](https://www.python.org)
[![Status](https://img.shields.io/badge/Status-Experimental-red)](https://github.com/chongdashu/unreal-mcp)

</div>

This project enables AI assistant clients like **VS Code**, Cursor, Windsurf and Claude Desktop to control Unreal Engine through natural language using the Model Context Protocol (MCP). With seamless VS Code integration, you can manipulate Blueprints, UMG widgets, and Unreal Engine assets directly from your code editor.

It's not perfect but it's a glimpse of a vision of how to better deal with No-Code solutions.  It's also kind of fun to play with.


## Credits

This project has split off of https://github.com/chongdashu/unreal-mcp

This implementation is focused on Blueprints, UMG Widgets, and seamless integration with VS Code for enhanced development workflows. The other project was focused on Assets and Level Design and appeared stale.  There's a limit to how many tools an LLM can handle so it seemed best to move in a different direction for my use cases.

## ⚠️ Experimental Status

This project is currently in an **EXPERIMENTAL** state. The API, functionality, and implementation details are subject to significant changes. While we encourage testing and feedback, please be aware that:

- Breaking changes may occur without notice
- Features may be incomplete or unstable
- Documentation may be outdated or missing

## 🚀 Installation & Quick Start

### Prerequisites
- Unreal Engine 5.6+
- Python 3.12+
- Git
- MCP Client (VS Code with MCP extension, Claude Desktop, Cursor, Windsurf)

### 1. Clone the Repository

**Clone directly into your Unreal Engine project's Plugins folder:**

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

### 2. Enable the Plugin in Unreal Engine

1. **Open your Unreal Engine project**
2. **Go to Edit > Plugins**
3. **Find "VibeUE" in the Editor category**
4. **Check the box to enable it**
5. **Restart the editor when prompted**

The plugin will build automatically when enabled.

### 3. Configure MCP Client

#### VS Code (Recommended)
1. **Install the "Model Context Protocol" extension** from the VS Code marketplace
2. **Create or update `.vscode/mcp.json`** in your project root:
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
3. **Reload VS Code** - The MCP server will start automatically

#### Other MCP Clients
Use this configuration for Claude Desktop, Cursor, or Windsurf:

```json
{
  "mcpServers": {
    "VibeUE": {
      "command": "python",
      "args": ["Plugins\\VibeUE\\Python\\vibe-ue-main\\Python\\vibe_ue_server.py"],
      "cwd": "<path/to/your/unreal/project>"
    }
  }
}
```

### 4. Test the Installation

1. **Open Unreal Engine** with your project
2. **Open your MCP client** (VS Code, Claude Desktop, etc.)
3. **Ask your AI assistant**: "Search for widgets in my project"
4. **Success!** If it returns widget information, VibeUE is working

For detailed setup instructions, see the [Complete Setup Guide](#complete-setup-guide) below.

## 🌟 Overview

VibeUE provides comprehensive AI-powered control over Unreal Engine through 60+ specialized tools organized into these major categories:

### 🎯 Asset Management & Discovery
- **Universal Asset Search**: Find any asset type (widgets, textures, materials, blueprints) with intelligent filtering
- **Asset Editor Launcher**: Open assets directly in their appropriate editors with one command
- **Texture Import/Export**: Smart local file import with format validation and AI-compatible export for visual analysis

### 🔧 Blueprint Development Suite
- **Blueprint Creation & Management**: Create, compile, and reparent Blueprint classes
- **Component System**: Add and configure mesh, camera, light, physics, and custom components
- **Node Graph Manipulation**: Create event nodes, function calls, variables, and wire connections
- **Property Management**: Set Blueprint properties, physics settings, and component configurations

### 📱 Complete UMG Widget System
- **Widget Blueprint Creation**: Create and manage UMG widget blueprints with full hierarchy support
- **Component Library**: 20+ widget types including text, buttons, images, sliders, progress bars, lists
- **Layout Panels**: Canvas, overlay, horizontal/vertical boxes, scroll boxes, and grid panels
- **Advanced Styling**: Comprehensive property system with color, font, positioning, and transform controls
- **Data Binding & Events**: MVVM pattern support, event binding, and dynamic content management

### 🔍 Discovery & Analysis Tools
- **Component Introspection**: Detailed widget hierarchy and property analysis
- **Event Graph Analysis**: List nodes, functions, custom events with complete graph summarization
- **Property Discovery**: Explore available properties and current values for any component
- **Validation Tools**: Check widget hierarchies and component relationships

### 🎨 Styling & Visual Enhancement
- **Color Management**: Full RGBA color support for text, backgrounds, and UI elements
- **Font Control**: Typography settings with size, style, and typeface control  
- **Transform System**: Position, size, scale, rotation, anchoring, and alignment
- **Visibility Control**: Multiple visibility states and z-order management
- **Image Integration**: Texture application with tinting and scaling options

### 🔗 Integration & Connectivity
- **Smart Workflow**: Tools designed to work together (search → inspect → modify → validate)
- **Error Prevention**: Built-in validation and exact name matching to prevent common mistakes
- **Performance Optimization**: Fast direct asset access vs. slow registry searches
- **AI Guidance**: Comprehensive styling guides and best practice recommendations

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

## 🛠️ Complete Tools Reference

### Asset Discovery & Management
| Tool | Purpose | Key Features |
|------|---------|--------------|
| `search_items` | 🔍 Universal asset search | Find widgets, textures, materials, blueprints by name/type/path |
| `open_asset_in_editor` | 🎯 Direct asset editing | Launch any asset in its appropriate editor |
| `import_texture_asset` | 📁 Smart texture import | Local file import with validation & format conversion |
| `export_texture_for_analysis` | 🖼️ AI-compatible export | Export textures for visual analysis by AI |

### Blueprint Development
| Tool | Purpose | Key Features |
|------|---------|--------------|
| `create_blueprint` | 🔨 Blueprint creation | Generate new Blueprint classes |
| `compile_blueprint` | ⚙️ Blueprint compilation | Compile and validate Blueprint changes |
| `reparent_blueprint` | 🔄 Class hierarchy | Change Blueprint parent classes |
| `add_component_to_blueprint` | 🧩 Component system | Add mesh, camera, light, physics components |
| `set_static_mesh_properties` | 📐 Mesh configuration | Configure static mesh components |
| `set_component_property` | 🎛️ Property control | Set any component property |
| `set_physics_properties` | ⚛️ Physics setup | Configure collision, gravity, mass |
| `set_blueprint_property` | 📋 Class properties | Modify Blueprint class defaults |

### Blueprint Node Graph
| Tool | Purpose | Key Features |
|------|---------|--------------|
| `add_blueprint_event_node` | 🎬 Event handling | BeginPlay, Tick, custom events |
| `add_blueprint_input_action_node` | 🎮 Input binding | Keyboard, mouse, gamepad inputs |
| `add_blueprint_function_node` | ⚡ Function calls | Call Blueprint or C++ functions |
| `connect_blueprint_nodes` | 🔗 Node wiring | Connect execution and data pins |
| `add_blueprint_variable` | 📊 Data storage | Add typed variables with editor exposure |
| `add_blueprint_get_self_component_reference` | 🧭 Component refs | Get references to owned components |
| `add_blueprint_self_reference` | 🎯 Actor refs | Get reference to Blueprint actor |
| `find_blueprint_nodes` | 🔍 Node discovery | Find existing nodes by type/name |

### UMG Widget Creation
| Tool | Purpose | Key Features |
|------|---------|--------------|
| `create_umg_widget_blueprint` | 🖼️ Widget creation | Generate new UMG widget blueprints |
| `add_text_block_to_widget` | 📝 Text display | Static text with font/color control |
| `add_button_to_widget` | 🔘 Interactive buttons | Clickable buttons with styling |
| `add_image` | 🎨 Image display | Static images with tinting |
| `add_editable_text` | ✏️ Text input | Single-line text input fields |
| `add_editable_text_box` | 📄 Multi-line input | Multi-line text areas |
| `add_rich_text_block` | 🎭 Rich text | Markup-supported formatted text |
| `add_check_box` | ☑️ Boolean input | Checkbox controls with labels |
| `add_slider` | 🎚️ Value selection | Horizontal/vertical sliders |
| `add_progress_bar` | 📊 Progress display | Visual progress indicators |
| `add_spacer` | ⬜ Layout spacing | Invisible spacing elements |

### UMG Layout Panels
| Tool | Purpose | Key Features |
|------|---------|--------------|
| `add_canvas_panel` | 🎯 Absolute positioning | Pixel-perfect placement |
| `add_overlay` | 📚 Layered content | Z-ordered layering system |
| `add_horizontal_box` | ➡️ Horizontal layout | Linear horizontal arrangement |
| `add_vertical_box` | ⬇️ Vertical layout | Linear vertical arrangement |
| `add_scroll_box` | 📜 Scrollable content | Vertical/horizontal scrolling |
| `add_grid_panel` | 📋 Tabular layout | Row/column grid system |
| `add_widget_switcher` | 🔄 Tab system | Multi-page widget switching |

### UMG Advanced Components
| Tool | Purpose | Key Features |
|------|---------|--------------|
| `add_list_view` | 📋 Data lists | Scrollable data-driven lists |
| `add_tile_view` | 🎯 Grid display | Grid-based data visualization |
| `add_tree_view` | 🌳 Hierarchical data | Tree structure display |
| `populate_list_with_data` | 📊 Data binding | Fill lists with data items |

### Widget Property Management
| Tool | Purpose | Key Features |
|------|---------|--------------|
| `set_widget_property` | 🎛️ Universal setter | Set any widget property with type detection |
| `get_widget_property` | 🔍 Property inspection | Get current property values |
| `list_widget_properties` | 📋 Property discovery | List all available properties |
| `set_widget_transform` | 📐 Layout control | Position, size, scale, rotation, anchoring |
| `set_widget_visibility` | 👁️ Visibility control | Visible, hidden, collapsed states |
| `set_widget_z_order` | 📚 Layer management | Control rendering order |

### Widget Analysis & Discovery
| Tool | Purpose | Key Features |
|------|---------|--------------|
| `get_widget_blueprint_info` | 🔍 Widget inspection | Complete widget structure analysis |
| `list_widget_components` | 📋 Component listing | Hierarchy with properties |
| `get_widget_component_properties` | 🔎 Property details | Deep property inspection |
| `get_available_widget_types` | 📚 Type discovery | List all creatable widget types |
| `validate_widget_hierarchy` | ✅ Structure validation | Check for hierarchy issues |

### Event & Data Binding
| Tool | Purpose | Key Features |
|------|---------|--------------|
| `bind_input_events` | 🎮 Event handling | Mouse, keyboard, focus events |
| `get_available_events` | 📋 Event discovery | List available events per component |
| `create_data_binding_context` | 🔗 MVVM pattern | Data context creation |
| `setup_list_item_template` | 🎨 List templates | Data-driven list item design |

### Graph Analysis & Debugging
| Tool | Purpose | Key Features |
|------|---------|--------------|
| `list_event_graph_nodes` | 🗺️ Graph overview | All nodes with types and connections |
| `get_node_details` | 🔍 Node inspection | Detailed pin and property information |
| `list_blueprint_functions` | 📋 Function catalog | All Blueprint functions with signatures |
| `list_custom_events` | 🎬 Event inventory | Custom events in the graph |
| `summarize_event_graph` | 📖 Graph summary | Human-readable graph overview |

### Utility & System Tools
| Tool | Purpose | Key Features |
|------|---------|--------------|
| `add_widget_to_viewport` | 🖥️ Widget display | Show widgets in game viewport |
| `convert_svg_to_png` | 🔄 Format conversion | SVG to PNG with customization |
| `get_umg_guide` | 📚 Styling reference | Comprehensive UMG styling guide |
| `check_unreal_connection` | 🔧 Diagnostics | Test plugin connectivity |

### Workflow Patterns
The tools are designed for intelligent workflows:

1. **Discovery Flow**: `search_items` → `get_widget_blueprint_info` → `list_widget_components`
2. **Creation Flow**: `create_umg_widget_blueprint` → `add_[component]_to_widget` → `set_widget_property`
3. **Styling Flow**: `get_umg_guide` → `set_widget_property` → `set_widget_transform`
4. **Analysis Flow**: `list_event_graph_nodes` → `get_node_details` → `summarize_event_graph`

## 💡 Best Practices & AI Integration

### Performance Optimization
- **Always use full asset paths** from `search_items()` results for instant loading
- **Avoid partial names** when possible - they trigger expensive Asset Registry searches
- **Use `get_umg_guide()`** before styling to understand container-specific requirements
- **Batch property changes** when modifying multiple components

### Error Prevention
- **Start with discovery**: Use `search_items()` to find exact asset names
- **Validate hierarchies**: Use `list_widget_components()` to see available components
- **Check connections**: Use `check_unreal_connection()` when tools fail
- **Use exact names**: Copy exact names from discovery tools to avoid typos

### Intelligent Workflow Design
- **Plan before creating**: Use analysis tools to understand existing structures
- **Incremental development**: Build widgets step-by-step with validation
- **Template-driven**: Use `get_umg_guide()` for styling patterns and best practices
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


## License
MIT

## Questions

For questions, you can reach me on Discord - Buckley603

## Thank you
Thank you to everyone who helped teach me coding, Unreal Engine and AI.
Thank you to everyone who tries this product and contributes.
Thank you to the team that started the original project I split this off of.
