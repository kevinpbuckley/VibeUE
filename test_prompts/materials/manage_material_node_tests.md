# Material Node Tests

Tests for creating and connecting nodes in material graphs. Run sequentially.

---

## Setup

Check if there's a NodeTest material in the materials test folder.

---

If not, create it.

---

Open the material editor.

---

## Finding Node Types

What nodes are available with "Add" in the name?

---

Show me all the math category nodes.

---

What categories exist for material nodes?

---

## Creating Basic Nodes

Add a constant node at position -400, 0.

---

Add a 3-component color constant at -400, 100.

---

Put an add node at -200, 0.

---

Add a multiply node at -200, 200.

---

## Managing Nodes

List all the nodes in the material graph.

---

Tell me about the add node we created.

---

What pins does the add node have?

---

Move the constant node to -500, 50.

---

## Node Properties

What properties can I set on the constant node?

---

What's the R value?

---

Set R to 0.5.

---

## Connecting Nodes

Connect the constant to the A input on the add node.

---

Connect the color vector to the multiply node.

---

Verify those connections.

---

## Creating Parameters

Add a scalar parameter called Roughness so artists can tweak it.

---

Set a reasonable default value.

---

Promote one of the constants to an editable parameter.

---

## Connecting to Material Outputs

Hook something up to the base color output.

---

Connect to the roughness output.

---

Compile the material to see if it works.

---

## Cleanup

Delete the NodeTest material. Force delete without asking.
