# create

Create a new Blueprint asset with a specified parent class.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| name | string | **Yes** | Full path including asset name (e.g., "/Game/Blueprints/BP_Pickup"). Also accepts `blueprint_name` as alias. |
| parent_class | string | No | Parent class name (default: "Actor") |

## Widget Blueprint Support

**When `parent_class` is "UserWidget" or a subclass, a proper Widget Blueprint is created** that can be used with `manage_umg_widget` to add UI components.

### Create Widget Blueprint
```json
{
  "Action": "create",
  "ParamsJson": "{\"name\": \"/Game/UI/WBP_MainMenu\", \"parent_class\": \"UserWidget\"}"
}
```

## Examples

### Create Actor Blueprint
```json
{
  "Action": "create",
  "ParamsJson": "{\"name\": \"/Game/Blueprints/BP_Pickup\", \"parent_class\": \"Actor\"}"
}
```

### Create Character Blueprint
```json
{
  "Action": "create",
  "ParamsJson": "{\"name\": \"/Game/Characters/BP_PlayerCharacter\", \"parent_class\": \"Character\"}"
}
```

### Create Pawn Blueprint
```json
{
  "Action": "create",
  "ParamsJson": "{\"name\": \"/Game/Vehicles/BP_Vehicle\", \"parent_class\": \"Pawn\"}"
}
```

### Create Component Blueprint
```json
{
  "Action": "create",
  "ParamsJson": "{\"name\": \"/Game/Components/BP_HealthComponent\", \"parent_class\": \"ActorComponent\"}"
}
```

## Returns

```json
{
  "success": true,
  "name": "BP_Pickup",
  "path": "/Game/Blueprints/BP_Pickup.BP_Pickup"
}
```

## Tips

- Common parent classes: Actor, Pawn, Character, PlayerController, GameModeBase, ActorComponent, **UserWidget**
- The `name` parameter should be the full path including the asset name
- The Blueprint is automatically saved after creation
- Use descriptive paths to organize your Blueprints by type
- Naming convention: BP_ prefix for actors, WBP_ prefix for widgets
