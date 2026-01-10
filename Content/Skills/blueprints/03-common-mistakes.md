# Blueprint Common Mistakes

Critical rules and common pitfalls when working with Blueprints.

---

## CRITICAL: Discover Return Types BEFORE Accessing Properties

**Problem:** AI frequently guesses wrong property names on return objects.

**Common mistakes:**
- `info.inputs` → Should be `info.input_parameters`
- `node.node_name` → Should be `node.node_title`  
- `pin.is_linked` → Should be `pin.is_connected`
- `var.name` → Should be `var.variable_name`
- `param.pin_name` → Should be `param.parameter_name`
- `param.pin_type` → Should be `param.parameter_type`

**Solution:** Use `discover_python_class` on return types:
```python
# ALWAYS discover return type properties first
discover_python_class("unreal.BlueprintFunctionDetailedInfo")
discover_python_class("unreal.BlueprintFunctionParameterInfo")
discover_python_class("unreal.BlueprintNodeInfo")
discover_python_class("unreal.BlueprintPinInfo")

# Now you know the correct property names
info = unreal.BlueprintService.get_function_info(path, func)
print(info.input_parameters)  # Correct - NOT info.inputs
for p in info.input_parameters:
    print(p.parameter_name)  # Correct - NOT p.pin_name
```

See "Return Types - CRITICAL" section in service-reference.md for all property names.

---

## CRITICAL: No Generic `add_node` Method

**Problem:** AI hallucinates a generic `add_node(path, graph, "NodeType", x, y)` method.

**This does NOT exist:**
```python
# WRONG - AttributeError!
unreal.BlueprintService.add_node(path, func, "K2Node_IfThenElse", 200, 0)
```

**Use specific methods instead:**
```python
# CORRECT
unreal.BlueprintService.add_branch_node(path, func, 200, 0)
unreal.BlueprintService.add_math_node(path, func, "Add", "Float", 200, 0)
unreal.BlueprintService.add_comparison_node(path, func, "Greater", "Float", 200, 0)
unreal.BlueprintService.add_get_variable_node(path, func, "Health", 200, 0)
unreal.BlueprintService.add_set_variable_node(path, func, "Health", 200, 0)
unreal.BlueprintService.add_print_string_node(path, func, 200, 0)
unreal.BlueprintService.add_function_call_node(path, func, "KismetMathLibrary", "Clamp", 200, 0)
```

---

## CRITICAL: Method Name Word Order

**Problem:** Node methods have specific word order that's easy to confuse.

**Common mistakes:**
- `add_variable_get_node` → Should be `add_get_variable_node`
- `add_variable_set_node` → Should be `add_set_variable_node`

**Correct pattern:** `add_<action>_<subject>_node`
```python
# CORRECT
unreal.BlueprintService.add_get_variable_node(path, func, "Health", x, y)
unreal.BlueprintService.add_set_variable_node(path, func, "Health", x, y)

# WRONG - will fail
unreal.BlueprintService.add_variable_get_node(...)  # AttributeError!
```

**When in doubt:** Use discovery to find exact method names:
```python
import unreal
methods = [m for m in dir(unreal.BlueprintService) if "variable" in m.lower()]
print(methods)  # Shows: ['add_get_variable_node', 'add_set_variable_node', ...]
```

---

## CRITICAL: Use Safe `next()` with Default

**Problem:** Using `next()` without a default throws `StopIteration` when no match found.

**Wrong:**
```python
# DANGEROUS - crashes if no match
entry = next(n for n in nodes if n.node_type == "K2Node_FunctionEntry")
```

**Correct:**
```python
# SAFE - returns None if no match
entry = next((n for n in nodes if n.node_type == "K2Node_FunctionEntry"), None)
if entry is None:
    print("Entry node not found!")
    return

# Or use list comprehension
entries = [n for n in nodes if n.node_type == "K2Node_FunctionEntry"]
if entries:
    entry = entries[0]
```

---

## CRITICAL: Compile Before Using Variables in Nodes

**Problem:** Trying to add Get/Set Variable nodes immediately after adding a variable fails.

**Why:** Unreal needs to compile the blueprint to register new variables in the blueprint VM.

**Solution:**
```python
import unreal

# Add variable
unreal.BlueprintService.add_variable("/Game/BP_Player", "Health", "float", "100.0")

# REQUIRED: Compile before using in nodes
unreal.BlueprintService.compile_blueprint("/Game/BP_Player")

# Now you can add Get/Set nodes
get_health = unreal.BlueprintService.add_get_variable_node("/Game/BP_Player", "MyFunc", "Health", 100, 0)
```

---

## CRITICAL: Check Before Creating

**Problem:** Creating duplicates causes errors or silent failures.

**Solution:** Always check if something exists first.

```python
import unreal

# Check if blueprint exists
existing = unreal.AssetDiscoveryService.find_asset_by_path("/Game/BP_Player")
if not existing:
    path = unreal.BlueprintService.create_blueprint("Player", "Character", "/Game/")

# Check if variable exists (use .variable_name NOT .name)
vars = unreal.BlueprintService.list_variables("/Game/BP_Player")
if not any(v.variable_name == "Health" for v in vars):
    unreal.BlueprintService.add_variable("/Game/BP_Player", "Health", "float", "100.0")

# Check if component exists
comps = unreal.BlueprintService.list_components("/Game/BP_Player")
if not any(c.name == "BodyMesh" for c in comps):
    unreal.BlueprintService.add_component("/Game/BP_Player", "StaticMeshComponent", "BodyMesh")
```

---

## CRITICAL: Discover Pin Names Before Connecting

**Problem:** Guessing pin names leads to connection failures.

**Why:** Different node types have different pin names. Even similar nodes may vary.

**Solution:**
```python
import unreal

# WRONG - Guessing pin names
# unreal.BlueprintService.connect_nodes(path, func, 1, "output", 2, "input")

# CORRECT - Discover pins first
nodes = unreal.BlueprintService.get_nodes_in_graph("/Game/BP_Player", "TakeDamage")
for node in nodes:
    print(f"Node {node.node_id}: {node.node_type}")
    for pin in node.pins:
        print(f"  {pin.name} ({pin.direction})")

# Now use exact pin names
unreal.BlueprintService.connect_nodes(path, func, 1, "then", 2, "execute")
```

---

## Node ID 0 and 1 are Reserved

**Entry Node** is always ID 0 (function start)
**Result Node** is always ID 1 (function return)

**Example:**
```python
import unreal

# Create function
unreal.BlueprintService.create_function("/Game/BP_Player", "TestFunc")

# Entry node is ID 0 - no need to create
entry_node = 0

# Result node is ID 1 - no need to create
result_node = 1

# Add custom nodes starting from ID 2+
print_node = unreal.BlueprintService.add_print_string_node("/Game/BP_Player", "TestFunc", -200, 0)
# print_node will be 2

# Connect entry → print → result
unreal.BlueprintService.connect_nodes("/Game/BP_Player", "TestFunc", entry_node, "then", print_node, "execute")
unreal.BlueprintService.connect_nodes("/Game/BP_Player", "TestFunc", print_node, "then", result_node, "execute")
```

---

## UE 5.7: Float → Double

Unreal Engine 5.7 deprecated `float` for `double` in math operations.

**Solution:** VibeUE normalizes this automatically. Use either "Float" or "Double" - both work.

```python
import unreal

# Both are valid
unreal.BlueprintService.add_math_node(path, func, "Add", "Float", -200, 0)
unreal.BlueprintService.add_math_node(path, func, "Add", "Double", -200, 0)
```

---

## Save After Making Changes

**Problem:** Changes are lost if editor crashes or blueprint isn't saved.

**Solution:**
```python
import unreal

# Make changes
unreal.BlueprintService.add_variable("/Game/BP_Player", "Health", "float", "100.0")
unreal.BlueprintService.compile_blueprint("/Game/BP_Player")

# Save immediately
unreal.EditorAssetLibrary.save_asset("/Game/BP_Player")

# Or save all dirty assets
unreal.AssetDiscoveryService.save_all_assets()
```

---

## Use Full Asset Paths

**Problem:** Relative paths don't work.

**Solution:**
```python
# WRONG
path = "BP_Player"

# CORRECT
path = "/Game/Blueprints/BP_Player"

# CORRECT - Use search to find exact path
results = unreal.AssetDiscoveryService.search_assets("BP_Player", "Blueprint")
if results:
    path = results[0].path
```

---

## Variable Types Must Be Exact

**Problem:** Using wrong type names causes variable creation to fail.

**Solution:** Use `search_variable_types` to find correct type names.

```python
import unreal

# Find the correct type name
types = unreal.BlueprintService.search_variable_types("Vector", "", 10)
for t in types:
    print(f"{t.type_name}: {t.type_path}")

# Use exact type name from search results
unreal.BlueprintService.add_variable("/Game/BP_Player", "Velocity", "Vector", "(X=0,Y=0,Z=0)")
```

---

## Struct Property Format

**Problem:** Setting component or variable properties with struct values fails.

**Why:** Unreal expects specific string format for structs.

**Solution:**
```python
import unreal

# Vectors, Rotators, Colors use parentheses
unreal.BlueprintService.set_component_property(
    "/Game/BP_Player",
    "Mesh",
    "RelativeLocation",
    "(X=0,Y=0,Z=50)"
)

unreal.BlueprintService.set_component_property(
    "/Game/BP_Player",
    "Mesh",
    "RelativeRotation",
    "(Pitch=0,Yaw=90,Roll=0)"
)

# Colors are 0.0-1.0, not 0-255
unreal.BlueprintService.set_component_property(
    "/Game/BP_Light",
    "Light",
    "LightColor",
    "(R=1.0,G=0.0,B=0.0,A=1.0)"
)
```

---

## Colors are 0.0-1.0, Not 0-255

**Problem:** Using RGB values 0-255 produces wrong colors.

**Solution:**
```python
import unreal

# WRONG
color = "(R=255,G=128,B=0,A=255)"

# CORRECT
color = "(R=1.0,G=0.5,B=0.0,A=1.0)"
```

---

## Error Recovery Pattern

**Problem:** Repeated failures waste tokens and frustrate users.

**Solution:**
- Max 3 attempts at same operation
- Max 2 discovery calls for same function
- Stop after 2 failed searches, ask user
- If success but no change after 2 tries, report limitation

```python
import unreal

attempts = 0
max_attempts = 3

while attempts < max_attempts:
    try:
        # Try operation
        unreal.BlueprintService.add_variable("/Game/BP_Player", "Health", "float", "100.0")
        break  # Success
    except Exception as e:
        attempts += 1
        if attempts >= max_attempts:
            print(f"Failed after {max_attempts} attempts: {e}")
            break
```

---

## Don't Use Modal Dialogs or Blocking Operations

**Problem:** Modal dialogs freeze the editor. Blocking operations prevent MCP from responding.

**Never use:**
- Modal dialogs
- `input()` or user prompts
- Long `time.sleep()` calls (>1 second)
- Infinite loops

**Safe alternatives:**
```python
import unreal

# Don't use modal dialogs - just execute
unreal.BlueprintService.compile_blueprint(path)

# Don't use long sleeps - Unreal operations are fast
# time.sleep(10)  # BAD

# Don't use infinite loops - use iteration with limits
for i in range(100):  # GOOD - bounded loop
    pass
```

---

## Function Call Node Class Names

**Problem:** Using wrong class names for function calls.

**Solution:** Use these standard class names:

```python
import unreal

# Math functions
unreal.BlueprintService.add_function_call_node(path, func, "KismetMathLibrary", "Add", x, y)

# System functions (PrintString, Delay)
unreal.BlueprintService.add_function_call_node(path, func, "KismetSystemLibrary", "PrintString", x, y)

# String operations
unreal.BlueprintService.add_function_call_node(path, func, "KismetStringLibrary", "Concat", x, y)

# Array operations
unreal.BlueprintService.add_function_call_node(path, func, "KismetArrayLibrary", "Add", x, y)

# Gameplay functions
unreal.BlueprintService.add_function_call_node(path, func, "GameplayStatics", "GetPlayerController", x, y)

# Actor functions
unreal.BlueprintService.add_function_call_node(path, func, "Actor", "GetActorLocation", x, y)
```

---

## Refresh Nodes After Function Signature Changes

**Problem:** Function call nodes become outdated when target function signature changes.

**Solution:** Use `refresh_node` to update nodes.

```python
import unreal

# Change function signature
unreal.BlueprintService.add_function_input("/Game/BP_Player", "TakeDamage", "NewParam", "float")
unreal.BlueprintService.compile_blueprint("/Game/BP_Player")

# Find nodes that call this function and refresh them
# (In another blueprint that calls TakeDamage)
nodes = unreal.BlueprintService.get_nodes_in_graph("/Game/BP_GameMode", "SpawnPlayer")
for node in nodes:
    if "TakeDamage" in node.node_type:
        unreal.BlueprintService.refresh_node("/Game/BP_GameMode", "SpawnPlayer", node.node_id)

unreal.BlueprintService.compile_blueprint("/Game/BP_GameMode")
```
