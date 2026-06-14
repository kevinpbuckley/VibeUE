# add_tags.pyx — Register gameplay tags and list them.
#
# Sample script for the gameplay-tags skill. Run via execute_python_code.
import unreal
gt = unreal.GameplayTagService

for tag in ["Ability.Attack.Melee", "Ability.Attack.Ranged", "State.Stunned"]:
    r = gt.add_tag(tag, comment="added by sample script")
    print("add", tag, "->", r)

print("has Ability.Attack.Melee:", gt.has_tag("Ability.Attack.Melee"))
print("tags:", [str(t) for t in gt.list_tags("Ability")][:10])
