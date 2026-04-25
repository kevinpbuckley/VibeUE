# PCG Graph Tests

Tests for creating, editing, and connecting nodes in PCG Graph assets via the PCGPythonInterop plugin.
Run sequentially. Requires PCG and PCGPythonInterop plugins enabled in the project.

---

## Prerequisites

Check whether the PCG and PCGPythonInterop plugins are loaded by verifying that `unreal.PCGGraph` exists in the Python environment. Report the result clearly.

---

Search for any existing PCG graph assets with "PCGTest" in the name (use manage_asset action="search") and delete them if found.

---

## Graph Creation

Create a new PCG Graph asset called "PCG_TestGraph" saved to "/Game/PCGTest". Confirm the asset exists at that path.

---

Open "PCG_TestGraph" in the PCG editor.

---

## Inspect Empty Graph

List all nodes currently in PCG_TestGraph. The `nodes` array will be empty — Input and Output nodes are not included in it, access them via get_input_node() and get_output_node().

---

Get the Input node from PCG_TestGraph and print its output pin labels. Note: the Input node's output pin is labelled "In" — this is intentional in PCG.

---

Get the Output node from PCG_TestGraph and print its input pin labels. Note: the Output node's input pin is labelled "Out" — this is intentional in PCG.

---

## Adding Nodes

Add a Surface Sampler node to PCG_TestGraph. Print the settings class name (node_title will be None — identify nodes via type(node.get_settings()).__name__) and its output pin labels.

---

Add a Static Mesh Spawner node to PCG_TestGraph. Print its settings class name and input pin labels.

---

Add a Density Filter node to PCG_TestGraph.

---

Add a Transform Points node to PCG_TestGraph.

---

Add a Merge node to PCG_TestGraph.

---

List all nodes in PCG_TestGraph now by their settings class name. Should show Surface Sampler, Static Mesh Spawner, Density Filter, Transform Points, and Merge — 5 nodes (Input and Output are not in the nodes array).

---

## Node Positioning

Set the Surface Sampler node to position (200, 0).

---

Set the Density Filter node to position (400, 0).

---

Set the Transform Points node to position (600, 0).

---

Set the Static Mesh Spawner node to position (800, 0).

---

Set the Merge node to position (600, -300).

---

Get the position of the Surface Sampler node and verify it is (200, 0). Note: get_node_position() returns a plain tuple — unpack as x, y = node.get_node_position().

---

## Configuring Node Settings

Set the Surface Sampler's `points_per_squared_meter` to 3.0 and `point_extents` to (50, 50, 50).

---

Read back the `points_per_squared_meter` property from the Surface Sampler. Should be 3.0.

---

Set the `unbounded` property to True on the Surface Sampler.

---

Read back `unbounded` from the Surface Sampler. Should be True.

---

## Inspecting Pin Labels

Print all input and output pin labels for the Surface Sampler node. Labels are accessed via pin.get_editor_property('properties').get_editor_property('label') — NOT directly via pin.get_editor_property('label'). Expect main input = "Surface" (not "In").

---

Print all input and output pin labels for the Density Filter node. Expect main input = "In".

---

Print all input and output pin labels for the Static Mesh Spawner node. Expect main input = "In".

---

## Connecting Nodes — Main Chain

Connect the Input node's output pin ("In") to the Surface Sampler's input pin ("Surface"). Note the inverted naming: the graph Input node's output pin is labelled "In".

---

Connect the Surface Sampler's output pin to the Density Filter's input pin.

---

Connect the Density Filter's output pin to the Transform Points' input pin.

---

Connect the Transform Points' output pin to the Static Mesh Spawner's input pin.

---

Connect the Static Mesh Spawner's output pin ("Out") to the Output node's input pin ("Out"). Note: the graph Output node's input pin is labelled "Out".

---

## Connecting Nodes — Branch

Connect the Surface Sampler's output pin to the Merge node's input pin (first input).

---

Connect the Merge node's output pin to the Output node's input pin.

---

## Verify Connections

List all nodes in PCG_TestGraph and their connected edges. Confirm the main chain (Input → Sampler → Filter → Transform → Spawner → Output) is fully wired.

---

## Removing an Edge

Disconnect the edge between the Density Filter and Transform Points nodes.

---

Verify the disconnection — list the edges on the Density Filter's output pin. Should be empty.

---

## Reconnecting

Reconnect the Density Filter output to the Transform Points input.

---

Verify the reconnection — the edge should be present again.

---

## Removing a Node

Remove the Merge node from PCG_TestGraph.

---

List all nodes in PCG_TestGraph. Merge node should no longer appear. Count should be 6.

---

## Force Editor Notification

Force a structural notification on PCG_TestGraph so the PCG editor refreshes.

---

## Adding a Generic Node by Class Name

Add a node to PCG_TestGraph using the generic approach — use `unreal.PCGCopyPointsSettings` directly (no find_class, all Settings classes are direct attributes of the unreal module). Position it at (400, -300).

---

List all nodes again. CopyPoints node should appear.

---

Connect the Surface Sampler output ("Out") to the CopyPoints "Source" pin — CopyPoints has no "In" pin, it uses "Source" and "Target". Connect CopyPoints output to the Output node's input ("Out").

---

## Subgraph Node

Create a second PCG Graph asset called "PCG_SubgraphTest" saved to "/Game/PCGTest".

---

Add a Surface Sampler node and a Static Mesh Spawner node to PCG_SubgraphTest. Connect Input → Sampler → Spawner → Output.

---

Save PCG_SubgraphTest.

---

Back in PCG_TestGraph, add a Subgraph node that references PCG_SubgraphTest. Assign it via settings.set_editor_property('subgraph_override', sub_graph_asset) — the property is 'subgraph_override', not 'subgraph' (which is protected).

---

Position the Subgraph node at (200, -300).

---

Connect the Input node's output to the Subgraph node's input.

---

List all nodes in PCG_TestGraph and confirm the Subgraph node is present.

---

## Save and Reload

Save PCG_TestGraph.

---

Reload PCG_TestGraph from disk and list all its nodes. Confirm the node count and connections match what was built.

---

## Edge Cases

Try to add an edge between two pins that are already connected. Expected: returns True permissively (does not error or deduplicate — be aware of this).

---

Try to add an edge between incompatible pin types (if any exist in this graph). Report what happens.

---

Try to remove an edge that doesn't exist. Should return false or report gracefully — not crash.

---

Try to get settings from the Output node. Note: it returns PCGGraphInputOutputSettings (not None) — the Output node does have settings.

---

## Bulk Node Removal

Add 5 Point Filter nodes to PCG_TestGraph using unreal.PCGPointFilterSettings. Note: the runtime settings class name will be PCGAttributeFilteringSettings — use that when filtering nodes by type.

---

List all nodes — confirm 5 new Point Filter nodes are present.

---

Remove all 5 Point Filter nodes one by one.

---

List nodes again — Point Filter nodes should be gone, others intact.

---

## Graph Properties

Set the `description` property on PCG_TestGraph to "Test graph for VibeUE PCG skill validation".

---

Read back the `description`. Should match what was set.

---

Set `is_standalone_graph` to True on PCG_TestGraph.

---

Read back `is_standalone_graph`. Should be True.

---

## Final Save and Verify

Force a structural notification on PCG_TestGraph.

---

Save both PCG_TestGraph and PCG_SubgraphTest.

---

Verify both assets exist at /Game/PCGTest by searching for PCG assets with "PCGTest" in the name.

---

## Cleanup

Delete all assets under /Game/PCGTest.

---

Search for PCG assets with "PCGTest" in the name. Should return nothing.
