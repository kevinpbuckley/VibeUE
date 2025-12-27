# Color Testing Suite

Comprehensive tests for all color handling scenarios in VibeUE. Tests all supported color formats across different systems.

# Part 1: Setup & Asset Creation

## Create Test Blueprints

Create an actor blueprint called ColorTestActor in /Game/Tests/ColorTests with parent_class "Actor". If it exists, delete it first and create a new one.

---

Create a widget blueprint called ColorTestWidget in /Game/Tests/ColorTests with parent_class "UserWidget". Use manage_blueprint to create widget blueprints, NOT manage_umg_widget. If it exists, delete it first and create a new one.

---

## Create Test Materials

Create a material called ColorTestMaterial in /Game/Tests/ColorTests with destination_path "/Game/Tests/ColorTests". If it exists, delete it first.

---

Open the ColorTestMaterial in the editor.

---

# Part 2: Blueprint Component Color Tests

## Add Components for Color Testing

Add a SpotLightComponent called TestSpotLight to ColorTestActor.

---

Add a PointLightComponent called TestPointLight to ColorTestActor.

---

Add a DirectionalLightComponent called TestDirectionalLight to ColorTestActor.

---

## LightColor Property - Array Format (RGBA)

Set the LightColor on TestSpotLight to an RGBA array [1.0, 0.5, 0.0, 1.0] for orange.

---

Get the LightColor from TestSpotLight to verify it was set.

---

## LightColor Property - Array Format (RGB)

Set the LightColor on TestPointLight using a 3-element array [0.0, 0.5, 1.0] for blue.

---

Get the LightColor from TestPointLight to verify it was set.

---

## LightColor Property - Hex Format

Set the LightColor on TestDirectionalLight using hex "#FF00FF" for magenta.

---

Get the LightColor from TestDirectionalLight to verify.

---

## LightColor Property - Named Colors

Set the LightColor on TestSpotLight to "red".

---

Set the LightColor on TestPointLight to "cyan".

---

Set the LightColor on TestDirectionalLight to "yellow".

---

## LightColor Property - Temperature Colors

Set the LightColor on TestSpotLight to "warm" for warm white.

---

Set the LightColor on TestPointLight to "cool" for cool white.

---

Set the LightColor on TestDirectionalLight to "candle" for candlelight.

---

## LightColor Property - Object Format

Set the LightColor on TestSpotLight using object format {"R": 0.8, "G": 0.2, "B": 0.6, "A": 1.0} for pink.

---

Get the LightColor from TestSpotLight to verify the object format worked.

---

# Part 3: Blueprint Variable Color Tests

## Create Color Variables

Create a LinearColor variable called TestLinearColor on ColorTestActor using type_path "/Script/CoreUObject.LinearColor".

---

Create another LinearColor variable called AmbientColor on ColorTestActor with type_path "/Script/CoreUObject.LinearColor" and default_value [0.2, 0.3, 0.4, 1.0].

---

## Set Variable Default Values - Various Formats

Modify TestLinearColor to have a default value of "orange".

---

Modify TestLinearColor to have a default value using hex "#00FF88".

---

Modify TestLinearColor to have a default value using array [0.5, 0.5, 0.5, 0.8].

---

Modify TestLinearColor to have a default value using object {"R": 1.0, "G": 0.0, "B": 0.5}.

---

Get the info for TestLinearColor to verify the value.

---

# Part 4: Blueprint Function Local Variable Color Tests

## Create Function with Color Parameters

Create a function called SetColors on ColorTestActor.

---

Add an input parameter called InColor with param_type "LinearColor" to the SetColors function.

---

Add an output parameter called OutColor with param_type "LinearColor" to the SetColors function.

---

Get the info on the SetColors function to verify the color parameters.

---

# Part 5: Blueprint Node Color Tests

## Discover Color Nodes

Discover nodes with search_term "MakeColor" in ColorTestActor blueprint.

---

Discover nodes with search_term "LinearColor" in ColorTestActor blueprint.

---

Discover nodes with search_term "Color" in ColorTestActor blueprint.

---

## Create Color Nodes in EventGraph

Using the spawner_key from the discover results, create a MakeLinearColor node in the EventGraph of ColorTestActor.

---

Using the spawner_key from the discover results, create a MakeColor node in the EventGraph of ColorTestActor.

---

Using the spawner_key from the discover results, create a LinearColorLerp node in the EventGraph of ColorTestActor.

---

List the nodes in the EventGraph of ColorTestActor to see our color nodes.

---

# Part 6: Material Color Tests

## Add Material Color Nodes

Add a Constant3Vector node to ColorTestMaterial for base color.

---

Add a VectorParameter node called BaseColorParam to ColorTestMaterial.

---

## Set Material Color Values

Set the Constant3Vector node value to [1.0, 0.3, 0.1] for a red-orange color.

---

Get the nodes in ColorTestMaterial to see the color nodes.

---

Compile the material.

---

Save the material.

---

# Part 7: UMG Widget Color Tests

## Add Widget Components for Color Testing

Add a TextBlock called ColorText to ColorTestWidget.

---

Add a Border called ColorBorder to ColorTestWidget.

---

Add an Image called ColorImage to ColorTestWidget.

---

Add a Button called ColorButton to ColorTestWidget.

---

## TextBlock ColorAndOpacity Property

Set the ColorAndOpacity on ColorText to "orange".

---

Set the ColorAndOpacity on ColorText to [1.0, 0.0, 0.5, 1.0] for pink.

---

Set the ColorAndOpacity on ColorText to "#00FFAA" for teal.

---

Get the ColorAndOpacity from ColorText to verify.

---

## Border ContentColorAndOpacity Property

Set ContentColorAndOpacity on ColorBorder to "cyan".

---

Set BrushColor on ColorBorder to [0.2, 0.2, 0.8, 1.0] for blue.

---

Get the BrushColor from ColorBorder to verify.

---

## Image ColorAndOpacity Property

Set ColorAndOpacity on ColorImage to "warm".

---

Set ColorAndOpacity on ColorImage to {"R": 0.9, "G": 0.7, "B": 0.2}.

---

Get ColorAndOpacity from ColorImage to verify.

---

## Button Colors

Set BackgroundColor on ColorButton to "green".

---

Get all the properties on ColorButton related to color.

---

# Part 8: Edge Case & Format Validation Tests

## Hex Format Variations

Set the LightColor on TestSpotLight using 6-digit hex "#AABBCC".

---

Set the LightColor on TestSpotLight using 8-digit hex "#AABBCCDD" (with alpha).

---

Set the LightColor on TestSpotLight using lowercase hex "#ff8800".

---

## Array Format Variations

Set the LightColor on TestPointLight using integer array [255, 128, 64, 255].

---

Set the LightColor on TestPointLight using 3-element array [0.5, 0.3, 0.1].

---

## Object Format Case Variations

Set the LightColor on TestDirectionalLight using lowercase object {"r": 0.5, "g": 0.6, "b": 0.7}.

---

Set the LightColor on TestDirectionalLight using mixed case object {"R": 1.0, "g": 0.5, "B": 0.25, "a": 0.9}.

---

## Named Color Validation

Set ColorAndOpacity on ColorText to "white".

---

Set ColorAndOpacity on ColorText to "black".

---

Set ColorAndOpacity on ColorText to "transparent".

---

Set ColorAndOpacity on ColorText to "gray".

---

Set ColorAndOpacity on ColorText to "grey".

---

# Part 9: Cross-System Color Consistency Tests

## Verify Same Color Across Systems

Set TestSpotLight's LightColor to "#FF6600".

---

Set ColorText's ColorAndOpacity to "#FF6600".

---

Modify TestLinearColor variable default to "#FF6600".

---

Get the LightColor from TestSpotLight.

---

Get the ColorAndOpacity from ColorText.

---

Get the info for TestLinearColor variable.

---

## Verify Named Colors Match

Set TestSpotLight's LightColor to "orange".

---

Set ColorText's ColorAndOpacity to "orange".

---

Get the LightColor from TestSpotLight to see the RGB values.

---

Get the ColorAndOpacity from ColorText to compare values.

---

# Part 10: Cleanup

## Save All Test Assets

Save all dirty assets.

---

## Delete Test Assets (Optional)

Delete the ColorTestActor blueprint.

---

Delete the ColorTestWidget blueprint.

---

Delete the ColorTestMaterial material.

---

# Part 11: Temperature Color Tests

## All Temperature Named Colors

Set TestSpotLight's LightColor to "warm white".

---

Get the LightColor from TestSpotLight to see the warm white RGB values.

---

Set TestSpotLight's LightColor to "cool white".

---

Get the LightColor from TestSpotLight to see the cool white RGB values.

---

Set TestSpotLight's LightColor to "daylight".

---

Get the LightColor from TestSpotLight to see the daylight RGB values.

---

Set TestSpotLight's LightColor to "candlelight".

---

Get the LightColor from TestSpotLight to see the candlelight RGB values.

---

# Part 12: Special Named Colors

## Test All Named Colors

Set ColorAndOpacity on ColorText to "red" and get the value.

---

Set ColorAndOpacity on ColorText to "green" and get the value.

---

Set ColorAndOpacity on ColorText to "blue" and get the value.

---

Set ColorAndOpacity on ColorText to "yellow" and get the value.

---

Set ColorAndOpacity on ColorText to "cyan" and get the value.

---

Set ColorAndOpacity on ColorText to "magenta" and get the value.

---

Set ColorAndOpacity on ColorText to "purple" and get the value.

---

# Summary

This test suite validates:
1. **Blueprint Components** - LightColor on SpotLightComponent, PointLightComponent, DirectionalLightComponent
2. **Blueprint Variables** - LinearColor type variables with default values
3. **Blueprint Functions** - Color input/output parameters
4. **Blueprint Nodes** - MakeColor, MakeLinearColor, LinearColorLerp nodes
5. **Materials** - Constant3Vector and VectorParameter nodes
6. **UMG Widgets** - ColorAndOpacity, BrushColor, BackgroundColor on TextBlock, Border, Image, Button

**All Color Formats Tested:**
- RGBA Arrays: `[1.0, 0.5, 0.0, 1.0]`
- RGB Arrays: `[1.0, 0.5, 0.0]`
- Hex Strings: `#FF8000`, `#FF8000FF`
- Object Format: `{"R": 1.0, "G": 0.5, "B": 0.0}`
- Named Colors: red, green, blue, yellow, cyan, magenta, purple, orange, white, black, gray, grey, transparent
- Temperature Colors: warm, cool, daylight, candle, candlelight, warm white, cool white
