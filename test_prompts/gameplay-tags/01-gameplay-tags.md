# Gameplay Tags Tests

Add, list, inspect, rename, remove gameplay tags.

---

### 1. Add a single tag

```
Add a gameplay tag called "Combat.MeleeAttack" with a comment "Triggered on melee attack"
```

---

### 2. Add multiple tags at once

```
Add these gameplay tags: "Cube.StartChasing", "Cube.StopChasing", and "Cube.Idle".
Use the comment "Cube state events" for all of them.
```

---

### 3. List all tags

```
Show me all registered gameplay tags in the project.
```

---

### 4. List tags with a filter

```
Show me only the gameplay tags that start with "Cube".
```

---

### 5. Check if a tag exists

```
Does the gameplay tag "Cube.StartChasing" exist?
What about "Fake.DoesNotExist"?
```

---

### 6. Get detailed info on a tag

```
Show me the full details of the "Cube.StartChasing" tag —
where it was defined, its comment, and how many children it has.
```

---

### 7. Inspect tag hierarchy

```
Show me all the direct children of the "Cube" tag.
```

---

### 8. Rename a tag

```
Rename the gameplay tag "Cube.Idle" to "Cube.Rest".
Then confirm it shows up under the new name.
```

---

### 9. Remove a tag

```
Remove the gameplay tag "Cube.Rest".
Then verify it no longer exists.
```

---

### 10. End-to-end with StateTree

```
Create two gameplay tags "AI.StartPatrol" and "AI.StopPatrol" with comment "Patrol events".
Then create a StateTree at /Game/AI/PatrolTree with Idle and Patrol states.
Add an OnEvent transition from Idle to Patrol.
Set the transition's event tag to "AI.StartPatrol".
Compile and save.
```
