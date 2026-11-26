"""Regression check for issue #704: LandscapeService.get_render_materials reports what a landscape actually draws.

Run from an execute_python script (top-level await) in a DISPOSABLE editor session WITH a renderer
(-RenderOffScreen, not -nullrhi: materials are only compiled with one). It creates and saves a test map and two
materials under /Game/VibeUETests/; never run it in a real project's editor.

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import landscape_render_materials_regression as t
    await t.run(call_tool)

Returns {"passed": [...], "failed": [...], "details": {...}}. It checks:
  - a two-layer landscape material painted in two areas gives several permutations whose component counts add up
    to the number of components, each with instruction and sample counts and no compile error;
  - the material set through ObjectToolset.SetProperties (the documented setter) is what every component draws;
  - a material that fails to compile is reported as such, with the fallback to the default material;
  - a LandscapeMaterial changed without PostEditChangeProperty is reported as stale instances, SetProperties with the
    unchanged value does not rebuild them, set_landscape_material does.
"""

import asyncio
import json

import unreal

ROOT = "/Game/VibeUETests"
MAP = ROOT + "/L_VibeUE_LandscapeRenderMaterials"
LABEL = "VibeUERenderLandscape"
GOOD = ROOT + "/M_VibeUE_RenderTwoLayers"
BROKEN = ROOT + "/M_VibeUE_RenderBroken"
LAYERS = ("VibeUERenderA", "VibeUERenderB")
COMPONENTS, QUADS, SECTIONS = 4, 15, 1


def _check(results, name, expected, actual):
    ok = expected == actual
    (results["passed"] if ok else results["failed"]).append(name)
    results["details"][name] = {"expected": expected, "actual": actual}
    return ok


def _new_material(path):
    folder, name = path.rsplit("/", 1)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def _make_layer_material():
    mel = unreal.MaterialEditingLibrary
    mat = _new_material(GOOD)
    blend = mel.create_material_expression(mat, unreal.MaterialExpressionLandscapeLayerBlend, -400, 0)
    inputs = []
    for name in LAYERS:
        layer = unreal.LayerBlendInput()
        layer.set_editor_property("layer_name", name)
        layer.set_editor_property("blend_type", unreal.LandscapeLayerBlendType.LB_WEIGHT_BLEND)
        inputs.append(layer)
    blend.set_editor_property("layers", inputs)
    for i, name in enumerate(LAYERS):
        tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -900, i * 300)
        tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultDiffuse"))
        mel.connect_material_expressions(tex, "RGB", blend, "Layer " + name)
    mel.connect_material_property(blend, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def _make_broken_material():
    mel = unreal.MaterialEditingLibrary
    mat = _new_material(BROKEN)
    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -400, 0)
    custom.set_editor_property("code", "return this is not hlsl;")
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


async def _set_material(call_tool, landscape, material):
    return await call_tool("EditorToolset.ObjectToolset.SetProperties", instance={"refPath": landscape.get_path_name()},
                           values=json.dumps({"LandscapeMaterial": material.get_path_name()}))


async def run(call_tool):
    results = {"passed": [], "failed": [], "details": {}}
    svc = unreal.LandscapeService
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        les.load_level(MAP)
    else:
        les.new_level(MAP)
    await asyncio.sleep(0.1)
    if svc.landscape_exists(LABEL):
        svc.delete_landscape(LABEL)
    created = svc.create_landscape(unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), unreal.Vector(100, 100, 100),
                                   SECTIONS, QUADS, COMPONENTS, COMPONENTS, LABEL)
    if not _check(results, "fixture landscape created", True, created.success):
        return results
    landscape = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                 if isinstance(a, unreal.Landscape) and a.get_actor_label() == LABEL][0]
    n_components = COMPONENTS * COMPONENTS

    # No material yet.
    r = svc.get_render_materials(LABEL)
    _check(results, "no material: reported", (True, "", n_components),
           (r.success, r.landscape_material, sum(p.component_count for p in r.permutations)))
    _check(results, "no material: a note says so", True, any("no LandscapeMaterial" in n for n in r.notes))

    good = _make_layer_material()
    await _set_material(call_tool, landscape, good)
    for name in LAYERS:
        info = unreal.LandscapeMaterialService.create_layer_info_object(name, ROOT, True)
        svc.add_layer(LABEL, info.asset_path)
    # Layer A on the west half, layer B on one corner: three layer sets (A, A+B, B or none).
    svc.paint_layer_in_world_rect(LABEL, LAYERS[0], 0.0, 0.0, 3000.0, 6000.0, 1.0)
    svc.paint_layer_at_location(LABEL, LAYERS[1], 1500.0, 1500.0, 900.0, 1.0)
    svc.paint_layer_at_location(LABEL, LAYERS[1], 5000.0, 5000.0, 600.0, 1.0)
    for _ in range(10):
        await asyncio.sleep(0.1)

    r = svc.get_render_materials(LABEL)
    perms = list(r.permutations)
    _check(results, "layered: success and component count", (True, n_components), (r.success, r.num_components))
    _check(results, "layered: permutation counts add up to the components", n_components, sum(p.component_count for p in perms))
    _check(results, "layered: more than one permutation (painted layer sets differ)", True, len(perms) > 1)
    _check(results, "layered: every component draws the assigned material", [GOOD.split("/")[-1]] * n_components,
           [c.base_material.split(".")[-1] for c in r.components])
    _check(results, "layered: nothing falls back or is stale", (0, 0), (r.num_components_falling_back, r.num_components_stale))
    _check(results, "layered: statistics for every permutation", True,
           all(p.statistics_available and p.num_pixel_shader_instructions > 0 and p.num_pixel_texture_samples > 0
               and not p.compile_errors for p in perms))
    results["details"]["layered permutations"] = [
        {"layers": list(p.painted_layers), "components": p.component_count, "ps": p.num_pixel_shader_instructions,
         "vs": p.num_vertex_shader_instructions, "samples": p.num_pixel_texture_samples, "samplers": p.num_samplers}
        for p in perms]
    _check(results, "layered: components' layers match their permutation", True,
           all(list(c.painted_layers) == list(next(p for p in perms if p.permutation == c.permutation).painted_layers)
               for c in r.components))

    broken = _make_broken_material()
    await _set_material(call_tool, landscape, broken)
    r = svc.get_render_materials(LABEL)
    perms = list(r.permutations)
    _check(results, "broken: permutation counts add up to the components", n_components, sum(p.component_count for p in perms))
    _check(results, "broken: compile errors reported", True, all(len(p.compile_errors) > 0 for p in perms))
    _check(results, "broken: every component falls back to the default material", n_components, r.num_components_falling_back)
    _check(results, "broken: a note says so", True, any("default material" in n for n in r.notes))
    results["details"]["broken first error"] = perms[0].compile_errors[0] if perms and perms[0].compile_errors else ""

    # Changed without PostEditChangeProperty: the instances still end in the broken material.
    landscape.set_editor_property("landscape_material", good, unreal.PropertyAccessChangeNotifyMode.NEVER)
    r = svc.get_render_materials(LABEL)
    _check(results, "stale: every component reported stale", n_components, r.num_components_stale)
    _check(results, "stale: a note says to rebuild", True, any("rebuild" in n for n in r.notes))

    # SetProperties with the value the property already holds changes nothing, so it does not rebuild...
    await _set_material(call_tool, landscape, good)
    r = svc.get_render_materials(LABEL)
    _check(results, "stale: SetProperties with the unchanged value does not rebuild", n_components, r.num_components_stale)
    # ...LandscapeService.set_landscape_material always fires PostEditChangeProperty: everything draws the good material.
    _check(results, "rebuild: set_landscape_material returns True", True, svc.set_landscape_material(LABEL, GOOD))
    r = svc.get_render_materials(LABEL)
    _check(results, "rebuilt: nothing stale or falling back", (0, 0, []),
           (r.num_components_stale, r.num_components_falling_back, list(r.notes)))

    r = svc.get_render_materials("NoSuchLandscape")
    _check(results, "unknown label is an error", False, r.success)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    return results
