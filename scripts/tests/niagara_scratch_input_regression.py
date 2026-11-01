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
    second = service.create_scratch_module(path, emitter, "ParticleUpdate", "SecondRegression")
    assert second.success, second.message
    second_ref = dict(ref, moduleName=second.module_name)
    assert vibeue.exec_tool(tool, "GetModuleTopology", {"moduleRef": second_ref})["inputs"] == []
    assert unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).open_editor_for_assets(
        [unreal.EditorAssetLibrary.load_asset(path)]
    )
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
    result = service.add_module_input(path, emitter, second.module_name, "SecondValue", "float")
    assert result.success, result.message
    result = service.add_module_output(path, emitter, second.module_name, "Particles.SecondValue", "float")
    assert result.success, result.message
    second_node = service.add_custom_hlsl_node(path, emitter, second.module_name, "OutputValue = InputValue;", 200, 0)
    assert second_node.success, second_node.message
    for direction, pin in [("Input", "InputValue"), ("Output", "OutputValue")]:
        result = service.add_pin(path, emitter, second.module_name, second_node.node_id, direction, "float", pin)
        assert result.success, result.message
    second_nodes = {n.node_type: n.node_id for n in service.list_nodes(path, emitter, second.module_name)}
    assert service.connect_pins(path, emitter, second.module_name, second_nodes["MapGet"], "Module.SecondValue", second_node.node_id, "InputValue")
    assert service.connect_pins(path, emitter, second.module_name, second_node.node_id, "OutputValue", second_nodes["MapSet"], "Particles.SecondValue")
    assert service.apply_changes(path), list(service.get_compile_messages(path))
    assert not list(service.get_compile_messages(path))
    names = input_names()
    assert names.count("Scale") == 1 and names.count("Bias") == 1, names
    second_names = [entry["name"] for entry in vibeue.exec_tool(tool, "GetModuleTopology", {"moduleRef": second_ref})["inputs"]]
    assert second_names.count("SecondValue") == 1, second_names

    ref["inputNameStack"] = ["Scale"]
    value = {"struct": {"refPath": "/Script/Niagara.NiagaraFloat"}, "value": {"value": 2.5}}
    vibeue.exec_tool(tool, "SetStackInputData", {"stackInputRef": ref, "inputData": value})
    second_ref["inputNameStack"] = ["SecondValue"]
    second_value = {"struct": {"refPath": "/Script/Niagara.NiagaraFloat"}, "value": {"value": 3.5}}
    vibeue.exec_tool(tool, "SetStackInputData", {"stackInputRef": second_ref, "inputData": second_value})
    assert vibeue.exec_tool(tool, "GetStackInputData", {"stackInputRef": ref})["value"]["value"] == 2.5
    assert vibeue.exec_tool(tool, "GetStackInputData", {"stackInputRef": second_ref})["value"]["value"] == 3.5

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
    result = service.add_module_input(path, emitter, second.module_name, "SecondExtra", "float")
    assert result.success, result.message
    result = service.add_module_output(path, emitter, second.module_name, "Particles.SecondExtra", "float")
    assert result.success, result.message
    assert service.connect_pins(path, emitter, second.module_name, second_nodes["MapGet"], "Module.SecondExtra", second_nodes["MapSet"], "Particles.SecondExtra")
    assert service.apply_changes(path), list(service.get_compile_messages(path))
    names = input_names()
    assert all(names.count(name) == 1 for name in ("Scale", "Bias", "Extra")), names
    second_names = [entry["name"] for entry in vibeue.exec_tool(tool, "GetModuleTopology", {"moduleRef": second_ref})["inputs"]]
    assert all(second_names.count(name) == 1 for name in ("SecondValue", "SecondExtra")), second_names
    actual = vibeue.exec_tool(tool, "GetStackInputData", {"stackInputRef": ref})
    assert actual["value"]["value"] == 2.5, actual
    second_actual = vibeue.exec_tool(tool, "GetStackInputData", {"stackInputRef": second_ref})
    assert second_actual["value"]["value"] == 3.5, second_actual
    assert not list(service.get_compile_messages(path))
    print("PASS", path, "inputs", names, second_names, "preserved", 2.5, 3.5)
