---
name: materials
display_name: Material System
description: Create materials, material instances, and edit material node graphs
services:
  - MaterialService
  - MaterialNodeService
keywords:
  - material
  - shader
  - expression
  - parameter
  - instance
  - node
  - texture
  - base color
  - roughness
  - metallic
auto_load_keywords:
  - material
  - M_
  - MI_
  - shader
  - material instance
  - material parameter
  - material expression
---

# Material System

This skill provides comprehensive documentation for working with Materials and Material Instances in Unreal Engine.

## What's Included

- **MaterialService API**: Create materials, instances, manage properties and parameters
- **MaterialNodeService API**: Build material node graphs, connect expressions, manage parameters
- **Workflows**: Common patterns for material creation and graph building

## When to Use This Skill

Load this skill when working with:
- Material assets (M_* prefix)
- Material instances (MI_* prefix)
- Material parameters (scalar, vector, texture)
- Material expressions and nodes
- Material properties (blend mode, shading model, etc.)
- Material node graphs and connections

## Core Services

### MaterialService
Material lifecycle and properties:
- Create materials and instances
- Compile and save materials
- Manage material properties
- Manage parameters (scalar, vector, texture)
- Get/set instance parameter overrides

### MaterialNodeService
Material graph manipulation:
- Create and delete expressions
- Connect expressions together
- Connect to material outputs (BaseColor, Roughness, etc.)
- Create and promote parameters
- Manage expression properties

## Quick Examples

### Create a Material with Parameters
```python
import unreal

path = unreal.MaterialService.create_material("Character", "/Game/Materials/")
unreal.MaterialNodeService.create_parameter(path, "Vector", "BaseColor", "Surface", "", -500, 0)
unreal.MaterialService.compile_material(path)
```

### Create Material Instance
```python
import unreal

mi_path = unreal.MaterialService.create_instance("/Game/Materials/M_Character", "MI_Character_Red", "/Game/Materials/")
unreal.MaterialService.set_instance_vector_parameter(mi_path, "BaseColor", 1.0, 0.0, 0.0, 1.0)
unreal.MaterialService.save_instance(mi_path)
```

## Related Skills

- **asset-management**: For finding and opening materials
- **blueprints**: For blueprint material parameters
