---
name: distance-breakage
description: Foliage breaking at distance: leaf distance masks, wind stretching instances, solid masked cards.
---

# Foliage that breaks at distance

- **Far LODs lose their leaves:** check the material's distance mask parameters (for example an `Alpha Mult Distance`
  that fades the opacity mask), not only the mesh LODs.
- **Instances stretched into columns:** pivot-painter wind (world position offset) can stretch instances, above all with
  non-uniform scale. Turn off `evaluate_world_position_offset` on the foliage type.
- **Masked cards drawn solid:** beyond `nanite_pixel_programmable_distance` a masked Nanite card is drawn without its
  mask. Fine for a full crown; a type whose leaves are masked off (bare branches) becomes solid shapes. Keep 0 for such a
  type.
