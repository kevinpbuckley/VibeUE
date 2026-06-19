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
| assets/manage_asset.md | 7 | ⬜ | – | – | – | |
| blueprint/blueprint_tests.md | 67 | ⬜ | – | – | – | 6 sub-suites |
| enhanced_input/enhanced_input_test.md | 7 | ⬜ | – | – | – | |
| enum_struct/enum_struct_tests.md | 18 | ⬜ | – | – | – | |
| data_asset/data_asset_test.md | 20 | ⬜ | – | – | – | |
| data_table/data_table_test.md | 17 | ⬜ | – | – | – | |
| gameplay-tags/gameplay_tags_tests.md | ? | ⬜ | – | – | – | header format differs (0 `##`) |
| color/color_testing.md | 31 | ⬜ | – | – | – | |
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
| foliage/foliage_tests.md | 14 | ⬜ | – | – | – | |
| landscape/landscape_tests.md | 77 | ⬜ | – | – | – | |
| map-blockout/map_blockout_tests.md | 28 | ⬜ | – | – | – | |
| pcg/pcg_tests.md | 22 | ⬜ | – | – | – | |
| uv-mapping/uv_mapping_tests.md | ? | ⬜ | – | – | – | header format differs (0 `##`) |
| terrain-data/terrain_data_tests.md | 27 | ⬜ | – | – | – | uses terrain_data tool |
| terrain-data/issues.md | 10 | ⬜ | – | – | – | known-issues list, not a test suite? |
| deep-research/deep_research_tests.md | 14 | ⬜ | – | – | – | uses deep_research tool |
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
