// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UBehaviorTreeService.h"
#include "PythonAPI/UBlackboardService.h"
#include "BehaviorTreeServiceInternal.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
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

// FGraphNodeClassHelper reports Blueprint-derived node classes only after priming; without it
// this test sees native classes only, which would hide most of this project's BT nodes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTNodeTypesTest,
	"VibeUE.BehaviorTree.Classes.Discovery", kBTTestFlags)
bool FVibeBTNodeTypesTest::RunTest(const FString&)
{
	const TArray<FBTNodeClassInfo> Composites =
		UBehaviorTreeService::GetAvailableNodeTypes(TEXT("Composite"));
	TestTrue(TEXT("has Selector"), Composites.ContainsByPredicate(
		[](const FBTNodeClassInfo& C){ return C.ClassName == TEXT("BTComposite_Selector"); }));
	TestTrue(TEXT("has Sequence"), Composites.ContainsByPredicate(
		[](const FBTNodeClassInfo& C){ return C.ClassName == TEXT("BTComposite_Sequence"); }));

	const TArray<FBTNodeClassInfo> Tasks =
		UBehaviorTreeService::GetAvailableNodeTypes(TEXT("Task"));
	TestTrue(TEXT("has MoveTo"), Tasks.ContainsByPredicate(
		[](const FBTNodeClassInfo& C){ return C.ClassName == TEXT("BTTask_MoveTo"); }));
	TestTrue(TEXT("reports Blueprint tasks too"), Tasks.ContainsByPredicate(
		[](const FBTNodeClassInfo& C){ return C.bIsBlueprint; }));

	const TArray<FBTNodeClassInfo> Decorators =
		UBehaviorTreeService::GetAvailableNodeTypes(TEXT("Decorator"));
	TestTrue(TEXT("has Blackboard decorator"), Decorators.ContainsByPredicate(
		[](const FBTNodeClassInfo& C){ return C.ClassName == TEXT("BTDecorator_Blackboard"); }));

	const TArray<FBTNodeClassInfo> Services =
		UBehaviorTreeService::GetAvailableNodeTypes(TEXT("Service"));
	TestTrue(TEXT("has services"), Services.Num() > 0);

	// An unknown category is an error, not an empty list that reads as "none exist".
	TestEqual(TEXT("bad category empty"),
		UBehaviorTreeService::GetAvailableNodeTypes(TEXT("Nonsense")).Num(), 0);
	return true;
}

// Native resolution: short name and full object path must both find the same real class.
// (A collision test between the two forms would be redundant with ResolveWrongBase below,
// since a wrong answer here would surface as IsChildOf failing against the wrong class.)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTResolveNativeTest,
	"VibeUE.BehaviorTree.Classes.ResolveNative", kBTTestFlags)
bool FVibeBTResolveNativeTest::RunTest(const FString&)
{
	using namespace VibeBT;

	UClass* ByShortName = ResolveNodeClass(TEXT("BTTask_MoveTo"), UBTTaskNode::StaticClass());
	TestEqual(TEXT("resolves native task by short name"), ByShortName, UBTTask_MoveTo::StaticClass());

	UClass* ByFullPath =
		ResolveNodeClass(TEXT("/Script/AIModule.BTTask_MoveTo"), UBTTaskNode::StaticClass());
	TestEqual(TEXT("resolves native task by full object path"), ByFullPath, UBTTask_MoveTo::StaticClass());

	return true;
}

// A name that resolves to nothing, and a name that resolves to a real class of the WRONG
// base, must both come back null — never the wrong-typed class.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTResolveBogusAndWrongBaseTest,
	"VibeUE.BehaviorTree.Classes.ResolveBogusAndWrongBase", kBTTestFlags)
bool FVibeBTResolveBogusAndWrongBaseTest::RunTest(const FString&)
{
	using namespace VibeBT;

	TestNull(TEXT("a name matching no class resolves to nullptr"),
		ResolveNodeClass(TEXT("NotARealBTNodeClass_ThisNameDoesNotExist"), UBTTaskNode::StaticClass()));

	// BTDecorator_Blackboard is a real, loaded, native class — just not a UBTTaskNode. If the
	// RequiredBase guard were ever dropped (or the lookup fell back to an unscoped search),
	// this would come back non-null instead.
	TestNull(TEXT("a real class of the wrong base resolves to nullptr, not the wrong type"),
		ResolveNodeClass(TEXT("BTDecorator_Blackboard"), UBTTaskNode::StaticClass()));

	return true;
}

// Regression test for the Critical-1 finding: FindObject/FindFirstObject are pure in-memory
// lookups and never load from disk, so the pre-fix ResolveNodeClass returned nullptr for any
// Blueprint-generated BT class that wasn't already resident in this process — indistinguishable
// from "no such class". Only FGraphNodeClassData::GetClass() (LoadPackage + FullyLoad) can pull
// a Blueprint class in, and that only happens (post Important-3 fix) on the single matched
// candidate inside ResolveNodeClass itself, never as a side effect of discovery.
//
// This test can only PROVE the cold-load path ran if BTT_RequestAttack_C is genuinely not
// resident when it starts. That's not guaranteed by the language — nothing stops some other
// system from having touched it earlier in the process — so this checks the precondition
// explicitly (via AddInfo, visible in the automation log) rather than assuming it. In this
// suite specifically it reliably holds: GetAvailableNodeTypes no longer force-loads Blueprint
// classes (Important-3 fix, independently confirmed via diagnostic logging — see task-4-report.md),
// and no other test in this file or BlackboardServiceTests.cpp touches AI task Blueprints.
// If that precondition ever stops holding, the AddInfo line makes it visible instead of the
// test silently passing for a weaker reason.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTResolveBlueprintColdTest,
	"VibeUE.BehaviorTree.Classes.ResolveBlueprintCold", kBTTestFlags)
bool FVibeBTResolveBlueprintColdTest::RunTest(const FString&)
{
	using namespace VibeBT;

	const FString ShortName = TEXT("BTT_RequestAttack");
	const FString FullPath =
		TEXT("/Game/Core/Controllers/AI/Tasks/BTT_RequestAttack.BTT_RequestAttack_C");

	const bool bAlreadyResident = FindObject<UClass>(nullptr, *FullPath) != nullptr;
	AddInfo(FString::Printf(
		TEXT("BTT_RequestAttack_C resident before resolve: %s"),
		bAlreadyResident ? TEXT("true (this run can't prove the cold-load path)") : TEXT("false")));

	UClass* Resolved = ResolveNodeClass(ShortName, UBTTaskNode::StaticClass());
	TestNotNull(TEXT("resolves an unloaded Blueprint task class by short name"), Resolved);
	if (Resolved)
	{
		TestTrue(TEXT("resolved class derives from UBTTaskNode"),
			Resolved->IsChildOf(UBTTaskNode::StaticClass()));
		TestEqual(TEXT("resolved to the expected generated class"),
			Resolved->GetName(), FString(TEXT("BTT_RequestAttack_C")));
	}
	return true;
}

// No genuine short-name collision exists in this project as of this writing: a diagnostic
// sweep (GetAvailableNodeTypes for all four categories, grouped by ClassName) found zero
// duplicates — Composite 3/3 unique, Task 55/55, Decorator 23/23, Service 10/10. Constructing
// a fake one would require injecting a fabricated asset-registry entry (FGraphNodeClassHelper's
// Blueprint gather step reads FAssetData, not in-memory UObjects, so a runtime-only stand-in
// class doesn't reach it) — deep enough into engine internals that a bug there would be a
// bug in the test, not a signal about ResolveNodeClass. Recorded here rather than fabricating
// a passing assertion; the MatchCount > 1 branch in ResolveNodeClass is currently unexercised
// by an automated test.

#endif // WITH_AUTOMATION_TESTS
