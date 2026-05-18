# StateTree Tests — Considerations (Utility AI)

Considerations are the utility-AI scoring inputs StateTree uses to rank candidate
states when a parent's selection behavior is utility-based. Each consideration
contributes a scalar [0,1] weight that the engine multiplies into the state's
final utility score.

---

### 1. List available consideration types

```
Show me every consideration struct I can add to a state in /Game/AI/BasicMovement —
whatever's registered with the engine.
```

---

### 2. Add a constant consideration

```
In /Game/AI/BasicMovement, add a Constant consideration to the Walking state
with a value of 0.5. Compile and save.
```

---

### 3. Add multiple considerations on one state

```
In /Game/AI/PatrolAI, on the Combat state, add:
- a Constant consideration weighted at 0.7
- a Float Curve consideration on a parameter named "EnemyDistance"
Compile and save.
```

---

### 4. Inspect consideration properties

```
What properties can I set on the second consideration of the Combat state
in /Game/AI/PatrolAI? Return their names and types.
```

---

### 5. Set a consideration property

```
On the Constant consideration of the Walking state in /Game/AI/BasicMovement,
change its value from 0.5 to 0.85. Compile and save.
```

---

### 6. Bind a consideration property to a root parameter

```
In /Game/AI/PatrolAI, the Combat state has a Float Curve consideration.
Bind its input value to the root parameter "EnemyDistance" so the curve
reads from that parameter at runtime. Compile and save.
```

---

### 7. Bind a consideration property to context

```
In /Game/AI/PatrolAI, on the Combat state's first consideration, bind its
"Owner" input to the context actor. Compile and save.
```

---

### 8. Unbind a consideration property

```
In /Game/AI/PatrolAI, the Combat state's first consideration has a binding on
its "Owner" input. Remove that binding so it falls back to the literal default.
Compile and save.
```

---

### 9. Remove a consideration

```
In /Game/AI/PatrolAI, remove the second consideration from the Combat state.
Compile and save.
```

---

### 10. Utility-driven selection end-to-end

```
Build /Game/AI/UtilityDemo with three sibling states under Root: Idle, Patrol, Combat.
Set Root's selection behavior to utility-based. On each child:
- Idle: Constant consideration value 0.3
- Patrol: Constant consideration value 0.6
- Combat: Constant consideration value 0.9
Verify by inspection that Combat now wins selection. Compile and save.
```
