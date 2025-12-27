# diff

Compare two blueprints and return the differences in JSON format. Similar to Unreal Engine's Blueprint Diff tool.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| blueprint_a | string | Yes* | Name or path of the first blueprint to compare |
| blueprint_b | string | Yes* | Name or path of the second blueprint to compare |
| source_blueprint | string | Yes* | Alternative name for blueprint_a |
| target_blueprint | string | Yes* | Alternative name for blueprint_b |
| include_graphs | boolean | No | Include graph node count differences (default: true) |
| include_defaults | boolean | No | Include default value differences (default: true) |

*Either blueprint_a/blueprint_b OR source_blueprint/target_blueprint pair is required.

## Examples

### Compare Two Blueprints by Name
```json
{
  "Action": "diff",
  "ParamsJson": {"blueprint_a": "BP_Player", "blueprint_b": "BP_Enemy"}
}
```

### Compare Using Source/Target Naming
```json
{
  "Action": "diff",
  "ParamsJson": {"source_blueprint": "LocalVarTest", "target_blueprint": "TestActor"}
}
```

### Compare with Full Paths
```json
{
  "Action": "diff",
  "ParamsJson": {"blueprint_a": "/Game/Blueprints/BP_OldVersion", "blueprint_b": "/Game/Blueprints/BP_NewVersion"}
}
```

## Returns

```json
{
  "success": true,
  "diff": {
    "blueprint_a": {
      "name": "BP_Player",
      "path": "/Game/Blueprints/BP_Player.BP_Player",
      "parent_class": "Character"
    },
    "blueprint_b": {
      "name": "BP_Enemy",
      "path": "/Game/Blueprints/BP_Enemy.BP_Enemy",
      "parent_class": "Character"
    },
    "parent_class_different": false,
    "variables": {
      "only_in_a": [{"name": "Health", "type": "float", "category": "Default"}],
      "only_in_b": [{"name": "Damage", "type": "float", "category": "Combat"}],
      "in_both": ["Speed"],
      "modified": [{"name": "MaxHealth", "changes": [{"field": "category", "in_a": "Default", "in_b": "Stats"}]}]
    },
    "components": {
      "only_in_a": [{"name": "Mesh", "type": "SkeletalMeshComponent"}],
      "only_in_b": [],
      "in_both": ["DefaultSceneRoot"],
      "modified": []
    },
    "functions": {
      "only_in_a": [{"name": "TakeDamage", "node_count": 5}],
      "only_in_b": [{"name": "Attack", "node_count": 8}],
      "in_both": ["Initialize"],
      "modified": [{"name": "Update", "node_count_in_a": 3, "node_count_in_b": 7, "node_difference": 4}]
    },
    "event_graphs": {
      "total_nodes_in_a": 12,
      "total_nodes_in_b": 15,
      "node_difference": 3,
      "has_differences": true
    },
    "summary": {
      "variables_only_in_a": 1,
      "variables_only_in_b": 1,
      "variables_modified": 1,
      "components_only_in_a": 1,
      "components_only_in_b": 0,
      "components_modified": 0,
      "functions_only_in_a": 1,
      "functions_only_in_b": 1,
      "functions_modified": 1,
      "total_differences": 6,
      "blueprints_identical": false
    }
  }
}
```

## Diff Categories

The diff compares the following aspects of blueprints:

### Variables
- **only_in_a**: Variables that exist only in blueprint A
- **only_in_b**: Variables that exist only in blueprint B
- **in_both**: Variables with the same name and properties
- **modified**: Variables with same name but different type, flags, or category

### Components
- **only_in_a**: Components that exist only in blueprint A
- **only_in_b**: Components that exist only in blueprint B
- **in_both**: Components with the same name and type
- **modified**: Components with same name but different class or parent

### Functions
- **only_in_a**: Functions that exist only in blueprint A
- **only_in_b**: Functions that exist only in blueprint B
- **in_both**: Functions with the same name and node count
- **modified**: Functions with same name but different node counts

### Event Graphs
- Compares total node counts across all event graphs

## Tips

- Use `compare` as an alias for `diff` action
- The diff is unidirectional - A is the "before" and B is the "after"
- Node count differences in functions indicate logic changes
- For detailed node-by-node comparison, use `manage_blueprint_node` to list nodes in each function
- Modified items show which specific fields changed between the two blueprints
