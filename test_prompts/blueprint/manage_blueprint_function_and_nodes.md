# manage_blueprint_function + nodes Combined Test

## Setup
Create Blueprint "BP_FunctionNodeTest" with parent "Actor"
Create variable "Health" type float
Open in editor

## Test 1: Function
List functions
Create "CalculateHealth"
Get details
List again

## Test 2: Parameters
List parameters
Add inputs "BaseHealth" and "Modifier" type float
Add output "ResultHealth" type float direction "out"
List
Update Modifier to int
List

## Test 3: Locals
List locals
Add "TempResult" and "Multiplier" type float
List
Update TempResult to int
Remove Multiplier
List

## Test 4: Node Discovery
Discover "Multiply" with return_descriptors=true
Discover "GET Health"
Discover "Print String"

## Test 5: Node Creation
List nodes in CalculateHealth
Create Convert Float to Int at [200, 100]
Create Multiply at [400, 100]
List
Describe for pins

## Test 6: Connections
Get pin details
Connect BaseHealth to Multiply A
Connect Modifier to Convert input
Connect Convert output to Multiply B
Connect result to output
Describe to verify

## Test 7: Compile
Compile BP_FunctionNodeTest
Get info
Call function with test values
Verify result

## Cleanup
Delete /Game/Blueprints/BP_FunctionNodeTest with force_delete=True and show_confirmation=False
