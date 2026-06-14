# create_data_table.pyx — Create a Data Table from a row struct and add rows.
#
# Sample script for the data-tables skill. Run via execute_python_code.
# row_struct_name is a UserDefinedStruct or C++ FTableRowBase struct name.
import unreal
dts = unreal.DataTableService

path = dts.create_data_table("F_ItemRow", "/Game/Data", "DT_Items")
print("created:", path)
if path:
    dts.add_row(path, "Sword", '{"Damage": 25.0, "Name": "Iron Sword"}')
    dts.add_row(path, "Axe", '{"Damage": 30.0, "Name": "Battle Axe"}')
    unreal.EditorAssetLibrary.save_asset(path)
