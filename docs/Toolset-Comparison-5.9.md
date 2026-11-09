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

VibeUE today is off-convention on three counts:

1. Skills live in **`Content/Skills/`**, which is *not* a discovery root — only `<Plugin>/Skills/` is.
2. A **116-line Python shim** (`Content/Python/init_unreal.py`) reads each markdown file and
   synthesises a `UAgentSkill` subclass per file, with CDO-updating reload support
   (`VibeUE.ReloadSkills`). All of that is now the engine's job.
3. The shim registers **every file** as its own skill (39 `SKILL.md` + 53 sub-docs = 92 markdown
   files). The native model registers **39** skills and expects sub-docs to be reached through
   relative links — but only **3 of 39** `SKILL.md` files currently link their sub-docs as
   `[text](file.md)`. The other 36 name them in backticks (`` `api-bindings` ``), which worked only
   because the shim registered each separately. Under native discovery those pages become
   unreachable.

**Migration:** move `Content/Skills/` → `Skills/`, convert backticked sub-doc references into
relative markdown links in the 36 affected `SKILL.md` files, delete the Python shim and the
`ReloadSkills` command. All 39 `SKILL.md` files already carry `name:` + `description:` frontmatter, so
no frontmatter work is needed. Verify a top-level `Skills/` folder survives UAT `BuildPlugin`
packaging for the FAB build; if it does not, keep `Content/Skills/` and call
`AddSearchRoot(<ContentDir>/Skills, "VibeUE")` from `Module.cpp` — that yields the identical
`/Skills/VibeUE/...` keys.

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

**Not yet started:** everything in §3, and the §4 skills migration.
