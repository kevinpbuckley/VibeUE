# find_and_save.pyx — Search/find an asset, then duplicate + save it (manage_asset patterns).
#
# Sample script for the asset-management skill. PREFER the manage_asset MCP tool over Python for
# asset ops; this shows the equivalent AssetDiscoveryService/EditorAssetLibrary calls.
import unreal
ad = unreal.AssetDiscoveryService

# Find by exact path (returns AssetData or None — no out-param in Python)
hit = ad.find_asset_by_path("/Game/Blueprints/BP_Player")
print("found:", hit.asset_name if hit else None)

# Search all mounted roots (/Game, /Engine, plugins) by name pattern + optional type (only 2 params)
for a in ad.search_assets("BP_", "Blueprint")[:10]:
    print(" ", a.asset_name, a.package_path)

# Duplicate + save via EditorAssetLibrary (true copy)
src = "/Game/Blueprints/BP_Player"
if unreal.EditorAssetLibrary.does_asset_exist(src):
    dup = unreal.EditorAssetLibrary.duplicate_asset(src, src + "_Copy")
    unreal.EditorAssetLibrary.save_asset(src + "_Copy")
    print("duplicated:", dup is not None)
