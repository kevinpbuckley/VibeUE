# Map Blockout Failure Recovery Tests

These deliberately drive the pipeline into failing gates and exercise the repair knobs. The MapDesigner spec's whole point is that every failure is *named* and there's a specific knob to turn — these tests prove that loop works.

Run sequentially. Each `---` is one prompt. Uses the Verkhova fixture so no live landscape is needed.

---

## Setup

Load the Verkhova fixture from `Plugins/VibeUE/Source/VibeUE/Tests/MapBlockout/example_data/landcover_grid.json`. Build a base config (`level_name="VerkhovaRecovery"`, layers `crop=L1, soil=L2, flood=L3, forest=L4`, seed=7) and confirm `run_full_pipeline` passes all gates as a baseline.

---

## Stage 1 Failure — Disconnected Road Network

Increase `road.dirt_spacing_km` to a huge value (e.g. 8.0) so the dirt grid collapses to almost nothing and connectivity fails. Run `generate_roads` only and confirm the `Connectivity` check fails with a largest-component % below 98.5%.

---

Now reduce `road.dirt_spacing_km` back to a sane value (1.7) and re-run. Connectivity should pass. Did `Grid Sensibility` and the other checks all come back true?

---

## Stage 2 Failure — Too Few POIs

Drop `pois.target_count` to 3 (way under the POI_MIN floor for a 12 km map). Run `place_pois` and confirm `POI Count` fails with a count < 15.

---

Restore `pois.target_count` to 16 and confirm `POI Count` passes.

---

## Stage 3 Failure — Field Coverage Too Low

Set `field_crop_threshold` to 0.6 (very strict — almost no cells qualify). Run through Stage 1 → Stage 2 → Stage 3 and confirm the `Coverage` check fails with `coverage < 50%`.

---

Now sweep `field_crop_threshold` down: try 0.4, 0.2, 0.12, 0.08. At what threshold does coverage first land in the 50–60% band? Report.

---

## Stage 3 Failure — Field Coverage Too High

Now go the other way: set `field_crop_threshold` to 0.02 (almost every cell with crop weight qualifies). Re-run Stage 3 and confirm `Coverage` fails on the upper bound (> 60%).

---

Repair by tightening: which knob turn (lower fields or raise forests) gets it back in band? Try `forest_fringe_iters` increase from 9 → 15 and observe.

---

## Stage 4 Failure — Tree Coverage Too Low

Set `forest_fringe_iters` to 0 (no treeline ring around forests). Run through Stages 1–4 and confirm `Coverage 30-40%` fails with tree % below the floor.

---

Sweep `forest_fringe_iters` up (3, 6, 9, 12) and find where coverage lands in the 30–40% band.

---

## Stage 4 Failure — No Forest-Surrounded POI

This is the trickiest check. It requires Stage 2 to have placed a `Strongpoint` POI AND Stage 4 to have grown a forest ring around it (≥85% of the ring area should be forest).

Set up a config where the forest layer has very low signal near the strongpoint (you can simulate by setting `cfg.layers.forest = ""` — no forest source). Run through Stages 1–4 and confirm `Forest-Surrounded POI` fails with ring % below 85.

---

Restore `cfg.layers.forest = "L4"` (the Verkhova fixture's forest layer) and re-run. The check should pass.

---

## Stage 5 Failure — Bridges Over Water Only

This is hard to trigger naturally — the bridge detection only fires at water/land transitions. But you can force it by manually adding a bogus bridge to the result:

```python
rail_result = stage5  # FMapBlockoutRailwayResult
fake = unreal.MapBlockoutBridge()
fake.world = unreal.Vector2D(0, 0)
fake.length_cm = 10000
fake.yaw_degrees = 0
fake.carries = unreal.MapBlockoutRoadType.MAIN
rail_result.bridges.append(fake)
```

Then call `run_final_pass` on a state with the modified rail and confirm the cross-layer integrity catches a bridge on land (or that Stage 5's gate already filtered it — which is the actual spec intent).

---

## Repair Cheatsheet Verification

Run the baseline config again and confirm every check passes. Then deliberately break one knob at a time, observe which named check fails, and apply the documented repair from the skill's [stage-rules.md](../../Content/Skills/map-blockout/stage-rules.md) "Repair" table. Confirm each repair returns the pipeline to passing.

The matrix to verify:

| Knob change | Expected failing check | Repair |
|---|---|---|
| `road.dirt_spacing_km = 8.0` | Stage 1 `Connectivity` | Lower `dirt_spacing_km` |
| `pois.target_count = 3` | Stage 2 `POI Count` | Raise `target_count` |
| `field_crop_threshold = 0.6` | Stage 3 `Coverage` (under) | Lower threshold |
| `field_crop_threshold = 0.02` | Stage 3 `Coverage` (over) | Raise threshold |
| `forest_fringe_iters = 0` | Stage 4 `Coverage` (under) | Raise fringe iters |
| `cfg.layers.forest = ""` | Stage 4 `Forest-Surrounded POI` | Provide a forest layer |

Walk through all six rows and report any that don't behave as documented.
