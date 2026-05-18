---
name: array-operations
description: Blueprint array operations with wildcard pins - Array_Random, KismetArrayLibrary functions, K2Node_GetArrayItem, wildcard pin type propagation, common mistakes
---

This sub-doc continues from skill.md → "Array Operations".

## Array Operations

Array operations use **wildcard pins** — the `TargetArray` and element pins start with no type until a typed array is connected. The engine automatically propagates the array element type through all wildcard pins when you connect a typed array variable.

### ⚠️ Random Element From Array — Use `Array_Random`, NOT Manual Length+Random+Get

When you need a random element from an array, use `Array_Random` — a single node that returns
a random item. Do **NOT** manually build `Array_Length` → `RandomIntegerInRange` → `Array_Get`.
That pattern has 3 nodes, is harder to wire, and `Array_Get` is deprecated in UE5.

```python
# CORRECT — single node, editor shows it as "Random Array Item"
arr_rand_id = unreal.BlueprintService.add_function_call_node(
    bp_path, "EventGraph", "KismetArrayLibrary", "Array_Random", 600, 200)
# Pins: TargetArray (in, wildcard), OutItem (out, wildcard), OutIndex (out, int)

# Or via build_graph:
{"ref": "rand", "type": "spawner_key", "params": {"key": "FUNC KismetArrayLibrary::Array_Random"}}
```

### Available Array Functions (KismetArrayLibrary)

| Function | Purpose | Key Pins |
|----------|---------|----------|
| `Array_Length` | Get array element count | TargetArray(in), ReturnValue(int) |
| `Array_LastIndex` | Get last valid index | TargetArray(in), ReturnValue(int) |
| `Array_IsEmpty` | Check if empty | TargetArray(in), ReturnValue(bool) |
| **`Array_Random`** | **Get random element ("Random Array Item")** | **TargetArray(in), OutItem(out), OutIndex(int)** |
| `Array_Add` | Add element, returns index | TargetArray(in/out), NewItem(in), ReturnValue(int) |
| `Array_Remove` | Remove by index | TargetArray(in/out), IndexToRemove(int) |
| `Array_Clear` | Remove all elements | TargetArray(in/out) |
| `Array_Contains` | Check if item exists | TargetArray(in), ItemToFind(in), ReturnValue(bool) |
| `Array_Append` | Append another array | TargetArray(in/out), SourceArray(in) |

### ⚠️ Array Get — Use `discover_nodes` First

`Array_Get` is marked **BlueprintInternalUseOnly** and is replaced by the dedicated `K2Node_GetArrayItem` node in modern UE5. Do NOT try to call `Array_Get` directly.

To get an element from an array by index:

```python
# 1. Discover the correct node key
result = unreal.BlueprintService.discover_nodes(bp_path, "MyFunction", "get array")
# Look for: "Get" with key "NODE K2Node_GetArrayItem"

# 2. Create the node
node_id = unreal.BlueprintService.create_node_by_key(bp_path, "MyFunction", "NODE K2Node_GetArrayItem", 400, 200)
```

### Wildcard Pin Type Propagation

When you connect a typed array to an array function's `TargetArray` pin, the engine automatically resolves
all wildcard pins on that node to the correct element type. This is handled by `UK2Node_CallArrayFunction`.

**Pattern for array operations in `build_graph`:**

```python
result = unreal.BlueprintService.build_graph(bp_path, graph_name,
    nodes=[
        # Get the array variable
        {"ref": "get_arr", "type": "variable_get", "params": {"variable": "MyTargetPoints"}},
        # Get array length
        {"ref": "arr_len", "type": "spawner_key", "params": {"key": "FUNC KismetArrayLibrary::Array_Length"}},
        # Get random element
        {"ref": "arr_rand", "type": "spawner_key", "params": {"key": "FUNC KismetArrayLibrary::Array_Random"}},
    ],
    connections=[
        # Connect typed array to wildcard pins — type propagation happens automatically
        ["get_arr.MyTargetPoints", "arr_len.TargetArray"],
        ["get_arr.MyTargetPoints", "arr_rand.TargetArray"],
    ],
    auto_layout=True
)
```

### Common Mistakes with Arrays

1. **Using Array_Get directly** → Deprecated, use `NODE K2Node_GetArrayItem` instead
2. **Not connecting the typed array source first** → Wildcard pins stay unresolved → compile error: "The type of Target Array is undetermined"
3. **Guessing pin names** → Always use `get_node_pins()` to verify pin names after node creation

---
