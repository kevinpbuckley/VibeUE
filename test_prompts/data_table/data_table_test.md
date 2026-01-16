# Data Table Management Tests - Comprehensive Stress Test

These tests should be run sequentially through the VibeUE chat interface in Unreal Engine. Each test builds on the previous ones. Check to see if assets exist before creating them. Delete them if they already exist, then create them if needed.

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

Add 5 rows at once to DT_TestItems:
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

## Part 6: Export/Import Pattern

### Export Pattern

Export all rows from DT_TestItems by iterating through each row and collecting the data.

---

### Import Pattern

Create a new table DT_ImportTest in /Game/Data/Test/Tables.

---

Import 3 rows with different mesh references into DT_ImportTest.

---

List rows to verify import worked.

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

Add rows with nested struct data.

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
- Export data (should return empty)
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

Delete the table.

---

Verify it's gone by trying to get info.

---

### Duplicate Table

Duplicate DT_TestItems to DT_TestItems_Copy.

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

Save all dirty assets.

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
