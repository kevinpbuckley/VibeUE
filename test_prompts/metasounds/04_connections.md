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
