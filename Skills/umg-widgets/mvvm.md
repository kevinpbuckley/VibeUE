---
name: umg-widgets/mvvm
description: MVVM ViewModel support on Epic's MVVMToolset — rules, binding modes, the GUID-keyed binding list, the AddViewModelToWidget alias/CreationType limitation and its workaround, context rules for CreateViewBinding, and full workflows to add ViewModels and create/list/remove bindings on a Widget Blueprint.
---

# UMG MVVM ViewModel Support

MVVM is now **Epic's `MVVMToolset`** (31 tools), not `WidgetService`. One `WidgetService` method
survives: `remove_view_model`, because MVVM can add a ViewModel to a widget but never remove one.

`toolset_name` is `MVVMToolset.MVVMToolset`. Its tools are `BlueprintCallable`, so they are reachable
both ways — `call_tool` for a single round-trip, or `unreal.MVVMToolset.*` inside `execute_python`.
Every tool is marked `Experimental`.

## Contents
- Rules
- Known limitation: alias name and CreationType
- Binding mode options
- The tool set
- Context rules (`SourceContext` / `DestinationContext`)
- Field names (`FMVVMViewBindingDescription`)
- Workflows (add VM, bind, full HUD, remove)

## Rules

1. The **ModelViewViewModel plugin** must be enabled.
2. ViewModel classes must inherit from `UMVVMViewModelBase` or implement `INotifyFieldValueChanged`.
3. Add the ViewModel (`AddViewModelToWidget`) **before** creating bindings (`CreateViewBinding`).
4. The target widget must already exist in the hierarchy before binding.
5. Property paths are **case-sensitive** and dot-separated (`"Health.Current"`). Discover them
   rather than guessing: `ListBindableWidgetProperties(WidgetBlueprint, Widget)` for a widget,
   `ListClassBindableFields(TargetClass, AccessorClass)` for a ViewModel,
   `ListPropertyPathBindableFields(WidgetBlueprint, Context, PropertyPath)` to resolve and describe
   one path.
6. Paths resolve against each segment's **declared** type, so a field that only exists on a subclass
   of an intermediate object's declared type will not resolve. Bind to the intermediate object and
   cast inside a conversion function instead.
7. `SetBindingMode`, `RemoveWidgetViewBinding`, `RemoveViewCondition` and the conversion-argument
   setters are **not supported during PIE**.

## 🚨 Known limitation: alias name and CreationType are gone

`MVVMToolset.AddViewModelToWidget(WidgetBlueprint, ViewModelClass)` takes **two** arguments. It
delegates to `AddViewModel(WidgetBlueprint, ViewModelClass)` — there is **no alias name** and **no
`CreationType`** parameter.

VibeUE's `Manual` / `GlobalViewModelCollection` / `PropertyPath` / `Resolver` creation types have
**no Epic equivalent**. (`CreateInstance`, VibeUE's default and by far the most common, is what
`AddViewModelToWidget` gives you.)

**Workaround when a non-default creation type is needed:** add the ViewModel first, then edit the
resulting view-model entry with `ObjectToolset.SetProperties`. Read its shape first — the entry
lives under the WBP's MVVM view, so:

1. `MVVMToolset.GetWidgetViewConfig(WidgetBlueprint)` / `ListWidgetViewModels(WidgetBlueprint)` to
   confirm the ViewModel landed.
2. `ObjectToolset.ListProperties` then `GetPropertySchema` on the view-model entry object to find
   the creation-type and name fields and their real enum values.
3. `ObjectToolset.SetProperties` with those exact keys.

Tell the user this is a manual fix-up, not a supported parameter — a later engine update may add
the parameter and change the shape.

## Binding mode options

`SetBindingMode(WidgetBlueprint, BindingID, Mode)` and the `BindingType` field use `EMVVMBindingMode`:

| Mode | Purpose |
|------|---------|
| `OneWayToDestination` | ViewModel → Widget (default, most common for display) |
| `TwoWay` | ViewModel ↔ Widget (editable inputs: sliders, text boxes) |
| `OneTimeToDestination` | ViewModel → Widget (once on init, no updates) |
| `OneWayToSource` | Widget → ViewModel (widget drives ViewModel) |
| `OneTimeToSource` | Widget → ViewModel (once on init) |

Which conversion runs depends on the mode: `SourceToDestinationConversion` for
`OneTimeToDestination`/`OneWayToDestination`/`TwoWay`, `DestinationToSourceConversion` for
`OneWayToSource`/`TwoWay`.

## The tool set

The ones this skill's workflows use:

| Tool | Signature |
|---|---|
| `ListWidgetViewModels` | `(WidgetBlueprint)` → `UClass[]` |
| `AddViewModelToWidget` | `(WidgetBlueprint, ViewModelClass)` → void. **No alias, no CreationType** |
| `ListWidgetViewBindings` | `(WidgetBlueprint)` → `FMVVMViewBindingDescription[]` |
| `CreateViewBinding` | `(WidgetBlueprint, SourceContext, SourcePropertyPath, DestinationContext, DestinationPropertyPath, ConversionName)` → `FGuid`; **invalid zero-GUID on failure** |
| `SetBindingMode` | `(WidgetBlueprint, BindingID, Mode)` → bool |
| `RemoveWidgetViewBinding` | `(WidgetBlueprint, BindingID)` → void. **Takes a GUID** |
| `SetWidgetViewBindingEnabled` | `(WidgetBlueprint, BindingID, bEnabled=true, bCompile=true)` |
| `ListWidgetViewEvents` | `(WidgetBlueprint)` → `FString[]` — the multicast-delegate event paths available |

Also available when you need them: `CreateViewModel` / `AddViewModelProperty` / `ListViewModels`
(authoring ViewModel assets), `ListBindableWidgetProperties` / `ListClassBindableFields` /
`ListPropertyPathBindableFields` (discovery), `ListConversionFunctions` /
`ListBindingConversionOptions` / `SetConversionFunctionArgumentLiteral` /
`ClearConversionFunctionArgumentPath` (conversions), `CreateViewEventBinding` /
`ListWidgetViewEventBindings` / `SetEventBindingArgumentLiteral` (event bindings), `AddViewCondition`
/ `ListViewConditions` / `SetViewConditionOperation` / `RemoveViewCondition` (conditions),
`GetWidgetViewConfig` / `SetWidgetViewConfig`, `SetWidgetPreviewActive` / `GetWidgetPreviewActive` /
`ReadPreviewWidgetProperty` (design-time preview), `FixupMVVMData`.

> ⚠️ **`RemoveWidgetViewBinding` takes a GUID, not an index.** Get it from
> `ListWidgetViewBindings()[i].BindingId`. There is no index-based removal any more.

> ⚠️ `ListBindableWidgetProperties` does **not** filter by direction — a read-only property cannot
> be a destination and a write-only one cannot be a source. `CreateViewBinding` reports that
> mismatch when you try it.

## Context rules (`SourceContext` / `DestinationContext`)

`CreateViewBinding`'s two context arguments decide what the property path is resolved against:

| Context value | The path is read from |
|---|---|
| a `UWidget` | that widget in the WBP's widget tree |
| a `UClass` | one of the WBP's ViewModels |
| `null` | the Widget Blueprint itself |

So a typical ViewModel → widget binding is `SourceContext` = the ViewModel **class**,
`DestinationContext` = the **widget object**.

`ConversionName` is optional (pass `None`/`""`). Given a name, that conversion function is used even
when the types already match (e.g. to format an `FText`). Left empty, a conversion is inferred only
if the types mismatch. Use each option's `Name` — not `DisplayName` — from
`ListBindingConversionOptions`. If the named function does not exist or cannot bridge the two
properties, **no binding is created**.

## Field names

**`FMVVMViewBindingDescription`** — returned by `ListWidgetViewBindings()`:

| Field | Description |
|-------|-------------|
| `BindingId` | **GUID** — use with `RemoveWidgetViewBinding` / `SetBindingMode` / `SetWidgetViewBindingEnabled` |
| `DisplayName` | canonical `"Destination <- Source"` label (`<->` / `->` by mode); conversions render as `Function(Arg1, …)`. **Not unique** — identify by `BindingId` |
| `SourcePath` | source property path (right-hand side); empty when a source→destination conversion supplies the value |
| `DestinationPath` | destination property path (left-hand side); empty when a destination→source conversion supplies it |
| `SourceToDestinationConversion` | forward conversion; `FunctionName` empty for a direct path |
| `DestinationToSourceConversion` | backward conversion; `FunctionName` empty for a direct path |
| `BindingType` | the `EMVVMBindingMode` |
| `bEnabled` | enabled by default (the instance may enable it at runtime) |
| `bCompile` | false = editor-visible only, not usable at runtime |

In Python the bool fields read as `b_enabled` / `b_compile` (they are authored with the `b` prefix,
unlike VibeUE's `bEnabled`).

> ⚠️ `unreal.WidgetService.remove_view_model(path, name)` **invalidates but does not delete** that
> ViewModel's bindings — they still appear in `ListWidgetViewBindings()` as dangling entries. Call
> `RemoveWidgetViewBinding` for each binding GUID **before** `remove_view_model` if you need a clean
> list.

## Workflows

### Add a ViewModel

```python
import unreal

wbp = unreal.load_asset("/Game/UI/WBP_HUD")
vm_class = unreal.load_class(None, "/Game/UI/VM_Health.VM_Health_C")   # Blueprint ViewModel

unreal.MVVMToolset.add_view_model_to_widget(wbp, vm_class)

for cls in unreal.MVVMToolset.list_widget_view_models(wbp):
    print(cls.get_name())
```

For a C++ ViewModel use `unreal.MyHealthViewModel` (or
`unreal.load_class(None, "/Script/MyGame.MyHealthViewModel")`) instead.

### Bind a ViewModel property to a widget

```python
import unreal

wbp  = unreal.load_asset("/Game/UI/WBP_HUD")
bar  = unreal.load_object(None, "/Game/UI/WBP_HUD.WBP_HUD:WidgetTree.HealthBar")
vm   = unreal.load_class(None, "/Game/UI/VM_Health.VM_Health_C")

guid = unreal.MVVMToolset.create_view_binding(
    wbp,
    vm,             # SourceContext: a UClass -> one of the WBP's ViewModels
    "HealthPercent",
    bar,            # DestinationContext: a UWidget -> a widget in the tree
    "Percent",
    "",             # ConversionName: empty = infer only on a type mismatch
)
if not unreal.KismetGuidLibrary.is_valid_guid(guid):   # FGuid has no .is_valid() in Python
    raise RuntimeError("binding was not created - check the property paths and types")

unreal.MVVMToolset.set_binding_mode(wbp, guid, unreal.MVVMBindingMode.ONE_WAY_TO_DESTINATION)

for b in unreal.MVVMToolset.list_widget_view_bindings(wbp):
    print(b.binding_id, b.display_name, b.binding_type)
```

Discover the valid paths first when unsure:

```python
hud_class = unreal.load_class(None, "/Game/UI/WBP_HUD.WBP_HUD_C")     # the WBP's own class
print(unreal.MVVMToolset.list_bindable_widget_properties(wbp, bar))   # widget side
print(unreal.MVVMToolset.list_class_bindable_fields(vm, hud_class))   # ViewModel side
```

`AccessorClass` (the second argument) is the class the fields would be accessed from — it gates
private/protected visibility. `ListPropertyPathBindableFields` does not gate that way, and
`CreateViewBinding` can bind private fields too, so prefer it when a field is missing from the list.

### Full MVVM HUD setup

See `scripts/mvvm_hud.txt` for the complete runnable example (create WBP → build hierarchy with
`WidgetService` → add ViewModel → create bindings → compile → save → verify).

### Remove bindings and ViewModel

```python
import unreal

path = "/Game/UI/WBP_HUD"
wbp  = unreal.load_asset(path)

# Bindings first - by GUID, not index.
for b in list(unreal.MVVMToolset.list_widget_view_bindings(wbp)):
    unreal.MVVMToolset.remove_widget_view_binding(wbp, b.binding_id)

# Then the ViewModel (this one is still VibeUE's - MVVMToolset cannot remove a ViewModel).
unreal.WidgetService.remove_view_model(path, "VM_Health_C")

unreal.BlueprintToolset.compile_blueprint(wbp, False)
unreal.EditorAssetLibrary.save_asset(path)
```

### Auditing bindings

For "are our bindings wired up correctly?", `WidgetStaticAnalysisToolset.GetWidgetConnectionMap`
beats walking `ListWidgetViewBindings` by hand: it returns MVVM field bindings, event bindings,
delegates and legacy property bindings in one checklist with Unconnected/Misconfigured rows first,
and marks a type-mismatched `VMFieldBinding` as `Misconfigured` with a `Fixability` of `Fixable`
(a conversion function can bridge it) or `Unbridgeable`. See SKILL.md ▸ Auditing widgets.
