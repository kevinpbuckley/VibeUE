# Skill Pack Review — Consolidation Alignment

Reviewing every `Content/Skills/<name>/SKILL.md` (+ sub-docs) after the 5.8 consolidation. For each:
**delete** if a fully-cut service made it dead/redundant with an engine toolset; **rewrite** to point
basics at the engine tool and keep only VibeUE's delta where the service was trimmed; **verify** if the
service was kept.

Ground truth (verdicts from `Toolset-Comparison-and-Consolidation.md` §5.0):
- **CUT services** (skill → delete or redirect to engine): `DataTableService` (→ DataTableTools),
  `DataAssetService` (→ DataAssetTools/ObjectTools), `ScreenshotService` (→ EditorAppToolset capture),
  `EditorTransactionService` (native undo). **CUT tools:** `read_logs` (→ LogsToolset),
  `manage_asset` (→ AssetTools), `editor_control` PIE+screenshot (→ EditorAppToolset).
- **NEW:** `PerformanceService` (`unreal.PerformanceService.*`) replaces `editor_control` profiling.
- **TRIMMED services** keep only their Epic-less delta — verify each against the current
  `Public/PythonAPI/U<Name>Service.h` and reference only methods that still exist.
- Skills load via the engine's `AgentSkillToolset` (`ListSkills`/`GetSkills`); the workhorse is
  `execute_python_code`; basics use the engine's native toolsets.

Status: ☐ pending · ◐ in progress · ☑ done

| Skill | Service(s) | Verdict | Status | Notes |
|---|---|---|---|---|
| data-tables | DataTableService (CUT) | **DELETE** → engine DataTableTools | ☑ | |
| data-assets | DataAssetService (CUT) | **DELETE** → engine DataAssetTools/ObjectTools | ☑ | |
| screenshots | ScreenshotService (CUT) | **DELETE** → engine EditorAppToolset capture | ☑ | |
| log-reading | read_logs/LogReaderService (CUT) | **DELETE** → engine LogsToolset | ☑ | |
| asset-management | AssetDiscoveryService (TRIM) | REWRITE: engine AssetTools/Python for CRUD; VibeUE import/export only | ☑ | |
| level-actors | ActorService (TRIM) | REWRITE: engine ActorTools/SceneTools for ops; VibeUE lock/camera-framing | ☑ | |
| viewport | ViewportService (TRIM) | REWRITE: engine EditorAppToolset camera; VibeUE view-mode/FOV/exposure/layout | ☑ | |
| gameplay-tags | GameplayTagService (TRIM) | REWRITE: engine GameplayTagsToolset CRUD; VibeUE HasTag/AddTags/GetChildren/GetTagInfo | ☑ | |
| engine-settings | EngineSettingsService (TRIM) | REWRITE: keep cvars/scalability/INI; basics → ConfigSettingsToolset | ☑ | |
| project-settings | ProjectSettingsService (TRIM) | REWRITE: keep discovery/INI; basics → ConfigSettingsToolset | ☑ | |
| frame-rate | EngineSettingsService (TRIM) | REWRITE: cvars survive; frame_timing → PerformanceService | ☑ | |
| blueprints | BlueprintService (TRIM) | REWRITE: engine BlueprintTools for basic var/func/node; VibeUE higher-order | ☑ | |
| blueprint-graphs | BlueprintService (TRIM) | REWRITE: keep build_graph/timelines/etc; basic nodes → BlueprintTools | ☑ | |
| materials | MaterialService/Node (TRIM) | REWRITE: engine MaterialTools for basics; VibeUE lifecycle/export/HLSL | ☑ | |
| umg-widgets | WidgetService (TRIM) | REWRITE: engine UMGToolSet/MVVMToolset for CRUD; VibeUE font/brush/anim/preview | ☑ | |
| niagara-systems | NiagaraService (TRIM) | REWRITE: engine NiagaraToolsets for CRUD; VibeUE RI/diagnostics/scratch-pad | ☑ | |
| niagara-emitters | NiagaraEmitterService/ScratchPad (TRIM/KEEP) | REWRITE: engine for module/renderer; VibeUE color/curve + scratch-pad | ☑ | |
| skeleton | SkeletonService (TRIM) | REWRITE: engine SkeletalMeshTools for socket/inspect; VibeUE bone-edit/retarget/profiles | ☑ | |
| pie-testing | editor_control PIE (CUT) + WidgetService | REWRITE: engine EditorAppToolset PIE; VibeUE widget-in-PIE validation | ☑ | |
| profiling | editor_control profiling (CUT) | REWRITE → PerformanceService (`unreal.PerformanceService.*`) | ☑ | |
| vibeue | entry point | REWRITE: new architecture (AgentSkillToolset, Python-first, no VibeUE server) | ☑ | |
| animation-blueprint | AnimGraphService (KEEP) | VERIFY (uses trimmed BlueprintService/AssetDiscovery) | ☑ | |
| animation-editing | AnimSequence/Skeleton (KEEP/TRIM) | VERIFY (Skeleton trimmed) | ☑ | |
| animation-montage | AnimMontageService (KEEP) | VERIFY | ☑ | |
| animsequence | AnimSequence/Skeleton (KEEP/TRIM) | VERIFY (Skeleton trimmed) | ☑ | |
| landscape | LandscapeService (KEEP) | VERIFY | ☑ | |
| landscape-materials | LandscapeMaterial/Material (KEEP/TRIM) | VERIFY (MaterialNode trimmed) | ☑ | |
| landscape-auto-material | LandscapeMaterial/Material (KEEP/TRIM) | VERIFY (MaterialNode trimmed) | ☑ | |
| foliage | FoliageService (KEEP) | VERIFY | ☑ | |
| metasounds | MetaSoundService (KEEP) | VERIFY | ☑ | |
| sound-cues | SoundCueService (KEEP) | VERIFY | ☑ | |
| uv-mapping | UVMappingService (KEEP) | VERIFY | ☑ | |
| map-blockout | MapBlockout (KEEP) + Actor (TRIM) | VERIFY (Actor trimmed) | ☑ | |
| terrain-data | terrain_data tool (KEEP) | VERIFY | ☑ | |
| enhanced-input | InputService (KEEP) | VERIFY | ☑ | |
| enum-struct | EnumStructService (KEEP) | VERIFY | ☑ | |
| state-trees | StateTreeService (KEEP) | VERIFY | ☑ | |
| pcg | native PCG Python (KEEP) | VERIFY | ☑ | |

---

## Review complete

**Deleted (4):** `data-tables`, `data-assets`, `screenshots`, `log-reading` — their services were cut;
the engine's `DataTableTools` / `DataAssetTools` / `EditorAppToolset` capture / `LogsToolset` own those.

**Rewritten — basics redirected to engine, VibeUE delta kept (17):** asset-management, level-actors,
viewport, gameplay-tags, engine-settings, project-settings, frame-rate, blueprints, blueprint-graphs,
materials, umg-widgets, niagara-systems, niagara-emitters, skeleton, pie-testing, profiling, vibeue.
Each skill (and its sample `.txt` scripts) was checked against the current `U<Name>Service.h`; cut
methods were replaced with the engine toolset (`call_tool`) or native `unreal.*` Python, and
`vibeue-skills-manager` was replaced with `ListSkills`/`GetSkills` on `AgentSkillToolset`.

**Verified, light/no change (15):** animation-blueprint, animation-editing, animsequence,
animation-montage, landscape, landscape-materials, landscape-auto-material, foliage, metasounds,
sound-cues, uv-mapping, map-blockout, terrain-data, enhanced-input, enum-struct, state-trees, pcg.
(animation-blueprint, map-blockout, enum-struct, uv-mapping, pcg needed small redirects where they
leaned on a trimmed/cut service.)

**Cross-cutting fixes:** removed every `manage_asset` / `read_logs` / `editor_control` /
`vibeue-skills-manager` call site (→ engine tools or native Python); `editor_control` profiling →
`unreal.PerformanceService.*`; removed VibeUE-MCP-server / API-key language; fixed dangling
cross-references to the four deleted skills. Verified: 0 cut-tool call sites, 0 cut-service method
calls remain.
