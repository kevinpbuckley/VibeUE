# VibeUE Skills System Design

## Problem Statement

The current VibeUE.instructions file is approximately 13,000 tokens, which consumes significant initial context before any chat interaction begins. This large initial payload includes:
- Tool reference documentation for 10+ services (BlueprintService, MaterialService, DataTableService, etc.)
- Workflow documentation for common operations
- Critical rules and best practices
- Return type documentation
- Error handling patterns

This creates two key issues:
1. **Context inefficiency**: Users pay the token cost for all documentation regardless of what domain they're working in
2. **Maintenance burden**: Adding new features or services increases the initial context size

## Solution: Skills System

Implement a lazy-loading Skills system where domain-specific knowledge is loaded on-demand via a new MCP tool.

### Core Concepts

**Skill**: A domain-specific knowledge package containing:
- Service API documentation
- Workflow patterns
- Best practices
- Common pitfalls
- Example code

**Skills Tool**: An MCP tool that allows the AI to discover and load skills dynamically

**Base Instructions**: A minimal core instruction set that teaches the AI:
- How to use the Skills tool
- When to load skills
- Core Python/Unreal concepts that apply to all domains

---

## MCP Tool Specification

### Tool Name: `manage_skills`

### Parameters

```typescript
{
  action: "list" | "load",
  skill_name?: string  // Required when action = "load"
}
```

### Actions

#### `list`
Scans the Skills directory, reads each skill.md frontmatter, and returns metadata:

```json
{
  "skills": [
    {
      "name": "blueprints",
      "display_name": "Blueprint System",
      "description": "Create and modify Blueprint assets, variables, functions, components, and node graphs",
      "services": ["BlueprintService"],
      "keywords": ["blueprint", "variable", "function", "component", "node", "graph"],
      "auto_load_keywords": ["blueprint", "BP_", "function", "variable", "node graph"],
      "file_count": 4,
      "estimated_tokens": 3500
    },
    {
      "name": "materials",
      "display_name": "Material System",
      "description": "Create materials, material instances, and edit material node graphs",
      "services": ["MaterialService", "MaterialNodeService"],
      "keywords": ["material", "shader", "expression", "parameter", "instance"],
      "auto_load_keywords": ["material", "M_", "MI_", "shader"],
      "file_count": 4,
      "estimated_tokens": 2200
    }
    // ... more skills
  ]
}
```

**Implementation:**
1. Enumerate subdirectories in `Content/Skills/`
2. For each directory, read `skill.md` and parse YAML frontmatter
3. Count markdown files in directory (excluding skill.md)
4. Return array of skill metadata

#### `load`
Loads all markdown files from a skill's directory and returns concatenated content.

**Parameters:**
- `skill_name`: Can be either the `name` field (e.g., "blueprints") or `display_name` field (e.g., "Blueprint System")

```json
{
  "skill_name": "blueprints",
  "content": "# Blueprint System\n\n## BlueprintService\n...",
  "files_loaded": [
    "blueprints/skill.md",
    "blueprints/service-reference.md",
    "blueprints/workflows.md",
    "blueprints/common-mistakes.md"
  ],
  "token_count": 3472
}
```

**Implementation:**
1. Resolve skill name to directory:
   - First try as directory name: `Content/Skills/{skill_name}/`
   - If not found, scan all skill.md files and match on `name` field (exact match)
   - If not found, match on `display_name` field (case-insensitive)
2. Once skill directory identified, find all `*.md` files in the directory
3. Read and concatenate files in alphabetical order (skill.md first, then others)
4. Return concatenated content with file list

**Examples:**
```python
# All of these work for the blueprints skill:
manage_skills(action="load", skill_name="blueprints")           # Directory name
manage_skills(action="load", skill_name="Blueprint System")     # Display name
manage_skills(action="load", skill_name="blueprint system")     # Case-insensitive
```

---

## Directory Structure

```
Plugins/VibeUE/Content/
├── instructions/
│   └── vibeue.instructions.md          # Minimal core instructions (~2-3k tokens)
│
└── Skills/                              # New directory
    ├── blueprints/
    │   ├── skill.md                    # Skill metadata (REQUIRED)
    │   ├── service-reference.md        # BlueprintService API docs
    │   ├── workflows.md                # Common workflows
    │   └── common-mistakes.md          # Critical rules
    ├── materials/
    │   ├── skill.md                    # Skill metadata (REQUIRED)
    │   ├── material-service.md         # MaterialService API
    │   ├── material-node-service.md    # MaterialNodeService API
    │   └── workflows.md
    ├── enhanced-input/
    │   ├── skill.md                    # Skill metadata (REQUIRED)
    │   ├── service-reference.md        # InputService API
    │   ├── workflows.md
    │   └── return-types.md             # Complex return type docs
    ├── data-tables/
    │   ├── skill.md                    # Skill metadata (REQUIRED)
    │   ├── service-reference.md
    │   ├── workflows.md
    │   └── json-format-guide.md
    ├── data-assets/
    │   ├── skill.md                    # Skill metadata (REQUIRED)
    │   ├── service-reference.md
    │   ├── workflows.md
    │   └── property-formats.md
    ├── umg-widgets/
    │   ├── skill.md                    # Skill metadata (REQUIRED)
    │   ├── service-reference.md
    │   └── workflows.md
    ├── level-actors/
    │   ├── skill.md                    # Skill metadata (REQUIRED)
    │   ├── workflows.md
    │   └── subsystems.md
    └── asset-management/
        ├── skill.md                    # Skill metadata (REQUIRED)
        ├── service-reference.md
        └── workflows.md
```

### skill.md Format (Frontmatter)

Each skill directory must contain a `skill.md` file with YAML frontmatter:

```markdown
---
name: blueprints
display_name: Blueprint System
description: Create and modify Blueprint assets, variables, functions, components, and node graphs
services:
  - BlueprintService
keywords:
  - blueprint
  - variable
  - function
  - component
  - node
  - graph
auto_load_keywords:
  - blueprint
  - BP_
  - function
  - variable
  - node graph
  - event graph
---

# Blueprint System

This skill provides comprehensive documentation for working with Blueprint assets in Unreal Engine using the BlueprintService.

## What's Included

- **service-reference.md**: Complete BlueprintService API reference
- **workflows.md**: Common patterns for creating blueprints, variables, functions, and node graphs
- **common-mistakes.md**: Critical rules and pitfalls to avoid

## When to Use

Load this skill when working with:
- Blueprint assets (BP_*)
- Blueprint variables
- Blueprint functions
- Blueprint components
- Blueprint node graphs
```

---

## Content Breakdown by Skill

### Current Content Mapping

| Current Section | Target Skill | Notes |
|----------------|--------------|-------|
| **CRITICAL: Available MCP Tools** | Base instructions | Core concept, always needed |
| **CRITICAL: Discover Services FIRST** | Base instructions | Core workflow |
| **CRITICAL: Check Before Creating** | Base instructions | Critical pattern |
| **Python Basics** | Base instructions | Core Python concepts |
| **BlueprintService** | `blueprints/service-reference.md` | Full API reference |
| **AssetDiscoveryService** | `asset-management/service-reference.md` | Full API reference |
| **MaterialService** | `materials/material-service.md` | Full API reference |
| **MaterialNodeService** | `materials/material-node-service.md` | Full API reference |
| **DataTableService** | `data-tables/service-reference.md` | Including return type docs |
| **DataAssetService** | `data-assets/service-reference.md` | Including return type docs |
| **WidgetService** | `umg-widgets/service-reference.md` | Full API reference |
| **InputService** | `enhanced-input/service-reference.md` | Including return type docs |
| **MCP Discovery Tools** | Base instructions | Core tools |
| **Common Workflows** | Split by skill | Each workflow goes to relevant skill |
| **Blueprint Node Reference** | `blueprints/node-reference.md` | Blueprint-specific |
| **Critical Rules** | Base + relevant skills | Split by relevance |
| **Communication Style** | Base instructions | Always needed |

---

## Updated Base Instructions Structure

The new [vibeue.instructions.md](vibeue.instructions.md) should contain:

1. **MCP Tools Overview** (unchanged)
   - 6 core tools
   - No individual service tools

2. **Skills System** (NEW)
   - How to list skills
   - How to load skills
   - When to load skills (keywords, user intent)

3. **Core Workflow Pattern** (condensed)
   - Discover → Check → Execute pattern
   - Example with BlueprintService (brief)

4. **Python Basics** (unchanged)
   - `import unreal`
   - Editor subsystems
   - VibeUE service access

5. **Discovery Tools** (unchanged)
   - `discover_python_class`
   - `execute_python_code`
   - etc.

6. **Critical Rules** (global only)
   - Check before creating
   - Error recovery (max attempts)
   - Safety rules (no modals, blocking)
   - Asset path format

7. **Communication Style** (unchanged)

**Estimated tokens**: ~2,500-3,000 (down from 13,000)

---

## AI Behavior Patterns

### When to Load Skills

**Automatic Loading (Smart Detection)**
The AI should automatically load skills when:
1. User mentions a skill keyword in auto_load_keywords
2. User references an asset type (BP_, WBP_, IA_, IMC_, DT_, etc.)
3. User asks to create/modify/inspect domain-specific content

**Examples:**
```
User: "Create a blueprint called BP_Enemy"
→ AI loads "blueprints" skill automatically

User: "Add a material parameter to M_Character"
→ AI loads "materials" skill automatically

User: "What input actions exist?"
→ AI loads "enhanced-input" skill automatically
```

**Manual Loading (Discovery Phase)**
The AI may call `manage_skills(action="list")` when:
1. User query is ambiguous about domain
2. AI needs to understand available capabilities
3. First message in session (optional, for context building)

### Multi-Skill Operations

When a task requires multiple domains:
```
User: "Create a blueprint with a material parameter component"
→ AI loads "blueprints" skill first
→ After blueprint work, loads "materials" skill for parameter setup
```

### Skill Caching

Once loaded, skills persist for the conversation. The AI should:
1. Track which skills are loaded in the current session
2. Not reload the same skill twice
3. Reference loaded skill content for subsequent questions

---

## Implementation Phases

### Phase 1: Infrastructure
1. Create `/Content/Skills/` directory structure
2. Create `skills-manifest.json`
3. Implement `manage_skills` MCP tool in C++/Python
4. Add unit tests for skill loading

### Phase 2: Content Migration
1. Create new minimal [vibeue.instructions.md](vibeue.instructions.md)
2. Split current content into skill-specific markdown files
3. Organize by service and workflow
4. Add cross-references between skills

### Phase 3: Testing & Refinement
1. Test with existing test_prompts
2. Measure token usage improvements
3. Verify AI loads skills correctly
4. Refine auto-load keywords

### Phase 4: Documentation
1. Update VibeUE README with skills concept
2. Document skill authoring guidelines
3. Create skill template for future additions

---

## Benefits

### Token Efficiency
- **Before**: 13k tokens baseline, every conversation
- **After**: ~2.5k baseline + skills on demand
- **Savings**: 80% reduction in unused context

**Example scenarios:**
- Blueprint-only work: 2.5k (base) + 3.5k (blueprints) = **6k tokens** (54% reduction)
- Material-only work: 2.5k (base) + 2.2k (materials) = **4.7k tokens** (64% reduction)
- Multi-domain work: Up to 8-10k tokens (still 23-38% reduction)

### Maintainability
- New services can be added as standalone skills
- Skill updates don't affect base instructions
- Skills can be versioned independently
- Easier to test individual domains

### Scalability
- Add new skills without context growth
- Deprecate old skills without breaking changes
- Community can contribute domain-specific skills
- A/B test skill variations

### User Experience
- Faster first response (less initial processing)
- Pay-per-use token model
- Skills loaded align with user intent
- Clear skill boundaries

---

## Risks & Mitigations

### Risk: AI Forgets to Load Skills
**Mitigation**:
- Strong auto-load keywords in manifest
- Examples in base instructions showing skill loading
- Tool descriptions emphasize proactive loading

### Risk: Incorrect Skill Selection
**Mitigation**:
- Clear skill descriptions with examples
- Keywords cover common variations
- Manifest includes related services

### Risk: Cross-Skill Dependencies
**Mitigation**:
- Most skills are independent (Blueprints, Materials, etc.)
- Asset management is loaded frequently (shared dependency)
- Skills can reference each other ("See materials skill for...")

### Risk: Increased API Calls
**Mitigation**:
- Skill loading is fast (file reads)
- Only load once per session
- List operation is cheap (metadata only)

---

## Future Enhancements

### Skill Recommendations
AI suggests related skills:
```
"I've completed the blueprint. Would you like me to load the 'materials'
skill to add material parameters?"
```

### Skill Composition
Composite skills that bundle related domains:
```
"character-creation" skill = blueprints + materials + animation
```

### Dynamic Skill Updates
Hot-reload skills without restarting Unreal:
```
Edit skill markdown → AI reloads on next use
```

### Skill Analytics
Track which skills are used most frequently:
```
{
  "blueprints": 847 loads,
  "materials": 423 loads,
  "enhanced-input": 234 loads
}
```

---

## Success Metrics

- **Token Usage**: Measure average tokens per conversation
- **Load Frequency**: Track which skills are loaded most
- **User Satisfaction**: Collect feedback on response relevance
- **Performance**: Measure first-response latency improvement
- **Coverage**: Ensure test_prompts still pass with new system

---

## Appendix: Example Skill File

### blueprints/service-reference.md

````markdown
# BlueprintService API Reference

All methods are called via `unreal.BlueprintService.<method_name>(...)`.

**ALWAYS use `discover_python_class("unreal.BlueprintService")` for parameter details.**

## Lifecycle & Properties

### create_blueprint(name, parent_class, path)
Create a new Blueprint asset.

**Parameters:**
- `name` (str): Blueprint name (without BP_ prefix)
- `parent_class` (str): Parent class (e.g., "Actor", "Character", "ActorComponent")
- `path` (str): Destination path (e.g., "/Game/Blueprints/")

**Returns:** Full asset path (str)

**Example:**
```python
import unreal
path = unreal.BlueprintService.create_blueprint("Enemy", "Character", "/Game/Blueprints/")
print(f"Created: {path}")  # "/Game/Blueprints/BP_Enemy"
```

### compile_blueprint(path)
Compile a Blueprint asset. **REQUIRED before using newly added variables in nodes.**

**Example:**
```python
unreal.BlueprintService.add_variable("/Game/BP_Player", "Health", "float", "100.0")
unreal.BlueprintService.compile_blueprint("/Game/BP_Player")  # Must compile!
```

... (continue with full API)
````

---

## Conclusion

The Skills system provides a scalable, maintainable solution to VibeUE's context size problem. By loading domain knowledge on-demand, we reduce initial token costs by 80% while maintaining full feature coverage. The modular design allows independent evolution of each skill and sets the foundation for community contributions.
