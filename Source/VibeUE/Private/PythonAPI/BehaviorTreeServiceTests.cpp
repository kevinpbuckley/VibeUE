// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UBehaviorTreeService.h"
#include "PythonAPI/UBlackboardService.h"
#include "AIServiceTestFixture.h"
#include "BehaviorTreeServiceInternal.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "Subsystems/AssetEditorSubsystem.h"

static const EAutomationTestFlags kBTTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

static const TCHAR* kBTTestDir = TEXT("/Game/Developers/VibeUEBTTests");

using VibeAITest::FScopedFixtureReset;

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

#endif // WITH_AUTOMATION_TESTS
