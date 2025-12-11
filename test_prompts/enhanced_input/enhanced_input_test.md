# Enhanced Input Test Prompts

## Test 1: Discovery
Discover all Enhanced Input types
Get modifier metadata for "Swizzle Input Axis Values"
Get trigger metadata for "Hold"
List available keys
List modifier types
List trigger types

## Test 2: Input Actions
List all Input Actions
Create "IA_Interact" with Digital value type
Create "IA_Movement" with Axis2D
Create "IA_Throttle" with Axis1D
Get properties of IA_Move
Configure IA_Interact bConsumeInput=true
Set description

## Test 3: Mapping Contexts
List all Mapping Contexts
Create "IMC_Combat"
Create "IMC_Vehicle" with priority 10
Get properties of IMC_Default
Update IMC_Combat description
Validate IMC_Weapons

## Test 4: Key Mappings
Get mappings in IMC_Weapons
Bind E to IA_Interact in IMC_Default
Bind left mouse to IA_Shoot in IMC_Combat
Bind gamepad right trigger to IA_Shoot
Bind Mouse2D to IA_Look
Bind Gamepad_Left2D to IA_Move
Remove mapping at index 0

## Test 5: Modifiers
Get modifiers on first mapping
Add Negate modifier
Add Swizzle modifier
Add Dead Zone modifier with 0.2
Add Scalar modifier 2.0
Remove first modifier

## Test 6: Triggers
Get triggers on IA_Shoot
Add Pressed trigger to IA_Interact
Add Released to IA_Crouch
Add Hold with 0.5s to IA_Sprint
Add Tap trigger
Remove at index 0

## Cleanup
Delete test Input Actions with force_delete=True and show_confirmation=False
Delete test Mapping Contexts with force_delete=True and show_confirmation=False
