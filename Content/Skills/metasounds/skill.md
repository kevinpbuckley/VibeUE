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

# Input node — derive the On Play vertex name the same way (display name differs from vertex name)
input_node = next(n for n in all_nodes if n.node_title == "Input")
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

| Method | Description |
|--------|-------------|
| `add_node(asset_path, namespace, name, variant="", major_version=1, pos_x=0, pos_y=0)` | Add a node. Returns `FMetaSoundResult` with `node_id` = GUID string. |
| `remove_node(asset_path, node_id)` | Remove node and all its edges. |
| `list_nodes(asset_path)` | List all nodes in the graph. Returns `TArray<FMetaSoundNodeInfo>`. |
| `get_node_pins(asset_path, node_id)` | Return `FMetaSoundNodeInfo` for a single node. |

### Connections

| Method | Description |
|--------|-------------|
| `connect_nodes(asset_path, from_node_id, output_name, to_node_id, input_name)` | Connect an output pin to an input pin. |
| `disconnect_pin(asset_path, node_id, input_name)` | Remove the connection going into an input pin. |

### Graph I/O

| Method | Description |
|--------|-------------|
| `add_graph_input(asset_path, input_name, data_type, default_value="")` | Add a named input exposed as a runtime parameter. |
| `add_graph_output(asset_path, output_name, data_type)` | Add a named output. |
| `remove_graph_input(asset_path, input_name)` | Remove a graph input. |
| `remove_graph_output(asset_path, output_name)` | Remove a graph output. |

### Node Configuration

| Method | Description |
|--------|-------------|
| `set_node_input_default(asset_path, node_id, input_name, value, data_type)` | Set a literal default on a node input. `data_type`: "Float", "Int32", "Bool", "String". |
| `set_node_location(asset_path, node_id, pos_x, pos_y)` | Update editor position. |

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
if not r.b_success: raise RuntimeError(f"connect On Play→Play failed: {r.message}")

# Wire Out Mono (WavePlayer) → audio sink (Output)
r = ms.connect_nodes(ap, wp_id, "Out Mono", audio_out_id, audio_in_pin)
if not r.b_success: raise RuntimeError(f"connect Out Mono→Output failed: {r.message}")

# Save
ms.save_meta_sound(ap)
print("Done:", ap)
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

### Sine (UE.Sine.Audio)

| Direction | Pin | Type |
|-----------|-----|------|
| Input | `Frequency` | Float |
| Input | `Modulation` | Float |
| Input | `Enabled` | Bool |
| Input | `Bi Polar` | Bool |
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
- A MetaSound Source has **multiple nodes with `node_title == "Output"`** — one per interface pin group (e.g. `"On Finished"` Trigger, `"Out Mono"` Audio). To find the audio sink node, filter for the Output node whose inputs contain an Audio-type pin: `next(n for n in nodes if n.node_title == "Output" and any(p.endswith(":Audio") for p in n.inputs))`. Pin strings are `"VertexName:TypeName"` — use `":".join(pin.split(":")[:-1])` to extract just the vertex name for `connect_nodes`.
- Node namespace/name/variant values differ from what the MetaSound editor displays. Always call `list_available_nodes("keyword")` to discover the correct values; do not guess.
- MetaSound Sources do **not** support SoundCue-style `SoundNodeWavePlayer` — use the `WavePlayer` MetaSound node instead (discover via `list_available_nodes("Wave Player")`).
