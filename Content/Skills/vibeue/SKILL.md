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
# List every available skill (full path → description)
call_tool(toolset_name="ToolsetRegistry.AgentSkillToolset", tool_name="ListSkills", arguments={})

# Read one or more skills — GetSkills takes the FULL paths that ListSkills returns
call_tool(toolset_name="ToolsetRegistry.AgentSkillToolset", tool_name="GetSkills",
          arguments={"skillPaths": ["/VibeUE/Python/init_unreal_PY.VibeUE_pcg",
                                    "/VibeUE/Python/init_unreal_PY.VibeUE_materials"]})
```

> **Naming rule (verified live):** skill pack `<slug>` registers as `VibeUE_<slug>` (hyphens →
> underscores) and sub-doc `<slug>/<file>.md` as `VibeUE_<slug>__<file>`, all under the path prefix
> `/VibeUE/Python/init_unreal_PY.`. Short names such as `"pcg"` or `"state-trees/api-reference"`
> return an **empty result with no error** — always pass full paths, taking them from `ListSkills`
> when unsure. Run `describe_toolset` on `ToolsetRegistry.AgentSkillToolset` if the call signature
> differs in your build. The old `vibeue-skills-manager` tool no longer exists.

**Route by functional area.** Find the area whose scope matches the task, then `GetSkills` the
listed skill(s) — full paths per the naming rule above. The *NOT for* column is the disambiguator —
when two areas seem to fit, the one that *excludes* your task is telling you where to go instead.

| Functional area | Use for — **NOT** for… | Load skill(s) |
|---|---|---|
| **Scene & actors** | place / move / arrange / organize / tag actors in a level — NOT gameplay logic (→ Blueprints), NOT world-scale terrain/foliage (→ Environment), NOT attaching a Niagara/particle component (→ VFX) | `level-actors` |
| **Blueprints & gameplay logic** | author Blueprint classes & graphs, Enhanced Input, gameplay tags — NOT AI behavior (→ AI), NOT AnimBP graphs (→ Animation), NOT C++/source (coding-agent handoff) | `blueprints`, `blueprint-graphs`, `enhanced-input`, `gameplay-tags` |
| **AI** | author StateTree logic — states, tasks, transitions, event payloads, delegate bindings — NOT character body animation (→ Animation), NOT generic actor placement (→ Scene) | `state-trees` |
| **Animation & rigging** | AnimBP state machines, AnimSequence keyframes, montages & AnimNotify wiring, bone/skeleton editing & retarget — NOT cinematic timelines (Epic Sequencer), NOT AI movement (→ AI), NOT authoring/adding sound assets — even a character's footstep sounds (→ Audio) | `animation-blueprint`, `animsequence`, `animation-editing`, `animation-montage`, `skeleton` |
| **Materials & shading** | materials, instances, graph nodes, Custom HLSL — NOT Niagara particle materials (→ VFX), NOT landscape auto-materials (→ Environment) | `materials` |
| **VFX (Niagara)** | particle systems, emitters, scratch-pad HLSL, attaching/placing a Niagara component on an actor — NOT surface materials (→ Materials) | `niagara-systems`, `niagara-emitters` |
| **UI (UMG)** | widget blueprints, layout, fonts/brushes, MVVM — NOT the gameplay behind the UI (→ Blueprints) | `umg-widgets` |
| **Environment (world-scale)** | landscape sculpt/paint, landscape materials, foliage, PCG, map blockout, real-world terrain — NOT single-actor placement (→ Scene), NOT sound/audio (→ Audio) | `landscape`, `landscape-materials`, `landscape-auto-material`, `foliage`, `pcg`, `map-blockout`, `terrain-data` |
| **Audio** | MetaSound and SoundCue authoring — creating the sound asset itself: ambient, a character's footstep/foley, UI sounds — NOT triggering sounds from gameplay logic (→ Blueprints), NOT wiring an existing sound to anim-notify foot-plant frames (→ Animation) | `metasounds`, `sound-cues` |
| **Assets, data & project** | import/export assets, UV mapping, enums/structs, engine & project settings — NOT actors placed in a level (→ Scene) | `asset-management`, `uv-mapping`, `enum-struct`, `engine-settings`, `project-settings` |
| **Diagnostics, testing & run** | start/stop/query PIE, profile (CPU-vs-GPU / Insights), uncap frame rate — NOT fixing the logic a bug points to (→ its authoring area) | `pie-testing`, `profiling`, `frame-rate` |
| **Camera & viewport** | viewport camera, view mode, FOV, exposure, layout — NOT material look (→ Materials), NOT placing/editing light actors (→ Scene) | `viewport` |
| **Cinematics · Physics** | Sequencer cinematics and Physics assets (ragdoll/skeletal) are **Epic-native** — VibeUE adds no skill here — NOT enabling simulate-physics on a level actor (→ Scene) | *(none — use `list_toolsets`: `animation_toolset.*` / `PhysicsToolsets`)* |

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
