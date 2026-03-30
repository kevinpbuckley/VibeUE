---
name: metasounds
display_name: MetaSound Editor
description: Create and modify MetaSound Source assets — add nodes, wire pins, set defaults, play sounds procedurally
vibeue_classes:
  - MetaSoundService
unreal_classes:
  - MetaSoundSource
  - MetaSoundBuilder
keywords:
  - metasound
  - MetaSound
  - MS_
  - audio
  - sound
  - wave
  - SoundWave
  - procedural
  - trigger
  - sine
  - oscillator
  - WavePlayer
---

# MetaSound Service Skill

Use this skill to create and edit MetaSound Source assets via Python.

```python
import unreal
ms = unreal.MetaSoundService()
```

## Key Concepts

- **MetaSound Source** — a procedural audio asset (`UMetaSoundSource`) defined by a node graph. It replaces SoundCue for runtime-parameterisable sounds.
- **Node** — a DSP processing block (Sine oscillator, Gain, Delay, etc.). Nodes have named **input pins** and **output pins** with associated **DataTypes**.
- **NodeId** — a GUID string returned by `add_node`. Pass it to connect, remove, set_default, etc.
- **Graph I/O** — `add_graph_input` / `add_graph_output` expose values at the asset level (settable at runtime via `Set Float Parameter`, etc.).
- **Standard interface** — every Source created by this service comes pre-wired with:
  - `On Play` (Trigger output) — fires when the sound starts
  - `On Finished` (Trigger input) — call to stop the sound
  - `Audio:0` (Audio input on the graph output node) — connect your audio signal here

---

## Workflow

### 1 — Discover available nodes

```python
# List all nodes whose class name or display name contains "Sine"
nodes = ms.list_available_nodes("Sine")
for n in nodes:
    print(n.full_class_name, "  inputs:", n.inputs, "  outputs:", n.outputs)
```

### 2 — Create a MetaSound

```python
r = ms.create_meta_sound("/Game/Audio", "MS_SineLoop", "Mono")
asset_path = r.asset_path   # "/Game/Audio/MS_SineLoop"
```

### 3 — Find the built-in interface node IDs

```python
all_nodes = ms.list_nodes(asset_path)
for n in all_nodes:
    print(n.node_id, n.node_title, n.inputs, n.outputs)

# IMPORTANT: A MetaSound Source has MULTIPLE nodes with title "Output" —
# one per interface pin group (e.g. "On Finished" Trigger, "Out Mono" Audio).
# You MUST filter by the node whose inputs contain an Audio-type pin.
# Pin strings are "VertexName:TypeName" — the TypeName suffix after the last colon.
audio_out_node = next(
    n for n in all_nodes
    if n.node_title == "Output" and any(p.endswith(":Audio") for p in n.inputs)
)
audio_out_id = audio_out_node.node_id
# Drop the last :TypeName suffix to get the raw vertex name for connect_nodes
audio_in_pin = ":".join(audio_out_node.inputs[0].split(":")[:-1])  # "UE.OutputFormat.Mono.Audio:0"

# Input node — filter by class_name "Input.Trigger" to avoid matching graph input nodes
# (graph inputs also appear as "Input" nodes but with class "Input.Float", "Input.Bool", etc.)
input_node = next(n for n in all_nodes if n.class_name == "Input.Trigger")
input_node_id = input_node.node_id
on_play_pin = ":".join(input_node.outputs[0].split(":")[:-1])  # "UE.Source.OnPlay"
```

### 4 — Add a node

```python
# add_node(asset_path, namespace, name, variant="", major_version=1, pos_x=0, pos_y=0)
# Use list_available_nodes() to discover the correct namespace/name/variant for any node
r2 = ms.add_node(asset_path, "UE", "Sine", "Audio", 1, -200.0, 0.0)
sine_id = r2.node_id   # GUID string
```

### 5 — Set a node input default

```python
# Set the Sine frequency to 880 Hz
# Use get_node_pins() to confirm exact input pin names before setting
ms.set_node_input_default(asset_path, sine_id, "Frequency", "880.0", "Float")
```

### 6 — Connect nodes

```python
# connect_nodes(asset_path, from_node_id, output_name, to_node_id, input_name)
# Sine output pin is "Audio"; AudioOut input pin is "UE.OutputFormat.Mono.Audio:0"
ms.connect_nodes(asset_path, sine_id, "Audio", audio_out_id, audio_in_pin)
```

### 7 — Save

```python
ms.save_meta_sound(asset_path)
```

---

## Method Reference

### Lifecycle

| Method | Description |
|--------|-------------|
| `create_meta_sound(package_path, asset_name, output_format="Mono")` | Create a new MetaSound Source asset. Returns `FMetaSoundResult` with `asset_path`. |
| `delete_meta_sound(asset_path)` | Delete a MetaSound asset. |
| `get_meta_sound_info(asset_path)` | Return `FMetaSoundInfo` (node count, output format, graph I/O names). |
| `save_meta_sound(asset_path)` | Save after edits. **Always call after making graph changes.** |

### Node Discovery

| Method | Description |
|--------|-------------|
| `list_available_nodes(search_filter="")` | List all registered External/DSP node classes. Returns `TArray<FMetaSoundNodeClassInfo>`. Each entry has `full_class_name`, `namespace`, `name`, `variant`, `inputs`, `outputs`, `display_name`. |

### Node Management

| Method | Returns | Description |
|--------|---------|-------------|
| `add_node(asset_path, namespace, name, variant="", major_version=1, pos_x=0, pos_y=0)` | `FMetaSoundResult` | Add a node. `node_id` on result is the GUID string. |
| `remove_node(asset_path, node_id)` | `FMetaSoundResult` | Remove node and all its edges. |
| `list_nodes(asset_path)` | `TArray<FMetaSoundNodeInfo>` | List all nodes in the graph. |
| `get_node_pins(asset_path, node_id)` | `FMetaSoundNodeInfo` | Return pin info for a single node. |

### Connections

| Method | Returns | Description |
|--------|---------|-------------|
| `connect_nodes(asset_path, from_node_id, output_name, to_node_id, input_name)` | `FMetaSoundResult` | Connect an output pin to an input pin. Check `r.success`. |
| `disconnect_pin(asset_path, node_id, input_name)` | `FMetaSoundResult` | Remove the connection going into an input pin. |

### Graph I/O

| Method | Returns | Description |
|--------|---------|-------------|
| `add_graph_input(asset_path, input_name, data_type, default_value="")` | `FMetaSoundResult` | Add a named input exposed as a runtime parameter. Appears as an `Input.<Type>` node in the graph. |
| `add_graph_output(asset_path, output_name, data_type)` | `FMetaSoundResult` | Add a named output. |
| `remove_graph_input(asset_path, input_name)` | `FMetaSoundResult` | Remove a graph input. |
| `remove_graph_output(asset_path, output_name)` | `FMetaSoundResult` | Remove a graph output. |

### Node Configuration

| Method | Returns | Description |
|--------|---------|-------------|
| `set_node_input_default(asset_path, node_id, input_name, value, data_type)` | `FMetaSoundResult` | Set a literal default on a node input. `data_type`: "Float", "Int32", "Bool", "String", "WaveAsset". |
| `set_node_location(asset_path, node_id, pos_x, pos_y)` | `FMetaSoundResult` | Update editor position. |

---

## Common DataTypes

| Type name | Description |
|-----------|-------------|
| `Float` | 32-bit float (frequency, gain, time) |
| `Int32` | Integer |
| `Bool` | Boolean |
| `String` | Text string |
| `Audio` | Audio signal (mono channel) |
| `Trigger` | Impulse / event signal |
| `Time` | Duration in seconds (use Float in most cases) |
| `WaveAsset` | Reference to a SoundWave asset |

---

## Complete Example — 880 Hz Sine Tone

```python
import unreal
ms = unreal.MetaSoundService()

# Create asset
r = ms.create_meta_sound("/Game/Audio", "MS_Test880Hz", "Mono")
ap = r.asset_path

# Find the AudioOut node — there are multiple nodes titled "Output" (one per interface
# pin group). Filter for the one whose inputs contain an Audio-type pin.
nodes = ms.list_nodes(ap)
audio_out_node = next(
    n for n in nodes
    if n.node_title == "Output" and any(p.endswith(":Audio") for p in n.inputs)
)
audio_out_id = audio_out_node.node_id
# Pin strings are "VertexName:TypeName" — drop only the last :TypeName to get the vertex name
audio_in_pin = ":".join(audio_out_node.inputs[0].split(":")[:-1])  # "UE.OutputFormat.Mono.Audio:0"

# Add Sine oscillator (use list_available_nodes("Sine") to discover namespace/name/variant)
r2 = ms.add_node(ap, "UE", "Sine", "Audio", 1, -300.0, 0.0)
sine_id = r2.node_id

# Set frequency to 880 Hz (use get_node_pins() to confirm exact pin names)
ms.set_node_input_default(ap, sine_id, "Frequency", "880.0", "Float")

# Connect Sine "Audio" output to the AudioOut sink pin
ms.connect_nodes(ap, sine_id, "Audio", audio_out_id, audio_in_pin)

# Save
ms.save_meta_sound(ap)
print("Done:", ap)
```

---

## Complete Example — One-Shot SoundWave (Gunshot)

```python
import unreal
ms = unreal.MetaSoundService()

# Create a Mono MetaSound Source
r = ms.create_meta_sound("/Game/Audio", "MS_Gunshot", "Mono")
ap = r.asset_path

# Find interface nodes
nodes = ms.list_nodes(ap)

# Input node — has "On Play" Trigger output
input_node = next(n for n in nodes if n.node_title == "Input")
input_node_id = input_node.node_id

# Audio Output node — filter for the one with an Audio-type input
audio_out_node = next(
    n for n in nodes
    if n.node_title == "Output" and any(p.endswith(":Audio") for p in n.inputs)
)
audio_out_id = audio_out_node.node_id
audio_in_pin = ":".join(audio_out_node.inputs[0].split(":")[:-1])

# Add Wave Player (Mono) node
# Pin names (no need to call get_node_pins — see Known Node Pins below):
#   Inputs:  "Play" (Trigger), "Stop" (Trigger), "Wave Asset" (WaveAsset), "Loop" (Bool)
#   Outputs: "Out Mono" (Audio), "On Play" (Trigger), "On Finished" (Trigger)
wp = ms.add_node(ap, "UE", "Wave Player", "Mono", 1, -300.0, 0.0)
wp_id = wp.node_id

# Set the wave asset
ms.set_node_input_default(ap, wp_id, "Wave Asset", "/Game/Audio/SW_Gunshot_01", "WaveAsset")

# Wire On Play (Input) → Play (WavePlayer)
# CRITICAL: use the vertex name "UE.Source.OnPlay" — NOT the display name "On Play"
r = ms.connect_nodes(ap, input_node_id, "UE.Source.OnPlay", wp_id, "Play")
if not r.success: raise RuntimeError(f"connect On Play→Play failed: {r.message}")

# Wire Out Mono (WavePlayer) → audio sink (Output)
r = ms.connect_nodes(ap, wp_id, "Out Mono", audio_out_id, audio_in_pin)
if not r.success: raise RuntimeError(f"connect Out Mono→Output failed: {r.message}")

# Save
ms.save_meta_sound(ap)
print("Done:", ap)
```

---

## Return Type Attribute Reference

Use these exact attribute names — guessing leads to `AttributeError`.

### `FMetaSoundResult` — returned by most mutating methods

| Attribute | Type | Example |
|-----------|------|---------|
| `success` | bool | `True` |
| `message` | str | `"Connected A.Audio -> B.UE.OutputFormat.Mono.Audio:0"` |
| `asset_path` | str | `"/Game/Audio/MS_Test"` |
| `node_id` | str (GUID) | `"A1B2C3D4-..."` (populated by `add_node` only) |

```python
r = ms.connect_nodes(ap, from_id, "Audio", to_id, pin)
if not r.success:
    raise RuntimeError(r.message)
# NOT r.b_success — that attribute does not exist
```

---

### `FMetaSoundInfo` — returned by `get_meta_sound_info()`

| Attribute | Type | Example |
|-----------|------|---------|
| `asset_path` | str | `"/Game/Audio/MS_Test_Blip"` |
| `asset_name` | str | `"MS_Test_Blip"` |
| `output_format` | str | `"Mono"` |
| `node_count` | int | `4` |
| `graph_inputs` | list[str] | `[]` |
| `graph_outputs` | list[str] | `[]` |

```python
info = svc.get_meta_sound_info(asset_path)
print(info.node_count, info.output_format)
# NOT info.success, info.b_success, info.message
```

---

### `FMetaSoundNodeInfo` — returned by `list_nodes()` and `get_node_pins()`

| Attribute | Type | Example |
|-----------|------|---------|
| `node_id` | str (GUID) | `"A1B2C3D4-..."` |
| `node_title` | str | `"Wave Player"` |
| `class_name` | str | `"UE.Wave Player.Mono"` |
| `inputs` | list[str] | `["Play:Trigger", "Wave Asset:WaveAsset"]` |
| `outputs` | list[str] | `["Out Mono:Audio", "On Finished:Trigger"]` |

```python
nodes = ms.list_nodes(asset_path)
for n in nodes:
    print(n.node_id, n.node_title, n.class_name)
    # NOT n.node_class, n.node_class_name, n.name
```

### `FMetaSoundNodeClassInfo` — returned by `list_available_nodes()`

| Attribute | Type | Example |
|-----------|------|---------|
| `full_class_name` | str | `"UE.Wave Player.Mono"` |
| `namespace` | str | `"UE"` |
| `name` | str | `"Wave Player"` |
| `variant` | str | `"Mono"` |
| `display_name` | str | `"Wave Player (Mono)"` |
| `inputs` | list[str] | `["Play:Trigger", ...]` |
| `outputs` | list[str] | `["Out Mono:Audio", ...]` |

```python
nodes = ms.list_available_nodes("Wave Player")
for n in nodes:
    print(n.name, n.variant, n.full_class_name)
    # NOT n.node_name, n.node_class, n.class_name
```

---

## Known Node Pins

Use these instead of calling `get_node_pins` on freshly-added nodes (can time out).

### Wave Player (UE.Wave Player.Mono)

| Direction | Pin | Type |
|-----------|-----|------|
| Input | `Play` | Trigger |
| Input | `Stop` | Trigger |
| Input | `Wave Asset` | WaveAsset |
| Input | `Loop` | Bool |
| Input | `Pitch Shift` | Float |
| Input | `Start Time` | Time |
| Input | `Loop Start` | Time |
| Input | `Loop Duration` | Time |
| Input | `Maintain Audio Sync` | Bool |
| Output | `Out Mono` | Audio |
| Output | `On Play` | Trigger |
| Output | `On Finished` | Trigger |
| Output | `On Nearly Finished` | Trigger |
| Output | `On Looped` | Trigger |
| Output | `On Cue Point` | Trigger |
| Output | `Cue Point ID` | Int32 |
| Output | `Cue Point Label` | String |
| Output | `Loop Percent` | Float |
| Output | `Playback Location` | Float |
| Output | `Playback Time` | Time |

### Sine (UE.Sine.Audio)

| Direction | Pin | Type |
|-----------|-----|------|
| Input | `Frequency` | Float |
| Input | `Modulation` | Audio |
| Input | `Enabled` | Bool |
| Input | `Bi Polar` | Bool |
| Input | `Sync` | Trigger |
| Input | `Phase Offset` | Float |
| Input | `Glide` | Float |
| Input | `Type` | Enum:SineGenerationType |
| Output | `Audio` | Audio |

### Standard Interface Nodes

**CRITICAL:** Interface node pins use namespaced vertex names. The display name shown
in the editor is NOT the vertex name. Always use the vertex names below in `connect_nodes`.

| Node title | Vertex name (EXACT string for connect_nodes) | Type | Direction |
|-----------|---------------------------------------------|------|-----------|
| `Input` | `UE.Source.OnPlay` | Trigger | Output |
| `Output` (Trigger) | `UE.Source.OneShot.OnFinished` | Trigger | Input |
| `Output` (Audio/Mono) | `UE.OutputFormat.Mono.Audio:0` | Audio | Input |

---

## Notes

- **Save after every batch of edits**, not after each individual operation, to avoid excessive disk I/O.
- `list_available_nodes("")` returns **all** registered node classes (~400+). Use a filter like `"Sine"`, `"Delay"`, `"Gain"` to narrow the results.
- Node pin names for standard nodes use UE display names (e.g. `"In Frequency"`, `"Out"`, `"Audio:0"`). Use `get_node_pins()` to confirm exact names.
- A MetaSound Source has **multiple nodes with `node_title == "Output"`** — one per interface pin group (e.g. `"On Finished"` Trigger, `"Out Mono"` Audio). To find the audio sink node, filter for the Output node whose inputs contain an Audio-type pin: `next(n for n in nodes if n.node_title == "Output" and any(p.endswith(":Audio") for p in n.inputs))`. Pin strings from `list_nodes` are `"VertexName:TypeName"` — you can pass them directly to `connect_nodes`, which now automatically strips the trailing `:TypeName` suffix. Manual stripping (`":".join(pin.split(":")[:-1])`) still works but is no longer required.
- Node namespace/name/variant values differ from what the MetaSound editor displays. Always call `list_available_nodes("keyword")` to discover the correct values; do not guess.
- MetaSound Sources do **not** support SoundCue-style `SoundNodeWavePlayer` — use the `WavePlayer` MetaSound node instead (discover via `list_available_nodes("Wave Player")`).
- **Graph inputs appear as `Input` nodes** in the graph (`class_name == "Input.Float"`, `"Input.Bool"`, etc.). When filtering for the standard interface Input node (the one that carries `On Play`), always match by `class_name == "Input.Trigger"` — not by `node_title == "Input"` alone, which will also match graph input nodes.
- **AudioMixer pins interleave audio and gain:** `Audio Mixer (Mono, 2)` has pins `["In 0:Audio", "Gain 0:Float", "In 1:Audio", "Gain 1:Float"]`. Never index by position — always filter for `:Audio` typed pins when connecting audio signals: `audio_ins = [p.split(":")[0] for p in mixer_node.inputs if p.endswith(":Audio")]`.
- **Stereo MetaSound Sources** report `UE.OutputFormat.Mono.Audio:0` as the audio output sink pin (same as Mono). The existing audio-output filter (`any(p.endswith(":Audio") for p in n.inputs)`) works correctly for both formats.
