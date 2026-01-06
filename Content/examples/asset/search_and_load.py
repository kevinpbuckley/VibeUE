# Title: Search and Load Assets
# Category: asset
# Tags: asset, search, load, AssetRegistry, find
# Description: Search for assets in the project using the Asset Registry

import unreal

# ============================================================================
# Get Asset Registry
# ============================================================================
asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()

# ============================================================================
# Search for Blueprint Assets
# ============================================================================
filter = unreal.ARFilter(
    class_names=['Blueprint'],
    package_paths=['/Game'],
    recursive_paths=True
)

assets = asset_registry.get_assets(filter)
print(f'Found {len(assets)} Blueprint assets:')
for asset_data in assets[:5]:  # Show first 5
    print(f'  - {asset_data.asset_name} at {asset_data.package_name}')

# ============================================================================
# Search by Name Pattern
# ============================================================================
# Note: Asset registry doesn't have built-in name filtering
# We filter the results ourselves
search_term = 'Player'
matching_assets = [a for a in assets if search_term.lower() in a.asset_name.lower()]
print(f'\nAssets matching "{search_term}": {len(matching_assets)}')
for asset_data in matching_assets:
    print(f'  - {asset_data.asset_name}')

# ============================================================================
# Load Specific Asset
# ============================================================================
if matching_assets:
    first_match = matching_assets[0]
    # Load the asset using its package name
    asset = unreal.load_asset(f'{first_match.package_name}.{first_match.asset_name}')
    if asset:
        print(f'\nLoaded: {asset.get_name()}')
        print(f'  Path: {asset.get_path_name()}')
        print(f'  Class: {asset.get_class().get_name()}')

# ============================================================================
# Search for Static Meshes
# ============================================================================
mesh_filter = unreal.ARFilter(
    class_names=['StaticMesh'],
    package_paths=['/Game'],
    recursive_paths=True
)

meshes = asset_registry.get_assets(mesh_filter)
print(f'\n\nFound {len(meshes)} Static Mesh assets')

# ============================================================================
# Check if Asset Exists
# ============================================================================
asset_path = '/Game/Blueprints/BP_Player'
exists = unreal.EditorAssetLibrary.does_asset_exist(asset_path)
print(f'\n\nDoes {asset_path} exist? {exists}')

# ============================================================================
# List Assets in Specific Folder
# ============================================================================
folder_assets = unreal.EditorAssetLibrary.list_assets('/Game/Blueprints', recursive=False)
print(f'\nAssets in /Game/Blueprints: {len(folder_assets)}')
for path in folder_assets[:5]:
    print(f'  - {path}')
