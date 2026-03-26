---
name: sound-cues
display_name: Sound Cue Editor
description: Create and modify SoundCue assets — add nodes, connect them, set audio properties
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

> **Wrong skill for MetaSound assets?** If the user asked about `MetaSound`, `MS_` assets, or `UMetaSoundSource`, unload this skill and load the `metasounds` skill instead:
> `manage_skills(action="load", skill_name="metasounds")`
> SoundCue and MetaSound are completely separate systems — do not use `SoundCueService` for MetaSound tasks.

# Sound Cue Editor Skill

## Service Access

```python
import unreal
svc = unreal.SoundCueService()
```

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
| `bLooping`      | `is_looping`    |
| `bStreaming`    | `is_streaming`  |

### ⚠️ Node indices are positional and may shift

`list_nodes()` returns nodes in graph order — indices can change if new nodes are added.
Always call `list_nodes()` to get current indices before connecting or modifying nodes.

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

# Save
svc.save_sound_cue('/Game/Audio/SC_Footstep')
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
svc.save_sound_cue(cue)
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
svc.save_sound_cue(cue)
```

### Volume / Pitch / SoundClass

```python
svc.set_volume_multiplier('/Game/Audio/SC_Explosion', 1.5)
svc.set_pitch_multiplier('/Game/Audio/SC_Explosion', 0.9)
svc.set_sound_class('/Game/Audio/SC_Explosion', '/Game/Audio/SC_SFX')
```

---

## API Reference

### Asset Lifecycle

| Method | Returns | Notes |
|--------|---------|-------|
| `create_sound_cue(path, wave_path)` | `FSoundCueResult` | `wave_path=''` for empty cue |
| `duplicate_sound_cue(src_path, dst_path)` | `FSoundCueResult` | copy a cue to a new path |
| `delete_sound_cue(path)` | `bool` | deletes the asset from disk |
| `get_sound_cue_info(path)` | `FSoundCueInfo` | node count, root index, vol, pitch, duration |
| `save_sound_cue(path)` | `bool` | saves to disk |

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
| `add_quality_level_node(path, x, y)` | One input per quality level | — |

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

### Cue-Level Settings

| Method | Returns | Notes |
|--------|---------|-------|
| `set_volume_multiplier(path, vol)` | `bool` | — |
| `set_pitch_multiplier(path, pitch)` | `bool` | — |
| `set_sound_class(path, class_path)` | `bool` | — |
| `set_attenuation(path, att_path)` | `bool` | `att_path=''` to clear |
| `get_attenuation(path)` | `FString` | returns asset path or `''` |
| `set_concurrency(path, conc_path, clear=False)` | `bool` | `conc_path=''` + `clear=True` to clear all |
| `get_concurrency(path)` | `TArray<FString>` | list of assigned concurrency asset paths |

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
| `import_sound_wave(file_path, asset_path)` | `FSoundCueResult` | import .wav/.mp3 from disk |
| `set_sound_wave_property(sw_path, prop, value)` | `bool` | set any UPROPERTY by name |

Common `set_sound_wave_property` property names:

| PropertyName | Type | Example value |
|---|---|---|
| `bLooping` | bool | `"true"` |
| `bStreaming` | bool | `"true"` |
| `VolumeMultiplier` | float | `"1.5"` |
| `PitchMultiplier` | float | `"0.9"` |
| `SubtitlePriority` | float | `"100.0"` |

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
