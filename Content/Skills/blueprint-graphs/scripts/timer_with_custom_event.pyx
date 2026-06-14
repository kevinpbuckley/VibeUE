# timer_with_custom_event.pyx — Non-blocking timer: Set Timer by Event + a Custom Event callback.
#
# Sample script for the blueprint-graphs skill. Run via execute_python_code.
# Use this pattern (NOT Delay) when the Blueprint must keep ticking during the wait.
# Create + verify one node at a time for fragile graphs; re-find custom events by returned GUID.
import unreal
bs = unreal.BlueprintService

BP = "/Game/Blueprints/BP_GraphTest"   # must already exist
GRAPH = "EventGraph"

# 1. Set Timer by Event node
timer_id = bs.add_function_call_node(BP, GRAPH, "KismetSystemLibrary", "K2_SetTimerDelegate", 600, 0)
assert timer_id, "timer node not created"
bs.set_node_pin_value(BP, GRAPH, timer_id, "Time", "1.0")
bs.set_node_pin_value(BP, GRAPH, timer_id, "bLooping", "false")

# 2. Custom Event callback (re-find it by the returned GUID — titles can be multi-line)
custom_id = bs.add_custom_event_node(BP, GRAPH, "OnTimerFinished", 600, 300)
custom = next((n for n in bs.get_nodes_in_graph(BP, GRAPH) if n.node_id == custom_id), None)
assert custom, "custom event not found after create"

# 3. Wire CustomEvent.OutputDelegate -> Timer.Delegate
assert bs.connect_nodes(BP, GRAPH, custom_id, "OutputDelegate", timer_id, "Delegate")

# 4. Verify + compile
for c in bs.get_connections(BP, GRAPH):
    print(f"{c.source_node_title}.{c.source_pin_name} -> {c.target_node_title}.{c.target_pin_name}")
r = bs.compile_blueprint(BP)
print("compile:", r.success, "errors:", r.num_errors)
unreal.EditorAssetLibrary.save_asset(BP)
