# StateTree Tests — Conditions

Enter conditions on states and conditions on transitions: add, configure, set operand, remove.

---

### 1. List available condition types

```
What kinds of conditions can I add to states in /Game/AI/BasicMovement?
```

---

### 2. Add an enter condition to a state

```
Add a random condition to the Idle state in /Game/AI/BasicMovement
so it doesn't always activate. Compile and save.
```

---

### 3. Add multiple conditions with And/Or

```
In /Game/AI/BasicMovement, add two enter conditions to Walking:
a float comparison and a random check.
The second one should be ANDed with the first. Compile and save.
```

---

### 4. Set enter condition properties

```
Show the properties on the first enter condition on Idle in /Game/AI/BasicMovement,
then set the first numeric property to 0.75. Compile and save.
```

---

### 5. Remove an enter condition

```
Remove the second enter condition from Walking in /Game/AI/BasicMovement. Compile and save.
```

---

### 6. Add a condition to a transition

```
Add a condition to the first transition on Idle in /Game/AI/BasicMovement
so it only fires when a random roll passes. Compile and save.
```

---

### 7. Set transition condition properties

```
Show the properties on the condition attached to the first transition on Idle
in /Game/AI/BasicMovement. Set the threshold to 0.5. Compile and save.
```

---

### 8. Remove a transition condition

```
Remove the first condition from the first transition on Idle in /Game/AI/BasicMovement. Compile and save.
```