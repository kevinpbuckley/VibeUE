# mapping_get_available_trigger_types

Get a list of all available input trigger types.

## Parameters

None required.

## Examples

### Get Available Triggers
```json
{
  "Action": "mapping_get_available_trigger_types",
  "ParamsJson": "{}"
}
```

## Returns

```json
{
  "Success": true,
  "TriggerTypes": [
    {
      "Name": "Down",
      "Description": "Fires every frame while input is held"
    },
    {
      "Name": "Pressed",
      "Description": "Fires once when input is first pressed"
    },
    {
      "Name": "Released",
      "Description": "Fires once when input is released"
    },
    {
      "Name": "Hold",
      "Description": "Fires after input is held for a duration",
      "Settings": ["HoldTimeThreshold", "bIsOneShot"]
    },
    {
      "Name": "HoldAndRelease",
      "Description": "Fires on release if held long enough",
      "Settings": ["HoldTimeThreshold"]
    },
    {
      "Name": "Tap",
      "Description": "Fires if input is quickly pressed and released",
      "Settings": ["TapReleaseTimeThreshold"]
    },
    {
      "Name": "Pulse",
      "Description": "Fires repeatedly while held",
      "Settings": ["Interval", "TriggerLimit"]
    },
    {
      "Name": "ChordAction",
      "Description": "Requires another action to be active",
      "Settings": ["ChordAction"]
    },
    {
      "Name": "ComboAction",
      "Description": "Requires actions in sequence",
      "Settings": ["ComboActions", "ComboTimeLimit"]
    }
  ],
  "Count": 9
}
```

## Tips

- Down: Default behavior, fires continuously
- Pressed/Released: For one-shot actions like jump
- Hold: For charged attacks or hold-to-interact
- Tap: For quick actions like dodging
- Pulse: For auto-fire weapons
- ChordAction: For modifier keys (Shift+Key, Ctrl+Key)
