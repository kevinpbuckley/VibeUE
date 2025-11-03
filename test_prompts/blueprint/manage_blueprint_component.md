# manage_blueprint_component - Test Prompts

## Purpose
Comprehensive test prompts for the `manage_blueprint_component` tool covering all 12 actions. This test suite validates component lifecycle management, property manipulation, and hierarchy operations.

## Prerequisites
- ✅ Unreal Engine running with VibeUE project loaded
- ✅ VibeUE MCP server connected
- ✅ Blueprint `BP_Player` exists at `/Game/Blueprints/Characters/BP_Player` (or will be created)
- ✅ Test Blueprint `BP_ComponentTest` will be created for testing

## Test Workflow Overview
This test suite follows a logical workflow:
1. **Discovery** - Search for available component types
2. **Inspection** - Get detailed component information
3. **Creation** - Create Blueprint and add components
4. **Property Management** - Read, write, and compare properties
5. **Hierarchy Operations** - Reorder and reparent components
6. **Cleanup** - Delete test components

---

## Test 1: search_types - Discover SpotLight Component Types

### Prompt
```
I need to find all SpotLight component types available in Unreal Engine. Use the manage_blueprint_component tool with action "search_types" to search for components with "SpotLight" in the name.
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="",
    action="search_types",
    search_text="SpotLight"
)
```

### Expected Outcome
- Returns list of SpotLight-related components
- Should include `SpotLightComponent`
- Response includes component metadata (category, base class, etc.)

### Validation
- ✅ Response contains `SpotLightComponent`
- ✅ Component has category information
- ✅ Component has base class information

---

## Test 2: search_types - Discover Lighting Components by Category

### Prompt
```
Show me all available lighting components. Use manage_blueprint_component with action "search_types" and filter by category "Lighting".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="",
    action="search_types",
    category="Lighting"
)
```

### Expected Outcome
- Returns list of all lighting components
- Should include: DirectionalLightComponent, PointLightComponent, SpotLightComponent, RectLightComponent, SkyLightComponent
- Each component has category and base class info

### Validation
- ✅ Response contains multiple light component types (at least 4)
- ✅ All components have category="Lighting"

---

## Test 3: get_info - Get SpotLightComponent Information

### Prompt
```
I need detailed information about the SpotLightComponent class. Use manage_blueprint_component with action "get_info" for component type "SpotLightComponent".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="",
    action="get_info",
    component_type="SpotLightComponent"
)
```

### Expected Outcome
- Returns comprehensive metadata about SpotLightComponent
- Includes all properties with types and default values
- Includes specific SpotLight properties: `InnerConeAngle`, `OuterConeAngle`
- Includes inherited light properties: `Intensity`, `LightColor`, `AttenuationRadius`

### Validation
- ✅ Response contains property list
- ✅ Properties include `InnerConeAngle` and `OuterConeAngle` (SpotLight-specific)
- ✅ Properties include `Intensity` and `LightColor` (inherited from LightComponent)

---

## Test 4: get_property_metadata - Get Property Details

### Prompt
```
I need to know the metadata for the "Intensity" property of SpotLightComponent. Use manage_blueprint_component with action "get_property_metadata".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="",
    action="get_property_metadata",
    component_type="SpotLightComponent",
    property_name="Intensity"
)
```

### Expected Outcome
- Returns detailed metadata for the Intensity property
- Includes property type (float)
- May include min/max values, UI metadata, tooltip

### Validation
- ✅ Response contains property type
- ✅ Property type is numeric (float or double)

---

## Test 5: create - Create Test Blueprint with SpotLight

### Prompt
```
Create a new Blueprint called "BP_ComponentTest" with a SpotLight component. First, use manage_blueprint to create the Blueprint at path "/Game/Blueprints/BP_ComponentTest". Then use manage_blueprint_component with action "create" to add a SpotLightComponent named "TestSpotLight".
```

### Expected Commands
```python
# First create the Blueprint
manage_blueprint(
    action="create",
    blueprint_name="BP_ComponentTest",
    parent_class="Actor",
    save_path="/Game/Blueprints/"
)

# Then add SpotLight component
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="create",
    component_type="SpotLightComponent",
    component_name="TestSpotLight",
    parent_name="DefaultSceneRoot"
)
```

### Expected Outcome
- Blueprint created successfully at `/Game/Blueprints/BP_ComponentTest`
- SpotLightComponent added to Blueprint with name "TestSpotLight"
- Component attached to root component

### Validation
- ✅ Blueprint creation succeeds
- ✅ Component creation succeeds
- ✅ Response includes component_name and component_type

---

## Test 6: list - List All Components in Blueprint

### Prompt
```
List all components in the BP_ComponentTest Blueprint to verify the SpotLight was created. Use manage_blueprint_component with action "list".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="list"
)
```

### Expected Outcome
- Returns component hierarchy
- Shows DefaultSceneRoot (inherited root component)
- Shows TestSpotLight as child of root
- Includes component types and hierarchy relationships

### Validation
- ✅ Response contains at least 2 components
- ✅ TestSpotLight component is present
- ✅ Hierarchy shows parent-child relationships

---

## Test 7: set_property - Set SpotLight Intensity

### Prompt
```
Set the Intensity property of the TestSpotLight component in BP_ComponentTest to 5000. Use manage_blueprint_component with action "set_property".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="set_property",
    component_name="TestSpotLight",
    property_name="Intensity",
    property_value=5000
)
```

### Expected Outcome
- Property set successfully
- Response indicates success

### Validation
- ✅ Response success=true
- ✅ No error messages

---

## Test 8: set_property - Set SpotLight Color

### Prompt
```
Set the LightColor of TestSpotLight to a purple color (R=0.8, G=0.2, B=1.0, A=1.0). Use manage_blueprint_component with action "set_property".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="set_property",
    component_name="TestSpotLight",
    property_name="LightColor",
    property_value={"R": 0.8, "G": 0.2, "B": 1.0, "A": 1.0}
)
```

### Expected Outcome
- Color property set successfully
- Component now has purple light color

### Validation
- ✅ Response success=true
- ✅ No error messages

---

## Test 9: set_property - Set Cone Angles

### Prompt
```
Set the InnerConeAngle to 15 degrees and OuterConeAngle to 35 degrees for TestSpotLight. Make two calls to manage_blueprint_component with action "set_property".
```

### Expected Commands
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="set_property",
    component_name="TestSpotLight",
    property_name="InnerConeAngle",
    property_value=15
)

manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="set_property",
    component_name="TestSpotLight",
    property_name="OuterConeAngle",
    property_value=35
)
```

### Expected Outcome
- Both cone angle properties set successfully
- Inner cone: 15 degrees
- Outer cone: 35 degrees

### Validation
- ✅ Both responses success=true
- ✅ No error messages

---

## Test 10: get_property - Verify Intensity Setting

### Prompt
```
Verify that the Intensity property of TestSpotLight is now 5000. Use manage_blueprint_component with action "get_property".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="get_property",
    component_name="TestSpotLight",
    property_name="Intensity"
)
```

### Expected Outcome
- Returns current value of Intensity property
- Value should be 5000 (or close to it as a float)

### Validation
- ✅ Response contains "value" field
- ✅ Value equals 5000 (within floating point precision)

---

## Test 11: get_all_properties - Get All SpotLight Properties

### Prompt
```
Get all properties of the TestSpotLight component including inherited properties. Use manage_blueprint_component with action "get_all_properties" and include_inherited=True.
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="get_all_properties",
    component_name="TestSpotLight",
    include_inherited=True
)
```

### Expected Outcome
- Returns all properties with current values
- Should include Intensity=5000
- Should include LightColor with purple values
- Should include InnerConeAngle=15
- Should include OuterConeAngle=35
- Includes many inherited properties from parent classes

### Validation
- ✅ Response contains "properties" dictionary
- ✅ Intensity value is 5000
- ✅ InnerConeAngle value is 15
- ✅ OuterConeAngle value is 35
- ✅ Property count > 50 (many inherited properties)

---

## Test 12: create - Add PointLight Component

### Prompt
```
Add a PointLightComponent named "TestPointLight" to BP_ComponentTest. Position it at location [100, 0, 50] and set initial properties: Intensity=3000, LightColor with cyan color (R=0, G=1, B=1, A=1). Use manage_blueprint_component with action "create".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="create",
    component_type="PointLightComponent",
    component_name="TestPointLight",
    parent_name="DefaultSceneRoot",
    location=[100, 0, 50],
    properties={
        "Intensity": 3000,
        "LightColor": {"R": 0, "G": 1, "B": 1, "A": 1}
    }
)
```

### Expected Outcome
- PointLight component created successfully
- Component positioned at [100, 0, 50]
- Initial properties set (Intensity=3000, cyan color)

### Validation
- ✅ Response success=true
- ✅ Component_name="TestPointLight"
- ✅ Component_type="PointLightComponent"

---

## Test 13: create - Add DirectionalLight Component

### Prompt
```
Add a DirectionalLightComponent named "TestDirLight" to BP_ComponentTest. Use manage_blueprint_component with action "create".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="create",
    component_type="DirectionalLightComponent",
    component_name="TestDirLight",
    parent_name="DefaultSceneRoot"
)
```

### Expected Outcome
- DirectionalLight component created successfully
- Component attached to root

### Validation
- ✅ Response success=true
- ✅ Component created

---

## Test 14: list - Verify All Components Created

### Prompt
```
List all components in BP_ComponentTest again to verify we now have 4 components (root + 3 lights). Use manage_blueprint_component with action "list".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="list"
)
```

### Expected Outcome
- Returns component hierarchy with 4 components:
  - DefaultSceneRoot
  - TestSpotLight
  - TestPointLight
  - TestDirLight

### Validation
- ✅ Response contains 4 components
- ✅ All three light components present
- ✅ All attached to root component

---

## Test 15: reorder - Change Component Order

### Prompt
```
Reorder the components in BP_ComponentTest so that TestDirLight appears first, then TestPointLight, then TestSpotLight. Use manage_blueprint_component with action "reorder".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="reorder",
    component_order=["TestDirLight", "TestPointLight", "TestSpotLight"]
)
```

### Expected Outcome
- Components reordered successfully
- New order: TestDirLight, TestPointLight, TestSpotLight
- Response shows final order

### Validation
- ✅ Response success=true
- ✅ Final order matches requested order

---

## Test 16: reparent - Move PointLight Under SpotLight

### Prompt
```
Reparent TestPointLight so it becomes a child of TestSpotLight instead of the root. Use manage_blueprint_component with action "reparent".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="reparent",
    component_name="TestPointLight",
    parent_name="TestSpotLight"
)
```

### Expected Outcome
- TestPointLight reparented successfully
- Old parent: DefaultSceneRoot
- New parent: TestSpotLight
- Hierarchy now has nested components

### Validation
- ✅ Response success=true
- ✅ old_parent shows DefaultSceneRoot or similar
- ✅ new_parent="TestSpotLight"

---

## Test 17: list - Verify Hierarchy After Reparenting

### Prompt
```
List components again to verify the new hierarchy after reparenting. Use manage_blueprint_component with action "list".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="list"
)
```

### Expected Outcome
- Component hierarchy shows:
  - DefaultSceneRoot
    - TestDirLight
    - TestSpotLight
      - TestPointLight (nested under SpotLight)

### Validation
- ✅ TestPointLight is child of TestSpotLight
- ✅ Hierarchy depth increased for TestPointLight

---

## Test 18: compare_properties - Compare Lights Between Blueprints

### Prompt
```
If BP_Player has a SpotLight component named "SpotLight_Top", compare the properties of TestSpotLight in BP_ComponentTest with SpotLight_Top in BP_Player. Use manage_blueprint_component with action "compare_properties".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="compare_properties",
    component_name="TestSpotLight",
    options={
        "compare_to_blueprint": "/Game/Blueprints/Characters/BP_Player",
        "compare_to_component": "SpotLight_Top"
    }
)
```

### Expected Outcome
- Comparison results showing matching and different properties
- Shows count of matches and differences
- Lists specific property differences with values from both components

### Validation
- ✅ Response contains "matches" field (boolean)
- ✅ Response contains "matching_count" and "difference_count"
- ✅ Response contains "differences" array with property comparisons

---

## Test 19: get_all_properties - Get Only Non-Inherited Properties

### Prompt
```
Get only the SpotLight-specific properties (non-inherited) from TestSpotLight. Use manage_blueprint_component with action "get_all_properties" and include_inherited=False.
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="get_all_properties",
    component_name="TestSpotLight",
    include_inherited=False
)
```

### Expected Outcome
- Returns only SpotLight-specific properties
- Should include InnerConeAngle and OuterConeAngle
- Property count much smaller than with inherited properties

### Validation
- ✅ Response contains fewer properties than Test 11
- ✅ Contains InnerConeAngle and OuterConeAngle
- ✅ Property count < 10 (typically just 2-3 SpotLight-specific properties)

---

## Test 20: delete - Remove PointLight Component

### Prompt
```
Delete the TestPointLight component from BP_ComponentTest. Use manage_blueprint_component with action "delete" and remove_children=True.
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="delete",
    component_name="TestPointLight",
    remove_children=True
)
```

### Expected Outcome
- TestPointLight deleted successfully
- Since it has no children, removed_children should be False or 0
- Component removed from hierarchy

### Validation
- ✅ Response success=true
- ✅ Component no longer exists in Blueprint

---

## Test 21: list - Verify Deletion

### Prompt
```
List components to verify TestPointLight was deleted. Use manage_blueprint_component with action "list".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="list"
)
```

### Expected Outcome
- Component list no longer includes TestPointLight
- Only shows DefaultSceneRoot, TestSpotLight, TestDirLight

### Validation
- ✅ TestPointLight not in component list
- ✅ Only 3 components remain

---

## Test 22: delete - Remove SpotLight with Children Warning

### Prompt
```
Try to delete TestSpotLight (which currently has no children since we deleted TestPointLight). Use manage_blueprint_component with action "delete" and remove_children=False.
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="delete",
    component_name="TestSpotLight",
    remove_children=False
)
```

### Expected Outcome
- Component deleted successfully (has no children)
- removed_children=False
- children_count=0

### Validation
- ✅ Response success=true
- ✅ Component deleted

---

## Test 23: delete - Clean Up DirectionalLight

### Prompt
```
Delete the TestDirLight component to clean up. Use manage_blueprint_component with action "delete".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="delete",
    component_name="TestDirLight",
    remove_children=True
)
```

### Expected Outcome
- Component deleted successfully
- Only DefaultSceneRoot remains

### Validation
- ✅ Response success=true
- ✅ Component removed

---

## Test 24: list - Final Verification

### Prompt
```
List components one final time to verify only the root component remains. Use manage_blueprint_component with action "list".
```

### Expected Command
```python
manage_blueprint_component(
    blueprint_name="/Game/Blueprints/BP_ComponentTest",
    action="list"
)
```

### Expected Outcome
- Only DefaultSceneRoot component remains
- All test components successfully deleted

### Validation
- ✅ Only 1 component (root) in the list
- ✅ All test components cleaned up

---

## Summary of Test Coverage

### All 12 Actions Tested ✅

1. ✅ **search_types** - Tests 1, 2 (by search text and category)
2. ✅ **get_info** - Test 3 (SpotLightComponent metadata)
3. ✅ **get_property_metadata** - Test 4 (Intensity property metadata)
4. ✅ **list** - Tests 6, 14, 17, 21, 24 (component hierarchy)
5. ✅ **create** - Tests 5, 12, 13 (SpotLight, PointLight, DirectionalLight)
6. ✅ **get_property** - Test 10 (verify Intensity)
7. ✅ **set_property** - Tests 7, 8, 9 (Intensity, Color, Angles)
8. ✅ **get_all_properties** - Tests 11, 19 (with and without inherited)
9. ✅ **compare_properties** - Test 18 (compare between Blueprints)
10. ✅ **reorder** - Test 15 (change component order)
11. ✅ **reparent** - Test 16 (move component to new parent)
12. ✅ **delete** - Tests 20, 22, 23 (remove components)

### Test Workflow Validation ✅

1. ✅ **Discovery Phase** - Tests 1-4 search and inspect available components
2. ✅ **Creation Phase** - Tests 5-6 create Blueprint and verify components
3. ✅ **Property Management** - Tests 7-11, 18-19 set, get, and compare properties
4. ✅ **Hierarchy Operations** - Tests 12-17 add components, reorder, reparent
5. ✅ **Cleanup Phase** - Tests 20-24 delete components and verify

### Component Types Tested ✅

- ✅ SpotLightComponent (primary focus with cone angles)
- ✅ PointLightComponent (with location and initial properties)
- ✅ DirectionalLightComponent (basic creation/deletion)

### Property Types Tested ✅

- ✅ Float properties (Intensity, angles)
- ✅ Color properties (LightColor with RGBA)
- ✅ Transform properties (location)
- ✅ Inherited vs non-inherited properties

### Blueprint Operations Tested ✅

- ✅ Uses existing Blueprint (BP_Player for comparison)
- ✅ Creates test Blueprint (BP_ComponentTest)
- ✅ Component hierarchy operations (parent-child relationships)
- ✅ Property manipulation and validation
- ✅ Complete cleanup of test data

---

## Notes for Test Execution

1. **Prerequisites Check**: Verify Unreal Engine is running and BP_Player exists before starting
2. **Sequential Execution**: Tests should be run in order as later tests depend on earlier setup
3. **Error Handling**: If any test fails, note the error and continue with remaining tests
4. **Blueprint Paths**: Adjust paths if BP_Player is located elsewhere
5. **Cleanup**: Tests 20-24 ensure all test components are removed
6. **Manual Verification**: After tests, can manually inspect BP_ComponentTest in Unreal Editor to verify results

---

## Success Criteria

- ✅ All 12 actions execute without errors
- ✅ Component creation, modification, and deletion work correctly
- ✅ Property values are set and retrieved accurately
- ✅ Hierarchy operations (reorder, reparent) function properly
- ✅ Comparison between Blueprints returns accurate results
- ✅ Test cleanup removes all temporary components
- ✅ Blueprint remains in valid state after all operations

---

**Test Suite Version**: 1.0  
**Last Updated**: November 3, 2025  
**Total Test Count**: 24 prompts covering 12 actions  
**Estimated Execution Time**: 15-20 minutes
