// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UBehaviorTreeService.h"
#include "PythonAPI/UBlackboardService.h"
#include "BehaviorTreeServiceInternal.h"

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

#endif // WITH_AUTOMATION_TESTS
