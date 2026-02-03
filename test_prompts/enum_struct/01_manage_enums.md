# Enum Lifecycle Tests

Tests for creating, editing, and deleting user-defined enums. Run sequentially. If an enum already exists, delete it silently and try again.

---

## Create Enum

Create a new user-defined enum named E_TestState in /Game/Enums.

---

## Add Values

Add the following values to E_TestState in this order: Idle, Walking, Running, Jumping.

---

## Enum Info

Show me full info for E_TestState, including all values and indices.

---

## Rename Value

Rename value Walking to Jogging.

---

## Remove Value

Remove the value Jumping.

---

## Verify

Show me E_TestState again and confirm it contains Idle, Jogging, Running only.

---

## Delete Enum

Delete E_TestState.

---

## Confirm Deletion

Verify E_TestState no longer exists.

