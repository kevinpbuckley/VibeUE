// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UBehaviorTreeGraphNode;

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
}
