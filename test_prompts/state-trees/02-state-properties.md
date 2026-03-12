# StateTree Tests — State Properties

Hierarchy management, rename, selection behavior, task completion, enable/disable, description, weight.

---

### 1. Nested group hierarchy

```
Create a StateTree at /Game/AI/PatrolAI.
Structure:
  Root
    Patrol (group containing Search and MoveToPoint)
    Combat (group containing Engage and Retreat)
Add transitions: Search → MoveToPoint on success, MoveToPoint → Search on completion.
Compile and save.
```

---

### 2. Add a state to an existing tree

```
The StateTree at /Game/AI/BasicMovement is missing a Sprinting state.
Add it under Root, then add a transition from Running to Sprinting on completion.
Recompile and save.
```

---

### 3. Remove a state

```
Remove the Running state from /Game/AI/BasicMovement. Recompile and save.
```

---

### 4. Rename a state

```
Rename the Walking state in /Game/AI/BasicMovement to Jogging. Compile and save.
```

---

### 5. Set selection behavior

```
In /Game/AI/BasicMovement, make Root randomly pick among its children
instead of always choosing in order. Compile and save.
```

---

### 6. Set task completion mode

```
In /Game/AI/BasicMovement, change the Idle state so it only completes
when ALL of its tasks finish, not just any one of them. Compile and save.
```

---

### 7. Enable and disable states

```
Disable the Running state in /Game/AI/BasicMovement.
Verify it shows as disabled, then re-enable it.
```

---

### 8. State description

```
Add the description "Character is moving at a brisk pace" to the Walking state
in /Game/AI/BasicMovement.
```

---

### 9. State weight for utility selection

```
In /Game/AI/PatrolAI, set the Combat state weight to 3.0 and Patrol to 1.0
so Combat is three times more likely to be selected by utility. Compile and save.
```

---

### 10. Context actor class

```
Set the context actor class for /Game/AI/BasicMovement to BP_Cube.
Recompile and save.
```