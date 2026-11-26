"""Regression check for FoliageService placement (#695): exact per-instance transforms, a surface trace that ignores
blockers above the ground, instance counts that are 0 (not -1) for a known type with no instances, and counts of what
was really added.

Run from an execute_python script (top-level await) in a DISPOSABLE editor session: it creates and opens a new,
non-partitioned level under /Game/VibeUETests/Foliage (saved once by new_level, never again), a small landscape, a
cube slab standing in for an invisible ceiling, and foliage instances of engine basic shapes.

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import foliage_placement_regression as t
    t.run()

Returns {"passed": [...], "failed": [...], "details": {...}}.
"""

import random
import time

import unreal

LEVEL = "/Game/VibeUETests/Foliage/L_FoliagePlacement"
LANDSCAPE = "VibeUETestLandscape"
SLAB = "VibeUETestCeilingSlab"
TRACE_MESH = "/Engine/BasicShapes/Cone"
TRANSFORM_MESH = "/Engine/BasicShapes/Sphere"
SNAP_MESH = "/Engine/BasicShapes/Cylinder"
SLAB_Z = 3000.0
GROUND_Z = -137.5  # a flat landscape (sculpting needs a GPU edit-layer merge, absent under -nullrhi), off Z = 0


def _check(results, name, ok, detail=None):
    (results["passed"] if ok else results["failed"]).append(name)
    if detail is not None:
        results["details"][name] = detail


def _raises(fn, *args):
    try:
        fn(*args)
    except Exception as exc:  # noqa: BLE001 - the script error is what is being tested
        return str(exc)
    return None


def _setup():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level = "%s_%d" % (LEVEL, int(time.time()))  # new_level refuses an existing map
    if not les.new_level(level):
        raise RuntimeError("could not create " + level)
    land = unreal.LandscapeService.create_landscape(
        unreal.Vector(0, 0, GROUND_Z), unreal.Rotator(0, 0, 0), unreal.Vector(100, 100, 100), 1, 7, 2, 2, LANDSCAPE)
    if not land.success:
        raise RuntimeError("landscape: " + land.error_message)
    # The invisible ceiling: a slab of the engine cube that blocks WorldStatic, hidden in game, 30 m above the ground
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    slab = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(700, 700, SLAB_Z))
    slab.set_actor_label(SLAB)
    slab.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube"))
    slab.set_actor_scale3d(unreal.Vector(30, 30, 1))
    slab.set_actor_hidden_in_game(True)
    return land.actor_label


def _ground_z(x, y):
    return unreal.LandscapeService.get_height_at_location(LANDSCAPE, x, y).height


def _quat(rot):
    return rot.quaternion()


def _quat_close(a, b, tol=1e-4):
    dot = abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w)
    return dot > 1.0 - tol


def run():
    results = {"passed": [], "failed": [], "details": {}}
    fs = unreal.FoliageService
    _setup()

    # --- counts: unknown path raises, a known type with no instances is 0
    err = _raises(fs.get_instance_count, "/Game/DoesNot/Exist")
    _check(results, "count_unknown_path_raises", err is not None and "no StaticMesh or FoliageType" in err, err)
    err = _raises(fs.get_instance_count, "/Engine/BasicShapes/BasicShapeMaterial")
    _check(results, "count_wrong_class_raises", err is not None and "not a StaticMesh or FoliageType" in err, err)
    n = fs.get_instance_count(TRANSFORM_MESH)
    _check(results, "count_known_type_none_placed_is_0", n == 0, n)
    _check(results, "has_instances_unknown_is_false", fs.has_foliage_instances("/Game/DoesNot/Exist") is False)

    # --- the trace: first WorldStatic hit (old behaviour) lands on the slab; landscape-only lands on the ground
    pts = [unreal.Vector(300 + 100 * i, 400 + 50 * i, 0) for i in range(8)]
    r_default = fs.add_foliage_instances(TRACE_MESH, pts, 1.0, 1.0, False, False, True)
    q = fs.get_foliage_in_radius(TRACE_MESH, 700, 700, 5000, 100)
    zs = sorted(i.location.z for i in q.instances)
    _check(results, "default_trace_hits_slab", r_default.instances_added == 8 and all(z > SLAB_Z for z in zs),
           {"added": r_default.instances_added, "z": zs[:3]})
    fs.remove_all_foliage_of_type(TRACE_MESH)

    r_land = fs.add_foliage_instances(TRACE_MESH, pts, 1.0, 1.0, False, False, True, True)
    q = fs.get_foliage_in_radius(TRACE_MESH, 700, 700, 5000, 100)
    errs = [abs(i.location.z - _ground_z(i.location.x, i.location.y)) + abs(i.location.z - GROUND_Z) for i in q.instances]
    _check(results, "landscape_only_trace_lands_on_ground",
           r_land.success and r_land.instances_added == 8 and len(errs) == 8 and max(errs) < 1.0,
           {"added": r_land.instances_added, "max_dz": max(errs) if errs else None,
            "z": [round(i.location.z, 2) for i in q.instances][:3]})
    fs.remove_all_foliage_of_type(TRACE_MESH)

    r_named = fs.add_foliage_instances(TRACE_MESH, pts, 1.0, 1.0, False, False, True, False, LANDSCAPE)
    q = fs.get_foliage_in_radius(TRACE_MESH, 700, 700, 5000, 100)
    _check(results, "named_actor_trace_lands_on_ground",
           r_named.instances_added == 8 and all(i.location.z < SLAB_Z - 100 for i in q.instances),
           {"added": r_named.instances_added})
    fs.remove_all_foliage_of_type(TRACE_MESH)

    r_bad = fs.add_foliage_instances(TRACE_MESH, pts, 1.0, 1.0, False, False, True, False, "NoSuchActor")
    _check(results, "unknown_trace_actor_refused_nothing_placed",
           (not r_bad.success) and r_bad.instances_added == 0 and fs.get_instance_count(TRACE_MESH) == 0
           and "NoSuchActor" in r_bad.error_message, r_bad.error_message)
    r_notland = fs.add_foliage_instances(TRACE_MESH, pts, 1.0, 1.0, False, False, True, True, SLAB)
    _check(results, "landscape_only_with_non_landscape_actor_refused", not r_notland.success, r_notland.error_message)

    off = [unreal.Vector(50000, 50000, 0)]
    r_miss = fs.add_foliage_instances(TRACE_MESH, off, 1.0, 1.0, False, False, True, True)
    _check(results, "landscape_only_miss_is_rejected",
           r_miss.success and r_miss.instances_added == 0 and r_miss.instances_rejected == 1,
           {"added": r_miss.instances_added, "rejected": r_miss.instances_rejected})

    # --- scatter with the landscape-only flag
    rs = fs.scatter_foliage(TRACE_MESH, 700, 700, 500, 20, 1.0, 1.0, False, False, 7, "", True)
    q = fs.get_foliage_in_radius(TRACE_MESH, 700, 700, 5000, 100)
    _check(results, "scatter_landscape_only_below_slab",
           rs.instances_added > 0 and rs.instances_added == q.total_instances
           and all(i.location.z < SLAB_Z - 100 for i in q.instances),
           {"added": rs.instances_added, "listed": q.total_instances})
    rs2 = fs.scatter_foliage(TRACE_MESH, 700, 700, 500, 20, 1.0, 1.0, False, False, 7)
    q2 = fs.get_foliage_in_radius(TRACE_MESH, 700, 700, 5000, 200)
    _check(results, "scatter_default_still_hits_slab",
           rs2.instances_added > 0 and sum(1 for i in q2.instances if i.location.z > SLAB_Z) == rs2.instances_added,
           {"added": rs2.instances_added})
    fs.remove_all_foliage_of_type(TRACE_MESH)

    # --- 1,000 exact transforms with non-uniform scale: round trip
    rng = random.Random(695)
    wanted = []
    for i in range(1000):
        loc = unreal.Vector(round(rng.uniform(0, 1400), 3), round(rng.uniform(0, 1400), 3), round(rng.uniform(-50, 50), 3))
        rot = unreal.Rotator(roll=rng.uniform(-30, 30), pitch=rng.uniform(-30, 30), yaw=rng.uniform(-180, 180))
        scale = unreal.Vector(rng.uniform(0.3, 2.0), rng.uniform(0.3, 2.0), rng.uniform(0.5, 4.0))
        wanted.append(unreal.Transform(loc, rot, scale))
    r_t = fs.add_foliage_instance_transforms(TRANSFORM_MESH, wanted)
    q = fs.get_foliage_in_radius(TRANSFORM_MESH, 700, 700, 5000, 5000)
    got = {(round(i.location.x, 2), round(i.location.y, 2)): i for i in q.instances}
    worst_loc = worst_scale = 0.0
    bad_rot = missing = 0
    for t in wanted:
        loc = t.translation
        inst = got.get((round(loc.x, 2), round(loc.y, 2)))
        if inst is None:
            missing += 1
            continue
        worst_loc = max(worst_loc, (inst.location - loc).length())
        s = t.scale3d
        worst_scale = max(worst_scale, abs(inst.scale.x - s.x), abs(inst.scale.y - s.y), abs(inst.scale.z - s.z))
        if not _quat_close(_quat(inst.rotation), t.rotation):
            bad_rot += 1
    _check(results, "transforms_1000_round_trip",
           r_t.success and r_t.instances_added == 1000 and r_t.instances_requested == 1000 and q.total_instances == 1000
           and missing == 0 and bad_rot == 0 and worst_loc < 1e-3 and worst_scale < 1e-5,
           {"added": r_t.instances_added, "listed": q.total_instances, "missing": missing, "bad_rotations": bad_rot,
            "worst_location_cm": worst_loc, "worst_scale": worst_scale})
    _check(results, "count_after_transforms", fs.get_instance_count(TRANSFORM_MESH) == 1000)
    r_t2 = fs.add_foliage_instance_transforms(TRANSFORM_MESH, wanted[:10])
    _check(results, "count_added_is_delta", r_t2.instances_added == 10 and fs.get_instance_count(TRANSFORM_MESH) == 1010,
           r_t2.instances_added)
    fs.remove_all_foliage_of_type(TRANSFORM_MESH)
    _check(results, "count_after_removal_is_0", fs.get_instance_count(TRANSFORM_MESH) == 0)

    # --- snap to the surface: only Z changes, landscape-only ignores the slab
    snap = [unreal.Transform(unreal.Vector(200 + 40 * i, 900, 9999), unreal.Rotator(roll=10, pitch=5, yaw=30 * i),
                             unreal.Vector(1.0, 2.0, 3.0)) for i in range(5)]
    r_s = fs.add_foliage_instance_transforms(SNAP_MESH, snap, True, True)
    q = fs.get_foliage_in_radius(SNAP_MESH, 700, 700, 5000, 100)
    ok = r_s.instances_added == 5 and q.total_instances == 5
    for inst in q.instances:
        src = min(snap, key=lambda t: (t.translation.x - inst.location.x) ** 2 + (t.translation.y - inst.location.y) ** 2)
        ok = ok and abs(inst.location.z - _ground_z(inst.location.x, inst.location.y)) < 1.0
        ok = ok and abs(inst.location.x - src.translation.x) < 1e-3 and _quat_close(_quat(inst.rotation), src.rotation)
        ok = ok and abs(inst.scale.y - 2.0) < 1e-5 and abs(inst.scale.z - 3.0) < 1e-5
    _check(results, "snap_to_landscape_keeps_rotation_and_scale", ok, {"added": r_s.instances_added})
    fs.remove_all_foliage_of_type(SNAP_MESH)
    return results
