---
name: materials
display_name: Material System
description: Create and edit materials and material instances using MaterialService and MaterialNodeService
vibeue_classes:
  - MaterialService
  - MaterialNodeService
unreal_classes:
  - EditorAssetLibrary
keywords:
  - material
  - shader
  - expression
  - node
  - parameter
  - texture
  - MaterialService
  - MaterialNodeService
---

## ⚠️ CRITICAL: Always Use discover_python_class() First

Before accessing any struct properties (MaterialExpressionInfo, MaterialExpressionTypeInfo, etc.):
1. Call `discover_python_class('unreal.StructName')` to see actual property names
2. Common mistake: Using `name` instead of `display_name`, or `expression_id` instead of `connected_expression_id`
3. Always check node existence with `next(..., None)` before using `.id` property
