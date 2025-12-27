# set_property

Set a property value on an actor or its component in the level.

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| actor_label | string | Yes* | Label of the actor |
| property_path | string | **Yes** | Path to the property (e.g., "ComponentName.PropertyName"). Use `get_all_properties` to find component names |
| value | any | Yes | New value for the property |

*Alternative identifiers: `actor_path`, `actor_guid`, `actor_tag`

**IMPORTANT**: The parameter is `property_path`, NOT `property_name`. Use `get_all_properties` to discover the correct component names.

## Examples

### Set Light Intensity
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"actor_label\": \"MyPointLight\", \"property_path\": \"LightComponent0.Intensity\", \"value\": \"10000\"}"
}
```

### Set Light Color (warm orange)
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"actor_label\": \"MyPointLight\", \"property_path\": \"LightComponent0.LightColor\", \"value\": \"warm\"}"
}
```

### Set Light Color (hex format)
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"actor_label\": \"MyPointLight\", \"property_path\": \"LightComponent0.LightColor\", \"value\": \"#FF8844\"}"
}
```

### Set Boolean Property
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"actor_label\": \"Cube\", \"property_path\": \"StaticMeshComponent0.CastShadow\", \"value\": \"false\"}"
}
```

### Set Actor-Level Property
```json
{
  "Action": "set_property",
  "ParamsJson": "{\"actor_label\": \"BP_Enemy_0\", \"property_path\": \"Health\", \"value\": \"200\"}"
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

**Tip**: Use `get_all_properties` action to discover exact component names for any actor.

## Returns

```json
{
  "success": true,
  "property_path": "LightComponent0.Intensity",
  "confirmed_value": "10000.000000"
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
| INVALID_VALUE | Value format doesn't match property type |

## Value Formats

Colors can be specified in multiple formats:
| Format | Example |
|--------|---------|
| Named color | `"warm"`, `"cool"`, `"daylight"` |
| Hex | `"#FF8800"` or `"FF8800"` |
| RGB array | `[255, 128, 0]` or `[1.0, 0.5, 0.0]` |
| Unreal format | `"(R=255,G=128,B=0,A=255)"` |

Other types:
| Type | Format Example |
|------|----------------|
| float | `"5000.0"` or `"5000"` |
| int | `"100"` |
| bool | `"true"` or `"false"` |
| FVector | `"(X=100,Y=200,Z=300)"` or `[100, 200, 300]` |
| FRotator | `"(Pitch=0,Yaw=90,Roll=0)"` |

## Tips

- **Always use `property_path`, not `property_name`**
- Use `get_all_properties` first to discover component names and available properties
- Property paths use format: `ComponentName.PropertyName`
- Values should be passed as strings (the system will convert them)
- Some properties may be read-only (`is_editable: false`)
- Save the level to persist property changes
