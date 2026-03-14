# StateTree Tests — Parameters

Root parameter bag: inspect, add, update, remove, rename, bind.

---

### 1. Inspect existing parameters

```
What parameters does /Game/AI/BasicMovement have? Show each one's name, type, and default value.
```

---

### 2. Add parameters of different types

```
Add the following parameters to /Game/AI/BasicMovement:
- patrol_speed (float, default 300.0)
- is_aggressive (bool, default false)
- max_alerts (int, default 3)
- status_message (string, default "Idle")
Compile and save.
```

---

### 3. Update a parameter's default value

```
Change the default value of patrol_speed in /Game/AI/BasicMovement to 450.0.
Compile and save.
```

---

### 4. Remove a parameter

```
Remove the max_alerts parameter from /Game/AI/BasicMovement. Compile and save.
```

---

### 5. Rename a parameter

```
Rename the patrol_speed parameter to movement_speed in /Game/AI/BasicMovement.
Compile and save.
```

---

### 6. Bind a task property to a parameter

```
In /Game/AI/BasicMovement, make the delay task in Idle use the idling_time parameter
as its duration instead of a fixed value.
First add a float parameter idling_time with default 1.5 if it doesn't exist.
Compile and save.
```

---

### 7. Bind a task property to context

```
In /Game/AI/BasicMovement, bind the debug text task's ReferenceActor property
to the Actor context so it follows the owning actor. Compile and save.
```