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

/** One editable property of a Behavior Tree node instance. */
USTRUCT(BlueprintType)
struct FBTPropertyInfo
{
	GENERATED_BODY()

	/** Property name, as passed to GetNodePropertyValue / SetNodePropertyValue. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString Name;

	/** C++ type of the property, e.g. "float", "FString", "FBlackboardKeySelector". */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString Type;

	/** Current value as a full literal — what SetNodePropertyValue would take to reproduce it. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString Value;

	/**
	 * True when the property is an FBlackboardKeySelector, which must be written with
	 * SetNodeBlackboardKey rather than SetNodePropertyValue.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	bool bIsBlackboardKeySelector = false;
};

/** Outcome of a node property write. */
USTRUCT(BlueprintType)
struct FBTPropertySetResult
{
	GENERATED_BODY()

	/** True only if the value was written AND the asset committed. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	bool bSuccess = false;

	/** Populated when the write was refused or failed. Empty on success. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString Error;

	/** Instance class of the node the path resolved to, so a wrong target is visible. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString ResolvedNodeClass;

	/** Display name of the node the path resolved to. */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString ResolvedNodeName;

	/**
	 * The property re-read AFTER the commit, never an echo of the input — so a value the engine
	 * rewrote or discarded on save comes back as what it really is.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "BehaviorTree")
	FString ValueAfterWrite;
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
	 * Every node object carries all ten fields, including the objects inside "decorators" and
	 * "services". A sub-node's "children" / "decorators" / "services" are always empty and its
	 * "bHasCompositeDecorator" always false — decorators and services have no pins and carry no
	 * sub-nodes of their own — but the keys are present, so a consumer can walk any node object
	 * with one piece of code instead of branching on where it found it. The alternative (six
	 * fields on a sub-node, ten on a tree node) is equally truthful and was rejected for that
	 * reason: it makes every consumer, starting with BuildTree, special-case the two shapes.
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
	 *
	 * SimpleParallel is the exception, and the numbering is the same one GetTree reports:
	 * child 0 is the main task (only a task class is accepted) and child 1 is the background
	 * branch (any composite or task). There are exactly two, and a third is rejected. ChildIndex
	 * < 0 takes the first free slot and never displaces anything; an explicit 0 or 1 replaces
	 * whatever is in that slot, removing it and its subtree. Replacing is the only way to author a
	 * background branch, because the engine refills an empty background slot with a Wait task on
	 * every save, so it is never free for a second call to fill.
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
	 * with no runtime root at all, which CommitGraph exists to refuse. Also refuses a
	 * SimpleParallel's background branch, which the engine regenerates as a Wait task on save;
	 * replace it with AddNode(<parallel>, <class>, 1) instead.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString RemoveNode(const FString& AssetPath, const FString& NodePath);

	/**
	 * Re-parent a node (with its subtree) to NewChildIndex under NewParentPath, and save.
	 * NewChildIndex < 0 appends. Returns an empty string on success, else the error.
	 *
	 * Refuses to move the root, and to move a node under itself or one of its own descendants.
	 * NewChildIndex means the same thing it does for AddNode, including SimpleParallel's two fixed
	 * slots — but MoveNode never displaces anything, so an occupied slot is an error rather than a
	 * replace.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString MoveNode(const FString& AssetPath, const FString& NodePath,
		const FString& NewParentPath, int32 NewChildIndex);

	// =================================================================
	// Sub-nodes — decorators and services
	// =================================================================

	/**
	 * Attach a decorator to the node at NodePath and save. Returns the new sub-node's path
	 * ("<owner path>/@decorator[n]"), or a string starting with "ERROR: ".
	 *
	 * Index is the insert position among the node's existing decorators; < 0 appends. Decorator
	 * order is evaluation order, so it is not cosmetic.
	 *
	 * Refused on any node that does not carry decorators into the runtime tree: the graph's Root
	 * node (UBehaviorTreeGraph::CreateBTFromGraph reads root-level decorators off the *top
	 * composite*, never off the Root graph node, so one attached there would simply never run),
	 * a decorator or service (sub-nodes carry no sub-nodes of their own), a node injected from a
	 * subtree, and any node whose instance is neither a composite nor a task.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString AddDecorator(const FString& AssetPath, const FString& NodePath,
		const FString& DecoratorClassName, int32 Index);

	/**
	 * Attach a service to the node at NodePath and save. Returns the new sub-node's path
	 * ("<owner path>/@service[n]"), or a string starting with "ERROR: ".
	 *
	 * Index is the insert position among the node's existing services; < 0 appends.
	 *
	 * Refused on the same set of nodes as AddDecorator, and additionally when the graph class
	 * answers false to UBehaviorTreeGraph::DoesSupportServices() — the engine's own gate, the one
	 * the Behavior Tree editor's context menu uses (BehaviorTreeGraphNode_Composite.cpp:84,
	 * BehaviorTreeGraphNode_Task.cpp:52).
	 *
	 * Note that in UE 5.8 both composites AND tasks carry services: CreateBTFromGraph writes a
	 * task node's services to UBTTaskNode::Services (BehaviorTreeGraph.cpp:605-627) exactly as it
	 * writes a composite's to UBTCompositeNode::Services (:518-535). A task is therefore accepted.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString AddService(const FString& AssetPath, const FString& NodePath,
		const FString& ServiceClassName, int32 Index);

	/**
	 * Detach and delete one decorator or service, addressed by its "@decorator[n]" / "@service[n]"
	 * path, and save. Returns an empty string on success, else the error.
	 *
	 * Refuses anything that is not a sub-node (use RemoveNode for tree nodes) and anything injected
	 * from a subtree — an injected decorator is a copy the engine regenerates from the subtree
	 * asset, so removing it here would report success and be undone by the next graph update.
	 *
	 * Sub-node paths are positional, like every other path this service issues: removing one
	 * renumbers the "@decorator[n]" of every later decorator on the same node.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString RemoveSubNode(const FString& AssetPath, const FString& SubNodePath);

	/**
	 * Set the display name of a node or sub-node (UBTNode::NodeName) and save. An empty NewName
	 * clears it, so the node falls back to the class-supplied description. Returns an empty string
	 * on success, else the error.
	 *
	 * The name IS the path segment: after this call the node's old path no longer resolves, and
	 * neither does any path through it. Re-read GetTree (or hold the node's "guid") across a
	 * rename. For that reason three name shapes are refused rather than written and left
	 * un-addressable: one containing "/" (the path separator), one ending in a numeric "[n]" (read
	 * as a sibling index), and one starting with "@" (reserved for sub-node slots).
	 *
	 * Refused on the Root node, which has no instance to name, and on injected nodes.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString SetNodeName(const FString& AssetPath, const FString& NodePath,
		const FString& NewName);

	// =================================================================
	// Node properties
	// =================================================================

	/**
	 * Every editable property of the node (or sub-node) at NodePath, with its current value.
	 *
	 * "Editable" is the same set GetTree reports under "properties" — CPF_Edit, not EditConst /
	 * Transient / Deprecated — but the whole set rather than only the edited ones, because this is
	 * the discovery call: it answers "what can I set here", not "what has been set".
	 *
	 * Values are complete literals — every member of a struct is emitted, including members left at
	 * their default — so a value read here can be handed to SetNodePropertyValue on this node or on
	 * any other node of the same class and reproduce it exactly. (GetTree's map reports the same
	 * encoding, filtered to properties that differ from the class default.)
	 *
	 * Two limits, both the engine's and neither hidden here: members of a STRUCT ELEMENT INSIDE AN
	 * ARRAY that sit at their struct default are omitted (FArrayProperty exports array elements
	 * against a default-constructed struct whatever it is handed), and an empty array reads back as
	 * an empty string, which is not importable — pass "()" to empty one.
	 *
	 * An empty array means the path named nothing, or named the graph's Root node, which carries no
	 * instance and therefore no properties — every real BT node has at least NodeName. Use
	 * GetNodeInfo to tell those apart; this call has no error channel of its own.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static TArray<FBTPropertyInfo> GetNodePropertyNames(const FString& AssetPath,
		const FString& NodePath);

	/**
	 * One property's current value in the encoding described on GetNodePropertyNames, or a string
	 * starting with "ERROR: ".
	 *
	 * Read-only: like GetTree it never creates the editor graph, so reading an asset that has never
	 * been opened reports that rather than writing to it.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FString GetNodePropertyValue(const FString& AssetPath, const FString& NodePath,
		const FString& PropertyName);

	/**
	 * Set one property from a literal (the form GetNodePropertyValue returns) and save.
	 *
	 * ValueAfterWrite is re-read after the commit rather than echoed, AND compared with what was
	 * written, because the commit is not a passive save: UBTNode::PreSave rewrites two families of
	 * property at save time, after this call has otherwise succeeded — FBlackboardKeySelector, and
	 * FValueOrBlackboardKeyBase (which in UE 5.8 is what an ordinary numeric property such as
	 * BTTask_Wait::WaitTime is). Both silently clear a binding whose key is missing or of the wrong
	 * type. When the saved value differs from the written one, this reports failure and
	 * ValueAfterWrite is what is actually on disk.
	 *
	 * A parse failure restores the property to what it was, because ImportText applies a struct or
	 * array literal member by member and stops where it fails — a refused write must not leave the
	 * node half-changed. The restore is a value-level copy, so it bypasses any property Setter
	 * (theoretical here: no BT node class declares one).
	 *
	 * Refuses injected nodes (copies the engine regenerates from a subtree asset) and anything that
	 * is not an editable property. For an FBlackboardKeySelector use SetNodeBlackboardKey: this
	 * route writes the selector's name and nothing else, and only finds out at commit time whether
	 * that was a legal binding — see below.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FBTPropertySetResult SetNodePropertyValue(const FString& AssetPath,
		const FString& NodePath, const FString& PropertyName, const FString& Value);

	/**
	 * Bind an FBlackboardKeySelector property to a key on the tree's blackboard, and save.
	 *
	 * Separate from SetNodePropertyValue because a key selector is not a value: it is a name plus a
	 * resolved key ID, and only the ID is read at runtime. Importing the name alone leaves the ID
	 * unresolved, and a node whose selector is unresolved does nothing at all, silently — nothing
	 * is logged, and the property reads back exactly as the caller wrote it.
	 *
	 * So this resolves the key against UBehaviorTree::BlackboardAsset and refuses, without touching
	 * the selector, when:
	 *   - the tree has no blackboard;
	 *   - the key does not exist on it;
	 *   - the key's type is not in the selector's AllowedTypes filter. Note that
	 *     FBlackboardKeySelector::ResolveSelectedKey does NOT check the filter (only InitSelection
	 *     does), so a mistyped binding resolves to a real ID and reads back as a working binding;
	 *     what actually happens is that the node reads a key of a type it cannot use, and that
	 *     UBTNode::PreSave silently resets the selector to None on the next save.
	 *
	 * As a last backstop the resolved ID is asserted after the write, and an unresolved one is
	 * reported as an error with the selector restored.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static FBTPropertySetResult SetNodeBlackboardKey(const FString& AssetPath,
		const FString& NodePath, const FString& PropertyName, const FString& KeyName);

	// =================================================================
	// Validation
	// =================================================================

	/**
	 * Read-only diagnostic sweep over the editor graph. Never mutates or commits — no EnsureGraph, no
	 * OpenWriteGuard, nothing written back — so it is safe to run on an asset nobody has decided to
	 * edit yet.
	 *
	 * An empty array means nothing was found. A single "ERROR: ..." element means validation itself
	 * could not run (missing asset, no editor graph, no root node) — never confused with "found no
	 * problems", because it is exactly one element and starts with "ERROR: ". Otherwise every element
	 * is one finding, each line beginning with the offending node's path (VibeBT::GetNodePath); an
	 * orphan has none by definition, so it is named "<unreachable:Title#Guid>" instead.
	 *
	 * Walks every node in the graph (Graph->Nodes) and reports:
	 *  - orphaned nodes: unreachable from the root. A hand-edited or corrupted graph can produce
	 *    these; GetTree and path resolution silently ignore them, so nothing else surfaces them;
	 *  - composites with no children. Scoped to UBTCompositeNode instances — a leaf task legitimately
	 *    has none. Reachability and children are both read through VibeBT::GetChildNodes, which spans
	 *    both of a SimpleParallel node's output pins ([0] "Task", [1] "Out"); a walk over "the" output
	 *    pin would silently miss the second branch;
	 *  - a node whose instance failed to load: UAIGraphNode::HasErrors() (NodeInstance == nullptr, or
	 *    a non-empty ErrorMessage from an unresolved ClassData), the same check the graph editor uses
	 *    to colour a node red. The graph's Root is excluded — its NodeInstance is null by design, not
	 *    a load failure;
	 *  - every FBlackboardKeySelector AND every FValueOrBlackboardKeyBase (BTTask_Wait::WaitTime and
	 *    RandomDeviation are FValueOrBBKey_Float in 5.8, not FBlackboardKeySelector — both families
	 *    are walked) bound to a key name absent from the tree's blackboard, or bound at all when the
	 *    tree has no blackboard. Walked on tree nodes AND on their decorators and services — a
	 *    decorator/service is a real BT node instance in its own right (UBTDecorator_Blackboard's own
	 *    BlackboardKey is exactly this), and lives outside Graph->Nodes, so it needs its own pass.
	 *    Walked at EVERY nesting depth a node's own value-type properties reach — struct members and
	 *    struct array elements, not just the node class's own top-level properties — because
	 *    UBTNode::PreSave (BTNode.cpp:231-254) only ever inspects the top level, so a selector or
	 *    value-or-key binding buried one struct deeper (e.g.
	 *    UBTTask_RunEQSQuery::EQSRequest.QueryConfig[n].BBKey) gets no engine validation at all, and a
	 *    mistyped key there is silently wiped on save with nothing logged. A binding left at its
	 *    default "None" is not reported: that is an intentionally empty selector, not a mistyped one;
	 *  - decorators or services attached to a node that cannot carry them into the runtime tree: the
	 *    graph's Root node, and any node whose instance is neither a composite nor a task.
	 *
	 * A node injected from a subtree (bInjectedNode) is skipped by every check above except the
	 * orphan sweep, which it can never fail — an injected node is always linked in by construction.
	 * It is a read-only copy of another asset's node, resolved against that asset's own blackboard;
	 * reporting it here would flag things this asset cannot fix and that are not actually broken.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|BehaviorTree")
	static TArray<FString> ValidateTree(const FString& AssetPath);
};
