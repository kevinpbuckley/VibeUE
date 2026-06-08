---
name: vibeue
description: Unreal Engine 5 development using the VibeUE Python API. Use when working
  in Unreal Engine — blueprints, state trees, materials, actors, landscapes, animation,
  niagara, widgets, sound, foliage, data tables, gameplay tags, enhanced input, skeletons,
  PCG (procedural content generation), and more. Requires the VibeUE MCP server to be connected.
compatibility: Requires VibeUE MCP server
---

VibeUE exposes its own skills system via MCP. Use it instead of this file for all tasks.

## Discover available skills

```
manage_skills(action="list")
```

## Load a skill before working in a domain

Always load the relevant skill **before writing any code**. The skill contains exact API patterns and gotchas — without it you will guess wrong property names and spiral into discovery loops.

| Task | Load this skill |
|---|---|
| Blueprints | `blueprints` |
| PCG / procedural generation / scatter | `pcg` |
| Materials | `materials` |
| State Trees | `state-trees` |
| Play / test / run / PIE | `pie-testing` |

```
manage_skills(action="load", skill_name="pcg")
manage_skills(action="load", skill_name="blueprints")
manage_skills(action="load", skill_name="materials")
```

The loaded skill returns:
- `content` — workflows, gotchas, and property formats for the domain
- `vibeue_classes` / `unreal_classes` — class names to feed into `discover_python_class('unreal.<ClassName>', method_filter='<keyword>')` to get live method signatures
- `COMMON_MISTAKES` — extracted "common mistakes" section (when the skill has one)
- `available_sections` — sibling sub-docs you can load via `skill_name="<skill>/<section>"` for deeper reference material

Always call `discover_python_class` on the classes in `vibeue_classes` before writing code — never guess method names from the skill content alone.

## MCP tools — what each is for

These tools are invoked directly (no skill needed), but each domain skill assumes them:

| Tool | Use it for | Validated by |
|------|-----------|--------------|
| `execute_python_code` | Run `unreal.*` Python in the editor (the workhorse for every service) | every skill |
| `manage_skills` | `list` / `suggest` / `load` skills + sub-docs | this skill |
| `manage_asset` | Search/find/list/open/save/duplicate/move/delete/import assets (prefer over raw Python) | `asset-management`, `test_prompts/asset_management`, `test_prompts/assets` |
| `read_logs` | List/read/filter/tail UE logs (main/chat/llm) | `test_prompts/logs` |
| `discover_python_class` / `discover_python_function` / `discover_python_module` | Get live signatures before writing code | every skill |
| `list_python_subsystems` | Enumerate editor subsystems for `unreal.get_editor_subsystem(...)` | `level-actors` |
| `terrain_data` | Real-world heightmaps + water splines | `terrain-data`, `test_prompts/terrain-data` |
| `deep_research` | Web research / page fetch / geocoding | `test_prompts/deep-research` |

## Tool-level test prompts (no single domain skill — exercised directly)

- `test_prompts/logs` → `read_logs`
- `test_prompts/transactions` → editor undo/redo via `execute_python_code` (`unreal` transaction APIs)
- `test_prompts/utilities` → connectivity/help (`vibeue_status`)
- `test_prompts/deep-research` → `deep_research`
- `test_prompts/Smoke_Test.md`, `demo_prompts.md`, `markdown_rendering_test.md` → general smoke/UX checks

