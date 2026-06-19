---
name: vibeue
description: Unreal Engine 5 development using the VibeUE Python API. Use when working in Unreal Engine — blueprints, state trees, materials, actors, landscapes, animation, niagara, widgets, sound, foliage, gameplay tags, enhanced input, skeletons, PCG (procedural content generation), and more. VibeUE is an extension of Unreal's native MCP endpoint.
---

VibeUE is an **extension on Unreal Engine's native MCP endpoint** (`http://localhost:8000/mcp`).
There is no separate VibeUE server, no API key, and no in-editor chat — VibeUE simply registers
extra Python services (`unreal.<Service>`) and skill packs on top of the engine's own toolsets.

Skill packs (this file and its siblings) are loaded through the engine's `AgentSkillToolset`.
Each skill carries exact API patterns and gotchas; **load the relevant skill before writing any
code** in a domain, or you will guess wrong property names and spiral into discovery loops.

## Discover and load skills

Skills are discovered and read through the engine `AgentSkillToolset`, invoked with `call_tool`:

```
# List every available skill (name + description)
call_tool(toolset="ToolsetRegistry.AgentSkillToolset", tool="ListSkills")

# Read one or more skills (returns their SKILL.md content + sub-doc list)
call_tool(toolset="ToolsetRegistry.AgentSkillToolset", tool="GetSkills", args={"skills": ["pcg", "materials"]})
```

> Run `describe_toolset` on `ToolsetRegistry.AgentSkillToolset` for the exact tool/argument names
> if the call signature differs in your build. The old `vibeue-skills-manager` tool no longer exists.

| Task | Load this skill |
|---|---|
| Blueprints | `blueprints` |
| PCG / procedural generation / scatter | `pcg` |
| Materials | `materials` |
| State Trees | `state-trees` |
| Play / test / run / PIE | `pie-testing` |
| Profiling / FPS / Insights traces | `profiling` |

A loaded skill gives you:
- workflows, gotchas, and property formats for the domain
- `vibeue_classes` / `unreal_classes` — class names to feed into `discover_python_class` for live method signatures
- sub-doc references (`<skill>/<section>`) you can fetch via `GetSkills` for deeper detail

Always call `discover_python_class` on the classes in `vibeue_classes` before writing code — never
guess method names from the skill content alone. **Batch the discovery into ONE call** instead of
one call per class:

```
# ONE call covers all classes and all topics:
discover_python_class(
    class_name="unreal.MaterialService, unreal.WidgetService, unreal.MaterialNodeService",
    method_filter="create|delete|compile|property|color")

# WRONG — three separate calls for three classes wastes round-trips and repeats boilerplate
```

`class_name` accepts a comma-separated list (response gains a `classes` array, one entry per class);
`method_filter` ORs keywords with `|`.

## How work gets done — `execute_python_code` is the workhorse

VibeUE services are plain Python on the editor's `unreal` module. Run everything through
`execute_python_code`:

```python
import unreal
widgets = unreal.WidgetService.list_widget_blueprints()
unreal.StateTreeService.create_state_tree("/Game/AI/MyBehavior")
```

You get the full `unreal.*` API plus every `unreal.<Service>` VibeUE adds. Reserve `call_tool` for
**engine toolsets and skills** (e.g. `AgentSkillToolset`, `EditorToolset.EditorAppToolset`,
`LogsToolset`, `GameplayTagsToolset`, `AssetTools`).

## Tools — what each is for

| Tool | Use it for |
|------|-----------|
| `execute_python_code` | Run `unreal.*` Python in the editor — the workhorse for every VibeUE service |
| `call_tool` | Invoke engine toolsets and skills (skills, PIE control, logs, assets, gameplay tags) |
| `describe_toolset` / `list_toolsets` | Discover engine toolsets and their actions/args |
| `discover_python_class` / `discover_python_function` / `discover_python_module` | Get live signatures before writing code |
| `list_python_subsystems` | Enumerate editor subsystems for `unreal.get_editor_subsystem(...)` |
| `terrain_data` | Real-world heightmaps + water splines (see `terrain-data` skill) |
| `deep_research` | Web research / page fetch / geocoding |

## Engine toolsets replace the old VibeUE tools

Several capabilities that used to be VibeUE-specific MCP tools are now the engine's native toolsets,
called via `call_tool` (run `describe_toolset` for action names/params):

| Need | Engine toolset (via `call_tool`) |
|------|----------------------------------|
| Start / stop / query PIE | `EditorToolset.EditorAppToolset` → `StartPIE` / `StopPIE` / `IsPIERunning` |
| Capture a viewport screenshot | `EditorToolset.EditorAppToolset` → `CaptureViewport` |
| List / read / filter / tail UE logs | `LogsToolset` |
| Search / open / save / move / import assets | `AssetTools` |
| Single-tag gameplay-tag CRUD | `GameplayTagsToolset` (see `gameplay-tags` skill) |

Performance/Insights tracing is the one net-new VibeUE service — `unreal.PerformanceService.*` (see
the `profiling` skill) — because Unreal 5.8 ships no performance toolset.
