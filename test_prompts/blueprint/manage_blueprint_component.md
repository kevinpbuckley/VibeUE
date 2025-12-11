# manage_blueprint_component Test Prompts

## Setup
Create Blueprint "BP_ComponentTest" with parent "Actor"

## Test 1: Discovery
Search for component types containing "SpotLight"
Get info about SpotLightComponent
Get metadata for Intensity property

## Test 2: Creation
Create BP_LightTest with Actor parent
List components
Create SpotLightComponent "MainLight"
Create SpotLightComponent "FillLight"
List to verify

## Test 3: Properties
Get Intensity from MainLight
Set Intensity to 5000.0
Set LightColor
Get all properties
Set cone angles

## Test 4: Hierarchy
Create SceneComponent "LightRig"
Reparent MainLight to LightRig
Reparent FillLight to LightRig
Reorder components
List to verify

## Test 5: Comparison
Create BP_LightTest2
Set different Intensity values
Compare properties between blueprints

## Test 6: Deletion
Delete FillLight
List to verify
Delete LightRig with remove_children=true
Verify cascade delete

## Test 7: Component Types
Create AudioComponent "SoundEffect"
Create ParticleSystemComponent "VFX"
Create NiagaraComponent "ModernVFX"
List all

## Cleanup
Delete /Game/Blueprints/BP_ComponentTest with force_delete=True and show_confirmation=False
Delete /Game/Blueprints/BP_LightTest with force_delete=True and show_confirmation=False
Delete /Game/Blueprints/BP_LightTest2 with force_delete=True and show_confirmation=False

