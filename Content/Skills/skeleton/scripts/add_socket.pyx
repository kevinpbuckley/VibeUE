# add_socket.pyx — Add a socket to a skeletal mesh (optionally to the skeleton) and verify.
#
# Sample script for the skeleton skill. Run via execute_python_code.
# add_socket(skeletal_mesh_path, socket_name, bone_name, rel_loc, rel_rot, rel_scale, add_to_skeleton=False)
import unreal
sks = unreal.SkeletonService

MESH = "/Game/Characters/SK_Mannequin"   # skeletal mesh asset
SOCKET = "WeaponSocket"
BONE = "hand_r"

ok = sks.add_socket(
    MESH, SOCKET, BONE,
    unreal.Vector(0, 0, 0),       # relative location
    unreal.Rotator(0, 0, 0),      # relative rotation
    unreal.Vector(1, 1, 1),       # relative scale
    True,                          # add_to_skeleton (shared across meshes)
)
print("add_socket:", ok)

for s in sks.list_sockets(MESH):
    print("socket:", s)
