# Struct Lifecycle Tests

Tests for creating, editing, and deleting user-defined structs. Run sequentially. If a struct already exists, delete it silently and try again.

---

## Create Struct

Create a new user-defined struct named F_TestStats in /Game/Structs.

---

## Add Properties

Add properties to F_TestStats:
- Health (float) default 100
- Stamina (float) default 50
- IsAlive (bool) default true
- DisplayName (FString) default "Test Subject"

---

## Struct Info

Show me full info for F_TestStats including property types, default values, and GUIDs.

---

## Rename Property

Rename DisplayName to Name.

---

## Change Property Type

Change Stamina to type int32.

---

## Set Default Value

Set default value for Health to 125.

---

## Remove Property

Remove IsAlive.

---

## Verify

Show F_TestStats again and confirm properties are Health (float, default 125), Stamina (int32), Name (FString).

---

## Delete Struct

Delete F_TestStats.

---

## Confirm Deletion

Verify F_TestStats no longer exists.

