---
name: vibeue
description: Index for Unreal Engine 5.9 editor work through VibeUE and Epic's native MCP toolsets - the execute_python and call_tool contract, which skill pack to load for a task (blueprints, state trees, materials, actors, landscapes, animation, cinematics, Niagara, widgets, sound, foliage, gameplay tags, enhanced input, skeletons, PCG, PIE testing, profiling and more), editor readiness and health signals, and safe level loading. Load it first when unsure which pack applies, or when a call fails with "unrecognized parameter(s) [code]", "Unable to import JSON value into property script", "imports must be at the top level of the script", a KeyError on returnValue, "There are unsaved level changes in /Temp/", "the Unreal Editor is running a slow task", or "The operation timed out" (long work: vibeue.start_job).
---

VibeUE extends Unreal 5.9's **native MCP endpoint** (`http://127.0.0.1:8000/mcp` by default). There is no
separate VibeUE server and no in-editor chat: VibeUE registers extra services (`unreal.<Name>Service`, also
callable as tools `VibeUE.<Name>Service.<Method>`), two MCP tools (`deep_research`, `terrain_data`) and the
skill packs listed below, next to the engine's own toolsets. A free VibeUE API key (Editor Preferences ->
Plugins -> VibeUE) is needed only for `terrain_data`.

## 1. The call contract (5.9)

**Over MCP**, every engine or VibeUE tool goes through `call_tool` with the toolset's FULL name, the tool
name, and the tool's own camelCase arguments:

```
call_tool(toolset_name="EditorToolset.EditorAppToolset", tool_name="StartPIE",
          arguments={"options": {"playMode": "PlayMode_InEditorFloating", "warmupSeconds": 2}})
```

A bare toolset name (`"EditorAppToolset"`, `"AutomationTestToolset"`) is not found. Look a tool up with
`search_symbols` (`kinds=["TOOL"]`) or `get_symbol(name="<toolset or full tool name>")` rather than
`describe_toolsets`, which dumps every schema in the toolset.

**Python** runs through the engine's `editor_toolset.toolsets.python_scripting.PythonScriptingTools`
(VibeUE's own `execute_python_code` is gone). Call `read_me` once per conversation, then `execute_python`
with ONE argument, `script`, a code block:

```
call_tool(toolset_name="editor_toolset.toolsets.python_scripting.PythonScriptingTools",
          tool_name="execute_python",
          arguments={"script": {"language": "Python", "content": "import unreal\nunreal.SystemLibrary.get_engine_version()"}})
```

The rules the runner enforces, and what it returns:

- **The result is the value of the script's final expression**: a string as it is, anything else as JSON
  (objects as paths, structs as JSON objects, enums as names). End the script with what you want back, e.g.
  a dict. `print()` goes to the Output Log (`LogPython`), not to the result.
- **Imports only at the top level** of the script, and no star imports. An import inside a function, `if`
  or `try` rejects the whole script before anything runs ("imports must be at the top level of the
  script").
- **Rejected calls**: `time.sleep` (use `await asyncio.sleep(...)`), `subprocess.run` / `call` /
  `check_output` (use `asyncio.create_subprocess_exec`), `requests.*` / `urllib.request.urlopen` (use
  `deep_research` or an async client), and `unreal.ToolsetRegistry.execute_tool` (use `call_tool`).
- **Top-level `await` works**, and the editor ticks during each await. Yield in long loops with
  `await asyncio.sleep(0)`. Keep one call well under a minute: the client times out while the script
  keeps running.
- **Each call has a fresh namespace.** `unreal` and `asyncio` are pre-imported; repeat your other imports
  in every script. Only module state (a module you import) lives on between calls.
- **Calling a tool from Python**: `await call_tool("<Toolset full name>.<Tool>", **camelCaseArgs)`, already
  in the namespace. It returns the tool's value **directly**: there is no `returnValue` envelope to unwrap.
  Object references come back as `{"refPath": path}`; pass Unreal objects, classes, structs and enums as
  they are. Reading a field the tool does not declare raises a KeyError that lists the fields it does.
  The signature is `call_tool(name, /, **arguments)`: a `(toolset, tool, {args})` or `(tool, toolset)` call
  raises `TypeError: ... takes 2 positional arguments`, and a call without `await` never runs.
  A `{"refPath": ...}` you pass needs the **object** path (`/Game/BP/BP_X.BP_X`); the bare package path
  is rejected with "is not a valid object path".

```python
import unreal
folders = await call_tool("EditorToolset.AssetToolset.ListFolders", rootPath="/Game/", bRecursive=False)
pie = await call_tool("EditorToolset.EditorAppToolset.IsPIERunning")
{"folders": folders, "pie": pie, "dirty": unreal.PythonTools.get_dirty_packages()}
```

| Error | Cause | Fix |
|---|---|---|
| `unrecognized parameter(s) [code]`, `Unable to import JSON value into property script` | The old `execute_python_code` shape | `arguments={"script": {"language": "Python", "content": "..."}}` |
| `imports must be at the top level of the script` | An import inside a function / `if` / `try` | Move every import to the top |
| `KeyError: 'returnValue'`, a KeyError listing fields | Unwrapping an envelope `call_tool` already removed, or a guessed field name | Use the value as returned; check field names with `get_symbol(name="<full tool name>")` |
| `Toolset ... not found`, `ToolsetIsNotRegistered` | Short toolset name | Full name, e.g. `SceneCaptureToolset.SceneCaptureToolset` |
| `get_symbol` rejects `class_name` / `method_filter` | Old VibeUE discovery shape | `get_symbol(name="LandscapeService")` or `get_symbol(name="LandscapeService.import_heightmap")`; `search_symbols(pattern="landscape.*layer")` |

**Discover before you write.** `search_symbols(pattern, kinds, match, accepts, returns)` searches the
registered tools AND the whole `unreal` module of this editor session; `get_symbol(name=...)` returns one
type (its editor properties, enum values and methods), member (`"Class.member"`), toolset or tool.
Class names are top-level names without the `unreal.` prefix. Skills say which class and why; discovery
gives the exact call.

**No auto-save.** `execute_python` never saves for you (the old `auto_save` sweep and its
`saved_packages` / `dirtied_packages` reply fields are gone). Save explicitly when you mean to:
`unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)`, and ask what a script left dirty
with `unreal.PythonTools.get_dirty_packages()`. Before risky work, save first.

To run a script FILE, put `runpy.run_path(r"C:/scripts/build.py")` in the content. A path passed as the
content is compiled as Python and fails.

**The first call of a session: `vibeue.editor_state()`.** One call returns what you otherwise check with
snippets before and after every step, and whether a person is working in this editor:

```python
import vibeue
s = vibeue.editor_state()
# s["level"], s["level_dirty"], s["pie"]["running"], s["dirty"]["maps"/"content"], s["resident_maps"],
# s["selection"]["actors"/"assets"], s["viewport"] (camera, FOV, exposure), s["background_throttling"],
# s["undo"]["from_user"/"from_scripts"/"unknown"], s["user_edits"]
```

`user_edits` lists the person's undo entries that are still applied (label, id, objects). VibeUE records
who began each transaction: "user" when it began inside a Slate input event (a gizmo drag, a Details-panel
edit, a menu entry or hotkey), "script" for execute_python, tool calls and timers, "unknown" for edits from
before VibeUE was recording. Keys or clicks you inject through `SlateInspectorToolset` count as "user": to
the editor they are input. If `user_edits` names edits on the open map, or PIE runs, do not reload,
discard or change the level without asking. Pass `since=<an earlier state>` to see only what happened after
it (did the person edit anything while my job ran?). Wrap camera, throttling, PIE-settings or selection
changes in `with vibeue.preserve_editor():` (or `p = vibeue.preserve_editor(); p.enter()` ... `p.exit()`
across calls) so the person gets their editor back as they left it.

## 2. Skills: list, load, route

Skills are Markdown packs served by the engine's `ToolsetRegistry.AgentSkillToolset`:

```
call_tool(toolset_name="ToolsetRegistry.AgentSkillToolset", tool_name="ListSkills", arguments={})
call_tool(toolset_name="ToolsetRegistry.AgentSkillToolset", tool_name="GetSkills",
          arguments={"skillPaths": ["/Skills/VibeUE/pcg", "/Skills/VibeUE/materials"]})
# or in a script:
docs = await call_tool("ToolsetRegistry.AgentSkillToolset.GetSkills", skillPaths=["/Skills/VibeUE/landscape"])
```

- VibeUE packs are **`/Skills/VibeUE/<slug>`** (the folder name below, hyphens kept). `ListSkills` is the
  live list: summaries only, cheap. A wrong path returns nothing, with no error, so copy paths from
  `ListSkills`.
- Engine Markdown skills are `/Skills/Engine/<name>` and `/Skills/Platforms/<Platform>/<name>`; other
  plugins' are `/Skills/<Plugin>/...`. Epic's Python-registered skills use object paths, e.g.
  `/AnimationAssistantToolset/Python/animation_toolset/skills/sequencer_core_PY.SequencerCoreSkill` and
  `/SlateInspectorToolset/Python/slate_inspector_toolset/skills/input_simulation_PY.SlateInspectorInputSimulationSkill`.
- A pack's deeper pages are Markdown files next to its `SKILL.md`, listed under **Reference pages**. The
  engine rewrites those links to absolute file paths when it serves the pack: open them with your
  file-reading tool. They are not separate skill entries.
- The engine re-reads the files on every call; no reload or restart needed.

**Engine knowledge is a different library.** If your session also has the **Unreal Engine Skills** MCP
server (one tool, `unreal-engine-skills-manager`: `{action: "search", query}` then `{action: "load",
skill}`), use it for engine knowledge: before writing engine C++, after an `unreal.` AttributeError or a
wrong-signature error, and for cook, packaging and platform work. The packs here cover how to drive the
editor's tools; that library covers how the engine works.

**Route by functional area.** Find the row whose scope fits and load its pack(s). The *NOT for* part
tells you where a near miss belongs.

| Functional area | Use for, NOT for | Load skill(s) |
|---|---|---|
| **Scene & actors** | place / move / arrange / organize / tag actors in a level. NOT gameplay logic (Blueprints), NOT world-scale terrain or foliage (Environment), NOT attaching a Niagara component (VFX) | `level-actors` |
| **Blueprints & gameplay logic** | Blueprint classes and graphs, Enhanced Input, gameplay tags, Gameplay Ability System. NOT AI behavior (AI), NOT AnimBP graphs (Animation), NOT C++ | `blueprints`, `blueprint-graphs`, `enhanced-input`, `gas` |
| **AI** | StateTree and Behavior Tree logic. Epic's `state_tree_toolset.state_tree.StateTreeToolset` and `AIModuleToolset` own most of it; VibeUE keeps StateTree bindings, asset creation, node-type discovery and root parameters, BT/EQS graph repair, blackboard key rename and reference sweep, and running an EQS query. NOT body animation (Animation), NOT actor placement (Scene) | `state-trees`, `behavior-trees` |
| **Animation & rigging** | AnimBP state machines, AnimSequence keyframes and notifies, montages, bones, sockets, IK retargeting. NOT Level Sequences (Cinematics), NOT AI movement (AI), NOT making sound assets (Audio) | `animation-blueprint`, `animsequence`, `animation-editing`, `animation-montage`, `skeleton` |
| **Cinematics** | Level Sequences: bindings, cameras and cuts, keys, audio sections, playing, scrubbing and capturing a sequence in the editor or PIE. Routes to Epic's nine Sequencer skills. NOT UMG widget animation (UI), NOT an AnimSequence's own keys (Animation) | `cinematics` |
| **Character customization (Mutable)** | Customizable Object graphs: modular parts, mesh sections, skin and clothing parameters, compiling a CO. NOT plain skeletal mesh edits (Animation), NOT material graphs (Materials) | `customizable-object` |
| **Materials & shading** | materials, instances, graph nodes, Custom HLSL. NOT Niagara materials (VFX), NOT landscape auto-materials (Environment) | `materials` |
| **VFX (Niagara)** | particle systems, emitters, scratch-pad HLSL, colour curves (systems, emitters and modules are Epic `NiagaraToolsets`). NOT surface materials (Materials) | `niagara-emitters` |
| **UI (UMG)** | Widget Blueprints, layout, fonts and brushes, MVVM, and driving a live UI in PIE (click, drag, type, read). NOT the gameplay behind the UI (Blueprints) | `umg-widgets` |
| **Environment (world-scale)** | landscape sculpt and paint, landscape materials, foliage, PCG, map blockout, real-world terrain. NOT single-actor placement (Scene), NOT audio (Audio) | `landscape`, `landscape-materials`, `landscape-auto-material`, `foliage`, `pcg`, `map-blockout`, `terrain-data` |
| **Audio** | SoundCue authoring: ambient, footstep and foley, UI sounds (MetaSound is Epic `MetaSoundToolset`). NOT triggering sounds from gameplay (Blueprints), NOT notify wiring (Animation) | `sound-cues` |
| **Fab content** | what the user owns on Fab or can get free, SEEING listings and installed packs as images (`FabPreviewService` contact sheets), installing them (Epic's `FabToolset`). NOT importing files from disk (Assets) | `fab-content` |
| **Assets, data & project** | import and export, UV mapping, the surviving PIE and engine settings helpers, bulk asset maintenance you drive yourself. NOT actors in a level (Scene) | `asset-management`, `uv-mapping`, `engine-settings` |
| **Diagnostics, testing & run** | start, drive and capture PIE, automation tests, profiling (CPU vs GPU, Insights), frame-rate caps. NOT fixing the logic a bug points to (its authoring area) | `pie-testing`, `profiling`, `frame-rate` |
| **Camera & viewport** | editor viewport camera, view mode, FOV, exposure, layout. NOT material look (Materials), NOT light actors (Scene), NOT sequence cameras (Cinematics) | `viewport` |
| **Physics** | Physics assets (ragdolls) are Epic-native: `PhysicsToolsets.PhysicsAssetToolset`. VibeUE adds no pack. NOT simulate-physics on one actor (Scene) | *(none - Epic's toolset)* |

A loaded pack gives workflows, gotchas and property formats, plus `vibeue_classes` / `unreal_classes` to
pass to `get_symbol` before you write code.

## 3. Editor readiness and health

`BuildAndLaunchGame.ps1` / `.sh` (README "Build & launch script") ask a running editor of the project to quit over
MCP and refuse with exit 5, listing the packages, when it has unsaved work (`-Force` / `--force` closes it anyway):
save or discard first, never kill it yourself. Old logs move to `Saved/Logs/Previous/<timestamp>`, so the log of a
crashed session is still there after a relaunch. `-Port <n>` gives the new editor its own MCP port, `-Muted` mutes
its Windows audio session (in-engine recording still works), `-WaitForReady` waits for the signal below while the
editor shows signs of life, and `-WaitForIdle` also waits until the health file reports no slow task, no shader jobs
and no assets compiling.

They print `Editor-PID=<pid>`. Watch the filesystem for
`<ProjectDir>/Saved/VibeUE/Signals/editor-<pid>-true.json` (at most 180 s, without polling MCP); it is JSON
written atomically: `signal`, `pid`, `createdUtc`, `sessionStartUtc`, `pluginVersion`, `currentMap`,
`mcpPort`, `mcpListening`. It is re-published on every map open, so **gate world edits on `currentMap`** (a
relaunch opens the project default map unless you pass `-Map /Game/Maps/YourMap`). PIDs get recycled: if
you launched the editor yourself, trust the signal only when `sessionStartUtc` is later than your launch.
`mcpListening` is normally `false` for a moment at startup and then `true`; if it stays `false` past
~15 s this editor has no MCP server (usually it lost the port to another editor; see section 6).

`Signals/editor-<pid>-health.json` is rewritten every ~5 s by a background thread, so read it instead of
waiting on a hanging call (Epic's MCP endpoint runs on the game thread with no request timeout):

```json
{"signal":"health","pid":21044,"updatedUtc":"2026-11-14T17:09:00.000Z","sessionStartUtc":"...",
 "gameThreadStallSeconds":0.03,"mcpPort":8000,"mcpListening":true,"shaderJobs":0,"assetCompilations":0,
 "slowTask":false,"slowTaskText":"","slowTaskSeconds":0,"modal":false,"modalTitle":"","modalSeconds":0}
```

`shaderJobs` / `assetCompilations` (-1 until first sampled) count the shader compile jobs and the assets still
compiling in the background (textures, meshes, distance fields, Lumen cards). Above 0 the editor answers calls, but
captures show grey materials and frame timings mean nothing yet: wait for both to read 0.

| Reading | Meaning | Do |
|---|---|---|
| File missing, or `updatedUtc` older than ~15 s | The process is gone | Relaunch |
| `slowTask: true` (`slowTaskText` says what, e.g. a build or a save) | Busy, not wedged. MCP refuses tool calls ("Not run: the Unreal Editor is running a slow task") and the stall grows while it runs | Do not retry in a loop and do not relaunch: watch the file and call again once `slowTask` is false |
| `modal: true` (`modalTitle`) | A modal dialog is open; MCP refuses calls ("is showing a modal dialog") | Ask the user to close it (or dismiss it if you opened it), then call again |
| `gameThreadStallSeconds` above ~10 with both flags false | Wedged: a native message box, a crash handler, or a hang | Relaunch |
| Fresh, small stall | Healthy | Look for the problem elsewhere |

A slow task that reports no progress, or a native OS message box, does not set the flags; then only the
stall grows.

**Work longer than one call, and waiting for the editor.** A call that outlives the client's timeout ("The
operation timed out") keeps running in the editor, and nothing tells you whether it finished; a resend can do
the work twice. Run such work as a job instead, and wait for compiles with `wait_idle`:

```python
import vibeue
async def rebuild(job):                       # takes the job (optional) to report progress
    for i, path in enumerate(paths):
        ...                                   # one step
        job.report(i / len(paths), path)
        await asyncio.sleep(0)                # the editor ticks between steps
    return {"done": len(paths)}
vibeue.start_job(rebuild, "rebuild")          # returns at once; the job keeps running after this call
# any later call (or `python scripts/vibeue_client.py job rebuild` from a shell):
vibeue.job_status("rebuild")                  # status running/done/failed/cancelled, progress, result, error
r = await vibeue.wait_idle(45)                # no slow task, no modal, no shader jobs, no asset compiling
```

A job runs on the editor's own asyncio loop, which the engine polls every frame while it has work, so it
advances between calls (a 3-minute job started from a 0.5 s call finished and was read afterwards). Its state
is also written to `Saved/VibeUE/Jobs/<key>.json`; after an editor restart `job_status` reads that file and
says `lost` for a job that was still running. A job runs only while it awaits: never `time.sleep` in one.
`cancel_job(key)`, `jobs()`, `await wait_job(key, timeout_s)`, `editor_busy()` (the counters `wait_idle`
reads) complete the set.

**From a shell, or a session whose MCP tools never connected:** `scripts/vibeue_client.py` (standard library
only) handles the session, runs `py <file>`, `exec "<code>"`, `call <toolset> <tool> '<json>'` and `job
<key>`, retries a call refused for a slow task (it made no change) until `--busy-wait` runs out, and stops
with exit code 3 and the dialog's title on a modal instead of looping. Pass `--project <project folder>` so
it reads the health file above. Exit codes: 0 ok, 1 the call failed, 2 still busy, 3 modal, 4 no
connection, 5 no reply within `--timeout` (do not resend: check `job_status` or the Output Log).

## 4. Levels: resident maps and unsaved changes

A map loaded as an **asset** (`unreal.load_asset` on a map, a Save-As, a Level Instance) stays resident,
and the engine's check on the NEXT level load is fatal (`World Memory Leaks`). `collect_garbage()` alone
does not release it; `unreal.EditorLoadingAndSavingUtils.unload_packages([unreal.find_package(path)])` does,
once no Python variable holds it (`vibeue.python_globals_holding(path)`, `vibeue.release_globals(names)`);
failing that, restart the editor. Read map metadata through the asset registry instead
(`unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path("/Game/Maps", recursive=True)`).

A host editor that releases such maps itself before every map load says so with the console variable
`VibeUE.HostReleasesResidentMaps` (off by default; `unreal.PythonTools.host_releases_resident_maps()` reads
it). `safe_load_level` then skips its two resident-map refusals and checks only that the target exists.

Change level with **`vibeue.safe_load_level(path)`**:

- It raises `vibeue.LevelLoadRefused`, loading nothing, while any map other than the open level is resident
  (`unreal.PythonTools.get_resident_maps()`), when the target itself is resident (unless
  `allow_resident_target=True`), or when no map exists at the path.
- `load_level` refuses while the open level has unsaved changes ("There are unsaved level changes in
  /Temp/..."). 5.9 added `unreal.LevelEditorUnsavedChangesPolicy` (`FAIL`, `SAVE`, `DISCARD`) as
  `load_level`'s second argument. `safe_load_level` (with `discard_temp=True`, the default) passes
  `DISCARD` when the open level is a never-saved `/Temp/` level (a turnaround, preview or scratch world)
  and no real map is dirty. A dirty real map is never discarded: save it, or pass the policy yourself if
  losing the changes is intended.

To drop unsaved changes of the OPEN map, reload it with **`vibeue.safe_reload_level(confirm_discard=True)`**,
never `reload_packages()` on the open map's package (that crashed an editor with "World Memory Leaks" while
Python still held one of the map's actors). It raises `vibeue.LevelReloadRefused`, changing nothing, while
PIE runs, for a never-saved `/Temp/` level, for a dirty map without `confirm_discard=True`, while another map
is resident, and when the undo history holds edits a PERSON made to this map that are not saved (the
`user_edits` of `editor_state()`; the message names them; `force=True` overrides, so ask first). Otherwise it
deletes the calling script's, `__main__`'s and `builtins`' variables that hold the map's objects, purges the
references other modules keep (`unreal.purge_object_references`: those variables then raise "ObjectInstance
is null" instead of pinning the old world), collects garbage and reloads with `load_level(map, DISCARD)`. The
reply lists `discarded`, `released`, `purged`, `resident_maps` and `level_dirty`.
`vibeue.python_holders_of_level()` lists those variables without reloading.

Do not hold level objects in variables across an `await` or a level change; fetch them again.
`vibeue.python_globals_holding(asset)` finds a global pinning an asset and `vibeue.release_globals(names)`
drops it.

## 5. Engine toolsets that replaced old VibeUE tools

| Need | Tool (full toolset name) |
|---|---|
| Start / stop / query PIE | `EditorToolset.EditorAppToolset` -> `StartPIE` / `StopPIE` / `IsPIERunning` |
| Capture the game (with UMG) / the editor viewport / the editor window | `SceneCaptureToolset.SceneCaptureToolset` -> `CaptureGameViewport` / `CaptureEditorViewport`, `CaptureActiveEditorView`; `EditorToolset.EditorAppToolset` -> `CaptureEditorImage`, `CaptureAssetImage` |
| Click, drag, type in a running UI | `SlateInspectorToolset.SlateInspectorToolset` (see `pie-testing` / `umg-widgets`) |
| Logs | `ConsoleToolset.LogsToolset` -> `GetLogEntries` (pass a `pattern` or `logCategory`) |
| Console command | `ConsoleToolset.ConsoleToolset` -> `RunConsoleCommand` |
| Assets: find, move, references, dependencies, redirectors | `EditorToolset.AssetToolset` |
| Materials and instances | `EditorToolset.MaterialToolset`, `EditorToolset.MaterialInstanceToolset` (VibeUE's `MaterialService` is deleted) |
| Any object property | `EditorToolset.ObjectToolset` -> `GetProperties` / `SetProperties` |
| Blueprint variables, interfaces, compile, graph DSL | `BlueprintToolsets.BlueprintToolset`, `BlueprintToolsets.BlueprintDslToolset` |
| Automation tests | `AutomationTestToolset.AutomationTestToolset` -> `DiscoverTests`, `ListTests`, `RunTests`, `GetTestStatus`, `GetTestResults`, `StopTests` |
| Frame timings | `PerfToolset.StatsToolset` -> `GetFrameTimings` |
| Files under `/Saved` | `FileSystemToolset.FileSystemToolset` |
| Which assets a change touched | `EditorToolset.UndoHistoryToolset` |
| Gameplay tags, GAS inspection | `GameplayTagsToolset.GameplayTagsToolset`, `GASToolsets.*` |
| Input in PIE (bounded press or hold) | `InputToolset.InputToolset` -> `ApplyInput` |

VibeUE keeps only what those lack (`get_symbol(name="<Name>Service")` lists each service's methods):
`PerformanceService` (`analyse`, `force_hitch`, standalone capture, `set_background_throttling`),
`WorkflowService.get_environment` (engine version, build configuration, toolchain, build staleness),
`PIEActorService` (spawn into and call functions in a running PIE world), `InputService` holds,
`FabPreviewService` (Fab listing pictures and installed packs as one numbered contact-sheet image), and the
domain services the packs above document. The run journal, scenarios and bulk maintenance are gone:
compose them from the tools above and keep your own record (`FileSystemToolset`).

## 6. `vibeue` helpers (`import vibeue`)

Each has a docstring with an example (`help(vibeue.pie_session)`).

```python
import unreal, vibeue

async with vibeue.pie_session(1280, 720) as s:            # throttling off, window size, StartPIE,
    shot = await vibeue.capture_game("C:/P/Saved/a.png")  # wait for the player; all restored on exit
w = vibeue.wait_for(lambda: some_condition(), timeout_s=10, description="seated")   # across calls
before = vibeue.dirty_snapshot(); ...; vibeue.dirty_delta(before)
with vibeue.viewport_state(): unreal.ViewportService.set_exposure(True, 11.5)       # restored on exit
with vibeue.gate_session(clients=3, net_mode="ListenServer"): ...                    # PIE settings restored
{"shot": shot}
```

- `pie_session` / `capture_game` / `game_world` / `pie_worlds`: PIE setup, capture to PNG, the PIE world(s)
  by role. Details in `pie-testing`.
- `capture_poses` / `contact_sheet` / `PoseCamera`: the running game from named camera poses at a fixed
  size (1920x1080 from any window), buffer views, one labelled sheet; view target, HUD and cvars
  restored. Details in `pie-testing`.
- `measure_ab` / `measure_ab_jobs`: GPU and game-thread cost of a change, states alternated inside one
  PIE session, split across calls, first state always restored. Details in `profiling`.
- `wait_for` / `wait_for_players` / `waits` / `cancel_all_waits`: waits that survive across calls (a Slate
  tick callback with a wall-clock deadline). Inside ONE call, `await asyncio.sleep(...)` is simpler.
- `editor_state` / `recent_transactions` / `preserve_editor`: the session's first call, who made each undo
  entry, and restoring the person's camera, throttling, PIE settings and selection. Section 1.
- `start_job` / `job_status` / `jobs` / `cancel_job` / `wait_job` / `job_progress`, and `wait_idle` /
  `editor_busy`: work that outlives a call, and waiting for compiles. Section 3.
- `safe_load_level`, `safe_reload_level`, `python_holders_of_level`, `python_globals_holding`,
  `release_globals`: section 4.
- `set_prop_pie(obj, name, value)` writes without rerunning a live PIE actor's construction script;
  `get_prop(obj, name, default)` returns `default` on a protected property; `set_array_prop` copies, sets
  with notify and calls `modify()` (an in-place `append` marks nothing dirty).
- `mapping_context_priority(subsystem, ctx)` returns the priority or None (priority 0 is common, so
  `bool()` of `has_mapping_context` is wrong). `canvas_slot` / `set_canvas_slot` edit a design-time
  `CanvasPanelSlot`.
- `exec_tool(toolset, tool, args)` / `exec_tool_await` call a tool from code that has no `call_tool` (a
  module, a tick callback): they fill missing optional arguments from the schema and decode the result.
  In a script, prefer `await call_tool(...)`.

## 7. Facts that cost a jam hours

- **A second, headless editor can write assets while the GUI editor is open.** A scripted asset write from
  `UnrealEditor-Cmd.exe <project> -ExecCmds="py <script>"` saved in 0.7 s beside a running GUI editor.
  Give it its own `-ModelContextProtocolPort=<port>` (both editors default to 8000, and the loser of that
  fight runs with no MCP), never start it while the GUI editor is still starting, and never let both
  editors write the same package: each keeps its own in-memory copy and the last save wins. A C++ build
  in a second checkout of the project needs `-NoHotReloadFromIDE`, or it tries to hot-reload into the
  running editor. Use `-ExecCmds="py ..."` with a script that ends with
  `unreal.SystemLibrary.quit_editor()`; `-ExecutePythonScript` quits as soon as the script returns.
- **Wasm (web) cooks and the editor AI plugins.** Give every editor-only AI plugin (VibeUE, the
  toolsets, `ModelContextProtocol`, `ToolsetRegistry`) `"PlatformDenyList": ["Wasm"]` in the `.uproject`.
  That is not enough: `ToolsetRegistry` makes manifest packages in memory when the cook starts, and they
  fail the Wasm cook ("imports class /Script/ToolsetRegistry.ToolsetRegistryManifest, which is in a
  module that is not available on the platform"). Add
  `+DirectoriesToNeverCook=(Path="/ToolsetRegistry/ToolsetManifestSeed")` under
  `[/Script/UnrealEd.ProjectPackagingSettings]` in `DefaultGame.ini`. A plugin ROOT cannot be never-cook
  (`/ToolsetRegistry` fails with "Registered the path ... for two different domains"); list a folder
  inside it.
- **Helper modules must do nothing at import.** A module you `import` from `execute_python` stays loaded
  and is imported again by other scripts. An export script that ran its export and called `quit_editor()`
  at module level closed the user's GUI editor when another script imported one of its functions. Put
  actions in functions; guard a script's own run with `if __name__ == "__main__":`. An `import` never
  runs that block. `runpy.run_path(path)` does not either (its default `__name__` is `"<run_path>"`):
  pass `run_name="__main__"` when you do want the file's own run. And never `time.sleep` in a helper:
  it freezes the game thread and PIE.

## 8. Other gotchas

- A Python traceback proves the MCP link is up; only "Unable to connect" / ECONNREFUSED means it is down.
  A stuck call is usually a slow task or a modal: read the health file (section 3). The client timeout
  does not stop a script, and nothing records an abandoned run, so check the Output Log before re-running
  a mutation; for work that may run long, use `vibeue.start_job` (section 3), whose status you can read.
- `-run=pythonscript` (a commandlet) works for pure asset work when no editor is running, but not for
  World Partition world edits.
- Do not change the viewport FOV or exposure without restoring it (`vibeue.viewport_state()`): those
  persist into the user's editor config.
- A hand-registered `register_slate_post_tick_callback` whose handle you lost ticks until the editor
  restarts; `vibeue.wait_for` cleans up after itself and `vibeue.cancel_all_waits()` ends the rest.
- `deep_research` and `terrain_data` run off the game thread: four at a time, more wait their turn, past 16
  a call is refused with `BUSY`. If Jina Reader refuses your network (HTTP 401), set `JINA_API_KEY` and
  restart the editor.
