# Blueprint Lifecycle Tests

Tests for creating, configuring, compiling, and reparenting blueprints. Run sequentially. If you try to create a blueprint and one already exists, delete the blueprint silently and try again.

---

## Creating Blueprints

I need a basic actor blueprint. Put it in the Blueprints folder and call it TestActor.

---

Also make a character blueprint called TestCharacter.

---

## Blueprint Information

Show me everything about the TestActor blueprint.

---

Is replication enabled on TestActor?

---

## Configuring Properties

Turn on replication for TestActor.

---

Compile it.

---

Now show me the info again to verify replication is on.

---

## Reparenting

What class does TestCharacter inherit from?

---

Change it to inherit from Pawn instead of Character.

---

Compile after the reparent.

---

Show me TestCharacter's info to confirm the parent changed.

---

## Event Graph Analysis

What custom events are in TestActor?

---

Give me a summary of the event graph. Keep it under 200 nodes.

---

Try a shorter summary with just 50 nodes max.

---

## More Configuration

Set the initial lifespan on TestActor.

---

Read back the properties to verify.

---

## Compilation

Recompile TestActor.

---

Check the state to make sure it compiled cleanly.

---

## Cleanup

Save any unsaved assets without asking me.

---

Delete the test blueprints created above (TestActor and TestCharacter).

---


---

# Blueprint Variable Tests

Tests for creating and configuring variables with various types. Run sequentially.

---

## Setup

Create an actor blueprint called VariableTest in the Blueprints folder. if it already exists delete it and create a new one.

---

## Finding Types

What widget types are available? I want to store a reference to a UI element.

---

Note down the full type path for user widgets.

---

Add a variable called AttributeWidget using that widget type.

---

Verify it was created.

---

## Basic Types

Add a float variable called Health.

---

Add an integer called MaxHealth.

---

Add a boolean called IsAlive.

---

Add a string called PlayerName.

---

Show me all the variables.

---

## Complex Types

What Niagara system types exist?

---

Add a variable called DeathEffect using the Niagara type.

---

What about sound cue types?

---

Add a JumpSound variable with that type.

---

Show me the variable info for those.

---

## Variable Metadata

Create another Health variable with a Combat category and a helpful tooltip. Make it editable in the details panel.

---

Show me the info on that.

---

Actually, move it to the Stats Combat subcategory.

---

Make it read-only in blueprints so nothing can accidentally change it.

---

Verify those metadata changes.

---

## Default Values

Set Health to default to 100.

---

IsAlive should default to true.

---

Check what values got set.

---

PlayerName should default to "TestPlayer".

---

## Filtering Variables

List all variables in this blueprint.

---

Just show me ones in the Combat category.

---

Find variables with "Health" in the name.

---

Show the list with full metadata included.

---


---

# Blueprint Component Tests

Tests for adding, configuring, and organizing components in blueprints. Run sequentially.

---

## Setup

Create an actor blueprint called ComponentTest in the Blueprints folder. If it already exists delete it and create a new one.

---

## Finding Component Types

What spotlight component types are available in Unreal? I want to see my options before adding one.

---

What properties does the SpotLightComponent class have? I want to know what I can configure.

---

Tell me about the Intensity property on SpotLightComponent. What are its constraints and metadata?

---

## Adding Lights

Make a new actor blueprint called LightTest. If it already exists delete it and create a new one.

---

What components does LightTest have right now?

---

Add a spotlight component to LightTest and call it MainLight.

---

Add another spotlight to LightTest called FillLight.

---

Show me what components LightTest has now.

---

## Configuring Lights

What's the current intensity on MainLight in LightTest?

---

Set MainLight's intensity to 5000.

---

Change MainLight's light color to something warm like orange.

---

Show me all the properties on MainLight in LightTest.

---

Adjust MainLight's cone angle to be narrower.

---

## Organizing Component Hierarchy

Add a scene component called LightRig to LightTest to organize the lights.

---

Put MainLight under the LightRig in LightTest.

---

Move FillLight under LightRig too.

---

Show me the LightTest hierarchy now.

---

## Comparing Components

Compare the MainLight and FillLight properties in LightTest to see the differences.

---

Create another actor blueprint called LightTest2. Add a spotlight called TestLight with intensity 8000.

---

Compare MainLight in LightTest with TestLight in LightTest2 to see cross-blueprint differences.

---

## Removing Components

Remove the FillLight from LightTest.

---

Show me what's left in LightTest.

---

Delete the LightRig from LightTest and take all its children with it.

---

List LightTest components to verify everything got deleted properly.

---

## Other Component Types

Add an audio component to ComponentTest for sound effects.

---

Add a particle system component to ComponentTest.

---

Add a Niagara particle system component to ComponentTest.

---

List all components on ComponentTest now.

---


---

# Blueprint Function and Nodes Test - Player Damage System

Tests creating a realistic player damage system with functions, parameters, local variables, and node connections.

---

## Setup

Create an actor blueprint called BP_Player_Test in the Blueprints folder. Delete it first if it already exists.

---

Add these variables to BP_Player_Test:
- Health (float, default 100)
- Armor (float, default 50)
- IsAlive (bool, default true)

---

Open BP_Player_Test in the editor.

---

## Creating the Damage Function

Create a function called ApplyDamage in BP_Player_Test.

---

Open the ApplyDamage function graph in the editor so we can see it.

---

Add these parameters to ApplyDamage:
- Input: DamageAmount (float)
- Input: IgnoreArmor (bool) 
- Output: DamageApplied (bool)

---

Add a local variable called DamageMultiplier (float) to ApplyDamage.

---

Show me the ApplyDamage function info including parameters and local variables.

---

## Building the Damage Logic

Add nodes to ApplyDamage that will:
1. Check if DamageAmount is greater than 0
2. If IgnoreArmor is false, subtract from Armor first
3. Any remaining damage goes to Health
4. Set DamageApplied to true if any damage was dealt

Start by adding a Branch node to check if damage should be applied.

---

Add nodes to get and set the Armor variable.

---

Add a Clamp node to ensure damage doesn't go negative.

---

Wire the function entry to the Branch node.

---

Connect the DamageAmount parameter to a Greater Than comparison with 0.

---

Show me all the nodes in ApplyDamage now.

---

## Connecting the Logic

Wire the nodes so the damage flows through the armor check first, then to health.

---

Connect the final result to DamageApplied output.

---

Show the current connections in ApplyDamage.

---

## Adding Death Check Function

Create another function called CheckDeath in BP_Player_Test.

---

Add an output parameter called IsDead (bool) to CheckDeath.

---

Add nodes to CheckDeath that check if Health is less than or equal to 0 and return the result.

---

Wire the CheckDeath function completely.

---

## Testing in EventGraph

Open the EventGraph of BP_Player_Test.

---

Add a Print String node to the EventGraph.

---

Verify the Print String is in EventGraph, not in any function.

---

## Cross-Graph Test (Should Fail)

Try to connect the DamageAmount parameter from ApplyDamage to the Print String in EventGraph. This should fail with a clear error.

---

## Compile and Verify

Compile BP_Player_Test.

---

List all functions in BP_Player_Test and verify ApplyDamage and CheckDeath exist.

---

Show the final state of the ApplyDamage function with all nodes and connections.

---

## Input Action Integration - Ragdoll System

Delete the input action IA_RagdollTest in /Game/Input if it exists.

---

Create a new input action called IA_RagdollTest in /Game/Input with value type Digital (Bool).

---

Find the mapping context IMC_Sandbox in /Game/Input and add a mapping for IA_RagdollTest bound to the L key with a Pressed trigger.

---

Add an EnhancedInputAction node for IA_RagdollTest to BP_Player_Test's EventGraph at position (200, 400).

---

Discover nodes in BP_Player_Test searching for "SetSimulatePhysics". We need this to enable ragdoll on the mesh.

---

Add a Get Mesh node to the EventGraph at position (400, 350).

---

Add a SetSimulatePhysics node at position (500, 400).

---

Wire the IA_RagdollTest Started pin to SetSimulatePhysics execute pin.

---

Wire Get Mesh return value to SetSimulatePhysics target pin.

---

Set the bSimulate parameter on SetSimulatePhysics to true (using a literal or set the default).

---

Compile BP_Player_Test and verify the ragdoll input chain is complete.

---

## Additional Input Action Tests

Check what input actions exist in /Game/Input (should now include IA_RagdollTest).

---

Add an EnhancedInputAction node for IA_Jump (or any available input action) to BP_Player_Test's EventGraph at position (200, 600).

---

Add a call to our ApplyDamage function in the EventGraph at position (400, 400).

---

Wire the input action's Started pin to the ApplyDamage function call.

---

Set the DamageAmount parameter on ApplyDamage to use a Make Literal Float node with value 10.

---

## Calling Local Functions with Self

Add another call to ApplyDamage using "Self" as the class name (tests the local function call pattern).

---

Add a call to CheckDeath at position (600, 400).

---

Wire the ApplyDamage output to CheckDeath input execution pin.

---

## Parent Class Function Discovery

Discover nodes in BP_Player_Test searching for "Destroy". Should find DestroyActor and similar functions from Actor parent class.

---

Add a DestroyActor node to the EventGraph.

---

Wire CheckDeath to a Branch node, then True to DestroyActor.

---

## Final Compile and Verification

Compile BP_Player_Test.

---

List all nodes in the EventGraph and verify the input action chain is complete.

---

## Summary

Verify:
1. BP_Player_Test has Health, Armor, and IsAlive variables
2. ApplyDamage function has correct parameters and local variable
3. open_function_graph opens ApplyDamage and EventGraph correctly
4. CheckDeath function works correctly
5. Cross-graph connections fail with helpful error
6. Blueprint compiles successfully
7. IA_RagdollTest created and bound to L key in IMC_Sandbox
8. Ragdoll chain: IA_RagdollTest Started → GetMesh → SetSimulatePhysics(true)
9. Input action node triggers ApplyDamage on Started event
10. Local function calls work with "Self" class pattern
11. Parent class functions (DestroyActor) are discoverable and callable

---


---

# Node Discovery and Input Action Tests

Tests the refactored discover_nodes functionality that uses BlueprintActionDatabase to find:
- Local blueprint functions (Self functions)
- Parent class functions
- Input action nodes (Enhanced Input)
- Library functions (Math, System, etc.)

---

## Setup

Create a Character blueprint called BP_NodeDiscoveryTest in /Game/Blueprints/Tests. Delete it first if it already exists.

---

Add two functions to BP_NodeDiscoveryTest:
- HandleDamage (no parameters)
- OnJumpPressed (no parameters)

---

## Test 1: Discover Local Functions

Discover nodes in BP_NodeDiscoveryTest searching for "HandleDamage". This should find the local function we just created.

---

Discover nodes searching for "OnJumpPressed". Verify it shows up under "Self Functions" category.

---

## Test 2: Discover Parent Class Functions (Character)

Discover nodes searching for "Ragdoll". Since BP_NodeDiscoveryTest inherits from Character, it should find Ragdoll_Start and Ragdoll_End functions from the parent class.

---

Discover nodes searching for "GetMesh". This should find the Character's GetMesh function.

---

Discover nodes searching for "Jump". Should find CanJump, Jump, StopJumping, and other jump-related functions from Character and its parents.

---

## Test 3: Discover Input Action Nodes

First, make sure we have an Input Action to work with. List input actions in /Game/Input.

---

Discover nodes searching for "IA_Jump" or similar input action name. Enhanced Input action nodes should appear in the results.

---

Discover nodes searching for "EnhancedInputAction". Should find the generic Enhanced Input Action event node.

---

## Test 4: Discover Library Functions

Discover nodes searching for "PrintString". Should find the utility function.

---

Discover nodes searching for "Delay". Should find the Delay node.

---

Discover nodes searching for "GetActorLocation". Should find this common Actor function.

---

## Test 5: Category Filtering

Discover nodes searching for "Math" with category filter "Math". Should only show math-related nodes.

---

Discover nodes with empty search term but category "Self Functions" to list all local functions in the blueprint.

---

## Test 6: Add Input Action Node to Blueprint

Add an EnhancedInputAction node for IA_Jump (or whatever jump action exists) to BP_NodeDiscoveryTest's EventGraph at position (0, 200).

---

Verify the input action node was added by listing nodes in the EventGraph.

---

## Test 7: Connect Input Action to Local Function

Add a call to our HandleDamage function in the EventGraph.

---

Wire the input action's Started pin to the HandleDamage function call.

---

Compile BP_NodeDiscoveryTest and verify no errors.

---

## Test 8: Add Parent Class Function Call

Add a call to Ragdoll_Start (from Character parent class) using "Self" as the class name.

---

Wire the input action's Triggered pin to Ragdoll_Start.

---

Show all nodes and connections in the EventGraph.

---

## Test 9: MaxResults Limit

Discover nodes with empty search term (to get all nodes) with MaxResults set to 10. Verify only 10 results are returned.

---

## Test 10: Search Keywords

Discover nodes searching for "clamp". Should find Clamp, ClampAngle, ClampAxis, and similar math functions.

---

Discover nodes searching for "lerp". Should find various interpolation functions.

---

## Cleanup

Delete BP_NodeDiscoveryTest.

---

## Summary Verification

The discover_nodes refactor should now find:
1. ✅ Local blueprint functions under "Self Functions" category
2. ✅ Parent class functions (Character's Ragdoll_Start, GetMesh, Jump, etc.)
3. ✅ Enhanced Input Action nodes
4. ✅ Library functions (PrintString, Delay, Math operations)
5. ✅ Functions searchable by keywords (lerp → interpolation functions)
6. ✅ Category filtering works correctly
7. ✅ MaxResults limiting works

---


---

# Blueprint Interface Tests

Tests for adding and removing Blueprint Interfaces on a Blueprint class
(`add_interface` / `remove_interface`). Run sequentially. If an asset already
exists, delete it silently and recreate it.

---

## Setup

Create a Blueprint Interface asset called BPI_Interactable in the Interfaces folder.
Give it one function called Interact. Save it.

---

Create an actor blueprint called InterfaceTest in the Blueprints folder.

---

## Add an Interface (full path)

Add the BPI_Interactable interface to InterfaceTest using its full asset path.

---

Show me the implemented interfaces on InterfaceTest and confirm BPI_Interactable is listed.

---

## Idempotency

Add the same BPI_Interactable interface to InterfaceTest again. It should succeed without
creating a duplicate.

---

Confirm BPI_Interactable still appears exactly once.

---

## Implement the interface function

The interface has an Interact function. Override it on InterfaceTest so it shows up in the graph,
then compile and save the blueprint.

---

## Add an Interface (short name)

Make a second actor blueprint called InterfaceTest2 in the Blueprints folder.

---

The BPI_Interactable asset is already loaded, so add it to InterfaceTest2 using just the short
name "BPI_Interactable" (no path). Confirm it was added.

---

## Remove an Interface

Remove the BPI_Interactable interface from InterfaceTest. Compile and save.

---

Verify BPI_Interactable is no longer listed on InterfaceTest.

---

## Error handling

Try to add an interface called BPI_DoesNotExist to InterfaceTest2. This should fail gracefully
with a clear message rather than crashing — report what happened.

---

Now try to add InterfaceTest2 itself (a regular actor blueprint, NOT an interface) as an interface
on InterfaceTest. This must fail gracefully and return false — it must NOT crash the editor.
Report what happened.

---

Try to remove BPI_Interactable from InterfaceTest a second time (it was already removed). Report
the result.

---

## Cleanup

Delete InterfaceTest, InterfaceTest2, and BPI_Interactable.


---

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

