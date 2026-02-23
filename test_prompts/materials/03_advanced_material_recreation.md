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
