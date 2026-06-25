---
name: umg-widgets/mvvm
description: MVVM ViewModel support in WidgetService — rules, creation types, binding modes, return-type field names, and full workflows to add ViewModels, create/list/remove bindings on a Widget Blueprint.
---

# UMG MVVM ViewModel Support

## Contents
- Rules
- CreationType options
- Binding mode options
- Field names (FWidgetViewModelInfo / FWidgetViewModelBindingInfo)
- Workflows (add VM, bind, full HUD, remove)

## Rules

1. The **ModelViewViewModel plugin** must be enabled (VibeUE enables it automatically).
2. ViewModel classes must inherit from `UMVVMViewModelBase` or implement `INotifyFieldValueChanged`.
3. Add the ViewModel (`add_view_model`) **before** creating bindings (`add_view_model_binding`).
4. The target widget component must already exist in the hierarchy before binding.
5. Property names must match exactly — use `list_properties` to discover available widget properties and
   confirm the ViewModel property name before binding.

## CreationType options

| Type | Purpose |
|------|---------|
| `CreateInstance` | Widget creates the ViewModel instance automatically (default, most common) |
| `Manual` | ViewModel is set manually at runtime via code |
| `GlobalViewModelCollection` | Shared ViewModel from the global collection |
| `PropertyPath` | ViewModel obtained from a property path on the widget |
| `Resolver` | Custom resolver class determines the ViewModel |

## Binding mode options

| Mode | Purpose |
|------|---------|
| `OneWayToDestination` | ViewModel → Widget (default, most common for display) |
| `TwoWay` | ViewModel ↔ Widget (editable inputs: sliders, text boxes) |
| `OneTimeToDestination` | ViewModel → Widget (once on init, no updates) |
| `OneWayToSource` | Widget → ViewModel (widget drives ViewModel) |
| `OneTimeToSource` | Widget → ViewModel (once on init) |

## Field names

**FWidgetViewModelInfo** — returned by `list_view_models()`:

| Field | Description |
|-------|-------------|
| `view_model_name` | Property name/alias of the ViewModel |
| `view_model_class_name` | Class name of the ViewModel |
| `creation_type` | How the ViewModel is created |
| `view_model_id` | GUID identifier |

**FWidgetViewModelBindingInfo** — returned by `list_view_model_bindings()`:

| Field | Description |
|-------|-------------|
| `binding_index` | Index for use with `remove_view_model_binding` |
| `source_path` | ViewModel property path |
| `destination_path` | Widget property path |
| `binding_mode` | Binding direction |
| `enabled` | Whether binding is active (the Python property is `enabled`, NOT `b_enabled`) |
| `binding_id` | GUID identifier |

> ⚠️ `remove_view_model(name)` **invalidates but does not delete** that ViewModel's bindings — they
> still appear in `list_view_model_bindings()` as dangling entries. Call `remove_view_model_binding`
> for each binding BEFORE `remove_view_model` if you need a clean list.

## Workflows

### Add a ViewModel

```python
import unreal

path = "/Game/UI/WBP_HUD"
unreal.WidgetService.add_view_model(path, "MyHealthViewModel", "HealthVM", "CreateInstance")

for vm in unreal.WidgetService.list_view_models(path):
    print(vm.view_model_name, vm.view_model_class_name, vm.creation_type)
```

### Bind a ViewModel property to a widget

```python
import unreal

path = "/Game/UI/WBP_HUD"
unreal.WidgetService.add_view_model_binding(
    path,
    "HealthVM",            # ViewModel name (as registered)
    "CurrentHealth",       # property on the ViewModel
    "HealthBar",           # widget component name
    "Percent",             # property on the widget
    "OneWayToDestination", # binding mode
)

for b in unreal.WidgetService.list_view_model_bindings(path):
    print(f"[{b.binding_index}] {b.source_path} -> {b.destination_path} ({b.binding_mode})")
```

### Full MVVM HUD setup

See `scripts/mvvm_hud.txt` for the complete runnable example (create WBP → build hierarchy →
add ViewModel → create bindings → save).

### Remove bindings and ViewModel

```python
import unreal

path = "/Game/UI/WBP_HUD"
unreal.WidgetService.remove_view_model_binding(path, 0)   # by index
unreal.WidgetService.remove_view_model(path, "HealthVM")  # also invalidates its bindings
```
