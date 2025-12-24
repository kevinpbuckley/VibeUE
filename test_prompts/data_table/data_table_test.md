# Data Table Management Tests - Comprehensive Stress Test

These tests should be run sequentially through the VibeUE chat interface in Unreal Engine. Each test builds on the previous ones. Create any required assets if they don't exist. This is an exhaustive test of all data table capabilities.

**Important Notes:**
- These are natural language prompts designed for the AI assistant. The AI will translate them into appropriate `manage_data_table` tool calls.
- **Run these tests ONE PART AT A TIME**, not all at once. Paste each section individually into the VibeUE chat.
- Data tables require a **row struct** type to define their schema. Always use `search_row_types` to discover available structs first.
- If trying to create a table that already exists, delete it and try again.

---

## Part 1: Row Struct Discovery

Show me ALL available row struct types in this project. I want the complete list.

---

Filter to find any row structs with "Item" in the name.

---

Filter for anything containing "Character" or "Player".

---

Filter for anything containing "Weapon" or "Combat".

---

Filter for anything containing "AI" or "NPC".

---

Filter for anything containing "Level" or "World".

---

Filter for anything containing "UI" or "Menu".

---

Get detailed info on FTableRowBase - show me all its properties and inheritance.

---

## Part 2: Data Table Discovery

List ALL data tables currently in the project.

---

List data tables filtered to /Game path only.

---

List data tables that use a specific row struct type (pick one from Part 1 results).

---

Get detailed info on an existing data table if any exist.

---

## Part 3: Simple Data Table Creation

Create a new data table called DT_TestItems in /Game/Data/Test/Tables using any available row struct that has item-related properties.

**Note:** First search for row structs to find an appropriate one, then create the table.

---

Get complete info on DT_TestItems including its row struct schema.

---

Get the row struct columns for DT_TestItems to see what properties each row can have.

---

## Part 4: Row Operations - Basic CRUD

### Add Rows

Add a row named "Sword" to DT_TestItems with appropriate property values based on the struct schema.

---

Add a row named "Shield" with different property values.

---

Add a row named "Potion" with different property values.

---

### List Rows

List all rows in DT_TestItems.

---

List rows with a limit of 2.

---

### Get Single Row

Get the "Sword" row from DT_TestItems.

---

Get the "Shield" row.

---

Get the "Potion" row.

---

### Update Rows

Update the "Sword" row - change one or more property values.

---

Update the "Potion" row - change different properties.

---

Verify the updates by getting the rows again.

---

### Rename Row

Rename "Sword" to "LegendarySword".

---

Verify the rename worked by listing all rows.

---

Try to get the old name "Sword" - it should fail.

---

Get the new name "LegendarySword" - it should succeed.

---

### Remove Row

Remove the "Potion" row from DT_TestItems.

---

List rows to verify it was removed.

---

Try to get the removed row - it should fail.

---

## Part 5: Bulk Row Operations

### Add Multiple Rows at Once

Add 5 rows at once to DT_TestItems using add_rows:
- Row_Bulk_01
- Row_Bulk_02
- Row_Bulk_03
- Row_Bulk_04
- Row_Bulk_05

Each with different property values.

---

List all rows to verify all 5 were added.

---

### Clear All Rows

Create a temporary table DT_TempClear in /Game/Data/Test/Tables.

---

Add 3 rows to DT_TempClear.

---

Clear all rows from DT_TempClear.

---

List rows to verify the table is empty.

---

Get info to confirm row count is 0.

---

## Part 6: Import/Export JSON

### Export to JSON

Export DT_TestItems to JSON format.

---

### Import from JSON

Create a new table DT_ImportTest in /Game/Data/Test/Tables.

---

Import JSON data into DT_ImportTest with this structure:
```json
{
  "ImportRow1": {"PropertyName": "Value1"},
  "ImportRow2": {"PropertyName": "Value2"},
  "ImportRow3": {"PropertyName": "Value3"}
}
```

**Note:** Adjust property names based on the actual row struct schema.

---

List rows to verify import worked.

---

Export DT_ImportTest to JSON and compare with original.

---

## Part 7: Multiple Data Table Types

### Create Tables with Different Row Structs

Search for 3 different row struct types.

---

Create DT_TypeA in /Game/Data/Test/Tables using the first row struct type.

---

Create DT_TypeB in /Game/Data/Test/Tables using the second row struct type.

---

Create DT_TypeC in /Game/Data/Test/Tables using the third row struct type.

---

Get info on each table to compare their schemas.

---

Add rows to each table using their specific struct properties.

---

## Part 8: Complex Property Types

### Numeric Properties

Find a row struct with numeric properties (int, float, etc.).

---

Create a table with that struct and add rows testing:
- Zero values
- Negative values
- Large values
- Decimal precision (for floats)

---

### String Properties

Find a row struct with FString, FName, or FText properties.

---

Add rows testing:
- Empty strings
- Long strings
- Special characters
- Unicode characters

---

### Boolean Properties

Find a row struct with bool properties.

---

Add rows with true and false values.

---

### Enum Properties

Find a row struct with enum properties.

---

Add rows with different enum values (by name and by index).

---

### Object Reference Properties

Find a row struct with object reference properties (soft references, asset paths).

---

Add rows that reference other assets.

---

### Struct Properties

Find a row struct with nested struct properties.

---

Add rows with nested struct data as JSON objects.

---

### Array Properties

Find a row struct with array properties.

---

Add rows with array data.

---

## Part 9: Stress Test - Rapid Operations

### Rapid Row Addition

Add 20 rows to a single table in rapid succession:
- StressRow_01 through StressRow_20

---

List all rows to verify count.

---

### Rapid Updates

Update 10 rows in rapid succession with different values.

---

### Rapid Reads

Get all 20 rows individually in rapid succession.

---

## Part 10: Error Handling

### Invalid Table Operations

Try to get info on a table that doesn't exist.

---

Try to add a row to a non-existent table.

---

### Invalid Row Operations

Try to get a row that doesn't exist.

---

Try to update a row that doesn't exist.

---

Try to remove a row that doesn't exist.

---

Try to rename a row that doesn't exist.

---

### Invalid Data

Try to add a row with missing required properties.

---

Try to add a row with wrong property types.

---

Try to add a row with an invalid property name.

---

### Duplicate Operations

Try to add a row with a name that already exists.

---

Try to rename a row to a name that already exists.

---

### Invalid Creation

Try to create a table with an invalid row struct name.

---

Try to create a table with no row struct specified.

---

Try to create a table in an invalid path.

---

## Part 11: Edge Cases

### Empty Operations

Create an empty table and try to:
- List rows (should return empty)
- Export JSON (should return empty object)
- Clear rows (should succeed even if empty)

---

### Special Row Names

Add rows with special names:
- "Row With Spaces"
- "Row_With_Underscores"
- "Row-With-Dashes"
- "Row.With.Dots"
- "123NumericStart"

---

Which names work and which fail?

---

### Large Data

Add a row with maximum length string values.

---

Add a row with many properties set at once.

---

## Part 12: Cross-Table Operations

### Copy Pattern

Get a row from one table.

---

Add that row's data to a different table (if compatible struct).

---

### Reference Check

Can rows in one table reference rows in another table?

---

## Part 13: Table Lifecycle

### Create and Delete Workflow

Create a temporary table DT_LifecycleTest.

---

Add rows to it.

---

Get full info.

---

Use manage_asset to delete the table.

---

Verify it's gone by trying to get info.

---

### Duplicate Table

Use manage_asset to duplicate DT_TestItems to DT_TestItems_Copy.

---

Verify the copy has all the same rows.

---

## Part 14: Complete Inventory

List ALL data tables we created during this test.

---

Get info on every table in /Game/Data/Test/Tables.

---

Count total rows across all test tables.

---

## Part 15: Final Verification

For each table created:
1. Get info showing row count
2. List all rows
3. Verify data integrity

---

Save all dirty assets using manage_asset save_all.

---

## Part 16: Cleanup (Optional)

Delete all test tables in /Game/Data/Test/Tables.

---

Verify cleanup by listing tables in that path.

---

## Part 17: Summary Report

Give me a final summary of:
1. How many different row struct types we discovered
2. How many data tables we successfully created
3. Total rows added across all tables
4. Which property types we successfully read/wrote
5. Which operations failed and why
6. Any limitations or issues discovered
7. Performance observations for bulk operations

---

## Quick Reference - Available Actions

| Action | Description | Required Params |
|--------|-------------|-----------------|
| `help` | Get help for this tool or a specific action | - |
| `search_row_types` | Find available row struct types | - |
| `list` | List data tables | - |
| `create` | Create a new data table | `table_path`, `row_struct` |
| `get_info` | Get table structure and row info | `table_path` |
| `get_row_struct` | Get row struct column definitions | `table_path` or `struct_name` |
| `list_rows` | List all rows in a table | `table_path` |
| `get_row` | Get a specific row | `table_path`, `row_name` |
| `add_row` | Add a new row | `table_path`, `row_name`, `data` |
| `update_row` | Update an existing row | `table_path`, `row_name`, `data` |
| `remove_row` | Remove a row | `table_path`, `row_name` |
| `rename_row` | Rename a row | `table_path`, `row_name`, `new_name` |
| `add_rows` | Bulk add multiple rows | `table_path`, `rows` |
| `clear_rows` | Remove all rows from table | `table_path` |
| `import_json` | Import rows from JSON | `table_path`, `json_data` |
| `export_json` | Export rows to JSON | `table_path` |

---
