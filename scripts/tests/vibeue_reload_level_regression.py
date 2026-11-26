"""Regression checks for vibeue.safe_reload_level() (#702). Needs a live UE 5.9 editor with VibeUE.

Run from an execute_python script (top-level await) in a DISPOSABLE editor session: it creates and saves
/Game/VibeUETests/L_D1_Reload, edits and reloads it, opens a scratch /Temp level once, and leaves the editor on
the test map.

    import sys
    sys.path.insert(0, r"<VibeUE>/scripts/tests")
    import vibeue_reload_level_regression as t
    await t.run()          # {"passed": [...], "failed": [...], "details": {...}}

The reload is done with Python references into the map still alive: a script global, a list holding an actor and
a module-level attribute. A reload that kept any of them would end in the "World Memory Leaks" fatal; this test
passing means the editor survived it. (A person's edits and the refusal for them are covered by
vibeue_editor_state_regression.py's Details-panel edit and by the user-edit check here, which uses force=False on
a map with only script edits.)
"""

import sys
import types

import unreal
import vibeue

MAP = "/Game/VibeUETests/L_D1_Reload"


def _check(results, name, expected, actual):
    ok = expected == actual
    (results["passed"] if ok else results["failed"]).append(name)
    results["details"][name] = {"expected": expected, "actual": actual}
    return ok


def _labels(eas):
    return sorted(a.get_actor_label() for a in eas.get_all_level_actors())


async def run():
    # Module globals on purpose: safe_reload_level drops the CALLER's globals that point into the map (here this
    # module's), the way it drops an execute_python script's.
    global keeper, extra, held_list
    results = {"passed": [], "failed": [], "details": {}}
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    # 1. A never-saved level has nothing to reload.
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    try:
        vibeue.safe_reload_level(confirm_discard=True)
        temp = "not refused"
    except vibeue.LevelReloadRefused as exc:
        temp = "never saved" in str(exc)
    _check(results, "/Temp level refused", True, temp)

    # 2. A saved map with one actor.
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        vibeue.safe_load_level(MAP)
    else:
        les.new_level(MAP)
    for actor in eas.get_all_level_actors():
        if actor.get_actor_label().startswith("D1_Reload"):
            eas.destroy_actor(actor)
    keeper = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(100, 0, 0))
    keeper.set_actor_label("D1_ReloadKeep")
    les.save_current_level()
    saved = _labels(eas)

    # 3. Not dirty: reloads without confirm_discard.
    reply = vibeue.safe_reload_level()
    _check(results, "clean map reloads without confirm_discard", True, reply["reloaded"])
    _check(results, "nothing discarded from a clean map", [], reply["discarded"])

    # 4. Script edits make it dirty; without confirm_discard it refuses.
    keeper = [a for a in eas.get_all_level_actors() if a.get_actor_label() == "D1_ReloadKeep"][0]
    with unreal.ScopedEditorTransaction("D1 reload test edits"):
        keeper.modify()
        keeper.set_actor_location(unreal.Vector(900, 0, 0), False, False)
    extra = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(0, 0, 300))
    extra.set_actor_label("D1_ReloadExtra")
    try:
        vibeue.safe_reload_level()
        refused = "not refused"
    except vibeue.LevelReloadRefused as exc:
        refused = "confirm_discard=True" in str(exc)
    _check(results, "dirty map without confirm_discard refused", True, refused)

    # 5. Script edits only: no person's edit, so no force needed. Python holds the map's objects three ways.
    held_list = [keeper, extra]                                        # a script global list
    module = types.ModuleType("vibeue_reload_test_holder")
    module.actor = extra                                               # a module-level attribute
    sys.modules[module.__name__] = module
    try:
        found = sorted("{} {}".format(h["where"], h["name"]) for h in vibeue.python_holders_of_level())
        reply = vibeue.safe_reload_level(confirm_discard=True)
        try:
            module.actor.get_actor_label()
            purged = "still alive"
        except Exception as exc:
            purged = "ObjectInstance is null" in str(exc)
    finally:
        del sys.modules[module.__name__]
    _check(results, "holders found", True,
           {"script keeper", "script extra", "script held_list",
            "module vibeue_reload_test_holder actor"} <= set(found))
    _check(results, "reloaded", True, reply["reloaded"])
    _check(results, "script globals released", True,
           {"script keeper", "script extra", "script held_list"} <= set(reply["released"]))
    _check(results, "module attribute purged", ["vibeue_reload_test_holder.actor"], reply["purged"])
    _check(results, "purged reference raises instead of pinning the world", True, purged)
    _check(results, "script globals are gone", [], [n for n in ("keeper", "extra", "held_list") if n in globals()])
    _check(results, "edits discarded (actors as saved)", saved, _labels(eas))
    again = [a for a in eas.get_all_level_actors() if a.get_actor_label() == "D1_ReloadKeep"][0]
    _check(results, "edited location back to the saved one", 100.0, round(again.get_actor_location().x, 1))
    _check(results, "map clean after reload", False, reply["level_dirty"])
    _check(results, "no resident maps after reload", [], reply["resident_maps"])
    _check(results, "still on the map", MAP, vibeue._open_level_package())
    return results
