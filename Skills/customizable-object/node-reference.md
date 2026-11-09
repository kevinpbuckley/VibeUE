---
name: node-reference
description: Common Customizable Object node classes (UE 5.8) with their pin names, pin categories and key properties, for CustomizableObjectService.add_node / connect_pins / set_node_property.
---

# Mutable node reference (UE 5.8)

Class names are what `add_node` takes. Pin names are the internal `pin_name`; the display name
the editor shows (in brackets when different) also works. Categories decide what connects.
`discover_node_types()` lists every class; `get_node_type_properties(cls)` lists properties.

## Object structure

| Class | Pins | Key properties |
|---|---|---|
| `CustomizableObjectNodeObject` (Base Object, one per asset, undeletable) | in `Name` string, `Components` component[], `Modifiers` modifier[], `Children` object[]; out `Object` (hidden) | `ObjectName`, `ParentObject`, `States` |
| `CustomizableObjectNodeObjectChild` | same as Object, output visible | `ObjectName` |
| `CustomizableObjectNodeObjectGroup` | in `Name`, `Objects` object[], `Projectors` groupProjector[]; out `Group` object | `GroupName`, `GroupType` (`COGT_TOGGLE`/`COGT_ALL`/`COGT_ONE`/`COGT_ONE_OR_NONE`), `DefaultValue` |
| `CONodeComponentSkeletalMesh` | out `Component`; in `Name`, `Passthrough Skeletal Mesh` [Skeletal Mesh], `Overlay Material` | `ComponentName`, `ReferenceSkeletalMesh` (required) |

## Mesh chain

| Class | Pins | Key properties |
|---|---|---|
| `CustomizableObjectNodeSkeletalMesh` | out `LOD i - Section j - Mesh` [LOD i - Section j] mesh (one per section; a single `Mesh` pin until a mesh is set) | `SkeletalMesh` |
| `CONodeSkeletalMeshSection` | in `Mesh_Input_Pin` [Mesh] mesh, `Material_Input_Pin` [Material] materialAsset, `Enable Tags_Input_Pin` [Enable Tags]; out `Mesh Section_Output_Pin` [Mesh Section] material; plus one pin per material parameter when `TextureParametersMode` exposes them | `Material`, `MaxLOD`, `Tags`, `TextureParametersMode` |
| `CONodeSkeletalMeshMake_V2` | in `LOD 0` material[] (sections); out `Skeletal Mesh` skeletalMesh | — |
| `CONodeSkeletalMeshObjectMake` | in `Name`, `Skeletal Mesh` skeletalMesh; out `Passthrough Skeletal Mesh` [Skeletal Mesh] | `NumLODs`, `LODSettings` |

Minimal chain: SkeletalMesh → Section → Make_V2 → ObjectMake → ComponentSkeletalMesh → Base Object `Components`.

## Parameters and constants

Every parameter has `ParameterName` (plus `ParamUIMetadata`) and an output named after its type,
e.g. `Float`, `Enum`, `Color`.

| Class | Output category | Key properties |
|---|---|---|
| `CustomizableObjectNodeEnumParameter` | enum | `Values` = `((Name="A"),(Name="B"))`, `DefaultIndex` |
| `CustomizableObjectNodeFloatParameter` | float | `DefaultValue` |
| `CustomizableObjectNodeColorParameter` | color | `DefaultValue` = `(R=1,G=0,B=0,A=1)` |
| `CustomizableObjectNodeTextureParameter` | `Texture` image, `Passthrough Texture` | `DefaultValue`, `ReferenceValue`, `TextureSizeX/Y` |
| `CustomizableObjectNodeMaterialParameter` | `Material` materialAsset | `DefaultValue`, `ReferenceValue` |
| `CustomizableObjectNodeSkeletalMeshParameter` | `Skeletal Mesh`, `Passthrough Skeletal Mesh` | `DefaultValue`, `ReferenceValue` |
| `CustomizableObjectNodeFloatConstant` / `ColorConstant` | `Float` / `Color` | `Value` |
| `CustomizableObjectNodeTexture` | `Texture` image, `Passthrough Texture` | `Texture` |
| `CONodeMaterialConstant` | `Material` materialAsset | `Material` |
| `CustomizableObjectNodeTable` | none until `Table` is set, then one per column | `ParameterName`, `Table`, `Structure`, `bAddNoneOption` |

There is no bool parameter node; use an enum or a Group with `COGT_TOGGLE`.

## Switches and variations

`CONodeSwitch` + `{"PinType": <category>}` at creation: out one pin of that category; in
`Switch Parameter` [Parameter] enum, then one input per enum value (named after the value).
The typed classes (`CustomizableObjectNodeMaterialSwitch`, …) are legacy/hidden — do not use them.

Variation nodes (`CONodeSkeletalMeshSectionVariation`, `CustomizableObjectNodeColorVariation`, …)
choose inputs by tag instead of by enum.

## Modifiers

Link a modifier's `Modifier` output into the Base Object's `Modifiers` input. Common ones:
`CustomizableObjectNodeModifierClipMorph`, `CustomizableObjectNodeModifierClipWithMesh`,
`CustomizableObjectNodeModifierRemoveMesh` (plus a `Remove Mesh` mesh input),
`CustomizableObjectNodeModifierRemoveMeshBlocks`, `CONodeModifierEditSkeletalMeshSection`,
`CONodeModifierExtendSkeletalMeshSection`, `CONodeModifierTransformWithBone`.

They choose what they affect by tag: the `RequiredTags` property (`("Hat","Helmet")`) with
`MultipleTagPolicy`, or strings linked into the `Target Tags` input. Sections carry tags in their
`Tags` property or through their `Enable Tags` pin.

## Pin categories

`object`, `component`, `material` (a mesh section), `modifier`, `mesh`, `skeletalMesh`,
`passThroughSkeletalMesh`, `image`, `passThroughImage`, `projector`, `groupProjector`, `color`,
`float`, `bool`, `enum`, `materialAsset`, `transform`, `string`, `staticMesh`, `poseAsset`,
`wildcard`. Same category connects; pass-through parameter pins also feed pass-through inputs.
