# Level Actor Management Guide

## Overview

The `manage_level_actors` tool provides comprehensive control over actors in the current level. It consolidates 18 actions across 4 phases: discovery, creation/deletion, transforms, properties, and hierarchy/organization.

## Quick Reference

- **Discovery**: `get_help(topic="level-actors")`
- **Test Prompts**: See `test_prompts/level_actors/level_actor_tests.md`

## Tool Design

The tool uses a **compact parameter pattern** with core params + `extra` dict:
- **10 core parameters**: action, actor_label, actor_path, location, rotation, scale, property_path, property_value, extra
- **Action-specific params**: Passed via `extra` dict

## Available Actions by Phase

### Phase 1 - Discovery & Lifecycle

| Action | Purpose |
|--------|---------|
| `list` | List actors with optional filtering |
| `find` | Find actors by tags or criteria |
| `get_info` | Get comprehensive actor information |
| `add` | Add new actor to level |
| `remove` | Remove actor from level |

### Phase 2 - Transform Operations

| Action | Purpose |
|--------|---------|
| `get_transform` | Get complete transform data |
| `set_transform` | Set location, rotation, scale at once |
| `set_location` | Set actor location only |
| `set_rotation` | Set actor rotation only |
| `set_scale` | Set actor scale only |
| `focus` | Focus viewport camera on actor |
| `move_to_view` | Move actor to viewport center |
| `refresh_viewport` | Force refresh all viewports |

### Phase 3 - Property Operations

| Action | Purpose |
|--------|---------|
| `get_property` | Get single property value |
| `set_property` | Set single property value |
| `get_all_properties` | Get all properties with filtering |

### Phase 4 - Hierarchy & Organization

| Action | Purpose |
|--------|---------|
| `set_folder` | Set World Outliner folder |
| `attach` | Attach actor to parent |
| `detach` | Detach actor from parent |
| `select` | Select/deselect actors |
| `rename` | Rename actor label |

## Common Workflows

### List Lights in Level
```python
manage_level_actors(
    action="list",
    extra={"class_filter": "*Light*", "max_results": 50}
)
```

### Add PointLight
```python
manage_level_actors(
    action="add",
    location=[500, 500, 300],
    extra={
        "actor_class": "PointLight",
        "actor_name": "MainLight",
        "tags": ["Lighting", "Indoor"]
    }
)
```

### Move Actor
```python
manage_level_actors(
    action="set_location",
    actor_label="MainLight",
    location=[1000, 1000, 400]
)
```

### Set Light Color
```python
manage_level_actors(
    action="set_property",
    actor_label="DirectionalLight",
    property_path="LightColor",
    property_value="(R=255,G=180,B=100,A=255)",
    extra={"component_name": "LightComponent0"}
)
```

### Get Light Property
```python
manage_level_actors(
    action="get_property",
    actor_label="DirectionalLight",
    property_path="Intensity",
    extra={"component_name": "LightComponent0"}
)
```

### Organize with Folders
```python
manage_level_actors(
    action="set_folder",
    actor_label="MainLight",
    extra={"folder_path": "Lighting/Indoor"}
)
```

### Rename Actor
```python
manage_level_actors(
    action="rename",
    actor_label="OldName",
    extra={"new_label": "NewName"}
)
```

### Select Multiple Actors
```python
manage_level_actors(
    action="select",
    extra={"actors": ["Actor1", "Actor2"], "add_to_selection": True}
)
```

## Extra Dict Parameters Reference

| Action | Extra Parameters |
|--------|------------------|
| `add` | actor_class, actor_name, tags, spawn_rotation, spawn_scale |
| `list/find` | class_filter, label_filter, required_tags, excluded_tags, selected_only, max_results |
| `get_info` | include_components, include_properties, category_filter |
| `remove` | with_undo |
| `transforms` | world_space, sweep, teleport |
| `focus` | instant |
| `properties` | component_name, include_inherited, category_filter |
| `set_folder` | folder_path |
| `attach` | parent_label, parent_path, socket_name, weld_simulated_bodies |
| `select` | actors, add_to_selection, deselect, deselect_all |
| `rename` | new_label |
| `identification` | actor_guid, actor_tag (alternatives to actor_label/actor_path) |

## Actor Identification Methods

Actors can be identified using any of:
- **actor_label**: Display name in World Outliner (most common)
- **actor_path**: Full path like `/Game/Level.Level:PersistentLevel.ActorName` (most precise)
- **actor_guid**: Unique GUID (via extra dict)
- **actor_tag**: First actor with matching tag (via extra dict)

## Common Actor Classes

| Class | Description |
|-------|-------------|
| `PointLight` | Omni-directional light |
| `SpotLight` | Directional cone light |
| `DirectionalLight` | Sun/sky light |
| `StaticMeshActor` | Static geometry |
| `SkeletalMeshActor` | Animated mesh |
| `CameraActor` | Camera placement |
| `PlayerStart` | Spawn point |
| `TriggerBox` | Collision trigger volume |

## Common Light Properties

| Property | Component | Type | Example |
|----------|-----------|------|---------|
| `LightColor` | LightComponent0 | FColor | `(R=255,G=180,B=100,A=255)` |
| `Intensity` | LightComponent0 | float | `5000.0` |
| `AttenuationRadius` | LightComponent0 | float | `1000.0` |
| `CastShadows` | LightComponent0 | bool | `True` |

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Actor not found | Use `list` to find exact label/path |
| Property not changing | Check component_name in extra dict |
| Transform not applying | Verify world_space setting |
| Timeout errors | Retry the operation (connection hiccup) |

## Related Topics

- `get_help(topic="properties")` - Property format reference
- `get_help(topic="troubleshooting")` - General troubleshooting
- `get_help(topic="overview")` - VibeUE overview
