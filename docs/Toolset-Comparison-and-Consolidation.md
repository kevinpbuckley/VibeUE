# VibeUE × Unreal Engine 5.8 — Toolset Comparison & Consolidation Plan

**Goal:** make VibeUE *enhance* Unreal 5.8's native AI toolset system, not compete with it. This
document inventories every Epic and VibeUE toolset/tool, maps the overlaps, and for each VibeUE
toolset says: **KEEP & ENHANCE**, **REFACTOR**, or **DELETE** (Epic already covers it).

Status as of the 5.8 migration: VibeUE is now an *extension* — 31 service toolsets + 10 bridged
MCP tools + 88 skills, all registered into Epic's `ToolsetRegistry` / `ModelContextProtocol` /
`AgentSkillToolset`. There is no separate VibeUE server, chat, or skill system anymore.

---

## 1. TL;DR

Epic 5.8 shipped a real AI toolset framework (`ToolsetRegistry` + `ModelContextProtocol`) and a
broad **general-purpose** toolset suite (`EditorToolset`: Blueprint, Material, Actor, Asset, Mesh,
DataTable, …) plus **domain** toolsets (StateTree, GameplayTags, Niagara, MVVM/UMG, PCG, Physics,
GAS, …). Much of VibeUE's surface now overlaps the engine's.

VibeUE's durable value is in three areas Epic does **not** cover, plus depth Epic doesn't reach:

- **Domains Epic has no toolset for:** Landscape (sculpt/heightmap/splines), Landscape Materials,
  Foliage, MetaSound, SoundCue, UV Mapping, Runtime Virtual Textures, Map Blockout, Enhanced Input,
  real-world terrain data, web research.
- **Depth beyond Epic** in shared domains: Blueprint timelines/dispatchers/delegates/build_graph,
  Niagara scratch-pad HLSL, Skeleton bones/sockets/retargeting, AnimSequence keyframe editing,
  AnimMontage authoring, StateTree utility-AI considerations, MVVM bindings.
- **Python-first introspection:** `execute_python_code` + `discover_python_*` over the whole
  `unreal` API — the escape hatch Epic's typed toolsets don't provide.

**Decision rule (default to cut):** a VibeUE toolset survives **only** if Epic has no equivalent, or
VibeUE's depth is a *distinct, demonstrable user win Epic can't deliver*. "VibeUE is a bit deeper"
is **not** a reason to keep — if Epic covers the workflow acceptably, we cut and defer. The win is a
small, sharp plugin that does what the engine can't, not a second copy of `EditorToolset`.

> **Buckets below are source-verified** (§5) by reading each Epic toolset and the matching VibeUE
> service against each other in the engine + plugin source, with every domain toolset enabled live.

| Bucket | VibeUE toolsets/tools |
|---|---|
| **KEEP** (Epic has no equivalent, or only read-only inspection) | LandscapeService, LandscapeMaterialService, FoliageService, MetaSoundService, SoundCueService, UVMappingService, RuntimeVirtualTextureService, MapBlockoutService, InputService, **StateTreeService** (Epic is inspection-only), **EnumStructService** (no Epic toolset), NiagaraScratchPadService, AnimSequenceService, AnimMontageService, AnimGraphService, **PerformanceService (new — from `editor_control`)**, `execute_python_code`, `discover_python_*`, `list_python_subsystems`, `terrain_data`, `deep_research` |
| **TRIM TO THE DELTA** (Epic overlaps; keep only the verified Epic-less methods, cut the rest) | BlueprintService, MaterialService, MaterialNodeService, WidgetService, SkeletonService, NiagaraService, NiagaraEmitterService, ActorService, ViewportService, AssetDiscoveryService, EngineSettingsService, ProjectSettingsService, GameplayTagService |
| **CUT** (Epic does it as well or better; only marginal deltas) | ScreenshotService, DataTableService, DataAssetService, EditorTransactionService, `manage_asset`, `read_logs`, the PIE+screenshot actions of `editor_control` |

---

## 1.5 Target footprint — the lean VibeUE

Source-verified outcome: **~26 services survive** (16 kept intact + ~10 trimmed groups), **~5 cut
outright** (ScreenshotService, DataTableService, DataAssetService, EditorTransactionService, plus the
`editor_control` PIE/screenshot split) and the `read_logs`/`manage_asset` MCP tools. The big win is the
**tool *count*** dropping hard as the 13 overlap services are trimmed to their Epic-less delta — not
deleting whole services, because VibeUE genuinely exceeds Epic almost everywhere. Grouped by user
benefit (KEEP = intact, *trimmed* = keep only the delta):

**Terrain & world** (Epic has *nothing* here)
- `LandscapeService` — sculpt, heightmaps, paint layers, splines, terrain analysis
- `LandscapeMaterialService` — layer-blend + auto-materials + RVT
- `FoliageService` — foliage types + scatter/paint
- `MapBlockoutService` — procedural open-world FPS blockout
- `RuntimeVirtualTextureService` — RVT assets/volumes
- `terrain_data` — real-world heightmaps/water from coordinates

**Audio** (Epic has *nothing* here)
- `MetaSoundService`, `SoundCueService`

**Animation assets** (Epic's tooling is Sequencer/ControlRig, not asset editing)
- `AnimSequenceService`, `AnimMontageService`, `AnimGraphService`, `SkeletonService` (bones/sockets/retarget delta)

**FX & mesh depth** (Epic's is off/shallow)
- `NiagaraScratchPadService` (Custom-HLSL — unique), `NiagaraService`/`NiagaraEmitterService` (delta only)
- `UVMappingService` — UV channels/unwrap/transform (Epic only imports meshes)
- `MaterialNodeService` → trimmed to graph-recreation/export + Custom HLSL (Epic's `MaterialTools` does plain CRUD)

**Input** (Epic has *nothing* here)
- `InputService` — Enhanced Input actions/contexts/triggers

**Gameplay logic & data** (Epic is inspection-only or absent)
- `StateTreeService` → **KEEP intact** — Epic's `StateTreeTools` is read-only; VibeUE owns all mutation/binding/compile
- `EnumStructService` → **KEEP** — no Epic enum/struct authoring toolset
- `EngineSettingsService` / `ProjectSettingsService` → *trimmed* to cvars + scalability + raw-INI +
  UDeveloperSettings discovery (Epic's `ConfigSettingsToolset` does none of these)
- `GameplayTagService` → *trimmed* to `HasTag`, `AddTags`, `GetChildren`, redirect tracking

**High-order Blueprint / UI builders** (keep only what Epic's `BlueprintTools`/`UMGToolSet` lack)
- `BlueprintService` → *trimmed* to `build_graph`, timelines, event dispatchers, delegates,
  custom-event input-pin CRUD, comment boxes, component bind/inspect, diff/introspection
- `WidgetService` → *trimmed* to font/brush editing, MVVM bindings, animation authoring, preview/PIE validation, snapshots

**Scene / assets / materials** (keep only the Epic-less slice)
- `ActorService` → *trimmed* to transform-lock, absolute-transform, preserve-scale, camera-framing, `GetAllProperties`
- `ViewportService` → *trimmed* to view modes / FOV / exposure / game-view / layout (Epic owns camera)
- `AssetDiscoveryService` → *trimmed* to crash-safe `ImportAsset`/`ImportTexture`/`ExportTexture` + content-browser selection
- `MaterialService` / `MaterialNodeService` → *trimmed* to lifecycle, bulk param set, graph export +
  diagnostics + compare, batch ops, custom expressions, collection params

**Performance & tracing** (Epic has *nothing* here)
- `PerformanceService` — `frame_timing`, Unreal Insights traces, `analyse`, trace-attached standalone

**Python backbone** (Epic's `ProgrammaticToolset` is thin)
- `execute_python_code`, `discover_python_module/class/function`, `list_python_subsystems`

**Research**
- `deep_research` — web search / fetch / geocode

Everything not in this list is **cut** and the agent uses Epic's tool instead. Skills stay (already
native `UAgentSkill`s) and should be pruned to match the surviving toolsets.

---

## 2. The two systems side by side

| | **Unreal 5.8 native** | **VibeUE** |
|---|---|---|
| Framework | `ToolsetRegistry`, `ModelContextProtocol` (HTTP `:8000/mcp`) | extends the above (no own server) |
| Tool unit | `UToolsetDefinition` static `UFUNCTION(meta=(AICallable))` / Python `@tool_call` | same (services reparented to `UToolsetDefinition`) |
| Params/returns | **typed USTRUCTs** (`Vector`, `Transform`, object refs) → auto JSON schema | mixed: services use typed structs; the 10 bridged tools take a flat string map and return a **JSON string** |
| Discovery | `list_toolsets` / `describe_toolset` / `call_tool` meta-tools | same endpoint |
| Skills | `AgentSkillToolset.ListSkills`/`GetSkills` over `UAgentSkill` classes | 88 packs registered as `UAgentSkill` via Python (unified) |
| Object handles | live `UObject` soft-path refs (`{"refPath": …}`) | string asset paths / actor names |
| Breadth | 28 toolset plugins (general + per-domain) | 31 domain services + 10 utility tools |

**Convention gap to close:** Epic passes/returns *typed structs* and *object references*; VibeUE's
services mostly take `FString`/JSON-string args and return JSON strings. This is the single biggest
"work more like Epic" refactor (see §6).

---

## 3. Epic's toolset catalog

### Enabled in this project (confirmed live via `list_toolsets`)

| Toolset | Tools (representative) | Purpose |
|---|---|---|
| `EditorToolset.BlueprintTools` | `create`, `compile_blueprint`, `add_variable`, `add_function_graph`, `add_event`, `create_node`, `connect_pins`, `set_pin_value`, `find_nodes`, `arrange_nodes`, `set_parent`, … (~60) | **Comprehensive Blueprint authoring** |
| `EditorToolset.MaterialTools` | `create_material`, `add_expression`, `connect_expressions`, `connect_to_output`, `recompile`, `layout_expressions`, … (~22) | Material graph authoring |
| `EditorToolset.MaterialInstanceTools` | `create`, get/set `scalar`/`vector`/`texture`/`static_switch` params, `set_parent` | Material instances |
| `EditorToolset.ActorTools` | get/set transform, label, tags, `add_component`, `remove_component`, `get_actor_bounds`, `look_at`, reparent | Per-actor ops |
| `EditorToolset.SceneTools` | `find_actors`, `spawn_actor`, `destroy_actor`, `select_actors`, `load_level`, `get_current_level` | Level/scene ops |
| `EditorToolset.AssetTools` | `find_assets`, `load_asset`, `save_assets`, `move`, `delete`, `duplicate`, `get_referencers`, `get_dependencies`, metadata, `read_file`/`write_file` | **Full asset management** |
| `EditorToolset.DataTableTools` | `create`, `import_file`, `get_schema`, `list/add/remove/rename/get/set_rows`, `search_row_structs` | DataTables |
| `EditorToolset.DataAssetTools` | `create` | DataAssets (minimal) |
| `EditorToolset.CurveTableTools` | `create`, `import_file`, rows + curve keys | CurveTables |
| `EditorToolset.StringTableTools` | `create`, `import_file` | StringTables |
| `EditorToolset.StaticMeshTools` | `import_file`, lod/triangle/vertex counts | Static mesh import/inspect |
| `EditorToolset.SkeletalMeshTools` | `import_file`, lod/vertex/section counts | Skeletal mesh import/inspect |
| `EditorToolset.TextureTools` | `import_file`, `create_render_target`, `get_texture_size`, `compress_texture` | Textures |
| `EditorToolset.ObjectTools` | `search_subclasses`, `get/list/set/reset_properties`, `get_class` | Generic UObject reflection |
| `EditorToolset.PrimitiveTools` | `add_cube/sphere/cylinder/cone` | Primitive components |
| `EditorToolset.ProgrammaticToolset` | `get_execution_environment`, `execute_tool_script` | **Run Python batches** |
| `EditorToolset.EditorAppToolset` | `StartPIE`/`StopPIE`/`IsPIERunning`, `CaptureViewport` (with annotations!), `CaptureEditorImage`, `CaptureAssetImage`, `SearchCVars`, get/set camera, focus, selection, content-browser nav, `OpenEditorForAsset` | **Editor app control + screenshots** |
| `EditorToolset.LogsToolset` | `GetLogEntries`, `GetLogCategories`, `Get/SetVerbosity` | **Log reading** |
| `AIAssistant.AIAssistantToolset` | `GetProjectContext`, `GetDockedContext` | In-editor assistant context (no VibeUE analog) |
| `ToolsetRegistry.AgentSkillToolset` | `ListSkills`, `GetSkills`, `CreateSkill`, `UpdateSkill` | **Skills** (VibeUE packs now register here) |
| `UserDefinedEnum`, `UserDefinedStruct` | create/edit enum & struct | Overlaps VibeUE `EnumStructService` |

### Exists in engine, NOT enabled here (`Engine/Plugins/Experimental/Toolsets/`)

Enable these to compare exact tools before deleting the matching VibeUE service:

`GameplayTagsToolset`, `StateTreeToolset`, `NiagaraToolsets` + `NiagaraAIAssistantTools`,
`MVVMToolset`, `UMGToolSet`, `PCGToolset`, `PhysicsToolsets`, `GASToolsets`, `DataRegistryToolset`,
`ConfigSettingsToolset`, `AnimationAssistantToolset` (Sequencer/ControlRig), `ConversationToolset`,
`AIModuleToolset` (Behavior Trees), `ChaosClothAssetToolset`, `GameFeaturesToolset`,
`LiveCodingToolset`, `SemanticSearchToolset`, `SlateInspectorToolset`, `WorldConditionsToolset`,
`AutomationTestToolset`, `SequencerAnimMixerToolset`, `MetaHumanGenerator`, `PluginToolset`,
`DataflowAgent`. (`AllToolsets` is an umbrella that enables the lot; `MCPClientToolset` is the
outbound MCP client.)

---

## 4. VibeUE's toolsets

31 reparented service toolsets (`VibeUE.<Name>Service`, ~1,052 AICallable tools) + 10 bridged MCP
tools. Method counts approximate.

| VibeUE toolset | ~tools | Epic counterpart? |
|---|---|---|
| BlueprintService | 119 | `BlueprintTools` (strong) |
| StateTreeService | 94 | `StateTreeToolset` (exists, off) |
| AnimSequenceService | 89 | none (AnimationAssistant ≠ asset editing) |
| LandscapeService | 68 | **none** |
| AnimMontageService | 62 | none |
| SkeletonService | 53 | `SkeletalMeshTools` (import only) |
| AnimGraphService | 48 | none (AIModuleToolset = BT, not AnimBP) |
| MaterialNodeService | 41 | `MaterialTools` (strong) |
| WidgetService | 41 | `UMGToolSet` + `MVVMToolset` (exist, off) |
| SoundCueService | 38 | **none** |
| NiagaraService | 37 | `NiagaraToolsets` (exists, off) |
| ActorService | 33 | `ActorTools` + `SceneTools` + `EditorAppToolset` (strong) |
| MaterialService | 30 | `MaterialTools`/`MaterialInstanceTools` (strong) |
| EngineSettingsService | 23 | `ConfigSettingsToolset` + `SearchCVars` |
| InputService | 23 | **none** |
| NiagaraEmitterService | 23 | `NiagaraToolsets` (partial) |
| LandscapeMaterialService | 22 | **none** |
| UVMappingService | 22 | **none** |
| AssetDiscoveryService | 21 | `AssetTools` (strong) |
| EnumStructService | 20 | `UserDefinedEnum`/`UserDefinedStruct` |
| MapBlockoutService | 19 | **none** |
| NiagaraScratchPadService | 19 | none (deep) |
| ViewportService | 19 | `EditorAppToolset` (camera only) |
| ProjectSettingsService | 16 | `ConfigSettingsToolset` |
| EditorTransactionService | 16 | none (engine has undo natively) |
| DataTableService | 15 | `DataTableTools` (strong) |
| FoliageService | 15 | **none** |
| DataAssetService | 11 | `DataAssetTools` (create only) |
| GameplayTagService | 8 | `GameplayTagsToolset` (exists, off) |
| ScreenshotService | 6 | `EditorAppToolset` capture (stronger) |
| RuntimeVirtualTextureService | 4 | **none** |

Bridged MCP tools: `execute_python_code`, `discover_python_module/class/function`,
`list_python_subsystems`, `manage_asset`, `read_logs`, `editor_control`, `terrain_data`,
`deep_research`.

---

## 5. Overlap analysis & per-toolset verdict

### 5.0 Source-verified verdict table (authoritative)

Verified by reading each Epic toolset and the matching VibeUE service in source, with all Epic domain
toolsets enabled live. **This table supersedes the narrative bullets below** where they differ.

| VibeUE service | Epic counterpart (live name) | Verdict | What survives / why |
|---|---|---|---|
| LandscapeService, LandscapeMaterialService | *(none)* | **KEEP** | Epic has no landscape toolset at all |
| FoliageService | *(none)* | **KEEP** | no Epic foliage toolset |
| MetaSoundService, SoundCueService | *(none)* | **KEEP** | no Epic audio-authoring toolset |
| UVMappingService | `SkeletalMeshTools`/`StaticMeshTools` (import/inspect only) | **KEEP** | Epic only imports/inspects meshes; no UV editing |
| RuntimeVirtualTextureService, MapBlockoutService | *(none)* | **KEEP** | unique |
| InputService | *(none)* | **KEEP** | no Epic Enhanced-Input toolset |
| AnimSequence/AnimMontage/AnimGraphService | `animation_toolset.*` (Sequencer/ControlRig) | **KEEP** | Epic is Sequencer/ControlRig; VibeUE edits the anim *assets* — different |
| StateTreeService | `state_tree_toolset.StateTreeTools` | **KEEP** | **Epic is read-only inspection (~9 getters); VibeUE does all mutation/binding/compile** |
| EnumStructService | *(none)* | **KEEP** | no Epic enum/struct authoring toolset |
| NiagaraScratchPadService | *(none)* | **KEEP** | Custom-HLSL scratch-pad node graph — unique |
| BlueprintService | `editor_toolset.BlueprintTools` (~60) | **TRIM** | keep timelines, custom-event input-pin CRUD, `build_graph`, delegates, component bind/inspect, diff/introspection, existence checks, split/recombine pin |
| MaterialService + MaterialNodeService | `MaterialTools` + `MaterialInstanceTools` | **TRIM** | keep lifecycle (save/compile/open/refresh), `SetInstanceParametersBulk`, graph export + `GetMaterialDiagnostics` + `CompareMaterialGraphs`, batch ops, `PromoteToParameter`, custom expressions, collection params |
| WidgetService | `UMGToolSet` + `MVVMToolset` | **TRIM** (≈keep) | keep font/brush editing, animation authoring, preview/PIE validation, snapshots, string-based event binding (Epic has none of these) |
| SkeletonService | `editor_toolset.SkeletalMeshTools` | **TRIM** | keep bone transform/hierarchy editing, retargeting, curve metadata, blend profiles, learn-from-animations; cut socket/material/LOD inspect overlap |
| NiagaraService, NiagaraEmitterService | `NiagaraToolsets.*` (5 toolsets) | **TRIM** | keep color-curve editing (`SetColorCurveKeys`, `ShiftColorHue`) + rapid-iteration params; cut system/emitter/module/renderer CRUD (Epic covers) |
| ActorService | `ActorTools` + `SceneTools` + `EditorAppToolset` | **TRIM** | keep transform-lock, absolute-transform flags, preserve-scale, camera-framing (`GetActorViewCamera`/`CalculateActorView`), `GetAllProperties`; cut transform/component/tag/selection/spawn overlap |
| ViewportService | `EditorAppToolset` (camera only) | **TRIM** | keep view modes / FOV / exposure / game-view / layout; cut camera get/set |
| AssetDiscoveryService | `editor_toolset.AssetTools` | **TRIM** | keep crash-safe `ImportAsset`/`ImportTexture`/`ExportTexture` + content-browser selection + `IsAssetOpen`; cut CRUD overlap |
| EngineSettingsService, ProjectSettingsService | `ConfigSettingsToolset` | **TRIM** | keep **cvars, scalability, raw-INI, UDeveloperSettings discovery** (Epic covers *none* of these — it only edits registered settings sections) |
| GameplayTagService | `GameplayTagsToolset` | **TRIM** | keep `HasTag`, `AddTags` (batch), `GetChildren`, redirect tracking; cut core add/remove/rename/list (both write INI+runtime) |
| ScreenshotService | `EditorAppToolset` capture | **CUT** | Epic's `CaptureViewport`/`CaptureEditorImage`/`CaptureAssetImage` are stronger (grid + labels) |
| DataTableService | `editor_toolset.DataTableTools` | **CUT** | Epic covers create/import/schema/rows CRUD; VibeUE deltas (GetInfo/RowExists/UpdateRow) marginal |
| DataAssetService | `DataAssetTools` + `ObjectTools` | **CUT** | create + generic get/set properties cover it; deltas marginal |
| EditorTransactionService | *(native undo)* | **CUT** | engine undo/redo is native; agents rarely drive the buffer |
| `read_logs` | `EditorToolset.LogsToolset` | **CUT** | `GetLogEntries` + categories + verbosity cover it |
| `manage_asset` | `editor_toolset.AssetTools` | **CUT** | fold the crash-safe image import into trimmed AssetDiscoveryService; rest is AssetTools |
| `editor_control` (PIE+screenshot) | `EditorAppToolset` | **CUT** | `StartPIE`/`StopPIE`/`IsPIERunning` + `CaptureViewport` |
| `editor_control` (profiling/tracing) | *(none)* | **KEEP → `PerformanceService`** | `frame_timing`, Insights traces, `analyse`, standalone — Epic has zero perf tooling |

**Net:** only **~5 services cut outright** (Screenshot, DataTable, DataAsset, EditorTransaction, + the `editor_control` split) plus `read_logs`/`manage_asset`; **13 trimmed** to their verified Epic-less delta; **16 kept** intact. VibeUE complements Epic far more than it duplicates — the win is mostly in *trimming method counts*, not deleting whole services.

### KEEP & ENHANCE — Epic has no equivalent (VibeUE's differentiators)

- **LandscapeService / LandscapeMaterialService** — Epic ships **no** landscape toolset. Sculpting,
  heightmap import/export, paint layers, splines, terrain analysis, auto-materials, RVT. *Crown jewel.*
- **FoliageService** — no Epic foliage toolset.
- **MetaSoundService / SoundCueService** — no Epic audio-authoring toolset.
- **UVMappingService** — no Epic UV toolset (StaticMeshTools only imports/inspects).
- **RuntimeVirtualTextureService**, **MapBlockoutService** — unique.
- **InputService** — no Epic Enhanced-Input toolset.
- **AnimSequenceService / AnimMontageService** — Epic's `AnimationAssistantToolset` is
  Sequencer/ControlRig-centric; VibeUE edits the *animation assets* (keyframes, bone tracks,
  montage sections/slots/notifies). Different and complementary.
- **NiagaraScratchPadService** — scratch-pad/Custom-HLSL module authoring; deeper than Epic's
  Niagara toolset is likely to go.
- **AnimGraphService** — AnimBP state machines; Epic's BT toolset is unrelated.
- **`execute_python_code` + `discover_python_*` + `list_python_subsystems`** — the Python escape
  hatch over the entire `unreal` API. Epic's `ProgrammaticToolset.execute_tool_script` is similar but
  VibeUE's discovery trio has no native analog and is what makes the services self-teaching. *Keep;
  this is the backbone.*
- **Performance & Tracing** (currently the `editor_control` profiling actions) — `frame_timing`
  (Game/Render/GPU thread split + CPU-vs-GPU-bound verdict), Unreal Insights trace control
  (`profiler_start/stop/status`, `bookmark`, `region_start/end`), `analyse` (trace+logs → perf
  summary), and trace-attached `start_standalone`. **Epic's toolset catalog has *zero* performance
  or tracing tools** (verified across all 28 plugins) — its `EditorAppToolset` can start PIE/Simulate
  but cannot measure anything. This is one of VibeUE's strongest differentiators and is currently
  hidden inside an overloaded tool. → *Enhance:* extract into a dedicated `VibeUE.PerformanceService`
  (or `InsightsToolset`) so the agent discovers "I can profile" via `list_toolsets`; keep
  `frame_timing` as the recommended-first entry; pair with the `profiling` + `frame-rate` skills.
- **`terrain_data`, `deep_research`** — real-world heightmaps/water + web search/geocode. Unique.

**Enhancement direction:** adopt Epic conventions inside these (typed structs, object refs, output
schemas) so they feel native, and register matching `AgentSkill`s (done).

### TRIM TO THE DELTA — keep only the slice Epic lacks (default: cut the rest)

For each of these, the target end-state is a *much smaller* service holding only the unique part.
Cut every method that merely re-does an Epic tool.

- **BlueprintService (119) → keep ~the higher-order builders.** Epic's `BlueprintTools` (~60) already
  covers create/compile/variables/functions/events/nodes/pins/layout. **Cut all of that.** Keep only
  what Epic lacks: `build_graph` batch builder, timelines, event dispatchers, delegates, custom-event
  input-pin CRUD, comment boxes, delegate-bind-on-variable.
- **MaterialNodeService → keep graph-recreation/export + Custom HLSL.** Cut plain expression CRUD
  (Epic's `MaterialTools` has it).
- **WidgetService → keep MVVM bindings + animation authoring + preview/PIE validation.** Enable Epic's
  `UMGToolSet`/`MVVMToolset`, diff, cut whatever they cover; align naming with Epic.
- **SkeletonService → keep bones/sockets/retargeting/blend-profiles.** Cut the import/LOD/count
  inspection (Epic's `SkeletalMeshTools`).
- **NiagaraScratchPadService → keep entirely** (Custom-HLSL authoring, unique).
  **NiagaraService / NiagaraEmitterService → trim to the delta** after diffing `NiagaraToolsets`.
- **StateTreeService (94) → keep the delta** (utility-AI considerations, Blueprint tasks, delegate
  transitions). Enable `StateTreeToolset`, diff, cut the overlap.
- **ViewportService → keep view modes / FOV / exposure / game-view / layout.** Cut camera get/set/focus
  (Epic's `EditorAppToolset`).
- **`editor_control` → split, don't refactor in place.** *Cut* the PIE (`start_pie`/`stop_pie`/
  `pie_status`) and screenshot actions (Epic's `EditorAppToolset` covers them). *Promote* the
  performance/tracing actions into the new `PerformanceService` (KEEP & ENHANCE). Nothing of
  `editor_control` survives as-is.

### CUT — Epic does it as well or better; delete and defer

- **ScreenshotService** → `EditorAppToolset.CaptureViewport` / `CaptureEditorImage` /
  `CaptureAssetImage` (Epic's even overlays a world grid + actor labels).
- **AssetDiscoveryService + `manage_asset`** → `AssetTools` (find/load/save/move/delete/duplicate/
  referencers/dependencies/metadata/read-write). Keep *only* the crash-safe image **import** if Epic's
  import proves weaker (verify); otherwise delete outright.
- **`read_logs`** → `LogsToolset` (`GetLogEntries` + categories + verbosity).
- **DataTableService** → `DataTableTools` (create/import/schema/rows CRUD — on par).
- **DataAssetService** → `DataAssetTools` (create) + `ObjectTools.get/set_properties` (everything else).
- **EnumStructService** → Epic's `UserDefinedEnum` / `UserDefinedStruct` toolsets.
- **EditorTransactionService** → undo/redo is native engine behavior; an AI rarely drives the
  transaction buffer.
- **ActorService** → `ActorTools` + `SceneTools` + `EditorAppToolset` cover transform/components/tags/
  selection/spawn/destroy/camera. (If transform-lock/constraints/preserve-scale prove genuinely
  Epic-less and useful, keep *only* those 2-3 methods — otherwise cut whole.)
- **MaterialService** → `MaterialTools` + `MaterialInstanceTools` (create/params/recompile).
- **GameplayTagService** → `GameplayTagsToolset`. Verify Epic writes INI **and** runtime-registers; if
  so, cut. (Only 8 methods — low value to keep.)
- **EngineSettingsService / ProjectSettingsService** → `ConfigSettingsToolset` + `SearchCVars` +
  `ObjectSettings`. Diff and cut; keep a scalability-preset helper only if Epic has no equivalent.

---

## 6. Conventions to adopt from Epic ("work more like Epic")

Apply across the KEEP/REFACTOR toolsets:

1. **Typed structs over stringly-typed args.** Replace `FString`/`ParamsJson` inputs with USTRUCT
   params (`FVector`, `FRotator`, transform/range structs). Epic's schema generator turns these into
   rich `inputSchema` objects; VibeUE's JSON-string args produce opaque schemas. This mostly affects
   the **10 bridged MCP tools** (flat string map → JSON string) — the reparented services already use
   typed params and benefit immediately.
2. **Return USTRUCTs, not JSON strings.** Typed returns give auto `outputSchema` (Epic does this
   everywhere). The bridged tools currently return a hand-built text blob.
3. **Object references, not path strings.** Where a tool acts on an actor/asset, accept a
   `UObject`/soft-path ref like Epic (`{"refPath": …}`) so results from one tool feed into the next.
4. **`AgentSkill` for guidance, tools for actions.** Already done — VibeUE packs are `UAgentSkill`s.
   Keep guidance out of tool descriptions.
5. **`ClampMin`/`ClampMax`, `ToolTip`, `@param`/`@return` docs** flow into the schema — annotate
   numeric ranges and every param.
6. **Don't re-implement reflection.** Prefer `ObjectTools`/`AssetTools` for generic property/asset ops
   instead of bespoke VibeUE equivalents.

---

## 7. Suggested execution order

1. **Verify the "off" domain toolsets** — temporarily enable `GameplayTagsToolset`, `StateTreeToolset`,
   `NiagaraToolsets`, `UMGToolSet`/`MVVMToolset`, `ConfigSettingsToolset`; `describe_toolset` each;
   record exact deltas vs the matching VibeUE service.
2. **Cut the verified redundancies** (§5.0 CUT): ScreenshotService, DataTableService, DataAssetService,
   EditorTransactionService, `read_logs`, `manage_asset`, and the PIE+screenshot actions of
   `editor_control`. (That's the *whole* clean-cut list — everything else earns its place.)
3. **Trim the 13 delta services** (§5.0 TRIM) down to only the Epic-less methods in the table, and
   adopt §6 conventions. This is where most of the ~1,052→~?? tool-count reduction happens.
4. **Extract the Performance/Tracing toolset** out of `editor_control` into its own
   `VibeUE.PerformanceService` (frame_timing + Insights traces + analyse + standalone) — net-new
   capability Epic lacks entirely.
5. **Polish the differentiators** (Landscape, Foliage, audio, UV, Niagara scratch-pad, anim, Python,
   terrain/research, performance/tracing) and ship matching skills.

Net effect: VibeUE shrinks from 31 services + 10 tools to the **~18 toolsets** in §1.5 — only what
the engine genuinely lacks — presents them in Epic's idiom, and stops shadowing
`EditorToolset`/`EditorAppToolset`/`LogsToolset`/`AssetTools`. Every retained tool earns its place by
doing something Claude (or any agent) on Epic's endpoint otherwise could not do.
