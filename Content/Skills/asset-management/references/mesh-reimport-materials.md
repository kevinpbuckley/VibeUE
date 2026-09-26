# Static mesh reimport: preserve section material mappings

`component.get_material(0)` reports an override, not the material used by every
render section. After `StaticMeshEditorSubsystem.import_lod`, the importer can
append a slot. Restoring an old `static_materials` array can leave all sections
pointing at a removed slot. A one-slot mesh with section indices of 1 can show
gray/default shading even though slot 0 has the intended material.

Use the adjacent `scripts/mesh_material_slots.py` helpers through
`execute_python_code` (resolve this skill's installed directory first):

```python
import unreal
import runpy
helpers = runpy.run_path(str(skill_directory / 'scripts/mesh_material_slots.py'))
state = helpers['describe_sections']('/Game/Models/SM_Surface')
print(state)
# Only for a deliberately single-material surface, after loading its material:
changes = [dict(lod=s['lod'], section=s['section'], slot=0) for s in state['sections']]
print(helpers['remap_sections']('/Game/Models/SM_Surface', changes, save=True))
```

For multi-material assets, snapshot every LOD/section assignment and the original
and imported slot names before reimport. Resolve the new slots by unique names;
stop on missing/ambiguous names or changed section structure instead of flattening
the mesh to slot 0. Restore/remap sections after restoring the slot array. Preserve
collision assets, collision trace mode, LOD screen sizes, transforms and actor
overrides. Mesh reimport is not a reason to replace level lighting.

Section tools are editor-only and fail/return empty results during PIE. End PIE
as a necessary repair step when authorized; do not treat an empty PIE query as
proof there are no LODs. The helpers refuse PIE and preflight all requested mappings
before mutation. They do not start gameplay or save unrelated packages.

A timed-out MCP request may still be importing: consult the matching run's
`Saved/VibeUE/Signals/python-<pid>-last.json` / log before retrying a mutation.
Distinguish completion from visual acceptance and obey the project's testing rules.
