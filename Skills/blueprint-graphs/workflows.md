---
name: workflows
description: Step-by-step Blueprint graph workflows in UE 5.9 - taking an event-style or function-style override, authoring a function graph with a signature and a body, and Enhanced Input action events with named exec sections
---

This sub-doc continues from SKILL.md → "The DSL workflow".

# Workflows

Every one of these is now a mix of **one or two `BlueprintToolset` calls to create the structure**
and **one `BlueprintDslToolset.Write` for the body**. Nothing here is a VibeUE tool any more.

Shared preamble for the snippets below:

```python
import unreal, vibeue

BT  = "BlueprintToolsets.BlueprintToolset"
DSL = "BlueprintToolsets.BlueprintDslToolset"

def ref(p):
    tail = p.rsplit("/", 1)[-1]
    return {"refPath": p if "." in tail else "{}.{}".format(p, tail)}
```

---

## Take an override

`override_function` and `list_overridable_functions` are gone. In 5.9 **naming an inherited member
*is* taking the override**, and which tool you call decides which shape you get:

| Override kind | Tool | Where the node lands |
|---|---|---|
| **Event-style** — void/latent (`ReceiveTick`, `ReceiveLatentEnterState`, `ReceiveStateCompleted`…) | `BlueprintToolset.AddEvent(Blueprint, EventName, Position)` | an event node in the **EventGraph** |
| **Function-style** — returns a value (`GetStaticDescription`…) | `BlueprintToolset.AddFunctionGraph(Blueprint, GraphName, Flags)` | a new **function graph** with entry + result |

Both are **idempotent**: if the override already exists you get the existing node/graph back. Pass a
name that is *not* an inherited member and you get a fresh custom event / plain function instead.

```python
bp = ref("/Game/StateTree/STT_Rotate")

# Event-style override: lands in the EventGraph as "Event EnterState".
vibeue.exec_tool(BT, "AddEvent",
                 {"blueprint": bp, "eventName": "ReceiveLatentEnterState", "position": [0, 0]})

# Function-style override: its own graph, entry + result.
g = vibeue.exec_tool(BT, "AddFunctionGraph",
                     {"blueprint": bp, "graphName": "GetStaticDescription"})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
```

### Discovering what can be overridden

There is no `list_overridable_functions`. Use:

- **`BlueprintToolset.ListEvents(Blueprint)`** — "all events visible on the Blueprint: locally
  defined custom events **plus inheritable events from the parent class chain and implemented
  interfaces**". Each `FBlueprintFunctionInfo` is `name`, `description`, `is_implemented`, where
  `is_implemented` is false for a member that is "only available to implement or override". Filter
  to `is_implemented == False` and you have the event-style override list.
- **`BlueprintToolset.ListFunctions(Blueprint)`** — the same thing for functions. A member that
  appears here rather than in `ListEvents` is the one to take with `AddFunctionGraph`; there is no
  `is_event_style` flag any more, the list you found it in *is* the answer.
- `BlueprintToolset.GetParent(Blueprint)` plus `search_symbols` / `get_symbol` on the parent class
  when you need a member's signature, which `FBlueprintFunctionInfo` does not carry.
- `BlueprintToolset.FindNodeTypes(Graph, "AddEvent|")` — the palette's own list of event nodes this
  graph will accept.
- `BlueprintToolset.ListCompatibleEventFunctions(Node)` answers a different question: the functions
  a given `CreateDelegate` node can bind to.

Names are case-sensitive. Do not guess them.

### Then write the body

```python
BPP = "/Game/StateTree/STT_Rotate.STT_Rotate"

vibeue.exec_tool(DSL, "Write", {"assetPath": BPP, "scope": "GetStaticDescription", "dsl": """
(fn `GetStaticDescription`
  (return "Rotate Cube"))
"""})
```

For an event-style override the entry is the **member reference** inside the ubergraph:

```
(event `self.ReceiveLatentEnterState`
  (`StateTreeTaskBlueprintBase.FinishTask` true))
```

The visible node title stays the friendly editor name (`Event EnterState`), not the raw function
name — that is display only, never a lookup key.

---

## Author a function with a signature and a body

The graph is structure (`BlueprintToolset`); the signature and body are both DSL.

```python
bp, BPP = ref("/Game/BP/BP_Player"), "/Game/BP/BP_Player.BP_Player"

# 1. The graph must exist before a (fn ...) body can target it.
vibeue.exec_tool(BT, "AddFunctionGraph", {"blueprint": bp, "graphName": "TakeDamage"})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})

# 2. Signature + body in one document.
vibeue.exec_tool(DSL, "Write", {"assetPath": BPP, "scope": "TakeDamage", "dsl": """
(fn `TakeDamage`
  (params  (Amount :type "Float (double-precision)"))
  (returns (NewHealth :type "Float (double-precision)"))
  (locals  (Scratch :type "Float (double-precision)" :default 0.0))
  (set `local.Scratch` (- (`self.Health`) Amount))
  (set `self.Health` (`KismetMathLibrary.FClamp` (`local.Scratch`) 0.0 100.0))
  (return (`self.Health`)))
"""})

vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
```

Rules worth re-reading before you type this:

- The sections go **after** the entry name and configuration keywords, each at most once, in the
  order `(params)` → `(returns)` → `(locals)`.
- A section that is **present is the exact set**: `(params)` with nothing in it removes every
  parameter, and an unlisted item is removed. A section that is **absent** leaves that part alone.
- A new item needs `:type`, spelled **as the editor words it**, with the kind suffixed and quoted
  when it contains spaces: `Integer`, `Boolean`, `String`, `Text`,
  `"Float (double-precision)"`, `"Vector Structure"`, `"Transform Structure"`,
  `"Actor Object Reference"`, `"EEndPlayReason Enum"`, `"Array of Actor Object Reference"`,
  `"Text (by ref)"`. A mismatched `:type` on an **inherited** signature warns and the graph wins —
  the error text names both, so read it rather than retrying.
- Items match by **name**, so renaming one is a removal plus an addition.
- The **first output adds the Result node**, and `(return …)` is what wires its `execute` pin. A
  function that computes a value and never reaches `return` compiles with
  `ReturnNode Exec pin has no connections` and silently hands callers defaults.
- `local.Name` addresses parameters and locals inside their own function; only locals are writable.
- `(returns …)` and `(locals …)` do not fit an `(event …)`.

`BlueprintToolset.AddFunctionParam(Graph, ParamName, ParamType, bInputParam, Options)` /
`AddStructFunctionParam` / `AddObjectFunctionParam` / `RemoveFunctionParam` are the imperative
alternative when you want to touch one parameter without rewriting the entry — and they are the only
route for an **event dispatcher's** parameters (dispatchers take inputs only). Detail-panel flags are
`Get`/`SetFunctionFlags`.

---

## Enhanced Input action events

The entry name encodes the family and the asset path:

```
(event `EnhancedInput|/Game/Input/Actions/IA_Sprint`
  (:`Started`
    (set `CharacterMovementComponent.MaxWalkSpeed` (`self.CharacterMovement`) 1000.0))
  (:`Completed`
    (set `CharacterMovementComponent.MaxWalkSpeed` (`self.CharacterMovement`) 600.0)))
```

```python
bp, BPP = ref("/Game/BP/BP_ThirdPersonCharacter"), "/Game/BP/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter"
dsl = """
(event `EnhancedInput|/Game/Input/Actions/IA_Sprint`
  (:`Started`
    (set `CharacterMovementComponent.MaxWalkSpeed` (`self.CharacterMovement`) 1000.0))
  (:`Completed`
    (set `CharacterMovementComponent.MaxWalkSpeed` (`self.CharacterMovement`) 600.0)))
"""
vibeue.exec_tool(DSL, "Write", {"assetPath": BPP, "dsl": dsl, "scope": "EventGraph"})
vibeue.exec_tool(BT, "CompileBlueprint", {"blueprint": bp})
```

### ⚠️ An event with several exec outputs **requires** named sections

A plain statement directly under such an entry is **refused**, with an error pointing at sections.
A statement after a section the flow never reaches is refused as **unreachable**. Trailing statements
are legal only where the sections' dangling ends join them.

```
(event `Key|F5`                      ; refused: "sections"
  (`KismetSystemLibrary.PrintString` "x"))

(event `Key|F5`                      ; refused: "unreachable"
  (:`Pressed`)
  (`KismetSystemLibrary.PrintString` "x"))

(event `Key|F5` :consume false       ; correct
  (params (Key))
  (:`Pressed`  (`KismetSystemLibrary.PrintString` "down"))
  (:`Released` (`KismetSystemLibrary.PrintString` "up")))
```

### The other input families

| Form | Takes |
|---|---|
| `Key\|F5`, `Key\|Ctrl+Shift+F5` | digital keys; **modifiers are part of the key's identity**, so two events of one key with different modifiers are two separate events |
| `InputAction\|Fire`, `InputAxis\|MoveForward` | mapping names from Project Settings → Input |
| `InputAxisKey\|Gamepad_LeftX` | 1D axes |
| `InputVectorAxis\|Mouse2D` | 2D and 3D axes |
| `InputTouch` | touch |
| `EnhancedInput\|/Game/Input/IA_Move` | an Enhanced Input action asset in a mounted content folder |

Configuration keywords (`:consume`, `:paused`, `:overrideparent`) are emitted only when they are
non-default, and an omitted key **resets to its default** — so preserve the ones `Read` gave you.

**The Input Action asset must exist first.** Create it with
`unreal.InputService.create_action(...)` (see the `enhanced-input` skill), then reference its path.
Run `Schema` on the entry type if you need the exact exec and data pin names for the family you are
using.

---
