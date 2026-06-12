---
name: gameplay-tags
display_name: Gameplay Tags
description: Create, list, remove, and rename Gameplay Tags with runtime registration (GameplayTagService). Use when the user asks to add or register gameplay tags, list/query existing tags, or rename/remove tags (e.g. Ability.Attack.Melee, State.Stunned).
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

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "gameplay-tags"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

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

### ⚠️ Renames Register a Redirect — has_tag(old_name) Stays True

`rename_tag` updates the INI **and registers a tag redirect** so existing assets keep
resolving the old name. Consequences:

- `has_tag(old_name)` returns **True** after a rename — that is the redirect, not a failed rename.
- `get_tag_info(old_name)` describes the redirect **target**; check its `redirected_to` field
  (non-empty = the requested name is a redirect, the value is the new canonical name).
- Verify a rename with `list_tags(prefix)` or `get_children(parent)` — the old name will be
  gone from those listings — or check `get_tag_info(old_name).redirected_to`.

### ⚠️ get_tag_info Returns a Struct or None — NOT a Tuple

```python
# ❌ WRONG — raises "cannot unpack non-iterable GameplayTagInfo"
found, info = unreal.GameplayTagService.get_tag_info("Cube.StartChasing")

# ✅ CORRECT
info = unreal.GameplayTagService.get_tag_info("Cube.StartChasing")
if info is not None:
    print(info.tag_name, info.comment)
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

# Verify via redirect info — NOT via has_tag (the old name still resolves through a redirect)
info = unreal.GameplayTagService.get_tag_info("Cube.StartChasing")
print(f"Old name now redirects to: {info.redirected_to}")  # 'Cube.BeginChase'
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
| `has_tag(tag_name)` | `bool` | Check if tag exists (also True for redirected old names of renamed tags) |
| `get_tag_info(tag_name)` | `FGameplayTagInfo` or `None` | Get detailed tag info (struct or None — NOT a tuple) |
| `get_children(parent_tag)` | `[FGameplayTagInfo]` | Get direct children of a tag |
| `add_tag(tag_name, comment, source)` | `FGameplayTagResult` | Add a single tag |
| `add_tags(tag_names, comment, source)` | `FGameplayTagResult` | Add multiple tags |
| `remove_tag(tag_name)` | `FGameplayTagResult` | Remove a tag |
| `rename_tag(old_name, new_name)` | `FGameplayTagResult` | Rename a tag |

### FGameplayTagInfo Fields

| Field | Type | Description |
|-------|------|-------------|
| `tag_name` | `str` | Full tag name (as requested) |
| `redirected_to` | `str` | If the requested name was renamed, the new canonical tag it redirects to (empty if none). Other fields describe the redirect target. |
| `comment` | `str` | Developer comment |
| `source` | `str` | Where tag was defined |
| `is_explicit` | `bool` | Explicitly defined vs implied parent |
| `child_count` | `int` | Number of direct children |

## Sample scripts (run via `execute_python_code`)

- **`scripts/add_tags.pyx`** — register gameplay tags and list them.
