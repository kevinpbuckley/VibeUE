# Blueprint Function and Nodes Combined Test

Tests the full workflow of creating functions with nodes and connecting them. Run sequentially.

---

## Setup

Create an actor blueprint called FunctionNodeTest in the Blueprints folder. If it already exists delete it and create a new one.

---

Add a Health float variable to FunctionNodeTest.

---

Open FunctionNodeTest in the editor.

---

## Creating the Function

What functions exist in FunctionNodeTest right now?

---

Add a function called CalculateHealth to FunctionNodeTest.

---

Tell me about the CalculateHealth function in FunctionNodeTest.

---

Show the function list for FunctionNodeTest again.

---

## Adding Parameters

What parameters does CalculateHealth have?

---

Add float inputs called BaseHealth and Modifier to the CalculateHealth function.

---

Add a float output called ResultHealth to CalculateHealth.

---

Show me all parameters on CalculateHealth.

---

Change the Modifier parameter in CalculateHealth to an integer type instead of float.

---

Show me the CalculateHealth parameters again to verify the change.

---

## Local Variables

What local variables are in the CalculateHealth function?

---

Add float local variables called TempResult and Multiplier to CalculateHealth.

---

Show the local variables in CalculateHealth.

---

Change TempResult to an integer type in CalculateHealth.

---

Remove the Multiplier local variable from CalculateHealth.

---

List the local variables in CalculateHealth again to verify.

---

## Finding the Right Nodes

What multiply nodes are available in Unreal? Show me the options.

---

How do I get the Health variable from FunctionNodeTest into a graph?

---

What print string nodes are available for debugging?

---

## Adding Nodes to the Function

What nodes are in the CalculateHealth function graph right now?

---

Add a Truncate node (FTrunc - converts float to integer) to CalculateHealth at position 200, 100.

---

Add a multiply node to CalculateHealth at position 400, 100.

---

Show me the nodes in CalculateHealth now.

---

Tell me about the pins on the nodes in CalculateHealth.

---

## Wiring It Up

Connect the BaseHealth parameter to the multiply node's input in CalculateHealth.

---

Connect the Modifier parameter to the conversion node input in CalculateHealth.

---

Connect the conversion output to the multiply node's B input in CalculateHealth.

---

Connect the multiply result to the ResultHealth output in CalculateHealth.

---

Show me the connections in CalculateHealth to make sure it's wired correctly.

---

## Compiling and Testing

Compile the FunctionNodeTest blueprint.

---

Check that FunctionNodeTest compiled successfully.

---

Run the CalculateHealth function with some test values to verify it works.

---

