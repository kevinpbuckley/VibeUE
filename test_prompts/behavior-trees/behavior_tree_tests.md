# Behavior Tree Tests — Blackboard Basics

Create blackboards, CRUD keys, all eleven key types, rename with broken-reference report.

---

### 1. Create a blackboard with common keys

```
Create a Blackboard at /Game/AI/BB_Guard with these keys:
TargetActor (Object, base class Actor), HomeLocation (Vector),
IsAlerted (Bool), PatrolIndex (Int). Then list its keys back to me
with their types.
```

---

### 2. Every key type

```
Create a Blackboard at /Game/AI/BB_AllTypes and add one key of every
supported type: Bool, Int, Float, String, Name, Vector, Rotator, Object,
Class, Enum, NativeEnum. Report which succeeded and read the keys back.
```

---

### 3. Object key base class

```
On /Game/AI/BB_Guard, restrict the TargetActor key so it only accepts
Pawn (or a Pawn subclass). Verify by reading the key back.
```

---

### 4. Instance-synced key

```
Add a Bool key named SquadAlerted to /Game/AI/BB_Guard that is
instance-synced (shared across all AI using this blackboard).
Verify the synced flag on read-back.
```

---

### 5. Parent blackboard inheritance

```
Create a Blackboard at /Game/AI/BB_GuardElite that uses /Game/AI/BB_Guard
as its parent. Add an EliteRank (Int) key to the child only.
List the child's keys and tell me which are inherited and which are its own.
```

---

### 6. Remove a key

```
Remove the PatrolIndex key from /Game/AI/BB_Guard and confirm it is gone.
Then try to remove it again and show me the error.
```

---

### 7. Rename a key that Behavior Trees reference

```
Rename the IsAlerted key on /Game/AI/BB_Guard to bIsAlerted.
If any Behavior Tree node selectors referenced the old name, list every
one of them (asset, node, property) so I can re-point the bindings.
```

---

### 8. Invalid key type is refused

```
Try to add a key of type "Texture" to /Game/AI/BB_Guard.
It should be refused with a clear error naming the valid types —
confirm no key was created.
```

---

### 9. Edit key metadata after creation

```
On /Game/AI/BB_Guard, make PatrolIndex instance-synced, put it in the
"Patrol" category, and give it the description "Index of the current
patrol waypoint". Read the keys back and confirm all three stuck.
Then clear the description again.
```

---

### 10. Re-parent a blackboard safely

```
Detach /Game/AI/BB_GuardElite from its parent, then re-attach it to
/Game/AI/BB_Guard. Now try two things that must be refused:
1. Make /Game/AI/BB_Guard a child of /Game/AI/BB_GuardElite (a cycle).
2. Add a key named TargetActor to BB_GuardElite and re-attach it to
   BB_Guard (the same-named key would shadow the parent's).
Both refusals should explain why.
```

---

### 11. Find key references before deleting

```
Before removing the HomeLocation key from /Game/AI/BB_Guard, list every
Behavior Tree node selector that is bound to it (FindBlackboardKeyReferences).
If anything references it, show me the list instead of removing it.
Also confirm that removing SelfActor is refused (it is engine-persistent).
```

---

# Behavior Tree Tests — Asset Lifecycle & Discovery

Create trees, list them, inspect, assign blackboards, node-type discovery.

---

### 1. Create a tree with a blackboard

```
Create a Behavior Tree at /Game/AI/BT_Guard using the blackboard
/Game/AI/BB_Guard. Confirm with GetBehaviorTreeInfo that it has a graph,
a root node, and the right blackboard assigned.
```

---

### 2. List Behavior Trees

```
List all Behavior Tree assets under /Game and show each one's node count
and assigned blackboard.
```

---

### 3. Discover available node classes

```
Show me the Behavior Tree node classes I can use: all composites, the
first ten tasks, the first ten decorators, and the first ten services.
Flag which of them are Blueprint-derived rather than native.
```

---

### 4. Reassign a blackboard

```
Point /Game/AI/BT_Guard at the blackboard /Game/AI/BB_GuardElite instead,
then verify the change with GetBehaviorTreeInfo.
```

---

# Behavior Tree Tests — Building Structure

AddNode, child ordering, decorators, services, SimpleParallel slots.

---

### 1. Build a patrol/combat tree

```
In /Game/AI/BT_Guard, build this structure:
  Root
    Selector "Brain"
      Sequence "Combat"   (only when a target is set)
        Wait 0.5s
        Wait 2s
      Sequence "Patrol"
        Wait 5s
Use AddNode for the composites and Wait tasks, name the composites with
SetNodeName, and read the tree back with GetTree to confirm the structure
and execution order match exactly.
```

---

### 2. Insertion order matters

```
In /Game/AI/BT_Guard, insert a new Sequence named "Investigate" between
Combat and Patrol (child index 1 of Brain). Read the tree back and confirm
the children are Combat, Investigate, Patrol — in that order.
```

---

### 3. Decorator with a blackboard binding

```
Add a Blackboard-based condition decorator to the Combat sequence in
/Game/AI/BT_Guard that checks the TargetActor key is set.
Use SetNodeBlackboardKey for the key binding, then read the decorator's
properties back to prove the binding stuck after save.
```

---

### 4. Decorator order is evaluation order

```
Add a Cooldown decorator to the Combat sequence, then insert a Time Limit
decorator BEFORE it (index 0). Confirm via GetTree that the decorator
order is TimeLimit, Cooldown — then remove the TimeLimit decorator and
confirm the Cooldown decorator's path renumbered to @decorator[0].
```

---

### 5. Service on a composite and on a task

```
Add a "Run EQS" or any available service to the Brain selector in
/Game/AI/BT_Guard, and a second service to one of the Wait tasks.
Both should succeed in UE 5.8 — verify both appear in GetTree under the
right owner.
```

---

### 6. SimpleParallel's two fixed slots

```
In a fresh tree /Game/AI/BT_ParallelTest, add a SimpleParallel under Root.
Then:
1. Add a Wait task — it should land in slot 0 (the main task).
2. Add a Sequence — it should land in slot 1 (the background branch).
3. Try to add a third child — that must be refused.
4. Try to remove the background branch — that must be refused, and the
   error should tell you to replace it via AddNode with index 1 instead.
Show the tree JSON at the end.
```

---

### 7. Composite-only rules

```
In /Game/AI/BT_Guard, try to add a child node UNDER a Wait task (tasks are
leaves). Then try to attach a decorator to the graph's Root node. Both
must be refused with errors that explain why, and the tree must be
unchanged afterwards — verify with GetTree.
```

---

# Behavior Tree Tests — Restructuring

MoveNode, RemoveNode, rename, path stability, GUID identity.

---

### 1. Reparent a subtree

```
In /Game/AI/BT_Guard, move the Investigate sequence (with everything
under it) to be the LAST child of Brain. Confirm the new order is
Combat, Patrol, Investigate and that Investigate kept its own children.
```

---

### 2. Cycle refusal

```
In /Game/AI/BT_Guard, try to move the Brain selector under its own child
Combat. This must be refused (a node cannot become its own descendant).
Verify the tree is unchanged.
```

---

### 3. Remove a subtree

```
Remove the Investigate sequence from /Game/AI/BT_Guard. Its whole subtree
must go with it. Then try to remove the Brain selector (the root's only
child) — that must be refused because it would leave the tree with no
runtime root.
```

---

### 4. Rename changes the path

```
Rename the Combat sequence in /Game/AI/BT_Guard to "Engage". Confirm the
old path no longer resolves, the new one does, and the node kept the same
GUID across the rename. Also try renaming a node to "Bad/Name" and to
"Thing[3]" — both shapes must be refused, including when written through
the NodeName property with SetNodePropertyValue.
```

---

### 5. Comment a subtree

```
Put the comment "Falls back to patrol when no target is set" on the
Patrol sequence in /Game/AI/BT_Guard (SetNodeComment). Confirm GetTree
reports it in the node's "comment" field, then round-trip the tree with
BuildTree into a copy and confirm the comment survived.
```

---

# Behavior Tree Tests — Node Properties

Discovery, read, write, re-read semantics, blackboard key selectors.

---

### 1. Discover a node's properties

```
List every editable property of one of the Wait tasks in /Game/AI/BT_Guard
with its type and current value. Which of them are blackboard key
selectors that need SetNodeBlackboardKey instead of SetNodePropertyValue?
```

---

### 2. Set a numeric property and trust the read-back

```
Set WaitTime to 3.5 on the first Wait task in the Engage sequence of
/Game/AI/BT_Guard. The result's ValueAfterWrite is re-read from the asset
after the save — confirm it reports 3.5 and that success is true.
```

---

### 3. Bad value cannot half-write

```
On that same Wait task, try to set WaitTime to "banana". The write must
fail, and reading WaitTime afterwards must still return 3.5 — a refused
write may not leave the node changed.
```

---

### 4. Mistyped blackboard binding is refused up front

```
In /Game/AI/BT_Guard, add a "Blackboard Decorator" to the Patrol sequence
and try to bind its BlackboardKey to HomeLocation (a Vector key) — if the
selector's filter doesn't accept vectors, the bind must be refused
immediately, NOT written and silently cleared by the next save.
Then bind it to bIsAlerted (Bool) and confirm that succeeds.
```

---

### 5. Tree with no blackboard refuses bindings

```
Create a tree /Game/AI/BT_NoBB with no blackboard. Add a Blackboard
decorator to a sequence under root and try to bind its key to anything.
The error must say the tree has no blackboard rather than failing
mysteriously.
```

---

# Behavior Tree Tests — Round-trip, Repair, Validation

GetTree → BuildTree, bReplaceExisting, RepairGraphFromRuntimeTree, ValidateTree.

---

### 1. Full round-trip copy

```
Read /Game/AI/BT_Guard with GetTree, create a fresh tree at
/Game/AI/BT_GuardCopy with the same blackboard, and replay the JSON into
it with BuildTree. Then GetTree both assets and diff the structures:
same classes, same child order, same decorators/services, same edited
properties. Report any node whose build entry failed.
```

---

### 2. BuildTree refuses a populated target

```
Run that same BuildTree against /Game/AI/BT_GuardCopy again WITHOUT
bReplaceExisting. It must be refused before anything is written, naming
the existing top-level node. Then run it WITH bReplaceExisting=true and
confirm it rebuilt cleanly (per-node results all green).
```

---

### 3. Partial failure is reported per node

```
Take the GetTree JSON of /Game/AI/BT_Guard, edit one node's "class" to
"BTTask_DoesNotExist", and BuildTree it into a fresh asset
/Game/AI/BT_PartialTest (same blackboard). The bad node and its subtree
must fail with a clear error, its siblings must still build, and the
overall result must not claim success. Show the per-node outcomes.
```

---

### 4. Validate a healthy tree

```
Run ValidateTree on /Game/AI/BT_Guard. It should return no findings.
```

---

### 5. Validation catches a broken binding

```
On /Game/AI/BT_Guard, rename the blackboard key bIsAlerted to AlertState
(on /Game/AI/BB_Guard). The rename response lists the BT bindings that
just broke. Now run ValidateTree on /Game/AI/BT_Guard — it must report
the node whose selector still references the old name. Re-point that
binding with SetNodeBlackboardKey and validate again until clean.
```

---

### 6. Repair a graph from the runtime tree

```
Check GetBehaviorTreeInfo for a tree that has a runtime tree but whose
editor graph shows (almost) no nodes. If you don't have one, note that
RepairGraphFromRuntimeTree refuses when there is nothing to repair —
verify that refusal on the healthy /Game/AI/BT_Guard: it must decline
because the graph already has nodes under its root.
```

---

# Behavior Tree Tests — Write Guards & Safety

The refusals that protect real assets. All of these must fail closed.

---

### 1. Open-editor guard

```
Open /Game/AI/BT_Guard in the Behavior Tree editor (double-click it, or
use the asset-open tooling). While it is open, try to add a node to it
via the service. The write must be refused because an open editor holds
its own copy of the graph. Close the editor and retry — it must succeed.
```

---

### 2. PIE guard

```
Start PIE. While the game is running, try any Behavior Tree write (add a
node to /Game/AI/BT_Guard). It must be refused with an error naming the
asset and PIE as the reason. Stop PIE and confirm the same write goes
through.
```

---

### 3. Reads never create graphs

```
Find or create a Behavior Tree asset that has never been opened by this
service. Call GetTree and GetNodePropertyValue on it. Both must report
the missing graph as an error WITHOUT writing anything to the asset —
confirm the asset is not dirty afterwards.
```

---

# Behavior Tree Tests — End-to-End Scenario

One realistic authoring session, start to finish.

---

### 1. Author a complete guard AI from scratch

```
Build a complete guard AI in /Game/AI/E2E:
1. Blackboard BB_Sentry: TargetActor (Object/Actor), LastKnownPos (Vector),
   Alertness (Float, instance-synced).
2. Behavior Tree BT_Sentry on that blackboard:
   Root → Selector
     Sequence "Attack" [decorator: TargetActor is set]
       task: rotate/move toward target (pick a suitable stock task), Wait 1s
     Sequence "Search" [decorator: LastKnownPos is set, cooldown 5s]
       Wait 3s
     Sequence "Idle" [service: any stock service at 1s interval]
       Wait 10s
3. Set every Wait's WaitTime as listed, bind every decorator key,
   run ValidateTree until it reports nothing, and CompileAndSave.
4. Read the final tree back and present it as an indented outline with
   each node's key properties.
```
