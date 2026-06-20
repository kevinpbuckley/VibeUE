# VibeUE Test-Prompt Status

Tracking sweep of every file under `test_prompts/`. Goal: confirm an agent can complete
each prompt **efficiently** using a mix of Epic's native toolsets and VibeUE's tools/skills.
Where something is broken or inefficient, we file a GitHub issue and move on.

## Process

- **Test with Sonnet, fix with Opus.** A Sonnet subagent drives the live editor through the
  prompt file and returns a structured pass/partial/fail report with root-cause + suggested fix
  per failure. Opus (main loop) triages, files issues, and applies code/skill fixes.
- **Serial only.** There is one live Unreal editor / one MCP endpoint, so exactly one test
  agent runs at a time. No parallel domain runs.
- **Issues, not detours.** Fixable gaps become GitHub issues on `kevinpbuckley/VibeUE`
  (label `test-sweep`). We keep moving through the prompts; batched fixes happen in a later pass.
- Each domain agent behaves like a real VibeUE agent: `ListSkills` → `GetSkills` for the relevant
  pack, then the efficient interaction model (`execute_python_code` workhorse + `call_tool`).

## Legend

⬜ Not started · 🔄 In progress · ✅ Pass (all/clean) · 🟡 Partial (issues filed) · ❌ Blocked/major gaps · ⏭️ Skipped

## Status

| Domain / File | Prompts | Status | P / Pt / F | Issues | Model | Notes |
|---|---:|:--:|:--:|---|---|---|
| Smoke_Test.md | 19 | 🟡 | 17/2/0 | #444✏️ #445✏️ | sonnet | pilot. #444/#445 **fixed** in canonical skill indexes (refPath rule + helper; param keyword). 2 of Sonnet's 5 proposals were hallucinated. Candidates pending verify: build_graph None on partial fail; compile return shape vs `result["success"]` |
| utilities/check_unreal_connection.md | 4 | ⬜ | – | – | – | |
| asset_management/Test_Open_And_Selected_Assets.md | 6 | ✅ | 6/0/0 | – | opus | Tested directly. Passes via Epic+VibeUE mix. VibeUE: is_asset_open, get_primary_content_browser_selection. Epic EditorAppToolset (call_tool): GetOpenAssets, GetSelectedAssets, OpenEditorForAsset. Added skill doc for the Epic complement. |
| assets/manage_asset.md | 22 | ✅ | 22/0/0 | – | sonnet | All green. Confirms #444 refPath guidance works (`asset_type:{refPath}` succeeded). find_assets("")=list-all; save_assets([])=save-dirty. |
| blueprint/blueprint_tests.md | 67 | 🟡 | 5/1/0 suites | #449 | sonnet+opus | Suites 1–5 PASS — **validates #444 refPath fix end-to-end**. Verified+fixed in skill: `int` not `int32` (authoritative type set documented), `add_object_variable` uses `object_class` {refPath}. #449 tracks remaining doc gaps (IA/self-call discovery, root-component order, macro-library creation). Suite 6 macro-creation deferred (agent's "crash" was a false MCP-disconnect, no dump). |
| enhanced_input/enhanced_input_test.md | 42 | ✅ | 42/0/0 | – | sonnet+opus | All green. Fixed `SwizzleAxis` modifier name (was `SwizzleInputAxisValues`). Verified `result.asset_path` examples are CORRECT (InputCreateResult has asset_path) — did NOT change them (Sonnet's claim there was a misread). |
| enum_struct/enum_struct_tests.md | 18 | ✅ | 18/0/0 | #453 | sonnet+opus | All 18 pass. **Verified bug:** search_structs/search_enums(bUserDefinedOnly=True) leak transient REINST ghosts — returned 50 results, all 50 /Engine/Transient ghosts, 0 real. |
| data_asset/data_asset_test.md | 20 | 🟡 | 17/2/6B | #451 | sonnet+opus | Works via native get/set_editor_property. **Verified:** no VibeUE DataAssetService + no data-asset skill exist (only engine DataAssetTools.create). 6 BLOCKED = test references non-DataAsset classes (PhysicalMaterial/SoundAttenuation/MPC). |
| data_table/data_table_test.md | 17 | 🟡 | 12/2/1 +2B | #452 | sonnet | Run cut short at Part 10 by an MCP **client** disconnect (NOT a crash — verified: editor alive, no dump, invalid path handled gracefully). Findings: get_schema exposes row-key as column; set_rows silent wrong-type coercion; no clear_rows; empty GameplayTag→"None". Re-run Parts 11–17 after reconnect. |
| gameplay-tags/gameplay_tags_tests.md | 10 | ✅ | 10/0/0 | #455 | sonnet+opus | All green; skill verified accurate (single-tag CRUD→engine GameplayTagsToolset, bulk add_tags on VibeUE). Minor: C++ docstring lists removed methods; RemoveTag error should name blocking assets. |
| color/color_testing.md | 12 | 🟡 | 0/7/5 | #450 | sonnet+opus | Core asset/node creation works + UE tuple `(R=,G=,B=,A=)` works everywhere, but friendly formats (hex/named/temp/arrays) NOT wired into setters despite FJsonValueHelper existing. **Verified in source:** MaterialNodeService.set_expression_property returns `true` on failure (false-positive, line 1004). |
| level_actors/level_actor_tests.md | 11 | ✅ | 11/0/0 | – | sonnet+opus | All 11 sections (36 steps) green; level restored. Fixed deprecated `TraceTypeQuery.TRACE_TYPE_QUERY1`→`ECC_VISIBILITY` in skill. |
| viewport/viewport_tests.md | 20 | ⬜ | – | – | – | |
| transactions/transactions.md | 11 | ⬜ | – | – | – | |
| logs/log_reader_tests.md | 28 | 🟡 | obsolete | #457 | sonnet+opus | **Test file obsolete** — built around the cut `read_logs` tool. Capability works via Epic `LogsToolset.GetLogEntries` (must pass `category=""` + `pattern=""` — bad default `"LogsToolset"` verified) + Python file I/O. Needs test rewrite. |
| materials/materials_tests.md | ~34 | 🟡 | 28/3/3 | #459 (+#450) | sonnet | F1 re-confirms #450. NEW: set_instance_parameters_bulk mis-parses hex (#FF00FF→white) & array (drops R); batch_create_expressions silent-fails on bad class ("Lerp"→use LinearInterpolate); list_expressions missing param_name for TextureObject; NodeTest stuck-delete after MEL (leftover in /Game/Tests/Materials/). |
| animation-blueprint/animation_blueprint.md | 74 | ❌ | – | #446 | – | **BLOCKS editor**: compile→fatal cast at KismetCompiler.cpp:5903 (`ReplaceConvertibleDelegates`, Epic CastChecked). The produced ABP is a *poison asset* (crashes on **load** too) — quarantined to `_crash_artifacts/`. Engine bug; binary-engine build means a source patch won't apply. Fix approach TBD (VibeUE pre-compile guard vs source-engine patch). |
| animation-editing.md | 25 | 🟡 | 21/2/2 | #447 | sonnet | No crash (safe domain). Fixed: percentile5/95 doc naming. Filed: learn_from_animations sampling, preview_pose_delta None-on-fail, manual-constraint non-enforcement. Refuted Sonnet claim that BoneDelta(kwargs) fails — it works. |
| animsequence/animsequence_tests.md | 57 | 🟡 | 43/11/3 | #448 | sonnet | No crash. Verified: search_animations is exact-match-only (core bug). + notify_class empty, track-name not persisted, morph flag, sync-marker float match. 2 confirmed-known (frame_rate=60→30, list duration=0). |
| montage/montage_tests.md | 30 | ✅ | 30/0/0 | #461 | sonnet+opus | All 30 green. Fixed skill b_loops/b_is_loop→loops/is_loop. Minor: list_montages rejects None filter (use ""); no set_playback_rate_scale method. |
| skeleton/skeleton_tests.md | 43 | 🟡 | 38/3/2 | #466 | sonnet | Socket/retarget/curve-meta work. ⚠️ **DAMAGED SK_Mannequin** (4 stuck bones + orphan blend profiles — reparent_bone corrupts SkeletonModifier, silent false-positive commits). rename_bone duplicates; add_bone hits SHARED skeleton; no delete_blend_profile. **SK_Mannequin may need revert/re-import (restart if unsaved).** |
| state-trees/state_trees_tests.md | 83 | 🟡 | 58/13/2 +10B | #468 | sonnet+opus | Large domain, no crash. Most of StateTreeService works (states/params/transitions/tasks/conditions/evaluators/considerations/linked/bindings). 10 BLOCKED = missing fixtures (ST_Cube asset, BP_Cube_1/2 actors w/ StateTreeComponent). **Verified bug:** `move_task` returns True but never reorders (#468). **Refuted 2 Sonnet "FAIL"s:** transition enabled/delay DO persist with documented gate flags `set_enabled`/`set_delay` — Sonnet omitted gates AND skill docs (changelog.md/blueprint-tasks.md) showed wrong `b_`-prefixed names → **fixed**. Other PARTIALs were natural-language prompt wording (int→Int32, "Value"→Constant, "LinkedSubtree"→set_linked_subtree) — skill is correct. |
| umg/manage_umg_widget.md | 29 | ⬜ | – | – | – | |
| umg/viewmodel_binding.md | 5 | ⬜ | – | – | – | |
| umg/widget_hierarchy.md | 4 | ⬜ | – | – | – | |
| niagara/niagara_test.md | ~62 | 🟡 | 42/8/4 | #462 #463 | sonnet+opus | ScratchPadService solid; engine NiagaraToolsets CRUD works. **Verified:** NiagaraService/EmitterService docstrings advertise unbound methods (move/duplicate/copy_emitter, get_module_info...); no emitter dup/copy; Niagara delete_asset fails. Agent's "AssetEditorSubsystem broken" = interpreter namespace pollution (#463), restored. |
| niagara/scratchpad_trackpainter.md | 9 | ❌ | deferred | #464 | – | NOT run — niagara user-param/read ops **crash the editor** (#464: AddUserVariables→NiagaraVariant assert; reads on the corrupted system→array-OOB). Scratchpad core already validated solid in #462. Added 🛑 crash guard to niagara-systems skill steering away from AddUserVariables. |
| metasounds/metasounds_tests.md | ~38 | ✅ | 36/2/0 | #460 | sonnet | MetaSoundService solid (graph/node CRUD, IO, scenarios all work). API gaps only: no read-back of node input defaults / connections; AudioMixer name convention undocumented; batch-delete 30s timeout. |
| sound-cues/sound_cues_tests.md | 45 | ✅ | 42/2/0 | #467 | sonnet+opus | Clean domain, no crashes. SoundCueService solid (lifecycle, all 14 node types, connect/disconnect/move/remove, props, all 6 E2E scenarios). Only gap: QualityLevel node has fixed single slot (project AudioQualitySettings-driven, no num_inputs) — Scenario 6 can't add slots (#467). Verified+refuted Sonnet's "vol should be 1.0" (0.75 is engine default). Added skill notes (QL slots + 0.75 default). |
| foliage/foliage_tests.md | 31 | ✅ | 26/2/0 +2B | #454 | sonnet | FoliageService clean/complete. Issues are mostly test-file expectation bugs (GroundSlopeAngle vs AlignMaxAngle, origin-centered rect, missing paint layers). 1 behavior note: add_foliage_instances places off-landscape points at z=0 (doc says reject). |
| landscape/landscape_tests.md | ~77 | 🟡 | 63/4/5 +8B | #456 | sonnet | No disconnect. 8 BLOCKED = missing content packs (Stylized_Spruce_Forest, Landscape1–4, Real_Landscape). Bugs: get_hole_at_location always False; layer cache needs 2nd assign_material; weightmap round-trip 1.0→0.557; RVT params remapped; material_exists needs full path. |
| map-blockout/map_blockout_tests.md | ~39 | ✅ | 28/0/0 +11B | #458 | sonnet | Core pipeline solid (all stages + PNGs). 11 BLOCKED = no Landscape1 in skeleton map. Real items: Forest-Surrounded-POI accepts treelines; target_count floor clamp. Doc gaps: rel-path, river return type. (foliage-partition crash noted on #454.) |
| pcg/pcg_tests.md | 44 | ✅ | 43/1/0 | #465 | sonnet | PCG skills comprehensive & accurate; native PCG Python covers all. Only minor: is_standalone_graph deprecated→GraphUsageContext; test node-count wording. 1 PARTIAL = documented Windows file-handle delete limit (PCG_TestGraph leftover, clears on restart). |
| uv-mapping/uv_mapping_tests.md | ? | ⬜ | – | – | – | header format differs (0 `##`) |
| terrain-data/terrain_data_tests.md | 27 | ⏭️ | 1/0/13B | – (config) | sonnet+opus | **BLOCKED — no API key.** `terrain_data` requires `[VibeUE] VibeUEApiKey` in `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`; not set on this project (verified via `TerrainDataTools.cpp:82` + `Saved/Terrain/` absent). `list_styles` works (key-free, 5 styles). All other actions return a **clean, correct `NO_API_KEY` error** — error handling verified good. Not a tool bug; needs user to configure their key, then re-run. Full-build sections skipped (dup of landscape #456 + map-blockout #458). |
| terrain-data/issues.md | 10 | ⏭️ | n/a | – | opus | **Not a test suite** — a pre-existing manual water-features bug report (2026-03-01): 8 issues (empty name/class on water bodies/waterways, tile-boundary dedup, coordinate-offset alignment, black screenshots). Mostly **server-side data-pipeline** (`water-features.ts`), not the UE plugin. Can't reproduce without an API key. Out of scope for this sweep; left as reference. |
| deep-research/deep_research_tests.md | ~50 | ✅ | core green | – | opus | Tested core directly: search/fetch_page/geocode/reverse_geocode all work; error handling clean (MISSING_PARAMS, UNKNOWN_ACTION) — no hangs. Note: fetch_page can return very large output (e.g. Wikipedia 154k chars). Integration/landscape-build section deferred to landscape+terrain-data domains. |
| markdown_rendering_test.md | 3 | ⬜ | – | – | – | chat rendering check |
| demo_prompts.md | ? | ⬜ | – | – | – | header format differs (0 `##`) |

**Totals:** ~924 prompts / 39 files.

## Filed issues

- [#444](https://github.com/kevinpbuckley/VibeUE/issues/444) — Blueprints skill shows string args for engine `BlueprintTools`; 5.8 schema requires `{refPath}` objects. **Cross-cutting** (recurs in every Blueprint domain).
- [#445](https://github.com/kevinpbuckley/VibeUE/issues/445) — `add_function_parameter` skill example passes default value into the `is_output` positional slot.
- [#446](https://github.com/kevinpbuckley/VibeUE/issues/446) — 🔴 **CRASH**: compiling an Anim Blueprint with a state machine via `BlueprintTools.compile_blueprint` → fatal `AnimationStateMachineSchema → EdGraphSchema_K2` cast. Blocks all anim domains until a crash-safe compile exists.
- [#467](https://github.com/kevinpbuckley/VibeUE/issues/467) — `SoundCueService.add_quality_level_node` creates a fixed single-slot node (project `AudioQualitySettings`-driven); no `num_inputs`, so multi-quality cues can't be wired. Low severity.
- [#468](https://github.com/kevinpbuckley/VibeUE/issues/468) — `StateTreeService.move_task` returns True but never reorders the Tasks array (silent no-op). + secondary: `set_selection_behavior` accepts unknown enum strings silently. Skill docs `b_`-prefixed kwarg/field names for `update_transition` fixed inline.

## Process notes (learned from pilot)

- **Sonnet diagnoses are unreliable (~50%).** In the pilot, 2 of 5 proposed issues were hallucinated skill content and 1 was wrong about an API that actually exists. **Opus must verify every proposed issue against the live editor / actual skill files before filing.** "Fix with Opus" really means "verify + file + fix with Opus."
- **#444 is the dominant failure mode** and will recur across all Blueprint-related domains — high leverage to fix early so domain sweeps produce clean signal instead of re-discovering it.
- **Test agents must NEVER relaunch the editor / run BuildAndLaunch / kill processes.** On the blueprint run, an agent hit a transient MCP disconnect, assumed a crash, ran the relaunch script, and broke the MCP client connection for the whole session. Harness rule added: if MCP becomes unreachable, STOP and report — never relaunch, never retry-loop. (Working as intended on the data_table run.)
- **execute_python_code shares a mutable global namespace for the whole session (#463).** Mid-sweep, `unreal.AssetEditorSubsystem` got shadowed by an instance (broke `get_editor_subsystem`). Restored at runtime via `load_class`. Future agents: if `get_editor_subsystem(unreal.AssetEditorSubsystem)` errors with a Class/instance TypeError, use `unreal.load_class(None,"/Script/UnrealEd.AssetEditorSubsystem")`.
- **Recurring MCP client disconnects are the main blocker.** The `unreal-mcp` client transport has dropped 3× mid-sweep (blueprint, data_table), each requiring a manual user reconnect. The editor stays alive each time (no crash dumps). Pattern: drops tend to occur during **error-handling test sections** that deliberately pass invalid input (empty/invalid `refPath`, etc.) — hypothesis: an unguarded edge case throws/hangs the MCP request thread and the client gives up. Worth investigating server-side; meanwhile, smaller per-agent scopes reduce exposure.
