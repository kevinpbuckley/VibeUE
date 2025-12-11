# Blueprint Component Tool - Issues Log

This document tracks issues found during testing of the manage_blueprint_component MCP tool.

**Test Date:** December 10, 2025

---

## Issues Found

### 1. search_types Action Does Not Filter by Search Text ⚠️

**Test Case:** `action="search_types"` with `search_text="SpotLight"`
- **Expected:** Return only component types matching "SpotLight"
- **Actual:** Returns all 87 component types, ignoring the search_text filter
- **Severity:** Minor - workaround is to filter results client-side

---

### 2. Intermittent Timeout Errors ⚠️

**Test Cases:** Various operations (create component, set_property, delete asset)
- **Issue:** Occasional "Timeout receiving Unreal response" errors
- **Frequency:** ~3 occurrences during testing (~10% of calls)
- **Workaround:** Retry the operation - it typically succeeds on second attempt
- **Severity:** Minor - transient issue, retries work

---

### 3. LightColor Property - JSON Object Format Not Accepted ⚠️

**Test Case:** `set_property` with `LightColor` using JSON object `{"R": 255, "G": 200, "B": 150, "A": 255}`
- **Expected:** Property set successfully
- **Actual:** "Failed to set property 'LightColor' value"
- **Workaround:** Use array format `[255, 200, 150, 255]` instead
- **Severity:** Minor - array format works

---

### 4. Cascade Delete (remove_children) Does Not Remove Child Components ⚠️

**Test Case:** Delete `LightRig` with `remove_children=true` when it has children `MainLight` and `FillLight`
- **Expected:** LightRig and its children are deleted
- **Actual:** LightRig is deleted, but MainLight remains (orphaned with parent="LightRig" still set)
- **Severity:** Medium - hierarchy integrity issue

---

## Tests Passed ✅

### Setup
- ✅ Create BP_ComponentTest with Actor parent

### Test 1: Discovery
- ⚠️ search_types - returns results but doesn't filter (see Issue #1)
- ✅ get_info for SpotLightComponent - returns 131 properties

### Test 2: Creation
- ✅ Create BP_LightTest
- ✅ List components - shows DefaultSceneRoot
- ✅ Create SpotLightComponent "MainLight"
- ✅ Create SpotLightComponent "FillLight"
- ✅ List to verify - shows both components

### Test 3: Properties
- ✅ get_property Intensity from MainLight (value: 5000)
- ✅ set_property Intensity to 5000
- ⚠️ set_property LightColor with JSON object - FAILED (see Issue #3)
- ✅ set_property LightColor with array format `[255, 200, 150, 255]`
- ✅ get_all_properties - returns 115 properties
- ✅ set_property InnerConeAngle to 25
- ✅ set_property OuterConeAngle to 60

### Test 4: Hierarchy
- ✅ Create SceneComponent "LightRig"
- ✅ Reparent MainLight to LightRig
- ✅ Reparent FillLight to LightRig
- ✅ Reorder components
- ✅ List verifies hierarchy

### Test 5: Comparison
- ✅ Create BP_LightTest2
- ✅ Set different Intensity (8000)
- ✅ compare_properties - correctly identifies 4 differences

### Test 6: Deletion
- ✅ Delete FillLight
- ⚠️ Delete LightRig with remove_children=true - parent deleted but child orphaned (see Issue #4)

### Test 7: Component Types
- ✅ Create AudioComponent "SoundEffect"
- ✅ Create ParticleSystemComponent "VFX"
- ✅ Create NiagaraComponent "ModernVFX"
- ✅ List all - shows 3 components

### Cleanup
- ✅ All test blueprints deleted successfully

---

## Summary

**Total Test Cases:** ~30
**Passed:** ~26
**Issues Found:** 4 (1 medium, 3 minor)
**Pass Rate:** ~87%

The manage_blueprint_component tool works well overall. Main issues are:
1. search_types filter not working
2. Intermittent timeouts (retry works)
3. JSON object format for FColor not accepted (use array)
4. Cascade delete doesn't actually cascade

---

## Additional Fixes Made During Testing

### Enhanced Input Tool - Service Routing Bug ✅ FIXED

**Issue:** All enhanced input actions were failing with "Unknown reflection action" because the Python code was defaulting to `service="reflection"` regardless of the action prefix.

**Root Cause:** The `manage_enhanced_input` Python tool didn't auto-determine the correct service from action name prefixes like `action_`, `mapping_`, `modifier_`, etc.

**Fix Applied:** Added logic to auto-detect service from action prefix:
- `action_*` → service="action"
- `mapping_*` → service="mapping"  
- `modifier_*` → service="modifier"
- `trigger_*` → service="trigger"
- `ai_*` → service="ai"
- `reflection_*` → service="reflection"

**File Modified:** `e:\az-dev-ops\FPS57\Plugins\VibeUE\Content\Python\tools\manage_enhanced_input.py`

**Result:** Enhanced input actions now work correctly:
- `action_list` returns 11 input actions
- `mapping_list_contexts` returns 4 mapping contexts
- `mapping_get_mappings` returns key bindings
