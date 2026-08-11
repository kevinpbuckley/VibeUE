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
 */
UCLASS(BlueprintType)
class VIBEUE_API UBlackboardService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** All Blackboard assets under DirectoryPath, as package paths. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Blackboard")
	static TArray<FString> ListBlackboards(const FString& DirectoryPath = TEXT("/Game"));
};
