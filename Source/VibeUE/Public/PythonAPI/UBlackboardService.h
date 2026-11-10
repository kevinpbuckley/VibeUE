// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UBlackboardService.generated.h"

/**
 * Read and author Blackboard (UBlackboardData) assets.
 *
 * Separate from UBehaviorTreeService because UBlackboardData is a flat UDataAsset with no
 * graph, and is useful (and testable) on its own.
 *
 * Writes hold to the same discipline as the BT service: refused while a Play In Editor session
 * is running (live UBlackboardComponents hold key IDs and value offsets computed at init, and
 * mutating Keys under them type-confuses every later read) and while a Blackboard editor is
 * open on the asset (its copy would silently overwrite the change on the human's next save).
 * Every key mutation runs the engine's own post-edit fix-ups — the ones
 * UBlackboardData::PostEditChangeProperty runs for a details-panel edit — so loaded child
 * blackboards, synchronized-key state and editor listeners stay coherent.
 */
UCLASS(BlueprintType)
class VIBEUE_API UBlackboardService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Every BT node key selector bound to KeyName, across every Behavior Tree whose blackboard is
	 * this asset or inherits from it, formatted "<btPath>:<node>.<property>". Read-only: the
	 * question RenameBlackboardKey answers after the fact, answerable before deciding to rename
	 * or remove. A failure returns a single entry prefixed "ERROR: ".
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blackboard")
	static TArray<FString> FindBlackboardKeyReferences(const FString& AssetPath,
		const FString& KeyName);

	/**
	 * Rename a key. Returns the list of BT nodes whose key selectors referenced the old name —
	 * key selectors resolve by name, so those bindings break and must be re-pointed.
	 *
	 * A rejected rename (missing/inherited OldName, name collision, or save failure) returns a
	 * single entry prefixed "ERROR: ", distinguishable from a real reference (always formatted
	 * "<btPath>:<node>.<property>", where btPath always starts with a content root such as
	 * "/Game/"). A successful rename that nothing referenced returns a plain empty array.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blackboard")
	static TArray<FString> RenameBlackboardKey(const FString& AssetPath, const FString& OldName,
		const FString& NewName);
};
