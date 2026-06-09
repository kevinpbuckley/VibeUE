---
name: umg-widgets/reference
description: Field-name reference for UMG WidgetService return types (WidgetInfo, snapshots, slot/font/brush/anim info, preview + PIE handles), the WidgetService action list, common widget properties, and event names.
---

# UMG Widget Reference

All struct names below are the Python names exactly as `unreal` exposes them (no `F` prefix).

## Contents
- WidgetInfo (list_components / get_hierarchy)
- WidgetComponentSnapshot (get_widget_snapshot / get_component_snapshot)
- WidgetSlotInfo
- WidgetPropertyInfo
- WidgetFontInfo
- WidgetBrushInfo
- WidgetAnimInfo
- WidgetPreviewResult
- PIEWidgetHandle
- WidgetAddComponentResult / WidgetEventInfo
- Common properties
- WidgetService actions
- Event names

## WidgetInfo

Returned by `list_components()` and `get_hierarchy()`.

| Field | Type | Description |
|-------|------|-------------|
| `widget_name` | str | Component name — **NOT** `component_name` or `name` |
| `widget_class` | str | UE class name (e.g. "TextBlock") |
| `parent_widget` | str | Parent component name (empty for root) |
| `is_root_widget` | bool | True if this is the root |
| `is_variable` | bool | Exposed as Blueprint variable |
| `children` | list[str] | Child component names |

## WidgetComponentSnapshot

Returned by `get_widget_snapshot()` and `get_component_snapshot()`. All `WidgetInfo` fields plus:

| Field | Type | Description |
|-------|------|-------------|
| `slot_info` | WidgetSlotInfo | Slot layout data |
| `properties` | list[WidgetPropertyInfo] | All widget properties |

## WidgetSlotInfo

Snapshot of a widget's slot. `set_property` can write: Canvas layout (`Position X/Y`, `Size X/Y`,
`Anchor Min/Max X/Y`, `Alignment X/Y`, `ZOrder`) and box/overlay layout (`Horizontal Alignment`,
`Vertical Alignment`, `Padding`[`Left/Top/Right/Bottom`], `Size Rule`, `Size Value`). See SKILL.md →
"Slot editing via set_property". To center on a canvas: anchors `0.5` + `Alignment X/Y` `0.5`.

| Field | Type | Description |
|-------|------|-------------|
| `slot_type` | str | "Canvas", "VerticalBox", "HorizontalBox", "Overlay", "None" |
| `anchor_min` | Vector2D | Canvas: anchor minimum |
| `anchor_max` | Vector2D | Canvas: anchor maximum |
| `offsets` | Margin | Canvas: position+size or margins |
| `alignment` | Vector2D | Canvas: pivot alignment |
| `z_order` | int | Canvas: Z-order |
| `auto_size` | bool | Canvas: auto-size to content |
| `size_rule` | str | Box: "Fill" or "Automatic" |
| `size_value` | float | Box: fill coefficient |
| `padding` | Margin | Box/Overlay: slot padding |
| `horizontal_alignment` | EHorizontalAlignment | Box/Overlay: horizontal alignment |
| `vertical_alignment` | EVerticalAlignment | Box/Overlay: vertical alignment |

## WidgetPropertyInfo

Returned in `WidgetComponentSnapshot.properties` and by `list_properties()`.

| Field | Type | Description |
|-------|------|-------------|
| `property_name` | str | Property name — **NOT** `name` |
| `property_type` | str | Property type — **NOT** `type` |
| `current_value` | str | Current value as string — **NOT** `property_value` |
| `is_editable` | bool | Whether the property is editable |

## WidgetFontInfo

Used by `set_font` / returned by `get_font`. `set_font(widget_path, component_name, font_info, property_name="Font")`.

| Field | Type | Description |
|-------|------|-------------|
| `font_family` | str | Font asset object path or family name when available |
| `typeface` | str | Typeface name such as `Regular` or `Bold` |
| `size` | int | Font size |
| `letter_spacing` | int | Letter spacing/tracking |
| `color` | str | Text color as linear color text |
| `shadow_offset` | str | Shadow offset as vector text |
| `shadow_color` | str | Shadow color as linear color text |
| `outline_size` | int | Outline thickness |
| `outline_color` | str | Outline color as linear color text |

## WidgetBrushInfo

Used by `set_brush` / returned by `get_brush`. **Both take a `slot_name`:**
`set_brush(widget_path, component_name, slot_name, brush_info)` /
`get_brush(widget_path, component_name, slot_name)`. For an Image the slot is `"Brush"`.

| Field | Type | Description |
|-------|------|-------------|
| `resource_path` | str | Texture or material object path |
| `tint_color` | str | Brush tint as linear color text |
| `draw_as` | str | `Image`, `Box`, `Border`, or `RoundedBox` |
| `image_size` | str | Image size as vector text |
| `margin` | str | Brush margin as margin text |
| `corner_radius` | str | Rounded-box radii text |

## WidgetAnimInfo

Returned by `list_animations()`.

| Field | Type | Description |
|-------|------|-------------|
| `animation_name` | str | Animation asset name inside the widget blueprint |
| `duration` | float | Playback duration in seconds |
| `track_count` | int | Number of possessable property tracks |

## WidgetPreviewResult

Returned by `capture_preview(widget_path, width=1920, height=1080)` — note: **no output-path
argument**; the PNG is written to `<project>/Saved/WidgetPreviews/<WidgetName>.png`.

| Field | Type | Description |
|-------|------|-------------|
| `success` | bool | True when the preview PNG was written (**not** `b_success`) |
| `output_path` | str | Auto-generated saved image path |
| `width` | int | Render width |
| `height` | int | Render height |
| `error_message` | str | Failure message when `success` is false |

## PIEWidgetHandle

Returned by `spawn_widget_in_pie(widget_path, z_order=0)`. Pass the **whole handle object** to
`get_live_property(handle, component_name, property_name)` and `remove_widget_from_pie(handle)`.

| Field | Type | Description |
|-------|------|-------------|
| `valid` | bool | True when the PIE widget instance was spawned (**not** `b_valid`) |
| `instance_id` | str | Stable ID string (informational) |
| `error_message` | str | Failure message when spawn fails (e.g. "PIE is not running.") |

## WidgetAddComponentResult / WidgetEventInfo

`add_component()` returns **WidgetAddComponentResult**: `success`, `component_name`, `component_type`,
`parent_name`, `is_variable`, `error_message`.

`get_available_events()` returns **WidgetEventInfo** list: `event_name`, `event_type`, `description`.

## Common properties

| Widget | Property | Example value (always a string) |
|--------|----------|----------------------------------|
| TextBlock | Text | "Hello World" |
| TextBlock | Font.Size | "24" |
| Button | Background Color | "(R=0.0,G=0.5,B=1.0,A=1.0)" |
| Image | ColorAndOpacity | "(R=1.0,G=1.0,B=1.0,A=0.5)" |
| ProgressBar | Percent | "0.75" |
| Any | Visibility | "Visible" / "Hidden" / "Collapsed" |

## WidgetService actions

| Action | Signature notes |
|--------|-----------------|
| `add_component` | `(widget_path, type, name, parent, set_as_root)` → WidgetAddComponentResult |
| `remove_component` | Remove a widget (and optionally its children) |
| `rename_widget` | `(widget_path, old_name, new_name)` |
| `reparent_widget` | `(widget_path, widget_name, new_parent_name)` — move a widget to a new parent panel |
| `bind_event` | `(widget_path, widget_name, event_name, function_name)` |
| `get_hierarchy` / `list_components` | All widgets as `WidgetInfo` list |
| `get_widget_snapshot` | Full hierarchy + slot + properties as `WidgetComponentSnapshot` list |
| `get_component_snapshot` | Single-widget snapshot |
| `search_types` | Search available widget type names (built-ins + `[WBP]` customs) |
| `list_widget_blueprints` | List custom WBP asset paths |
| `set_property` / `get_property` | Read/write a single widget property (string value) |
| `list_properties` | All editable properties for a widget |
| `get_available_events` | List valid events (`WidgetEventInfo`) for a widget |
| `set_font` / `get_font` | `(..., font_info, property_name="Font")` / `(..., property_name="Font")` |
| `set_brush` / `get_brush` | `(widget_path, component_name, slot_name, brush_info)` / `(widget_path, component_name, slot_name)` |
| `create_animation` | `(widget_path, animation_name, duration=1.0)` |
| `add_animation_track` | `(widget_path, animation_name, component_name, property_name)` |
| `add_keyframe` | `(widget_path, animation_name, component_name, property_name, keyframe)` |
| `list_animations` / `remove_animation` | Manage widget animations |
| `capture_preview` | `(widget_path, width=1920, height=1080)` → WidgetPreviewResult (no path arg) |
| `is_pie_running` / `start_pie` / `stop_pie` | PIE lifecycle (`start_pie` is async) |
| `spawn_widget_in_pie` | `(widget_path, z_order=0)` → PIEWidgetHandle |
| `get_live_property` | `(handle, component_name, property_name)` → str |
| `remove_widget_from_pie` | `(handle)` → bool |
| `add_view_model` / `list_view_models` / `remove_view_model` | MVVM ViewModels (see `mvvm.md`) |
| `add_view_model_binding` / `list_view_model_bindings` / `remove_view_model_binding` | MVVM bindings |

## Event names

- `OnClicked` — Button clicked
- `OnPressed` / `OnReleased` — Press/release
- `OnHovered` / `OnUnhovered` — Mouse enter/leave
