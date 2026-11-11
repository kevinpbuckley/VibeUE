# Blueprint Graph Layout Tests

Tests the quality of UE 5.9's **own** graph arrangers, measured with the one layout tool VibeUE
still owns.

The split matters for reading these results. Authoring is `BlueprintToolsets.BlueprintDslToolset`
(`Write` / `Edit`), arranging is `BlueprintToolsets.BlueprintToolset`
(`ArrangeGraphLayered` for a whole graph, `ArrangeNodes` for a subset), and both are the engine's.
`analyze_graph_layout` is VibeUE's, and it is the only thing in either surface that **measures** a
layout — Epic's arrangers are write-only, so without it there is no way to tell whether an arrange
helped. That is the whole reason it survived the 5.9 cut.

So a failure here is usually a finding about the engine, not a bug in VibeUE. Report what you
measure rather than forcing a pass, and say which side you think owns each problem.

All fixtures live in `/Game/Dev/LayoutTest/` so cleanup is one folder delete. Run sequentially. If a
blueprint you are asked to create already exists, delete it silently and try again.

## The report

`unreal.BlueprintService.analyze_graph_layout(blueprint_path, graph_name)` returns
`(report_json, error)`. Parse the JSON. Fields:

| Field | Meaning |
|---|---|
| `nodeCount`, `wireCount`, `execWireCount` | sizes |
| `nodeOverlaps` + `overlappingPairs` | overlapping node pairs. **`overlappingPairs` is an array of display strings** (`"A <-> B"`), capped at 50, not id pairs |
| `wireCrossings` | straight-line approximation, not the spline the editor draws |
| `backwardExecWires` + `backwardExecWireList` | exec wires running right-to-left |
| `longWires` + `longWireList` | wires over the long threshold |
| `totalWireLength`, `avgWireLength`, `execWireMeanAbsDeltaY` | aggregate quality; `execWireMeanAbsDeltaY` near 0 means the exec chain reads as a straight horizontal line |
| `bounds` | `minX`/`minY`/`maxX`/`maxY` |
| `issues` | the analyzer's own complaints |
| `nodes` | per-node `id` (GUID) and bounding box |
| `note` | states that sizes are estimates and crossings are straight-line |

Read `note` before trusting a borderline number: sizes are estimated widget dimensions and
crossings ignore spline curvature, so small non-zero values may be measurement error rather than a
real layout fault.

Unless a prompt says otherwise, "clean" means `nodeOverlaps == 0`, `backwardExecWires == 0`, and
`issues` empty.

---

## Setup

Create an Actor blueprint called BP_LayoutTest in /Game/Dev/LayoutTest. Add float variables Health,
Armor, Stamina and a bool variable IsDead, then compile. Variables and compile are engine-side:
`BlueprintToolset.AddVariable` and `CompileBlueprint`.

Before your first `Write`, call `BlueprintDslToolset.Docs` once for the grammar, and `Schema` for any
node type you are unsure of. Keep the `Revision` each `Read`/`Write` returns; `Edit` needs the latest
one and is refused on a stale one.

---

## Scenario A — exec chain with pure data feeders

In BP_LayoutTest's EventGraph, in one `BlueprintDslToolset.Write`, create: a BeginPlay event, then a
chain of four Set nodes (Health, Armor, Stamina, then Health again) wired in sequence off BeginPlay.
Feed each set node's value from its own pure Multiply (float * float), and feed each Multiply from a
Get of a different variable. Compile.

Then run `BlueprintToolset.ArrangeGraphLayered` on the EventGraph. There is no auto-layout flag on
the DSL — authoring and arranging are two calls now, which is worth noting as a behaviour change
from the old single `build_graph(auto_layout=True)`.

---

Run `analyze_graph_layout` on BP_LayoutTest EventGraph and report the JSON.

Report, don't assume: does `ArrangeGraphLayered` place each pure feeder adjacent to its consumer, or
does it park them all in a far-left column? `longWires == 0` and a low `avgWireLength` mean adjacent;
a high `longWires` with the getters clustered at `bounds.minX` means the far-left column. Say which
you see.

---

Using the `nodes` bounding boxes from that report, measure each Multiply node's x gap to the Set node
it feeds. Report each of the four gaps. (A gap under roughly one column, ~900 px, is adjacency.)

---

## Scenario B — nested branches

Make a function called BranchCascade in BP_LayoutTest. In one `Write`: entry → Branch on IsDead;
True → Branch (Health > 0) whose True and False each go to a Print String; False of the first branch
→ Branch (Armor > 0) whose True and False each go to a Print String. Give the four prints distinct
messages. Compile, then `ArrangeGraphLayered`.

---

Analyze BranchCascade's layout. Expect clean. `wireCrossings` should be 0 or near 0 for a tree this
small — report the number, and remember it is a straight-line approximation, so 1–2 on a small tree
may be measurement error rather than a real crossing.

---

Open BP_LayoutTest, capture an editor image of the BranchCascade graph, and judge it visually: does
execution read left to right, is each branch's True path above its False path, can you follow every
wire without it passing under an unrelated node? Report pass/fail per question. This is the check
the metrics cannot make — the straight-line crossing count does not know what the editor actually
draws.

---

## Scenario C — multiple events, component bands

In BP_LayoutTest's EventGraph (keep Scenario A's nodes), add in one `Write`: a Tick event driving two
chained Print Strings, and a custom event named OnScored driving a Set Stamina fed by a Multiply of
Get Health times Get Armor. Compile, then `ArrangeGraphLayered`.

Note that `Write` is upsert — omitted top-level entries stay and produce a warning — so adding
entries does not require resending Scenario A's.

---

Analyze the EventGraph. Three independent components should be stacked in separate vertical bands
with no overlaps between them: `nodeOverlaps == 0`. Report whether the bands are genuinely separated
by comparing the three components' y ranges in `nodes`.

---

Capture an editor image of the EventGraph and verify visually: each band reads left to right, the
bands are clearly separated, and no wires cross between bands. Report pass/fail.

---

## Scenario D — cycle (loop-back wire)

Make a function called RetryLoop in BP_LayoutTest. Via `Write`: entry → Set Health → Branch on
IsDead; True → Print String; False wired BACK into the Set Health exec input (a deliberate cycle).
Compile, then `ArrangeGraphLayered`.

---

Analyze RetryLoop. Expect exactly 1 `backwardExecWires` — the loop-back — with its
`backwardExecWireList` entry naming the Branch → Set pair, and no overlaps. Check that the cycle did
not inflate the column count: `bounds` width should stay under roughly 3000 px. A layered arranger
that treats a back edge as a forward one is the classic failure, and this is the scenario that
catches it.

---

## Scenario E — comment boxes

Make a function called GroupedLogic in BP_LayoutTest. Create six nodes wired into one exec chain: a
Get Health, a Multiply and a Set Health, then a comparison, a Branch and a Print String. Compile.

Then group them with `BlueprintToolset.AddCommentBox` — one box titled "Damage Math" around the first
three, one titled "Death Check" around the last three. The old `build_graph` `"group"` descriptor and
its `ref_to_node_id` map are gone; comment boxes are their own engine tool now, which means you place
the nodes first and box them afterwards.

---

Verify both boxes with `ListCommentBoxes` and `GetNodesInCommentBox`, and report which nodes each box
actually contains.

---

Capture an editor image of GroupedLogic and verify both comment boxes exist, are titled correctly,
and fully enclose their three member nodes with none sticking out. Report pass/fail per box.

---

Run `ArrangeGraphLayered` on GroupedLogic again, then re-capture and re-run `GetNodesInCommentBox`.

This is the interesting one: **does Epic's arranger re-fit comment boxes around their members' new
positions, or does it move nodes out from under their boxes?** Report what you observe. If members
end up outside their box, that is a finding about `ArrangeGraphLayered`, and worth recording with the
image as evidence.

---

## Idempotency

Run `analyze_graph_layout` on the EventGraph and save the report. Run `ArrangeGraphLayered` on it.
Analyze again.

Compare the two reports: overlaps, crossings, bounds, and the per-node positions in `nodes`. An
arranger should be a fixed point of itself — arranging an already-arranged graph should change
nothing. Report any node whose position moved, by GUID. If positions drift, say by how much and
whether it converges on a third run.

---

## Selection arrange

Take Scenario C's OnScored component (the custom event and its downstream nodes). Find them with
`BlueprintToolset.FindNodes` / `GetNodeInfos`, then scatter ONLY those nodes with `SetNodePosition` to
random-looking spots (x/y offsets of several thousand).

Now run `ArrangeNodes` on just those nodes. It takes the subset to arrange, and also has
`PinnedNodes` and `RestrictToIslandOf` — check `describe_toolset` for the exact argument names before
calling it.

Expected: those nodes are re-layered near their original bounding-box corner, and every OTHER node's
position is unchanged. Verify by comparing the `nodes` arrays of the before/after analyze reports for
the untouched GUIDs — they should be identical. Report any untouched node that moved, since that
would mean `ArrangeNodes` is not honouring its subset.

---

## Cleanup

Delete the /Game/Dev/LayoutTest folder and everything in it, silently.
