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
};

/**
 * Read and author Blackboard (UBlackboardData) assets.
 *
 * Separate from UBehaviorTreeService because UBlackboardData is a flat UDataAsset with no
 * graph, and is useful (and testable) on its own.
 */
UCLASS(BlueprintType)
class VIBEUE_API UBlackboardService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** All Blackboard assets under DirectoryPath, as package paths. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blackboard")
	static TArray<FString> ListBlackboards(const FString& DirectoryPath = TEXT("/Game"));

	/** Create a Blackboard asset. ParentBlackboardPath may be empty. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blackboard")
	static bool CreateBlackboard(const FString& AssetPath, const FString& ParentBlackboardPath);

	/** Every key on the asset, including keys inherited from its parent. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blackboard")
	static TArray<FBBKeyInfo> GetBlackboardKeys(const FString& AssetPath);

	/**
	 * Add a key. KeyType is one of Bool, Int, Float, String, Name, Vector, Rotator,
	 * Object, Class, Enum, NativeEnum. Returns an empty string on success, otherwise the error.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blackboard")
	static FString AddBlackboardKey(const FString& AssetPath, const FString& KeyName,
		const FString& KeyType, bool bInstanceSynced = false);

	/** Set the base class of an Object/Class key, or the enum of an Enum key. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blackboard")
	static FString SetBlackboardKeyObjectClass(const FString& AssetPath, const FString& KeyName,
		const FString& ClassOrEnumPath);

	/** Remove a key. Returns an empty string on success, otherwise the error. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blackboard")
	static FString RemoveBlackboardKey(const FString& AssetPath, const FString& KeyName);

	/**
	 * Rename a key. Returns the list of BT nodes whose key selectors referenced the old name —
	 * key selectors resolve by name, so those bindings break and must be re-pointed.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blackboard")
	static TArray<FString> RenameBlackboardKey(const FString& AssetPath, const FString& OldName,
		const FString& NewName);
};
