# manipulate_actors.pyx — List actors in the level and move/rotate one (ActorService).
#
# Sample script for the level-actors skill. Run via execute_python_code.
import unreal
acs = unreal.ActorService

# List actors (optionally filter by class)
for a in acs.list_level_actors("", False, 20):
    print(a)

# Find by class
lights = acs.find_actors_by_class("PointLight")
print("point lights:", [str(l) for l in lights][:5])

# Move/rotate by name or label
# acs.set_location("BP_Player_1", unreal.Vector(0, 0, 200))
# acs.set_rotation("BP_Player_1", unreal.Rotator(yaw=90))  # kwargs: positional order is (Roll, Pitch, Yaw)
