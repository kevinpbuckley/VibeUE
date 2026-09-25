---
name: customizable-object
display_name: Mutable Customizable Objects
description: Read, build, wire, parameterize and compile Mutable Customizable Object graphs (UCustomizableObject) — skeletal mesh chains, mesh sections and materials, components, enum/float/color/texture parameters, switches, groups and child objects, compile errors (CustomizableObjectService). Use when the user asks about Mutable, a Customizable Object / CO asset, character customization graphs, or modular characters built with Mutable.
vibeue_classes:
  - CustomizableObjectService
unreal_classes:
  - CustomizableObject
  - CustomizableObjectInstance
keywords:
  - mutable
  - customizable object
  - customizable objects
  - co graph
  - character customization
  - modular character
  - skeletal mesh section
  - mesh section
  - object group
  - child object
  - enum parameter
  - switch node
  - compile customizable object
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this
> session, call it with `{action: "search", query: "mutable customizable object"}` for UE domain
> knowledge on Mutable. If no such tool is available, skip this line entirely and proceed with this
> skill alone — do NOT attempt the call.

# Mutable Customizable Objects Skill

One service, reachable from Python (`unreal.CustomizableObjectService.*`) and as AICallable tools.
It edits the Customizable Object's **editor graph** (the node graph you see in the CO editor) and
compiles it. Discover exact signatures with
`discover_python_class('unreal.CustomizableObjectService')`.

- **Is Mutable on?** `is_mutable_available()`. In UE 5.8 it normally is (the default-enabled
  MutableAssetUserData plugin loads it). If it returns False, every method explains that the
  project has Mutable disabled — tell the user; VibeUE never enables plugins for them.
- **Addressing:** nodes by `node_id` (the NodeGuid, from `get_graph` / `add_node`); pins by
  `pin_name` OR the display name the editor shows (`"Mesh"` works for `Mesh_Input_Pin`).
- **Every write saves the asset** and returns `success` / `error`. Read `error` — it names the
  cause (incompatible pin categories, unknown property, value that did not parse).

## The golden workflow — a compilable object

```python
import unreal, json
CO = unreal.CustomizableObjectService
path = "/Game/Characters/CO_Hero"
MESH = "/Game/Characters/Meshes/SK_Body.SK_Body"          # any skeletal mesh WITH a skeleton
MAT  = "/Game/Characters/Materials/M_Body.M_Body"

base = CO.create_customizable_object(path)                # graph + Base Object node
b = base.node_id

# Nodes. Pass properties that shape pins AT CREATION (see "Pins depend on properties" below).
sk   = CO.add_node(path, "CustomizableObjectNodeSkeletalMesh", -1200, 0, json.dumps({"SkeletalMesh": MESH}))
sec  = CO.add_node(path, "CONodeSkeletalMeshSection",      -900, 0, json.dumps({"Material": MAT}))
make = CO.add_node(path, "CONodeSkeletalMeshMake_V2",      -600, 0)
obj  = CO.add_node(path, "CONodeSkeletalMeshObjectMake",   -400, 0)
comp = CO.add_node(path, "CONodeComponentSkeletalMesh",    -200, 0,
                   json.dumps({"ReferenceSkeletalMesh": MESH, "ComponentName": "Body"}))

# Wiring: output -> input (either order is accepted).
CO.connect_pins(path, sk.node_id,   "LOD 0 - Section 0",         sec.node_id,  "Mesh")
CO.connect_pins(path, sec.node_id,  "Mesh Section",              make.node_id, "LOD 0")
CO.connect_pins(path, make.node_id, "Skeletal Mesh",             obj.node_id,  "Skeletal Mesh")
CO.connect_pins(path, obj.node_id,  "Passthrough Skeletal Mesh", comp.node_id, "Passthrough Skeletal Mesh")
CO.connect_pins(path, comp.node_id, "Component",                 b,            "Components")

r = CO.compile_object(path)
print(r.success, r.state, list(r.errors), list(r.warnings))
info = CO.get_object_info(path)                           # compiled parameters, states, components
```

That chain — **Skeletal Mesh → Skeletal Mesh Section → Skeletal Mesh Make V2 → Skeletal Mesh
Object Make → Component Skeletal Mesh → Base Object "Components"** — is the minimal compilable
object in UE 5.8. Everything else (parameters, switches, modifiers, children) hangs off it.

## Reading an existing object

```python
g = CO.get_graph(path)                 # nodes (class, title, pins with links) + flat connections
for n in g.nodes: print(n.node_id, n.node_class, n.title)
CO.get_node_properties(path, node_id)  # settable properties with current values (text format)
CO.get_object_info(path)               # parent/children, graph + compiled parameters, states, components
CO.discover_node_types("Parameter")    # node classes add_node accepts (filter is a substring)
CO.get_node_type_properties("CONodeSwitch")   # a class's properties and defaults
```

## Things that are not obvious and will bite you

- **A Section needs a material.** Without `Material` on the `CONodeSkeletalMeshSection` the object
  still compiles, but with the warning "Could not generate a mesh section because it didn't have a
  material selected" — and that section is missing from the result. Set it at creation or with
  `set_node_property(path, sec, "Material", "/Game/.../M_X.M_X")`, or link a Material node into its
  `Material` pin.
- **The component needs `ReferenceSkeletalMesh`**, and that mesh needs a Skeleton. Without it the
  compile FAILS: "Missing reference Skeletal Mesh".
- **Pins depend on properties.** A Skeletal Mesh node has one output per LOD/section
  (`"LOD 0 - Section 0 - Mesh"`, display `"LOD 0 - Section 0"`) only once `SkeletalMesh` is set; a
  Switch's output type comes from `PinType`. Pass these in `add_node`'s `properties_json`, or set
  them afterwards with `set_node_property` (the node rebuilds its pins and the result lists them).
  `PinType` on a switch must be given at creation.
- **Switches:** create `CONodeSwitch` with `{"PinType": "<category>"}` — `material` (mesh
  section), `mesh`, `color`, `float`, `image`, `component`, … — then link an Enum Parameter's
  `Enum` output into its `Switch Parameter` input. It grows one input per enum value, named after
  the value. Editing the enum's `Values` later rebuilds the switch automatically.
- **An unconnected switch option compiles silently.** Add an enum value and the switch grows an
  input for it; leave that input empty and the compile reports no error and no warning, yet the
  option has nothing behind it. After editing enum values, check every switch input is linked
  (`get_node(path, switch).pins[*].linked_to`).
- **Compile problems are not on the nodes.** `get_graph` nodes' `message` stays empty; read
  `compile_object(...).errors` / `.warnings`, which end with `(Node <title>)`.
- **Enum values are a struct array:** `set_node_property(path, enum, "Values", '((Name="Red"),(Name="Blue"))')`.
  Read `get_node_properties` first and mirror the shape it shows for any struct/array property.
- **Parameter names are properties, not pins:** `ParameterName` (parameters), `ObjectName` (Object
  nodes), `GroupName` (groups), `ComponentName` (components). They are listed with
  `editable=False` because the editor uses custom UI for them — they are still settable.
- **Compiled vs graph parameters.** `get_object_info().graph_parameters` lists parameter nodes in
  this asset right away; `compiled_parameters` only reflects the last successful compile and only
  includes parameters that feed the object. Recompile after edits before trusting it. An enum
  parameter compiles to type `Int`.
- **Numbers and booleans are validated.** `"abc"` for an int is refused (the engine would store 0).
- **Pin types must match.** `connect_pins` refuses incompatible categories with the schema's
  reason; a single-link input silently dropping its old link is reported in `notes`.
- **The Base Object node cannot be deleted.** Tunnel nodes (Macro Library only) cannot be added.

## Groups and child objects

A **Group** node (`CustomizableObjectNodeObjectGroup`) is a parameter over child objects:
`GroupType` = `COGT_TOGGLE` / `COGT_ALL` / `COGT_ONE` / `COGT_ONE_OR_NONE`, named by `GroupName`.
Link its `Group` output to the Base Object's `Children` input. Children live in their own assets:

```python
grp = CO.add_node(root, "CustomizableObjectNodeObjectGroup", 0, 300,
                  json.dumps({"GroupName": "Hat", "GroupType": "COGT_ONE_OR_NONE"}))
CO.connect_pins(root, grp.node_id, "Group", root_base_id, "Children")

cap = CO.create_customizable_object("/Game/Characters/CO_Hat_Cap")
CO.set_node_property("/Game/Characters/CO_Hat_Cap", cap.node_id, "ObjectName", "Cap")
CO.set_parent_object("/Game/Characters/CO_Hat_Cap", root, grp.node_id)   # "" parent detaches
# ...build the cap's own mesh chain into ITS base node's Components...
CO.compile_object(root)     # compile the ROOT; the "Hat" parameter gains the option "Cap"
```

The child's `ObjectName` becomes its option name. `get_object_info(root).child_objects` lists the
children; parenting refuses cycles and non-Group targets.

## Compile

`compile_object(path, optimization_level="None", texture_compression="Fast")` is synchronous and
always a real compile. `errors` / `warnings` are the compiler's own messages (most end with
`(Node <title>)` naming the culprit); `log` has every Mutable log line. `success` needs a valid model
AND no errors. Compiling a child compiles its root.

## Deeper reference

`customizable-object/node-reference` — the common node classes with their pins and properties.
