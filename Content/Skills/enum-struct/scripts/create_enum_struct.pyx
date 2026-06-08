# create_enum_struct.pyx — Create a UserDefinedEnum + UserDefinedStruct.
#
# Sample script for the enum-struct skill. Run via execute_python_code.
import unreal
es = unreal.EnumStructService

# Enum
enum_path = es.create_enum("/Game/Data", "E_TeamColor")   # returns full path
print("enum:", enum_path)
es.add_enum_value(enum_path, "Red", "Red Team")
es.add_enum_value(enum_path, "Blue", "Blue Team")

# Struct (add members with the add_struct_* method — discover via discover_python_class)
struct_path = es.create_struct("/Game/Data", "F_ItemRow")
print("struct:", struct_path)
# e.g. es.add_struct_variable(struct_path, "Damage", "float", "0.0")
unreal.EditorAssetLibrary.save_asset(enum_path)
unreal.EditorAssetLibrary.save_asset(struct_path)
