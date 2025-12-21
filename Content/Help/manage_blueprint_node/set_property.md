# set_property

Set a property value directly on a node (like default pin values).

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | Yes | Content path to the Blueprint |
| GraphName | string | Yes | Name of the graph |
| NodeId | string | Yes | ID of the node to modify |
| PropertyName | string | Yes | Name of the property or pin to set |
| Value | any | Yes | Value to set |

## Examples

### Set Print String Default Value
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeId\": \"PrintString_0\", \"PropertyName\": \"InString\", \"Value\": \"Player Spawned!\"}"
}
```

### Set Boolean Default
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"EventGraph\", \"NodeId\": \"PrintString_0\", \"PropertyName\": \"bPrintToScreen\", \"Value\": true}"
}
```

### Set Numeric Value
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\", \"GraphName\": \"TakeDamage\", \"NodeId\": \"Multiply_0\", \"PropertyName\": \"B\", \"Value\": 2.5}"
}
```

## Returns

```json
{
  "Success": true,
  "BlueprintPath": "/Game/Blueprints/BP_Player",
  "GraphName": "EventGraph",
  "NodeId": "PrintString_0",
  "PropertyName": "InString",
  "OldValue": "Hello",
  "NewValue": "Player Spawned!",
  "Message": "Property set successfully"
}
```

## Tips

- This sets default values on unconnected pins
- Connected pins use the wire's value, not the default
- Use `details` action to find property/pin names
- Value type must match the pin type
- Compile after setting properties to validate
