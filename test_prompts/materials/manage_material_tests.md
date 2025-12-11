# manage_material Test Prompts

## Setup
Search for "M_MaterialTest"
If not found, create at /Game/Materials/test
Open in editor

## Test 1: Create
Create "M_TestCreate" at /Game/Materials/test
Open

## Test 2: Get Info
Get info for M_MaterialTest

## Test 3: List Properties
List with include_advanced=true

## Test 4: Get Property
Get "TwoSided"

## Test 5: Set Property
Set "TwoSided" to true

## Test 6: Set Multiple
Set TwoSided=true, BlendMode=Masked, OpacityMaskClipValue=0.33

## Test 7: Property Info
Get info for "BlendMode"

## Test 8: List Parameters
List all parameters

## Test 9: Get Parameter
Get "Roughness"

## Test 10: Set Parameter
Set "Roughness" to 0.75

## Test 11: Compile
Compile material

## Test 12: Save
Save material

## Test 13: Create Instance
Create "MI_MaterialTest_Red" from M_MaterialTest
Set scalar and vector parameters
Open

## Cleanup
Delete /Game/Materials/test/M_MaterialTest with force_delete=True and show_confirmation=False
Delete /Game/Materials/test/M_TestCreate with force_delete=True and show_confirmation=False
Delete /Game/Materials/test/MI_MaterialTest_Red with force_delete=True and show_confirmation=False
