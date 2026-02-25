# Parameter Reference

## Master Material Parameters

These are the typical parameters exposed by a master landscape material following the Real_Landscape paradigm. Parameter names follow the convention `<Category>_<Name>`.

### Layer Texture Parameters (per layer)

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `Layer##_BaseColor` | Texture | Albedo/diffuse texture for layer | None |
| `Layer##_Normal` | Texture | Normal map for layer | FlatNormal |
| `Layer##_Roughness` | Texture | Roughness (or RMA packed) | Gray |
| `Layer##_Height` | Texture | Height map for height-blending | White |
| `Layer##_UVTiling` | Scalar | UV tiling scale | 0.01 |
| `Layer##_DetailBaseColor` | Texture | Close-up detail texture | None |
| `Layer##_DetailScale` | Scalar | Detail texture tiling multiplier | 5.0 |
| `Layer##_RoughnessMin` | Scalar | Roughness remap minimum | 0.0 |
| `Layer##_RoughnessMax` | Scalar | Roughness remap maximum | 1.0 |

*`##` = layer index: 01, 02, 03, etc.*

### Auto-Blend Parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `SlopeThreshold` | Scalar | Dot product threshold for slope detection (0.7 ≈ 45°) | 0.7 |
| `SlopeBlendWidth` | Scalar | Slope transition smoothness | 0.15 |
| `AltitudeThreshold` | Scalar | World Z height where altitude blend begins | 5000.0 |
| `AltitudeBlendHeight` | Scalar | Height range for altitude transition | 500.0 |
| `HeightBlendSharpness` | Scalar | Height-based layer transition sharpness | 10.0 |

### Distance & LOD Parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `NearDistance` | Scalar | Distance for full-detail rendering | 1000.0 |
| `FarDistance` | Scalar | Distance for simplified rendering | 10000.0 |
| `FadeDistance` | Scalar | Distance at which material fades completely | 50000.0 |
| `DetailBlendDistance` | Scalar | Distance for detail texture fade-in | 500.0 |

### Color Correction Parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `ColorSaturation` | Scalar | Overall color saturation | 1.0 |
| `ColorBrightness` | Scalar | Overall brightness multiplier | 1.0 |
| `ColorContrast` | Scalar | Color contrast adjustment | 1.0 |
| `ColorTint` | Vector | Per-biome color tint (RGB) | (1,1,1,1) |
| `NormalIntensity` | Scalar | Normal map strength multiplier | 1.0 |
| `RoughnessScale` | Scalar | Global roughness scaling | 1.0 |

### Feature Toggles (Static Switches)

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `EnableSnow` | StaticSwitch | Enable snow layer (altitude blend) | true |
| `EnableDisplacement` | StaticSwitch | Enable world position offset | false |
| `EnableRVT` | StaticSwitch | Enable Runtime Virtual Texture output | true |
| `EnableDistanceFades` | StaticSwitch | Enable distance-based LOD | true |
| `EnableDetailTextures` | StaticSwitch | Enable close-up detail textures | true |
| `EnableColorCorrection` | StaticSwitch | Enable per-biome color grading | true |
| `EnableBumpOffset` | StaticSwitch | Enable parallax/bump offset | false |
| `EnableWindSystem` | StaticSwitch | Enable vegetation wind animation | false |

### Displacement / WPO Parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `DisplacementScale` | Scalar | World position offset strength | 10.0 |
| `DisplacementOffset` | Scalar | Displacement center offset | 0.0 |
| `TessellationMultiplier` | Scalar | Tessellation density (if supported) | 1.0 |

### RVT Parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| `RVT_VirtualTexture` | Texture | Runtime Virtual Texture asset reference | None |
| `RVT_WorldHeight` | Scalar | Reference world height for RVT | 0.0 |

## Real_Landscape Texture Naming Convention

| Suffix | Meaning | Example |
|--------|---------|---------|
| `_BC` | Base Color / Albedo | `T_Ground_Grass_01_BC` |
| `_N` | Normal Map | `T_Ground_Grass_01_N` |
| `_H` | Height Map | `T_Ground_Grass_01_H` |
| `_RMA` | Roughness-Metallic-AO packed | `T_Nature_Default_RMA_01` |
| `_D` | Diffuse (alternative to _BC) | `T_Ground_Dirt_01_D` |

Full pattern: `T_Ground_<TerrainType>_##_<Suffix>`

## FindLandscapeTextures Suffix Matching

The `FindLandscapeTextures` tool recognizes these suffixes when scanning directories:

**Albedo/BaseColor:** `_Albedo`, `_Diffuse`, `_BaseColor`, `_D`, `_Color`, `_BC`, `_Base`

**Normal:** `_Normal`, `_N`, `_NormalMap`, `_Norm`

**Roughness:** `_Roughness`, `_R`, `_Rough`, `_RMA`, `_ORM`, `_ARM`

**Terrain type inference from path keywords:**
Grass, Rock, Snow, Dirt, Sand, Forest, Mud, Clay, Gravel, Moss, Bark, Ground, Soil, Stone, Pebble, Mountain, Alpine, Desert, Beach, Cliff, Tundra, Swamp, Wetland, Ice

## Material Function Input Types

| Type String | EFunctionInputType | Common Use |
|-------------|---------------------|-----------|
| `Scalar` | 0 | Thresholds, multipliers, single values |
| `Vector2` | 1 | UV coordinates, 2D offsets |
| `Vector3` | 2 | Colors, normals, world positions |
| `Vector4` | 3 | Full RGBA, packed channel data |
| `Texture2D` | 4 | Texture references |
| `TextureCube` | 5 | Cubemaps (reflections) |
| `TextureExternal` | 6 | External video textures |
| `VolumeTexture` | 7 | 3D textures (clouds, volumes) |
| `StaticBool` | 8 | Compile-time feature toggles |
| `MaterialAttributes` | 9 | Full material attribute struct |
| `Bool` | 10 | Runtime boolean |
