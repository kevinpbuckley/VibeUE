# build_event_graph.pyx — Wire Event BeginPlay -> Print String, then verify + compile.
#
# Sample script for the blueprint-graphs skill. Run via execute_python_code.
# Demonstrates: find an existing event node by title, add a function-call node, connect, verify.
import unreal
bs = unreal.BlueprintService

BP = "/Game/Blueprints/BP_GraphTest"   # must already exist (Actor-based)
GRAPH = "EventGraph"

# 1. Add the Print String node
ps = bs.add_function_call_node(BP, GRAPH, "KismetSystemLibrary", "PrintString", 400, 0)
assert ps, "PrintString node not created"

# 2. Find the BeginPlay event by its DISPLAY title (event-style overrides live in EventGraph)
begin = next((n for n in bs.get_nodes_in_graph(BP, GRAPH) if n.node_title == "Event BeginPlay"), None)
assert begin, "Event BeginPlay not found"

# 3. Connect exec: BeginPlay.then -> PrintString.execute
assert bs.connect_nodes(BP, GRAPH, begin.node_id, "then", ps, "execute")

# 4. Verify the connection exists, then compile
for c in bs.get_connections(BP, GRAPH):
    print(f"{c.source_node_title}.{c.source_pin_name} -> {c.target_node_title}.{c.target_pin_name}")
r = bs.compile_blueprint(BP)
print("compile:", r.success, "errors:", r.num_errors)
unreal.EditorAssetLibrary.save_asset(BP)
