---
name: distance-artifacts
description: Landscape seams and artifacts at distance: normals at mesh edges, height-blend lattices, object tilings.
---

# Seams and artifacts that show at distance

- **A lit line where the landscape meets a surrounding mesh** (skirt, cliff, vista ring) with matching base colour is
  usually the **normals**, not colour: recomputed normals on the mesh average in a buried lip. Give the mesh's edge ring
  the landscape's border normals. Compare buffers first (`ShowFlag.VisualizeBuffer 1` + `r.BufferVisualizationTarget
  WorldNormal` / `BaseColor`) before touching colour or material.
- **A dotted lattice on height-blended layers far away:** a small scan's height still wins per tile. Fade the blend
  height towards low-frequency noise with distance.
- **Detail tilings that contain objects** (roots, sticks, stones) must not be reused as the "mid" scale: the objects
  repeat in a visible grid.
