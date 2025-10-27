# Blueprint Development Workflow

## CRITICAL DEPENDENCY ORDER

**⚠️ ALWAYS follow this order to prevent dependency failures:**

### Phase 1: Foundation (Create Before Use)
1. **Create Blueprint** 
   ```python
   create_blueprint(name="BP_Player2", parent_class="Character")
   ```

2. **Create Blueprint Variables**
   ```python
   manage_blueprint_variable(
       blueprint_name="/Game/Blueprints/BP_Player2",
       action="create",
       variable_name="Health",
       variable_config={"type_path": "/Script/CoreUObject.FloatProperty"}
   )
   ```

3. **Add Components**
   ```python
   manage_blueprint_component(
       blueprint_name="/Game/Blueprints/BP_Player2",
       action="create",
       component_type="SpotLightComponent",
       component_name="SpotLight_Top"
   )
   ```

### Phase 2: Function Structure
4. **Create Function**
   ```python
   manage_blueprint_function(
       blueprint_name="/Game/Blueprints/BP_Player2",
       action="create",
       function_name="CalculateHealth"
   )
   ```

5. **Add Function Parameters**
   ```python
   # Input parameter
   manage_blueprint_function(
       blueprint_name="/Game/Blueprints/BP_Player2",
       action="add_param",
       function_name="CalculateHealth",
       param_name="BaseHealth",
       direction="input",
       type="float"
   )
   
   # Output parameter (use "out" not "output"!)
   manage_blueprint_function(
       blueprint_name="/Game/Blueprints/BP_Player2",
       action="add_param",
       function_name="CalculateHealth",
       param_name="ResultHealth",
       direction="out",  # ✅ CORRECT
       type="float"
   )
   ```

6. **Add Local Variables**
   ```python
   manage_blueprint_function(
       blueprint_name="/Game/Blueprints/BP_Player2",
       action="add_local",
       function_name="CalculateHealth",
       param_name="TempValue",
       type="float"
   )
   ```

### Phase 3: Node Implementation
7. **Discover Available Nodes**
   ```python
   # Get exact spawner_keys for reliable node creation
   nodes = get_available_blueprint_nodes(
       blueprint_name="/Game/Blueprints/BP_Player2",
       search_term="Random Integer"
   )
   # Look for spawner_key in results
   ```

8. **Create Nodes with Exact Spawner Keys**
   ```python
   manage_blueprint_node(
       blueprint_name="/Game/Blueprints/BP_Player2",
       action="create",
       graph_scope="function",
       function_name="CalculateHealth",
       node_params={"spawner_key": "KismetMathLibrary::RandomIntegerInRange"},
       position=[200, 100]
   )
   ```

9. **Connect Pins**
   ```python
   manage_blueprint_node(
       blueprint_name="/Game/Blueprints/BP_Player2",
       action="connect_pins",
       graph_scope="function",
       function_name="CalculateHealth",
       extra={
           "connections": [{
               "source_node_id": "{SOURCE_GUID}",
               "source_pin_name": "ReturnValue",
               "target_node_id": "{TARGET_GUID}",
               "target_pin_name": "InValue"
           }]
       }
   )
   ```

10. **Test Compilation**
    ```python
    compile_blueprint("BP_Player2")
    ```

### Phase 4: Event Graph Integration
11. **Create Event Graph Nodes**
    ```python
    manage_blueprint_node(
        blueprint_name="/Game/Blueprints/BP_Player2",
        action="create",
        graph_scope="event",  # Event Graph context
        node_type="Event BeginPlay"
    )
    ```

12. **Final Compilation**
    ```python
    compile_blueprint("BP_Player2")
    ```

## Common Mistakes

❌ **Creating nodes before variables** → Nodes can't reference non-existent variables  
❌ **Using "output" instead of "out"** → Invalid direction parameter  
❌ **Using fuzzy node names** → Creates wrong node variants  
❌ **Skipping compilation** → Changes don't take effect  
❌ **Wrong graph_scope** → Nodes appear in wrong graph

## Best Practices

✅ Use full Blueprint paths: `/Game/Blueprints/BP_Player2`  
✅ Use `spawner_key` for exact node creation  
✅ Call `describe` action to verify node structure  
✅ Connect nodes immediately after creation  
✅ Compile frequently to catch errors early
