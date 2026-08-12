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

// Listing must find assets, which also proves the AIModule/BehaviorTreeEditor link is live.
// SELF-PROVISIONING: the previous version asserted that the host project already contained at
// least one BT and one Blackboard under /Game — a project dependency dressed up as neutrality,
// and a hard failure in any project whose AI is not Behavior Tree-based (empirically: this very
// plugin's own host). The fixtures are created through the services and asserted BY NAME, which
// is a strictly stronger check than "found anything".
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTListTest,
	"VibeUE.BehaviorTree.Asset.List", kBTTestFlags)
bool FVibeBTListTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_ListFixture");
	const FString BBPath = FString(kBTTestDir) / TEXT("BB_ListFixture");
	FScopedFixtureReset ResetBT(BTPath);
	FScopedFixtureReset ResetBB(BBPath);

	TestTrue(TEXT("fixture blackboard created"), UBlackboardService::CreateBlackboard(BBPath));
	TestEqual(TEXT("fixture tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, BBPath), FString());

	const TArray<FString> Trees = UBehaviorTreeService::ListBehaviorTrees(kBTTestDir);
	TestTrue(TEXT("listing finds the fixture tree"), Trees.Contains(BTPath));
	AddInfo(FString::Printf(TEXT("ListBehaviorTrees(%s) found %d: %s"),
		kBTTestDir, Trees.Num(), *FString::Join(Trees, TEXT(", "))));

	const TArray<FString> Boards = UBlackboardService::ListBlackboards(kBTTestDir);
	TestTrue(TEXT("listing finds the fixture blackboard"), Boards.Contains(BBPath));

	// The wider sweep must INCLUDE the scoped result — /Game is above the fixture dir.
	TestTrue(TEXT("the /Game sweep includes the fixture tree"),
		UBehaviorTreeService::ListBehaviorTrees(TEXT("/Game")).Contains(BTPath));
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

	// Sentinels, so "a position was written" is distinguishable from "happened to be 0".
	Root->NodePosX = Root->NodePosY = -12345;
	Child->NodePosX = Child->NodePosY = -12345;

	// ArrangeGraph must handle the cycle without hanging or crashing — and must still have laid
	// out the acyclic part. (The previous version ended in TestTrue(..., true): it could only
	// ever catch a crash, and its own comment described assertions it never made.)
	TestEqual(TEXT("cycle arranged without error"), ArrangeGraph(Root), FString());
	TestEqual(TEXT("root placed at depth 0"), Root->NodePosY, 0);
	TestEqual(TEXT("child placed at depth 1"), Child->NodePosY, VibeBT::NodeSpacingY);
	TestNotEqual(TEXT("root X was written"), Root->NodePosX, -12345);
	TestNotEqual(TEXT("child X was written"), Child->NodePosX, -12345);
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

	Root->NodePosX = Root->NodePosY = -12345;
	Child->NodePosX = Child->NodePosY = -12345;

	// ArrangeGraph must deduplicate before allocating Mirror slots. Without deduplication the
	// phantom slot desynchronises positions from the node order — which is now a reported error
	// rather than an editor-killing check(), so both facts are asserted: no error, and the child
	// laid out exactly once, centred under its parent (one leaf column => same X).
	TestEqual(TEXT("duplicate link arranged without error"), ArrangeGraph(Root), FString());
	TestEqual(TEXT("root placed at depth 0"), Root->NodePosY, 0);
	TestEqual(TEXT("child placed at depth 1"), Child->NodePosY, VibeBT::NodeSpacingY);
	TestEqual(TEXT("single leaf column: parent directly above child"), Root->NodePosX, Child->NodePosX);
	return true;
}

// Diamond: TWO parents pin-linked to the SAME child. Distinct from DuplicateLink (one parent,
// two links): the child passes an up-front visited filter under its second parent — its first
// parent has not recursed yet — so a slot-allocation-time visited test is the only correct
// shape. The pre-fix BuildMirror allocated a Mirror slot the recursion then abandoned, and the
// position/order mismatch hit a check() that killed the editor on EVERY subsequent write to the
// asset. Now it must arrange cleanly, with the child owned by the first parent that reached it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTDiamondLinkTest,
	"VibeUE.BehaviorTree.Layout.Diamond", kBTTestFlags)
bool FVibeBTDiamondLinkTest::RunTest(const FString&)
{
	using namespace VibeBT;

	UBehaviorTree* Tree = NewObject<UBehaviorTree>(GetTransientPackage());
	UBehaviorTreeGraph* Graph = NewObject<UBehaviorTreeGraph>(Tree);

	auto MakeNode = [Graph]() -> UBehaviorTreeGraphNode_Composite*
	{
		UBehaviorTreeGraphNode_Composite* Node = NewObject<UBehaviorTreeGraphNode_Composite>(
			Graph, NAME_None, RF_NoFlags);
		Node->CreateNewGuid();
		Node->AllocateDefaultPins();
		Graph->AddNode(Node, false, false);
		Node->NodePosX = Node->NodePosY = -12345;
		return Node;
	};

	UBehaviorTreeGraphNode_Composite* Root = MakeNode();
	UBehaviorTreeGraphNode_Composite* A = MakeNode();
	UBehaviorTreeGraphNode_Composite* B = MakeNode();
	UBehaviorTreeGraphNode_Composite* C = MakeNode();

	Root->GetOutputPin()->MakeLinkTo(A->GetInputPin());
	Root->GetOutputPin()->MakeLinkTo(B->GetInputPin());
	A->GetOutputPin()->MakeLinkTo(C->GetInputPin());
	B->GetOutputPin()->MakeLinkTo(C->GetInputPin());

	TestEqual(TEXT("diamond arranged without error"), ArrangeGraph(Root), FString());
	TestEqual(TEXT("shared child placed at depth 2, under its first parent"),
		C->NodePosY, 2 * VibeBT::NodeSpacingY);
	TestNotEqual(TEXT("both mid nodes were placed"), A->NodePosX, -12345);
	TestNotEqual(TEXT("..."), B->NodePosX, -12345);
	TestTrue(TEXT("siblings strictly ordered"), A->NodePosX < B->NodePosX);
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

	// The Blueprint arm is a PROJECT-CONTENT assertion: it can only hold in a project that has at
	// least one Blueprint-derived BT node class. Where the host has none, that is a fact about the
	// host, not a discovery failure — reported as a skip, loudly, rather than failing the suite in
	// every project whose AI is native C++ (empirically: this plugin's own host project).
	const bool bAnyBlueprintNode =
		Tasks.ContainsByPredicate([](const FBTNodeClassInfo& C){ return C.bIsBlueprint; })
		|| UBehaviorTreeService::GetAvailableNodeTypes(TEXT("Decorator")).ContainsByPredicate(
			[](const FBTNodeClassInfo& C){ return C.bIsBlueprint; })
		|| UBehaviorTreeService::GetAvailableNodeTypes(TEXT("Service")).ContainsByPredicate(
			[](const FBTNodeClassInfo& C){ return C.bIsBlueprint; });
	if (!bAnyBlueprintNode)
	{
		AddWarning(TEXT("SKIPPED Blueprint-discovery arm: this project has no Blueprint-derived "
						"BT node classes, so whether discovery reports them cannot be exercised "
						"here. Run the suite in a project that has one to cover it."));
	}

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

	// SELF-SELECTING, not hard-coded: the previous version named one Blueprint asset from the
	// author's own project, which failed in every other host. The candidate is discovered from
	// the same registry-only listing GetAvailableNodeTypes uses (which performs no loads, so it
	// cannot warm the class it is about to nominate), preferring one that is genuinely not
	// resident so the cold-load path is what gets exercised.
	const TArray<FBTNodeClassInfo> Tasks =
		UBehaviorTreeService::GetAvailableNodeTypes(TEXT("Task"));

	const FBTNodeClassInfo* Cold = nullptr;
	const FBTNodeClassInfo* AnyBlueprint = nullptr;
	for (const FBTNodeClassInfo& Info : Tasks)
	{
		if (!Info.bIsBlueprint)
		{
			continue;
		}
		AnyBlueprint = AnyBlueprint ? AnyBlueprint : &Info;
		if (!FindObject<UClass>(nullptr, *Info.ClassPath))
		{
			Cold = &Info;
			break;
		}
	}

	if (!AnyBlueprint)
	{
		AddWarning(TEXT("SKIPPED: this project has no Blueprint-derived BT task classes, so "
						"Blueprint class resolution (cold or warm) cannot be exercised here."));
		return true;
	}
	const FBTNodeClassInfo& Candidate = Cold ? *Cold : *AnyBlueprint;
	AddInfo(FString::Printf(TEXT("candidate %s resident before resolve: %s"),
		*Candidate.ClassPath,
		Cold ? TEXT("false (cold-load path exercised)")
			 : TEXT("true (only the warm name-matching path can be exercised this run)")));

	// Resolve by SHORT name without the _C suffix — the form an agent naturally passes, and the
	// one that exercises the generated-name matching in ResolveNodeClass.
	FString ShortName = Candidate.ClassName;
	ShortName.RemoveFromEnd(TEXT("_C"));

	UClass* Resolved = ResolveNodeClass(ShortName, UBTTaskNode::StaticClass());
	if (!Resolved)
	{
		// A null here may be a genuine failure OR an ambiguous short name (two BP tasks sharing
		// it) — retry by full object path, which is unambiguous by construction.
		AddInfo(FString::Printf(TEXT("short name '%s' did not resolve (possibly ambiguous); "
			"retrying by full path"), *ShortName));
		Resolved = ResolveNodeClass(Candidate.ClassPath, UBTTaskNode::StaticClass());
	}
	TestNotNull(TEXT("resolves a Blueprint task class"), Resolved);
	if (Resolved)
	{
		TestTrue(TEXT("resolved class derives from UBTTaskNode"),
			Resolved->IsChildOf(UBTTaskNode::StaticClass()));
		TestEqual(TEXT("resolved to the expected generated class"),
			Resolved->GetName(), Candidate.ClassName);
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

// UBehaviorTreeFactory leaves BTGraph null — the editor creates it lazily on open. If the
// service does not create it, every later write has nothing to write to.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTCreateTest,
	"VibeUE.BehaviorTree.Asset.Create", kBTTestFlags)
bool FVibeBTCreateTest::RunTest(const FString&)
{
	const FString BBPath = FString(kBTTestDir) / TEXT("BB_CreateTest");
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_CreateTest");
	const FString BarePath = FString(kBTTestDir) / TEXT("BT_BareTest");
	// Never successfully created — but reset anyway, so that if a regression ever does create it,
	// the next run still starts clean instead of inheriting the wreckage.
	const FString MissingBBPath = FString(kBTTestDir) / TEXT("BT_MissingBBTest");
	FScopedFixtureReset ResetBB(BBPath);
	FScopedFixtureReset ResetBT(BTPath);
	FScopedFixtureReset ResetBare(BarePath);
	FScopedFixtureReset ResetMissingBB(MissingBBPath);

	TestTrue(TEXT("blackboard created"), UBlackboardService::CreateBlackboard(BBPath, FString()));
	TestEqual(TEXT("tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, BBPath), FString());

	FBTAssetInfo Info;
	TestTrue(TEXT("info readable"), UBehaviorTreeService::GetBehaviorTreeInfo(BTPath, Info));
	TestTrue(TEXT("graph exists"),   Info.bHasGraph);
	TestTrue(TEXT("root exists"),    Info.bHasRootNode);
	TestTrue(TEXT("blackboard set"), Info.BlackboardPath.Contains(TEXT("BB_CreateTest")));
	// A fresh tree is exactly the root node and nothing else.
	TestEqual(TEXT("one node on create"), Info.NodeCount, 1);

	// The only proof a save happened is the file: an in-memory re-read returns the same
	// in-process object whether or not anything reached disk, and Content/Developers is
	// gitignored so `git status` cannot show it either.
	const FString BTFile = VibeAITest::FixtureFilename(BTPath);
	TestTrue(TEXT("tree .uasset exists on disk"), IFileManager::Get().FileExists(*BTFile));
	const int64 BTFileSize = IFileManager::Get().FileSize(*BTFile);
	TestTrue(TEXT("tree .uasset is not empty"), BTFileSize > 0);
	AddInfo(FString::Printf(TEXT("on-disk: %s (%lld bytes, modified %s)"), *BTFile, BTFileSize,
		*IFileManager::Get().GetTimeStamp(*BTFile).ToString()));

	// Creating over an existing asset is an error, not a silent overwrite.
	TestTrue(TEXT("duplicate create rejected"),
		!UBehaviorTreeService::CreateBehaviorTree(BTPath, BBPath).IsEmpty());

	// ...but only while it really is there. This covers the in-process half of that: an asset
	// created and then deleted out of band during this run can be created again.
	//
	// It does NOT reproduce the cross-process half, which is where this actually broke: a .uasset
	// present when the process STARTED and deleted afterwards still reads as existing through
	// FPackageName::DoesPackageExist, whose package-path index is built at startup and never
	// invalidated by a direct file deletion. That is the state every rerun of this suite begins in
	// (the fixture reset deletes files exactly that way), and it is why CreateBehaviorTree asks
	// the filesystem instead. Reproducing it needs two processes, so it is verified by running the
	// suite against fixtures left behind by a separate run — see task-5-report.md.
	VibeAITest::ResetFixtureAsset(BTPath);
	TestEqual(TEXT("create succeeds again once the asset is deleted"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, BBPath), FString());

	// A tree with no blackboard is legitimate. Note that BB_CreateTest is loaded in this process
	// by now, which is exactly the condition under which UBehaviorTreeGraphNode_Root's
	// PostPlacedNewNode would otherwise assign it to this tree behind our back.
	TestEqual(TEXT("bare tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BarePath, FString()), FString());
	FBTAssetInfo BareInfo;
	TestTrue(TEXT("bare info readable"), UBehaviorTreeService::GetBehaviorTreeInfo(BarePath, BareInfo));
	TestTrue(TEXT("bare graph exists"), BareInfo.bHasGraph);
	TestEqual(TEXT("bare has no blackboard"), BareInfo.BlackboardPath, FString());

	const FString BareFile = VibeAITest::FixtureFilename(BarePath);
	TestTrue(TEXT("bare tree .uasset exists on disk"), IFileManager::Get().FileExists(*BareFile));
	AddInfo(FString::Printf(TEXT("on-disk: %s (%lld bytes, modified %s)"), *BareFile,
		IFileManager::Get().FileSize(*BareFile),
		*IFileManager::Get().GetTimeStamp(*BareFile).ToString()));

	// Bad input is reported, not half-applied.
	TestTrue(TEXT("empty path rejected"),
		!UBehaviorTreeService::CreateBehaviorTree(FString(), FString()).IsEmpty());
	TestTrue(TEXT("non-package path rejected"),
		!UBehaviorTreeService::CreateBehaviorTree(TEXT("NotAPath"), FString()).IsEmpty());
	TestTrue(TEXT("missing blackboard rejected"),
		!UBehaviorTreeService::CreateBehaviorTree(MissingBBPath, TEXT("/Game/NoSuchBlackboard")).IsEmpty());
	TestFalse(TEXT("a rejected create leaves no asset behind"),
		IFileManager::Get().FileExists(*VibeAITest::FixtureFilename(MissingBBPath)));

	return true;
}

namespace
{
	/**
	 * Minimal IAssetEditorInstance, registered through UAssetEditorSubsystem::NotifyAssetOpened,
	 * so the open-editor guard is exercised by a real subsystem query rather than asserted about.
	 * Opening a genuine FBehaviorTreeEditor is not possible under -nullrhi (it needs Slate), but
	 * the guard reads FindEditorsForAsset, and that reads the OpenedAssets map this fills — the
	 * same map every real toolkit registers itself in.
	 *
	 * IncludeAssetInRestoreOpenAssetsPrompt returns false so registering does not rewrite the
	 * editor's "reopen these assets" config on a test machine.
	 */
	class FVibeFakeAssetEditor : public IAssetEditorInstance
	{
	public:
		virtual FName GetEditorName() const override { return FName(TEXT("VibeUEFakeBTEditor")); }
		virtual void FocusWindow(UObject* /*ObjectToFocusOn*/ = nullptr) override {}
		virtual bool CloseWindow(EAssetEditorCloseReason /*InCloseReason*/) override { return true; }
		virtual bool IncludeAssetInRestoreOpenAssetsPrompt(UObject* /*Asset*/) const override { return false; }
		virtual bool IsPrimaryEditor() const override { return true; }
		virtual void InvokeTab(const FTabId& /*TabId*/) override {}
		virtual TSharedPtr<FTabManager> GetAssociatedTabManager() override { return nullptr; }
		virtual double GetLastActivationTime() override { return 0.0; }
		virtual void RemoveEditingAsset(UObject* /*Asset*/) override {}
	};
}

// The two write guards, and the fact that a commit actually reaches disk. Both guards protect
// against a write that reports success and changes nothing durable: an open editor overwrites it
// on its next save, and bLockUpdates makes UpdateAsset() a silent no-op so the runtime tree is
// never regenerated from the graph.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTWriteGuardTest,
	"VibeUE.BehaviorTree.Asset.WriteGuards", kBTTestFlags)
bool FVibeBTWriteGuardTest::RunTest(const FString&)
{
	const FString BBPath = FString(kBTTestDir) / TEXT("BB_GuardTest");
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_GuardTest");
	const FString BarePath = FString(kBTTestDir) / TEXT("BT_GuardBareTest");
	FScopedFixtureReset ResetBB(BBPath);
	FScopedFixtureReset ResetBT(BTPath);
	FScopedFixtureReset ResetBare(BarePath);

	TestTrue(TEXT("blackboard created"), UBlackboardService::CreateBlackboard(BBPath, FString()));
	TestEqual(TEXT("tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, BBPath), FString());
	TestEqual(TEXT("bare tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BarePath, FString()), FString());

	UBehaviorTree* Tree = nullptr;
	UBehaviorTreeGraph* Graph = nullptr;
	TestEqual(TEXT("write guard opens a healthy tree"),
		VibeBT::OpenWriteGuard(BTPath, Tree, Graph), FString());
	if (!TestNotNull(TEXT("guard returned the tree"), Tree) ||
		!TestNotNull(TEXT("guard returned the graph"), Graph))
	{
		return false;
	}

	// --- Guard 1: a Behavior Tree editor is open on the asset. ---------------------------------
	UAssetEditorSubsystem* AssetEditorSubsystem =
		GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (TestNotNull(TEXT("asset editor subsystem available"), AssetEditorSubsystem))
	{
		FVibeFakeAssetEditor FakeEditor;
		AssetEditorSubsystem->NotifyAssetOpened(Tree, &FakeEditor);

		UBehaviorTree* BlockedTree = Tree;
		UBehaviorTreeGraph* BlockedGraph = Graph;
		const FString OpenEditorError = VibeBT::OpenWriteGuard(BTPath, BlockedTree, BlockedGraph);
		TestTrue(TEXT("guard refuses while an editor is open"), !OpenEditorError.IsEmpty());
		TestTrue(TEXT("the refusal names the open editor"), OpenEditorError.Contains(TEXT("editor is open")));
		TestNull(TEXT("a refused guard hands back no tree"), BlockedTree);
		TestNull(TEXT("a refused guard hands back no graph"), BlockedGraph);
		TestTrue(TEXT("CompileAndSave refuses too"),
			!UBehaviorTreeService::CompileAndSave(BTPath).IsEmpty());

		AssetEditorSubsystem->NotifyAssetClosed(Tree, &FakeEditor);
		TestEqual(TEXT("closing the editor unblocks the write"),
			UBehaviorTreeService::CompileAndSave(BTPath), FString());
	}

	// --- Guard 2: bLockUpdates. UpdateAsset() early-returns under it, so a commit would save a
	//     stale runtime tree and still report success. ------------------------------------------
	Graph->LockUpdates();
	const FString LockedError = UBehaviorTreeService::CompileAndSave(BTPath);
	TestTrue(TEXT("a locked graph refuses the write"), !LockedError.IsEmpty());
	TestTrue(TEXT("the refusal names the lock"), LockedError.Contains(TEXT("bLockUpdates")));
	Graph->UnlockUpdates();
	TestEqual(TEXT("unlocking restores the write"),
		UBehaviorTreeService::CompileAndSave(BTPath), FString());

	// --- The commit reaches disk. ---------------------------------------------------------------
	// Removing the file first makes this unambiguous: the package is already clean at this point
	// (it was just saved), so if CommitGraph relied on SavePackages' bOnlyDirty behaviour the file
	// would simply never come back.
	const FString BTFile = VibeAITest::FixtureFilename(BTPath);
	IFileManager::Get().Delete(*BTFile, /*RequireExists*/ false, /*EvenReadOnly*/ true);
	TestFalse(TEXT("tree .uasset removed from disk"), IFileManager::Get().FileExists(*BTFile));
	TestEqual(TEXT("CompileAndSave on a clean package"),
		UBehaviorTreeService::CompileAndSave(BTPath), FString());
	TestTrue(TEXT("the commit wrote the .uasset back"), IFileManager::Get().FileExists(*BTFile));
	TestTrue(TEXT("the rewritten .uasset is not empty"), IFileManager::Get().FileSize(*BTFile) > 0);
	AddInfo(FString::Printf(TEXT("rewritten on-disk: %s (%lld bytes, modified %s)"), *BTFile,
		IFileManager::Get().FileSize(*BTFile),
		*IFileManager::Get().GetTimeStamp(*BTFile).ToString()));

	// --- SetBlackboardAsset, including clearing it. ---------------------------------------------
	TestEqual(TEXT("blackboard assigned"),
		UBehaviorTreeService::SetBlackboardAsset(BarePath, BBPath), FString());
	FBTAssetInfo BareInfo;
	TestTrue(TEXT("bare info readable"), UBehaviorTreeService::GetBehaviorTreeInfo(BarePath, BareInfo));
	TestTrue(TEXT("blackboard shows on the tree"), BareInfo.BlackboardPath.Contains(TEXT("BB_GuardTest")));

	TestTrue(TEXT("an unknown blackboard is rejected"),
		!UBehaviorTreeService::SetBlackboardAsset(BarePath, TEXT("/Game/NoSuchBlackboard")).IsEmpty());
	TestTrue(TEXT("bare info still readable"), UBehaviorTreeService::GetBehaviorTreeInfo(BarePath, BareInfo));
	TestTrue(TEXT("a rejected assignment changed nothing"),
		BareInfo.BlackboardPath.Contains(TEXT("BB_GuardTest")));

	TestEqual(TEXT("blackboard cleared"),
		UBehaviorTreeService::SetBlackboardAsset(BarePath, FString()), FString());
	TestTrue(TEXT("bare info readable after clear"),
		UBehaviorTreeService::GetBehaviorTreeInfo(BarePath, BareInfo));
	TestEqual(TEXT("cleared blackboard is reported as none"), BareInfo.BlackboardPath, FString());

	// --- EnsureGraph is idempotent, and repairs a graph whose root went missing without
	//     inventing a blackboard for it on the way. -----------------------------------------------
	UBehaviorTree* BareTree = LoadObject<UBehaviorTree>(nullptr, *BarePath);
	if (!TestNotNull(TEXT("bare tree loaded"), BareTree))
	{
		return false;
	}
	TestEqual(TEXT("EnsureGraph is a no-op on a healthy tree"), VibeBT::EnsureGraph(BareTree), FString());
	TestTrue(TEXT("info readable after EnsureGraph"),
		UBehaviorTreeService::GetBehaviorTreeInfo(BarePath, BareInfo));
	TestEqual(TEXT("EnsureGraph added no nodes"), BareInfo.NodeCount, 1);

	UBehaviorTreeGraph* BareGraph = Cast<UBehaviorTreeGraph>(BareTree->BTGraph);
	if (!TestNotNull(TEXT("bare graph present"), BareGraph))
	{
		return false;
	}
	for (int32 Index = BareGraph->Nodes.Num() - 1; Index >= 0; --Index)
	{
		if (Cast<UBehaviorTreeGraphNode_Root>(BareGraph->Nodes[Index]))
		{
			BareGraph->RemoveNode(BareGraph->Nodes[Index]);
		}
	}
	TestTrue(TEXT("root removed"), UBehaviorTreeService::GetBehaviorTreeInfo(BarePath, BareInfo));
	TestFalse(TEXT("no root before repair"), BareInfo.bHasRootNode);

	TestEqual(TEXT("EnsureGraph repairs a missing root"), VibeBT::EnsureGraph(BareTree), FString());
	TestTrue(TEXT("info readable after repair"),
		UBehaviorTreeService::GetBehaviorTreeInfo(BarePath, BareInfo));
	TestTrue(TEXT("root restored"), BareInfo.bHasRootNode);
	// The spawned root's PostPlacedNewNode picks up whatever blackboard is loaded (BB_GuardTest
	// certainly is, by now) and pushes it onto the asset. It must not survive.
	TestEqual(TEXT("repair did not invent a blackboard"), BareInfo.BlackboardPath, FString());

	// --- Missing assets are reported, never silently treated as empty. --------------------------
	const FString MissingPath = FString(kBTTestDir) / TEXT("BT_NoSuchTree");
	FBTAssetInfo MissingInfo;
	TestFalse(TEXT("info on a missing tree fails"),
		UBehaviorTreeService::GetBehaviorTreeInfo(MissingPath, MissingInfo));
	TestTrue(TEXT("the failure is explained"), !MissingInfo.Error.IsEmpty());
	TestTrue(TEXT("CompileAndSave on a missing tree fails"),
		!UBehaviorTreeService::CompileAndSave(MissingPath).IsEmpty());
	TestTrue(TEXT("SetBlackboardAsset on a missing tree fails"),
		!UBehaviorTreeService::SetBlackboardAsset(MissingPath, BBPath).IsEmpty());

	return true;
}

namespace
{
	/** Forces UAISystem's DefaultBlackboard for the scope, so the root-node spawn's blackboard
	 *  hijack picks a known board instead of "whichever one loaded first in this process". */
	struct FScopedDefaultBlackboard
	{
		TSoftObjectPtr<UBlackboardData> Previous;
		explicit FScopedDefaultBlackboard(UBlackboardData* Board)
		{
			UAISystem* Config = GetMutableDefault<UAISystem>();
			Previous = Config->DefaultBlackboard;
			Config->DefaultBlackboard = Board;
		}
		~FScopedDefaultBlackboard()
		{
			GetMutableDefault<UAISystem>()->DefaultBlackboard = Previous;
		}
	};

	/** The BlackboardKey selector of a decorator instance. Protected on UBTDecorator_BlackboardBase,
	 *  so reached through reflection — the same access path the Blackboard service's own sweep uses. */
	FBlackboardKeySelector* GetKeySelector(UBTDecorator_Blackboard* Decorator)
	{
		FStructProperty* KeyProp = CastField<FStructProperty>(
			UBTDecorator_BlackboardBase::StaticClass()->FindPropertyByName(TEXT("BlackboardKey")));
		return KeyProp ? KeyProp->ContainerPtrToValuePtr<FBlackboardKeySelector>(Decorator) : nullptr;
	}
}

// Regression test for the Important review finding on EnsureGraph's blackboard restore.
//
// Spawning the root node does not merely assign a blackboard: PostPlacedNewNode calls
// UpdateBlackboard(), which runs UpdateBlackboardChange() and re-runs InitializeFromAsset on every
// node instance in the graph, re-resolving each blackboard key selector's cached key ID against
// the hijacking board (BehaviorTreeGraph.cpp:50-95). Writing the two pointers back afterwards
// restores what GetBehaviorTreeInfo reports while leaving the node instances bound elsewhere.
//
// Only observable when EnsureGraph repairs a POPULATED graph and the caller then bails without
// committing — CommitGraph's UpdateAsset() ends in UpdateBlackboardChange() and washes it out.
//
// Both boards carry "Alpha", at different key IDs, so the assertion is an ID comparison rather
// than a valid/invalid one: that keeps the failure mode "bound to the wrong board" rather than
// "unresolved", and avoids a spurious engine warning about a key that cannot be found.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTRepairKeyBindingTest,
	"VibeUE.BehaviorTree.Asset.RepairKeyBinding", kBTTestFlags)
bool FVibeBTRepairKeyBindingTest::RunTest(const FString&)
{
	const FString BoardAPath = FString(kBTTestDir) / TEXT("BB_RepairA");
	const FString BoardBPath = FString(kBTTestDir) / TEXT("BB_RepairB");
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_RepairTest");
	FScopedFixtureReset ResetA(BoardAPath);
	FScopedFixtureReset ResetB(BoardBPath);
	FScopedFixtureReset ResetBT(BTPath);

	// Board A: [SelfActor, Alpha]. Board B: [SelfActor, Beta, Alpha] — same key name, different ID.
	TestTrue(TEXT("board A created"), UBlackboardService::CreateBlackboard(BoardAPath, FString()));
	TestEqual(TEXT("A.Alpha added"),
		UBlackboardService::AddBlackboardKey(BoardAPath, TEXT("Alpha"), TEXT("Bool"), false), FString());
	TestTrue(TEXT("board B created"), UBlackboardService::CreateBlackboard(BoardBPath, FString()));
	TestEqual(TEXT("B.Beta added"),
		UBlackboardService::AddBlackboardKey(BoardBPath, TEXT("Beta"), TEXT("Bool"), false), FString());
	TestEqual(TEXT("B.Alpha added"),
		UBlackboardService::AddBlackboardKey(BoardBPath, TEXT("Alpha"), TEXT("Bool"), false), FString());

	UBlackboardData* BoardA = LoadObject<UBlackboardData>(nullptr, *BoardAPath);
	UBlackboardData* BoardB = LoadObject<UBlackboardData>(nullptr, *BoardBPath);
	if (!TestNotNull(TEXT("board A loaded"), BoardA) || !TestNotNull(TEXT("board B loaded"), BoardB))
	{
		return false;
	}

	const FBlackboard::FKey AlphaOnA = BoardA->GetKeyID(TEXT("Alpha"));
	const FBlackboard::FKey AlphaOnB = BoardB->GetKeyID(TEXT("Alpha"));
	// Without this the test would still pass while proving nothing.
	TestTrue(TEXT("Alpha resolves on both boards"),
		AlphaOnA != FBlackboard::InvalidKey && AlphaOnB != FBlackboard::InvalidKey);
	TestTrue(TEXT("Alpha has a different key ID on each board, so the two are distinguishable"),
		AlphaOnA != AlphaOnB);

	TestEqual(TEXT("tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, BoardAPath), FString());
	UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
	if (!TestNotNull(TEXT("tree loaded"), Tree))
	{
		return false;
	}
	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
	if (!TestNotNull(TEXT("graph present"), Graph))
	{
		return false;
	}

	// Populate the graph: a composite carrying one Blackboard decorator bound to "Alpha". This is
	// what UpdateBlackboardChange walks (Graph->Nodes, then each node's Decorators/Services).
	UBehaviorTreeGraphNode_Composite* Composite =
		NewObject<UBehaviorTreeGraphNode_Composite>(Graph, NAME_None, RF_Transactional);
	Composite->CreateNewGuid();
	Composite->AllocateDefaultPins();
	Composite->NodeInstance = NewObject<UBTComposite_Sequence>(Tree, NAME_None, RF_Transactional);
	Graph->AddNode(Composite, false, false);

	UBehaviorTreeGraphNode_Decorator* DecoratorNode =
		NewObject<UBehaviorTreeGraphNode_Decorator>(Graph, NAME_None, RF_Transactional);
	DecoratorNode->CreateNewGuid();
	UBTDecorator_Blackboard* DecoratorInstance =
		NewObject<UBTDecorator_Blackboard>(Tree, NAME_None, RF_Transactional);
	DecoratorNode->NodeInstance = DecoratorInstance;
	Composite->Decorators.Add(DecoratorNode);

	FBlackboardKeySelector* Selector = GetKeySelector(DecoratorInstance);
	if (!TestNotNull(TEXT("BlackboardKey selector reached"), Selector))
	{
		return false;
	}
	Selector->SelectedKeyName = TEXT("Alpha");

	// Baseline: resolved against the tree's real board.
	Graph->UpdateBlackboardChange();
	TestEqual(TEXT("decorator starts bound to board A's Alpha"),
		Selector->GetSelectedKeyID(), AlphaOnA);

	{
		// Make the hijack deterministic: PostPlacedNewNode prefers the AI config's DefaultBlackboard
		// when it is already loaded, and only falls back to iterating loaded boards otherwise.
		FScopedDefaultBlackboard ForceDefault(BoardB);
		TestTrue(TEXT("the forced default board is resident, so the root spawn will pick it"),
			GetDefault<UAISystem>()->DefaultBlackboard.IsValid());

		for (int32 Index = Graph->Nodes.Num() - 1; Index >= 0; --Index)
		{
			if (Cast<UBehaviorTreeGraphNode_Root>(Graph->Nodes[Index]))
			{
				Graph->RemoveNode(Graph->Nodes[Index]);
			}
		}

		// The bail-without-committing path: EnsureGraph alone, no CommitGraph to wash it out.
		TestEqual(TEXT("EnsureGraph repairs the missing root"), VibeBT::EnsureGraph(Tree), FString());

		// FBlackboard::FKey's TestEqual overload reports only "The two values are not equal", so
		// the values are logged here — otherwise a failure gives nothing to diagnose from.
		AddInfo(FString::Printf(
			TEXT("after repair: tree board=%s, decorator key id=%d (A.Alpha=%d, B.Alpha=%d)"),
			*GetNameSafe(ToRawPtr(Tree->BlackboardAsset)),
			(int32)Selector->GetSelectedKeyID(), (int32)AlphaOnA, (int32)AlphaOnB));
		TestTrue(TEXT("root restored"), Graph->Nodes.ContainsByPredicate(
			[](const UEdGraphNode* Node){ return Node && Node->IsA<UBehaviorTreeGraphNode_Root>(); }));

		// The pointer half of the restore.
		TestEqual(TEXT("the tree still points at board A"), ToRawPtr(Tree->BlackboardAsset), BoardA);

		// The half a pointer write cannot do: the decorator instance was re-resolved against board B
		// during the spawn, and must have been re-resolved back.
		TestEqual(TEXT("the decorator is still bound to board A's Alpha, not board B's"),
			Selector->GetSelectedKeyID(), AlphaOnA);
	}

	// The lock refusal must be side-effect-free too: a locked graph missing its root is refused
	// WITHOUT spawning one (which would Modify the graph and churn every node's key bindings).
	for (int32 Index = Graph->Nodes.Num() - 1; Index >= 0; --Index)
	{
		if (Cast<UBehaviorTreeGraphNode_Root>(Graph->Nodes[Index]))
		{
			Graph->RemoveNode(Graph->Nodes[Index]);
		}
	}
	Graph->LockUpdates();

	UBehaviorTree* GuardTree = nullptr;
	UBehaviorTreeGraph* GuardGraph = nullptr;
	const FString LockedError = VibeBT::OpenWriteGuard(BTPath, GuardTree, GuardGraph);
	TestTrue(TEXT("a locked graph is refused"), !LockedError.IsEmpty());
	TestTrue(TEXT("the refusal names the lock"), LockedError.Contains(TEXT("bLockUpdates")));
	TestFalse(TEXT("the refused write did not spawn a root node"), Graph->Nodes.ContainsByPredicate(
		[](const UEdGraphNode* Node){ return Node && Node->IsA<UBehaviorTreeGraphNode_Root>(); }));

	Graph->UnlockUpdates();
	return true;
}

// The play-session refusal. A write during PIE is not merely awkward: CommitGraph's OnSave() ->
// UpdateAsset() ends in UAIGraph::RemoveOrphanedNodes(), which marks every unreferenced UBTNode
// RF_Transient and renames it into the transient package (AIGraph.cpp:215-236) — possibly one a live
// UBehaviorTreeComponent is executing — and then saves the package. The open-editor refusal does not
// cover it, because a running game holds no asset editor.
//
// Testability: the automation harness runs headless (-nullrhi -unattended) and cannot start a play
// session, so the decision is a separate predicate, VibeBT::PlaySessionRefusal, and that is what is
// asserted here in both directions. The wiring from OpenWriteGuard into it is then proved separately
// by pointing GEditor->PlayWorld at a real world for the duration of exactly one guard call — which
// is what the guard tests, and all it tests: nothing else in this process reads PlayWorld in that
// window, and the value is restored whatever happens.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTPlaySessionGuardTest,
	"VibeUE.BehaviorTree.Asset.PlaySessionGuard", kBTTestFlags)
bool FVibeBTPlaySessionGuardTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_PlaySessionGuardTest");
	FScopedFixtureReset ResetBT(BTPath);

	// --- The predicate, both ways. -------------------------------------------------------------
	TestEqual(TEXT("no play world means no refusal"),
		VibeBT::PlaySessionRefusal(BTPath, nullptr), FString());

	UWorld* const StandInWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("an editor world is available to stand in for a play world"), StandInWorld))
	{
		return false;
	}

	const FString Refusal = VibeBT::PlaySessionRefusal(BTPath, StandInWorld);
	AddInfo(FString::Printf(TEXT("refusal text: %s"), *Refusal));
	TestTrue(TEXT("a play world is refused"), !Refusal.IsEmpty());
	TestTrue(TEXT("the refusal names the asset"), Refusal.Contains(TEXT("BT_PlaySessionGuardTest")));
	TestTrue(TEXT("the refusal says a play session is what is in the way"),
		Refusal.Contains(TEXT("Play In Editor")));

	// --- The wiring: OpenWriteGuard actually asks. ----------------------------------------------
	TestEqual(TEXT("tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, FString()), FString());

	UBehaviorTree* GuardTree = nullptr;
	UBehaviorTreeGraph* GuardGraph = nullptr;
	TestEqual(TEXT("the guard opens the asset with no play session"),
		VibeBT::OpenWriteGuard(BTPath, GuardTree, GuardGraph), FString());
	TestNotNull(TEXT("...and hands back the tree"), GuardTree);

	UWorld* const PlayWorldBefore = ToRawPtr(GEditor->PlayWorld);
	FString GuardedError;
	{
		// Restores on every path out, including an exception or an early return added later.
		struct FScopedPlayWorld
		{
			UWorld* Previous;
			explicit FScopedPlayWorld(UWorld* World) : Previous(ToRawPtr(GEditor->PlayWorld))
			{
				GEditor->PlayWorld = World;
			}
			~FScopedPlayWorld() { GEditor->PlayWorld = Previous; }
		} PretendPIE(StandInWorld);

		GuardTree = nullptr;
		GuardGraph = nullptr;
		GuardedError = VibeBT::OpenWriteGuard(BTPath, GuardTree, GuardGraph);
	}

	TestEqual(TEXT("PlayWorld is restored"), ToRawPtr(GEditor->PlayWorld), PlayWorldBefore);
	TestTrue(TEXT("the guard refuses while a play world is set"), !GuardedError.IsEmpty());
	TestEqual(TEXT("and refuses with exactly the predicate's message"), GuardedError,
		VibeBT::PlaySessionRefusal(BTPath, StandInWorld));
	TestNull(TEXT("a refused write hands back no tree"), GuardTree);
	TestNull(TEXT("...and no graph"), GuardGraph);

	// The refusal is a condition, not a latch.
	GuardTree = nullptr;
	GuardGraph = nullptr;
	TestEqual(TEXT("the guard opens the asset again once the play world is gone"),
		VibeBT::OpenWriteGuard(BTPath, GuardTree, GuardGraph), FString());
	TestNotNull(TEXT("...and hands back the tree again"), GuardTree);

	return true;
}

// UBehaviorTreeGraph::CreateBTFromGraph opens by discarding the runtime tree outright and rebuilds
// it from the graph, and UpdateAsset only hands it something to rebuild from when the graph root's
// output pin leads somewhere. Committing an empty or sparse graph over an asset whose node tree is
// intact therefore destroys that tree on disk with no undo — the hazard that motivated the guard,
// on an asset (BT_Villager) whose graph is suspected missing while its runtime tree is populated.
//
// Both directions are asserted: the guard must fire on that shape, and must NOT fire on an
// ordinary healthy commit, or it would silently block every write in Tasks 6-10.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTCommitDiscardGuardTest,
	"VibeUE.BehaviorTree.Asset.CommitDiscardGuard", kBTTestFlags)
bool FVibeBTCommitDiscardGuardTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_DiscardGuardTest");
	FScopedFixtureReset ResetBT(BTPath);

	TestEqual(TEXT("tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, FString()), FString());
	UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
	if (!TestNotNull(TEXT("tree loaded"), Tree))
	{
		return false;
	}
	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
	if (!TestNotNull(TEXT("graph present"), Graph))
	{
		return false;
	}
	UBehaviorTreeGraphNode_Root* Root = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UBehaviorTreeGraphNode_Root* Candidate = Cast<UBehaviorTreeGraphNode_Root>(Node))
		{
			Root = Candidate;
			break;
		}
	}
	if (!TestNotNull(TEXT("root node present"), Root))
	{
		return false;
	}

	// Give the tree a real runtime node tree, built by the real path: a composite wired under the
	// root, then a normal commit, which is what runs CreateBTFromGraph and populates RootNode.
	UBehaviorTreeGraphNode_Composite* Composite =
		NewObject<UBehaviorTreeGraphNode_Composite>(Graph, NAME_None, RF_Transactional);
	Composite->CreateNewGuid();
	Composite->AllocateDefaultPins();
	UBTComposite_Sequence* Sequence = NewObject<UBTComposite_Sequence>(Tree, NAME_None, RF_Transactional);
	Composite->NodeInstance = Sequence;
	Graph->AddNode(Composite, false, false);

	UEdGraphPin* RootOut = Root->Pins.Num() > 0 ? Root->Pins[0] : nullptr;
	UEdGraphPin* CompositeIn = Composite->GetInputPin();
	if (!TestNotNull(TEXT("root output pin"), RootOut) ||
		!TestNotNull(TEXT("composite input pin"), CompositeIn))
	{
		return false;
	}
	RootOut->MakeLinkTo(CompositeIn);

	// Direction 1: an ordinary healthy commit is not blocked.
	TestEqual(TEXT("a healthy commit is not blocked by the guard"),
		UBehaviorTreeService::CompileAndSave(BTPath), FString());
	TestEqual(TEXT("the commit built the runtime tree"), ToRawPtr(Tree->RootNode),
		static_cast<UBTCompositeNode*>(Sequence));

	// Direction 2: the graph now says "nothing under the root" while the asset still has a tree.
	RootOut->BreakAllPinLinks();
	TestEqual(TEXT("the graph root now feeds nothing"), RootOut->LinkedTo.Num(), 0);
	TestNotNull(TEXT("the runtime tree is still populated going in"), ToRawPtr(Tree->RootNode));

	const FString DiscardError = UBehaviorTreeService::CompileAndSave(BTPath);
	TestTrue(TEXT("the commit is refused"), !DiscardError.IsEmpty());
	TestTrue(TEXT("the refusal names the asset"), DiscardError.Contains(TEXT("BT_DiscardGuardTest")));
	TestTrue(TEXT("the refusal says what it is protecting"),
		DiscardError.Contains(TEXT("discard")) || DiscardError.Contains(TEXT("runtime")));
	// The point of the guard: the tree survived.
	TestEqual(TEXT("the runtime tree was NOT discarded"), ToRawPtr(Tree->RootNode),
		static_cast<UBTCompositeNode*>(Sequence));

	// Every mutating entry point commits through CommitGraph, so all of them are covered.
	const FString BlackboardError = UBehaviorTreeService::SetBlackboardAsset(BTPath, FString());
	TestTrue(TEXT("SetBlackboardAsset is refused the same way"), !BlackboardError.IsEmpty());
	TestEqual(TEXT("and the runtime tree still survived"), ToRawPtr(Tree->RootNode),
		static_cast<UBTCompositeNode*>(Sequence));

	// The guard is a condition, not a latch: restoring the link restores the write.
	RootOut->MakeLinkTo(CompositeIn);
	TestEqual(TEXT("relinking the graph unblocks the commit"),
		UBehaviorTreeService::CompileAndSave(BTPath), FString());
	TestEqual(TEXT("the runtime tree is intact after the successful commit"), ToRawPtr(Tree->RootNode),
		static_cast<UBTCompositeNode*>(Sequence));

	return true;
}

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
// tests RootEdNode (BehaviorTreeGraph.cpp:510-517). That is a null dereference inside OnSave(), so
// the post-OnSave check can never see this shape; only refusing before the commit stops it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTCommitNullInstanceGuardTest,
	"VibeUE.BehaviorTree.Asset.CommitNullInstanceGuard", kBTTestFlags)
bool FVibeBTCommitNullInstanceGuardTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_NullInstanceGuardTest");
	FScopedFixtureReset ResetBT(BTPath);

	FPopulatedTree Fixture;
	if (!BuildPopulatedTree(*this, BTPath, Fixture))
	{
		return false;
	}

	// Re-point the root at a task graph node that carries no instance at all.
	Fixture.RootOut->BreakAllPinLinks();
	UBehaviorTreeGraphNode_Task* EmptyTask =
		NewObject<UBehaviorTreeGraphNode_Task>(Fixture.Graph, NAME_None, RF_Transactional);
	EmptyTask->CreateNewGuid();
	EmptyTask->AllocateDefaultPins();
	EmptyTask->NodeInstance = nullptr;
	Fixture.Graph->AddNode(EmptyTask, false, false);
	UEdGraphPin* TaskIn = EmptyTask->GetInputPin();
	if (!TestNotNull(TEXT("task input pin"), TaskIn))
	{
		return false;
	}
	Fixture.RootOut->MakeLinkTo(TaskIn);
	TestEqual(TEXT("the root now feeds exactly one node"), Fixture.RootOut->LinkedTo.Num(), 1);

	// If this returns instead of refusing, the process does not survive to report it.
	const FString Error = UBehaviorTreeService::CompileAndSave(BTPath);
	TestTrue(TEXT("the commit is refused"), !Error.IsEmpty());
	TestTrue(TEXT("the refusal names the asset"), Error.Contains(TEXT("BT_NullInstanceGuardTest")));
	TestEqual(TEXT("the runtime tree was NOT discarded"), ToRawPtr(Fixture.Tree->RootNode),
		static_cast<UBTCompositeNode*>(Fixture.Sequence));

	return true;
}

// RepairGraphFromRuntimeTree: the explicit, opt-in rebuild for an asset whose graph holds nothing
// but its root while its runtime tree is intact (the BT_Villager shape). Never implicit — the
// refusal cases below are what keep it that way.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTStructureTest,
	"VibeUE.BehaviorTree.Structure.Crud", kBTTestFlags)
bool FVibeBTStructureTest::RunTest(const FString&)
{
	const FString BTPath = FString(TEXT("/Game/Developers/VibeUEBTTests")) / TEXT("BT_StructTest");
	// Not in the brief, but the suite has to survive being run twice in a row without anything
	// being deleted in between, and CreateBehaviorTree refuses to overwrite an existing asset.
	FScopedFixtureReset ResetBT(BTPath);

	TestEqual(TEXT("fixture tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, FString()), FString());

	// Graph node count, sub-nodes included — GetBehaviorTreeInfo's own arithmetic, which until now
	// has only ever been exercised on a tree holding nothing but its root node.
	auto NodeCount = [&]() -> int32
	{
		FBTAssetInfo Info;
		return UBehaviorTreeService::GetBehaviorTreeInfo(BTPath, Info) ? Info.NodeCount : -1;
	};
	TestEqual(TEXT("a fresh tree holds only its root"), NodeCount(), 1);

	// Root -> Selector
	const FString Sel = UBehaviorTreeService::AddNode(
		BTPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	TestFalse(TEXT("selector added"), Sel.StartsWith(TEXT("ERROR")));

	// Selector -> Sequence, Sequence
	const FString SeqA = UBehaviorTreeService::AddNode(BTPath, Sel, TEXT("BTComposite_Sequence"), -1);
	const FString SeqB = UBehaviorTreeService::AddNode(BTPath, Sel, TEXT("BTComposite_Sequence"), -1);
	TestFalse(TEXT("seqA added"), SeqA.StartsWith(TEXT("ERROR")));
	TestFalse(TEXT("seqB added"), SeqB.StartsWith(TEXT("ERROR")));
	TestNotEqual(TEXT("siblings get distinct paths"), SeqA, SeqB);

	// A task under the first sequence.
	const FString Task = UBehaviorTreeService::AddNode(BTPath, SeqA, TEXT("BTTask_Wait"), -1);
	TestFalse(TEXT("task added"), Task.StartsWith(TEXT("ERROR")));

	TestEqual(TEXT("four nodes plus the root"), NodeCount(), 5);

	// A task has no output pin — nothing can be added under it.
	TestTrue(TEXT("no children under a task"),
		UBehaviorTreeService::AddNode(BTPath, Task, TEXT("BTTask_Wait"), -1).StartsWith(TEXT("ERROR")));

	// An unknown class is rejected.
	TestTrue(TEXT("unknown class rejected"),
		UBehaviorTreeService::AddNode(BTPath, Sel, TEXT("BTTask_Nonexistent"), -1).StartsWith(TEXT("ERROR")));

	// The root has exactly one child slot and it is taken, so nothing more goes under it whatever
	// the class — a second child there would be silently dropped by UpdateAsset. Both assertions
	// check the message, because "it errored" would also hold if AddNode had rejected the class, the
	// path, or the whole call for an unrelated reason.
	//
	// Note this is the occupied-slot refusal, not the schema's. The root's pin is
	// PinCategory_SingleComposite and a task linked there crashes OnSave() outright
	// (CreateBTFromGraph hands a null BTAsset->RootNode to CreateChildren, which opens with
	// RootNode->Children.Reset() behind a guard that only tests the *ed-graph* node,
	// BehaviorTreeGraph.cpp:915-936 and 511-518) — but reaching that needs the root slot free, i.e.
	// a tree with no runtime root, where a regression would take the automation host down rather
	// than report. That schema refusal is exercised safely by Structure.SimpleParallel instead,
	// which asks for a composite in a SingleTask slot.
	TestTrue(TEXT("no task directly under the root"),
		UBehaviorTreeService::AddNode(BTPath, TEXT("Root"), TEXT("BTTask_Wait"), -1)
			.Contains(TEXT("no free child slot")));
	TestTrue(TEXT("no second child under the root"),
		UBehaviorTreeService::AddNode(BTPath, TEXT("Root"), TEXT("BTComposite_Sequence"), -1)
			.Contains(TEXT("no free child slot")));
	TestTrue(TEXT("the root's one slot is not replaceable by index"),
		UBehaviorTreeService::AddNode(BTPath, TEXT("Root"), TEXT("BTComposite_Sequence"), 0)
			.Contains(TEXT("not replaceable through AddNode")));

	// A refused add must not leave a stray node behind in the graph.
	TestEqual(TEXT("refused adds changed nothing"), NodeCount(), 5);

	// Child GUIDs of the Selector, in the order GetTree reports them. Ordering is the whole
	// point of this test: BT execution order is node X position, so a layout bug silently
	// reorders execution and nothing else would catch it.
	auto SelectorChildGuids = [&]() -> TArray<FString>
	{
		TArray<FString> Guids;
		const TSharedPtr<FJsonObject> Tree = ParseTree(UBehaviorTreeService::GetTree(BTPath));
		if (!Tree.IsValid()) { return Guids; }

		// Root has exactly one child, the Selector.
		const TArray<TSharedPtr<FJsonValue>>& RootKids = Tree->GetArrayField(TEXT("children"));
		if (RootKids.Num() != 1) { return Guids; }

		for (const TSharedPtr<FJsonValue>& Kid :
			RootKids[0]->AsObject()->GetArrayField(TEXT("children")))
		{
			Guids.Add(Kid->AsObject()->GetStringField(TEXT("guid")));
		}
		return Guids;
	};

	TArray<FString> Order = SelectorChildGuids();
	TestEqual(TEXT("selector has two children"), Order.Num(), 2);
	const FString GuidA = Order.IsValidIndex(0) ? Order[0] : FString();
	const FString GuidB = Order.IsValidIndex(1) ? Order[1] : FString();
	TestNotEqual(TEXT("two distinct children"), GuidA, GuidB);

	// "properties" is the node's *edited* state, so a node nobody has edited reports none — and in
	// particular a composite's own Children array, which certainly differs from the class default
	// by now, is structure rather than a property and belongs in "children" alone.
	{
		const TSharedPtr<FJsonObject> Tree = ParseTree(UBehaviorTreeService::GetTree(BTPath));
		const TSharedPtr<FJsonObject> SelObject = Tree.IsValid()
			&& Tree->GetArrayField(TEXT("children")).Num() == 1
			? Tree->GetArrayField(TEXT("children"))[0]->AsObject()
			: nullptr;
		if (TestTrue(TEXT("the selector is in the dump"), SelObject.IsValid()))
		{
			TestTrue(TEXT("nodes carry a properties object"), SelObject->HasField(TEXT("properties")));
			const TSharedPtr<FJsonObject>& Props = SelObject->GetObjectField(TEXT("properties"));
			TestFalse(TEXT("structure is not reported as a property"),
				Props->HasField(TEXT("Children")));
			TestEqual(TEXT("an unedited node has no edited properties"), Props->Values.Num(), 0);
		}
	}

	// Insert at index 0: it must become the FIRST child, not merely "a" child.
	const FString First = UBehaviorTreeService::AddNode(
		BTPath, Sel, TEXT("BTComposite_Sequence"), 0);
	TestFalse(TEXT("insert at 0 ok"), First.StartsWith(TEXT("ERROR")));

	Order = SelectorChildGuids();
	TestEqual(TEXT("three children"), Order.Num(), 3);
	const FString GuidFirst = Order.IsValidIndex(0) ? Order[0] : FString();
	TestNotEqual(TEXT("new node is first"), GuidFirst, GuidA);
	TestEqual(TEXT("A shifted to index 1"), Order.IsValidIndex(1) ? Order[1] : FString(), GuidA);
	TestEqual(TEXT("B shifted to index 2"), Order.IsValidIndex(2) ? Order[2] : FString(), GuidB);

	// Move that node to the end: order must become A, B, First.
	TestEqual(TEXT("move ok"),
		UBehaviorTreeService::MoveNode(BTPath, First, Sel, 2), FString());
	Order = SelectorChildGuids();
	TestEqual(TEXT("still three children"), Order.Num(), 3);
	TestEqual(TEXT("A now first"),  Order.IsValidIndex(0) ? Order[0] : FString(), GuidA);
	TestEqual(TEXT("B now second"), Order.IsValidIndex(1) ? Order[1] : FString(), GuidB);
	TestEqual(TEXT("moved node last"), Order.IsValidIndex(2) ? Order[2] : FString(), GuidFirst);

	// A move must not be able to detach a subtree from the root.
	//
	// The message, not merely "an error". With MoveNode's descendant guard deleted this call still
	// fails, from the schema rather than from the guard: MoveNode unlinks Sel from the root and then
	// asks CanCreateConnection(SeqA's output, Sel's input), whose FNodeVisitorCycleChecker walks
	// *up* from SeqA — SeqA's input pin is still linked to Sel's output pin, unlinking Sel from the
	// root did not touch that — reaches the node it was asked about and answers DISALLOW,
	// "Can't create a graph cycle" (EdGraphSchema_BehaviorTree.cpp:279-336). MoveNode then restores
	// the old link exactly. So the guard's entire contribution is the diagnosis and the absence of
	// that unlink/relink window, and only the message can tell the two apart.
	TestTrue(TEXT("no reparenting under a descendant"),
		UBehaviorTreeService::MoveNode(BTPath, Sel, SeqA, -1)
			.Contains(TEXT("one of its own descendants")));

	// True down either path above, and asserted anyway: a restore that is not exact is the shape the
	// SimpleParallel bug was made of, and this is where it would show.
	TestEqual(TEXT("the refused move changed nothing"), NodeCount(), 6);
	Order = SelectorChildGuids();
	TestEqual(TEXT("the refused move left the tree reachable"), Order.Num(), 3);
	TestEqual(TEXT("the refused move left child order alone"),
		Order.IsValidIndex(0) ? Order[0] : FString(), GuidA);

	// Likewise this one has to look at the message: with the explicit root check deleted, the root
	// has no input pin, so the "has no parent" branch errors anyway and a bare non-empty assertion
	// would stay green.
	TestTrue(TEXT("the root cannot be moved"),
		UBehaviorTreeService::MoveNode(BTPath, TEXT("Root"), Sel, -1)
			.Contains(TEXT("root node cannot be moved")));

	// GetNodeInfo agrees with the dump.
	FBTNodeInfo SelInfo;
	TestTrue(TEXT("node info readable"), UBehaviorTreeService::GetNodeInfo(BTPath, Sel, SelInfo));
	TestEqual(TEXT("selector child count"), SelInfo.ChildCount, 3);
	TestEqual(TEXT("selector class"), SelInfo.ClassName, FString(TEXT("BTComposite_Selector")));
	TestFalse(TEXT("selector not injected"), SelInfo.bInjected);

	// A path that resolves to nothing is an error, not a default-constructed success.
	FBTNodeInfo Missing;
	TestFalse(TEXT("bad path rejected"),
		UBehaviorTreeService::GetNodeInfo(BTPath, TEXT("Root/Nope"), Missing));
	TestTrue(TEXT("bad path explains itself"), !Missing.Error.IsEmpty());

	// An unindexed segment naming three same-named siblings is ambiguous, and must be refused
	// rather than resolved to whichever one happens to come first.
	FBTNodeInfo Ambiguous;
	TestFalse(TEXT("ambiguous path rejected"),
		UBehaviorTreeService::GetNodeInfo(BTPath, TEXT("Root/Selector/Sequence"), Ambiguous));
	TestTrue(TEXT("an index disambiguates it"),
		UBehaviorTreeService::GetNodeInfo(BTPath, TEXT("Root/Selector/Sequence[1]"), SelInfo));

	TestEqual(TEXT("five nodes plus the root"), NodeCount(), 6);

	// Remove and confirm it is gone.
	TestEqual(TEXT("remove ok"), UBehaviorTreeService::RemoveNode(BTPath, Task), FString());
	TestTrue(TEXT("removing twice errors"),
		!UBehaviorTreeService::RemoveNode(BTPath, Task).IsEmpty());
	TestEqual(TEXT("the removed node is really gone"), NodeCount(), 5);

	// The root can never be removed. Message-based for the same reason as the move above: without
	// the explicit check the root simply has no parent, so it would error regardless.
	TestTrue(TEXT("root not removable"),
		UBehaviorTreeService::RemoveNode(BTPath, TEXT("Root"))
			.Contains(TEXT("root node cannot be removed")));

	// Nor can the tree be emptied through RemoveNode: the root takes exactly one child, so removing
	// it would leave no runtime tree at all. Refused before anything is mutated.
	TestTrue(TEXT("the root's only child is not removable"),
		!UBehaviorTreeService::RemoveNode(BTPath, Sel).IsEmpty());
	TestEqual(TEXT("the refused removal changed nothing"), NodeCount(), 5);

	// Removing a node takes its subtree with it, rather than leaving children unreachable from
	// "Root" and therefore un-addressable forever.
	const FString SubTask = UBehaviorTreeService::AddNode(
		BTPath, TEXT("Root/Selector/Sequence[0]"), TEXT("BTTask_Wait"), -1);
	TestFalse(TEXT("subtree task added"), SubTask.StartsWith(TEXT("ERROR")));
	TestEqual(TEXT("six nodes"), NodeCount(), 6);
	TestEqual(TEXT("remove the parent"),
		UBehaviorTreeService::RemoveNode(BTPath, TEXT("Root/Selector/Sequence[0]")), FString());
	TestEqual(TEXT("parent and child both went"), NodeCount(), 4);

	// And the asset that has been through all of that is still a valid, committable tree.
	TestEqual(TEXT("the tree still commits"),
		UBehaviorTreeService::CompileAndSave(BTPath), FString());

	return true;
}

/** Every node in the runtime tree, so the same object appearing twice can be detected. */
static void CollectRuntimeNodes(UBTCompositeNode* Composite, TArray<UBTNode*>& Out, int32 Depth = 0)
{
	// Depth cap rather than a visited set: a duplicated child is the thing being looked for, so
	// deduplicating the walk would hide it. Depth is what keeps a malformed tree finite.
	if (!Composite || Depth > 32)
	{
		return;
	}
	Out.Add(Composite);
	for (const FBTCompositeChild& Child : Composite->Children)
	{
		if (UBTTaskNode* Task = ToRawPtr(Child.ChildTask))
		{
			Out.Add(Task);
		}
		CollectRuntimeNodes(ToRawPtr(Child.ChildComposite), Out, Depth + 1);
	}
}

// SimpleParallel is the only BT graph node with two output pins — [0] "Task"
// (PinCategory_SingleTask) and [1] "Out" (PinCategory_SingleNode), BehaviorTreeGraphNode_SimpleParallel.cpp:24-30.
// Everything that reaches for "the" output pin quietly means the main-task pin on one of these,
// which is how a background child gets unlinked from a pin it was never on.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTSimpleParallelTest,
	"VibeUE.BehaviorTree.Structure.SimpleParallel", kBTTestFlags)
bool FVibeBTSimpleParallelTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_ParallelTest");
	FScopedFixtureReset ResetBT(BTPath);

	TestEqual(TEXT("fixture tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, FString()), FString());

	const FString Sel = UBehaviorTreeService::AddNode(
		BTPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	const FString Par = UBehaviorTreeService::AddNode(
		BTPath, Sel, TEXT("BTComposite_SimpleParallel"), -1);
	const FString Seq = UBehaviorTreeService::AddNode(BTPath, Sel, TEXT("BTComposite_Sequence"), -1);
	TestFalse(TEXT("parallel added"), Par.StartsWith(TEXT("ERROR")));
	TestFalse(TEXT("sibling sequence added"), Seq.StartsWith(TEXT("ERROR")));

	// Class names of the parallel's children, in the order GetTree reports them, joined so a
	// mismatch prints both sequences rather than just a count.
	auto ParallelChildren = [&]() -> FString
	{
		TArray<FString> Names;
		const TSharedPtr<FJsonObject> Tree = ParseTree(UBehaviorTreeService::GetTree(BTPath));
		if (!Tree.IsValid() || Tree->GetArrayField(TEXT("children")).Num() != 1)
		{
			return TEXT("<unreadable>");
		}

		const TSharedPtr<FJsonObject> Selector = Tree->GetArrayField(TEXT("children"))[0]->AsObject();
		for (const TSharedPtr<FJsonValue>& Kid : Selector->GetArrayField(TEXT("children")))
		{
			const TSharedPtr<FJsonObject> KidObject = Kid->AsObject();
			if (KidObject->GetStringField(TEXT("class")) != TEXT("BTComposite_SimpleParallel"))
			{
				continue;
			}
			for (const TSharedPtr<FJsonValue>& GrandKid : KidObject->GetArrayField(TEXT("children")))
			{
				Names.Add(GrandKid->AsObject()->GetStringField(TEXT("class")));
			}
		}
		return FString::Join(Names, TEXT(","));
	};

	// The engine fills an empty background slot with a Wait on every save
	// (SpawnMissingNodesForParallel), so a freshly created parallel already has exactly one child.
	TestEqual(TEXT("the engine supplied a background branch"), ParallelChildren(),
		FString(TEXT("BTTask_Wait")));

	// Slot 0 is the main task. It must land BEFORE the background branch, because that is the order
	// GetTree reports and the order CreateChildren writes into UBTCompositeNode::Children.
	const FString MainTask = UBehaviorTreeService::AddNode(BTPath, Par, TEXT("BTTask_MoveTo"), 0);
	TestFalse(TEXT("main task added"), MainTask.StartsWith(TEXT("ERROR")));
	TestEqual(TEXT("main task comes first"), ParallelChildren(),
		FString(TEXT("BTTask_MoveTo,BTTask_Wait")));

	FBTNodeInfo ParInfo;
	TestTrue(TEXT("parallel readable"), UBehaviorTreeService::GetNodeInfo(BTPath, Par, ParInfo));
	TestEqual(TEXT("GetNodeInfo agrees on the child count"), ParInfo.ChildCount, 2);

	// Exactly two slots, and both are full.
	TestTrue(TEXT("no third child"),
		UBehaviorTreeService::AddNode(BTPath, Par, TEXT("BTTask_Wait"), -1).StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("child index 2 is out of range"),
		UBehaviorTreeService::AddNode(BTPath, Par, TEXT("BTTask_Wait"), 2).StartsWith(TEXT("ERROR")));

	// Slot 0 takes tasks only, and a refused replace must not destroy the occupant.
	TestTrue(TEXT("no composite in the main task slot"),
		UBehaviorTreeService::AddNode(BTPath, Par, TEXT("BTComposite_Sequence"), 0)
			.StartsWith(TEXT("ERROR")));
	TestEqual(TEXT("the refused replace left the main task alone"), ParallelChildren(),
		FString(TEXT("BTTask_MoveTo,BTTask_Wait")));

	// Replacing slot 1 is the only way to author a background branch.
	const FString Background = UBehaviorTreeService::AddNode(
		BTPath, Par, TEXT("BTComposite_Sequence"), 1);
	TestFalse(TEXT("background branch authored"), Background.StartsWith(TEXT("ERROR")));
	TestEqual(TEXT("background branch replaced the Wait"), ParallelChildren(),
		FString(TEXT("BTTask_MoveTo,BTComposite_Sequence")));

	// The background branch is regenerated by the engine, so removing it would be a lie.
	TestTrue(TEXT("the background branch is not removable"),
		UBehaviorTreeService::RemoveNode(BTPath, Background).Contains(TEXT("background branch")));

	// Moving the background branch away is refused for the same reason removing it is: the
	// emptied pin would be refilled with an unrequested Wait inside the very commit the move
	// saves through. (This refusal also proves the link is read off pin 1 — the pin the child is
	// actually on — because a pin-0 read would not have identified it as the background branch.)
	const FString BackgroundMove = UBehaviorTreeService::MoveNode(BTPath, Background, Seq, -1);
	TestTrue(TEXT("moving the background branch out is refused"),
		BackgroundMove.Contains(TEXT("background")));
	TestEqual(TEXT("the refused move left both slots as they were"), ParallelChildren(),
		FString(TEXT("BTTask_MoveTo,BTComposite_Sequence")));

	// THE REGRESSION, now exercised through the main-task slot (the movable one): unlinking must
	// happen on the pin the child is actually linked through. A pin read off the parent's pin
	// list would still be pin 0 here — correct by luck — but the two-parent-graph shape this
	// guarded against is asserted below over the runtime tree either way.
	TestEqual(TEXT("main task moved out"),
		UBehaviorTreeService::MoveNode(BTPath, MainTask, Seq, -1), FString());
	TestEqual(TEXT("the emptied main slot stays empty (only the background pin is refilled)"),
		ParallelChildren(), FString(TEXT("BTComposite_Sequence")));
	FBTNodeInfo SeqInfo;
	TestTrue(TEXT("sibling readable"), UBehaviorTreeService::GetNodeInfo(BTPath, Seq, SeqInfo));
	TestEqual(TEXT("the moved node landed on the sibling"), SeqInfo.ChildCount, 1);

	// One more successful commit, which is what would write a two-parent graph to disk, and then
	// the runtime tree itself: no UBTNode may appear twice in UBTCompositeNode::Children.
	TestFalse(TEXT("a later add still works"),
		UBehaviorTreeService::AddNode(BTPath, Seq, TEXT("BTTask_Wait"), -1).StartsWith(TEXT("ERROR")));

	UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
	if (TestNotNull(TEXT("tree loaded"), Tree))
	{
		TArray<UBTNode*> RuntimeNodes;
		CollectRuntimeNodes(ToRawPtr(Tree->RootNode), RuntimeNodes);
		TestTrue(TEXT("the runtime tree is populated"), RuntimeNodes.Num() > 4);
		TestEqual(TEXT("no node appears twice in the runtime tree"),
			TSet<UBTNode*>(RuntimeNodes).Num(), RuntimeNodes.Num());
	}

	return true;
}

/**
 * The node object GetTree emitted for Path, wherever it sits — under "children", "decorators" or
 * "services". Searching by the node's own "path" field rather than walking a known route is what
 * lets the same helper address a sub-node and a tree node.
 */
static TSharedPtr<FJsonObject> FindNodeByPath(const TSharedPtr<FJsonObject>& Node, const FString& Path)
{
	if (!Node.IsValid())
	{
		return nullptr;
	}

	FString NodePath;
	if (Node->TryGetStringField(TEXT("path"), NodePath) && NodePath == Path)
	{
		return Node;
	}

	for (const TCHAR* Field : { TEXT("decorators"), TEXT("services"), TEXT("children") })
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!Node->TryGetArrayField(Field, Array))
		{
			continue;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			if (TSharedPtr<FJsonObject> Found = FindNodeByPath(Value->AsObject(), Path))
			{
				return Found;
			}
		}
	}
	return nullptr;
}

// Decorator and service CRUD, plus SetNodeName.
//
// The hazard behind all of this is UAIGraphNode::AddSubNode, which ends in NotifyGraphChanged() +
// GetAIGraph()->UpdateAsset() (AIGraphNode.cpp:297-298) — a synchronous rebuild of the runtime tree
// from a graph that is mid-edit, outside CommitGraph's discard guard. Nothing here calls it; see the
// header of UBehaviorTreeService_Edit.cpp. The end-of-test runtime-tree assertions are what prove the
// data-level attach is nevertheless complete: a sub-node missing from UAIGraphNode::SubNodes has its
// instance destroyed as an orphan by the very commit that was supposed to save it, and one missing
// from Decorators/Services never reaches UBTCompositeNode/UBTTaskNode at all.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTSubNodesTest,
	"VibeUE.BehaviorTree.SubNodes.Crud", kBTTestFlags)
bool FVibeBTSubNodesTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_SubNodeTest");
	FScopedFixtureReset ResetBT(BTPath);

	// No blackboard on purpose. UBTDecorator_BlackboardBase::InitializeFromAsset invalidates its key
	// selector silently when the tree has no board, where resolving against one that lacks a matching
	// key logs a LogBehaviorTree warning (BehaviorTreeTypes.cpp:556) and fails the run.
	TestEqual(TEXT("fixture tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, FString()), FString());

	// GetBehaviorTreeInfo counts sub-nodes as well as graph nodes. Until this test there were never
	// any sub-nodes to count, so that arm of its arithmetic has never been exercised.
	auto NodeCount = [&]() -> int32
	{
		FBTAssetInfo Info;
		return UBehaviorTreeService::GetBehaviorTreeInfo(BTPath, Info) ? Info.NodeCount : -1;
	};
	auto NodeObject = [&](const FString& Path)
	{
		return FindNodeByPath(ParseTree(UBehaviorTreeService::GetTree(BTPath)), Path);
	};
	// Class names of one node's decorators or services, in order, joined so a mismatch prints both
	// sequences instead of a count.
	auto ClassList = [&](const FString& OwnerPath, const TCHAR* Field) -> FString
	{
		const TSharedPtr<FJsonObject> Owner = NodeObject(OwnerPath);
		if (!Owner.IsValid())
		{
			return TEXT("<no such node>");
		}
		TArray<FString> Names;
		for (const TSharedPtr<FJsonValue>& Value : Owner->GetArrayField(Field))
		{
			Names.Add(Value->AsObject()->GetStringField(TEXT("class")));
		}
		return FString::Join(Names, TEXT(","));
	};

	const FString Sel = UBehaviorTreeService::AddNode(
		BTPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	const FString Seq = UBehaviorTreeService::AddNode(BTPath, Sel, TEXT("BTComposite_Sequence"), -1);
	const FString Task = UBehaviorTreeService::AddNode(BTPath, Seq, TEXT("BTTask_Wait"), -1);
	TestFalse(TEXT("selector added"), Sel.StartsWith(TEXT("ERROR")));
	TestFalse(TEXT("sequence added"), Seq.StartsWith(TEXT("ERROR")));
	TestFalse(TEXT("task added"), Task.StartsWith(TEXT("ERROR")));
	TestEqual(TEXT("three nodes plus the root"), NodeCount(), 4);

	// --- A decorator on the sequence. ------------------------------------------------------------
	const FString Dec = UBehaviorTreeService::AddDecorator(
		BTPath, Seq, TEXT("BTDecorator_Blackboard"), -1);
	TestFalse(TEXT("decorator added"), Dec.StartsWith(TEXT("ERROR")));
	TestEqual(TEXT("its path is an @decorator slot on its owner"), Dec, Seq + TEXT("/@decorator[0]"));
	TestEqual(TEXT("GetTree reports it under decorators[0]"),
		ClassList(Seq, TEXT("decorators")), FString(TEXT("BTDecorator_Blackboard")));
	TestEqual(TEXT("GetBehaviorTreeInfo counts the sub-node"), NodeCount(), 5);

	// VibeBT::ResolveNodePath has always implemented "@decorator[n]"/"@service[n]"; nothing had ever
	// created a sub-node for it to resolve, so this is the first time those branches run.
	FBTNodeInfo DecInfo;
	TestTrue(TEXT("the @decorator[n] path resolves"),
		UBehaviorTreeService::GetNodeInfo(BTPath, Dec, DecInfo));
	TestEqual(TEXT("...to the decorator itself"), DecInfo.ClassName,
		FString(TEXT("BTDecorator_Blackboard")));
	TestEqual(TEXT("...and reports the path it was addressed by"), DecInfo.Path, Dec);
	FBTNodeInfo SeqInfo;
	TestTrue(TEXT("owner readable"), UBehaviorTreeService::GetNodeInfo(BTPath, Seq, SeqInfo));
	TestEqual(TEXT("the owner reports one decorator"), SeqInfo.DecoratorCount, 1);
	TestEqual(TEXT("...and no services"), SeqInfo.ServiceCount, 0);

	// Sub-node JSON shape: the same ten fields a tree node carries, so one walk handles both.
	{
		const TSharedPtr<FJsonObject> DecObject = NodeObject(Dec);
		if (TestTrue(TEXT("the decorator appears in the dump"), DecObject.IsValid()))
		{
			for (const TCHAR* Field : { TEXT("path"), TEXT("guid"), TEXT("class"), TEXT("name"),
				TEXT("properties"), TEXT("bInjected"), TEXT("children"), TEXT("decorators"),
				TEXT("services"), TEXT("bHasCompositeDecorator") })
			{
				TestTrue(FString::Printf(TEXT("a sub-node carries '%s'"), Field),
					DecObject->HasField(Field));
			}
			TestEqual(TEXT("a sub-node has no children"),
				DecObject->GetArrayField(TEXT("children")).Num(), 0);
			TestEqual(TEXT("a sub-node has no decorators of its own"),
				DecObject->GetArrayField(TEXT("decorators")).Num(), 0);
			TestEqual(TEXT("a sub-node has no services of its own"),
				DecObject->GetArrayField(TEXT("services")).Num(), 0);
			TestFalse(TEXT("an authored sub-node is not injected"),
				DecObject->GetBoolField(TEXT("bInjected")));
			TestFalse(TEXT("and carries no composite decorator"),
				DecObject->GetBoolField(TEXT("bHasCompositeDecorator")));
		}
	}

	// --- A service on the selector. ----------------------------------------------------------------
	const FString Svc = UBehaviorTreeService::AddService(
		BTPath, Sel, TEXT("BTService_DefaultFocus"), -1);
	TestFalse(TEXT("service added"), Svc.StartsWith(TEXT("ERROR")));
	TestEqual(TEXT("its path is an @service slot"), Svc, Sel + TEXT("/@service[0]"));
	TestEqual(TEXT("GetTree reports it under services[0]"),
		ClassList(Sel, TEXT("services")), FString(TEXT("BTService_DefaultFocus")));
	TestEqual(TEXT("the service is counted too"), NodeCount(), 6);

	// --- Index 0 means first, not "somewhere". Decorator order is evaluation order. ---------------
	const FString DecFirst = UBehaviorTreeService::AddDecorator(
		BTPath, Seq, TEXT("BTDecorator_ForceSuccess"), 0);
	TestEqual(TEXT("the inserted decorator takes slot 0"), DecFirst, Seq + TEXT("/@decorator[0]"));
	TestEqual(TEXT("and the existing one shifted to slot 1"), ClassList(Seq, TEXT("decorators")),
		FString(TEXT("BTDecorator_ForceSuccess,BTDecorator_Blackboard")));

	// --- Removal, by the same "@decorator[n]" path. ------------------------------------------------
	TestEqual(TEXT("the first decorator is removed"),
		UBehaviorTreeService::RemoveSubNode(BTPath, Seq + TEXT("/@decorator[0]")), FString());
	TestEqual(TEXT("the survivor slid back to slot 0"), ClassList(Seq, TEXT("decorators")),
		FString(TEXT("BTDecorator_Blackboard")));
	TestEqual(TEXT("the count dropped"), NodeCount(), 6);
	TestTrue(TEXT("removing a slot that is now empty errors"),
		!UBehaviorTreeService::RemoveSubNode(BTPath, Seq + TEXT("/@decorator[1]")).IsEmpty());

	// The same round trip through the OTHER array. Decorators and services are two separate arrays on
	// the owner and one wrong pick would put a service into the decorator list — where it would then
	// be handed to CollectDecorators as a UBTDecorator and silently dropped.
	{
		const FString TempSvc = UBehaviorTreeService::AddService(
			BTPath, Seq, TEXT("BTService_DefaultFocus"), -1);
		TestEqual(TEXT("a service lands in the service array, not the decorator one"),
			TempSvc, Seq + TEXT("/@service[0]"));
		TestEqual(TEXT("and shows up under services"),
			ClassList(Seq, TEXT("services")), FString(TEXT("BTService_DefaultFocus")));
		TestEqual(TEXT("the decorator list is untouched by it"),
			ClassList(Seq, TEXT("decorators")), FString(TEXT("BTDecorator_Blackboard")));
		TestEqual(TEXT("and it removes again"),
			UBehaviorTreeService::RemoveSubNode(BTPath, TempSvc), FString());
		TestEqual(TEXT("leaving no services behind"), ClassList(Seq, TEXT("services")), FString());
		TestEqual(TEXT("and the decorator still there"),
			ClassList(Seq, TEXT("decorators")), FString(TEXT("BTDecorator_Blackboard")));
	}

	// --- A service on a TASK node. -----------------------------------------------------------------
	// The brief expected this to be refused ("only composites carry services"). It is not true of
	// UE 5.8: CreateBTFromGraph writes a task graph node's services to UBTTaskNode::Services
	// (BehaviorTreeGraph.cpp:605-627), the Behavior Tree editor offers "Add Service" on a task
	// (BehaviorTreeGraphNode_Task.cpp:52), and DoesSupportServices is a property of the *graph*, not
	// of the node. Refusing it would block legal authoring, so it is accepted — and the runtime-tree
	// assertion at the end of this test is what proves the acceptance is not merely cosmetic.
	const FString TaskSvc = UBehaviorTreeService::AddService(
		BTPath, Task, TEXT("BTService_DefaultFocus"), -1);
	TestFalse(TEXT("a task accepts a service in UE 5.8"), TaskSvc.StartsWith(TEXT("ERROR")));
	TestEqual(TEXT("its path is an @service slot on the task"), TaskSvc, Task + TEXT("/@service[0]"));

	// --- Refusals. Each checks the message, because "it errored" would also hold if the call had
	//     failed for an unrelated reason. -----------------------------------------------------------
	// The graph's Root node: CreateBTFromGraph reads root-level decorators off the top composite, so
	// one attached to the Root graph node would be saved and never run.
	TestTrue(TEXT("no decorator on the graph Root node"),
		UBehaviorTreeService::AddDecorator(BTPath, TEXT("Root"), TEXT("BTDecorator_ForceSuccess"), -1)
			.Contains(TEXT("Root node cannot carry")));
	TestTrue(TEXT("no service on the graph Root node"),
		UBehaviorTreeService::AddService(BTPath, TEXT("Root"), TEXT("BTService_DefaultFocus"), -1)
			.Contains(TEXT("Root node cannot carry")));
	// A decorator carries no sub-nodes of its own.
	TestTrue(TEXT("no decorator on a decorator"),
		UBehaviorTreeService::AddDecorator(BTPath, Dec, TEXT("BTDecorator_ForceSuccess"), -1)
			.Contains(TEXT("carry no sub-nodes")));
	TestTrue(TEXT("no service on a decorator"),
		UBehaviorTreeService::AddService(BTPath, Dec, TEXT("BTService_DefaultFocus"), -1)
			.Contains(TEXT("carry no sub-nodes")));
	// Class resolution is scoped to the right base, so a task class is not a decorator.
	TestTrue(TEXT("a task class is not a decorator"),
		UBehaviorTreeService::AddDecorator(BTPath, Seq, TEXT("BTTask_Wait"), -1)
			.Contains(TEXT("unknown or ambiguous")));
	TestTrue(TEXT("a decorator class is not a service"),
		UBehaviorTreeService::AddService(BTPath, Seq, TEXT("BTDecorator_Blackboard"), -1)
			.Contains(TEXT("unknown or ambiguous")));
	TestTrue(TEXT("an invented class is refused"),
		UBehaviorTreeService::AddDecorator(BTPath, Seq, TEXT("BTDecorator_Nonexistent"), -1)
			.Contains(TEXT("unknown or ambiguous")));
	// RemoveSubNode is not RemoveNode.
	TestTrue(TEXT("RemoveSubNode refuses a tree node"),
		UBehaviorTreeService::RemoveSubNode(BTPath, Seq).Contains(TEXT("Use RemoveNode")));
	TestTrue(TEXT("RemoveSubNode refuses a path that names nothing"),
		!UBehaviorTreeService::RemoveSubNode(BTPath, Seq + TEXT("/@decorator[7]")).IsEmpty());
	// Four tree nodes, one decorator, two services — every refusal above left the asset alone.
	TestEqual(TEXT("the refusals changed nothing"), NodeCount(), 7);

	// --- SetNodeName. ------------------------------------------------------------------------------
	TestEqual(TEXT("the sequence is renamed"),
		UBehaviorTreeService::SetNodeName(BTPath, Seq, TEXT("GuardedBranch")), FString());
	const FString Renamed = Sel + TEXT("/GuardedBranch[0]");
	{
		const TSharedPtr<FJsonObject> RenamedObject = NodeObject(Renamed);
		if (TestTrue(TEXT("the node is addressable at its new path"), RenamedObject.IsValid()))
		{
			TestEqual(TEXT("GetTree reports the new name"),
				RenamedObject->GetStringField(TEXT("name")), FString(TEXT("GuardedBranch")));
			TestEqual(TEXT("the class is unchanged"),
				RenamedObject->GetStringField(TEXT("class")), FString(TEXT("BTComposite_Sequence")));
		}
	}
	// The name IS the path segment, so the old path is dead. Asserted rather than left implicit,
	// because a caller holding a path across a rename is the obvious way to lose an edit.
	FBTNodeInfo Stale;
	TestFalse(TEXT("the old path no longer resolves"),
		UBehaviorTreeService::GetNodeInfo(BTPath, Seq, Stale));

	// A sub-node can be renamed too, and its path does not move: sub-node paths are positional.
	const FString RenamedDec = Renamed + TEXT("/@decorator[0]");
	TestEqual(TEXT("the decorator is renamed"),
		UBehaviorTreeService::SetNodeName(BTPath, RenamedDec, TEXT("HasTarget")), FString());
	{
		const TSharedPtr<FJsonObject> DecObject = NodeObject(RenamedDec);
		if (TestTrue(TEXT("the decorator is still at the same slot"), DecObject.IsValid()))
		{
			TestEqual(TEXT("GetTree reports the decorator's new name"),
				DecObject->GetStringField(TEXT("name")), FString(TEXT("HasTarget")));
		}
	}

	TestTrue(TEXT("the graph Root node has no name to set"),
		UBehaviorTreeService::SetNodeName(BTPath, TEXT("Root"), TEXT("Anything"))
			.Contains(TEXT("no Behavior Tree node instance")));
	TestTrue(TEXT("a name containing the path separator is refused"),
		UBehaviorTreeService::SetNodeName(BTPath, Renamed, TEXT("a/b"))
			.Contains(TEXT("path separator")));
	TestTrue(TEXT("a name ending in a numeric index is refused"),
		UBehaviorTreeService::SetNodeName(BTPath, Renamed, TEXT("Guard[2]"))
			.Contains(TEXT("sibling index")));
	// ResolveNodePath dispatches on a leading '@' before comparing names, so "@foo" resolves to
	// nothing at all — the same un-addressable node the other two guards exist to prevent.
	TestTrue(TEXT("a name starting with '@' is refused"),
		UBehaviorTreeService::SetNodeName(BTPath, Renamed, TEXT("@foo"))
			.Contains(TEXT("reserves that prefix")));
	{
		const TSharedPtr<FJsonObject> RenamedObject = NodeObject(Renamed);
		TestTrue(TEXT("the refused renames left the name alone"), RenamedObject.IsValid());
	}

	// Clearing it falls back to the class-supplied title, which is what the path grammar uses.
	TestEqual(TEXT("the name is cleared"),
		UBehaviorTreeService::SetNodeName(BTPath, Renamed, FString()), FString());
	TestTrue(TEXT("the class-supplied title is back"),
		NodeObject(Sel + TEXT("/Sequence[0]")).IsValid());

	// --- The runtime tree, which is the only thing that actually runs. -----------------------------
	TestEqual(TEXT("the tree still commits"), UBehaviorTreeService::CompileAndSave(BTPath), FString());

	UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
	if (TestNotNull(TEXT("tree loaded"), Tree) &&
		TestNotNull(TEXT("the runtime tree has a root"), ToRawPtr(Tree->RootNode)))
	{
		// The Selector is the top composite, so its service is the root composite's.
		TestEqual(TEXT("the composite's service reached the runtime tree"),
			Tree->RootNode->Services.Num(), 1);

		if (TestEqual(TEXT("the top composite has one child"), Tree->RootNode->Children.Num(), 1))
		{
			const FBTCompositeChild& Child = Tree->RootNode->Children[0];
			TestEqual(TEXT("the decorator reached the runtime tree"), Child.Decorators.Num(), 1);

			UBTCompositeNode* Sequence = ToRawPtr(Child.ChildComposite);
			if (TestNotNull(TEXT("that child is the sequence"), Sequence) &&
				TestEqual(TEXT("which has one child of its own"), Sequence->Children.Num(), 1))
			{
				UBTTaskNode* WaitTask = ToRawPtr(Sequence->Children[0].ChildTask);
				if (TestNotNull(TEXT("the sequence's child is the task"), WaitTask))
				{
					// The other half of the UE-5.8 correction above: not merely accepted, but written
					// into UBTTaskNode::Services by the commit.
					TestEqual(TEXT("the TASK's service reached the runtime tree"),
						WaitTask->Services.Num(), 1);
				}
			}
		}
	}

	return true;
}

// Injected nodes: the root-level decorators of a subtree, copied onto the BTTask_RunBehavior graph
// node that references it (UBehaviorTreeGraphNode_SubtreeTask::UpdateInjectedNodes). They are copies
// the engine regenerates, so every mutator has to refuse them — an edit that reports success and is
// silently undone on the next graph update is worse than an error.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTInjectedSubNodesTest,
	"VibeUE.BehaviorTree.SubNodes.Injected", kBTTestFlags)
bool FVibeBTInjectedSubNodesTest::RunTest(const FString&)
{
	const FString SubPath = FString(kBTTestDir) / TEXT("BT_InjectedSub");
	const FString MainPath = FString(kBTTestDir) / TEXT("BT_InjectedMain");
	FScopedFixtureReset ResetSub(SubPath);
	FScopedFixtureReset ResetMain(MainPath);

	// The subtree: a decorator on its TOP COMPOSITE, which is what becomes RootDecorators.
	TestEqual(TEXT("subtree created"),
		UBehaviorTreeService::CreateBehaviorTree(SubPath, FString()), FString());
	const FString SubSel = UBehaviorTreeService::AddNode(
		SubPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	TestFalse(TEXT("subtree selector added"), SubSel.StartsWith(TEXT("ERROR")));
	TestFalse(TEXT("subtree decorator added"),
		UBehaviorTreeService::AddDecorator(SubPath, SubSel, TEXT("BTDecorator_ForceSuccess"), -1)
			.StartsWith(TEXT("ERROR")));

	UBehaviorTree* SubTree = LoadObject<UBehaviorTree>(nullptr, *SubPath);
	if (!TestNotNull(TEXT("subtree loaded"), SubTree))
	{
		return false;
	}
	// The point of attaching to the top composite rather than to the Root graph node: this is where
	// UBehaviorTree::RootDecorators comes from, and it is the only thing injection reads.
	TestEqual(TEXT("the decorator became a root-level decorator of the subtree"),
		SubTree->RootDecorators.Num(), 1);

	// The main tree: a RunBehavior task pointing at it.
	TestEqual(TEXT("main tree created"),
		UBehaviorTreeService::CreateBehaviorTree(MainPath, FString()), FString());
	const FString MainSel = UBehaviorTreeService::AddNode(
		MainPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	const FString RunSub = UBehaviorTreeService::AddNode(
		MainPath, MainSel, TEXT("BTTask_RunBehavior"), -1);
	TestFalse(TEXT("RunBehavior task added"), RunSub.StartsWith(TEXT("ERROR")));

	UBehaviorTree* MainTree = LoadObject<UBehaviorTree>(nullptr, *MainPath);
	if (!TestNotNull(TEXT("main tree loaded"), MainTree))
	{
		return false;
	}
	UBehaviorTreeGraph* MainGraph = Cast<UBehaviorTreeGraph>(MainTree->BTGraph);
	if (!TestNotNull(TEXT("main graph present"), MainGraph))
	{
		return false;
	}
	UBehaviorTreeGraphNode* RunNode = VibeBT::ResolveNodePath(MainGraph, RunSub);
	if (!TestNotNull(TEXT("the RunBehavior graph node resolves"), RunNode))
	{
		return false;
	}

	// UBTTask_RunBehavior::BehaviorAsset is protected and writing node properties is Task 8's job, so
	// it is set through reflection here — test scaffolding, not the code under test.
	FObjectProperty* AssetProperty = CastField<FObjectProperty>(
		UBTTask_RunBehavior::StaticClass()->FindPropertyByName(TEXT("BehaviorAsset")));
	if (!TestNotNull(TEXT("BehaviorAsset property reached"), AssetProperty))
	{
		return false;
	}
	AssetProperty->SetObjectPropertyValue(
		AssetProperty->ContainerPtrToValuePtr<void>(ToRawPtr(RunNode->NodeInstance)), SubTree);

	// What UBehaviorTreeGraph::Initialize() runs when a human opens the asset.
	TestTrue(TEXT("the subtree's root decorator was injected"), MainGraph->UpdateInjectedNodes());

	FBTNodeInfo RunInfo;
	TestTrue(TEXT("the RunBehavior node is readable"),
		UBehaviorTreeService::GetNodeInfo(MainPath, RunSub, RunInfo));
	TestEqual(TEXT("it now carries one decorator"), RunInfo.DecoratorCount, 1);

	const FString Injected = RunSub + TEXT("/@decorator[0]");
	FBTNodeInfo InjectedInfo;
	TestTrue(TEXT("the injected decorator is addressable"),
		UBehaviorTreeService::GetNodeInfo(MainPath, Injected, InjectedInfo));
	TestTrue(TEXT("and reported as injected"), InjectedInfo.bInjected);

	// The refusals.
	TestTrue(TEXT("RemoveSubNode refuses an injected decorator"),
		UBehaviorTreeService::RemoveSubNode(MainPath, Injected)
			.Contains(TEXT("injected from a subtree")));
	TestTrue(TEXT("SetNodeName refuses an injected node"),
		UBehaviorTreeService::SetNodeName(MainPath, Injected, TEXT("Renamed"))
			.Contains(TEXT("injected from a subtree")));
	TestTrue(TEXT("AddDecorator refuses to attach to an injected node"),
		UBehaviorTreeService::AddDecorator(MainPath, Injected, TEXT("BTDecorator_ForceSuccess"), -1)
			.Contains(TEXT("injected from a subtree")));
	TestTrue(TEXT("AddService refuses to attach to an injected node"),
		UBehaviorTreeService::AddService(MainPath, Injected, TEXT("BTService_RunEQS"), -1)
			.Contains(TEXT("injected from a subtree")));
	TestTrue(TEXT("SetNodeComment refuses an injected node"),
		UBehaviorTreeService::SetNodeComment(MainPath, Injected, TEXT("annotated"))
			.Contains(TEXT("injected from a subtree")));

	// The property writers (Task 8). Unlike the structural mutators these have no natural barrier —
	// they resolve a path and write — so the guard is explicit, and this is what proves it is there.
	// The value is a real one for the property (ForceSuccess carries NodeName like every BT node), so
	// a missing guard would genuinely write it rather than failing for an unrelated reason.
	{
		const FString BeforeName =
			UBehaviorTreeService::GetNodePropertyValue(MainPath, Injected, TEXT("NodeName"));
		TestFalse(TEXT("the injected node's NodeName is readable"), BeforeName.StartsWith(TEXT("ERROR")));

		const FBTPropertySetResult NameWrite =
			UBehaviorTreeService::SetNodePropertyValue(MainPath, Injected, TEXT("NodeName"), TEXT("Hijacked"));
		TestFalse(TEXT("SetNodePropertyValue refuses an injected node"), NameWrite.bSuccess);
		TestTrue(TEXT("...and says why"), NameWrite.Error.Contains(TEXT("injected from a subtree")));
		// The echo fields are filled before the refusal, so the caller can see what it hit.
		TestEqual(TEXT("...while still reporting what the path resolved to"), NameWrite.ResolvedNodeClass,
			FString(TEXT("BTDecorator_ForceSuccess")));
		TestEqual(TEXT("and the refused write changed nothing"),
			UBehaviorTreeService::GetNodePropertyValue(MainPath, Injected, TEXT("NodeName")), BeforeName);

		const FBTPropertySetResult KeyWrite = UBehaviorTreeService::SetNodeBlackboardKey(
			MainPath, Injected, TEXT("BlackboardKey"), TEXT("Anything"));
		TestFalse(TEXT("SetNodeBlackboardKey refuses an injected node"), KeyWrite.bSuccess);
		TestTrue(TEXT("...and says why, rather than complaining about the property or the board"),
			KeyWrite.Error.Contains(TEXT("injected from a subtree")));
	}

	TestTrue(TEXT("the RunBehavior node is still readable"),
		UBehaviorTreeService::GetNodeInfo(MainPath, RunSub, RunInfo));
	TestEqual(TEXT("the refused edits left the injected decorator in place"),
		RunInfo.DecoratorCount, 1);

	// An authored decorator on the same node lands AHEAD of the injected one, which is the order the
	// engine maintains (OnSubNodeAdded inserts before the first injected entry) and the order that
	// survives the next UpdateInjectedNodes, which strips and re-appends every injected decorator.
	const FString Authored = UBehaviorTreeService::AddDecorator(
		MainPath, RunSub, TEXT("BTDecorator_ForceSuccess"), -1);
	TestEqual(TEXT("an appended decorator is placed ahead of the injected ones"),
		Authored, RunSub + TEXT("/@decorator[0]"));

	FBTNodeInfo AuthoredInfo;
	TestTrue(TEXT("the authored decorator is readable"),
		UBehaviorTreeService::GetNodeInfo(MainPath, Authored, AuthoredInfo));
	TestFalse(TEXT("and is not itself injected"), AuthoredInfo.bInjected);
	TestTrue(TEXT("the injected one moved to slot 1"),
		UBehaviorTreeService::GetNodeInfo(MainPath, RunSub + TEXT("/@decorator[1]"), InjectedInfo));
	TestTrue(TEXT("...and is still the injected one"), InjectedInfo.bInjected);

	// Only the authored decorator reaches the runtime tree: CollectDecorators skips injected entries
	// outright (BehaviorTreeGraph.cpp:337-341), because the subtree supplies them at runtime.
	if (TestEqual(TEXT("the main tree's top composite has one child"),
		MainTree->RootNode ? MainTree->RootNode->Children.Num() : -1, 1))
	{
		TestEqual(TEXT("exactly one decorator reached the runtime tree"),
			MainTree->RootNode->Children[0].Decorators.Num(), 1);
	}

	return true;
}

// Task 7 made GetTree's decorator and service arms recursive (one serialiser for every node object,
// so a sub-node carries the same fields as a tree node). The serialiser they replaced never
// recursed, so those two arms arrived without the Visited test the "children" arm has always had —
// and GetTree's contract is that a malformed graph is reported as a finite tree "instead of hanging
// the caller".
//
// Unreachable through this service's own writes: AttachSubNode always creates a fresh graph node, so
// no sequence of AddDecorator/AddService/RemoveSubNode calls can build a sub-node cycle. It is
// reachable by a hand-edited or corrupted asset, and Task 9's ValidateTree and Task 10's BuildTree
// both walk this path. Built here by direct object manipulation for the same reason
// Layout.BackEdgeCycle and Layout.DuplicateLink are: no API can produce the shape.
//
// Without the guard this test does not report a failure — it recurses until the stack overflows and
// takes the automation host down with it. That is the measured behaviour; see task-7-report.md.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTCyclicSubNodeTest,
	"VibeUE.BehaviorTree.SubNodes.CyclicSubNode", kBTTestFlags)
bool FVibeBTCyclicSubNodeTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_CyclicSubNodeTest");
	FScopedFixtureReset ResetBT(BTPath);

	TestEqual(TEXT("fixture tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, FString()), FString());
	const FString Sel = UBehaviorTreeService::AddNode(
		BTPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	TestFalse(TEXT("selector added"), Sel.StartsWith(TEXT("ERROR")));

	UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
	if (!TestNotNull(TEXT("tree loaded"), Tree))
	{
		return false;
	}
	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
	if (!TestNotNull(TEXT("graph present"), Graph))
	{
		return false;
	}
	UBehaviorTreeGraphNode* Selector = VibeBT::ResolveNodePath(Graph, Sel);
	if (!TestNotNull(TEXT("the selector resolves"), Selector))
	{
		return false;
	}

	// The corruption: the node carries ITSELF as a decorator. Deliberately not committed while it is
	// in place — UpdateAsset would set Selector->ParentNode to Selector and change what path the node
	// reports, which is a different (and equally malformed) shape from the one under test here.
	Selector->Decorators.Add(Selector);
	TestEqual(TEXT("the node now lists itself as a decorator"), Selector->Decorators.Num(), 1);

	// The assertion is that this RETURNS. Reaching the next line at all is the result.
	const FString Json = UBehaviorTreeService::GetTree(BTPath);
	TestFalse(TEXT("GetTree returned instead of recursing forever"), Json.IsEmpty());

	const TSharedPtr<FJsonObject> Parsed = ParseTree(Json);
	if (TestTrue(TEXT("and returned parseable JSON"), Parsed.IsValid()))
	{
		const TSharedPtr<FJsonObject> SelObject = FindNodeByPath(Parsed, Sel);
		if (TestTrue(TEXT("the node is in the dump"), SelObject.IsValid()))
		{
			// Truncated at the revisit, exactly as the children arm truncates a back edge, rather
			// than emitted as an infinitely nested object.
			TestEqual(TEXT("the self-referencing decorator slot is dropped, not recursed into"),
				SelObject->GetArrayField(TEXT("decorators")).Num(), 0);
		}
	}

	// A mutual cycle too: the guard must hold for A->B->A, not just for a self-reference.
	const FString Seq = UBehaviorTreeService::AddNode(
		BTPath, Sel, TEXT("BTComposite_Sequence"), -1);
	TestFalse(TEXT("sequence added"), Seq.StartsWith(TEXT("ERROR")));
	UBehaviorTreeGraphNode* Sequence = VibeBT::ResolveNodePath(Graph, Seq);
	if (TestNotNull(TEXT("the sequence resolves"), Sequence))
	{
		Selector->Decorators.Reset();
		Selector->Decorators.Add(Sequence);
		Sequence->Decorators.Add(Selector);

		const FString MutualJson = UBehaviorTreeService::GetTree(BTPath);
		TestFalse(TEXT("GetTree survives a mutual sub-node cycle"), MutualJson.IsEmpty());
		TestTrue(TEXT("and that result parses too"), ParseTree(MutualJson).IsValid());

		Sequence->Decorators.Reset();
	}

	// Undo the corruption so the commit below, and the fixture reset, see a sane graph.
	Selector->Decorators.Reset();
	TestEqual(TEXT("the tree still commits once the cycle is removed"),
		UBehaviorTreeService::CompileAndSave(BTPath), FString());

	return true;
}

namespace
{
	/** The live node instance at NodePath, read straight off the committed graph. */
	UObject* VibeBTFindLiveInstance(const FString& BTPath, const FString& NodePath)
	{
		UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
		UBehaviorTreeGraph* Graph = Tree ? Cast<UBehaviorTreeGraph>(Tree->BTGraph) : nullptr;
		UBehaviorTreeGraphNode* Node = Graph ? VibeBT::ResolveNodePath(Graph, NodePath) : nullptr;
		return Node ? ToRawPtr(Node->NodeInstance) : nullptr;
	}

	/**
	 * The live FBlackboardKeySelector on a node.
	 *
	 * The resolved key ID is the whole point of SetNodeBlackboardKey and it cannot be seen from the
	 * service's own return values: SelectedKeyID is transient (so it is not in the exported text)
	 * and protected (so only GetSelectedKeyID() can read it). Asserting the selector's *name* would
	 * prove nothing at all — an unresolved selector reads its name back perfectly and does nothing
	 * at runtime, which is the entire reason that method exists.
	 */
	FBlackboardKeySelector* VibeBTFindLiveSelector(const FString& BTPath, const FString& NodePath,
		const TCHAR* PropertyName)
	{
		UObject* Instance = VibeBTFindLiveInstance(BTPath, NodePath);
		FStructProperty* Property = Instance
			? CastField<FStructProperty>(Instance->GetClass()->FindPropertyByName(PropertyName))
			: nullptr;
		return (Property && Property->Struct == FBlackboardKeySelector::StaticStruct())
			? Property->ContainerPtrToValuePtr<FBlackboardKeySelector>(Instance)
			: nullptr;
	}
}

// Node property reflection and blackboard key binding.
//
// Three things are being defended here, and none of them is "the value came back":
//
//  1. The exported value is a COMPLETE literal, never a diff. Anything exported relative to a
//     default drops the members that match it, and a dropped member is not "unchanged" — it is
//     whatever the target already had, or, on a node built from scratch, that class's default. The
//     copy-between-two-Wait-nodes assertion below is what discriminates the encodings, and Task 10's
//     BuildTree is what would silently rebuild a { Key="WaitSeconds", DefaultValue=0.0 } Wait node
//     as 5.0 if this were got wrong.
//
//  2. A blackboard key selector is a name plus a resolved key ID, and only the ID is read at
//     runtime. Every assertion about a binding here is therefore about GetSelectedKeyID(), read off
//     the live instance; the name proves nothing.
//
//  3. A write is not finished when ImportText succeeds. UBTNode::PreSave rewrites blackboard
//     bindings — FBlackboardKeySelector AND FValueOrBlackboardKeyBase, which is what an ordinary
//     numeric property like BTTask_Wait::WaitTime is in UE 5.8 — during the save the commit
//     performs. Both writers therefore verify what the asset actually kept.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTNodePropertiesTest,
	"VibeUE.BehaviorTree.Properties.SetAndRead", kBTTestFlags)
bool FVibeBTNodePropertiesTest::RunTest(const FString&)
{
	const FString BBPath = FString(kBTTestDir) / TEXT("BB_PropsTest");
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_PropsTest");
	FScopedFixtureReset ResetBB(BBPath);
	FScopedFixtureReset ResetBT(BTPath);

	// --- The blackboard. -------------------------------------------------------------------------
	// Beyond the Bool and Object keys the brief calls for, the board carries a Float key and an
	// Object(AnimMontage) key. Those are not decoration: the nodes below carry key selectors whose
	// filters admit nothing else, and FBlackboardKeySelector::ResolveSelectedKey logs a
	// LogBehaviorTree warning (BehaviorTreeTypes.cpp:556) — which fails the run — when
	// InitSelection finds no key matching a filter at commit time.
	TestTrue(TEXT("blackboard created"), UBlackboardService::CreateBlackboard(BBPath, FString()));
	TestEqual(TEXT("Alpha (Bool) added"),
		UBlackboardService::AddBlackboardKey(BBPath, TEXT("Alpha"), TEXT("Bool"), false), FString());
	TestEqual(TEXT("TargetActor (Object) added"),
		UBlackboardService::AddBlackboardKey(BBPath, TEXT("TargetActor"), TEXT("Object"), false), FString());
	TestEqual(TEXT("TargetActor is an Actor key"),
		UBlackboardService::SetBlackboardKeyObjectClass(BBPath, TEXT("TargetActor"),
			TEXT("/Script/Engine.Actor")), FString());
	TestEqual(TEXT("WaitSeconds (Float) added"),
		UBlackboardService::AddBlackboardKey(BBPath, TEXT("WaitSeconds"), TEXT("Float"), false), FString());
	TestEqual(TEXT("Montage (Object) added"),
		UBlackboardService::AddBlackboardKey(BBPath, TEXT("Montage"), TEXT("Object"), false), FString());
	TestEqual(TEXT("Montage is an AnimMontage key"),
		UBlackboardService::SetBlackboardKeyObjectClass(BBPath, TEXT("Montage"),
			TEXT("/Script/Engine.AnimMontage")), FString());

	UBlackboardData* Board = LoadObject<UBlackboardData>(nullptr, *BBPath);
	if (!TestNotNull(TEXT("board loaded"), Board))
	{
		return false;
	}
	const FBlackboard::FKey AlphaID = Board->GetKeyID(TEXT("Alpha"));
	const FBlackboard::FKey TargetID = Board->GetKeyID(TEXT("TargetActor"));
	// Without this the ID assertions below could hold for the wrong reason.
	TestTrue(TEXT("Alpha and TargetActor resolve to distinct, valid key IDs"),
		AlphaID != FBlackboard::InvalidKey && TargetID != FBlackboard::InvalidKey && AlphaID != TargetID);

	// --- The tree. -------------------------------------------------------------------------------
	TestEqual(TEXT("tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, BBPath), FString());
	const FString Sel = UBehaviorTreeService::AddNode(
		BTPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	const FString Seq = UBehaviorTreeService::AddNode(BTPath, Sel, TEXT("BTComposite_Sequence"), -1);
	const FString WaitA = UBehaviorTreeService::AddNode(BTPath, Seq, TEXT("BTTask_Wait"), -1);
	const FString WaitB = UBehaviorTreeService::AddNode(BTPath, Seq, TEXT("BTTask_Wait"), -1);
	const FString MoveTo = UBehaviorTreeService::AddNode(BTPath, Sel, TEXT("BTTask_MoveTo"), -1);
	const FString Dec = UBehaviorTreeService::AddDecorator(
		BTPath, Seq, TEXT("BTDecorator_Blackboard"), -1);
	// Bailing out rather than continuing: every assertion below addresses these paths, and a suite
	// of "no node at path" failures would say nothing about what actually broke.
	if (!TestFalse(TEXT("selector added"), Sel.StartsWith(TEXT("ERROR")))
		|| !TestFalse(TEXT("sequence added"), Seq.StartsWith(TEXT("ERROR")))
		|| !TestFalse(TEXT("first wait added"), WaitA.StartsWith(TEXT("ERROR")))
		|| !TestFalse(TEXT("second wait added"), WaitB.StartsWith(TEXT("ERROR")))
		|| !TestFalse(TEXT("move-to added"), MoveTo.StartsWith(TEXT("ERROR")))
		|| !TestFalse(TEXT("decorator added"), Dec.StartsWith(TEXT("ERROR"))))
	{
		AddError(FString::Printf(TEXT("fixture build failed: %s | %s | %s | %s | %s | %s"),
			*Sel, *Seq, *WaitA, *WaitB, *MoveTo, *Dec));
		return false;
	}

	// --- GetNodePropertyNames. -------------------------------------------------------------------
	const TArray<FBTPropertyInfo> WaitProps =
		UBehaviorTreeService::GetNodePropertyNames(BTPath, WaitA);
	const FBTPropertyInfo* WaitTimeInfo = WaitProps.FindByPredicate(
		[](const FBTPropertyInfo& P){ return P.Name == TEXT("WaitTime"); });
	if (TestNotNull(TEXT("BTTask_Wait reports its WaitTime property"), WaitTimeInfo))
	{
		// UE 5.8 changed this from a bare float to a struct that can bind to a blackboard key
		// (BTTask_Wait.h:24) — recorded because it is what the value literals below have to look like.
		AddInfo(FString::Printf(TEXT("BTTask_Wait::WaitTime is of type %s, value %s"),
			*WaitTimeInfo->Type, *WaitTimeInfo->Value));
		TestFalse(TEXT("...and it is not a blackboard key selector"),
			WaitTimeInfo->bIsBlackboardKeySelector);
		TestFalse(TEXT("...and reports a value"), WaitTimeInfo->Value.IsEmpty());
	}
	TestFalse(TEXT("structure is not reported as a settable property"),
		UBehaviorTreeService::GetNodePropertyNames(BTPath, Sel).ContainsByPredicate(
			[](const FBTPropertyInfo& P){ return P.Name == TEXT("Children"); }));
	// The graph's Root node carries no instance at all.
	TestEqual(TEXT("the Root node reports no properties"),
		UBehaviorTreeService::GetNodePropertyNames(BTPath, TEXT("Root")).Num(), 0);

	const TArray<FBTPropertyInfo> DecProps = UBehaviorTreeService::GetNodePropertyNames(BTPath, Dec);
	const FBTPropertyInfo* KeyInfo = DecProps.FindByPredicate(
		[](const FBTPropertyInfo& P){ return P.Name == TEXT("BlackboardKey"); });
	if (TestNotNull(TEXT("the decorator reports its BlackboardKey property"), KeyInfo))
	{
		TestTrue(TEXT("...flagged as a blackboard key selector"), KeyInfo->bIsBlackboardKeySelector);
	}
	TestTrue(TEXT("and a plain property on the same node is not flagged"),
		DecProps.ContainsByPredicate([](const FBTPropertyInfo& P)
		{
			return P.Name == TEXT("NodeName") && !P.bIsBlackboardKeySelector;
		}));

	// --- SetNodePropertyValue: the ordinary case. -------------------------------------------------
	const FBTPropertySetResult WaitWrite = UBehaviorTreeService::SetNodePropertyValue(
		BTPath, WaitA, TEXT("WaitTime"), TEXT("(DefaultValue=3.500000)"));
	TestTrue(TEXT("WaitTime written"), WaitWrite.bSuccess);
	TestEqual(TEXT("no error on success"), WaitWrite.Error, FString());
	TestEqual(TEXT("the response names the node it actually hit"), WaitWrite.ResolvedNodeClass,
		FString(TEXT("BTTask_Wait")));
	TestTrue(TEXT("the read-back carries the value that was written"),
		WaitWrite.ValueAfterWrite.Contains(TEXT("3.500000")));
	TestEqual(TEXT("GetNodePropertyValue agrees with ValueAfterWrite"),
		UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitA, TEXT("WaitTime")),
		WaitWrite.ValueAfterWrite);

	// --- The encoding, measured rather than assumed. -----------------------------------------------
	//
	// Three encodings of the same live value: against the instance itself (what this service emits,
	// which is the engine's "export everything" path rather than a diff), against the class default
	// object, and against no defaults at all. The last two both drop members, and a dropped member
	// is a value a caller cannot write back.
	FString DeltaVsCdo;
	FString DeltaVsNull;
	FString CdoLiteral;
	if (UObject* WaitInstance = VibeBTFindLiveInstance(BTPath, WaitA))
	{
		if (FProperty* Property = WaitInstance->GetClass()->FindPropertyByName(TEXT("WaitTime")))
		{
			UObject* Cdo = WaitInstance->GetClass()->GetDefaultObject();
			Property->ExportText_InContainer(0, DeltaVsCdo, WaitInstance, Cdo, WaitInstance, PPF_None);
			Property->ExportText_InContainer(0, DeltaVsNull, WaitInstance, nullptr,
				WaitInstance, PPF_None);
			VibeBT::ExportPropertyValue(Property, Cdo, CdoLiteral);
		}
	}
	AddInfo(FString::Printf(
		TEXT("WaitTime encodings — self-delta (what this service emits): '%s' | vs CDO: '%s' | vs no "
			 "defaults: '%s' | the CDO's own value: '%s'"),
		*WaitWrite.ValueAfterWrite, *DeltaVsCdo, *DeltaVsNull, *CdoLiteral));
	// Not the discriminator — it holds under every encoding, because exporting the CDO against the
	// CDO is itself a self-delta and always emits in full. The discriminating assertions are the two
	// on ValueAtDefault below, and the cross-node copy after them.
	TestFalse(TEXT("the class default literal is not empty"),
		CdoLiteral.IsEmpty() || CdoLiteral == TEXT("()"));
	TestTrue(TEXT("a complete literal carries every member, including ones left at their default"),
		WaitWrite.ValueAfterWrite.Contains(TEXT("DefaultValue"))
			&& WaitWrite.ValueAfterWrite.Contains(TEXT("Key")));

	// Same node, straight back: the round trip this service actually promises.
	TestTrue(TEXT("the second Wait takes a two-member literal"),
		UBehaviorTreeService::SetNodePropertyValue(
			BTPath, WaitB, TEXT("WaitTime"), TEXT("(Key=\"WaitSeconds\",DefaultValue=1.000000)")).bSuccess);
	const FString ValueB = UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitB, TEXT("WaitTime"));
	TestTrue(TEXT("both members are reported, not just the one that differs from the CDO"),
		ValueB.Contains(TEXT("WaitSeconds")) && ValueB.Contains(TEXT("1.000000")));
	TestTrue(TEXT("the value it reported is accepted back"),
		UBehaviorTreeService::SetNodePropertyValue(BTPath, WaitB, TEXT("WaitTime"), ValueB).bSuccess);
	TestEqual(TEXT("get -> set -> get on the same node reproduces the struct exactly"),
		UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitB, TEXT("WaitTime")), ValueB);

	// The discriminating case for the export decision: a value that happens to EQUAL the class
	// default. Exported against the CDO it collapses to "()" — indistinguishable from "unset", and
	// worthless as an input. Exported against no defaults it is still the value it is.
	// The CDO's own literal, taken from the CDO rather than hardcoded, so this keeps meaning the same
	// thing if Epic changes BTTask_Wait's default. It needs no patching to clear the non-default Key
	// this node currently carries: a complete literal names every member, so writing it IS a reset.
	TestTrue(TEXT("the second Wait is set to exactly its class default"),
		UBehaviorTreeService::SetNodePropertyValue(BTPath, WaitB, TEXT("WaitTime"), CdoLiteral).bSuccess);
	const FString ValueAtDefault =
		UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitB, TEXT("WaitTime"));
	AddInfo(FString::Printf(TEXT("a value equal to the class default reads back as '%s' (CDO literal "
								 "'%s', delta against the CDO would be '()')"),
		*ValueAtDefault, *CdoLiteral));
	TestTrue(TEXT("a value equal to the class default is still reported in full"),
		ValueAtDefault.Contains(TEXT("DefaultValue")));
	TestEqual(TEXT("...and matches the class default's own literal"), ValueAtDefault, CdoLiteral);

	// Carrying a struct value BETWEEN nodes: a copy, not a merge. This is the assertion the encoding
	// exists for, and the one Task 10's BuildTree depends on — it replays recorded values onto FRESH
	// node instances, so any member the reader dropped comes back as that class's default instead of
	// as the recorded value. WaitA's Key is at its default and the target's is not, which is exactly
	// the case a defaults-relative export gets wrong.
	const FString ValueA = UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitA, TEXT("WaitTime"));
	TestTrue(TEXT("the source value names its default-valued Key member too"),
		ValueA.Contains(TEXT("Key")));
	TestTrue(TEXT("the target is given a non-default Key first"),
		UBehaviorTreeService::SetNodePropertyValue(
			BTPath, WaitB, TEXT("WaitTime"), TEXT("(Key=\"WaitSeconds\",DefaultValue=1.000000)")).bSuccess);
	TestTrue(TEXT("and accepts the other node's value"),
		UBehaviorTreeService::SetNodePropertyValue(BTPath, WaitB, TEXT("WaitTime"), ValueA).bSuccess);
	const FString Copied = UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitB, TEXT("WaitTime"));
	AddInfo(FString::Printf(TEXT("cross-node copy of '%s' onto a node with Key=\"WaitSeconds\" gives '%s'"),
		*ValueA, *Copied));
	TestEqual(TEXT("get -> set -> get across two nodes reproduces the struct exactly"), Copied, ValueA);
	TestFalse(TEXT("...leaving nothing of what the target used to hold"),
		Copied.Contains(TEXT("WaitSeconds")));

	// --- An array property, round-tripped the same way. --------------------------------------------
	// UE 5.8's AIModule has no BT node class with an editable array property at all (checked: the
	// only TArray UPROPERTYs on BT types are FBlackboardKeySelector::AllowedTypes, which is
	// transient, and UBlackboardData::Keys), so this needs a project-supplied task and SKIPS where
	// there is none. VibeUE is a standalone plugin: a hard dependency on a game class would make
	// this suite unrunnable in any other project, and the array round trip is worth covering where
	// it can be rather than not at all.
	const FString Idle = UBehaviorTreeService::AddNode(BTPath, Sel, TEXT("NSTask_PatrolIdleWait"), -1);
	if (Idle.StartsWith(TEXT("ERROR")))
	{
		AddInfo(TEXT("skipped the array round trip: no BT node class carrying an editable array "
					 "property is available in this project (looked for NSTask_PatrolIdleWait)"));
	}
	else
	{
		const FBTPropertySetResult ArrayWrite = UBehaviorTreeService::SetNodePropertyValue(
			BTPath, Idle, TEXT("IdleMontages"), TEXT("(None,None)"));
		TestTrue(TEXT("an array literal is accepted"), ArrayWrite.bSuccess);
		AddInfo(FString::Printf(TEXT("IdleMontages after write: '%s' (error: '%s')"),
			*ArrayWrite.ValueAfterWrite, *ArrayWrite.Error));

		const FString ArrayValue =
			UBehaviorTreeService::GetNodePropertyValue(BTPath, Idle, TEXT("IdleMontages"));
		TestEqual(TEXT("the array reads back as it was written"), ArrayValue, FString(TEXT("(None,None)")));
		TestTrue(TEXT("and can be written straight back"),
			UBehaviorTreeService::SetNodePropertyValue(BTPath, Idle, TEXT("IdleMontages"), ArrayValue).bSuccess);
		TestEqual(TEXT("get -> set -> get reproduces the array exactly"),
			UBehaviorTreeService::GetNodePropertyValue(BTPath, Idle, TEXT("IdleMontages")), ArrayValue);

		// An empty array exports as an empty string, not "()" — FArrayProperty::ExportTextInnerItem
		// opens the parenthesis on the first element and emits nothing at all when there are none
		// (PropertyArray.cpp:1063-1116). Asserted rather than glossed over: it is the one value this
		// service reports that cannot be handed straight back to it.
		TestTrue(TEXT("the array can be emptied again"),
			UBehaviorTreeService::SetNodePropertyValue(BTPath, Idle, TEXT("IdleMontages"), TEXT("()")).bSuccess);
		TestEqual(TEXT("an empty array reads back as an empty string"),
			UBehaviorTreeService::GetNodePropertyValue(BTPath, Idle, TEXT("IdleMontages")), FString());
	}

	// --- Refusals on the generic setter. -----------------------------------------------------------
	const FBTPropertySetResult NoSuchProperty = UBehaviorTreeService::SetNodePropertyValue(
		BTPath, WaitA, TEXT("NoSuchProperty"), TEXT("1"));
	TestFalse(TEXT("a nonexistent property is refused"), NoSuchProperty.bSuccess);
	TestTrue(TEXT("...with an explanation"), NoSuchProperty.Error.Contains(TEXT("has no property")));

	// A real property that is structure rather than settable state. The message matters: "it errored"
	// would also hold if the value had simply failed to parse.
	const FBTPropertySetResult Structure = UBehaviorTreeService::SetNodePropertyValue(
		BTPath, Sel, TEXT("Children"), TEXT("()"));
	TestFalse(TEXT("a non-editable property is refused"), Structure.bSuccess);
	TestTrue(TEXT("...as not editable, not as missing"), Structure.Error.Contains(TEXT("not editable")));

	// A value that cannot be parsed must leave the property exactly as it was: ImportText applies a
	// struct literal member by member and stops where it fails.
	//
	// Deliberately a literal that fails LATE: an unterminated struct, whose first member imports
	// cleanly before the parse runs off the end. That is what makes this assertion about the
	// rollback rather than about ImportText refusing the string on sight — a literal that is merely
	// garbage from the first character never writes anything, so it cannot tell the two apart.
	// (A literal that is wrong only in a member's VALUE — "(DefaultValue=**nonsense**)" — is not
	// refused at all: the struct importer coerces what it cannot parse and returns success. Measured.)
	const FString BeforeBadWrite =
		UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitA, TEXT("WaitTime"));
	const FBTPropertySetResult BadValue = UBehaviorTreeService::SetNodePropertyValue(
		BTPath, WaitA, TEXT("WaitTime"), TEXT("(Key=\"Hijacked\",DefaultValue=9.000000"));
	TestFalse(TEXT("an unparseable value is refused"), BadValue.bSuccess);
	TestTrue(TEXT("...saying it could not parse it"), BadValue.Error.Contains(TEXT("could not parse")));
	TestEqual(TEXT("and the half-applied literal was rolled back"),
		UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitA, TEXT("WaitTime")), BeforeBadWrite);

	// Reads report their own failures rather than returning something value-shaped.
	TestTrue(TEXT("reading a nonexistent property errors"),
		UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitA, TEXT("NoSuchProperty"))
			.StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("reading through a nonexistent path errors"),
		UBehaviorTreeService::GetNodePropertyValue(BTPath, TEXT("Root/Nope"), TEXT("NodeName"))
			.StartsWith(TEXT("ERROR")));

	// --- SetNodeBlackboardKey. ----------------------------------------------------------------------
	const FBTPropertySetResult Bind = UBehaviorTreeService::SetNodeBlackboardKey(
		BTPath, Dec, TEXT("BlackboardKey"), TEXT("Alpha"));
	TestTrue(TEXT("the decorator's key is bound"), Bind.bSuccess);
	TestEqual(TEXT("no error on success"), Bind.Error, FString());

	// THE assertion. The name reads back correctly whether or not the binding resolved, so the ID is
	// the only thing that distinguishes a working node from one that silently does nothing.
	FBlackboardKeySelector* DecSelector = VibeBTFindLiveSelector(BTPath, Dec, TEXT("BlackboardKey"));
	if (TestNotNull(TEXT("the decorator's live selector is reachable"), DecSelector))
	{
		AddInfo(FString::Printf(TEXT("decorator selector: name=%s id=%d (Alpha=%d, TargetActor=%d)"),
			*DecSelector->SelectedKeyName.ToString(), (int32)DecSelector->GetSelectedKeyID(),
			(int32)AlphaID, (int32)TargetID));
		TestEqual(TEXT("the selector resolved to Alpha's key ID, not merely to its name"),
			DecSelector->GetSelectedKeyID(), AlphaID);
	}

	// A key that does not exist fails loudly, and changes nothing.
	const FBTPropertySetResult Missing = UBehaviorTreeService::SetNodeBlackboardKey(
		BTPath, Dec, TEXT("BlackboardKey"), TEXT("NoSuchKey"));
	TestFalse(TEXT("binding a nonexistent key is refused"), Missing.bSuccess);
	TestTrue(TEXT("...naming the key and the board"),
		Missing.Error.Contains(TEXT("NoSuchKey")) && Missing.Error.Contains(TEXT("BB_PropsTest")));
	if (DecSelector)
	{
		TestEqual(TEXT("and the refused binding left the working one alone"),
			DecSelector->GetSelectedKeyID(), AlphaID);
	}

	// The type filter. UBTTask_MoveTo accepts Object(AActor) and Vector keys only
	// (BTTask_MoveTo.cpp:35-36), and ResolveSelectedKey does NOT check the filter — a Bool key would
	// resolve to a perfectly valid ID here, so nothing but an explicit filter test can refuse it.
	const FBTPropertySetResult WrongType = UBehaviorTreeService::SetNodeBlackboardKey(
		BTPath, MoveTo, TEXT("BlackboardKey"), TEXT("Alpha"));
	TestFalse(TEXT("binding a Bool key to an Object/Vector selector is refused"), WrongType.bSuccess);
	TestTrue(TEXT("...as a type mismatch, not as a missing key"),
		WrongType.Error.Contains(TEXT("does not accept")));
	AddInfo(FString::Printf(TEXT("type-filter refusal: %s"), *WrongType.Error));

	const FBTPropertySetResult RightType = UBehaviorTreeService::SetNodeBlackboardKey(
		BTPath, MoveTo, TEXT("BlackboardKey"), TEXT("TargetActor"));
	TestTrue(TEXT("...while an accepted type binds"), RightType.bSuccess);
	if (FBlackboardKeySelector* MoveSelector = VibeBTFindLiveSelector(BTPath, MoveTo, TEXT("BlackboardKey")))
	{
		TestEqual(TEXT("and resolves to TargetActor's key ID"),
			MoveSelector->GetSelectedKeyID(), TargetID);
	}

	// Wrong tool for the property.
	const FBTPropertySetResult NotASelector = UBehaviorTreeService::SetNodeBlackboardKey(
		BTPath, WaitA, TEXT("WaitTime"), TEXT("Alpha"));
	TestFalse(TEXT("SetNodeBlackboardKey refuses a property that is not a key selector"),
		NotASelector.bSuccess);
	TestTrue(TEXT("...and says so"),
		NotASelector.Error.Contains(TEXT("not a blackboard key selector")));

	// --- Why the dedicated setter exists, and what the generic path can only do afterwards. ---------
	//
	// The generic path writes the selector's NAME and nothing else, and has no idea whether that was
	// a legal binding. It finds out at commit time, from UBTNode::PreSave (BTNode.cpp:231-254), which
	// resets any selector bound to a key its filter forbids — at which point the value the caller
	// asked for is gone. The post-commit check is what turns that into a reported failure instead of
	// a success with a surprising read-back; SetNodeBlackboardKey refuses the same call up front,
	// without mutating the node and without the engine warning.
	AddExpectedMessagePlain(TEXT("but the key type isn't allowed"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, /*Occurrences*/ 0);
	const FBTPropertySetResult GenericWrite = UBehaviorTreeService::SetNodePropertyValue(
		BTPath, MoveTo, TEXT("BlackboardKey"), TEXT("(SelectedKeyName=\"Alpha\")"));
	AddInfo(FString::Printf(TEXT("generic write of a mistyped key: success=%s, value after write=%s, "
								 "error=%s"),
		GenericWrite.bSuccess ? TEXT("true") : TEXT("false"), *GenericWrite.ValueAfterWrite,
		*GenericWrite.Error));
	TestFalse(TEXT("the generic path reports the discarded binding as a failure"), GenericWrite.bSuccess);
	TestTrue(TEXT("...saying the commit did not keep what was written"),
		GenericWrite.Error.Contains(TEXT("but the asset saved")));
	if (FBlackboardKeySelector* MoveSelector = VibeBTFindLiveSelector(BTPath, MoveTo, TEXT("BlackboardKey")))
	{
		// What the asset really holds: a node bound to nothing at all.
		TestTrue(TEXT("...and the node really is bound to nothing"),
			MoveSelector->SelectedKeyName.IsNone());
	}
	TestEqual(TEXT("and the read-back is the saved state, not an echo of the input"),
		GenericWrite.ValueAfterWrite,
		UBehaviorTreeService::GetNodePropertyValue(BTPath, MoveTo, TEXT("BlackboardKey")));
	TestFalse(TEXT("which is no longer the value that was asked for"),
		GenericWrite.ValueAfterWrite.Contains(TEXT("Alpha")));

	// The same hazard on an ordinary-looking numeric property. In UE 5.8 BTTask_Wait::WaitTime is an
	// FValueOrBBKey_Float, whose Key binds to a blackboard entry and whose
	// FValueOrBlackboardKeyBase::PreSave (ValueOrBBKey.cpp:582-610) clears it exactly the way the key
	// selector's does. GetNodePropertyNames reports bIsBlackboardKeySelector = false for it and
	// SetNodeBlackboardKey refuses it, so the generic path is the ONLY way to write it, and the
	// post-commit check is the only thing standing between a caller and a silently dropped binding.
	AddExpectedMessagePlain(TEXT("but the type doesn't match in blackboard"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, /*Occurrences*/ 0);
	const FString BeforeMistyped =
		UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitB, TEXT("WaitTime"));
	const FBTPropertySetResult MistypedValueKey = UBehaviorTreeService::SetNodePropertyValue(
		BTPath, WaitB, TEXT("WaitTime"), TEXT("(Key=\"Alpha\",DefaultValue=2.000000)"));
	AddInfo(FString::Printf(TEXT("FValueOrBBKey_Float bound to a Bool key: success=%s, before='%s', "
								 "after='%s', error=%s"),
		MistypedValueKey.bSuccess ? TEXT("true") : TEXT("false"), *BeforeMistyped,
		*MistypedValueKey.ValueAfterWrite, *MistypedValueKey.Error));
	TestFalse(TEXT("a mistyped FValueOrBBKey binding is reported as a failure, not silently dropped"),
		MistypedValueKey.bSuccess);
	TestTrue(TEXT("...saying the commit did not keep what was written"),
		MistypedValueKey.Error.Contains(TEXT("but the asset saved")));
	TestFalse(TEXT("...and the saved value really has lost the key"),
		MistypedValueKey.ValueAfterWrite.Contains(TEXT("Alpha")));
	TestEqual(TEXT("the reported value is what is really on the node"),
		UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitB, TEXT("WaitTime")),
		MistypedValueKey.ValueAfterWrite);
	// A correctly typed key on the same property still succeeds, so the check is not simply refusing
	// everything that touches a blackboard binding.
	const FBTPropertySetResult TypedValueKey = UBehaviorTreeService::SetNodePropertyValue(
		BTPath, WaitB, TEXT("WaitTime"), TEXT("(Key=\"WaitSeconds\",DefaultValue=2.000000)"));
	TestTrue(TEXT("a correctly typed FValueOrBBKey binding still succeeds"), TypedValueKey.bSuccess);
	TestTrue(TEXT("...and survives the commit"),
		TypedValueKey.ValueAfterWrite.Contains(TEXT("WaitSeconds")));

	// ...and the dedicated setter puts the selector back.
	TestTrue(TEXT("the dedicated setter restores a working binding"),
		UBehaviorTreeService::SetNodeBlackboardKey(BTPath, MoveTo, TEXT("BlackboardKey"),
			TEXT("TargetActor")).bSuccess);
	if (FBlackboardKeySelector* MoveSelector = VibeBTFindLiveSelector(BTPath, MoveTo, TEXT("BlackboardKey")))
	{
		TestEqual(TEXT("resolved again"), MoveSelector->GetSelectedKeyID(), TargetID);
	}

	// --- A tree with no blackboard has nothing to bind against. -------------------------------------
	const FString BarePath = FString(kBTTestDir) / TEXT("BT_PropsBareTest");
	FScopedFixtureReset ResetBare(BarePath);
	TestEqual(TEXT("bare tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BarePath, FString()), FString());
	const FString BareSel = UBehaviorTreeService::AddNode(
		BarePath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	const FString BareDec = UBehaviorTreeService::AddDecorator(
		BarePath, BareSel, TEXT("BTDecorator_Blackboard"), -1);
	if (TestFalse(TEXT("bare decorator added"), BareDec.StartsWith(TEXT("ERROR"))))
	{
		const FBTPropertySetResult NoBoard = UBehaviorTreeService::SetNodeBlackboardKey(
			BarePath, BareDec, TEXT("BlackboardKey"), TEXT("Alpha"));
		TestFalse(TEXT("binding against a tree with no blackboard is refused"), NoBoard.bSuccess);
		TestTrue(TEXT("...saying that is why"), NoBoard.Error.Contains(TEXT("no blackboard asset")));
	}

	// --- The asset survived all of it. ---------------------------------------------------------------
	TestEqual(TEXT("the tree still commits"), UBehaviorTreeService::CompileAndSave(BTPath), FString());
	TestEqual(TEXT("and the edited value is still there afterwards"),
		UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitA, TEXT("WaitTime")), BeforeBadWrite);

	// GetTree's "properties" map and GetNodePropertyValue are the same encoding, since a caller that
	// reads a value out of the dump must be able to write it back.
	const TSharedPtr<FJsonObject> Dump = ParseTree(UBehaviorTreeService::GetTree(BTPath));
	const TSharedPtr<FJsonObject> WaitObject = FindNodeByPath(Dump, WaitA);
	if (TestTrue(TEXT("the edited Wait node is in the dump"), WaitObject.IsValid()))
	{
		const TSharedPtr<FJsonObject>& Props = WaitObject->GetObjectField(TEXT("properties"));
		FString DumpValue;
		if (TestTrue(TEXT("the dump reports WaitTime as edited"),
			Props->TryGetStringField(TEXT("WaitTime"), DumpValue)))
		{
			TestEqual(TEXT("and reports it in the same encoding GetNodePropertyValue does"),
				DumpValue, UBehaviorTreeService::GetNodePropertyValue(BTPath, WaitA, TEXT("WaitTime")));
		}
	}

	return true;
}

// ValidateTree: a read-only diagnostic sweep. Four hazards from earlier tasks, specifically:
//
//  1. UBTNode::PreSave (BTNode.cpp:231-254) validates only TOP-LEVEL struct properties on the node
//     class. A selector nested one struct deeper gets no engine validation at all. Covered by the
//     nested-selector case below, using UBTTask_RunEQSQuery::EQSRequest (an
//     FEQSParametrizedQueryExecutionRequest) -> QueryConfig (TArray<FAIDynamicParam>) ->
//     FAIDynamicParam::BBKey (a plain FBlackboardKeySelector two levels down) — all native AIModule
//     types, nothing project-specific.
//  2. FValueOrBBKey_* (BTTask_Wait::WaitTime in 5.8) carries the identical silent-reset hazard as
//     FBlackboardKeySelector. Covered by the WaitTime case below.
//  3. SimpleParallel's two output pins. Not independently re-tested here — ValidateTree's
//     composite/child walk goes through VibeBT::GetChildNodes exactly like every other read path in
//     this service, and that is already covered by Structure.SimpleParallel.
//  4. Injected nodes must never be reported. Covered by the injected-node case below, which gives an
//     injected decorator an unresolved key that WOULD be reported if the injected-node skip were
//     missing, and asserts it is not.
//
// Evidence discipline: every "reports X" assertion below is discriminating, not a tautology — proved
// by temporarily short-circuiting ValidateTree's node walk to always return an empty array and
// re-running this test, which turned every one of those assertions Result={Fail} while the healthy-
// tree assertion still trivially passed. See task-9-report.md for the exact log lines. Reverted
// before committing.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTValidateTest,
	"VibeUE.BehaviorTree.Validate.Diagnostics", kBTTestFlags)
bool FVibeBTValidateTest::RunTest(const FString&)
{
	const FString BBPath = FString(kBTTestDir) / TEXT("BB_ValidateTest");
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_ValidateTest");
	FScopedFixtureReset ResetBB(BBPath);
	FScopedFixtureReset ResetBT(BTPath);

	// --- The blackboard, and a healthy baseline tree. -----------------------------------------------
	TestTrue(TEXT("blackboard created"), UBlackboardService::CreateBlackboard(BBPath, FString()));
	TestEqual(TEXT("Flag (Bool) key added"),
		UBlackboardService::AddBlackboardKey(BBPath, TEXT("Flag"), TEXT("Bool"), false), FString());

	TestEqual(TEXT("tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, BBPath), FString());
	const FString Seq = UBehaviorTreeService::AddNode(BTPath, TEXT("Root"), TEXT("BTComposite_Sequence"), -1);
	const FString WaitA = UBehaviorTreeService::AddNode(BTPath, Seq, TEXT("BTTask_Wait"), -1);
	const FString Dec = UBehaviorTreeService::AddDecorator(BTPath, Seq, TEXT("BTDecorator_Blackboard"), -1);
	if (!TestFalse(TEXT("sequence added"), Seq.StartsWith(TEXT("ERROR")))
		|| !TestFalse(TEXT("wait added"), WaitA.StartsWith(TEXT("ERROR")))
		|| !TestFalse(TEXT("decorator added"), Dec.StartsWith(TEXT("ERROR"))))
	{
		AddError(FString::Printf(TEXT("fixture build failed: %s | %s | %s"), *Seq, *WaitA, *Dec));
		return false;
	}
	TestTrue(TEXT("decorator bound to a real key"),
		UBehaviorTreeService::SetNodeBlackboardKey(BTPath, Dec, TEXT("BlackboardKey"), TEXT("Flag")).bSuccess);
	TestEqual(TEXT("baseline commits"), UBehaviorTreeService::CompileAndSave(BTPath), FString());

	TestEqual(TEXT("a healthy tree validates clean"),
		UBehaviorTreeService::ValidateTree(BTPath).Num(), 0);

	// --- Defect: a composite with no children. --------------------------------------------------------
	const FString EmptySel = UBehaviorTreeService::AddNode(BTPath, Seq, TEXT("BTComposite_Selector"), -1);
	if (TestFalse(TEXT("empty selector added"), EmptySel.StartsWith(TEXT("ERROR"))))
	{
		TestEqual(TEXT("commits with a childless composite present"),
			UBehaviorTreeService::CompileAndSave(BTPath), FString());
		const TArray<FString> ChildlessDiag = UBehaviorTreeService::ValidateTree(BTPath);
		TestTrue(TEXT("reports the childless composite"), ChildlessDiag.ContainsByPredicate(
			[&EmptySel](const FString& Line)
			{ return Line.StartsWith(EmptySel) && Line.Contains(TEXT("composite has no children")); }));
	}

	// --- Defect: a node whose NodeInstance class failed to load. --------------------------------------
	{
		UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
		UBehaviorTreeGraph* Graph = Tree ? Cast<UBehaviorTreeGraph>(Tree->BTGraph) : nullptr;
		UBehaviorTreeGraphNode* WaitNode = Graph ? VibeBT::ResolveNodePath(Graph, WaitA) : nullptr;
		if (TestNotNull(TEXT("wait node resolved for corruption"), WaitNode) && Tree)
		{
			WaitNode->NodeInstance = nullptr; // Simulates a class that failed to resolve on load.
			// GetNodePath is recomputed AFTER the corruption, not taken from WaitA: nulling
			// NodeInstance changes the graph node's own title too
			// (UBehaviorTreeGraphNode_Task::GetNodeTitle falls back to a "Class ... not found"
			// message once NodeInstance is null), which changes what path segment names it.
			const FString CorruptedPath = VibeBT::GetNodePath(WaitNode);
			const TArray<FString> NullInstanceDiag = UBehaviorTreeService::ValidateTree(BTPath);
			TestTrue(TEXT("reports the failed-to-load node"), NullInstanceDiag.ContainsByPredicate(
				[&CorruptedPath](const FString& Line)
				{ return Line.StartsWith(CorruptedPath) && Line.Contains(TEXT("failed to load")); }));

			// Restore it before any further commit: nothing below should have to route around a
			// broken WaitA, and CompileAndSave must not be exercised against a null instance here —
			// that shape is Task 6/9's concern, not this test's.
			WaitNode->NodeInstance = NewObject<UBTTask_Wait>(Tree, NAME_None, RF_Transactional);
		}
	}

	// --- Defect: an unresolved (top-level) blackboard key selector. -----------------------------------
	if (FBlackboardKeySelector* Selector = VibeBTFindLiveSelector(BTPath, Dec, TEXT("BlackboardKey")))
	{
		Selector->SelectedKeyName = TEXT("NoSuchKeyAtAll");
		const TArray<FString> SelectorDiag = UBehaviorTreeService::ValidateTree(BTPath);
		TestTrue(TEXT("reports the unresolved selector"), SelectorDiag.ContainsByPredicate(
			[&Dec](const FString& Line)
			{ return Line.StartsWith(Dec) && Line.Contains(TEXT("NoSuchKeyAtAll")); }));
		// Restored immediately: a later CompileAndSave in this test would otherwise hit
		// UBTNode::PreSave's own top-level reset of this exact selector and log a warning.
		Selector->SelectedKeyName = TEXT("Flag");
	}

	// --- Hazard #1: the SAME kind of defect, but nested two structs deep. -----------------------------
	const FString EqsTask = UBehaviorTreeService::AddNode(BTPath, Seq, TEXT("BTTask_RunEQSQuery"), -1);
	if (TestFalse(TEXT("EQS task added"), EqsTask.StartsWith(TEXT("ERROR"))))
	{
		UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
		UBehaviorTreeGraph* Graph = Tree ? Cast<UBehaviorTreeGraph>(Tree->BTGraph) : nullptr;
		UBehaviorTreeGraphNode* EqsNode = Graph ? VibeBT::ResolveNodePath(Graph, EqsTask) : nullptr;
		UObject* EqsInstance = EqsNode ? ToRawPtr(EqsNode->NodeInstance) : nullptr;
		FStructProperty* ReqProp = EqsInstance
			? CastField<FStructProperty>(EqsInstance->GetClass()->FindPropertyByName(TEXT("EQSRequest")))
			: nullptr;
		FEQSParametrizedQueryExecutionRequest* Req = ReqProp
			? ReqProp->ContainerPtrToValuePtr<FEQSParametrizedQueryExecutionRequest>(EqsInstance)
			: nullptr;
		if (TestNotNull(TEXT("EQSRequest reflected"), Req))
		{
			FAIDynamicParam Param;
			Param.ParamName = TEXT("TestParam");
			Param.BBKey.SelectedKeyName = TEXT("NoSuchNestedKey");
			Req->QueryConfig.Add(Param);

			const TArray<FString> NestedDiag = UBehaviorTreeService::ValidateTree(BTPath);
			TestTrue(TEXT("reports the nested unresolved selector, two structs deep"),
				NestedDiag.ContainsByPredicate([&EqsTask](const FString& Line)
				{
					return Line.StartsWith(EqsTask) && Line.Contains(TEXT("NoSuchNestedKey"))
						&& Line.Contains(TEXT("QueryConfig"));
				}));
		}
	}

	// --- Hazard #2: FValueOrBBKey_Float, an ordinary-looking numeric property that is also
	//     blackboard-bindable and carries the same silent-reset hazard as a key selector. -------------
	if (UObject* WaitInstance = VibeBTFindLiveInstance(BTPath, WaitA))
	{
		FStructProperty* WaitTimeProp = CastField<FStructProperty>(
			WaitInstance->GetClass()->FindPropertyByName(TEXT("WaitTime")));
		FValueOrBBKey_Float* WaitTimeKey = WaitTimeProp
			? WaitTimeProp->ContainerPtrToValuePtr<FValueOrBBKey_Float>(WaitInstance)
			: nullptr;
		if (TestNotNull(TEXT("WaitTime reflected as FValueOrBBKey_Float"), WaitTimeKey))
		{
			WaitTimeKey->SetKey(TEXT("NoSuchValueKey"));
			const TArray<FString> ValueKeyDiag = UBehaviorTreeService::ValidateTree(BTPath);
			TestTrue(TEXT("reports the unresolved FValueOrBBKey_Float binding"),
				ValueKeyDiag.ContainsByPredicate([&WaitA](const FString& Line)
					{ return Line.StartsWith(WaitA) && Line.Contains(TEXT("NoSuchValueKey")); }));
			WaitTimeKey->SetKey(NAME_None);
		}
	}

	// --- Defect: a node disconnected from the root. ----------------------------------------------------
	{
		UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
		UBehaviorTreeGraph* Graph = Tree ? Cast<UBehaviorTreeGraph>(Tree->BTGraph) : nullptr;
		if (TestNotNull(TEXT("graph loaded for orphan case"), Graph) && Tree)
		{
			UBehaviorTreeGraphNode_Composite* Orphan =
				NewObject<UBehaviorTreeGraphNode_Composite>(Graph, NAME_None, RF_Transactional);
			Orphan->CreateNewGuid();
			Orphan->AllocateDefaultPins();
			Orphan->NodeInstance = NewObject<UBTComposite_Sequence>(Tree, NAME_None, RF_Transactional);
			Graph->AddNode(Orphan, false, false);
			// Deliberately never linked to anything.

			const TArray<FString> OrphanDiag = UBehaviorTreeService::ValidateTree(BTPath);
			const FString OrphanTitle = Orphan->GetNodeTitle(ENodeTitleType::ListView).ToString();
			TestTrue(TEXT("reports the orphaned node"), OrphanDiag.ContainsByPredicate(
				[&OrphanTitle](const FString& Line)
				{ return Line.Contains(OrphanTitle) && Line.Contains(TEXT("orphaned")); }));

			Graph->RemoveNode(Orphan);
		}
	}

	// --- Defect: decorators attached to the graph's Root node, which never run. -----------------------
	{
		UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
		UBehaviorTreeGraph* Graph = Tree ? Cast<UBehaviorTreeGraph>(Tree->BTGraph) : nullptr;
		UBehaviorTreeGraphNode_Root* RootNode = Graph ? VibeBT::FindRootGraphNode(Graph) : nullptr;
		if (TestNotNull(TEXT("root resolved for decorator-on-root case"), RootNode) && Tree)
		{
			UBehaviorTreeGraphNode_Decorator* DecOnRoot =
				NewObject<UBehaviorTreeGraphNode_Decorator>(Graph, NAME_None, RF_Transactional);
			DecOnRoot->CreateNewGuid();
			DecOnRoot->NodeInstance = NewObject<UBTDecorator_Blackboard>(Tree, NAME_None, RF_Transactional);
			RootNode->Decorators.Add(DecOnRoot);

			const TArray<FString> RootDecoratorDiag = UBehaviorTreeService::ValidateTree(BTPath);
			TestTrue(TEXT("reports the decorator on Root"), RootDecoratorDiag.ContainsByPredicate(
				[](const FString& Line)
				{ return Line.StartsWith(TEXT("Root")) && Line.Contains(TEXT("decorator")); }));

			RootNode->Decorators.Reset();
		}
	}

	// --- Hazard #4: an injected node must never be reported, even carrying something that would trip
	//     a check on an ordinary node. Reuses the RunBehavior injection pattern from
	//     SubNodes.Injected: a subtree whose top-composite decorator is bound to a key name that does
	//     not exist anywhere (the subtree has no blackboard at all), injected onto a RunBehavior task
	//     in a separate main tree. ---------------------------------------------------------------------
	{
		const FString InjSubPath = FString(kBTTestDir) / TEXT("BT_ValidateInjectedSub");
		const FString InjMainPath = FString(kBTTestDir) / TEXT("BT_ValidateInjectedMain");
		FScopedFixtureReset ResetInjSub(InjSubPath);
		FScopedFixtureReset ResetInjMain(InjMainPath);

		TestEqual(TEXT("injected-case subtree created"),
			UBehaviorTreeService::CreateBehaviorTree(InjSubPath, FString()), FString());
		const FString InjSubSel = UBehaviorTreeService::AddNode(
			InjSubPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
		const FString InjSubDec = UBehaviorTreeService::AddDecorator(
			InjSubPath, InjSubSel, TEXT("BTDecorator_Blackboard"), -1);
		TestFalse(TEXT("injected-case subtree selector added"), InjSubSel.StartsWith(TEXT("ERROR")));
		TestFalse(TEXT("injected-case subtree decorator added"), InjSubDec.StartsWith(TEXT("ERROR")));

		if (FBlackboardKeySelector* SubSelector =
			VibeBTFindLiveSelector(InjSubPath, InjSubDec, TEXT("BlackboardKey")))
		{
			SubSelector->SelectedKeyName = TEXT("UnresolvedInjectedKey");
		}

		UBehaviorTree* SubTree = LoadObject<UBehaviorTree>(nullptr, *InjSubPath);
		TestEqual(TEXT("the decorator became a root-level decorator of the subtree"),
			SubTree ? SubTree->RootDecorators.Num() : -1, 1);

		TestEqual(TEXT("injected-case main tree created"),
			UBehaviorTreeService::CreateBehaviorTree(InjMainPath, FString()), FString());
		const FString InjMainSel = UBehaviorTreeService::AddNode(
			InjMainPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
		const FString InjRunSub = UBehaviorTreeService::AddNode(
			InjMainPath, InjMainSel, TEXT("BTTask_RunBehavior"), -1);
		TestFalse(TEXT("injected-case RunBehavior task added"), InjRunSub.StartsWith(TEXT("ERROR")));

		UBehaviorTree* MainTree = LoadObject<UBehaviorTree>(nullptr, *InjMainPath);
		UBehaviorTreeGraph* MainGraph = MainTree ? Cast<UBehaviorTreeGraph>(MainTree->BTGraph) : nullptr;
		UBehaviorTreeGraphNode* RunNode =
			MainGraph ? VibeBT::ResolveNodePath(MainGraph, InjRunSub) : nullptr;
		if (TestNotNull(TEXT("the RunBehavior graph node resolves"), RunNode) && SubTree)
		{
			FObjectProperty* AssetProperty = CastField<FObjectProperty>(
				UBTTask_RunBehavior::StaticClass()->FindPropertyByName(TEXT("BehaviorAsset")));
			if (TestNotNull(TEXT("BehaviorAsset property reached"), AssetProperty))
			{
				AssetProperty->SetObjectPropertyValue(
					AssetProperty->ContainerPtrToValuePtr<void>(ToRawPtr(RunNode->NodeInstance)), SubTree);
				TestTrue(TEXT("the subtree's root decorator was injected"),
					MainGraph->UpdateInjectedNodes());

				const TArray<FString> InjectedDiag = UBehaviorTreeService::ValidateTree(InjMainPath);
				TestFalse(TEXT("the injected node's unresolved key is NOT reported"),
					InjectedDiag.ContainsByPredicate([](const FString& Line)
						{ return Line.Contains(TEXT("UnresolvedInjectedKey")); }));
			}
		}
	}

	return true;
}

// The sparse-graph shape — graph root feeding nothing, runtime tree populated — is the one asset
// every mutator on this service refuses to write. ValidateTree has to say so: two tools in the same
// service giving opposite answers about the same asset is worse than either answer alone, because
// the diagnostic is the one a caller reaches for when a write is refused.
//
// Asserted as an agreement between the two, not as a message match: the same predicate
// (VibeBT::GraphWouldRebuildARuntimeTree) decides both, so the test is that a refused write and a
// clean bill of health can never coexist. The healthy direction is covered twice — here, before the
// graph is made sparse, and by the "a healthy tree validates clean" assertion above, which would
// break outright if this diagnostic misfired.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTValidateSparseGraphTest,
	"VibeUE.BehaviorTree.Validate.SparseGraph", kBTTestFlags)
bool FVibeBTValidateSparseGraphTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_ValidateSparseTest");
	FScopedFixtureReset ResetBT(BTPath);

	FPopulatedTree Fixture;
	if (!BuildPopulatedTree(*this, BTPath, Fixture))
	{
		return false;
	}

	auto FindRepairDiagnostic = [](const TArray<FString>& Diagnostics)
	{
		return Diagnostics.FindByPredicate([](const FString& Line)
			{ return Line.Contains(TEXT("RepairGraphFromRuntimeTree")); });
	};

	// Before: the graph feeds a composite, so the write is allowed and there is nothing to report.
	// (The fixture's lone childless Sequence does draw the composite-has-no-children diagnostic;
	// what must be absent is specifically the repair one.)
	const TArray<FString> HealthyDiag = UBehaviorTreeService::ValidateTree(BTPath);
	TestNull(TEXT("a graph that would rebuild its runtime tree is not reported as damaged"),
		FindRepairDiagnostic(HealthyDiag));

	MakeGraphSparse(Fixture);
	TestNotNull(TEXT("the runtime tree is still populated"), ToRawPtr(Fixture.Tree->RootNode));
	TestEqual(TEXT("...behind a graph root that feeds nothing"), Fixture.RootOut->LinkedTo.Num(), 0);

	// The write half of the disagreement: every mutator refuses this asset.
	const FString WriteError = UBehaviorTreeService::CompileAndSave(BTPath);
	TestTrue(TEXT("the write is refused"), !WriteError.IsEmpty());
	TestTrue(TEXT("...and the refusal points at RepairGraphFromRuntimeTree"),
		WriteError.Contains(TEXT("RepairGraphFromRuntimeTree")));

	// The read half, which used to say "healthy".
	const TArray<FString> SparseDiag = UBehaviorTreeService::ValidateTree(BTPath);
	AddInfo(FString::Printf(TEXT("ValidateTree on the sparse asset returned %d line(s): %s"),
		SparseDiag.Num(), *FString::Join(SparseDiag, TEXT(" | "))));
	const FString* Reported = FindRepairDiagnostic(SparseDiag);
	if (TestNotNull(TEXT("ValidateTree reports the asset the mutators refuse"), Reported))
	{
		TestTrue(TEXT("the diagnostic names the asset"),
			Reported->Contains(TEXT("BT_ValidateSparseTest")));
		TestTrue(TEXT("...and says what it is protecting"),
			Reported->Contains(TEXT("runtime")));
	}

	// The runtime tree the whole guard exists for survived being diagnosed — ValidateTree is
	// read-only, and running it on this shape must not be what destroys it.
	TestEqual(TEXT("validating changed nothing"), ToRawPtr(Fixture.Tree->RootNode),
		static_cast<UBTCompositeNode*>(Fixture.Sequence));

	return true;
}

// ================================================================================================
//  BuildTree — the batch layer.
// ================================================================================================

/**
 * Every node under Node — children, decorators and services, at every depth — but not Node itself.
 * The count BuildTree's Nodes array has to match when it skipped nothing.
 */
static int32 VibeBTCountNodes(const TSharedPtr<FJsonObject>& Node)
{
	int32 Count = 0;
	if (!Node.IsValid())
	{
		return Count;
	}
	for (const TCHAR* Field : { TEXT("decorators"), TEXT("services"), TEXT("children") })
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!Node->TryGetArrayField(Field, Array))
		{
			continue;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			Count += 1 + VibeBTCountNodes(Value->AsObject());
		}
	}
	return Count;
}

/**
 * Deep comparison of two GetTree dumps, ignoring "guid" and NOTHING else — "path", "class", "name",
 * "bInjected", "bHasCompositeDecorator", the whole "properties" map (in both directions, so a
 * property the rebuild invented is a mismatch too) and the length and order of all three node arrays.
 *
 * The guid is the one field that legitimately differs: the rebuilt nodes are new graph nodes. Paths
 * are NOT exempt — they are structural, so an exact rebuild reproduces them exactly, and a rebuild
 * that put a SimpleParallel's background branch in the main-task slot would not.
 */
static bool VibeBTNodesMatch(const TSharedPtr<FJsonObject>& Expected,
	const TSharedPtr<FJsonObject>& Actual, const FString& Where, FString& OutMismatch)
{
	if (!Expected.IsValid() || !Actual.IsValid())
	{
		OutMismatch = FString::Printf(TEXT("%s: one side is not a node object"), *Where);
		return false;
	}

	for (const TCHAR* Field : { TEXT("path"), TEXT("class"), TEXT("name") })
	{
		FString Left;
		FString Right;
		Expected->TryGetStringField(Field, Left);
		Actual->TryGetStringField(Field, Right);
		if (Left != Right)
		{
			OutMismatch = FString::Printf(TEXT("%s: \"%s\" is '%s', expected '%s'"),
				*Where, Field, *Right, *Left);
			return false;
		}
	}

	for (const TCHAR* Field : { TEXT("bInjected"), TEXT("bHasCompositeDecorator") })
	{
		bool bLeft = false;
		bool bRight = false;
		Expected->TryGetBoolField(Field, bLeft);
		Actual->TryGetBoolField(Field, bRight);
		if (bLeft != bRight)
		{
			OutMismatch = FString::Printf(TEXT("%s: \"%s\" is %s, expected %s"), *Where, Field,
				bRight ? TEXT("true") : TEXT("false"), bLeft ? TEXT("true") : TEXT("false"));
			return false;
		}
	}

	{
		const TSharedPtr<FJsonObject>* LeftProps = nullptr;
		const TSharedPtr<FJsonObject>* RightProps = nullptr;
		Expected->TryGetObjectField(TEXT("properties"), LeftProps);
		Actual->TryGetObjectField(TEXT("properties"), RightProps);

		// Collected by hand: FJsonObject's key type is UE::FSharedString in this engine version, not
		// FString, so TMap::GetKeys deduces nothing for a TArray<FString>.
		TSet<FString> Names;
		if (LeftProps && LeftProps->IsValid())
		{
			for (const auto& Pair : (*LeftProps)->Values)
			{
				Names.Add(FString(*Pair.Key));
			}
		}
		if (RightProps && RightProps->IsValid())
		{
			for (const auto& Pair : (*RightProps)->Values)
			{
				Names.Add(FString(*Pair.Key));
			}
		}
		for (const FString& Name : Names)
		{
			FString Left;
			FString Right;
			if (LeftProps && LeftProps->IsValid())
			{
				(*LeftProps)->TryGetStringField(Name, Left);
			}
			if (RightProps && RightProps->IsValid())
			{
				(*RightProps)->TryGetStringField(Name, Right);
			}
			if (Left != Right)
			{
				OutMismatch = FString::Printf(TEXT("%s: property %s is '%s', expected '%s'"),
					*Where, *Name, *Right, *Left);
				return false;
			}
		}
	}

	for (const TCHAR* Field : { TEXT("decorators"), TEXT("services"), TEXT("children") })
	{
		const TArray<TSharedPtr<FJsonValue>>* Left = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Right = nullptr;
		Expected->TryGetArrayField(Field, Left);
		Actual->TryGetArrayField(Field, Right);
		const int32 LeftNum = Left ? Left->Num() : 0;
		const int32 RightNum = Right ? Right->Num() : 0;
		if (LeftNum != RightNum)
		{
			OutMismatch = FString::Printf(TEXT("%s: %d \"%s\", expected %d"),
				*Where, RightNum, Field, LeftNum);
			return false;
		}
		for (int32 Index = 0; Index < LeftNum; ++Index)
		{
			if (!VibeBTNodesMatch((*Left)[Index]->AsObject(), (*Right)[Index]->AsObject(),
				FString::Printf(TEXT("%s/%s[%d]"), *Where, Field, Index), OutMismatch))
			{
				return false;
			}
		}
	}

	return true;
}

static FString VibeBTSerialiseJson(const TSharedPtr<FJsonObject>& Object)
{
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	return Out;
}

/**
 * Pull a tree's RunBehavior subtrees' root decorators in as injected nodes, and say whether anything
 * was injected.
 *
 * This is what UBehaviorTreeGraph::Initialize() runs when a human opens the asset
 * (BehaviorTreeGraph.cpp:203-206). UpdateAsset — and therefore every commit this service makes — does
 * NOT do it, so a tree whose RunBehavior task was just pointed at a subtree carries the injected
 * copies only after this. Test scaffolding on both sides of the round trip, not the code under test.
 */
static bool VibeBTUpdateInjectedNodes(const FString& AssetPath)
{
	UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *AssetPath);
	UBehaviorTreeGraph* Graph = Tree ? Cast<UBehaviorTreeGraph>(Tree->BTGraph) : nullptr;
	return Graph && Graph->UpdateInjectedNodes();
}

/** How many entries of a build failed. */
static int32 VibeBTFailedNodes(const FBTBuildResult& Result)
{
	int32 Failed = 0;
	for (const FBTBuildNodeResult& Entry : Result.Nodes)
	{
		Failed += Entry.bSuccess ? 0 : 1;
	}
	return Failed;
}

/**
 * BuildTree, end to end.
 *
 * The core assertion is the round trip: a tree built with the primitives, dumped with GetTree, and
 * rebuilt from that dump into a second asset must produce a byte-identical dump apart from the GUIDs.
 * That single comparison covers every field GetTree emits — including the two shapes this task was
 * warned about, a SimpleParallel whose compacted "children" hides which fixed slot each child is in,
 * and a struct element inside an array property, whose zero-valued members the engine omits on
 * export (FArrayProperty::ExportTextInnerItem forces a fresh StructDefaults, PropertyArray.cpp:1055).
 *
 * The rest of the test is about what happens when it cannot do that: an unresolvable class, a target
 * that already has a tree, and a target whose top-level node cannot be replaced at all.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTBuildRoundTripTest,
	"VibeUE.BehaviorTree.Build.RoundTrip", kBTTestFlags)
bool FVibeBTBuildRoundTripTest::RunTest(const FString&)
{
	const FString BBPath = FString(kBTTestDir) / TEXT("BB_BuildTest");
	const FString SubPath = FString(kBTTestDir) / TEXT("BT_BuildSub");
	const FString SrcPath = FString(kBTTestDir) / TEXT("BT_BuildSrc");
	const FString DstPath = FString(kBTTestDir) / TEXT("BT_BuildDst");
	const FString FreshPath = FString(kBTTestDir) / TEXT("BT_BuildFresh");
	const FString BadPath = FString(kBTTestDir) / TEXT("BT_BuildBad");
	const FString OverPath = FString(kBTTestDir) / TEXT("BT_BuildOver");
	const FString WrongPath = FString(kBTTestDir) / TEXT("BT_BuildWrongTop");
	const FString KeptPath = FString(kBTTestDir) / TEXT("BT_BuildKept");
	const FString OtherBBPath = FString(kBTTestDir) / TEXT("BB_BuildOther");
	const FString OtherPath = FString(kBTTestDir) / TEXT("BT_BuildOtherBoard");
	FScopedFixtureReset ResetOtherBB(OtherBBPath);
	FScopedFixtureReset ResetOtherBoard(OtherPath);
	FScopedFixtureReset ResetBB(BBPath);
	FScopedFixtureReset ResetSub(SubPath);
	FScopedFixtureReset ResetSrc(SrcPath);
	FScopedFixtureReset ResetDst(DstPath);
	FScopedFixtureReset ResetFresh(FreshPath);
	FScopedFixtureReset ResetBad(BadPath);
	FScopedFixtureReset ResetOver(OverPath);
	FScopedFixtureReset ResetWrong(WrongPath);
	FScopedFixtureReset ResetKept(KeptPath);

	// --- The blackboard. -------------------------------------------------------------------------
	// The key set is not decoration: every FBlackboardKeySelector on the nodes below is resolved
	// against this board at commit time, and InitSelection logs a LogBehaviorTree warning — which
	// fails the run — when a selector's type filter matches no key at all (BehaviorTreeTypes.cpp:556).
	TestTrue(TEXT("blackboard created"), UBlackboardService::CreateBlackboard(BBPath, FString()));
	TestEqual(TEXT("Alpha (Bool) added"),
		UBlackboardService::AddBlackboardKey(BBPath, TEXT("Alpha"), TEXT("Bool"), false), FString());
	TestEqual(TEXT("TargetActor (Object) added"),
		UBlackboardService::AddBlackboardKey(BBPath, TEXT("TargetActor"), TEXT("Object"), false),
		FString());
	TestEqual(TEXT("TargetActor is an Actor key"),
		UBlackboardService::SetBlackboardKeyObjectClass(BBPath, TEXT("TargetActor"),
			TEXT("/Script/Engine.Actor")), FString());
	TestEqual(TEXT("WaitSeconds (Float) added"),
		UBlackboardService::AddBlackboardKey(BBPath, TEXT("WaitSeconds"), TEXT("Float"), false),
		FString());

	// --- The source tree, built with the primitives. ----------------------------------------------
	//
	// Deliberately covers everything the JSON has to carry: nested composites, two decorators in a
	// specific order on one node, a service, a bound key selector on a sub-node and on a task, an
	// ordinary struct property, a node with an explicit NodeName (whose path segment is that name),
	// and TWO SimpleParallels — one with both slots filled and one with only its background branch,
	// which is the case where "children" array position is NOT the child slot.
	TestEqual(TEXT("source tree created"),
		UBehaviorTreeService::CreateBehaviorTree(SrcPath, BBPath), FString());

	const FString Sel = UBehaviorTreeService::AddNode(
		SrcPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	const FString Seq = UBehaviorTreeService::AddNode(
		SrcPath, Sel, TEXT("BTComposite_Sequence"), -1);
	const FString WaitA = UBehaviorTreeService::AddNode(SrcPath, Seq, TEXT("BTTask_Wait"), -1);
	const FString Move = UBehaviorTreeService::AddNode(SrcPath, Seq, TEXT("BTTask_MoveTo"), -1);
	const FString Eqs = UBehaviorTreeService::AddNode(SrcPath, Seq, TEXT("BTTask_RunEQSQuery"), -1);
	const FString Par = UBehaviorTreeService::AddNode(
		SrcPath, Sel, TEXT("BTComposite_SimpleParallel"), -1);
	// Slot 1 first, then slot 0: the background slot is the one the engine refills with a Wait on
	// every save, so authoring it is a replace.
	const FString ParBg = UBehaviorTreeService::AddNode(
		SrcPath, Par, TEXT("BTComposite_Sequence"), 1);
	const FString ParBgWait = UBehaviorTreeService::AddNode(SrcPath, ParBg, TEXT("BTTask_Wait"), -1);
	const FString ParMain = UBehaviorTreeService::AddNode(SrcPath, Par, TEXT("BTTask_MoveTo"), 0);
	// The second parallel keeps its main-task slot EMPTY, so GetTree reports exactly one child —
	// sitting at array position 0 while occupying fixed slot 1.
	const FString Lone = UBehaviorTreeService::AddNode(
		SrcPath, Sel, TEXT("BTComposite_SimpleParallel"), -1);
	const FString LoneBg = UBehaviorTreeService::AddNode(
		SrcPath, Lone, TEXT("BTComposite_Sequence"), 1);
	const FString LoneBgWait = UBehaviorTreeService::AddNode(SrcPath, LoneBg, TEXT("BTTask_Wait"), -1);

	// A RunBehavior task pointing at a subtree whose top composite carries a decorator: that decorator
	// is INJECTED into this tree, read-only, and must be skipped rather than replayed. The engine
	// re-injects on every UpdateAsset (BehaviorTreeGraph.cpp:205), so a rebuild that skips it and
	// replays the RunBehavior task's BehaviorAsset gets the injected copy back for free — while a
	// rebuild that replayed it would attach a SECOND, authored decorator alongside the injected one.
	TestEqual(TEXT("subtree created"),
		UBehaviorTreeService::CreateBehaviorTree(SubPath, BBPath), FString());
	const FString SubSel = UBehaviorTreeService::AddNode(
		SubPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	TestFalse(TEXT("subtree decorator added"),
		UBehaviorTreeService::AddDecorator(SubPath, SubSel, TEXT("BTDecorator_ForceSuccess"), -1)
			.StartsWith(TEXT("ERROR")));
	const FString RunSub = UBehaviorTreeService::AddNode(
		SrcPath, Sel, TEXT("BTTask_RunBehavior"), -1);
	const FBTPropertySetResult SubAssetWrite = UBehaviorTreeService::SetNodePropertyValue(
		SrcPath, RunSub, TEXT("BehaviorAsset"),
		FString::Printf(TEXT("%s.%s"), *SubPath, *FPaths::GetBaseFilename(SubPath)));
	TestTrue(TEXT("the RunBehavior task points at the subtree"), SubAssetWrite.bSuccess);

	const FString Dec1 = UBehaviorTreeService::AddDecorator(
		SrcPath, Seq, TEXT("BTDecorator_Blackboard"), -1);
	const FString Dec2 = UBehaviorTreeService::AddDecorator(
		SrcPath, Seq, TEXT("BTDecorator_ForceSuccess"), -1);
	const FString Svc = UBehaviorTreeService::AddService(
		SrcPath, Sel, TEXT("BTService_DefaultFocus"), -1);

	// Bailing out rather than continuing: every assertion below is about the dump of this tree, and a
	// suite of "no node at path" failures would say nothing about what actually broke.
	const TArray<FString> Built = { Sel, Seq, WaitA, Move, Eqs, Par, ParBg, ParBgWait, ParMain, Lone,
		LoneBg, LoneBgWait, SubSel, RunSub, Dec1, Dec2, Svc };
	for (const FString& Path : Built)
	{
		if (Path.StartsWith(TEXT("ERROR")))
		{
			AddError(FString::Printf(TEXT("fixture build failed: %s"), *Path));
			return false;
		}
	}

	TestTrue(TEXT("an ordinary struct property is set"),
		UBehaviorTreeService::SetNodePropertyValue(
			SrcPath, WaitA, TEXT("WaitTime"), TEXT("(DefaultValue=3.500000)")).bSuccess);
	TestTrue(TEXT("a key selector on a sub-node is bound"),
		UBehaviorTreeService::SetNodeBlackboardKey(
			SrcPath, Dec1, TEXT("BlackboardKey"), TEXT("Alpha")).bSuccess);
	TestTrue(TEXT("a key selector on a task is bound"),
		UBehaviorTreeService::SetNodeBlackboardKey(
			SrcPath, Move, TEXT("BlackboardKey"), TEXT("TargetActor")).bSuccess);

	// A STRUCT ELEMENT INSIDE AN ARRAY: FEQSParametrizedQueryExecutionRequest::QueryConfig is a
	// TArray<FAIDynamicParam>, and every member of that element left at the struct default is omitted
	// by the exporter whatever delta it is given (PropertyArray.cpp:1055-1102). The round-trip
	// comparison below is what says whether that omission survives a rebuild.
	const FBTPropertySetResult EqsWrite = UBehaviorTreeService::SetNodePropertyValue(
		SrcPath, Eqs, TEXT("EQSRequest"), TEXT("(QueryConfig=((ParamName=\"BuildTreeParam\")))"));
	AddInfo(FString::Printf(TEXT("EQSRequest after write: '%s' (error: '%s')"),
		*EqsWrite.ValueAfterWrite, *EqsWrite.Error));
	TestTrue(TEXT("an array-of-struct property is set"), EqsWrite.bSuccess);
	TestTrue(TEXT("...and its element member survived the commit"),
		EqsWrite.ValueAfterWrite.Contains(TEXT("BuildTreeParam")));

	// Renamed LAST: the name IS the node's path segment, so this invalidates Seq and every path
	// through it. BuildTree replays it as the NodeName property rather than through SetNodeName —
	// "name" in the dump is the node TITLE, which for an unnamed node is its class description.
	TestEqual(TEXT("a node is renamed"),
		UBehaviorTreeService::SetNodeName(SrcPath, Seq, TEXT("GuardedBranch")), FString());

	TestTrue(TEXT("the subtree's root decorator was injected into the source"),
		VibeBTUpdateInjectedNodes(SrcPath));

	const FString SrcJson = UBehaviorTreeService::GetTree(SrcPath);
	const TSharedPtr<FJsonObject> SrcTree = ParseTree(SrcJson);
	if (!TestTrue(TEXT("the source dump parses"), SrcTree.IsValid()))
	{
		AddError(FString::Printf(TEXT("source dump: %s"), *SrcJson));
		return false;
	}
	const int32 SourceNodeCount = VibeBTCountNodes(SrcTree);
	AddInfo(FString::Printf(TEXT("source tree: %d nodes below the root"), SourceNodeCount));

	// The injected decorator is in the dump, and it is the engine's, not something authored here.
	const FString InjectedPath = RunSub + TEXT("/@decorator[0]");
	FBTNodeInfo InjectedInfo;
	TestTrue(TEXT("the injected decorator is addressable in the source"),
		UBehaviorTreeService::GetNodeInfo(SrcPath, InjectedPath, InjectedInfo));
	TestTrue(TEXT("...and is flagged as injected"), InjectedInfo.bInjected);
	TestTrue(TEXT("the dump carries the array-of-struct element"),
		SrcJson.Contains(TEXT("BuildTreeParam")));
	TestTrue(TEXT("the dump carries the renamed node's NodeName property"),
		SrcJson.Contains(TEXT("GuardedBranch")));

	// --- THE ROUND TRIP. -------------------------------------------------------------------------
	TestEqual(TEXT("destination created"),
		UBehaviorTreeService::CreateBehaviorTree(DstPath, BBPath), FString());
	const FBTBuildResult Rebuild = UBehaviorTreeService::BuildTree(DstPath, SrcJson, true);
	for (const FBTBuildNodeResult& Entry : Rebuild.Nodes)
	{
		if (!Entry.bSuccess)
		{
			AddError(FString::Printf(TEXT("BuildTree failed on '%s': %s"), *Entry.Path, *Entry.Error));
		}
	}
	TestTrue(TEXT("the rebuild succeeded"), Rebuild.bSuccess);
	TestEqual(TEXT("...with no overall error"), Rebuild.Error, FString());
	TestEqual(TEXT("...and one entry per node in the JSON"), Rebuild.Nodes.Num(), SourceNodeCount);

	// The injected decorator was skipped rather than replayed — and it is nevertheless present in the
	// rebuilt tree (the comparison below), because replaying the RunBehavior task's BehaviorAsset is
	// enough for the engine to inject it there too. A rebuild that replayed it instead would end up
	// with a second, authored copy alongside this one, and the comparison would say so.
	TestTrue(TEXT("the rebuilt tree injects the same decorator"), VibeBTUpdateInjectedNodes(DstPath));
	const FBTBuildNodeResult* InjectedEntry = Rebuild.Nodes.FindByPredicate(
		[&InjectedPath](const FBTBuildNodeResult& Entry) { return Entry.Path == InjectedPath; });
	if (TestNotNull(TEXT("the injected node has an entry of its own"), InjectedEntry))
	{
		TestTrue(TEXT("...reported as a success"), InjectedEntry->bSuccess);
		TestTrue(TEXT("...that says it was skipped"), InjectedEntry->Error.Contains(TEXT("skipped")));
	}

	const TSharedPtr<FJsonObject> DstTree = ParseTree(UBehaviorTreeService::GetTree(DstPath));
	FString Mismatch;
	if (!TestTrue(TEXT("the rebuilt tree matches the source, guids aside"),
		VibeBTNodesMatch(SrcTree, DstTree, TEXT("Root"), Mismatch)))
	{
		AddError(Mismatch);
	}

	// The comparison above ignores exactly one field, and that field really did change — otherwise it
	// would be asserting that two dumps of the same asset are equal.
	if (SrcTree->GetArrayField(TEXT("children")).Num() == 1
		&& DstTree.IsValid() && DstTree->GetArrayField(TEXT("children")).Num() == 1)
	{
		TestNotEqual(TEXT("the rebuild made new graph nodes"),
			SrcTree->GetArrayField(TEXT("children"))[0]->AsObject()->GetStringField(TEXT("guid")),
			DstTree->GetArrayField(TEXT("children"))[0]->AsObject()->GetStringField(TEXT("guid")));
	}

	// bReplaceExisting = false is not "always refuse": it is "refuse a tree that already has one".
	TestEqual(TEXT("a fresh asset created"),
		UBehaviorTreeService::CreateBehaviorTree(FreshPath, BBPath), FString());
	const FBTBuildResult FreshBuild = UBehaviorTreeService::BuildTree(FreshPath, SrcJson, false);
	TestTrue(TEXT("building into an empty tree does not need bReplaceExisting"), FreshBuild.bSuccess);
	VibeBTUpdateInjectedNodes(FreshPath);
	FString FreshMismatch;
	if (!TestTrue(TEXT("...and produces the same tree"),
		VibeBTNodesMatch(SrcTree, ParseTree(UBehaviorTreeService::GetTree(FreshPath)), TEXT("Root"),
			FreshMismatch)))
	{
		AddError(FreshMismatch);
	}

	// --- A node whose class does not resolve. ------------------------------------------------------
	// Broken on the parallel's BACKGROUND branch, which has a child of its own, so this covers both
	// halves of the contract: the failing node is named, and its subtree is reported as not built
	// rather than silently missing.
	const TSharedPtr<FJsonObject> BrokenTree = ParseTree(SrcJson);
	const TSharedPtr<FJsonObject> BrokenNode =
		BrokenTree.IsValid() ? FindNodeByPath(BrokenTree, ParBg) : nullptr;
	if (!TestTrue(TEXT("the node to break was found in the dump"), BrokenNode.IsValid()))
	{
		return false;
	}
	BrokenNode->SetStringField(TEXT("class"), TEXT("BTComposite_Nonexistent"));
	const FString BrokenJson = VibeBTSerialiseJson(BrokenTree);

	TestEqual(TEXT("bad-build target created"),
		UBehaviorTreeService::CreateBehaviorTree(BadPath, BBPath), FString());
	const FBTBuildResult BadBuild = UBehaviorTreeService::BuildTree(BadPath, BrokenJson, true);
	TestFalse(TEXT("an unresolvable class fails the build"), BadBuild.bSuccess);
	TestTrue(TEXT("...and the overall error points at the per-node results"),
		BadBuild.Error.Contains(TEXT("nodes failed")));
	TestEqual(TEXT("...with an entry for every node in the JSON"), BadBuild.Nodes.Num(),
		SourceNodeCount);

	const FBTBuildNodeResult* FailedEntry = BadBuild.Nodes.FindByPredicate(
		[&ParBg](const FBTBuildNodeResult& Entry) { return Entry.Path == ParBg; });
	if (TestNotNull(TEXT("the failing node is named in Nodes"), FailedEntry))
	{
		TestFalse(TEXT("...as a failure"), FailedEntry->bSuccess);
		TestTrue(TEXT("...naming the class that did not resolve"),
			FailedEntry->Error.Contains(TEXT("BTComposite_Nonexistent")));
	}
	const FBTBuildNodeResult* OrphanEntry = BadBuild.Nodes.FindByPredicate(
		[&ParBgWait](const FBTBuildNodeResult& Entry) { return Entry.Path == ParBgWait; });
	if (TestNotNull(TEXT("its child is reported too"), OrphanEntry))
	{
		TestFalse(TEXT("...as not built"), OrphanEntry->bSuccess);
		TestTrue(TEXT("...because its parent was not created"),
			OrphanEntry->Error.Contains(TEXT("its parent")));
	}

	// The point of the per-node list: the rest of the tree still reports its own outcomes, and was
	// really built.
	TestEqual(TEXT("exactly the broken node and its child failed"), VibeBTFailedNodes(BadBuild), 2);
	TestTrue(TEXT("everything else succeeded"), BadBuild.Nodes.Num() - 2 > 5);
	FBTNodeInfo BadSelInfo;
	TestTrue(TEXT("the partially built asset is readable"),
		UBehaviorTreeService::GetNodeInfo(BadPath, Sel, BadSelInfo));
	TestEqual(TEXT("...and carries the branches that did build"), BadSelInfo.ChildCount, 4);

	// ...and the comparison used above is not vacuous: this tree is genuinely different.
	FString BadMismatch;
	TestFalse(TEXT("the tree comparison rejects a tree that differs"),
		VibeBTNodesMatch(SrcTree, ParseTree(UBehaviorTreeService::GetTree(BadPath)), TEXT("Root"),
			BadMismatch));
	AddInfo(FString::Printf(TEXT("first difference in the partially built tree: %s"), *BadMismatch));

	// --- A target whose blackboard is not the source's. --------------------------------------------
	// GetTree does not report the tree's blackboard, so key bindings are resolved against whatever the
	// TARGET has. Every selector goes through SetNodeBlackboardKey, which checks the key against that
	// board and refuses before touching the node; routing them through the generic setter instead
	// would write the name and only find out at commit time, when UBTNode::PreSave rewrites it. The
	// assertion is therefore on the message, not merely on the failure.
	TestTrue(TEXT("a second blackboard created"),
		UBlackboardService::CreateBlackboard(OtherBBPath, FString()));
	// Keys of the right TYPES but the wrong names: a board with no key a selector's filter accepts
	// makes FBlackboardKeySelector::InitSelection log a warning at commit, which fails the run.
	TestEqual(TEXT("Other (Bool) added"),
		UBlackboardService::AddBlackboardKey(OtherBBPath, TEXT("Other"), TEXT("Bool"), false), FString());
	TestEqual(TEXT("Thing (Object) added"),
		UBlackboardService::AddBlackboardKey(OtherBBPath, TEXT("Thing"), TEXT("Object"), false),
		FString());
	TestEqual(TEXT("Thing is an Actor key"),
		UBlackboardService::SetBlackboardKeyObjectClass(OtherBBPath, TEXT("Thing"),
			TEXT("/Script/Engine.Actor")), FString());
	TestEqual(TEXT("Number (Float) added"),
		UBlackboardService::AddBlackboardKey(OtherBBPath, TEXT("Number"), TEXT("Float"), false),
		FString());
	TestEqual(TEXT("wrong-board target created"),
		UBehaviorTreeService::CreateBehaviorTree(OtherPath, OtherBBPath), FString());

	const FBTBuildResult OtherBoard = UBehaviorTreeService::BuildTree(OtherPath, SrcJson, true);
	TestFalse(TEXT("bindings against a board without those keys fail"), OtherBoard.bSuccess);
	int32 MissingKeyErrors = 0;
	for (const FBTBuildNodeResult& Entry : OtherBoard.Nodes)
	{
		if (!Entry.bSuccess)
		{
			AddInfo(FString::Printf(TEXT("wrong-board failure on '%s': %s"), *Entry.Path, *Entry.Error));
			MissingKeyErrors += Entry.Error.Contains(TEXT("has no key")) ? 1 : 0;
		}
	}
	// Both of the source's bound selectors — one on a decorator, one on a task — and each refused by
	// the key binder rather than by a post-commit read-back.
	TestEqual(TEXT("...naming the key and the board, on every failing node"), MissingKeyErrors,
		VibeBTFailedNodes(OtherBoard));
	TestTrue(TEXT("...on at least the two nodes that carry a binding"), MissingKeyErrors >= 2);

	// --- bReplaceExisting = false, on a tree that already has one. ---------------------------------
	const FString DstBefore = UBehaviorTreeService::GetTree(DstPath);
	const FBTBuildResult Refused = UBehaviorTreeService::BuildTree(DstPath, SrcJson, false);
	TestFalse(TEXT("a populated tree is refused without bReplaceExisting"), Refused.bSuccess);
	TestTrue(TEXT("...saying so"), Refused.Error.Contains(TEXT("bReplaceExisting is false")));
	TestEqual(TEXT("...having attempted no node at all"), Refused.Nodes.Num(), 0);
	// Guids included: an identical dump means nothing was rebuilt, not merely that it ended up the same.
	TestEqual(TEXT("...and having written nothing"), UBehaviorTreeService::GetTree(DstPath), DstBefore);

	// --- A top-level node that cannot be replaced. -------------------------------------------------
	// The tree's top-level node is the one node RemoveNode refuses (removing the root's only child
	// would leave no runtime tree), and there is no ReplaceNode — so a different class there is
	// refused BEFORE anything is cleared, rather than producing a Sequence-rooted hybrid carrying a
	// Selector's children.
	TestEqual(TEXT("wrong-top target created"),
		UBehaviorTreeService::CreateBehaviorTree(WrongPath, BBPath), FString());
	TestFalse(TEXT("...with a Sequence at the top"),
		UBehaviorTreeService::AddNode(WrongPath, TEXT("Root"), TEXT("BTComposite_Sequence"), -1)
			.StartsWith(TEXT("ERROR")));
	const FString WrongBefore = UBehaviorTreeService::GetTree(WrongPath);
	const FBTBuildResult WrongTop = UBehaviorTreeService::BuildTree(WrongPath, SrcJson, true);
	TestFalse(TEXT("a mismatched top-level node is refused"), WrongTop.bSuccess);
	TestTrue(TEXT("...naming both classes"),
		WrongTop.Error.Contains(TEXT("BTComposite_Sequence"))
			&& WrongTop.Error.Contains(TEXT("BTComposite_Selector")));
	TestEqual(TEXT("...before clearing anything"),
		UBehaviorTreeService::GetTree(WrongPath), WrongBefore);

	// --- Rebuilding over a matching tree. ----------------------------------------------------------
	// Same top-level class, so everything below it — children AND the top node's own decorators and
	// services — is cleared and rebuilt from the JSON.
	TestEqual(TEXT("overwrite target created"),
		UBehaviorTreeService::CreateBehaviorTree(OverPath, BBPath), FString());
	const FString OverSel = UBehaviorTreeService::AddNode(
		OverPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	const FString OverSeq = UBehaviorTreeService::AddNode(
		OverPath, OverSel, TEXT("BTComposite_Sequence"), -1);
	TestFalse(TEXT("stale subtree authored"),
		UBehaviorTreeService::AddNode(OverPath, OverSeq, TEXT("BTTask_Wait"), -1)
			.StartsWith(TEXT("ERROR")));
	TestFalse(TEXT("stale decorator authored"),
		UBehaviorTreeService::AddDecorator(OverPath, OverSel, TEXT("BTDecorator_ForceSuccess"), -1)
			.StartsWith(TEXT("ERROR")));
	TestFalse(TEXT("stale service authored"),
		UBehaviorTreeService::AddService(OverPath, OverSel, TEXT("BTService_DefaultFocus"), -1)
			.StartsWith(TEXT("ERROR")));

	const FBTBuildResult Over = UBehaviorTreeService::BuildTree(OverPath, SrcJson, true);
	for (const FBTBuildNodeResult& Entry : Over.Nodes)
	{
		if (!Entry.bSuccess)
		{
			AddError(FString::Printf(TEXT("rebuild over an existing tree failed on '%s': %s"),
				*Entry.Path, *Entry.Error));
		}
	}
	TestTrue(TEXT("rebuilding over a matching tree succeeds"), Over.bSuccess);
	VibeBTUpdateInjectedNodes(OverPath);
	FString OverMismatch;
	if (!TestTrue(TEXT("...and leaves nothing of what was there before"),
		VibeBTNodesMatch(SrcTree, ParseTree(UBehaviorTreeService::GetTree(OverPath)), TEXT("Root"),
			OverMismatch)))
	{
		AddError(OverMismatch);
	}

	// --- The one hybrid bReplaceExisting can produce, reported rather than hidden. -----------------
	// The retained top-level node keeps a property the JSON never mentions (here an explicit
	// NodeName), because that node is reused rather than rebuilt. The build says so on that node.
	TestEqual(TEXT("retained-node target created"),
		UBehaviorTreeService::CreateBehaviorTree(KeptPath, BBPath), FString());
	const FString KeptSel = UBehaviorTreeService::AddNode(
		KeptPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	TestEqual(TEXT("the top node carries a name of its own"),
		UBehaviorTreeService::SetNodeName(KeptPath, KeptSel, TEXT("Kept")), FString());

	const FBTBuildResult Kept = UBehaviorTreeService::BuildTree(KeptPath, SrcJson, true);
	TestFalse(TEXT("a leftover on the retained top node fails the build"), Kept.bSuccess);
	const FBTBuildNodeResult* KeptEntry = Kept.Nodes.FindByPredicate(
		[&Sel](const FBTBuildNodeResult& Entry) { return Entry.Path == Sel; });
	if (TestNotNull(TEXT("the top node has an entry"), KeptEntry))
	{
		TestFalse(TEXT("...which failed"), KeptEntry->bSuccess);
		TestTrue(TEXT("...saying it was retained rather than rebuilt"),
			KeptEntry->Error.Contains(TEXT("retained")));
		TestTrue(TEXT("...and naming the property that does not match"),
			KeptEntry->Error.Contains(TEXT("NodeName")));
	}
	TestEqual(TEXT("and nothing else failed"), VibeBTFailedNodes(Kept), 1);

	return true;
}

// A sub-node stays addressable when UAIGraphNode::ParentNode is null.
//
// That pointer is UPROPERTY(transient) (AIGraphNode.h:31-32), so it is NOT saved, and the only thing
// that repopulates it is UBehaviorTreeGraph::UpdateAsset()'s "parent chain" block
// (BehaviorTreeGraph.cpp:137-147) — which runs when the Behavior Tree editor opens an asset, or when
// this service commits one. On an asset loaded from disk and merely READ, it is therefore null on
// every decorator and every service.
//
// This is not hypothetical: it is what the project's own assets do. Reading
// /Game/Core/Controllers/AI/BT_Combat_HermitCrab through GetTree reported an empty "path" for all 32
// of its sub-nodes, and BT_Enemy for all 21 of its — addresses no caller could use, and the ones
// BuildTree quotes in its per-node results. RemoveSubNode was worse than useless on them: it read
// the same null pointer and answered "is not a decorator or service — it is a node in the tree".
//
// Nulling the pointer here reproduces the post-load state exactly, and is the only way to reach it
// from a test: a fixture built through this service has just been committed, so UpdateAsset has by
// construction set every one of these pointers.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTSubNodeOwnerWithoutParentPointerTest,
	"VibeUE.BehaviorTree.SubNodes.OwnerSurvivesTransientParent", kBTTestFlags)
bool FVibeBTSubNodeOwnerWithoutParentPointerTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_TransientParentTest");
	FScopedFixtureReset ResetBT(BTPath);

	TestEqual(TEXT("fixture tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, FString()), FString());
	const FString Sel = UBehaviorTreeService::AddNode(
		BTPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	const FString Seq = UBehaviorTreeService::AddNode(BTPath, Sel, TEXT("BTComposite_Sequence"), -1);
	const FString Dec = UBehaviorTreeService::AddDecorator(
		BTPath, Seq, TEXT("BTDecorator_ForceSuccess"), -1);
	const FString Svc = UBehaviorTreeService::AddService(
		BTPath, Sel, TEXT("BTService_DefaultFocus"), -1);
	TestEqual(TEXT("the decorator is an @decorator slot on the sequence"),
		Dec, Seq + TEXT("/@decorator[0]"));
	TestEqual(TEXT("the service is an @service slot on the selector"),
		Svc, Sel + TEXT("/@service[0]"));

	// --- Reproduce the post-load state: transient back pointers gone, serialised arrays intact. ----
	UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
	if (!TestNotNull(TEXT("fixture loads"), Tree))
	{
		return false;
	}
	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
	if (!TestNotNull(TEXT("fixture has a graph"), Graph))
	{
		return false;
	}
	int32 Cleared = 0;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(Node))
		{
			for (const TObjectPtr<UBehaviorTreeGraphNode>& Sub : BTNode->Decorators)
			{
				if (Sub) { Sub->ParentNode = nullptr; ++Cleared; }
			}
			for (const TObjectPtr<UBehaviorTreeGraphNode>& Sub : BTNode->Services)
			{
				if (Sub) { Sub->ParentNode = nullptr; ++Cleared; }
			}
		}
	}
	TestEqual(TEXT("both sub-nodes now have the null ParentNode a load would give them"), Cleared, 2);

	// --- GetTree still issues usable addresses. ---------------------------------------------------
	const TSharedPtr<FJsonObject> Dump = ParseTree(UBehaviorTreeService::GetTree(BTPath));
	const TSharedPtr<FJsonObject> SeqObject = FindNodeByPath(Dump, Seq);
	const TSharedPtr<FJsonObject> SelObject = FindNodeByPath(Dump, Sel);
	if (TestTrue(TEXT("the owners are still in the dump"),
		SeqObject.IsValid() && SelObject.IsValid()))
	{
		const TArray<TSharedPtr<FJsonValue>>& Decorators = SeqObject->GetArrayField(TEXT("decorators"));
		const TArray<TSharedPtr<FJsonValue>>& Services = SelObject->GetArrayField(TEXT("services"));
		if (TestEqual(TEXT("the decorator is still reported"), Decorators.Num(), 1)
			&& TestEqual(TEXT("the service is still reported"), Services.Num(), 1))
		{
			TestEqual(TEXT("the decorator's path is its slot, not empty"),
				Decorators[0]->AsObject()->GetStringField(TEXT("path")), Dec);
			TestEqual(TEXT("the service's path is its slot, not empty"),
				Services[0]->AsObject()->GetStringField(TEXT("path")), Svc);
		}
	}

	// --- And the address round-trips back to the same node. ---------------------------------------
	FBTNodeInfo DecInfo;
	if (TestTrue(TEXT("the decorator path still resolves"),
		UBehaviorTreeService::GetNodeInfo(BTPath, Dec, DecInfo)))
	{
		TestEqual(TEXT("...to the decorator"), DecInfo.ClassName,
			FString(TEXT("BTDecorator_ForceSuccess")));
		TestEqual(TEXT("...reporting the path it was addressed by"), DecInfo.Path, Dec);
	}

	// --- RemoveSubNode reads the same relation, so it must not deny the sub-node exists. -----------
	// Asserted through the service rather than by inspecting the graph: this is the call that
	// answered "it is a node in the tree" on every production decorator.
	TestEqual(TEXT("RemoveSubNode detaches it instead of misreporting it"),
		UBehaviorTreeService::RemoveSubNode(BTPath, Dec), FString());
	const TSharedPtr<FJsonObject> AfterSeq =
		FindNodeByPath(ParseTree(UBehaviorTreeService::GetTree(BTPath)), Seq);
	if (TestTrue(TEXT("the owner survives the removal"), AfterSeq.IsValid()))
	{
		TestEqual(TEXT("...carrying no decorator any more"),
			AfterSeq->GetArrayField(TEXT("decorators")).Num(), 0);
	}

	return true;
}

// =================================================================================================
//  Regression tests for the post-review fixes (fix/bt-service-review).
// =================================================================================================

// The NodeName mid-ApplyProperties path-staleness bug: BuildTree replayed NodeName alphabetically
// among ordinary properties, so a renamed node's later-sorting properties (WaitTime...) were
// written through a stale path — a loud failure at best, a silent write onto a same-titled
// sibling at worst. NodeName must now land LAST, and the whole dump — rename, WaitTime edit and
// comment — must round-trip exactly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTRenamedNodeRoundTripTest,
	"VibeUE.BehaviorTree.Build.RenamedNodeRoundTrip", kBTTestFlags)
bool FVibeBTRenamedNodeRoundTripTest::RunTest(const FString&)
{
	const FString SrcPath = FString(kBTTestDir) / TEXT("BT_RenamedRoundTripSrc");
	const FString DstPath = FString(kBTTestDir) / TEXT("BT_RenamedRoundTripDst");
	FScopedFixtureReset ResetSrc(SrcPath);
	FScopedFixtureReset ResetDst(DstPath);

	TestEqual(TEXT("source created"),
		UBehaviorTreeService::CreateBehaviorTree(SrcPath, FString()), FString());
	const FString Sel = UBehaviorTreeService::AddNode(SrcPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	const FString Wait = UBehaviorTreeService::AddNode(SrcPath, Sel, TEXT("BTTask_Wait"), -1);
	TestFalse(TEXT("wait added"), Wait.StartsWith(TEXT("ERROR")));

	// The exact shape that broke: a RENAME (NodeName sorts before W) plus an edited WaitTime.
	const FBTPropertySetResult TimeWrite = UBehaviorTreeService::SetNodePropertyValue(
		SrcPath, Wait, TEXT("WaitTime"), TEXT("(DefaultValue=3.5,Key=\"\")"));
	TestTrue(TEXT("WaitTime set on source"), TimeWrite.bSuccess);
	TestEqual(TEXT("wait renamed"),
		UBehaviorTreeService::SetNodeName(SrcPath, Wait, TEXT("Guard Wait")), FString());
	TestEqual(TEXT("wait commented"),
		UBehaviorTreeService::SetNodeComment(SrcPath, TEXT("Root/Selector/Guard Wait"),
			TEXT("why: stand watch")), FString());

	const FString Dump = UBehaviorTreeService::GetTree(SrcPath);
	TestEqual(TEXT("destination created"),
		UBehaviorTreeService::CreateBehaviorTree(DstPath, FString()), FString());
	const FBTBuildResult Build = UBehaviorTreeService::BuildTree(DstPath, Dump, false);
	for (const FBTBuildNodeResult& Node : Build.Nodes)
	{
		if (!Node.bSuccess)
		{
			AddInfo(FString::Printf(TEXT("node failed: %s | %s"), *Node.Path, *Node.Error));
		}
	}
	TestTrue(TEXT("a dump containing a renamed node with edited properties builds clean"),
		Build.bSuccess);

	// The renamed node exists at its renamed path, with the property and the comment intact.
	const FString DstWaitTime = UBehaviorTreeService::GetNodePropertyValue(
		DstPath, TEXT("Root/Selector/Guard Wait"), TEXT("WaitTime"));
	TestTrue(TEXT("WaitTime survived the round-trip onto the RENAMED node"),
		DstWaitTime.Contains(TEXT("3.5")));
	FBTNodeInfo DstInfo;
	if (TestTrue(TEXT("renamed node resolvable in the copy"),
		UBehaviorTreeService::GetNodeInfo(DstPath, TEXT("Root/Selector/Guard Wait"), DstInfo)))
	{
		TestEqual(TEXT("comment survived the round-trip"), DstInfo.Comment,
			FString(TEXT("why: stand watch")));
	}
	return true;
}

// MoveNode must apply the same SimpleParallel background-branch guard RemoveNode has: moving the
// background child away leaves a pin SpawnMissingNodesForParallel refills with a Wait inside the
// very commit the move saves through — success reported, unrequested node left behind.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTParallelBackgroundMoveGuardTest,
	"VibeUE.BehaviorTree.Structure.ParallelBackgroundMoveGuard", kBTTestFlags)
bool FVibeBTParallelBackgroundMoveGuardTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_ParallelMoveGuard");
	FScopedFixtureReset ResetBT(BTPath);

	TestEqual(TEXT("tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, FString()), FString());
	const FString Sel = UBehaviorTreeService::AddNode(BTPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	const FString Par = UBehaviorTreeService::AddNode(BTPath, Sel, TEXT("BTComposite_SimpleParallel"), -1);
	TestFalse(TEXT("parallel added"), Par.StartsWith(TEXT("ERROR")));
	// Replace the engine-spawned background Wait with a Sequence so the victim is unmistakable.
	const FString Background = UBehaviorTreeService::AddNode(BTPath, Par, TEXT("BTComposite_Sequence"), 1);
	TestFalse(TEXT("background branch replaced"), Background.StartsWith(TEXT("ERROR")));

	const FString MoveError = UBehaviorTreeService::MoveNode(BTPath, Background, Sel, -1);
	TestTrue(TEXT("moving the background branch away is refused"), !MoveError.IsEmpty());
	TestTrue(TEXT("...naming the mechanism"), MoveError.Contains(TEXT("background")));

	FBTNodeInfo BgInfo;
	if (TestTrue(TEXT("background still resolvable"),
		UBehaviorTreeService::GetNodeInfo(BTPath, Background, BgInfo)))
	{
		TestEqual(TEXT("...as the sequence, not a respawned Wait"), BgInfo.ClassName,
			FString(TEXT("BTComposite_Sequence")));
	}
	return true;
}

// UBTNode::NodeName through SetNodePropertyValue IS a rename, so the path-grammar refusals of
// SetNodeName must hold on that route too — otherwise "NodeName" = "a/b" renames a node into
// un-addressability through reflection (including BuildTree's replay of a details-panel-typed
// name).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTNodeNameGrammarViaPropertyTest,
	"VibeUE.BehaviorTree.Properties.NodeNameGrammarViaProperty", kBTTestFlags)
bool FVibeBTNodeNameGrammarViaPropertyTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_NameGrammarViaProperty");
	FScopedFixtureReset ResetBT(BTPath);

	TestEqual(TEXT("tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, FString()), FString());
	const FString Sel = UBehaviorTreeService::AddNode(BTPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	const FString Wait = UBehaviorTreeService::AddNode(BTPath, Sel, TEXT("BTTask_Wait"), -1);

	for (const TCHAR* Bad : { TEXT("a/b"), TEXT("@sneaky"), TEXT("Thing[3]") })
	{
		const FBTPropertySetResult Write = UBehaviorTreeService::SetNodePropertyValue(
			BTPath, Wait, TEXT("NodeName"), Bad);
		TestFalse(FString::Printf(TEXT("NodeName '%s' via the property route is refused"), Bad),
			Write.bSuccess);
	}
	// A legal name through the same route still works, so the grammar check is not over-broad.
	const FBTPropertySetResult GoodWrite = UBehaviorTreeService::SetNodePropertyValue(
		BTPath, Wait, TEXT("NodeName"), TEXT("Plain Name"));
	TestTrue(TEXT("a legal NodeName via the property route succeeds"), GoodWrite.bSuccess);

	FBTNodeInfo Info;
	TestTrue(TEXT("the node is addressable at its new name"),
		UBehaviorTreeService::GetNodeInfo(BTPath, TEXT("Root/Selector/Plain Name"), Info));
	return true;
}

// The RootNode==null arm of the discard guard. An asset whose top composite's class was deleted
// loads with RootNode ALREADY null while its graph root still links to the instance-less node;
// the pre-fix guard was gated on RootNode being non-null, waved the commit through, and the
// engine null-derefed inside CreateBTFromGraph (BehaviorTreeGraph.cpp:510-517) — an editor kill.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTCommitNullRootCrashGuardTest,
	"VibeUE.BehaviorTree.Asset.CommitNullRootCrashGuard", kBTTestFlags)
bool FVibeBTCommitNullRootCrashGuardTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_NullRootCrashGuard");
	FScopedFixtureReset ResetBT(BTPath);

	TestEqual(TEXT("tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, FString()), FString());
	const FString Sel = UBehaviorTreeService::AddNode(BTPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	TestFalse(TEXT("selector added"), Sel.StartsWith(TEXT("ERROR")));

	UBehaviorTree* Tree = LoadObject<UBehaviorTree>(nullptr, *BTPath);
	if (!TestNotNull(TEXT("tree loads"), Tree))
	{
		return false;
	}
	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
	UBehaviorTreeGraphNode* TopNode = VibeBT::ResolveNodePath(Graph, TEXT("Root/Selector"));
	if (!TestNotNull(TEXT("top node resolves"), TopNode))
	{
		return false;
	}

	// The deleted-class shape, installed by hand: instance gone, runtime tree gone.
	UObject* const SavedInstance = ToRawPtr(TopNode->NodeInstance);
	UBTCompositeNode* const SavedRoot = ToRawPtr(Tree->RootNode);
	TopNode->NodeInstance = nullptr;
	Tree->RootNode = nullptr;

	const FString CommitError = UBehaviorTreeService::CompileAndSave(BTPath);

	// Restore before asserting, so a failure here cannot leave the in-memory fixture corrupted
	// for the exit reset.
	TopNode->NodeInstance = SavedInstance;
	Tree->RootNode = SavedRoot;

	TestTrue(TEXT("committing a linked-but-instanceless root is refused, not crashed"),
		!CommitError.IsEmpty());
	TestTrue(TEXT("...and the refusal names the mechanism"),
		CommitError.Contains(TEXT("no composite instance")));
	return true;
}

// Input hardening: over-long names must come back as errors, never as the engine's fatal
// NAME_SIZE log; a graph deeper than MaxTreeDepth must be a refused layout, not a stack
// overflow; and a hostile BuildTree dump whose whole subtree is unbuildable must produce a
// bounded report.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBTInputBoundsTest,
	"VibeUE.BehaviorTree.Hardening.InputBounds", kBTTestFlags)
bool FVibeBTInputBoundsTest::RunTest(const FString&)
{
	const FString BTPath = FString(kBTTestDir) / TEXT("BT_InputBounds");
	FScopedFixtureReset ResetBT(BTPath);

	TestEqual(TEXT("tree created"),
		UBehaviorTreeService::CreateBehaviorTree(BTPath, FString()), FString());
	const FString Sel = UBehaviorTreeService::AddNode(BTPath, TEXT("Root"), TEXT("BTComposite_Selector"), -1);
	const FString Wait = UBehaviorTreeService::AddNode(BTPath, Sel, TEXT("BTTask_Wait"), -1);

	const FString LongName = FString::ChrN(2000, TEXT('x'));

	// FName-fatal routes, each of which must now answer with an error instead of killing the
	// editor: a property name, an asset path on the write path, an asset path on the read paths.
	const FBTPropertySetResult LongProp = UBehaviorTreeService::SetNodePropertyValue(
		BTPath, Wait, LongName, TEXT("1"));
	TestFalse(TEXT("a 2000-char property name is an error"), LongProp.bSuccess);
	TestTrue(TEXT("...that names the bound"), LongProp.Error.Contains(TEXT("characters")));

	const FString LongPath = TEXT("/Game/") + LongName;
	TestTrue(TEXT("a 2000-char asset path is refused on the write path"),
		!UBehaviorTreeService::CompileAndSave(LongPath).IsEmpty());
	TestTrue(TEXT("...and on GetTree"),
		UBehaviorTreeService::GetTree(LongPath).Contains(TEXT("error")));
	FBTNodeInfo LongInfo;
	TestFalse(TEXT("...and on GetNodeInfo"),
		UBehaviorTreeService::GetNodeInfo(LongPath, TEXT("Root"), LongInfo));
	TestEqual(TEXT("...and on listing"),
		UBehaviorTreeService::ListBehaviorTrees(LongPath).Num(), 0);

	// A linear chain deeper than MaxTreeDepth: the layout must refuse it as an error. Built as a
	// transient graph so no asset pays for a thousand commits.
	{
		UBehaviorTree* Tree = NewObject<UBehaviorTree>(GetTransientPackage());
		UBehaviorTreeGraph* Graph = NewObject<UBehaviorTreeGraph>(Tree);
		UBehaviorTreeGraphNode_Composite* Head = NewObject<UBehaviorTreeGraphNode_Composite>(
			Graph, NAME_None, RF_NoFlags);
		Head->CreateNewGuid();
		Head->AllocateDefaultPins();
		Graph->AddNode(Head, false, false);
		UBehaviorTreeGraphNode_Composite* ChainRoot = Head;
		for (int32 Index = 0; Index < VibeBT::MaxTreeDepth + 50; ++Index)
		{
			UBehaviorTreeGraphNode_Composite* Next = NewObject<UBehaviorTreeGraphNode_Composite>(
				Graph, NAME_None, RF_NoFlags);
			Next->CreateNewGuid();
			Next->AllocateDefaultPins();
			Graph->AddNode(Next, false, false);
			Head->GetOutputPin()->MakeLinkTo(Next->GetInputPin());
			Head = Next;
		}
		const FString DeepError = VibeBT::ArrangeGraph(ChainRoot);
		TestTrue(TEXT("a chain deeper than MaxTreeDepth is a refused layout, not an overflow"),
			!DeepError.IsEmpty());
		TestTrue(TEXT("...that names the bound"), DeepError.Contains(TEXT("deeper than")));
	}

	// A hostile dump: a valid top class (so the walk actually runs — an unresolvable TOP is
	// refused before any entry is written) whose child class does not resolve and carries a
	// subtree nested far past the bound. Everything below the bad child routes through
	// RecordSubtreeUnbuilt — no engine work per level, the cheapest pre-fix stack overflow — and
	// must come back as a bounded per-node report.
	{
		FString DeepJson;
		const int32 Levels = VibeBT::MaxTreeDepth + 200;
		DeepJson.Reserve(Levels * 64);
		DeepJson += TEXT("{\"class\":\"\",\"children\":[{\"class\":\"BTComposite_Selector\",")
			TEXT("\"children\":[{\"class\":\"NotARealBTClass_Bounds\"");
		for (int32 Index = 0; Index < Levels; ++Index)
		{
			DeepJson += TEXT(",\"children\":[{\"class\":\"BTComposite_Sequence\"");
		}
		for (int32 Index = 0; Index < Levels; ++Index)
		{
			DeepJson += TEXT("}]");
		}
		DeepJson += TEXT("}]}]}");

		const FBTBuildResult DeepBuild = UBehaviorTreeService::BuildTree(BTPath, DeepJson, true);
		TestFalse(TEXT("a hostile deep dump does not report success"), DeepBuild.bSuccess);
		TestTrue(TEXT("...and its report is bounded"),
			DeepBuild.Nodes.Num() <= VibeBT::MaxTreeDepth + 210);
		TestTrue(TEXT("...with the truncation named"), DeepBuild.Nodes.ContainsByPredicate(
			[](const FBTBuildNodeResult& Node)
			{ return Node.Error.Contains(TEXT("deeper than")) || Node.Error.Contains(TEXT("depth bound")); }));
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
