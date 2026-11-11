---
name: function-classes
description: Owner names for a Graph DSL `Owner.Member` reference - KismetMathLibrary, KismetSystemLibrary, KismetArrayLibrary, GameplayStatics and the other UE library classes, plus how owners differ from BlueprintToolset palette type ids
---

This sub-doc continues from SKILL.md → "Members and node types" and graph-dsl.md.

# Function owners for a DSL reference

A DSL call head is a **reference**: `` (`Owner.Member` args…) ``. `Owner` is a native class name
(no C++ `U`/`A` prefix), `self` for this Blueprint's class chain, `parent` for the nearest parent
implementation, `local` for a function's own parameters and locals, or a Blueprint class path. The
member is spelled by its **authored** name, exactly as `Read` returns it — `K2_SetActorLocation`,
not `SetActorLocation`.

```
(`KismetSystemLibrary.PrintString` "hello")
(`KismetMathLibrary.VInterpTo` current target delta_time speed)
(`Actor.K2_SetActorLocation` target (0 0 100))
(`self.UpdateScore` 5)
(`parent.ReceiveBeginPlay`)
(`/Game/Doors/BP_Door.Open` door)
```

## Common library owners

| Owner | Covers | Members you will actually type |
|---|---|---|
| `KismetMathLibrary` | math, interpolation, random, vector/rotator ops | `Add_DoubleDouble`, `Multiply_DoubleDouble`, `Greater_DoubleDouble`, `Sin`, `Sqrt`, `VInterpTo`, `RandomBool`, `RandomIntegerInRange`, `Clamp` |
| `KismetSystemLibrary` | printing, timers, delays, literals, validity | `PrintString`, `Delay`, `K2_SetTimerDelegate`, `MakeLiteralDouble`, `IsValid`, `GetDisplayName` |
| `KismetStringLibrary` | strings | `Concat_StrStr`, `Contains`, `EqualEqual_StrStr` |
| `KismetTextLibrary` | text | `EqualEqual_TextText`, `Conv_TextToString` |
| `KismetArrayLibrary` | arrays | `Array_Length`, `Array_Random`, `Array_Add`, `Array_Remove`, `Array_Contains`, `Array_Append` |
| `BlueprintMapLibrary` | maps | `Map_Find`, `Map_Add`, `Map_Remove`, `Map_Keys`, `Map_Values` — but see the caution in `array-operations.md`: these also carry short `DisplayName`s, so resolve the spelling with `FindNodeReferences` |
| `BlueprintSetLibrary` | sets | `Set_Add`, `Set_Contains`, `Set_Remove`, `Set_ToArray` — same caution |
| `GameplayStatics` | game-level helpers | `GetPlayerController`, `GetPlayerPawn`, `GetGameInstance`, `ApplyDamage`, `OpenLevel`, `SpawnSoundAtLocation`, `SpawnEmitterAtLocation`. **"Spawn Actor from Class" is not here** — it is the `UK2Node_SpawnActorFromClass` node (`GameplayStatics` only has the deferred pair `BeginDeferredActorSpawnFromClass` / `FinishSpawningActor`) |
| `KismetMaterialLibrary` | material parameter collections | `SetScalarParameterValue`, `SetVectorParameterValue` |
| `Actor` | actor functions reached through a target pin | `K2_GetActorLocation`, `K2_SetActorLocation`, `SetActorHiddenInGame`, `K2_DestroyActor`, `ForceNetUpdate` |
| `SceneComponent` | transform | `K2_SetRelativeLocation`, `AddRelativeRotation`, `K2_AttachToComponent` |
| `PrimitiveComponent` | physics and collision | `SetSimulatePhysics`, `SetCollisionEnabled` |
| `CharacterMovementComponent` | movement properties (members, not just functions) | `MaxWalkSpeed`, `MaxAcceleration`, `GravityScale` |

Non-member nodes have no owner — they use a `|` token instead: `|MakeArray`, `|MakeSet`,
`|MakeMap`, `|FormatText`, `|CreateDelegate`, `|SwitchInteger`, `|CallParentFunction`, plus the
anchored `Timeline|Name` / `AddComponent|Name` and the input families `Key|F5`,
`EnhancedInput|/Game/Input/IA_Move`.

## Resolve, don't guess

Display titles and authored names routinely differ (`Get Actor Location` →
`K2_GetActorLocation`; `VInterp To` → `VInterpTo`; `Random Array Item` → `Array_Random`). Two
lookups, and they return **different** kinds of string:

| Looking for | Call | Returns |
|---|---|---|
| a DSL call head | `BlueprintDslToolset.FindNodeReferences(AssetPath, Query)` | the **reference** (plus its palette category and name) — this is what goes in a `Write` |
| a `CreateNode` argument | `BlueprintToolset.FindNodeTypes(Graph, TypeIdFilter, ContextPins?)` | a palette **type id**, `Category\|Name` |

**A palette type id is not a DSL call head.** `Development|PrintString` works in
`BlueprintToolset.CreateNode` and fails in a DSL document; `KismetSystemLibrary.PrintString` is the
other way round.

Once you have a reference, `BlueprintDslToolset.Schema(AssetPath, NodeType)` gives its pins and
writable properties. If a search returns nothing useful, make the query **more** specific rather
than raising the result cap.

## ⚠️ Blueprint math is double-precision

| Wrong | Right |
|---|---|
| `Add_FloatFloat` | `Add_DoubleDouble` |
| `Greater_FloatFloat` | `Greater_DoubleDouble` |
| `MakeLiteralFloat` | `MakeLiteralDouble` |

`UKismetSystemLibrary::MakeLiteralFloat` is `UE_DEPRECATED(5.1)` and is not a `UFUNCTION` at all;
`MakeLiteralDouble` carries `DisplayName = "Make Literal Float"`, which is why the editor still
shows the old label. A reference to the float name resolves to nothing.

In practice the DSL spares you most of these: `(+ a b)`, `(> a b)` and a bare literal resolve the
operator from the wired operands, from a Float/Bool literal, or — for an Int literal — from the
width another operand selects. You only need the exact member name when you call one directly.

## Blueprint classes as owners

A Blueprint class is an owner too: use its **asset path** (`/Game/Doors/BP_Door.Open`) or a short
class name where the pack's own examples do. If two Blueprint assets share a short name, use the
full path — short-name resolution is ambiguous and will pick wrong or refuse.

An interface call is `` (message `Interface.Function` target args…) `` — the runtime-checked form. A
plain `` (`Interface.Function` target) `` is a direct call and needs a target that really implements
it, which is why a bare interface function otherwise wants a cast in front of it.
