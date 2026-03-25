# manage_asset — save_all

Save all dirty (modified but unsaved) assets in the project.

## Parameters

None required.

## Notes

- Useful after a sequence of programmatic edits across multiple assets.
- Reports how many assets were saved.
- Does not save maps/levels — those require explicit save via Editor UI or `EditorLevelLibrary`.

## Examples

```
manage_asset(action="save_all")
```

## Response

```json
{
  "success": true,
  "saved_count": 4,
  "message": "Saved 4 dirty assets"
}
```
