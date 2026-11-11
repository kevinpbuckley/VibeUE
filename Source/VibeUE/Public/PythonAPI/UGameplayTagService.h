// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UGameplayTagService.generated.h"

/**
 * Gameplay Tag Service - the Python FGameplayTag escape hatch, and nothing else.
 *
 * Cut to the delta on the 5-9 branch. Use the engine for everything else:
 *  - the whole tag surface -- registering, renaming, moving, removing, listing, inspecting and
 *    finding referencers: `GameplayTagsToolset` (11 tools: AddTag, RemoveTag, RenameTag, MoveTags,
 *    ListTags, ListTagsInSource, GetTagInfo, AddTagSource, ListTagSources, ListTagSourceRoots,
 *    FindReferencersByTag)
 *  - reading the tags on a live ASC: `GASToolsets.AbilitySystemInspectorToolset` (GetActiveTags),
 *    which answers in tag NAMES and so needs no tag value at all
 *
 * What is left is the one thing none of those can do, because a tool returns JSON while these
 * return a live struct VALUE into the calling Python session: Python cannot construct an
 * FGameplayTag (TagName is read-only, and MakeLiteralGameplayTag takes a tag, not a string), which
 * otherwise blocks every tag-typed engine API -- has_matching_gameplay_tag,
 * send_gameplay_event_to_actor, assign_tag_set_by_caller_magnitude, tag-keyed TMap authoring.
 * Request a tag here only to obtain its value for such a call.
 *
 * Python Usage:
 *   import unreal
 *
 *   tag = unreal.GameplayTagService.request_tag("Ability.Fire")
 *   asc.has_matching_gameplay_tag(tag)
 *
 *   c = unreal.GameplayTagService.request_tag_container(["State.Stunned", "State.Rooted"])
 */
UCLASS(BlueprintType)
class VIBEUE_API UGameplayTagService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// =================================================================
	// Query Operations
	// =================================================================

	/**
	 * Look up a registered gameplay tag by name and return the real FGameplayTag VALUE.
	 *
	 * This is the editor-scripting escape hatch: Python cannot construct an FGameplayTag
	 * (the struct's TagName is read-only and BlueprintGameplayTagLibrary::MakeLiteralGameplayTag
	 * takes a tag, not a string), which blocks every tag-typed engine API — e.g.
	 * SendGameplayEventToActor, HasMatchingGameplayTag, AssignTagSetByCallerMagnitude,
	 * and TMap<FGameplayTag, ...> authoring.
	 *
	 * Python:
	 *   tag = unreal.GameplayTagService.request_tag("Ability.Fire")
	 *   asc.has_matching_gameplay_tag(tag)
	 *
	 * @param TagName - Full tag name (redirects from renamed tags are applied)
	 * @return The registered tag, or an INVALID tag (is_valid() false) when the name is
	 *         not registered — never asserts.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|GameplayTags")
	static FGameplayTag RequestTag(const FString& TagName);

	/**
	 * Look up several registered tags at once and return them as a container.
	 * Unregistered names are skipped (see the log for which). Convenience for
	 * container-typed APIs (tag queries, RemoveActiveEffectsWithGrantedTags, ...).
	 *
	 * Python:
	 *   c = unreal.GameplayTagService.request_tag_container(["State.Stunned", "State.Rooted"])
	 *
	 * @param TagNames - Full tag names
	 * @return Container holding every name that resolved to a registered tag
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|GameplayTags")
	static FGameplayTagContainer RequestTagContainer(const TArray<FString>& TagNames);

};
