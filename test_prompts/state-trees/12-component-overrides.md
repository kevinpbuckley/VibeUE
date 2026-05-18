# StateTree Tests — Component Parameter Overrides

When a placed actor has a `StateTreeComponent` that points at a tree, each
instance of that actor can override the tree's exposed root parameters
without forking the asset. These tests cover reading the link and the
per-instance override path.

---

### 1. Read which StateTree an actor uses

```
The level has an actor labeled "BP_Cube_1". What StateTree asset is its
StateTreeComponent referencing? Return the full path.
```

---

### 2. List current overrides on an actor

```
For the actor "BP_Cube_1" in the level, list every parameter override
its StateTreeComponent has set, with values.
```

---

### 3. Set a numeric parameter override

```
On the actor labeled "BP_Cube_1", override the StateTreeComponent's
"IdlingTime" parameter to 4.5 — leaving every other instance using the
asset's default. Verify the override took.
```

---

### 4. Set a vector parameter override

```
On "BP_Cube_2", override the StateTreeComponent parameter "PatrolHome"
to (X=500, Y=200, Z=0). Verify by reading it back.
```

---

### 5. Override propagates only to the target actor

```
Confirm that setting "IdlingTime" to 4.5 on BP_Cube_1 has no effect on
BP_Cube_2's override or on the underlying asset's default. List both
overrides and the asset default to demonstrate isolation.
```

---

### 6. Clear an override (revert to asset default)

```
On BP_Cube_1, remove the per-instance override on "IdlingTime" so it falls
back to whatever the underlying StateTree asset defines. Verify the
override is gone and the runtime value matches the asset default.
```

---

### 7. Bulk-apply an override to many actors

```
Every actor in the level with a StateTreeComponent that references
/Game/AI/PatrolAI should have its "PatrolSpeed" parameter set to 250.0.
Apply this and report how many actors were updated.
```
