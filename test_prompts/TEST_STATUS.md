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
| level_actors/level_actor_tests.md | 11 | ⬜ | – | – | – | |
| viewport/viewport_tests.md | 20 | ⬜ | – | – | – | |
| transactions/transactions.md | 11 | ⬜ | – | – | – | |
| logs/log_reader_tests.md | 28 | ⬜ | – | – | – | |
| materials/materials_tests.md | 31 | ⬜ | – | – | – | |
| animation-blueprint/animation_blueprint.md | 74 | ❌ | – | #446 | – | **BLOCKS editor**: compile→fatal cast at KismetCompiler.cpp:5903 (`ReplaceConvertibleDelegates`, Epic CastChecked). The produced ABP is a *poison asset* (crashes on **load** too) — quarantined to `_crash_artifacts/`. Engine bug; binary-engine build means a source patch won't apply. Fix approach TBD (VibeUE pre-compile guard vs source-engine patch). |
| animation-editing.md | 25 | 🟡 | 21/2/2 | #447 | sonnet | No crash (safe domain). Fixed: percentile5/95 doc naming. Filed: learn_from_animations sampling, preview_pose_delta None-on-fail, manual-constraint non-enforcement. Refuted Sonnet claim that BoneDelta(kwargs) fails — it works. |
| animsequence/animsequence_tests.md | 57 | 🟡 | 43/11/3 | #448 | sonnet | No crash. Verified: search_animations is exact-match-only (core bug). + notify_class empty, track-name not persisted, morph flag, sync-marker float match. 2 confirmed-known (frame_rate=60→30, list duration=0). |
| montage/montage_tests.md | 10 | ⬜ | – | – | – | |
| skeleton/skeleton_tests.md | 44 | ⬜ | – | – | – | |
| state-trees/state_trees_tests.md | ? | ⬜ | – | – | – | header format differs (0 `##`) |
| umg/manage_umg_widget.md | 29 | ⬜ | – | – | – | |
| umg/viewmodel_binding.md | 5 | ⬜ | – | – | – | |
| umg/widget_hierarchy.md | 4 | ⬜ | – | – | – | |
| niagara/niagara_test.md | 15 | ⬜ | – | – | – | |
| niagara/scratchpad_trackpainter.md | 9 | ⬜ | – | – | – | |
| metasounds/metasounds_tests.md | 39 | ⬜ | – | – | – | |
| sound-cues/sound_cues_tests.md | 45 | ⬜ | – | – | – | |
| foliage/foliage_tests.md | 31 | ✅ | 26/2/0 +2B | #454 | sonnet | FoliageService clean/complete. Issues are mostly test-file expectation bugs (GroundSlopeAngle vs AlignMaxAngle, origin-centered rect, missing paint layers). 1 behavior note: add_foliage_instances places off-landscape points at z=0 (doc says reject). |
| landscape/landscape_tests.md | 77 | ⬜ | – | – | – | |
| map-blockout/map_blockout_tests.md | 28 | ⬜ | – | – | – | |
| pcg/pcg_tests.md | 22 | ⬜ | – | – | – | |
| uv-mapping/uv_mapping_tests.md | ? | ⬜ | – | – | – | header format differs (0 `##`) |
| terrain-data/terrain_data_tests.md | 27 | ⬜ | – | – | – | uses terrain_data tool |
| terrain-data/issues.md | 10 | ⬜ | – | – | – | known-issues list, not a test suite? |
| deep-research/deep_research_tests.md | ~50 | ✅ | core green | – | opus | Tested core directly: search/fetch_page/geocode/reverse_geocode all work; error handling clean (MISSING_PARAMS, UNKNOWN_ACTION) — no hangs. Note: fetch_page can return very large output (e.g. Wikipedia 154k chars). Integration/landscape-build section deferred to landscape+terrain-data domains. |
| markdown_rendering_test.md | 3 | ⬜ | – | – | – | chat rendering check |
| demo_prompts.md | ? | ⬜ | – | – | – | header format differs (0 `##`) |

**Totals:** ~924 prompts / 39 files.

## Filed issues

- [#444](https://github.com/kevinpbuckley/VibeUE/issues/444) — Blueprints skill shows string args for engine `BlueprintTools`; 5.8 schema requires `{refPath}` objects. **Cross-cutting** (recurs in every Blueprint domain).
- [#445](https://github.com/kevinpbuckley/VibeUE/issues/445) — `add_function_parameter` skill example passes default value into the `is_output` positional slot.
- [#446](https://github.com/kevinpbuckley/VibeUE/issues/446) — 🔴 **CRASH**: compiling an Anim Blueprint with a state machine via `BlueprintTools.compile_blueprint` → fatal `AnimationStateMachineSchema → EdGraphSchema_K2` cast. Blocks all anim domains until a crash-safe compile exists.

## Process notes (learned from pilot)

- **Sonnet diagnoses are unreliable (~50%).** In the pilot, 2 of 5 proposed issues were hallucinated skill content and 1 was wrong about an API that actually exists. **Opus must verify every proposed issue against the live editor / actual skill files before filing.** "Fix with Opus" really means "verify + file + fix with Opus."
- **#444 is the dominant failure mode** and will recur across all Blueprint-related domains — high leverage to fix early so domain sweeps produce clean signal instead of re-discovering it.
- **Test agents must NEVER relaunch the editor / run BuildAndLaunch / kill processes.** On the blueprint run, an agent hit a transient MCP disconnect, assumed a crash, ran the relaunch script, and broke the MCP client connection for the whole session. Harness rule added: if MCP becomes unreachable, STOP and report — never relaunch, never retry-loop. (Working as intended on the data_table run.)
- **Recurring MCP client disconnects are the main blocker.** The `unreal-mcp` client transport has dropped 3× mid-sweep (blueprint, data_table), each requiring a manual user reconnect. The editor stays alive each time (no crash dumps). Pattern: drops tend to occur during **error-handling test sections** that deliberately pass invalid input (empty/invalid `refPath`, etc.) — hypothesis: an unguarded edge case throws/hangs the MCP request thread and the client gives up. Worth investigating server-side; meanwhile, smaller per-agent scopes reduce exposure.
