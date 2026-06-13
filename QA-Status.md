# VibeUE Test Prompt QA Status

Tracks results of running each `test_prompts/` file end-to-end through the in-editor chat
(`manage_editor_chat`, model: deepseek/deepseek-v4-flash), reviewing the chat log, and fixing
skills / services to reduce token usage and time-to-complete for common tasks.

Legend: ✅ pass · ⚠️ pass with fixes applied · ❌ blocked (git issue filed) · ⏳ pending · 🚫 skipped

| # | Test file | Status | Notes |
|---|-----------|--------|-------|
| 1 | niagara/scratchpad_trackpainter.md | ✅ | Completed in prior session (branch fix verified). |
| 2 | animation-editing.md | ⚠️ | 27/27 prompts passed. Added 3 gotchas to animation-editing/common-mistakes.md. |
| 3 | demo_prompts.md | ⚠️ | 7/9 done (2 referenced non-existent assets; graceful fallback). Added AttributeError-trap table to level-actors. |
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

### 3. demo_prompts.md — ⚠️ pass with fixes
Open-ended creative grab-bag (event graph, build Mario castle from basic shapes, sky text, lights,
niagara theming). 7/9 fully succeeded; 2 referenced assets that don't exist in this project
(no "Weapon" AnimBP → failed; `NS_Fire_Big_2` missing → fell back to `NS_Fire`, partial). The agent
handled missing assets gracefully (no crash). Castle = 48 actors + 6 MI materials at floor height.

**Friction points (wasted discovery iterations):** `StaticMeshComponent.get_static_mesh()`,
`actor.get_components()`, `DirectionalLight.directional_light_component`, light `cast_shadow`
(should be `cast_shadows`), and `unreal.HorizontalTextAligment` (should be `HorizTextAligment`) all
raised AttributeError.

**Fix applied:** Added a "Verified AttributeError traps (UE 5.7 Python)" table to
`Content/Skills/level-actors/SKILL.md` covering all five, plus a "discover the class once instead of
guessing" note.
