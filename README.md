<div align="center">

# VibeUE - AI-Powered Unreal Engine Development

https://www.vibeue.com/

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7%2B-orange)](https://www.unrealengine.com)
[![MCP](https://img.shields.io/badge/MCP-2025--11--25-blue)](https://modelcontextprotocol.io)

</div>

**VibeUE brings AI directly into Unreal Engine** with an In-Editor Chat Client and Model Context Protocol (MCP) integration. Control Blueprints, UMG widgets, materials, and assets through natural language.

## ✨ Key Features

- **In-Editor AI Chat** - Chat with AI directly inside Unreal Editor
- **Python API Services** - 9 specialized services with 203 methods for Blueprints, Materials, Widgets, and more
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

### 2. VibeUE Python API Services (9 services, 203 methods)
High-level services exposed to Python for common game development tasks:

| Service | Methods | Domain |
|---------|---------|--------|
| `BlueprintService` | 64 | Blueprint lifecycle, variables, functions, components, nodes |
| `MaterialService` | 26 | Materials and material instances |
| `MaterialNodeService` | 21 | Material graph expressions and connections |
| `WidgetService` | 14 | UMG widget blueprints and components |
| `InputService` | 20 | Enhanced Input actions, contexts, modifiers, triggers |
| `AssetDiscoveryService` | 13 | Asset search, import/export, references |
| `DataAssetService` | 10 | UDataAsset instances and properties |
| `DataTableService` | 13 | DataTable rows and structure |
| `ActorService` | 22 | Level actor management |

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

## 🔧 Plugin Dependencies

VibeUE automatically enables these required plugins during installation:

| Plugin | Purpose |
|--------|---------|| **PythonScriptPlugin** | Python runtime and Unreal Engine Python API || **EditorScriptingUtilities** | Blueprint and asset manipulation APIs |
| **EnhancedInput** | Input system discovery and configuration |
| **AudioCapture** | Speech-to-text input for in-editor chat |

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

## 🧠 Using VibeUE with External AI Agents

When using VibeUE's MCP server with external AI agents (Claude, GitHub Copilot, Cursor, etc.), **you must include the VibeUE instructions** in your AI system prompt or context.

### Why This Matters

The `Plugins/VibeUE/Content/instructions/vibeue.instructions.md` file contains:
- Critical API rules and gotchas (e.g., "compile before variable nodes")
- Skills system documentation (lazy-loaded knowledge domains)
- Common method naming mistakes to avoid
- Property format requirements for different services
- Essential safety rules (never block the editor, use full asset paths, etc.)

**Without these instructions, AI agents will make incorrect assumptions about the API and encounter failures.**

### Claude (Claude Desktop, Cursor, Windsurf)

1. Copy `Plugins/VibeUE/Content/instructions/vibeue.instructions.md` content
2. Add it to your Claude context with instructions like:

```markdown
# VibeUE API System Prompt

Include the full contents of vibeue.instructions.md here.
```

Or use Claude's **context window feature** to load the file directly.

### GitHub Copilot

1. Include VibeUE instructions in your workspace settings
2. Create `.github/copilot-instructions.md` and reference the VibeUE instructions
3. Add to your project's README or documentation

### VS Code + Custom LLM

If using VS Code with a custom LLM provider:

1. Add `Plugins/VibeUE/Content/instructions/vibeue.instructions.md` to your `.vscode/settings.json`:

```json
{
  "llm.systemPrompt": "Include contents of Plugins/VibeUE/Content/instructions/vibeue.instructions.md"
}
```

### Quick Reference: Essential Rules

The AI **must know**:
- ✅ Always use `discover_python_class()` before calling service methods
- ✅ Compile blueprints before adding variable nodes
- ✅ Use full asset paths (`/Game/Path/Asset`, not `Asset`)
- ✅ Property values are strings, not Python types
- ✅ Load skills with `manage_skills` for domain-specific knowledge
- ❌ Never guess method names - discover first
- ❌ Never use modal dialogs or blocking operations
- ❌ Never assume service counts or method availability

---

## 🎯 Skills System - Lazy-Loaded Domain Knowledge

VibeUE uses a **Skills System** to dramatically reduce AI context overhead while providing domain-specific guidance.

### How It Works

Instead of loading all documentation at once, skills are lazy-loaded on demand:

1. **AI detects the task** (e.g., "Create a blueprint with variables")
2. **Skill is automatically or manually loaded** via `manage_skills` tool
3. **Skill contains**: Critical rules, workflows, common mistakes, property formats
4. **AI uses skill knowledge** combined with live discovery via `discover_python_class`

### Available Skills

Each skill includes:
- **Critical Rules** - Gotchas that discovery can't tell you (e.g., method name mistakes)
- **Workflows** - Step-by-step patterns for common tasks
- **Common Mistakes** - Things to avoid (wrong property names, etc.)
- **Property Formats** - How to format values in Unreal string syntax

**8 Domain Skills:**
- `blueprints` - Blueprint lifecycle, variables, functions, components, nodes
- `materials` - Material creation and graph editing
- `enhanced-input` - Input Actions, Mapping Contexts, Modifiers, Triggers
- `data-tables` - Data Table creation and row management
- `data-assets` - Data Asset instances and properties
- `umg-widgets` - Widget Blueprint creation and styling
- `level-actors` - Level actor manipulation
- `asset-management` - Asset search, import/export, references

### Using Skills

**In-Editor Chat** - Skills auto-load based on keywords

**External AI** - Manually load with `manage_skills` tool:

```python
# List all available skills
manage_skills(action="list")

# Load a specific skill
manage_skills(action="load", skill_name="blueprints")

# Load multiple skills together (deduplicated discovery)
manage_skills(action="load", skill_names=["blueprints", "enhanced-input"])
```

Skill response includes:
- `vibeue_classes` - Services to discover (e.g., BlueprintService)
- `unreal_classes` - Native UE classes (e.g., EditorAssetLibrary)
- `content` - Markdown with workflows and critical rules
- `COMMON_MISTAKES` - Quick reference for frequent errors

### Workflow with Skills

The recommended pattern:

```python
import unreal

# 1. Load relevant skill for domain knowledge
manage_skills(action="load", skill_name="blueprints")
# ↓ Skill response tells you about BlueprintService methods and critical rules

# 2. Discover exact method signatures BEFORE calling
unreal.BlueprintService  # Already know this from skill
discover_python_class("unreal.BlueprintService", method_filter="variable")
# ↓ Discovery returns: add_variable, remove_variable, list_variables, ...

# 3. Use discovered signatures with parameters from skill
path = unreal.BlueprintService.create_blueprint("BP_Player", "Actor", "/Game/Blueprints")
unreal.BlueprintService.add_variable(path, "Health", "Float", "100.0")
unreal.BlueprintService.compile_blueprint(path)  # Critical rule from skill!
```

### Token Efficiency

**Before Skills:** 13,000 tokens of all docs loaded every conversation

**After Skills:** 2,500 base + domain skills on demand
- Blueprint task: 2.5k + 3.2k = 5.7k (56% reduction)
- Material task: 2.5k + 2.2k = 4.7k (64% reduction)
- Multi-domain: Load only what's needed (50-65% average)

---

All services are available via `unreal.<ServiceName>.<method>()`.

### Workflow: Discover Before Using

```python
# ALWAYS discover service methods first
# MCP: discover_python_class("unreal.BlueprintService")

# Then call methods with correct parameters
unreal.BlueprintService.create_blueprint("BP_MyActor", "Actor", "/Game/Blueprints")
```

### BlueprintService (64 methods)

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

### WidgetService (14 methods)

- `list_widget_blueprints(path)` - Find widget blueprints
- `add_component(path, type, name, parent)` - Add widget
- `get/set_property(path, component, property, value)` - Properties
- `get_hierarchy(path)` - Get widget tree
- `bind_event(path, event, function)` - Bind events

### InputService (20 methods)

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

### DataAssetService (10 methods)

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

### ActorService (22 methods)

ActorService provides comprehensive level actor manipulation:
- Actor discovery and queries
- Transform operations (position, rotation, scale)
- Selection management
- Spawning and destruction
- Property access
- And more

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

