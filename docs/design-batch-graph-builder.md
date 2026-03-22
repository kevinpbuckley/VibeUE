# Design: Batch Graph Builder & Auto-Layout

## Problem Statement

Building a Blueprint graph through VibeUE today requires **one tool call per node** plus **one tool call per connection**. A 15-node graph with 20 connections requires ~35+ sequential round-trips between the AI and the MCP server. Each round-trip costs tokens, latency, and increases the chance of the AI losing track of accumulated GUIDs mid-sequence.

Meanwhile, the existing per-node API is **correct by design** — every node creation is verified, every connection is schema-validated, every failure is isolated. We don't want to lose that.

The goal is to add a **batch graph builder** that accepts a complete graph description in a single call, creates all nodes and connections atomically, auto-layouts the result, and returns a full audit of what was created — while reusing the existing verified creation and wiring internals.

---

## Design Goals

1. **Single-call graph creation** — describe an entire graph (nodes + connections + defaults) and create it atomically
2. **Version-resilient** — no hardcoded node class names. Use the same discovery/spawner resolution that `AddFunctionCallNode` and `CreateNodeByKey` already use
3. **Schema-validated connections** — every wire goes through `UEdGraphSchema_K2::TryCreateConnection()`, not raw pin linking
4. **Transactional** — wrap the entire operation in `FScopedTransaction` so Ctrl+Z undoes the whole batch
5. **Auto-layout** — topological ordering with layered positioning so the graph is readable without manual arrangement
6. **Graceful degradation** — if node 8/15 fails to create, nodes 1-7 remain valid. Report what succeeded and what failed. Don't corrupt the graph
7. **Additive** — the existing per-node methods remain unchanged. This is a new method that calls into the same internals
8. **Round-trip capable** — an export method can read an existing graph back into the description format, enabling learning from hand-built graphs

---

## Graph Description Schema

The AI provides a JSON structure with three arrays: **nodes**, **connections**, and **pin_defaults**.

### Node Description

Each node has a local **ref** (any unique string the AI chooses) and a **type** that determines how the node is resolved.

```json
{
  "ref": "string",
  "type": "string",
  "params": { "key": "value", ... }
}
```

#### Supported Node Types

| Type | Required Params | Resolved By |
|------|----------------|-------------|
| `function_call` | `class`, `function` | `AddFunctionCallNode` internal path — class resolution + `FindFunctionByName` with spawner fallback |
| `spawner_key` | `key` | `CreateNodeByKey` internal path — parses `FUNC`, `EVENT`, `NODE` prefixes |
| `variable_get` | `variable` | `AddGetVariableNode` internal path |
| `variable_set` | `variable` | `AddSetVariableNode` internal path |
| `event` | `event` | `AddEventNode` internal path — parent class event override |
| `custom_event` | `name` | `AddCustomEventNode` internal path |
| `branch` | *(none)* | `AddBranchNode` internal path |
| `cast` | `target_class` | `AddCastNode` internal path |
| `print_string` | *(none)* | `AddPrintStringNode` internal path |
| `input_action` | `action` | `AddInputActionNode` internal path |
| `math` | `operation`, `operand_type` | `AddMathNode` internal path |
| `comparison` | `operation`, `operand_type` | `AddComparisonNode` internal path |
| `delegate_bind` | `delegate`, `component` | `AddDelegateBindNode` internal path |
| `create_event` | `delegate_class`, `function` | `AddCreateEventNode` internal path |
| `validated_get` | `variable`, `is_validated` | `AddValidatedGetNode` internal path |
| `member_get` | `member`, `class` | `AddMemberGetNode` internal path |
| `create_delegate` | `delegate_class`, `function` | `AddCreateDelegateNode` internal path |

Every type maps directly to an existing `UBlueprintService` method. No new node creation logic is needed — we reuse the proven implementations.

#### Example Nodes

```json
{"ref": "BeginPlay", "type": "event", "params": {"event": "ReceiveBeginPlay"}},
{"ref": "GetSpeed",  "type": "variable_get", "params": {"variable": "Speed"}},
{"ref": "Multiply",  "type": "function_call", "params": {"class": "KismetMathLibrary", "function": "Multiply_DoubleDouble"}},
{"ref": "SetVelocity", "type": "function_call", "params": {"class": "CharacterMovementComponent", "function": "SetMaxWalkSpeed"}},
{"ref": "Print",     "type": "print_string", "params": {}},
{"ref": "IsFast",    "type": "branch", "params": {}},
{"ref": "CheckSpeed","type": "comparison", "params": {"operation": "Greater", "operand_type": "Double"}}
```

### Connection Description

Connections use the format `"NodeRef.PinName"` for both source and target.

```json
{"from": "BeginPlay.then", "to": "CheckSpeed.execute"},
{"from": "GetSpeed.value",  "to": "CheckSpeed.A"},
{"from": "CheckSpeed.ReturnValue", "to": "IsFast.Condition"},
{"from": "IsFast.True",  "to": "SetVelocity.execute"},
{"from": "GetSpeed.value", "to": "Multiply.A"}
```

**Pin name resolution rules:**
1. Exact match against `UEdGraphPin::PinName` (case-insensitive)
2. Alias mapping for common names:
   - `execute` / `exec` → the execution input pin (first pin with `PC_Exec` and `EGPD_Input`)
   - `then` / `output` → the execution output pin (first pin with `PC_Exec` and `EGPD_Output`)
   - `value` / `result` → first non-exec output pin
   - `true` / `false` → branch output aliases (`then` / `else` internally)
3. If no match: log warning, skip connection, continue

### Pin Defaults

```json
{"node": "Multiply", "pin": "B", "value": "2.0"},
{"node": "Print",    "pin": "InString", "value": "Moving fast!"}
```

---

## C++ API Surface

### New Structs

```cpp
USTRUCT(BlueprintType)
struct FGraphNodeDesc
{
    GENERATED_BODY()

    // Local reference for wiring (e.g. "A", "BeginPlay", any unique string)
    UPROPERTY() FString Ref;

    // Node type (see supported types table)
    UPROPERTY() FString Type;

    // Type-specific parameters
    UPROPERTY() TMap<FString, FString> Params;
};

USTRUCT(BlueprintType)
struct FGraphConnectionDesc
{
    GENERATED_BODY()

    // Format: "NodeRef.PinName"
    UPROPERTY() FString From;
    UPROPERTY() FString To;
};

USTRUCT(BlueprintType)
struct FGraphPinDefaultDesc
{
    GENERATED_BODY()

    UPROPERTY() FString NodeRef;
    UPROPERTY() FString PinName;
    UPROPERTY() FString Value;
};

USTRUCT(BlueprintType)
struct FBuildGraphResult
{
    GENERATED_BODY()

    UPROPERTY() bool bSuccess = false;
    UPROPERTY() int32 NodesCreated = 0;
    UPROPERTY() int32 NodesFailed = 0;
    UPROPERTY() int32 ConnectionsMade = 0;
    UPROPERTY() int32 ConnectionsFailed = 0;
    UPROPERTY() int32 DefaultsSet = 0;
    UPROPERTY() int32 DefaultsFailed = 0;

    // Maps local ref → actual engine GUID string
    UPROPERTY() TMap<FString, FString> RefToNodeId;

    // Per-node creation errors (ref → error message)
    UPROPERTY() TArray<FString> Errors;
    UPROPERTY() TArray<FString> Warnings;

    // Compilation result (if compile was requested)
    UPROPERTY() bool bCompiled = false;
    UPROPERTY() int32 CompileErrors = 0;
    UPROPERTY() int32 CompileWarnings = 0;
};
```

### New Methods on UBlueprintService

```cpp
// ─── Batch Graph Builder ────────────────────────────────────

/**
 * Create multiple nodes, wire connections, and set pin defaults
 * in a single atomic transaction. Optionally auto-layouts the result.
 *
 * Uses the same discovery/spawner resolution as individual methods.
 * Every connection goes through Schema->TryCreateConnection().
 * The entire operation is wrapped in FScopedTransaction (Ctrl+Z undoes all).
 *
 * Nodes that fail to create are skipped (with errors logged).
 * Connections referencing failed nodes are skipped.
 * The graph is left in a valid state regardless of partial failures.
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprint")
static bool BuildGraph(
    const FString& BlueprintPath,
    const FString& GraphName,
    const TArray<FGraphNodeDesc>& Nodes,
    const TArray<FGraphConnectionDesc>& Connections,
    const TArray<FGraphPinDefaultDesc>& PinDefaults,
    bool bAutoLayout,
    bool bCompileAfter,
    FBuildGraphResult& OutResult
);

/**
 * Auto-layout all nodes in an existing graph.
 * Uses topological sort on execution flow + layered positioning.
 * Does not modify any connections or node logic — only positions.
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprint")
static bool AutoLayoutGraph(
    const FString& BlueprintPath,
    const FString& GraphName,
    FString& OutError
);

/**
 * Export an existing graph to the description format.
 * Enables round-tripping and learning from hand-built graphs.
 * The exported description can be fed back into BuildGraph.
 */
UFUNCTION(BlueprintCallable, Category = "VibeUE|Blueprint")
static bool GetGraphDefinition(
    const FString& BlueprintPath,
    const FString& GraphName,
    TArray<FGraphNodeDesc>& OutNodes,
    TArray<FGraphConnectionDesc>& OutConnections,
    TArray<FGraphPinDefaultDesc>& OutPinDefaults,
    FString& OutError
);
```

---

## Implementation Strategy

### BuildGraph Execution Flow

```
1. Load Blueprint + find Graph
2. Open FScopedTransaction("BuildGraph")
3. ── Phase 1: Create Nodes ──
   For each FGraphNodeDesc:
     a. Dispatch to existing creation method based on Type
     b. If success → store ref → UEdGraphNode* mapping
     c. If failure → log error, add to Errors, continue
4. ── Phase 2: Wire Connections ──
   For each FGraphConnectionDesc:
     a. Parse "Ref.PinName" for both From and To
     b. Look up UEdGraphNode* from ref map
     c. Resolve pin by name (with alias fallback)
     d. Schema->TryCreateConnection(SourcePin, TargetPin)
     e. If failure → log warning, continue
5. ── Phase 3: Set Pin Defaults ──
   For each FGraphPinDefaultDesc:
     a. Look up node by ref, find pin by name
     b. Schema->TrySetDefaultValue() or pin->DefaultValue = Value
     c. If failure → log warning, continue
6. ── Phase 4: Auto-Layout (optional) ──
     a. Run AutoLayoutGraph on the target graph
7. ── Phase 5: Compile (optional) ──
     a. FKismetEditorUtilities::CompileBlueprint()
     b. Capture errors/warnings into result
8. Mark blueprint modified
9. Return FBuildGraphResult with full audit
```

### Node Creation Dispatch

The `BuildGraph` method dispatches each node type to an **internal helper** that mirrors the existing public methods but returns a `UEdGraphNode*` instead of a `FString` GUID. This avoids code duplication:

```cpp
// Internal: same logic as AddFunctionCallNode but returns node pointer
UEdGraphNode* CreateFunctionCallNodeInternal(
    UBlueprint* Blueprint,
    UEdGraph* Graph,
    const FString& ClassName,
    const FString& FunctionName,
    float PosX, float PosY
);

// Internal: same logic as AddCustomEventNode but returns node pointer
UEdGraphNode* CreateCustomEventNodeInternal(
    UBlueprint* Blueprint,
    UEdGraph* Graph,
    const FString& EventName,
    float PosX, float PosY
);

// ... one internal helper per Type
```

The existing public methods (`AddFunctionCallNode`, etc.) can be refactored to call these same internals, returning `Node->NodeGuid.ToString()` as before. Zero behavior change for existing API consumers.

### Pin Resolution

```cpp
UEdGraphPin* ResolvePinByName(
    UEdGraphNode* Node,
    const FString& PinName,
    EEdGraphPinDirection PreferredDirection = EGPD_MAX  // no preference
)
{
    // 1. Exact match (case-insensitive)
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
            return Pin;
    }

    // 2. Alias resolution
    if (PinName.Equals("execute") || PinName.Equals("exec"))
        return FindFirstExecPin(Node, EGPD_Input);
    if (PinName.Equals("then") || PinName.Equals("output"))
        return FindFirstExecPin(Node, EGPD_Output);
    if (PinName.Equals("value") || PinName.Equals("result"))
        return FindFirstDataPin(Node, EGPD_Output);
    if (PinName.Equals("true"))
        return ResolvePinByName(Node, "then");  // Branch alias
    if (PinName.Equals("false"))
        return ResolvePinByName(Node, "else");  // Branch alias

    return nullptr;
}
```

---

## Auto-Layout Algorithm

### Overview

A simplified Sugiyama-style layered graph layout with three passes:

### Pass 1: Layer Assignment (Longest-Path)

Execution flow determines layers (left-to-right columns):

```
1. Find all root nodes (no incoming exec pins) → Layer 0
2. BFS along exec output pins:
   - Each successor goes to max(parent_layer + 1, current_layer)
3. Pure nodes (no exec pins) → placed in the layer of their
   first consumer minus 0.5 (visually above/beside their consumer)
```

### Pass 2: Ordering Within Layers

Minimize edge crossings using median heuristic:

```
For each layer (left to right):
  Sort nodes by median position of connected nodes in previous layer
  Break ties by vertical connectivity density
```

### Pass 3: Position Assignment

```
Constants:
  COLUMN_WIDTH  = 400   // horizontal spacing between layers
  ROW_HEIGHT    = 200   // vertical spacing between nodes in same layer
  MARGIN_LEFT   = 100   // left margin for entry nodes
  MARGIN_TOP    = 100   // top margin

For each layer L:
  For each node N at index I within layer:
    N.NodePosX = MARGIN_LEFT + (L * COLUMN_WIDTH)
    N.NodePosY = MARGIN_TOP  + (I * ROW_HEIGHT)
```

### Special Cases

- **Event/Entry nodes**: Always pinned to Layer 0 (leftmost column)
- **Disconnected nodes**: Placed in a separate "island" below the main graph
- **Comments/Reroute nodes**: Not repositioned
- **Function graphs**: Entry and Result nodes keep their relative positions

---

## GetGraphDefinition

Reads an existing graph and produces the description format:

```
1. For each UEdGraphNode in Graph->Nodes:
   a. Determine Type by inspecting node class:
      - UK2Node_CallFunction → "function_call" with class + function params
      - UK2Node_VariableGet → "variable_get" with variable name
      - UK2Node_VariableSet → "variable_set" with variable name
      - UK2Node_IfThenElse → "branch"
      - UK2Node_DynamicCast → "cast" with target class
      - UK2Node_Event → "event" with event name
      - UK2Node_CustomEvent → "custom_event" with name
      - UK2Node_InputAction → "input_action" with action
      - Other → "spawner_key" with best-guess key reconstruction
   b. Generate ref: use sanitized node title or "Node_<index>"
   c. Collect all non-default pin values → pin_defaults

2. For each connection (pin LinkedTo):
   a. Map source node → ref, target node → ref
   b. Emit {"from": "RefA.PinName", "to": "RefB.PinName"}
```

This enables:
- **Learning**: AI exports a hand-built graph, learns the pattern, reproduces variants
- **Templates**: Save graph descriptions as JSON templates
- **Version control**: Diff graph descriptions as JSON instead of binary uasset

---

## Error Handling

### Failure Modes

| Failure | Behavior |
|---------|----------|
| Blueprint not found | Return `bSuccess = false` immediately. No nodes created |
| Graph not found | Return `bSuccess = false` immediately |
| Node creation fails | Skip node, log error, continue with remaining nodes |
| Connection references missing node | Skip connection, log warning |
| Pin not found on node | Skip connection, log warning with available pin names |
| Pin type mismatch | `TryCreateConnection` returns false, log warning |
| Pin default value invalid | Skip default, log warning |
| Compilation fails | Report errors but keep nodes (graph is structurally valid) |

### Error Messages

Errors include actionable context:

```
"Node 'Multiply' (ref): function_call failed — 'KismetMathLibrary::Multiply_FloatFloat' 
 not found. Did you mean 'Multiply_DoubleDouble'? (UE 5.4+ renamed Float to Double)"

"Connection 'GetSpeed.value' → 'Multiply.X' failed — pin 'X' not found on Multiply. 
 Available pins: [execute, A, B, ReturnValue]"
```

---

## Skill Updates

The `blueprint-graphs` skill needs a new section:

### When to Use BuildGraph vs Individual Methods

```
USE BuildGraph WHEN:
  - Creating a new graph from scratch (≥5 nodes)
  - Building a complete system (timer loop, health system, input handler)
  - The entire graph structure is known upfront
  - Speed matters more than step-by-step debugging

USE Individual Methods WHEN:
  - Modifying an existing graph (add 1-2 nodes to existing work)
  - Debugging a specific connection issue
  - The graph structure is being discovered incrementally
  - Working with unfamiliar node types (discover_nodes first)

HYBRID PATTERN:
  1. Use discover_nodes() to find unfamiliar spawner keys
  2. Use BuildGraph() with those keys for bulk creation
  3. Use individual get_node_pins() / connect_nodes() for surgical fixes
```

### BuildGraph Workflow

```
1. Plan the graph: list all nodes, connections, and defaults
2. If unsure about node types or pin names:
   - Call discover_nodes() to find spawner keys
   - Call get_node_pins() on a test node to learn pin names
3. Call BuildGraph with the complete description
4. Check the result:
   - If bSuccess and no errors → done
   - If partial failures → use individual methods to fix
5. If bAutoLayout was false, optionally call AutoLayoutGraph
```

---

## Python API Exposure

All three methods are exposed through the standard VibeUE Python bridge:

```python
# Batch create a graph
result = unreal.BlueprintService.build_graph(
    blueprint_path="/Game/BP_Player",
    graph_name="EventGraph",
    nodes=[
        {"ref": "BeginPlay", "type": "event", "params": {"event": "ReceiveBeginPlay"}},
        {"ref": "Print", "type": "print_string", "params": {}},
    ],
    connections=[
        {"from": "BeginPlay.then", "to": "Print.execute"},
    ],
    pin_defaults=[
        {"node": "Print", "pin": "InString", "value": "Hello from BuildGraph!"},
    ],
    auto_layout=True,
    compile_after=True
)
# result.nodes_created, result.connections_made, result.ref_to_node_id, etc.

# Auto-layout an existing messy graph
unreal.BlueprintService.auto_layout_graph(
    blueprint_path="/Game/BP_Player",
    graph_name="EventGraph"
)

# Get a graph's definition to learn its structure
nodes, connections, defaults = unreal.BlueprintService.get_graph_definition(
    blueprint_path="/Game/BP_Player",
    graph_name="EventGraph"
)
```

---

## Why Not T3D Directly?

The batch graph builder achieves the same **single-call creation** benefit without T3D's drawbacks:

| Concern | T3D Injection | BuildGraph |
|---------|--------------|------------|
| Pin names change between UE versions | Hardcoded in text → breaks silently | Resolved at runtime via `FindFunctionByName` + pin enumeration |
| Node class names change | Hardcoded → entire injection fails | Discovery-based resolution adapts automatically |
| Connection validation | None — raw text patching | `Schema->TryCreateConnection()` validates type compatibility |
| Partial failure | All-or-nothing (entire blob fails) | Individual nodes that fail are skipped; rest succeed |
| Custom/plugin nodes | Must know exact T3D syntax | `spawner_key` type works with any registered node |
| Undo support | No `FScopedTransaction` | Full undo support via scoped transaction |
| Learning from existing graphs | Can't export to T3D easily | `GetGraphDefinition` round-trips cleanly |
| LLM prompt cost | LLM must know T3D format | JSON is natural for LLMs |

The key insight: **the abstraction layer between the AI and the engine should be at the semantic level (what nodes and connections to create), not at the serialization level (what text format to inject).**

---

## Implementation Priority

1. **Phase 1**: `BuildGraph` — the core batch method (highest impact)
2. **Phase 2**: `AutoLayoutGraph` — layout independently useful even for per-node workflows
3. **Phase 3**: `GetGraphDefinition` — round-trip and learning capability

---

## Testing Strategy

### Unit Tests (C++ / Automation)

1. Create a Blueprint via `CreateBlueprint`
2. Call `BuildGraph` with a known graph (BeginPlay → PrintString)
3. Verify node count, connection count, compilation success
4. Call `GetGraphDefinition` and verify round-trip fidelity
5. Test partial failure: include one invalid node, verify others still created
6. Test auto-layout: verify no overlapping positions

### AI Test Prompts

```
"Create a Blueprint at /Game/Test/BP_BatchTest with a BeginPlay 
 that prints 'Hello' then checks if a boolean variable IsReady is true. 
 If true, call a custom event OnReady. Use BuildGraph for the entire graph."

"Export the EventGraph from /Game/ExistingBP and recreate it 
 in a new Blueprint /Game/ClonedBP using BuildGraph."

"Auto-layout the EventGraph in /Game/MessyBP — 
 it has nodes scattered everywhere."
```
