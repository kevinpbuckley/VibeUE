"""Regression checks for AssetDiscoveryService mesh imports: import_mesh_into_fresh_folder (PR #713) and the
unattended browser-sync guard around every AssetTools import (PR #711).

Run from an execute_python script (top-level await) in a DISPOSABLE editor session. It writes under
/Game/VibeUETests/FreshImport_<id> (deleted at the end) and reads the engine's own FBX fixture:

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import fresh_mesh_import_regression as t
    await t.run()               # import checks only (any RHI, -nullrhi is fine)
    await t.run(pie=True)       # also: after the import a PIE world must tick (needs a renderer:
                                # -RenderOffScreen, not -nullrhi; the open level is used for PIE)

Returns {"passed": [...], "failed": [...], "details": {...}}.
"""

import asyncio
import os
import shutil
import uuid

import unreal

ROOT = "/Game/VibeUETests"
SYNC_CVAR = "Interchange.FeatureFlags.Import.SyncToBrowser"


def _check(results, name, ok, detail=None):
    (results["passed"] if ok else results["failed"]).append(name)
    if detail is not None:
        results["details"][name] = detail
    return ok


def _fixture():
    engine = unreal.Paths.convert_relative_path_to_full(unreal.Paths.engine_content_dir())
    return os.path.join(engine, "FbxEditorAutomation", "BlenderCube.fbx")


def _disk_folder(folder):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, folder[len("/Game/"):].replace("/", os.sep))


def _sync_value():
    return unreal.SystemLibrary.get_console_variable_bool_value(SYNC_CVAR)


def _import_checks(results, base):
    svc = unreal.AssetDiscoveryService
    fbx = _fixture()
    _check(results, "fixture exists", os.path.isfile(fbx), fbx)

    # Argument refusals, before anything runs.
    r = svc.import_mesh_into_fresh_folder(fbx, "/Engine/NotGame")
    _check(results, "non-/Game destination refused", not r.success and bool(r.error_message), r.error_message)
    r = svc.import_mesh_into_fresh_folder(fbx[:-4] + ".txt", base + "/BadExt")
    _check(results, "non-mesh extension refused", not r.success and "FBX" in r.error_message, r.error_message)

    # A folder that holds an unregistered file on disk is refused, and the file is left alone.
    occupied = base + "/Occupied"
    disk = _disk_folder(occupied)
    os.makedirs(disk, exist_ok=True)
    marker = os.path.join(disk, "untracked.txt")
    with open(marker, "w") as handle:
        handle.write("untouched")
    r = svc.import_mesh_into_fresh_folder(fbx, occupied)
    with open(marker) as handle:
        kept = handle.read()
    _check(results, "folder with an untracked file refused", not r.success and "occupied" in r.error_message,
           r.error_message)
    _check(results, "refusal writes nothing", len(r.created_assets) == 0 and kept == "untouched"
           and os.listdir(disk) == ["untracked.txt"], {"created": list(r.created_assets), "files": os.listdir(disk)})
    os.remove(marker)

    # A same-named material elsewhere in the project must come out byte-identical.
    other = base + "/Elsewhere"
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("BackRed", other, unreal.Material,
                                                                   unreal.MaterialFactoryNew())
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    mat_file = os.path.join(_disk_folder(other), "BackRed.uasset")
    with open(mat_file, "rb") as handle:
        before = handle.read()

    # The fresh import itself.
    sync_before = _sync_value()
    fresh = base + "/Fresh"
    r = svc.import_mesh_into_fresh_folder(fbx, fresh)
    _check(results, "fresh import succeeds", r.success, r.error_message)
    _check(results, "browser-sync cvar restored after the import", sync_before == _sync_value(),
           {"before": sync_before, "after": _sync_value()})
    main = unreal.load_asset(r.main_asset) if r.main_asset else None
    _check(results, "main asset is a mesh", isinstance(main, (unreal.StaticMesh, unreal.SkeletalMesh)), r.main_asset)
    created = list(r.created_assets)
    _check(results, "main asset listed in created_assets", r.main_asset in created, created)
    on_disk = [p for p in created if os.path.isfile(_disk_folder(p.split(".")[0]) + ".uasset")]
    _check(results, "every created asset is inside the folder and saved",
           all(p.startswith(fresh + "/") for p in created) and len(on_disk) == len(created),
           {"created": created, "saved": on_disk})
    with open(mat_file, "rb") as handle:
        after = handle.read()
    _check(results, "same-named material elsewhere is byte-identical", before == after)
    if isinstance(main, unreal.StaticMesh):
        slots = [s.material_interface.get_path_name() for s in main.static_materials if s.material_interface]
        external = list(r.external_materials)
        _check(results, "every material slot is created or reported external",
               all((p in created) != (p in external) for p in slots), {"slots": slots, "external": external})

    # A second import into the now-occupied folder is refused and creates nothing.
    again = svc.import_mesh_into_fresh_folder(fbx, fresh)
    _check(results, "second import into the same folder refused", not again.success and len(again.created_assets) == 0,
           again.error_message)
    return fresh


async def _pie_ticks(results):
    import vibeue
    async with vibeue.pie_session(640, 360) as session:
        world = session["world"] or vibeue.game_world()
        t0 = unreal.GameplayStatics.get_time_seconds(world)
        await asyncio.sleep(2.0)
        t1 = unreal.GameplayStatics.get_time_seconds(world)
    _check(results, "PIE world ticks after an unattended import", t1 - t0 > 0.5, {"t0": t0, "t1": t1})


async def run(pie=False):
    results = {"passed": [], "failed": [], "details": {}}
    base = f"{ROOT}/FreshImport_{uuid.uuid4().hex[:12]}"
    try:
        _import_checks(results, base)
        if pie:
            await _pie_ticks(results)
    finally:
        if unreal.EditorAssetLibrary.does_directory_exist(base):
            unreal.EditorAssetLibrary.delete_directory(base)
        shutil.rmtree(_disk_folder(base), ignore_errors=True)  # this run's own folder only
    return results
