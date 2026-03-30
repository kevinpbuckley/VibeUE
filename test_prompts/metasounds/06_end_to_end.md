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

