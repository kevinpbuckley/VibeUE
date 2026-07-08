# Blueprint Graph Auto-Layout Tests

Tests for `build_graph` auto-layout quality, `analyze_graph_layout` metrics, `group` comment-box
hints, and layout idempotency (issue #427). Run sequentially. If a blueprint you are asked to
create already exists, delete it silently and try again. All fixtures live in
`/Game/Dev/LayoutTest/` so cleanup is one folder delete.

Metric assertions use `analyze_graph_layout` — parse the returned JSON. Unless a prompt says
otherwise, "clean" means: `nodeOverlaps == 0`, `backwardExecWires == 0`, and `issues` is empty.

---

## Setup

Create an Actor blueprint called BP_LayoutTest in /Game/Dev/LayoutTest. Add float variables
Health, Armor, Stamina and a bool variable IsDead. Compile it.

---

## Scenario A — exec chain with pure data feeders

In BP_LayoutTest's EventGraph, use a single build_graph call with auto_layout=True to create:
a BeginPlay event, then a chain of four SetVariable nodes (Health, Armor, Stamina, then Health
again) wired in sequence off BeginPlay. Feed each set node's value from its own pure math node
(Multiply float*float), and feed each math node from a Get of a different variable. Compile.

---

Run analyze_graph_layout on BP_LayoutTest EventGraph and report the JSON. (Expected: clean —
0 nodeOverlaps, 0 backwardExecWires, empty issues, and longWires == 0 because every getter/math
node should now sit adjacent to its consumer, not in the far-left column.)

---

Using the "nodes" bounding boxes from that report, verify each Multiply node's x is within one
column of the Set node it feeds (x gap under ~900 px). (Expected: true for all four — this is
the pure-node adjacency behavior; report each gap.)

---

## Scenario B — nested branches

Make a function called BranchCascade in BP_LayoutTest. In one build_graph call
(auto_layout=True) create: entry → Branch on IsDead; True → Branch (Health > 0 comparison) whose
True and False each go to a Print String; False of the first branch → Branch (Armor > 0) whose
True and False each go to a Print String. Give the four prints distinct messages. Compile.

---

Analyze BranchCascade's layout. (Expected: clean; wireCrossings should be 0 or near 0 for a
tree this small — anything above 4 is a fail, report it.)

---

Open BP_LayoutTest, capture an editor image of the BranchCascade graph, and judge it visually:
does execution read left to right, is each branch's True path above its False path, can you
follow every wire without it passing under an unrelated node? Report pass/fail per question.

---

## Scenario C — multiple events, component bands

In BP_LayoutTest's EventGraph (keep Scenario A's nodes), add via one build_graph call: a Tick
event driving two chained Print Strings, and a custom event named OnScored driving a Set
Stamina fed by a Multiply of Get Health times Get Armor. Compile.

---

Analyze the EventGraph. (Expected: still clean — three independent components must be stacked
in separate vertical bands with no overlaps between bands; nodeOverlaps == 0.)

---

Capture an editor image of the EventGraph and verify visually: BeginPlay's band is above Tick's
band or at least clearly separated, no wires cross between bands, and each band reads left to
right. Report pass/fail.

---

## Scenario D — cycle (loop-back wire)

Make a function called RetryLoop in BP_LayoutTest. Via build_graph (auto_layout=True): entry →
Set Health → Branch on IsDead; True → Print String; False wired BACK into the Set Health exec
input (a deliberate cycle). Compile.

---

Analyze RetryLoop. (Expected: exactly 1 backwardExecWire — the loop-back — and its entry in
backwardExecWireList names the Branch → Set pair. No overlaps. The loop must NOT have inflated
the column count: bounds width should stay under ~3000 px.)

---

## Scenario E — group comment boxes

Make a function called GroupedLogic in BP_LayoutTest. In one build_graph call (auto_layout=True)
create six nodes where the first three descriptors carry params "group":"Damage Math" (a Get
Health, a Multiply, a Set Health chain segment) and the last three carry "group":"Death Check"
(a comparison, a Branch, a Print String), wired into one exec chain. Compile.

---

Check the build_graph result's ref_to_node_id map. (Expected: it contains keys
"group:Damage Math" and "group:Death Check" with valid node GUIDs — the comment boxes.)

---

Capture an editor image of GroupedLogic and verify both comment boxes exist, are titled
correctly, and fully enclose their three member nodes with no member sticking out. Report
pass/fail per box.

---

Run auto_layout_graph on GroupedLogic again, then re-capture. (Expected: the comment boxes
re-fitted around their members' new positions — members still fully enclosed.)

---

## Idempotency

Run analyze_graph_layout on the EventGraph and save the report. Run auto_layout_graph on the
EventGraph. Analyze again. (Expected: the two reports are identical — same overlaps, crossings,
bounds, and node positions. Auto-layout must be a fixed point of itself.)

---

## Selection layout

Get the nodes of Scenario C's OnScored component (the custom event and its downstream nodes)
via get_nodes_in_graph, then scatter ONLY those nodes with set_node_position to random-looking
spots (e.g. x/y offsets of several thousand). Run auto_layout_selected_nodes with just those
node ids. (Expected: those nodes are re-layered near their original bounding-box corner; every
OTHER node's position is byte-identical to before — verify by comparing analyze reports'
"nodes" arrays for the untouched GUIDs.)

---

## Cleanup

Delete the /Game/Dev/LayoutTest folder and everything in it, silently.
