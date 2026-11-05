"""Check incomplete Landscape rejection in an existing disposable WP fixture.

Run ``run(map_path, landscape_label, from_path, to_path, proxy_paths)`` with
all listed proxies unloaded and the parent Landscape loaded. The fixture must
have at least two saved streaming proxies. The check loads one proxy, verifies
rejection, then loads all proxies and exercises a successful replacement/Undo.
Unload the proxies again before repeating the check.
It never saves packages; the control changes Undo history and may leave the
disposable fixture packages dirty even after the layer identities are restored.
"""

import unreal


def _targets(actor):
    return [(str(key), value.get_editor_property("layer_info_obj"))
            for key, value in actor.get_editor_property("target_layers").items()]


def _replace_and_undo(world, parent, label, source, destination, proxies):
    service = unreal.LandscapeService
    before = [(str(layer.layer_name), layer.layer_info_path) for layer in service.list_layers(label)]
    proxy_before = {actor.get_path_name(): _targets(actor) for actor in proxies}
    recorded = False
    try:
        with unreal.ScopedEditorTransaction("Landscape partial-load regression"):
            recorded = bool(parent.modify(False))
            assert recorded, "could not establish the test transaction"
            result = service.replace_layer_info(label, source, destination)
            during = [(str(layer.layer_name), layer.layer_info_path) for layer in service.list_layers(label)]
            proxy_during = {actor.get_path_name(): _targets(actor) for actor in proxies}
            dirty = ([package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()],
                     [package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()])
    finally:
        if recorded:
            unreal.SystemLibrary.execute_console_command(world, "TRANSACTION UNDO")
    assert [(str(layer.layer_name), layer.layer_info_path) for layer in service.list_layers(label)] == before, "Undo did not restore parent target"
    assert {actor.get_path_name(): _targets(actor) for actor in proxies} == proxy_before, "Undo did not restore proxy targets"
    return result, before, during, proxy_before, proxy_during, dirty


def run(map_path, landscape_label, from_path, to_path, proxy_paths):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_path_name().split(".")[0] == map_path, "wrong map"
    assert len(proxy_paths) >= 2 and len(set(proxy_paths)) == len(proxy_paths), "invalid proxy list"
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), "dirty map packages"
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(), "dirty content packages"

    service = unreal.LandscapeService
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    parents = [actor for actor in actors if isinstance(actor, unreal.Landscape)
               and actor.get_actor_label() == landscape_label]
    assert len(parents) == 1, "parent Landscape missing or ambiguous"
    parent = parents[0]
    assert not any(isinstance(actor, unreal.LandscapeStreamingProxy)
                   and actor.get_editor_property("landscape_actor_ref") == parent
                   for actor in actors), "unload all target Landscape proxies before running"
    source = unreal.EditorAssetLibrary.load_asset(from_path)
    destination = unreal.EditorAssetLibrary.load_asset(to_path)
    assert source and destination and source != destination, "invalid LayerInfo assets"
    for field in ("layer_name", "blend_method", "phys_material", "minimum_collision_relevance_weight"):
        assert source.get_editor_property(field) == destination.get_editor_property(field), field
    if source.get_editor_property("blend_method") == unreal.LandscapeTargetLayerBlendMethod.PREMULTIPLIED_ALPHA_BLENDING:
        assert source.get_editor_property("blend_group") == destination.get_editor_property("blend_group"), "blend_group"
    before = [(str(layer.layer_name), layer.layer_info_path) for layer in service.list_layers(landscape_label)]
    assert any(path == source.get_path_name() for _, path in before), "source is not a target layer"
    paths = set(proxy_paths)
    descriptions = [desc for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs()
                    if str(desc.get_editor_property("actor_path")) in paths]
    assert len(descriptions) == len(paths), "missing or duplicate proxy descriptors"
    assert all(desc.get_editor_property("native_class") == unreal.LandscapeStreamingProxy.static_class()
               for desc in descriptions), "descriptor is not a Landscape streaming proxy"

    def live_proxies():
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        return [actor for actor in actors if actor.get_path_name() in paths]

    assert not live_proxies(), "unload every listed proxy before running"
    guids = [desc.get_editor_property("guid") for desc in descriptions]
    result, _, during, _, _, dirty = _replace_and_undo(world, parent, landscape_label, source, destination, [])
    assert result is False and during == before and not any(dirty), "unloaded replacement was not rejected cleanly"
    try:
        unreal.WorldPartitionBlueprintLibrary.load_actors([guids[0]])
        live = live_proxies()
        assert len(live) == 1, "could not establish a partially loaded Landscape"
        proxy = live[0]
        assert proxy.get_editor_property("landscape_actor_ref") == parent, "proxy belongs to another Landscape"
        result, _, during, proxy_before, proxy_during, dirty = _replace_and_undo(
            world, parent, landscape_label, source, destination, live)
        assert result is False and during == before and proxy_before == proxy_during and not any(dirty), "partial replacement was not rejected cleanly"
    finally:
        unreal.WorldPartitionBlueprintLibrary.load_actors(guids)
        restored = live_proxies()
        print("RESTORED", len(restored), "of", len(paths), "proxies")
        assert len(restored) == len(paths), "failed to restore fixture proxies"
        assert all(proxy.get_editor_property("landscape_actor_ref") == parent for proxy in restored), "wrong Landscape proxies"
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()

    result, _, during, _, proxy_during, _ = _replace_and_undo(
        world, parent, landscape_label, source, destination, restored)
    assert result is True, "loaded positive control failed"
    assert any(path == destination.get_path_name() for _, path in during), "parent not rebound"
    assert all(path != source.get_path_name() for _, path in during), "parent still references source"
    assert all(any(value == destination for _, value in values) for values in proxy_during.values()), "proxies not rebound"
    assert all(all(value != source for _, value in values) for values in proxy_during.values()), "proxy still references source"
    print("DIRTY_AFTER_UNDO", [package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()])
    print("PASS: unloaded/partial rejection and fully loaded replacement with Undo")
