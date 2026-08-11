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

	/**
	 * The graph's root node, or nullptr. BT sub-nodes (decorators, services) live in
	 * UAIGraphNode::SubNodes and are never added to UEdGraph::Nodes, so sweeping Nodes is the
	 * whole search.
	 */
	UBehaviorTreeGraphNode_Root* FindRootGraphNode(UBehaviorTreeGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UBehaviorTreeGraphNode_Root* Root = Cast<UBehaviorTreeGraphNode_Root>(Node))
			{
				return Root;
			}
		}
		return nullptr;
	}

	/** Sub-nodes (decorators/services) hanging off Node, recursively. */
	int32 CountSubNodes(const UAIGraphNode* Node)
	{
		int32 Count = 0;
		if (Node)
		{
			for (const UAIGraphNode* SubNode : Node->SubNodes)
			{
				if (SubNode)
				{
					Count += 1 + CountSubNodes(SubNode);
				}
			}
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
		UBehaviorTreeGraphNode_Root* Root = FindRootGraphNode(Graph);
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
	if (AssetPath.IsEmpty())
	{
		return TEXT("AssetPath is empty");
	}
	if (!FPackageName::IsValidLongPackageName(AssetPath))
	{
		return FString::Printf(TEXT("Not a valid asset path: %s"), *AssetPath);
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
		OutInfo.bHasRootNode = FindRootGraphNode(Graph) != nullptr;

		OutInfo.NodeCount = Graph->Nodes.Num();
		for (const UEdGraphNode* Node : Graph->Nodes)
		{
			OutInfo.NodeCount += CountSubNodes(Cast<UAIGraphNode>(Node));
		}
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
