# StateTree Tests — Linked Subtrees & Linked Assets

States can be promoted to special types that delegate execution to other parts
of the same tree (`LinkedSubtree`) or to a separate StateTree asset
(`LinkedAsset`). Useful for sharing behavior fragments without duplicating them.

---

### 1. Convert a state to a Group

```
In /Game/AI/BasicMovement, change the Idle state's type from a normal State
to a Group so it can hold child states without running its own tasks.
Compile and save.
```

---

### 2. Convert a state to a LinkedSubtree

```
In /Game/AI/PatrolAI, change the type of the Combat state to LinkedSubtree.
Then link it to the Patrol subtree (also under Root) so Combat re-uses Patrol's
states at runtime. Compile and save.
```

---

### 3. Convert a state to a LinkedAsset

```
Create /Game/AI/SharedCombat as a separate StateTree with a Root, Engage, and
Retreat. Then in /Game/AI/PatrolAI, change the Combat state to a LinkedAsset
referencing /Game/AI/SharedCombat. Compile both and save.
```

---

### 4. Re-point a LinkedSubtree

```
In /Game/AI/PatrolAI, the Combat state is currently linked to the Patrol subtree.
Re-point it to a different subtree under Root named Idle. Compile and save.
```

---

### 5. Re-point a LinkedAsset

```
In /Game/AI/PatrolAI, the Combat state is a LinkedAsset pointing at
/Game/AI/SharedCombat. Re-point it to /Game/AI/AlternateCombat instead.
Compile and save.
```

---

### 6. Verify linking is non-destructive

```
The Combat state in /Game/AI/PatrolAI has tasks, considerations, and
transitions configured. Convert it to a LinkedSubtree and confirm that none
of its existing data was destroyed (it should be preserved on the state object).
Then convert it back to a regular State.
```

---

### 7. Set a state tag on a linked state

```
In /Game/AI/PatrolAI, the Combat LinkedSubtree state should be tagged with
the gameplay tag "AI.Behavior.Combat". Set the tag and verify it was applied.
Compile and save.
```
