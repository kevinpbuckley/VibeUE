# Blueprint Component Tests

Tests for adding, configuring, and organizing components in blueprints. Run sequentially.

---

## Setup

Create an actor blueprint called ComponentTest in the Blueprints folder. If it already exists delete it and create a new one.

---

## Finding Component Types

What spotlight component types are available in Unreal? I want to see my options before adding one.

---

What properties does the SpotLightComponent class have? I want to know what I can configure.

---

Tell me about the Intensity property on SpotLightComponent. What are its constraints and metadata?

---

## Adding Lights

Make a new actor blueprint called LightTest. If it already exists delete it and create a new one.

---

What components does LightTest have right now?

---

Add a spotlight component to LightTest and call it MainLight.

---

Add another spotlight to LightTest called FillLight.

---

Show me what components LightTest has now.

---

## Configuring Lights

What's the current intensity on MainLight in LightTest?

---

Set MainLight's intensity to 5000.

---

Change MainLight's light color to something warm like orange.

---

Show me all the properties on MainLight in LightTest.

---

Adjust MainLight's cone angle to be narrower.

---

## Organizing Component Hierarchy

Add a scene component called LightRig to LightTest to organize the lights.

---

Put MainLight under the LightRig in LightTest.

---

Move FillLight under LightRig too.

---

Reorder the components in LightTest so LightRig is at the top.

---

Show me the LightTest hierarchy now.

---

## Comparing Components

Compare the MainLight and FillLight properties in LightTest to see the differences.

---

Create another actor blueprint called LightTest2. Add a spotlight called TestLight with intensity 8000.

---

Compare MainLight in LightTest with TestLight in LightTest2 to see cross-blueprint differences.

---

## Removing Components

Remove the FillLight from LightTest.

---

Show me what's left in LightTest.

---

Delete the LightRig from LightTest and take all its children with it.

---

List LightTest components to verify everything got deleted properly.

---

## Other Component Types

Add an audio component to ComponentTest for sound effects.

---

Add a particle system component to ComponentTest.

---

Add a Niagara particle system component to ComponentTest.

---

List all components on ComponentTest now.

---


