# Blueprint Node Discovery Tests — discover_nodes / create_node_by_key

Exercises `discover_nodes` (search the Blueprint action database) and `create_node_by_key`
(deterministic editor-style node creation). Covers result-size limits, filtering, the full
node-type coverage (functions, events, Get Subsystem and other template nodes), and that
`SPAWN` keys create fully-bound nodes. Run sequentially.

---

## Setup

Create an Actor Blueprint called BP_NodeDiscoveryTest in the Blueprints folder. Delete it first if it already exists.

---

# Result Size & Filtering

The point of this section: `discover_nodes` must NEVER dump the whole action database — it is
capped by `max_results` (default 20) and narrowed by the search term / category.

---

## Default cap

On BP_NodeDiscoveryTest, run a node search for "Get" without passing `max_results`. Report how many results came back. Confirm it returns at most the default of 20 — NOT hundreds or thousands. This is the guard that discovery never returns everything at once.

---

## The cap is a limit, not a fixed list

Run the same "Get" search again but ask for `max_results = 100`. Confirm you now get more than 20 results. This proves the count is bounded by `max_results`, and that lowering it genuinely limits payload size.

---

## A specific term returns few, relevant hits

Search for "Set Actor Location". Confirm the result set is small and every hit is clearly relevant to the term (the term matched the display name, keywords, or spawner key). Note that this is the recommended way to use discovery — specific term, small result set.

---

## Category filter

Search with an empty term but category "Flow Control". Confirm the results are flow-control nodes (Branch, Sequence, etc.) and nothing unrelated.

---

## Spaceless menu names

Search "Enhanced Input" (with a space) and then "EnhancedInput" (no space). Report the difference. Confirm that node menu names are spaceless, so the no-space term is the one that matches node types like the Enhanced Input subsystem getters. This is the most common discovery mistake.

---

# Node-Type Coverage

discover_nodes must surface ALL node types from the action database, not just functions and events.

---

## Functions (FUNC keys)

Search "Jump". Confirm at least one result has a `spawner_key` of the form `FUNC <Class>::Jump` and a `node_class` of `K2Node_CallFunction`.

---

## Events (EVENT keys)

Search "BeginPlay". Confirm an event spawner is returned (an "Event BeginPlay" style entry the Actor can implement).

---

## Get Subsystem template nodes (SPAWN keys)

Search "EnhancedInputLocalPlayerSubsystem" with `max_results = 100`. Confirm at least one result has `node_class` of `K2Node_GetSubsystem` (or `K2Node_GetSubsystemFromPC`) and a `spawner_key` that starts with `SPAWN `. List the subsystem-getter node classes you see.

---

## Cast nodes (SPAWN keys)

Search "Cast To Pawn". Confirm a `K2Node_DynamicCast` result comes back with a `SPAWN ...` key.

---

## Async / latent action nodes (SPAWN keys)

Async action nodes (the online-session family and friends) register through a *function*
spawner whose factory is marked `BlueprintInternalUseOnly`, yet the real node is a
`K2Node_AsyncAction`. Discovery must NOT drop these (the bug from issue #483) and must surface
them with a `SPAWN K2Node_AsyncAction|...` key — never a `FUNC ...::Create Session` key (a FUNC
key would build a plain Call Function node, not the async node).

Run each of these searches and confirm a result comes back whose `node_class` is
`K2Node_AsyncAction` and whose `spawner_key` starts with `SPAWN K2Node_AsyncAction|`:

- "Create Session"
- "Find Sessions"
- "Join Session"
- "Destroy Session"

Confirm NONE of these return zero results, and that the returned key is a `SPAWN` key (not a
`FUNC` key). This is the exact regression: before the fix these four returned 0 hits.

---

## Other latent / async nodes are discoverable

Confirm these common latent/async nodes also enumerate (each should return at least one usable
result — a `FUNC` key for latent library calls, or a `SPAWN K2Node_AsyncAction|...` key for
async-action nodes):

- "Delay" — latent `FUNC` call (KismetSystemLibrary::Delay).
- "AI MoveTo" — async/latent movement node.
- "Load Stream Level" — latent `FUNC` call.
- "Open Level" — `FUNC` travel call (the Blueprint-facing replacement for raw ClientTravel).

Report the `node_class` and `spawner_key` for each. The point: latent/async coverage is no
longer a blind spot in discovery.

---

# Deterministic Creation From Keys

Each discovered `spawner_key` must feed straight back into `create_node_by_key` and produce the
correct, fully-bound node.

---

## Create from a FUNC key

Take the `FUNC ...::Jump` key from the Jump search and create that node in BP_NodeDiscoveryTest's EventGraph. Confirm a valid node ID comes back and the node is a Jump call.

---

## Create a bound Get Subsystem node from a SPAWN key

Take the `SPAWN K2Node_GetSubsystem|Get EnhancedInputWorldSubsystem` key (search "EnhancedInputWorldSubsystem" if you need the exact key) and create that node. Then read the node's pins and confirm its `ReturnValue` output pin is typed to the EnhancedInputWorldSubsystem class — i.e. the node came out BOUND to its subsystem, not as a blank node. This is the key behavior: `SPAWN` keys invoke the real editor spawner.

---

## Discovery ↔ creation parity for an async node

Take the `SPAWN K2Node_AsyncAction|Create Session` key returned by the "Create Session" search
above and feed it straight into `create_node_by_key` on BP_NodeDiscoveryTest's EventGraph.
Confirm a valid node ID comes back, the node is a `K2Node_AsyncAction`, and reading its pins
shows the async exec outputs (`then` plus `OnSuccess` / `On Success` and `OnFailure` / `On
Failure`). This proves the key discovery surfaces is exactly the key creation accepts — the same
node the editor's right-click menu would add — so an agent never has to fall back to C++ for
session nodes.

---

## Macro nodes are NOT discoverable (expected)

Search "ForEachLoop". Confirm it does NOT return a usable spawner key (standard macros are `K2Node_MacroInstance` with no spawner). Note that the correct path for those is `add_macro_instance_node`, not `create_node_by_key`.

---

## Compile

Compile BP_NodeDiscoveryTest and confirm it compiles (the Jump + Get Subsystem nodes are valid even if unwired).

---

## Cleanup

Delete BP_NodeDiscoveryTest.

---

# Expected Results

1. No-arg "Get" search returns ≤ 20 results; `max_results = 100` returns more — proving the cap bounds payload size and discovery never returns the whole database at once.
2. A specific term yields a small, relevant result set; the category filter narrows to that category.
3. Spaceless menu names: "EnhancedInput" matches where "Enhanced Input" does not.
4. Full coverage: FUNC (functions), EVENT (events), and SPAWN (Get Subsystem variants, Cast To, async actions) keys all appear.
5. Async/latent coverage: "Create Session", "Find Sessions", "Join Session", "Destroy Session" each return a `K2Node_AsyncAction` result with a `SPAWN K2Node_AsyncAction|...` key (never 0 hits, never a FUNC key); Delay / AI MoveTo / Load Stream Level / Open Level all enumerate.
6. `create_node_by_key` works for FUNC and SPAWN keys; a SPAWN'd Get Subsystem node is bound (ReturnValue typed to the subsystem class); the `SPAWN K2Node_AsyncAction|Create Session` key creates the real async node with `then` / `OnSuccess` / `OnFailure` exec outputs (discovery ↔ creation parity).
7. ForEachLoop / standard macros are correctly NOT returned as spawner keys.
8. BP_NodeDiscoveryTest compiles, then is deleted.
