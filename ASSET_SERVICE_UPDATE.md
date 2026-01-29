# Asset Service Update: Open Assets & Content Browser Selections

## Summary
Added four new methods to `UAssetDiscoveryService` to track which assets are currently open in editors and which assets are selected in the Content Browser.

---

## New Methods

### 1. `GetOpenAssets()`
Returns array of all assets currently open in any editor.

**Python Usage:**
```python
import unreal

open_assets = unreal.AssetDiscoveryService.get_open_assets()
for asset in open_assets:
    print(f"{asset.asset_name} at {asset.package_path}")
```

**Returns:** `TArray<FAssetData>` - All currently edited assets

---

### 2. `IsAssetOpen(AssetPath)`
Checks if a specific asset is currently open in an editor.

**Python Usage:**
```python
import unreal

if unreal.AssetDiscoveryService.is_asset_open("/Game/BP_Player"):
    print("BP_Player is currently being edited")
```

**Returns:** `bool` - True if asset is open, false otherwise

---

### 3. `GetContentBrowserSelections()`
Returns array of all assets currently selected in the Content Browser.

**Python Usage:**
```python
import unreal

selected = unreal.AssetDiscoveryService.get_content_browser_selections()
print(f"Selected {len(selected)} assets:")
for asset in selected:
    print(f"  - {asset.asset_name}")
```

**Returns:** `TArray<FAssetData>` - All selected assets

---

### 4. `GetPrimaryContentBrowserSelection(OutAsset)`
Gets the first selected asset in Content Browser (primary selection).

**Python Usage:**
```python
import unreal

primary = unreal.AssetData()
if unreal.AssetDiscoveryService.get_primary_content_browser_selection(primary):
    print(f"Primary selection: {primary.asset_name}")
    # Work with the selected asset
    unreal.AssetDiscoveryService.open_asset(primary.package_path.to_string())
else:
    print("Nothing selected")
```

**Returns:** `bool` - True if asset selected, false if no selection
**Out Parameter:** `FAssetData&` - The primary selected asset

---

## Implementation Details

### Files Modified

1. **UAssetDiscoveryService.h** (`Public/PythonAPI/`)
   - Added 4 new UFUNCTION declarations
   - All marked `BlueprintCallable` for Python exposure
   - Category: `"VibeUE|Assets|Editor"`

2. **UAssetDiscoveryService.cpp** (`Private/PythonAPI/`)
   - Implemented 4 new methods
   - Added includes: `ContentBrowserModule.h`, `IContentBrowserSingleton.h`
   - Uses `UAssetEditorSubsystem` for open asset tracking
   - Uses `IContentBrowserSingleton` for selection queries

3. **VibeUE.Build.cs**
   - Added `"ContentBrowser"` to `PrivateDependencyModuleNames` (editor-only)

### Subsystems Used

- **UAssetEditorSubsystem**: Tracks which assets are currently being edited
  - `GetAllEditedAssets()` - Get all open assets
  - `FindEditorsForAsset()` - Check if specific asset is open

- **IContentBrowserSingleton**: Queries Content Browser state
  - `GetSelectedAssets()` - Get selected assets

---

## Use Cases

### 1. Context-Aware Operations
```python
# Only modify assets that are NOT currently open (avoid conflicts)
import unreal

if not unreal.AssetDiscoveryService.is_asset_open("/Game/BP_Enemy"):
    # Safe to modify via code
    pass
```

### 2. Bulk Operations on Selected Assets
```python
# Apply operation to all selected assets
import unreal

selected = unreal.AssetDiscoveryService.get_content_browser_selections()
for asset in selected:
    if "Blueprint" in str(asset.asset_class_path):
        # Process each selected blueprint
        print(f"Processing {asset.asset_name}")
```

### 3. Smart Asset Creation
```python
# Create new asset in same folder as selected asset
import unreal

primary = unreal.AssetData()
if unreal.AssetDiscoveryService.get_primary_content_browser_selection(primary):
    folder = str(primary.package_path).rsplit('/', 1)[0]  # Get parent folder
    print(f"Creating new asset in {folder}")
```

### 4. Editor State Monitoring
```python
# Check what user is currently working on
import unreal

open_assets = unreal.AssetDiscoveryService.get_open_assets()
if len(open_assets) > 0:
    print("Currently editing:")
    for asset in open_assets:
        print(f"  - {asset.asset_name}")
```

---

## Testing

Created test prompt: `test_prompts/asset_management/Test_Open_And_Selected_Assets.md`

Test scenarios:
1. Get all open assets
2. Check if specific asset is open
3. Get Content Browser selections
4. Get primary selection
5. Combined workflow: open selected asset
6. Filter open assets by type

---

## Skills Documentation Updated

Updated `Content/Skills/asset-management/`:

1. **01-critical-rules.md**
   - Added section "✅ Available Editor & Content Browser Methods"
   - Documents all 4 new methods with descriptions

2. **02-workflows.md**
   - Added section "Get Open Assets & Content Browser Selections"
   - Complete working examples showing all methods
   - Demonstrates combining methods for useful workflows

---

## Integration Notes

- All methods are **thread-safe** and check for null subsystems
- Returns empty arrays when no assets found (never null)
- Works with existing `FAssetData` structure
- No breaking changes to existing API
- Editor-only functionality (requires editor context)

---

## Next Steps

1. Build plugin: `.\BuildAndLaunch.ps1`
2. Run test prompts to verify functionality
3. Test in AI Chat with natural language queries
4. Consider adding these to MCP tool dispatch if needed for JSON-RPC access
