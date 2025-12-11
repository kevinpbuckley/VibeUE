# manage_material_node Test Prompts

## Setup
Search for "M_NodeTest"
If not found, create at /Game/Materials/test
Open in editor

## Test 1: Discover Types
Discover with search_term="Add"

## Test 2: Discover by Category
Discover category="Math"

## Test 3: Get Categories
Get all categories

## Test 4: Create Constant
Create Constant at [-400, 0]

## Test 5: Create Vector Constant
Create Constant3Vector at [-400, 100]

## Test 6: Create Add
Create Add at [-200, 0]

## Test 7: Create Multiply
Create Multiply at [-200, 200]

## Test 8: List
List all expressions

## Test 9: Get Details
Get details for expression

## Test 10: Get Pins
Get pins for Add

## Test 11: Move
Move Constant to [-500, 50]

## Test 12: List Properties
List properties of Constant

## Test 13: Get Property
Get "R" value

## Test 14: Set Property
Set "R" to 0.5

## Test 15: Connect
Connect Constant to Add A
Connect Vector to Multiply
Verify

## Test 16: Create Parameter
Create scalar parameter "Roughness"
Set default
Promote to parameter

## Test 17: Connect to Output
Connect to Base Color
Connect to Roughness
Compile

## Cleanup
Delete /Game/Materials/test/M_NodeTest with force_delete=True and show_confirmation=False
