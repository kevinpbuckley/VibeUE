# manage_blueprint_function Test Prompts

## Setup
Create Blueprint "BP_FunctionTest" with parent "Actor"

## Test 1: Lifecycle
Create BP_FunctionTest
List functions
Create function "CalculateHealth"
Get details
List again

## Test 2: Parameters
List parameters
Add input "BaseHealth" type float
Add input "Modifier" type float
Add output "ResultHealth" type float direction "out"
List all
Update BaseHealth type to int
Remove Modifier
List to confirm

## Test 3: Local Variables
List locals
Add local "TempResult" type float
Add local "Multiplier" type float
List
Update TempResult to int
Remove Multiplier
List to verify

## Test 4: Properties
Update BlueprintPure = true
Set Category = "Combat|Health"
Enable CallInEditor = true
Get info to verify

## Test 5: Parameter Types
Create function "TypeTest"
Add params: int, float, bool, string, name
Add param "object:AActor"
Add param "struct:FVector"
List parameters

## Test 6: Deletion
Delete CalculateHealth
List to verify

## Cleanup
Delete /Game/Blueprints/BP_FunctionTest with force_delete=True and show_confirmation=False

