# Blueprint Function and Nodes Test - Player Damage System

Tests creating a realistic player damage system with functions, parameters, local variables, and node connections.

---

## Setup

Create an actor blueprint called BP_Player in the Blueprints folder. Delete it first if it already exists.

---

Add these variables to BP_Player:
- Health (float, default 100)
- Armor (float, default 50)
- IsAlive (bool, default true)

---

Open BP_Player in the editor.

---

## Creating the Damage Function

Create a function called ApplyDamage in BP_Player.

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

Create another function called CheckDeath in BP_Player.

---

Add an output parameter called IsDead (bool) to CheckDeath.

---

Add nodes to CheckDeath that check if Health is less than or equal to 0 and return the result.

---

Wire the CheckDeath function completely.

---

## Testing in EventGraph

Add a Print String node to the EventGraph.

---

Verify the Print String is in EventGraph, not in any function.

---

## Cross-Graph Test (Should Fail)

Try to connect the DamageAmount parameter from ApplyDamage to the Print String in EventGraph. This should fail with a clear error.

---

## Compile and Verify

Compile BP_Player.

---

List all functions in BP_Player and verify ApplyDamage and CheckDeath exist.

---

Show the final state of the ApplyDamage function with all nodes and connections.

---

## Summary

Verify:
1. BP_Player has Health, Armor, and IsAlive variables
2. ApplyDamage function has correct parameters and local variable
3. CheckDeath function works correctly
4. Cross-graph connections fail with helpful error
5. Blueprint compiles successfully

---

