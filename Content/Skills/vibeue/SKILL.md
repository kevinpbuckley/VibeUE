---
name: vibeue
description: Unreal Engine 5 development using the VibeUE Python API. Use when working in Unreal Engine â€” blueprints, state trees, materials, actors, landscapes, animation, niagara, widgets, sound, foliage, gameplay tags, enhanced input, skeletons, PCG (procedural content generation), and more. VibeUE is an extension of Unreal's native MCP endpoint.
---

VibeUE is an **extension on Unreal Engine's native MCP endpoint** (`http://localhost:8000/mcp`).
There is no separate VibeUE server, no API key, and no in-editor chat â€” VibeUE simply registers
extra Python services (`unreal.<Service>`) and skill packs on top of the engine's own toolsets.

## Wait for VibeUE readiness after launch

`BuildAndLaunchGame.ps1` / `.sh` print `Editor-PID=<pid>` â€” treat that as the process identity. Check
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
(issue #554) â€” **gate world-edit scripts on it**. A relaunch opens the project default map unless
you pass `-Map /Game/Maps/YourMap` (`--map` on the .sh) to the launch script; editing "the current
world" after a relaunch without checking has silently modified the wrong level before.

## Health heartbeat â€” dead or wedged editor detection

`Signals/editor-<pid>-health.json` is rewritten every ~5s by a background thread (issue #555):

```json
{"signal":"health","pid":21044,"updatedUtc":"2026-08-03T17:09:00.000Z",
 "sessionStartUtc":"2026-08-03T17:03:22.108Z","gameThreadStallSeconds":0.03,
 "mcpPort":8000,"mcpListening":true}
```

Epic's MCP endpoint runs on the game thread with no request timeout, so a dead or wedged editor
hangs MCP calls for the client's full timeout â€” and even JSON-RPC `ping` hangs with it. Read the
health file instead: file missing or `updatedUtc` older than ~15s â†’ the process is gone (relaunch);
`gameThreadStallSeconds` above ~10 â†’ alive but wedged (modal dialog / crash handler; MCP will
hang â€” relaunch); fresh and small â†’ the editor is healthy, debug something else.

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

## Persisted Python results -- a timed-out call is not a failed call

The MCP client aborts an `execute_python_code` call after its own timeout (~30s for most clients,
300s for some), but the script keeps running in the editor. **Read the persisted result before
re-running** -- re-running double-executes a mutation. Every run's outcome is written to
`Signals/python-<pid>-last.json` (always the latest) and appended to `Signals/python-<pid>-runs.jsonl`
(last ~200 runs / ~2 MB):

A run that COMPLETES but overruns the server-side timeout is no longer reported as an error: the
reply comes back `success:true` with `timed_out:true`, its `run_id`, and `signal_file_path` (the
`python-<pid>-last.json` path), so a client whose own budget is longer -- or a retry -- gets the real
output instead of a bare `PYTHON_EXECUTION_TIMEOUT`. Only a call the client actually abandoned (it
gave up while the script was still running) returns nothing to you; that is the case
`last_python_result()` below is for. Error replies now also append `run_id=<N>` and the signal path
to the message for the same recovery.

```json
{"runId":7,"pid":21044,"success":true,"label":"#7 execute_python_code","output":"...",
 "error":"","result":"","execution_time_ms":48210.5,"startedUtc":"...","finishedUtc":"..."}
```

The record deliberately does not store the submitted Python source: snippets commonly contain
credentials passed to SDKs, and timeout recovery must not create a plaintext source-code secret log.

Recovery after a timeout -- the aborted call kept running in THIS same editor process, so the next
call just reads the file:

```python
import vibeue
print(vibeue.last_python_result())      # dict of the most recent run, or None
print(vibeue.python_run(7))             # a specific run by its runId, from the JSONL history
```

`last_python_result()` sees the lost run because the read happens before this recovery call's own
result is persisted. A SECOND read reflects the recovery read itself -- grab the `runId` from the
first read and use `python_run(run_id)` for a stable handle. Successful MCP replies also carry
`run_id` so you can correlate a reply that did return with its record.

Skill packs (this file and its siblings) are loaded through the engine's `AgentSkillToolset`.
Each skill carries exact API patterns and gotchas; **load the relevant skill before writing any
code** in a domain, or you will guess wrong property names and spiral into discovery loops.

## Discover and load skills

Skills are discovered and read through the engine `AgentSkillToolset`, invoked with `call_tool`:

```
# List every available skill (full path â†’ description)
call_tool(toolset_name="ToolsetRegistry.AgentSkillToolset", tool_name="ListSkills", arguments={})

# Read one or more skills â€” GetSkills takes the FULL paths that ListSkills returns
call_tool(toolset_name="ToolsetRegistry.AgentSkillToolset", tool_name="GetSkills",
          arguments={"skillPaths": ["/VibeUE/Python/init_unreal_PY.VibeUE_pcg",
                                    "/VibeUE/Python/init_unreal_PY.VibeUE_materials"]})
```

> **Naming rule (verified live):** skill pack `<slug>` registers as `VibeUE_<slug>` (hyphens â†’
> underscores) and sub-doc `<slug>/<file>.md` as `VibeUE_<slug>__<file>`, all under the path prefix
> `/VibeUE/Python/init_unreal_PY.`. Short names such as `"pcg"` or `"state-trees/api-reference"`
> return an **empty result with no error** â€” always pass full paths, taking them from `ListSkills`
> when unsure. Run `describe_toolset` on `ToolsetRegistry.AgentSkillToolset` if the call signature
> differs in your build. The old `vibeue-skills-manager` tool no longer exists.

**Route by functional area.** Find the area whose scope matches the task, then `GetSkills` the
listed skill(s) â€” full paths per the naming rule above. The *NOT for* column is the disambiguator â€”
when two areas seem to fit, the one that *excludes* your task is telling you where to go instead.

| Functional area | Use for â€” **NOT** forâ€¦ | Load skill(s) |
|---|---|---|
| **Scene & actors** | place / move / arrange / organize / tag actors in a level â€” NOT gameplay logic (â†’ Blueprints), NOT world-scale terrain/foliage (â†’ Environment), NOT attaching a Niagara/particle component (â†’ VFX) | `level-actors` |
| **Blueprints & gameplay logic** | author Blueprint classes & graphs, Enhanced Input, gameplay tags, Gameplay Ability System (abilities/attributes/effects/cues) â€” NOT AI behavior (â†’ AI), NOT AnimBP graphs (â†’ Animation), NOT C++/source (coding-agent handoff) | `blueprints`, `blueprint-graphs`, `enhanced-input`, `gameplay-tags`, `gas` |
| **AI** | author StateTree or Behavior Tree logic, Blackboards, states, tasks, transitions, event payloads, and delegate bindings â€” NOT character body animation (â†’ Animation), NOT generic actor placement (â†’ Scene) | `state-trees`, `behavior-trees` |
| **Animation & rigging** | AnimBP state machines, AnimSequence keyframes, montages & AnimNotify wiring, bone/skeleton editing & retarget â€” NOT cinematic timelines (Epic Sequencer), NOT AI movement (â†’ AI), NOT authoring/adding sound assets â€” even a character's footstep sounds (â†’ Audio) | `animation-blueprint`, `animsequence`, `animation-editing`, `animation-montage`, `skeleton` |
| **Materials & shading** | materials, instances, graph nodes, Custom HLSL â€” NOT Niagara particle materials (â†’ VFX), NOT landscape auto-materials (â†’ Environment) | `materials` |
| **VFX (Niagara)** | particle systems, emitters, scratch-pad HLSL, attaching/placing a Niagara component on an actor â€” NOT surface materials (â†’ Materials) | `niagara-systems`, `niagara-emitters` |
| **UI (UMG)** | widget blueprints, layout, fonts/brushes, MVVM â€” NOT the gameplay behind the UI (â†’ Blueprints) | `umg-widgets` |
| **Environment (world-scale)** | landscape sculpt/paint, landscape materials, foliage, PCG, map blockout, real-world terrain â€” NOT single-actor placement (â†’ Scene), NOT sound/audio (â†’ Audio) | `landscape`, `landscape-materials`, `landscape-auto-material`, `foliage`, `pcg`, `map-blockout`, `terrain-data` |
| **Audio** | MetaSound and SoundCue authoring â€” creating the sound asset itself: ambient, a character's footstep/foley, UI sounds â€” NOT triggering sounds from gameplay logic (â†’ Blueprints), NOT wiring an existing sound to anim-notify foot-plant frames (â†’ Animation) | `metasounds`, `sound-cues` |
| **Assets, data & project** | import/export assets, Fab catalog acquisition, UV mapping, enums/structs, engine & project settings, and bounded bulk asset maintenance/migrations â€” NOT actors placed in a level (â†’ Scene) | `asset-management`, `fab`, `bulk-maintenance`, `uv-mapping`, `enum-struct`, `engine-settings`, `project-settings` |
| **Diagnostics, testing & run** | start/stop/query PIE, profile (CPU-vs-GPU / Insights), uncap frame rate â€” NOT fixing the logic a bug points to (â†’ its authoring area) | `pie-testing`, `profiling`, `frame-rate` |
| **Camera & viewport** | viewport camera, view mode, FOV, exposure, layout â€” NOT material look (â†’ Materials), NOT placing/editing light actors (â†’ Scene) | `viewport` |
| **Cinematics Â· Physics** | Sequencer cinematics and Physics assets (ragdoll/skeletal) are **Epic-native** â€” VibeUE adds no skill here â€” NOT enabling simulate-physics on a level actor (â†’ Scene) | *(none â€” use `list_toolsets`: `animation_toolset.*` / `PhysicsToolsets`)* |

A loaded skill gives you:
- workflows, gotchas, and property formats for the domain
- `vibeue_classes` / `unreal_classes` â€” class names to feed into `discover_python_class` for live method signatures
- sub-doc references (`<skill>/<section>`) you can fetch via `GetSkills` for deeper detail

Always call `discover_python_class` on the classes in `vibeue_classes` before writing code â€” never
guess method names from the skill content alone. **Batch the discovery into ONE call** instead of
one call per class:

```
# ONE call covers all classes and all topics:
discover_python_class(
    class_name="unreal.MaterialService, unreal.WidgetService, unreal.MaterialNodeService",
    method_filter="create|delete|compile|property|color")

# WRONG â€” three separate calls for three classes wastes round-trips and repeats boilerplate
```

`class_name` accepts a comma-separated list (response gains a `classes` array, one entry per class);
`method_filter` ORs keywords with `|`.

## How work gets done â€” `execute_python_code` is the workhorse

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

**`auto_save` (default true).** Before running your script, `execute_python_code` saves every dirty
content AND world package headlessly (issue #433: this avoids the modal save dialog that would hang
the call). Every reply reports what happened: `auto_save` (the flag in effect) and `saved_packages`
(the list of package names written). Pass `auto_save=false` to run the script WITHOUT that sweep --
use it when you do not want in-flight editor edits flushed to disk, or to keep a mutation you are
about to make from being interleaved with an unrelated dirty package. The sweep is skipped anyway
after a crashed run, when GEditor is missing, or in PIE.

## Tools â€” what each is for

| Tool | Use it for |
|------|-----------|
| `execute_python_code` | Run `unreal.*` Python in the editor â€” the workhorse for every VibeUE service |
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
| Start / stop / query PIE | `EditorToolset.EditorAppToolset` â†’ `StartPIE` / `StopPIE` / `IsPIERunning` |
| Capture a viewport screenshot | `EditorToolset.EditorAppToolset` â†’ `CaptureViewport` |
| List / read / filter / tail UE logs | `LogsToolset` |
| Search / open / save / move / import assets | `AssetTools` |
| Single-tag gameplay-tag CRUD | `GameplayTagsToolset` (see `gameplay-tags` skill) |
| Inspect a live Ability System (attributes/tags/effects/abilities), attribute-set discovery, gameplay cues | `GASToolsets.*` (see `gas` skill) |

Performance/Insights tracing is the one net-new VibeUE service â€” `unreal.PerformanceService.*` (see
the `profiling` skill) â€” because Unreal 5.8 ships no performance toolset.


## Additional gotchas

- A Python traceback still proves the MCP link is up — only "Unable to connect" means it is down. A dropped server or a newly added tool needs a Claude Code restart. A stuck call is usually a modal dialog and the client timeout does not stop the script, so persisted results (`vibeue.last_python_result()`) let you read the outcome on the next call rather than re-running.
- Don't run a headless `UnrealEditor-Cmd` while the GUI editor is starting — they fight over the MCP port and the loser runs with no MCP; the readiness signal's `mcpListening` flag tells you when the port is claimed. `-run=pythonscript` fully substitutes for pure-asset work when no editor runs, but not for World Partition world surgery.
- Engine toolset quirks: `LogsToolset.GetLogEntries` requires a `pattern`; `StartPIE` via `execute_tool` needs the full options object; results can be double-encoded (`vibeue.exec_tool` decodes them). A genuinely async tool like `CaptureAssetImage` never completes inside one call — fire it with `vibeue.exec_tool_async` and read it with `vibeue.collect_tool_result` on the next call.
- A leaked `register_slate_post_tick_callback` can survive its own unregister; an editor restart is the only reliable purge.
