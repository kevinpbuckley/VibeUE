// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UBlackboardService.generated.h"

/** One blackboard key as reported by GetBlackboardKeys. */
USTRUCT(BlueprintType)
struct FBBKeyInfo
{
	GENERATED_BODY()

	/** Key name as referenced by BT node key selectors. */
	UPROPERTY(BlueprintReadOnly, Category = "Blackboard")
	FString Name;

	/** Bool, Int, Float, String, Name, Vector, Rotator, Object, Class, Enum, NativeEnum. */
	UPROPERTY(BlueprintReadOnly, Category = "Blackboard")
	FString Type;

	/** Whether the key is synchronised across instances of this blackboard. */
	UPROPERTY(BlueprintReadOnly, Category = "Blackboard")
	bool bInstanceSynced = false;

	/** True if the key comes from the parent blackboard rather than this asset. */
	UPROPERTY(BlueprintReadOnly, Category = "Blackboard")
	bool bInherited = false;

	/** Base class for Object/Class keys, or the enum path for Enum keys. Empty otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Blackboard")
	FString ObjectClassPath;

	/** The details-panel category the key is grouped under. Empty when uncategorised. */
	UPROPERTY(BlueprintReadOnly, Category = "Blackboard")
	FString Category;

	/** The key's free-text description. Empty when none was written. */
	UPROPERTY(BlueprintReadOnly, Category = "Blackboard")
	FString Description;
};

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
	// ---- Internal fixtures: NOT AI tools. Blackboard CRUD belongs to the engine
	// (AIModuleToolset.BehaviorTreeToolset -> CreateBlackboardData, AddBlackboardKey,
	// RemoveBlackboardKey, GetBlackboardKeyNames, GetAllowedBlackboardKeys, SetBlackboard).
	// These stay BlueprintCallable-only so the rename/reference tests can build a board.
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blackboard")
	static bool CreateBlackboard(const FString& AssetPath, const FString& ParentBlackboardPath = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blackboard")
	static TArray<FBBKeyInfo> GetBlackboardKeys(const FString& AssetPath);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blackboard")
	static FString AddBlackboardKey(const FString& AssetPath, const FString& KeyName,
		const FString& KeyType, bool bInstanceSynced = false);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Blackboard")
	static FString SetBlackboardKeyObjectClass(const FString& AssetPath, const FString& KeyName,
		const FString& ClassOrEnumPath);

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
