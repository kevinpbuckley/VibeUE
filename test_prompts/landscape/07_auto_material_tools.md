# Auto-Material Tool Tests

Tests for the new landscape auto-material workflow tools. Run sequentially.

---

## Skill + Preflight

Load the `landscape-auto-material` skill.

---

Does `/Game/Real_Landscape` exist?

---

Does a landscape named `AutoMatTerrain` already exist?

---

If `AutoMatTerrain` does not exist, create a landscape at origin named `AutoMatTerrain` with default settings.

---

## Texture Discovery

Run `FindLandscapeTextures` on `/Game/Real_Landscape` with no filter. Report how many texture sets were found.

---

Run `FindLandscapeTextures` on `/Game/Real_Landscape` with filter `Grass`. Report matches.

---

Run `FindLandscapeTextures` on `/Game/Real_Landscape` with filter `Rock`. Report matches.

---

Run `FindLandscapeTextures` on `/Game/DoesNotExist` with no filter. Confirm graceful failure or empty results (no crash).

---

## Build Auto Material From Discovered Textures

Create an auto-material named `M_AutoLandscape_Test` in `/Game/LandscapeAutoTest/Materials` using 3 discovered layers (Grass, Rock, Dirt or closest available names).

---

Assign the new material to `AutoMatTerrain` during creation if supported.

---

Create layer info assets in `/Game/LandscapeAutoTest/LayerInfos` for each generated layer.

---

Verify `/Game/LandscapeAutoTest/Materials/M_AutoLandscape_Test` exists.

---

Get a summary of `M_AutoLandscape_Test` and report expression count and landscape-layer related nodes.

---

Compile and save `M_AutoLandscape_Test`.

---

## Validation + Failure Cases

Attempt to create `M_AutoLandscape_Test` again in the same path. Confirm duplicate-name handling returns a clear error.

---

Get landscape layer info for `AutoMatTerrain` and confirm generated layers are present.

---

Paint one generated layer at `(0, 0)` with full strength and radius `1500`, then read back layer weights at `(0, 0)`.

---

Paint a second generated layer at `(3000, 0)` with full strength and radius `1500`, then read back layer weights at `(3000, 0)`.

---

Return a final pass/fail checklist for:
- texture discovery
- auto-material creation
- layer info creation
- landscape assignment
- duplicate protection
