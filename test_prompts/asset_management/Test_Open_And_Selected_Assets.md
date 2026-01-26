# Test: Asset Discovery - Open & Selected Assets

## Objective
Test the new asset discovery methods for tracking open assets and Content Browser selections.

---

## Test Steps

### 1. Get All Open Assets
```
Show me all assets currently open in editors
```

**Expected:**
- Returns list of all assets currently being edited
- Shows asset names and paths
- Empty list if nothing is open

---

### 2. Check Specific Asset is Open
```
Is BP_Purple_Test_Cube currently open in an editor?
```

**Expected:**
- Returns true if the blueprint is open
- Returns false if it's not open
- Handles asset not found gracefully

---

### 3. Get Content Browser Selections
```
Show me what assets are selected in the Content Browser right now
```

**Expected:**
- Returns list of all selected assets
- Shows names, types, and paths
- Empty list if nothing selected
- Handles multiple selections

---

### 4. Get Primary Selection
```
What is the primary asset selected in Content Browser?
```

**Expected:**
- Returns the first selected asset
- Shows asset details
- Returns None/false if nothing selected

---

### 5. Combined Workflow - Open Selected Asset
```
Get the currently selected asset in Content Browser and open it in the editor
```

**Expected:**
- Gets primary selection
- Opens that asset
- Handles no selection case
- Verifies asset is now in open assets list

---

### 6. List All Open Blueprints
```
Show me all Blueprint assets that are currently open in editors
```

**Expected:**
- Filters open assets by type
- Only shows Blueprints
- Handles no open blueprints case

---

## Success Criteria

✅ All methods execute without errors
✅ Returns correct data structures (FAssetData arrays)
✅ Handles edge cases (nothing open, nothing selected)
✅ Asset properties (asset_name, package_path) are accessible
✅ Methods integrate smoothly with existing AssetDiscoveryService functions
