# learn_constraints.pyx — Derive realistic bone ranges by analyzing existing animations.
#
# Sample script for the animation-editing skill. Run via execute_python_code.
import unreal
sks = unreal.SkeletonService

SKELETON = "/Game/Characters/SK_Mannequin"
MAX_ANIMS = 100
SAMPLES_PER = 20

c = sks.learn_from_animations(SKELETON, MAX_ANIMS, SAMPLES_PER)
print("analyzed", c.animation_count, "animations,", c.total_samples, "samples")
for r in c.bone_ranges:
    if "arm" in r.bone_name.lower():
        print(f"{r.bone_name}: safe {r.percentile_5} -> {r.percentile_95} ({r.sample_count} samples)")
