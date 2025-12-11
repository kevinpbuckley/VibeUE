# manage_blueprint Test Prompts

## Setup
Create Blueprint "BP_TestActor" with parent "Actor"
Create Blueprint "BP_TestCharacter" with parent "Character"

## Test 1: Blueprint Lifecycle
Create Blueprint named BP_TestActor
Get info about /Game/Blueprints/BP_TestActor
Get property bReplicates
Set bReplicates to true
Compile
Get info to verify

## Test 2: Reparenting
Create Blueprint BP_TestCharacter with Character parent
Get info to confirm parent
Reparent to Pawn class
Compile
Get info to confirm new parent

## Test 3: Event Graph
List custom events
Summarize event graph with max 200 nodes
Summarize with max_nodes=50

## Test 4: Properties
Set bReplicates on BP_TestActor
Set InitialLifeSpan property
Get properties to verify

## Test 5: Compilation
Compile BP_TestActor
Get info to check state
Recompile

## Cleanup
Delete /Game/Blueprints/BP_TestActor with force_delete=True and show_confirmation=False
Delete /Game/Blueprints/BP_TestCharacter with force_delete=True and show_confirmation=False
