# discover

Discover available Blueprint node types that can be created. Supports pin-type-sensitive filtering like Unreal's "drag from pin" context menu.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| blueprint_name | string | Yes | Blueprint name or path |
| search_term | string | Yes* | Search term to filter node types (*required unless using category_filter) |
| category_filter | string | No | Filter by category (e.g., "Math", "Flow Control", "Utilities") |
| class_filter | string | No | Class context for available functions (e.g., "GameplayStatics", "KismetMathLibrary") |
| max_results | number | No | Maximum number of results to return (default: 10) |
| compact | boolean | No | Return minimal fields only (default: true) |
| source_node_id | string | No | Node ID to use as pin context (for pin-type-sensitive filtering) |
| source_pin | string | No | Pin name on source_node_id to filter compatible nodes (for pin-type-sensitive filtering) |

## Pin-Type-Sensitive Discovery

When you provide both `source_node_id` and `source_pin`, discovery filters results to only show nodes that can connect to that pin - exactly like Unreal's behavior when you drag off a pin in the Blueprint editor.

**CRITICAL: Do NOT use exec pin filtering for pure functions!**

Pure math/utility functions like `Clamp`, `Lerp`, `Multiply`, `Add` have **NO execution pins**. They execute automatically when their output is read. If you filter by exec pin, you'll get irrelevant results.

### When to Use Pin Filtering
- ✅ Finding nodes that accept a data output (float, int, string, object, etc.)
- ✅ Discovering what you can do with a function's return value
- ✅ Finding compatible conversions for a specific type
- ❌ **DO NOT** use exec pin filtering for Clamp, Lerp, or other pure functions

### When NOT to Use Pin Filtering
- Searching for pure math functions (Clamp, Lerp, Add, Multiply, etc.)
- Looking for utility functions that don't have exec pins
- General node discovery without specific pin compatibility needs

### Use Cases for Pin Filtering
- Finding nodes that accept a specific output type (e.g., "what can I do with this float?")
- Discovering type-compatible nodes after getting a return value
- Filtering operations to those that match your variable type

### Example: Find Nodes That Accept a Float Output
```json
{
  "Action": "discover",
  "ParamsJson": "{\"blueprint_name\": \"BP_Example\", \"search_term\": \"print\", \"source_node_id\": \"{ABC-123}\", \"source_pin\": \"ReturnValue\"}"
}
```

This filters to only nodes with pins compatible with the ReturnValue pin's type.

## Examples

### Search for Print Nodes
```json
{
  "Action": "discover",
  "ParamsJson": "{\"blueprint_name\": \"BP_Example\", \"search_term\": \"Print\"}"
}
```

### Find Math Nodes
```json
{
  "Action": "discover",
  "ParamsJson": "{\"blueprint_name\": \"BP_Example\", \"search_term\": \"add\", \"category_filter\": \"Math\"}"
}
```

### Find Actor Functions
```json
{
  "Action": "discover",
  "ParamsJson": "{\"blueprint_name\": \"BP_Example\", \"search_term\": \"Location\", \"class_filter\": \"Actor\"}"
}
```

### Find Flow Control Nodes
```json
{
  "Action": "discover",
  "ParamsJson": "{\"blueprint_name\": \"BP_Example\", \"category_filter\": \"Flow Control\", \"search_term\": \"branch\"}"
}
```

## Returns

### Standard Response
```json
{
  "success": true,
  "descriptors": [
    {
      "spawner_key": "UKismetSystemLibrary::PrintString(UObject*,FString,bool,bool,FLinearColor,float,FName)",
      "display_name": "Print String",
      "category": "Development|Debug"
    }
  ],
  "count": 1,
  "blueprint_name": "BP_Example"
}
```

### Pin-Filtered Response
```json
{
  "success": true,
  "descriptors": [...],
  "count": 5,
  "blueprint_name": "BP_Example",
  "pin_filtered": true,
  "context_pin": "ReturnValue",
  "context_pin_type": "real"
}
```

## Tips

- **Always provide search_term** - empty searches return an error
- Use broad search terms to discover related functionality
- Categories help narrow down to specific areas (Math, Flow Control, Utilities, String, Array, Transform)
- Use `source_node_id` + `source_pin` for pin-type-sensitive filtering (like drag-from-pin)
- Keywords in results can help find related nodes
- **For math operations**: UE5's wildcard pins can cause type issues - use typed nodes or Clamp/Lerp instead of basic operators
