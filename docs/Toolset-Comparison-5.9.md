# VibeUE × Unreal Engine 5.9 — Toolset Comparison & Consolidation (5-9 branch)

Successor to [`Toolset-Comparison-and-Consolidation.md`](Toolset-Comparison-and-Consolidation.md),
which was written against **5.8**. Epic's 5.9 toolset catalog grew enough that several of that
document's "KEEP — Epic has nothing here" verdicts are now **wrong**. Read this one for the 5-9
branch; keep the 5.8 doc for the history of why a service exists at all.

**Decision rule (unchanged, and still default-to-cut):** a VibeUE tool survives only if 5.9 has no
equivalent, or VibeUE's depth is a distinct, demonstrable win the engine cannot deliver. "A bit
deeper" is not a reason to keep.

---

## 0. Method — how the numbers here were produced

Both sides were extracted from source, not from `list_toolsets` on a live editor (so the counts
include toolsets that ship disabled):

- **Epic C++ tools:** every `UFUNCTION` carrying `AICallable` or `AIRuntimeCallable` under
  `Engine/Plugins/Experimental/Toolsets` → **917 functions in 78 classes**.
- **Epic Python tools:** every `@toolset_registry.tool_call` under `Engine/Plugins` →
  **114 functions in 6 plugins**. *5.8-era analysis missed this channel entirely — `GeometryToolset`
  and `StateTreeToolset` are pure-Python toolsets with no AICallable C++ at all.*
- **Epic total: ~1031 tools** across **49 toolset plugin directories**.
- **VibeUE:** same C++ extraction over `Source/VibeUE` → **1050 AICallable functions in 38 service
  classes**, plus the `FToolRegistry` MCP bridge tools (`execute_python_code`,
  `discover_python_module/class/function`, `list_python_subsystems`, `deep_research`, `terrain_data`).
  VibeUE declares **no** Python `tool_call`s.

So the two surfaces are now roughly the same size. In 5.8 VibeUE was the broader of the two.

### Conventions that changed in 5.9 (affects every tool we keep)

- `AIRuntimeCallable` now marks tools callable **at runtime/in PIE**, distinct from editor-time
  `AICallable`; `AIAccessMode = "Read"|"Write"` is declared per tool. Python uses
  `@toolset_registry.tool_call(mode=unreal.ToolAccessMode.READ|WRITE)`.
- `meta={'AIIgnore': ''}` hides a member; `is_toolset_hidden(provider)` withholds a whole toolset
  from a restricted host.
- `UToolCallAsyncResultString` is **gone**; `UToolsetRegistry::ExecuteTool` returns
  `UAsyncFunctionResultString*` (module `AsyncFunctionResult`). The namespaced
  `UE::ToolsetRegistry::FToolsetRegistry::Get()->ExecuteTool(...)` returns
  `TFuture<TValueOrError<FString, FString>>` and takes one fully-qualified `"Toolset.Tool"` name.
- `FModelContextProtocolToolRequestId::RequestId` is private; use `GetRequestId()`. Its `operator==`
  and `GetTypeHash` **are** exported now, and the struct carries `SessionId` / `InvocationSerial` /
  `InvocationId` — so cancellation can finally be scoped per session instead of per id. *(VibeUE
  still cancels per id; see §5.)*

---

## 1. What 5.9 added that invalidates 5.8 verdicts

Toolset plugins present in 5.9 but **not** in the 5.8 catalog, with the VibeUE service each one
threatens:

| New 5.9 toolset | Tools | 5.8 verdict it invalidates |
|---|---|---|
| `MetaSoundToolset` (Authoring 24, Inspection 13, Discovery 5) | 42 | "**no Epic audio-authoring toolset**" → `MetaSoundService` (20) |
| `StateTreeToolset` (Python) | 49, **32 write** | "Epic is **read-only inspection**" → `StateTreeService` (99) |
| `BlueprintToolsets` (`UBlueprintToolset` 74, `UBlueprintDslToolset` 7) | 81 | Blueprint tools left `EditorToolset`; a **graph DSL** now exists → `BlueprintService` (110) |
| `EditorToolset` Python (`read_graph_dsl`, `write_graph_dsl`, `get_graph_dsl_docs`, `diff`, `get_symbol`, `search_symbols`, `read_me`) | 7 | VibeUE's `build_graph` batch builder and graph diff/introspection delta |
| `PerfToolset` (`UCsvProfilerToolset` 5, `UStatsToolset` 4, `UGpuProfilerToolset` 1) | 10 | "Epic's catalog has **zero** performance or tracing tools" → `PerformanceService` (17) |
| `InputToolset` | 2, runtime | "**no Epic Enhanced-Input toolset**" → `InputService` (28) — **but see §3** |
| `WidgetAnimationToolset` | 10 | "keep **animation authoring**" → `WidgetService` (43) |
| `WidgetStaticAnalysisToolset` | 5 | WidgetService validation delta |
| `CommonUIToolset` | 15 | WidgetService / UMG delta |
| `ConsoleToolset` (`UConsoleToolset` 5, `ULogsToolset` 4) | 9, runtime | "Epic covers **no** cvars" → `EngineSettingsService` (18); `read_logs` |
| `UserDefinedStruct`/`UserDefinedEnum` toolsets | 22 | "**no Epic enum/struct authoring toolset**" → `EnumStructService` (20) |
| `UUndoHistoryToolset` | 4 | `TransactionService` (14) |
| `GeometryToolset` (Python) | 6 write | mesh-append primitives (not UV — see §2) |
| `SceneCaptureToolset` | 14 | screenshot/capture delta |
| `NavMeshToolset` | 21, runtime | — (new domain) |
| `TextureGraphToolset` | 18 | — |
| `FileSystemToolset` | 9 | `AssetDiscoveryService` import/export delta |
| `KnowledgeBaseToolset` | 6 | `deep_research` overlap (needs diff) |
| `RewindDebuggerToolset` | 10 | `PerformanceService` overlap (needs diff) |
| `CaptureManagerToolset`, `MediaToolset`, `MovieRenderPipelineToolset`, `CheatsToolset`, `AgentPluginToolset`, MetaHuman set | 40+ | — (new domains) |

Also note: `UMGToolSet` and `SemanticSearchToolset` (both named in the 5.8 doc) are **absent** from
5.9's `Experimental/Toolsets`; the nearest 5.9 equivalents are the three Widget/CommonUI toolsets and
`KnowledgeBaseToolset` respectively. `LogsToolset` moved from `EditorToolset` into `ConsoleToolset`.

---

## 2. What is still VibeUE-only in 5.9 (verified)

A grep for `AICallable`/`AIRuntimeCallable` across **all** of `Engine/Plugins` returns **no**
landscape, foliage or terrain tools. These keep their 5.8 "crown jewel" status:

- `LandscapeService` (75), `LandscapeMaterialService` (22) — sculpt, heightmaps, paint layers,
  splines, auto-materials, RVT
- `FoliageService` (15), `MapBlockoutService` (19), `RuntimeVirtualTextureService` (4)
- `terrain_data` — real-world heightmaps/water
- `SoundCueService` (38) — `MetaSoundToolset` is MetaSound-only; SoundCue is untouched
- `UVMappingService` (22) — `GeometryToolset` only *appends primitives*
  (`append_box/cone/cylinder/disc/sphere/torus`); it does not touch UVs
- `NiagaraScratchPadService` (20) — no Custom-HLSL scratch-pad equivalent
- `CustomizableObjectService` (18) — Mutable has no engine toolset
- `AnimSequenceService` (92), `AnimMontageService` (62), `AnimGraphService` (48) —
  `AnimationAssistantToolset`'s 44 Python tools are **ControlRig graph** authoring
  (`add_bone`, `add_control`, `add_null`, solve graphs) plus 4 Sequencer-director tools; none of it
  edits AnimSequence keyframes, montage sections/slots/notifies, or AnimBP state machines

---

## 3. Verdicts needing a tool-level diff before cutting

Ordered by expected tool-count win. **Name overlap is not evidence** — `InputToolset` is the
cautionary case: it sounds like it replaces `InputService`, but its 2 tools are `ApplyInput` /
`IsInputApplied`, i.e. *runtime injection of presses and holds on a local player*. It does not author
`UInputAction` or `UInputMappingContext` assets at all, so `InputService`'s authoring survives —
while VibeUE's **own input-injection** path (see `InputInjectionTests.cpp`) is now the redundant part.

| VibeUE service | 5.9 counterpart | Expected verdict |
|---|---|---|
| `BlueprintService` (110) | `UBlueprintToolset` 74 + `UBlueprintDslToolset` 7 + EditorToolset graph DSL | **TRIM hard** — re-diff `build_graph` against `write_graph_dsl` first |
| `StateTreeService` (99) | `StateTreeToolset` 49 (32 write) + engine `StateTreeEditorData` binding API | **TRIM hard** — first cut already landed, see §5 |
| `WidgetService` (43) | WidgetAnimation 10 + CommonUI 15 + MVVM 31 + WidgetStaticAnalysis 5 | **TRIM hard** |
| `MetaSoundService` (20) | MetaSound Authoring/Inspection/Discovery 42 | **likely CUT whole** |
| `EnumStructService` (20) | UserDefinedStruct 14 + UserDefinedEnum 8 | **likely CUT whole** |
| `TransactionService` (14) | `UUndoHistoryToolset` 4 | **CUT** (5.8 doc already said cut; it never happened) |
| `EngineSettingsService` (18) / `ProjectSettingsService` (11) | ConfigSettings 12 + Console 5 + Logs 4 | **TRIM** — the cvar delta that justified these is now native |
| `PerformanceService` (17) | PerfToolset 10 + RewindDebugger 10 | **TRIM** — Insights trace control may still be VibeUE-only |
| `BehaviorTreeService` (23) + `BlackboardService` (11) | `UBehaviorTreeToolset` 25 | **TRIM/CUT** |
| `EnvQueryService` (24) | `UEnvironmentQueryToolset` 14 | **TRIM/CUT** |
| `SkeletonService` (39) | `USkeletalMeshToolset` 22 | **TRIM** |
| `MaterialService` (16) + `MaterialNodeService` (36) | Material 25 + MaterialInstance 14 + CurveAtlas 13 | **TRIM** |
| `NiagaraService` (6) + `NiagaraEmitterService` (6) | NiagaraToolsets 57 | **likely CUT both** (keep ScratchPad) |
| `ActorService` (11) | `UActorToolset` 21 + `ULevelToolset` 36 | **TRIM** |
| `ViewportService` (22) | `UEditorAppToolset` 42 + SceneCapture 14 | **TRIM** |
| `AssetDiscoveryService` (7) | `UAssetToolset` 24 + FileSystem 9 | **TRIM/CUT** |
| `GameplayTagService` (6) | `UGameplayTagsToolset` 11 | **CUT** |
| `InputService` (28) | `InputToolset` 2 (runtime only) | **KEEP authoring**, cut VibeUE's injection |

---

## 4. Skills — 5.9 handles markdown skills natively

This is a **mechanism** change, not just a content change.

`ToolsetRegistry/MarkdownAgentSkills.h` (5.9-only) discovers skills from disk and serves them through
the same `AgentSkillToolset.ListSkills` / `GetSkills` the engine's own skills use:

- Root per enabled plugin: **`<Plugin>/Skills/**/SKILL.md`** → key `/Skills/<PluginName>[/<sub>]`
- Also `Engine/Extras/AgentTools/Skills/`, `Engine/Platforms/<P>/Skills/`, `<Project>/Skills/`, and
  anything added via `AddSearchRoot(Directory, Prefix)`
- A `SKILL.md` needs frontmatter **`name`** and **`description`**; the body is the instructions
- Files are re-read on each call, and **relative markdown links are rewritten to absolute paths** so
  the agent can open sibling pages and scripts
- `UAgentSkill` C++/Blueprint classes still work and are listed alongside markdown skills

VibeUE was off-convention on three counts, all now fixed on this branch:

1. Skills lived in **`Content/Skills/`**, which is *not* a discovery root — only `<Plugin>/Skills/`
   is. → **moved to `Skills/`**, so the engine serves them as `/Skills/VibeUE/<pack>`.
2. A **116-line Python shim** (`Content/Python/init_unreal.py`) read each markdown file and
   synthesised a `UAgentSkill` subclass per file, with CDO-updating reload support. → **deleted**,
   along with the `VibeUE.ReloadSkills` console command and Module.cpp's now-unused
   `IPythonScriptPlugin` include. The engine re-reads the files on every call, so reload is free.
3. The shim registered **every file** as its own skill (39 `SKILL.md` + 53 sub-docs). The native
   model registers one skill per `SKILL.md` and expects sub-docs to be reached through **relative
   links**, which the engine rewrites to absolute paths — but only 3 of 39 `SKILL.md` files linked
   their sub-docs that way; the rest named them in backticks (`` `api-bindings` ``), which worked
   only because the shim registered each separately. → every pack now ends with a **Reference
   pages** section of relative links to its sub-docs and `scripts/` samples (28 packs updated).

Converting the backticked *prose* mentions was deliberately not attempted: stems like `color` and
`reference` also occur as ordinary backticked words, so a blind rewrite would have produced wrong
links. An explicit link section makes every page reachable without touching the prose.

Frontmatter needed no work — all packs already carry `name:` and `description:`, which is exactly
what `ParseSkillText` reads.

**Packaging** was the one real risk, and it is handled: UAT `BuildPlugin` auto-includes only the
`.uplugin`, `Source/`, `Content/` and `Resources/`; anything else ships only if listed in
`Config/FilterPlugin.ini`. A `/Skills/...` entry was added there, so the FAB package still carries
the skills from their new home.

**Every removal must sweep the skills too.** A cut tool that is still documented in a skill is worse
than no skill: the agent is told to call something that no longer exists.

---

## 5. Status on this branch

**5.9 build migration** (gate for everything else — the plugin did not compile on 5.9 at branch
point):

1. `UToolCallAsyncResultString` → `UAsyncFunctionResultString` in `MaterialServiceTests.cpp`, plus
   the `AsyncFunctionResult` module and `AsyncFunctionResult/AsyncFunctionResultString.h`.
2. `FStateTreeBindingInfo` collided with 5.9's new engine struct of the same name — see below.
3. `FModelContextProtocolToolRequestId::RequestId` → `GetRequestId()`.

**First removal, done end to end as the template for the rest:**

`UStateTreeService::GetBindings` is gone. 5.9's engine ships
`UStateTreeEditorData::GetBindings(State)` and `GetBinding(Target, bFound)` as
`UFUNCTION(meta=(PythonCallable))`, returning a richer `FStateTreeBindingInfo` (typed
`FPropertyBindingPath` addresses, `Usage`, `Compatibility`, `CompletionCondition`) and taking the
**same optional per-state filter** VibeUE's version offered. Reachable from `execute_python_code` as
`unreal.StateTreeEditorData.get_editor_data(st).get_bindings(state)`. That Epic chose the identical
struct name is a fair signal the capability was absorbed wholesale.

Removed: the `GetBindings` tool and its 2 automation tests. Kept: the internal binding reader that
fills `FStateTreePropertyInfo::bIsBound`/`BoundTo`, whose struct is renamed
`FVibeUEStateTreeBindingInfo` (the collision that broke the build). Skills updated:
`state-trees/api-bindings.md` now documents the engine API, and the `SKILL.md` index entry points at
it.


### Stage A — whole-service cuts (done)

**1050 -> 996 AI-callable tools, 38 -> 35 service classes.**

| Service | Was | Now | Why |
|---|---|---|---|
| `MetaSoundService` | 20 | **deleted** | `MetaSoundToolset` (42) covers it and more: builder/preset lifecycle, graph pages, variables, interfaces, `DescribeGraph`/`DescribeNode`, `DiffPages`, node-class discovery. Also dropped the `MetasoundEngine`/`Frontend`/`GraphCore`/`Editor` module deps and the `MetaSound` plugin dependency |
| `EnumStructService` | 20 | **deleted** | `UserDefinedStructToolset` (14) + `UserDefinedEnumToolset` (8) cover every method and add move/reorder, bitflags, tooltips, editable-on-instance and savegame flags |
| `NiagaraService` | 6 | **deleted** | `NiagaraToolsets` (57): stack input data replaces rapid-iteration params, `GetEmitterData` replaces lifecycle, `GetStackIssues` + `GetSystemCompileState` replace `DebugActivation`, schema tools + EditorToolset `diff` replace `CompareSystems` |
| `NiagaraEmitterService` | 6 | **3** | cut `SetSimTarget` (-> `SetEmitterData`), `GetRapidIterationParameters` (-> `GetStackInputData`) and `SetColorTint` (-> `SetStackInputData` on Scale Color). Kept `GetColorCurveKeys` / `SetColorCurveKeys` / `ShiftColorHue`: a ColorFromCurve module stores keyframe **data**, not an input value, so no stack-input write reaches it — the skill's own live-verified note says curve keys "CANNOT be modified via rapid iteration parameters". Dead `ListModules`/`AddModule` and two orphaned USTRUCTs went with `SetColorTint`, their only caller |
| `GameplayTagService` | 6 | **2** | cut `HasTag`, `GetTagInfo`, `GetChildren`, `AddTags` -> `GameplayTagsToolset` (11), which also adds sources, referencers, rename and move |

**The GameplayTagService correction is worth recording.** It was first deleted outright on the
strength of the tool-name comparison, then partly restored: `RequestTag` / `RequestTagContainer`
are **not** replaceable by a toolset. Python cannot construct an `FGameplayTag` (`TagName` is
read-only, `MakeLiteralGameplayTag` takes a tag rather than a string), and a tool call returns JSON,
not a live struct value into the calling `execute_python_code` session. Those two tools are the only
way to obtain a usable tag value for `has_matching_gameplay_tag`,
`send_gameplay_event_to_actor`, `assign_tag_set_by_caller_magnitude` or tag-keyed `TMap` authoring.
The `gas` skill had this documented as a live-verified finding; the tool-name diff alone would have
destroyed it. **Read the skills before cutting, not only the headers.**

**Skills swept with the code** (a cut tool still documented in a skill is worse than no skill):
deleted the `metasounds`, `enum-struct`, `gameplay-tags` and `niagara-systems` skill packs; rewrote
`niagara-emitters` (SKILL.md + color.md, deleted reference.md and build_emitter.txt, rewrote
recolor_emitter.txt, repointed scratch-pad.md's sim-target and spawn-rate guidance at
`SetEmitterData` / `SetStackInputData`); repointed the dead skill references in `gas`, `sound-cues`
and the `vibeue` index; corrected `AGENTS.md.sample` for enums/structs and for `LogsToolset` having
moved into `ConsoleToolset`. The rewritten niagara pages use **relative markdown links**, which is
what 5.9's native skill discovery rewrites (see §4).

### Stage B — TransactionService split (done)

**996 -> 989 tools.** The 5.8 doc called this a clean CUT; verification says it is a 7/7 split.

| Cut (5.9 covers it) | Replacement |
|---|---|
| `BeginTransaction`, `EndTransaction`, `CancelTransaction` | `unreal.ScopedEditorTransaction("...")` — `FPyScopedEditorTransaction` in PythonScriptPlugin |
| `GetHistory`, `GetState` | `EditorToolset.UndoHistoryToolset.GetRecentTransactions` |
| `GetUndoDescription`, `GetRedoDescription` | `GetTransactionDetails` + `GetTransactionRecencyIndex` |

**Kept, because stepping the buffer is exposed nowhere:** `Undo`, `Redo`, `UndoMultiple`,
`RedoMultiple`, `CanUndo`, `CanRedo`, `ResetBuffer`. `PyEditor.cpp` binds no undo/redo and `GEditor`
is not scriptable, so an agent has no other way to press Ctrl+Z. The two orphaned USTRUCTs
(`FTransactionState`, `FTransactionHistoryEntry`) and the dead `GActiveTransactionIndex` went with
the cut half. No skill referenced any of the removed tools.

### Stage C — the duplicate MCP tools, and the agent guides (done)

**The MCP bridge goes from 8 tools to 2.** 5.9 ships `EditorToolset.PythonScriptingTools`, which
replaced the 5.8-era `ProgrammaticToolset` (gone from this engine):

| Removed from VibeUE | 5.9 replacement |
|---|---|
| `execute_python_code` | `execute_python(script: CodeBlock)` — EXECUTE mode, unsandboxed, game thread, top-level `await`, composes other tools via `call_tool`; `read_me` carries the usage rules |
| `discover_python_module` / `_class` / `_function`, `list_python_subsystems` | `search_symbols` + `get_symbol` — regex over the registered tools **and** the whole `unreal` API, filtered by kind, by parameter type (`accepts`) or return type (`returns`), matched against name/signature/documentation, ranked, and limited to symbols this session actually provides |
| `capture_image` | `SceneCaptureToolset.CaptureGameViewport` — captures the Game/PIE viewport *as Slate draws it, with UMG and any other UI over the viewport*, takes a `World` from `ListWorlds` to pick a PIE instance, and `MaxResolution` to clamp. `CaptureActiveEditorView`/`CaptureEditorViewport` cover the no-UI sources and `EditorAppToolset.CaptureEditorImage` the whole window |

Surviving bridge tools: **`deep_research`** and **`terrain_data`** — web research and real-world
terrain, neither of which the engine has any tool for.

Two 5.8-era claims in VibeUE's own guide turned out to be what justified keeping these, and 5.9
falsified both. "Epic's capture returns base64 **inside a text block** on every route" is no longer
true: `ModelContextProtocolToolResults.cpp` emits real `"type": "image"` content blocks, and
`FToolsetImage` is the standard return. And `capture_image`'s headline feature — seeing the Slate/UMG
HUD, "the thing CaptureViewport cannot see" — is exactly what `CaptureGameViewport` is documented to
do. Re-read the engine before trusting a delta recorded against an older version.

**The one real loss, and how it is covered.** VibeUE's `execute_python_code` had three
crash-prevention features Epic's `execute_python` does not: `auto_save` (default on), a
`fresh_namespace` option, and replies carrying `resident_maps` / `dirtied_packages` (a non-empty
`resident_maps` means the next level load *will* fatal the editor). The guard is now explicit rather
than a hidden default, and nothing became unreachable: `unreal.PythonTools.get_resident_maps()` and
`get_dirty_packages()` are still tools, and `vibeue.safe_load_level(path)` still refuses a level
change while a map is resident. The agent guide leads with that check.

**Guides and skills rewritten to match.** `Content/samples/AGENTS.md.sample` is the single source for
CLAUDE.md / GEMINI.md / AGENTS.md / copilot-instructions.md (`VibeUE.GenerateAgentConfig` copies it),
so updating it updates every generated file:
- §1 now leads with `read_me` then `execute_python`, and carries the resident-map guard.
- §2's roster is rebuilt: engine-native Python/discovery, the two surviving VibeUE tools, and a note
  that screenshots are engine-native.
- §3 is rewritten for native markdown skills: keys are **`/Skills/VibeUE/<name>`** (not the Python
  shim's `/VibeUE/Python/init_unreal_PY.VibeUE_<name>`), and **sub-docs are no longer separate skill
  entries** — they are relative links under each pack's Reference pages section, which UE rewrites to
  absolute paths, so an agent reads the file at the given path.
- §6 is rebuilt around the engine capture tools, including viewport video recording.
- Across the 35 skill packs: **128 references** to the removed tools substituted
  (`execute_python_code` -> `execute_python` x100, `discover_python_*`/`list_python_subsystems` ->
  `get_symbol`/`search_symbols` x28), and the 7 `capture_image` references rewritten, including a
  full rewrite of the `pie-testing` capture section.
- README's architecture section and agent-workflow bullets updated.

**The orphaned code is gone too** (follow-up done in the same branch). Removing the
`execute_python_code` registration left a whole execution layer with no caller, and it has now been
deleted rather than left compiling:

| Deleted | Why it was orphaned |
|---|---|
| `PythonExecutionService.{h,cpp}` | the executor itself; only `ExecutePythonCode` drove it |
| `PythonSchemaService.{h,cpp}` | only `UPythonTools::GetSchemaService` used it |
| `PythonTypes.h` | its structs (`FPythonExecutionResult`, module/class/function info) existed for the executor and the deleted discovery service |
| `VibeUEPythonResultLog.{h,cpp}` | wrote the `python-<pid>-last.json` run records nothing reads now |
| `Content/Python/vibeue_await.py` | driven only by `FPythonExecutionService`'s await stepping |
| `CaptureTools.cpp` | the whole file was `capture_image`'s implementation plus two tests that assert it is registered |
| `PythonAwaitTests.cpp`, `PythonExecutionServiceTests.cpp` | tests of the above |

Also stripped: `UPythonTools::ExecutePythonCode`, `NeedsAwaitRun`, `StartAwaitRun`,
`CancelAwaitRuns`, `AbandonAwaitRuns`, `GetAwaitRunCount`, `ConvertExecutionResultToJson`,
`GetExecutionService`, `GetSchemaService`, `Shutdown` (and its `Module.cpp` call), the `GAwaitRuns`
state and the auto-save/snapshot helpers — `PythonTools.cpp` goes from 887 lines to 169. The MCP
bridge lost its Python special-casing (`PythonToolName`, the `NeedsAwaitRun`/`StartAwaitRun`
dispatch, the script-cancel branch in `CancelAsync`, the `AbandonAwaitRuns` call on shutdown) while
keeping the off-game-thread machinery `deep_research` and `terrain_data` still need. `vibeue.py` lost
the defunct run-record readers (`last_python_result`, `python_run`, and their path helpers).

**What survives in `UPythonTools`** is exactly the editor-safety surface the agent guide now tells
agents to call explicitly: `GetResidentMaps`, `GetDirtyPackages` (both AICallable),
`GetResidentMapWorlds` and `ComputeDirtyPackageDelta`.

**Test coverage lost with it, and worth re-adding.** `PythonToolsTests.cpp` goes from 1004 lines to
92. Four of its twelve tests were kept-or-dropped on whether they had a subject left: only
`ResidentMapDetection` stood alone. Three others — `DirtiedPackagesReported`,
`GetResidentMapsIsCallable` and `SafeLoadLevelRefuses` — ran their Python through a
`RunPythonNoSave` helper on the removed executor and asserted on *its* reply fields, so their subject
went with it. That means `ComputeDirtyPackageDelta` and `vibeue.safe_load_level` are now untested
even though both are still shipped. Re-pointing those three at the engine's `execute_python` (and at
`PythonTools` directly for the delta) is the follow-up, and it is test authoring rather than cleanup,
which is why it was not folded in here.

Net effect of the cleanup: 11 files deleted, ~4,600 lines removed, and
`UnrealEditor-VibeUE.dll` down to 17.49 MB from 18.93 MB at the branch point.

## 6. The full-surface audit (8 parallel audits, all 12 remaining services)

### The scoping rule, as decided by the owner

**Only a DIRECT Epic AI-editor-toolset duplicate is cut.** A capability reachable only through a
native engine Python API (`execute_python` + `unreal.*`) is **NOT** a duplicate and stays. Taken the
other way the rule would end the plugin, since nearly everything VibeUE does is "reachable from
Python" eventually. Two consequences settled at the same time: `PerformanceService.Analyse` stays
(nothing in Epic's surface parses a `.utrace`), and where a cut would strand a caller the tool is
**demoted** (strip `AICallable`, keep the function) rather than deleted — the test is that an agent
can still complete the task.

### The engine fact that governs most verdicts

`EditorToolset.ObjectToolset` is **not** an unrestricted property backdoor, and three of the eight
audits disagreed about this until it was settled in source:

- `ObjectToolset.cpp:96` — `SetProperties` calls `RequireBlueprintOwnedTemplate`, which **refuses
  class-default data of a non-Blueprint class**. Native `UDeveloperSettings` CDOs are therefore
  unreachable; settings work belongs to `ConfigSettingsToolset`, never `ObjectToolset`. Ordinary
  asset objects (a `UStateTreeState`, a `UMaterialExpression`) have no `UClass` in their outer chain
  and pass.
- `ToolsetLibrary.cpp:332` — every key then passes `CheckKeyAccess` →
  `UE::PropertyEditor::CheckCanSetProperty`, refused on denial. So a `VisibleAnywhere`
  (`CPF_EditConst`) property, a bare `UPROPERTY()`, or an `Instanced` property without
  `CPF_Edit`/`CPF_BlueprintVisible` **cannot be written, and often cannot even be read.**

This is what makes several keeps engine-blocked rather than merely convenient, and it is what
invalidated one already-committed cut (see §5's ActorService correction).

### Verdicts

| Service | Cut | Keep | State |
|---|---|---|---|
| `BlueprintService` | 89 + 2 demoted | 19 | **done** |
| `SkeletonService` | 13 | 26 | **done** |
| `FabService` | 7 | 0 | **done** — service deleted |
| `StateTreeService` | 53 | 45 | **done** — resolved, see below |
| `MaterialService` + `MaterialNodeService` | 50 | 2 | **done** |
| `WidgetService` | 34 | 9 | **done** |
| `EngineSettings` + `ProjectSettings` | 27 + 2 demoted | 0 | **done** |
| `ViewportService` | 4 | 18 | **done** |
| `PerformanceService` | 10 + 2 demoted | 7 | **done** |
| `WorkflowService` | 11 | 1 | **done** |
| `InputService` | 3, then 1 restored | 26 | **done** — see below |
| `PIEActorService` | 0 | 7 | nothing to do |
| `PythonTools` | 0 | 2 | nothing to do |

All twelve are executed. 1000 AI tools at the branch point, 620 now; 397 UFUNCTIONs deleted
outright and 4 demoted to Python-only. DLL 18.93 MB -> 9.58 MB.

### InputService: one cut partly reversed

Cutting `InjectActionFor` as a duplicate of `InputToolset.ApplyInput` was right about the *timed*
hold and wrong about the indefinite one. It was the only writer of `GHeldInjections`, so
`StopInjection` was left unable to ever succeed (`was_active` permanently false) and
`InjectAction`'s own user-visible note still told agents to call the removed tool. `ApplyInput`
takes a `DurationSeconds` and returns an async result that completes only once the hold has ended,
so the hold and the agent's turn are one span — you cannot hold an input and then look at anything
while it is held. `StartInjection` restores the continuous injection and hold registration without
the release ticker: `ApplyInput` keeps every hold of known length (and multi-entry presses, which
VibeUE never did), and VibeUE keeps the one whose release depends on something you have to observe
first. The lesson is the same one the ActorService and GameplayTag corrections taught — a
replacement that covers *most* of a tool's behaviour is not a duplicate of it.

### How StateTree resolved

Its audit cut 85 of 98 on the premise that `ObjectToolset` "reaches every `UPROPERTY`, skipping only
`CPF_Deprecated`". That premise is false (above), and it is load-bearing for specific cuts —
`SetStateExpanded` was justified verbatim as "non-Edit `UPROPERTY`, still reflected". Separately, the
owner's scoping rule returns the **24-tool `Bind*`/`Unbind*` family** to KEEP: its replacement was
`UStateTreeEditorData::SetBinding`/`ClearBinding`, which are `PythonCallable` native APIs, not
toolset tools. What remains genuinely cuttable is the `StateTreeToolset`-backed CRUD
(`add_child_state`, `add_task`, `remove_*`, `move_*`, `compile`, `request_ui_refresh`),
`AssetToolset`-backed list/save, and the property setters that survive a per-property
`CPF_Edit` check against `StateTreeState.h`.

**That per-property pass was run, and it found a third gate clause neither earlier pass had:**
`PropertyAccessUtil::CanSetPropertyValue` also refuses `CPF_DisableEditOnInstance`, which
`EditDefaultsOnly` sets — and nearly every StateTree editor property is `EditDefaultsOnly`. That
clause alone would have flipped the whole list to KEEP. It resolves toward CUT because
`PropertyAccessUtil::IsObjectTemplate` admits "objects within an asset that are edit-inline can edit
default properties", and `UStateTreeState`, `UStateTreeEditorData` and `UStateTreeComponentSchema`
are all `EditInlineNew` inside the `UStateTree` asset, so `bOwnerIsTemplate` is true and there is no
`CanEditChange` override anywhere in `StateTree/` to intervene. Epic's route is in fact *more*
correct than VibeUE's: a toolset write goes through `EmitPostChangeNotify` ->
`PostEditChangeChainProperty`, so the engine's own fixups fire (setting `Type` resets `Tasks` for a
Group, forces `SelectionBehavior` for the linked types, clears `LinkedSubtree`/`LinkedAsset` when
leaving them, flips `Parameters.bFixedLayout`) where VibeUE hand-mirrored them and could drift.

Final verdict 53 CUT / 45 KEEP. The per-property pass also confirmed the `SetStateExpanded` keep
outright: `bExpanded` is `UPROPERTY(meta = (ExcludeFromHash))` — no `CPF_Edit`, no
`CPF_BlueprintVisible` — so the toolset refuses it for read *and* write and it is invisible even to
`ListProperties`. Seven keeps survived that pass, reducing to three rules worth reusing:

1. A bare `UPROPERTY()` is refused for read AND write (`SetStateExpanded`; same shape as `Parent`,
   which adds `PythonReadOnly`).
2. Anything inside an `FInstancedStruct` or behind an `Instanced` object reference is not
   path-addressable at all — `ToolsetPropertyPath` refuses to cross an object reference and
   `FInstancedStruct` has no reflected fields (`SetTaskEnabled`, `SetTaskConsideredForCompletion`,
   `GetTransitionConditionPropertyNames`, `SetTransitionConditionPropertyValue`,
   `AddTransitionCondition`, which additionally needs `InitializeAs` to allocate
   `Instance`/`InstanceObject` with the right outer and mint the node ID).
3. Array **reorder** is expressible only as a lossy same-size rewrite that never emits `ArrayMove`
   (`MoveTransition`). Add and remove are safe because `HandleArrayAdds`/`HandleArrayRemoves` apply
   them incrementally — an add mints only the new element's GUID, a remove is diffed against the
   engine's own `PreEditTransitions` copy — so the IDs `EditorBindings` is keyed on survive. A
   wholesale clear-then-re-add would regenerate every ID and silently orphan every binding, which is
   why the array tools were judged per operation rather than as a group. `grep` confirms zero
   occurrences of `EPropertyChangeType::ArrayMove` anywhere in the engine's toolset library.

Epic's own StateTree toolset file confirms two of the keeps in its header comment: "there is no K2
path for creating a StateTree asset with a schema in head (`UStateTreeFactory::SetSchemaClass` isn't
a UFUNCTION) ... asset creation is deliberately left out", and "The wrappers below do not open undo
or transaction scopes of their own" — so a batch of Epic StateTree edits needs the caller's own
transaction, which VibeUE's removed tools each opened for themselves.

The 13 originally-argued keeps stand and are well evidenced: `CreateStateTree` (Epic has no StateTree
factory tool and the skill records `unreal.StateTreeFactory` failing from Python), the four
`GetAvailable*Types` (Epic's adders take a `UScriptStruct` but nothing in the catalog enumerates
`UScriptStruct` node types — `SearchSubclasses` is `UClass`-only), the three theme-colour tools
(`FStateTreeEditorColor::ExportTextItem` returns false unless `PPF_Copy|PPF_Duplicate`, so Epic reads
empty strings), the three root-parameter tools (`FInstancedPropertyBag`'s schema can only be mutated
from C++; JSON import requires an already-initialised bag), `SetComponentParameterOverride` (needs
property-bag descriptor GUIDs no Epic tool exposes) and `SelectState` (`SelectAssetEditorObjects`
raises for non-Blueprint/Widget editors).

### Noted inconsistency, deliberately not churned

`StateTreeService.GetBindings` was removed earlier on the now-rejected "engine `PythonCallable`
exists" basis. It is left removed on outcome grounds: reading bindings is still available to an agent
as `unreal.StateTreeEditorData.get_editor_data(st).get_bindings(state)`, which the `state-trees`
skill documents. Restoring it would add a tool without adding a capability.

### Method flaw found and fixed — four times

Every attempt at the Epic tool index missed a whole family, and each miss would have produced
wrong verdicts. Recording them because the pattern is the lesson: **a toolset can be declared in
more ways than you expect, so never enumerate them structurally when you can enumerate the
metadata instead.**

1. The first index covered only `Engine/Plugins/Experimental/Toolsets`, so it missed
   `Engine/Plugins/Fab/Source/FabToolset` (`UFabToolset`, 5 tools, in an `EnabledByDefault`
   plugin) and would have preserved all 7 `FabService` tools. 979 of the then-known 1831 names
   were outside the subtree it scanned.
2. The next matched the `UFUNCTION` specifier with `[^)]*`, which stops at the inner paren of
   `meta = (AICallable)`. It found **zero** C++ toolsets and looked plausible doing it.
3. The next required `: public UToolsetDefinition` directly, and so missed all five
   `NiagaraToolsets` toolsets (57 tools), which derive from an intermediate `UNiagaraToolset`.
4. The next missed `UInputToolset` because it is a `ULocalPlayerSubsystem` implementing
   **`IToolsetProvider`**, not a `UToolsetDefinition` subclass at all. `UToolsetDefinition` is
   itself just `UObject, IToolsetProvider` — the interface is the real abstraction, and 48 more
   classes in `Plugins` plus 4 in `Source` implement it directly, including
   `ControlRigSequencerEditorLibrary` (230 tools), `LevelSequenceEditorSubsystem` (136), the
   whole `SequencerScripting` extension-library family, `WidgetBlueprintEditorSubsystem` (23) and
   `ProceduralVegetationEditorSubsystem` (18).

The working method attributes every `UFUNCTION` carrying `AICallable` or `AIRuntimeCallable` to
its enclosing class by position, over every header in a module that contains a toolset. That
metadata is what `ToolsetRegistry` keys on, so a tool cannot hide from it however its owner is
declared. Result: **137 toolsets, 2073 tools.**

It is validated against the live registry rather than trusted: a headless editor run calling
`unreal.ToolsetRegistry.get_all_toolset_json_schemas()` reports 85 toolsets and 1616 tools
registered in this project, and **every one of them appears in the static index** — zero gaps,
the index being a strict superset because it also covers plugins this project has not enabled.
Any future audit should re-run that comparison before trusting the index.

### The surface is not the same as what is reachable

`FPlugin::IsEnabledByDefault` (`PluginManager.cpp:455-469`) disables an engine plugin whose
`EnabledByDefault` field is **absent**, not only one that sets it to `false` — the fallback is
`GetLoadedFrom() == Project`, and these are engine plugins. Of the 2073 tools, 540 are in plugins
enabled by default, 263 are in always-available `Engine/Source` modules, and **1270 are in
plugins that are off** until something turns them on.

`EditorToolset` is in that last group, and it carries `ObjectToolset`, `AssetToolset`,
`ActorToolset`, `LevelToolset`, `MaterialToolset`, `SkeletalMeshToolset`, `StaticMeshToolset`,
`EditorAppToolset`, `DataTableToolset`, `TextureToolset` and `UndoHistoryToolset` — the backbone
of nearly every cut in this document, referenced 218 times by VibeUE's own skills. Across the
plugin, 443 references pointed at 29 toolset plugins that were neither enabled nor declared as
VibeUE dependencies, so a user following the rewritten skills would have found the tools absent.
`VibeUE.uplugin` now declares the eighteen the skills route to, all `Optional` so a renamed or
dropped experimental toolset cannot stop VibeUE loading.

This is the part of an audit like this that is easy to skip: deciding a capability belongs to
Epic is only half of it, and the other half is making sure Epic's version is actually there.

### Known-stale items the pending cuts must fix

- `set_engine_ini_value` / `set_ini_value` **never reach disk today**: they call `GConfig->SetString`
  + `Flush` on a NoSave path and return `bSuccess=true` regardless. `Skills/frame-rate`'s
  workflow depends on them, so its caps 3-5 need repointing at
  `ConfigSettingsToolset.SetSectionProperties`.
- `Skills/materials/workflows.md` still claims "there is no layout_expressions API"; 5.9 ships
  `MaterialToolset.LayoutExpressions`.
- Epic's `ConsoleToolset` / `ConfigSettingsToolset` / `ObjectToolset` / `FileSystemToolset`
  UFUNCTIONs carry **no** `PythonCallable`, so skills that reach settings from `execute_python` must
  be rewritten to `call_tool`. This is also why `GetPIESettings`/`SetPIESettings` demote rather than
  delete: `vibeue.py`'s `GateSession` calls them from Python.
- `ReparentBone` survives the Epic rule but `Skills/skeleton/SKILL.md` records it as live-broken in
  UE 5.7 ("fails with a hierarchy-mismatch validation error and can leave the commit stalled"). It
  should go on broken-functionality grounds, which is a separate decision from duplication.

### Not yet done

**`InputService` injection — verdict verified, edit deliberately deferred.** 5.9's `InputToolset`
(`ApplyInput(entries, durationSeconds)` / `IsInputApplied(entries)`) is runtime injection on a local
player, and it covers **`InjectActionFor`** (same duration-hold semantics) and **`InjectKey`** (it
takes legacy `FKey` entries, with zero duration meaning a one-tick tap) — and does both for several
inputs at once, which VibeUE cannot. It does **not** cover `InjectAction` + `StopInjection`: it
always self-releases after the duration, so an indefinite hold released by a later call has no
equivalent. So: cut 2, keep 2.

This was implemented and then reverted on purpose. `InputInjectionTests.cpp` exercises the cut and
kept tools in the same three automation-test bodies (interleaved assertions, not separable blocks),
so removing the two tools means roughly ten surgical edits inside tests that also cover the survivors.
That deserves a focused pass rather than a hurried one; the headers and implementations are unchanged,
and the verdict above is what to execute.

**The §3 trims** are still open, and they are where most of the remaining tool count sits:
Blueprint (110 vs `UBlueprintToolset` 74 + `UBlueprintDslToolset` 7 + EditorToolset's graph DSL),
StateTree (99 vs `StateTreeToolset` 49/32-write + the engine binding API), Widget (43 vs
WidgetAnimation 10 + CommonUI 15 + MVVM 31 + WidgetStaticAnalysis 5), Skeleton (39 vs
`USkeletalMeshToolset` 22), EnvQuery (24 vs 14), BehaviorTree + Blackboard (34 vs
`UBehaviorTreeToolset` 25), the Material pair (52 vs Material 25 + MaterialInstance 14 + CurveAtlas
13), EngineSettings + ProjectSettings (29 vs ConfigSettings 12 + Console 5 + Logs 4), Performance
(17 vs PerfToolset 10 + RewindDebugger 10), Viewport (22 vs `UEditorAppToolset` 42 + SceneCapture 14),
Actor (11 vs `UActorToolset` 21 + `ULevelToolset` 36) and AssetDiscovery (7 vs `UAssetToolset` 24 +
FileSystem 9).

Each needs the same treatment the finished ones got: diff at tool level, check the skills for a
documented capability the tool names do not reveal (the `GameplayTagService` lesson), cut, sweep the
skills, build. Do not cut on name overlap alone.

- Converting the remaining backticked sub-doc *prose* mentions into inline links (optional; the
  Reference pages section already makes every page reachable).
