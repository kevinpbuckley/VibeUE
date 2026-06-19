<div align="center">

# VibeUE — AI-Powered Unreal Engine Development

https://www.vibeue.com/

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.8-orange)](https://www.unrealengine.com)
[![MCP](https://img.shields.io/badge/MCP-Model%20Context%20Protocol-blue)](https://modelcontextprotocol.io)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

</div>

**VibeUE extends Unreal Engine 5.8's native AI toolset system.** It registers a deep library of
editor capabilities — Blueprints, materials, landscape, foliage, animation, Niagara, UMG, audio,
StateTree, gameplay tags, input, UVs, performance tracing, and more — into the engine's own
`ToolsetRegistry` and `ModelContextProtocol` server, and serves rich domain **skills** through
Unreal's native `AgentSkill` system. Any MCP-capable agent (Claude Code, Cursor, Copilot, …) drives
your editor through Unreal's standard MCP endpoint.

> **No separate server, no API key, no in-editor chat.** VibeUE is a pure extension: everything shows
> up on Unreal's MCP endpoint alongside the engine's own tools.

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
- **Performance & tracing** — frame-timing (CPU-vs-GPU verdict), Unreal Insights capture, and trace
  analysis. *(The engine has no perf tooling — this is net-new.)*
- **Python-first access** — run any `unreal.*` Python in the editor and introspect the whole API.
- **Web research** — search / fetch / geocode for in-context research and terrain workflows.

It deliberately **does not duplicate** the engine's general tools (basic asset/actor/blueprint/material
CRUD, screenshots, logs, PIE) — agents use Unreal's native toolsets for those.

---

## 🏗️ Architecture

VibeUE plugs into three native UE 5.8 systems:

1. **Toolsets** (`ToolsetRegistry`) — VibeUE's services register as `UToolsetDefinition`s, so their
   methods become AICallable tools on the MCP endpoint. They're also `BlueprintCallable`, so the same
   methods are callable from Python as `unreal.<Name>Service.<method>()`.
2. **MCP server** (`ModelContextProtocol`) — a small set of VibeUE utility tools are registered
   directly on the endpoint: `execute_python_code`, `discover_python_module`/`_class`/`_function`,
   `list_python_subsystems`, `deep_research`, `terrain_data`, `memory`.
3. **Skills** (`AgentSkillToolset`) — ~88 markdown skill packs register as native `UAgentSkill`s,
   discoverable via `ListSkills` and loaded lazily via `GetSkills`, alongside the engine's own skills.

**Efficient usage (for agents):** `execute_python_code` is the workhorse — it batches a whole
multi-step task into one round-trip and reaches every VibeUE service plus the full `unreal.*` API. Use
`call_tool` only for skills and the few engine toolsets with no Python path (screenshots, etc.). See
[`Content/samples/AGENTS.md.sample`](Content/samples/AGENTS.md.sample) for the full agent guide.

---

## 🚀 Installation

### Prerequisites
- Unreal Engine **5.8**
- Git

### 1. Clone into your project's Plugins folder
```bash
cd /path/to/YourProject/Plugins
git clone https://github.com/kevinpbuckley/VibeUE.git
```

### 2. Build
```
Plugins/VibeUE/BuildAndLaunchGame.ps1                  # builds + launches the editor
Plugins/VibeUE/BuildAndLaunchGame.ps1 -StrictRebuild   # full recompile (warnings-as-errors)
```

### 3. Enable the required plugins
In your `.uproject` (or **Edit → Plugins**), enable:
- **VibeUE**
- **ModelContextProtocol** (the MCP server) and **ToolsetRegistry** (the toolset framework)
- **EditorToolset** (the engine's own AI toolsets, so agents can use both)

Restart the editor.

### 4. Turn on the MCP server
In **Project Settings → Plugins → Model Context Protocol**, enable **Auto Start Server** (or run the
console command `ModelContextProtocol.StartServer 8000`). The endpoint is then served at
`http://localhost:8000/mcp` (port/path configurable in those settings). Enabling **Tool Search** keeps
the agent's context small — it sees `list_toolsets`/`describe_toolset`/`call_tool` and loads tools on
demand.

---

## 🧠 Using with external AI agents

Point your MCP client at Unreal's endpoint (`http://localhost:8000/mcp`) and include VibeUE's agent
guide so the assistant knows the efficient patterns.

> See [`Content/samples/README.md`](Content/samples/README.md) for per-tool setup. Quick version:

**Claude Code** — create `CLAUDE.md` at your project root:
```markdown
# My Unreal Project

@Plugins/VibeUE/Content/samples/AGENTS.md.sample
```

**Cursor / Copilot / Codex / others** — copy `Plugins/VibeUE/Content/samples/AGENTS.md.sample` to that
tool's rules file (`.cursor/rules/vibeue.mdc`, `.github/copilot-instructions.md`, `AGENTS.md`, …).

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

VibeUE enables these automatically:

| Plugin | Purpose |
|--------|---------|
| **ToolsetRegistry** | Native AI toolset registration + skills |
| **ModelContextProtocol** | The MCP server endpoint |
| **PythonScriptPlugin** | Python runtime + the `unreal.*` API |
| **EditorScriptingUtilities** | Blueprint & asset manipulation |
| **Niagara**, **MetaSound**, **EnhancedInput**, **ModelViewViewModel**, **StateTree**, **MeshModelingToolset** | Domain support for the services |

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
