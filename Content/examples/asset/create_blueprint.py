# Title: Create Blueprint Asset
# Category: asset
# Tags: asset, blueprint, create, BlueprintFactory
# Description: Create a new blueprint asset from an actor class

import unreal

# ============================================================================
# Create Actor Blueprint
# ============================================================================
factory = unreal.BlueprintFactory()
factory.set_editor_property('parent_class', unreal.Actor)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
bp = asset_tools.create_asset(
    'BP_MyNewActor',           # Asset name
    '/Game/Blueprints',        # Package path
    unreal.Blueprint,          # Asset class
    factory                    # Factory to use
)

if bp:
    print(f'Created blueprint: {bp.get_name()}')
    
    # Save the new asset
    saved = unreal.EditorAssetLibrary.save_asset(bp.get_path_name())
    print(f'Saved: {saved}')
else:
    print('Failed to create blueprint')

# ============================================================================
# Create Character Blueprint
# ============================================================================
char_factory = unreal.BlueprintFactory()
char_factory.set_editor_property('parent_class', unreal.Character)

char_bp = asset_tools.create_asset(
    'BP_MyCharacter',
    '/Game/Blueprints',
    unreal.Blueprint,
    char_factory
)

if char_bp:
    print(f'Created character blueprint: {char_bp.get_name()}')
    unreal.EditorAssetLibrary.save_asset(char_bp.get_path_name())

# ============================================================================
# Create Pawn Blueprint
# ============================================================================
pawn_factory = unreal.BlueprintFactory()
pawn_factory.set_editor_property('parent_class', unreal.Pawn)

pawn_bp = asset_tools.create_asset(
    'BP_MyPawn',
    '/Game/Blueprints',
    unreal.Blueprint,
    pawn_factory
)

if pawn_bp:
    print(f'Created pawn blueprint: {pawn_bp.get_name()}')
    unreal.EditorAssetLibrary.save_asset(pawn_bp.get_path_name())
