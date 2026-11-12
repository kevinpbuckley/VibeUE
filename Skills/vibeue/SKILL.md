---
name: vibeue
description: Unreal Engine 5 development using the VibeUE Python API. Use when working in Unreal Engine — blueprints, state trees, materials, actors, landscapes, animation, niagara, widgets, sound, foliage, gameplay tags, enhanced input, skeletons, PCG (procedural content generation), and more. VibeUE is an extension of Unreal's native MCP endpoint.
---

VibeUE is an **extension on Unreal Engine's native MCP endpoint** (`http://localhost:8000/mcp`).
There is no separate VibeUE server, no API key, and no in-editor chat — VibeUE simply registers
extra Python services (`unreal.<Service>`) and skill packs on top of the engine's own toolsets.

## Wait for VibeUE readiness after launch

`BuildAndLaunchGame.ps1` / `.sh` print `Editor-PID=<pid>` — treat that as the process identity. Check
once, then watch the filesystem for `<ProjectDir>/Saved/VibeUE/Signals/editor-<pid>-true.json` before
using MCP. Wait at most 180 seconds, do not poll MCP while waiting, and fail if that Editor process
exits or the timeout expires. Ignore signal files for other or dead PIDs. The signal only means
`RegisterToolsets()` reached its end; Python, World, and level readiness remain separate checks.

The file is JSON, written atomically, so it is complete the moment it appears:

```json
{"signal":"toolsets-registered","pid":21044,"createdUtc":"2026-08-03T17:04:11.921Z",
 "sessionStartUtc":"2026-08-03T17:03:22.108Z","pluginVersion":"3.0",
 "currentMap":"/Game/Maps/Level1_FullBody","mcpPort":8000,"mcpListening":true}
```

Process IDs get recycled. The launch scripts clear a matching stale signal right after starting the
Editor, but if you launch it some other way, verify `sessionStartUtc` is later than the moment you
started the process before trusting the signal.

`currentMap` is the loaded map's package name and the signal is re-published on every map open
(issue #554) — **gate world-edit scripts on it**. A relaunch opens the project default map unless
you pass `-Map /Game/Maps/YourMap` (`--map` on the .sh) to the launch script; editing "the current
world" after a relaunch without checking has silently modified the wrong level before.

## Health heartbeat — dead or wedged editor detection

`Signals/editor-<pid>-health.json` is rewritten every ~5s by a background thread (issue #555):

```json
{"signal":"health","pid":21044,"updatedUtc":"2026-08-03T17:09:00.000Z",
 "sessionStartUtc":"2026-08-03T17:03:22.108Z","gameThreadStallSeconds":0.03,
 "mcpPort":8000,"mcpListening":true}
```

Epic's MCP endpoint runs on the game thread with no request timeout, so a dead or wedged editor
hangs MCP calls for the client's full timeout — and even JSON-RPC `ping` hangs with it. Read the
health file instead: file missing or `updatedUtc` older than ~15s → the process is gone (relaunch);
`gameThreadStallSeconds` above ~10 → alive but wedged (modal dialog / crash handler; MCP will
hang — relaunch); fresh and small → the editor is healthy, debug something else.

Both the readiness and health JSON also carry `mcpPort` and `mcpListening` (issue B6). **Check
`mcpListening` before assuming a live link.** The very first readiness signal is written the instant
`RegisterToolsets()` ends, which is ~10 ms before Epic's MCP module finishes binding its HTTP listener,
so `mcpListening` is normally `false` for a fraction of a second at startup and then flips to `true`.
VibeUE republishes the readiness signal the moment the listener reports running, so a brief
`false`-then-`true` is expected and healthy — poll the file (or the health heartbeat) rather than
trusting the first read. If `mcpListening` stays `false` for more than the startup grace window
(~15 s), this editor's MCP module has no running HTTP server and every MCP call to it will fail to
connect; VibeUE logs one `LogVibeUEMcp: Error` line at that point saying whether the port is FREE
(this editor's server failed to start / never started) or held by ANOTHER process (it lost the
port fight). The usual cause of the latter is the port-8000 fight: a headless `UnrealEditor-Cmd` and
the GUI editor started together, one lost the bind, and the loser looks healthy while owning no MCP.
Restart the loser (never run a headless editor while the GUI editor is starting; also grep for Epic's
`LogHttpListener ... unable to bind to 127.0.0.1:<port>`). Note that a bind-probe issued from inside
the process cannot tell whether this editor or another holds the port, so `mcpListening=true` means
only that this process's module started a server, not that it won the port — and VibeUE never forces
`mcpListening` true from a probe result, it only ever reflects the module's own claim.

## A long-running script and the editor's safety rails

Python execution is engine-native on this branch: `editor_toolset.toolsets.python_scripting.PythonScriptingTools.execute_python`
(call `read_me` once per conversation first). VibeUE's own `execute_python_code` is gone, and with it
its run-record persistence — so there is no `last_python_result()` recovery for a call you abandoned.
Keep scripts short enough to answer inside your client's budget, and split long work with `await`
(`await asyncio.sleep(0)` yields a frame; `await vibeue.exec_tool_await(...)` waits for an async
engine tool).

What VibeUE still gives you are the editor's safety rails, which `execute_python` does not have:

```python
import unreal, vibeue

# A map left resident by an earlier script or tool will FATAL the editor on the next level load.
resident = unreal.PythonTools.get_resident_maps()
if resident:
    print("resident maps - release these first:", resident)

# So change level through the guard, which refuses instead of crashing:
vibeue.safe_load_level("/Game/Maps/Target")

# What did my script leave dirty?
print(unreal.PythonTools.get_dirty_packages())
```

`execute_python` does not save before it runs, which VibeUE's tool used to do by default. Save
explicitly before risky work:
`unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)`.

Do not hold level objects in variables across an `await` or a level change — re-fetch them, or a
level change can crash the editor. `vibeue.python_globals_holding(asset)` finds a global that is
pinning an asset and `vibeue.release_globals(names)` drops it.
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
| **Blueprints & gameplay logic** | author Blueprint classes & graphs, Enhanced Input, gameplay tags, Gameplay Ability System (abilities/attributes/effects/cues) — NOT AI behavior (→ AI), NOT AnimBP graphs (→ Animation), NOT C++/source (coding-agent handoff) | `blueprints`, `blueprint-graphs`, `enhanced-input`, `gas` |
| **AI** | author StateTree and Behavior Tree logic. Both are mostly Epic's in 5.9: `state_tree_toolset.state_tree.StateTreeToolset` owns StateTree states/tasks/conditions and `AIModuleToolset` owns BT and EQS. VibeUE keeps StateTree property bindings, asset creation, node-type discovery, theme colours and root parameters, plus BT/EQS graph repair, blackboard key rename/reference sweep and running an EQS query. Covers Behavior Trees / Blackboards / Environment Queries (Epic's AIModuleToolset, with VibeUE only for BT graph repair, blackboard key rename/reference sweep and running an EQS query) — NOT character body animation (→ Animation), NOT generic actor placement (→ Scene) | `state-trees`, `behavior-trees` |
| **Animation & rigging** | AnimBP state machines, AnimSequence keyframes, montages & AnimNotify wiring, bone/skeleton editing & retarget — NOT cinematic timelines (Epic Sequencer), NOT AI movement (→ AI), NOT authoring/adding sound assets — even a character's footstep sounds (→ Audio) | `animation-blueprint`, `animsequence`, `animation-editing`, `animation-montage`, `skeleton` |
| **Character customization (Mutable)** | Customizable Object graphs: modular character parts, mesh sections, skin/clothing parameters, switches, hat/outfit groups and child objects, compiling a CO — NOT plain skeletal mesh or skeleton edits (→ Animation), NOT ordinary material graphs (→ Materials) | `customizable-object` |
| **Materials & shading** | materials, instances, graph nodes, Custom HLSL — NOT Niagara particle materials (→ VFX), NOT landscape auto-materials (→ Environment) | `materials` |
| **VFX (Niagara)** | particle systems, emitters, scratch-pad HLSL, attaching/placing a Niagara component on an actor — NOT surface materials (→ Materials) | `niagara-emitters` (colour curves; systems/emitters/modules are Epic `NiagaraToolsets`) |
| **UI (UMG)** | widget blueprints, layout, fonts/brushes, MVVM — NOT the gameplay behind the UI (→ Blueprints) | `umg-widgets` |
| **Environment (world-scale)** | landscape sculpt/paint, landscape materials, foliage, PCG, map blockout, real-world terrain — NOT single-actor placement (→ Scene), NOT sound/audio (→ Audio) | `landscape`, `landscape-materials`, `landscape-auto-material`, `foliage`, `pcg`, `map-blockout`, `terrain-data` |
| **Audio** | SoundCue authoring — creating the sound asset itself: ambient, a character's footstep/foley, UI sounds — NOT triggering sounds from gameplay logic (→ Blueprints), NOT wiring an existing sound to anim-notify foot-plant frames (→ Animation) | `sound-cues` (MetaSound is Epic `MetaSoundToolset`) |
| **Assets, data & project** | import/export assets, UV mapping, the surviving PIE/engine settings helpers, and bulk asset maintenance you now drive yourself (iterate `AssetToolset` / `BlueprintToolset`, check before you write) — NOT actors placed in a level (→ Scene) | `asset-management`, `uv-mapping`, `engine-settings` |
| **Diagnostics, testing & run** | start/stop/query PIE, profile (CPU-vs-GPU / Insights), uncap frame rate — NOT fixing the logic a bug points to (→ its authoring area) | `pie-testing`, `profiling`, `frame-rate` |
| **Camera & viewport** | viewport camera, view mode, FOV, exposure, layout — NOT material look (→ Materials), NOT placing/editing light actors (→ Scene) | `viewport` |
| **Cinematics · Physics** | Sequencer cinematics and Physics assets (ragdoll/skeletal) are **Epic-native** — VibeUE adds no skill here — NOT enabling simulate-physics on a level actor (→ Scene) | *(none — use `list_toolsets`: `animation_toolset.*` / `PhysicsToolsets`)* |

A loaded skill gives you:
- workflows, gotchas, and property formats for the domain
- `vibeue_classes` / `unreal_classes` — class names to feed into `get_symbol` for live method signatures
- sub-doc references (`<skill>/<section>`) you can fetch via `GetSkills` for deeper detail

Always call `get_symbol` on the classes in `vibeue_classes` before writing code — never
guess method names from the skill content alone. **Batch the discovery into ONE call** instead of
one call per class:

```
# ONE call covers all classes and all topics:
get_symbol(
    class_name="unreal.LandscapeMaterialService, unreal.WidgetService, unreal.MaterialNodeService",
    method_filter="create|delete|layer|property|color")

# WRONG — three separate calls for three classes wastes round-trips and repeats boilerplate
```

`class_name` accepts a comma-separated list (response gains a `classes` array, one entry per class);
`method_filter` ORs keywords with `|`.

## How work gets done — `execute_python` is the workhorse

VibeUE services are plain Python on the editor's `unreal` module. Run everything through
`execute_python`:

```python
import unreal
unreal.StateTreeService.create_state_tree("/Game/AI/MyBehavior")
unreal.PerformanceService.set_background_throttling(False)
```

You get the full `unreal.*` API plus every `unreal.<Service>` VibeUE adds. Reserve `call_tool` for
**engine toolsets and skills** (e.g. `AgentSkillToolset`, `EditorToolset.EditorAppToolset`,
`LogsToolset`, `GameplayTagsToolset`, `AssetTools`).

**`code` is Python source, never a script path.** A path such as `C:/scripts/build.py` passed as
`code` is compiled as Python and fails (`SyntaxError` / `NameError`); it is not run as a file. (Code
that merely mentions a `.py` file in a comment or string runs normally.) To run a script file, pass
code that runs it:

```python
import unreal, runpy
runpy.run_path(r"C:/scripts/build.py")        # the script gets its own globals
# or, sharing the console globals every execute_python call sees:
exec(open(r"C:/scripts/build.py").read())
```

**`auto_save` (default true).** Before running your script, `execute_python` saves every dirty
content AND world package headlessly (issue #433: this avoids the modal save dialog that would hang
the call). Every reply reports what actually happened: `auto_save` (true only when the sweep really
ran), `auto_save_note` (empty when it ran, otherwise `opted_out`, `previous_run_crashed`,
`editor_unavailable`, `pie_active`, `save_failed`, or `disabled_by_setting` when the "Disable Python
Auto-Save" editor preference is on) and `saved_packages` (the package names it
wrote). `auto_save` is the OUTCOME, not an echo of your argument -- so `auto_save: true,
saved_packages: []` means "swept, nothing was dirty", never "skipped". Pass `auto_save=false` to run
the script WITHOUT that sweep -- use it when you do not want in-flight editor edits flushed to disk,
or to keep a mutation you are about to make from being interleaved with an unrelated dirty package.
The sweep is skipped anyway after a crashed run, when GEditor is missing, or in PIE.

**`dirtied_packages` / `cleaned_packages` (always present, also on an error reply).** The packages
YOUR SCRIPT left dirty, and the ones it left clean (saved, cleaned or unloaded), from snapshots taken
after the auto-save sweep and right after the script. Pre-existing dirt and the sweep's own saves never
show up there. Check `dirtied_packages` after a script that raised: a half-finished edit stays dirty,
and the next `auto_save=true` call would save it. To read the dirty set mid-script, use
`unreal.PythonTools.get_dirty_packages()` (pass `False` to leave maps out) instead of snapshotting
`EditorLoadingAndSavingUtils.get_dirty_content_packages()` by hand. Expect two engine extras in the
list: a never-saved map appears as `/Temp/Untitled_N`, and a dirty unsaved map's MapBuildData package
can be marked dirty by the enumeration itself. Both lists are empty after a crashed (SEH) run.

## Never leave a map loaded - it crashes the editor on the next level change

Opening a map as an **asset** keeps it resident:

```python
w = unreal.load_asset("/Game/Maps/Foo")          # loads and KEEPS Foo
unreal.EditorAssetLibrary.load_asset("/Game/Maps/Foo")
unreal.find_object(None, "/Game/Maps/Foo.Foo")
```

The engine checks on every level load that no other map package is still alive. That check is a
**fatal, not a warning** - the editor dies with `World Memory Leaks` (`EditorServer.cpp`) and
`Old level package /Game/Maps/Foo not cleaned up by garbage collection`. The crash lands on whoever
calls `load_level` next, which may be minutes later and a different tool entirely, and the message
names neither the script nor the map that caused it.

`execute_python` reports this in every reply so the warning arrives with its cause:

```json
"resident_maps": ["/Game/Maps/Foo.Foo"]
```

**Non-empty `resident_maps` means the next level load will crash the editor.**

There is no reliable in-process cure. A map package loads with `RF_Standalone`, and **neither
`unreal.SystemLibrary.collect_garbage()` nor `EditorLoadingAndSavingUtils.unload_packages()`
releases it** - both were tried against a live editor and the world stayed resident. Once a map is
stranded, **restart the editor** (`BuildAndLaunchGame.ps1`) before changing levels.

So treat this as prevention, not repair:

- To read a map's **metadata**, use the asset registry - it loads nothing:
  ```python
  ar = unreal.AssetRegistryHelpers.get_asset_registry()
  maps = ar.get_assets_by_path("/Game/Maps", recursive=True)
  ```
- To **change level**, use `vibeue.safe_load_level(path)`: it reads
  `unreal.PythonTools.get_resident_maps()` (the `resident_maps` list minus streaming sublevels of the
  open level) and raises
  `vibeue.LevelLoadRefused`, loading nothing, while any map other than the open level is resident, when
  the target itself is resident (unless `allow_resident_target=True`), or when the registry has no map
  at that path; otherwise it calls `unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(path)`
  and returns its bool. Never `load_asset` a map to "look at it".

The open level is never reported - only stragglers.

## Tools — what each is for

| Tool | Use it for |
|------|-----------|
| `execute_python` | Run `unreal.*` Python in the editor — the workhorse for every VibeUE service |
| `call_tool` | Invoke engine toolsets and skills (skills, PIE control, logs, assets, gameplay tags) |
| `describe_toolset` / `list_toolsets` | Discover engine toolsets and their actions/args |
| `get_symbol` / `get_symbol` / `search_symbols` | Get live signatures before writing code |
| `search_symbols` | Enumerate editor subsystems for `unreal.get_editor_subsystem(...)` |
| `terrain_data` | Real-world heightmaps + water splines (see `terrain-data` skill) |
| `deep_research` | Web research / page fetch / geocoding. If Jina Reader refuses your network (HTTP 401), set a free Jina key in the `JINA_API_KEY` environment variable and restart the editor |

`deep_research` and `terrain_data` run off the game thread, so the editor keeps working while they wait. Four run at
a time and more wait their turn; past 16 running or waiting, a call is refused with `BUSY` (retry when one finishes).
Cancelling one from the client (`notifications/cancelled`) matches it by JSON-RPC request id only: with several MCP
clients connected, a cancel can also end another client's call that carries the same id.

## Engine toolsets replace the old VibeUE tools

Several capabilities that used to be VibeUE-specific MCP tools are now the engine's native toolsets,
called via `call_tool` (run `describe_toolset` for action names/params):

| Need | Engine toolset (via `call_tool`) |
|------|----------------------------------|
| Start / stop / query PIE | `EditorToolset.EditorAppToolset` → `StartPIE` / `StopPIE` / `IsPIERunning` |
| Capture a viewport screenshot | `EditorToolset.EditorAppToolset` → `CaptureViewport` |
| List / read / filter / tail UE logs | `ConsoleToolset.LogsToolset` -> `GetLogEntries` (needs a `pattern`) |
| Run a console command | `ConsoleToolset` -> `RunConsoleCommand` |
| Search / open / save / move / import assets | `AssetTools` |
| Materials: create, expressions, connections, recompile, stats | `EditorToolset.MaterialToolset` (VibeUE's `MaterialService` is **deleted**) |
| Material instances: create, reparent, list and set typed parameters | `EditorToolset.MaterialInstanceToolset` (no bulk setter: iterate) |
| Any material / expression / object property | `EditorToolset.ObjectToolset` -> `GetProperties` / `SetProperties` |
| Run / poll / cancel automation tests (the old `run_scenario` shape) | `AutomationTestToolset` -> `DiscoverTests`, `RunTests`, `GetTestStatus`, `GetTestResults`, `StopTests` |
| Frame timings, stat groups, stat counters | `PerfToolset.StatsToolset` -> `GetFrameTimings` |
| Read / write / list files under `/Saved` (run logs, reports, journals) | `FileSystemToolset` -> `ReadTextFile`, `WriteTextFile`, `EditTextByAnchor`, `ListDirectory`, `DeleteFile` |
| Which assets a change actually touched (provenance) | `UndoHistoryToolset` -> `GetRecentTransactions`, `GetTransactionDetails`, `SearchTransactions`, `GetTransactionRecencyIndex` |
| Blueprint interfaces, metadata, compile (the old bulk-maintenance jobs) | `BlueprintToolset` -> `AddInterface`, `RemoveInterface`, `CompileBlueprint`, `SetVariableTooltip`, `SetVariableCategory`, `SetVariableInstanceEditable`, `SetVariableReadOnly` |
| Move assets, fix redirectors, references, dependencies, search | `AssetToolset` -> `Move`, `FixUpRedirectors`, `GetReferencers`, `GetDependencies`, `FindAssets` |
| Single-tag gameplay-tag CRUD | Epic `GameplayTagsToolset` via `call_tool` |
| Inspect a live Ability System (attributes/tags/effects/abilities), attribute-set discovery, gameplay cues | `GASToolsets.*` (see `gas` skill) |

Performance/Insights tracing is the one net-new VibeUE service — `unreal.PerformanceService.*` (see
the `profiling` skill) — because Unreal 5.8 shipped no performance toolset. On 5.9 the split is narrower: frame timings are
Epic's (`PerfToolset.StatsToolset.GetFrameTimings` -- `PerformanceService.frame_timing` is **gone**),
trace start/stop is `ConsoleToolset.RunConsoleCommand("Trace.*")` (`start_trace` / `stop_trace` are
gone), and what is left on `PerformanceService` is `force_hitch`, `analyse`, the standalone-capture
trio (`start_standalone` / `stop_standalone` / `get_standalone_status`) and
`set_background_throttling` / `get_background_throttling`.

**There is no bulk-maintenance tool any more.** `WorkflowService.run_bulk_maintenance` is deleted,
and so is the `bulk-maintenance` skill pack. The individual operations are all engine-native --
`BlueprintToolset.AddInterface` / `RemoveInterface` / `CompileBlueprint` / `SetVariableTooltip` /
`SetVariableCategory` / `SetVariableInstanceEditable` / `SetVariableReadOnly`, and
`AssetToolset.Move` / `FixUpRedirectors` / `GetReferencers` / `GetDependencies` / `FindAssets` -- but
what VibeUE added around them is what you lose: batching, the dry run, path scoping, the per-target
report and rollback. **That is now your job:** iterate the targets yourself one at a time, check
each one (`GetReferencers` / `GetDependencies` / `Exists`) *before* you write, keep the explicit
target list rather than feeding a search straight into a mutation, and record what you did as you go
(`FileSystemToolset.WriteTextFile`, plus `UndoHistoryToolset.GetRecentTransactions` for what really
changed) because nothing resumes or rolls back for you. A failure mid-way leaves the earlier targets
applied.

`WorkflowService.get_environment()` survives, and it is the **only** Workflow tool left -- nothing in
Epic's toolsets reports the engine version, build configuration, toolchain location or build
staleness, so call it before claiming any of those.


## Waiting inside one call: top-level await

A script with a top-level `await` keeps the editor ticking at each await, and the reply arrives when
the script ends, with everything it printed. Scripts without one run as they always have.

```python
import asyncio, unreal, vibeue

await asyncio.sleep(0)    # one editor frame
await asyncio.sleep(2.0)  # two seconds of wall time, with the editor running
# An async engine tool, finished in this one call (no exec_tool_async + collect_tool_result):
image = await vibeue.exec_tool_await("EditorToolset.EditorAppToolset", "CaptureAssetImage", {...})
```

- Other calls and the user can change the editor while a script is paused: re-fetch what you need
  after each await. Do not keep level objects in variables across an await; a level change with one
  still held can crash the editor.
- A paused script does not advance during garbage collection, a save, a slow task or a modal dialog.
- Over MCP only. A client's `notifications/cancelled` raises CancelledError at the current await.
  At most 8 scripts can be paused at once; past that the call answers `BUSY`.
- While any script is paused, other calls skip the auto-save sweep (`auto_save_note`:
  `await_run_in_progress`), and their `dirtied_packages` can include what the paused script changed.

`fresh_namespace=true` (with or without await) runs a script in a namespace dropped when it ends,
instead of the globals every call shares, so a top-level variable cannot keep a level alive.

## Gate and audit helpers in `vibeue`

`import vibeue` also carries the helpers that gate harnesses used to hand-roll. Each has a docstring
with an example (`help(vibeue.wait_for)`).

```python
import unreal, vibeue

# Waits never block: the editor does not tick inside a call, and time.sleep() on the game thread
# freezes every PIE net driver. Start the wait in one call, read it in a later one (or chain .then),
# or use top-level await (below) to wait inside one call.
w = vibeue.wait_for(lambda: some_condition(), timeout_s=10, description="seated")
vibeue.wait_for_players(3, timeout_s=20).then(lambda w: print("players", w.ok, w.result))
# ... next call:
print(w.done, w.ok, w.elapsed_s, vibeue.waits())
vibeue.cancel_all_waits()                 # unregisters every handle; run at the end of a gate

before = vibeue.dirty_snapshot()          # bracket a read-only audit (pass auto_save=false too)
print(vibeue.dirty_delta(before))         # {'dirtied': [...], 'cleaned': [...]}

with vibeue.viewport_state():             # camera, FOV and exposure come back on exit
    unreal.ViewportService.set_exposure(True, 11.5)
with vibeue.gate_session(clients=3, net_mode="ListenServer"):   # throttling + PIE settings restored
    pass
# A gate that spans calls: gs = vibeue.gate_session(...); gs.enter() in call 1, gs.exit() in the last.
```

- `set_prop_pie(obj, name, value)` writes with notify mode NEVER; the default mode reruns a live PIE
  actor's construction script and recreates every Blueprint-added component locally.
  `get_prop(obj, name, default)` returns `default` on a protected property; other errors such as a
  misspelled name still raise (`strict=True` raises on protected ones too).
- `set_array_prop(obj, name, values)` copies the values, sets them with notify mode ALWAYS and calls
  `modify()`. A plain `get_editor_property` + `append` + `set_editor_property` edits a live
  reference, so the set sees no change: no dirty mark and no undo entry.
- `mapping_context_priority(subsystem, ctx)` returns the priority, or None. `has_mapping_context`
  returns the priority, and priority 0 is common, so `bool()` of it is wrong.
- `canvas_slot(wbp_path, widget)` / `set_canvas_slot(...)` edit a design-time `CanvasPanelSlot`
  through `<Asset>:WidgetTree.<Widget>`, then compile and save.

### Already in the plugin (stop hand-rolling these)

- Compiling is engine-side: `BlueprintToolsets.BlueprintToolset.CompileBlueprint` via `call_tool`
  (or `vibeue.exec_tool` from inside `execute_python`). VibeUE's `compile_blueprint` is gone.
- Pin values are engine-side too: `BlueprintToolset.GetPinValue` / `SetPinValue`, or read the whole
  graph as DSL with `BlueprintDslToolset.Read`. VibeUE's `set_node_pin_value` / `get_node_pins` are
  gone with the rest of the node surface.
- `unreal.PerformanceService.set_background_throttling(False)` / `get_background_throttling()` replace the hand-written `EditorPerformanceSettings` CDO write. Both return a JSON string.
- Runtime input: Epic `InputToolset.ApplyInput` (`entries` + `durationSeconds`) via `call_tool` for every bounded press or hold -- `inject_action_for` and `inject_key` are **gone**. VibeUE keeps `unreal.InputService.start_injection(action_path, x, y, z, pie_instance)` and `stop_injection(action_path, pie_instance)` for an INDEFINITE hold released in a later call, and `inject_action(action_path, x, y, z, pie_instance)` for a single queued one-tick press. Either way, don't hand-roll a slate-tick loop around `inject_input_vector_for_action`. `pie_instance` picks the PIE world; -1 means the first PIE world with a local player.
- `unreal.EngineSettingsService.set_pie_settings(net_mode, clients, run_under_one_process)` / `get_pie_settings()` set and read the PIE net mode ("Standalone", "ListenServer", "Client"). It is saved to the user's ini. These two are **Python-only helpers** now: `EngineSettingsService` has no MCP tools left, so they only exist inside `execute_python`.
- `unreal.ActorService.rerun_construction_scripts(label_or_name)` matches an actor by label or object name. It works in the editor world only and refuses during PIE.
- `WidgetService.capture_preview` is **gone** (as are `get_live_property`, `is_pie_running`, `start_pie` and `stop_pie`): capture a widget asset with `EditorAppToolset.CaptureAssetImage`, or spawn it with `WidgetService.spawn_widget_in_pie` and shoot the running viewport with `SceneCaptureToolset.CaptureGameViewport`, which draws the UMG over it. To read a live widget's properties: `SlateInspectorToolset.Snapshot` -> `GetUMGInfo(ref)` -> `ObjectToolset.GetProperties`.

## Additional gotchas

- A Python traceback still proves the MCP link is up — only "Unable to connect" means it is down. A dropped server or a newly added tool needs a Claude Code restart. A stuck call is usually a modal dialog, and the client timeout does not stop the script; there is no run-record recovery on this branch, so dismiss the dialog and check the Output Log rather than re-running a mutation.
- Don't run a headless `UnrealEditor-Cmd` while the GUI editor is starting — they fight over the MCP port and the loser runs with no MCP; the readiness signal's `mcpListening` flag tells you when the port is claimed. `-run=pythonscript` fully substitutes for pure-asset work when no editor runs, but not for World Partition world surgery.
- Engine toolset quirks: `LogsToolset.GetLogEntries` requires a `pattern`; `StartPIE` via `execute_tool` needs the full options object; results can be double-encoded (`vibeue.exec_tool` decodes them). A genuinely async tool like `CaptureAssetImage` never completes inside one call — fire it with `vibeue.exec_tool_async` and read it with `vibeue.collect_tool_result` on the next call.
- A hand-registered `register_slate_post_tick_callback` whose handle you lost keeps ticking until the editor restarts. A `vibeue.wait_for` wait unregisters its handle when it finishes, and the registry keeps every pending one, so `vibeue.cancel_all_waits()` can end a wait whose `Wait` object you no longer hold.
