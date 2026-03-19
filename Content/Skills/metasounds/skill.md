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
# Look for the AudioOut input node (node_title contains "Audio")
audio_out_id = next(n.node_id for n in all_nodes if "Audio" in n.node_title and n.inputs)
```

### 4 — Add a node

```python
# add_node(asset_path, namespace, name, variant="", major_version=1, pos_x=0, pos_y=0)
r2 = ms.add_node(asset_path, "Metasound.Standard", "Sine", "Audio", 1, -200.0, 0.0)
sine_id = r2.node_id   # GUID string
```

### 5 — Set a node input default

```python
# Set the Sine frequency to 880 Hz
ms.set_node_input_default(asset_path, sine_id, "In Frequency", "880.0", "Float")
```

### 6 — Connect nodes

```python
# connect_nodes(asset_path, from_node_id, output_name, to_node_id, input_name)
ms.connect_nodes(asset_path, sine_id, "Out", audio_out_id, "Audio:0")
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

# Find the built-in AudioOut input node (where we connect audio signal)
nodes = ms.list_nodes(ap)
audio_out_node = next(n for n in nodes if "Audio" in n.node_title and n.inputs)
audio_out_id = audio_out_node.node_id
audio_in_pin = audio_out_node.inputs[0].split(":")[0]   # e.g. "Audio:0"

# Add Sine oscillator
r2 = ms.add_node(ap, "Metasound.Standard", "Sine", "Audio", 1, -300.0, 0.0)
sine_id = r2.node_id

# Set frequency
ms.set_node_input_default(ap, sine_id, "In Frequency", "880.0", "Float")

# Connect Sine output to AudioOut
ms.connect_nodes(ap, sine_id, "Out", audio_out_id, audio_in_pin)

# Save
ms.save_meta_sound(ap)
print("Done:", ap)
```

---

## Notes

- **Save after every batch of edits**, not after each individual operation, to avoid excessive disk I/O.
- `list_available_nodes("")` returns **all** registered node classes (~400+). Use a filter like `"Sine"`, `"Delay"`, `"Gain"` to narrow the results.
- Node pin names for standard nodes use UE display names (e.g. `"In Frequency"`, `"Out"`, `"Audio:0"`). Use `get_node_pins()` to confirm exact names.
- The built-in `AudioOut` input node receives the final audio signal. Its input pin is typically named `"Audio:0"` for Mono, `"Audio:0"` / `"Audio:1"` for Stereo.
- MetaSound Sources do **not** support SoundCue-style `SoundNodeWavePlayer` — use the `WavePlayer` MetaSound node instead (`Metasound.Standard`, `"Wave Player"`, `"Mono"` or `"Stereo"`).
