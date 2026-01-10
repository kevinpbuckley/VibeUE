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
  - compile
  - CDO
auto_load_keywords:
  - blueprint
  - BP_
  - function
  - variable
  - component
  - node graph
  - event graph
---

# Blueprint System

This skill provides comprehensive documentation for working with Blueprint assets in Unreal Engine using the BlueprintService.

## What's Included

- **01-service-reference.md**: Complete BlueprintService API reference with all methods
- **02-workflows.md**: Common patterns for creating blueprints, variables, functions, and node graphs
- **03-common-mistakes.md**: Critical rules and pitfalls to avoid

## When to Use This Skill

Load this skill when working with:
- Blueprint assets (BP_* prefix)
- Blueprint variables and properties
- Blueprint functions and custom events
- Blueprint components (mesh, light, etc.)
- Blueprint node graphs and connections
- Function Entry/Result nodes
- Math nodes, Branch nodes, Variable Get/Set nodes

## Core Services

### BlueprintService
The main service for blueprint manipulation, providing:
- Blueprint lifecycle (create, compile, reparent)
- Variable management (add, remove, modify)
- Function management (create, parameters, local variables)
- Component management (add, remove, configure)
- Node operations (add, connect, configure)
- Graph inspection and modification

## Quick Examples

### Create a Blueprint with Variables
```python
import unreal

path = unreal.BlueprintService.create_blueprint("Enemy", "Character", "/Game/")
unreal.BlueprintService.add_variable(path, "Health", "float", "100.0")
unreal.BlueprintService.compile_blueprint(path)
```

### Add a Function with Logic
```python
unreal.BlueprintService.create_function(bp_path, "TakeDamage")
unreal.BlueprintService.add_function_input(bp_path, "TakeDamage", "Amount", "float")
unreal.BlueprintService.compile_blueprint(bp_path)
```

## Related Skills

- **asset-management**: For finding and opening blueprints
- **materials**: For blueprint material parameters
- **umg-widgets**: For Widget Blueprints (use WidgetService instead)
