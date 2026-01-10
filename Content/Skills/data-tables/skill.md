---
name: data-tables
display_name: Data Tables
description: Create and modify Data Tables with row operations and JSON data
services:
  - DataTableService
keywords:
  - datatable
  - data table
  - row
  - column
  - struct
  - DT_
  - json
  - table data
auto_load_keywords:
  - DT_
  - data table
  - datatable
  - table row
---

# Data Tables

This skill provides comprehensive documentation for working with Data Tables in Unreal Engine using the DataTableService.

## What's Included

- **DataTableService API**: Create tables, manage rows, work with JSON data
- **JSON Format Guide**: How to format row data correctly
- **Workflows**: Common patterns for table operations

## When to Use This Skill

Load this skill when working with:
- Data Table assets (DT_* prefix)
- Table rows and columns
- Row struct definitions
- JSON data for rows
- Bulk row operations

## Core Services

### DataTableService
Data Table management:
- Create Data Tables with row structs
- Add, update, remove, rename rows
- Bulk row operations
- Search row struct types
- List Data Tables
- Export/import row data as JSON

## Quick Examples

### Create Data Table and Add Rows
```python
import unreal
import json

# Create table
path = unreal.DataTableService.create_data_table("FItemData", "/Game/Data/", "DT_Items")

# Add row
data = {"Name": "Sword", "Damage": 50, "Price": 100}
unreal.DataTableService.add_row(path, "Sword", json.dumps(data))
```

### Bulk Add Rows
```python
import unreal
import json

rows = {
    "Sword": {"Damage": 50, "Price": 100},
    "Shield": {"Defense": 30, "Price": 80}
}
result = unreal.DataTableService.add_rows(path, json.dumps(rows))
print(f"Added: {result.succeeded_rows}")
```

## Critical: JSON Format

**ALWAYS use `json.dumps()` to convert Python dicts to JSON strings:**
```python
import json

# CORRECT
data = {"Health": 100}
unreal.DataTableService.add_row(path, "Row1", json.dumps(data))

# WRONG - will fail
# unreal.DataTableService.add_row(path, "Row1", {"Health": 100})
```

## Related Skills

- **asset-management**: For finding Data Tables
- **data-assets**: For Data Assets (different from Data Tables)
