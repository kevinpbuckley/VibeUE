# Material Tests

Tests for creating and configuring materials and material instances. Run sequentially.

---

## Setup

Is there already a MaterialTest material somewhere? Search for it.

---

If not, create one in /Game/Materials/test.

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
