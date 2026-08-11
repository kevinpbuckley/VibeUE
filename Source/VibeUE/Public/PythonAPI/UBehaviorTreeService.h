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

/** One node in a Behavior Tree's editor graph, addressed by path. */
USTRUCT(BlueprintType)
struct FBTNodeInfo
{
	GENERATED_BODY()

	/** Path this node resolves from, e.g. "Root/Selector/Sequence[1]/Wait". */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString Path;

	/** The graph node's stable GUID. Unlike Path, it does not change when siblings are inserted. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString Guid;

	/** Node instance class name, e.g. "BTComposite_Selector". Empty for the root node. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString ClassName;

	/** Display name, i.e. the node's title in the graph. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString NodeName;

	/** Number of child nodes, in execution order. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	int32 ChildCount = 0;

	/** Number of decorators attached to this node. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	int32 DecoratorCount = 0;

	/** Number of services attached to this node. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	int32 ServiceCount = 0;

	/** True when the node was injected from a subtree and must not be edited. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	bool bInjected = false;

	/** True when one of the decorators is a composite (logic-operator) decorator. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	bool bHasCompositeDecorator = false;

	/** Populated when the path could not be resolved. */
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

	// =================================================================
	// Structure — read
	// =================================================================

	/**
	 * The whole tree as JSON, rooted at the graph's root node. Per node: "path", "guid", "class",
	 * "name", "children" (in execution order), "decorators", "services", "properties" (edited,
	 * non-default values only), "bInjected", "bHasCompositeDecorator".
	 *
	 * Read-only: unlike the mutators it never creates the editor graph, so an asset that has never
	 * been opened reads back as an "error" field rather than being silently written to. The result
	 * always parses, and always has a "children" array, error or not.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString GetTree(const FString& AssetPath);

	/**
	 * One node, addressed by path. Returns false — with OutInfo.Error explaining why — when the
	 * path names nothing or is ambiguous, so that "no such node" can never be mistaken for a node
	 * whose fields happen to be empty.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static bool GetNodeInfo(const FString& AssetPath, const FString& NodePath, FBTNodeInfo& OutInfo);

	// =================================================================
	// Structure — write
	// =================================================================

	/**
	 * Add a composite or task node under ParentNodePath and save.
	 *
	 * ChildIndex is the insert position among the parent's existing children; < 0 appends. Returns
	 * the new node's path, or a string starting with "ERROR: ".
	 *
	 * Note that the returned path is positional: adding or removing a same-named sibling later
	 * changes what it resolves to.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString AddNode(const FString& AssetPath, const FString& ParentNodePath,
		const FString& NodeClassName, int32 ChildIndex);

	/**
	 * Remove a node and its subtree, and save. Returns an empty string on success, else the error.
	 *
	 * The whole subtree goes, not just the one node: a child left disconnected would be unreachable
	 * from "Root", so no path could ever name it again, and it would sit in the asset forever as
	 * invisible weight.
	 *
	 * Refuses on the root node, and on the root's only child — removing that would leave the tree
	 * with no runtime root at all, which CommitGraph exists to refuse.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString RemoveNode(const FString& AssetPath, const FString& NodePath);

	/**
	 * Re-parent a node (with its subtree) to NewChildIndex under NewParentPath, and save.
	 * NewChildIndex < 0 appends. Returns an empty string on success, else the error.
	 *
	 * Refuses to move the root, and to move a node under itself or one of its own descendants.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString MoveNode(const FString& AssetPath, const FString& NodePath,
		const FString& NewParentPath, int32 NewChildIndex);
};
