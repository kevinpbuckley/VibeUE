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
| `get_sound_cue_info(path)` | `FSoundCueInfo` | node count, root index, vol, pitch |
| `save_sound_cue(path)` | `bool` | saves to disk |

### Node Creation — all return `FSoundCueResult`

| Method | Node Type | Special params |
|--------|-----------|---------------|
| `add_wave_player_node(path, wave_path, x, y)` | Leaf — plays a SoundWave | `wave_path` optional |
| `add_random_node(path, x, y)` | Picks one child randomly | — |
| `add_mixer_node(path, num_inputs, x, y)` | Blends children in parallel | `num_inputs` 1–32 |
| `add_concatenator_node(path, num_inputs, x, y)` | Plays children in sequence | `num_inputs` 2–32 |
| `add_modulator_node(path, x, y)` | Random pitch/volume variance | — |
| `add_attenuation_node(path, x, y)` | Spatial attenuation | — |
| `add_looping_node(path, x, y)` | Loops its child | — |
| `add_delay_node(path, x, y)` | Adds a delay before playing | — |

### Node Connections

| Method | Returns | Notes |
|--------|---------|-------|
| `list_nodes(path)` | `TArray<FSoundCueNodeInfo>` | index, class, children, is_root_node |
| `connect_nodes(path, parent, child, slot)` | `bool` | child feeds audio INTO parent |
| `set_root_node(path, index)` | `bool` | sets cue output node |
| `set_wave_player_asset(path, index, wave_path)` | `bool` | reassign wave on existing node |

### FSoundCueNodeInfo fields

```python
n.node_index      # int — position in list_nodes array
n.node_class      # str — e.g. "SoundNodeWavePlayer"
n.node_title      # str — display name
n.pos_x, n.pos_y  # float — graph position
n.is_root_node    # bool — True if this node is SoundCue.FirstNode
n.child_indices   # list[int] — indices of connected children (-1 = unconnected slot)
```

### FSoundCueResult fields

```python
r.success      # bool
r.asset_path   # str
r.message      # str — human-readable status or error
```
