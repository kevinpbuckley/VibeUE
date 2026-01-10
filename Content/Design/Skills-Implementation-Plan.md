# VibeUE Skills System - Implementation Plan

## Overview

This document outlines the step-by-step implementation plan for the VibeUE Skills system, which reduces initial context from ~13k tokens to ~2.5k tokens while maintaining full feature coverage through lazy-loaded domain-specific knowledge.

---

## Implementation Phases

### Phase 1: Core Infrastructure (C++/Python)

#### 1.1 Create MCP Tool Handler

**File:** `Plugins/VibeUE/Source/VibeUE/Private/MCP/SkillsToolHandler.cpp`

**Functionality:**
- Implement `manage_skills` tool with two actions: `list` and `load`
- Scan `Content/Skills/` directories and read `skill.md` YAML frontmatter
- Load and concatenate markdown files from skill directories
- Support skill name resolution (directory name, `name` field, or `display_name` field)
- Return formatted JSON responses

**Key Methods:**
```cpp
class FSkillsToolHandler
{
public:
    FString HandleListAction();
    FString HandleLoadAction(const FString& SkillName);

private:
    TArray<FSkillInfo> ScanSkillDirectories();
    FSkillInfo ParseSkillMetadata(const FString& SkillMdPath);
    FString ResolveSkillDirectory(const FString& SkillName);
    FString LoadSkillContent(const FString& SkillDirectory);
    TArray<FString> GetMarkdownFilesInSkillDir(const FString& SkillDirectory);
};
```

**Skill Name Resolution:**
1. Try as directory name: `Content/Skills/{SkillName}/`
2. If not found, scan all `skill.md` files for exact `name` match
3. If not found, scan for case-insensitive `display_name` match

#### 1.2 Register Tool with MCP Server

**File:** `Plugins/VibeUE/Source/VibeUE/Private/MCP/MCPToolRegistry.cpp`

Add `manage_skills` to the tool registry:
```cpp
{
    "name": "manage_skills",
    "description": "Discover and load domain-specific knowledge skills",
    "inputSchema": {
        "type": "object",
        "properties": {
            "action": {
                "type": "string",
                "enum": ["list", "load"],
                "description": "Action to perform"
            },
            "skill_name": {
                "type": "string",
                "description": "Name of skill to load (required for 'load' action)"
            }
        },
        "required": ["action"]
    }
}
```

#### 1.3 Create Unit Tests

**File:** `Plugins/VibeUE/Source/VibeUE/Tests/SkillsToolTests.cpp`

Test cases:
- List action returns all skills from manifest
- Load action returns correct markdown content
- Load non-existent skill returns error
- Manifest parsing handles malformed JSON
- Skill directories with no markdown files

---

### Phase 2: Content Migration

#### 2.1 Create Directory Structure

```bash
mkdir -p Content/Skills/blueprints
mkdir -p Content/Skills/materials
mkdir -p Content/Skills/enhanced-input
mkdir -p Content/Skills/data-tables
mkdir -p Content/Skills/data-assets
mkdir -p Content/Skills/umg-widgets
mkdir -p Content/Skills/level-actors
mkdir -p Content/Skills/asset-management
```

#### 2.2 Split Current Instructions

**Source:** `Content/instructions/vibeue.instructions.md` (current)

**Targets:**

| Current Section | Target File | Tokens |
|----------------|-------------|--------|
| BlueprintService API | `blueprints/01-service-reference.md` | ~2500 |
| Blueprint Workflows | `blueprints/02-workflows.md` | ~800 |
| Blueprint Rules | `blueprints/03-common-mistakes.md` | ~200 |
| MaterialService API | `materials/01-material-service.md` | ~800 |
| MaterialNodeService API | `materials/02-material-node-service.md` | ~900 |
| Material Workflows | `materials/03-workflows.md` | ~500 |
| InputService API | `enhanced-input/01-service-reference.md` | ~1000 |
| InputService Return Types | `enhanced-input/02-return-types.md` | ~600 |
| Input Workflows | `enhanced-input/03-workflows.md` | ~200 |
| DataTableService API | `data-tables/01-service-reference.md` | ~800 |
| DataTable JSON Format | `data-tables/02-json-guide.md` | ~500 |
| DataTable Workflows | `data-tables/03-workflows.md` | ~200 |
| DataAssetService API | `data-assets/01-service-reference.md` | ~700 |
| DataAsset Properties | `data-assets/02-property-formats.md` | ~300 |
| DataAsset Workflows | `data-assets/03-workflows.md` | ~200 |
| WidgetService API | `umg-widgets/01-service-reference.md` | ~600 |
| Widget Workflows | `umg-widgets/02-workflows.md` | ~400 |
| Level Actor Workflows | `level-actors/01-workflows.md` | ~500 |
| Editor Subsystems | `level-actors/02-subsystems.md` | ~300 |
| AssetDiscoveryService API | `asset-management/01-service-reference.md` | ~800 |
| Asset Workflows | `asset-management/02-workflows.md` | ~400 |

**Total skill content:** ~11,500 tokens (split across 8 skills)
**New base instructions:** ~2,500 tokens

#### 2.3 Create Manifest File

**File:** `Content/Skills/skills-manifest.json`

Status: ✅ **COMPLETE** (already created)

#### 2.4 Create Base Instructions v2

**File:** `Content/instructions/vibeue.instructions.v2.md`

Status: ✅ **COMPLETE** (already created)

Contents:
- MCP tools overview (7 tools including manage_skills)
- Skills system introduction
- Skill loading patterns
- Core workflow (Discover → Check → Execute)
- Python basics
- Discovery tools
- Critical rules (global only)
- Communication style

---

### Phase 3: Content Creation

#### 3.1 Blueprints Skill

Status: ✅ **COMPLETE** (example created)

Files:
- ✅ `01-service-reference.md` - Full BlueprintService API
- ✅ `02-workflows.md` - Common blueprint patterns
- ✅ `03-common-mistakes.md` - Critical rules

#### 3.2 Materials Skill

Create:
- `01-material-service.md` - MaterialService API
- `02-material-node-service.md` - MaterialNodeService API
- `03-workflows.md` - Material creation and graph building

#### 3.3 Enhanced Input Skill

Create:
- `01-service-reference.md` - InputService API
- `02-return-types.md` - Complex return type documentation
- `03-workflows.md` - Input action and mapping context patterns

#### 3.4 Data Tables Skill

Create:
- `01-service-reference.md` - DataTableService API
- `02-json-guide.md` - JSON data format examples
- `03-workflows.md` - Row operations and bulk updates

#### 3.5 Data Assets Skill

Create:
- `01-service-reference.md` - DataAssetService API
- `02-property-formats.md` - Complex property formatting
- `03-workflows.md` - Asset creation and property management

#### 3.6 UMG Widgets Skill

Create:
- `01-service-reference.md` - WidgetService API
- `02-workflows.md` - Widget creation and component management

#### 3.7 Level Actors Skill

Create:
- `01-workflows.md` - Actor manipulation patterns
- `02-subsystems.md` - Editor subsystem reference

#### 3.8 Asset Management Skill

Create:
- `01-service-reference.md` - AssetDiscoveryService API
- `02-workflows.md` - Search, import, export, save patterns

---

### Phase 4: Testing & Validation

#### 4.1 Test with Existing Test Prompts

Run all existing test prompts with new system:
- `test_prompts/blueprint/*.md`
- `test_prompts/materials/*.md`
- `test_prompts/enhanced_input/*.md`
- `test_prompts/data_table/*.md`
- `test_prompts/data_asset/*.md`
- `test_prompts/umg/*.md`
- `test_prompts/level_actors/*.md`

**Success criteria:**
- All tests pass with same or better accuracy
- AI loads correct skills automatically
- Token usage is reduced

#### 4.2 Token Usage Analysis

Measure token usage for common scenarios:

| Scenario | Old (v1) | New (v2) | Savings |
|----------|----------|----------|---------|
| Blueprint-only work | 13k + task | 2.5k + 2.5k + task | ~62% |
| Material-only work | 13k + task | 2.5k + 2.2k + task | ~64% |
| Input-only work | 13k + task | 2.5k + 1.8k + task | ~67% |
| Multi-domain (BP + Mat) | 13k + task | 2.5k + 2.5k + 2.2k + task | ~44% |

**Target:** Average 50%+ reduction in context tokens

#### 4.3 Skill Loading Behavior

Test that AI:
- Automatically loads skills based on keywords
- Doesn't reload already-loaded skills
- Loads multiple skills when needed
- Falls back to list action when unsure

#### 4.4 Regression Testing

Verify no functionality loss:
- All service methods still documented
- All workflows still covered
- All critical rules preserved
- Return type documentation complete

---

### Phase 5: Deployment

#### 5.1 Backup Current System

```bash
cp Content/instructions/vibeue.instructions.md Content/instructions/vibeue.instructions.v1.backup.md
```

#### 5.2 Switch to New System

```bash
mv Content/instructions/vibeue.instructions.v2.md Content/instructions/vibeue.instructions.md
```

#### 5.3 Update VibeUE Settings

Add configuration option to enable/disable skills system:
```cpp
UPROPERTY(Config, EditAnywhere, Category="MCP")
bool bUseSkillsSystem = true;
```

#### 5.4 Update Documentation

- Update `README.md` to mention skills system
- Add skills authoring guide
- Document `manage_skills` tool in API reference
- Create migration guide for users on v1

---

## File Checklist

### Created ✅
- [x] `Content/Design/Skills-System-Design.md` - Full design specification
- [x] `Content/Design/Skills-Implementation-Plan.md` - This file
- [x] `Content/instructions/vibeue.instructions.v2.md` - New base instructions
- [x] `Content/Skills/blueprints/skill.md` - Skill metadata with YAML frontmatter
- [x] `Content/Skills/blueprints/01-service-reference.md` - Example skill
- [x] `Content/Skills/blueprints/02-workflows.md` - Example skill
- [x] `Content/Skills/blueprints/03-common-mistakes.md` - Example skill
- [x] `Content/Skills/materials/skill.md` - Skill metadata
- [x] `Content/Skills/enhanced-input/skill.md` - Skill metadata
- [x] `Content/Skills/data-tables/skill.md` - Skill metadata
- [x] `Content/Skills/data-assets/skill.md` - Skill metadata
- [x] `Content/Skills/umg-widgets/skill.md` - Skill metadata
- [x] `Content/Skills/level-actors/skill.md` - Skill metadata
- [x] `Content/Skills/asset-management/skill.md` - Skill metadata

### To Create
- [ ] `Source/.../SkillsToolHandler.cpp` - MCP tool implementation with name resolution
- [ ] `Source/.../SkillsToolTests.cpp` - Unit tests
- [ ] `Content/Skills/materials/01-material-service.md`
- [ ] `Content/Skills/materials/02-material-node-service.md`
- [ ] `Content/Skills/materials/03-workflows.md`
- [ ] `Content/Skills/enhanced-input/01-service-reference.md`
- [ ] `Content/Skills/enhanced-input/02-return-types.md`
- [ ] `Content/Skills/enhanced-input/03-workflows.md`
- [ ] `Content/Skills/data-tables/01-service-reference.md`
- [ ] `Content/Skills/data-tables/02-json-guide.md`
- [ ] `Content/Skills/data-tables/03-workflows.md`
- [ ] `Content/Skills/data-assets/01-service-reference.md`
- [ ] `Content/Skills/data-assets/02-property-formats.md`
- [ ] `Content/Skills/data-assets/03-workflows.md`
- [ ] `Content/Skills/umg-widgets/01-service-reference.md`
- [ ] `Content/Skills/umg-widgets/02-workflows.md`
- [ ] `Content/Skills/level-actors/01-workflows.md`
- [ ] `Content/Skills/level-actors/02-subsystems.md`
- [ ] `Content/Skills/asset-management/01-service-reference.md`
- [ ] `Content/Skills/asset-management/02-workflows.md`
- [ ] `README.md` updates
- [ ] Skill authoring guide

---

## Timeline Estimate

### Phase 1: Infrastructure (C++/Python)
**Effort:** 2-3 days
- MCP tool handler implementation
- Tool registration
- Unit tests

### Phase 2: Content Migration
**Effort:** 1 day
- Directory creation
- Manifest setup (✅ done)
- Base instructions (✅ done)

### Phase 3: Content Creation
**Effort:** 3-4 days
- Split and reorganize existing content
- Write skill-specific markdown files
- Add cross-references and examples

### Phase 4: Testing & Validation
**Effort:** 2-3 days
- Run test_prompts suite
- Token usage analysis
- Behavior validation
- Bug fixes

### Phase 5: Deployment
**Effort:** 1 day
- Backup and switch
- Documentation updates
- Release notes

**Total:** ~10-12 days of development

---

## Success Metrics

1. **Token Efficiency**
   - Target: 50%+ reduction in average conversation tokens
   - Measure: Track token usage across 100 conversations

2. **Feature Coverage**
   - Target: 100% of existing functionality preserved
   - Measure: All test_prompts pass

3. **User Experience**
   - Target: AI loads correct skills 95%+ of the time
   - Measure: Manual review of skill loading behavior

4. **Performance**
   - Target: <100ms to load a skill
   - Measure: Benchmark skill loading time

5. **Maintainability**
   - Target: New skills can be added in <1 hour
   - Measure: Time to create and test a new skill

---

## Risks & Mitigations

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| AI forgets to load skills | High | Medium | Strong auto-load keywords, examples |
| Incorrect skill selection | Medium | Low | Clear descriptions, comprehensive keywords |
| Cross-skill dependencies | Medium | Low | Most skills independent, document dependencies |
| Increased API calls | Low | High | Fast file reads, cache loaded skills |
| Content duplication | Low | Medium | Single source of truth per concept |

---

## Next Steps

1. **Implement C++ MCP tool handler** (Phase 1)
2. **Create remaining skill markdown files** (Phase 3)
3. **Test with existing test_prompts** (Phase 4)
4. **Deploy and monitor** (Phase 5)

---

## Questions & Decisions

### Q: Should skills be versioned?
**Decision:** Not in v1. Add versioning in future if skills diverge significantly.

### Q: Can users create custom skills?
**Decision:** Yes, document skill authoring guidelines for community contributions.

### Q: Should AI proactively suggest skills?
**Decision:** Not in v1. Add as Phase 6 enhancement after monitoring usage patterns.

### Q: How to handle skill updates during development?
**Decision:** AI can hot-reload skills by re-calling load action (files read fresh each time).

---

## Appendix: Example Tool Interaction

### List Skills
```json
// Request
{
  "tool": "manage_skills",
  "arguments": {
    "action": "list"
  }
}

// Response
{
  "skills": [
    {
      "name": "blueprints",
      "display_name": "Blueprint System",
      "description": "Create and modify Blueprint assets...",
      "services": ["BlueprintService"],
      "estimated_tokens": 3500
    },
    // ... more skills
  ]
}
```

### Load Skill
```json
// Request
{
  "tool": "manage_skills",
  "arguments": {
    "action": "load",
    "skill_name": "blueprints"
  }
}

// Response
{
  "skill_name": "blueprints",
  "content": "# BlueprintService API Reference\n\n...",
  "files_loaded": [
    "blueprints/01-service-reference.md",
    "blueprints/02-workflows.md",
    "blueprints/03-common-mistakes.md"
  ],
  "token_count": 3472
}
```
