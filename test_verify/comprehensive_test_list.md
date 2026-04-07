# VibeUE Comprehensive Test List

Run in order. Tier 1 must pass before proceeding. Regression markers should be woven into the relevant tier rather than run as a separate pass.

**All assets created during tests go in `/Game/VerifyTests/`** — keeps everything visible and easy to clean up. Tell Claude to put everything there. Delete the folder when done.

---

## Tier 1 — Smoke / Sanity (run first)

**Connection & Basic Execution**
- Print engine version via `execute_python_code`
- List all editor subsystems (`list_python_subsystems`)
- Read recent UE logs (`read_logs` with alias `main`)
- Search for any existing asset in the project (`manage_asset` search)

---

## Tier 2 — Blueprint Core

**Lifecycle**
- Create actor blueprint in `/Game/VerifyTests/` → compile → check state
- Reparent blueprint (Character → Pawn)
- Toggle replication on/off and verify

**Variables**
- Add float, bool, int, string variables
- Add object reference variable (hits known issue #6 — good regression marker)
- Set/get defaults

**Components**
- Add SpotLight, PointLight, StaticMesh, CapsuleComponent
- Set component properties (intensity, radius, etc.)

**Functions & Nodes**
- Create function with input/output pins
- Add function call node, branch node, print string
- Connect nodes across exec/data pins
- Wire damage calculator (base damage × random → output)

**Input Actions**
- Discover available input actions
- Add Enhanced Input binding nodes

---

## Tier 3 — Asset Management

- Search by name, by class, by path
- Open asset in editor
- Duplicate asset into `/Game/VerifyTests/`
- Move asset within `/Game/VerifyTests/`
- Delete asset (force)
- List all assets in `/Game/VerifyTests/`

---

## Tier 4 — UMG / UI

**Widget Blueprint**
- Create widget blueprint in `/Game/VerifyTests/`
- Add Image with dark background
- Add VerticalBox with Button children
- Add TextBlock inside buttons
- Compile widget

**Widget v2 APIs** *(added 2026-03-24)*
- Set font on a TextBlock (`set_font`)
- Get font from a TextBlock (`get_font`)
- Set brush on an Image (`set_brush`)
- Get brush from an Image (`get_brush`)
- Rename a widget component (`rename_widget`)
- Bind an event to a button click (`bind_event` — requires WidgetName param)
- Add a widget animation and keyframe a property
- Use `list_functions` to verify uncompiled functions are visible

**ViewModel Binding**
- Create MVVM viewmodel in `/Game/VerifyTests/`
- Bind widget properties to viewmodel fields

---

## Tier 5 — Materials

**Basic Material**
- Create material in `/Game/VerifyTests/`, set blend mode, shading model
- Add scalar/vector parameters

**Material Nodes**
- Add Multiply, Lerp, Texture Sample nodes
- Connect to Base Color, Roughness, Normal outputs

**Advanced Recreation**
- Recreate a multi-layer material from description

---

## Tier 6 — Landscape

- Create landscape in current level
- Apply landscape material
- Add landscape splines
- Set up RVT pipeline
- Auto-material layer tools
- Landscape + foliage demo

---

## Tier 7 — Foliage

- Add foliage type to painter
- Set density, scale min/max
- Paint foliage on landscape
- Clear foliage

---

## Tier 8 — State Trees

These are the most complex tests — best signal on multi-step reasoning quality.

- Create StateTree asset in `/Game/VerifyTests/`, add root state
- Add state properties (struct-typed)
- Add StateTree parameters
- Add transitions (on completion, on event, delegate-based)
- Add tasks (enter/tick/exit logic)
- Add conditions (tag-based, property comparisons)
- Add evaluators
- End-to-end: full AI enemy behavior tree

---

## Tier 9 — Animation

**Animation Sequence**
- Discover animation sequences
- Read bone tracks and poses
- Add/remove notifies
- Add/remove curves
- Sync markers, root motion
- Create new animation in `/Game/VerifyTests/`

**Skeleton**
- Query bone hierarchy
- Modify bones
- Add/manage sockets
- Retargeting setup
- Blend profiles

**Animation Blueprint**
- Create AnimBP in `/Game/VerifyTests/`
- Add state machine
- Add blend spaces

**Montage**
- Create montage in `/Game/VerifyTests/`
- Add montage sections
- Set blend in/out

---

## Tier 10 — Data Assets

**Enums & Structs**
- Create enum in `/Game/VerifyTests/`, add values
- Create struct in `/Game/VerifyTests/`, add typed fields

**Data Asset**
- Create data asset from class in `/Game/VerifyTests/`
- Read/write fields

**Data Table**
- Create data table with row struct in `/Game/VerifyTests/`
- Add/read rows

---

## Tier 11 — Enhanced Input

- Create Input Mapping Context in `/Game/VerifyTests/`
- Add Input Actions (movement, look, jump)
- Assign modifiers/triggers
- Bind to player controller in blueprint

---

## Tier 12 — Niagara

- Create Niagara system in `/Game/VerifyTests/`
- Add emitter
- Set spawn rate, lifetime, velocity
- Compile system

---

## Tier 13 — Sound Cues

- Load `sound-cues` skill
- Create Sound Cue asset in `/Game/VerifyTests/`
- Add sound wave node
- Add modulator (volume/pitch)
- Add random node with multiple waves
- Set attenuation settings
- Compile and verify

---

## Tier 14 — MetaSound *(new — verify build first)*

- Load `metasounds` skill
- Create MetaSound Source asset in `/Game/VerifyTests/`
- Add an Input node (float parameter)
- Add an Output node (Audio mono)
- Connect nodes — verify auto-strip of `:DataType` suffix on pin names (e.g. `Out Mono:Audio` → `Out Mono`)
- Set a float input default value
- Build MetaSound to asset
- Query node list and verify structure
- End-to-end: simple procedural tone with controllable pitch

---

## Tier 15 — Terrain Data (real-world)

- Fetch heightmap for a real-world location
- Query water features (rivers, lakes, coastlines)
- Import heightmap as UE landscape

---

## Tier 16 — Deep Research

- Web search for UE API docs
- Fetch a specific docs page
- Geocode a location (for terrain data workflow)

---

## Tier 17 — Gameplay Tags

- Add gameplay tags to project
- Query tags on an actor
- Filter assets by gameplay tag

---

## Tier 18 — Logs & Diagnostics

- Read `main` log, filter by severity
- Read `chat` log (VibeUE chat history)
- Read `llm` log
- Search logs for a specific error string

---

## Regression Markers

Probe these specifically to catch regressions on known-fixed issues:

| Test | Issue |
|---|---|
| Create node, verify GUID is non-null | #1 |
| Add Branch node, connect True/False (not then/else) | #8 |
| Add variable, verify type name is correct | #9 |
| Connect nodes to a default K2Node_Event | #5 |
| Add object reference variable — expect failure, log it | #6 (still open) |
| Call `get_default_object` read-only — should succeed | CDO safety filter |
| Attempt inline CDO modification — should be blocked | CDO safety filter |
| MetaSound `connect_nodes` with `:DataType` suffix on pin name — should auto-strip | MetaSound pin fix |

---

## Suggested Run Order

1. **Tier 1** — Smoke test. Bail if this fails.
2. **Tier 2 + 3** — Blueprint core + asset management. Most-used path.
3. **Tier 4 + 5** — UMG + materials. Second most common.
4. **Tier 8** — State trees. Highest complexity, best reasoning signal.
5. **Tier 13 + 14** — Sound Cue then MetaSound. Do 13 before 14.
6. **Tiers 6, 7, 9–12, 15–18** — Based on which features need validation.

Regression markers should be run inline with the most relevant tier, not as a standalone pass.

---

## Cleanup

When done: delete `/Game/VerifyTests/` entirely via `manage_asset` force delete.
