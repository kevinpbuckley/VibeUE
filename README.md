<div align="center">

# VibeUE — AI-Powered Unreal Engine Development

### 🧩 MCP Expansion + AI Editor Toolset for Unreal Engine 5.9+

https://www.vibeue.com/

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.9%2B-orange)](https://www.unrealengine.com)
[![MCP](https://img.shields.io/badge/MCP-Model%20Context%20Protocol-blue)](https://modelcontextprotocol.io)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?logo=discord&logoColor=white)](https://discord.gg/hZs73ST59a)
[![Donate](https://img.shields.io/badge/Donate-vibeue.com-ff5e5b?logo=githubsponsors&logoColor=white)](https://www.vibeue.com/donate)

</div>

**VibeUE is the MCP Expansion + AI Editor Toolset for Unreal Engine 5.9+.** Unreal 5.8 added a
built-in MCP server and AI toolsets; VibeUE is an **MCP Expansion** that plugs straight into them and
adds a deep **AI Editor Toolset** — a library of editor capabilities the engine's own toolsets
don't reach: landscape, foliage, animation, Niagara, StateTree bindings, enhanced input, UVs,
**performance/profiling**, real-world terrain, and more — registered into the engine's own
`ToolsetRegistry` and `ModelContextProtocol` server, plus rich domain **skills** served through
Unreal's native `AgentSkill` system. Any MCP-capable agent (Claude Code, Cursor, Copilot, …) drives
your editor through Unreal's standard MCP endpoint.

> ⚠️ **VibeUE requires Unreal's native MCP to be set up first** — enable the **Unreal MCP** plugin
> (which auto-enables **Toolset Registry**), then start the MCP server. Enabling VibeUE pulls in
> **Editor Tools** and the other engine toolsets its skills use.
> Follow Epic's guide: **[Unreal MCP in the Unreal Editor](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)**.
> VibeUE then expands that endpoint — **no separate server and no in-editor chat.** A free API key
> (set in **Editor Preferences → Plugins → VibeUE**) unlocks the real-world **terrain** tools; everything
> else works without one.

---

## ✨ What VibeUE adds

Unreal 5.9 ships its own AI toolsets (Blueprints, materials, actors, assets, meshes, data tables, …).
VibeUE **complements** them — it focuses on the domains and depth the engine doesn't cover:

- **Terrain & world** — Landscape sculpting/heightmaps/splines, landscape auto-materials + RVT,
  Foliage, procedural FPS **Map Blockout**, and **real-world terrain** (heightmaps + water from GPS).
- **Audio** — SoundCue graph authoring.
- **Animation assets** — AnimSequence keyframe editing, AnimMontage authoring, AnimBP state machines,
  Skeleton bone/socket/retarget/blend-profile editing.
- **FX depth** — Niagara emitter colour/curve authoring and **Custom-HLSL scratch-pad** modules.
- **UI** — the UMG work Epic's `WidgetStaticAnalysisToolset` / `WidgetAnimationToolset` / `MVVMToolset`
  don't reach. Those own widget inspection, animation and view-model binding now; VibeUE keeps the
  remainder.
- **Higher-order Blueprint authoring** — timelines, event dispatchers, delegates, custom-event pins
  and comment boxes. Variables, components, interfaces, compile and node CRUD are Epic's
  `BlueprintToolsets.BlueprintToolset`.
- **AI behavior repair** -- Behavior Tree and EQS authoring is Epic's `AIModuleToolset`; VibeUE adds
  only what it lacks: rebuilding a BT or EQS graph that has gone sparse while the runtime data is
  intact, renaming a blackboard key with BT selector fix-ups, sweeping a key's references, and
  **running** an EQS query (Epic's EQS tools author and inspect, but never run).
- **StateTree** — Epic's `StateTreeToolset` owns states, tasks, conditions and node data. VibeUE keeps
  property **binding** (24 tools), asset creation, node-type discovery, theme colours and root
  parameters — see `Skills/state-trees/`.
- **Editor safety** -- `TransactionService` exposes the undo/redo *stepping* (undo / redo /
  multiple / can-undo / reset) that neither Python nor the engine toolsets expose; history reading is
  Epic's `UndoHistoryToolset` and scoping is `unreal.ScopedEditorTransaction`.
- **Build manifest** — `WorkflowService.get_environment` reports engine version, build config and
  toolchain in one authoritative call.
- **Performance & profiling** — the parts Epic's 5.9 profiling toolsets still don't cover: see below.
- **Web research** — search / fetch / geocode for in-context research and terrain workflows.

It deliberately **does not duplicate** the engine's general tools (basic asset/actor/blueprint/material
CRUD, screenshots, logs, PIE) — agents use Unreal's native toolsets for those.

---

## ⚡ Performance & Profiling

UE 5.9 added real profiling toolsets of its own — `PerfToolset` (`StatsToolset.GetFrameTimings`),
`CsvProfilerToolset`, `GpuProfilerToolset` and `RewindDebuggerToolset` — so start there:

```
call_tool GetFrameTimings {}        # FrameMs + Game/Render/RHI thread + GPU split
```

The verdict is arithmetic you do yourself: the largest of the thread/GPU figures gates the frame and
should sit near `FrameMs`. **If none of them comes near `FrameMs`, that is the signature of a cap**
rather than a bottleneck — see `Skills/frame-rate/`.

`PerformanceService` now covers only what that surface still can't do:

- **`analyse()`** — reads a finished `.utrace` **and** the log back and returns a perf summary (frame
  stats, worst frames, hitches, notable log lines). Nothing in Epic's surface parses a trace file.
  Capture the trace with the engine's own `Trace.*` console commands via
  `ConsoleToolset.RunConsoleCommand`.
- **Trace-attached `start_standalone` / `stop_standalone` / `get_standalone_status`** — profile a
  representative out-of-process standalone build. Epic's `EditorAppToolset.StartPIE` downgrades
  standalone play modes to in-viewport PIE, which is not what you want to measure.
- **`set_background_throttling()`** — registers the `ShouldDisableCPUThrottlingDelegates` override.
  Clearing `bThrottleCPUWhenNotForeground` is not enough: `UEditorEngine::ShouldThrottleCPUUsage`
  falls through to `AreAllWindowsHidden()`, so a minimized editor throttles anyway. This is the one
  that matters for unattended runs.
- **`force_hitch()`** — inject a deliberate stall to validate that your capture and analysis pipeline
  actually catches one.

```python
import unreal
print(unreal.PerformanceService.analyse("both", ""))
```

---

## 🏗️ Architecture

VibeUE plugs into three native UE 5.9+ systems:

1. **Toolsets** (`ToolsetRegistry`) — VibeUE's services register as `UToolsetDefinition`s, so their
   methods become AICallable tools on the MCP endpoint. They're also `BlueprintCallable`, so the same
   methods are callable from Python as `unreal.<Name>Service.<method>()`.
2. **MCP server** (`ModelContextProtocol`) — only two VibeUE utility tools are registered directly on
   the endpoint, both things the engine has no tool for: `deep_research` and `terrain_data`. Python
   execution and API discovery are engine-native in 5.9 (`editor_toolset.toolsets.python_scripting.PythonScriptingTools`:
   `execute_python`, `read_me`, `search_symbols`, `get_symbol`), so VibeUE no longer ships its own.
3. **Skills** (`AgentSkillToolset`) — 32 markdown skill packs under `Skills/`, discovered natively by
   5.9 (`<Plugin>/Skills/**/SKILL.md`), listed by `ListSkills` and loaded lazily via `GetSkills`,
   alongside the engine's own skills.

**Efficient usage (for agents):** the engine's `execute_python` is the workhorse — it batches a whole
multi-step task into one round-trip and reaches every VibeUE service plus the full `unreal.*` API. Use
`call_tool` only for skills and the few engine toolsets with no Python path (screenshots, etc.). See
[`Content/samples/AGENTS.md.sample`](Content/samples/AGENTS.md.sample) for the full agent guide.

---

## 🚀 Installation & setup

**Requirements:** Unreal Engine **5.9+** · Git

### Step 1 — Set up Unreal's native MCP (do this FIRST)

VibeUE is an **expansion** of Unreal's built-in MCP support, so enable that first. Full details in
Epic's guide: **[Unreal MCP in the Unreal Editor](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)**.

1. **Edit → Plugins** → enable **Unreal MCP** (this auto-enables **Toolset Registry**). It is
   Experimental. Restart when prompted. You do **not** need to hunt down **Editor Tools** or the other
   engine toolsets by hand: VibeUE declares the eighteen its skills route to, so enabling VibeUE in
   Step 2 switches them on. None of them is enabled by default on a stock engine, which is exactly
   why VibeUE declares them.
2. **Edit → Editor Preferences → General → Model Context Protocol** → enable **Auto Start Server**
   (or run the console command `ModelContextProtocol.StartServer`). Default endpoint
   `http://127.0.0.1:8000/mcp` (port/path configurable). Enabling **Tool Search** keeps an agent's
   context small — it sees `list_toolsets` / `describe_toolset` / `call_tool` and loads tools on demand.

### Step 2 — Install VibeUE

```bash
cd /path/to/YourProject/Plugins
git clone https://github.com/kevinpbuckley/VibeUE.git
```
Build with the project script:
```
Plugins/VibeUE/BuildAndLaunchGame.ps1                  # builds + launches the editor
Plugins/VibeUE/BuildAndLaunchGame.ps1 -StrictRebuild   # full recompile (warnings-as-errors)
Plugins/VibeUE/BuildAndLaunchGame.ps1 -Map /Game/Maps/MyMap   # open a specific map (issue #554)
```
On Linux or macOS, use the platform-detecting shell script:
```bash
Plugins/VibeUE/BuildAndLaunchGame.sh --engine /path/to/UE5
Plugins/VibeUE/BuildAndLaunchGame.sh --engine /path/to/UE5 --strict-rebuild
Plugins/VibeUE/BuildAndLaunchGame.sh --engine /path/to/UE5 --map /Game/Maps/MyMap
```
Then **Edit → Plugins** → enable **VibeUE** and restart. Its services, tools, and skills now register
onto the same endpoint, alongside the engine's own.

### Step 3 — Connect your agent

Two console commands from the editor (open the console with `` ` ``):

**1. Write the MCP server config** (`.mcp.json` at the project root):
```
ModelContextProtocol.GenerateClientConfig ClaudeCode
```
(supports `ClaudeCode`, `Cursor`, `VSCode`, `Gemini`, `Codex`, or `All`.)

**2. Write VibeUE's agent guide** so the assistant uses the efficient patterns:
```
VibeUE.GenerateAgentConfig ClaudeCode
```
This writes the guide to the correct file for your agent — `CLAUDE.md` (Claude Code), `GEMINI.md`
(Gemini), `AGENTS.md` (Codex / Hermes / Cursor), or `.github/copilot-instructions.md` (Copilot) — or pass
`All` to write CLAUDE.md + GEMINI.md + AGENTS.md at once. It resolves the plugin location
automatically, so it works whether VibeUE was installed from **FAB** or **Git**. The guide goes in a
managed block, so re-run any time to refresh without disturbing your own notes. Pass `import` to link
the guide with a one-line `@import` instead of copying it (Claude Code / Gemini only — other agents
don't resolve imports, so they always get a copy).

> The MCP server is loopback-only with no authentication — same-machine use only (per Epic's docs).

The guide teaches: discover before you call (`get_symbol` / `search_symbols`), batch with `execute_python`,
load skills via `ListSkills`/`GetSkills`, and when to reach for `deep_research` / `terrain_data`.

---

## 🎯 Skills

Skills are lazy-loaded domain knowledge (workflows, gotchas, property formats) served by Unreal's
native `AgentSkillToolset`:

```
# discover (summaries only — cheap)
call_tool(tool_name="ListSkills", toolset_name="ToolsetRegistry.AgentSkillToolset")

# load the packs you need (full markdown, lazy)
call_tool(tool_name="GetSkills", toolset_name="ToolsetRegistry.AgentSkillToolset",
          arguments={"skillPaths": ["/Skills/VibeUE/blueprints"]})
```

`ListSkills` is the **live source of truth** for what's available (it reads each pack's `SKILL.md`).
Skills tell you *what* to do and *why*; use `get_symbol('unreal.<Name>Service')`
for exact signatures before writing code.

---

## 🔧 Plugin dependencies

**Native engine prerequisites (enable in Step 1 — Epic's MCP stack):**

| Plugin | Purpose |
|--------|---------|
| **Unreal MCP** (`ModelContextProtocol`) | The native MCP server endpoint |
| **Toolset Registry** (`ToolsetRegistry`) | Native AI toolset + `AgentSkill` registration (auto-enabled by Unreal MCP) |

**Enabled automatically by VibeUE.** Enabling VibeUE pulls in `ToolsetRegistry` and
`ModelContextProtocol`, `PythonScriptPlugin` (the `unreal.*` API), `EditorScriptingUtilities`, the
domain plugins its services need (`Niagara`, `EnhancedInput`, `ModelViewViewModel`, `StateTree`,
`MeshModelingToolset`, `GameplayTagsEditor`, `EnvironmentQueryEditor`), **and the engine toolsets
VibeUE's skills route to**:

`EditorToolset` · `BlueprintToolsets` · `ConsoleToolset` · `ConfigSettingsToolset` ·
`FileSystemToolset` · `StateTreeToolset` · `AIModuleToolset` · `NiagaraToolsets` · `MVVMToolset` ·
`WidgetStaticAnalysisToolset` · `WidgetAnimationToolset` · `SlateInspectorToolset` ·
`SceneCaptureToolset` · `PerfToolset` · `InputToolset` · `GameplayTagsToolset` ·
`AutomationTestToolset` · `GASToolsets`

> Those eighteen matter more than they look. **None of Epic's toolset plugins is enabled by
> default** — `FPlugin::IsEnabledByDefault` disables an engine plugin whose `EnabledByDefault` is
> absent, not just one that sets it to `false` — and VibeUE's skills now deliberately send you to
> them for everything the engine does better. Without them enabled, those calls just are not there.
> They are declared `Optional`, so a future engine version renaming or dropping an experimental
> toolset cannot stop VibeUE loading; if one is missing you simply lose that domain's engine tools.
>
> A handful of toolsets the skills mention only in passing (`RewindDebuggerToolset`,
> `CommonUIToolset`, `NavMeshToolset`, `PCGToolset`, `PluginToolset`, `GameFeaturesToolset`,
> `MediaToolset`, `ChaosClothAssetToolset`, `LiveCodingToolset`) are **not** declared, to avoid
> switching on a pile of experimental plugins nobody asked for. Enable those yourself if you want
> them.

---

## 🛠️ Build & launch script

`BuildAndLaunchGame.ps1` (project root or `Plugins/VibeUE/`) stops the running editor, builds, and
relaunches:
- `-StrictRebuild` — full plugin recompile under warnings-as-errors
- `-Clean` — wipe intermediate/binaries first
- `-SkipBuild` — relaunch only

---

## 📚 The live API

VibeUE intentionally keeps **no static method catalog** in this README — the surface evolves with the
engine. The authoritative, always-current references are:
- **`ListSkills`** → which domains exist and when to use them.
- **`get_symbol('unreal.<Name>Service')`** → exact method signatures (engine-native).
- **`describe_toolset('VibeUE.<Name>Service')`** → the toolset's tools + JSON schemas (token-heavy;
  prefer skills + discovery).

---

## License

MIT — see [LICENSE](LICENSE). Project home: https://www.vibeue.com/
