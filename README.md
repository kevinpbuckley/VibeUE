<div align="center">

# VibeUE - AI-Powered Unreal Engine Development

https://www.vibeue.com/

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7%2B-orange)](https://www.unrealengine.com)
[![MCP](https://img.shields.io/badge/MCP-2025--11--25-blue)](https://modelcontextprotocol.io)

</div>

**VibeUE brings AI directly into Unreal Engine** with an In-Editor Chat Client and Model Context Protocol (MCP) integration. Control Blueprints, UMG widgets, materials, and assets through natural language.

## ✨ Key Features

- **In-Editor AI Chat** - Chat with AI directly inside Unreal Editor
- **Python API Services** - 10 specialized services with 150+ methods for Blueprints, Materials, Widgets, and more
- **Full Unreal Python Access** - Execute any Unreal Engine Python API through MCP
- **MCP Discovery Tools** - 6 tools for exploring and executing Python in Unreal context
- **Custom Instructions** - Add project-specific context via markdown files
- **External IDE Integration** - Connect VS Code, Claude Desktop, Cursor, and Windsurf via MCP

---

## 🏗️ Architecture Overview

VibeUE uses a **Python-first architecture** that gives AI assistants access to:

### 1. MCP Discovery & Execution Tools (6 tools)
Lightweight MCP tools for exploring and executing Python:

| Tool | Purpose |
|------|---------|
| `discover_python_module` | Inspect module contents (classes, functions, constants) |
| `discover_python_class` | Get class methods, properties, and inheritance |
| `discover_python_function` | Get function signatures and docstrings |
| `execute_python_code` | Run Python code in Unreal Editor context |
| `list_python_subsystems` | List available UE editor subsystems |

### 2. VibeUE Python API Services (10 services, 150+ methods)
High-level services exposed to Python for common game development tasks:

| Service | Methods | Domain |
|---------|---------|--------|
| `BlueprintService` | 48 | Blueprint lifecycle, variables, functions, components, nodes |
| `MaterialService` | 26 | Materials and material instances |
| `MaterialNodeService` | 21 | Material graph expressions and connections |
| `WidgetService` | 15 | UMG widget blueprints and components |
| `InputService` | 19 | Enhanced Input actions, contexts, modifiers, triggers |
| `AssetDiscoveryService` | 13 | Asset search, import/export, references |
| `DataAssetService` | 9 | UDataAsset instances and properties |
| `DataTableService` | 13 | DataTable rows and structure |
| `ActorService` | 3 | Level actor management |

### 3. Full Unreal Engine Python API
Direct access to all `unreal.*` modules:
- `unreal.EditorAssetLibrary` - Asset operations
- `unreal.EditorLevelLibrary` - Level manipulation
- `unreal.EditorUtilityLibrary` - Editor utilities
- `unreal.SystemLibrary` - System functions
- All Unreal Python APIs available in the editor

---

## 🚀 Installation

### Prerequisites
- Unreal Engine 5.7+
- Git (for manual installation)

### 1. Clone the Repository

```bash
cd /path/to/your/unreal/project/Plugins
git clone https://github.com/kevinpbuckley/VibeUE.git
```

### 2. Build the Plugin

Double-click to build:
```
Plugins/VibeUE/BuildPlugin.bat
```

### 3. Enable in Unreal

1. Open your project in Unreal Editor
2. Go to **Edit > Plugins**
3. Find **"VibeUE"** and enable it
4. Restart the editor

### 4. Configure API Key

1. Open **Window > VibeUE > AI Chat**
2. Click the ⚙️ gear icon
3. Get a free API key at [vibeue.com](https://vibeue.com)
4. Paste and save

---

## 💬 In-Editor AI Chat

The built-in chat interface runs directly in Unreal Editor:

- **Menu**: `Window > VibeUE > AI Chat`
- **Features**: Tool integration, conversation history, external MCP support
- **Providers**: VibeUE API (free) or OpenRouter

### Configuration (Project Settings > Plugins > VibeUE)

| Setting | Default | Description |
|---------|---------|-------------|
| **LLM Provider** | VibeUE | Select VibeUE or OpenRouter |
| **Temperature** | 0.2 | Creativity (0.0-1.0) |
| **Max Tool Iterations** | 100 | Max tool calls per turn |

---

## 📚 Python API Reference

All services are available via `unreal.<ServiceName>.<method>()`.

### Workflow: Discover Before Using

```python
# ALWAYS discover service methods first
# MCP: discover_python_class("unreal.BlueprintService")

# Then call methods with correct parameters
unreal.BlueprintService.create_blueprint("BP_MyActor", "Actor", "/Game/Blueprints")
```

### BlueprintService (48 methods)

**Lifecycle:**
- `create_blueprint(name, parent_class, path)` - Create new blueprint
- `compile_blueprint(path)` - Compile blueprint
- `reparent_blueprint(path, new_parent)` - Change parent class

**Variables:**
- `add_variable(path, name, type, default, ...)` - Add variable
- `remove_variable(path, name)` - Remove variable
- `list_variables(path)` - List all variables
- `get_variable_info(path, name)` - Get variable details
- `modify_variable(path, name, ...)` - Modify properties
- `search_variable_types(filter, category)` - Find available types

**Functions:**
- `create_function(path, name, is_pure)` - Create function
- `delete_function(path, name)` - Delete function
- `add_function_input/output(...)` - Add parameters
- `add_function_local_variable(...)` - Add local variables
- `get_function_info(path, name)` - Get function details

**Components:**
- `add_component(path, type, name, parent)` - Add component
- `remove_component(path, name)` - Remove component
- `get/set_component_property(...)` - Property access
- `get_component_hierarchy(path)` - Get hierarchy

**Nodes:**
- `add_*_node(...)` - Add nodes (branch, variable, math, etc.)
- `connect_nodes(...)` - Connect pins
- `get_nodes_in_graph(path, graph)` - List nodes
- `discover_nodes(path, search, category)` - Find node types
- `create_node_by_key(...)` - Create any node type

### MaterialService (26 methods)

**Lifecycle:**
- `create_material(name, path)` - Create material
- `create_instance(parent, name, path)` - Create instance
- `compile_material(path)` - Recompile shaders

**Properties:**
- `get/set_property(path, name, value)` - Property access
- `list_properties(path)` - List all properties
- `list_parameters(path)` - List parameters

**Instances:**
- `set_instance_scalar/vector/texture_parameter(...)` - Set overrides
- `clear_instance_parameter_override(...)` - Clear override

### MaterialNodeService (21 methods)

- `discover_types(category, search)` - Find expression types
- `create_expression(path, class, x, y)` - Create expression
- `connect_expressions(...)` - Connect nodes
- `connect_to_output(path, expr, output, property)` - Connect to material output
- `create_parameter(...)` - Create parameter expression

### WidgetService (15 methods)

- `list_widget_blueprints(path)` - Find widget blueprints
- `add_component(path, type, name, parent)` - Add widget
- `get/set_property(path, component, property, value)` - Properties
- `get_hierarchy(path)` - Get widget tree
- `bind_event(path, event, function)` - Bind events

### InputService (19 methods)

- `create_action(name, path, value_type)` - Create Input Action
- `create_mapping_context(name, path, priority)` - Create context
- `add_key_mapping(context, action, key)` - Add binding
- `add_modifier/trigger(...)` - Add modifiers/triggers
- `get_available_keys(filter)` - List bindable keys

### AssetDiscoveryService (13 methods)

- `search_assets(term, type)` - Find assets
- `save_asset(path)` / `save_all_assets()` - Save
- `import_texture(file, dest)` - Import texture
- `export_texture(asset, file)` - Export texture
- `get_asset_dependencies/referencers(path)` - References

### DataAssetService (9 methods)

- `search_types(filter)` - Find DataAsset subclasses
- `create_data_asset(class, path, name)` - Create instance
- `get/set_property(path, name, value)` - Property access
- `set_properties(path, json)` - Bulk set properties

### DataTableService (13 methods)

- `search_row_types(filter)` - Find row struct types
- `create_data_table(struct, path, name)` - Create table
- `add_row/add_rows(path, name, json)` - Add rows
- `get_row/update_row/remove_row(...)` - Row operations
- `get_row_struct(path)` - Get column schema

### ActorService (3 methods)

- `list_level_actors(class_filter)` - List actors
- `find_actors_by_class(class)` - Find by class
- `get_actor_info(name)` - Get actor details

---

## 🔧 Common Workflows

### Create Blueprint with Variables

```python
# 1. Create blueprint
path = unreal.BlueprintService.create_blueprint("BP_Player", "Actor", "/Game/Blueprints")

# 2. Add variables
unreal.BlueprintService.add_variable(path, "Health", "Float", "100.0")
unreal.BlueprintService.add_variable(path, "MaxHealth", "Float", "100.0")

# 3. Compile (REQUIRED before adding variable nodes)
unreal.BlueprintService.compile_blueprint(path)

# 4. Save
unreal.EditorAssetLibrary.save_asset(path)
```

### Build Material Graph

```python
# 1. Create material
path = "/Game/Materials/M_Custom"
unreal.MaterialService.create_material("M_Custom", "/Game/Materials")

# 2. Create parameter
param_id = unreal.MaterialNodeService.create_parameter(
    path, "Vector", "BaseColor", "Surface", "", -300, 0
)

# 3. Connect to output
unreal.MaterialNodeService.connect_to_output(path, param_id, "", "BaseColor")

# 4. Compile
unreal.MaterialService.compile_material(path)
```

### Work with DataTables

```python
# 1. Find row struct types
types = unreal.DataTableService.search_row_types("Character")

# 2. Create table
unreal.DataTableService.create_data_table("CharacterStats", "/Game/Data", "DT_Characters")

# 3. Add rows
unreal.DataTableService.add_row("/Game/Data/DT_Characters", "Hero", 
    '{"Name": "Hero", "Health": 100, "Damage": 25}')

# 4. Query rows
row = unreal.DataTableService.get_row("/Game/Data/DT_Characters", "Hero")
```

---

## 🌐 External IDE Integration

Connect VS Code, Claude Desktop, Cursor, or Windsurf to control Unreal via MCP.

### Enable MCP Server

In **Project Settings > Plugins > VibeUE**:

| Setting | Default | Description |
|---------|---------|-------------|
| **Enable MCP Server** | Enabled | Toggle HTTP server |
| **Port** | 8088 | MCP endpoint port |
| **API Key** | (empty) | Bearer token |

Server runs at `http://127.0.0.1:8088/mcp`

### VS Code Configuration

Create `.vscode/mcp.json`:

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

### Claude Desktop / Cursor / Windsurf

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

---

## 📝 Custom Instructions

Add project-specific context in `Plugins/VibeUE/Config/Instructions/`:

```markdown
# Project: My Game

## Naming Conventions
- Blueprints: BP_<Type>_<Name>
- Widgets: WBP_<Name>
- Materials: M_<Surface>_<Variant>
```

All `.md` files are automatically included as AI context.

---

## 🔌 External MCP Servers

Connect additional MCP servers via `Config/vibeue.mcp.json`:

```json
{
  "servers": {
    "my-tool": {
      "type": "stdio",
      "command": "python",
      "args": ["-m", "my_mcp_tool"]
    }
  }
}
```

---

## ⚠️ Important Rules

### Always Discover First
```python
# DON'T guess APIs - discover them
# MCP: discover_python_class("unreal.BlueprintService")
```

### Compile Before Variable Nodes
```python
add_variable(path, "Health", "Float")
compile_blueprint(path)  # REQUIRED!
add_get_variable_node(path, graph, "Health", 0, 0)  # Now works
```

### Use Full Asset Paths
```python
# CORRECT
"/Game/Blueprints/BP_Player"

# WRONG
"BP_Player"
```

### Never Block the Editor
Avoid: modal dialogs, `input()`, long `time.sleep()`, infinite loops

---

## 📂 Directory Structure

```
Plugins/VibeUE/
├── Source/VibeUE/
│   ├── Public/PythonAPI/      # Python service headers
│   └── Private/PythonAPI/     # Python service implementations
├── Config/
│   ├── Instructions/          # Custom instruction files
│   └── vibeue.mcp.json        # External MCP servers
├── Content/instructions/      # AI system prompts
└── VibeUE.uplugin
```

---

## 🤝 Community

- **Discord**: https://discord.gg/hZs73ST59a
- **Documentation**: https://www.vibeue.com/docs

---

## 📄 License

VibeUE is available on the Unreal Marketplace and GitHub.

