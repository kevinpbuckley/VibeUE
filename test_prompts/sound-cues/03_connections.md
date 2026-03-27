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

