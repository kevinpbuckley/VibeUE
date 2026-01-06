# Title: Add Variables to Blueprint
# Category: blueprint
# Tags: blueprint, variable, member, property, add
# Description: Add member variables to a blueprint with different types and default values

import unreal

# Load the blueprint
bp = unreal.load_asset('/Game/Blueprints/BP_Player')

# ============================================================================
# Add Float Variable
# ============================================================================
float_type = unreal.EdGraphPinType()
float_type.import_text('(PinCategory="real",PinSubCategory="float")')
unreal.BlueprintEditorLibrary.add_member_variable(bp, 'Health', float_type)
unreal.BlueprintEditorLibrary.set_blueprint_variable_default_value(bp, 'Health', '100.0')

# ============================================================================
# Add Boolean Variable
# ============================================================================
bool_type = unreal.EdGraphPinType()
bool_type.import_text('(PinCategory="bool")')
unreal.BlueprintEditorLibrary.add_member_variable(bp, 'IsAlive', bool_type)
unreal.BlueprintEditorLibrary.set_blueprint_variable_default_value(bp, 'IsAlive', 'true')

# ============================================================================
# Add Integer Variable
# ============================================================================
int_type = unreal.EdGraphPinType()
int_type.import_text('(PinCategory="int")')
unreal.BlueprintEditorLibrary.add_member_variable(bp, 'Score', int_type)
unreal.BlueprintEditorLibrary.set_blueprint_variable_default_value(bp, 'Score', '0')

# ============================================================================
# Add String Variable
# ============================================================================
string_type = unreal.EdGraphPinType()
string_type.import_text('(PinCategory="string")')
unreal.BlueprintEditorLibrary.add_member_variable(bp, 'PlayerName', string_type)
unreal.BlueprintEditorLibrary.set_blueprint_variable_default_value(bp, 'PlayerName', '"Player"')

# ============================================================================
# Add Vector Variable (Struct)
# ============================================================================
vector_type = unreal.EdGraphPinType()
vector_type.import_text('(PinCategory="struct",PinSubCategoryObject="/Script/CoreUObject.Vector")')
unreal.BlueprintEditorLibrary.add_member_variable(bp, 'SpawnLocation', vector_type)
# Vector default: (X=0.0,Y=0.0,Z=0.0)
unreal.BlueprintEditorLibrary.set_blueprint_variable_default_value(bp, 'SpawnLocation', '(X=0.0,Y=0.0,Z=100.0)')

# ============================================================================
# Add Actor Reference Variable
# ============================================================================
actor_type = unreal.EdGraphPinType()
actor_type.import_text('(PinCategory="object",PinSubCategoryObject="/Script/Engine.Actor")')
unreal.BlueprintEditorLibrary.add_member_variable(bp, 'TargetActor', actor_type)
# Object references default to None

# ============================================================================
# Add Array Variable
# ============================================================================
float_array_type = unreal.EdGraphPinType()
float_array_type.import_text('(PinCategory="real",PinSubCategory="float",ContainerType="Array")')
unreal.BlueprintEditorLibrary.add_member_variable(bp, 'DamageMultipliers', float_array_type)
# Arrays default to empty

# ============================================================================
# Make Variable Editable in Details Panel
# ============================================================================
unreal.BlueprintEditorLibrary.set_blueprint_variable_instance_editable(bp, 'Health', True)
unreal.BlueprintEditorLibrary.set_blueprint_variable_instance_editable(bp, 'IsAlive', True)

# ============================================================================
# Make Variable Exposed on Spawn
# ============================================================================
unreal.BlueprintEditorLibrary.set_blueprint_variable_expose_on_spawn(bp, 'SpawnLocation', True)

# Save and compile
unreal.EditorAssetLibrary.save_asset(bp.get_path_name())
unreal.BlueprintEditorLibrary.compile_blueprint(bp)

print(f'Added variables to {bp.get_name()}')
