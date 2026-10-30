"""Editor integration check; run runpy.run_path(...)["run"] with a unique asset name.

Creates and saves a disposable system under /Game/Developers/VibeUEVerification.
It deliberately leaves the asset in place for inspection; never overwrites an asset.
"""

import unreal
import vibeue


def run(name):
    path = "/Game/Developers/VibeUEVerification/" + name
    assert not unreal.EditorAssetLibrary.does_asset_exist(path), path
    assert unreal.EditorAssetLibrary.duplicate_asset(
        "/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion", path
    )
    print("CREATED", path)
    emitter = "OmnidirectionalBurst"
    service = unreal.NiagaraScratchPadService
    created = service.create_scratch_module(path, emitter, "ParticleUpdate", "InputRegression")
    assert created.success, created.message
    module = created.module_name
    ref = {
        "system": {"refPath": path + "." + name},
        "emitterName": emitter,
        "scriptName": "ParticleUpdateScript",
        "moduleName": module,
        "rendererIndex": 0,
        "inputNameStack": [],
    }
    tool = "NiagaraToolsets.NiagaraToolset_System"

    def input_names():
        result = vibeue.exec_tool(tool, "GetModuleTopology", {"moduleRef": ref})
        return [entry["name"] for entry in result["inputs"]]

    assert input_names() == []  # Populate the original module's stack view before editing.
    for value in ("Scale", "Bias"):
        result = service.add_module_input(path, emitter, module, value, "float")
        assert result.success, result.message
    result = service.add_module_output(path, emitter, module, "Particles.RegressionValue", "float")
    assert result.success, result.message
    node = service.add_custom_hlsl_node(path, emitter, module, "Result = Scale + Bias;", 200, 0)
    assert node.success, node.message
    for direction, value in [("Input", "Scale"), ("Input", "Bias"), ("Output", "Result")]:
        result = service.add_pin(path, emitter, module, node.node_id, direction, "float", value)
        assert result.success, result.message
    nodes = {n.node_type: n.node_id for n in service.list_nodes(path, emitter, module)}
    for source, source_pin, target, target_pin in [
        (nodes["MapGet"], "Module.Scale", node.node_id, "Scale"),
        (nodes["MapGet"], "Module.Bias", node.node_id, "Bias"),
        (node.node_id, "Result", nodes["MapSet"], "Particles.RegressionValue"),
    ]:
        assert service.connect_pins(path, emitter, module, source, source_pin, target, target_pin)
    assert service.apply_changes(path), list(service.get_compile_messages(path))
    assert not list(service.get_compile_messages(path))
    names = input_names()
    assert names.count("Scale") == 1 and names.count("Bias") == 1, names

    ref["inputNameStack"] = ["Scale"]
    value = {"struct": {"refPath": "/Script/Niagara.NiagaraFloat"}, "value": {"value": 2.5}}
    vibeue.exec_tool(tool, "SetStackInputData", {"stackInputRef": ref, "inputData": value})

    result = service.add_module_input(path, emitter, module, "Extra", "float")
    assert result.success, result.message
    result = service.add_module_output(path, emitter, module, "Particles.ExtraValue", "float")
    assert result.success, result.message
    extra = service.add_custom_hlsl_node(path, emitter, module, "OutputValue = InputValue;", 400, 0)
    assert extra.success, extra.message
    for direction, pin in [("Input", "InputValue"), ("Output", "OutputValue")]:
        result = service.add_pin(path, emitter, module, extra.node_id, direction, "float", pin)
        assert result.success, result.message
    assert service.connect_pins(path, emitter, module, nodes["MapGet"], "Module.Extra", extra.node_id, "InputValue")
    assert service.connect_pins(path, emitter, module, extra.node_id, "OutputValue", nodes["MapSet"], "Particles.ExtraValue")
    assert service.apply_changes(path), list(service.get_compile_messages(path))
    names = input_names()
    assert all(names.count(name) == 1 for name in ("Scale", "Bias", "Extra")), names
    actual = vibeue.exec_tool(tool, "GetStackInputData", {"stackInputRef": ref})
    assert actual["value"]["value"] == 2.5, actual
    assert not list(service.get_compile_messages(path))
    print("PASS", path, "inputs", names, "preserved Scale", actual["value"]["value"])
