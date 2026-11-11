---
name: node-layout
description: Blueprint graph layout - arrange with BlueprintToolset.ArrangeGraphLayered / ArrangeNodes, measure with the surviving VibeUE analyze_graph_layout, what each metric means, comment boxes, when (rarely) to hand-place
---

This sub-doc continues from SKILL.md → "VibeUE's surviving graph tools".

# Node Layout

The split here is clean: **Epic writes layouts, VibeUE measures them.** Epic's layout tools only
*apply* an arrangement — nothing in `BlueprintToolsets` or `EditorToolset` reads one back — which is
why `analyze_graph_layout` is one of VibeUE's 19 survivors.

## The loop

**Arrange → measure → fix the outliers → re-measure.** Do not hand-place coordinates as a first
resort: the engine's layered layout already knows about connected components, execution chains and
comment boxes, and it does not need an open graph editor.

```python
import json, unreal, vibeue

BT = "BlueprintToolsets.BlueprintToolset"
bp, graph_name = "/Game/BP/BP_Door", "EventGraph"

def ref(p):
    tail = p.rsplit("/", 1)[-1]
    return {"refPath": p if "." in tail else "{}.{}".format(p, tail)}

# 1. Arrange the whole graph (Epic).
graph = vibeue.exec_tool(BT, "GetGraph", {"blueprint": ref(bp), "graphName": graph_name})
vibeue.exec_tool(BT, "ArrangeGraphLayered", {"graph": graph})

# 2. Measure it (VibeUE). The bool return is folded away by UE Python, so the two
#    out-strings come back as a tuple.
report, err = unreal.BlueprintService.analyze_graph_layout(bp, graph_name)
data = json.loads(report)
print(data["issues"])    # [] means clean

# 3. Fix only the offenders, then re-measure. overlappingPairs are display strings
#    ("Branch <-> Print String"); data["nodes"] gives every box as
#    {id, title, x, y, width, height}, where id is the node's GUID.
if data["issues"]:
    print("offenders:", data["overlappingPairs"], data["backwardExecWireList"])
    # ArrangeNodes takes node HANDLES, not GUID strings. FindNodes(graph, title=...) is the
    # cheapest way to get a handle back for a node you identified by title.
    bad = vibeue.exec_tool(BT, "FindNodes", {"graph": graph, "title": "Print String"})
    vibeue.exec_tool(BT, "ArrangeNodes", {"graph": graph, "nodes": bad})
    report, err = unreal.BlueprintService.analyze_graph_layout(bp, graph_name)
    print(json.loads(report)["issues"])
```

A DSL `Write` also produces a reasonable layout on its own, so the common sequence is
`BlueprintDslToolset.Write` → `analyze_graph_layout` → (only if `issues` is non-empty)
`ArrangeNodes` on the offenders → re-measure.

## Epic's layout tools

| Tool | Scope |
|---|---|
| `ArrangeGraphLayered(Graph, PinnedNodes?, RestrictToIslandOf?)` | The **whole graph**, left-to-right, one horizontal band per connected component, execution chains on a single straight row, comment boxes honoured as nested groups and rebuilt around their members |
| `ArrangeNodes(Graph, Nodes)` | **Just those nodes**, using every other node in the graph as a fixed anchor so the subset integrates with the rest. Comment boxes whose members are all inside the touched region are refitted |
| `SetNodePosition(Node, Pos)` | One node, exactly where you say |

Two useful options on `ArrangeGraphLayered`:

- **`PinnedNodes`** — nodes that keep their current position. They steer placement and are avoided,
  but they do not define columns. Use this for a node a human deliberately placed.
- **`RestrictToIslandOf`** — arranges only the connected component containing that node and leaves
  everything else alone. Use it when a graph has several independent event chains and you only
  touched one.

Neither needs the Blueprint editor to be open.

## What `analyze_graph_layout` returns

`analyze_graph_layout(blueprint_path, graph_name)` → `(report_json, error)`. The JSON carries:

| Field | Meaning |
|---|---|
| `nodeCount`, `wireCount`, `execWireCount` | sizes, for context on the ratios below |
| `nodeOverlaps` + `overlappingPairs` | **must be 0** — any overlap is a fail. The pairs are display strings, `"TitleA <-> TitleB"`, capped at 50 |
| `backwardExecWires` + `backwardExecWireList` | exec flowing right-to-left. 0 except deliberate loop-backs: a cycle contributes exactly one, the loop wire |
| `wireCrossings` | straight-line approximation; a readable graph keeps this well under the wire count |
| `longWires` | wires over 1500 px — usually a producer placed far from its consumer |
| total / average wire length | bulk readability signal |
| `execWireMeanAbsDeltaY` | 0 means a perfectly horizontal exec backbone |
| `bounds` | the graph's extent |
| `longWireList` | the offending wires, as display strings |
| `issues` | summary array — **empty means the layout looks clean**, and this is the field to gate on |
| `nodes` | per-node bounding boxes `{id, title, x, y, width, height}`; `id` is the node's GUID |
| `note` | reminder that sizes are *estimated* widget dimensions and crossings are a straight-line approximation |

Comment boxes are ignored by the measurement — they are decoration, not structure.

Gate on `issues` being empty, and quote the specific numbers when you report a layout as good. A
`None`/empty report means the Blueprint or graph did not load: check PIE, the path, and the graph
name before concluding the graph is unmeasurable.

## Comment boxes

`BlueprintToolset.AddCommentBox(Graph, CommentText, Nodes?, Rect?, Padding?)` does both jobs the old
pair of VibeUE methods did:

- Pass **`Nodes`** to wrap them in a box sized to their bounds, with `Padding` between the nodes and
  the border. This is the programmatic select-then-press-`C`.
- Pass **`Rect`** (min is the top-left corner) for a standalone box. Required when you give no nodes.

The rest of the surface: `ListCommentBoxes`, `GetNodesInCommentBox`, `GetCommentBoxText`,
`SetCommentBoxText`, `RemoveCommentBox`. Standard editor behaviour applies — K2 nodes whose bounds
fall inside a box get picked up and dragged with it.

Both arrange tools rebuild comment boxes around their members, so grouping survives a re-layout.
Write comment text that explains **intent**, not node names — see the comment-box rules in
`SKILL.md`.

## When to hand-place

Only for a deliberate visual convention the algorithm cannot know: pinning a Function Result at a
fixed spot, matching a human-authored graph's style, or holding a node a designer positioned.
Prefer `ArrangeGraphLayered`'s `PinnedNodes` over `SetNodePosition`, because pinning keeps the rest
of the layout consistent around the fixed node instead of fighting it.

Layout runs left-to-right: exec flows rightward, data feeders sit in the column left of their
consumer, and a branch's `then` path lands above its `else`. If you hand-place anything, re-run
`analyze_graph_layout` afterwards to prove you did not introduce an overlap.
