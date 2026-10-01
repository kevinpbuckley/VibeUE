"""Unreal editor helpers for explicit, non-guessing LOD section material repair.

Load with runpy.run_path from this installed skill directory. No import-time edits.
Works with UE's existing Python API; no VibeUE binary rebuild is required.
"""
import unreal


def _context(mesh):
    if unreal.EditorLevelLibrary.get_pie_worlds(False):
        raise RuntimeError('Stop Play mode before using static mesh section tools')
    if isinstance(mesh, str):
        mesh = unreal.load_asset(mesh)
    if not isinstance(mesh, unreal.StaticMesh):
        raise ValueError('Expected a loaded StaticMesh or its asset path')
    return mesh, unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def describe_sections(mesh):
    """Read the section mappings, not merely component/material slot 0."""
    mesh, editor = _context(mesh)
    slots = list(mesh.get_editor_property('static_materials'))
    sections = []
    for lod in range(editor.get_lod_count(mesh)):
        for section in range(mesh.get_num_sections(lod)):
            index = editor.get_lod_material_slot(mesh, lod, section)
            valid = 0 <= index < len(slots)
            material = slots[index].get_editor_property('material_interface') if valid else None
            sections.append({'lod': lod, 'section': section, 'slot': index,
                             'valid_index': valid,
                             'material': material.get_path_name() if material else None})
    return {'asset': mesh.get_path_name(), 'slots': [
        {'index': i, 'name': str(slot.get_editor_property('material_slot_name')),
         'imported_name': str(slot.get_editor_property('imported_material_slot_name'))}
        for i, slot in enumerate(slots)], 'sections': sections}


def remap_sections(mesh, assignments, save=False):
    """Apply explicit {lod, section, slot} entries after checking the whole request.

    No fallback-to-zero policy, slot-array replacement, automatic PIE stop, actor
    edits or automatic save. Multi-material intent must be resolved by the caller.
    """
    mesh, editor = _context(mesh)
    state = describe_sections(mesh)
    known = {(s['lod'], s['section']): s['slot'] for s in state['sections']}
    slots = list(mesh.get_editor_property('static_materials'))
    changes, seen = [], set()
    for entry in assignments:
        lod, section, slot = (entry[key] for key in ('lod', 'section', 'slot'))
        if any(type(value) is not int for value in (lod, section, slot)):
            raise ValueError('LOD, section and slot must be integers')
        key = (lod, section)
        if key not in known or key in seen:
            raise ValueError('Unknown or repeated section: ' + str(key))
        if not 0 <= slot < len(slots) or not slots[slot].get_editor_property('material_interface'):
            raise ValueError('Target slot is missing or has no material: ' + str(slot))
        seen.add(key)
        changes.append({'lod': lod, 'section': section, 'before': known[key], 'slot': slot})
    with unreal.ScopedEditorTransaction('Remap static mesh section materials'):
        mesh.modify()
        for entry in changes:
            editor.set_lod_material_slot(mesh, entry['slot'], entry['lod'], entry['section'])
        if save and not unreal.EditorAssetLibrary.save_loaded_asset(mesh):
            raise RuntimeError('Material mappings changed in memory but asset save failed')
    return {'asset': mesh.get_path_name(), 'changes': changes, 'saved': bool(save)}
