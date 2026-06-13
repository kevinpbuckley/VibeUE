# VibeUE Test Prompt QA Status

Tracks results of running each `test_prompts/` file end-to-end through the in-editor chat
(`manage_editor_chat`, model: deepseek/deepseek-v4-flash), reviewing the chat log, and fixing
skills / services to reduce token usage and time-to-complete for common tasks.

Legend: ✅ pass · ⚠️ pass with fixes applied · ❌ blocked (git issue filed) · ⏳ pending · 🚫 skipped

| # | Test file | Status | Notes |
|---|-----------|--------|-------|
| 1 | niagara/scratchpad_trackpainter.md | ✅ | Completed in prior session (branch fix verified). |
| 2 | animation-editing.md | ⚠️ | 27/27 prompts passed. Added 3 gotchas to animation-editing/common-mistakes.md. |
| 3 | demo_prompts.md | ⏳ | |
| 4 | pcg/pcg_tests.md | ⏳ | |
| 5 | skeleton/skeleton_tests.md | ⏳ | |
| 6 | Smoke_Test.md | ⏳ | |
| 7 | sound-cues/sound_cues_tests.md | ⏳ | |
| 8 | state-trees/state_trees_tests.md | ⏳ | |
| 9 | terrain-data/terrain_data_tests.md | ⏳ | |
| 10 | transactions/transactions.md | ⏳ | |
| 11 | umg/manage_umg_widget.md | ⏳ | |
| 12 | umg/viewmodel_binding.md | ⏳ | |
| 13 | umg/widget_hierarchy.md | ⏳ | |
| 14 | utilities/check_unreal_connection.md | ⏳ | |
| 15 | uv-mapping/uv_mapping_tests.md | ⏳ | |
| 16 | viewport/viewport_tests.md | ⏳ | |

---

## Detailed results

### 2. animation-editing.md — ⚠️ pass with fixes
**Result:** 27/27 prompts passed (skeleton profile, learn constraints, preview/validate/bake,
manual hinge constraints, copy/mirror pose, retarget preview, 5 complex animation builds).
Skeleton used: `SK_UEFN_Mannequin` (101 bones); 131 skeletons in project.

**Friction points (cost extra tool iterations):**
- Agent tried `transform.location` on an `unreal.Transform` → `AttributeError`; recovered via
  `discover_python_class`. Field is `.translation` (+`.rotation`/`.scale3d`).
- No `duplicate_anim_sequence` method — agent guessed, then used `manage_asset(action="duplicate")`.
- `create_from_pose` returned a path without the object suffix; needed manual `.AssetName` append.

**Fix applied:** Added all three as concrete gotchas to
`Content/Skills/animation-editing/common-mistakes.md` so future runs skip the trial-and-error.

**Not bugs (expected behavior):** clamping on previews when learned constraints are tight (only
10 anims sampled), `copy_pose` informational track-overwrite warnings.
