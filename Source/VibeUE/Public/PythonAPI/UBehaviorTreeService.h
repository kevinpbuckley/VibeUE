// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UBehaviorTreeService.generated.h"

/**
 * Read and author Behavior Tree assets.
 *
 * All writes go through the editor EdGraph (UBehaviorTree::BTGraph) and are committed with
 * UBehaviorTreeGraph::OnSave(), which regenerates the runtime UBehaviorTree::RootNode.
 * Writing RootNode directly reads back correctly and is silently discarded on the next
 * graph update, so it is never done here.
 */
UCLASS(BlueprintType)
class VIBEUE_API UBehaviorTreeService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// =================================================================
	// Discovery
	// =================================================================

	/** All Behavior Tree assets under DirectoryPath, as package paths. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static TArray<FString> ListBehaviorTrees(const FString& DirectoryPath = TEXT("/Game"));
};
