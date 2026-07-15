<div align="center">

# VibeUE — AI-Powered Unreal Engine Development

### 🧩 MCP Expansion + AI Editor Toolset for Unreal Engine 5.8+

https://www.vibeue.com/

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.8%2B-orange)](https://www.unrealengine.com)
[![MCP](https://img.shields.io/badge/MCP-Model%20Context%20Protocol-blue)](https://modelcontextprotocol.io)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Discord](https://img.shields.io/badge/Discord-Join-5865F2?logo=discord&logoColor=white)](https://discord.gg/hZs73ST59a)
[![Donate](https://img.shields.io/badge/Donate-vibeue.com-ff5e5b?logo=githubsponsors&logoColor=white)](https://www.vibeue.com/donate)

</div>

**VibeUE is the MCP Expansion + AI Editor Toolset for Unreal Engine 5.8+.** Unreal 5.8 added a
built-in MCP server and AI toolsets; VibeUE is an **MCP Expansion** that plugs straight into them and
adds a deep **AI Editor Toolset** — a library of editor capabilities — Blueprints, materials, landscape, foliage, animation, Niagara, UMG, audio, StateTree,
gameplay tags, input, UVs, **performance/profiling**, and more — registered into the engine's own
`ToolsetRegistry` and `ModelContextProtocol` server, plus rich domain **skills** served through
Unreal's native `AgentSkill` system. Any MCP-capable agent (Claude Code, Cursor, Copilot, …) drives
your editor through Unreal's standard MCP endpoint.

> ⚠️ **VibeUE requires Unreal's native MCP to be set up first** — enable the **Unreal MCP** plugin
> (which auto-enables **Toolset Registry**) and the **Editor Tools** plugin, then start the MCP server.
> Follow Epic's guide: **[Unreal MCP in the Unreal Editor](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)**.
> VibeUE then expands that endpoint — **no separate server and no in-editor chat.** A free API key
> (set in **Editor Preferences → Plugins → VibeUE**) unlocks the real-world **terrain** tools; everything
> else works without one.

---

## ✨ What VibeUE adds

Unreal 5.8 ships its own AI toolsets (Blueprints, materials, actors, assets, meshes, data tables, …).
VibeUE **complements** them — it focuses on the domains and depth the engine doesn't cover:

- **Terrain & world** — Landscape sculpting/heightmaps/splines, landscape auto-materials + RVT,
  Foliage, procedural FPS **Map Blockout**, and **real-world terrain** (heightmaps + water from GPS).
- **Audio** — MetaSound and SoundCue graph authoring.
- **Animation assets** — AnimSequence keyframe editing, AnimMontage authoring, AnimBP state machines,
  Skeleton bone/socket/retarget/blend-profile editing.
- **FX depth** — Niagara emitter color/curve authoring and **Custom-HLSL scratch-pad** modules.
- **UI** — UMG widgets with MVVM bindings, animation authoring, and preview/PIE validation.
- **Higher-order Blueprint authoring** — timelines, event dispatchers, delegates, custom-event pins,
  comment boxes, and a batch `build_graph` builder.
- **Editor safety** — `TransactionService` wraps the editor's transaction buffer (undo / redo /
  checkpoints) so an agent can group and roll back its own edits — the engine's toolsets expose none.
- **⚡ Performance & profiling** — VibeUE's standout: see the dedicated section below.
- **Python-first access** — run any `unreal.*` Python in the editor and introspect the whole API.
- **Web research** — search / fetch / geocode for in-context research and terrain workflows.

It deliberately **does not duplicate** the engine's general tools (basic asset/actor/blueprint/material
CRUD, screenshots, logs, PIE) — agents use Unreal's native toolsets for those.

---

## ⚡ Performance & Profiling (flagship)

**Unreal's native AI toolsets have *zero* performance tooling** — they can start PIE/Simulate but can't
measure anything. VibeUE's `PerformanceService` fills that gap so an agent can actually *diagnose and
fix* frame rate:

- **`frame_timing()`** — Game/Render/GPU/RHI thread split + a **CPU-vs-GPU-bound verdict** and a
  concrete next-step hint. *Run this first* — optimising the GPU does nothing on a CPU-bound frame.
- **Unreal Insights capture** — `start_trace` / `stop_trace` / `get_trace_status`, with `bookmark`
  and `region_start` / `region_end` markers.
- **`analyse()`** — reads the trace **and** log back and returns a perf summary (frame stats, worst
  frames, hitches, notable log lines).
- **Trace-attached `start_standalone`** — profile a representative standalone build, not just the
  editor viewport.

```python
import unreal
print(unreal.PerformanceService.frame_timing())   # CPU vs GPU bound — diagnose FIRST
unreal.PerformanceService.start_trace("cap", "")   # Insights trace
# … reproduce the workload (ideally under PIE / standalone) …
unreal.PerformanceService.stop_trace()
print(unreal.PerformanceService.analyse("both", ""))
```

Pair with the `profiling` and `frame-rate` skills for the full CPU/GPU drill-down.

---

## 🏗️ Architecture

VibeUE plugs into three native UE 5.8+ systems:

1. **Toolsets** (`ToolsetRegistry`) — VibeUE's services register as `UToolsetDefinition`s, so their
   methods become AICallable tools on the MCP endpoint. They're also `BlueprintCallable`, so the same
   methods are callable from Python as `unreal.<Name>Service.<method>()`.
2. **MCP server** (`ModelContextProtocol`) — a small set of VibeUE utility tools are registered
   directly on the endpoint: `execute_python_code`, `discover_python_module`/`_class`/`_function`,
   `list_python_subsystems`, `deep_research`, `terrain_data`.
3. **Skills** (`AgentSkillToolset`) — ~34 markdown skill packs register as native `UAgentSkill`s,
   discoverable via `ListSkills` and loaded lazily via `GetSkills`, alongside the engine's own skills.

**Efficient usage (for agents):** `execute_python_code` is the workhorse — it batches a whole
multi-step task into one round-trip and reaches every VibeUE service plus the full `unreal.*` API. Use
`call_tool` only for skills and the few engine toolsets with no Python path (screenshots, etc.). See
[`Content/samples/AGENTS.md.sample`](Content/samples/AGENTS.md.sample) for the full agent guide.

---

## 🚀 Installation & setup

**Requirements:** Unreal Engine **5.8+** · Git

### Step 1 — Set up Unreal's native MCP (do this FIRST)

VibeUE is an **expansion** of Unreal's built-in MCP support, so enable that first. Full details in
Epic's guide: **[Unreal MCP in the Unreal Editor](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)**.

1. **Edit → Plugins** → enable **Unreal MCP** (this auto-enables **Toolset Registry**) and **Editor
   Tools** (the engine's own AI toolsets, so agents get both). These are Experimental. Restart when prompted.
2. **Edit → Editor Preferences → General → Model Context Protocol** → enable **Auto Start Server**
   (or run the console command `ModelContextProtocol.StartServer`). Default endpoint
   `http://127.0.0.1:8000/mcp` (port/path configurable). Enabling **Tool Search** keeps an agent's
   context small — it sees `list_toolsets` / `describe_toolset` / `call_tool` and loads tools on demand.

### Step 2 — Install VibeUE

```bash
cd /path/to/YourProject/Plugins
git clone https://github.com/kevinpbuckley/VibeUE.git
```
Build with the project script (don't run `Build.bat` directly):
```
Plugins/VibeUE/BuildAndLaunchGame.ps1                  # builds + launches the editor
Plugins/VibeUE/BuildAndLaunchGame.ps1 -StrictRebuild   # full recompile (warnings-as-errors)
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
(Gemini), `AGENTS.md` (Codex / Cursor), or `.github/copilot-instructions.md` (Copilot) — or pass
`All` to write CLAUDE.md + GEMINI.md + AGENTS.md at once. It resolves the plugin location
automatically, so it works whether VibeUE was installed from **FAB** or **Git**. The guide goes in a
managed block, so re-run any time to refresh without disturbing your own notes. Pass `import` to link
the guide with a one-line `@import` instead of copying it (Claude Code / Gemini only — other agents
don't resolve imports, so they always get a copy).

> The MCP server is loopback-only with no authentication — same-machine use only (per Epic's docs).

The guide teaches: discover before you call (`discover_python_class`), batch with `execute_python_code`,
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
          arguments={"skillPaths": ["/VibeUE/Python/init_unreal_PY.VibeUE_blueprints"]})
```

`ListSkills` is the **live source of truth** for what's available (it reads each pack's `SKILL.md`).
Skills tell you *what* to do and *why*; use `discover_python_class('unreal.<Name>Service', method_filter='…')`
for exact signatures before writing code.

---

## 🔧 Plugin dependencies

**Native engine prerequisites (enable in Step 1 — Epic's MCP stack):**

| Plugin | Purpose |
|--------|---------|
| **Unreal MCP** (`ModelContextProtocol`) | The native MCP server endpoint |
| **Toolset Registry** (`ToolsetRegistry`) | Native AI toolset + `AgentSkill` registration (auto-enabled by Unreal MCP) |
| **Editor Tools** (`EditorToolset`) | The engine's own AI toolsets — VibeUE complements these |

**Enabled automatically by VibeUE:** `PythonScriptPlugin` (the `unreal.*` API),
`EditorScriptingUtilities`, and the domain plugins its services need — `Niagara`, `MetaSound`,
`EnhancedInput`, `ModelViewViewModel`, `StateTree`, `MeshModelingToolset`,
`GameplayTagsEditor`. (VibeUE also depends on
`ToolsetRegistry` + `ModelContextProtocol`, so enabling VibeUE pulls them in — but you still enable
**Editor Tools** and start the server per Step 1.)

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
- **`discover_python_class('unreal.<Name>Service')`** → exact method signatures.
- **`describe_toolset('VibeUE.<Name>Service')`** → the toolset's tools + JSON schemas (token-heavy;
  prefer skills + discovery).

---

## License

MIT — see [LICENSE](LICENSE). Project home: https://www.vibeue.com/
