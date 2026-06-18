// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UVibeUESkillsToolset.generated.h"

/**
 * VibeUE skill packs — lazy-loaded domain knowledge for Unreal Engine workflows
 * (Blueprints, materials, landscape, animation, Niagara, UMG widgets, gameplay tags, and more).
 *
 * Each pack is authored as markdown under the plugin's Content/Skills/<name>/SKILL.md. Call
 * ListSkills to see what's available with one-line descriptions, then LoadSkills to pull the full
 * guidance for only the packs you need (keeps context small). This is the AICallable equivalent of
 * Unreal 5.8's native agent-skill discovery, served directly from VibeUE's markdown packs.
 */
UCLASS(BlueprintType)
class VIBEUE_API UVibeUESkillsToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * List all available VibeUE skill packs.
	 *
	 * @return A map of skill name -> one-line description. Pass a name to LoadSkills for the full guidance.
	 */
	UFUNCTION(meta = (AICallable), Category = "VibeUE|Skills")
	static TMap<FString, FString> ListSkills();

	/**
	 * Load the full markdown guidance for one or more skill packs.
	 *
	 * @param SkillNames Skill names from ListSkills. Use "skill/section" to load a sibling sub-doc
	 *                   (e.g. "blueprint-graphs/build-graph", "state-trees/api-reference").
	 * @return A map of the requested name -> full markdown content (or a not-found message).
	 */
	UFUNCTION(meta = (AICallable), Category = "VibeUE|Skills")
	static TMap<FString, FString> LoadSkills(const TArray<FString>& SkillNames);
};
