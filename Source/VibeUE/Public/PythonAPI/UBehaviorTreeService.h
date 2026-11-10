// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UBehaviorTreeService.generated.h"

/** Summary of a Behavior Tree asset, returned by the internal GetBehaviorTreeInfo fixture. */
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
 * Behavior Tree Service - one repair capability the engine does not provide.
 *
 * Cut to the delta on the 5-9 branch. UE 5.9's `AIModuleToolset.BehaviorTreeToolset` (25 tools)
 * owns Behavior Tree authoring; use it for everything else:
 *  - create the asset: `CreateBehaviorTree`, `CreateBlackboardData`
 *  - build the tree: `AddRootComposite`, `AddComposite`, `AddTask`, `AddDecorator`,
 *    `AddRootDecorator`, `AddService`
 *  - read it: `GetRootGraphNode`, `GetChildGraphNodes`, `GetDecoratorGraphNodes`,
 *    `GetServiceGraphNodes`, `GetSubtree`, `GetNodeInstance`
 *  - rearrange or remove: `MoveGraphNode`, `MoveSubnode`, `RemoveGraphNode`, `RemoveSubnode`
 *  - blackboard wiring: `SetBlackboard`, `GetBlackboard`, `AddBlackboardKey`,
 *    `RemoveBlackboardKey`, `GetBlackboardKeyNames`, `GetAllowedBlackboardKeys`,
 *    `SetNodeBlackboardKey`
 *  - node properties (names, comments, any field): `EditorToolset.ObjectToolset`
 *    (`GetProperties` / `SetProperties` / `ListProperties`) on the node from `GetNodeInstance`
 *  - available node classes: `ObjectToolset.SearchSubclasses`, or `search_symbols` / `get_symbol`
 *  - find assets, save, validate: `EditorToolset.AssetToolset` (`FindAssets`, `SaveAsset`,
 *    `ValidateAssets`). Epic's BT tools only mark the package dirty, so saving through
 *    `AssetToolset.SaveAsset` is what commits and compiles the tree -- VibeUE's old
 *    `CompileAndSave` was a convenience, not a capability.
 *
 * What is left is the one thing none of those can do.
 */
UCLASS(BlueprintType)
class VIBEUE_API UBehaviorTreeService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// ---- Internal fixtures: NOT AI tools (see the class comment). Kept for the repair tests.
	UFUNCTION(BlueprintCallable, Category = "VibeUE|BehaviorTree")
	static FString CreateBehaviorTree(const FString& AssetPath, const FString& BlackboardAssetPath = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "VibeUE|BehaviorTree")
	static bool GetBehaviorTreeInfo(const FString& AssetPath, FBTAssetInfo& OutInfo);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|BehaviorTree")
	static FString SetBlackboardAsset(const FString& AssetPath, const FString& BlackboardAssetPath);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|BehaviorTree")
	static FString CompileAndSave(const FString& AssetPath);

	/**
	 * Rebuild a missing or sparse editor graph from the asset's runtime node tree, then save.
	 *
	 * For assets whose graph holds nothing but its root while the runtime tree is intact: they
	 * display as empty in the Behavior Tree editor, and any ordinary write to them is refused,
	 * because committing that graph would overwrite the runtime tree with an empty one. Epic's
	 * toolset has no equivalent -- it edits the graph it finds.
	 *
	 * Explicit and never implicit: no other entry point calls this, because rebuilding a
	 * production asset's graph as a side effect of an unrelated edit is exactly the kind of
	 * unrequested write this service exists to prevent. Refuses when there is nothing to repair --
	 * no runtime tree, or a graph that already has nodes under its root. The before/after node
	 * counts are logged.
	 *
	 * Returns an empty string on success, else the error.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString RepairGraphFromRuntimeTree(const FString& AssetPath);
};
