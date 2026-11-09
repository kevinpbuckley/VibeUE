---
name: node-layout
description: Node layout for Blueprint graphs - auto-layout first, audit with analyze_graph_layout, group hints for comment boxes, when (rarely) to hand-place
---

This sub-doc continues from skill.md → "Node Layout Best Practices".

## Node Layout Best Practices

**Default: let auto-layout do it, then audit.** Do NOT hand-place coordinates as a first resort —
the C++ layered layout knows about dependency depth, cycles, pure-node adjacency, measured node
widths, component bands, and comment boxes, and it is idempotent.

```python
import unreal, json

# 1. Build with auto_layout=True (or run auto_layout_graph on an existing graph)
result = unreal.BlueprintService.build_graph(bp, "EventGraph", nodes, conns, defaults, True, True)

# 2. Audit — empty "issues" means the layout is clean
# (bool return is folded away by UE Python — the two out-strings come back as a tuple)
report, err = unreal.BlueprintService.analyze_graph_layout(bp, "EventGraph")
data = json.loads(report)
print(data["issues"])   # e.g. ["2 overlapping node pair(s)", "1 backward exec wire(s) — ..."]

# 3. If issues remain, fix the outliers only, then re-audit
#    data["nodes"] gives per-node bounding boxes {id, title, x, y, width, height}
unreal.BlueprintService.auto_layout_selected_nodes(bp, "EventGraph", [bad_id_1, bad_id_2])
```

### What the metrics mean

- `nodeOverlaps` / `overlappingPairs` — must be 0; any overlap is a fail.
- `backwardExecWires` — exec flowing right-to-left. 0 except deliberate loop-backs (a cycle
  contributes exactly one: the loop wire).
- `wireCrossings` — straight-line approximation; a readable graph keeps this well under the
  wire count.
- `longWires` — wires over 1500 px; usually a producer far from its consumer.
- `execWireMeanAbsDeltaY` — 0 means a perfectly horizontal exec backbone.

### Grouping into comment boxes

Add `"group":"<title>"` to any build_graph node descriptor — each distinct title becomes a
comment box wrapping its members after layout (GUID under `ref_to_node_id["group:<title>"]`).
Re-running `auto_layout_graph` re-fits existing comment boxes around the nodes they contained.

```python
{"ref": "GetHP", "type": "variable_get", "params": {"variable": "Health", "group": "Damage Math"}}
```

### When to hand-place (`set_node_position`)

Only for deliberate visual conventions the algorithm can't know — e.g. pinning a FunctionResult
at a fixed spot, or matching a human-authored graph's style. Layout runs left-to-right from
roughly (100, 100); exec flows rightward, data feeders sit in the column left of their consumer,
a branch's True path lands above its False path. If you hand-place, re-run
`analyze_graph_layout` afterwards to prove you didn't introduce overlaps.
