# manage_level_actors Test Prompts

## Prerequisites
- ✅ Unreal Engine 5.7+ running
- ✅ VibeUE plugin loaded and active
- ✅ MCP server connection established
- ✅ Level loaded with actors (e.g., Lvl_FirstPerson or any test level)
- ✅ Working in test project (not production)

## Overview
Tests all actions of the `manage_level_actors` MCP tool including:
- **Discovery**: list, find, get_info, get_transform
- **Creation/Deletion**: add, remove
- **Transform Operations**: set_transform, set_location, set_rotation, set_scale
- **Property Management**: get_property, set_property, get_all_properties
- **Hierarchy/Organization**: set_folder, attach, detach, select, rename
- **Editor Operations**: focus, move_to_view, refresh_viewport

---

## Test 1: Actor Discovery and Listing

**Purpose**: Test listing and finding actors in the level

### Steps

1. **List All Actors**
   ```
   List all actors in the current level
   ```

2. **List with Class Filter**
   ```
   List all actors with class filter "*Light*" to find all light actors
   ```

3. **Find Actors by Tag**
   ```
   Find all actors with tag "Lighting" (may return empty if no tags set)
   ```

4. **List Static Mesh Actors**
   ```
   List all StaticMeshActor class actors with max_results=20
   ```

### Validation
- ✅ list returns array of actors with paths, labels, classes, locations
- ✅ class_filter correctly filters by class name wildcards
- ✅ find returns matching actors or empty array
- ✅ max_results limits output correctly

### Expected Outcomes
- Actor list includes WorldSettings, Floor, PlayerStart, lights, meshes
- Light filter returns DirectionalLight, SkyLight, PointLight actors
- StaticMeshActor filter returns floor, cubes, ramps, cylinders

---

## Test 2: Actor Creation - PointLight

**Purpose**: Test adding a new PointLight actor to the level

### Steps

1. **Add PointLight Actor**
   ```
   Add a PointLight actor at location [500, 500, 300] named "TestLight1"
   ```

2. **Verify Creation**
   ```
   Get info for TestLight1 including components and properties
   ```

3. **Add PointLight with Tags**
   ```
   Add another PointLight at [800, 500, 300] named "TestLight2" with tags ["Test", "Lighting"]
   ```

4. **List to Confirm**
   ```
   List all actors with label filter "TestLight*"
   ```

### Validation
- ✅ add returns success with new actor path and guid
- ✅ get_info shows correct location and class
- ✅ Tags are applied to actor
- ✅ Both lights appear in filtered list

### Expected Outcomes
```json
{
  "success": true,
  "actor_path": "/Game/.../TestLight1",
  "actor_label": "TestLight1",
  "class_name": "PointLight"
}
```

---

## Test 3: Actor Creation - SpotLight with Initial Transform

**Purpose**: Test adding SpotLight with rotation and scale

### Steps

1. **Add SpotLight with Full Transform**
   ```
   Add a SpotLight actor at location [0, 0, 500] with rotation [45, 0, 0] (pointing down at 45°) named "TestSpotLight"
   ```

2. **Get Transform**
   ```
   Get the complete transform of TestSpotLight
   ```

3. **Verify Rotation Applied**
   ```
   Confirm the SpotLight is rotated 45 degrees on pitch
   ```

### Validation
- ✅ SpotLight created with specified rotation
- ✅ get_transform returns world and relative transforms
- ✅ Rotation values match [45, 0, 0] for pitch/yaw/roll

### Expected Outcomes
- SpotLight points downward at 45-degree angle
- Transform includes forward/right/up vectors
- Bounds information returned

---

## Test 4: Transform Operations - Location

**Purpose**: Test moving actors via set_location

### Steps

1. **Get Current Location**
   ```
   Get transform of TestLight1 to see current position
   ```

2. **Move Actor**
   ```
   Set TestLight1 location to [1000, 1000, 400] in world space
   ```

3. **Verify New Location**
   ```
   Get transform again to confirm move
   ```

4. **Move with Sweep**
   ```
   Move TestLight1 to [1200, 1000, 400] with sweep=true to check for collisions
   ```

### Validation
- ✅ set_location moves actor to specified position
- ✅ world_space=true uses absolute coordinates
- ✅ sweep option performs collision check
- ✅ get_transform confirms new position

### Expected Outcomes
- Actor moves to exact coordinates specified
- No collision issues in open space

---

## Test 5: Transform Operations - Rotation

**Purpose**: Test rotating actors via set_rotation

### Steps

1. **Rotate SpotLight**
   ```
   Set TestSpotLight rotation to [90, 0, 0] to point straight down
   ```

2. **Verify Rotation**
   ```
   Get transform of TestSpotLight to confirm rotation
   ```

3. **Apply Yaw Rotation**
   ```
   Set TestSpotLight rotation to [90, 45, 0] to add yaw while pointing down
   ```

4. **Test Relative Rotation**
   ```
   Set rotation with world_space=false for relative rotation
   ```

### Validation
- ✅ set_rotation applies pitch/yaw/roll correctly
- ✅ [90, 0, 0] points light straight down
- ✅ Yaw rotation spins light around vertical axis
- ✅ world_space parameter controls coordinate system

### Expected Outcomes
- Rotation values match specified [Pitch, Yaw, Roll]
- Light cone direction changes visually in editor

---

## Test 6: Transform Operations - Scale

**Purpose**: Test scaling actors

### Steps

1. **Add Scalable Actor**
   ```
   Add a StaticMeshActor (cube) at [0, 0, 100] named "TestCube"
   ```

2. **Scale Uniformly**
   ```
   Set TestCube scale to [2, 2, 2] to double size
   ```

3. **Scale Non-Uniformly**
   ```
   Set TestCube scale to [1, 2, 0.5] for stretched shape
   ```

4. **Verify Scale**
   ```
   Get transform to confirm scale values
   ```

### Validation
- ✅ Uniform scaling [2,2,2] doubles all dimensions
- ✅ Non-uniform scaling applies different values per axis
- ✅ Scale persists after get_transform

### Expected Outcomes
- Cube visually changes size in editor
- Transform returns matching scale values

---

## Test 7: Full Transform Set

**Purpose**: Test set_transform with all parameters at once

### Steps

1. **Set Complete Transform**
   ```
   Set TestCube transform: location [500, 500, 200], rotation [0, 45, 0], scale [1.5, 1.5, 1.5]
   ```

2. **Verify All Values**
   ```
   Get transform and confirm all three components match
   ```

3. **Reset to Identity**
   ```
   Set transform: location [0, 0, 0], rotation [0, 0, 0], scale [1, 1, 1]
   ```

### Validation
- ✅ All three transform components set in single call
- ✅ Values match exactly what was specified
- ✅ Can reset to identity transform

### Expected Outcomes
- Single API call modifies location, rotation, and scale
- More efficient than three separate calls

---

## Test 8: Light Property Management - Intensity

**Purpose**: Test getting and setting light intensity properties

### Steps

1. **Get Light Intensity**
   ```
   Get property "Intensity" from TestLight1's LightComponent
   ```

2. **Set High Intensity**
   ```
   Set TestLight1 Intensity property to 10000.0
   ```

3. **Verify Change**
   ```
   Get property again to confirm new value
   ```

4. **Set Low Intensity**
   ```
   Set TestLight1 Intensity to 1000.0
   ```

### Validation
- ✅ get_property returns float value for Intensity
- ✅ set_property accepts string representation of float
- ✅ Value changes reflected in subsequent get_property
- ✅ Light brightness visually changes in editor

### Expected Outcomes
```json
{
  "success": true,
  "name": "Intensity",
  "value": "10000.0",
  "type": "float"
}
```

---

## Test 9: Light Property Management - Color

**Purpose**: Test setting light color (complex struct property)

### Steps

1. **Get Current Light Color**
   ```
   Get property "LightColor" from TestLight1
   ```

2. **Set Warm Color (Orange)**
   ```
   Set TestLight1 LightColor to "(R=255,G=180,B=100,A=255)"
   ```

3. **Verify Color Change**
   ```
   Get LightColor property to confirm RGB values
   ```

4. **Set Cool Color (Blue)**
   ```
   Set TestLight1 LightColor to "(R=100,G=150,B=255,A=255)"
   ```

5. **Set Red Alert Color**
   ```
   Set TestLight1 LightColor to "(R=255,G=0,B=0,A=255)"
   ```

### Validation
- ✅ LightColor property uses FLinearColor or FColor format
- ✅ Color changes visible in editor viewport
- ✅ RGB values correctly parsed and applied

### Expected Outcomes
- Light visually changes color in editor
- Property value returns color struct format

---

## Test 10: SpotLight Cone Properties

**Purpose**: Test SpotLight-specific properties

### Steps

1. **Get Inner Cone Angle**
   ```
   Get property "InnerConeAngle" from TestSpotLight
   ```

2. **Get Outer Cone Angle**
   ```
   Get property "OuterConeAngle" from TestSpotLight
   ```

3. **Set Narrow Spotlight**
   ```
   Set TestSpotLight InnerConeAngle to 10.0 and OuterConeAngle to 15.0
   ```

4. **Set Wide Spotlight**
   ```
   Set TestSpotLight InnerConeAngle to 30.0 and OuterConeAngle to 60.0
   ```

5. **Verify Angles**
   ```
   Get both properties to confirm values
   ```

### Validation
- ✅ Cone angle properties are floats in degrees
- ✅ InnerConeAngle should be <= OuterConeAngle
- ✅ Visual cone shape changes in editor

### Expected Outcomes
- Spotlight cone narrows/widens based on angles
- Property values match what was set

---

## Test 11: Light Attenuation Properties

**Purpose**: Test light falloff and range properties

### Steps

1. **Get Attenuation Radius**
   ```
   Get property "AttenuationRadius" from TestLight1
   ```

2. **Set Large Radius**
   ```
   Set TestLight1 AttenuationRadius to 2000.0
   ```

3. **Set Small Radius**
   ```
   Set TestLight1 AttenuationRadius to 500.0
   ```

4. **Toggle Cast Shadows**
   ```
   Get and set "CastShadows" property on TestLight1
   ```

### Validation
- ✅ AttenuationRadius controls light range
- ✅ CastShadows is boolean property
- ✅ Light influence area changes in editor

### Expected Outcomes
- Light radius visualization changes
- Shadows enable/disable correctly

---

## Test 12: Static Mesh Material Properties

**Purpose**: Test changing materials on StaticMeshActor

### Steps

1. **Get Actor Info**
   ```
   Get full info for a floor or cube StaticMeshActor including components
   ```

2. **List Component Properties**
   ```
   Get all properties from StaticMeshComponent
   ```

3. **Get Override Materials**
   ```
   Get property "OverrideMaterials" to see current materials
   ```

4. **Note**: Material assignment may require asset references
   ```
   Observe available material properties and their format
   ```

### Validation
- ✅ StaticMeshComponent properties accessible
- ✅ OverrideMaterials array visible
- ✅ Material slot structure understood

### Expected Outcomes
- Understanding of material property format for future changes
- Component hierarchy visible in get_info

---

## Test 12B: Static Mesh Actor Creation with Mesh Assignment

**Purpose**: Test adding a StaticMeshActor and assigning a static mesh asset

### Steps

1. **Add StaticMeshActor**
   ```
   Add a StaticMeshActor at location [600, 600, 100] named "TestMeshActor"
   ```

2. **Get Actor Info**
   ```
   Get info for TestMeshActor including components to find StaticMeshComponent
   ```

3. **List Available Meshes** (Optional)
   ```
   Search for static mesh assets containing "Cube" or "Sphere" using manage_asset
   ```

4. **Get Current Static Mesh**
   ```
   Get property "StaticMesh" from TestMeshActor's StaticMeshComponent
   ```

5. **Set Static Mesh to Cube**
   ```
   Set property "StaticMesh" on StaticMeshComponent to "/Engine/BasicShapes/Cube.Cube"
   ```

6. **Verify Mesh Assignment**
   ```
   Get property "StaticMesh" to confirm cube mesh is assigned
   ```

7. **Change to Sphere**
   ```
   Set property "StaticMesh" to "/Engine/BasicShapes/Sphere.Sphere"
   ```

8. **Set to Cylinder**
   ```
   Set property "StaticMesh" to "/Engine/BasicShapes/Cylinder.Cylinder"
   ```

9. **Set to Project Mesh** (if available)
   ```
   Set property "StaticMesh" to "/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"
   ```

### Validation
- ✅ StaticMeshActor created at specified location
- ✅ StaticMesh property accepts asset path format
- ✅ Engine basic shapes accessible: Cube, Sphere, Cylinder, Cone, Plane
- ✅ Project meshes accessible via /Game/ paths
- ✅ Mesh visually changes in editor viewport

### Expected Outcomes
```json
{
  "success": true,
  "name": "StaticMesh",
  "value": "/Engine/BasicShapes/Cube.Cube",
  "type": "TObjectPtr<UStaticMesh>"
}
```

### Common Static Mesh Paths
- `/Engine/BasicShapes/Cube.Cube`
- `/Engine/BasicShapes/Sphere.Sphere`
- `/Engine/BasicShapes/Cylinder.Cylinder`
- `/Engine/BasicShapes/Cone.Cone`
- `/Engine/BasicShapes/Plane.Plane`
- `/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube` (project-specific)

### Notes
- Asset path format: `/Path/To/Asset.AssetName`
- Engine assets use `/Engine/` prefix
- Project assets use `/Game/` prefix
- Some meshes may require materials to be visible

---

## Test 13: Collision Properties

**Purpose**: Test collision settings on actors

### Steps

1. **Get Collision Properties**
   ```
   Get property "CollisionEnabled" from a StaticMeshActor component
   ```

2. **Get Collision Preset**
   ```
   Get property "CollisionProfileName" from StaticMeshComponent
   ```

3. **Disable Collision**
   ```
   Set CollisionEnabled to "NoCollision" on TestCube
   ```

4. **Enable Query Only**
   ```
   Set CollisionEnabled to "QueryOnly" on TestCube
   ```

5. **Restore Full Collision**
   ```
   Set CollisionEnabled to "QueryAndPhysics" on TestCube
   ```

### Validation
- ✅ CollisionEnabled enum property works
- ✅ Values: NoCollision, QueryOnly, PhysicsOnly, QueryAndPhysics
- ✅ Collision behavior changes in play mode

### Expected Outcomes
- Collision properties set correctly
- Player can walk through when NoCollision

---

## Test 14: World Settings Properties

**Purpose**: Test reading and setting World Settings

### Steps

1. **Get World Settings Info**
   ```
   Get info for actor "WorldSettings1" with all properties
   ```

2. **Get Allow Tick Before Begin Play**
   ```
   Get property "bAllowTickBeforeBeginPlay" from WorldSettings1
   ```

3. **Get Kill Z**
   ```
   Get property "KillZ" from WorldSettings1
   ```

4. **Set Kill Z**
   ```
   Set WorldSettings1 KillZ to -2000.0
   ```

5. **Get Global Gravity**
   ```
   Get property "GlobalGravityZ" from WorldSettings1
   ```

### Validation
- ✅ WorldSettings actor accessible like any other actor
- ✅ World-level properties readable
- ✅ Critical properties like KillZ modifiable

### Expected Outcomes
- World Settings panel values match API returns
- Can modify world-level configuration via API

---

## Test 15: Actor Organization - Folders

**Purpose**: Test World Outliner folder organization

### Steps

1. **Set Folder Path**
   ```
   Set TestLight1 folder path to "TestActors/Lights"
   ```

2. **Set Same Folder for Related Actor**
   ```
   Set TestLight2 folder path to "TestActors/Lights"
   ```

3. **Set Different Folder**
   ```
   Set TestCube folder path to "TestActors/Geometry"
   ```

4. **Verify in Outliner**
   ```
   Get info for each actor and check folder_path property
   ```

### Validation
- ✅ folder_path creates hierarchy in World Outliner
- ✅ Multiple actors can share same folder
- ✅ Folder structure visible in editor UI

### Expected Outcomes
- Actors organized in folders in World Outliner
- folder_path property reflects current folder

---

## Test 16: Actor Attachment

**Purpose**: Test parent/child actor relationships

### Steps

1. **Create Parent Actor**
   ```
   Add a StaticMeshActor at [0, 0, 200] named "ParentActor"
   ```

2. **Create Child Actor**
   ```
   Add a PointLight at [100, 0, 200] named "ChildLight"
   ```

3. **Attach Child to Parent**
   ```
   Attach ChildLight to ParentActor
   ```

4. **Move Parent**
   ```
   Set ParentActor location to [500, 500, 200]
   ```

5. **Verify Child Moved**
   ```
   Get transform of ChildLight - should have moved with parent
   ```

6. **Detach Child**
   ```
   Detach ChildLight from ParentActor
   ```

### Validation
- ✅ attach creates parent-child relationship
- ✅ Child moves with parent
- ✅ detach breaks relationship
- ✅ Child maintains world position after detach

### Expected Outcomes
- Light follows mesh when parent moves
- After detach, light stays at current position

---

## Test 17: Actor Selection

**Purpose**: Test editor selection via API

### Steps

1. **Select Single Actor**
   ```
   Select TestLight1 actor
   ```

2. **Verify Selection**
   ```
   Get info for TestLight1 and check is_selected=true
   ```

3. **Add to Selection**
   ```
   Select TestLight2 with add_to_selection=true
   ```

4. **Select Multiple**
   ```
   Select actors ["TestLight1", "TestLight2", "TestCube"] at once
   ```

5. **Deselect All**
   ```
   Deselect all actors
   ```

6. **Verify Deselection**
   ```
   Get info for TestLight1 and check is_selected=false
   ```

### Validation
- ✅ Single selection works
- ✅ add_to_selection extends selection
- ✅ Multi-select array works
- ✅ deselect_all clears selection

### Expected Outcomes
- Selection visible in editor viewport and outliner
- API accurately reports selection state

---

## Test 18: Actor Renaming

**Purpose**: Test renaming actor labels

### Steps

1. **Rename Actor**
   ```
   Rename TestLight1 to "MainSceneLight"
   ```

2. **Verify New Name**
   ```
   Get info using new label "MainSceneLight"
   ```

3. **Verify Old Name Fails**
   ```
   Try to get info for "TestLight1" - should fail or return not found
   ```

4. **Rename Back**
   ```
   Rename MainSceneLight back to TestLight1
   ```

### Validation
- ✅ rename changes actor label
- ✅ New label usable for identification
- ✅ Old label no longer valid

### Expected Outcomes
- Actor appears with new name in World Outliner
- All subsequent operations use new name

---

## Test 19: Editor View Operations

**Purpose**: Test focus and move_to_view

### Steps

1. **Focus on Actor**
   ```
   Focus viewport camera on TestSpotLight
   ```

2. **Focus with Instant**
   ```
   Focus on TestLight1 with instant=true (no smooth transition)
   ```

3. **Move Actor to View**
   ```
   Move TestCube to the current viewport center using move_to_view
   ```

4. **Refresh Viewport**
   ```
   Refresh all level editing viewports
   ```

### Validation
- ✅ focus moves camera to actor
- ✅ instant parameter controls animation
- ✅ move_to_view places actor at camera location
- ✅ refresh_viewport updates display

### Expected Outcomes
- Camera animates/snaps to actor position
- Actor appears where camera is looking

---

## Test 20: Actor Deletion

**Purpose**: Test removing actors with undo support

### Steps

1. **List Test Actors**
   ```
   Find all actors with label filter "Test*"
   ```

2. **Delete Single Actor**
   ```
   Remove TestLight2 from level with with_undo=true
   ```

3. **Verify Deletion**
   ```
   Try to get info for TestLight2 - should fail
   ```

4. **Undo in Editor**
   ```
   Use Ctrl+Z in Unreal Editor to undo deletion
   ```

5. **Verify Undo**
   ```
   Get info for TestLight2 - should exist again
   ```

6. **Delete Without Undo**
   ```
   Remove TestLight2 with with_undo=false (permanent)
   ```

### Validation
- ✅ remove deletes actor from level
- ✅ with_undo=true allows Ctrl+Z recovery
- ✅ with_undo=false is permanent
- ✅ Deleted actors not found in subsequent queries

### Expected Outcomes
- Actor disappears from level
- Undo restores actor with all properties

---

## Test 21: Complex Workflow - Lighting Rig

**Purpose**: Test realistic workflow creating a lighting setup

### Steps

1. **Create Key Light**
   ```
   Add SpotLight at [500, 0, 400] with rotation [45, 0, 0] named "KeyLight"
   Set KeyLight Intensity to 8000
   Set KeyLight LightColor to "(R=255,G=245,B=230,A=255)" (warm white)
   Set KeyLight OuterConeAngle to 50.0
   ```

2. **Create Fill Light**
   ```
   Add SpotLight at [-300, 300, 300] with rotation [30, -135, 0] named "FillLight"
   Set FillLight Intensity to 3000
   Set FillLight LightColor to "(R=200,G=220,B=255,A=255)" (cool blue)
   ```

3. **Create Rim Light**
   ```
   Add SpotLight at [-200, -400, 500] with rotation [60, 120, 0] named "RimLight"
   Set RimLight Intensity to 5000
   Set RimLight LightColor to "(R=255,G=255,B=255,A=255)" (pure white)
   ```

4. **Organize in Folder**
   ```
   Set folder "Lighting/ThreePointRig" for KeyLight, FillLight, RimLight
   ```

5. **Verify Setup**
   ```
   List all actors in folder or with class filter "*SpotLight*"
   ```

### Validation
- ✅ Three-point lighting rig created
- ✅ Each light has distinct color and intensity
- ✅ All organized in same folder
- ✅ Rotations point lights at scene center

### Expected Outcomes
- Professional lighting setup visible in viewport
- Easy to select/modify entire rig via folder

---

## Test 22: Complex Property - Physics Settings

**Purpose**: Test physics-related properties on actors

### Steps

1. **Get Physics Properties**
   ```
   Get all properties from a StaticMeshActor component with category filter "Physics"
   ```

2. **Check Simulate Physics**
   ```
   Get property "bSimulatePhysics" from StaticMeshComponent
   ```

3. **Enable Physics**
   ```
   Set bSimulatePhysics to "True" on TestCube
   ```

4. **Set Mass**
   ```
   Set "MassInKg" property if available
   ```

5. **Check in Play Mode**
   ```
   Note: Run PIE to verify cube falls with gravity
   ```

### Validation
- ✅ Physics properties accessible on mesh components
- ✅ bSimulatePhysics toggles physics simulation
- ✅ Changes affect runtime behavior

### Expected Outcomes
- Cube falls when play mode starts (if SimulatePhysics=true)
- Physics properties properly configured

---

## Test 23: Get All Properties with Filters

**Purpose**: Test comprehensive property retrieval

### Steps

1. **Get All Actor Properties**
   ```
   Get all properties from TestLight1 actor
   ```

2. **Filter by Category**
   ```
   Get all properties from TestLight1 with category_filter="Rendering"
   ```

3. **Get Component Properties**
   ```
   Get all properties from TestLight1's LightComponent
   ```

4. **Include Inherited**
   ```
   Get all properties with include_inherited=true
   ```

### Validation
- ✅ get_all_properties returns comprehensive list
- ✅ category_filter reduces results to relevant properties
- ✅ component_name targets specific component
- ✅ include_inherited shows base class properties

### Expected Outcomes
- Full property dump useful for exploration
- Filtered results easier to work with

---

## Test 24: Error Handling

**Purpose**: Test API behavior with invalid inputs

### Steps

1. **Non-Existent Actor**
   ```
   Get info for actor "ThisActorDoesNotExist"
   ```

2. **Invalid Property**
   ```
   Get property "FakeProperty123" from TestLight1
   ```

3. **Invalid Class for Add**
   ```
   Try to add actor with class "NotARealClass"
   ```

4. **Invalid Location Format**
   ```
   Test error handling for malformed coordinates
   ```

### Validation
- ✅ API returns clear error messages
- ✅ success=false for failed operations
- ✅ Errors don't crash or corrupt level

### Expected Outcomes
- Graceful error handling
- Descriptive error messages for debugging

---

## Cleanup

After completing all tests, clean up test actors:

```
Remove all actors with label filter "Test*"
Remove actors: KeyLight, FillLight, RimLight, ParentActor, ChildLight
```

Or keep them for future reference and manual inspection.

---

## Summary Checklist

### Discovery Actions
- [ ] list - List actors with optional filters
- [ ] find - Find actors by criteria
- [ ] get_info - Get comprehensive actor information
- [ ] get_transform - Get complete transform data

### Creation/Deletion Actions
- [ ] add - Add new actor to level
- [ ] remove - Remove actor with undo support

### Transform Actions
- [ ] set_transform - Set full transform
- [ ] set_location - Set position only
- [ ] set_rotation - Set rotation only
- [ ] set_scale - Set scale only

### Property Actions
- [ ] get_property - Get single property value
- [ ] set_property - Set single property value
- [ ] get_all_properties - Get all properties with filters

### Organization Actions
- [ ] set_folder - Set World Outliner folder
- [ ] attach - Attach to parent actor
- [ ] detach - Detach from parent
- [ ] select - Select/deselect actors
- [ ] rename - Rename actor label

### Editor Actions
- [ ] focus - Focus viewport on actor
- [ ] move_to_view - Move actor to viewport center
- [ ] refresh_viewport - Refresh viewports

---

## Notes

- All tests should be run in sequence for proper setup
- Some tests depend on actors created in previous tests
- Property names are case-sensitive
- Location/rotation/scale arrays use [X, Y, Z] / [Pitch, Yaw, Roll] format
- Color values use "(R=x,G=y,B=z,A=w)" string format
- Always verify changes visually in Unreal Editor viewport
