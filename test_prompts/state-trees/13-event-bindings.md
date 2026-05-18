# StateTree Tests — Delegate Transitions, Event Payloads, Cross-Bindings

Coverage for the "advanced wiring" surface of StateTreeService:

- Transitions triggered by delegates broadcast from tasks
- Transition conditions reading event payload fields
- Cross-bindings between tasks, global tasks, and conditions
- Unbind variants (mirror of every Bind* method)

---

### 1. Bind a transition to a task delegate

```
In /Game/AI/PatrolAI, the Patrol state has a task that exposes a
"OnPatrolComplete" delegate. Add a transition from Patrol to Idle that
fires when this delegate broadcasts. Compile and save.
```

---

### 2. Inspect event payload property names

```
The Patrol state in /Game/AI/PatrolAI has a transition that listens for an
event named "PatrolFinished". What property names are available on the
event's payload struct? Return them so I can bind a condition to one.
```

---

### 3. Bind a transition condition to an event payload field

```
On the Patrol → Idle transition in /Game/AI/PatrolAI, add a condition that
fires only when the "PatrolFinished" event's payload `bSuccess` field is true.
Compile and save.
```

---

### 4. Bind a task property to a global task property

```
In /Game/AI/PatrolAI, there's a global task "WorldStateTracker" with a
"CurrentTarget" property. The Combat state's task has an "Enemy" input.
Bind the Combat task's "Enemy" to the global task's "CurrentTarget" so it
reads the world state every tick. Compile and save.
```

---

### 5. Bind an enter-condition to a global task property

```
In /Game/AI/PatrolAI, the Combat state's enter condition checks whether
combat should start. Bind the condition's "Threshold" property to the
global task "WorldStateTracker"'s "AggroRadius" output. Compile and save.
```

---

### 6. Bind an evaluator property to context

```
In /Game/AI/PatrolAI, the "EnemyDetector" evaluator has a "Pawn" input.
Bind it to the context actor so it operates on whichever pawn owns the
StateTree at runtime. Compile and save.
```

---

### 7. Bind a global-task property to a root parameter

```
In /Game/AI/PatrolAI, the global task "WorldStateTracker" has an
"AggroRadius" input. Bind it to the root parameter of the same name so
designers can tune the radius from the placed actor. Compile and save.
```

---

### 8. Unbind every Bind* surface

For each binding made in tests 1–7, verify that an Unbind call removes
exactly that binding without disturbing the others.

```
List every binding currently configured on /Game/AI/PatrolAI. Then unbind
the Combat task's "Enemy" → global "CurrentTarget" link, and re-list to
confirm the others survive.
```

---

### 9. Detailed task property setter (typed write)

```
In /Game/AI/PatrolAI, set the Patrol task's "WaitTime" property to a strongly
typed Float value of 3.5 (not a stringified value). Use the detailed setter
that takes a value-type tag plus a value. Compile and save.
```

---

### 10. Read a task's current property value

```
What is the Patrol task's "WaitTime" set to right now in /Game/AI/PatrolAI?
Return the literal value plus, if it's a binding, the source path.
```
