# manage_blueprint

Create and manage Blueprint assets in Unreal Engine - create new blueprints, compile, get/set properties, and manage inheritance.

## Getting Action-Specific Help

For detailed help on any action, use the `help_action` parameter:
```python
# Get help for a specific action
manage_blueprint(action="help", help_action="create")
manage_blueprint(action="help", help_action="compile")
manage_blueprint(action="help", help_action="get_info")
```

**Available help_action values:** create, get_info, compile, reparent, set_property, get_property

## Summary

The `manage_blueprint` tool provides core Blueprint management functionality. It allows you to create new Blueprint classes, compile them, change their parent class, and read or modify Blueprint default properties. This is the foundation for programmatic Blueprint manipulation.

## Actions

| Action | Description |
|--------|-------------|
| create | Create a new Blueprint asset with a specified parent class |
| get_info | Get detailed information about a Blueprint |
| compile | Compile a Blueprint to update its generated class |
| reparent | Change the parent class of a Blueprint |
| set_property | Set a default property value on a Blueprint |
| get_property | Get the current value of a default property |

## Usage

### Create a New Blueprint
```json
{
  "Action": "create",
  "ParamsJson": "{\"Path\": \"/Game/Blueprints/BP_NewActor\", \"ParentClass\": \"Actor\"}"
}
```

### Compile Blueprint
```json
{
  "Action": "compile",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Player\"}"
}
```

### Get Property Value
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Enemy\", \"PropertyName\": \"MaxHealth\"}"
}
```

## Notes

- Always compile Blueprints after making structural changes
- Property names are case-sensitive
- Parent class changes may invalidate existing components or logic
- Use `get_info` to discover available properties and structure

## Blueprint Development Order

**Dependencies matter!** Follow this order when building Blueprints:
1. Create Blueprint with `manage_blueprint(action="create")`
2. Add Variables with `manage_blueprint_variable`
3. Add Components with `manage_blueprint_component`
4. Create Functions with `manage_blueprint_function`
5. Add Nodes with `manage_blueprint_node`
6. Compile with `manage_blueprint(action="compile")`

## Widget Blueprint Development Order

**Widget Blueprints use TWO tools - `manage_blueprint` for creation, `manage_umg_widget` for components:**
1. **Create Widget Blueprint**: `manage_blueprint(action="create", blueprint_name="/Game/UI/MyWidget", parent_class="UserWidget")`
2. **Add UI Components**: `manage_umg_widget(action="add_component", widget_name="/Game/UI/MyWidget", component_type="Button", component_name="PlayBtn")`
3. **Configure Properties**: `manage_umg_widget(action="set_property", ...)`
4. **Bind Events**: Use `manage_umg_widget(action="bind_events")` or create handler functions manually
5. **Compile**: `manage_blueprint(action="compile", blueprint_name="/Game/UI/MyWidget")`

**Common Mistake:** Trying to use `manage_umg_widget(action="create")` for widget blueprints - this doesn't work! Use `manage_blueprint` to create widget blueprints.

## Common Patterns

### Creating a Blueprint Actor
```python
# 1. Create the Blueprint
manage_blueprint(action="create", name="BP_Enemy", parent_class="Actor")

# 2. Add a mesh component
manage_blueprint_component(action="add", blueprint_name="BP_Enemy", 
                          component_type="StaticMeshComponent", component_name="EnemyMesh")

# 3. Add a variable
manage_blueprint_variable(action="add", blueprint_name="BP_Enemy",
                         variable_name="Health", type="Float")

# 4. Compile
manage_blueprint(action="compile", blueprint_name="BP_Enemy")

# 5. Save
manage_asset(action="save_all")
```
