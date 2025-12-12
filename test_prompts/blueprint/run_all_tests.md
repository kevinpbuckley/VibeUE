# Blueprint Tools - Complete Test Suite

This document runs through all blueprint tool test prompts to verify functionality.

## Test 1: manage_blueprint

### Setup
Create Blueprint "BP_TestActor" with parent "Actor"
Create Blueprint "BP_TestCharacter" with parent "Character"

### Lifecycle
Create Blueprint named BP_TestActor
Get info about /Game/Blueprints/BP_TestActor
Get property bReplicates
Set bReplicates to true
Compile
Get info to verify

### Reparenting
Create Blueprint BP_TestCharacter with Character parent
Get info to confirm parent
Reparent to Pawn class
Compile
Get info to confirm new parent

### Event Graph
List custom events
Summarize event graph with max 200 nodes
Summarize with max_nodes=50

### Properties
Set bReplicates on BP_TestActor
Set InitialLifeSpan property
Get properties to verify

### Compilation
Compile BP_TestActor
Get info to check state
Recompile

---

## Test 2: manage_blueprint_component

### Setup
Create Blueprint "BP_ComponentTest" with parent "Actor"

### Discovery
Search for component types containing "SpotLight"
Get info about SpotLightComponent
Get metadata for Intensity property

### Creation
Create BP_LightTest with Actor parent
List components
Create SpotLightComponent "MainLight"
Create SpotLightComponent "FillLight"
List to verify

### Properties
Get Intensity from MainLight
Set Intensity to 5000.0
Set LightColor
Get all properties
Set cone angles

### Hierarchy
Create SceneComponent "LightRig"
Reparent MainLight to LightRig
Reparent FillLight to LightRig
Reorder components
List to verify

### Comparison
Create BP_LightTest2
Set different Intensity values
Compare properties between blueprints

### Deletion
Delete FillLight
List to verify
Delete LightRig with remove_children=true
Verify cascade delete

### Component Types
Create AudioComponent "SoundEffect"
Create ParticleSystemComponent "VFX"
Create NiagaraComponent "ModernVFX"
List all

---

## Test 3: manage_blueprint_variable

### Setup
Create Blueprint "BP_VariableTest" with parent "Actor"

### Type Discovery
Search for "UserWidget" types
Note type_path
Create BP_VariableTest
Create variable "AttributeWidget" using type_path
List to confirm

### Primitive Types
Create "Health" type_path "/Script/CoreUObject.FloatProperty"
Create "MaxHealth" type_path "/Script/CoreUObject.IntProperty"
Create "IsAlive" type_path "/Script/CoreUObject.BoolProperty"
Create "PlayerName" type_path "/Script/CoreUObject.StrProperty"
List all

### Complex Types
Search for NiagaraSystem
Create "DeathEffect" with NiagaraSystem type
Search for SoundCue
Create "JumpSound" with SoundCue type
List and get info

### Metadata
Create "Health" with category "Combat", tooltip, is_editable=true
Get info
Update category to "Stats|Combat"
Set is_blueprint_readonly to true
Verify

### Default Values
Set Health default to 100.0
Set IsAlive to true
Get values
Set PlayerName to "TestPlayer"

### Listing
List all variables
List by category="Combat"
List with name_contains="Health"
List with include_metadata=true

### Deletion
Delete JumpSound
List to verify
Delete Health, MaxHealth, IsAlive
List to confirm

---

## Test 4: manage_blueprint_function

### Setup
Create Blueprint "BP_FunctionTest" with parent "Actor"

### Lifecycle
Create BP_FunctionTest
List functions
Create function "CalculateHealth"
Get details
List again

### Parameters
List parameters
Add input "BaseHealth" type float
Add input "Modifier" type float
Add output "ResultHealth" type float direction "out"
List all
Update BaseHealth type to int
Remove Modifier
List to confirm

### Local Variables
List locals
Add local "TempResult" type float
Add local "Multiplier" type float
List
Update TempResult to int
Remove Multiplier
List to verify

### Properties
Update BlueprintPure = true
Set Category = "Combat|Health"
Enable CallInEditor = true
Get info to verify

### Parameter Types
Create function "TypeTest"
Add params: int, float, bool, string, name
Add param "object:AActor"
Add param "struct:FVector"
List parameters

### Deletion
Delete CalculateHealth
List to verify

---

## Test 5: manage_blueprint_node

### Setup
Create Blueprint "BP_NodeTest" with parent "Actor"
Open in editor
Create function "GetRandomNumber" with params Low/High (input int), Result (output int)
Create variable "TestValue" type float

### Complete Function
Discover RandomIntegerInRange
Create function with parameters
Create Random node at [400, 100]
Connect parameters
Compile

### Discovery
Discover "GetPlayerController"
Examine spawner keys
Create function "TestNodes"
Create node using spawner_key
Verify

### Positioning
Create function "TestNodeLayout"
Create GetPlayerController at [300, 100]
Create GetHUD at [600, 100]
Create Print String at [900, 100]
List

### Connections
Describe nodes for pins
Connect execution flow
Connect GetPlayerController to GetHUD
Connect GetHUD to PrintString
Verify

### Struct Pins
Create Make Vector
Split Vector pin
Verify X, Y, Z
Recombine

### Properties
List properties
Get property value
Set property value
Verify

### Deletion
Delete node
Verify removal
Confirm Blueprint valid

---

## Test 6: manage_blueprint_function_and_nodes (Combined)

### Setup
Create Blueprint "BP_FunctionNodeTest" with parent "Actor"
Create variable "Health" type float
Open in editor

### Function
List functions
Create "CalculateHealth"
Get details
List again

### Parameters
List parameters
Add inputs "BaseHealth" and "Modifier" type float
Add output "ResultHealth" type float direction "out"
List
Update Modifier to int
List

### Locals
List locals
Add "TempResult" and "Multiplier" type float
List
Update TempResult to int
Remove Multiplier
List

### Node Discovery
Discover "Multiply" with return_descriptors=true
Discover "GET Health"
Discover "Print String"

### Node Creation
List nodes in CalculateHealth
Create Convert Float to Int at [200, 100]
Create Multiply at [400, 100]
List
Describe for pins

### Connections
Get pin details
Connect BaseHealth to Multiply A
Connect Modifier to Convert input
Connect Convert output to Multiply B
Connect result to output
Describe to verify

### Compile
Compile BP_FunctionNodeTest
Get info
Call function with test values
Verify result

---

## Cleanup

Delete all test blueprints:
- /Game/Blueprints/BP_TestActor
- /Game/Blueprints/BP_TestCharacter
- /Game/Blueprints/BP_ComponentTest
- /Game/Blueprints/BP_LightTest
- /Game/Blueprints/BP_LightTest2
- /Game/Blueprints/BP_VariableTest
- /Game/Blueprints/BP_FunctionTest
- /Game/Blueprints/BP_NodeTest
- /Game/Blueprints/BP_FunctionNodeTest

Use force_delete=True and show_confirmation=False for all deletions.
