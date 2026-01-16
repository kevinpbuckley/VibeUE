# VibeUE Skills System - Summary

## Overview

The Skills System reduces VibeUE's initial context from ~13,000 tokens to ~2,500 tokens by lazy-loading domain-specific knowledge on-demand.

## Key Design Decision: skill.md Instead of JSON

✅ **Each skill directory contains a `skill.md` file with YAML frontmatter** (instead of a centralized JSON manifest)

### Why This Approach?

- **Self-contained**: Each skill is fully self-documenting
- **Maintainable**: Add new skills by creating a folder with skill.md
- **Version control friendly**: Skills can be modified independently
- **No JSON parsing complexity**: Just YAML frontmatter + markdown

---

## Directory Structure

```
Content/Skills/
├── blueprints/
│   ├── skill.md                    # Metadata + overview (REQUIRED)
│   ├── 01-service-reference.md     # BlueprintService API
│   ├── 02-workflows.md             # Common patterns
│   └── 03-common-mistakes.md       # Critical rules
├── materials/
│   ├── skill.md                    # Metadata + overview (REQUIRED)
│   └── [content files...]
└── [other skills...]
```

---

## skill.md Format

Each skill requires a `skill.md` file with:

### YAML Frontmatter (Metadata)
```yaml
---
name: blueprints                    # Directory-friendly name
display_name: Blueprint System      # Human-readable name
description: Create and modify Blueprint assets...
services:
  - BlueprintService
keywords:
  - blueprint
  - variable
  - function
auto_load_keywords:
  - blueprint
  - BP_
  - function
---
```

### Markdown Content (Overview)
- What's included in the skill
- When to use this skill
- Core services
- Quick examples
- Related skills

---

## MCP Tool: `manage_skills`

### Actions

#### 1. `list`
Returns metadata for all available skills.

**Implementation:**
1. Scan `Content/Skills/` subdirectories
2. Read each `skill.md` and parse YAML frontmatter
3. Count markdown files in directory
4. Return array of skill metadata

**Example Response:**
```json
{
  "skills": [
    {
      "name": "blueprints",
      "display_name": "Blueprint System",
      "description": "Create and modify Blueprint assets...",
      "services": ["BlueprintService"],
      "keywords": [...],
      "auto_load_keywords": [...],
      "file_count": 4,
      "estimated_tokens": 3500
    }
  ]
}
```

#### 2. `load`
Loads all markdown content from a skill directory.

**Parameters:**
- `skill_name`: Supports **three formats**:
  - Directory name: `"blueprints"`
  - `name` field: `"blueprints"`
  - `display_name` field: `"Blueprint System"` (case-insensitive)

**Implementation:**
1. **Resolve skill name to directory:**
   - Try as directory name first
   - If not found, scan all skill.md files for `name` match
   - If not found, scan for case-insensitive `display_name` match
2. Load all `*.md` files in the directory
3. Concatenate in alphabetical order (skill.md first)
4. Return content with file list

**Example Response:**
```json
{
  "skill_name": "blueprints",
  "content": "[concatenated markdown content]",
  "files_loaded": [
    "blueprints/skill.md",
    "blueprints/01-service-reference.md",
    "blueprints/02-workflows.md",
    "blueprints/03-common-mistakes.md"
  ],
  "token_count": 3472
}
```

**Supported Usage:**
```python
# All of these work:
manage_skills(action="load", skill_name="blueprints")
manage_skills(action="load", skill_name="Blueprint System")
manage_skills(action="load", skill_name="blueprint system")
```

---

## AI Behavior

### Automatic Loading
The AI automatically loads skills when:
- User mentions auto_load_keywords (e.g., "BP_", "create a blueprint")
- User references specific asset types
- Domain is clear from context

### Manual Discovery
The AI calls `list` when:
- User query is ambiguous
- First message in session (optional)
- Exploring available capabilities

### Multi-Skill Operations
The AI loads multiple skills when tasks span domains:
```
User: "Create BP_Player with material parameters"
→ Load "blueprints" skill
→ Load "materials" skill
```

---

## Benefits

### Token Efficiency
- **Before**: 13k baseline every conversation
- **After**: 2.5k baseline + skills on-demand
- **Savings**: 50-80% depending on task

### Maintainability
- Add skills without affecting base instructions
- Update skills independently
- Version skills separately

### Scalability
- No central JSON to update
- Self-documenting structure
- Easy for contributors to add skills

---

## Files Created

### Design Documents ✅
- [Skills-System-Design.md](Skills-System-Design.md) - Complete specification
- [Skills-Implementation-Plan.md](Skills-Implementation-Plan.md) - Step-by-step implementation guide
- [Skills-System-Summary.md](Skills-System-Summary.md) - This file

### Core Files ✅
- [vibeue.instructions.v2.md](../instructions/vibeue.instructions.v2.md) - New condensed base instructions (~2.5k tokens)

### Skill Metadata (All 8 Skills) ✅
- [blueprints/skill.md](../Skills/blueprints/skill.md)
- [materials/skill.md](../Skills/materials/skill.md)
- [enhanced-input/skill.md](../Skills/enhanced-input/skill.md)
- [data-tables/skill.md](../Skills/data-tables/skill.md)
- [data-assets/skill.md](../Skills/data-assets/skill.md)
- [umg-widgets/skill.md](../Skills/umg-widgets/skill.md)
- [level-actors/skill.md](../Skills/level-actors/skill.md)
- [asset-management/skill.md](../Skills/asset-management/skill.md)

### Example Skill Content ✅
- [blueprints/01-service-reference.md](../Skills/blueprints/01-service-reference.md) - Complete API reference
- [blueprints/02-workflows.md](../Skills/blueprints/02-workflows.md) - Common patterns
- [blueprints/03-common-mistakes.md](../Skills/blueprints/03-common-mistakes.md) - Critical rules

---

## Implementation Status

### Phase 1: Infrastructure (C++/Python)
- [ ] Implement `manage_skills` MCP tool handler
- [ ] Add skill name resolution logic (directory, name, display_name)
- [ ] Parse YAML frontmatter from skill.md files
- [ ] Register tool with MCP server
- [ ] Write unit tests

### Phase 2: Content Migration
- [x] Create directory structure
- [x] Create skill.md for all 8 skills
- [x] Create new base instructions (vibeue.instructions.v2.md)
- [x] Create complete blueprints skill (example)

### Phase 3: Content Creation
- [ ] Split remaining content from old instructions
- [ ] Create markdown files for 7 remaining skills
- [ ] Add cross-references between skills

### Phase 4: Testing
- [ ] Test with existing test_prompts
- [ ] Validate token usage improvements
- [ ] Verify skill auto-loading behavior

### Phase 5: Deployment
- [ ] Switch to new instructions file
- [ ] Update documentation
- [ ] Create skill authoring guide

---

## Next Steps

1. **Implement C++ tool handler** with skill name resolution
2. **Split content** from current instructions into skill files
3. **Test** with existing test prompts
4. **Deploy** and monitor

---

## Example Tool Interactions

### List All Skills
```json
{
  "tool": "manage_skills",
  "arguments": { "action": "list" }
}
```

### Load by Directory Name
```json
{
  "tool": "manage_skills",
  "arguments": {
    "action": "load",
    "skill_name": "blueprints"
  }
}
```

### Load by Display Name
```json
{
  "tool": "manage_skills",
  "arguments": {
    "action": "load",
    "skill_name": "Blueprint System"
  }
}
```

All three approaches return the same skill content!
