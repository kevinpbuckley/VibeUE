---
name: level-actors
display_name: Level Actors & Editor Subsystems
description: Manipulate actors in the current level using editor subsystems
services:
  - LevelEditorSubsystem
  - EditorActorSubsystem
  - EditorLevelLibrary
keywords:
  - level
  - actor
  - spawn
  - place
  - viewport
  - world
  - editor subsystem
  - scene
auto_load_keywords:
  - level actor
  - spawn actor
  - place actor
  - editor subsystem
  - level viewport
---

# Level Actors & Editor Subsystems

This skill provides documentation for working with actors in the current level using Unreal Engine's editor subsystems.

## What's Included

- **Editor Subsystems Reference**: LevelEditorSubsystem, EditorActorSubsystem, EditorLevelLibrary
- **Workflows**: Common patterns for actor manipulation

## When to Use This Skill

Load this skill when working with:
- Spawning actors in the level
- Placing Blueprint instances
- Selecting actors
- Modifying actor transforms
- Getting actors in the level
- Editor viewport operations

## Core Subsystems

### LevelEditorSubsystem
Editor-level operations:
- Get current level
- Refresh viewports
- Save current level

### EditorActorSubsystem
Actor manipulation:
- Spawn actors
- Destroy actors
- Get/set actor location
- Select actors
- Duplicate actors

### EditorLevelLibrary
Level utilities:
- Get all level actors
- Load level
- Spawn actor from Blueprint

## Quick Examples

### Spawn Actor in Level
```python
import unreal

# Get subsystem
subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Spawn actor
location = unreal.Vector(0, 0, 0)
rotation = unreal.Rotator(0, 0, 0)
actor = subsys.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)

print(f"Spawned: {actor.get_name()}")
```

### Spawn Blueprint Instance
```python
import unreal

# Load blueprint class
bp_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/BP_Enemy")

# Spawn instance
subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor = subsys.spawn_actor_from_class(bp_class, unreal.Vector(100, 0, 0))
```

### Refresh Viewport
```python
import unreal

subsys = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
subsys.editor_invalidate_viewports()
```

## Related Skills

- **blueprints**: For spawning Blueprint instances
- **asset-management**: For finding Blueprint classes to spawn
