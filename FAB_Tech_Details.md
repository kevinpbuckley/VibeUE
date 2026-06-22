MCP Expansion + AI Editor Toolset for Unreal Engine 5.8+. Plugs into UE5.8's native MCP server, ToolsetRegistry, and AgentSkill system and adds the editor depth Epic's own toolsets don't cover. Drive your editor from any MCP agent (Claude Code, Cursor, Copilot, Codex, Gemini). No separate server, no in-editor chat.

Service toolsets — 30 AICallable service toolsets exposing 891 tools, registered onto the engine's MCP endpoint. Standouts: BlueprintService (101), StateTreeService (95), AnimSequenceService (89), LandscapeService (68), AnimMontageService (62), WidgetService (42), SoundCueService & SkeletonService (38), MaterialNodeService (34), InputService & UVMappingService & LandscapeMaterialService (22-23), MetaSoundService (20, audio graph authoring), ViewportService (17), TransactionService (14, editor undo/redo), PerformanceService (11, frame timing + Unreal Insights trace), plus broad coverage for Niagara, foliage, landscape materials + RVT, gameplay tags, and project/engine settings. Every service is callable both as an AICallable MCP tool and from Python (unreal.<Name>Service.*).

Discovery & execution — 7 MCP tools: execute_python_code, discover_python_module/_class/_function, list_python_subsystems, deep_research (web search/fetch/geocode), and terrain_data (real-world heightmaps + water features).

Skills — ~34 lazy-loaded domain skill packs served through Unreal's native AgentSkill system, reducing agent context overhead while preserving full domain guidance.

Module: VibeUE (Editor), 105 C++ source files. Win64 / Mac / Linux. Unreal Engine 5.8+.

Prerequisites — enable Unreal's native MCP stack first: Unreal MCP (ModelContextProtocol), Toolset Registry (ToolsetRegistry), and Editor Tools (EditorToolset), then start the MCP server (see Epic's docs). VibeUE auto-enables the engine plugins its services need: PythonScriptPlugin, EditorScriptingUtilities, EnhancedInput, Niagara, MetaSound, MeshModelingToolset, ModelViewViewModel, StateTree, GameplayTagsEditor.

API key — optional and free (vibeue.com/login), set in Editor Preferences → Plugins → VibeUE. It unlocks only the real-world terrain tools; everything else works without one.

Docs: https://www.vibeue.com/docs
