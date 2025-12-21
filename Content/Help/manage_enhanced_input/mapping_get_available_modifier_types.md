# mapping_get_available_modifier_types

Get a list of all available input modifier types.

## Parameters

None required.

## Examples

### Get Available Modifiers
```json
{
  "Action": "mapping_get_available_modifier_types",
  "ParamsJson": "{}"
}
```

## Returns

```json
{
  "Success": true,
  "ModifierTypes": [
    {
      "Name": "Negate",
      "Description": "Inverts the input value"
    },
    {
      "Name": "DeadZone",
      "Description": "Applies a dead zone to analog input",
      "Settings": ["LowerThreshold", "UpperThreshold", "Type"]
    },
    {
      "Name": "Scalar",
      "Description": "Multiplies input by a scalar value",
      "Settings": ["Scalar"]
    },
    {
      "Name": "SwizzleInputAxisValues",
      "Description": "Reorders input axis components",
      "Settings": ["Order"]
    },
    {
      "Name": "Smooth",
      "Description": "Smooths input over time",
      "Settings": ["SmoothingMethod", "Speed"]
    },
    {
      "Name": "ResponseCurve",
      "Description": "Applies a response curve to input",
      "Settings": ["CurveType", "CurveExponent"]
    },
    {
      "Name": "FOVScaling",
      "Description": "Scales input based on field of view"
    },
    {
      "Name": "ToWorldSpace",
      "Description": "Transforms input to world space coordinates"
    }
  ],
  "Count": 8
}
```

## Tips

- Negate: Essential for backward/left movement from single keys
- DeadZone: Prevents drift from gamepad sticks
- Swizzle: Converts 1D input to specific axis of 2D action
- Scalar: Adjusts sensitivity
- Smooth: Reduces jitter in analog input
