# Sound Cue Tests — Asset Lifecycle

Create, inspect, duplicate, save, delete. Run sequentially.

---

## Setup

Search for any existing sound cues under /Game/Audio/SoundCueTests and delete them if found.

---

## Create

Create a new empty sound cue at /Game/Audio/SoundCueTests/SC_Test_Basic and save it.

---

## Inspect

Tell me about SC_Test_Basic — how many nodes does it have, what's the root, volume and pitch multipliers?

---

## Duplicate

Duplicate SC_Test_Basic to /Game/Audio/SoundCueTests/SC_Test_Copy.

---

Confirm SC_Test_Copy exists and show its info.

---

## Save

Save SC_Test_Basic.

---

## Cleanup

Delete SC_Test_Copy.

---

Confirm SC_Test_Basic still exists after deleting the copy.


---

# Sound Cue Tests — Node Creation

Add every supported node type to a cue and verify they appear. Run sequentially.

---

## Setup

Create a fresh empty sound cue at /Game/Audio/SoundCueTests/SC_Test_Nodes. We'll use this for this whole section.

---

## Wave Player

Add a wave player node to SC_Test_Nodes (no wave asset needed yet).

---

List all nodes in SC_Test_Nodes to confirm it was added.

---

## Random

Add a random node to SC_Test_Nodes.

---

## Mixer

Add a mixer node with 3 inputs to SC_Test_Nodes.

---

## Concatenator

Add a concatenator node with 2 inputs to SC_Test_Nodes.

---

## Modulator

Add a modulator node to SC_Test_Nodes (for random pitch and volume variance).

---

## Attenuation

Add an attenuation node to SC_Test_Nodes.

---

## Looping

Add a looping node to SC_Test_Nodes.

---

## Delay

Add a delay node to SC_Test_Nodes.

---

## Switch

Add a switch node to SC_Test_Nodes (routes audio based on an integer parameter).

---

## Enveloper

Add an enveloper node to SC_Test_Nodes.

---

## Distance Cross Fade

Add a distance cross-fade node with 2 inputs to SC_Test_Nodes.

---

## Branch

Add a branch node to SC_Test_Nodes (routes audio based on a boolean parameter).

---

## Parameter Cross Fade

Add a parameter cross-fade node with 2 inputs to SC_Test_Nodes.

---

## Quality Level

Add a quality level node to SC_Test_Nodes.

---

## Verify

List all nodes in SC_Test_Nodes and confirm all the node types we added are present.

---


---

# Sound Cue Tests — Node Connections

Wire nodes together, set root, disconnect, move nodes. Run sequentially.

---

## Setup

Create an empty sound cue at /Game/Audio/SoundCueTests/SC_Test_Connections.

---

Add two wave player nodes and a random node to it.

---

List all nodes to see their current indices.

---

## Wiring

Wire both wave players into the random node so it picks between them randomly.

---

Set the random node as the output (root) of the cue.

---

List nodes again to confirm the root is set correctly and the connections are in place.

---

## Inspect Connections

Show me the full node list for SC_Test_Connections including connection info for each node.

---

## Disconnect

Disconnect the first wave player from the random node.

---

List nodes again to confirm that input slot is now empty.

---

## Reconnect

Reconnect that wave player back into the random node.

---

List nodes to confirm it's reconnected.

---

## Move Nodes

Move the random node to position x=0, y=0 in the graph.

---

Move each wave player to x=-400, y=-100 and x=-400, y=100 respectively.

---

## Remove a Node

Remove one of the wave player nodes from SC_Test_Connections.

---

List nodes to confirm only two nodes remain (one wave player and the random node).

---


---

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


---

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


---

# Sound Cue Tests — Cleanup

Run this section only after completing all the Sound Cue test sections above and verifying the results.

---

## Summary

List everything currently under /Game/Audio/SoundCueTests so the tester can see what exists.

---

> **All Sound Cue tests are complete. Please open the UE Content Browser, navigate to /Game/Audio/SoundCueTests, and verify the assets look correct before continuing.**
>
> When you're ready to clean up, reply "yes delete them all" and the following steps will run.

---

## Delete All Test Assets

Delete all assets under /Game/Audio/SoundCueTests — SC_Test_Basic, SC_Test_Nodes, SC_Test_Connections, SC_Test_Props, SC_Footstep, SC_Ambient, SC_BgMusic, SC_IntroLoop, SC_Explosion, SC_QualityTest, and anything else found there.

---

Confirm /Game/Audio/SoundCueTests is now empty.

