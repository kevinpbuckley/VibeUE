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
