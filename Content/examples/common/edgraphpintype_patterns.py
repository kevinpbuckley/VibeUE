# Title: EdGraphPinType Common Patterns
# Category: common
# Tags: EdGraphPinType, types, float, bool, int, string, object, array
# Description: Common patterns for creating EdGraphPinType for blueprint variables and function parameters

import unreal

# EdGraphPinType fields are protected, so we use import_text() to construct them
# Use export_text() on an existing EdGraphPinType to see all available fields

# ============================================================================
# BASIC TYPES
# ============================================================================

# Float (real number)
float_type = unreal.EdGraphPinType()
float_type.import_text('(PinCategory="real",PinSubCategory="float")')

# Double (real number, double precision)
double_type = unreal.EdGraphPinType()
double_type.import_text('(PinCategory="real",PinSubCategory="double")')

# Integer
int_type = unreal.EdGraphPinType()
int_type.import_text('(PinCategory="int")')

# Int64 (large integer)
int64_type = unreal.EdGraphPinType()
int64_type.import_text('(PinCategory="int64")')

# Boolean
bool_type = unreal.EdGraphPinType()
bool_type.import_text('(PinCategory="bool")')

# Byte (0-255)
byte_type = unreal.EdGraphPinType()
byte_type.import_text('(PinCategory="byte")')

# String
string_type = unreal.EdGraphPinType()
string_type.import_text('(PinCategory="string")')

# Name
name_type = unreal.EdGraphPinType()
name_type.import_text('(PinCategory="name")')

# Text (localized string)
text_type = unreal.EdGraphPinType()
text_type.import_text('(PinCategory="text")')

# ============================================================================
# STRUCT TYPES
# ============================================================================

# Vector (3D)
vector_type = unreal.EdGraphPinType()
vector_type.import_text('(PinCategory="struct",PinSubCategoryObject="/Script/CoreUObject.Vector")')

# Rotator
rotator_type = unreal.EdGraphPinType()
rotator_type.import_text('(PinCategory="struct",PinSubCategoryObject="/Script/CoreUObject.Rotator")')

# Transform
transform_type = unreal.EdGraphPinType()
transform_type.import_text('(PinCategory="struct",PinSubCategoryObject="/Script/CoreUObject.Transform")')

# Color (LinearColor)
color_type = unreal.EdGraphPinType()
color_type.import_text('(PinCategory="struct",PinSubCategoryObject="/Script/CoreUObject.LinearColor")')

# ============================================================================
# OBJECT TYPES
# ============================================================================

# Object reference (generic UObject)
object_type = unreal.EdGraphPinType()
object_type.import_text('(PinCategory="object",PinSubCategoryObject="/Script/CoreUObject.Object")')

# Actor reference
actor_type = unreal.EdGraphPinType()
actor_type.import_text('(PinCategory="object",PinSubCategoryObject="/Script/Engine.Actor")')

# Component reference
component_type = unreal.EdGraphPinType()
component_type.import_text('(PinCategory="object",PinSubCategoryObject="/Script/Engine.ActorComponent")')

# Specific class reference (example: StaticMeshComponent)
mesh_component_type = unreal.EdGraphPinType()
mesh_component_type.import_text('(PinCategory="object",PinSubCategoryObject="/Script/Engine.StaticMeshComponent")')

# ============================================================================
# ARRAY TYPES (CONTAINERS)
# ============================================================================

# Array of floats
float_array_type = unreal.EdGraphPinType()
float_array_type.import_text('(PinCategory="real",PinSubCategory="float",ContainerType="Array")')

# Array of integers
int_array_type = unreal.EdGraphPinType()
int_array_type.import_text('(PinCategory="int",ContainerType="Array")')

# Array of strings
string_array_type = unreal.EdGraphPinType()
string_array_type.import_text('(PinCategory="string",ContainerType="Array")')

# Array of Actors
actor_array_type = unreal.EdGraphPinType()
actor_array_type.import_text('(PinCategory="object",PinSubCategoryObject="/Script/Engine.Actor",ContainerType="Array")')

# ============================================================================
# REFERENCE TYPES
# ============================================================================

# Float by reference (for output parameters)
float_ref_type = unreal.EdGraphPinType()
float_ref_type.import_text('(PinCategory="real",PinSubCategory="float",bIsReference=True)')

# Object reference by reference
object_ref_type = unreal.EdGraphPinType()
object_ref_type.import_text('(PinCategory="object",PinSubCategoryObject="/Script/CoreUObject.Object",bIsReference=True)')

# ============================================================================
# EXAMPLE: Adding a variable to a blueprint
# ============================================================================

# bp = unreal.load_asset('/Game/Blueprints/BP_MyActor')
# pin_type = unreal.EdGraphPinType()
# pin_type.import_text('(PinCategory="real",PinSubCategory="float")')
# success = unreal.BlueprintEditorLibrary.add_member_variable(bp, 'Health', pin_type)
# 
# # Set default value
# unreal.BlueprintEditorLibrary.set_blueprint_variable_default_value(bp, 'Health', '100.0')

# ============================================================================
# TIPS
# ============================================================================

# 1. To discover fields in any EdGraphPinType, use:
#    pin_type = unreal.EdGraphPinType()
#    print(pin_type.export_text())

# 2. For custom struct types, find the full path using:
#    struct = unreal.load_object(None, '/Script/YourModule.YourStruct')
#    print(struct.get_path_name())

# 3. For custom class types, use:
#    cls = unreal.load_class(None, '/Script/YourModule.YourClass')
#    print(cls.get_path_name())
