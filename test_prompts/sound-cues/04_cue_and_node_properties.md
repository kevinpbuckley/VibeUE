# Sound Cue Tests — Cue and Node Properties

Volume, pitch, sound class, attenuation, concurrency, node-level properties, SoundWave info.

---

## Setup

Create an empty sound cue at /Game/Audio/SoundCueTests/SC_Test_Props.

Add a wave player node and a modulator node to it.

---

## Cue-Level Volume and Pitch

Set the volume multiplier on SC_Test_Props to 1.5.

---

Set the pitch multiplier on SC_Test_Props to 0.8.

---

Show me the cue info to confirm the new volume and pitch values are stored.

---

Reset the volume multiplier back to 1.0 and the pitch back to 1.0.

---

## Attenuation

Set an attenuation override on SC_Test_Props. Search for any available attenuation asset first — if none exists, confirm the set fails gracefully.

---

Clear the attenuation override on SC_Test_Props.

---

Confirm the attenuation is now empty.

---

## Concurrency

Search for any existing concurrency assets under /Game. If one is found, assign it to SC_Test_Props and then clear it. If none exist, confirm the operation behaves correctly.

---

## Node Properties — Modulator

List nodes in SC_Test_Props to find the modulator node index.

---

Read the pitch min property on the modulator node.

---

Set the pitch min property on the modulator node to 0.9 and the pitch max to 1.1.

---

Read the pitch min back to confirm the change took effect.

---

## Wave Player — Reassign Wave Asset

Find any SoundWave asset in the project to use as a reference. If one exists, assign it to the wave player node in SC_Test_Props.

---

Read the wave asset back from the wave player node to confirm the assignment.

---

## SoundWave Info

If a SoundWave was found above, show me its info — duration, sample rate, channels, looping, and streaming status.

---

## Save

Save SC_Test_Props.

---

