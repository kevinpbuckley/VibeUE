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

