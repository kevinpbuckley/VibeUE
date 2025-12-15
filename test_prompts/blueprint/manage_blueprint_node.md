# Blueprint Node Tests

Tests for creating, connecting, and configuring nodes in blueprint graphs. Run sequentially.

---

## Setup

Create an actor blueprint called NodeTest in the Blueprints folder.

---

Open it in the editor.

---

Add a function called GetRandomNumber with integer inputs for Low and High, and an integer output for the result.

---

Also add a float variable called TestValue.

---

## Building a Function

What node gives me a random integer within a range?

---

Create that random node in the function at position 400, 100.

---

Hook up the input parameters to it.

---

Compile and make sure it works.

---

## Discovering Nodes

Show me what node options there are for getting the player controller.

---

What spawner keys are available for that?

---

Create a new function called TestNodes.

---

Add the player controller node using one of those spawner keys.

---

Make sure it's there.

---

## Laying Out Nodes

Make another function called TestNodeLayout.

---

Add a get player controller node at position 300, 100.

---

Put a get HUD node at 600, 100.

---

And a print string node at 900, 100.

---

Show me all the nodes in that function.

---

## Connecting Nodes

Tell me about the pins on these nodes.

---

Connect the execution pins so they run in sequence.

---

Connect the player controller output to the HUD node input.

---

Connect the HUD output to the print message.

---

Show me the connections to verify.

---

## Working with Struct Pins

Add a make vector node somewhere.

---

Split the vector pin into X, Y, Z components.

---

Verify the individual pins are exposed.

---

Combine them back into a single vector pin.

---

## Node Properties

What properties can I set on that print string node?

---

What's the current value?

---

Change it to a debug message.

---

Verify it updated.

---

## Removing Nodes

Delete one of the nodes we created.

---

Make sure it's gone.

---

Confirm the blueprint still works after removal.

---


