# Copilot Instructions

<!-- TODO: Add your project description here -->
This is an Unreal Engine 5.7+ project with the **VibeUE** plugin for AI-powered development via MCP.

## Critical Workflow

**ALWAYS use MCP tools for Unreal Engine operations - NEVER try to read .uasset files from disk.**

## MCP Tools

### Discovery Tools (use before executing)
- `discover_python_module` - Inspect module contents (use `unreal` lowercase)
- `discover_python_class` - Get class methods and properties
- `discover_python_function` - Get function signatures
- `list_python_subsystems` - List available editor subsystems
- `execute_python_code` - Run Python in Unreal context

### Skills System
Load domain-specific knowledge before performing operations:

```python
# List available skills
manage_skills(action="list")

# Load skill before domain work
manage_skills(action="load", skill_name="blueprints")

# Load multiple skills
manage_skills(action="load", skill_names=["blueprints", "umg-widgets"])
```

**Available Skills:**
| Domain | Skill Name |
|--------|------------|
| Blueprints (BP_) | `blueprints` |
| UMG Widgets (WBP_) | `umg-widgets` |
| Enhanced Input (IA_, IMC_) | `enhanced-input` |
| Enums & Structs | `enum-struct` |
| Materials (M_, MI_) | `materials` |
| Level Actors | `level-actors` |
| Data Tables (DT_) | `data-tables` |
| Data Assets | `data-assets` |
| Asset Search/Import | `asset-management` |
| Niagara Systems (NS_) | `niagara-systems` |
| Niagara Emitters | `niagara-emitters` |
| Landscape Terrain | `landscape` |
| Landscape Materials | `landscape-materials` |
| Foliage & Vegetation | `foliage` |
| Skeleton & Skeletal Mesh | `skeleton` |
| Animation Sequences | `animsequence` |
| Animation Blueprints | `animation-blueprint` |
| Animation Montages | `animation-montage` |
| Animation Editing | `animation-editing` |
| Screenshots | `screenshots` |
| Project Settings | `project-settings` |
| Engine Settings | `engine-settings` |

### Skill Response Usage
- `vibeue_apis` - Use for method signatures (auto-discovered at runtime)
- `content` - Workflows and gotchas
- Never guess method names - if not in `vibeue_apis`, it doesn't exist

### Log Reading
```python
read_logs(action="help")  # See all log reading options
read_logs(action="errors", file="main")  # Find errors in project log
```
