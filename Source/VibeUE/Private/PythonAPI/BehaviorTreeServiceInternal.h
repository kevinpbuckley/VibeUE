// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FProperty;
class UBehaviorTree;
class UBehaviorTreeGraph;
class UBehaviorTreeGraphNode;
class UBehaviorTreeGraphNode_Root;
class UWorld;

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

	/**
	 * Depth bound on every recursive walk over a tree or a caller-supplied tree description.
	 *
	 * No legitimate Behavior Tree approaches it — the deepest production tree seen while building
	 * this was 9 levels — but a corrupt asset (or a hostile BuildTree JSON) with a many-thousand
	 * node linear chain would otherwise convert a read into a stack overflow, which takes the whole
	 * editor down. Walks that hit the bound report an error; none of them silently truncate.
	 */
	constexpr int32 MaxTreeDepth = 1024;

	/**
	 * FName construction from a string longer than NAME_SIZE is a Fatal engine log
	 * (UnrealNames.cpp: UE_CLOGF(Len > NAME_SIZE, ..., Fatal)) — an editor kill, reachable from any
	 * MCP caller that passes an over-long key, property or asset name. Every entry point that turns
	 * caller input into an FName (or into an object path lookup, which builds FNames per segment)
	 * checks this first. Returns an error naming What, or empty when Value is safe.
	 */
	FString CheckNameLength(const FString& Value, const TCHAR* What);

	/**
	 * Throw away a package's dirty in-memory state by reloading it from disk (non-interactive),
	 * and describe the outcome as a sentence to append to the error that made it necessary.
	 *
	 * For the failure paths where an in-memory object has diverged from disk and the divergence
	 * must not linger: a dirty package left behind is shipped, silently, by the next successful
	 * save of the same asset — a human's Save All included — and a stale copy turns a caller's
	 * natural retry into a write of exactly the state a guard refused.
	 */
	FString DiscardDirtyStateFromDisk(class UPackage* Package);

	/**
	 * Whether AssetPath may be created or written by these services: a valid long package name,
	 * within the FName length bound, and not under /Engine, /Script or /Temp — an agent has no
	 * business writing assets into the engine installation, class packages are not assets, and
	 * /Temp does not survive the session. Plugin content roots are deliberately allowed: mutating
	 * a plugin's own content is a legitimate workflow. Returns an error, or empty when writable.
	 */
	FString CheckWritableAssetPath(const FString& AssetPath);

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
	 *
	 * Returns an error — and writes no position at all — when the graph is a shape the layout
	 * cannot arrange: deeper than MaxTreeDepth, or with an internal mirror/order mismatch (which
	 * a hand-corrupted graph could in principle produce). Positions ARE execution order
	 * (CreateBTFromGraph sorts children by X), so arranging such a graph partially would silently
	 * reorder the tree; refusing is the only honest outcome, and CommitGraph turns the refusal
	 * into a refused write.
	 */
	FString ArrangeGraph(UBehaviorTreeGraphNode* RootNode);

	/**
	 * Release the module-lifetime FGraphNodeClassHelper cache. Called from the module's
	 * ShutdownModule: ~FGraphNodeClassHelper unhooks FModuleManager and asset-registry delegates,
	 * which must happen while those systems still exist, not at static teardown.
	 */
	void ShutdownClassHelperCache();

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
	 * The tree node that carries Node as a decorator or a service, or nullptr when Node is not a
	 * sub-node.
	 *
	 * Does NOT simply read UAIGraphNode::ParentNode: that is UPROPERTY(transient), so it is null on
	 * every sub-node of every asset freshly loaded from disk until something runs
	 * UBehaviorTreeGraph::UpdateAsset(). The owner's own Decorators/Services arrays are serialised
	 * and carry the same relation, so they are the fallback.
	 */
	UBehaviorTreeGraphNode* GetSubNodeOwner(const UBehaviorTreeGraphNode* Node);

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
	 * Whether committing this graph would leave the asset with a runtime tree, i.e. whether
	 * UBehaviorTree::RootNode would come out non-null on the other side of UpdateAsset().
	 *
	 * Deliberately mirrors the engine statement for statement, so it can never disagree with it
	 * about what is about to happen — UpdateAsset() picks the node:
	 *
	 *   if (RootNode && RootNode->Pins.Num() > 0 && RootNode->Pins[0]->LinkedTo.Num() > 0)
	 *       Node = Cast<UBehaviorTreeGraphNode>(RootNode->Pins[0]->LinkedTo[0]->GetOwningNode());
	 *   CreateBTFromGraph(Node);
	 *
	 * and CreateBTFromGraph turns it into the new tree:
	 *
	 *   BTAsset->RootNode = Cast<UBTCompositeNode>(RootEdNode->NodeInstance);
	 *
	 * That last cast is why the NodeInstance check is not belt-and-braces. A root linked to a graph
	 * node carrying no instance (or a non-composite one) leaves BTAsset->RootNode null, and the very
	 * next line — BTGraphHelpers::CreateChildren(BTAsset, BTAsset->RootNode, ...) — dereferences it as
	 * RootNode->Children.Reset() behind a guard that only tests RootEdNode
	 * (BehaviorTreeGraph.cpp:510-517). So that shape does not merely discard the tree: it crashes the
	 * editor inside OnSave(). Refusing it before the commit is the only place it can be stopped,
	 * because the post-OnSave check never gets to run.
	 *
	 * Shared with ValidateTree, so the diagnostic and the refusal are the same predicate: an asset the
	 * mutators will not write must never read back as healthy.
	 */
	bool GraphWouldRebuildARuntimeTree(const UBehaviorTreeGraphNode_Root* Root);

	/**
	 * The refusal a write must issue while a Play In Editor session is running, or an empty string
	 * when PlayWorld is null.
	 *
	 * Split out from OpenWriteGuard so the decision is testable: an automation run is headless and
	 * never has a play session, so the branch is otherwise unreachable from a test.
	 *
	 * Why writes are refused at all: CommitGraph runs UBehaviorTreeGraph::OnSave() -> UpdateAsset(),
	 * which ends in UAIGraph::RemoveOrphanedNodes() (AIGraph.cpp:215-236). That marks every UBTNode
	 * instance no longer referenced by the graph RF_Transient and renames it into the transient
	 * package — while a live UBehaviorTreeComponent in the play session may be executing those very
	 * instances — and then the package is saved. OpenWriteGuard's open-editor refusal does not cover
	 * this: a running game holds no asset editor.
	 */
	FString PlaySessionRefusal(const FString& AssetPath, const UWorld* PlayWorld);

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
	 * Refuses when a Play In Editor session is running (see PlaySessionRefusal), when a Behavior Tree
	 * editor is open on the asset — the open editor holds its own EdGraph state, will not show the
	 * change, and overwrites it on the human's next save — and when the graph is locked, because
	 * UpdateAsset() early-returns under bLockUpdates (BehaviorTreeGraph.cpp:104) and the commit would
	 * report success having regenerated nothing.
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
	 * Export Property's current value on Instance as a COMPLETE literal, by passing Instance as its
	 * own delta container. Measured behaviour, in this engine version:
	 *
	 *  - Self-delta is not a diff against itself, it is the engine's "export everything" path.
	 *    ExportText_InContainer resolves the delta pointer through
	 *    ContainerPtrToValuePtrForDefaults(NULL, Delta, Idx), whose IsInContainer(nullptr) test
	 *    compares against MAX_int32 and is therefore always true, so the delta resolves to the same
	 *    address as the value; FProperty::ExportText_Direct then opens with `if (Data==Delta || ...)`
	 *    and exports (Property.cpp). The delta travels down unchanged, so UScriptStruct::ExportText
	 *    hands each member Defaults == Value and every member takes the same short-circuit, at any
	 *    depth.
	 *  - Passing a real default container (the CDO, say) emits NOTHING for any property or member
	 *    that matches it. Two ways that loses data: a node whose WaitTime equals its class default
	 *    reports as "" or "()" — not a value, and not something SetNodePropertyValue could write
	 *    back; and a member sitting at zero is dropped even when the class default is not zero, so
	 *    { Key="WaitSeconds", DefaultValue=0.0 } exports as (Key="WaitSeconds") and replays onto a
	 *    fresh BTTask_Wait as 5.0, silently.
	 *  - Passing NO defaults is better but not enough: with a null delta each member is compared
	 *    against ZERO (FProperty::Identical(Data, nullptr)), so members at zero are still dropped —
	 *    the same silent-5.0 bug. Self-delta is what makes get -> set -> get exact, including
	 *    between two different nodes.
	 *  - The one residue: struct elements INSIDE an array. FArrayProperty::ExportTextInnerItem
	 *    forces PropDefault to a default-constructed struct for a struct inner regardless of the
	 *    delta (PropertyArray.cpp:1055-1102), so members of an array element that are at their
	 *    struct default are omitted. Nothing available here changes that.
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
