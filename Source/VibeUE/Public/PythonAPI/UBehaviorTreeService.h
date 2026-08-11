// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UBehaviorTreeService.generated.h"

/** One BT node class available for AddNode / AddDecorator / AddService. */
USTRUCT(BlueprintType)
struct FBTNodeClassInfo
{
	GENERATED_BODY()

	/** Class name as passed to AddNode, e.g. "BTComposite_Selector" or "BTT_ChaseTarget". */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString ClassName;

	/** Full object path of the class. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString ClassPath;

	/** Author-declared category shown in the node picker. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString Category;

	/** True if this is a Blueprint-derived node class rather than a native one. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	bool bIsBlueprint = false;
};

/** Summary of a Behavior Tree asset. */
USTRUCT(BlueprintType)
struct FBTAssetInfo
{
	GENERATED_BODY()

	/** Package path of the tree's blackboard, empty if none is assigned. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString BlackboardPath;

	/** Number of nodes in the editor graph, including decorators and services. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	int32 NodeCount = 0;

	/** False if BTGraph is null — the asset has never been opened or created by this service. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	bool bHasGraph = false;

	/** Whether the graph contains a root node. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	bool bHasRootNode = false;

	/** Populated when the asset could not be read. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString Error;
};

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

	/**
	 * BT node classes available in one category: "Composite", "Task", "Decorator", "Service".
	 * Includes Blueprint-derived classes. An unrecognised category returns an empty array.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static TArray<FBTNodeClassInfo> GetAvailableNodeTypes(const FString& Category);

	// =================================================================
	// Asset lifecycle
	// =================================================================

	/**
	 * Create a Behavior Tree asset, its editor graph and its root node.
	 * BlackboardAssetPath may be empty. Returns an empty string on success, else the error.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString CreateBehaviorTree(const FString& AssetPath, const FString& BlackboardAssetPath);

	/** Summary of a Behavior Tree asset. Returns false if the asset could not be loaded. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static bool GetBehaviorTreeInfo(const FString& AssetPath, FBTAssetInfo& OutInfo);

	/** Point the tree at a blackboard asset. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString SetBlackboardAsset(const FString& AssetPath, const FString& BlackboardAssetPath);

	/** Re-run layout, regenerate the runtime tree from the graph, and save. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString CompileAndSave(const FString& AssetPath);

	/**
	 * Rebuild a missing or sparse editor graph from the asset's runtime node tree, then save.
	 *
	 * For assets whose graph holds nothing but its root while the runtime tree is intact: they
	 * display as empty in the Behavior Tree editor, and any ordinary write to them is refused,
	 * because committing that graph would overwrite the runtime tree with an empty one.
	 *
	 * Explicit and never implicit: no other entry point calls this, because rebuilding a
	 * production asset's graph as a side effect of an unrelated edit is exactly the kind of
	 * unrequested write this service exists to prevent. Refuses when there is nothing to repair —
	 * no runtime tree, or a graph that already has nodes under its root. The before/after node
	 * counts are logged.
	 *
	 * Returns an empty string on success, else the error.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString RepairGraphFromRuntimeTree(const FString& AssetPath);
};
