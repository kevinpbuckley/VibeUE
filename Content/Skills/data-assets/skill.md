---
name: data-assets
display_name: Data Assets
description: Create and modify Primary Data Assets with property management
services:
  - DataAssetService
keywords:
  - data asset
  - primary data asset
  - PDA_
  - asset property
  - asset class
auto_load_keywords:
  - data asset
  - PDA_
  - primary data asset
---

# Data Assets

This skill provides comprehensive documentation for working with Primary Data Assets in Unreal Engine using the DataAssetService.

## What's Included

- **DataAssetService API**: Create Data Assets, manage properties
- **Property Format Guide**: How to format complex properties
- **Workflows**: Common patterns for asset creation

## When to Use This Skill

Load this skill when working with:
- Primary Data Assets (PDA_* prefix)
- Data Asset classes
- Asset properties and configuration
- Custom Data Asset types

## Core Services

### DataAssetService
Data Asset management:
- Create Data Assets from classes
- List Data Assets by class
- Get class schema information
- Get/set properties (single or bulk)
- Search Data Asset types

## Quick Examples

### Create Data Asset
```python
import unreal

# Create InputAction data asset
path = unreal.DataAssetService.create_data_asset("InputAction", "/Game/Input/", "IA_Jump")

# Set properties
unreal.DataAssetService.set_property(path, "bConsumeInput", "true")
unreal.DataAssetService.set_property(path, "ActionDescription", "Player jump")
```

### Get Class Info
```python
import unreal

info = unreal.DataAssetService.get_class_info("InputAction", True)
print(f"Class: {info.name}")
for prop in info.properties:
    print(f"  {prop.name}: {prop.type}")
```

## Complex Property Formats

For complex properties, use Unreal's string format:
```python
# Array of structs
keys_str = '((EntryName="Key1",EntryCategory="AI"),(EntryName="Key2"))'
unreal.DataAssetService.set_property(path, "Keys", keys_str)
```

## Related Skills

- **asset-management**: For finding Data Assets
- **data-tables**: For Data Tables (different from Data Assets)
- **enhanced-input**: For Input Action/Context data assets
