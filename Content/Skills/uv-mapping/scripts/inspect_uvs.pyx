# inspect_uvs.pyx — Inspect UV channels + health on a StaticMesh, and transform (tile) a channel.
#
# Sample script for the uv-mapping skill. Run via execute_python_code.
import unreal
uv = unreal.UVMappingService

MESH = "/Game/Meshes/SM_Example"   # a StaticMesh asset

print("channels:", [str(c) for c in uv.list_uv_channels(MESH)])
print("health:", uv.get_uv_health(MESH))
# Tile UV channel 0 by 2x (transform_u_vs: scale/offset/rotate — discover signature first)
# uv.transform_u_vs(MESH, 0, 2.0, 2.0, 0.0, 0.0, 0.0)
img = uv.export_uv_layout_image(MESH, 0)
print("layout image:", img)
