# manage_level_actors Test Prompts

## Setup
Load level with actors

## Test 1: Discovery
List all actors
List with class filter "*Light*"
Find actors with tag "Lighting"
List StaticMeshActor with max_results=20

## Test 2: Create PointLight
Add PointLight at [500, 500, 300] named "TestLight1"
Get info
Add PointLight at [800, 500, 300] named "TestLight2" with tags ["Test", "Lighting"]
List "TestLight*"

## Test 3: Create SpotLight
Add SpotLight at [0, 0, 500] rotation [45, 0, 0] named "TestSpotLight"
Get transform
Verify rotation

## Test 4: Location
Get transform of TestLight1
Set location to [1000, 1000, 400]
Verify
Move to [1200, 1000, 400] with sweep=true

## Test 5: Rotation
Set TestSpotLight rotation to [90, 0, 0]
Verify
Set to [90, 45, 0]
Test relative with world_space=false

## Test 6: Scale
Add StaticMeshActor at [0, 0, 100] named "TestCube"
Set scale to [2, 2, 2]
Set to [1, 2, 0.5]
Verify

## Test 7: Full Transform
Set TestCube: location [500, 500, 200], rotation [0, 45, 0], scale [1.5, 1.5, 1.5]
Verify
Reset to identity

## Test 8: Light Properties
Get Intensity from TestLight1
Set to 10000.0
Verify
Set LightColor
Get all properties

## Test 9: Hierarchy
Set TestLight1 folder to "TestLights"
Attach TestLight2 to TestCube
Detach TestLight2
Select TestLight1

## Test 10: Editor Operations
Focus on TestSpotLight
Move TestCube to view
Refresh viewport
Rename TestLight1 to "MainLight"

## Cleanup
Delete TestLight1 with force_delete=True and show_confirmation=False
Delete TestLight2 with force_delete=True and show_confirmation=False
Delete TestSpotLight with force_delete=True and show_confirmation=False
Delete TestCube with force_delete=True and show_confirmation=False
