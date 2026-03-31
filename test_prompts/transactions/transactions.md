# Transaction Tests

Test undo, redo, transaction grouping, cancel, property changes, history inspection, and buffer reset. Run sequentially.

---

## Setup

Delete any actors in the current level that start with "Txn_". We want a clean slate.

---

## Spawn and Undo

Spawn a static mesh actor called Txn_Cube at the origin.

---

Now undo that last operation using the transaction service.

---

Verify Txn_Cube no longer exists in the level. List all actors starting with "Txn_".

---

## Redo

Redo the last undo so Txn_Cube comes back.

---

Confirm Txn_Cube is back in the level.

---

## Multiple Undo / Redo

Spawn Txn_Sphere at 500, 0, 0.

---

Spawn Txn_Cylinder at 0, 500, 0.

---

How many undoable transactions are there? Show me the undo description for the next undo.

---

Undo the last 2 operations at once. Then list all actors starting with "Txn_" — only the cube should remain.

---

Redo both undone operations. All three actors (Cube, Sphere, Cylinder) should be back.

---

Confirm all three exist.

---

## Transaction Grouping

Begin a transaction called "Build Test Wall". Then spawn three static mesh actors:
- Txn_WallLeft at 0, 0, 200
- Txn_WallCenter at 200, 0, 200
- Txn_WallRight at 400, 0, 200

End the transaction after all three are spawned.

---

Confirm all three wall actors exist.

---

Undo one time. All three wall actors should be gone since they were grouped into a single transaction.

---

List actors starting with "Txn_" — the three originals (Cube, Sphere, Cylinder) should remain but no wall pieces.

---

Redo. All three wall pieces should come back.

---

## Grouped Room Build

Begin a transaction called "Build Test Room". Inside it:
1. Spawn Txn_Floor at 200, 0, -100
2. Spawn Txn_Ceiling at 200, 0, 400
3. Spawn Txn_DoorFrame at 200, -100, 150

End the transaction.

---

How many total actors start with "Txn_" now? Should be 9.

---

Undo once. The room pieces (Floor, Ceiling, DoorFrame) should be gone but everything else remains.

---

What is the undo description right now? It should reference the wall build.

---

What is the redo description? It should say "Build Test Room".

---

Redo the room so all 9 actors are back.

---

## Property Change Undo

Spawn a point light called Txn_Light at 0, 0, 300.

---

Set Txn_Light's intensity to 50000.

---

Confirm the intensity is 50000.

---

Undo the last operation.

---

What is Txn_Light's intensity now? It should be back to the default.

---

## Cancel Transaction

Begin a transaction called "Dangerous Changes". Then:
1. Spawn Txn_BadActor at 100, 100, 0
2. Set Txn_Light intensity to 999999

Now cancel the transaction instead of ending it.

---

Verify Txn_BadActor does NOT exist and Txn_Light's intensity is at its previous value (not 999999).

---

## History Inspection

Show me the full undo history (last 20). I want to see all the transaction titles.

---

How many undoable transactions are there? Can we undo? Can we redo?

---

Undo once. Now show me both undo and redo history. The last undone operation should appear in redo history.

---

What's the redo count? Should be 1.

---

Redo it back.

---

## Reset Buffer

Reset the transaction history with reason "Test cleanup".

---

Confirm: can_undo should be false, can_redo should be false, undo count should be 0, redo count should be 0.

---

List actors starting with "Txn_". They should all still exist — reset clears history, not the world.

---

## Cleanup

Delete all actors starting with "Txn_".
