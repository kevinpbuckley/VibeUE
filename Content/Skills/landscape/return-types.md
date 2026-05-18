---
name: return-types
description: Exact property names for LandscapeService return types (LandscapeCreateResult, LandscapeInfo_Custom, LandscapeLayerInfo_Custom, LandscapeSplinePointInfo, LandscapeSplineSegmentInfo, LandscapeSplineMeshEntryInfo) and the create_landscape signature
---

# Return Types - EXACT Properties

> The Common Mistakes table for these return types lives in the main `skill.md` (it is loaded
> automatically alongside the index). This sub-doc is the deeper struct reference.

### LandscapeCreateResult (from create_landscape)

| Property | Type | Description |
|----------|------|-------------|
| `success` | bool | Whether creation succeeded |
| `actor_label` | str | The label assigned to the landscape |
| `error_message` | str | Error details if failed |

**No other properties exist.** No `actor_path`, no `actor_name`.

### LandscapeInfo_Custom (from get_landscape_info)

| Property | Type | Description |
|----------|------|-------------|
| `actor_name` | str | Internal actor name |
| `actor_label` | str | Display label |
| `location` | Vector | World location |
| `rotation` | Rotator | World rotation |
| `scale` | Vector | Scale (e.g. 100,100,100) |
| `component_size_quads` | int | Quads per component |
| `subsection_size_quads` | int | Quads per subsection |
| `num_subsections` | int | Sections per component |
| `num_components` | int | Total component count |
| `resolution_x` | int | Total X resolution |
| `resolution_y` | int | Total Y resolution |
| `material_path` | str | Assigned material path |
| `layers` | Array | List of LandscapeLayerInfo_Custom |

**No `success` field.** Check `actor_label` to verify data was returned.

### LandscapeLayerInfo_Custom (from get_landscape_info or list_layers)

| Property | Type | Description |
|----------|------|-------------|
| `layer_name` | str | Name of the paint layer (e.g. "L1", "Grass") |
| `layer_info_path` | str | Asset path of the layer info object (**NOT** `asset_path`) |
| `is_weight_blended` | bool | Whether the layer uses weight blending |

### create_landscape Signature

```
create_landscape(location, rotation, scale,
    sections_per_component=1, quads_per_section=63,
    component_count_x=8, component_count_y=8,
    landscape_label="") -> LandscapeCreateResult
```

The label parameter is `landscape_label`, NOT `actor_label`.

### LandscapeSplinePointInfo (from get_spline_info)

| Property | Type | Description |
|----------|------|-------------|
| `point_index` | int | Index of this control point |
| `location` | Vector | World-space position |
| `rotation` | Rotator | Control point rotation (tangent direction) |
| `width` | float | Half-width of spline influence |
| `side_falloff` | float | Side falloff distance |
| `end_falloff` | float | End tip falloff distance |
| `layer_name` | str | Paint layer applied under spline (**NOT** `paint_layer_name`) |
| `raise_terrain` | bool | Whether terrain is raised to spline |
| `lower_terrain` | bool | Whether terrain is lowered to spline |
| `mesh_path` | str | Static mesh assigned to this control point (empty = none) |
| `mesh_scale` | Vector | Scale of the control point mesh |
| `segment_mesh_offset` | float | Offset for mesh at segment connections |

### LandscapeSplineSegmentInfo (from get_spline_info)

| Property | Type | Description |
|----------|------|-------------|
| `segment_index` | int | Index of this segment |
| `start_point_index` | int | Index of start control point (**NOT** `start_control_point_index`) |
| `end_point_index` | int | Index of end control point (**NOT** `end_control_point_index`) |
| `start_tangent_length` | float | Start tangent arm length |
| `end_tangent_length` | float | End tangent arm length |
| `layer_name` | str | Layer painted under segment |
| `raise_terrain` | bool | Raise terrain under segment |
| `lower_terrain` | bool | Lower terrain under segment |
| `spline_meshes` | Array[LandscapeSplineMeshEntryInfo] | Mesh entries assigned to this segment |

### LandscapeSplineMeshEntryInfo (mesh entry in segment spline_meshes)

| Property | Type | Description |
|----------|------|-------------|
| `mesh_path` | str | Asset path of the static mesh (e.g. `/Game/.../SM_River.SM_River`) |
| `scale` | Vector | XYZ scale of the mesh along the spline |
| `scale_to_width` | bool | Whether mesh scales to match spline width |
| `material_override_paths` | Array[str] | Asset paths of material overrides (empty = use mesh defaults) |
| `center_adjust` | Vector2D | XY offset to center the mesh on the spline |
| `forward_axis` | int | Spline mesh forward axis: 0=X, 1=Y, 2=Z |
| `up_axis` | int | Spline mesh up axis: 0=X, 1=Y, 2=Z |
