# Data Table Workflows

Common patterns for working with Data Tables.

---

## Create Data Table with Row Struct

```python
import unreal
import json

# 1. Search for available row struct types
structs = unreal.DataTableService.search_row_types("Item")
for s in structs:
    print(f"{s.name}: {s.path}")
    print(f"  Properties: {list(s.property_names)}")

# 2. Create Data Table using struct
table_path = unreal.DataTableService.create_data_table(
    "FItemData",            # Row struct name
    "/Game/Data/",          # Path
    "DT_Items"              # Asset name
)
print(f"Created: {table_path}")

# 3. Get struct schema to understand fields
columns = unreal.DataTableService.get_row_struct(table_path)
print("\nRow structure:")
for col in columns:
    print(f"  {col.name}: {col.type}")

# 4. Add rows
item1 = {
    "Name": "Health Potion",
    "Type": "Consumable",
    "Value": 50,
    "Price": 10
}

unreal.DataTableService.add_row(table_path, "HealthPotion", json.dumps(item1))

# 5. Save
unreal.EditorAssetLibrary.save_asset(table_path)
```

---

## Populate Data Table from CSV/List

```python
import unreal
import json

table_path = "/Game/Data/DT_Weapons"

# Define weapon data
weapons = [
    {"row": "Sword", "data": {"Name": "Iron Sword", "Damage": 50, "Price": 100}},
    {"row": "Axe", "data": {"Name": "Battle Axe", "Damage": 75, "Price": 150}},
    {"row": "Bow", "data": {"Name": "Long Bow", "Damage": 40, "Price": 120}},
    {"row": "Spear", "data": {"Name": "Steel Spear", "Damage": 60, "Price": 110}},
    {"row": "Dagger", "data": {"Name": "Iron Dagger", "Damage": 30, "Price": 50}}
]

# Add rows one by one
for weapon in weapons:
    success = unreal.DataTableService.add_row(
        table_path,
        weapon["row"],
        json.dumps(weapon["data"])
    )
    if success:
        print(f"Added: {weapon['row']}")
    else:
        print(f"Failed to add: {weapon['row']}")

# Or use bulk operation
rows_dict = {weapon["row"]: weapon["data"] for weapon in weapons}
result = unreal.DataTableService.add_rows(table_path, json.dumps(rows_dict))

print(f"\nBulk operation:")
print(f"  Succeeded: {len(result.succeeded_rows)} rows")
print(f"  Failed: {len(result.failed_rows)} rows")

# Save
unreal.EditorAssetLibrary.save_asset(table_path)
```

---

## Inspect and Query Data Table

```python
import unreal
import json

table_path = "/Game/Data/DT_Items"

# 1. Get table info
info = unreal.DataTableService.get_info(table_path)
if info:
    print(f"Table: {info.name}")
    print(f"Row struct: {info.row_struct}")
    print(f"Row count: {info.row_count}")
    print(f"Columns: {info.columns_json}")

# 2. List all row names
rows = unreal.DataTableService.list_rows(table_path)
print(f"\nRows in table: {rows}")

# 3. Read specific rows
for row_name in rows:
    row_json = unreal.DataTableService.get_row(table_path, row_name)
    if row_json:
        row_data = json.loads(row_json)
        print(f"\n{row_name}:")
        for key, value in row_data.items():
            print(f"  {key}: {value}")

# 4. Filter rows by criteria (Python side)
matching_items = []
for row_name in rows:
    row_json = unreal.DataTableService.get_row(table_path, row_name)
    if row_json:
        row_data = json.loads(row_json)
        # Find items with Damage > 50
        if "Damage" in row_data and row_data["Damage"] > 50:
            matching_items.append((row_name, row_data))

print(f"\nItems with Damage > 50:")
for name, data in matching_items:
    print(f"  {name}: {data['Damage']}")
```

---

## Update Multiple Rows

```python
import unreal
import json

table_path = "/Game/Data/DT_Items"

# 1. Get all rows
rows = unreal.DataTableService.list_rows(table_path)

# 2. Update each row (e.g., increase all prices by 10%)
for row_name in rows:
    row_json = unreal.DataTableService.get_row(table_path, row_name)
    if row_json:
        row_data = json.loads(row_json)

        if "Price" in row_data:
            # Increase price by 10%
            new_price = int(row_data["Price"] * 1.1)
            updates = {"Price": new_price}

            unreal.DataTableService.update_row(
                table_path,
                row_name,
                json.dumps(updates)
            )
            print(f"Updated {row_name}: Price {row_data['Price']} -> {new_price}")

# 3. Save
unreal.EditorAssetLibrary.save_asset(table_path)
```

---

## Copy Rows Between Tables

```python
import unreal
import json

source_table = "/Game/Data/DT_AllItems"
dest_table = "/Game/Data/DT_Weapons"

# 1. Get rows from source
source_rows = unreal.DataTableService.list_rows(source_table)

# 2. Filter and copy rows
for row_name in source_rows:
    row_json = unreal.DataTableService.get_row(source_table, row_name)
    if row_json:
        row_data = json.loads(row_json)

        # Copy only weapon items
        if "Type" in row_data and row_data["Type"] == "Weapon":
            # Add to destination table
            success = unreal.DataTableService.add_row(
                dest_table,
                row_name,
                row_json  # Already JSON string
            )
            if success:
                print(f"Copied: {row_name}")

# 3. Save destination
unreal.EditorAssetLibrary.save_asset(dest_table)
```

---

## Rename Rows by Pattern

```python
import unreal
import json

table_path = "/Game/Data/DT_Items"

# Get all rows
rows = unreal.DataTableService.list_rows(table_path)

# Rename rows to add prefix
for row_name in rows:
    # Skip if already has prefix
    if not row_name.startswith("Item_"):
        new_name = f"Item_{row_name}"

        success = unreal.DataTableService.rename_row(
            table_path,
            row_name,
            new_name
        )
        if success:
            print(f"Renamed: {row_name} -> {new_name}")

# Save
unreal.EditorAssetLibrary.save_asset(table_path)
```

---

## Validate Data Table Contents

```python
import unreal
import json

table_path = "/Game/Data/DT_Items"

# Get struct schema
columns = unreal.DataTableService.get_row_struct(table_path)
required_fields = {col.name: col.type for col in columns}

print(f"Required fields:")
for field, field_type in required_fields.items():
    print(f"  {field}: {field_type}")

# Validate all rows
rows = unreal.DataTableService.list_rows(table_path)
errors = []

for row_name in rows:
    row_json = unreal.DataTableService.get_row(table_path, row_name)
    if row_json:
        row_data = json.loads(row_json)

        # Check for missing required fields
        for field in required_fields.keys():
            if field not in row_data:
                errors.append(f"{row_name}: Missing field '{field}'")

        # Check for invalid values (custom validation)
        if "Price" in row_data and row_data["Price"] < 0:
            errors.append(f"{row_name}: Price cannot be negative")

        if "Damage" in row_data and row_data["Damage"] < 0:
            errors.append(f"{row_name}: Damage cannot be negative")

if errors:
    print(f"\nValidation errors:")
    for error in errors:
        print(f"  {error}")
else:
    print(f"\nAll {len(rows)} rows are valid!")
```

---

## Export Data Table to Python Dict

```python
import unreal
import json

table_path = "/Game/Data/DT_Items"

# Export all rows
rows = unreal.DataTableService.list_rows(table_path)
exported_data = {}

for row_name in rows:
    row_json = unreal.DataTableService.get_row(table_path, row_name)
    if row_json:
        exported_data[row_name] = json.loads(row_json)

# Print or save to file
print(json.dumps(exported_data, indent=2))

# Or write to file (if using file system access)
# with open("exported_data.json", "w") as f:
#     json.dump(exported_data, f, indent=2)
```

---

## Create Data Table from Python Dict

```python
import unreal
import json

# Data structure
items_data = {
    "HealthPotion": {
        "Name": "Health Potion",
        "Type": "Consumable",
        "Effect": "Healing",
        "Value": 50,
        "Price": 10
    },
    "ManaPotion": {
        "Name": "Mana Potion",
        "Type": "Consumable",
        "Effect": "Restore Mana",
        "Value": 30,
        "Price": 8
    },
    "Antidote": {
        "Name": "Antidote",
        "Type": "Consumable",
        "Effect": "Cure Poison",
        "Value": 100,
        "Price": 15
    }
}

# 1. Create Data Table (assuming struct exists)
table_path = unreal.DataTableService.create_data_table(
    "FItemData",
    "/Game/Data/",
    "DT_Consumables"
)

# 2. Bulk add all rows
result = unreal.DataTableService.add_rows(table_path, json.dumps(items_data))

print(f"Added {len(result.succeeded_rows)} rows")
if result.failed_rows:
    print(f"Failed rows: {list(result.failed_rows)}")
    print(f"Reasons: {list(result.failed_reasons)}")

# 3. Save
unreal.EditorAssetLibrary.save_asset(table_path)
```

---

## Clear and Repopulate Data Table

```python
import unreal
import json

table_path = "/Game/Data/DT_Items"

# 1. Backup existing data
rows = unreal.DataTableService.list_rows(table_path)
backup = {}

for row_name in rows:
    row_json = unreal.DataTableService.get_row(table_path, row_name)
    if row_json:
        backup[row_name] = json.loads(row_json)

print(f"Backed up {len(backup)} rows")

# 2. Clear table
removed_count = unreal.DataTableService.clear_rows(table_path)
print(f"Removed {removed_count} rows")

# 3. Add new data
new_data = {
    "NewItem1": {"Name": "Item 1", "Value": 10},
    "NewItem2": {"Name": "Item 2", "Value": 20}
}

result = unreal.DataTableService.add_rows(table_path, json.dumps(new_data))
print(f"Added {len(result.succeeded_rows)} new rows")

# 4. Save
unreal.EditorAssetLibrary.save_asset(table_path)

# To restore backup later:
# result = unreal.DataTableService.add_rows(table_path, json.dumps(backup))
```

---

## Merge Data Tables

```python
import unreal
import json

table1_path = "/Game/Data/DT_Weapons"
table2_path = "/Game/Data/DT_Armor"
merged_table_path = "/Game/Data/DT_AllEquipment"

# 1. Create merged table (using same struct as table1)
info1 = unreal.DataTableService.get_info(table1_path)
unreal.DataTableService.create_data_table(
    info1.row_struct,
    "/Game/Data/",
    "DT_AllEquipment"
)

# 2. Copy rows from table1
rows1 = unreal.DataTableService.list_rows(table1_path)
for row_name in rows1:
    row_json = unreal.DataTableService.get_row(table1_path, row_name)
    if row_json:
        unreal.DataTableService.add_row(merged_table_path, row_name, row_json)

print(f"Copied {len(rows1)} rows from {table1_path}")

# 3. Copy rows from table2 (with prefix to avoid conflicts)
rows2 = unreal.DataTableService.list_rows(table2_path)
for row_name in rows2:
    row_json = unreal.DataTableService.get_row(table2_path, row_name)
    if row_json:
        # Add prefix to avoid name conflicts
        prefixed_name = f"Armor_{row_name}"
        unreal.DataTableService.add_row(merged_table_path, prefixed_name, row_json)

print(f"Copied {len(rows2)} rows from {table2_path}")

# 4. Save
unreal.EditorAssetLibrary.save_asset(merged_table_path)
```

---

## Common Patterns Summary

### Pattern 1: Check → Create → Populate → Save
```python
import unreal
import json

# Check if exists
existing = unreal.AssetDiscoveryService.find_asset_by_path("/Game/Data/DT_Items")
if not existing:
    # Create
    table_path = unreal.DataTableService.create_data_table("FItemData", "/Game/Data/", "DT_Items")

    # Populate
    data = {"Item1": {"Name": "Test", "Value": 10}}
    unreal.DataTableService.add_rows(table_path, json.dumps(data))

    # Save
    unreal.EditorAssetLibrary.save_asset(table_path)
```

### Pattern 2: Read → Modify → Write
```python
import unreal
import json

# Read
row_json = unreal.DataTableService.get_row("/Game/Data/DT_Items", "Sword")
row_data = json.loads(row_json)

# Modify
row_data["Damage"] += 10

# Write
unreal.DataTableService.update_row("/Game/Data/DT_Items", "Sword", json.dumps(row_data))
```

### Pattern 3: Bulk Operation for Performance
```python
import unreal
import json

# Collect all changes
updates = {}
for i in range(100):
    updates[f"Item{i}"] = {"Name": f"Item {i}", "Value": i * 10}

# Single bulk operation
unreal.DataTableService.add_rows("/Game/Data/DT_Items", json.dumps(updates))
```

### Pattern 4: Always Use json.dumps() and json.loads()
```python
import unreal
import json

# Writing: Python dict → JSON string
data = {"Name": "Sword"}
unreal.DataTableService.add_row("/Game/Data/DT_Items", "Sword", json.dumps(data))

# Reading: JSON string → Python dict
row_json = unreal.DataTableService.get_row("/Game/Data/DT_Items", "Sword")
row_data = json.loads(row_json)
```
