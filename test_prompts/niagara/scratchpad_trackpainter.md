# Niagara Scratch Pad: Track Painter

End-to-end natural-language test of `NiagaraScratchPadService` against the persistent vehicle
track-painting scenario from `issues/ChatHistory.json`. Run sequentially. Each `---` is a turn.

This test exercises the gap fixed in this branch: the prior session could neither populate a
scratch-pad graph nor reach the scratch UNiagaraScript at all, and ultimately corrupted the stack
and crashed the editor. After these turns the system should compile clean with the splat module
in place.

---

## Setup

Search for an existing `NS_TrackPainter` niagara system anywhere in `/Game` and tell me where it
is (or that none exists).

---

If `NS_TrackPainter` exists, summarize it (emitters, user parameters, stack modules). If it does
not exist, create `NS_TrackPainter` at `/Game/_GameplaySystems/SYS_TracksManager/Niagara` and add
a minimal emitter called `CompletelyEmpty` to it.

---

## User parameters (the inputs BP_TrackManager will write each frame)

On `NS_TrackPainter`, make sure the following User parameters exist (create any that are missing):

- User.BatchSize          (Int, default 200)
- User.DecayRate          (Float, default 0.997)
- User.GridWidth          (Int, default 512)
- User.GridHeight         (Int, default 512)
- User.TireRadius         (Float, default 45)
- User.VolumeMin          (Vector, default `(X=-5000,Y=-5000,Z=-500)`)
- User.VolumeMax          (Vector, default `(X=5000,Y=5000,Z=500)`)
- User.StartPositions     (NiagaraDataInterfaceArrayFloat3)
- User.EndPositions       (NiagaraDataInterfaceArrayFloat3)
- User.Directions         (NiagaraDataInterfaceArrayFloat3)
- User.DynamicRT          (TextureRenderTarget)

Report which were added vs. already existed.

---

## Scratch module via NiagaraScratchPadService

Use `unreal.NiagaraScratchPadService.create_scratch_module` to add an empty scratch module
called `SplatLine` to the `CompletelyEmpty` emitter on the `ParticleUpdate` stage. Then call
`get_scratch_script_path` and print where the scratch UNiagaraScript was stored - it should be
a sub-object path under the emitter, not `/Game/...`.

---

List every node currently in the `SplatLine` scratch graph (`list_nodes`). The default template
should include at least an Input (Map In), a Map Get, a Map Set, and an Output (Map Out). Report
the node IDs and types.

---

## Declare module inputs (Map Get reads)

Add the following stack inputs to `SplatLine` via `add_module_input`. Each one becomes a new
`Module.<name>` output pin on the existing Map Get node and shows up as a stack input on the
emitter:

- StartPositions (ArrayVector)
- EndPositions   (ArrayVector)
- Directions     (ArrayVector)
- VolumeMin      (Vector)
- VolumeMax      (Vector)
- GridWidth      (int)
- GridHeight     (int)
- TireRadius     (float)
- DecayRate      (float)
- TracksGrid     (Grid2D)

After all calls, run `get_node_pins` on the Map Get node and confirm every `Module.X` output pin
is present.

---

## Custom HLSL node + typed pins

Add a Custom HLSL node at position (350, 0) with a minimal splat body that:

- reads `ExecutionIndex` as the particle index,
- samples `StartPositions.Get(Idx)`, `EndPositions.Get(Idx)`, `Directions.Get(Idx)`,
- projects start/end into UV space using `VolumeMin`/`VolumeMax`,
- iterates the integer AABB of the segment expanded by `TireRadius` (in UV) and writes to
  `TracksGrid.SetVector4Value(0, X, Y, Float4(...))` when distance < radius,
- sets a `Splatted` bool output to true when at least one cell was written.

Then, on that Custom HLSL node, declare typed input pins for every module input (same name,
same type) and an `Output` `bool` pin named `Splatted`.

---

## Wire it

Connect, in order:

1. Every `Module.X` output of Map Get -> the matching named input on the Custom HLSL node.
2. `add_module_output` for `Particles.Splatted` (bool) on the Map Set node.
3. Connect the Custom HLSL `Splatted` output -> the Map Set's `Particles.Splatted` input.

Then call `list_connections` and confirm every wire is present.

---

## Apply + compile

Call `apply_changes` on the system, then `compile_with_results`. Report:
- success / error count / warning count
- any error or warning text verbatim

If compilation fails, surface the error to me; do NOT silently retry.

---

## Robustness check

These three calls should now all succeed cleanly (they previously failed):

The three VibeUE tools these checks were written against (`get_module_info`, `set_module_input`,
`remove_module`/`add_module`) were removed on the 5-9 branch -- Epic's
`NiagaraToolsets.NiagaraToolset_System` owns the stack now. Run the equivalents and report what the
engine does:

1. `GetModuleSchema` on the `SplatLine` module of `CompletelyEmpty` should resolve the scratch
   script rather than returning it blank.

2. `SetStackInputData` should set `GridWidth` to 512 on that emitter's SetVariables module. Note
   whether it needs the bare name or the fully-qualified `Module.GridWidth` -- VibeUE used to try
   both, and this is the behaviour worth recording.

3. `RemoveModule` followed by `AddModule` of the same module should not trip the
   `StackNodeGroups.Num() >= 2` assert or crash the editor. On a broken stack, record whether Epic
   refuses cleanly or asserts -- `GetStackIssues` / `ApplyStackIssueFix` are the engine's own
   repair path and are worth trying first.

Report pass/fail for each.

---

## Final state

Give me an AI-friendly summary of `NS_TrackPainter` (emitters, modules per stage, user params,
scratch modules). The Particle Update stack of `CompletelyEmpty` should contain the `SplatLine`
scratch module.
