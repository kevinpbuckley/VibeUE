# Animation Blueprint Test Prompts

Test prompts for creating and manipulating Animation Blueprints from scratch.
Goal: Be able to recreate SandboxCharacter_CMC_ABP without using duplicate functionality.

Reference Blueprint: `/Game/Blueprints/SandboxCharacter_CMC_ABP`



## 01 - Create Animation Blueprint

Create an Animation Blueprint called "ABP_TestCharacter" in /Game/Tests/ that inherits from AnimInstance. delete it if it already exists.
Make sure we can open it after creating it.
---

## 02 - List Available Variable Types

List all available variable types I can use when adding variables to an Animation Blueprint. Include basic types (bool, int, float, etc.), structs, enums, and object references.

---

## 03 - Add Basic Variables

Add the following variables to ABP_TestCharacter:
- Speed2D (float, default 0.0)
- HasVelocity (bool, default false)
- HasAcceleration (bool, default false)

---

## 04 - Add Struct Variables

Add these struct variables to ABP_TestCharacter:
- Velocity (FVector)
- Acceleration (FVector)
- CharacterTransform (FTransform)
- RootTransform (FTransform)

---

## 05 - Add Enum Variables

Add these enum variables to ABP_TestCharacter:
- MovementMode (EMovementMode, byte)
- MovementState (byte enum)
- Gait (byte enum)
- Stance (byte enum)

---

## 06 - Add Object Reference Variables

Add these object reference variables to ABP_TestCharacter:
- Mover (reference to MoverComponent)
- CurrentSelectedAnim (reference to AnimSequence)
- CurrentSelectedDatabase (reference to PoseSearchDatabase)

---

## 07 - Add Array Variables

Add these array variables to ABP_TestCharacter:
- PawnSpeedHistory (array of float)
- ValidDatabases (array of PoseSearchDatabase references)

---

## 08 - List Variables in Blueprint

List all variables in ABP_TestCharacter showing their name, type, and default value.

---

## 09 - List Available Graphs

List all graphs available in ABP_TestCharacter (EventGraph, AnimGraph, any state machines).

---

## 10 - Get Event Graph Nodes

Get all nodes currently in the EventGraph of ABP_TestCharacter.

---

## 11 - Add Event Node - Blueprint Initialize Animation

Add the "Blueprint Initialize Animation" event node to the EventGraph of ABP_TestCharacter.

---

## 12 - Add Event Node - Blueprint Update Animation

Add the "Blueprint Update Animation" event node to the EventGraph of ABP_TestCharacter.

---

## 13 - Add Event Node - Blueprint Thread Safe Update Animation

Add the "Blueprint Thread Safe Update Animation" event node to the EventGraph of ABP_TestCharacter.

---

## 14 - Add Get Owner Node

Add a "Try Get Pawn Owner" node to the EventGraph and connect it to the Blueprint Initialize Animation event.

---

## 15 - Add Cast Node

Add a "Cast To Character" node after Try Get Pawn Owner and connect them.

---

## 16 - Add Branch Node

Add a Branch node that checks if the cast was successful.

---

## 17 - Add Variable Get Node

Add a Get node for the Velocity variable in EventGraph.

---

## 18 - Add Variable Set Node

Add a Set node for the Speed2D variable and connect it to update based on velocity length.

---

## 19 - Create Custom Function

Create a custom function called "UpdateMovementData" with no inputs/outputs in ABP_TestCharacter.

---

## 20 - Add Function Input Parameters

Add input parameters to UpdateMovementData:
- DeltaTime (float)
- NewVelocity (FVector)

---

## 21 - Add Function Output Parameters

Add output parameters to UpdateMovementData:
- Speed (float)
- HasMovement (bool)

---

## 22 - Call Custom Function

Add a node in EventGraph that calls the UpdateMovementData function.

---

## 23 - List Available AnimGraph Node Types

List all available node types that can be added to an AnimGraph (state machines, blend nodes, pose nodes, etc.).

---

## 24 - Get AnimGraph Nodes

Get all nodes currently in the AnimGraph of ABP_TestCharacter.

---

## 25 - Add Output Pose Node

Verify the Output Pose node exists in AnimGraph (should be there by default).

---

## 26 - Add State Machine Node

Add a State Machine node called "Locomotion" to the AnimGraph.

---

## 27 - List States in State Machine

List all states in the Locomotion state machine.

---

## 28 - Add State to State Machine

Add a new state called "Idle" to the Locomotion state machine.

---

## 29 - Add Another State

Add a state called "Moving" to the Locomotion state machine.

---

## 30 - Add Transition Between States

Add a transition from Idle to Moving state.

---

## 31 - Add Transition Condition

Set the transition condition from Idle to Moving to check if HasVelocity is true.

---

## 32 - Add Reverse Transition

Add a transition from Moving back to Idle when HasVelocity is false.

---

## 33 - Add Blend Space Player Node

Add a Blend Space Player node inside the Moving state.

---

## 34 - Add Sequence Player Node

Add a Sequence Player node inside the Idle state.

---

## 35 - Set Animation Asset on Node

Set the animation asset on the Sequence Player to an idle animation.

---

## 36 - Add Pose Search Node

Add a Motion Matching / Pose Search node to AnimGraph.

---

## 37 - Configure Pose Search Database

Set the Pose Search Database on the Motion Matching node.

---

## 38 - Add Blend Node

Add a Blend Poses by Bool node to AnimGraph.

---

## 39 - Add Layered Blend Node

Add a Layered Blend Per Bone node to AnimGraph.

---

## 40 - Add Slot Node

Add an Animation Slot node to AnimGraph for montages.

---

## 41 - Connect AnimGraph Nodes

Connect the State Machine output to the Output Pose node.

---

## 42 - Add Control Rig Node

Add a Control Rig node to AnimGraph.

---

## 43 - Add Foot IK Node

Add a Foot Placement / Foot IK node to AnimGraph.

---

## 44 - Add Modify Bone Node

Add a Modify Bone (Transform Bone) node to AnimGraph.

---

## 45 - List All Functions

List all functions in ABP_TestCharacter including inherited ones.

---

## 46 - Override Parent Function

Override the "Blueprint Update Animation" function from the parent AnimInstance class.

---

## 47 - Add Local Variable to Function

Add a local variable to the UpdateMovementData function.

---

## 48 - Add Math Expression Node

Add a VectorLength node to calculate speed from velocity in EventGraph.

---

## 49 - Add Comparison Node

Add a Greater Than comparison node to check if speed > 0.

---

## 50 - Compile Animation Blueprint

Compile ABP_TestCharacter and report any errors.

---

## 51 - Save Animation Blueprint

Save ABP_TestCharacter to disk.

---

## 52 - Get Node Pin Information

Get all pins on a specific node (show input/output pins, their types, and connections).

---

## 53 - Connect Pins By Name

Connect the output pin "ReturnValue" from one node to input pin "Target" on another node.

---

## 54 - Disconnect Pins

Disconnect a specific pin connection.

---

## 55 - Set Node Property

Set a property on a node (e.g., set the animation asset on a sequence player).

---

## 56 - Get Available Parent Classes

List available parent classes for Animation Blueprints (AnimInstance, and any custom AnimInstance subclasses in the project).

---

## 57 - Create AnimBP with Specific Skeleton

Create an Animation Blueprint that targets a specific skeleton (e.g., the Mannequin skeleton).

---

## 58 - Add Cached Pose Node

Add a "Save Cached Pose" node and a "Use Cached Pose" node to AnimGraph.

---

## 59 - Add Blend by Int Node

Add a Blend Poses by Int node to AnimGraph.

---

## 60 - Add Two Bone IK Node

Add a Two Bone IK node to AnimGraph.

---

## 61 - Inspect Reference Blueprint

Get complete information about SandboxCharacter_CMC_ABP including all variables, functions, event graph nodes, anim graph structure, and state machines.

---

## 62 - Compare Blueprints

Compare ABP_TestCharacter with SandboxCharacter_CMC_ABP and list what's missing.


