// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBehaviorTreeService.h"

#include "AIGraphNode.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeServiceInternal.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

namespace VibeBT
{
	/** Shared asset-registry sweep used by both AI services. */
	TArray<FString> ListAssetsOfClass(const UClass* Class, const FString& DirectoryPath)
	{
		// FARFilter converts the path to an FName, which is a fatal engine error past NAME_SIZE.
		if (!CheckNameLength(DirectoryPath, TEXT("DirectoryPath")).IsEmpty())
		{
			return {};
		}
		const FAssetRegistryModule& ARM =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		FARFilter Filter;
		Filter.ClassPaths.Add(Class->GetClassPathName());
		Filter.PackagePaths.Add(*DirectoryPath);
		Filter.bRecursivePaths = true;

		TArray<FAssetData> Assets;
		ARM.Get().GetAssets(Filter, Assets);

		TArray<FString> Paths;
		Paths.Reserve(Assets.Num());
		for (const FAssetData& Asset : Assets)
		{
			Paths.Add(Asset.PackageName.ToString());
		}
		Paths.Sort();
		return Paths;
	}
}

namespace
{
	/**
	 * Load an asset that may legitimately not exist. LOAD_NoWarn | LOAD_Quiet because "not found"
	 * is a value these entry points return to the caller, not an incident: without it, every
	 * probe for a missing path also writes engine warnings into the log (and into automation
	 * test output) for something that was handled.
	 */
	template <typename T>
	T* LoadAssetQuietly(const FString& AssetPath)
	{
		return LoadObject<T>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** Whether anything hangs off the graph root's output pin. */
	bool RootHasLink(const UBehaviorTreeGraphNode_Root* Root)
	{
		return Root && Root->Pins.Num() > 0 && Root->Pins[0] && Root->Pins[0]->LinkedTo.Num() > 0;
	}

	/**
	 * Sub-nodes (decorators/services) hanging off Node, recursively. A legitimate asset never
	 * nests past depth 1 — sub-nodes carry no sub-nodes of their own — but SubNodes is serialised
	 * data a corrupt asset can shape into a cycle, and this is reachable from the read-only
	 * GetBehaviorTreeInfo, so the walk is depth-bounded rather than trusted.
	 */
	int32 CountSubNodes(const UAIGraphNode* Node, int32 Depth = 0)
	{
		int32 Count = 0;
		if (Node && Depth <= 8)
		{
			for (const UAIGraphNode* SubNode : Node->SubNodes)
			{
				if (SubNode)
				{
					Count += 1 + CountSubNodes(SubNode, Depth + 1);
				}
			}
		}
		return Count;
	}

	/** Every node in the graph, sub-nodes included — the same count GetBehaviorTreeInfo reports. */
	int32 CountGraphNodes(const UBehaviorTreeGraph* Graph)
	{
		if (!Graph)
		{
			return 0;
		}
		int32 Count = Graph->Nodes.Num();
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			Count += CountSubNodes(Cast<UAIGraphNode>(Node));
		}
		return Count;
	}

	/**
	 * Point both the asset and its root graph node at Board, which may be null (a tree with no
	 * blackboard is legitimate).
	 *
	 * UBehaviorTreeGraphNode_Root::UpdateBlackboard() is the engine's own path and does more than
	 * the assignment: it runs UpdateBlackboardChange(), which re-runs InitializeFromAsset on every
	 * node instance so key selectors re-resolve against the new board. Writing
	 * UBehaviorTree::BlackboardAsset alone would leave the root node's own copy stale (it is what
	 * the human sees in the details panel, and what a later root-node edit writes back) and every
	 * node's cached key IDs pointing at the old board.
	 */
	FString ApplyBlackboard(UBehaviorTree* Tree, UBehaviorTreeGraph* Graph, UBlackboardData* Board)
	{
		UBehaviorTreeGraphNode_Root* Root = VibeBT::FindRootGraphNode(Graph);
		if (!Root)
		{
			return TEXT("Behavior Tree graph has no root node");
		}

		Tree->Modify();
		Root->Modify();
		Root->BlackboardAsset = Board;
		// No-op when the asset already points at Board; otherwise assigns it and refreshes nodes.
		Root->UpdateBlackboard();
		Tree->BlackboardAsset = Board;
		return FString();
	}
}

TArray<FString> UBehaviorTreeService::ListBehaviorTrees(const FString& DirectoryPath)
{
	return VibeBT::ListAssetsOfClass(UBehaviorTree::StaticClass(), DirectoryPath);
}

FString UBehaviorTreeService::CreateBehaviorTree(const FString& AssetPath, const FString& BlackboardAssetPath)
{
	const FString PathError = VibeBT::CheckWritableAssetPath(AssetPath);
	if (!PathError.IsEmpty())
	{
		return PathError;
	}

	FString PackagePath;
	FString AssetName;
	if (!AssetPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd)
		|| AssetName.IsEmpty())
	{
		return FString::Printf(TEXT("Not a valid asset path: %s"), *AssetPath);
	}

	// Creating over an existing asset replaces someone's tree with an empty one, so it is an error
	// rather than a silent overwrite. Both halves matter: the file check covers an asset on disk
	// that this process has never loaded, FindPackage covers one created earlier in this session
	// and not yet saved.
	//
	// The question is put to the filesystem rather than to FPackageName::DoesPackageExist, which
	// answers from a package-path index built when the process started: a .uasset that existed at
	// startup and has since been deleted out of band still reads as present there, and creation is
	// then refused for an asset that is not actually there. That is not a corner case — it is the
	// state every rerun of this plugin's own test suite starts in.
	FString ExistingFilename;
	const bool bAssetFileOnDisk =
		FPackageName::TryConvertLongPackageNameToFilename(
			AssetPath, ExistingFilename, FPackageName::GetAssetPackageExtension())
		&& IFileManager::Get().FileExists(*ExistingFilename);
	if (bAssetFileOnDisk || FindPackage(nullptr, *AssetPath))
	{
		return FString::Printf(TEXT("Asset already exists: %s"), *AssetPath);
	}

	UBlackboardData* Board = nullptr;
	if (!BlackboardAssetPath.IsEmpty())
	{
		Board = LoadAssetQuietly<UBlackboardData>(BlackboardAssetPath);
		if (!Board)
		{
			return FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardAssetPath);
		}
	}

	UPackage* Package = CreatePackage(*AssetPath);
	if (!Package)
	{
		return FString::Printf(TEXT("Failed to create package: %s"), *AssetPath);
	}

	UBehaviorTree* Tree = NewObject<UBehaviorTree>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!Tree)
	{
		return FString::Printf(TEXT("Failed to create Behavior Tree: %s"), *AssetPath);
	}

	// The factory-equivalent object above has a null BTGraph; without this it has nothing to write
	// to and nothing to show when a human opens it.
	const FString GraphError = VibeBT::EnsureGraph(Tree);
	if (!GraphError.IsEmpty())
	{
		return GraphError;
	}

	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
	if (!Graph)
	{
		return FString::Printf(TEXT("%s has no Behavior Tree graph"), *AssetPath);
	}

	// Unconditional, including for a null Board: EnsureGraph restores whatever the tree had before
	// the root node was spawned (nothing, here), and this is what makes the requested assignment —
	// or the requested absence of one — the value that reaches disk.
	const FString BlackboardError = ApplyBlackboard(Tree, Graph, Board);
	if (!BlackboardError.IsEmpty())
	{
		return BlackboardError;
	}

	FAssetRegistryModule::AssetCreated(Tree);

	return VibeBT::CommitGraph(Tree, Graph);
}

bool UBehaviorTreeService::GetBehaviorTreeInfo(const FString& AssetPath, FBTAssetInfo& OutInfo)
{
	OutInfo = FBTAssetInfo();

	if (AssetPath.IsEmpty())
	{
		OutInfo.Error = TEXT("AssetPath is empty");
		return false;
	}
	OutInfo.Error = VibeBT::CheckNameLength(AssetPath, TEXT("AssetPath"));
	if (!OutInfo.Error.IsEmpty())
	{
		return false;
	}

	UBehaviorTree* Tree = LoadAssetQuietly<UBehaviorTree>(AssetPath);
	if (!Tree)
	{
		OutInfo.Error = FString::Printf(TEXT("Behavior Tree not found: %s"), *AssetPath);
		return false;
	}

	OutInfo.BlackboardPath = Tree->BlackboardAsset
		? Tree->BlackboardAsset->GetOutermost()->GetName()
		: FString();

	// Deliberately read-only — no EnsureGraph. This is the one call that can report bHasGraph
	// false, i.e. an asset the factory made and nobody has opened; creating the graph here would
	// make every read a write and mean that state could never be observed.
	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
	OutInfo.bHasGraph = Graph != nullptr;
	if (Graph)
	{
		OutInfo.bHasRootNode = VibeBT::FindRootGraphNode(Graph) != nullptr;
		OutInfo.NodeCount = CountGraphNodes(Graph);
	}

	return true;
}

FString UBehaviorTreeService::SetBlackboardAsset(const FString& AssetPath, const FString& BlackboardAssetPath)
{
	// Resolved before the write guard: a bad blackboard path must not leave the tree half-written
	// (or, worse, saved) on the way to reporting the error.
	UBlackboardData* Board = nullptr;
	if (!BlackboardAssetPath.IsEmpty())
	{
		Board = LoadAssetQuietly<UBlackboardData>(BlackboardAssetPath);
		if (!Board)
		{
			return FString::Printf(TEXT("Blackboard not found: %s"), *BlackboardAssetPath);
		}
	}

	UBehaviorTree* Tree = nullptr;
	UBehaviorTreeGraph* Graph = nullptr;
	const FString GuardError = VibeBT::OpenWriteGuard(AssetPath, Tree, Graph);
	if (!GuardError.IsEmpty())
	{
		return GuardError;
	}

	const FString BlackboardError = ApplyBlackboard(Tree, Graph, Board);
	if (!BlackboardError.IsEmpty())
	{
		return BlackboardError;
	}

	return VibeBT::CommitGraph(Tree, Graph);
}

FString UBehaviorTreeService::CompileAndSave(const FString& AssetPath)
{
	UBehaviorTree* Tree = nullptr;
	UBehaviorTreeGraph* Graph = nullptr;
	const FString GuardError = VibeBT::OpenWriteGuard(AssetPath, Tree, Graph);
	if (!GuardError.IsEmpty())
	{
		return GuardError;
	}

	return VibeBT::CommitGraph(Tree, Graph);
}

FString UBehaviorTreeService::RepairGraphFromRuntimeTree(const FString& AssetPath)
{
	// The eligibility facts are captured BEFORE OpenWriteGuard, because the guard runs EnsureGraph,
	// and for an asset with no graph at all EnsureGraph creates one and reconstructs it as a side
	// effect (OnCreated -> SpawnMissingNodes). Judging "is there anything to repair" after that
	// would report "nothing to do" about the very asset that had just been rebuilt, and then throw
	// the rebuild away by returning without committing.
	UBehaviorTree* PreloadedTree = LoadAssetQuietly<UBehaviorTree>(AssetPath);
	if (!PreloadedTree)
	{
		return FString::Printf(TEXT("Behavior Tree not found: %s"), *AssetPath);
	}
	UBehaviorTreeGraph* PreloadedGraph = Cast<UBehaviorTreeGraph>(PreloadedTree->BTGraph);
	const bool bHasRuntimeTree = PreloadedTree->RootNode != nullptr;
	const bool bGraphAlreadyPopulated = RootHasLink(VibeBT::FindRootGraphNode(PreloadedGraph));
	const int32 NodesBefore = CountGraphNodes(PreloadedGraph);

	// Runs first so an open editor or a locked graph is reported as such, rather than being
	// pre-empted by a "nothing to repair" message about an asset we were not allowed to touch.
	UBehaviorTree* Tree = nullptr;
	UBehaviorTreeGraph* Graph = nullptr;
	const FString GuardError = VibeBT::OpenWriteGuard(AssetPath, Tree, Graph);
	if (!GuardError.IsEmpty())
	{
		return GuardError;
	}

	if (!bHasRuntimeTree)
	{
		return FString::Printf(
			TEXT("Nothing to repair in %s: it has no runtime node tree to rebuild the graph from. "
				 "Repair repopulates a graph from UBehaviorTree::RootNode; an asset without one is "
				 "simply empty, not damaged."),
			*AssetPath);
	}
	if (bGraphAlreadyPopulated)
	{
		return FString::Printf(
			TEXT("Nothing to repair in %s: its graph already has a node under the root (%d nodes). "
				 "Repair targets the sparse-graph shape only — root present, feeding nothing, runtime "
				 "tree populated — so that it can never duplicate nodes that are already there."),
			*AssetPath, NodesBefore);
	}

	UBehaviorTreeGraphNode_Root* Root = VibeBT::FindRootGraphNode(Graph);
	if (!Root)
	{
		return FString::Printf(TEXT("%s has no root node to rebuild from"), *AssetPath);
	}

	// EnsureGraph will already have reconstructed the graph if it had to create it from nothing
	// (its OnCreated() runs SpawnMissingNodes()). Only drive the reconstruction here when the root
	// is still unlinked, so the engine's walk is never run twice over the same tree.
	if (!RootHasLink(Root))
	{
		// The engine's own reconstruction: walks Asset->RootNode, each composite's Children, and
		// their per-child Decorators and Services, spawning a matching EdGraph node for each with
		// NodeInstance pointed at the existing runtime node, and linking the pins. Because the
		// spawned nodes reuse the live instances rather than copying them, the commit below rebuilds
		// the same tree object rather than a new one.
		Graph->Modify();
		Graph->SpawnMissingNodes();
	}

	// A reconstruction that produced nothing must not reach CommitGraph: the discard guard would
	// refuse it anyway, but reporting it here says what actually went wrong.
	if (!RootHasLink(Root))
	{
		return FString::Printf(
			TEXT("Reconstruction produced no nodes for %s; nothing was written. Its runtime tree is "
				 "present but SpawnMissingNodes() built nothing from it, which usually means the node "
				 "instances failed to load (a missing or renamed node class)."),
			*AssetPath);
	}

	const FString CommitError = VibeBT::CommitGraph(Tree, Graph);
	if (!CommitError.IsEmpty())
	{
		return CommitError;
	}

	// Reported through the log rather than the return value: every entry point on this service uses
	// "empty string means success", and handing back a non-empty summary would read as an error to
	// any caller that checks it that way. GetBehaviorTreeInfo reports the resulting count on demand.
	UE_LOG(LogTemp, Display,
		TEXT("RepairGraphFromRuntimeTree: rebuilt %s from its runtime tree — %d graph node(s) before, %d after."),
		*AssetPath, NodesBefore, CountGraphNodes(Graph));
	return FString();
}
