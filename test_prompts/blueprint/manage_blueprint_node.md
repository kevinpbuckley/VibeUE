# manage_blueprint_node Test Prompts

## Setup
Create Blueprint "BP_NodeTest" with parent "Actor"
Open in editor
Create function "GetRandomNumber" with params Low/High (input int), Result (output int)
Create variable "TestValue" type float

## Test 1: Complete Function
Discover RandomIntegerInRange
Create function with parameters
Create Random node at [400, 100]
Connect parameters
Compile

## Test 2: Discovery
Discover "GetPlayerController"
Examine spawner keys
Create function "TestNodes"
Create node using spawner_key
Verify

## Test 3: Positioning
Create function "TestNodeLayout"
Create GetPlayerController at [300, 100]
Create GetHUD at [600, 100]
Create Print String at [900, 100]
List

## Test 4: Connections
Describe nodes for pins
Connect execution flow
Connect GetPlayerController to GetHUD
Connect GetHUD to PrintString
Verify

## Test 5: Struct Pins
Create Make Vector
Split Vector pin
Verify X, Y, Z
Recombine

## Test 6: Properties
List properties
Get property value
Set property value
Verify

## Test 7: Deletion
Delete node
Verify removal
Confirm Blueprint valid

## Cleanup
Delete /Game/Blueprints/BP_NodeTest with force_delete=True and show_confirmation=False

