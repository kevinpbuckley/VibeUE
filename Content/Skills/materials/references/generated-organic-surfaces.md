# Generated PBR detail on traversable organic meshes

Diagnose the visible boundary before editing: UV-atlas seams, non-periodic bitmap
borders, hard/custom corner normals, invalid LOD section slots, and overlapping
geometry need different fixes. A continuous projected material cannot smooth
split mesh normals. A component material assignment alone cannot prove a section
renders it; see the asset-management skill's section inspection/remapping helper.

Use mesh-local physical coordinates for rigidly moving or uniformly breathing
surfaces. Match positions, normal basis and projection weights. Do not feed a
seamed UV normal into the weights of UV-independent triplanar color. Nonuniform
scale and skeletal deformation need separate normal/coordinate treatment.

Random patch offsets/rotations can wrap a generated image across mismatched
borders. Use periodic source maps or inset every rotated footprint. With support
`d in [-1,1]^2`, `uv=.5+offset+rotate(d)*.28` and offsets within `[-.04,.04]^2`,
the footprint stays inside roughly `[.064,.936]`. Blend overlapping support with
weights going smoothly to zero; apply the same rotation/scale to gradients and
normal vectors. Smaller image footprints change physical density. Projection
blending softens detail and can be expensive; don't claim performance without
authorized profiling.

Use sRGB for albedo, linear for masks/normals. Albedo RGB + roughness A may use
sRGB because alpha is linear; retain alpha in compression. UE tangent normals
usually need green inversion for OpenGL +Y maps, not DirectX -Y maps. Custom
projection decoding must use its own explicit basis convention. Avoid excessive
normal strength or baked-shadow contrast that makes tissue look deeply wrinkled.

When shared-vertex corner normals are inconsistent on an intended smooth organic
surface, smooth those normals without moving vertices, welding or changing face
winding. Preserve intentional creases and separate shells. Apply before a new
tangent-normal bake and to every exported LOD. Do not smooth all imported assets.

Keep approved assets and authored lighting unchanged while repairing another
asset. Red lighting in a body interior is an art direction, not a material bug.
Use the asset preview for neutral lighting if inspection is needed. Label the
GLB/Blender atlas appearance separately from an Unreal-only projection shader.
Save completion does not establish visual acceptance; respect project restrictions
on screenshots, testing and PIE. Obtain no new paid textures just to repair slots
or normals that can be fixed locally.
