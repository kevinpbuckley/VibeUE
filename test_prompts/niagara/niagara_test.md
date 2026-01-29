# Niagara System Test

Natural language prompts to test all NiagaraService and NiagaraEmitterService methods. Run sequentially.

---

## Setup & Search

Search for any existing niagara systems with "SystemTest" in the name and delete them if found.

---

Search for existing niagara systems in /Game to see what's available to clone.

---

## Clone and Modify Existing System

Find a niagara system with "Fire" in the name to use as a base. Clone it to /Game/VFX/NS_SystemTest.

---

Open NS_SystemTest in the niagara editor.

---

List all emitters in the cloned system.

---

Get system properties for NS_SystemTest.

---

Give me an AI-friendly summary of NS_SystemTest.

---

## Build Emitter from Scratch

Add a minimal emitter called "TestSparks" to NS_SystemTest.

---

Add a sprite renderer to TestSparks.

---

Add the InitializeParticle module to TestSparks in the ParticleSpawn stage.

---

Add the SpawnRate module to TestSparks in the EmitterUpdate stage.

---

Add the ParticleState module to TestSparks in the ParticleUpdate stage.

---

Add the SolveForcesAndVelocity module to TestSparks in the ParticleUpdate stage.

---

Set the spawn rate on TestSparks to 50 (use rapid iteration params).

---

Compile NS_SystemTest with results and show me if there are any errors. If there are errors, fix them and recompile.

---

List all modules in TestSparks to verify everything was added.

---

List all emitters in NS_SystemTest now.

---

Duplicate the TestSparks emitter and call the copy "TestSparks2".

---

Rename TestSparks2 to "TestFlames".

---

Compile with results to verify the duplicate worked correctly. Fix any errors if needed.

---

Disable the TestFlames emitter.

---

Enable the TestFlames emitter again.

---

Check if an emitter named "TestSparks" exists in NS_SystemTest.

---

Move the TestFlames emitter to index 0.

---

Get the graph position of the TestSparks emitter.

---

Set the graph position of TestFlames to x=500, y=200.

---

## User Parameters

List all user parameters on NS_SystemTest.

---

Add a float user parameter called "SpawnMultiplier" with default value 1.0.

---

Set SpawnMultiplier to 2.5.

---

Read back the SpawnMultiplier value.

---

Check if parameter SpawnMultiplier exists.

---

Remove the SpawnMultiplier parameter.

---

## Rapid Iteration Parameters

List all rapid iteration parameters on the TestSparks emitter.

---

Set the spawn rate on TestSparks to 100 (look for SpawnRate in the params).

---

Set the color scale on TestSparks to bright green (0, 3, 0) in all stages.

---

## Compilation Testing

Compile NS_SystemTest with results. Show any errors or warnings. If errors exist, diagnose and fix them, then recompile.

---

## Module Management

Search for module scripts with "Color" in the name.

---

Add a ScaleColor module to TestSparks in the ParticleUpdate stage.

---

Compile with results after adding the module. Fix any errors if needed.

---

List all modules in TestSparks to verify the color module was added.

---

List built-in spawn modules.

---

## Renderer Management

List all renderers on the TestSparks emitter.

---

Add a ribbon renderer to the TestSparks emitter.

---

Compile with results to verify the renderer was added correctly. Fix any errors.

---

List renderers again to verify the ribbon was added.

---

Disable the ribbon renderer (last index).

---

Remove the ribbon renderer.

---

Compile with results after removing renderer. Verify no errors.

---

## Emitter Properties

Get the emitter properties for TestSparks.

---

Get the lifecycle info for TestSparks.

---

## Copy Operations

Copy the TestSparks emitter to a new emitter called "TestSparksCopy" in the same system.

---

Compile with results to verify the copy operation worked. Fix any errors.

---

## System Properties & Comparison

Get the system properties for NS_SystemTest.

---

Compare NS_SystemTest with the original system we cloned from. Show the differences.

---

## Final Compilation Test

Make one final compilation with results to ensure everything is working.

---

Save NS_SystemTest.

---

Search for niagara systems with "Test" in the name to verify NS_SystemTest is saved.

---

## Complete

All tests complete! NS_SystemTest should have TestSparks emitter with sprite renderer and all required modules, fully compiled and working.

---

Remove the TestFlames emitter.

---

Save NS_SmokeTest.

---

Search for niagara systems with "Test" in the name to verify NS_SmokeTest is saved.

---

## Complete

All tests complete! NS_SmokeTest should have one emitter (Sparks) remaining.
