<div align="center">

# VibeUE - AI-Powered Unreal Engine Development

https://www.vibeue.com/

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7%2B-orange)](https://www.unrealengine.com)
[![MCP](https://img.shields.io/badge/MCP-2025--11--25-blue)](https://modelcontextprotocol.io)

</div>

**VibeUE brings AI directly into Unreal Engine** with an In-Editor Chat Client and Model Context Protocol (MCP) integration. Control Blueprints, UMG widgets, materials, and assets through natural language.

## ✨ Key Features

- **In-Editor AI Chat** - Chat with AI directly inside Unreal Editor
- **Python API Services** - 14 specialized services with 300+ methods for Blueprints, Materials, Widgets, Niagara, Screenshots, Project/Engine Settings, and more
- **Full Unreal Python Access** - Execute any Unreal Engine Python API through MCP
- **MCP Discovery Tools** - 6 tools for exploring and executing Python in Unreal context
- **Custom Instructions** - Add project-specific context via markdown files
- **External IDE Integration** - Connect VS Code, Claude Desktop, Cursor, and Windsurf via MCP

---

## 🏗️ Architecture Overview

VibeUE uses a **Python-first architecture** that gives AI assistants access to:

### 1. MCP Discovery & Execution Tools (7 tools)
Lightweight MCP tools for exploring and executing Python:

| Tool | Purpose |
|------|---------|
| `discover_python_module` | Inspect module contents (classes, functions, constants) |
| `discover_python_class` | Get class methods, properties, and inheritance |
| `discover_python_function` | Get function signatures and docstrings |
| `execute_python_code` | Run Python code in Unreal Editor context |
| `list_python_subsystems` | List available UE editor subsystems |
| `read_logs` | Read and filter Unreal Engine log files with regex support |

**Note:** The `read_logs` MCP tool provides access to Unreal Engine's log files for debugging, error analysis, and workflow understanding.

### Log Reader Tool (`read_logs`)

The `read_logs` MCP tool provides comprehensive log file access with filtering and analysis capabilities:

**Actions:**
- `list` - Browse available log files by category (System, Blueprint, Niagara, VibeUE)
- `info` - Get file metadata (size, line count, last modified)
- `read` - Read file with pagination (default 2000 lines, offset support)
- `tail` - Get last N lines (like PowerShell's `Get-Content -Tail`)
- `head` - Get first N lines
- `filter` - Regex search with context lines and match limit
- `errors` - Find error messages
- `warnings` - Find warning messages
- `since` - Get new content since last read (by line number)
- `help` - Get detailed documentation

**File Aliases:**
- `main` or `system` → Main project log (FPS57.log)
- `chat` or `vibeue` → VibeUE chat history log
- `llm` → Raw LLM API request/response log
- Or use full file paths

**Examples:**
```python
# List all logs
read_logs(action="list")

# Filter by category
read_logs(action="list", category="Niagara")

# Get last 50 lines of main log
read_logs(action="tail", file="main", lines=50)

# Search for errors with context
read_logs(action="filter", file="main", pattern="ERROR|EXCEPTION", context_lines=5)

# Find compilation errors
read_logs(action="errors", file="main", max_matches=20)

# Read specific range
read_logs(action="read", file="chat", offset=1000, limit=500)

# Check for new content since line 2500
read_logs(action="since", file="main", last_line=2500)
```

### 2. VibeUE Python API Services (14 services, 300+ methods)
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
| `ScreenshotService` | 6 | Editor window and viewport screenshot capture for AI vision |
| `NiagaraService` | 38 | Niagara system lifecycle, emitters, parameters, rapid iteration |
| `NiagaraEmitterService` | 19 | Niagara emitter modules, renderers, properties |
| `ProjectSettingsService` | 10+ | Project settings, editor preferences, UI configuration |
| `EngineSettingsService` | 15+ | Engine settings, rendering, physics, audio, cvars, scalability |

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

**Domain Skills** (dynamically discovered from `Content/Skills/*/skill.md`):

Skills are automatically discovered at runtime from the `Content/Skills/` directory. Each skill folder contains a `skill.md` with YAML frontmatter defining its metadata. The system prompt's `{SKILLS}` token is replaced with a dynamically generated table of all available skills.

Current skills include: `blueprints`, `materials`, `enhanced-input`, `data-tables`, `data-assets`, `umg-widgets`, `level-actors`, `asset-management`, `screenshots`, `niagara-systems`, `niagara-emitters`

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

### NiagaraService (38 methods)

**Lifecycle:**
- `create_system(name, path, template)` - Create new Niagara system
- `compile_system(path)` / `compile_with_results(path)` - Compile with error messages
- `save_system(path)` - Save to disk
- `open_in_editor(path)` - Open in Niagara Editor

**Information:**
- `get_system_info(path)` - Comprehensive system information
- `get_system_properties(path)` - Effect type, determinism, warmup, bounds
- `summarize(path)` - AI-friendly system summary
- `list_emitters(path)` - List all emitters

**Emitter Management:**
- `add_emitter(system, emitter_asset, name)` - Add emitter to system
- `copy_emitter(src_system, src_emitter, target_system, new_name)` - Copy between systems
- `duplicate_emitter(system, source, new_name)` - Duplicate within system
- `remove_emitter(system, emitter)` - Remove emitter
- `rename_emitter(system, current, new)` - Rename emitter
- `enable_emitter(system, emitter, enabled)` - Enable/disable
- `move_emitter(system, emitter, index)` - Reorder emitter

**Parameter Management:**
- `list_parameters(path)` - List user-exposed parameters
- `get/set_parameter(path, name, value)` - Parameter access
- `add/remove_user_parameter(...)` - Add/remove parameters
- `list_rapid_iteration_params(system, emitter)` - List module parameters
- `set_rapid_iteration_param(system, emitter, name, value)` - Set module values (color, spawn rate, etc.)
- `set_rapid_iteration_param_by_stage(...)` - Set parameter in specific script stage

**Search & Utilities:**
- `search_systems(path, filter)` - Find Niagara systems
- `search_emitter_assets(path, filter)` - Find emitter assets
- `list_emitter_templates(path, filter)` - List available emitter templates
- `compare_systems(source, target)` - Compare two systems
- `copy_system_properties(target, source)` - Copy system settings
- `debug_activation(path)` - Debug why system isn't playing

### NiagaraEmitterService (19 methods)

**Module Management:**
- `list_modules(system, emitter, type)` - List all modules
- `get_module_info(system, emitter, module)` - Get module details
- `add_module(system, emitter, script, type)` - Add module
- `remove_module(system, emitter, module)` - Remove module
- `enable_module(system, emitter, module, enabled)` - Enable/disable module
- `set_module_input(system, emitter, module, input, value)` - Set module input
- `get_module_input(system, emitter, module, input)` - Get module input value
- `reorder_module(system, emitter, module, index)` - Reorder module

**Renderer Management:**
- `list_renderers(system, emitter)` - List renderers
- `get_renderer_details(system, emitter, index)` - Get renderer details
- `add_renderer(system, emitter, type)` - Add renderer (Sprite, Mesh, Ribbon, Light, Component)
- `remove_renderer(system, emitter, index)` - Remove renderer
- `enable_renderer(system, emitter, index, enabled)` - Enable/disable renderer
- `set_renderer_property(system, emitter, index, property, value)` - Set renderer property

**Script Discovery:**
- `search_module_scripts(filter, type)` - Find module scripts
- `list_builtin_modules(type)` - List built-in modules
- `get_script_info(path)` - Get script asset info

**Emitter Properties:**
- `get_emitter_properties(system, emitter)` - Get lifecycle and property info
- `get_rapid_iteration_parameters(system, emitter, type)` - Get rapid iteration parameters

### ScreenshotService (6 methods)

ScreenshotService enables AI vision by capturing editor content:
- `capture_editor_window(path)` - Capture entire editor window (works for blueprints, materials, etc.)
- `capture_viewport(path, width, height)` - Capture level viewport
- `capture_active_window(path)` - Capture foreground window
- `get_open_editor_tabs()` - List open editor tabs with asset info
- `get_active_window_title()` - Get focused window title
- `is_editor_window_active()` - Check if editor is in focus

### ProjectSettingsService (10+ methods)

ProjectSettingsService provides access to project configuration and editor preferences:
- `list_settings_categories()` - List all available settings categories
- `list_settings(category)` - List settings in a category
- `get_setting(category, setting)` - Get current setting value
- `set_setting(category, setting, value)` - Modify setting value
- `get_editor_style()` - Get editor UI appearance settings (toolbar icons, scale, colors)
- `set_editor_style(property, value)` - Modify editor appearance (SmallToolBarIcons, ApplicationScale, etc.)
- `get_project_info()` - Get project metadata (name, version, description)
- `set_project_info(property, value)` - Update project metadata
- `get_default_maps()` - Get editor/game startup map settings
- `set_default_maps(editor_map, game_map)` - Configure startup maps

### EngineSettingsService (15+ methods)

EngineSettingsService controls core engine configuration across multiple domains:

**Rendering Settings:**
- `get/set_rendering_setting(setting, value)` - Configure rendering options
- `list_rendering_settings()` - List available rendering settings

**Physics Settings:**
- `get/set_physics_setting(setting, value)` - Configure physics simulation
- `list_physics_settings()` - List available physics settings

**Audio Settings:**
- `get/set_audio_setting(setting, value)` - Configure audio system
- `list_audio_settings()` - List available audio settings

**Console Variables (CVars):**
- `get_cvar(name)` - Get console variable value
- `set_cvar(name, value)` - Set console variable (runtime changes)
- `list_cvars(filter)` - Search available console variables

**Scalability Settings:**
- `get_scalability_level(category)` - Get quality level (ViewDistance, AntiAliasing, etc.)
- `set_scalability_level(category, level)` - Set quality level (0-4: Low to Epic)
- `get_scalability_settings()` - Get all current scalability levels
- `apply_scalability_preset(preset)` - Apply preset (Low, Medium, High, Epic, Cinematic)

**Garbage Collection:**
- `get/set_gc_setting(setting, value)` - Configure garbage collection behavior
- `list_gc_settings()` - List available GC settings

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

### Configure Project and Engine Settings

```python
# Configure Editor UI Appearance
unreal.ProjectSettingsService.set_editor_style("SmallToolBarIcons", "true")
unreal.ProjectSettingsService.set_editor_style("ApplicationScale", "1.2")

# Set Project Metadata
unreal.ProjectSettingsService.set_project_info("ProjectName", "My Awesome Game")
unreal.ProjectSettingsService.set_project_info("Description", "An epic adventure")

# Configure Startup Maps
unreal.ProjectSettingsService.set_default_maps(
    editor_map="/Game/Levels/EditorLevel",
    game_map="/Game/Levels/MainMenu"
)

# Configure Rendering Settings
unreal.EngineSettingsService.set_rendering_setting("bDefaultFeatureLevelES31", "true")
unreal.EngineSettingsService.set_rendering_setting("DefaultGraphicsRHI", "DefaultGraphicsRHI_DX12")

# Adjust Graphics Quality via Scalability
unreal.EngineSettingsService.apply_scalability_preset("High")
# Or set individual categories
unreal.EngineSettingsService.set_scalability_level("ViewDistance", 3)  # 0-4: Low to Epic
unreal.EngineSettingsService.set_scalability_level("AntiAliasing", 4)

# Configure Physics Settings
unreal.EngineSettingsService.set_physics_setting("bEnableStabilization", "true")
unreal.EngineSettingsService.set_physics_setting("MaxSubsteps", "6")

# Set Console Variables (CVars) for runtime changes
unreal.EngineSettingsService.set_cvar("r.ScreenPercentage", "100")
unreal.EngineSettingsService.set_cvar("r.Bloom", "1")

# Configure Garbage Collection
unreal.EngineSettingsService.set_gc_setting("gc.MaxObjectsInGame", "2097152")
unreal.EngineSettingsService.set_gc_setting("gc.TimeBetweenPurgingPendingKillObjects", "60.0")
```

# 3. Add rows
unreal.DataTableService.add_row("/Game/Data/DT_Characters", "Hero", 
    '{"Name": "Hero", "Health": 100, "Damage": 25}')

# 4. Query rows
row = unreal.DataTableService.get_row("/Game/Data/DT_Characters", "Hero")
```

### Build Niagara VFX System

```python
# 1. Create system
result = unreal.NiagaraService.create_system("NS_Fire", "/Game/VFX")
path = result.asset_path

# 2. Add emitters
unreal.NiagaraService.add_emitter(path, "minimal", "Flames")
unreal.NiagaraService.add_emitter(path, "minimal", "Smoke")

# 3. Get available modules
modules = unreal.NiagaraEmitterService.search_module_scripts("Color", "Update")

# 4. Add color module to emitter
unreal.NiagaraEmitterService.add_module(path, "Flames", modules[0], "Update")

# 5. Set rapid iteration parameters (like spawn rate, color, lifetime)
params = unreal.NiagaraService.list_rapid_iteration_params(path, "Flames")
unreal.NiagaraService.set_rapid_iteration_param(
    path, "Flames", 
    "Constants.Flames.SpawnRate.SpawnRate", 
    "500.0"
)

# 6. Set emitter colors via rapid iteration params
unreal.NiagaraService.set_rapid_iteration_param(
    path, "Flames",
    "Constants.Flames.Color.Scale Color",
    "(3.0, 0.5, 0.0)"  # Orange fire
)

# 7. Compile and save
unreal.NiagaraService.compile_system(path)
unreal.NiagaraService.save_system(path)
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

Add project-specific context in `Plugins/VibeUE/Content/instructions/`:

```markdown
# Project: My Game

## Naming Conventions
- Blueprints: BP_<Type>_<Name>
- Widgets: WBP_<Name>
- Materials: M_<Surface>_<Variant>
```

All `.md` files in this directory are automatically concatenated and loaded as AI context.

### Dynamic Token Replacement

The system prompt supports dynamic token replacement. When the instructions are loaded, certain tokens are replaced with dynamically generated content:

| Token | Replacement | Source |
|-------|-------------|--------|
| `{SKILLS}` | Skills table with names, descriptions, and services | Scanned from `Content/Skills/*/skill.md` frontmatter |

**Example usage in `vibeue.instructions.md`:**

```markdown
## Available Skills

Load skills using `manage_skills(action="load", skill_name="<name>")`:

{SKILLS}
```

**Generated output:**

```markdown
| Skill | Description | Services |
|-------|-------------|----------|
| `blueprints` | Create and modify Blueprint assets... | BlueprintService, AssetDiscoveryService |
| `materials` | Create and edit materials... | MaterialService, MaterialNodeService |
...
```

This allows the skills list to stay in sync automatically when skills are added, removed, or modified. Each skill's metadata is defined in its `skill.md` YAML frontmatter:

```yaml
---
name: blueprints
display_name: Blueprint System
description: Create and modify Blueprint assets, variables, functions, components, and node graphs
vibeue_classes:
  - BlueprintService
  - AssetDiscoveryService
---
```

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

