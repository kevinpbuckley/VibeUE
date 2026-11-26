"""Regression checks for the mesh AnimSequenceService.capture_animation_pose / capture_animation_sequence
render with (PR #712): an optional skeletal_mesh_path, a compatibility check instead of a skeleton-name
substring match, and skeletal_mesh_path on every result.

Run from an execute_python script (top-level await) in a DISPOSABLE editor session with a renderer
(-RenderOffScreen, not -nullrhi: the captures render). It uses the engine's SkeletalCube fixture, writes
under /Game/VibeUETests/Capture_<id> and Saved/VibeUE/Tests/Capture_<id> and deletes both at the end:

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import capture_mesh_regression as t
    await t.run()

Returns {"passed": [...], "failed": [...], "details": {...}}.
"""

import os
import shutil
import uuid

import unreal

ROOT = "/Game/VibeUETests"
CUBE_MESH = "/Engine/EngineMeshes/SkeletalCube.SkeletalCube"
CUBE_SKELETON = "/Engine/EngineMeshes/SkeletalCube_Skeleton"


def _check(results, name, ok, detail=None):
    (results["passed"] if ok else results["failed"]).append(name)
    if detail is not None:
        results["details"][name] = detail
    return ok


def _checks(results, base, out_dir):
    svc = unreal.AnimSequenceService
    clip = svc.create_anim_sequence(CUBE_SKELETON, "AS_CubeClip", base, 0.5, 30.0, [])
    if not _check(results, "fixture clip on the engine cube skeleton", bool(clip), clip):
        return

    # Explicit mesh: used and reported, frames written.
    frames = svc.capture_animation_sequence(clip, os.path.join(out_dir, "explicit") + os.sep, 2, "front", 64, 64,
                                            CUBE_MESH)
    _check(results, "explicit mesh: two frames", len(frames) == 2, len(frames))
    _check(results, "explicit mesh: every frame succeeds and names the mesh",
           all(f.success and f.skeletal_mesh_path == CUBE_MESH for f in frames),
           [(f.success, f.skeletal_mesh_path, f.error_message) for f in frames])
    _check(results, "explicit mesh: PNGs written",
           all(os.path.isfile(f.image_path) and os.path.getsize(f.image_path) > 0 for f in frames),
           [f.image_path for f in frames])

    # No mesh given: the skeleton's preview mesh / exact skeleton match, reported.
    pose = svc.capture_animation_pose(clip, 0.0, os.path.join(out_dir, "default.png"), "front", 64, 64)
    _check(results, "default mesh: capture succeeds and reports the cube", pose.success
           and pose.skeletal_mesh_path == CUBE_MESH, (pose.success, pose.skeletal_mesh_path, pose.error_message))

    # A copy of the cube skeleton named "SkeletalCube": its name is a substring of the engine cube's
    # skeleton "SkeletalCube_Skeleton", which the old substring match accepted. It is not compatible.
    other = unreal.EditorAssetLibrary.duplicate_asset(CUBE_SKELETON, base + "/SkeletalCube")
    if not _check(results, "fixture skeleton copy named SkeletalCube", other is not None):
        return
    other_clip = svc.create_anim_sequence(other.get_path_name().split(".")[0], "AS_OtherClip", base, 0.5, 30.0, [])
    _check(results, "fixture clip on the copied skeleton", bool(other_clip), other_clip)
    wrong = svc.capture_animation_pose(other_clip, 0.0, os.path.join(out_dir, "wrong.png"), "front", 32, 32, CUBE_MESH)
    _check(results, "incompatible explicit mesh refused, error names it",
           not wrong.success and CUBE_MESH in wrong.error_message, wrong.error_message)
    fallback = svc.capture_animation_pose(other_clip, 0.0, os.path.join(out_dir, "fallback.png"), "front", 32, 32)
    _check(results, "similar skeleton name is not matched", not fallback.success
           and "Could not find a capture mesh" in fallback.error_message, fallback.error_message)
    seq = svc.capture_animation_sequence(other_clip, os.path.join(out_dir, "seq") + os.sep, 3, "front", 32, 32,
                                         CUBE_MESH)
    _check(results, "sequence with an incompatible mesh: one error per frame, no file",
           len(seq) == 3 and all(not f.success and CUBE_MESH in f.error_message for f in seq)
           and not os.path.isdir(os.path.join(out_dir, "seq")), [f.error_message for f in seq])

    missing = svc.capture_animation_pose(base + "/AS_DoesNotExist", 0.0, "", "front", 32, 32)
    _check(results, "missing animation fails with a reason", not missing.success
           and "Failed to load animation" in missing.error_message, missing.error_message)


async def run():
    results = {"passed": [], "failed": [], "details": {}}
    tag = uuid.uuid4().hex[:12]
    base = f"{ROOT}/Capture_{tag}"
    saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
    out_dir = os.path.join(saved, "VibeUE", "Tests", f"Capture_{tag}")
    os.makedirs(out_dir, exist_ok=True)
    try:
        _checks(results, base, out_dir)
    finally:
        if unreal.EditorAssetLibrary.does_directory_exist(base):
            unreal.EditorAssetLibrary.delete_directory(base)
        shutil.rmtree(out_dir, ignore_errors=True)  # this run's own folder only
    return results
