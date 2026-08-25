---
name: bulk-maintenance
display_name: Safe Bulk Maintenance
description: Plan, dry-run, apply, verify, roll back, and resume explicit bounded batches for Blueprint interface migration, metadata hygiene, asset naming/organization, and unused-asset cleanup preparation.
vibeue_classes:
  - WorkflowService
  - BlueprintService
  - TransactionService
keywords:
  - bulk
  - migrate interface
  - metadata hygiene
  - rename assets
  - organize assets
  - unused assets
  - cleanup
---

# Safe Bulk Maintenance

Use `unreal.WorkflowService.run_bulk_maintenance(plan_json, apply, batch_size, stop_on_error)` for an
explicit target list. It is dry-run-first, path-scoped, bounded to 100 targets per GC batch, returns
one result per target, rolls back a failed apply operation, and writes a resumable report under
`Saved/VibeUE/Bulk`.

Rules:

1. Discover candidates, but pass the final explicit `targets` array yourself. Never turn a search
   result directly into deletion.
2. Call once with `apply=False`, inspect every `would-change`/refusal, then repeat the exact plan with
   `apply=True` and the same `resume_id`.
3. Set `path_scope` so a malformed target cannot escape the intended content folder.
4. Use a small `batch_size` (10–25) for Blueprint work. Each apply target gets a named transaction;
   compile/save failure is reported as failure and rolled back.
5. Wrap an unattended task in `start_run` / `finish_run`; attach the bulk report and verification.

## 1. Blueprint interface migration

Discover implementers/referencers with GameIQ when available, compile the candidates before the
change, and migrate in two reviewed phases:

```python
import json, unreal
plan = {
  "resume_id": "combat-interface-v2",
  "operation": "interface_add",
  "path_scope": "/Game/Combat",
  "interface": "/Game/Combat/Interfaces/BPI_CombatV2",
  "targets": ["/Game/Combat/BP_Rifle", "/Game/Combat/BP_Sword"]
}
print(unreal.WorkflowService.run_bulk_maintenance(json.dumps(plan), False, 10, True))
# Review, then use True. Remove the old interface only after compile/readback proves v2 is present.
```

`interface_add` and `interface_remove` compile each Blueprint and save only verified successes.

## 2. Blueprint metadata hygiene

Descriptions are caller-provided intent; the service never invents text. Existing authored metadata
is preserved unless that specific target says `"overwrite": true`.

```json
{"resume_id":"combat-metadata-1","operation":"variable_metadata","path_scope":"/Game/Combat","targets":[
  {"asset":"/Game/Combat/BP_Rifle","variable":"Ammo","category":"Weapon|Ammo","description":"Rounds remaining in the current magazine."},
  {"asset":"/Game/Combat/BP_Sword","variable":"Damage","category":"Weapon|Damage","description":"Base damage before modifiers."}
]}
```

## 3. Asset naming and organization

Audit naming separately, resolve collisions, then supply explicit source/destination pairs. Moves
outside `path_scope` and existing destinations are refused.

```json
{"resume_id":"weapon-moves-1","operation":"asset_move","path_scope":"/Game/Weapons","targets":[
  {"source":"/Game/Weapons/Rifle","destination":"/Game/Weapons/Blueprints/BP_Rifle"}
]}
```

After apply, fix redirectors with the normal Unreal workflow and re-run GameIQ references/impact.

## 4. Dead/unused cleanup preparation

Use `cleanup_review` with GameIQ `ProjectStats("unused")` plus `Impact` evidence. This operation is
deliberately report-only even in apply mode. Deletion is a separate, human-approved action using the
reviewed exact list; an unused heuristic alone is never approval.

## Partial failure and resume

Reports contain a stable `targetKey`, aggregate counts, rollback status, and `reportPath`. Repeat with
the same `resume_id`; previously successful targets are `resumed-skip`, so a crash or stop-on-error
does not duplicate completed work. Fix failed inputs, keep the same explicit scope, and rerun.
