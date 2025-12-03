# manage_material_node Test Prompts

## Prerequisites
- ✅ Unreal Engine 5.7+ running
- ✅ VibeUE plugin loaded
- ✅ MCP connection active

## 🚨 IMPORTANT: Test Asset Management

**DO NOT delete test assets until after reviewing ALL test results!**

### Setup: Create Test Material FIRST

**Run these commands at the START of testing:**

1. **Search for Existing Test Material**
   ```
   Use manage_asset with action="search":
   - search_term: "M_NodeTest"
   - asset_type: "Material"
   ```

2. **If Not Found, Create Test Material**
   ```
   Use manage_material with action="create":
   - destination_path: "/Game/Materials/test"
   - material_name: "M_NodeTest"
   ```

3. **Open Material in Editor**
   ```
   Use manage_asset with action="open_in_editor":
   - asset_path: "/Game/Materials/test/M_NodeTest"
   - This opens the Material Editor so you can see nodes being created
   ```

**💡 TIP**: Keep the Material Editor open throughout testing to watch nodes appear in real-time!

---

## Test 1: Discover Expression Types

**Purpose**: Find available material expression types

### Steps

```
Use manage_material_node with action="discover_types":
- search_term: "Add"
- max_results: 20
```

### Expected Outcomes
- ✅ Returns list of expression types containing "Add"
- ✅ Each type shows class_name, display_name, category
- ✅ MaterialExpressionAdd should be in results

---

## Test 2: Discover Types by Category

**Purpose**: Filter expression types by category

### Steps

```
Use manage_material_node with action="discover_types":
- category: "Math"
- max_results: 50
```

### Expected Outcomes
- ✅ Returns math-related expressions
- ✅ Add, Subtract, Multiply, Divide should be included

---

## Test 3: Get Expression Categories

**Purpose**: List all available expression categories

### Steps

```
Use manage_material_node with action="get_categories"
```

### Expected Outcomes
- ✅ Returns list of category names
- ✅ Should include Math, Texture, Coordinates, etc.

---

## Test 4: Create Constant Expression

**Purpose**: Create a scalar constant node

### Steps

```
Use manage_material_node with action="create":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_class: "Constant"
- pos_x: -400
- pos_y: 0

👀 Watch the Material Editor - a Constant node should appear!
```

### Expected Outcomes
- ✅ Constant expression created
- ✅ Returns expression_id
- ✅ Node visible in Material Editor at position (-400, 0)
- ✅ Shows default value of 0

---

## Test 5: Create Vector Constant Expression

**Purpose**: Create a color/vector constant node

### Steps

```
Use manage_material_node with action="create":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_class: "Constant3Vector"
- pos_x: -400
- pos_y: 100

👀 A Constant3Vector (color) node should appear below the first one!
```

### Expected Outcomes
- ✅ Constant3Vector expression created
- ✅ Node shows RGB output pins
- ✅ Default black color (0, 0, 0)

---

## Test 6: Create Math Expression (Add)

**Purpose**: Create an Add node for combining values

### Steps

```
Use manage_material_node with action="create":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_class: "Add"
- pos_x: -200
- pos_y: 0

👀 An Add node should appear with A and B inputs!
```

### Expected Outcomes
- ✅ Add expression created
- ✅ Shows inputs A and B
- ✅ Shows single output

---

## Test 7: Create Multiply Expression

**Purpose**: Create a Multiply node

### Steps

```
Use manage_material_node with action="create":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_class: "Multiply"
- pos_x: -200
- pos_y: 200

👀 A Multiply node should appear!
```

### Expected Outcomes
- ✅ Multiply expression created
- ✅ Shows inputs A and B

---

## Test 8: List All Expressions

**Purpose**: Get list of all expressions in the material

### Steps

```
Use manage_material_node with action="list":
- material_path: "/Game/Materials/test/M_NodeTest"
```

### Expected Outcomes
- ✅ Returns array of all expressions
- ✅ Each has id, class_name, position, etc.
- ✅ Should show 4 expressions from previous tests

---

## Test 9: Get Expression Details

**Purpose**: Get detailed info about a specific expression

### Steps

```
First, get the expression_id from Test 8 (list action)

Use manage_material_node with action="get_details":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_id: "<id_from_list>"
```

### Expected Outcomes
- ✅ Returns complete expression info
- ✅ Shows inputs and outputs
- ✅ Shows position
- ✅ Shows if it's a parameter

---

## Test 10: Get Expression Pins

**Purpose**: Get all pins (inputs and outputs) for an expression

### Steps

```
Use manage_material_node with action="get_pins":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_id: "<add_node_id>"
```

### Expected Outcomes
- ✅ Returns input pins (A, B)
- ✅ Returns output pin
- ✅ Shows connection status for each
- ✅ Shows pin index and direction

---

## Test 11: Move Expression

**Purpose**: Reposition an expression in the graph

### Steps

```
Use manage_material_node with action="move":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_id: "<constant_node_id>"
- pos_x: -500
- pos_y: 50

👀 Watch the Constant node move to a new position!
```

### Expected Outcomes
- ✅ Expression moved to new position
- ✅ Node visible at (-500, 50) in editor

---

## Test 12: List Expression Properties

**Purpose**: Get editable properties of an expression

### Steps

```
Use manage_material_node with action="list_properties":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_id: "<constant_node_id>"
```

### Expected Outcomes
- ✅ Lists "R" property for Constant
- ✅ Shows current value
- ✅ May show "Desc" property

---

## Test 13: Get Expression Property

**Purpose**: Get a specific property value

### Steps

```
Use manage_material_node with action="get_property":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_id: "<constant_node_id>"
- property_name: "R"
```

### Expected Outcomes
- ✅ Returns current R value (default 0.0)
- ✅ Shows property name and value

---

## Test 14: Set Expression Property

**Purpose**: Modify an expression's property

### Steps

```
Use manage_material_node with action="set_property":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_id: "<constant_node_id>"
- property_name: "R"
- value: "0.75"

👀 The Constant node should now show 0.75 as its value!
```

### Expected Outcomes
- ✅ Property updated
- ✅ Node displays new value
- ✅ Material may need recompile

---

## Test 15: Connect Expressions

**Purpose**: Wire two expressions together

### Steps

```
Use manage_material_node with action="connect":
- material_path: "/Game/Materials/test/M_NodeTest"
- source_expression_id: "<constant_node_id>"
- source_output: "" (empty = first output)
- target_expression_id: "<add_node_id>"
- target_input: "A"

👀 A wire should appear from Constant to Add's A input!
```

### Expected Outcomes
- ✅ Connection created
- ✅ Wire visible in editor
- ✅ Add node's A input now connected

---

## Test 16: List All Connections

**Purpose**: Get all connections in the material

### Steps

```
Use manage_material_node with action="list_connections":
- material_path: "/Game/Materials/test/M_NodeTest"
```

### Expected Outcomes
- ✅ Returns array of connections
- ✅ Each shows source/target expression IDs
- ✅ Shows which input/output connected
- ✅ Should include connection from Test 15

---

## Test 17: Get Material Output Properties

**Purpose**: List available material output slots

### Steps

```
Use manage_material_node with action="get_output_properties":
- material_path: "/Game/Materials/test/M_NodeTest"
```

### Expected Outcomes
- ✅ Lists: BaseColor, Metallic, Specular, Roughness, etc.
- ✅ Lists all available material property outputs
- ✅ Includes EmissiveColor, Normal, etc.

---

## Test 18: Get Material Output Connections

**Purpose**: See what's connected to material outputs

### Steps

```
Use manage_material_node with action="get_output_connections":
- material_path: "/Game/Materials/test/M_NodeTest"
```

### Expected Outcomes
- ✅ Shows which expressions connect to material outputs
- ✅ Returns map of property → expression_id
- ✅ Empty properties not connected

---

## Test 19: Connect to Material Output

**Purpose**: Wire an expression to a material property (e.g., BaseColor)

### Steps

```
Use manage_material_node with action="connect_to_output":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_id: "<constant3vector_node_id>"
- output_name: "" (empty = first output)
- material_property: "BaseColor"

👀 A wire should connect to the material's BaseColor input!
👀 The material preview should change color!
```

### Expected Outcomes
- ✅ Expression connected to BaseColor
- ✅ Wire visible to material node
- ✅ Preview updates with new color

---

## Test 20: Connect Another Expression to Output

**Purpose**: Wire to Roughness material property

### Steps

```
Use manage_material_node with action="connect_to_output":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_id: "<constant_node_id>"
- material_property: "Roughness"

👀 The constant should now control roughness!
```

### Expected Outcomes
- ✅ Constant connected to Roughness
- ✅ Preview shows different roughness based on value

---

## Test 21: Disconnect Expression Input

**Purpose**: Break a connection to an expression input

### Steps

```
Use manage_material_node with action="disconnect":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_id: "<add_node_id>"
- input_name: "A"

👀 The wire to Add's A input should disappear!
```

### Expected Outcomes
- ✅ Connection removed
- ✅ Wire no longer visible
- ✅ Add node's A input now empty

---

## Test 22: Disconnect Material Output

**Purpose**: Break connection to material output property

### Steps

```
Use manage_material_node with action="disconnect_output":
- material_path: "/Game/Materials/test/M_NodeTest"
- material_property: "Roughness"

👀 The wire to Roughness should disappear!
```

### Expected Outcomes
- ✅ Connection to Roughness removed
- ✅ Material preview changes (default roughness)

---

## Test 23: Create Scalar Parameter

**Purpose**: Create a named scalar parameter for material instances

### Steps

```
Use manage_material_node with action="create_parameter":
- material_path: "/Game/Materials/test/M_NodeTest"
- parameter_type: "Scalar"
- parameter_name: "RoughnessValue"
- default_value: "0.5"
- group_name: "Surface"
- pos_x: -600
- pos_y: 0

👀 A ScalarParameter node should appear with name "RoughnessValue"!
```

### Expected Outcomes
- ✅ ScalarParameter created
- ✅ Shows parameter name
- ✅ Default value set to 0.5
- ✅ Group set to "Surface"

---

## Test 24: Create Vector Parameter

**Purpose**: Create a color parameter

### Steps

```
Use manage_material_node with action="create_parameter":
- material_path: "/Game/Materials/test/M_NodeTest"
- parameter_type: "Vector"
- parameter_name: "BaseColorTint"
- group_name: "Color"
- pos_x: -600
- pos_y: 150

👀 A VectorParameter node should appear for color control!
```

### Expected Outcomes
- ✅ VectorParameter created
- ✅ Shows RGBA output pins
- ✅ Can be used for color in instances

---

## Test 25: Promote Constant to Parameter

**Purpose**: Convert existing constant to a parameter

### Steps

```
First, create a fresh constant:
Use manage_material_node with action="create":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_class: "Constant"
- pos_x: -700
- pos_y: 300

Then promote it:
Use manage_material_node with action="promote_to_parameter":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_id: "<new_constant_id>"
- parameter_name: "PromotedValue"
- group_name: "Promoted"

👀 The Constant should transform into a ScalarParameter!
```

### Expected Outcomes
- ✅ Constant converted to ScalarParameter
- ✅ Old constant removed
- ✅ New parameter has same value
- ✅ Connections preserved (if any)

---

## Test 26: Set Parameter Metadata

**Purpose**: Update parameter group and sort priority

### Steps

```
Use manage_material_node with action="set_parameter_metadata":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_id: "<roughness_param_id>"
- group_name: "NewGroup"
- sort_priority: 10

👀 Check the parameter details - group should be "NewGroup"!
```

### Expected Outcomes
- ✅ Group name updated
- ✅ Sort priority set
- ✅ Parameter reorders in material instance UI

---

## Test 27: Create Texture Sample

**Purpose**: Create a texture sampling node

### Steps

```
Use manage_material_node with action="create":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_class: "TextureSample"
- pos_x: -400
- pos_y: 400

👀 A TextureSample node should appear with UV input and color outputs!
```

### Expected Outcomes
- ✅ TextureSample created
- ✅ Shows UV input
- ✅ Shows RGB, R, G, B, A output pins

---

## Test 28: Create Texture Coordinate

**Purpose**: Create a UV coordinate node

### Steps

```
Use manage_material_node with action="create":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_class: "TextureCoordinate"
- pos_x: -600
- pos_y: 400

👀 A TexCoord node should appear!
```

### Expected Outcomes
- ✅ TextureCoordinate created
- ✅ Default UV channel 0
- ✅ Can be connected to texture UV input

---

## Test 29: Create Lerp (Linear Interpolate)

**Purpose**: Create a blend/lerp node

### Steps

```
Use manage_material_node with action="create":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_class: "LinearInterpolate"
- pos_x: -100
- pos_y: 100

👀 A Lerp node should appear with A, B, and Alpha inputs!
```

### Expected Outcomes
- ✅ LinearInterpolate created
- ✅ Shows A, B, and Alpha inputs
- ✅ Single output for blended result

---

## Test 30: Delete Expression

**Purpose**: Remove an expression from the material

### Steps

```
Use manage_material_node with action="delete":
- material_path: "/Game/Materials/test/M_NodeTest"
- expression_id: "<lerp_node_id>"

👀 The Lerp node should disappear!
```

### Expected Outcomes
- ✅ Expression removed
- ✅ Any connections broken
- ✅ Node no longer in list

---

## Complete Workflow Test: Simple PBR Material

**Purpose**: Create a complete simple material with parameters

### Steps

1. **Create Parameters**
   ```
   Create ScalarParameter "Roughness" at (-600, 0) with default 0.5, group "Surface"
   Create ScalarParameter "Metallic" at (-600, 100) with default 0.0, group "Surface"
   Create VectorParameter "BaseColor" at (-600, 200) with group "Color"
   ```

2. **Connect to Material Outputs**
   ```
   Connect BaseColor → material BaseColor
   Connect Roughness → material Roughness
   Connect Metallic → material Metallic
   ```

3. **Verify Connections**
   ```
   Get output connections - should show all three connected
   ```

4. **Compile and Save**
   ```
   Compile material
   Save material
   ```

5. **Create Instance**
   ```
   Create instance MI_NodeTest_Chrome with:
   - Roughness: 0.1
   - Metallic: 1.0
   - BaseColor: (0.8, 0.8, 0.9, 1.0)
   ```

👀 Open the instance - should look like shiny chrome!

### Expected Outcomes
- ✅ Material has 3 parameters connected to outputs
- ✅ Instance shows chrome-like appearance
- ✅ Parameters exposed in instance

---

## Cleanup (Run AFTER manual review)

⚠️ **Only run after reviewing all results in Unreal Editor!**

```
Use manage_asset with action="delete":
- asset_path: "/Game/Materials/test/MI_NodeTest_Chrome"
- force_delete: true
- show_confirmation: false

Use manage_asset with action="delete":
- asset_path: "/Game/Materials/test/M_NodeTest"
- force_delete: true
- show_confirmation: false
```

---

## Summary Checklist

### Discovery Actions
| Test | Action | Status |
|------|--------|--------|
| 1 | discover_types (search) | ⬜ |
| 2 | discover_types (category) | ⬜ |
| 3 | get_categories | ⬜ |

### Expression Lifecycle Actions
| Test | Action | Status |
|------|--------|--------|
| 4 | create (Constant) | ⬜ |
| 5 | create (Constant3Vector) | ⬜ |
| 6 | create (Add) | ⬜ |
| 7 | create (Multiply) | ⬜ |
| 27 | create (TextureSample) | ⬜ |
| 28 | create (TextureCoordinate) | ⬜ |
| 29 | create (LinearInterpolate) | ⬜ |
| 30 | delete | ⬜ |

### Expression Information Actions
| Test | Action | Status |
|------|--------|--------|
| 8 | list | ⬜ |
| 9 | get_details | ⬜ |
| 10 | get_pins | ⬜ |
| 11 | move | ⬜ |

### Expression Property Actions
| Test | Action | Status |
|------|--------|--------|
| 12 | list_properties | ⬜ |
| 13 | get_property | ⬜ |
| 14 | set_property | ⬜ |

### Connection Actions
| Test | Action | Status |
|------|--------|--------|
| 15 | connect | ⬜ |
| 16 | list_connections | ⬜ |
| 21 | disconnect | ⬜ |

### Material Output Actions
| Test | Action | Status |
|------|--------|--------|
| 17 | get_output_properties | ⬜ |
| 18 | get_output_connections | ⬜ |
| 19 | connect_to_output | ⬜ |
| 20 | connect_to_output (second) | ⬜ |
| 22 | disconnect_output | ⬜ |

### Parameter Actions
| Test | Action | Status |
|------|--------|--------|
| 23 | create_parameter (Scalar) | ⬜ |
| 24 | create_parameter (Vector) | ⬜ |
| 25 | promote_to_parameter | ⬜ |
| 26 | set_parameter_metadata | ⬜ |

### Workflow Tests
| Test | Description | Status |
|------|-------------|--------|
| Complete | Simple PBR Material | ⬜ |
