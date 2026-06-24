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

Manage Unreal Engine Gameplay Tags. Core CRUD (add / remove / rename / list) is now owned by
Unreal 5.8's native **`GameplayTagsToolset`** (call it via `call_tool`). VibeUE keeps only the
delta the engine toolset does NOT provide — runtime hierarchy queries and bulk registration:
`unreal.GameplayTagService.has_tag` / `get_tag_info` / `get_children` / `add_tags`.

Tags are written to INI config **and** registered at runtime — they appear immediately in the editor tag picker without restart.

## Critical Rules

### 🔀 Where each operation lives

| Operation | Use |
|-----------|-----|
| Add / remove / rename a single tag, list all tags | engine **`GameplayTagsToolset`** via `call_tool` (run `describe_toolset` for its action names/params) |
| Bulk-register many tags at once | `unreal.GameplayTagService.add_tags([...], comment, source)` |
| Check existence | `unreal.GameplayTagService.has_tag(name)` |
| Detailed info (comment/source/redirect/child count) | `unreal.GameplayTagService.get_tag_info(name)` |
| Direct children of a tag | `unreal.GameplayTagService.get_children(parent)` |

> The single-tag `add_tag` / `remove_tag` / `rename_tag` / `list_tags` helpers were removed from
> `GameplayTagService` in the 5.8 consolidation — use the engine `GameplayTagsToolset` for those.
> VibeUE's `add_tags` remains for registering several tags in one call.

### ⚠️ Do NOT Use ProjectSettingsService for Gameplay Tags

`ProjectSettingsService.set_ini_value()` writes to GConfig memory but does **NOT** register tags with `UGameplayTagsManager`. Tags created this way will not appear in tag pickers or dropdowns.

Use the engine **`GameplayTagsToolset`** (single-tag CRUD) or `unreal.GameplayTagService.add_tags` (bulk) for gameplay tag operations.

### ⚠️ Tag Names Use Dot Hierarchy

Tags use dot-separated hierarchy: `Category.Subcategory.TagName`

```python
# ✅ CORRECT
unreal.GameplayTagService.add_tags(["Cube.StartChasing"])
unreal.GameplayTagService.add_tags(["Ability.Fireball.Cast"])

# ❌ WRONG - don't use spaces or special characters
unreal.GameplayTagService.add_tags(["Cube Start Chasing"])
```

### ⚠️ Default Source is DefaultGameplayTags.ini

Tags default to `DefaultGameplayTags.ini` source. This is the standard project-level tag source. You can specify a different source if needed, but the default is correct for most cases.

### ⚠️ Check Results

```python
result = unreal.GameplayTagService.add_tags(["Cube.StartChasing"], "Event to start chasing")
if not result.success:
    print(f"Failed: {result.error_message}")
```

### ⚠️ Renames Register a Redirect — has_tag(old_name) Stays True

Renaming a tag (via the engine `GameplayTagsToolset`) updates the INI **and registers a tag
redirect** so existing assets keep resolving the old name. Consequences:

- `has_tag(old_name)` returns **True** after a rename — that is the redirect, not a failed rename.
- `get_tag_info(old_name)` describes the redirect **target**; check its `redirected_to` field
  (non-empty = the requested name is a redirect, the value is the new canonical name).
- Verify a rename with the engine toolset's list action or `get_children(parent)` — the old name
  will be gone from those listings — or check `get_tag_info(old_name).redirected_to`.

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

Single-tag add lives in the engine **`GameplayTagsToolset`** — call it with `call_tool`
(run `describe_toolset` on it for the exact action name and parameters). For one-or-many in a
single call from Python, `add_tags` also accepts a single-element list:

```python
import unreal

result = unreal.GameplayTagService.add_tags(
    ["Ability.Fireball.Cast"],
    "Triggered when player casts fireball"
)
if result.success:
    print(f"Tag added: {result.tags_modified[0]}")
```

### List All Tags (with Filter)

Listing/filtering all tags is an engine `GameplayTagsToolset` action (via `call_tool`). To walk
a specific subtree from Python, use `get_children`:

```python
import unreal

# Direct children under "Cube"
for t in unreal.GameplayTagService.get_children("Cube"):
    print(f"{t.tag_name} (source={t.source}, children={t.child_count})")
```

### Check Tag Existence Before Use

```python
import unreal

tag_name = "Cube.StartChasing"
if unreal.GameplayTagService.has_tag(tag_name):
    print(f"Tag '{tag_name}' exists")
else:
    # Register it (bulk-capable VibeUE helper; or use the engine GameplayTagsToolset)
    unreal.GameplayTagService.add_tags([tag_name])
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

Renaming is an engine **`GameplayTagsToolset`** action — call it via `call_tool`. Then verify the
redirect from Python (the old name still resolves through the redirect, so `has_tag` stays True):

```python
import unreal

# (rename Cube.StartChasing -> Cube.BeginChase via the engine GameplayTagsToolset, then:)
info = unreal.GameplayTagService.get_tag_info("Cube.StartChasing")
print(f"Old name now redirects to: {info.redirected_to}")  # 'Cube.BeginChase'
```

### Remove a Tag

Removing a single tag is an engine **`GameplayTagsToolset`** action (via `call_tool`). Confirm it
is gone from Python with `has_tag` / `get_children` on the parent.

---

## API Reference

VibeUE delta (call as `unreal.GameplayTagService.<method>` via `execute_python_code`):

| Method | Returns | Description |
|--------|---------|-------------|
| `has_tag(tag_name)` | `bool` | Check if tag exists (also True for redirected old names of renamed tags) |
| `get_tag_info(tag_name)` | `FGameplayTagInfo` or `None` | Get detailed tag info (struct or None — NOT a tuple) |
| `get_children(parent_tag)` | `[FGameplayTagInfo]` | Get direct children of a tag |
| `add_tags(tag_names, comment, source)` | `FGameplayTagResult` | Bulk-register multiple tags (also works for one) |

Single-tag **add / remove / rename** and **list-all** are provided by Unreal 5.8's native
**`GameplayTagsToolset`** — reach it with `call_tool`; run `describe_toolset` for its actions.

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

- **`scripts/add_tags.txt`** — register gameplay tags and list them.
