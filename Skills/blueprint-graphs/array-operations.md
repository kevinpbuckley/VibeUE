---
name: array-operations
description: Arrays, sets and maps in the Blueprint Graph DSL - MakeArray / MakeSet / MakeMap and their :!type metadata, for-each over an array, the KismetArrayLibrary functions and their pins, element-type propagation on wildcard pins, common mistakes
---

This sub-doc continues from SKILL.md → "The DSL workflow".

# Array Operations

Array nodes use **wildcard pins**: `TargetArray` and the element pins have no type until a typed
array reaches them, and the engine (`UK2Node_CallArrayFunction`) then propagates the element type
through every wildcard pin on the node. In the DSL that propagation happens when the document is
lowered, which is why an array container often needs an explicit **`:!type`** annotation to replay to
the same type.

## Iterating — use the statement form

The grammar has a for-each, so you do not place a `ForEachLoop` macro by hand:

```
(event `self.ReceiveBeginPlay`
  (for actor (`self.Candidates`)
    (`KismetSystemLibrary.PrintString` (`KismetSystemLibrary.GetDisplayName` actor)))

  ; with the index as well
  (for (actor i) (`self.GetAttachedActors` true)
    (`KismetSystemLibrary.PrintString` (`KismetStringLibrary.Conv_IntToString` i))))
```

`(for item (range stop) …)` and `(for item (range start stop) …)` cover the counted loops; range
stops are **exclusive**. `(break)` exits the innermost breakable loop. `:!label` on the `for`
signature makes it a `goto` target, and `` (goto _scan :!via `Break`) `` enters its Break pin —
that is how the DSL expresses a ForEachLoopWithBreak.

## Building a container

`|MakeArray`, `|MakeSet` and `|MakeMap` are non-member node tokens. Element pins are named `[0]`,
`[1]`, … so you can supply them positionally or by keyword:

```
(set `self.Comps` (`|MakeArray` (`self.ChildComp`) (`self.ParentComp`)
                   :!type "StaticMeshComponent Object Reference"))
(set `self.Scores` (`|MakeArray` :`[0]` 5 :`[1]` 9 :!type "Integer"))
(set `self.CompMap` (`|MakeMap` "a" (`self.ChildComp`) "b" (`self.ParentComp`)
                     :!valuetype "StaticMeshComponent Object Reference"))
```

**Preserve `:!type` / `:!valuetype` on a round-trip.** `Read` emits them when replaying the values
alone would infer a different element type — typically when every element is a literal, or when the
elements are narrower than the container. Dropping the annotation silently changes the container's
type.

Node tokens are `|` plus the node class name with the `K2Node_` prefix removed — `|MakeArray` for
`UK2Node_MakeArray`, `|FormatText`, `|GetArrayItem`, `|SwitchInteger`. Confirm any you are unsure of
with `BlueprintDslToolset.FindNodeReferences`, then `Schema` for its pins.

## `KismetArrayLibrary` functions

Call them as ordinary member references: `` (`KismetArrayLibrary.Array_Length` (`self.MyArray`)) ``.

| Member | Purpose | Pins |
|---|---|---|
| `Array_Length` | element count | `TargetArray` in, `ReturnValue` int |
| `Array_LastIndex` | last valid index | `TargetArray` in, `ReturnValue` int |
| `Array_IsEmpty` | emptiness test | `TargetArray` in, `ReturnValue` bool |
| **`Array_Random`** | **random element** — editor title "Random Array Item" | `TargetArray` in, `OutItem` out (wildcard), `OutIndex` out int |
| `Array_RandomFromStream` | random element from an `FRandomStream` | `TargetArray`, `RandomStream` (by-ref) in, `OutItem`, `OutIndex` out |
| `Array_Add` | add, returns the new index | `TargetArray` in/out, `NewItem` in, `ReturnValue` int |
| `Array_Remove` | remove by index | `TargetArray` in/out, `IndexToRemove` int |
| `Array_Clear` | remove all | `TargetArray` in/out |
| `Array_Contains` | membership test | `TargetArray`, `ItemToFind` in, `ReturnValue` bool |
| `Array_Append` | append another array | `TargetArray` in/out, `SourceArray` in |

### ⚠️ Random element — one node, not three

```
(bind (item index) (`KismetArrayLibrary.Array_Random` (`self.PatrolPoints`)))
```

`Array_Random` has **two** data outputs, so select them with a two-name bind in pin order — an
inline pure call would give you only `OutItem`. Do **not** hand-build
`Array_Length` → `RandomIntegerInRange` → index: three nodes, more wiring, and the index path hits
the problem below.

### ⚠️ `Array_Get` is `BlueprintInternalUseOnly` — do not call it

`UKismetArrayLibrary::Array_Get` carries `meta=(BlueprintInternalUseOnly = "true")`. It is the
internal thunk behind the dedicated **`UK2Node_GetArrayItem`** node (editor title "Get",
compact "GET"), and a reference to it is refused. Index an array with the node token instead:

```
(bind item (`|GetArrayItem` (`self.PatrolPoints`) 2))
```

Run `Schema(AssetPath, "|GetArrayItem")` first — its pins are wildcards until the array is wired, and
the schema prints which properties change the pin set.

## Sets and maps

The `UFUNCTION` names on these two libraries all carry a `Set_` / `Map_` prefix:

| Owner | Members (as the C++ declares them) |
|---|---|
| `BlueprintSetLibrary` | `Set_Add`, `Set_AddItems`, `Set_Remove`, `Set_RemoveItems`, `Set_Contains`, `Set_Length`, `Set_IsEmpty`, `Set_IsNotEmpty`, `Set_Clear`, `Set_ToArray`, `Set_Union`, `Set_Intersection`, `Set_Difference` |
| `BlueprintMapLibrary` | `Map_Add`, `Map_Find`, `Map_Remove`, `Map_Contains`, `Map_Length`, `Map_IsEmpty`, `Map_IsNotEmpty`, `Map_Clear`, `Map_Keys`, `Map_Values`, `Map_GetKeyValueByIndex`, `Map_GetLastIndex` |

> ⚠️ **Resolve the exact reference spelling for these two with `FindNodeReferences` before writing
> them.** Each of these functions also carries a short `DisplayName` (`Map_Find` is
> `DisplayName = "Find"`), and the engine's own grammar docs spell the map lookup
> `` `BlueprintMapLibrary.Find` ``, not `Map_Find`. Elsewhere the DSL uses the `UFUNCTION` name
> (`K2_GetActorLocation`, not "Get Actor Location"), so which one wins here is not something to
> guess — ask `FindNodeReferences` and use what it returns.

The map lookup returns a value **and** a found flag, so it needs a two-name bind — the form the
grammar docs show:

```
(bind (value found) (`BlueprintMapLibrary.Find` (`self.CompMap`) "a"))
```

Confirm each member's pins with `Schema` before writing it; set and map members are heavily
wildcard-typed.

## Common mistakes

1. **Dropping `:!type` / `:!valuetype`** on a `|MakeArray` / `|MakeSet` / `|MakeMap` round-trip →
   the container silently comes back with a different element type.
2. **Calling `Array_Get`** → refused; use `|GetArrayItem`.
3. **Inlining a multi-output array call** → you get only the first output. `Array_Random` and
   `BlueprintMapLibrary.Find` need a multi-name `bind`.
4. **Not giving the wildcard pin a typed source** → compile error
   *"The type of Target Array is undetermined"*. Wire the typed array (or annotate the type) before
   anything else on that node.
5. **Guessing pin names** → read them from `Schema` (DSL) or `GetNodeTypePins`
   (`BlueprintToolset`). `TargetArray`, `OutItem`, `OutIndex`, `NewItem`, `ItemToFind` and
   `SourceArray` are not interchangeable.
6. **Placing a loop macro by hand when the grammar has a statement** → write `(for …)` /
   `(while …)`; reach for `/Engine/EditorBlueprintResources/StandardMacros.<Name>` only for the
   macros with no statement form.

---
