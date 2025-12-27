# get_available

List all available component types that can be added to a Blueprint.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| BlueprintPath | string | No | Content path to Blueprint (some components are only valid for certain parent classes) |
| Filter | string | No | Filter component list by name pattern |

## Examples

### Get All Available Components
```json
{
  "Action": "get_available",
  "ParamsJson": "{}"
}
```

### Get Components for Specific Blueprint
```json
{
  "Action": "get_available",
  "ParamsJson": "{\"BlueprintPath\": \"/Game/Blueprints/BP_Character\"}"
}
```

### Filter by Name
```json
{
  "Action": "get_available",
  "ParamsJson": "{\"Filter\": \"Light\"}"
}
```

## Returns

```json
{
  "Success": true,
  "Components": [
    {
      "Class": "StaticMeshComponent",
      "Category": "Rendering",
      "Description": "A component for rendering static meshes"
    },
    {
      "Class": "SkeletalMeshComponent",
      "Category": "Rendering",
      "Description": "A component for rendering skeletal meshes with animations"
    },
    {
      "Class": "PointLightComponent",
      "Category": "Lights",
      "Description": "A point light that emits in all directions"
    },
    {
      "Class": "BoxComponent",
      "Category": "Collision",
      "Description": "A box-shaped collision/trigger volume"
    },
    {
      "Class": "AudioComponent",
      "Category": "Audio",
      "Description": "A component for playing audio"
    }
  ]
}
```

## Tips

- Components are organized by category (Rendering, Lights, Collision, Audio, etc.)
- Some components require specific parent classes (e.g., CharacterMovementComponent requires Character)
- Use the Filter parameter to narrow down large component lists
- Custom C++ components from your project will also appear in the list
