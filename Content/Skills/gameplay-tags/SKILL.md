---
name: gameplay-tags
display_name: Gameplay Tags
description: Create, list, remove, and rename Gameplay Tags via the editor module with runtime registration
vibeue_classes:
  - GameplayTagService
unreal_classes:
  - UGameplayTagsManager
  - FGameplayTag
  - FGameplayTagContainer
keywords:
  - gameplay tag
  - gameplay tags
  - tag
  - event tag
  - gameplay event
  - tag hierarchy
---

# Gameplay Tags Skill

Manage Unreal Engine Gameplay Tags programmatically via `unreal.GameplayTagService`.
Tags are written to INI config **and** registered at runtime — they appear immediately in the editor tag picker without restart.

## Critical Rules

### ⚠️ Do NOT Use ProjectSettingsService for Gameplay Tags

`ProjectSettingsService.set_ini_value()` writes to GConfig memory but does **NOT** register tags with `UGameplayTagsManager`. Tags created this way will not appear in tag pickers or dropdowns.

**Always use `GameplayTagService`** for gameplay tag operations.

### ⚠️ Tag Names Use Dot Hierarchy

Tags use dot-separated hierarchy: `Category.Subcategory.TagName`

```python
# ✅ CORRECT
unreal.GameplayTagService.add_tag("Cube.StartChasing")
unreal.GameplayTagService.add_tag("Ability.Fireball.Cast")

# ❌ WRONG - don't use spaces or special characters
unreal.GameplayTagService.add_tag("Cube Start Chasing")
```

### ⚠️ Default Source is DefaultGameplayTags.ini

Tags default to `DefaultGameplayTags.ini` source. This is the standard project-level tag source. You can specify a different source if needed, but the default is correct for most cases.

### ⚠️ Check Results

```python
result = unreal.GameplayTagService.add_tag("Cube.StartChasing", "Event to start chasing")
if not result.success:
    print(f"Failed: {result.error_message}")
```

---

## Workflows

### Add Tags for StateTree Events

When a StateTree transition needs `OnEvent` trigger, create the event tags first:

```python
import unreal

# 1. Create the gameplay tags
result = unreal.GameplayTagService.add_tags(
    ["Cube.StartChasing", "Cube.StopChasing"],
    "StateTree chase events"
)
print(f"Added {len(result.tags_modified)} tags: {result.tags_modified}")

# 2. Now use them in StateTree transitions (via StateTreeService)
```

### Add a Single Tag

```python
import unreal

result = unreal.GameplayTagService.add_tag(
    "Ability.Fireball.Cast",
    "Triggered when player casts fireball"
)
if result.success:
    print(f"Tag added: {result.tags_modified[0]}")
```

### List All Tags (with Filter)

```python
import unreal

# All tags
all_tags = unreal.GameplayTagService.list_tags()
for t in all_tags:
    print(f"{t.tag_name} (source={t.source}, children={t.child_count})")

# Only tags starting with "Cube"
cube_tags = unreal.GameplayTagService.list_tags("Cube")
```

### Check Tag Existence Before Use

```python
import unreal

tag_name = "Cube.StartChasing"
if unreal.GameplayTagService.has_tag(tag_name):
    print(f"Tag '{tag_name}' exists")
else:
    # Create it
    unreal.GameplayTagService.add_tag(tag_name)
```

### Inspect Tag Hierarchy

```python
import unreal

# Get children of root "Cube" tag
children = unreal.GameplayTagService.get_children("Cube")
for child in children:
    print(f"  {child.tag_name} (explicit={child.is_explicit})")
```

### Rename a Tag

```python
import unreal

result = unreal.GameplayTagService.rename_tag("Cube.StartChasing", "Cube.BeginChase")
if result.success:
    print(f"Renamed: {result.tags_modified}")
```

### Remove a Tag

```python
import unreal

result = unreal.GameplayTagService.remove_tag("Cube.StopChasing")
if not result.success:
    print(f"Cannot remove: {result.error_message}")
```

---

## API Reference

| Method | Returns | Description |
|--------|---------|-------------|
| `list_tags(filter="")` | `[FGameplayTagInfo]` | List tags, optionally filtered by prefix |
| `has_tag(tag_name)` | `bool` | Check if tag exists |
| `get_tag_info(tag_name)` | `bool, FGameplayTagInfo` | Get detailed tag info |
| `get_children(parent_tag)` | `[FGameplayTagInfo]` | Get direct children of a tag |
| `add_tag(tag_name, comment, source)` | `FGameplayTagResult` | Add a single tag |
| `add_tags(tag_names, comment, source)` | `FGameplayTagResult` | Add multiple tags |
| `remove_tag(tag_name)` | `FGameplayTagResult` | Remove a tag |
| `rename_tag(old_name, new_name)` | `FGameplayTagResult` | Rename a tag |

### FGameplayTagInfo Fields

| Field | Type | Description |
|-------|------|-------------|
| `tag_name` | `str` | Full tag name |
| `comment` | `str` | Developer comment |
| `source` | `str` | Where tag was defined |
| `is_explicit` | `bool` | Explicitly defined vs implied parent |
| `child_count` | `int` | Number of direct children |
