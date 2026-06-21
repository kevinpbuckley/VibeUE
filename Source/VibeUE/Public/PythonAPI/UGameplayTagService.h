// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UGameplayTagService.generated.h"

/**
 * Information about a single gameplay tag.
 * Python access: info = unreal.GameplayTagService.get_tag_info("Cube.StartChasing")
 *
 * Properties:
 * - tag_name (str): Full tag name (e.g., "Cube.StartChasing")
 * - comment (str): Developer comment associated with the tag
 * - source (str): Where the tag was defined (e.g., "DefaultGameplayTags.ini", "Native")
 * - is_explicit (bool): Whether this tag was explicitly defined (vs. implied parent)
 * - child_count (int): Number of direct child tags
 * - redirected_to (str): If the requested name was renamed, the tag it now redirects to
 *   (empty if no redirect). All other fields describe the redirect target in that case.
 */
USTRUCT(BlueprintType)
struct FVibeGameplayTagInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	FString TagName;

	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	FString RedirectedTo;

	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	FString Comment;

	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	FString Source;

	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	bool bIsExplicit = false;

	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	int32 ChildCount = 0;
};

/**
 * Result of a gameplay tag operation (add, remove, rename).
 * Python access: result = unreal.GameplayTagService.add_tag("Cube.StartChasing")
 *
 * Properties:
 * - success (bool): Whether the operation succeeded
 * - error_message (str): Error message if failed (empty if success)
 * - tags_modified (Array[str]): Tags that were successfully modified
 */
USTRUCT(BlueprintType)
struct FGameplayTagResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	FString ErrorMessage;

	UPROPERTY(BlueprintReadWrite, Category = "GameplayTags")
	TArray<FString> TagsModified;
};

/**
 * Gameplay Tag Service - Python API for managing Unreal Engine Gameplay Tags.
 *
 * Provides full CRUD operations for gameplay tags including:
 * - Listing and filtering tags
 * - Adding new tags (to INI config + runtime registration)
 * - Removing tags
 * - Renaming tags
 * - Querying tag hierarchy (children, parents)
 * - Checking tag existence
 *
 * Python Usage:
 *   import unreal
 *
 *   # List all tags
 *   tags = unreal.GameplayTagService.list_tags()
 *   for t in tags:
 *       print(f"{t.tag_name} ({t.source})")
 *
 *   # List tags matching a prefix
 *   cube_tags = unreal.GameplayTagService.list_tags("Cube")
 *
 *   # Add a single tag
 *   result = unreal.GameplayTagService.add_tag("Cube.StartChasing", "Event to start chasing")
 *   if result.success:
 *       print("Tag added!")
 *
 *   # Add multiple tags at once
 *   result = unreal.GameplayTagService.add_tags(["Cube.StartChasing", "Cube.StopChasing"], "Chase events")
 *
 *   # Check if a tag exists
 *   exists = unreal.GameplayTagService.has_tag("Cube.StartChasing")
 *
 *   # Get detailed tag info
 *   info = unreal.GameplayTagService.get_tag_info("Cube.StartChasing")  # info struct or None (NOT a tuple)
 *
 *   # Get direct children of a tag
 *   children = unreal.GameplayTagService.get_children("Cube")
 *
 * @note This service owns: has_tag, get_tag_info, get_children, add_tags (bulk).
 *       Single-tag create / rename / remove are the engine's GameplayTagsToolset —
 *       reach them with call_tool (run describe_toolset on the GameplayTags toolset for
 *       the exact action names). There is no rename_tag / remove_tag on this service.
 * @note Tags are written to DefaultGameplayTags.ini by default and registered at runtime.
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
	 * Check if a gameplay tag exists.
	 *
	 * @param TagName - Full tag name (e.g., "Cube.StartChasing")
	 * @return True if the tag is registered. NOTE: also true for the OLD name of a renamed
	 *         tag (renames register a redirect) — use get_tag_info(name).redirected_to to
	 *         tell a real tag from a redirected one.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|GameplayTags")
	static bool HasTag(const FString& TagName);

	/**
	 * Get detailed information about a specific tag.
	 *
	 * @param TagName - Full tag name
	 * @param OutInfo - Output structure with tag details
	 * @return True if tag was found
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|GameplayTags")
	static bool GetTagInfo(const FString& TagName, FVibeGameplayTagInfo& OutInfo);

	/**
	 * Get direct children of a tag.
	 *
	 * @param ParentTag - Parent tag name (e.g., "Cube" returns "Cube.StartChasing", "Cube.StopChasing")
	 * @return Array of child tag information
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|GameplayTags")
	static TArray<FVibeGameplayTagInfo> GetChildren(const FString& ParentTag);

#if WITH_EDITOR
	// =================================================================
	// Add Operations
	// =================================================================

	/**
	 * Add multiple gameplay tags at once.
	 *
	 * @param TagNames - Array of full tag names
	 * @param Comment - Optional developer comment (applied to all)
	 * @param TagSource - Config file source (default: "DefaultGameplayTags.ini")
	 * @return Operation result with list of tags modified
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|GameplayTags")
	static FGameplayTagResult AddTags(const TArray<FString>& TagNames, const FString& Comment = TEXT(""), const FString& TagSource = TEXT("DefaultGameplayTags.ini"));
#endif // WITH_EDITOR

private:
	/** Helper to populate FVibeGameplayTagInfo from the tag manager */
	static FVibeGameplayTagInfo BuildTagInfo(const FString& TagName);
};
