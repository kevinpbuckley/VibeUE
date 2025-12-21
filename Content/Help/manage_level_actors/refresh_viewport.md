# refresh_viewport

Refresh the viewport to display any recent changes.

## Parameters

None required.

## Examples

### Refresh Viewport
```json
{
  "Action": "refresh_viewport",
  "ParamsJson": "{}"
}
```

## Returns

```json
{
  "Success": true,
  "Message": "Viewport refreshed"
}
```

## Tips

- Use after batch modifications to update the view
- Some changes may not be visible until viewport refresh
- Equivalent to clicking in the viewport
