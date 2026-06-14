# build_state_machine.pyx — Add a state machine with two states + a transition, set a state anim.
#
# Sample script for the animation-blueprint skill. Run via execute_python_code.
# Requires an Animation Blueprint asset (ABP_*). Returned ids are node GUID strings.
import unreal
ag = unreal.AnimGraphService

ABP = "/Game/Characters/ABP_Mannequin"   # must already exist
MACHINE = "Locomotion"

machine = ag.add_state_machine(ABP, MACHINE, 0, 0)
print("machine:", machine)

idle = ag.add_state(ABP, MACHINE, "Idle", 0, 200)
run = ag.add_state(ABP, MACHINE, "Run", 400, 200)
print("states:", idle, run)

# Assign animations to states (loop=True)
ag.set_state_animation(ABP, MACHINE, "Idle", "/Game/Anims/AS_Idle", True, 1.0)
ag.set_state_animation(ABP, MACHINE, "Run", "/Game/Anims/AS_Run", True, 1.0)

# Transition Idle -> Run (0.2s blend)
t = ag.add_transition(ABP, MACHINE, "Idle", "Run", 0.2)
print("transition:", t)

unreal.BlueprintService.compile_blueprint(ABP)
unreal.EditorAssetLibrary.save_asset(ABP)
