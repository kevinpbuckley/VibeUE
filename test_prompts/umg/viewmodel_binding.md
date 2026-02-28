# UMG ViewModel Binding Test Prompt

## Purpose
Tests the MVVM ViewModel integration in WidgetService: adding ViewModels, creating bindings, listing, and removing.

## Prerequisites
- A ViewModel class must exist in the project (C++ or Blueprint ViewModel inheriting from `UMVVMViewModelBase`)
- If no ViewModel class exists, create a Blueprint ViewModel first using the editor

---

## Test Sequence

### Phase 1: Setup — Create Widget Blueprint

Create a Widget Blueprint called `WBP_VMTest` in `/Game/UI/Tests/` with:
- Root: `CanvasPanel` named `RootCanvas`
- Child: `ProgressBar` named `HealthBar` (parent: `RootCanvas`)
- Child: `TextBlock` named `ScoreText` (parent: `RootCanvas`)
- Child: `Slider` named `VolumeSlider` (parent: `RootCanvas`)

**Expected**: Widget hierarchy with 4 components created successfully.

### Phase 2: List ViewModels (Empty)

List all ViewModels on `WBP_VMTest`.

```
list_view_models("/Game/UI/Tests/WBP_VMTest")
```

**Expected**: Empty array — no ViewModels registered yet.

### Phase 3: Add ViewModel

Add a ViewModel to the widget. If a test ViewModel class exists (e.g., `VM_GameHUD`), use it. Otherwise, use any available ViewModel class.

```
add_view_model("/Game/UI/Tests/WBP_VMTest", "<ViewModelClass>", "GameHudVM", "CreateInstance")
```

**Expected**: Returns `True`. ViewModel added successfully.

### Phase 4: List ViewModels (Verify Add)

List ViewModels again to verify the add.

```
list_view_models("/Game/UI/Tests/WBP_VMTest")
```

**Expected**: One entry with:
- `view_model_name` = "GameHudVM"
- `view_model_class_name` = the class used
- `creation_type` = "CreateInstance"

### Phase 5: Add ViewModel Binding — OneWayToDestination

Create a binding from the ViewModel to the HealthBar's Percent property.

```
add_view_model_binding(
    "/Game/UI/Tests/WBP_VMTest",
    "GameHudVM",
    "<HealthProperty>",   # e.g., "Health" or "HealthPercent"
    "HealthBar",
    "Percent",
    "OneWayToDestination"
)
```

**Expected**: Returns `True`. Binding created.

### Phase 6: Add ViewModel Binding — TwoWay

Create a two-way binding for the volume slider.

```
add_view_model_binding(
    "/Game/UI/Tests/WBP_VMTest",
    "GameHudVM",
    "<VolumeProperty>",   # e.g., "Volume"
    "VolumeSlider",
    "Value",
    "TwoWay"
)
```

**Expected**: Returns `True`. Two-way binding created.

### Phase 7: List ViewModel Bindings

List all bindings to verify both were created.

```
list_view_model_bindings("/Game/UI/Tests/WBP_VMTest")
```

**Expected**: Two binding entries:
1. Index 0: Source contains health property, Destination contains HealthBar.Percent, Mode = OneWayToDestination
2. Index 1: Source contains volume property, Destination contains VolumeSlider.Value, Mode = TwoWay

### Phase 8: Remove ViewModel Binding

Remove the first binding (index 0).

```
remove_view_model_binding("/Game/UI/Tests/WBP_VMTest", 0)
```

**Expected**: Returns `True`. Only one binding remains.

### Phase 9: Verify Binding Removal

List bindings again.

```
list_view_model_bindings("/Game/UI/Tests/WBP_VMTest")
```

**Expected**: One binding entry (the TwoWay slider binding, now at index 0).

### Phase 10: Remove ViewModel

Remove the ViewModel entirely.

```
remove_view_model("/Game/UI/Tests/WBP_VMTest", "GameHudVM")
```

**Expected**: Returns `True`. ViewModel removed.

### Phase 11: Verify ViewModel Removal

List ViewModels again.

```
list_view_models("/Game/UI/Tests/WBP_VMTest")
```

**Expected**: Empty array.

---

## Error Case Tests

### E1: Add ViewModel with Invalid Class

```
add_view_model("/Game/UI/Tests/WBP_VMTest", "NonExistentVM", "TestVM")
```

**Expected**: Returns `False`. Log warning about class not found.

### E2: Add Duplicate ViewModel Name

```
add_view_model("/Game/UI/Tests/WBP_VMTest", "<ValidClass>", "DuplicateVM")
add_view_model("/Game/UI/Tests/WBP_VMTest", "<ValidClass>", "DuplicateVM")
```

**Expected**: First call returns `True`, second returns `False` (name already exists).

### E3: Add Binding Without ViewModel

```
add_view_model_binding("/Game/UI/Tests/WBP_VMTest", "NonExistentVM", "Prop", "HealthBar", "Percent")
```

**Expected**: Returns `False`. ViewModel not found.

### E4: Add Binding With Invalid Widget

```
add_view_model_binding("/Game/UI/Tests/WBP_VMTest", "GameHudVM", "Health", "NonExistentWidget", "Percent")
```

**Expected**: Returns `False`. Widget not found.

### E5: Add Binding With Invalid Property

```
add_view_model_binding("/Game/UI/Tests/WBP_VMTest", "GameHudVM", "NonExistentProp", "HealthBar", "Percent")
```

**Expected**: Returns `False`. Property not found on ViewModel.

### E6: Remove Binding Out of Range

```
remove_view_model_binding("/Game/UI/Tests/WBP_VMTest", 999)
```

**Expected**: Returns `False`. Index out of range.

### E7: Remove Non-Existent ViewModel

```
remove_view_model("/Game/UI/Tests/WBP_VMTest", "DoesNotExist")
```

**Expected**: Returns `False`. ViewModel not found.

---

## Cleanup

Delete the test widget `WBP_VMTest` after all tests complete:

```python
unreal.EditorAssetLibrary.delete_asset("/Game/UI/Tests/WBP_VMTest")
```
