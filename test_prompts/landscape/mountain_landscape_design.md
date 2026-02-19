# Mountain Valley Landscape - Design Document

## What Was Created

A landscape called **MountainValley** with a 4-layer material (**M_MountainTerrain**) featuring:
- **Grass** - Green meadows and valley floor (R=0.12, G=0.38, B=0.05)
- **Rock** - Gray-brown cliff faces and steep slopes (R=0.35, G=0.28, B=0.20)
- **Water** - Blue-green river channel (R=0.04, G=0.22, B=0.35)
- **Snow** - White/blue peaks (R=0.90, G=0.92, B=0.95)

### Terrain Shape
- Dominant flat-topped mountain peak (center-left, height ~20262, inspired by Squaretop Mountain)
- Mountain ridges on the right side (height ~8915)
- Background mountain range (height ~4620)
- River valley in the foreground with carved channel (depth ~-5169)
- Meadow area between river and mountains

### Landscape Specs
- **Resolution**: 1009×1009 vertices (upgraded from original 505×505)
- **Components**: 8×8, sections_per_component=2, quads_per_section=63
- **Scale**: 100×100×100
- **Location**: Origin (0, 0, 0)

### Material Graph
- Three `LandscapeLayerBlend` nodes (BaseColor + Roughness + Metallic)
- 4 Vector parameters for layer colors (adjustable via material instances)
- 4 Scalar parameters for roughness: Grass=0.85, Rock=0.92, Water=0.02, Snow=0.45
- 4 Scalar parameters for metallic: Water=0.3, all others=0.0

### Water System
- **Water Material** (`M_Water`): Translucent, two-sided material with:
  - WaterBodyColor parameter (R=0.04, G=0.22, B=0.35)
  - WaterOpacity (0.6), WaterRoughness (0.02), WaterMetallic (0.3)
- **Water Plane**: StaticMeshActor with engine Plane mesh at Z=-3500, scaled 200×200

### Painting Strategy
- Height-based: Rock above 2000 units, Snow above 10000 units
- Grass as base layer everywhere
- Water hand-painted along river channel path

### Lighting Setup
- **DirectionalLight**: Golden hour angle (pitch=-18°, yaw=220°), warm color (255,200,140), intensity=8
- **ExponentialHeightFog**: Density=0.01, height falloff=0.15
- **SkyAtmosphere**, **SkyLight**, **VolumetricCloud**: Present (default settings)

---

## Current Limitations & Improvements Needed

### 1. No Textures (Flat Colors Only) — ⚠️ OPEN
**Issue**: The material uses flat Vector parameter colors instead of texture maps.  
**Impact**: Looks like a plain color-by-number rather than natural terrain.  
**Solution**: Import or create tileable textures for each layer:
- Grass: tileable grass diffuse + normal map
- Rock: tileable rock/cliff diffuse + normal map  
- Snow: tileable snow diffuse + normal map

Use `LandscapeMaterialService.setup_layer_textures()` to connect textures to each blend layer with proper UV tiling.  
**Blocker**: No terrain texture assets exist in this project. Would need to import from Quixel/Megascans or create custom textures.

### 2. No Normal Maps — ⚠️ OPEN
**Issue**: No normal map blending, so surfaces look completely flat under lighting.  
**Impact**: Mountains and rocks have no surface detail or depth.  
**Solution**: Add a `LandscapeLayerBlend` node connected to the Normal output, with normal map textures per layer.  
**Blocker**: Requires texture assets (see Issue #1).

### 3. No Height-Based Auto-Blending — ⚠️ OPEN
**Issue**: Layer painting is manual brush strokes, not procedural.  
**Impact**: Transitions between layers are uniform circles rather than natural patterns.  
**Solution**: 
- Use `LB_HeightBlend` blend type instead of `LB_WeightBlend` for rock/snow transitions
- Add a `WorldPosition.Z` node to drive automatic snow at high elevation
- Add slope-based blending (using `VertexNormalWS`) to auto-place rock on steep surfaces
- **VibeUE Limitation**: No slope-detection paint API; would need custom material graph logic via `MaterialNodeService`

### 4. No Real Water System — ✅ ADDRESSED
**Previous Issue**: "Water" was just a blue paint layer on the terrain surface.  
**What Was Done**:
- Created translucent water material (`M_Water`) with blend mode = Translucent, two-sided
- Water material has adjustable color, opacity (0.6), roughness (0.02), metallic (0.3) parameters
- Placed a scaled StaticMeshActor plane (`WaterPlane`) at Z=-3500 over the river valley
- Paint layer retained for shoreline color blending on terrain  
**Remaining**: No flow animation, reflections, or proper Unreal Water plugin integration. Consider upgrading to Water Body River actor for full water features.

### 5. No Foliage/Vegetation — ❌ CANNOT FIX (API Limitation)
**Issue**: The reference image shows dense pine/evergreen forests and meadow grass.  
**Impact**: Scene looks barren compared to the lush reference.  
**Blockers**:
- No `FoliageService` exists in VibeUE plugin
- No tree or grass meshes available in the project
- Would need both asset imports and a foliage placement API
- **Recommendation**: Manually add foliage in Unreal Editor using the Foliage tool, or request `FoliageService` be added to VibeUE

### 6. Limited Sculpting Fidelity — ⚠️ PARTIALLY ADDRESSED
**Issue**: The sculpting API uses circular brushes and rectangular regions; complex shapes are hard to create.  
**What Was Done**: Rebuilt landscape at higher resolution (1009×1009 vs 505×505) and applied multiple sculpting passes with noise, peaks, ridges, valley, and detail.  
**Remaining**: Still lacks dramatic vertical cliff faces, erosion detail, and overhangs of the reference photo.  
**Solution**: Import a heightmap from World Machine/Gaea via `import_heightmap()` for precise terrain shapes.

### 7. No Atmosphere/Sky/Lighting — ✅ ADDRESSED
**Previous Issue**: Scene had no lighting tuning or atmospheric depth.  
**What Was Done**:
- Level already had SkyAtmosphere, SkyLight, DirectionalLight, ExponentialHeightFog, VolumetricCloud
- Tuned DirectionalLight: golden hour angle (pitch=-18°, yaw=220°), warm color (255,200,140), intensity=8
- Tuned ExponentialHeightFog: density=0.01, height falloff=0.15 for atmospheric depth  
**Remaining**: Could further tune sky atmosphere for more dramatic sunset colors, adjust volumetric cloud density.

### 8. Paint Resolution vs Brush Size — ✅ ADDRESSED
**Previous Issue**: 505×505 vertex landscape had coarse paint transitions.  
**What Was Done**: Rebuilt landscape at 1009×1009 vertices (sections_per_component=2), doubling the resolution.  
**Remaining**: Could go higher still, but current resolution provides reasonable detail for the scale.

---

## Recommended Next Steps (Priority Order)

1. **Import tileable textures** for grass, rock, and snow layers (requires external assets)
2. **Add normal maps** per layer for surface detail (requires texture assets)
3. **Add height/slope-based blending** in the material for automatic transitions (via MaterialNodeService)
4. **Upgrade water** to Water Body River actor for reflections and flow
5. **Add foliage** (trees, grass) manually in Unreal Editor or via future FoliageService
6. **Consider heightmap import** for more realistic mountain shapes (World Machine/Gaea)
7. **Fine-tune atmosphere** — adjust SkyAtmosphere sunset colors, volumetric cloud density

---

## Progress Summary

| Issue | Status | Notes |
|-------|--------|-------|
| #1 No Textures | ⚠️ Open | Needs external texture assets |
| #2 No Normal Maps | ⚠️ Open | Blocked by #1 |
| #3 No Auto-Blending | ⚠️ Open | Possible via MaterialNodeService |
| #4 No Water System | ✅ Done | Translucent water plane placed |
| #5 No Foliage | ❌ Blocked | No FoliageService API |
| #6 Sculpting Fidelity | ✅ Improved | Higher resolution, multi-pass sculpting |
| #7 Lighting/Atmosphere | ✅ Done | Golden hour tuning applied |
| #8 Paint Resolution | ✅ Done | Upgraded to 1009×1009 |

---

## Assets Created

| Asset | Path | Type |
|-------|------|------|
| Mountain Material | `/Game/Landscape/M_MountainTerrain` | Material (4-layer blend) |
| Water Material | `/Game/Landscape/M_Water` | Material (Translucent) |
| Grass Layer Info | `/Game/Landscape/LI_Grass` | LandscapeLayerInfoObject |
| Rock Layer Info | `/Game/Landscape/LI_Rock` | LandscapeLayerInfoObject |
| Water Layer Info | `/Game/Landscape/LI_Water` | LandscapeLayerInfoObject |
| Snow Layer Info | `/Game/Landscape/LI_Snow` | LandscapeLayerInfoObject |
| Landscape Actor | `MountainValley` (in level) | Landscape (1009×1009) |
| Water Plane | `WaterPlane` (in level) | StaticMeshActor |

---

## Lessons Learned

1. **MaterialService method names** differ from docstring shortcuts — use `compile_material` not `compile`, `save_material` not `save`
2. **Sculpting may not persist** across landscape re-creation — always verify heights after sculpting
3. **Material OverrideMaterials** can't be set via `ActorService.set_property()` — use direct Python `set_material()` on the component
4. **Landscape position matters** — creating at Z=-5000 confused height queries; origin (0,0,0) works better
5. **Duplicate layers** can occur when re-assigning materials — always clean up with remove_layer before re-adding
