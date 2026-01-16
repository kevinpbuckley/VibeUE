# Data Asset Management Tests - Comprehensive Stress Test

These tests should be run sequentially through the VibeUE chat interface in Unreal Engine. Each test builds on the previous ones. check to see if assets exists before creating them. Delete them silently if they already exist. DO NOT TRY TO CREATE ASSETS THAT ALREADY EXIST! Delete them then create. This is an exhaustive test of all data asset capabilities. 


---

## Part 1: Complete Discovery

Show me EVERY single data asset class available in this project. I want the full list with no filters.

---

Now filter to find anything with "Primary" in the name.

---

Filter for anything containing "Curve".

---

Filter for anything containing "Table".

---

Filter for anything containing "AI" or "Behavior".

---

Filter for anything containing "Animation".

---

Filter for anything containing "Ability".

---

Filter for anything containing "Input".

---

Get detailed class info for PrimaryDataAsset - show me the full inheritance chain and all properties.

---

## Part 2: Primary Data Assets (SKIP - Abstract Base Class)

**Note:** `PrimaryDataAsset` is an abstract base class and cannot be directly instantiated. The `search_types` method filters out abstract classes automatically. Only concrete subclasses like `PrimaryAssetLabel` or `PlayerMappableInputConfig` can be created. Skip to Part 5 to work with concrete data asset types that have actual properties (like InputAction, BlackboardData, etc.).

---

## Part 3: Curve Data Assets (SKIP - Curves are NOT DataAssets)

**Note:** UCurveFloat, UCurveVector, and UCurveLinearColor are NOT DataAsset types - they inherit from UCurveBase, not UDataAsset. The manage_data_asset tool cannot create or manage these. Skip this section.

If you want to search for curve-related data asset types that DO exist (like curve collections or curve libraries), do that instead.

---

## Part 4: Data Tables (Check if DataTable is a DataAsset)

Check if DataTable or CompositeDataTable appear in the data asset search_types results. If they do, try creating one.

**Note:** UDataTable inherits from UObject, not UDataAsset, so it likely won't appear in search results. Only attempt creation if it shows up in the type discovery.

---

## Part 5: AI & Behavior Trees

Search for all AI-related data asset types - blackboards, behavior trees, environment queries, etc.

---

Create a blackboard asset called BB_ComplexEnemy in /Game/Data/Test/AI if UBlackboardData is available.

---

Get complete info on the blackboard and list all its properties.

---

Create a behavior tree called BT_ComplexEnemy in /Game/Data/Test/AI if UBehaviorTree is available.

---

Get info on the behavior tree.

---

Search for environment query (EQS) related classes.

---

Create an EQS query asset called EQS_FindCover in /Game/Data/Test/AI if the class exists.

---

## Part 6: Animation Assets

Search for animation-related data asset types - montages, blend spaces, aim offsets, etc.

---

What animation data asset classes are available that we can create?

---

Create any animation data assets that are available in /Game/Data/Test/Animation.

---

## Part 7: Gameplay Abilities

Search for gameplay ability system related data asset types.

---

Look for GameplayEffect, GameplayAbility, or GameplayCue data asset classes.

---

Create any GAS-related data assets that are available in /Game/Data/Test/Abilities.

---

## Part 8: Input System Assets

Search for input-related data asset types.

---

Can we find InputAction or InputMappingContext as data asset types?

---

Create input-related data assets if they're available.

---

## Part 9: Physics Assets

Search for physics-related data asset types - physical materials, physics assets, etc.

---

Create a physical material called PM_Ice in /Game/Data/Test/Physics if PhysicalMaterial is available.

---

Get info and set properties like friction and restitution if they exist.

---

## Part 10: Audio Assets

Search for audio-related data asset types - sound classes, sound mixes, attenuation settings, etc.

---

Create a sound attenuation asset called SA_Gunshot in /Game/Data/Test/Audio if available.

---

Create a sound concurrency asset called SC_Footsteps in /Game/Data/Test/Audio if available.

---

Get info on these audio assets and try setting their properties.

---

## Part 11: Material & Rendering Assets

Search for material parameter collections or other rendering data assets.

---

Create a material parameter collection called MPC_GameSettings in /Game/Data/Test/Rendering if available.

---

## Part 12: Niagara & Particle Assets

Search for Niagara-related data asset types.

---

Create any Niagara data assets that are available.

---

## Part 13: Complex Property Types

### Array in Struct (NiagaraSystemCollection)

Get the NiagaraSystemCollection NSC_Effects and list its properties.

---

Read the SystemCollection property to see the nested struct with Systems array.

---

Add the NS_JumpPad Niagara system to the Systems array:
- First search for the NS_JumpPad asset to get its full path
- Then set the SystemCollection property with the system in the array

---

Verify the system was added by reading back the SystemCollection property.

---

### Deeply Nested Structs (NetworkPhysicsSettingsDataAsset)

Get PM_Ice's properties - it should have a Settings struct with deeply nested data.

---

Read the full Settings property to see all the nested structs (GeneralSettings, DefaultReplicationSettings, PredictiveInterpolationSettings, ResimulationSettings, NetworkPhysicsComponentSettings).

---

Try to modify a deeply nested value - set Settings.GeneralSettings.EventSchedulingMinDelaySeconds to 0.5.

---

Try to modify Settings.NetworkPhysicsComponentSettings.RedundantInputs to 3.

---

Verify the changes by reading back the Settings property.

---

### Blackboard Keys Array

**Important Note on Complex Property Formats:**
When setting complex properties like arrays of structs, you must use **Unreal's string format (T3D-like syntax)**, not JSON. Example:
```python
# Complex struct array format:
keys_str = '((EntryName="Key1"),(EntryName="Key2",EntryCategory="Combat"))'
unreal.DataAssetService.set_property(path, "Keys", keys_str)
```

---

Get the BB_ComplexEnemy blackboard and read its Keys array.

---

Add a new key entry to the blackboard:
- EntryName: "TargetLocation"
- EntryCategory: "Combat"  
- EntryDescription: "The location we're moving to"

---

Add another key:
- EntryName: "LastKnownEnemyPosition"
- EntryCategory: "Combat"
- EntryDescription: "Where we last saw the enemy"

---

Verify both keys were added by reading back the Keys property.

---

### InputAction Arrays (Triggers and Modifiers)

Get info on one of our InputAction assets and examine the Triggers and Modifiers array properties.

---

Try to add an entry to the Triggers array if possible.

---

### Object References in Structs

Find an asset with TSoftObjectPtr properties and try to set a soft reference.

---

Find an asset with FGameplayTag or FGameplayTagContainer and try to set tags.

---

## Part 14: Stress Test - Rapid Creation

Create 10 InputAction data assets in rapid succession with different names in /Game/Data/Test/Stress:
- IA_Stress_01 through IA_Stress_10

**Use InputAction class because it has actual editable properties we can test with.**

---

List all assets in /Game/Data/Test/Stress to verify they were created.

---

First, list properties on one of the stress test assets to see what properties are available. Then set a different property value on each of the 10 stress test assets using properties that actually exist.

**Only set properties that exist!** Use list_properties first, then only set properties from that list.

---

## Part 15: Stress Test - Bulk Properties

On one InputAction asset, set multiple properties at once using set_properties. Use properties from the list_properties result.

**Example:** set bTriggerWhenPaused, bConsumeInput, bReserveAllMappings, etc.

---

Read back the properties using get_info to verify.

---

## Part 16: Error Handling

Try to get info on an asset that doesn't exist.

---

Try to get a property that doesn't exist on an asset.

---

Try to set a property that doesn't exist.

---

Try to create an asset with an invalid class name.

---

Try to set a read-only property if one exists.

---

## Part 17: Cross-Reference Testing

Create two data assets that reference each other if object reference properties exist.

---

Set an asset to reference another asset we created.

---

Read back the reference to verify it was set correctly.

---

## Part 18: Complete Inventory

List ALL data assets we created during this test.

---

Count how many assets are in /Game/Data/Test total.

---

Get info on every single asset in /Game/Data/Test/Primary.

---

Get info on every single asset in /Game/Data/Test/Curves.

---

Get info on every single asset in /Game/Data/Test/AI.

---

Get info on every single asset in /Game/Data/Test/Physics.

---

Get info on every single asset in /Game/Data/Test/Audio.

---

Get info on every single asset in /Game/Data/Test/Stress.

---

## Part 19: Final Verification

For each asset type we successfully created, read back and display all non-default property values.

---

Save all dirty assets.

---

## Part 20: Summary Report

Give me a final summary of:
1. How many different data asset classes we discovered
2. How many assets we successfully created
3. Which property types we successfully read/wrote
4. Which operations failed and why
5. Any limitations discovered

---
