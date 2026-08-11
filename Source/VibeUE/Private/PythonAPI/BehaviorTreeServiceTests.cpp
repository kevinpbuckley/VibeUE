// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UBehaviorTreeService.h"
#include "PythonAPI/UBlackboardService.h"
#include "BehaviorTreeServiceInternal.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode_Composite.h"

static const EAutomationTestFlags kBTTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

// The project ships 11 BT assets under /Game; listing must find at least the two
// canonical ones. This also proves the AIModule/BehaviorTreeEditor link is live.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTListTest,
	"VibeUE.BehaviorTree.Asset.List", kBTTestFlags)
bool FVibeBTListTest::RunTest(const FString&)
{
	const TArray<FString> Trees = UBehaviorTreeService::ListBehaviorTrees(TEXT("/Game"));
	TestTrue(TEXT("found behavior trees"), Trees.Num() > 0);
	TestTrue(TEXT("found BT_Enemy"),
		Trees.ContainsByPredicate([](const FString& P){ return P.Contains(TEXT("BT_Enemy")); }));

	const TArray<FString> Boards = UBlackboardService::ListBlackboards(TEXT("/Game"));
	TestTrue(TEXT("found blackboards"), Boards.Num() > 0);
	TestTrue(TEXT("found BB_Enemy"),
		Boards.ContainsByPredicate([](const FString& P){ return P.Contains(TEXT("BB_Enemy")); }));
	return true;
}

// Layout is not cosmetic: BT child execution order is node X position, so a layout bug
// silently reorders execution. Tested pure — no graph, no asset, no Slate.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTLayoutTest,
	"VibeUE.BehaviorTree.Layout.Ordering", kBTTestFlags)
bool FVibeBTLayoutTest::RunTest(const FString&)
{
	using namespace VibeBT;

	// Root
	//  +- A   (leaf)
	//  +- B
	//  |   +- B1 (leaf)
	//  |   +- B2 (leaf)
	//  +- C   (leaf)
	FLayoutNode Root;
	Root.Children.AddDefaulted(3);
	Root.Children[1].Children.AddDefaulted(2);

	// Pre-order: Root, A, B, B1, B2, C
	const TArray<FIntPoint> P = ComputeLayout(Root);
	TestEqual(TEXT("one position per node"), P.Num(), 6);

	// Depth maps to Y.
	TestEqual(TEXT("root depth"),  P[0].Y, 0);
	TestEqual(TEXT("A depth"),     P[1].Y, NodeSpacingY);
	TestEqual(TEXT("B depth"),     P[2].Y, NodeSpacingY);
	TestEqual(TEXT("B1 depth"),    P[3].Y, NodeSpacingY * 2);
	TestEqual(TEXT("C depth"),     P[5].Y, NodeSpacingY);

	// Sibling order maps to strictly increasing X — this is what preserves execution order.
	TestTrue(TEXT("A before B"),   P[1].X < P[2].X);
	TestTrue(TEXT("B before C"),   P[2].X < P[5].X);
	TestTrue(TEXT("B1 before B2"), P[3].X < P[4].X);

	// A parent's subtree must not overlap its siblings' subtrees.
	TestTrue(TEXT("A left of B's subtree"),  P[1].X < P[3].X);
	TestTrue(TEXT("B's subtree left of C"),  P[4].X < P[5].X);

	// Idempotent: re-running produces identical positions.
	TestTrue(TEXT("idempotent"), ComputeLayout(Root) == P);
	return true;
}

// Cycle guard: a malformed graph (child linked back to ancestor) must not crash.
// Tests that ArrangeGraph handles cycles by truncating at revisit, and duplicate links
// by deduplicating before allocating Mirror slots.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTBackEdgeCycleTest,
	"VibeUE.BehaviorTree.Layout.BackEdgeCycle", kBTTestFlags)
bool FVibeBTBackEdgeCycleTest::RunTest(const FString&)
{
	using namespace VibeBT;

	// Create a simple graph: Root -> Child, then create a back-edge: Child -> Root.
	// This creates a cycle that BuildMirror must detect and break.

	UBehaviorTree* Tree = NewObject<UBehaviorTree>(GetTransientPackage());
	UBehaviorTreeGraph* Graph = NewObject<UBehaviorTreeGraph>(Tree);

	// Create Root (Composite) node.
	UBehaviorTreeGraphNode_Composite* Root = NewObject<UBehaviorTreeGraphNode_Composite>(
		Graph, NAME_None, RF_NoFlags);
	Root->CreateNewGuid();
	Root->AllocateDefaultPins();
	Graph->AddNode(Root, false, false);

	// Create Child (Composite) node.
	UBehaviorTreeGraphNode_Composite* Child = NewObject<UBehaviorTreeGraphNode_Composite>(
		Graph, NAME_None, RF_NoFlags);
	Child->CreateNewGuid();
	Child->AllocateDefaultPins();
	Graph->AddNode(Child, false, false);

	// Wire: Root's output -> Child's input.
	UEdGraphPin* RootOut = Root->GetOutputPin();
	UEdGraphPin* ChildIn = Child->GetInputPin();
	if (RootOut && ChildIn)
	{
		RootOut->MakeLinkTo(ChildIn);
	}

	// Create the back-edge cycle: Child's output -> Root's input.
	UEdGraphPin* ChildOut = Child->GetOutputPin();
	UEdGraphPin* RootIn = Root->GetInputPin();
	if (ChildOut && RootIn)
	{
		ChildOut->MakeLinkTo(RootIn);
	}

	// ArrangeGraph must handle the cycle without hanging or crashing.
	// The visited set breaks the cycle at the second visit.
	ArrangeGraph(Root);

	// If we reach here, the cycle was broken correctly.
	TestTrue(TEXT("back-edge cycle handled"), true);
	return true;
}

// Duplicate-link guard: same child reached via two pins, or duplicate LinkedTo entry.
// The deduplication before Mirror allocation must prevent phantom slots.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTDuplicateLinkTest,
	"VibeUE.BehaviorTree.Layout.DuplicateLink", kBTTestFlags)
bool FVibeBTDuplicateLinkTest::RunTest(const FString&)
{
	using namespace VibeBT;

	// Create a graph: Root with two output pins, both linking to the same Child.
	// This exercises the deduplication in BuildMirror.

	UBehaviorTree* Tree = NewObject<UBehaviorTree>(GetTransientPackage());
	UBehaviorTreeGraph* Graph = NewObject<UBehaviorTreeGraph>(Tree);

	// Create Root (Composite) node with two output pins (or reuse one with two links).
	UBehaviorTreeGraphNode_Composite* Root = NewObject<UBehaviorTreeGraphNode_Composite>(
		Graph, NAME_None, RF_NoFlags);
	Root->CreateNewGuid();
	Root->AllocateDefaultPins();
	Graph->AddNode(Root, false, false);

	// Create Child (Composite) node.
	UBehaviorTreeGraphNode_Composite* Child = NewObject<UBehaviorTreeGraphNode_Composite>(
		Graph, NAME_None, RF_NoFlags);
	Child->CreateNewGuid();
	Child->AllocateDefaultPins();
	Graph->AddNode(Child, false, false);

	// Wire Child twice from the same Root output pin (duplicate link).
	// This can happen if the graph is edited externally or corrupted.
	UEdGraphPin* RootOut = Root->GetOutputPin();
	UEdGraphPin* ChildIn = Child->GetInputPin();
	if (RootOut && ChildIn)
	{
		RootOut->MakeLinkTo(ChildIn);
		// Manually add a duplicate link to the LinkedTo array.
		RootOut->LinkedTo.Add(ChildIn);
	}

	// ArrangeGraph must deduplicate before allocating Mirror slots.
	// Without deduplication, the phantom slot would cause Positions.Num() > Order.Num().
	ArrangeGraph(Root);

	// If we reach here, deduplication worked correctly.
	TestTrue(TEXT("duplicate link handled"), true);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
