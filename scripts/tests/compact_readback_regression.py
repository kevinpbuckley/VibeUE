"""Regression check for the compact read-back calls (#706): MaterialNodeService.material_instance_overrides,
AssetDiscoveryService.mesh_summary and FoliageService.foliage_summary. Each answer is compared with the editor's own
values (MaterialEditingLibrary, Epic's StaticMeshToolset, the assets' properties) and its MCP payload must stay under
about 1 KB.

Run from an execute_python script with top-level await in a DISPOSABLE editor session. It creates assets under
/Game/VibeUETests/Readback/Run<time> (a material, two instances, a Nanite copy of the engine cube with LODs, a foliage type) and
opens a new level there:

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import compact_readback_regression as t
    await t.run(call_tool)

Returns {"passed": [...], "failed": [...], "details": {...}}.
"""

import json
import time

import unreal

# A fresh folder per run: re-running never deletes (a referenced asset's delete dialog would wedge the editor)
FOLDER = "/Game/VibeUETests/Readback/Run%d" % int(time.time())
LEVEL = FOLDER + "/L_Readback"
SMT = "EditorToolset.StaticMeshToolset"


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


def _num(text):
    return [float(x) for x in text.split(",")]


def _material():
    mel = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset("M_Readback", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -400, 0)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.5)
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 200)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -400, 400)
    tex.set_editor_property("parameter_name", "BaseTex")
    switch = mel.create_material_expression(mat, unreal.MaterialExpressionStaticSwitchParameter, -150, 200)
    switch.set_editor_property("parameter_name", "UseTexture")
    switch.set_editor_property("default_value", False)
    mel.connect_material_expressions(tex, "RGB", switch, "True")
    mel.connect_material_expressions(tint, "", switch, "False")
    mel.connect_material_property(switch, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)

    mi = tools.create_asset("MI_Readback", FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", 0.2)
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(0.1, 0.2, 0.3, 1.0))
    mel.set_material_instance_static_switch_parameter_value(mi, "UseTexture", True)
    mel.update_material_instance(mi)

    child = tools.create_asset("MI_Readback_Child", FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(child, mi)
    grid = unreal.load_asset("/Engine/EngineMaterials/DefaultDiffuse")
    mel.set_material_instance_texture_parameter_value(child, "BaseTex", grid)
    mel.update_material_instance(child)
    return mat, mi, child, grid


async def _payload(call_tool, tool, **args):
    value = await call_tool(tool, **args)
    return value, len(json.dumps(value, separators=(",", ":")))


async def run(call_tool):
    global FOLDER, LEVEL
    FOLDER = "/Game/VibeUETests/Readback/Run%d" % int(time.time())
    LEVEL = FOLDER + "/L_Readback"
    results = {"passed": [], "failed": [], "details": {}}
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level(LEVEL)
    mel = unreal.MaterialEditingLibrary

    # ------------------------------------------------------------------ material_instance_overrides
    mat, mi, child, grid = _material()
    o = unreal.MaterialNodeService.material_instance_overrides(mi.get_path_name())
    by = {p.name: p for p in o.overrides}
    _check(results, "mi_parent_and_base", o.parent == mat.get_path_name() and o.base_material == mat.get_path_name(),
           {"parent": o.parent, "base": o.base_material})
    _check(results, "mi_exactly_the_overrides", sorted(by) == ["Roughness", "Tint", "UseTexture"], sorted(by))
    if sorted(by) == ["Roughness", "Tint", "UseTexture"]:
        r, t, s = by["Roughness"], by["Tint"], by["UseTexture"]
        editor_r = mel.get_material_instance_scalar_parameter_value(mi, "Roughness")
        editor_t = mel.get_material_instance_vector_parameter_value(mi, "Tint")
        editor_s = mel.get_material_instance_static_switch_parameter_value(mi, "UseTexture")
        _check(results, "mi_scalar_matches_editor",
               r.type == "Scalar" and abs(float(r.value) - editor_r) < 1e-6 and abs(float(r.parent_value) - 0.5) < 1e-6,
               {"value": r.value, "editor": editor_r, "parent": r.parent_value})
        tv = _num(t.value)
        _check(results, "mi_vector_matches_editor",
               t.type == "Vector" and all(abs(a - b) < 1e-6 for a, b in zip(tv, (editor_t.r, editor_t.g, editor_t.b, editor_t.a)))
               and _num(t.parent_value) == [1.0, 1.0, 1.0, 1.0], {"value": t.value, "parent": t.parent_value})
        _check(results, "mi_static_switch_matches_editor",
               s.type == "StaticSwitch" and s.value == ("true" if editor_s else "false") and s.parent_value == "false",
               {"value": s.value, "editor": editor_s, "parent": s.parent_value})

    oc = unreal.MaterialNodeService.material_instance_overrides(child.get_path_name())
    names = [p.name for p in oc.overrides]
    inherited_r = mel.get_material_instance_scalar_parameter_value(child, "Roughness")
    _check(results, "child_lists_only_its_own_override",
           names == ["BaseTex"] and oc.overrides[0].type == "Texture" and oc.overrides[0].value == grid.get_path_name()
           and oc.parent == mi.get_path_name() and oc.base_material == mat.get_path_name() and abs(inherited_r - 0.2) < 1e-6,
           {"names": names, "value": oc.overrides[0].value if oc.overrides else None,
            "parent_value": oc.overrides[0].parent_value if oc.overrides else None, "inherited_roughness": inherited_r})

    err = _raises(unreal.MaterialNodeService.material_instance_overrides, mat.get_path_name())
    _check(results, "mi_on_a_material_raises", err is not None and "not a material instance" in err, err)
    err = _raises(unreal.MaterialNodeService.material_instance_overrides, FOLDER + "/MI_Nothing")
    _check(results, "mi_missing_raises", err is not None and "no material instance" in err, err)

    payload, size = await _payload(call_tool, "VibeUE.MaterialNodeService.MaterialInstanceOverrides",
                                   materialInstancePath=mi.get_path_name())
    _check(results, "mi_payload_under_1kb", size < 1024, {"bytes": size})
    results["details"]["mi_payload"] = payload

    # ------------------------------------------------------------------ mesh_summary
    eal = unreal.EditorAssetLibrary
    mesh_path = FOLDER + "/SM_ReadbackCube"
    mesh = eal.duplicate_asset("/Engine/BasicShapes/Cube", mesh_path)
    await call_tool(SMT + ".GenerateLods", mesh=mesh, trianglePercents=[1.0, 0.5, 0.25])
    await call_tool(SMT + ".SetNaniteEnabled", mesh=mesh, bEnabled=True)
    s = unreal.AssetDiscoveryService.mesh_summary(mesh_path)
    lods = await call_tool(SMT + ".GetLodCount", mesh=mesh)
    tris = [await call_tool(SMT + ".GetTriangleCount", mesh=mesh, lodIndex=i) for i in range(lods)]
    verts = [await call_tool(SMT + ".GetVertexCount", mesh=mesh, lodIndex=i) for i in range(lods)]
    thresholds = await call_tool(SMT + ".GetLodThresholds", mesh=mesh)
    bounds = await call_tool(SMT + ".GetBounds", mesh=mesh)
    slots = await call_tool(SMT + ".GetMaterialSlots", mesh=mesh)
    nanite = await call_tool(SMT + ".IsNaniteMesh", mesh=mesh)
    complexity = await call_tool(SMT + ".GetCollisionComplexity", mesh=mesh)
    _check(results, "mesh_lods_match_epic",
           len(s.lods) == lods and [l.triangles for l in s.lods] == tris and [l.vertices for l in s.lods] == verts,
           {"summary": [(l.triangles, l.vertices, l.screen_size) for l in s.lods], "epic_tris": tris, "epic_verts": verts})
    _check(results, "mesh_screen_sizes_match_epic",
           len(thresholds) >= len(s.lods) - 1
           and all(abs(a - b) < 1e-3 for a, b in zip([l.screen_size for l in s.lods], thresholds)),
           {"summary": [l.screen_size for l in s.lods], "epic": thresholds})
    bmin, bmax = bounds["min"], bounds["max"]
    _check(results, "mesh_bounds_match_epic",
           abs(s.bounds_min.x - bmin["x"]) < 0.01 and abs(s.bounds_max.z - bmax["z"]) < 0.01,
           {"summary": [s.bounds_min.x, s.bounds_max.z], "epic": [bmin, bmax]})
    _check(results, "mesh_slots_match_epic", [x.slot for x in s.slots] == list(slots)
           and s.slots[0].material == mesh.get_material(0).get_path_name(), {"summary": [(x.slot, x.material) for x in s.slots], "epic": slots})
    _check(results, "mesh_nanite_matches_epic", s.nanite == nanite and (s.nanite_triangles > 0) == nanite,
           {"nanite": s.nanite, "epic": nanite, "nanite_triangles": s.nanite_triangles, "settings": list(s.nanite_settings)})
    norm = lambda x: str(x).lower().replace("ctf_", "").replace("_", "").split(".")[-1]  # noqa: E731
    mine = norm(s.collision_complexity)
    mine = "usesimpleandcomplex" if mine == "simpleandcomplex" else mine
    _check(results, "mesh_collision_matches_epic", norm(complexity).endswith(mine),
           {"summary": s.collision_complexity, "epic": str(complexity), "simple": s.simple_collision, "profile": s.collision_profile})
    _check(results, "mesh_simple_collision_box", s.simple_collision == "1 box", s.simple_collision)

    # a Nanite setting changed from its default is listed
    ns = mesh.get_editor_property("nanite_settings")
    ns.set_editor_property("keep_percent_triangles", 0.5)
    mesh.set_editor_property("nanite_settings", ns)
    s2 = unreal.AssetDiscoveryService.mesh_summary(mesh_path)
    _check(results, "mesh_changed_nanite_setting_listed", any(x.startswith("KeepPercentTriangles=0.5") for x in s2.nanite_settings),
           list(s2.nanite_settings))
    err = _raises(unreal.AssetDiscoveryService.mesh_summary, mat.get_path_name())
    _check(results, "mesh_on_a_material_raises", err is not None and "not a StaticMesh" in err, err)
    payload, size = await _payload(call_tool, "VibeUE.AssetDiscoveryService.MeshSummary", meshPath=mesh_path)
    _check(results, "mesh_payload_under_1kb", size < 1024, {"bytes": size})
    results["details"]["mesh_payload"] = payload

    # ------------------------------------------------------------------ foliage_summary
    ft_res = unreal.FoliageService.create_foliage_type(mesh_path, FOLDER, "FT_ReadbackCube", 1.0, 1.0, True, 45.0, 45.0, 12345.0)
    ft = unreal.load_asset(ft_res.asset_path)
    ft.set_editor_property("cast_shadow", False)
    ft.set_editor_property("world_position_offset_disable_distance", 4000)
    ft.set_editor_property("nanite_pixel_programmable_distance", 2500.0)
    ft.set_editor_property("override_materials", [None, mi])
    unreal.FoliageService.add_foliage_instances(ft_res.asset_path, [unreal.Vector(i * 300, 0, 0) for i in range(3)],
                                                1.0, 1.0, False, False, False)
    f = unreal.FoliageService.foliage_summary(ft_res.asset_path)
    cull = ft.get_editor_property("cull_distance")
    _check(results, "foliage_matches_properties",
           f.mesh == mesh.get_path_name() and f.mesh_nanite is True
           and f.cull_distance_min == cull.min and f.cull_distance_max == cull.max == 12345
           and f.cast_shadow is False and f.cast_dynamic_shadow == ft.get_editor_property("cast_dynamic_shadow")
           and f.world_position_offset_disable_distance == 4000 and abs(f.nanite_pixel_programmable_distance - 2500.0) < 1e-3
           and f.evaluate_world_position_offset == ft.get_editor_property("evaluate_world_position_offset")
           and list(f.override_materials) == ["1: " + mi.get_path_name()] and f.instance_count == 3,
           {"mesh": f.mesh, "cull": [f.cull_distance_min, f.cull_distance_max], "shadow": f.cast_shadow,
            "wpo": [f.evaluate_world_position_offset, f.world_position_offset_disable_distance],
            "nanite_pp": f.nanite_pixel_programmable_distance, "overrides": list(f.override_materials),
            "count": f.instance_count, "mobility": f.mobility})
    f_mesh = unreal.FoliageService.foliage_summary(mesh_path)
    _check(results, "foliage_by_mesh_path", f_mesh.foliage_type == ft.get_path_name() and f_mesh.instance_count == 3,
           f_mesh.foliage_type)
    err = _raises(unreal.FoliageService.foliage_summary, "/Engine/BasicShapes/Sphere")
    _check(results, "foliage_mesh_without_type_raises", err is not None and "no foliage type" in err, err)
    err = _raises(unreal.FoliageService.foliage_summary, mat.get_path_name())
    _check(results, "foliage_wrong_class_raises", err is not None and "not a FoliageType" in err, err)
    payload, size = await _payload(call_tool, "VibeUE.FoliageService.FoliageSummary", foliageTypeOrMeshPath=ft_res.asset_path)
    _check(results, "foliage_payload_under_1kb", size < 1024, {"bytes": size})
    results["details"]["foliage_payload"] = payload
    unreal.FoliageService.remove_all_foliage_of_type(ft_res.asset_path)
    return results
