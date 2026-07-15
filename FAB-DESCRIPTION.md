🚀 VibeUE — MCP Expansion + AI Editor Toolset for Unreal Engine 5.8+

Unreal Engine 5.8 added a built-in MCP server and AI toolsets. VibeUE is an MCP Expansion that plugs straight into them and adds a deep AI Editor Toolset — a library of editor capabilities in the domains Epic's own toolsets don't cover. Any MCP-capable agent (Claude Code, Cursor, GitHub Copilot, Codex, Gemini, …) drives your editor through Unreal's standard MCP endpoint. No separate server, no in-editor chat.


✨ WHAT MAKES VIBEUE SPECIAL:

• 30 AICallable service toolsets exposing 890+ tools, registered onto the engine's own ToolsetRegistry and MCP server (every service is also callable from Python)
• ~34 lazy-loaded domain skills served through Unreal's native AgentSkill system — full workflows and gotchas, loaded only when needed
• 7 discovery & execution tools — run any unreal.* Python in the editor and introspect the whole API
• Complements Epic's toolsets instead of duplicating them — it focuses on the depth the engine doesn't ship


🎯 WHAT IT ADDS ON TOP OF EPIC'S TOOLSETS:

• ⚡ Performance & profiling (flagship) — frame timing with a CPU-vs-GPU-bound verdict, Unreal Insights trace capture, and a trace+log analyser. Epic's toolsets can start PIE but can't measure anything.
• ↩️ Editor undo / redo — a Transaction toolset to group many edits into one undo step, roll back, and inspect history. Epic ships no transaction toolset.
• 🌍 Terrain & world — Landscape sculpting/heightmaps/splines, landscape auto-materials + Runtime Virtual Texture, Foliage, procedural Map Blockout, and real-world heightmaps + water features from GPS coordinates.
• 🎬 Animation — AnimSequence keyframe editing, AnimMontage authoring, AnimBP state machines, and Skeleton bone/socket/retarget/blend-profile editing.
• 🎆 Niagara depth — emitter color/curve authoring and Custom-HLSL scratch-pad modules.
• 🎵 Audio — SoundCue and MetaSound graph authoring (create sources, add/wire DSP nodes, set defaults, manage graph I/O).
• 🖼️ UI — UMG widgets with MVVM bindings, animation authoring, and preview/PIE validation.
• 🧩 Higher-order Blueprint authoring — timelines, event dispatchers, delegates, custom-event pins, comment boxes, and a batch graph builder.
• 🔧 StateTree, Gameplay Tags, Enhanced Input, Materials (+ node graph), UV mapping, Viewport control, and Project/Engine settings.
• 🌐 Web research — search / fetch / geocode for in-context research and terrain workflows.


🛠️ HOW TO USE:

VibeUE expands Unreal's native MCP, so set that up first (enable the Unreal MCP, Toolset Registry, and Editor Tools plugins and start the server — see Epic's docs), then install VibeUE and enable it. Its services, tools, and skills register onto the same endpoint alongside the engine's own. Point any MCP agent at the endpoint and go. To set up the agent guide, run the console command VibeUE.GenerateAgentConfig — it writes VibeUE's guide to the right file for your agent (CLAUDE.md, GEMINI.md, AGENTS.md, or .github/copilot-instructions.md; pass All for every file), resolving the plugin's FAB or Git install location automatically. The guide teaches the efficient patterns: discover before you call, batch with execute_python_code, and load skills on demand.


🔑 API KEY:

VibeUE works without a key. A free API key (grab one at vibeue.com/login, set it in Editor Preferences → Plugins → VibeUE) unlocks only the real-world terrain tools — every other feature works without one.


📚 RESOURCES:

• Project home: https://www.vibeue.com/v5-8
• Free API key: https://www.vibeue.com/login


Requirements: Win64 · Unreal Engine 5.8+ · Unreal's native MCP stack enabled (Unreal MCP, Toolset Registry, Editor Tools). VibeUE auto-enables the engine plugins its services need: Python Script Plugin, EditorScriptingUtilities, Enhanced Input, Niagara, MetaSound, MeshModelingToolset, ModelViewViewModel, StateTree, GameplayTagsEditor, plus ToolsetRegistry and ModelContextProtocol.
