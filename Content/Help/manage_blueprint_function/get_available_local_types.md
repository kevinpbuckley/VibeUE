# get_available_local_types

Get a list of all available types that can be used for local variables in functions.

## Aliases

- `list_local_types`

## Parameters

None required. This action doesn't need any parameters.

## Examples

### Get Available Types
```json
{
  "Action": "get_available_local_types"
}
```

### Using Alias
```json
{
  "Action": "list_local_types"
}
```

## Returns

Complete list of available types organized by category:
```json
{
  "success": true,
  "types": [
    {
      "descriptor": "bool",
      "display_name": "Boolean",
      "category": "basic",
      "notes": "True/false value"
    },
    {
      "descriptor": "int",
      "display_name": "Integer",
      "category": "basic",
      "notes": "32-bit signed integer"
    },
    {
      "descriptor": "float",
      "display_name": "Float",
      "category": "basic",
      "notes": "Single-precision floating point"
    },
    {
      "descriptor": "struct:Vector",
      "display_name": "Vector",
      "category": "struct",
      "notes": "3D vector (X,Y,Z)"
    },
    {
      "descriptor": "object:Actor",
      "display_name": "Actor",
      "category": "object",
      "notes": "Reference to AActor"
    }
  ],
  "count": 35,
  "usage": "Use descriptors directly or wrap with array<...> for arrays."
}
```

## Type Categories

### Basic Types
- `bool` - Boolean (true/false)
- `byte` - Unsigned byte (0-255)
- `int` - 32-bit signed integer
- `int64` - 64-bit signed integer
- `float` - Single-precision floating point
- `double` - Double-precision floating point
- `string` - UTF-16 string
- `name` - Name identifier
- `text` - Localized text

### Struct Types
- `struct:Vector` - 3D vector (X,Y,Z)
- `struct:Vector2D` - 2D vector (X,Y)
- `struct:Rotator` - Rotation (Pitch/Yaw/Roll)
- `struct:Transform` - Location, rotation, scale
- `struct:LinearColor` - RGBA color (0-1 range)

### Object Types
- `object:Actor` - Reference to Actor
- `object:StaticMesh` - Reference to Static Mesh asset
- `object:Material` - Reference to Material asset

### Class Types
- `class:Actor` - TSubclassOf<AActor> reference

## Usage

Use the `descriptor` value directly when creating local variables:

```json
{
  "Action": "add_local_variable",
  "ParamsJson": {
    "blueprint_name": "BP_Player",
    "function_name": "CalculateHealth",
    "local_name": "MyVector",
    "type": "struct:Vector"
  }
}
```

For arrays, wrap the descriptor:
```json
{
  "type": "array<float>"
}
```

## Notes

- Call this action first to discover available types before adding local variables
- The `descriptor` field contains the exact string to use in `type` parameter
- Categories help organize types: basic, struct, object, class, interface
- Most common types have simple aliases (int, float, bool) that work without prefixes
- For object/struct types, use the full descriptor format shown in the results
