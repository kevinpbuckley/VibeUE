// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FProperty;
class UBehaviorTree;
class UBehaviorTreeGraph;
class UBehaviorTreeGraphNode;
class UBehaviorTreeGraphNode_Root;

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
	 * The graph's root node, or nullptr. A linear sweep of Nodes is the whole search: BT sub-nodes
	 * (decorators, services) live in UAIGraphNode::SubNodes and are never added to UEdGraph::Nodes
	 * (AIGraphNode.cpp, UAIGraphNode::AddSubNode).
	 */
	UBehaviorTreeGraphNode_Root* FindRootGraphNode(UBehaviorTreeGraph* Graph);

	/**
	 * Node's children, in current output-pin link order — which, on a graph committed through
	 * CommitGraph (and therefore ArrangeGraph), is BT execution order.
	 *
	 * Neither deduplicated nor cycle-guarded: it reports exactly what the pins say, so callers that
	 * walk the whole graph can decide for themselves what to do about a malformed one.
	 */
	TArray<UBehaviorTreeGraphNode*> GetChildNodes(const UBehaviorTreeGraphNode* Node);

	/** Node's parent through its input pin, or nullptr for the root (and for orphaned nodes). */
	UBehaviorTreeGraphNode* GetParentNode(const UBehaviorTreeGraphNode* Node);

	/**
	 * The path that ResolveNodePath will resolve back to Node, e.g.
	 * "Root/Selector[0]/Sequence[1]/Wait[0]". Empty when Node is not reachable from the graph root.
	 *
	 * Segments are display names (UEdGraphNode::GetNodeTitle(ListView)) followed by an index among
	 * the same-named siblings. The index is always emitted here — resolution treats it as optional,
	 * but an issued path that omitted it would stop resolving as soon as a same-named sibling
	 * appeared beside it. Sub-nodes get an "@decorator[n]" / "@service[n]" final segment.
	 *
	 * Paths are positional, not identities: inserting or removing a sibling before this one changes
	 * what a previously returned path resolves to. Callers holding a path across a structural edit
	 * must treat it the way they would a list index; FBTNodeInfo::Guid is the stable identity.
	 */
	FString GetNodePath(const UBehaviorTreeGraphNode* Node);

	/**
	 * Resolve a "/"-separated node path against Graph, or nullptr when it names nothing or is
	 * ambiguous. The first segment is the literal keyword "Root" (case-insensitive), matching the
	 * root graph node whatever its title; see GetNodePath for the rest of the grammar.
	 */
	UBehaviorTreeGraphNode* ResolveNodePath(UBehaviorTreeGraph* Graph, const FString& Path);

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

	/**
	 * Whether this service treats Property as one of the node's *properties* — the single
	 * definition shared by GetTree's "properties" map, GetNodePropertyNames, GetNodePropertyValue,
	 * SetNodePropertyValue and SetNodeBlackboardKey.
	 *
	 * It is "what a human could change in the details panel": CPF_Edit, minus EditConst, Transient
	 * and Deprecated. Everything else is either structure this service already describes another way
	 * (a composite's Children array, a node's ParentNode back pointer), or derived state that
	 * CommitGraph's UpdateAsset regenerates from the graph anyway (ExecutionIndex, MemoryOffset,
	 * TreeDepth) — writing one of those reads back correct and is gone by the next commit, which is
	 * precisely the failure mode this service exists to refuse.
	 */
	bool IsAuthorableProperty(const FProperty* Property);

	/**
	 * Export Property's current value on Instance against NO defaults, which is as close to a full
	 * literal as UE's text export gets. Measured behaviour, in this engine version:
	 *
	 *  - Passing a default container (the CDO, say) makes FProperty::ExportText_InContainer emit
	 *    nothing at all for any property that matches it. A node whose WaitTime happens to equal its
	 *    class default would then be reported as "" or "()" — not a value, and not something
	 *    SetNodePropertyValue could write back. Passing null is what keeps a value a value.
	 *  - Passing the *instance itself* as the default container — the shape that is easiest to write
	 *    by accident, since the container argument is already in hand — is the same bug at full
	 *    strength: every member equals itself, so a struct exports as "()" and reads back as
	 *    "unchanged".
	 *  - What null defaults does NOT buy is a complete struct literal. UScriptStruct::ExportText
	 *    substitutes a default-CONSTRUCTED struct when it is handed no defaults, so struct members
	 *    still at their zero value are omitted whatever is passed here (Class.cpp:3560-3600). That
	 *    is the engine's own copy/paste encoding, and it means a value read from one node and
	 *    written to another merges rather than copies: the omitted members keep the target's
	 *    values. Round-tripping a value onto the node it came from is exact; carrying one across
	 *    nodes is not, and no port flag available here changes that (PPF_ExternalEditor would, but
	 *    it also switches member names to authored names, which Blueprint-defined structs do not
	 *    import back).
	 *  - An empty array exports as an empty string rather than "()" — FArrayProperty emits the
	 *    opening parenthesis with its first element and nothing at all when there are none
	 *    (PropertyArray.cpp:1063-1116).
	 */
	void ExportPropertyValue(const FProperty* Property, const UObject* Instance, FString& OutValue);

	/**
	 * The authorable property named PropertyName on Instance, or nullptr with OutError explaining
	 * whether the name is unknown or merely not authorable — two different mistakes.
	 */
	FProperty* FindAuthorableProperty(const UObject* Instance, const FString& PropertyName,
		FString& OutError);
}
