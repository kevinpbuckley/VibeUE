# Customizable Object (Mutable) Tests

Read, build, parameterize and compile Mutable Customizable Object graphs through
CustomizableObjectService. Needs a UE 5.8 project with the Mutable plugin loaded (the default).
Uses engine content only (`/Engine/EngineMeshes/SkeletalCube`, `/Engine/EngineMaterials/DefaultMaterial`),
so it runs in any project. Run in order: later prompts build on earlier ones.

---

### 1. Is Mutable available?

```
Can you work with Mutable Customizable Objects in this project? Check, and tell me
how many node types you can create.
```

Expect: `is_mutable_available()` is True and `discover_node_types()` reports ~90 placeable types.

---

### 2. Minimal compilable object

```
Create a Customizable Object at /Game/MutableTest/CO_Cube that uses the engine's
SkeletalCube mesh (/Engine/EngineMeshes/SkeletalCube) with DefaultMaterial
(/Engine/EngineMaterials/DefaultMaterial) as a single component named "Body".
Compile it and tell me whether it compiled cleanly.
```

Expect: the Skeletal Mesh -> Section -> Make V2 -> Object Make -> Component Skeletal Mesh -> Base Object
chain, a compile with success=True, no errors and no warnings, component "Body".

---

### 3. Read a graph back (the Discord question)

```
Describe the node graph of /Game/MutableTest/CO_Cube: every node, and what is
connected to what.
```

Expect: `get_graph` — 6 nodes, 5 connections, described by node title and pin names.

---

### 4. Material without a section material

```
In /Game/MutableTest/CO_Cube, clear the material on the mesh section node and
recompile. What does the compiler say? Then put DefaultMaterial back and confirm the
warning is gone.
```

Expect: a warning "Could not generate a mesh section because it didn't have a material selected",
then a clean compile after restoring `Material`.

---

### 5. Enum parameter and switch

```
Add an enum parameter "Skin" with values Pale, Tan and Dark to /Game/MutableTest/CO_Cube,
and a switch that picks the mesh section by that parameter. Wire the existing mesh section
into all three switch options, route the switch into the Make node instead of the section,
compile, and list the compiled parameters with their options.
```

Expect: `CONodeSwitch` created with `{"PinType":"material"}`, 3 option inputs named Pale/Tan/Dark,
compiled parameter `Skin` (type Int) with the three options.

---

### 6. Editing enum values updates the switch

```
Add a fourth value "Blue" to the Skin parameter in /Game/MutableTest/CO_Cube. Does the
switch now have an input for it? Leave it unconnected and compile — what happens?
```

Expect: the switch gains a "Blue" input without a manual refresh; compile result reported honestly
(warning or error about the empty option, whatever the compiler says).

---

### 7. Float and color parameters

```
Add a float parameter "Roughness" (default 0.5) and a color parameter "Tint"
(default pure red) to /Game/MutableTest/CO_Cube and list the graph parameters.
```

Expect: `ParameterName` / `DefaultValue` set through `set_node_property`, color as
`(R=1,G=0,B=0,A=1)`, both listed in `graph_parameters`.

---

### 8. Groups and child objects

```
Give /Game/MutableTest/CO_Cube a "Hat" slot where the player can pick one hat or none.
Create two hat objects, /Game/MutableTest/CO_Hat_Cap and /Game/MutableTest/CO_Hat_Crown,
each using the SkeletalCube mesh with DefaultMaterial. Compile the root and tell me the
options of the Hat parameter.
```

Expect: a Group node (`GroupName`="Hat", `GroupType`=COGT_ONE_OR_NONE) on the base's `Children`,
two child assets named Cap/Crown attached with `set_parent_object`, each with its own mesh chain,
and a compiled `Hat` parameter with options None, Cap, Crown.

---

### 9. Relationships

```
Which objects are children of /Game/MutableTest/CO_Cube, and what is CO_Hat_Cap's parent?
```

Expect: `get_object_info` — child_objects lists both hats; the cap reports is_child_object and the
root as parent_object.

---

### 10. Diagnose a broken object

```
Create /Game/MutableTest/CO_Broken with the same mesh chain as CO_Cube but WITHOUT a
reference skeletal mesh on the component. Compile it, explain the error, fix it, and
recompile.
```

Expect: compile fails with "Missing reference Skeletal Mesh"; the fix sets `ReferenceSkeletalMesh`
on `CONodeComponentSkeletalMesh`; the recompile succeeds.

---

### 11. Refusals are explained, not hidden

```
In /Game/MutableTest/CO_Cube, try to: connect the Skin enum output straight into the base
object's Components input; delete the Base Object node; set the Skin parameter's
DefaultIndex to "two". Report exactly what each attempt returned.
```

Expect: three refusals with reasons — incompatible pin categories (enum vs component), base node
cannot be deleted, "two" is not a number. Nothing in the asset changes.

---

### 12. Remove a feature cleanly

```
Remove the Roughness and Tint parameters from /Game/MutableTest/CO_Cube, detach
CO_Hat_Crown from the Hat group, recompile, and confirm Hat now offers only None and Cap.
```

Expect: `remove_node` x2, `set_parent_object(crown, "", "")`, compiled Hat options None and Cap.
