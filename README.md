<div align="center">

# VibeUE - AI-Powered Unreal Engine Development

https://www.vibeue.com/

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7%2B-orange)](https://www.unrealengine.com)
[![MCP](https://img.shields.io/badge/MCP-2025--11--25-blue)](https://modelcontextprotocol.io)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

</div>

**VibeUE brings AI directly into Unreal Engine** with an In-Editor Chat Client and Model Context Protocol (MCP) integration. Plan, inspect, create, and modify Blueprints, assets, materials, animation, landscapes, audio, UI, AI behavior, project settings, and more through natural language.

> **🔑 A free VibeUE API key is required** for the MCP server and AI Chat to work. Get yours at **[vibeue.com/login](https://www.vibeue.com/login)** — then paste it into the VibeUE settings gear icon inside Unreal Editor. See the [AI Clients setup guide](https://www.vibeue.com/docs/ai-clients) for IDE configuration.

## ✨ Key Features

- **In-Editor AI Chat** - Chat with AI directly inside Unreal Editor
- **Python API Services** - 29 specialized services with 950 methods for Blueprints, Materials, Widgets, Landscape Terrain, Splines, Foliage, Animation Sequences, Animation Blueprints, Animation Montages, Niagara, Skeletons, Sound Cues, MetaSounds, Gameplay Tags, Screenshots, Viewport Control, Runtime Virtual Textures, StateTree Behavior, Editor Transactions, Project/Engine Settings, and more
- **Full Unreal Python Access** - Execute any Unreal Engine Python API through MCP
- **MCP Tools** - 10 tools for discovery, execution, asset workflows, debugging, terrain generation, and web research
- **Domain Skills** - 30 lazy-loaded skill packs covering Blueprints, graph editing, materials, terrain, animation, audio, AI, gameplay tags, widgets, viewport, data, and more
- **Custom Instructions** - Add project-specific context via markdown files
- **External IDE Integration** - Connect VS Code, Claude Code, Cursor, and AntiGravity via MCP

---

## 🏗️ Architecture Overview

VibeUE uses a **Python-first architecture** that gives AI assistants access to:

### 1. MCP Tools (10 tools)
Lightweight MCP tools for AI interaction with Unreal:

| Tool | Purpose |
|------|---------|
| `discover_python_module` | Inspect module contents (classes, functions, constants) |
| `discover_python_class` | Get class methods, properties, and inheritance |
| `discover_python_function` | Get function signatures and docstrings |
| `execute_python_code` | Run Python code in Unreal Editor context |
| `list_python_subsystems` | List available UE editor subsystems |
| `manage_skills` | Load domain-specific knowledge on demand |
| `manage_asset` | Search, open, save, move, duplicate, and delete assets safely |
| `read_logs` | Read and filter Unreal Engine log files with regex support |
| `terrain_data` | Generate real-world heightmaps, map images, and water feature data from geographic coordinates |
| `deep_research` | Web search, page fetching, and GPS geocoding — no API key required |

**Note:** The `read_logs` MCP tool provides access to Unreal Engine's log files for debugging, error analysis, and workflow understanding.

**Note:** The `manage_asset` MCP tool is the preferred path for asset search, save, move, duplicate, and delete workflows. It avoids ad hoc Python asset handling and preserves references when renaming or moving content.

**Note:** The `terrain_data` and `deep_research` tools work together for real-world terrain workflows: geocode a place name → generate a heightmap → import into a landscape → fetch water features.

#### MCP Tool Reference

##### Core Discovery Tools

**`discover_python_module`**
```python
discover_python_module(
    module_name: str,              # e.g., "unreal" (lowercase for UE module)
    name_filter: str = "",         # Filter by name substring (case-insensitive)
    include_classes: bool = True,  # Include classes in results
    include_functions: bool = True, # Include functions in results
    max_items: int = 100,          # Maximum items to return (0 = unlimited)
    case_sensitive: bool = False   # Whether filtering is case-sensitive
)
```
**Example:** `discover_python_module("unreal", name_filter="Blueprint")`

**`discover_python_class`**
```python
discover_python_class(
    class_name: str,                # Fully qualified class name (e.g., "unreal.BlueprintService")
    method_filter: str = "",        # Filter methods by name substring
    include_inherited: bool = False, # Include inherited methods
    include_private: bool = False,  # Include private methods starting with _
    max_methods: int = 0            # Maximum methods to return (0 = unlimited)
)
```
**Example:** `discover_python_class("unreal.BlueprintService", method_filter="variable")`

**`discover_python_function`**
```python
discover_python_function(
    function_name: str  # Fully qualified function name (e.g., "unreal.load_asset")
)
```

**`list_python_subsystems`**
```python
list_python_subsystems()  # No parameters - returns all UE editor subsystems
```
**Usage:** Access via `unreal.get_editor_subsystem(unreal.SubsystemName)`

##### Execution Tool

**`execute_python_code`**
```python
execute_python_code(
    code: str  # Python code to execute (must start with "import unreal")
)
```
**Important:** Use `import unreal` (lowercase). For subsystems: `unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)`

**Returns:** stdout, stderr, and execution status

##### Skills System Tool

**`manage_skills`**
```python
# List all available skills
manage_skills(action="list")

# Suggest skills based on query
manage_skills(action="suggest", query="create widget button")

# Load single skill
manage_skills(action="load", skill_name="blueprints")

# Load multiple skills together (more efficient - deduplicated discovery)
manage_skills(action="load", skill_names=["blueprints", "enhanced-input"])
```

Skill names: `animation-blueprint`, `animation-editing`, `animation-montage`, `animsequence`, `asset-management`, `blueprint-graphs`, `blueprints`, `data-assets`, `data-tables`, `engine-settings`, `enhanced-input`, `enum-struct`, `foliage`, `gameplay-tags`, `landscape`, `landscape-auto-material`, `landscape-materials`, `level-actors`, `materials`, `metasounds`, `niagara-emitters`, `niagara-systems`, `project-settings`, `screenshots`, `skeleton`, `sound-cues`, `state-trees`, `terrain-data`, `umg-widgets`

##### Asset Workflow Tool

**`manage_asset`**
```python
# Search before acting
manage_asset(action="search", search_term="BP_Player", asset_type="Blueprint")

# Open or save a specific asset
manage_asset(action="open", asset_path="/Game/Blueprints/BP_Player")
manage_asset(action="save", asset_path="/Game/Blueprints/BP_Player")

# Save everything dirty before a build or launch
manage_asset(action="save_all")

# Move or rename while preserving references
manage_asset(action="move", source_path="/Game/StateTree/STT_Rotate", destination_path="/Game/StateTree/Tasks/STT_Rotate")
```

**Important:** Use `move`, not duplicate + delete, when the intent is a rename or relocation. Duplicate creates a second asset identity.

##### Log Reading Tool

**`read_logs`**
```python
# List all logs
read_logs(action="list")

# Filter by category: System, Blueprint, Niagara, VibeUE
read_logs(action="list", category="Niagara")

# Get file info
read_logs(action="info", file="main")

# Read paginated content
read_logs(action="read", file="main", offset=0, limit=2000)

# Get last N lines
read_logs(action="tail", file="main", lines=50)

# Get first N lines
read_logs(action="head", file="main", lines=50)

# Regex filter with context
read_logs(
    action="filter",
    file="main",
    pattern="ERROR|EXCEPTION",
    context_lines=5,
    max_matches=100,
    case_sensitive=False
)

# Find errors
read_logs(action="errors", file="main", max_matches=20)

# Find warnings
read_logs(action="warnings", file="main", max_matches=20)

# Get new content since last read
read_logs(action="since", file="main", last_line=2500)

# Get help documentation
read_logs(action="help")
```

**File Aliases:**
- `main`, `system` → Project log (FPS57.log)
- `chat`, `vibeue` → Chat history log
- `llm` → Raw LLM API log

##### Terrain Data Tool

**`terrain_data`**
```python
# Preview elevation stats and get suggested settings
terrain_data(action="preview_elevation", lng=-122.4194, lat=37.7749)

# Generate a heightmap matching your landscape resolution
terrain_data(
    action="generate_heightmap",
    lng=-122.4194, lat=37.7749,
    base_level=0,
    height_scale=100,
    resolution=1009,        # MUST match landscape resolution
    format="png"
)

# Get a satellite or map reference image
terrain_data(
    action="get_map_image",
    lng=-122.4194, lat=37.7749,
    style="satellite-v9"    # satellite-v9, outdoors-v11, streets-v11, light-v10, dark-v10
)

# List available map image styles
terrain_data(action="list_styles")

# Fetch water features (rivers, lakes, ponds) for the same area
terrain_data(
    action="get_water_features",
    lng=-122.4194, lat=37.7749,
    map_size=17.28
)
```

**Parameters (generate_heightmap):**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `action` | string | *(required)* | `generate_heightmap`, `preview_elevation`, `get_map_image`, `list_styles`, `get_water_features` |
| `lng` | number | — | Longitude of center point |
| `lat` | number | — | Latitude of center point |
| `format` | string | `png` | Output: `png`, `raw`, `zip` |
| `resolution` | number | 1081 | Output NxN pixels — MUST match landscape resolution |
| `map_size` | number | 17.28 | Map size in km |
| `base_level` | number | 0 | Base elevation offset in meters |
| `height_scale` | number | 100 | Height scale % (1-250) |
| `water_depth` | number | 40 | Water depth in C:S units |
| `blur_passes` | number | 10 | Plains smoothing passes |
| `sharpen` | boolean | true | Apply sharpening |
| `save_path` | string | `Saved/Terrain/` | Custom output path |

**Workflow:** `preview_elevation` → use suggested `base_level` and `height_scale` → `generate_heightmap` with `resolution` matching your landscape → `import_heightmap` via LandscapeService.

**Water workflow:** After importing a heightmap, call `get_water_features` with the same `lng`/`lat`/`map_size`. This saves a JSON file to `Saved/Terrain/` with rivers (`ue5_points`) and water bodies (`ue5_rings`) in origin-centered UE5 coordinates. Use the saved JSON to create landscape splines for rivers and mesh planes for lakes.

##### Deep Research Tool

**`deep_research`**
```python
# Web search (DuckDuckGo)
deep_research(action="search", query="Unreal Engine landscape best practices")

# Fetch URL as clean markdown
deep_research(action="fetch_page", url="https://dev.epicgames.com/documentation/...")

# Place name → GPS coordinates
deep_research(action="geocode", query="Mount Fuji")

# GPS coordinates → place name
deep_research(action="reverse_geocode", lat=35.3606, lng=138.7274)
```

**Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `action` | string | Yes | `search`, `fetch_page`, `geocode`, `reverse_geocode` |
| `query` | string | For search/geocode | Search topic or place name |
| `url` | string | For fetch_page | Full URL to fetch as markdown |
| `lat` | number | For reverse_geocode | Latitude |
| `lng` | number | For reverse_geocode | Longitude |

**Typical workflows:**
- Research: `search` → `fetch_page` on best URL → synthesize
- Terrain: `geocode "Mount Fuji"` → pass lat/lng to `terrain_data`

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

### 2. VibeUE Python API Services (29 services, 950 methods)
High-level services exposed to Python for common game development tasks:

| Service | Methods | Domain |
|---------|---------|--------|
| `AnimSequenceService` | 89 | Animation sequence creation, keyframes, bone tracks, curves, notifies, preview |
| `BlueprintService` | 87 | Blueprint lifecycle, variables, functions, components, nodes, batch graph builder |
| `AnimMontageService` | 62 | Animation montages: sections, slots, segments, branching points, blend settings |
| `SkeletonService` | 53 | Skeleton & skeletal mesh manipulation, bones, sockets, retargeting, curves, blend profiles |
| `LandscapeService` | 68 | Landscape creation, sculpting, heightmaps, weight layers, holes, splines |
| `AnimGraphService` | 38 | Animation Blueprint state machines, states, transitions, anim nodes |
| `NiagaraService` | 37 | Niagara system lifecycle, emitters, parameters, settings discovery |
| `MaterialService` | 30 | Materials and material instances |
| `MetaSoundService` | 17 | MetaSound graph authoring, nodes, interfaces, inputs/outputs, and wiring |
| `ActorService` | 33 | Level actor management, viewport camera control, transform lock/constraints |
| `InputService` | 23 | Enhanced Input actions, contexts, modifiers, triggers |
| `EngineSettingsService` | 23 | Engine settings, rendering, physics, audio, cvars, scalability |
| `NiagaraEmitterService` | 23 | Niagara emitter modules, renderers, properties |
| `MaterialNodeService` | 40 | Material graph expressions and connections |
| `EnumStructService` | 20 | User-defined enums and structs (create, edit, delete) |
| `AssetDiscoveryService` | 20 | Asset search, import/export, references, move/rename workflows |
| `LandscapeMaterialService` | 22 | Landscape material layers, blend nodes, auto-material creation, layer info objects, grass output |
| `WidgetService` | 41 | UMG widget blueprints, components, snapshots, styling, animation, preview/PIE validation, and MVVM ViewModel bindings |
| `ProjectSettingsService` | 16 | Project settings, editor preferences, UI configuration |
| `FoliageService` | 15 | Foliage type management, scatter placement, layer-aware painting, instance queries |
| `GameplayTagService` | 8 | Gameplay tag CRUD: add, remove, rename, list, filter, hierarchy inspection |
| `DataTableService` | 15 | DataTable rows and structure |
| `DataAssetService` | 11 | UDataAsset instances and properties |
| `ScreenshotService` | 5 | Editor window and viewport screenshot capture for AI vision |
| `ViewportService` | 19 | Viewport camera type (perspective/ortho), view mode, FOV, clip planes, exposure, game view, cinematic control, camera speed, viewport layout (single/quad) |
| `RuntimeVirtualTextureService` | 4 | Runtime Virtual Texture assets, RVT volume actors, and landscape RVT assignment |
| `SoundCueService` | 38 | Sound cue graph editing, sound node creation, wiring, and audio behavior authoring |
| `StateTreeService` | 77 | StateTree asset creation, state hierarchy, state type/link configuration, editor selection, tasks, evaluators, conditions, transitions, delegate bindings, parameters, component overrides, property bindings, compile/save |
| `EditorTransactionService` | 16 | Undo/redo, transaction grouping, history inspection, buffer reset |

### 3. Full Unreal Engine Python API
Direct access to all `unreal.*` modules:
- `unreal.EditorAssetLibrary` - Asset operations
- `unreal.EditorActorSubsystem` - Level actor manipulation (via `unreal.get_editor_subsystem()`)
- ~~`unreal.EditorLevelLibrary`~~ - **DEPRECATED** — use `EditorActorSubsystem` instead
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

Or specify your Unreal Engine path explicitly (skips auto-detection):
```
Plugins/VibeUE/BuildPlugin.bat "C:\Program Files\Epic Games\UE_5.7"
```

If the provided path is invalid, the script falls back to automatic detection.

### 3. Enable in Unreal

1. Open your project in Unreal Editor
2. Go to **Edit > Plugins**
3. Find **"VibeUE"** and enable it
4. Restart the editor

### 4. Configure API Key

1. Open **Tools > VibeUE > AI Chat**
2. Click the ⚙️ gear icon
3. Get a free API key at [vibeue.com/login](https://www.vibeue.com/login)
4. Paste the key into the **VibeUE API Key** field and click **Save**

> **⚠️ Required for MCP tools:** The VibeUE API key is validated at startup. If no valid key is configured, all MCP tools will return an error. Get your free key at [vibeue.com/login](https://www.vibeue.com/login).

---

## 🔧 Plugin Dependencies

VibeUE automatically enables these required plugins during installation:

| Plugin | Purpose |
|--------|---------|
| **PythonScriptPlugin** | Python runtime and Unreal Engine Python API |
| **EditorScriptingUtilities** | Blueprint and asset manipulation APIs |
| **EnhancedInput** | Input system discovery and configuration |
| **AudioCapture** | Speech-to-text input for in-editor chat |
| **Niagara** | Niagara VFX system and emitter manipulation |
| **MeshModelingToolset** | Skeleton modifier for bone manipulation |

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

When using VibeUE's MCP server with external AI agents (Claude Code, GitHub Copilot, Cursor, Antigravity, etc.), **you must include the VibeUE instructions** in your AI system prompt or context.

> **Ready-made templates**: See [`Content/samples/README.md`](Content/samples/README.md) for copy-paste setup instructions for each supported AI tool.

### Why This Matters

The `Plugins/VibeUE/Content/instructions/vibeue.instructions.md` file contains:
- Critical API rules and gotchas (e.g., "compile before variable nodes")
- Skills system documentation (lazy-loaded knowledge domains)
- Common method naming mistakes to avoid
- Property format requirements for different services
- Essential safety rules (never block the editor, use full asset paths, etc.)

**Without these instructions, AI agents will make incorrect assumptions about the API and encounter failures.**

### Claude Code ✅ Import Supported (Recommended)

Create `CLAUDE.md` at your project root:

```markdown
# My Unreal Project

@Plugins/VibeUE/Content/samples/AGENTS.md.sample
```

The `@` directive inlines the file automatically — no copying needed.

### Generic agents (AGENTS.md)

Copy `Plugins/VibeUE/Content/samples/AGENTS.md.sample` to your project root as `AGENTS.md`. Read by OpenAI Codex, Cursor, and other agents that follow the AGENTS.md convention.

### GitHub Copilot

Copy `Plugins/VibeUE/Content/samples/AGENTS.md.sample` to:

```
.github/copilot-instructions.md
```

### Cursor

Copy `Plugins/VibeUE/Content/samples/AGENTS.md.sample` to:

```
.cursor/rules/vibeue.mdc
```

### Google Antigravity

Copy `Plugins/VibeUE/Content/samples/AGENTS.md.sample` to:

```
.agent/rules/vibeue.md
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

Current skills include: `animation-blueprint`, `animation-editing`, `animation-montage`, `animsequence`, `asset-management`, `blueprint-graphs`, `blueprints`, `data-assets`, `data-tables`, `engine-settings`, `enhanced-input`, `enum-struct`, `foliage`, `gameplay-tags`, `landscape`, `landscape-auto-material`, `landscape-materials`, `level-actors`, `materials`, `metasounds`, `niagara-emitters`, `niagara-systems`, `project-settings`, `screenshots`, `skeleton`, `sound-cues`, `state-trees`, `terrain-data`, `umg-widgets`, `viewport`

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

### BlueprintService (87 methods)

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

### AnimGraphService (38 methods)

AnimGraphService provides comprehensive Animation Blueprint manipulation for state machines, states, transitions, and animation nodes:

**State Machine Management:**
- `add_state_machine(path, name, x, y)` - Add state machine to AnimGraph
- `list_state_machines(path)` - List all state machines
- `get_state_machine_info(path, name)` - Get detailed state machine info

**State Management:**
- `add_state(path, machine, name, x, y)` - Add state to state machine
- `remove_state(path, machine, name, remove_transitions)` - Remove state
- `list_states_in_machine(path, machine)` - List all states and transitions
- `get_state_info(path, machine, state)` - Get detailed state info
- `open_anim_state(path, machine, state)` - Open state in editor

**Transition Management:**
- `add_transition(path, machine, source, dest, blend_duration)` - Add transition
- `remove_transition(path, machine, source, dest)` - Remove transition
- `get_state_transitions(path, machine, state)` - Get transitions for state
- `open_transition(path, machine, source, dest)` - Open transition rule in editor

**Conduit Management:**
- `add_conduit(path, machine, name, x, y)` - Add conduit node

**Animation Nodes:**
- `add_sequence_player(path, graph, anim_path, x, y)` - Add sequence player
- `add_blend_space_player(path, graph, bs_path, x, y)` - Add blend space player
- `add_blend_by_bool(path, graph, x, y)` - Add Blend By Bool node
- `add_blend_by_int(path, graph, num_poses, x, y)` - Add Blend By Int node
- `add_layered_blend(path, graph, x, y)` - Add Layered Blend Per Bone
- `add_slot_node(path, graph, slot_name, x, y)` - Add slot node
- `add_save_cached_pose(path, graph, cache_name, x, y)` - Add Save Cached Pose
- `add_use_cached_pose(path, graph, cache_name, x, y)` - Add Use Cached Pose
- `add_two_bone_ik_node(path, graph, x, y)` - Add Two Bone IK
- `add_modify_bone_node(path, graph, bone_name, x, y)` - Add Modify Bone

**Node Connections:**
- `connect_anim_nodes(path, graph, source_id, source_pin, target_id, target_pin)` - Connect pose pins
- `connect_to_output_pose(path, graph, source_id, source_pin)` - Connect to Output Pose
- `disconnect_anim_node(path, graph, node_id, pin_name)` - Disconnect node

**Asset Management:**
- `set_sequence_player_asset(path, graph, node_id, anim_path)` - Set animation on player
- `set_blend_space_asset(path, graph, node_id, bs_path)` - Set blend space on player
- `get_node_animation_asset(path, graph, node_id)` - Get current animation asset

**Information & Navigation:**
- `is_anim_blueprint(path)` - Check if asset is AnimBP
- `get_skeleton(path)` - Get skeleton used by AnimBP
- `get_preview_mesh(path)` - Get preview skeletal mesh
- `get_parent_class(path)` - Get parent class
- `get_output_pose_node_id(path, graph)` - Get Output Pose node ID
- `get_used_anim_sequences(path)` - List all used animations
- `list_graphs(path)` - List all graphs in AnimBP
- `open_anim_graph(path, graph)` - Open graph in editor
- `focus_node(path, node_id)` - Focus on specific node

### AnimMontageService (62 methods)

AnimMontageService provides comprehensive CRUD operations for Animation Montage assets including section management, slot tracks, animation segments, branching points, and blend settings:

**Discovery:**
- `list_montages(path, skeleton_filter)` - List all montages in a path
- `get_montage_info(path)` - Get comprehensive montage information
- `find_montages_for_skeleton(skeleton_path)` - Find all montages compatible with a skeleton
- `find_montages_using_animation(anim_path)` - Find montages using a specific animation

**Properties:**
- `get_montage_length(path)` - Get total duration in seconds
- `get_montage_skeleton(path)` - Get skeleton asset path
- `set_blend_in(path, time, option)` - Set blend in settings (Linear, Cubic, etc.)
- `set_blend_out(path, time, option)` - Set blend out settings
- `get_blend_settings(path)` - Get all blend settings
- `set_blend_out_trigger_time(path, time)` - Set when blend out begins

**Section Management (C++ - required for TArray modification):**
- `list_sections(path)` - List all sections with timing info
- `get_section_info(path, name)` - Get detailed section info
- `get_section_index_at_time(path, time)` - Get section index at time
- `get_section_name_at_time(path, time)` - Get section name at time
- `add_section(path, name, start_time)` - Add new section
- `remove_section(path, name)` - Remove section
- `rename_section(path, old_name, new_name)` - Rename section
- `set_section_start_time(path, name, time)` - Move section
- `get_section_length(path, name)` - Get section duration

**Section Linking (Branching):**
- `get_next_section(path, section)` - Get linked next section
- `set_next_section(path, section, next)` - Link section to next
- `set_section_loop(path, section, loop)` - Set section to loop
- `get_all_section_links(path)` - Get complete flow chart
- `clear_section_link(path, section)` - Clear link (ends montage)

**Slot Tracks:**
- `list_slot_tracks(path)` - List all slot tracks
- `get_slot_track_info(path, index)` - Get track details
- `add_slot_track(path, slot_name)` - Add new slot track
- `remove_slot_track(path, index)` - Remove track
- `set_slot_name(path, index, name)` - Change slot name
- `get_all_used_slot_names(path)` - Get unique slot names

**Animation Segments (Multiple Animations per Montage):**
- `list_anim_segments(path, track_index)` - List segments in track
- `get_anim_segment_info(path, track, segment)` - Get segment details
- `add_anim_segment(path, track, anim_path, start_time, play_rate)` - Add animation
- `remove_anim_segment(path, track, segment)` - Remove segment
- `set_segment_start_time(path, track, segment, time)` - Move segment
- `set_segment_play_rate(path, track, segment, rate)` - Set playback speed
- `set_segment_start_position(path, track, segment, pos)` - Trim start
- `set_segment_end_position(path, track, segment, pos)` - Trim end
- `set_segment_loop_count(path, track, segment, count)` - Set loop count

**Notifies:**
- `list_notifies(path)` - List all notifies
- `add_notify(path, class, time, name)` - Add instant notify
- `add_notify_state(path, class, start, duration, name)` - Add state notify
- `remove_notify(path, index)` - Remove notify
- `set_notify_trigger_time(path, index, time)` - Move notify
- `set_notify_link_to_section(path, index, section)` - Link to section

**Branching Points:**
- `list_branching_points(path)` - List all branching points
- `add_branching_point(path, name, time)` - Add frame-accurate event
- `remove_branching_point(path, index)` - Remove branching point
- `is_branching_point_at_time(path, time)` - Check for branching point

**Root Motion:**
- `get/set_enable_root_motion_translation(path, enable)` - Control translation
- `get/set_enable_root_motion_rotation(path, enable)` - Control rotation
- `get_root_motion_at_time(path, time)` - Get root motion transform

**Creation:**
- `create_montage_from_animation(anim, dest, name)` - Create from animation
- `create_empty_montage(skeleton, dest, name)` - Create empty montage
- `duplicate_montage(source, dest, name)` - Duplicate montage

**Editor:**
- `open_montage_editor(path)` - Open in Animation Editor
- `refresh_montage_editor(path)` - Refresh UI after modifications
- `jump_to_section(path, section)` - Preview jump to section
- `set_preview_time(path, time)` - Set preview playhead
- `play_preview(path, start_section)` - Play in preview

### MaterialService (30 methods)

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

### MaterialNodeService (40 methods)

- `discover_types(category, search)` - Find expression types
- `create_expression(path, class, x, y)` - Create expression
- `connect_expressions(...)` - Connect nodes
- `connect_to_output(path, expr, output, property)` - Connect to material output
- `create_parameter(...)` - Create parameter expression

### WidgetService (41 methods)

- `list_widget_blueprints(path)`, `widget_blueprint_exists(path)`, `widget_exists(path, component)` - Discover widget assets and validate targets before editing
- `get_hierarchy(path)`, `get_root_widget(path)`, `list_components(path)` - Inspect widget trees and hierarchy state
- `get_widget_snapshot(path)`, `get_component_snapshot(path, component)` - Capture hierarchy, slot layout, and property state in a single call
- `add_component(path, type, name, parent)`, `remove_component(path, name)`, `rename_widget(path, old_name, new_name)`, `validate(path)` - Manage widget composition safely
- `get_component_properties(path, component)`, `list_properties(path, component)`, `get/set_property(path, component, property, value)`, `search_types(filter)` - Query and edit widget properties and available types
- `get_available_events(path, component, type)`, `bind_event(path, widget, event, function)` - Discover and wire widget events
- `add_view_model(path, class, name, creation_type)`, `remove_view_model(path, name)`, `list_view_models(path)` - Register MVVM ViewModels
- `add_view_model_binding(path, vm, vm_prop, widget, widget_prop, mode)`, `remove_view_model_binding(path, index)`, `list_view_model_bindings(path)` - Create and manage MVVM bindings
- `set_font/get_font(...)`, `set_brush/get_brush(...)` - Apply full font and brush styling APIs for UMG widgets
- `list_animations(path)`, `create_animation(path, name)`, `remove_animation(path, name)`, `add_animation_track(path, anim, widget, property)`, `add_keyframe(path, anim, widget, property, keyframe)` - Author UMG animations
- `capture_preview(path)`, `start_pie()`, `stop_pie()`, `is_pie_running()`, `spawn_widget_in_pie(path)`, `get_live_property(handle, component, property)`, `remove_widget_from_pie(handle)` - Preview and runtime validation workflows

### InputService (23 methods)

- `create_action(name, path, value_type)` - Create Input Action
- `create_mapping_context(name, path, priority)` - Create context
- `add_key_mapping(context, action, key)` - Add binding
- `add_modifier/trigger(...)` - Add modifiers/triggers
- `get_available_keys(filter)` - List bindable keys

### AssetDiscoveryService (20 methods)

- `search_assets(term, type)` - Find assets
- `save_asset(path)` / `save_all_assets()` - Save
- `import_texture(file, dest)` - Import texture
- `export_texture(asset, file)` - Export texture
- `get_asset_dependencies/referencers(path)` - References

### DataAssetService (11 methods)

- `search_types(filter)` - Find DataAsset subclasses
- `create_data_asset(class, path, name)` - Create instance
- `get/set_property(path, name, value)` - Property access
- `set_properties(path, json)` - Bulk set properties

### DataTableService (15 methods)

- `search_row_types(filter)` - Find row struct types
- `create_data_table(struct, path, name)` - Create table
- `add_row/add_rows(path, name, json)` - Add rows
- `get_row/update_row/remove_row(...)` - Row operations
- `get_row_struct(path)` - Get column schema

### ActorService (33 methods)

ActorService provides comprehensive level actor manipulation:
- Actor discovery and queries
- Transform operations (position, rotation, scale)
- Transform locking and constraints
- Selection management
- Spawning and destruction
- Property access
- Viewport camera control

**Camera Control:**
- `set_viewport_camera(location, rotation)` — Position the editor viewport camera directly
- `get_actor_view_camera(name, direction, padding)` — Calculate and apply a camera view that frames an actor from a direction (Top, Bottom, Left, Right, Front, Back)
- `calculate_actor_view(name, direction, padding)` — Calculate view info without moving the camera

**Transform Lock / Constraints:**
- `set_actor_lock_location(name, locked)` — Lock/unlock an actor's location in the editor
- `get_actor_lock_location(name)` — Query whether an actor's location is locked
- `set_absolute_transform(name, location, rotation, scale)` — Set absolute (world-space) flags for location, rotation, scale
- `get_absolute_transform(name)` — Query current absolute transform flags
- `set_preserve_scale_ratio(enabled)` — Toggle the global "Preserve Scale Ratio" editor preference (padlock icon)
- `get_preserve_scale_ratio()` — Query whether the scale ratio padlock is enabled

### SkeletonService (53 methods)

SkeletonService provides comprehensive skeleton and skeletal mesh manipulation:

**Discovery:**
- `search_skeletons(path, filter)` - Find skeleton assets
- `search_skeletal_meshes(path, filter)` - Find skeletal mesh assets
- `get_skeleton_info(path)` - Get skeleton metadata
- `get_mesh_skeleton(path)` - Get skeleton used by a mesh
- `get_meshes_using_skeleton(path)` - Find all meshes using a skeleton

**Bone Operations:**
- `list_bones(path)` - List all bones with hierarchy
- `get_bone_info(path, name)` - Get detailed bone info
- `get_bone_hierarchy(path, name, depth)` - Get bone subtree
- `get_bone_children(path, name)` - Get direct children
- `get_bone_transform(path, name, space)` - Get transform (local/global)
- `find_bone_by_index(path, index)` - Find bone by index
- `bone_exists(path, name)` - Check if bone exists
- `get_bone_path_to_root(path, name)` - Get chain from bone to root

**Bone Modification (requires commit):**
- `add_bone(path, name, parent, transform)` - Add new bone
- `remove_bone(path, name, remove_children)` - Remove bone
- `rename_bone(path, old_name, new_name)` - Rename bone
- `mirror_bone(path, name, axis, prefix)` - Mirror bone
- `reparent_bone(path, name, new_parent)` - Change parent
- `set_bone_transform(path, name, transform, space)` - Set transform
- `copy_bone_chain(path, source, new_parent, prefix)` - Duplicate chain
- `commit_bone_changes(path)` - **CRITICAL: Apply all changes**

**Sockets:**
- `list_sockets(path)` - List all sockets
- `get_socket_info(path, name)` - Get socket details
- `add_socket(path, name, bone, location, rotation, scale, to_skeleton)` - Add socket
- `remove_socket(path, name)` - Remove socket
- `socket_exists(path, name)` - Check if socket exists
- `update_socket(path, name, ...)` - Modify socket properties
- `copy_socket(path, source, new_name, new_bone)` - Duplicate socket

**Retargeting:**
- `get_bone_retargeting_mode(path, name)` - Get retarget mode
- `set_bone_retargeting_mode(path, name, mode)` - Set retarget mode
- `get_all_retargeting_modes(path)` - Get all bones' modes
- `set_batch_retargeting_mode(path, bones, mode)` - Set multiple bones

**Curve Metadata:**
- `list_curves(path)` - List curve metadata
- `add_curve(path, name, morph, material)` - Add curve
- `remove_curve(path, name)` - Remove curve
- `get_curve_info(path, name)` - Get curve details
- `set_curve_flags(path, name, morph, material)` - Set curve flags
- `curve_exists(path, name)` - Check if curve exists

**Blend Profiles:**
- `list_blend_profiles(path)` - List profiles
- `get_blend_profile(path, name)` - Get profile data
- `create_blend_profile(path, name)` - Create profile
- `set_blend_profile_scale(path, profile, bone, scale, children)` - Set blend scale

**Properties & Editor:**
- `get_preview_mesh(path)` - Get preview mesh
- `set_preview_mesh(path, mesh)` - Set preview mesh
- `get_physics_asset(mesh)` - Get associated physics asset
- `open_in_editor(path)` - Open in Skeleton Tree editor
- `open_mesh_in_editor(path)` - Open skeletal mesh editor
- `refresh_skeleton(path)` - Refresh after changes

### LandscapeService (68 methods)

LandscapeService provides comprehensive landscape terrain manipulation including sculpting, weight layer painting, heightmap import/export, visibility holes, and spline-based road/path creation:

**Discovery:**
- `list_landscapes()` - Find all landscape actors in the current level
- `get_landscape_info(name)` - Get landscape metadata (size, components, layers)
- `landscape_exists(name)` / `layer_exists(name, layer)` - Existence checks

**Lifecycle:**
- `create_landscape(name, ...)` - Create a new landscape actor
- `delete_landscape(name)` - Remove a landscape actor

**Heightmap Operations:**
- `import_heightmap(name, file)` - Import heightmap from PNG/R16 file
- `export_heightmap(name, file)` - Export heightmap to file
- `get_height_at_location(name, x, y)` - Sample height at world position
- `get_height_in_region(name, ...)` - Get heights across a region
- `set_height_in_region(name, ...)` - Set heights across a region

**Sculpting:**
- `sculpt_at_location(name, x, y, radius, strength)` - Raise/lower terrain
- `flatten_at_location(name, x, y, radius, height)` - Flatten to target height
- `smooth_at_location(name, x, y, radius, strength)` - Smooth terrain
- `raise_lower_region(name, ...)` - Raise or lower a rectangular region
- `apply_noise(name, ...)` - Apply procedural noise

**Paint Layer Operations:**
- `list_layers(name)` - List all landscape layers
- `add_layer(name, layer, ...)` - Add a paint layer
- `remove_layer(name, layer)` - Remove a paint layer
- `get_layer_weights_at_location(name, x, y)` - Sample layer weights
- `paint_layer_at_location(name, layer, x, y, radius, strength)` - Paint a layer
- `paint_layer_in_region(name, layer, ...)` - Batch paint a region
- `paint_layer_in_world_rect(name, layer, ...)` - Paint in world-space rect

**Weight Map Import/Export:**
- `export_weight_map(name, layer, file)` - Export layer weight map
- `import_weight_map(name, layer, file)` - Import layer weight map
- `get_weights_in_region(name, ...)` - Read weights across a region
- `set_weights_in_region(name, ...)` - Write weights across a region

**Visibility Holes:**
- `get_hole_at_location(name, x, y)` - Check if hole exists at location
- `set_hole_at_location(name, x, y, hole)` - Create/remove a hole
- `set_hole_in_region(name, ...)` - Set holes across a region

**Splines:**
- `create_spline_point(name, x, y, z)` - Create a landscape spline point
- `connect_spline_points(name, p1, p2)` - Connect two spline points
- `create_spline_from_points(name, points)` - Create a spline from a list of positions
- `get_spline_info(name)` - Get all spline points and segments
- `modify_spline_point(name, index, ...)` - Move or adjust a spline point
- `delete_spline_point(name, index)` - Remove a spline point
- `delete_all_splines(name)` - Clear all splines on the landscape
- `apply_splines_to_landscape(name)` - Bake splines into the terrain shape
- `set_spline_segment_meshes(name, segment, mesh)` - Assign mesh to spline segment
- `set_spline_point_mesh(name, index, mesh)` - Assign mesh at spline point

**Properties:**
- `get_landscape_property(name, prop)` / `set_landscape_property(name, prop, val)` - Property access
- `set_landscape_material(name, material)` - Assign material to landscape
- `set_landscape_visibility(name, visible)` / `set_landscape_collision(name, enabled)` - Visibility and collision

**Resize:**
- `resize_landscape(name, ...)` - Change landscape dimensions

### LandscapeMaterialService (22 methods)

LandscapeMaterialService handles the creation and configuration of landscape-specific materials including layer blend nodes, layer info objects, and grass output:

**Material Creation:**
- `create_landscape_material(name, path)` - Create a landscape-compatible material

**Layer Blend Nodes:**
- `create_layer_blend_node(material, x, y)` - Create a LandscapeLayerBlend node
- `create_layer_blend_node_with_layers(material, layers, x, y)` - Create node with predefined layers
- `add_layer_to_blend_node(material, node_id, layer_name, blend_type)` - Add layer to blend node
- `remove_layer_from_blend_node(material, node_id, layer_name)` - Remove layer from blend node
- `get_layer_blend_info(material, node_id)` - Get blend node configuration
- `connect_to_layer_input(material, node_id, layer, texture)` - Connect texture to layer input

**Landscape-Specific Expressions:**
- `create_layer_coords_node(material, x, y)` - Create LandscapeLayerCoords UV node
- `create_layer_sample_node(material, layer, x, y)` - Create LandscapeLayerSample node
- `create_layer_weight_node(material, layer, x, y)` - Create LandscapeLayerWeight expression
- `create_grass_output(material, grass_types, x, y)` - Create LandscapeGrassOutput node

**Layer Info Objects:**
- `create_layer_info_object(name, path, layer_name)` - Create a LandscapeLayerInfoObject asset
- `get_layer_info_details(path)` - Get layer info properties (phys material, hardness, etc.)

**Material Assignment:**
- `assign_material_to_landscape(landscape, material)` - Assign material to a landscape actor

**Convenience:**
- `setup_layer_textures(material, layers)` - One-call setup for multi-layer textured landscape material

**Existence Checks:**
- `landscape_material_exists(path)` / `layer_info_exists(path)` - Existence checks

### FoliageService (15 methods)

FoliageService provides foliage type management, instance scattering, layer-aware placement, and instance queries:

**Discovery:**
- `list_foliage_types()` - List all foliage types in the current level
- `get_instance_count(foliage_type)` - Get total instance count for a foliage type

**Foliage Type Management:**
- `create_foliage_type(mesh, name, path)` - Create a new FoliageType asset from a static mesh
- `get_foliage_type_property(type, property)` - Get a foliage type property value
- `set_foliage_type_property(type, property, value)` - Set a foliage type property value

**Scatter Placement:**
- `scatter_foliage(type, center, radius, count)` - Scatter instances in a circular area
- `scatter_foliage_rect(type, min, max, density)` - Scatter in a rectangular region
- `add_foliage_instances(type, transforms)` - Add instances at specific transforms
- `scatter_foliage_on_layer(type, landscape, layer, ...)` - Place foliage weighted by a landscape paint layer

**Removal:**
- `remove_foliage_in_radius(type, center, radius)` - Remove instances within a radius
- `remove_all_foliage_of_type(type)` - Remove all instances of a foliage type
- `clear_all_foliage()` - Remove all foliage instances from the level

**Query:**
- `get_foliage_in_radius(type, center, radius)` - Get instance locations within a radius

**Existence Checks:**
- `foliage_type_exists(type)` / `has_foliage_instances(type)` - Existence checks

### NiagaraService (37 methods)

**Lifecycle:**
- `create_system(name, path, template)` - Create new Niagara system
- `compile_system(path)` / `compile_with_results(path)` - Compile with error messages
- `save_system(path)` - Save to disk
- `open_in_editor(path)` - Open in Niagara Editor

**Information & Discovery:**
- `get_system_info(path)` - Comprehensive system information
- `get_system_properties(path)` - Effect type, determinism, warmup, bounds
- `summarize(path)` - AI-friendly system summary
- `list_emitters(path)` - List all emitters
- `get_all_editable_settings(path)` - **Discover ALL editable settings** (user params, system scripts, emitter scripts)

**Emitter Management:**
- `add_emitter(system, emitter_asset, name)` - Add emitter to system
- `copy_emitter(src_system, src_emitter, target_system, new_name)` - Copy between systems
- `duplicate_emitter(system, source, new_name)` - Duplicate within system
- `remove_emitter(system, emitter)` - Remove emitter
- `rename_emitter(system, current, new)` - Rename emitter
- `enable_emitter(system, emitter, enabled)` - Enable/disable
- `move_emitter(system, emitter, index)` - Reorder emitter
- `get_emitter_graph_position(system, emitter)` - Get emitter position in graph
- `set_emitter_graph_position(system, emitter, x, y)` - Set emitter position
- `get_emitter_lifecycle(system, emitter)` - Get loop behavior and lifecycle

**Parameter Management:**
- `list_parameters(path)` - List user-exposed parameters
- `get_parameter(path, name)` - Get parameter value (searches User → System scripts → Emitter scripts)
- `set_parameter(path, name, value)` - Set parameter value (searches User → System scripts → Emitter scripts)
- `add/remove_user_parameter(...)` - Add/remove user parameters
- `list_rapid_iteration_params(system, emitter)` - List emitter module parameters
- `set_rapid_iteration_param(system, emitter, name, value)` - Set module values (color, spawn rate, etc.)
- `set_rapid_iteration_param_by_stage(...)` - Set parameter in specific script stage

**Search & Utilities:**
- `search_systems(path, filter)` - Find Niagara systems
- `search_emitter_assets(path, filter)` - Find emitter assets
- `list_emitter_templates(path, filter)` - List available emitter templates
- `system_exists(path)` / `emitter_exists(system, emitter)` / `parameter_exists(...)` - Existence checks
- `compare_systems(source, target)` - Compare two systems
- `copy_system_properties(target, source)` - Copy system settings
- `debug_activation(path)` - Debug why system isn't playing

### NiagaraEmitterService (23 methods)

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

### ScreenshotService (5 methods)

ScreenshotService enables AI vision by capturing editor content:
- `capture_editor_window(path)` - Capture entire editor window (works for blueprints, materials, etc.)
- `capture_viewport(path, width, height)` - Capture level viewport
- `capture_active_window(path)` - Capture foreground window
- `get_open_editor_tabs()` - List open editor tabs with asset info
- `get_active_window_title()` - Get focused window title
- `is_editor_window_active()` - Check if editor is in focus

### ViewportService (19 methods)

ViewportService provides direct control of the active Unreal Editor level viewport:
- `get_viewport_info()` - Get camera transform, viewport type, FOV, clip planes, exposure, layout, realtime/game view flags, and camera speed
- `set/get_viewport_type(type)` - Switch between `perspective`, `top`, `bottom`, `left`, `right`, `front`, and `back`
- `set/get_view_mode(mode)` - Control rendering mode: `lit`, `unlit`, `wireframe`, `detaillighting`, `lightingonly`, `lightcomplexity`, `shadercomplexity`, `pathtracing`, `clay`
- `set/get_fov(degrees)` - Read and set perspective FOV
- `set_near_clip_plane(distance)` / `set_far_clip_plane(distance)` - Adjust clipping planes for close-up or large-scene work
- `set_exposure(fixed, ev100)` / `set_exposure_game_settings()` - Toggle fixed exposure or return to game settings / auto exposure
- `set_game_view(enable)` - Toggle editor icon and gizmo visibility
- `set_allow_cinematic_control(enable)` - Allow Sequencer to take over the viewport camera
- `set_realtime(enable)` - Toggle realtime rendering
- `set_camera_location(vector)` / `set_camera_rotation(rotator)` / `set_camera_speed(speed)` - Position and tune the editor camera
- `set/get_viewport_layout(name)` - Switch between `OnePane`, `TwoPanesHoriz`, `TwoPanesVert`, `ThreePanesLeft`, `ThreePanesRight`, `ThreePanesTop`, `ThreePanesBottom`, `FourPanesLeft`, `FourPanesRight`, `FourPanesTop`, `FourPanesBottom`, and `FourPanes2x2` / `Quad`

```python
import unreal

# Inspect the active viewport
info = unreal.ViewportService.get_viewport_info()
print(info.viewport_type, info.fov, info.layout)

# Switch to quad view, then put the focused pane in top view
unreal.ViewportService.set_viewport_layout("Quad")
unreal.ViewportService.set_viewport_type("top")

# Return to perspective with fixed exposure for look-dev
unreal.ViewportService.set_viewport_type("perspective")
unreal.ViewportService.set_fov(60.0)
unreal.ViewportService.set_exposure(True, 1.0)
unreal.ViewportService.set_view_mode("lit")
```

### RuntimeVirtualTextureService (4 methods)

RuntimeVirtualTextureService manages RVT assets and landscape integration:
- `create_runtime_virtual_texture(name, path, ...)` - Create a Runtime Virtual Texture asset
- `get_runtime_virtual_texture_info(path)` - Get RVT asset metadata
- `create_rvt_volume(landscape, rvt_path, ...)` - Create an RVT volume actor sized to a landscape
- `assign_rvt_to_landscape(landscape, rvt_path)` - Assign an RVT asset to a landscape actor

### ProjectSettingsService (16 methods)

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

### EngineSettingsService (23 methods)

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

### SoundCueService (38 methods)

SoundCueService provides full SoundCue graph authoring — create assets, add audio nodes, wire connections, control volume/pitch/attenuation, and import SoundWave files:

**Asset Lifecycle:**
- `create_sound_cue(path, wave_path)` - Create a new SoundCue (empty or with initial WavePlayer)
- `duplicate_sound_cue(src_path, dst_path)` - Copy a cue to a new path
- `delete_sound_cue(path)` - Delete the asset from disk
- `get_sound_cue_info(path)` - Get node count, root index, volume, pitch, duration
- `save_sound_cue(path)` - Save to disk

**Node Creation (14 node types):**
- `add_wave_player_node(path, wave_path, x, y)` - Leaf node that plays a SoundWave
- `add_random_node(path, x, y)` - Randomly selects one child
- `add_mixer_node(path, num_inputs, x, y)` - Blends children in parallel
- `add_concatenator_node(path, num_inputs, x, y)` - Plays children in sequence
- `add_modulator_node(path, x, y)` - Random pitch/volume variance
- `add_attenuation_node(path, x, y)` - Spatial attenuation
- `add_looping_node(path, x, y)` - Loops its child
- `add_delay_node(path, x, y)` - Delay before playing
- `add_switch_node(path, x, y)` - Routes by integer parameter
- `add_enveloper_node(path, x, y)` - Volume/pitch envelope over time
- `add_distance_cross_fade_node(path, num_inputs, x, y)` - Crossfades by distance
- `add_branch_node(path, x, y, bool_param)` - Routes by bool parameter
- `add_param_cross_fade_node(path, num_inputs, x, y)` - Crossfades by float param
- `add_quality_level_node(path, x, y)` - One input per quality level

**Node Management:**
- `list_nodes(path)` - List all nodes with index, class, children, root status
- `connect_nodes(path, parent, child, slot)` - Wire child audio into parent
- `disconnect_node(path, parent_index, slot)` - Break a single input link
- `set_root_node(path, index)` - Set cue output node
- `set_wave_player_asset(path, index, wave_path)` - Reassign wave on existing node
- `remove_node(path, index)` - Delete a node from the graph
- `move_node(path, index, x, y)` - Reposition a node
- `get_node_property(path, index, prop_name)` - Read any node UPROPERTY
- `set_node_property(path, index, prop_name, value)` - Write any node UPROPERTY

**Cue-Level Settings:**
- `set_volume_multiplier(path, vol)` - Set volume
- `set_pitch_multiplier(path, pitch)` - Set pitch
- `set_sound_class(path, class_path)` - Set sound class
- `set_attenuation(path, att_path)` - Set attenuation (empty to clear)
- `get_attenuation(path)` - Get current attenuation asset path
- `set_concurrency(path, conc_path, clear)` - Set concurrency settings
- `get_concurrency(path)` - List assigned concurrency assets

**SoundWave Utilities:**
- `get_sound_wave_info(sw_path)` - Get duration, sample rate, channels, looping, streaming
- `import_sound_wave(file_path, asset_path)` - Import .wav/.mp3 from disk
- `set_sound_wave_property(sw_path, prop, value)` - Set any SoundWave UPROPERTY

### MetaSoundService (17 methods)

MetaSoundService provides programmatic MetaSound Source asset creation and editing — add DSP nodes, wire audio pins, manage graph I/O for runtime-parameterisable procedural audio:

**Lifecycle:**
- `create_meta_sound(package_path, asset_name, output_format)` - Create a new MetaSound Source asset
- `delete_meta_sound(asset_path)` - Delete a MetaSound asset
- `get_meta_sound_info(asset_path)` - Get node count, output format, graph I/O names
- `save_meta_sound(asset_path)` - Save after edits

**Node Discovery:**
- `list_available_nodes(search_filter)` - List all registered DSP node classes with pin info

**Node Management:**
- `add_node(asset_path, namespace, name, variant, major_version, pos_x, pos_y)` - Add a node by class
- `remove_node(asset_path, node_id)` - Remove node and all its edges
- `list_nodes(asset_path)` - List all nodes in the graph
- `get_node_pins(asset_path, node_id)` - Get pin info for a single node

**Connections:**
- `connect_nodes(asset_path, from_node_id, output_name, to_node_id, input_name)` - Connect output pin to input pin
- `disconnect_pin(asset_path, node_id, input_name)` - Remove connection to an input pin

**Graph I/O:**
- `add_graph_input(asset_path, input_name, data_type, default_value)` - Add a runtime parameter input
- `add_graph_output(asset_path, output_name, data_type)` - Add a named output
- `remove_graph_input(asset_path, input_name)` - Remove a graph input
- `remove_graph_output(asset_path, output_name)` - Remove a graph output

**Node Configuration:**
- `set_node_input_default(asset_path, node_id, input_name, value, data_type)` - Set literal default on a node input
- `set_node_location(asset_path, node_id, pos_x, pos_y)` - Update editor position

### StateTreeService (77 methods)

StateTreeService provides full programmatic control over StateTree assets: creation, state hierarchy, state type and linked-state configuration, non-destructive state reparenting, tasks, evaluators, conditions, transitions, parameters, theme colors, component overrides, property bindings, and compilation.

**Discovery & Info:**
- `list_state_trees(directory_path)` - List all StateTree assets under a content path
- `get_state_tree_info(asset_path)` - Get full structural info (states, tasks, transitions, parameters, compile status)
- `get_available_task_types()` - List all registered task struct names
- `get_available_evaluator_types()` - List all registered evaluator struct names
- `get_available_condition_types()` - List all registered condition struct names

**Asset Creation:**
- `create_state_tree(asset_path, schema_class_name)` - Create a new StateTree asset (schema: `StateTreeComponentSchema`, `StateTreeAIComponentSchema`, etc.)

**State Management:**
- `add_state(asset_path, parent_path, state_name, state_type)` - Add a state (`"State"`, `"Group"`, `"Subtree"`, `"Linked"`, `"LinkedAsset"`)
- `move_state(asset_path, state_path, new_parent_path, new_index)` - Reparent an existing state in-place without losing its data
- `remove_state(asset_path, state_path)` - Remove a state and all its children
- `rename_state(asset_path, state_path, new_name)` - Rename a state
- `set_state_enabled(asset_path, state_path, enabled)` - Enable or disable a state
- `set_state_type(asset_path, state_path, state_type)` - Change a state's type in place (`"State"`, `"Group"`, `"Subtree"`, `"Linked"`, `"LinkedAsset"`)
- `set_linked_subtree(asset_path, state_path, subtree_name)` - Configure which local subtree a `Linked` state references
- `set_linked_asset(asset_path, state_path, linked_asset_path)` - Configure which external StateTree asset a `LinkedAsset` state references
- `set_state_description(asset_path, state_path, description)` - Set editor description
- `set_state_tag(asset_path, state_path, gameplay_tag)` - Set gameplay tag (pass empty to clear)
- `set_state_weight(asset_path, state_path, weight)` - Set utility weight for utility-based selection
- `set_state_expanded(asset_path, state_path, expanded)` - Expand or collapse in editor tree view
- `select_state(asset_path, state_path)` - Highlight a state in the open StateTree editor panel
- `set_context_actor_class(asset_path, actor_class_path)` - Set ContextActorClass on the schema

Use `move_state` when the intent is to move or reparent an existing StateTree state. Do not emulate a move with `remove_state` plus `add_state`, because that recreates the state instead of preserving its existing identity and attached data.

Use `set_state_type`, `set_linked_subtree`, and `set_linked_asset` for in-place StateTree type/link edits. Do not emulate those operations by deleting and recreating states.

**State Properties:**
- `set_selection_behavior(asset_path, state_path, behavior)` - How children are selected (`TrySelectChildrenInOrder`, `TrySelectChildrenAtRandom`, etc.)
- `set_tasks_completion(asset_path, state_path, completion)` - Task completion mode: `"Any"` or `"All"`

**Theme Colors:**
- `get_theme_colors(asset_path)` - List all named theme colors and which states use them
- `set_state_theme_color(asset_path, state_path, color_name, color)` - Assign a named color to a state
- `rename_theme_color(asset_path, old_color_name, new_color_name)` - Rename a color entry

**Parameters:**
- `get_root_parameters(asset_path)` - Get all root parameters (name, type, default value)
- `add_or_update_root_parameter(asset_path, name, type, default_value)` - Add/update a parameter (`"Bool"`, `"Int32"`, `"Float"`, `"Double"`, `"String"`, etc.)
- `add_or_update_root_float_parameter(asset_path, parameter_name, default_value)` - Convenience: add/update a float parameter
- `remove_root_parameter(asset_path, name)` - Remove a parameter by name
- `rename_root_parameter(asset_path, old_name, new_name)` - Rename a parameter

**Level Actor Component Overrides:**
- `get_component_state_tree_path(actor_name_or_label)` - Get the StateTree asset used by a placed actor's StateTreeComponent
- `get_component_parameter_overrides(actor_name_or_label)` - Inspect current per-instance parameter override values on a placed actor
- `set_component_parameter_override(actor_name_or_label, parameter_name, value)` - Set a per-instance parameter override on a placed actor's StateTreeComponent

**Tasks:**
- `add_task(asset_path, state_path, task_struct_name)` - Add a task by C++ struct name (e.g. `"FStateTreeDelayTask"`)
- `remove_task(asset_path, state_path, task_struct_name, task_match_index)` - Remove a task
- `move_task(asset_path, state_path, task_struct_name, task_match_index, new_index)` - Reorder a task
- `set_task_enabled(asset_path, state_path, task_struct_name, task_match_index, enabled)` - Enable/disable a task
- `set_task_considered_for_completion(asset_path, state_path, task_struct_name, task_match_index, considered)` - Whether task counts toward state completion
- `get_task_property_names(asset_path, state_path, task_struct_name, task_match_index)` - Discover all editable task properties
- `get_task_property_value(asset_path, state_path, task_struct_name, property_path, task_match_index)` - Read a task property value
- `set_task_property_value(asset_path, state_path, task_struct_name, property_path, value, task_match_index)` - Set a task property value
- `set_task_property_value_detailed(...)` - Set task property with structured result (failure reason + readback value)
- `bind_task_property_to_root_parameter(asset_path, state_path, task_struct_name, task_property_path, parameter_path, task_match_index)` - Bind task property to a root parameter
- `bind_task_property_to_context(asset_path, state_path, task_struct_name, task_property_path, context_name, context_property_path, task_match_index)` - Bind task property to context data
- `unbind_task_property(asset_path, state_path, task_struct_name, task_property_path, task_match_index)` - Remove a task property binding

**Evaluators & Global Tasks:**
- `add_evaluator(asset_path, evaluator_struct_name)` - Add a global evaluator
- `remove_evaluator(asset_path, evaluator_struct_name, match_index)` - Remove an evaluator
- `get_evaluator_property_names(asset_path, evaluator_struct_name, match_index)` - Get evaluator properties
- `set_evaluator_property_value(asset_path, evaluator_struct_name, property_path, value, match_index)` - Set evaluator property
- `bind_evaluator_property_to_root_parameter(asset_path, evaluator_struct_name, evaluator_property_path, parameter_path, match_index)` - Bind evaluator property to a root parameter
- `bind_evaluator_property_to_context(asset_path, evaluator_struct_name, evaluator_property_path, context_name, context_property_path, match_index)` - Bind evaluator property to context data
- `unbind_evaluator_property(asset_path, evaluator_struct_name, evaluator_property_path, match_index)` - Remove an evaluator property binding
- `add_global_task(asset_path, task_struct_name)` - Add a global task
- `remove_global_task(asset_path, task_struct_name, match_index)` - Remove a global task
- `get_global_task_property_names(asset_path, task_struct_name, match_index)` - Get global task properties
- `set_global_task_property_value(asset_path, task_struct_name, property_path, value, match_index)` - Set global task property

**Enter Conditions:**
- `add_enter_condition(asset_path, state_path, condition_struct_name)` - Add an enter condition to a state
- `remove_enter_condition(asset_path, state_path, condition_index)` - Remove an enter condition by index
- `set_enter_condition_operand(asset_path, state_path, condition_index, operand)` - Set `"And"` / `"Or"` / `"Copy"` operand
- `get_enter_condition_property_names(asset_path, state_path, condition_struct_name, condition_match_index)` - Discover condition properties
- `set_enter_condition_property_value(asset_path, state_path, condition_struct_name, property_path, value, condition_match_index)` - Set a condition property
- `bind_enter_condition_property_to_context(asset_path, state_path, condition_struct_name, condition_property_path, context_name, context_property_path, condition_match_index)` - Bind an enter condition property to context data
- `bind_enter_condition_property_to_root_parameter(asset_path, state_path, condition_struct_name, condition_property_path, parameter_path, condition_match_index)` - Bind an enter condition property to a root parameter
- `unbind_enter_condition_property(asset_path, state_path, condition_struct_name, condition_property_path, condition_match_index)` - Remove an enter condition property binding

**Transitions:**
- `add_transition(asset_path, state_path, trigger, transition_type, target_path, priority)` - Add a transition (`"OnStateCompleted"`, `"GotoState"`, etc.)
- `update_transition(asset_path, state_path, transition_index, ...)` - Modify trigger, type, target, priority, enabled, or delay settings
- `bind_transition_to_delegate(asset_path, state_path, transition_index, task_struct_name, dispatcher_property_name, task_match_index)` - Bind an `OnDelegate` transition to a task dispatcher property
- `remove_transition(asset_path, state_path, transition_index)` - Remove a transition by index
- `move_transition(asset_path, state_path, from_index, to_index)` - Reorder a transition

**Transition Conditions:**
- `add_transition_condition(asset_path, state_path, transition_index, condition_struct_name)` - Add a condition to a transition
- `remove_transition_condition(asset_path, state_path, transition_index, condition_index)` - Remove a transition condition
- `set_transition_condition_operand(asset_path, state_path, transition_index, condition_index, operand)` - Set operand
- `get_transition_condition_property_names(asset_path, state_path, transition_index, condition_struct_name, condition_match_index)` - Discover properties
- `get_transition_event_payload_property_names(asset_path, state_path, transition_index)` - Discover bindable properties from an `OnEvent` transition's payload struct
- `set_transition_condition_property_value(asset_path, state_path, transition_index, condition_struct_name, property_path, value, condition_match_index)` - Set property
- `bind_transition_condition_property_to_context(asset_path, state_path, transition_index, condition_struct_name, condition_property_path, context_name, context_property_path, condition_match_index)` - Bind a transition condition property to context data
- `bind_transition_condition_property_to_event_payload(asset_path, state_path, transition_index, condition_struct_name, condition_property_path, payload_property_path, condition_match_index)` - Bind a transition condition property to an event payload property
- `unbind_transition_condition_property(asset_path, state_path, transition_index, condition_struct_name, condition_property_path, condition_match_index)` - Remove a transition condition property binding

**Compile & Save:**
- `compile_state_tree(asset_path)` - Compile the asset; returns success flag, errors, and warnings
- `save_state_tree(asset_path)` - Save to disk

### EditorTransactionService (16 methods)

EditorTransactionService provides programmatic undo/redo and transaction management:

**Undo / Redo:**
- `undo()` — Undo the last transaction
- `redo()` — Redo the last undone transaction
- `undo_multiple(count)` — Undo multiple transactions at once
- `redo_multiple(count)` — Redo multiple transactions at once

**Transaction Grouping:**
- `begin_transaction(description)` — Open a named transaction scope
- `end_transaction()` — Close the current transaction scope
- `cancel_transaction()` — Cancel (rollback) the current transaction

**History Inspection:**
- `can_undo()` — Check if undo is available
- `can_redo()` — Check if redo is available
- `get_undo_description()` — Get the description of the next undo action
- `get_redo_description()` — Get the description of the next redo action
- `get_undo_history(count)` — List recent undo history entries
- `get_redo_history(count)` — List recent redo history entries
- `get_undo_count()` — Get total number of undo entries
- `get_redo_count()` — Get total number of redo entries

**Buffer Reset:**
- `reset_history()` — Clear the entire undo/redo buffer

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

### Build Animation Montage with Multiple Animations

```python
# 1. Create empty montage from skeleton
path = unreal.AnimMontageService.create_empty_montage(
    "/Game/Characters/Mannequin/SK_Mannequin",
    "/Game/Montages",
    "AM_ComboAttack"
)

# 2. Add animation segments sequentially
unreal.AnimMontageService.add_anim_segment(path, 0, "/Game/Animations/Attack1", 0.0)
unreal.AnimMontageService.add_anim_segment(path, 0, "/Game/Animations/Attack2", 1.0)
unreal.AnimMontageService.add_anim_segment(path, 0, "/Game/Animations/Attack3", 2.0)

# 3. Add sections for each attack phase
unreal.AnimMontageService.add_section(path, "Attack1", 0.0)
unreal.AnimMontageService.add_section(path, "Attack2", 1.0)
unreal.AnimMontageService.add_section(path, "Attack3", 2.0)

# 4. Link sections for combo flow
unreal.AnimMontageService.set_next_section(path, "Attack1", "Attack2")
unreal.AnimMontageService.set_next_section(path, "Attack2", "Attack3")

# 5. Add branching points for combo input windows
unreal.AnimMontageService.add_branching_point(path, "ComboWindow1", 0.7)
unreal.AnimMontageService.add_branching_point(path, "ComboWindow2", 1.7)

# 6. Set blend settings
unreal.AnimMontageService.set_blend_in(path, 0.15, "Cubic")
unreal.AnimMontageService.set_blend_out(path, 0.2, "Linear")

# 7. Refresh editor to see changes
unreal.AnimMontageService.refresh_montage_editor(path)

# 8. Save
unreal.EditorAssetLibrary.save_asset(path)
```

### Build Niagara VFX System

```python
# 1. Create system
result = unreal.NiagaraService.create_system("NS_Fire", "/Game/VFX")
path = result.asset_path

# 2. Add emitters
unreal.NiagaraService.add_emitter(path, "minimal", "Flames")
unreal.NiagaraService.add_emitter(path, "minimal", "Smoke")

# 3. Discover ALL editable settings (recommended first step for existing systems)
settings = unreal.NiagaraService.get_all_editable_settings(path)
print(f"Total settings: {settings.total_settings_count}")

# User Parameters (top priority)
for p in settings.user_parameters:
    print(f"[User] {p.setting_path} ({p.value_type}): {p.current_value}")

# System & Emitter script settings
for p in settings.rapid_iteration_parameters:
    if "Color" in p.setting_path:
        print(f"[{p.emitter_name}] {p.setting_path}: {p.current_value}")

# 4. Set parameters (searches User → System scripts → Emitter scripts)
unreal.NiagaraService.set_parameter(path, "User.Color", "(R=1,G=0.5,B=0,A=1)")
unreal.NiagaraService.set_parameter(path, "System.Color", "(R=3,G=0.5,B=0,A=1)")

# 5. Set rapid iteration parameters (emitter-specific)
unreal.NiagaraService.set_rapid_iteration_param(
    path, "Flames", 
    "Constants.Flames.SpawnRate.SpawnRate", 
    "500.0"
)

# 6. Compile and save
unreal.NiagaraService.compile_system(path)
unreal.NiagaraService.save_system(path)
```

### Real-World Terrain to Landscape

```python
# 1. Geocode a place name to get GPS coordinates
# MCP: deep_research(action="geocode", query="Mount Fuji")
# → lat=35.3606, lng=138.7274

# 2. Preview elevation to get suggested settings
# MCP: terrain_data(action="preview_elevation", lng=138.7274, lat=35.3606)
# → suggested_base_level=340, suggested_height_scale=27

# 3. Generate heightmap matching your landscape resolution
# MCP: terrain_data(
#     action="generate_heightmap",
#     lng=138.7274, lat=35.3606,
#     base_level=340, height_scale=27,
#     resolution=1009, format="png"
# )
# → file="C:/Project/Saved/Terrain/heightmap_35.3606_138.7274.png"

# 4. Create landscape and import heightmap
path = unreal.LandscapeService.create_landscape("MtFuji",
    num_components_x=16, num_components_y=16,
    quads_per_section=63, sections_per_component=1)
unreal.LandscapeService.import_heightmap("MtFuji",
    "C:/Project/Saved/Terrain/heightmap_35.3606_138.7274.png")

# 5. Optionally get a satellite reference image
# MCP: terrain_data(action="get_map_image", lng=138.7274, lat=35.3606, style="satellite-v9")
```

---

## 🌐 External IDE Integration

Connect VS Code, Claude Code, Cursor, or AntiGravity to control Unreal via MCP.

### Enable MCP Server

In **Project Settings > Plugins > VibeUE**:

| Setting | Default | Description |
|---------|---------|-------------|
| **Enable MCP Server** | Enabled | Toggle HTTP server |
| **Port** | 8088 | MCP endpoint port |
| **Bearer Token** | (empty) | MCP clients must send this as `Authorization: Bearer <token>`. Leave empty to allow unauthenticated connections. |

Server runs at `http://127.0.0.1:8088/mcp`

> **⚠️ VibeUE API Key required:** The MCP server validates your **VibeUE API Key** (set in the AI Chat settings) against `vibeue.com` at startup. All MCP tool calls will fail with an error until a valid key is configured. Get your free key at [vibeue.com/login](https://www.vibeue.com/login).

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

### Claude Code

Run once in your terminal:

```bash
claude mcp add --scope user --transport stdio VibeUE-Claude -- npx -y mcp-remote http://127.0.0.1:8088/mcp --transport http-only --allow-http --header "Authorization:Bearer YOUR_API_KEY"
```

Omit the `--header` arguments if no API key is set in VibeUE.

### Cursor / AntiGravity

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

### Optional: Use the VibeUE Proxy (recommended for persistent setups)

The proxy is optional and runs on `http://127.0.0.1:8089/mcp`.

Use it when you want:

- Tool definitions available even while Unreal Editor is closed
- MCP clients without local `Authorization` header management
- AI agent tools (Claude Code, Cursor, Windsurf) that cannot set `Authorization` headers per-request

#### 1) Set proxy bearer token

Create `Plugins/VibeUE/vibeue-proxy.json`:

```json
{
  "bearer_token": "YOUR_API_KEY"
}
```

`YOUR_API_KEY` must match **Project Settings > Plugins > VibeUE > API Key**.

#### 2) Start the proxy

```powershell
# Windows (silent background process — interactive shells / Explorer)
Start-Process -FilePath 'pythonw.exe' -ArgumentList 'Plugins/VibeUE/Content/Python/vibeue-proxy.py' -WindowStyle Hidden
```

> **AI agent / headless shells (Claude Code, Cursor, Windsurf):** `pythonw` and `start /B` fail silently in headless contexts — errors are swallowed and the proxy never starts. Use `python` directly instead:
>
> ```bash
> # Windows — AI agent / headless shell
> python Plugins/VibeUE/Content/Python/vibeue-proxy.py &
> ```

```bash
# Mac / Linux
python3 Plugins/VibeUE/Content/Python/vibeue-proxy.py &
```

**The proxy survives session close.** Once started, it keeps running until the machine restarts or it is killed manually — no need to restart it each AI session.

Optional Windows auto-start: add a shortcut to `Plugins/VibeUE/Content/Python/start-vibeue-proxy.bat` in `shell:startup`. The bat kills any existing proxy instance before starting a fresh one, so it is safe to re-run if something goes wrong.

#### 3) Point your MCP client to proxy (no auth header needed)

VS Code `.vscode/mcp.json`:

```json
{
  "servers": {
    "VibeUE": {
      "type": "http",
      "url": "http://127.0.0.1:8089/mcp"
    }
  }
}
```

Claude Code:

```bash
claude mcp add --scope user --transport stdio VibeUE-Claude -- npx -y mcp-remote http://127.0.0.1:8089/mcp --transport http-only --allow-http
```

Cursor / AntiGravity:

```json
{
  "mcpServers": {
    "VibeUE": {
      "type": "http",
      "url": "http://127.0.0.1:8089/mcp"
    }
  }
}
```

The proxy injects the token from `vibeue-proxy.json` when forwarding requests to Unreal.

**Verify the proxy is running** before wiring up your MCP client:

```bash
# Windows
netstat -ano | findstr :8089

# Mac / Linux
lsof -i :8089
```

A `LISTENING` result on port 8089 confirms the proxy is up. If nothing appears, check that `vibeue-proxy.json` exists and Python is on your PATH.

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
├── Content/
│   ├── instructions/          # AI system prompts
│   └── samples/               # Ready-made AI assistant instruction templates
└── VibeUE.uplugin
```

---

## 🤝 Community

- **Discord**: https://discord.gg/hZs73ST59a
- **Documentation**: https://www.vibeue.com/docs

---

## 📄 License

MIT License — Copyright © 2025 Kevin Buckley / Buckley Builds LLC

This project is open source and freely available under the [MIT License](LICENSE). You may use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, subject to the license terms.

