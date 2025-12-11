# manage_blueprint_variable Test Prompts

## Setup
Create Blueprint "BP_VariableTest" with parent "Actor"

## Test 1: Type Discovery
Search for "UserWidget" types
Note type_path
Create BP_VariableTest
Create variable "AttributeWidget" using type_path
List to confirm

## Test 2: Primitive Types
Create "Health" type_path "/Script/CoreUObject.FloatProperty"
Create "MaxHealth" type_path "/Script/CoreUObject.IntProperty"
Create "IsAlive" type_path "/Script/CoreUObject.BoolProperty"
Create "PlayerName" type_path "/Script/CoreUObject.StrProperty"
List all

## Test 3: Complex Types
Search for NiagaraSystem
Create "DeathEffect" with NiagaraSystem type
Search for SoundCue
Create "JumpSound" with SoundCue type
List and get info

## Test 4: Metadata
Create "Health" with category "Combat", tooltip, is_editable=true
Get info
Update category to "Stats|Combat"
Set is_blueprint_readonly to true
Verify

## Test 5: Default Values
Set Health default to 100.0
Set IsAlive to true
Get values
Set PlayerName to "TestPlayer"

## Test 6: Listing
List all variables
List by category="Combat"
List with name_contains="Health"
List with include_metadata=true

## Test 7: Deletion
Delete JumpSound
List to verify
Delete Health, MaxHealth, IsAlive
List to confirm

## Cleanup
Delete /Game/Blueprints/BP_VariableTest with force_delete=True and show_confirmation=False

