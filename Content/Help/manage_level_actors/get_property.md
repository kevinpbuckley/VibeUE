# get_property

Get a property value from an actor or its component in the level.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| actor_label | string | Yes* | Label of the actor |
| property_path | string | **Yes** | Path to the property (e.g., "ComponentName.PropertyName"). Use `get_all_properties` to find component names |

*Alternative identifiers: `actor_path`, `actor_guid`, `actor_tag`

**IMPORTANT**: The parameter is `property_path`, NOT `property_name`.

## Examples

### Get Light Intensity
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"actor_label\": \"MyPointLight\", \"property_path\": \"LightComponent0.Intensity\"}"
}
```

### Get Light Color
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"actor_label\": \"MyPointLight\", \"property_path\": \"LightComponent0.LightColor\"}"
}
```

### Get Actor-Level Property
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"actor_label\": \"BP_Enemy_0\", \"property_path\": \"Health\"}"
}
```

### Get Mesh Component Property
```json
{
  "Action": "get_property",
  "ParamsJson": "{\"actor_label\": \"Cube\", \"property_path\": \"StaticMeshComponent0.CastShadow\"}"
}
```

## Common Component Names

| Actor Type | Component Name | Common Properties |
|------------|----------------|-------------------|
| PointLight | LightComponent0 | Intensity, LightColor, AttenuationRadius |
| SpotLight | LightComponent0 | Intensity, LightColor, InnerConeAngle, OuterConeAngle |
| DirectionalLight | LightComponent0 | Intensity, LightColor |
| StaticMeshActor | StaticMeshComponent0 | CastShadow, bVisible |

**Note**: Use `get_all_properties` to verify exact component names as they may vary.

## Returns

```json
{
  "success": true,
  "actor_label": "MyPointLight",
  "component": "LightComponent0",
  "property_name": "Intensity",
  "property_path": "LightComponent0.Intensity",
  "value": "5000.000000",
  "type": "float",
  "is_editable": true,
  "category": "Light"
}
```

## Error Responses

| Error Code | Description |
|------------|-------------|
| INVALID_IDENTIFIER | No actor identifier provided |
| ACTOR_NOT_FOUND | Actor with given identifier not found |
| MISSING_PROPERTY | property_path is required (not property_name!) |
| PROPERTY_NOT_FOUND | Property does not exist on actor/component |
| COMPONENT_NOT_FOUND | Component name not found - use get_all_properties to find correct names |

## Tips

- **Always use `property_path`, not `property_name`**
- Use `get_all_properties` first to discover component names and available properties
- Property paths use format: `ComponentName.PropertyName`
- For actor-level properties, just use the property name directly
- Property names are case-sensitive
