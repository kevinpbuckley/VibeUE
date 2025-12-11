# Enhanced Input Tool - Issues Log

This document tracks issues found during testing of the manage_enhanced_input MCP tool.

**Test Date:** December 10, 2025

---

## Issues Found

### Issue 1: Python valid_actions list mismatch with C++

**Status**: ✅ FIXED

**Description**: The Python `valid_actions` list didn't match C++ action handlers:
- Python had `reflection_get_types` but C++ expects `reflection_discover_types`
- Python missing `mapping_get_property`, `mapping_get_available_keys`, `mapping_get_modifiers`, `mapping_get_available_modifier_types`, `mapping_get_triggers`, `mapping_get_available_trigger_types`

**Fix Applied**: Updated `manage_enhanced_input.py` valid_actions list to match C++ handlers.

**Note**: Changes require MCP server restart to take effect.

---

### Issue 2: Non-existent actions in Python valid_actions

**Status**: ✅ FIXED

**Description**: Python allows actions that don't exist in C++:
- `action_delete` - Removed (use `manage_asset(action="delete")`)
- `action_set_property` - Fixed to `action_configure` (matches C++)
- `action_rename` - Removed (not implemented)
- `mapping_delete_context` - Removed (use `manage_asset(action="delete")`)
- Deprecated services kept for backwards compatibility with redirect messages

**Fix Applied**: Updated `manage_enhanced_input.py`:
- Changed `action_set_property` to `action_configure`
- Removed `action_delete`, `action_rename`, `mapping_delete_context`
- Added `mapping_update_context`

---

### Issue 3: Timeout on modifier operations

**Status**: ✅ NOT A BUG (User Error)

**Description**: `mapping_add_modifier` appeared to timeout when extra params were passed.

**Root Cause**: Test incorrectly passed `action_path` parameter which is not needed.
Correct usage requires only: `context_path`, `mapping_index`, `modifier_type`

**Verified Working**:
```
action="mapping_add_modifier"
context_path="/Game/Input.IMC_TestContext"
mapping_index=0
modifier_type="Negate"
```
Returns: `{"success":true,"message":"Modifier 'Negate' added to mapping 0"}`

---

### Issue 4: Asset path inconsistency after creation

**Status**: ✅ FIXED

**Description**: When creating assets with `asset_path="/Game/Input/Actions"`:
- User expects: `/Game/Input/Actions/IA_TestReload.IA_TestReload`
- Actual result: `/Game/Input/Actions.IA_TestReload` (wrong)

**Root Cause**: Package creation didn't include asset name in the path.

**Fix Applied**: Updated `InputMappingService.cpp` and `InputActionService.cpp`:
- Changed package path from `AssetPath` to `AssetPath/AssetName`
- Now follows UE convention: `/Game/Input/Actions/IA_Jump.IA_Jump`

**Verified Working**: New assets created after fix have correct paths.

---

### Issue 5: mapping_delete_context not implemented

**Status**: ✅ RESOLVED (By Design)

**Description**: `mapping_delete_context` returns "Unknown mapping action".

**Resolution**: Per design, delete operations should use `manage_asset`:
```
manage_asset(action="delete", asset_path="/Game/Input/IMC_TestContext")
```

**Fix Applied**: Removed `mapping_delete_context` from Python valid_actions list.

---

## Test Results Summary (Post-Fix)

| Test | Status | Notes |
|------|--------|-------|
| reflection_discover_types | ✅ FIXED | Python updated, needs MCP restart |
| action_list | ✅ PASS | Works correctly |
| action_create | ✅ FIXED | Path format now correct |
| action_configure | ✅ PASS | Renamed from action_set_property |
| action_delete | ➡️ REDIRECT | Use manage_asset(action="delete") |
| mapping_list_contexts | ✅ PASS | Works correctly |
| mapping_create_context | ✅ FIXED | Path format now correct |
| mapping_add_key_mapping | ✅ PASS | Works correctly |
| mapping_add_modifier | ✅ PASS | Works with correct params |
| mapping_add_trigger | ✅ PASS | Works correctly |
| mapping_delete_context | ➡️ REDIRECT | Use manage_asset(action="delete") |

**All 5 issues resolved!**