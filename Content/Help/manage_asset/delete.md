# manage_asset — delete

Delete an asset from the project.

## Parameters

| Parameter              | Type    | Required | Description                                              |
|------------------------|---------|----------|----------------------------------------------------------|
| `asset_path`           | string  | ✓        | Full Content Browser path to the asset to delete         |
| `skip_reference_check` | boolean |          | If true, skip reference check (default: false)          |

## Safety — Reference Check

By default, `delete` checks for other assets that reference the target.
If any references are found, the delete is **blocked** and the referencers are listed.

To delete anyway (e.g. you know the references are safe to break):
```
manage_asset(action="delete", asset_path="/Game/Old/M_Unused", skip_reference_check=true)
```

## Examples

```
# Safe delete — blocked if anything references it
manage_asset(action="delete", asset_path="/Game/Old/ST_Abandoned")

# Force delete ignoring references
manage_asset(action="delete", asset_path="/Game/Old/ST_Abandoned", skip_reference_check=true)
```

## Response (success)

```json
{ "success": true, "asset_path": "/Game/Old/ST_Abandoned" }
```

## Response (blocked by references)

```json
{
  "success": false,
  "error": "Asset '/Game/Old/ST_Abandoned' is referenced by 2 other assets. Pass skip_reference_check=true to delete anyway, or remove references first.",
  "referencers": [
    "/Game/Blueprints/BP_Enemy",
    "/Game/AI/ST_Combat"
  ]
}
```
