---
name: sound-cues
display_name: Sound Cue Editor
description: Create and modify SoundCue assets — add/connect nodes (mixer, random, delay, attenuation, modulator, etc.) with SoundCueService; cue/wave settings, import, save, duplicate and delete go through Epic's AssetToolset, SoundWaveToolset and ObjectToolset. Use when the user asks to create a Sound Cue, build a SoundCue node graph, or wire audio playback logic.
vibeue_classes:
  - SoundCueService
unreal_classes:
  - SoundCue
  - SoundNodeWavePlayer
  - SoundNodeRandom
  - SoundNodeMixer
  - SoundNodeConcatenator
  - SoundNodeModulator
  - SoundNodeAttenuation
  - SoundNodeLooping
  - SoundNodeDelay
  - SoundNodeSwitch
  - SoundNodeEnveloper
  - SoundNodeDistanceCrossFade
  - SoundNodeBranch
  - SoundNodeParamCrossFade
  - SoundNodeQualityLevel
keywords:
  - sound cue
  - sound node
  - wave player
  - random node
  - mixer
  - audio graph
  - sound design
  - SoundCue
related_skills:
  - asset-management
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "audio-and-metasounds"}` for UE domain knowledge on this topic — correct APIs, architecture, best practices — and treat it as the rubric for any review / "best practices" question. If no such tool is available (e.g. running under Claude Code or Codex without that MCP), skip this line entirely and proceed with this skill alone — do NOT attempt the call.

> **Wrong skill for MetaSound assets?** If the user asked about `MetaSound`, `MS_` assets, or `UMetaSoundSource`, use the engine's **`MetaSoundToolset`** via `call_tool` (Authoring / Inspection / Discovery, 42 tools) — VibeUE no longer ships a MetaSound service.
> SoundCue and MetaSound are completely separate systems — do not use `SoundCueService` for MetaSound tasks.

# Sound Cue Editor Skill

## Service Access

```python
import unreal
svc = unreal.SoundCueService()
```

`SoundCueService` owns the cue's **node graph** only. Everything else is Epic's, called from the
same `execute_python` script with the injected `await call_tool("Toolset.Full.Name.Tool", ...)`:

| Task | Epic tool |
|------|-----------|
| Save / duplicate / delete a cue or wave | `EditorToolset.AssetToolset.SaveAsset(assetPath)` / `Duplicate(path, newPath)` / `Delete(path)` |
| Import a .wav/.mp3/.ogg/.flac/... as a SoundWave | `EditorToolset.SoundWaveToolset.ImportFile(folderPath, assetName, sourceFile, bReplaceExisting=False, bSave=False)` |
| Cue volume, pitch, sound class, attenuation, concurrency | `EditorToolset.ObjectToolset.SetProperties` / `GetProperties` on the `USoundCue` |
| SoundWave looping, volume, pitch, subtitle priority | `EditorToolset.ObjectToolset.SetProperties` / `GetProperties` on the `USoundWave` |

`SoundCueService` edits only dirty the package — save with `AssetToolset.SaveAsset` when done.

---

## Audio Flow Direction

**Audio flows FROM leaves TOWARD root.**

```
[WavePlayer] ──> [Random] ──> [Mixer] ──> [ROOT]
                                ^
                    [WavePlayer2] ──────┘
```

- `connect_nodes(parent, child, slot)` means: child **provides audio INTO** parent at that input slot.
- `set_root_node(index)` makes that node the final output (what you hear).

---

## Critical Rules

### ⚠️ Bool Properties — `b` prefix stripped in Python

UE strips the `b` prefix from bool UPROPERTY names in Python:

| C++ / UPROPERTY | Python attribute |
|-----------------|-----------------|
| `bSuccess`      | `success`       |
| `bIsRootNode`   | `is_root_node`  |
| `bLooping`      | `looping`       |
| `bStreaming`    | `streaming`     |

(Epic's `ObjectToolset` takes the **C++** names — `bLooping`, not `looping`.)

### ⚠️ Node indices are positional and may shift

`list_nodes()` returns nodes in graph order — indices can change if new nodes are added.
Always call `list_nodes()` to get current indices before connecting or modifying nodes.

### ⚠️ Quality Level node has a fixed slot count

`add_quality_level_node` creates a node whose input-slot count is governed by the project's
**audio quality levels** (`AudioQualitySettings`), not by an argument — there is no `num_inputs`
param. In a default/skeleton project this is often a **single slot**, so `connect_nodes(..., slot=1)`
returns `False`. If a cue must select between sounds per scalability bucket, confirm the project
defines multiple quality levels first; otherwise only slot 0 is wireable.

### ⚠️ New-cue defaults: `volume_multiplier` is `0.75`, not `1.0`

A freshly created empty cue reports `volume_multiplier = 0.75` (the engine `USoundCue` default),
`pitch_multiplier = 1.0`. Do **not** assert `1.0` for volume on a new cue — read it back instead.

### ⚠️ `connect_nodes` parameter order

```python
# parent receives audio FROM child at slot
svc.connect_nodes(cue_path, parent_index, child_index, input_slot)

# CORRECT: WavePlayer(0) feeds into Random(1) at slot 0
svc.connect_nodes('/Game/MyCue', 1, 0, 0)

# WRONG: don't confuse child/parent order
```

---

## Workflows

### Create a Simple SoundCue

```python
import unreal
svc = unreal.SoundCueService()

# Create the asset (optionally wire an initial WavePlayer)
r = svc.create_sound_cue('/Game/Audio/SC_Footstep', '')
assert r.success, r.message

# Add a WavePlayer node
r = svc.add_wave_player_node('/Game/Audio/SC_Footstep', '/Game/Audio/Footstep_01', -250, 0)
assert r.success

# Set it as the root (output)
nodes = svc.list_nodes('/Game/Audio/SC_Footstep')
svc.set_root_node('/Game/Audio/SC_Footstep', nodes[0].node_index)

# Save (Epic)
await call_tool("EditorToolset.AssetToolset.SaveAsset", assetPath='/Game/Audio/SC_Footstep')
```

### Random from Multiple Waves

```python
import unreal
svc = unreal.SoundCueService()
cue = '/Game/Audio/SC_Footsteps'

svc.create_sound_cue(cue, '')

# Add 3 wave players
for i, wave in enumerate(['/Game/Audio/Step_01', '/Game/Audio/Step_02', '/Game/Audio/Step_03']):
    svc.add_wave_player_node(cue, wave, -400, i * 150)

# Add a Random node to select one
svc.add_random_node(cue, -150, 150)

nodes = svc.list_nodes(cue)
# nodes[0..2] = WavePlayers, nodes[3] = Random
random_idx = next(i for i, n in enumerate(nodes) if 'Random' in n.node_class)
for i in range(3):
    svc.connect_nodes(cue, random_idx, i, i)  # each wave into random

svc.set_root_node(cue, random_idx)
await call_tool("EditorToolset.AssetToolset.SaveAsset", assetPath=cue)
```

### Mixer (parallel blend)

```python
import unreal
svc = unreal.SoundCueService()
cue = '/Game/Audio/SC_Ambient'

svc.create_sound_cue(cue, '')
svc.add_wave_player_node(cue, '/Game/Audio/Wind', -400, 0)
svc.add_wave_player_node(cue, '/Game/Audio/Rain', -400, 150)
svc.add_mixer_node(cue, 2, -150, 75)   # 2 inputs

nodes = svc.list_nodes(cue)
wave0 = next(i for i, n in enumerate(nodes) if 'WavePlayer' in n.node_class and i == 0)
wave1 = next(i for i, n in enumerate(nodes) if 'WavePlayer' in n.node_class and i != wave0)
mixer = next(i for i, n in enumerate(nodes) if 'Mixer' in n.node_class)

svc.connect_nodes(cue, mixer, wave0, 0)
svc.connect_nodes(cue, mixer, wave1, 1)
svc.set_root_node(cue, mixer)
await call_tool("EditorToolset.AssetToolset.SaveAsset", assetPath=cue)
```

### Volume / Pitch / SoundClass / Attenuation / Concurrency (Epic `ObjectToolset`)

```python
import json
OT = "EditorToolset.ObjectToolset"
cue = {"refPath": "/Game/Audio/SC_Explosion.SC_Explosion"}   # full object path

# `values` is a JSON STRING. Object references take an object path (or {"refPath": path}); null clears.
await call_tool(f"{OT}.SetProperties", instance=cue, values=json.dumps({
    "VolumeMultiplier": 1.5,
    "PitchMultiplier": 0.9,
    "SoundClassObject": "/Game/Audio/SFX.SFX",
    "AttenuationSettings": "/Game/Audio/ATT_Explosion.ATT_Explosion",
}))

# ConcurrencySet is a set: read it, add, write the whole set back (there is no "append one").
# It has EditCondition !bOverrideConcurrency (SoundBase.h), so ObjectToolset REFUSES this write while
# bOverrideConcurrency is true: read that flag first (GetProperties properties=["bOverrideConcurrency"]).
cur = await call_tool(f"{OT}.GetProperties", instance=cue, properties=["ConcurrencySet"])
print(cur)   # current members (JSON); keep them in the list you write back, then:
await call_tool(f"{OT}.SetProperties", instance=cue, values=json.dumps({
    "ConcurrencySet": ["/Game/Audio/CON_Explosions.CON_Explosions"],   # [] clears
}))
await call_tool("EditorToolset.AssetToolset.SaveAsset", assetPath="/Game/Audio/SC_Explosion")
```

> `AttenuationSettings` is refused while the cue's `bOverrideAttenuation` is on (the asset is not
> used then — the cue's inline `AttenuationOverrides` are). Set `bOverrideAttenuation` false first,
> or edit `AttenuationOverrides` instead.

> `ConcurrencySet` is likewise refused while `bOverrideConcurrency` is true (the cue's inline `ConcurrencyOverrides` apply then); set `bOverrideConcurrency` false in a separate SetProperties call first, or edit `ConcurrencyOverrides`.

### Import a SoundWave and set its properties

```python
import json
# Async import; awaiting returns the produced asset(s).
r = await call_tool("EditorToolset.SoundWaveToolset.ImportFile",
                    folderPath="/Game/Audio", assetName="SW_Gunshot",
                    sourceFile=r"C:/Sounds/gunshot.wav", bReplaceExisting=True, bSave=True)
print(r)

await call_tool("EditorToolset.ObjectToolset.SetProperties",
                instance={"refPath": "/Game/Audio/SW_Gunshot.SW_Gunshot"},
                values=json.dumps({"bLooping": False, "Volume": 0.8, "Pitch": 1.0}))
await call_tool("EditorToolset.AssetToolset.SaveAsset", assetPath="/Game/Audio/SW_Gunshot")
```

---

## API Reference

### Asset Lifecycle

| Method | Returns | Notes |
|--------|---------|-------|
| `create_sound_cue(path, wave_path)` | `FSoundCueResult` | `wave_path=''` for empty cue |
| `get_sound_cue_info(path)` | `FSoundCueInfo` | node count, root index, vol, pitch, class, attenuation, duration |

Save / duplicate / delete are Epic's: `EditorToolset.AssetToolset.SaveAsset(assetPath)`,
`Duplicate(path, newPath)`, `Delete(path)`.

### Node Creation — all return `FSoundCueResult`

| Method | Node Type | Special params |
|--------|-----------|---------------|
| `add_wave_player_node(path, wave_path, x, y)` | Leaf — plays a SoundWave | `wave_path` optional |
| `add_random_node(path, x, y)` | Picks one child randomly | — |
| `add_mixer_node(path, num_inputs, x, y)` | Blends children in parallel | `num_inputs` 1–32 |
| `add_concatenator_node(path, num_inputs, x, y)` | Plays children in sequence | `num_inputs` 2–32 |
| `add_modulator_node(path, x, y)` | Random pitch/volume variance | — |
| `add_attenuation_node(path, x, y)` | Spatial attenuation (graph node) | — |
| `add_looping_node(path, x, y)` | Loops its child | — |
| `add_delay_node(path, x, y)` | Adds a delay before playing | — |
| `add_switch_node(path, x, y)` | Routes by integer parameter | — |
| `add_enveloper_node(path, x, y)` | Volume/pitch envelope over time | — |
| `add_distance_cross_fade_node(path, num_inputs, x, y)` | Crossfades by distance | `num_inputs` 2–32 |
| `add_branch_node(path, x, y, bool_param='')` | Routes by bool parameter; True(0) False(1) Unset(2) | optional param name last |
| `add_param_cross_fade_node(path, num_inputs, x, y)` | Crossfades by named float param | `num_inputs` 2–32 |
| `add_quality_level_node(path, x, y)` | One input per quality level | slot count = project quality levels (see note) |

### Node Management

| Method | Returns | Notes |
|--------|---------|-------|
| `list_nodes(path)` | `TArray<FSoundCueNodeInfo>` | index, class, children, is_root_node |
| `connect_nodes(path, parent, child, slot)` | `bool` | child feeds audio INTO parent |
| `disconnect_node(path, parent_index, slot)` | `bool` | break a single input link |
| `set_root_node(path, index)` | `bool` | sets cue output node |
| `set_wave_player_asset(path, index, wave_path)` | `bool` | reassign wave on existing node |
| `remove_node(path, index)` | `bool` | delete a node from the graph |
| `move_node(path, index, x, y)` | `bool` | reposition a node in the graph |
| `get_node_property(path, index, prop_name)` | `FString` | read any node UPROPERTY |
| `set_node_property(path, index, prop_name, value)` | `bool` | write any node UPROPERTY |

### Cue-Level Settings — Epic `EditorToolset.ObjectToolset` on the `USoundCue`

| UPROPERTY (C++ name) | Type | Notes |
|---|---|---|
| `VolumeMultiplier` | float | new cues default to `0.75` |
| `PitchMultiplier` | float | |
| `SoundClassObject` | SoundClass ref | object path; `null` clears |
| `AttenuationSettings` | SoundAttenuation ref | object path; `null` clears; refused while `bOverrideAttenuation` is true |
| `ConcurrencySet` | set of SoundConcurrency refs | write the whole set (read-modify-write to add one); `[]` clears; refused while `bOverrideConcurrency` is true (inline `ConcurrencyOverrides` apply then): set it false in a separate SetProperties call first, or edit `ConcurrencyOverrides` |

Read with `GetProperties(instance, properties=[...])`, write with `SetProperties(instance, values=<JSON string>)`.
`get_sound_cue_info` also reports volume, pitch, class name and attenuation path.

### FSoundCueNodeInfo fields

```python
n.node_index      # int — position in list_nodes array
n.node_class      # str — e.g. "SoundNodeWavePlayer"
n.node_title      # str — display name
n.pos_x, n.pos_y  # float — graph position
n.is_root_node    # bool — True if this node is SoundCue.FirstNode
n.child_indices   # list[int] — indices of connected children (-1 = unconnected slot)
```

### SoundWave Utilities

| Method | Returns | Notes |
|--------|---------|-------|
| `get_sound_wave_info(sw_path)` | `FSoundWaveInfo` | duration, sample rate, channels, looping, streaming |

Import is Epic's `EditorToolset.SoundWaveToolset.ImportFile(folderPath, assetName, sourceFile,
bReplaceExisting=False, bSave=False)` (wav, aif, aiff, ogg, flac, opus, mp3; async — await it).
Wave properties are set with `EditorToolset.ObjectToolset.SetProperties` on the `USoundWave`:

| UPROPERTY (C++ name) | Type | Example value |
|---|---|---|
| `bLooping` | bool | `true` |
| `Volume` | float | `1.5` (ClampMin 0) |
| `Pitch` | float | `0.9` (0.125 – 4.0) |
| `SubtitlePriority` | float | `100.0` |

USoundWave has **no** `VolumeMultiplier` / `PitchMultiplier` (those are `USoundCue`'s). `bStreaming`
is a legacy bare `UPROPERTY()` — not editable, so `SetProperties` cannot set it; streaming is
controlled by the wave's `LoadingBehavior`.

### FSoundWaveInfo fields

```python
info.asset_path    # str
info.duration      # float — seconds
info.sample_rate   # int — Hz
info.num_channels  # int — 1=mono, 2=stereo
info.looping       # bool  (note: NOT is_looping — UE strips the 'b' only)
info.streaming     # bool  (note: NOT is_streaming)
```

### FSoundCueResult fields

```python
r.success      # bool
r.asset_path   # str
r.message      # str — human-readable status or error
```

## Sample scripts (run via `execute_python`)

- **`scripts/create_sound_cue.txt`** — create a SoundCue (optionally from a wave) and add nodes.

## Reference pages

Open these with the paths below; UE rewrites them to absolute paths when it serves this skill.

- [scripts/create_sound_cue.txt](scripts/create_sound_cue.txt) - runnable sample
