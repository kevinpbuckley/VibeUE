# StateTree Tests — Transitions

Add, edit, disable, delay, remove, reorder transitions.

---

### 1. All trigger types

```
In /Game/AI/BasicMovement, add the following transitions to the Idle state:
- fires when the state succeeds → go to Walking
- fires every tick → go to Running
- fires on an event → go to Idle
Compile and save.
```

---

### 2. All transition types

```
In /Game/AI/BasicMovement, add transitions to Walking:
- on completion → go to the next sibling state
- on failure → mark as Failed
- on success → mark as Succeeded
Compile and save.
```

---

### 3. Priority transitions

```
Add two transitions to the Running state in /Game/AI/BasicMovement:
- on failure → go to Idle with Critical priority
- on completion → next state with Normal priority
Compile and save.
```

---

### 4. Change a transition's trigger

```
The first transition on the Idle state in /Game/AI/BasicMovement fires on completion.
Change it to fire only on success instead. Compile and save.
```

---

### 5. Change a transition's target

```
In /Game/AI/BasicMovement, the transition from Walking that goes to Running
should now go to Idle instead. Fix it. Compile and save.
```

---

### 6. Disable a transition

```
Temporarily disable the second transition on the Running state
in /Game/AI/BasicMovement without removing it. Compile and save.
```

---

### 7. Add a delay to a transition

```
Make the transition from Idle to Walking in /Game/AI/BasicMovement
wait 2 seconds before firing, with a random variance of 0.5 seconds. Compile and save.
```

---

### 8. Remove a transition

```
Remove the last transition on the Idle state in /Game/AI/BasicMovement. Compile and save.
```

---

### 9. Reorder transitions

```
In /Game/AI/BasicMovement, move the third transition on Running to be first.
Compile and save.
```