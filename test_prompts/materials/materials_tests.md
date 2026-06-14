# Material Tests

Tests for creating and configuring materials and material instances. Run sequentially.

---

## Setup

Is there already a MaterialTest material somewhere? Search for it.

---

If not, create it

---

Open it in the editor.

---

## Creating Materials

Make a new material called TestCreate in the materials test folder.

---

Open that one too.

---

## Material Information

Tell me about the MaterialTest material.

---

## Material Properties

Show me all the properties, including the advanced ones.

---

Is two-sided rendering enabled?

---

Turn on two-sided rendering.

---

## Setting Multiple Properties

Configure the material for masked transparency - enable two-sided, set the blend mode to masked, and set the opacity mask clip to 0.33.

---

## Property Details

Tell me more about the blend mode property - what are my options?

---

## Adding Material Parameters

Add a scalar parameter node called Roughness with a default value of 0.5 to MaterialTest.

---

Add a vector parameter node called BaseColor with a default value of red to MaterialTest.

---

What parameters does this material have now?

---

## Modifying Parameter Values

Set the Roughness parameter's default value to 0.75.

---

Set the BaseColor parameter's default value to orange.

---

Set the BaseColor parameter using hex color #00FF88.

---

## Compiling

Compile the material.

---

## Saving

Save the material.

---

## Material Instances

Create an instance of MaterialTest called RedVariant.

---

What parameters are available on the RedVariant instance?

---

Set the roughness scalar parameter on the instance to 0.8.

---

Set the BaseColor vector parameter on the instance to cyan.

---

Set the BaseColor vector parameter using array format [0.5, 0.3, 0.1, 1.0].

---

Set the BaseColor vector parameter using hex #FF00FF.

---

Open the instance in the editor.

---

## Cleanup

Delete the test materials - MaterialTest, TestCreate, and the RedVariant instance. Force delete without asking.


---

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


---

# Advanced Material Recreation Tests

Tests for specialized creation, batch operations, export, and material recreation capabilities.
Run sequentially — each test depends on previous state.

---

## Setup

Create a test material called M_AdvancedTest in /Game/Tests/Materials/

---

## Function Call Nodes

Add a BlendAngleCorrectedNormals function call node at -600, 0. The function path is /Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals.

---

List the pins on the function call node we just created.

---

## Custom HLSL Expression

Create a custom HLSL expression node with the code `return sin(Time * Speed);`, description "SineWave", output type Float1, and inputs "Time,Speed". Place it at -500, 200.

---

List the pins on the custom expression node.

---

## Collection Parameter

Create a material parameter collection at /Game/Tests/Materials/MPC_TestParams with a scalar parameter "TestFloat" set to 0.5.

---

Now create a collection parameter node in M_AdvancedTest that references MPC_TestParams and parameter "TestFloat". Place at -500, 400.

---

## New Parameter Types

### TextureObject Parameter

Add a TextureObject parameter called "DetailMap" in group "Textures" at position -700, 0.

---

### StaticSwitch Parameter

Add a StaticSwitch parameter called "UseDetail" in group "Features" with default value true at position -700, 200.

---

## Batch Operations

### Batch Create

Create 4 nodes in a single batch call: Multiply at (-300,0), Add at (-300,200), Lerp at (-100,100), OneMinus at (-500,0).

---

Verify all 4 batch-created nodes exist by listing all expressions.

---

### Batch Set Properties

Set properties on the batch-created nodes: set the OneMinus node name to "Invert".

---

### Batch Connect

Connect the OneMinus output to the Multiply A input, and the Add output to the Multiply B input, all in one batch call.

---

Verify the connections are correct.

---

## Export Material Graph

Export M_AdvancedTest as JSON and show me the summary — how many expressions, connections, and what types are present.

---

## Landscape Material Enhancements

Create a test landscape material called M_LandscapeAdvTest in /Game/Tests/Materials/

---

### Layer Sample Node

Create a layer sample node for layer "Grass" at -600, 0.

---

### Grass Output Node

Create a grass output node with a single grass type mapping: "TestGrass" pointing to some test path. Place at -200, 400. (This may warn about missing asset — that's expected for testing.)

---

## Material Recreation Workflow

### Step 1: Export Source

Export M_AdvancedTest to JSON.

---

### Step 2: Analyze

Parse the exported JSON and tell me: how many of each expression type exist, how many connections, and what material settings are used.

---

### Step 3: Recreate

Create a new material M_AdvancedTest_Copy in /Game/Tests/Materials/ and recreate the graph from the export — use batch operations where possible. Set the same material properties (blend mode, shading model).

---

### Step 4: Verify

Export M_AdvancedTest_Copy and compare it to the original export. Report any differences in expression count, connection count, or missing types.

---

## Cleanup

Delete all test materials we created in /Game/Tests/Materials/.

---

