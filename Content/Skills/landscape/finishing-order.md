# Landscape finishing order after an edit

> Sourced from [issue #707](https://github.com/kevinpbuckley/VibeUE/issues/707), a one-week
> agent-work review on UE 5.9 (branch `5-9`). Confirm the steps and tool names against your
> target engine version before relying on them.

After **any** height, paint-weight, or material change, finish in this order:

1. **Re-assign the landscape material** — this is what picks up newly added grass types.
2. **Build grass maps** — a material change discards the baked grass maps; until they are rebuilt,
   grass silently draws nothing.
3. **Build Nanite** for the landscape.
4. **Save the map.**

**Symptom:** grass appears to vanish after a material edit even though the material and grass types
are still assigned. The cause is the discarded baked grass maps, not the material — do step 2.

The source report names `set_nanite_landscape` and `build_landscape_grass_maps` as the `5-9` calls
for steps 2 and 3; verify they are exposed in your build before scripting them.
