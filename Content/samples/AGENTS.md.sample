# VibeUE — AI agent guide (Unreal Engine 5.8)

VibeUE is a plugin that **extends Unreal 5.8's native AI toolset system**. Its services, tools, and
skills register into the engine's own `ToolsetRegistry` + `ModelContextProtocol` and are served from
Unreal's MCP endpoint (default `http://localhost:8000/mcp`). There is **no separate VibeUE server,
no API key, and no in-editor chat** — you talk to the engine's endpoint.

**ALWAYS use the MCP tools / Python API for Unreal operations — NEVER read `.uasset` files from disk.**

---

## 1. The efficient interaction model (read this first)

There are two ways to act on the editor. Pick the cheap one:

- **`execute_python_code` — your workhorse.** Runs an arbitrary Python script in the editor in **one
  round-trip**. Every VibeUE service is exposed to Python (`unreal.BlueprintService.add_variable(...)`)
  and sits next to the whole native `unreal.*` API in the same script. **Batch aggressively** — do a
  whole multi-step task (create + edit + compile + verify) in a single call, and `print()` only what
  you need back.
- **`call_tool` — one tool per round-trip.** Use it only for things that are **not** Python-callable:
  loading **skills** (`AgentSkillToolset`), and the few Epic toolsets with no good Python path
  (screenshots, etc.). Don't use it to do work `execute_python_code` can batch.

**Rules of thumb (speed + tokens):**
1. Default to `execute_python_code`; one call should accomplish a logical step, not one operation.
2. `call_tool` only for skills and Epic-only toolsets (below).
3. **Avoid `describe_toolset` as a habit** — it dumps the full JSON schema of every tool in a toolset
   (the most token-heavy thing here). Reach for a **skill** + `discover_python_class` instead.
4. Discover signatures narrowly: `discover_python_class('unreal.BlueprintService', method_filter='variable')`.

---

## 2. Tool roster — what's where

**VibeUE MCP tools (call directly):**
- `execute_python_code` — run Python (must start with `import unreal`). The workhorse.
- `discover_python_module` / `discover_python_class` / `discover_python_function` — inspect the API
  (use `unreal` lowercase; narrow with `name_filter` / `method_filter`).
- `list_python_subsystems` — list editor subsystems.
- `deep_research` — web search / page fetch / geocode (see §5).
- `terrain_data` — real-world heightmaps + water splines (see §5).
- `memory` — persistent cross-session notes (only write when the user asks).

**VibeUE services (call from Python inside `execute_python_code`):** `unreal.<Name>Service.<method>()`
— Blueprint, BlueprintGraph (via BlueprintService), Material(+Node), Widget, Skeleton, AnimSequence,
AnimMontage, AnimGraph, Landscape(+Material), Foliage, MetaSound, SoundCue, Niagara(+Emitter,
+ScratchPad), StateTree, Input, EnumStruct, UVMapping, RuntimeVirtualTexture, MapBlockout,
GameplayTag, Viewport, Actor, Engine/ProjectSettings, **Performance** (`unreal.PerformanceService.frame_timing()`).
These overlap-trimmed services keep only what the engine lacks — for plain asset/actor/blueprint
basics the engine's own tools may be simpler (below).

**Use the engine's native tools for these (VibeUE intentionally doesn't duplicate them):**
- **Assets** (find / save / move / delete / duplicate / metadata): native Python
  `unreal.EditorAssetLibrary` / `EditorAssetSubsystem` inside `execute_python_code` (batchable), or
  Epic's `AssetTools` toolset via `call_tool`.
- **Screenshots / vision**: Epic's `EditorAppToolset` via `call_tool` — `CaptureViewport` (returns a
  PNG, and can overlay a world grid + actor labels), `CaptureEditorImage`, `CaptureAssetImage`.
- **PIE**: Epic's `EditorAppToolset.StartPIE` / `StopPIE` / `IsPIERunning` via `call_tool`.
- **Logs**: Epic's `LogsToolset.GetLogEntries` via `call_tool` (or read the `.log` file).
- **DataTables / DataAssets / enum-struct basics**: Epic's `DataTableTools` / `DataAssetTools` /
  `ObjectTools`. (VibeUE keeps only `EnumStructService` for create/edit of user enums & structs.)

---

## 3. Skills — native `AgentSkill` (lazy domain knowledge)

VibeUE's ~88 skill packs are registered as Unreal **AgentSkills** and served by the engine's
`AgentSkillToolset`. Skills tell you **what to do and why**; they do **not** replace discovery of exact
signatures.

**Discover + load (both are `call_tool` on `ToolsetRegistry.AgentSkillToolset`):**
```
call_tool(tool_name="ListSkills", toolset_name="ToolsetRegistry.AgentSkillToolset")
  → { "/VibeUE/Python/init_unreal_PY.VibeUE_blueprints": "Create and modify Blueprint assets…", … }

call_tool(tool_name="GetSkills", toolset_name="ToolsetRegistry.AgentSkillToolset",
          arguments={"skillPaths": ["/VibeUE/Python/init_unreal_PY.VibeUE_blueprints"]})
  → full markdown for that pack
```
- `ListSkills` returns **summaries only** (cheap) — call it once per session to see what exists. VibeUE
  packs are `/VibeUE/Python/init_unreal_PY.VibeUE_<name>`; the engine's own skills appear alongside.
- `GetSkills` returns full instructions **lazily** — request only the packs you need.
- **Sub-docs are their own skill entries** (e.g. `…VibeUE_blueprint_graphs__build_graph`,
  `…VibeUE_state_trees__api_reference`) — load them by path the same way; no `skill/section` argument.

**When to load a skill:** the user names a domain ("create a blueprint", "build a state tree"), or you
hit a non-obvious workflow. Then: read the pack → `discover_python_class` the classes it names → write
the Python. Don't reload a pack you already loaded this session.

---

## 4. Python basics

```python
import unreal  # lowercase

# Editor subsystems:
sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

# VibeUE services are static classes, called directly:
info = unreal.BlueprintService.get_blueprint_info("/Game/MyBP")

# Batch a whole task in ONE execute_python_code call, printing evidence as you go:
bp = unreal.BlueprintService.create_blueprint("BP_Enemy", "Actor", "/Game/Blueprints"); print("CREATED:", bp)
unreal.BlueprintService.add_variable(bp, "Health", "float"); print("ADDED: Health")
unreal.BlueprintService.compile_blueprint(bp); print("COMPILED:", bp)
```

---

## 5. When to use `deep_research` and `terrain_data`

**`deep_research`** — when you need information that isn't in the editor:
- `action="search"` / `action="fetch_page"` — research a UE topic, API, or technique before writing code.
- `action="geocode"` / `action="reverse_geocode"` — turn a place name into lat/lng (feeds `terrain_data`).

**`terrain_data`** — when the user wants terrain from a **real-world location**:
- `preview_elevation` → use the suggested `base_level`/`height_scale` → `generate_heightmap`
  (`resolution` MUST match the landscape) → import via `unreal.LandscapeService` → `get_water_features`
  for rivers/lakes.

**The real-world-terrain chain:** `deep_research(geocode "Mount Fuji")` → `terrain_data(generate_heightmap, lng/lat)`
→ `LandscapeService` import → `terrain_data(get_water_features)` → landscape splines. Load the
`terrain-data` and `landscape` skills for the resolution formulas and water workflow.

---

## 6. See what you built (screenshots)

After any **visible** change, capture and actually look before claiming success:
```
call_tool(tool_name="CaptureViewport", toolset_name="EditorToolset.EditorAppToolset")
```
It returns a PNG (base64) and can overlay a world-space grid + actor labels for spatial awareness. For
a running game, `StartPIE` first. **Open/read the image, judge it against the request, fix, re-capture.**

---

## 7. Diagnose performance

`PerformanceService` is VibeUE's net-new capability (the engine has no perf tooling). **STEP 0 is
always CPU-bound vs GPU-bound** — optimising the GPU does nothing on a CPU-bound frame:
```python
import unreal, json
print(unreal.PerformanceService.frame_timing())            # game/render/gpu ms + bound verdict — RUN FIRST
unreal.PerformanceService.start_trace("cap", "")           # Unreal Insights trace
# … reproduce the workload (ideally under PIE / standalone) …
unreal.PerformanceService.stop_trace()
print(unreal.PerformanceService.analyse("both", ""))       # frame stats + worst frames + log hitches
```
Load the `profiling` and `frame-rate` skills for the full drill-down.

---

## 8. Build & launch

When asked to rebuild / relaunch / test, use the project script — not manual `Build.bat`/editor commands:
- `./Plugins/VibeUE/BuildAndLaunchGame.ps1` (stops the editor, builds, relaunches).
- `-StrictRebuild` for a full plugin recompile under warnings-as-errors; `-Clean` to wipe artifacts;
  `-SkipBuild` to relaunch only.

---

## 9. Critical rules (evergreen)

- **Log every change for rollback.** Python has no auto-rollback — `print("CREATED:/ADDED:/MODIFIED:/DELETED:", path)`
  after each op so a mid-script failure can be undone.
- **Idempotent: check before create.** Use the service `*_exists()` (or `unreal.EditorAssetLibrary.does_asset_exist`)
  before creating, to avoid duplicates.
- **Compile after structure changes.** `unreal.BlueprintService.compile_blueprint(path)` after adding
  variables/functions/components.
- **Verify success with evidence.** For Blueprint/Widget/Material/AnimGraph/StateTree edits, a successful
  tool call isn't proof — re-read the asset (`get_nodes_in_graph`, `get_connections`, compile result)
  and report brief evidence.
- **Non-destructive.** Never remove-and-recreate to change a value, clear data to make a write succeed,
  or replace a whole object to change one field. Discover the supported setter; if none exists, report
  the gap. (StateTree reparenting: `move_state`, never remove+add.)
- **Loop prevention.** Track *outcomes*. Never repeat the same call with the same args >2× when output
  is unchanged; after 2 failed attempts at a goal, stop and report — don't try a 3rd variation.
- **Never** use modal dialogs, `input()`, blocking ops, long `time.sleep()`, or infinite loops.
- **Full asset paths** (`/Game/Blueprints/BP_Name`). **Colors are 0.0–1.0** (`{"R":1.0,"G":0.5,"B":0.0,"A":1.0}`).
- **`unreal.EditorLevelLibrary` is deprecated** — use `EditorActorSubsystem` (`get_all_level_actors()`
  + `isinstance` filtering; `get_all_level_actors_of_class` does not exist).

---

## 10. Communication & working style

- **Be concise** — this is an IDE tool. Before each tool call, one sentence on what/why; after, 1–2 on
  the result. Execute multi-step tasks straight through — don't pause for "continue".
- **Discover before you call.** Method signatures come from `discover_python_class`, not memory or skill
  prose. Skills say *which* class and *why*; discovery gives the exact call shape.
- **Commit at milestones** if the project is a git repo, so a bad experiment reverts cleanly.
- **Living gotchas:** when you solve a real problem, append a one-line gotcha+fix to this file so the
  next session doesn't relearn it.
