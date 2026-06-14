# scatter_foliage.pyx — Create a foliage type from a mesh and scatter instances at points.
#
# Sample script for the foliage skill. Run via execute_python_code.
import unreal
fs = unreal.FoliageService

MESH = "/Game/Meshes/SM_Bush"
ft = fs.create_foliage_type(MESH, "/Game/Foliage", "FT_Bush",
                            min_scale=0.8, max_scale=1.2, align_to_normal=True)
print("foliage type:", ft)

# Scatter at explicit world locations (use trace_to_surface to drop onto the landscape)
locations = [unreal.Vector(x*200.0, 0.0, 0.0) for x in range(5)]
r = fs.add_foliage_instances(MESH, locations, min_scale=0.9, max_scale=1.1,
                             align_to_normal=True, random_yaw=True, trace_to_surface=True)
print("scatter:", r, "count:", fs.get_instance_count(MESH) if fs.has_foliage_instances(MESH) else 0)
