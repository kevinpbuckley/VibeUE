# Sound Cue Tests — End to End Workflows

Full realistic scenarios. Run sequentially. Each section is self-contained.

---

## Scenario 1: Randomised Footstep Cue

Search for any SoundWave assets in the project that could serve as footstep sounds. We'll use whatever we find (or empty wave players if nothing is available).

---

Create a sound cue at /Game/Audio/SoundCueTests/SC_Footstep for a randomised footstep effect. It should pick randomly between three different sounds each time it plays. Wire it up so the random node is the output. If SoundWave assets were found, assign them to the wave players. Save the result.

---

Show me the full node structure of SC_Footstep to verify the random node has three wave player inputs and is set as root.

---

## Scenario 2: Ambient Blend

Create a sound cue at /Game/Audio/SoundCueTests/SC_Ambient that plays two sounds simultaneously by mixing them together. Wire both into a mixer node and set the mixer as the output. Save it.

---

Show me the node list for SC_Ambient and confirm the mixer has two connected inputs and is root.

---

## Scenario 3: Looping Background Music

Create a sound cue at /Game/Audio/SoundCueTests/SC_BgMusic designed to loop a single sound continuously. Add a wave player and a looping node, wire the wave player into the looping node, and set the looping node as root. Set the volume multiplier to 0.75. Save it.

---

Show me the info and nodes for SC_BgMusic to confirm it's wired correctly.

---

## Scenario 4: Sequential Intro + Loop

Create a sound cue at /Game/Audio/SoundCueTests/SC_IntroLoop that plays two sounds in sequence using a concatenator. Wire two wave players into it and set it as root. Save it.

---

## Scenario 5: Distance-Aware Explosion

Create a sound cue at /Game/Audio/SoundCueTests/SC_Explosion for an explosion that sounds different at different distances. Add two wave player nodes and a distance cross-fade node with 2 inputs. Wire both wave players into the distance cross-fade, set it as root, and add an attenuation node between the cross-fade and root for spatial positioning if possible. Save the cue.

---

Show me the full node list for SC_Explosion to verify the structure.

---

## Scenario 6: Quality-Dependent Sound

Create a sound cue at /Game/Audio/SoundCueTests/SC_QualityTest that uses a quality level node so different audio plays at different scalability levels. Add a wave player for each quality slot and wire them into the quality level node. Set the quality level node as root. Save it.

---

