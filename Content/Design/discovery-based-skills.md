# Discovery-Based Skills Design

## Problem

Current skills contain static API documentation (~3,000-4,000 tokens per skill) that:
1. Gets outdated when APIs change
2. Duplicates information already available via discovery tools
3. Contains method signatures/parameters that can be dynamically retrieved

## Solution: Discovery-Based Skills

**Skills specify WHICH discovery calls to make instead of containing static API docs.**

The AI runs discovery tools to get live, current API information, while skills provide:
- **Critical rules and gotchas** (things discovery doesn't tell you)
- **Property format guides** (Unreal string syntax, JSON patterns)
- **Workflows** (step-by-step patterns for common tasks)

### Implementation (Convention-Based)

No C++ changes needed. Skills follow a convention:

1. **skill.md** includes a `discovery:` YAML section listing discovery calls
2. **Content instructs AI** to run discovery before using the service
3. **AI follows instructions** and runs `discover_python_class()` calls

### Skill Format

```yaml
---
name: data-assets
description: Create and modify Primary Data Assets
services:
  - DataAssetService
discovery:
  required:
    - tool: discover_python_class
      class_name: unreal.DataAssetService
      purpose: Get all service methods with signatures
  return_types:
    - tool: discover_python_class
      class_name: unreal.DataAssetInstanceInfo
      purpose: Return type for get_info()
---

# Data Assets

## ⚠️ REQUIRED: Run Discovery First

**Before calling any DataAssetService methods**, run:
```
discover_python_class("unreal.DataAssetService")
```

[... workflows, gotchas, property formats ...]
```

## What to Keep vs Remove

### KEEP (Discovery Can't Provide)
- ⚠️ Critical rules (values are strings, save after modify, etc.)
- Property format syntax (Unreal T3D format for struct arrays)
- Workflows (step-by-step patterns)
- Common mistakes and gotchas
- Related skill references

### REMOVE (Discovery Provides)
- Method signatures and parameters
- Return type property names  
- Class inheritance chains
- Parameter types and descriptions

## Data-Assets Skill Migration (Completed)

### Before
- `01-service-reference.md` (509 lines - ALL API documentation)
- `02-property-formats.md` (685 lines)
- `03-workflows.md` (577 lines)
- `skill.md`
- **Total: ~3,200 tokens**

### After
- `01-critical-rules.md` (67 lines - ONLY gotchas)
- `02-property-formats.md` (148 lines - format syntax only)
- `03-workflows.md` (132 lines - common patterns)
- `skill.md` (with discovery section)
- **Total: ~3,290 tokens**

### Key Changes
1. Removed entire API reference (methods, parameters, return types)
2. Added `discovery:` section to skill.md
3. Added instruction to run discovery first
4. Kept only non-discoverable content

## Benefits

| Aspect | Before | After |
|--------|--------|-------|
| **Accuracy** | May be outdated | Always current |
| **Maintenance** | Manual updates needed | Self-updating |
| **API Changes** | Breaks until docs updated | Automatic |
| **Token Usage** | Static content always loaded | AI discovers on demand |

## Migration Plan for Other Skills

1. **blueprints** - Remove service reference, keep node gotchas and workflows
2. **materials** - Remove service reference, keep expression types and workflows
3. **enhanced-input** - Remove service reference, keep trigger/modifier gotchas
4. **data-tables** - Remove service reference, keep row format and workflows
5. **umg-widgets** - Remove service reference, keep hierarchy gotchas
6. **level-actors** - Remove service reference, keep subsystem patterns
7. **asset-management** - Remove service reference, keep search patterns

Each migration:
1. Add `discovery:` section to skill.md
2. Add "Run Discovery First" instruction
3. Delete 01-service-reference.md
4. Condense remaining files to essentials
5. Test with domain test prompts
