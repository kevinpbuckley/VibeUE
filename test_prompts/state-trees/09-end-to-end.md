# StateTree Tests — End-to-End

Complex multi-step scenarios that exercise multiple API areas together.

---

### 1. Full AI behavior tree from scratch

```
Build a complete enemy AI StateTree at /Game/AI/EnemyBehavior:

Root
  Idle — delay task, transitions to Patrol on completion
  Patrol (group)
    MoveToWaypoint — loops on success, falls back to Idle on failure
  Combat
    Engage — succeeds to Idle, fails with Failed transition

Add a float parameter patrol_speed with default 300.
Make Root randomly pick among its children.
Compile and save.
```

---

### 2. Refactor a tree

```
In /Game/AI/BasicMovement:
1. Remove all transitions from the Idle state
2. Remove the delay task from Idle
3. Rename Idle to Resting
4. Add a float parameter rest_duration with default 2.0
5. Add a delay task to Resting with its duration bound to rest_duration
6. Add a transition from Resting to Walking on completion
Compile and save.
```

---

### 3. Parameter-driven multi-state tree

```
In /Game/AI/BasicMovement:
- Add parameters: idle_time (float 1.0), walk_time (float 2.0), run_time (float 0.5)
- Ensure Idle, Walking, Running each have a delay task
- Bind each delay duration to its matching parameter
- Wire transitions so they cycle: Idle → Walking → Running → Idle on completion
Compile and save.
```

---

### 4. Conditions + delayed fallback

```
In /Game/AI/BasicMovement:
- Add a random enter condition to Idle
- Add a condition to Walking's first transition
- Add a delayed transition (1.5s delay, 0.3s variance) from Running back to Idle as a safety net
Compile and save.
```

---

### 5. Full Python script

```
Write a Python script using unreal.StateTreeService that:
1. Creates a StateTree at /Game/Test/PythonTree
2. Adds Root, StateA, StateB, StateC
3. Adds a delay task to StateA
4. Wires transitions: A→B on completion, B→C on success, C→A on completion (loop)
5. Adds a float parameter loop_delay defaulting to 1.0
6. Binds StateA's delay duration to loop_delay
7. Compiles, prints any errors/warnings, saves if successful
8. Calls get_state_tree_info and prints the state count
```