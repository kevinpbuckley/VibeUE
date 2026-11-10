// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UBehaviorTreeService.h"
#include "PythonAPI/UBlackboardService.h"
#include "AIServiceTestFixture.h"
#include "BehaviorTreeServiceInternal.h"
#include "BehaviorTree/BehaviorTree.h"
#include "AISystem.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTree/Tasks/BTTask_RunEQSQuery.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTree/ValueOrBBKey.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Subsystems/AssetEditorSubsystem.h"

static const EAutomationTestFlags kBTTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

static const TCHAR* kBTTestDir = TEXT("/Game/Developers/VibeUEBTTests");

using VibeAITest::FScopedFixtureReset;

// RepairGraphFromRuntimeTree: the explicit, opt-in rebuild for an asset whose graph holds nothing
// but its root while its runtime tree is intact (the BT_Villager shape). Never implicit — the
// refusal cases below are what keep it that way.
namespace
{
	/** Root, composite-with-instance and their link, built the way the engine's own reconstruction
	 *  would leave them. Returns the composite's runtime instance so identity can be compared. */
	struct FPopulatedTree
	{
		UBehaviorTree* Tree = nullptr;
		UBehaviorTreeGraph* Graph = nullptr;
		UBehaviorTreeGraphNode_Root* Root = nullptr;
		UBehaviorTreeGraphNode_Composite* Composite = nullptr;
		UBTComposite_Sequence* Sequence = nullptr;
		UEdGraphPin* RootOut = nullptr;
	};

	/** Creates BTPath, wires one composite under its root and commits, so the asset ends up with a
	 *  real runtime node tree built by the real path. Returns false if any step did not hold. */
	bool BuildPopulatedTree(FAutomationTestBase& Test, const FString& BTPath, FPopulatedTree& Out)
	{
		if (!Test.TestEqual(TEXT("tree created"),
			UBehaviorTreeService::CreateBehaviorTree(BTPath, FString()), FString()))
		{
			return false;
		}
		Out.Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
		if (!Test.TestNotNull(TEXT("tree loaded"), Out.Tree))
		{
			return false;
		}
		Out.Graph = Cast<UBehaviorTreeGraph>(Out.Tree->BTGraph);
		if (!Test.TestNotNull(TEXT("graph present"), Out.Graph))
		{
			return false;
		}
		for (UEdGraphNode* Node : Out.Graph->Nodes)
		{
			if (UBehaviorTreeGraphNode_Root* Candidate = Cast<UBehaviorTreeGraphNode_Root>(Node))
			{
				Out.Root = Candidate;
				break;
			}
		}
		if (!Test.TestNotNull(TEXT("root node present"), Out.Root))
		{
			return false;
		}

		Out.Composite = NewObject<UBehaviorTreeGraphNode_Composite>(Out.Graph, NAME_None, RF_Transactional);
		Out.Composite->CreateNewGuid();
		Out.Composite->AllocateDefaultPins();
		Out.Sequence = NewObject<UBTComposite_Sequence>(Out.Tree, NAME_None, RF_Transactional);
		Out.Composite->NodeInstance = Out.Sequence;
		Out.Graph->AddNode(Out.Composite, false, false);

		Out.RootOut = Out.Root->Pins.Num() > 0 ? Out.Root->Pins[0] : nullptr;
		UEdGraphPin* CompositeIn = Out.Composite->GetInputPin();
		if (!Test.TestNotNull(TEXT("root output pin"), Out.RootOut) ||
			!Test.TestNotNull(TEXT("composite input pin"), CompositeIn))
		{
			return false;
		}
		Out.RootOut->MakeLinkTo(CompositeIn);

		if (!Test.TestEqual(TEXT("initial commit succeeds"),
			UBehaviorTreeService::CompileAndSave(BTPath), FString()))
		{
			return false;
		}
		return Test.TestEqual(TEXT("the commit built the runtime tree"), ToRawPtr(Out.Tree->RootNode),
			static_cast<UBTCompositeNode*>(Out.Sequence));
	}

	/**
	 * Reduce the graph to its root alone while leaving the runtime tree intact — the sparse shape a
	 * damaged asset has on disk.
	 *
	 * Deliberately NOT UEdGraph::RemoveNode. That calls BreakAllNodeLinks, which goes through the
	 * schema's BreakPinLinks(..., bSendsNodeNotification = true) and reaches
	 * UAIGraphNode::NodeConnectionListChanged() -> GetAIGraph()->UpdateAsset(), which rebuilds the
	 * runtime tree from the now-empty graph on the spot: RootNode reads as None the moment
	 * RemoveNode returns (measured — it is what made the first draft of these tests fail). That
	 * would destroy the very thing they exist to protect before the code under test ever runs.
	 * UEdGraphPin::BreakAllPinLinks() sends no such notification, and dropping the node straight out
	 * of Graph->Nodes reproduces what the asset looks like when loaded from disk in this state.
	 */
	void MakeGraphSparse(FPopulatedTree& Fixture)
	{
		Fixture.RootOut->BreakAllPinLinks();
		Fixture.Graph->Nodes.Remove(Fixture.Composite);
	}
}

// A graph node linked under the root but carrying no composite instance is worse than the empty
// graph CommitDiscardGuardTest covers: CreateBTFromGraph assigns
// Cast<UBTCompositeNode>(RootEdNode->NodeInstance) — null here — and then passes it straight to
// BTGraphHelpers::CreateChildren, which does RootNode->Children.Reset() behind a guard that only

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTRepairGraphTest,
	"VibeUE.BehaviorTree.Asset.RepairGraph", kBTTestFlags)
bool FVibeBTRepairGraphTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_RepairGraphTest");
	const FString EmptyPath = FString(kBTTestDir) / TEXT("BT_RepairEmptyTest");
	FScopedFixtureReset ResetBT(BTPath);
	FScopedFixtureReset ResetEmpty(EmptyPath);

	FPopulatedTree Fixture;
	if (!BuildPopulatedTree(*this, BTPath, Fixture))
	{
		return false;
	}

	// Refusal 1: a healthy tree has nothing to repair.
	TestTrue(TEXT("repair refuses a healthy graph"),
		!UBehaviorTreeService::RepairGraphFromRuntimeTree(BTPath).IsEmpty());

	// Refusal 2: a tree with no runtime node tree is empty, not damaged.
	TestEqual(TEXT("empty tree created"),
		UBehaviorTreeService::CreateBehaviorTree(EmptyPath, FString()), FString());
	TestTrue(TEXT("repair refuses a tree with no runtime tree"),
		!UBehaviorTreeService::RepairGraphFromRuntimeTree(EmptyPath).IsEmpty());

	// Now reduce the graph to the root alone, leaving the runtime tree intact: the sparse shape.
	MakeGraphSparse(Fixture);
	FBTAssetInfo Sparse;
	TestTrue(TEXT("info readable while sparse"), UBehaviorTreeService::GetBehaviorTreeInfo(BTPath, Sparse));
	TestEqual(TEXT("the graph is down to the root alone"), Sparse.NodeCount, 1);
	TestNotNull(TEXT("but the runtime tree is still there"), ToRawPtr(Fixture.Tree->RootNode));

	// An ordinary write is refused in this state — that is what makes repair necessary.
	TestTrue(TEXT("an ordinary commit is refused while sparse"),
		!UBehaviorTreeService::CompileAndSave(BTPath).IsEmpty());

	// The repair itself.
	TestEqual(TEXT("repair succeeds"),
		UBehaviorTreeService::RepairGraphFromRuntimeTree(BTPath), FString());

	FBTAssetInfo Repaired;
	TestTrue(TEXT("info readable after repair"), UBehaviorTreeService::GetBehaviorTreeInfo(BTPath, Repaired));
	TestTrue(TEXT("the graph is repopulated"), Repaired.NodeCount > Sparse.NodeCount);
	TestEqual(TEXT("root plus the rebuilt composite"), Repaired.NodeCount, 2);
	TestTrue(TEXT("the graph still has its root"), Repaired.bHasRootNode);

	// Identity, not just non-null: the rebuild must reuse the existing runtime node, because the
	// spawned graph nodes point NodeInstance at it rather than at a copy.
	TestEqual(TEXT("the runtime tree survived as the same object"), ToRawPtr(Fixture.Tree->RootNode),
		static_cast<UBTCompositeNode*>(Fixture.Sequence));

	// And the asset is writable again.
	TestEqual(TEXT("an ordinary commit works after repair"),
		UBehaviorTreeService::CompileAndSave(BTPath), FString());
	TestEqual(TEXT("the runtime tree is still intact after that commit"), ToRawPtr(Fixture.Tree->RootNode),
		static_cast<UBTCompositeNode*>(Fixture.Sequence));

	// Repair is not repeatable once it has worked — the graph is populated, so there is nothing
	// left to repair. This is what stops it duplicating nodes if it is run twice.
	TestTrue(TEXT("repair refuses a second time"),
		!UBehaviorTreeService::RepairGraphFromRuntimeTree(BTPath).IsEmpty());

	// A missing asset is reported, not silently ignored.
	TestTrue(TEXT("repair on a missing tree fails"),
		!UBehaviorTreeService::RepairGraphFromRuntimeTree(
			FString(kBTTestDir) / TEXT("BT_NoSuchRepairTree")).IsEmpty());

	return true;
}

// EnsureGraph and the mutators must never repair implicitly: rebuilding a production asset's graph
// as a side effect of an unrelated edit is the unrequested write this service exists to prevent.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTRepairIsNeverImplicitTest,
	"VibeUE.BehaviorTree.Asset.RepairIsNeverImplicit", kBTTestFlags)
bool FVibeBTRepairIsNeverImplicitTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_NoImplicitRepairTest");
	FScopedFixtureReset ResetBT(BTPath);

	FPopulatedTree Fixture;
	if (!BuildPopulatedTree(*this, BTPath, Fixture))
	{
		return false;
	}
	MakeGraphSparse(Fixture);
	TestNotNull(TEXT("the runtime tree is intact going in"), ToRawPtr(Fixture.Tree->RootNode));

	// EnsureGraph sees a graph that already has its root, so it must do nothing at all here.
	TestEqual(TEXT("EnsureGraph reports success"), VibeBT::EnsureGraph(Fixture.Tree), FString());
	FBTAssetInfo AfterEnsure;
	TestTrue(TEXT("info readable"), UBehaviorTreeService::GetBehaviorTreeInfo(BTPath, AfterEnsure));
	TestEqual(TEXT("EnsureGraph did not rebuild the graph"), AfterEnsure.NodeCount, 1);

	// Nor does opening for write, nor a refused mutator.
	UBehaviorTree* GuardTree = nullptr;
	UBehaviorTreeGraph* GuardGraph = nullptr;
	TestEqual(TEXT("the write guard opens the asset"),
		VibeBT::OpenWriteGuard(BTPath, GuardTree, GuardGraph), FString());
	TestTrue(TEXT("SetBlackboardAsset is refused, not repaired"),
		!UBehaviorTreeService::SetBlackboardAsset(BTPath, FString()).IsEmpty());
	TestTrue(TEXT("info readable after the refusal"),
		UBehaviorTreeService::GetBehaviorTreeInfo(BTPath, AfterEnsure));
	TestEqual(TEXT("still not rebuilt"), AfterEnsure.NodeCount, 1);

	return true;
}

static TSharedPtr<FJsonObject> ParseTree(const FString& Json)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Root);
	return Root;
}
#endif // WITH_AUTOMATION_TESTS
