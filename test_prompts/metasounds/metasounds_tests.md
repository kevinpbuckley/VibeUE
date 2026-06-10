# MetaSound Tests — Asset Lifecycle

Create, inspect, save, delete. Run sequentially.

---

## Setup

Search for any MetaSound assets under /Game/Audio/MetaSoundTests and delete them if found.

---

## Create

Create a new MetaSound Source at /Game/Audio/MetaSoundTests/MS_Test_Basic in Mono format.

---

## Inspect

Show me the info for MS_Test_Basic — output format, how many nodes it has, and what graph inputs and outputs are defined.

---

## Node List

List all the nodes currently in MS_Test_Basic. What nodes does a freshly created MetaSound Source have by default?

---

## Save

Save MS_Test_Basic.

---

## Create Stereo

Create a second MetaSound Source at /Game/Audio/MetaSoundTests/MS_Test_Stereo in Stereo format.

---

Show me its info and confirm the output format is Stereo.

---

List all nodes in MS_Test_Stereo and note how the output nodes differ from the Mono version.

---

Confirm MS_Test_Basic still exists after deleting the stereo version.


---

# MetaSound Tests — Node Discovery

Search available node classes before building graphs. Run sequentially.

---

## Broad Search

Show me all available MetaSound node types that relate to sine waves or oscillators.

---

Show me all available node types related to wave playback or wave files.

---

Show me available delay and reverb node types.

---

Show me available gain and volume-related node types.

---

Show me available trigger and event node types.

---

Show me available math and arithmetic node types.

---

## Specific Lookups

Find the exact details (inputs, outputs, variant) for the Sine oscillator node.

---

Find the exact details for the Wave Player node — inputs, outputs, and what variants are available.

---

Find all available node types with "Mix" in the name.

---

Find all node types related to random number generation.

---

Find node types related to envelopes or ADSR shaping.

---

## Pin Inspection

Create a temporary MetaSound at /Game/Audio/MetaSoundTests/MS_Test_Discovery for pin inspection.

---

Add a Sine oscillator node to MS_Test_Discovery and show me all its input and output pins.

---

Add a Wave Player (Mono) node and show me all its input and output pins.

---

Add a Delay node (find the right one first) and show me its pins.

---


---

# MetaSound Tests — Node Management and Defaults

Add nodes, set default values, reposition, remove. Run sequentially.

---

## Setup

Create a MetaSound Source at /Game/Audio/MetaSoundTests/MS_Test_Nodes in Mono format.

---

List its default nodes so we know what's there before we start.

---

## Add Nodes

Add a Sine oscillator node to MS_Test_Nodes.

---

Add a Wave Player (Mono) node to MS_Test_Nodes.

---

Find an available delay node and add it to MS_Test_Nodes.

---

Find an available gain or volume node and add it to MS_Test_Nodes.

---

List all nodes now to confirm all four were added successfully.

---

## Set Input Defaults

Set the Sine oscillator's frequency to 440 Hz.

---

Set the Sine oscillator's frequency to 220 Hz (change it again to test overwrite).

---

Set the Wave Player node to loop.

---

Find any SoundWave asset in the project and assign it to the Wave Player node's wave input. If none exists, skip this step and note the result.

---

## Node Position

Move the Sine node to x=-500, y=-200 in the graph.

---

Move the Wave Player node to x=-500, y=100.

---

Move the Delay node to x=-200, y=-200.

---

## Inspect

Show me the pin details for the Sine node to confirm the frequency default is now 220.

---

## Remove Nodes

Remove the Delay node from MS_Test_Nodes.

---

Remove the gain node from MS_Test_Nodes.

---

List all nodes to confirm only the Sine and Wave Player remain (plus the built-in interface nodes).

---

## Save

Save MS_Test_Nodes.


---

# MetaSound Tests — Connections

Wire nodes together and disconnect them. Run sequentially.

---

## Setup

Create a MetaSound Source at /Game/Audio/MetaSoundTests/MS_Test_Connections in Mono format.

---

List its nodes so we can see the default interface nodes.

---

## Connect a Sine to the Audio Output

Add a Sine oscillator node to MS_Test_Connections.

---

Wire the Sine oscillator's audio output into the audio output sink of the graph. The result should be an audible Mono signal when the sound plays.

---

List nodes to confirm the Sine is connected to the output.

---

## Disconnect

Disconnect the Sine from the audio output.

---

List nodes to confirm the audio output sink is now unconnected.

---

## Wave Player Full Chain

Add a Wave Player (Mono) node to MS_Test_Connections.

---

Wire the graph's On Play trigger into the Wave Player's Play input so the wave starts when the sound is triggered.

---

Wire the Wave Player's audio output into the graph audio output sink.

---

List all nodes and confirm both connections are in place.

---

## Disconnect Individual Pins

Disconnect only the trigger connection going into the Wave Player's Play pin, leaving the audio wiring intact.

---

List nodes to confirm the Play pin is now disconnected but the audio output is still wired.

---

## Reconnect

Reconnect On Play into the Wave Player's Play pin.

---

List nodes to confirm the full chain is restored.

---

## Save

Save MS_Test_Connections.


---

# MetaSound Tests — Graph Inputs and Outputs

Expose parameters as runtime-controllable graph inputs and outputs. Run sequentially.

---

## Setup

Create a MetaSound Source at /Game/Audio/MetaSoundTests/MS_Test_GraphIO in Mono format.

---

Show me its graph inputs and outputs before we add anything.

---

## Add Float Input

Add a graph input called "Frequency" as a float with a default value of 440.

---

Show the graph info to confirm Frequency was added.

---

## Add More Inputs

Add a graph input called "Volume" as a float with a default value of 1.0.

---

Add a graph input called "Loop" as a boolean with a default of false.

---

Add a graph input called "Label" as a string with a default of "default".

---

Show the graph info again to confirm all four inputs are listed.

---

## Add Integer Input

Add a graph input called "VoiceIndex" as an integer with a default of 0.

---

## Inspect

Show all graph inputs and outputs for MS_Test_GraphIO.

---

## Remove Inputs

Remove the "Label" string input.

---

Remove the "VoiceIndex" integer input.

---

Show the graph info to confirm only Frequency, Volume, and Loop remain.

---

## Use an Input in the Graph

Add a Sine oscillator node to MS_Test_GraphIO.

---

Wire the Frequency graph input into the Sine oscillator's frequency pin, then connect the Sine's audio output to the graph output.

---

List all nodes to confirm the setup.

---

## Save

Save MS_Test_GraphIO.


---

# MetaSound Tests — End to End Workflows

Full realistic scenarios. Each section is self-contained. Run sequentially.

---

## Scenario 1: Simple Sine Tone

Create a MetaSound Source at /Game/Audio/MetaSoundTests/MS_SineTone that plays a continuous 880 Hz sine tone in Mono. Wire the oscillator to the audio output and save it.

---

Show me the full node list and confirm the Sine is connected to the output.

---

## Scenario 2: Tunable Sine Tone

Create a MetaSound Source at /Game/Audio/MetaSoundTests/MS_TunableSine in Mono format. Expose the frequency as a runtime parameter defaulting to 440 Hz so it can be controlled externally. Wire the oscillator through to the audio output. Save it.

---

Show the graph info and node list to confirm the frequency input is exposed and the graph is connected end to end.

---

## Scenario 3: One-Shot SoundWave Playback

Create a MetaSound Source at /Game/Audio/MetaSoundTests/MS_OneShot in Mono format for triggering a one-shot sound. Add a Wave Player, trigger it from On Play, and route its audio to the output. Find any available SoundWave in the project and assign it — if none exists, leave the wave asset unset. Save it.

---

Show the node list and confirm On Play is wired to the Wave Player's trigger and the audio output is connected.

---

## Scenario 4: Looping Wave with Pitch Control

Create a MetaSound Source at /Game/Audio/MetaSoundTests/MS_LoopingWave in Mono format. Add a Wave Player that loops. Expose a float graph input called "PitchShift" defaulting to 0.0 and wire it into the Wave Player's pitch shift pin. Wire On Play to trigger the playback and route the audio to the output. Save it.

---

Show the full graph — nodes, inputs, outputs — to confirm the structure.

---

## Scenario 5: Layered Stereo Sound

Create a MetaSound Source at /Game/Audio/MetaSoundTests/MS_LayeredStereo in Stereo format. Find an appropriate node for mixing two audio signals together. Add two Sine oscillators — one at 200 Hz and one at 400 Hz — mix them, and route the result to the stereo audio output. Save it.

---

Show the node list and confirm both oscillators are present and connected.

---

## Scenario 6: Delay Effect

Create a MetaSound Source at /Game/Audio/MetaSoundTests/MS_DelayEffect in Mono. Add a Sine oscillator, route it through a delay node, then to the audio output. Set a short delay time (around 0.3 seconds if the node supports it). Save it.

---

Show the node chain to confirm Sine → Delay → Output.

---


---

# MetaSound Tests — Cleanup

Run this section only after completing all the MetaSound test sections above and verifying the results.

---

## Summary

List everything currently under /Game/Audio/MetaSoundTests so the tester can see what exists.

---

> **All MetaSound tests are complete. Please open the UE Content Browser, navigate to /Game/Audio/MetaSoundTests, and verify the assets look correct before continuing.**
>
> When you're ready to clean up, reply "yes delete them all" and the following steps will run.

---

## Delete All Test Assets

Delete all assets under /Game/Audio/MetaSoundTests — MS_Test_Basic, MS_Test_Discovery, MS_Test_Nodes, MS_Test_Connections, MS_Test_GraphIO, MS_SineTone, MS_TunableSine, MS_OneShot, MS_LoopingWave, MS_LayeredStereo, MS_DelayEffect, and anything else found there.

---

Confirm /Game/Audio/MetaSoundTests is now empty.

