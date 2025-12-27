# create_instance

Create a material instance (MIC) from a parent material.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| parent_material_path | string | Yes | Path to parent material |
| destination_path | string | Yes | Package path for new instance |
| instance_name | string | Yes | Name for the instance |
| scalar_parameters | object | No | Initial scalar parameter overrides `{"ParamName": 0.5}` |
| vector_parameters | object | No | Initial vector parameter overrides `{"ParamName": [R,G,B,A]}` |
| texture_parameters | object | No | Initial texture parameter overrides `{"ParamName": "/Game/Textures/MyTex"}` |

## Examples

Create a basic instance:
```json
{
  "action": "create_instance",
  "parent_material_path": "/Game/Materials/M_Base",
  "destination_path": "/Game/Materials/Instances",
  "instance_name": "MI_RedVariant"
}
```

Create with parameter overrides:
```json
{
  "action": "create_instance",
  "parent_material_path": "/Game/Materials/M_Base",
  "destination_path": "/Game/Materials/Instances",
  "instance_name": "MI_RedVariant",
  "scalar_parameters": {"Roughness": 0.8, "Metallic": 0.0},
  "vector_parameters": {"BaseColor": [1.0, 0.0, 0.0, 1.0]}
}
```

## Returns

- `success`: true/false
- `instance_path`: Full path to created instance
- `message`: Success/error message

## Tips

- Parent material must exist and have parameters defined
- Use `list_parameters` on parent to see available parameters
- Vector colors use linear values [0.0-1.0], not sRGB
