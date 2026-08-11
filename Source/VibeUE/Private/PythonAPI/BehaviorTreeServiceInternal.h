// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UBehaviorTree;
class UBehaviorTreeGraph;
class UBehaviorTreeGraphNode;

// Real type is Editor/AIGraph's AIGraphTypes.h (global scope, not namespaced). Forward-declared
// here at global scope so the elaborated-type-specifier below resolves to it rather than
// injecting a distinct, incomplete VibeBT::FGraphNodeClassHelper.
struct FGraphNodeClassHelper;

/**
 * Shared internals for UBehaviorTreeService. Private to the module.
 */
namespace VibeBT
{
	/** Horizontal gap between sibling columns, in graph units. */
	constexpr int32 NodeSpacingX = 300;

	/** Vertical gap between tree depths, in graph units. */
	constexpr int32 NodeSpacingY = 180;

	/** Structure-only mirror of a BT subtree, so layout can be tested without an editor. */
	struct FLayoutNode
	{
		TArray<FLayoutNode> Children;
	};

	/**
	 * Assign a position to every node in the tree.
	 *
	 * Y is depth * NodeSpacingY. X is a tidy-tree layout over leaf columns, which guarantees
	 * that siblings have strictly increasing X and that sibling subtrees never overlap —
	 * the property BT execution order depends on.
	 *
	 * Returned positions are index-aligned with a pre-order walk of Root.
	 */
	TArray<FIntPoint> ComputeLayout(const FLayoutNode& Root);

	/**
	 * Apply ComputeLayout to a live BT EdGraph, walking children through output-pin links
	 * in their current LinkedTo order, and writing NodePosX / NodePosY back.
	 *
	 * Replaces UBehaviorTreeGraph::AutoArrange(), which dereferences the Slate node widget
	 * and crashes when no Behavior Tree editor tab is open.
	 */
	void ArrangeGraph(UBehaviorTreeGraphNode* RootNode);

	/** Shared asset-registry sweep used by both AI services. */
	TArray<FString> ListAssetsOfClass(const UClass* Class, const FString& DirectoryPath);

	/**
	 * Create Tree->BTGraph and its root node if either is missing, and leave the tree's
	 * blackboard assignment exactly as it found it.
	 *
	 * UBehaviorTreeFactory leaves BTGraph null; FBehaviorTreeEditor builds it lazily the first
	 * time a human opens the asset (BehaviorTreeEditor.cpp:345-361). A tree created or read
	 * headlessly therefore has nothing to write to until this has run.
	 *
	 * Idempotent: a no-op (no Modify, no dirtying) once both the graph and its root exist.
	 * Returns an empty string on success, otherwise the error.
	 */
	FString EnsureGraph(UBehaviorTree* Tree);

	/**
	 * Load AssetPath for writing: resolve the tree, make sure it has a graph, and refuse when
	 * the write could not be trusted to land. Returns an empty string on success (OutTree and
	 * OutGraph set), otherwise the error (both out-params left null).
	 *
	 * Refuses when a Behavior Tree editor is open on the asset — the open editor holds its own
	 * EdGraph state, will not show the change, and overwrites it on the human's next save — and
	 * when the graph is locked, because UpdateAsset() early-returns under bLockUpdates
	 * (BehaviorTreeGraph.cpp:104) and the commit would report success having regenerated nothing.
	 */
	FString OpenWriteGuard(const FString& AssetPath, UBehaviorTree*& OutTree,
		UBehaviorTreeGraph*& OutGraph);

	/**
	 * Lay the graph out, regenerate the runtime tree from it, and save the package to disk.
	 * Returns an empty string on success, otherwise the error.
	 */
	FString CommitGraph(UBehaviorTree* Tree, UBehaviorTreeGraph* Graph);

	/**
	 * Resolve a BT node class by short name ("BTTask_MoveTo"), generated-class name
	 * ("BTT_ChaseTarget_C") or full object path, and verify it derives from RequiredBase.
	 * Returns nullptr if unresolved or of the wrong base.
	 */
	UClass* ResolveNodeClass(const FString& ClassName, UClass* RequiredBase);

	/**
	 * Cached FGraphNodeClassHelper for one base class, primed so Blueprint-derived classes
	 * are reported. Priming is FGraphNodeClassHelper::AddObservedBlueprintClasses(Base)
	 * followed by UpdateAvailableBlueprintClasses(); without it only native classes appear.
	 */
	TSharedPtr<struct FGraphNodeClassHelper> GetClassHelper(UClass* BaseClass);
}
