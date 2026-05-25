# Blueprint Interface Tests

Tests for adding and removing Blueprint Interfaces on a Blueprint class
(`add_interface` / `remove_interface`). Run sequentially. If an asset already
exists, delete it silently and recreate it.

---

## Setup

Create a Blueprint Interface asset called BPI_Interactable in the Interfaces folder.
Give it one function called Interact. Save it.

---

Create an actor blueprint called InterfaceTest in the Blueprints folder.

---

## Add an Interface (full path)

Add the BPI_Interactable interface to InterfaceTest using its full asset path.

---

Show me the implemented interfaces on InterfaceTest and confirm BPI_Interactable is listed.

---

## Idempotency

Add the same BPI_Interactable interface to InterfaceTest again. It should succeed without
creating a duplicate.

---

Confirm BPI_Interactable still appears exactly once.

---

## Implement the interface function

The interface has an Interact function. Override it on InterfaceTest so it shows up in the graph,
then compile and save the blueprint.

---

## Add an Interface (short name)

Make a second actor blueprint called InterfaceTest2 in the Blueprints folder.

---

The BPI_Interactable asset is already loaded, so add it to InterfaceTest2 using just the short
name "BPI_Interactable" (no path). Confirm it was added.

---

## Remove an Interface

Remove the BPI_Interactable interface from InterfaceTest. Compile and save.

---

Verify BPI_Interactable is no longer listed on InterfaceTest.

---

## Error handling

Try to add an interface called BPI_DoesNotExist to InterfaceTest2. This should fail gracefully
with a clear message rather than crashing — report what happened.

---

Now try to add InterfaceTest2 itself (a regular actor blueprint, NOT an interface) as an interface
on InterfaceTest. This must fail gracefully and return false — it must NOT crash the editor.
Report what happened.

---

Try to remove BPI_Interactable from InterfaceTest a second time (it was already removed). Report
the result.

---

## Cleanup

Delete InterfaceTest, InterfaceTest2, and BPI_Interactable.
