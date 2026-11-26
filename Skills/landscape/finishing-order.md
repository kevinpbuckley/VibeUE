---
name: finishing-order
description: Order after a landscape height, weight or material change: material, grass maps, Nanite, save.
---

# Landscape finishing order after an edit

After **any** height, paint-weight or material change, finish in this order:

1. **Assign the landscape material again** (`LandscapeMaterialService.assign_material_to_landscape`, or
   `ObjectToolset.SetProperties` with `{"LandscapeMaterial": ...}`). This picks up grass types added to the material.
2. **Build the grass maps:** `LandscapeService.build_landscape_grass_maps(False)`.
3. **Build Nanite:** `LandscapeService.set_nanite_landscape(label)` (about 30 s for 1009 x 1009).
4. **Save the map.** The Nanite mesh and the grass maps live in it.

**Symptom:** grass vanishes after a material or weight edit although the material and its grass types are still assigned.
The material change threw the baked grass maps away; until step 2 runs, grass silently draws nothing.
