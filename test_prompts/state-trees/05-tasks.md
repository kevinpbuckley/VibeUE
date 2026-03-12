# StateTree Tests — Tasks

Add, configure, remove, reorder, enable/disable tasks. Task property get/set. Considered-for-completion toggle.

---

### 1. Add a task and discover its properties

```
Add a delay task to the Idle state in /Game/AI/BasicMovement.
Show all editable properties on it. Compile and save.
```

---

### 2. Set a task property

```
In /Game/AI/BasicMovement, set the delay task on Idle to wait 3 seconds. Compile and save.
```

---

### 3. Remove a task

```
Remove the delay task from the Idle state in /Game/AI/BasicMovement. Compile and save.
```

---

### 4. Reorder tasks

```
In /Game/AI/BasicMovement, the Walking state has two tasks.
Move the second one to run first. Compile and save.
```

---

### 5. Enable / disable a task

```
Disable the delay task on Idle in /Game/AI/BasicMovement without removing it.
Verify it shows as disabled. Re-enable it. Compile and save.
```

---

### 6. Task completion toggle

```
In /Game/AI/BasicMovement, make the delay task on Idle run in the background —
it shouldn't count toward the state completing. Compile and save.
```

---

### 7. Add a global task and configure it

```
Add a delay task as a global task to /Game/AI/BasicMovement.
Set its duration to 0.1 seconds.
Compile and save.
```

---

### 8. Remove a global task

```
Remove the global delay task from /Game/AI/BasicMovement. Compile and save.
```