# create_data_asset.pyx — Create a Primary Data Asset and set properties.
#
# Sample script for the data-assets skill. Run via execute_python_code.
import unreal
das = unreal.DataAssetService

# class_name must be a PrimaryDataAsset subclass (C++ or Blueprint)
path = das.create_data_asset("MyItemDataAsset", "/Game/Data", "DA_Sword")
print("created:", path)
if path:
    das.set_property(path, "DisplayName", "Iron Sword")
    das.set_property(path, "Damage", "25")
    unreal.EditorAssetLibrary.save_asset(path)
