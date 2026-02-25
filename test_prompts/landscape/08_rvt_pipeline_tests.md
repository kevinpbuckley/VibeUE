# Runtime Virtual Texture Pipeline Tests

Tests for the new Runtime Virtual Texture service tools. Run sequentially.

---

## Skill + Preflight

Load the `landscape-auto-material` skill.

---

Verify a landscape named `AutoMatTerrain` exists. If not, create it at origin with default settings.

---

Verify `/Game/LandscapeAutoTest/Materials/M_AutoLandscape_Test` exists. If not, create a basic landscape material and assign it to `AutoMatTerrain`.

---

## RVT Asset Creation

Create an RVT asset named `RVT_AutoTest` in `/Game/LandscapeAutoTest/RVT` with:
- material type: `BaseColor_Normal_Roughness`
- tile count: `256`
- tile size: `256`
- tile border size: `4`
- continuous update: `true`
- single physical space: `false`

---

Verify `/Game/LandscapeAutoTest/RVT/RVT_AutoTest` exists.

---

Get runtime virtual texture info for `RVT_AutoTest` and report all fields.

---

## RVT Volume + Landscape Assignment

Create an RVT volume for `AutoMatTerrain` using `RVT_AutoTest` with actor label `RVT_Vol_AutoTest`.

---

Verify the RVT volume actor exists in the level and report its label/name.

---

Assign `RVT_AutoTest` to `AutoMatTerrain` at slot `0`.

---

Read back landscape RVT slots and confirm slot `0` references `RVT_AutoTest`.

---

## Additional RVT Type

Create a second RVT asset named `RVT_AutoTest_WorldHeight` in `/Game/LandscapeAutoTest/RVT` with material type `WorldHeight`.

---

Get runtime virtual texture info for `RVT_AutoTest_WorldHeight` and confirm material type is `WorldHeight`.

---

Assign `RVT_AutoTest_WorldHeight` to `AutoMatTerrain` at slot `1`.

---

Read back landscape RVT slots and confirm slot `1` references `RVT_AutoTest_WorldHeight`.

---

## Failure Cases

Attempt to create `RVT_AutoTest` again in `/Game/LandscapeAutoTest/RVT`. Confirm duplicate-name handling returns a clear error.

---

Attempt to create an RVT volume for non-existent landscape `NoSuchLandscape`. Confirm it fails gracefully.

---

Attempt to assign RVT to non-existent landscape `NoSuchLandscape` at slot `0`. Confirm it returns false/error without crashing.

---

Attempt to get RVT info for invalid path `/Game/Nope/NoRVT`. Confirm clear error text.

---

Return a final pass/fail checklist for:
- RVT asset creation
- RVT info introspection
- RVT volume creation
- landscape slot assignment
- invalid input handling
