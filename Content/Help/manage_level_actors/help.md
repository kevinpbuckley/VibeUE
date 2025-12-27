# manage_level_actors

Manage actors in the current level - spawn, transform, query, modify, and organize actors in the scene.

## Summary

The `manage_level_actors` tool provides comprehensive actor management for level editing. It allows you to add and remove actors, transform them, query their properties, organize with folders, and manage parent-child attachments.

## Actions

| Action | Description |
|--------|-------------|
| help | Get help information about level actor management |
| add | Spawn a new actor in the level |
| remove | Remove an actor from the level |
| list | List all actors in the level |
| find | Find actors by name or class |
| get_info | Get detailed information about an actor |
| set_transform | Set the full transform of an actor |
| get_transform | Get the current transform of an actor |
| set_location | Set only the location of an actor |
| set_rotation | Set only the rotation of an actor |
| set_scale | Set only the scale of an actor |
| focus | Focus the viewport camera on an actor |
| move_to_view | Move an actor to the current camera view |
| refresh_viewport | Refresh the viewport to show changes |
| get_property | Get a property value from an actor |
| set_property | Set a property value on an actor |
| get_all_properties | Get all properties of an actor |
| set_folder | Set the folder path for organizing actors |
| attach | Attach an actor to another actor |
| detach | Detach an actor from its parent |
| select | Select actors in the editor |
| rename | Rename an actor |

## Usage

### Add Actor to Level
```json
{
  "Action": "add",
  "ParamsJson": "{\"Class\": \"PointLight\", \"Location\": {\"X\": 0, \"Y\": 0, \"Z\": 200}}"
}
```

### Find Actors
```json
{
  "Action": "find",
  "ParamsJson": "{\"NamePattern\": \"*Light*\"}"
}
```

### Set Actor Location
```json
{
  "Action": "set_location",
  "ParamsJson": "{\"ActorName\": \"PointLight_0\", \"Location\": {\"X\": 100, \"Y\": 200, \"Z\": 300}}"
}
```

## Common Patterns

### Spawning Actors
```python
# Add a native actor to the level (use actor_class for engine types)
manage_level_actors(action="add", actor_class="/Script/Engine.SpotLight",
                   location=[100, 200, 0], actor_label="MySpotLight")

# Add a Blueprint actor (use actor_class with asset path)
manage_level_actors(action="add", actor_class="/Game/Blueprints/BP_Enemy",
                   location=[100, 200, 0], actor_label="Enemy_1")
```

### move_to_view vs focus - Understanding the Difference
```python
# move_to_view: Moves the ACTOR to the current viewport camera location
manage_level_actors(action="move_to_view", actor_label="SpotLight")

# focus: Moves the VIEWPORT to center on the actor (opposite of move_to_view)
manage_level_actors(action="focus", actor_label="SpotLight")

# When user says "move to my viewport" or "place at my view" → use move_to_view
# When user says "focus on" or "look at" → use focus
```

### Attaching Actors
```python
# Attach actors (e.g., attach weapon to character)
# IMPORTANT: Use child_label and parent_label (NOT actor_label, NOT parent_actor_label)
manage_level_actors(action="attach", child_label="Sword", parent_label="Knight")

# Attach to a specific socket
manage_level_actors(action="attach", child_label="Sword", 
                   parent_label="Knight", socket_name="hand_r")

# Detach an actor from its parent
manage_level_actors(action="detach", actor_label="Sword")
```

### Organizing into Folders
```python
# Organize actors into folders (folders are created automatically)
# IMPORTANT: Use folder_path (NOT folder_name), action is set_folder (NOT create_folder)
manage_level_actors(action="set_folder", actor_label="Enemy_1", folder_path="Enemies")
manage_level_actors(action="set_folder", actor_label="Enemy_2", folder_path="Enemies/Bosses")
```

## Color Property Format (FColor)

Level actors use FColor with 0-255 byte values:
```python
# ✅ CORRECT - Level actors use string format
manage_level_actors(action="set_property", actor_label="MyLight",
                   property_path="LightComponent0.LightColor", 
                   property_value="(R=0,G=255,B=0,A=255)")  # Green

# ❌ WRONG - These formats will fail for level actors:
# property_value="[0, 255, 0]"      # String array - WRONG
# property_value="0.0, 1.0, 0.0"    # Float format - WRONG  
# property_value="FColor(0,255,0)"  # Constructor format - WRONG

# ⚠️ NOTE: FColor uses 0-255 byte values (NOT 0-1 normalized!)
```

## Common Mistakes to Avoid

**Attaching Actors:**
- ❌ WRONG: `manage_level_actors(action="attach", actor_label="Sword", parent_actor_label="Knight")` - wrong param names!
- ✅ CORRECT: `manage_level_actors(action="attach", child_label="Sword", parent_label="Knight")` - use child_label and parent_label

**Organizing into Folders:**
- ❌ WRONG: `manage_level_actors(action="create_folder", ...)` - action doesn't exist!
- ❌ WRONG: `manage_level_actors(action="set_folder", folder_name="Enemies", ...)` - wrong param name!
- ✅ CORRECT: `manage_level_actors(action="set_folder", actor_label="Enemy_1", folder_path="Enemies")` - folders auto-create

## Notes

- Actor names must be unique in the level
- Transform operations use world coordinates by default
- Use folders to organize complex levels
- Always save the level after making changes
