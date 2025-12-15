# Blueprint Function and Nodes Combined Test

Tests the full workflow of creating functions with nodes and connecting them. Run sequentially.

---

## Setup

Create an actor blueprint called FunctionNodeTest in the Blueprints folder.

---

Add a Health float variable to it.

---

Open it in the editor.

---

## Creating the Function

What functions exist right now?

---

Add a function called CalculateHealth.

---

Tell me about that function.

---

Show the function list again.

---

## Adding Parameters

What parameters does it have?

---

Add float inputs for BaseHealth and Modifier.

---

Add a float output called ResultHealth.

---

Show me all parameters.

---

Actually, change Modifier to an integer.

---

Verify the change.

---

## Local Variables

What local variables are in the function?

---

Add float locals called TempResult and Multiplier.

---

Show the locals.

---

Change TempResult to integer.

---

Remove Multiplier.

---

List the locals again.

---

## Finding the Right Nodes

What multiply nodes are available? Show me the options.

---

How do I get the Health variable in a graph?

---

What about print string for debugging?

---

## Adding Nodes to the Function

What nodes are in CalculateHealth right now?

---

Add a float to int conversion node at position 200, 100.

---

Add a multiply node at 400, 100.

---

Show me the nodes in the function now.

---

Tell me about the pins on those nodes.

---

## Wiring It Up

Connect the BaseHealth parameter to the multiply A input.

---

Connect Modifier to the conversion node input.

---

Connect the conversion output to multiply B.

---

Connect the multiply result to the output.

---

Show me the connections to make sure it's right.

---

## Compiling and Testing

Compile the blueprint.

---

Check that it compiled successfully.

---

Run the function with some test values to verify.

---

