# Animation Editing Test Prompts

This file contains sequential test prompts for the animation editing workflow.

## Prerequisites

- An Unreal Engine project with skeletal meshes and animations
- The UE5 Mannequin or similar humanoid skeleton
- At least 10 existing animations for learning constraints

## Expected Content Paths

- Skeleton: `/Game/Characters/Mannequin/Mesh/SK_Mannequin` (or similar)
- Animations: `/Game/Animations/` directory with walk, run, idle animations

## Test Order

1. Skeleton Profile Creation
2. Learn Constraints from Animations
3. Preview Single Bone Rotation
4. Preview Multi-Bone Atomic Edits
5. Validate and Bake Edits
6. Constraint Violation Detection
7. Copy and Mirror Pose Utilities
8. Retarget Preview
9. Complex Animation Creation

---

# 1. Skeleton Profile Creation

## Goal
Create and inspect a skeleton profile to understand bone hierarchy and constraints.

---

## Prompt 1.1: Find a skeleton in the project

Find all skeleton assets in the project. List the first 5 skeletons with their paths.

---

## Expected Result
- Lists skeleton paths like `/Game/Characters/.../SK_...`
- Shows skeleton names and bone counts

---

## Prompt 1.2: Create skeleton profile

Create a skeleton profile for the UE5 Mannequin skeleton (or first available skeleton). Show:
- Skeleton name
- Bone count
- Whether learned constraints are available
- List the first 10 bones in the hierarchy with their parents

---

## Expected Result
- Profile created successfully
- Bone hierarchy displayed with parent relationships
- `has_learned_constraints` shows False initially

---

## Prompt 1.3: Inspect arm bones

For the same skeleton, find all bones with "arm" in the name. For each, show:
- Bone name
- Parent bone name
- Depth in hierarchy
- Number of children

---

## Expected Result
- Shows bones like: clavicle, upperarm, lowerarm, hand
- Hierarchy depth increases from clavicle to hand
- Parent-child relationships are correct

---

# 2. Learn Constraints from Animations

## Goal
Analyze existing animations to learn realistic bone rotation ranges.

---

## Prompt 2.1: Find animations for the skeleton

List all animation sequences that use the UE5 Mannequin skeleton (or the skeleton from test 01). Show the first 10 animations with:
- Animation name
- Duration
- Frame count

---

## Expected Result
- Lists animations like Walk, Run, Idle, Jump, etc.
- Shows duration and frame counts

---

## Prompt 2.2: Learn constraints from animations

Learn bone constraints from the first 30 animations, sampling **keyframes or evenly spaced frames** (not 15 consecutive frames). Show:
- Number of animations analyzed
- Total samples collected
- For `upperarm_r` and `lowerarm_r`: min/max rotation ranges and 5th/95th percentile values

---

## Expected Result
- Shows `animation_count` and `total_samples`
- Per-bone ranges show realistic values:
  - `lowerarm` should have limited pitch range (elbow only bends one direction)
  - `upperarm` should have wider rotation freedom

---

## Prompt 2.3: Verify learned constraints cached

Get the skeleton profile again and verify that `has_learned_constraints` is now True.

---

## Expected Result
- `profile.has_learned_constraints` returns True
- Learned ranges are included in the profile

---

# 3. Preview Single Bone Rotation

## Goal
Preview a single bone rotation edit using the preview → validate → bake workflow.

---

## Prompt 3.1: Get an animation to edit

Find an idle or simple looping animation. Create a duplicate called "AS_Test_Preview" to avoid modifying original assets.

---

## Expected Result
- Animation duplicated successfully
- Path to new animation returned

---

## Prompt 3.2: Preview upper arm rotation

Preview rotating `upperarm_r` by 45 degrees on the pitch axis (raising the arm) at frame 0 in local space. Show:
- Whether preview was successful
- Which bones were modified
- Whether any clamping occurred

---

## Expected Result
- `success` is True
- `modified_bones` contains "upperarm_r"
- `was_clamped` is False (45 degrees is within normal range)

---

## Prompt 3.3: Check preview state

Get the current preview state for the animation. Show:
- Whether preview is active
- Number of pending edits
- Bones with pending edits

---

## Expected Result
- `is_active` is True
- `pending_edit_count` is 1
- `pending_bones` contains "upperarm_r"

---

## Prompt 3.4: Cancel the preview

Cancel the pending preview and verify the preview state is now inactive.

---

## Expected Result
- Preview cancelled successfully
- `is_active` becomes False
- `pending_edit_count` becomes 0

---

# 4. Preview Multi-Bone Atomic Edits

## Goal
Edit multiple bones atomically as part of a coordinated movement (like raising an arm).

---

## Prompt 4.1: Create arm wave deltas

Using the test animation from the previous test, create a preview with these simultaneous bone deltas:
- `clavicle_r`: Yaw +15 degrees
- `upperarm_r`: Pitch +90 degrees (raise arm sideways)
- `lowerarm_r`: Pitch +45 degrees (bend elbow)
- `hand_r`: Pitch -20 degrees, Yaw +30 degrees (wave angle)

Preview at frame 0 in local space.

---

## Expected Result
- All 4 bones modified in single atomic operation
- `modified_bones` contains all 4 bone names
- No errors or constraint violations

---

## Prompt 4.2: Verify atomic behavior

Try to preview an edit that includes an invalid bone name (e.g., "fake_bone_xyz"). Verify that:
- The entire operation fails (atomic)
- No bones are modified
- Error message mentions the invalid bone

---

## Expected Result
- `success` is False
- `error_message` mentions "fake_bone_xyz"
- Preview state shows no pending edits (rollback occurred)

---

## Prompt 4.3: Preview again and check state

Re-apply the valid 4-bone edit from Prompt 4.1. Check that:
- Preview state shows 4 pending bones
- Pending bones match our edited bones

---

## Expected Result
- `pending_edit_count` is 4
- `pending_bones` contains clavicle_r, upperarm_r, lowerarm_r, hand_r

---

# 5. Validate and Bake Edits

## Goal
Validate previewed edits against constraints and bake them to keyframes.

---

## Prompt 5.1: Validate the current pose

With the 4-bone preview from test 04 still active, validate the pose using learned constraints. Show:
- Whether pose is valid
- Number of passed/failed bones
- Any violation messages

---

## Expected Result
- `is_valid` is True (our edits are within normal ranges)
- `passed_count` matches total bones checked
- `failed_count` is 0
- No violation messages

---

## Prompt 5.2: Bake to keyframes

Bake the previewed edits to all frames (0 to -1) using cubic interpolation. Show:
- Whether bake succeeded
- Frame range affected
- Bones modified

---

## Expected Result
- `success` is True
- `start_frame` is 0
- `end_frame` is the last frame of the animation
- `modified_bones` contains our 4 arm bones

---

## Prompt 5.3: Verify preview cleared

After baking, check that:
- Preview state is no longer active
- Pending edit count is 0

---

## Expected Result
- `is_active` is False
- `pending_edit_count` is 0
- Edits are now permanent in the animation data

---

## Prompt 5.4: Verify changes persisted

Get the bone transform at frame 0 for `upperarm_r`. Verify that the pitch value reflects our 90-degree edit (compared to reference pose).

---

## Expected Result
- Bone transform shows increased pitch rotation
- Delta from reference pose is approximately 90 degrees

---

# 6. Constraint Violation Detection

## Goal
Test that constraint violations are properly detected and reported.

---

## Prompt 6.1: Set manual elbow constraint

Set a manual constraint on `lowerarm_r` as a hinge joint:
- Min rotation: (0, 0, 0)
- Max rotation: (0, 145, 0) - elbow can only bend up to 145 degrees
- Is hinge: True
- Hinge axis: 1 (Pitch/Y)

---

## Expected Result
- Constraint set successfully
- Bone is now marked as a hinge joint

---

## Prompt 6.2: Preview an impossible rotation

Create a new test animation or use an existing one. Preview rotating `lowerarm_r` by:
- Pitch: 180 degrees (exceeds 145 degree limit)
- Roll: 45 degrees (violates hinge constraint - non-hinge axis)

---

## Expected Result
- Preview succeeds but `was_clamped` is True
- Rotation was clamped to valid range

---

## Prompt 6.3: Validate and see violations

Validate the pose (using manual constraints, not learned). Show:
- Violation type for lowerarm_r
- Original vs clamped rotation values
- Violation message

---

## Expected Result
- `is_valid` is False
- `violating_bones` contains "lowerarm_r"
- Violation type is "MaxExceeded" or "HingeViolation"
- Clamped rotation shows pitch capped at 145

---

## Prompt 6.4: Validate a single bone rotation

Use `validate_bone_rotation` directly to test:
- Bone: lowerarm_r
- Rotation: (0, 160, 0) - exceeds limit

Show the validation result details.

---

## Expected Result
- `is_valid` is False
- `violation_type` is "MaxExceeded"
- `original_rotation` shows (0, 160, 0)
- `clamped_rotation` shows (0, 145, 0)
- Message explains the violation

---

## Prompt 6.5: Cancel and cleanup

Cancel any pending previews to clean up the test state.

---

## Expected Result
- Preview cancelled
- No pending edits remain

---

# 7. Copy and Mirror Pose Utilities

## Goal
Test the copy_pose and mirror_pose utility functions.

---

## Prompt 7.1: Create a source animation with a distinct pose

Find an animation with a raised arm or wave motion. Identify a frame where the right arm is clearly posed differently from frame 0.

---

## Expected Result
- Animation found with distinct poses
- Frame number identified (e.g., frame 15 has arm raised)

---

## Prompt 7.2: Copy pose between frames

Copy the pose from the distinct frame to frame 0 of a new test animation. Copy all bones. Show:
- Number of bones copied
- Success status

---

## Expected Result
- `success` is True
- `modified_bones` shows all skeleton bones
- Frame 0 now matches the source frame's pose

---

## Prompt 7.3: Copy only arm bones

Create another test animation. Copy only the right arm bones (clavicle_r, upperarm_r, lowerarm_r, hand_r) from the source frame to frame 10.

---

## Expected Result
- Only 4 bones in `modified_bones`
- Other bones (spine, left arm, legs) unchanged
- Right arm pose matches source

---

## Prompt 7.4: Mirror a pose

Take the animation with the right arm raised and mirror frame 15 across the X axis. Show:
- Number of bones mirrored
- Success status

---

## Expected Result
- `success` is True
- Left/right bone pairs were swapped
- arm_l bones now have arm_r transforms and vice versa

---

## Prompt 7.5: Verify mirror result

Get the pose at the mirrored frame and compare left vs right arm bone rotations. They should be swapped from the original.

---

## Expected Result
- upperarm_l now has the rotation that upperarm_r had
- upperarm_r now has the rotation that upperarm_l had
- Same for other arm bones

---

# 8. Retarget Preview

## Goal
Test previewing an animation on a different skeleton (retargeting).

---

## Prompt 8.1: Find a second compatible skeleton

List all skeletons in the project. Find one that is different from the UE5 Mannequin but might be compatible (similar bone structure).

If no second skeleton exists, note that retarget preview requires a second skeleton to test.

---

## Expected Result
- List of available skeletons
- Identification of a potential target skeleton (or note if only one exists)

---

## Prompt 8.2: Set up retarget preview

If a second skeleton exists, preview a run or walk animation on the target skeleton. Show:
- Whether preview was set up successfully
- Any warnings or compatibility issues

---

## Expected Result (if second skeleton exists)
- `success` indicates whether retarget is possible
- `messages` contain any bone mapping issues
- Animation Editor shows preview on target skeleton

---

## Prompt 8.3: Check for missing bones

If retarget preview reports issues, list:
- Bones that exist in source but not target
- Bones that exist in target but not source
- Suggested remapping

---

## Expected Result
- Clear report of bone mismatches
- Suggestions for manual bone mapping if needed

---

## Note

If the project only has one skeleton, this test can be skipped or used to verify that retarget_preview correctly reports the error when source and target are the same.

---

## Prompt 8.4: Cancel retarget preview

If a preview was set up, cancel it to return to normal editing mode.

---

## Expected Result
- Retarget preview cancelled
- Animation Editor returns to source skeleton view

---

# 9. Complex Animation Creation

These prompts test end-to-end animation creation using the preview workflow.

## Prerequisites

- Completed tests 1-5 (skeleton profile and constraints are set up)
- A humanoid skeleton with arm and leg bones

---

## Prompt 9.1: Wave Hello Animation

Create a 2-second waving animation called "AS_Wave_Hello" using the UE5 Mannequin skeleton.

The animation should:
1. Start from reference pose (frame 0)
2. Raise the right arm to wave position (frames 0-15)
3. Wave back and forth 3 times (frames 15-45)
4. Return to reference pose (frames 45-60)

Use the preview → validate → bake workflow for each phase. Validate against learned constraints before baking.

---

## Expected Result
- Animation created with smooth arm motion
- Shoulder, upper arm, forearm, and hand all coordinated
- No constraint violations
- Animation opens in editor for review

---

## Prompt 9.2: Jump Rope Animation

Create a 3-second jumping animation called "AS_Jump_Rope" that shows:
1. Idle stance (frames 0-10)
2. Jump preparation - bend knees (frames 10-20)
3. Jump up - extend legs, arms swing (frames 20-35)
4. Landing - knees bend to absorb (frames 35-50)
5. Return to idle (frames 50-60)

Focus on:
- Pelvis vertical translation for the jump
- Coordinated arm swings during jump
- Knee bend angles staying within constraints

---

## Expected Result
- Pelvis moves up/down for jump
- Arms and legs coordinate naturally
- Knees don't hyperextend on landing
- Constraint validation passes

---

## Prompt 9.3: Walk Like an Egyptian

Create a stylized 2-second walk cycle "AS_Egyptian_Walk" with:
1. Alternating arm positions (one arm forward horizontal, one back horizontal)
2. Head stays level, slight side-to-side motion
3. Exaggerated leg lifts
4. 2 full step cycles

Use the copy_pose and mirror_pose utilities to create symmetric left/right steps.

---

## Expected Result
- Arms move in horizontal plane (minimal vertical)
- Stylized exaggerated motion
- Left and right steps are mirrored versions
- Loops cleanly

---

## Prompt 9.4: Dance Animation

Create a 4-second looping dance animation "AS_Simple_Dance" with:
1. Torso twist left/right (spine bones)
2. Arm flourishes (hands moving in arcs)
3. Weight shifts (pelvis lateral movement)
4. Head bob synced with body movement

Build 1-second of motion, then use copy_pose to duplicate and mirror for the full 4 seconds.

---

## Expected Result
- Smooth looping dance
- Spine twist stays within constraints
- Arms don't clip through body
- Natural weight distribution

---

## Prompt 9.5: Prone Crawl Animation

Create a 3-second prone crawl animation "AS_Prone_Crawl" showing:
1. Character starts prone (face down)
2. Right arm reaches forward, left knee bends
3. Pull forward, swap to left arm forward, right knee bends
4. Two full crawl cycles

This tests:
- Pelvis low to ground (translation)
- Elbow/knee constraints respected
- Coordinated limb movement

---

## Expected Result
- Character stays close to ground plane
- Elbows and knees don't hyperextend
- Natural crawling rhythm
- Constraint validation passes for all limbs
