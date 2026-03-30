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
