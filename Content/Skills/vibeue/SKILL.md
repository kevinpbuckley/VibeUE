---
name: vibeue
description: Unreal Engine 5 development using the VibeUE Python API. Use when working
  in Unreal Engine — blueprints, state trees, materials, actors, landscapes, animation,
  niagara, widgets, sound, foliage, data tables, gameplay tags, enhanced input, skeletons,
  PCG (procedural content generation), and more. Requires the VibeUE MCP server to be connected.
compatibility: Requires VibeUE MCP server
---

VibeUE exposes its own skills system via MCP. Use it instead of this file for all tasks.

## Discover available skills

```
manage_skills(action="list")
```

## Load a skill before working in a domain

```
manage_skills(action="load", skill_name="blueprints")
manage_skills(action="load", skill_name="state-trees")
manage_skills(action="load", skill_name="materials")
```

The loaded skill returns:
- `vibeue_apis` — live method signatures discovered at runtime (use these, not guesses)
- `content` — workflows, gotchas, and property formats for the domain

Always use `vibeue_apis` for exact method names and parameters.
