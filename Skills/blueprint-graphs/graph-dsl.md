---
name: graph-dsl
description: The Blueprint Graph DSL in depth - Docs/Schema/FindNodeReferences first, Read for text plus a revision, Write for a document, Edit for exact replacements, scopes, upsert and tombstones, entries, statements, expressions, binds, multi-exec continuations, struct values, Diff, error handling
---

This sub-doc continues from SKILL.md → "The DSL workflow".

# The Blueprint Graph DSL (`BlueprintToolsets.BlueprintDslToolset`)

The replacement for VibeUE's old `build_graph` batch builder is **not** a tool with a node list. It
is a document format. A graph is canonical text; you read the text, change the text, and write it
back against a revision token. Seven tools:

| Tool | Signature | What it is for |
|---|---|---|
| `Docs` | `Docs()` → the grammar | **Read it before your first `Write` or `Edit`.** One call, once per session. |
| `Schema` | `Schema(AssetPath, NodeType)` | One node type's pins and writable properties, as a DSL form |
| `FindNodeReferences` | `FindNodeReferences(AssetPath, Query)` | Palette name / category → the **reference** that goes in a call head |
| `Read` | `Read(AssetPath, Scope="", bIncludePositions=false)` | Canonical DSL text + the `Revision` that `Edit` needs |
| `Write` | `Write(AssetPath, Dsl, Scope="")` | Apply a complete document. Transactional. Returns canonical DSL + a new `Revision`. |
| `Edit` | `Edit(AssetPath, Edits, Revision, Scope="")` | Ordered exact text replacements against the text you just read |
| `Diff` | `Diff(OldAssetPath, NewAssetPath)` | Line diff of two Blueprints' graphs as DSL, plus class defaults. Empty when they match. |

All seven are `AICallable` only — reach them with `call_tool`, or `vibeue.exec_tool` from inside
`execute_python`. `unreal.BlueprintDslToolset.*` does not exist.

## Order of operations

```python
import vibeue
DSL = "BlueprintToolsets.BlueprintDslToolset"
BP  = "/Game/BP/BP_Door.BP_Door"

grammar = vibeue.exec_tool(DSL, "Docs", {})                       # 1. once
refs    = vibeue.exec_tool(DSL, "FindNodeReferences",             # 2. name -> reference
                           {"assetPath": BP, "query": "print string"})
schema  = vibeue.exec_tool(DSL, "Schema",                         # 3. pins of that reference
                           {"assetPath": BP, "nodeType": "KismetSystemLibrary.PrintString"})
cur     = vibeue.exec_tool(DSL, "Read",                           # 4. text + revision
                           {"assetPath": BP, "scope": "EventGraph"})
res     = vibeue.exec_tool(DSL, "Write",                          # 5. apply
                           {"assetPath": BP, "dsl": doc, "scope": "EventGraph"})
rev     = res["revision"]                                         #    keep for Edit
```

Skipping step 1 is the single most common way to waste a turn: the grammar is dense (statement forms,
implicit locals, metadata keywords, struct spellings) and guessing it produces a document that does
not parse. Skipping step 3 is the second: pin names and writable properties differ per node type, and
`Schema` also tells you which properties *change the pin set*.

### `Schema`'s output shape

```
(node `Type`
  (in (`Pin` Type) ...)              ; data input pins
  (out (`Pin` Type) ...)             ; data output pins
  (params (`Prop` Type [:over]) ...) ; writable properties, baked as :`Prop` value
  (determines `Prop` ...))           ; properties whose value changes the pin set
```

`(Enum A B ...)` lists a property's writable values. `:over` marks a property that also has an
override **input**, supplied as ``:`Prop` `` with a wired source. An `(Instanced /Script/Pkg.Base)`
property takes a concrete class-headed value:
`(class:/Script/Pkg.Concrete :`Field` value ...)`. Empty sections are omitted.

## `Write` vs `Edit`

> "Use `Write` for a complete document or a large structural change. Use `Edit` for a targeted change
> that can be expressed as exact replacements against text you just read."

| | `Write` | `Edit` |
|---|---|---|
| Input | a whole DSL document for the scope | an ordered list of `{oldString, newString, bReplaceAll?}` |
| Needs a revision | no | **yes** — from the latest `Read`/`Write`/`Edit` on the same asset **and scope** |
| Fails when | the document does not parse, a referenced entry would be deleted, the asset is a macro library | no session, stale revision, `oldString` absent or non-unique (without `bReplaceAll`), result does not parse |
| On failure | asset unchanged | asset unchanged |
| Returns | canonical DSL + new `Revision` | canonical DSL + new `Revision` |

Both carry `NonTransactableToolCall`: they manage their own transaction, so do not wrap them in one.
Both may answer with `bDslOmitted` set, meaning the text you submitted was already canonical (for
`Write`) or that you should apply the replacements to your own copy (for `Edit`) — in which case your
copy, as amended, is the authoritative text for the next edit.

### Revisions go stale

A revision is tied to one asset **and one scope**. It goes stale when anything else writes that
asset — another tool call, the user in the editor, a compile that reconstructs nodes. The symptom is
an `Edit` failure, not a silent mis-apply. Recovery is always the same: `Read` again, rebuild the
replacement against the fresh text, re-`Edit`. Never resend the old token.

```python
def edit(bp, scope, edits, rev):
    try:
        return vibeue.exec_tool(DSL, "Edit",
                                {"assetPath": bp, "edits": edits, "revision": rev, "scope": scope})
    except RuntimeError:
        fresh = vibeue.exec_tool(DSL, "Read", {"assetPath": bp, "scope": scope})
        # rebuild `edits` against fresh["dsl"] before retrying — the old strings may be gone
        return vibeue.exec_tool(DSL, "Edit", {"assetPath": bp, "edits": edits,
                                              "revision": fresh["revision"], "scope": scope})
```

## Scopes

| `Scope` | Selects |
|---|---|
| `""` | the whole asset — every event and function graph |
| `GraphName` | one graph |
| `GraphName/EntryName` | one event or function entry |

Backtick-quote a segment containing spaces, syntax characters or `/`; `` `A/B` `` is one segment.
Read and write the **narrowest** scope that covers your change: a single-entry scope keeps the
document small, makes `Edit` strings unique, and means a stale revision costs one cheap re-`Read`.

Writes create missing **custom events**. A `(fn …)` body still needs an existing function graph —
create it with `BlueprintToolset.AddFunctionGraph` first. A **macro library** is refused outright.

## Upsert and tombstones

> "Top-level entries use upsert semantics: omitted entries remain and produce a warning. Delete an
> entry with a tombstone such as ``(event `Foo` :!delete)``."

So:

- A `Write` with one entry **adds or replaces that entry** and leaves every other entry alone. It
  warns about the ones you omitted. That warning is expected, not a failure.
- Removing an entry is an explicit tombstone: `` (event `Foo` :!delete) ``, `` (fn `Bar` :!delete) ``.
- Deleting a missing entry only warns. Deleting a **referenced** entry fails.

```python
vibeue.exec_tool(DSL, "Write", {"assetPath": BP, "scope": "EventGraph", "dsl": """
(event `self.ReceiveBeginPlay`
  (`self.Setup`))
(event `OldHandler` :!delete)
"""})
```

Inside an entry the rule is different and stricter: a **section** that is present is the exact set.
`(params)` with nothing in it removes every parameter; a listed item is added or updated and an
unlisted one is removed. A section that is **absent** leaves that part alone.

## Writing a document

### Entries

```
(event `EventName` [(params ...)] stmt ...)
(fn   `FunctionName` [(params ...)] [(returns ...)] [(locals ...)] stmt ...)
```

- An engine or interface override uses the implemented **member reference**:
  `` (event `self.ReceiveBeginPlay` …) ``, `` (event `SomeInterface.OnThing` …) ``.
- A component-bound event uses `self.Component.Dispatcher`:
  `` (event `self.MeshComp.OnComponentHit` …) ``.
- A custom event is a bare graph-owned name. Preserve an emitted `Custom|` prefix.
- A handler whose own name differs from the function or dispatcher supplying its parameters declares
  the source with ``:!signature``:

```
(event `HandleDied`
  :!signature `self.OnDied`
  (params (Damage))
  (`KismetSystemLibrary.PrintString` "died"))
```

An **inherited** signature is read-only: an override, an interface function, a `:!signature`
handler, or a dispatcher-bound event. Its sections describe the live pins — naming a pin the entry
lacks fails, and a mismatched `:type` / `:default` warns while the graph wins. Locals are always the
function's own.

### Signature sections

```
(fn `Score`
  (params  (Name :type TypeId :default Value) ...)
  (returns (Name :type TypeId) ...)
  (locals  (Name :type TypeId :default Value) ...)
  stmt ...)
```

They follow the entry name and configuration keywords, each at most once, in that order. `(returns)`
and `(locals)` do not fit an `(event …)`. A new item needs `:type`. Items match by **name**, so a
rename is a removal plus an addition, and the section order is the pin order. A function's first
output adds its Result node.

A pin type is spelled as the editor words it, with the kind suffixed, and **quoted when it contains
spaces**: `Integer`, `Boolean`, `String`, `Text`, `"Float (double-precision)"`,
`"Vector Structure"`, `"Transform Structure"`, `"Pawn Object Reference"`,
`"/Game/Modes/E_Mode Enum"`, `"Array of Actor Object Reference"`, `"Text (by ref)"`. A mismatched
`:type` on an inherited signature warns and names both types — read the message instead of retrying.

### Statements

```
(set `self.Member` value)                  (set `Owner.Member` target value)
(add `self.Dispatcher` handler)            (remove `self.Dispatcher` handler)
(clear `self.Dispatcher`)
(message `Interface.Function` target args...)
(cast `Class` object (:`then` ...) (:`CastFailed` ...))
(castclass `Class` class-value (:`then` ...) (:`CastFailed` ...))
(setmembers `Struct` :`StructRef` target :`Member` value ...)
(return [value ...])
(if cond stmt ... [(elif cond stmt ...) | (else stmt ...)])
(for item (range stop) stmt ...)           (for item (range start stop) stmt ...)
(for item array stmt ...)                  (for (item index) array stmt ...)
(while cond stmt ...)
(switch `Type` value (:`Case` stmt ...) ...)
(break)                                    (goto label)
```

`setmembers` changes only the members it names. `message` is the runtime-checked interface call; a
plain member call is direct. `elif` / `else` must end its enclosing branch. `break` exits the
innermost breakable loop. Range stops are **exclusive**. `_` is a no-op statement, and as an
argument it leaves that pin unchanged — use it for an unspellable struct default or a by-reference
parameter rather than inventing a value.

### Expressions

```
variable      Name
arithmetic    (+ a b ...) (- a b ...) (* a b ...) (/ a b ...) (mod a b) (- value)
comparison    (== a b) (!= a b) (< a b) (<= a b) (> a b) (>= a b)
              (== a b :`ErrorTolerance` 0.01)
boolean       (and a b ...) (or a b ...) (xor a b) (not value)
conditional   (select `Type` value :`Case` result ...)
```

Numeric literals preserve pin type: a real pin reads `0.0`, an integer pin `0`. An operator resolves
from wired operands first, then from Float/Bool literals. `==` / `!=` on strings and text ignore
case — an exact comparison is `KismetStringLibrary.EqualEqual_StrStr` or
`KismetTextLibrary.EqualEqual_TextText` by name.

**Every bare non-literal atom is a local reference**, so quote identifier-like *values*:
`"/Script/Engine.StaticMeshActor"`, `"AlwaysSpawn"`, `"/Game/Meshes/SM_Cube.SM_Cube"`. `null` clears
an object, class, soft or interface reference. The bare local `self` is the owning Blueprint object
(a parameter named `self` shadows it).

### Members and node types

Members are `Owner.Member`. `self` is this Blueprint's class chain; another owner is a native class
name or a Blueprint class path. **Every member is spelled by its authored name, as `Read` returns
it** — `K2_SetActorLocation`, not `SetActorLocation`.

```
(`self.UpdateScore` 5)
(set `self.Health` 100)
(`Actor.SetActorHiddenInGame` target true)
(`/Game/Doors/BP_Door.Open` door)
(`parent.ReceiveBeginPlay`)
```

`parent.Function` is `Super::Function`. Function-local variables and parameters are `local.Name`
inside their function (only variables are writable). Non-member nodes use a `|` token: `|MakeArray`,
`|FormatText`, `|CreateDelegate`, `Timeline|Name`, `AddComponent|Name`, and entry families such as
`Key|F5`. **`BlueprintToolset`'s palette type ids (`Category|Name`, e.g. `Development|PrintString`)
are not DSL call heads.**

Macros use their library asset or `self` as owner:
`` (`/Engine/EditorBlueprintResources/StandardMacros.Gate`) ``, `` (`self.MyMacro`) ``.
Struct ops use a native struct name or struct asset path: `` (make `Margin` 1.0 2.0) ``,
`` (bind (_min _max) (break `Box` box)) ``.

### Casts

Without `:!pure`, a cast is the **exec** cast: a bind takes the typed value, statements after it run
only on success, and a failure ends the flow unless a `` (:`CastFailed` …) `` continuation handles
it. `:!pure true` selects the pure cast — no exec pins, a two-name bind takes value and success.
Casting an enum reference is the byte-to-enum cast, always pure.

```
(bind _pawn (cast `Pawn` object))
(cast `Pawn` object (:`CastFailed` (`KismetSystemLibrary.PrintString` "not a pawn")))
(bind (_pawn _ok) (cast `Pawn` object :!pure true))
```

### Binds, multi-exec calls and continuations

```
(`NodeType` args...
  (:`ExecOut1` stmt ...)
  (:`ExecOut2` stmt ...))
```

Named exec sections follow the data arguments; trailing statements run after the continuations
rejoin. `` (:`Pin`) `` leaves an output unwired, `` (:`Pin` _) `` connects it straight to the
trailing statement.

A continuation body exposes each data output as an **implicit local**: lowercase the pin name,
replace spaces with underscores, prefix `_` — `Return Value` becomes `_return_value`. To use an
output *after* the call, bind it. An inline pure call selects its **first** data output; to select
others, bind names in pin order and use throwaway names for the ones you do not want:

```
(bind (value found) (`BlueprintMapLibrary.Find` map key))
```

Bindings stay visible to later continuations and to the trailing statements.

### Struct values

Vector-family defaults use positional tuples; everything else uses ``(struct :`Field` value …)``
with default-valued fields omitted:

```
(1 2 3)
(struct :`Yaw` 90)
(struct :`Min` (1 2 3))
```

A Rotator uses `Pitch` / `Yaw` / `Roll`; colors use their named channels. A struct value on a
by-reference pin splits and fills that pin. An incompatible struct form **fails the write**.

### Output and component access

These fixed attributes lower to Break nodes: `.x .y .z` (vector), `.pitch .yaw .roll` (rotator),
`.location .rotation .scale` (transform). Any other `.Name` selects that output pin *without*
creating a node — reads use it for statement outputs and visible optional members, while pure calls
use a multi-output bind instead.

### Metadata keywords

| Keyword | Meaning |
|---|---|
| `:!pos` | node position (emitted by `Read` only with `bIncludePositions`) |
| `:!delete` | tombstone — remove this top-level entry |
| `:!label name` | mark a statement as a `goto` target; labels are graph-unique |
| ``:!via `Pin` `` | enter a statement through an exec input other than its first |
| `:!pure true` | select a cast's pure form |
| ``:!event `Entry` `` | the entry wrapped by `\|CreateDelegate` |
| `:!type "Pin Type"` | element type for `\|MakeArray` / `\|MakeSet` / `\|MakeMap` / `select` / a wildcard macro |
| `:!valuetype "Pin Type"` | value type for `\|MakeMap` |
| ``:!signature `Owner.Member` `` | the parameter source of an event entry (event heads only) |
| `:!pinned true` | keep a node's note bubble visible |

**Preserve emitted container-type metadata on a round-trip** — values alone may not replay to the
same type.

### Delegates

`|CreateDelegate` wraps an event or function named by `:!event`:

```
(event `HandleDestroyed` :!signature `self.OnDestroyed`
  (params (DestroyedActor :type "Actor Object Reference")))

(event `self.ReceiveBeginPlay`
  (add `self.OnDestroyed` (`|CreateDelegate` :!event `HandleDestroyed`)))
```

`` (delegate `EntryName`) `` references a same-graph custom event's own delegate output.

### Timeline and add-component calls are **anchored**

A `` (`Timeline|Name` …) `` or `` (`AddComponent|TemplateName` …) `` statement addresses an
**existing** node. A missing match is an error, and omitting the statement does **not** delete the
node. An anchored call owns that node's wiring; omitted keywords preserve the live pin values. A
timeline enters `Play` unless ``:!via `Pin` `` names another exec input, and other arrivals are
`goto`s to its label with the same `:!via`:

```
(bind (_direction _fader) (`Timeline|FadeTimeline` :!via `PlayFromStart` :!label _fade)
  (:`Update` (`KismetMaterialLibrary.SetScalarParameterValue`
    :`ParameterName` "Fade" :`ParameterValue` _fader)))
...
(goto _fade :!via `Reverse`)
```

Create the timeline itself with `unreal.BlueprintService.add_timeline` + the track/key tools first
(see `node-reference.md`) — the DSL anchors to it, it does not create it. Query `Schema` for the
node's available pins.

### Gotos and labels

Every node has one statement. An exec wire into a node owned by another chain or entry becomes a
`goto`. Place `:!label name` on a call or bind, or after an `if` / `for` / `while` / `switch`
signature. Statements enter the first exec input unless ``:!via `Pin` `` says otherwise, and gotos
use the same annotation. `goto`, `return` and `break` terminate their statement chain.

### Stale references

`(stale name)` reads a bound value from another exec scope in the current entry;
`` (stale `Entry` Name) `` reads a parameter or binding from another entry. It changes no execution
order and cannot be rebound — it is how `Read` represents a cross-chain data read, so preserve it.

### Comments

A comment-box header covers the next contiguous statement run in the same entry or nested body. A
trailing note attaches to a statement node, or to the node a bind created; node-free binds and
control forms cannot carry notes.

### Collapsed graphs

If `Read` expands a collapsed graph, a write that changes its entry **permanently replaces the
collapsed node with flat nodes**. If the collapsed graph must survive, edit it with
`BlueprintToolset` instead of writing over it.

## Worked example: an event with a branch and a loop

```python
import vibeue
DSL = "BlueprintToolsets.BlueprintDslToolset"
BP  = "/Game/BP/BP_Spawner.BP_Spawner"

doc = """
(event `self.ReceiveBeginPlay`
  (bind loc (`Actor.K2_GetActorLocation`))
  (if (> (.z loc) 0.0)
    (for _ (range 5)
      (`KismetSystemLibrary.PrintString` "tick"))
    (else
      (`KismetSystemLibrary.PrintString` "underground"))))
"""
res = vibeue.exec_tool(DSL, "Write", {"assetPath": BP, "dsl": doc, "scope": "EventGraph"})
print(res["dsl"])          # canonical form — diff it against what you sent
rev = res["revision"]
```

Read the canonical form back from the result and compare it with what you sent: the canonicaliser
tells you how it understood your document (resolved references, literal types, inserted metadata).
That is the cheapest possible verification and it costs no extra call.

## Error handling

- A failed `Write` or `Edit` leaves the asset **unchanged**. There is no half-applied graph to clean
  up — fix the document and resubmit. This is the opposite of the old `build_graph`, which left
  successful nodes behind on partial failure, so the old "inspect the graph and delete the strays"
  recovery no longer applies.
- `vibeue.exec_tool` raises `RuntimeError` with the tool's error text. Read it: parse errors name the
  offending form, a bad reference names the member, a stale revision says so.
- Warnings are not failures. The omitted-entry warning on an upsert `Write` is expected.
- After the write: `BlueprintToolset.CompileBlueprint`, then
  `unreal.BlueprintService.analyze_graph_layout` for the layout, then save
  (`EditorToolset.AssetToolset.SaveAsset`, or `unreal.EditorAssetLibrary.save_asset`). Never save on
  a compile error.

---
