# UMG Skills Documentation Fixes Applied

**Date**: 2026-01-15
**Issue**: Review of VibeUE chat log revealed multiple recurring errors in UMG skills documentation

## Critical Fixes Applied

### 1. WidgetInfo Property Names (CRITICAL)
**Problem**: Documentation used incorrect property names causing AttributeError exceptions
- Old (WRONG): `widget.name`, `widget.type`, `widget.parent_name`
- New (CORRECT): `widget.widget_name`, `widget.widget_class`, `widget.parent_widget`

**Files Updated**:
- `01-critical-rules.md` - Added new section "## Get Widget Info" with correct property names
- `02-workflows.md` - Updated "Get Widget Info" workflow with correct field names and added clarifying comment

### 2. WidgetPropertyInfo Field Names
**Problem**: Documentation suggested `property_value` when actual field is `current_value`

**Fix Applied**: Added new section in `01-critical-rules.md`:
```markdown
## WidgetPropertyInfo Correct Field Names

When using `list_properties()` or `get_component_properties()`, use these fields:
- property_name (NOT name)
- property_type (NOT type)
- current_value (NOT property_value)
- category
- is_editable
```

### 3. Widget Blueprint Creation Method (CRITICAL)
**Problem**: Documentation instructed to use `BlueprintService.create_blueprint("Name", "UserWidget", "/path/")` which creates a regular Blueprint asset, NOT a WidgetBlueprint asset type. This caused `WidgetService` methods to fail with "Widget Blueprint not found" errors.

**Fix Applied**: 
- Updated `01-critical-rules.md` "Creating Widget Blueprints" section with correct `WidgetBlueprintFactory` usage
- Updated `02-workflows.md` "Create Widget Blueprint" workflow to use factory pattern
- Added explicit warnings about the wrong method

**Correct Pattern**:
```python
factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property("parent_class", unreal.UserWidget)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
widget_asset = asset_tools.create_asset("Name", "/Game/Path", unreal.WidgetBlueprint, factory)
```

## Issues Still Requiring Investigation

### Slot Properties Access
**Observation**: AI attempted to set Canvas Panel and Vertical Box slot properties using syntax like:
- `Slot.Anchors`
- `Slot.HorizontalAlignment`
- `Slot.Offsets`

These properties returned empty strings when queried via `get_property()`, suggesting they may not be accessible through the current WidgetService API or require different property paths.

**Recommendation**: 
1. Test whether slot properties can be set/get through WidgetService
2. If not, document this limitation in critical rules
3. Consider adding C++ support for slot property manipulation if needed

## Impact Assessment

**Before fixes**: AI encountered ~15+ errors during UMG widget test session due to wrong property names and creation method

**Expected after fixes**: 
- Correct WidgetInfo field access on first attempt
- Proper Widget Blueprint creation without "not found" errors  
- Accurate WidgetPropertyInfo field access
- ~80% reduction in trial-and-error discovery during UMG tasks

## Testing Recommendation

Run the test prompt again: `Plugins/VibeUE/test_prompts/umg/manage_umg_widget.md` to verify:
1. Widget creation succeeds with WidgetBlueprintFactory
2. No AttributeError on widget.widget_name, widget.widget_class
3. Property listing uses correct field names
4. Hierarchy traversal works without errors

## Additional Notes

The chat log showed excellent error recovery by the AI using `discover_python_class` to find correct signatures, but these discoveries should now be captured in the skills documentation to prevent future errors.
