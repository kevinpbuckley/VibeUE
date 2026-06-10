# Blueprint Macro Nodes Test - Standard Macros & Macro Libraries

Tests placing Standard Macro nodes (ForEachLoop, ForLoop, WhileLoop, DoOnce, Gate, IsValid, FlipFlop, …) via `add_macro_instance_node`, and creating macro graphs on a Macro Library blueprint via `create_macro_graph`. These are `K2Node_MacroInstance` nodes with no spawner key — `discover_nodes` / `create_node_by_key` will silently fail on them, which is the main thing this suite guards against.

---

## Setup

Create an actor blueprint called BP_MacroTest in the Blueprints folder. Delete it first if it already exists.

---

Open BP_MacroTest's EventGraph in the editor.

---

## Placing Standard Macro Nodes

Add a ForEachLoop macro node to the EventGraph at position (200, 0).

---

Add a ForLoop macro node at (200, 250), a WhileLoop at (200, 500), and a DoOnce at (600, 0).

---

Add the remaining standard macros so all of these exist in the graph: ForEachLoopWithBreak, ReverseForEachLoop, ForLoopWithBreak, Gate, IsValid, DoN, FlipFlop. Lay them out so they don't overlap.

---

List all nodes in the EventGraph and confirm there are 11 macro nodes, each with a non-empty node ID.

---

## IsValid Pin Behavior

Show me the pins on the IsValid node. Confirm it exposes BOTH an "Is Valid" and an "Is Not Valid" exec output on the same node (there is no separate IsNotValid macro).

---

## Full Path Form

Add a ForEachLoop using the full `AssetPath:GraphName` string `/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:ForEachLoop` at position (600, 250). Confirm it returns a valid node ID.

---

## Graceful Failure

Try to add a macro node using the shorthand "NotARealMacro". This should return an empty/failed result with a clear error logged and NO crash. Report what came back.

---

Try to add a ForEachLoop using discover_nodes + create_node_by_key instead of add_macro_instance_node. Confirm that path does NOT work for macros (no spawner key) — this documents why add_macro_instance_node is required.

---

## Wiring a Macro Into Real Logic

Add a Print String node at (900, 0).

---

Wire the ForEachLoop's "Loop Body" exec output to the Print String's execute pin, and the ForEachLoop's "Array Element" output toward the Print String input. Confirm the connections appear in the graph.

---

Compile BP_MacroTest and verify it compiles with 0 errors.

---

## Macro Library & create_macro_graph

Create a Macro Library blueprint called BPL_MacroTestLib in the Blueprints folder. Delete it first if it already exists.

---

Create a macro graph called MyCustomMacro on BPL_MacroTestLib.

---

List the graphs on BPL_MacroTestLib and confirm MyCustomMacro appears.

---

Call create_macro_graph for "MyCustomMacro" on BPL_MacroTestLib a second time. Confirm it is idempotent — MyCustomMacro still appears exactly once, no duplicate, no crash.

---

Place an instance of the user-defined MyCustomMacro from BPL_MacroTestLib into BP_MacroTest's EventGraph using the full `AssetPath:GraphName` form. Confirm it returns a valid node ID.

---

## Cleanup

Delete BP_MacroTest and BPL_MacroTestLib.

---

## Summary

Verify:
1. All 11 standard macro shorthands place a valid `K2Node_MacroInstance` node
2. IsValid exposes both "Is Valid" and "Is Not Valid" exec outputs on one node
3. The full `AssetPath:GraphName` form works for both engine and user-defined macros
4. An invalid shorthand fails gracefully (empty result, error logged, no crash)
5. discover_nodes / create_node_by_key do NOT create macro nodes (documents the trap)
6. A macro node wires into normal logic and the blueprint compiles cleanly
7. create_macro_graph adds a graph to a Macro Library blueprint and is idempotent

---
