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
