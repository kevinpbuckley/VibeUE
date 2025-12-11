# manage_asset Test Prompts

## Setup
Create Blueprint "BP_AssetSearchTest" with parent "Actor"
Create Blueprint "WBP_AssetSearchTest" with parent "UserWidget"

## Test 1: Search Assets
Search for assets with search_term="radar", asset_type="Widget"
Search for assets with search_term="background", asset_type="Texture2D"
Search all Blueprints with asset_type="Blueprint"
Search in path="/Game/UI" for widget assets

## Test 2: Search Options
Search with include_engine_content=true
Search with max_results=10

## Test 3: Duplicate Assets
Duplicate /Game/Blueprints/BP_AssetSearchTest to /Game/Blueprints with new_name "BP_AssetSearchTest_Dup1"
Duplicate /Game/Blueprints/WBP_AssetSearchTest to /Game/Blueprints with new_name "WBP_AssetSearchTest_Dup1"
Search for "_Dup1"

## Test 4: Open and Save
Open /Game/Blueprints/BP_AssetSearchTest in editor
Open /Game/Blueprints/WBP_AssetSearchTest in editor
Save asset
Save all with prompt_user=false

## Test 5: List References
List references for /Game/Input/Actions/IA_Move
List references for /Game/Blueprints/BP_AssetSearchTest

## Cleanup
Delete /Game/Blueprints/BP_AssetSearchTest with force_delete=True and show_confirmation=False
Delete /Game/Blueprints/WBP_AssetSearchTest with force_delete=True and show_confirmation=False
Delete /Game/Blueprints/BP_AssetSearchTest_Dup1 with force_delete=True and show_confirmation=False
Delete /Game/Blueprints/WBP_AssetSearchTest_Dup1 with force_delete=True and show_confirmation=False

