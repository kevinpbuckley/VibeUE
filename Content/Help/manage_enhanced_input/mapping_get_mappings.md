# mapping_get_mappings

Get all key mappings in an Input Mapping Context.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ContextPath | string | Yes | Content path to the Input Mapping Context |

## Examples

### Get All Mappings
```json
{
  "Action": "mapping_get_mappings",
  "ParamsJson": "{\"ContextPath\": \"/Game/Input/IMC_Default\"}"
}
```

## Returns

```json
{
  "Success": true,
  "ContextPath": "/Game/Input/IMC_Default",
  "Mappings": [
    {
      "Index": 0,
      "ActionPath": "/Game/Input/IA_Move",
      "Key": "W",
      "Modifiers": ["Swizzle Input Axis Values"],
      "Triggers": []
    },
    {
      "Index": 1,
      "ActionPath": "/Game/Input/IA_Move",
      "Key": "S",
      "Modifiers": ["Swizzle Input Axis Values", "Negate"],
      "Triggers": []
    },
    {
      "Index": 2,
      "ActionPath": "/Game/Input/IA_Jump",
      "Key": "SpaceBar",
      "Modifiers": [],
      "Triggers": ["Pressed"]
    }
  ],
  "Count": 3
}
```

## Tips

- Index is used to reference specific mappings when modifying
- Multiple mappings can exist for the same action (different keys)
- Shows which modifiers and triggers are applied to each mapping
