---
name: gas
display_name: Gameplay Ability System (GAS)
description: Author and PIE-verify Gameplay Ability System content — abilities, attribute sets, gameplay effects, gameplay events, costs/cooldowns, cues — by composing Epic's GASToolsets/GameplayTagsToolset with Blueprint/Object tools and Python. Use when the user asks to add abilities, attributes, damage/buff effects, cooldowns, gameplay cues, or to inspect a live ASC.
vibeue_classes:
  - GameplayTagService
  - BlueprintService
unreal_classes:
  - UAbilitySystemComponent
  - UGameplayAbility
  - UGameplayEffect
  - UAttributeSet
  - UAbilitySystemBlueprintLibrary
keywords:
  - gameplay ability system
  - GAS
  - ability system component
  - ASC
  - attribute set
  - gameplay effect
  - gameplay event
  - gameplay cue
  - cooldown
  - cost
---

> 🧠 **Brains complement:** IF an `unreal-engine-skills-manager` tool (external MCP) exists in this session, call it with `{action: "load", skill: "gameplay-ability-system"}` for UE domain knowledge (ASC setup, attribute/effect/ability APIs, networking) — treat it as the rubric for any review / "best practices" question. If no such tool is available, skip this line entirely and proceed with this skill alone — do NOT attempt the call.

# Gameplay Ability System Skill

**Compose, do not duplicate.** Everything GAS needs already exists across Epic's engine toolsets,
the generic Blueprint/Object tools, and Python. VibeUE adds no GAS wrapper service; this skill is
the routing map plus the live-verified gotchas.

## Ownership matrix — who does what

| Operation | Owner |
|---|---|
| Inspect a live ASC (PIE): attributes, active tags, active effects, granted abilities | Epic **`GASToolsets.AbilitySystemInspectorToolset`** → `GetAttributeValues` / `GetActiveTags` / `GetActiveEffects` / `GetGrantedAbilities`. Input is `{"actor": {"refPath": "<actor path>"}}` — works on PIE actors, no selection needed |
| Discover attribute set classes / attributes | Epic **`GASToolsets.AttributeSetToolset`** → `FindAttributeSetClasses` / `ListAttributes` |
| Gameplay Cue notify assets + cue tags | Epic **`GASToolsets.GameplayCueToolset`** (list/get/create/execute-on-selected) |
| Tag CRUD (add/remove/rename/list single tags) | Epic **`GameplayTagsToolset`** — see the `gameplay-tags` skill |
| Bulk tag registration, hierarchy queries, existence checks | `unreal.GameplayTagService` (VibeUE delta — see `gameplay-tags`) |
| Create GameplayAbility / GameplayEffect / AttributeSet **Blueprint** subclasses, CDO config | Epic `BlueprintTools` + `ObjectTools` (or native Python `BlueprintFactory` with the right `ParentClass`) |
| Ability / cue Blueprint graphs | Epic `BlueprintTools` + `unreal.BlueprintService.build_graph` (see `blueprint-graphs`) |
| GameplayEffect legacy tag containers (Asset/Target Tags) | `unreal.BlueprintService.set_property` — maps them onto the UE 5.3+ GE Components (VibeUE #539/#541) |
| Runtime PIE testing | `EditorToolset.EditorAppToolset` StartPIE/StopPIE + Python on live actors (below) |
| Native C++ attribute sets / effects / abilities | **Coding-agent handoff** — C++ patterns below are for that agent, not for editor tools |

## Python exposure facts (live-verified, UE 5.8)

- `UAbilitySystemBlueprintLibrary` is exposed as **`unreal.AbilitySystemLibrary`** (the
  "BlueprintLibrary" name does not exist in Python).
  `asc = unreal.AbilitySystemLibrary.get_ability_system_component(actor)`.
- **`FGameplayTag` cannot be constructed from raw Python**: `unreal.GameplayTag(tag_name=...)` is
  rejected, `TagName` is read-only via `set_editor_property`, and
  `GameplayTagLibrary.make_literal_gameplay_tag` takes an FGameplayTag (circular).
  **Use VibeUE's escape hatch**: `unreal.GameplayTagService.request_tag("A.B.C")` returns the real
  registered tag VALUE (invalid tag for unknown names — check with
  `GameplayTagLibrary.is_gameplay_tag_valid`), and `request_tag_container([...])` batches into a
  container. That unlocks every tag-typed API: `asc.has_matching_gameplay_tag`,
  `send_gameplay_event_to_actor`, `assign_tag_set_by_caller_magnitude`, tag-keyed `TMap`
  authoring, and tag queries. For pure READS the Epic inspector `GetActiveTags` also returns tag
  names as strings without needing tag values.
- `asc.get_owned_gameplay_tags()` is NOT exposed. Use the inspector.
- ASC `BlueprintCallable`s that take a **class** work fine:
  `asc.try_activate_ability_by_class(unreal.MyGA_Class)` — this is the reliable Python path to
  drive abilities in PIE (it bypasses any component-level gates, which makes tests deterministic).
- Multiple activations inside ONE `execute_python_code` call happen in the same frame — no
  timers tick and no GE durations expire between them. Exploit this to hit caps/lockouts
  deterministically instead of sleeping wall-clock time.

## PIE verification recipe (one round-trip per step)

```python
import unreal, json
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
ref = json.dumps({"actor": {"refPath": pawn.get_path_name()}})
for tool in ["GetAttributeValues", "GetActiveTags", "GetActiveEffects", "GetGrantedAbilities"]:
    r = unreal.ToolsetRegistry.execute_tool("GASToolsets.AbilitySystemInspectorToolset", tool, ref)
    print(tool, r.get_value_as_json_string())
# drive an ability and re-inspect IN THE SAME CALL for deterministic assertions
asc = unreal.AbilitySystemLibrary.get_ability_system_component(pawn)
asc.try_activate_ability_by_class(unreal.MyGA_Fire)
```

Damage through the classic funnel also exercises GE pipelines when the project routes
`TakeDamage` into effects: `unreal.GameplayStatics.apply_damage(actor, 25.0, None, None, None)`.

## C++ authoring gotchas (for the coding-agent handoff)

- **Never call `UGameplayEffect::FindOrAddComponent`/`AddComponent` in a GE constructor** — they
  `NewObject` with an empty name, which fatal-asserts at CDO construction and kills the editor on
  module load. In constructors use
  `CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"))`, call
  `SetAndApplyTargetTagChanges(...)`, then `GEComponents.Add(...)` (protected — do it inside the
  class).
- SetByCaller cost/cooldown GEs: `CommitAbility` alone applies a **0-magnitude** spec and logs a
  warning. Override `ApplyCost`/`ApplyCooldown`, build the spec with
  `MakeOutgoingGameplayEffectSpec`, `SetSetByCallerMagnitude(tag, value)`, then
  `ApplyGameplayEffectSpecToOwner`.
- `AbilityTags` is deprecated in 5.5+ — call `SetAssetTags(container)` in the constructor.
- `NonInstanced` instancing is deprecated — default to `InstancedPerActor`.
- `UAbilitySystemGlobals::Get().InitGlobalData()` must run once at startup (GameInstance::Init /
  AssetManager) or target data and montage prediction fail silently.
- Set `[/Script/GameplayAbilities.AbilitySystemGlobals]` `+GameplayCueNotifyPaths=/Game/GameplayCues`
  in DefaultGame.ini, or the cue manager scans all of /Game/ (startup warning + cost).

## Verification rules

After authoring: compile (`unreal.BlueprintEditorLibrary.compile_blueprint`), then prove behavior
in PIE with the inspector — a successful tool call is not evidence. Attribute deltas, active-tag
presence, and effect duration/stack readouts are the evidence to report.
