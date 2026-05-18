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
