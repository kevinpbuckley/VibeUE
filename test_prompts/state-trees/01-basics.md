# StateTree Tests — Basics

Create, inspect, list, schema selection.

---

### 1. Create a simple three-state tree

```
Create a StateTree at /Game/AI/BasicMovement with three states under Root:
Idle, Walking, and Running. Add a delay task to Idle.
Add transitions: Idle → Walking on completion, Walking → Running on completion,
Running → Idle on completion. Compile and save.
```

---

### 2. Create with explicit schema

```
Create a StateTree at /Game/StateTree/MyComponentTree.
Add a Root subtree and a single Idle state. Compile and save.
```

---

### 3. Create for an AI character

```
Create a StateTree at /Game/StateTree/MyAITree for an AI-controlled character.
Add a Root subtree with Patrol and Combat states. Compile and save.
Then confirm what schema it's using.
```

---

### 4. Inspect an existing tree

```
Show me the full structure of /Game/AI/BasicMovement —
every state, its tasks, transitions, and whether it's compiled.
```

---

### 5. List all StateTrees

```
List all StateTree assets under /Game and show their compile status.
```

---

### 6. Compile failure detection

```
Create a StateTree at /Game/AI/BrokenTree with a Root state and a child named Orphan
that has a transition to a non-existent state called Ghost.
Try to compile and report the errors.
```