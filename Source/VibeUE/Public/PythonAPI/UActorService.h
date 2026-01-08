// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UActorService.generated.h"

/**
 * Information about an actor in the level
 */
USTRUCT(BlueprintType)
struct FLevelActorInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	FString ActorName;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	FString ActorLabel;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	FString ActorClass;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	FVector Location;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	FRotator Rotation;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	bool bIsHidden = false;
};

/**
 * Actor service exposed directly to Python.
 *
 * Python Usage:
 *   import unreal
 *
 *   # List all actors in level
 *   actors = unreal.ActorService.list_level_actors()
 *
 *   # Find actors by class
 *   players = unreal.ActorService.find_actors_by_class("BP_Player_Test")
 *
 * @note This replaces the JSON-based manage_actor MCP tool
 */
UCLASS(BlueprintType)
class VIBEUE_API UActorService : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * List all actors in the current level.
	 *
	 * @param ActorClassFilter - Optional filter by actor class name
	 * @param bIncludeHidden - Whether to include hidden actors
	 * @return Array of actor information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static TArray<FLevelActorInfo> ListLevelActors(
		const FString& ActorClassFilter = TEXT(""),
		bool bIncludeHidden = false);

	/**
	 * Find actors by class name.
	 *
	 * @param ClassName - Actor class name to search for
	 * @return Array of matching actors
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static TArray<FLevelActorInfo> FindActorsByClass(const FString& ClassName);

	/**
	 * Get detailed information about an actor by name or label.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param OutInfo - Structure containing actor details
	 * @return True if actor found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool GetActorInfo(const FString& ActorNameOrLabel, FLevelActorInfo& OutInfo);
};
