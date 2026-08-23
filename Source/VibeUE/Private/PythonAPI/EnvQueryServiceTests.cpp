// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UEnvQueryService.h"

static const EAutomationTestFlags kEQSTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

// (VibeUE.EQS.Asset.List lives below, after the fixture helpers it needs.)

#include "EnvQueryServiceInternal.h"

// Option order IS node X position, so layout is a correctness requirement. Tested pure —
// no graph, no asset, no Slate.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSLayoutTest,
	"VibeUE.EQS.Layout.Ordering", kEQSTestFlags)
bool FVibeEQSLayoutTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	using namespace VibeEQS;

	const FIntPoint Root(500, 0);
	const TArray<FIntPoint> P = ComputeOptionLayout(4, Root);

	TestEqual(TEXT("one position per option"), P.Num(), 4);

	// Strictly increasing X — equal values would sort arbitrarily and scramble option order.
	for (int32 i = 1; i < P.Num(); ++i)
	{
		TestTrue(FString::Printf(TEXT("option %d is right of %d"), i, i - 1), P[i].X > P[i - 1].X);
		TestTrue(FString::Printf(TEXT("option %d is spaced"), i), P[i].X - P[i - 1].X >= OptionSpacingX);
	}

	// All options share one row, below the root.
	for (int32 i = 0; i < P.Num(); ++i)
	{
		TestEqual(FString::Printf(TEXT("option %d row"), i), P[i].Y, Root.Y + OptionRowY);
	}

	// Idempotent.
	TestTrue(TEXT("idempotent"), ComputeOptionLayout(4, Root) == P);

	// Degenerate cases must not crash or invert.
	TestEqual(TEXT("zero options"), ComputeOptionLayout(0, Root).Num(), 0);
	TestEqual(TEXT("one option"), ComputeOptionLayout(1, Root).Num(), 1);
	return true;
#endif
}

#if WITH_VIBEUE_EQS
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#endif

// Discovery must find both native EQS classes and resolve them through the primed helper.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSClassDiscoveryTest,
	"VibeUE.EQS.Classes.Discovery", kEQSTestFlags)
bool FVibeEQSClassDiscoveryTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const TArray<FEQSClassInfo> Generators = UEnvQueryService::GetAvailableGeneratorTypes();
	TestTrue(TEXT("has generators"), Generators.Num() > 0);
	TestTrue(TEXT("has the actors-of-class generator"), Generators.ContainsByPredicate(
		[](const FEQSClassInfo& C){ return C.ClassName.Contains(TEXT("ActorsOfClass")); }));

	const TArray<FEQSClassInfo> Tests = UEnvQueryService::GetAvailableTestTypes();
	TestTrue(TEXT("has the distance test"), Tests.ContainsByPredicate(
		[](const FEQSClassInfo& C){ return C.ClassName.Contains(TEXT("Distance")); }));

	const TArray<FEQSClassInfo> Contexts = UEnvQueryService::GetAvailableContextTypes();
	TestTrue(TEXT("has the querier context"), Contexts.ContainsByPredicate(
		[](const FEQSClassInfo& C){ return C.ClassName.Contains(TEXT("Querier")); }));

	AddInfo(FString::Printf(TEXT("generators=%d tests=%d contexts=%d"),
		Generators.Num(), Tests.Num(), Contexts.Num()));

	// Resolution accepts a short native name and rejects a wrong base.
	TestNotNull(TEXT("resolves a test by short name"),
		VibeEQS::ResolveClass(TEXT("EnvQueryTest_Distance"), UEnvQueryTest::StaticClass()));
	TestNull(TEXT("rejects a wrong-base class"),
		VibeEQS::ResolveClass(TEXT("EnvQueryTest_Distance"), UEnvQueryGenerator::StaticClass()));
	TestNull(TEXT("rejects a bogus name"),
		VibeEQS::ResolveClass(TEXT("EnvQueryTest_Nonexistent"), UEnvQueryTest::StaticClass()));
	return true;
#endif
}

#include "EQSServiceTestFixture.h"

#if WITH_VIBEUE_EQS
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQueryGraph.h"
#include "EnvironmentQueryGraphNode_Option.h"
#include "EnvironmentQueryGraphNode_Root.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

/**
 * Spawn an option node and link it to the graph root, mirroring what
 * UEnvironmentQueryGraph::SpawnMissingNodes does (EnvironmentQueryGraph.cpp:426-446).
 *
 * With GeneratorClass set the node carries a live UEnvQueryOption with a generator — the shape
 * UpdateAsset keeps. With GeneratorClass null it is a *blank* option node: still an option node,
 * still root-linked, but one UpdateAsset silently drops. Both shapes are built by hand rather than
 * with AddOption, which cannot produce the blank one at all — it requires a generator, because an
 * option IS its generator.
 */
static UEnvironmentQueryGraphNode_Option* LinkOptionNodeForTest(
	UEnvironmentQueryGraph* Graph, UClass* GeneratorClass)
{
	UEnvironmentQueryGraphNode_Root* Root = VibeEQS::FindRootNode(Graph);
	if (!Root || Root->Pins.Num() == 0 || !Root->Pins[0])
	{
		return nullptr;
	}

	// Finalize() runs AllocateDefaultPins() and PostPlacedNewNode(); the latter is a no-op with
	// empty ClassData (EnvironmentQueryGraphNode_Option.cpp:29-31), so the instance below is the
	// only one the node ever gets.
	FGraphNodeCreator<UEnvironmentQueryGraphNode_Option> Creator(*Graph);
	UEnvironmentQueryGraphNode_Option* Node = Creator.CreateNode();
	Creator.Finalize();

	if (GeneratorClass)
	{
		// Outer is the UEnvQuery for both, as PostPlacedNewNode does: UEnvironmentQueryGraph::
		// CollectAllNodeInstances only protects generators reachable that way, and anything else
		// outered to the query is marked transient by RemoveOrphanedNodes on the next commit.
		UEnvQuery* Query = Cast<UEnvQuery>(Graph->GetOuter());
		UEnvQueryOption* Option = NewObject<UEnvQueryOption>(Query);
		Option->Generator = NewObject<UEnvQueryGenerator>(Query, GeneratorClass);
		Option->SetFlags(RF_Transactional);
		Option->Generator->SetFlags(RF_Transactional);
		Node->NodeInstance = Option;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input)
		{
			Root->Pins[0]->MakeLinkTo(Pin);
			break;
		}
	}
	return Node;
}
#endif

/** Every fixture in this file writes here and nowhere else. Content/Developers is gitignored. */
static const TCHAR* const kEQSTestDir = TEXT("/Game/Developers/VibeUEEQSTests");

// Listing must find EQS assets under a directory — asserted against a fixture this test CREATES,
// never against whatever the host project happens to contain.
//
// That is a correctness fix, not a preference. VibeUE ships standalone, so a bare
// "ListQueries("/Game").Num() > 0" is RED on the first run in a project that has no environment
// queries yet — it was the only assertion in this suite depending on pre-existing content.
//
// Creating the fixture also makes the assertion strictly stronger: the listing has to contain THIS
// path, so an implementation returning some other query, or ignoring its DirectoryPath argument, now
// fails. The recursive and negative cases below are what pin the directory filter itself, which a
// count could never do.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSListTest,
	"VibeUE.EQS.Asset.List", kEQSTestFlags)
bool FVibeEQSListTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_ListTest");
	FEQSScopedFixtureReset Reset(Path);
	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());

	// Its own directory. CreateQuery is what registered it, so this covers that too.
	const TArray<FString> InDir = UEnvQueryService::ListQueries(kEQSTestDir);
	TestTrue(FString::Printf(TEXT("the fixture is listed under %s (got %d: %s)"),
			kEQSTestDir, InDir.Num(), *FString::Join(InDir, TEXT(", "))),
		InDir.Contains(Path));

	// Recursive by design, which is what makes the documented "/Game" default useful rather than a
	// listing of the content root's loose assets alone.
	const TArray<FString> FromRoot = UEnvQueryService::ListQueries(TEXT("/Game"));
	TestTrue(TEXT("and from /Game, so the search recurses"), FromRoot.Contains(Path));
	TestTrue(TEXT("a recursive listing is never smaller than the nested one"),
		FromRoot.Num() >= InDir.Num());

	// The filter really filters. Without this, "it is in the list" would pass just as well for an
	// implementation that ignored DirectoryPath and returned every query in the project.
	const TArray<FString> Elsewhere =
		UEnvQueryService::ListQueries(FString(kEQSTestDir) / TEXT("NoSuchSubdirectory"));
	TestFalse(TEXT("an unrelated directory does not list it"), Elsewhere.Contains(Path));

	AddInfo(FString::Printf(TEXT("%d EQS asset(s) under %s, %d under /Game"),
		InDir.Num(), kEQSTestDir, FromRoot.Num()));
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSCreateTest,
	"VibeUE.EQS.Asset.Create", kEQSTestFlags)
bool FVibeEQSCreateTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_CreateTest");
	FEQSScopedFixtureReset Reset(Path);

	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());

	FEQSQueryInfo Info;
	TestTrue(TEXT("info readable"), UEnvQueryService::GetQueryInfo(Path, Info));
	TestTrue(TEXT("graph exists"), Info.bHasGraph);
	TestTrue(TEXT("root exists"), Info.bHasRootNode);
	TestEqual(TEXT("no options yet"), Info.OptionCount, 0);
	TestEqual(TEXT("no tests yet"), Info.TestCount, 0);

	// Creating over an existing asset is an error, not a silent overwrite.
	TestTrue(TEXT("duplicate create rejected"), !UEnvQueryService::CreateQuery(Path).IsEmpty());

	// The asset really reached disk — an in-memory re-read proves nothing.
	TestTrue(TEXT("uasset on disk"), FEQSScopedFixtureReset::FixtureFileExists(Path));

	// Reading an asset that is not there is a reported failure, not a crash or a fabricated stub.
	FEQSQueryInfo Missing;
	TestFalse(TEXT("missing asset reported"),
		UEnvQueryService::GetQueryInfo(FString(kEQSTestDir) / TEXT("EQS_NoSuchAsset"), Missing));
	TestTrue(TEXT("missing asset explains itself"), !Missing.Error.IsEmpty());

	AddInfo(FString::Printf(TEXT("uasset: %s (%lld bytes)"),
		*VibeEQSTest::FixtureFilename(Path), FEQSScopedFixtureReset::FixtureFileSize(Path)));
	return true;
#endif
}

// CompileAndSave must actually write. The only proof is the file: a re-read returns the same
// in-process object whether or not anything was persisted.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSCompileAndSaveTest,
	"VibeUE.EQS.Asset.CompileAndSave", kEQSTestFlags)
bool FVibeEQSCompileAndSaveTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_CompileTest");
	FEQSScopedFixtureReset Reset(Path);

	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());
	const int64 SizeAfterCreate = FEQSScopedFixtureReset::FixtureFileSize(Path);
	TestTrue(TEXT("create wrote a non-empty uasset"), SizeAfterCreate > 0);

	// Delete the file first, so "it is there afterwards" can only be true if CompileAndSave wrote
	// it. Asserting the size alone would pass whether or not anything was persisted, which leaves
	// the whole point of Modify() + MarkPackageDirty() + bOnlyDirty=false unasserted: the package is
	// clean at this moment, and SavePackages(..., bOnlyDirty=true) would silently skip it.
	IFileManager::Get().Delete(*VibeEQSTest::FixtureFilename(Path),
		/*RequireExists*/ false, /*EvenReadOnly*/ true);
	TestFalse(TEXT("uasset removed before recompile"),
		FEQSScopedFixtureReset::FixtureFileExists(Path));

	TestEqual(TEXT("recompiles"), UEnvQueryService::CompileAndSave(Path), FString());
	TestTrue(TEXT("recompile rewrote the uasset"),
		FEQSScopedFixtureReset::FixtureFileSize(Path) > 0);

	// Committing twice must be idempotent, not additive.
	FEQSQueryInfo Info;
	TestTrue(TEXT("info readable"), UEnvQueryService::GetQueryInfo(Path, Info));
	TestEqual(TEXT("no options invented"), Info.OptionCount, 0);
	TestTrue(TEXT("graph survived the commit"), Info.bHasGraph && Info.bHasRootNode);

	TestTrue(TEXT("unknown asset refused"),
		!UEnvQueryService::CompileAndSave(FString(kEQSTestDir) / TEXT("EQS_NoSuchAsset")).IsEmpty());

	const FString Filename = VibeEQSTest::FixtureFilename(Path);
	AddInfo(FString::Printf(TEXT("uasset: %s (%lld bytes, mtime %s)"),
		*Filename, FEQSScopedFixtureReset::FixtureFileSize(Path),
		*IFileManager::Get().GetTimeStamp(*Filename).ToString()));
	return true;
#endif
}

// The write guards are the point of this task: each one stops a commit that would otherwise report
// success having destroyed or written nothing. Each is exercised against a real fixture.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSWriteGuardTest,
	"VibeUE.EQS.Asset.WriteGuards", kEQSTestFlags)
bool FVibeEQSWriteGuardTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_GuardTest");
	FEQSScopedFixtureReset Reset(Path);

	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());

	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	TestEqual(TEXT("opens for writing"), VibeEQS::OpenWriteGuard(Path, Query, Graph), FString());
	if (!Query || !Graph)
	{
		return false;
	}

	// 1) Locked graph. UpdateAsset early-returns under bLockUpdates, so an unguarded commit writes
	//    a stale option list and reports success.
	Graph->LockUpdates();
	const FString LockedError = UEnvQueryService::CompileAndSave(Path);
	// CommitGraph re-asserts the lock itself, for a caller holding a handle that was opened before
	// the lock was taken. CompileAndSave above cannot reach that branch — OpenWriteGuard refuses
	// first — so it is exercised directly.
	const FString LockedCommitError = VibeEQS::CommitGraph(Query, Graph);
	Graph->UnlockUpdates();
	TestTrue(TEXT("locked graph refused"), !LockedError.IsEmpty());
	TestTrue(TEXT("locked commit refused"), !LockedCommitError.IsEmpty());
	AddInfo(FString::Printf(TEXT("lock refusal: %s"), *LockedError));

	// 2) Play session. Faked by setting the flag the guard reads — the automation harness has no
	//    PIE session of its own, and the guard's whole job is to read this one value. The world is
	//    never initialized or ticked, and PlayWorld is restored before anything else can observe
	//    it: the guard returns on the first branch of OpenWriteGuard.
	if (GEditor)
	{
		UWorld* const PreviousPlayWorld = GEditor->PlayWorld;
		UWorld* const FakePlayWorld =
			NewObject<UWorld>(GetTransientPackage(), UWorld::StaticClass(), NAME_None, RF_Transient);
		GEditor->PlayWorld = FakePlayWorld;
		const FString PieError = UEnvQueryService::CompileAndSave(Path);
		GEditor->PlayWorld = PreviousPlayWorld;

		TestTrue(TEXT("play session refused"), !PieError.IsEmpty());
		AddInfo(FString::Printf(TEXT("play-session refusal: %s"), *PieError));
	}
	else
	{
		AddWarning(TEXT("no GEditor: play-session guard not exercised"));
	}

	// 3) Discard guard. A populated option list plus a root feeding no option node is exactly the
	//    shape UpdateAsset turns into an empty query, on disk, with no undo. Constructed directly
	//    rather than via AddOption, which does not exist yet.
	Query = nullptr;
	Graph = nullptr;
	TestEqual(TEXT("reopens for writing"), VibeEQS::OpenWriteGuard(Path, Query, Graph), FString());
	if (!Query || !Graph)
	{
		return false;
	}

	Query->GetOptionsMutable().Add(NewObject<UEnvQueryOption>(Query));
	const FString DiscardError = VibeEQS::CommitGraph(Query, Graph);
	TestTrue(TEXT("option-destroying commit refused"), !DiscardError.IsEmpty());
	TestEqual(TEXT("refusal left the option list alone"), Query->GetOptionsMutable().Num(), 1);
	AddInfo(FString::Printf(TEXT("discard refusal: %s"), *DiscardError));

	// 4) EnsureGraph's own loss check. That orphan option is still on the in-memory query, and
	//    Graph->Initialize() -> UnlockUpdates -> UpdateAsset would silently drop it while merely
	//    *opening* the asset — before the discard guard above could ever see it.
	UEnvQuery* ReopenedQuery = nullptr;
	UEnvironmentQueryGraph* ReopenedGraph = nullptr;
	const FString ReopenError = VibeEQS::OpenWriteGuard(Path, ReopenedQuery, ReopenedGraph);
	TestTrue(TEXT("open refused when reconstruction would drop options"), !ReopenError.IsEmpty());
	TestNull(TEXT("nothing handed out on a refused open"), ReopenedQuery);
	TestNull(TEXT("no graph handed out on a refused open"), ReopenedGraph);
	AddInfo(FString::Printf(TEXT("reconstruction refusal: %s"), *ReopenError));

	// Nothing above was allowed to reach disk.
	TestTrue(TEXT("uasset still present"), FEQSScopedFixtureReset::FixtureFileExists(Path));
	return true;
#endif
}

// A refused reconstruction leaves the in-memory query damaged, so the refusal has to survive a
// retry. The intended caller is an agent, and agents retry on error: if the second open succeeded,
// it would commit exactly the loss the first one refused.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSStickyRefusalTest,
	"VibeUE.EQS.Asset.StickyRefusal", kEQSTestFlags)
bool FVibeEQSStickyRefusalTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_StickyTest");
	FEQSScopedFixtureReset Reset(Path);

	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());

	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	TestEqual(TEXT("opens for writing"), VibeEQS::OpenWriteGuard(Path, Query, Graph), FString());
	if (!Query || !Graph)
	{
		return false;
	}

	// An option the graph cannot represent: reconstruction will drop it, which is what makes the
	// next open refuse — and damage this object on the way.
	Query->GetOptionsMutable().Add(NewObject<UEnvQueryOption>(Query));

	UEnvQuery* Q1 = nullptr;
	UEnvironmentQueryGraph* G1 = nullptr;
	const FString FirstError = VibeEQS::OpenWriteGuard(Path, Q1, G1);
	TestTrue(TEXT("first open refused"), !FirstError.IsEmpty());
	TestEqual(TEXT("the drop really happened"), Query->GetOptionsMutable().Num(), 0);

	// The retry. Nothing was reloaded, so the damaged object is what LoadObject hands back; without
	// a sticky refusal this open sees 0 -> 0, reports no shrink, and lets the commit through.
	UEnvQuery* Q2 = nullptr;
	UEnvironmentQueryGraph* G2 = nullptr;
	const FString RetryError = VibeEQS::OpenWriteGuard(Path, Q2, G2);
	TestTrue(TEXT("retry without a reload still refused"), !RetryError.IsEmpty());
	TestNull(TEXT("retry hands out no query"), Q2);
	TestNull(TEXT("retry hands out no graph"), G2);

	// The same through the public entry point, and through a direct commit with the pre-damage
	// handle a caller may still be holding.
	TestTrue(TEXT("CompileAndSave after damage refused"),
		!UEnvQueryService::CompileAndSave(Path).IsEmpty());
	TestTrue(TEXT("direct commit with a stale handle refused"),
		!VibeEQS::CommitGraph(Query, Graph).IsEmpty());

	AddInfo(FString::Printf(TEXT("sticky refusal: %s"), *RetryError));
	return true;
#endif
}

// The destructive shape the first discard guard missed: a caller that unlinks a query's real option
// nodes and links a blank one satisfies "the root feeds an option node" while UpdateAsset re-adds
// nothing — so every option is reset away and saved, reporting success.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSBlankOptionTest,
	"VibeUE.EQS.Asset.BlankOptionDiscard", kEQSTestFlags)
bool FVibeEQSBlankOptionTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_BlankOptionTest");
	FEQSScopedFixtureReset Reset(Path);

	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());

	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	TestEqual(TEXT("opens for writing"), VibeEQS::OpenWriteGuard(Path, Query, Graph), FString());
	if (!Query || !Graph)
	{
		return false;
	}

	// A real option, committed. This is also the only route by which an option reaches disk at all,
	// so it doubles as proof that a healthy commit is not blocked by either guard.
	UClass* const GeneratorClass =
		VibeEQS::ResolveClass(TEXT("EnvQueryGenerator_ActorsOfClass"), UEnvQueryGenerator::StaticClass());
	TestNotNull(TEXT("generator class resolves"), GeneratorClass);
	if (!GeneratorClass || !LinkOptionNodeForTest(Graph, GeneratorClass))
	{
		return false;
	}

	TestEqual(TEXT("healthy commit allowed"), VibeEQS::CommitGraph(Query, Graph), FString());
	TestEqual(TEXT("option was rebuilt from the graph"), Query->GetOptionsMutable().Num(), 1);

	FEQSQueryInfo Info;
	TestTrue(TEXT("info readable"), UEnvQueryService::GetQueryInfo(Path, Info));
	TestEqual(TEXT("one option"), Info.OptionCount, 1);

	// Now the destructive edit, exactly as SetOptionGenerator would leave it mid-flight: the real
	// option node unlinked, a blank option node linked in its place.
	UEnvironmentQueryGraphNode_Root* Root = VibeEQS::FindRootNode(Graph);
	TestNotNull(TEXT("root found"), Root);
	if (!Root || Root->Pins.Num() == 0)
	{
		return false;
	}
	Root->Pins[0]->BreakAllPinLinks();
	TestNotNull(TEXT("blank option node linked"), LinkOptionNodeForTest(Graph, nullptr));

	const FString DiscardError = VibeEQS::CommitGraph(Query, Graph);
	TestTrue(TEXT("blank-option commit refused"), !DiscardError.IsEmpty());
	TestEqual(TEXT("options survived the refusal"), Query->GetOptionsMutable().Num(), 1);
	AddInfo(FString::Printf(TEXT("blank-option refusal: %s"), *DiscardError));

	// And the file still holds the option, not an emptied query.
	TestTrue(TEXT("uasset still present"), FEQSScopedFixtureReset::FixtureFileExists(Path));
	return true;
#endif
}

#if WITH_VIBEUE_EQS
#include "AIGraphTypes.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvironmentQueryGraphNode_Test.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * The option guids GetQuery reports, in the order it reports them.
 *
 * This is the whole point of the ordering assertions below and the reason they are written against
 * guids rather than against paths. A path is positional — "Option[0]" is index 0 by definition — so
 * asserting that an inserted option's path is "Option[0]" asserts nothing at all. A guid is the
 * node's own identity, so comparing the guid sequence before and after an insert is the only way to
 * see that the *other* options moved, which is what "inserted first" means.
 *
 * Option order is node X position, written by ArrangeGraph from link order and consumed by
 * UpdateAsset's sort. A regression there reorders a query silently, with no error anywhere: nothing
 * fails, the asset just does something else at runtime. Returns empty on any parse failure, so a
 * malformed result fails the count assertion instead of quietly comparing nothing.
 */
static TArray<FString> OptionGuidsInOrder(const FString& QueryJson)
{
	TArray<FString> Guids;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(QueryJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return Guids;
	}

	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	if (!Root->TryGetArrayField(TEXT("options"), Options) || Options == nullptr)
	{
		return Guids;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Options)
	{
		const TSharedPtr<FJsonObject>* Option = nullptr;
		FString Guid;
		if (Value.IsValid() && Value->TryGetObject(Option) && Option && Option->IsValid()
			&& (*Option)->TryGetStringField(TEXT("guid"), Guid))
		{
			Guids.Add(Guid);
		}
	}
	return Guids;
}

/** The "generator" field of option OptionIndex, or empty. */
static FString OptionGeneratorAt(const FString& QueryJson, int32 OptionIndex)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(QueryJson);
	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
		|| !Root->TryGetArrayField(TEXT("options"), Options) || Options == nullptr
		|| !Options->IsValidIndex(OptionIndex))
	{
		return FString();
	}

	const TSharedPtr<FJsonObject>* Option = nullptr;
	FString Generator;
	if ((*Options)[OptionIndex].IsValid() && (*Options)[OptionIndex]->TryGetObject(Option)
		&& Option && Option->IsValid())
	{
		(*Option)->TryGetStringField(TEXT("generator"), Generator);
	}
	return Generator;
}

/** The "class" names of option OptionIndex's tests, in order. */
static TArray<FString> TestClassesAt(const FString& QueryJson, int32 OptionIndex)
{
	TArray<FString> Classes;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(QueryJson);
	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
		|| !Root->TryGetArrayField(TEXT("options"), Options) || Options == nullptr
		|| !Options->IsValidIndex(OptionIndex))
	{
		return Classes;
	}

	const TSharedPtr<FJsonObject>* Option = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Tests = nullptr;
	if (!(*Options)[OptionIndex].IsValid() || !(*Options)[OptionIndex]->TryGetObject(Option)
		|| !Option || !Option->IsValid() || !(*Option)->TryGetArrayField(TEXT("tests"), Tests)
		|| Tests == nullptr)
	{
		return Classes;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Tests)
	{
		const TSharedPtr<FJsonObject>* Test = nullptr;
		FString ClassName;
		if (Value.IsValid() && Value->TryGetObject(Test) && Test && Test->IsValid()
			&& (*Test)->TryGetStringField(TEXT("class"), ClassName))
		{
			Classes.Add(ClassName);
		}
	}
	return Classes;
}

/** The "enabled" flag of option OptionIndex's tests, in order. Missing flags read as true. */
static TArray<bool> TestEnabledFlagsAt(const FString& QueryJson, int32 OptionIndex)
{
	TArray<bool> Flags;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(QueryJson);
	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
		|| !Root->TryGetArrayField(TEXT("options"), Options) || Options == nullptr
		|| !Options->IsValidIndex(OptionIndex))
	{
		return Flags;
	}

	const TSharedPtr<FJsonObject>* Option = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Tests = nullptr;
	if (!(*Options)[OptionIndex].IsValid() || !(*Options)[OptionIndex]->TryGetObject(Option)
		|| !Option || !Option->IsValid() || !(*Option)->TryGetArrayField(TEXT("tests"), Tests)
		|| Tests == nullptr)
	{
		return Flags;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Tests)
	{
		const TSharedPtr<FJsonObject>* Test = nullptr;
		bool bEnabled = true;
		if (Value.IsValid() && Value->TryGetObject(Test) && Test && Test->IsValid())
		{
			// Deliberately reads the field rather than defaulting silently: a missing "enabled"
			// would otherwise look exactly like an enabled test, which is the bug being tested for.
			(*Test)->TryGetBoolField(TEXT("enabled"), bEnabled);
			Flags.Add(bEnabled);
		}
	}
	return Flags;
}

/**
 * One named string field of every test on option OptionIndex, in order.
 *
 * "guid" is the field the ordering assertions use, and for the same reason the option assertions
 * use it: a test path is positional, so asserting that an inserted test's path is "@test[0]"
 * asserts nothing at all. Comparing the guid SEQUENCE before and after an insert is the only way to
 * see that the other tests moved, which is what "inserted first" means. "path" is checked
 * separately, because the renumbering it reports is the contract callers are told to re-read.
 */
static TArray<FString> TestFieldsAt(const FString& QueryJson, int32 OptionIndex, const TCHAR* Field)
{
	TArray<FString> Values;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(QueryJson);
	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
		|| !Root->TryGetArrayField(TEXT("options"), Options) || Options == nullptr
		|| !Options->IsValidIndex(OptionIndex))
	{
		return Values;
	}

	const TSharedPtr<FJsonObject>* Option = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Tests = nullptr;
	if (!(*Options)[OptionIndex].IsValid() || !(*Options)[OptionIndex]->TryGetObject(Option)
		|| !Option || !Option->IsValid() || !(*Option)->TryGetArrayField(TEXT("tests"), Tests)
		|| Tests == nullptr)
	{
		return Values;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Tests)
	{
		const TSharedPtr<FJsonObject>* Test = nullptr;
		FString Field_;
		if (Value.IsValid() && Value->TryGetObject(Test) && Test && Test->IsValid()
			&& (*Test)->TryGetStringField(Field, Field_))
		{
			Values.Add(Field_);
		}
	}
	return Values;
}

/**
 * Attach a test sub-node to an option, at the data level.
 *
 * Builds the shape by hand rather than with AddTest, so the tests below exercise the READ and path
 * layers against a sub-node the service did not create — and deliberately without
 * UAIGraphNode::AddSubNode, which ends in an immediate UpdateAsset() (AIGraphNode.cpp:297-298).
 * Sub-nodes are not added to Graph->Nodes: the engine never does, and UpdateAsset clears ParentNode
 * for every entry there.
 */
static UEnvironmentQueryGraphNode_Test* AttachTestNodeForTest(UEnvironmentQueryGraph* Graph,
	UEnvironmentQueryGraphNode_Option* OptionNode, UClass* TestClass)
{
	if (!Graph || !OptionNode || !TestClass)
	{
		return nullptr;
	}

	UEnvironmentQueryGraphNode_Test* TestNode = NewObject<UEnvironmentQueryGraphNode_Test>(Graph);
	TestNode->ClassData = FGraphNodeClassData(TestClass, FString());
	TestNode->CreateNewGuid();
	// UEnvironmentQueryGraphNode_Test does not override PostPlacedNewNode, so this is
	// UAIGraphNode's: it creates the UEnvQueryTest instance under the graph's owner, which is the
	// outer CollectAllNodeInstances protects from RemoveOrphanedNodes.
	TestNode->PostPlacedNewNode();
	TestNode->ParentNode = OptionNode;
	OptionNode->SubNodes.Add(TestNode);
	return TestNode;
}
#endif

// Option CRUD, and the ordering that gives an index meaning. The insert-at-index assertions are the
// point: an option's position IS its execution order.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSOptionCrudTest,
	"VibeUE.EQS.Options.Crud", kEQSTestFlags)
bool FVibeEQSOptionCrudTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_OptionTest");
	FEQSScopedFixtureReset Reset(Path);
	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());

	const FString A = UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1);
	TestFalse(TEXT("first option added"), A.StartsWith(TEXT("ERROR")));

	const FString B = UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1);
	TestFalse(TEXT("second option added"), B.StartsWith(TEXT("ERROR")));
	TestNotEqual(TEXT("options get distinct paths"), A, B);

	FEQSQueryInfo Info;
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("two options"), Info.OptionCount, 2);

	// An unknown generator is rejected, and nothing is added.
	TestTrue(TEXT("unknown generator rejected"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_Nonexistent"), -1)
			.StartsWith(TEXT("ERROR")));
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("still two options"), Info.OptionCount, 2);

	// Identity, captured before the insert. Everything below compares guids rather than paths,
	// because a path is positional and cannot show that anything moved.
	const TArray<FString> Before = OptionGuidsInOrder(UEnvQueryService::GetQuery(Path));
	TestEqual(TEXT("GetQuery lists two options"), Before.Num(), 2);
	if (Before.Num() != 2)
	{
		return false;
	}
	TestNotEqual(TEXT("options are distinct nodes"), Before[0], Before[1]);

	// Insert at 0 must become FIRST, not merely present.
	const FString C = UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), 0);
	TestFalse(TEXT("insert at 0 ok"), C.StartsWith(TEXT("ERROR")));
	TestEqual(TEXT("insert at 0 reports Option[0]"), C, FString(TEXT("Option[0]")));

	const TArray<FString> After = OptionGuidsInOrder(UEnvQueryService::GetQuery(Path));
	TestEqual(TEXT("three options after the insert"), After.Num(), 3);
	if (After.Num() != 3)
	{
		return false;
	}
	// The new node is at 0 — asserted as "neither of the two that were there", so this cannot pass
	// by the inserted option merely existing somewhere.
	TestTrue(TEXT("index 0 holds the newly inserted option"),
		After[0] != Before[0] && After[0] != Before[1]);
	TestEqual(TEXT("the previous first option shifted to index 1"), After[1], Before[0]);
	TestEqual(TEXT("the previous second option shifted to index 2"), After[2], Before[1]);
	const FString InsertedGuid = After[0];

	// Removal takes the node the path names, not just any node.
	TestEqual(TEXT("remove ok"), UEnvQueryService::RemoveOption(Path, C), FString());
	const TArray<FString> AfterRemove = OptionGuidsInOrder(UEnvQueryService::GetQuery(Path));
	TestEqual(TEXT("two options after the removal"), AfterRemove.Num(), 2);
	TestFalse(TEXT("the removed option is gone"), AfterRemove.Contains(InsertedGuid));
	if (AfterRemove.Num() == 2)
	{
		TestEqual(TEXT("the survivors kept their order"), AfterRemove[0], Before[0]);
		TestEqual(TEXT("the second survivor kept its order"), AfterRemove[1], Before[1]);
	}

	// Deliberately NOT "removing twice errors": paths are positional, so removing "Option[0]" again
	// removes the option that is now first, which is a legitimate call. What must error is a path
	// that names nothing, and one that is not a path at all.
	TestTrue(TEXT("out-of-range removal errors"),
		!UEnvQueryService::RemoveOption(Path, TEXT("Option[7]")).IsEmpty());
	TestTrue(TEXT("malformed path errors"),
		!UEnvQueryService::RemoveOption(Path, TEXT("Option[zero]")).IsEmpty());
	TestTrue(TEXT("negative index errors"),
		!UEnvQueryService::RemoveOption(Path, TEXT("Option[-1]")).IsEmpty());
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("failed removals removed nothing"), Info.OptionCount, 2);

	// Down to one, then the documented refusal: the last option cannot be removed, because a graph
	// that rebuilds no options is what empties a query on disk.
	TestEqual(TEXT("second removal ok"),
		UEnvQueryService::RemoveOption(Path, TEXT("Option[0]")), FString());
	const FString LastError = UEnvQueryService::RemoveOption(Path, TEXT("Option[0]"));
	TestTrue(TEXT("removing the last option refused"), !LastError.IsEmpty());
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("the last option survived the refusal"), Info.OptionCount, 1);
	AddInfo(FString::Printf(TEXT("last-option refusal: %s"), *LastError));

	// Reading an asset that is not there is a reported failure carrying a walkable, empty array.
	const FString MissingJson =
		UEnvQueryService::GetQuery(FString(kEQSTestDir) / TEXT("EQS_NoSuchAsset"));
	TestTrue(TEXT("missing asset reports an error"), MissingJson.Contains(TEXT("\"error\"")));
	// Both halves are needed. OptionGuidsInOrder returns an empty array for unparseable garbage too,
	// so on its own the count below would pass for a result no caller could read; the field check is
	// what asserts the error result is still the walkable shape the contract promises.
	TestTrue(TEXT("missing asset still carries an options field"),
		MissingJson.Contains(TEXT("\"options\"")));
	TestEqual(TEXT("and it parses to no options"), OptionGuidsInOrder(MissingJson).Num(), 0);

	TestTrue(TEXT("uasset on disk"), FEQSScopedFixtureReset::FixtureFileExists(Path));
	return true;
#endif
}

// MoveOption's reordering, and SetOptionGenerator's one reason to exist: the tests survive.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSOptionMoveGeneratorTest,
	"VibeUE.EQS.Options.MoveAndGenerator", kEQSTestFlags)
bool FVibeEQSOptionMoveGeneratorTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_OptionMoveTest");
	FEQSScopedFixtureReset Reset(Path);
	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());

	TestFalse(TEXT("first option added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1)
			.StartsWith(TEXT("ERROR")));
	TestFalse(TEXT("second option added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_SimpleGrid"), -1)
			.StartsWith(TEXT("ERROR")));

	const TArray<FString> Initial = OptionGuidsInOrder(UEnvQueryService::GetQuery(Path));
	TestEqual(TEXT("two options"), Initial.Num(), 2);
	if (Initial.Num() != 2)
	{
		return false;
	}
	TestEqual(TEXT("option 0 is the actors generator"),
		OptionGeneratorAt(UEnvQueryService::GetQuery(Path), 0),
		FString(TEXT("EnvQueryGenerator_ActorsOfClass")));

	// Move the second option to the front.
	TestEqual(TEXT("move to 0 ok"),
		UEnvQueryService::MoveOption(Path, TEXT("Option[1]"), 0), FString());
	const TArray<FString> Moved = OptionGuidsInOrder(UEnvQueryService::GetQuery(Path));
	TestEqual(TEXT("still two options"), Moved.Num(), 2);
	if (Moved.Num() != 2)
	{
		return false;
	}
	TestEqual(TEXT("the moved option is now first"), Moved[0], Initial[1]);
	TestEqual(TEXT("the other option is now second"), Moved[1], Initial[0]);
	TestEqual(TEXT("the generator moved with it"),
		OptionGeneratorAt(UEnvQueryService::GetQuery(Path), 0),
		FString(TEXT("EnvQueryGenerator_SimpleGrid")));

	// A negative index means "last", matching AddOption.
	TestEqual(TEXT("move to the end ok"),
		UEnvQueryService::MoveOption(Path, TEXT("Option[0]"), -1), FString());
	const TArray<FString> MovedBack = OptionGuidsInOrder(UEnvQueryService::GetQuery(Path));
	TestTrue(TEXT("order restored"), MovedBack == Initial);

	TestTrue(TEXT("moving a nonexistent option errors"),
		!UEnvQueryService::MoveOption(Path, TEXT("Option[9]"), 0).IsEmpty());

	// A test on option 0, built by hand rather than with AddTest, so this asserts SetOptionGenerator's
	// whole claim — that a test authored against an option survives its generator being replaced —
	// without depending on the test-authoring path being correct.
	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	TestEqual(TEXT("opens for writing"), VibeEQS::OpenWriteGuard(Path, Query, Graph), FString());
	if (!Query || !Graph)
	{
		return false;
	}

	UClass* const TestClass =
		VibeEQS::ResolveClass(TEXT("EnvQueryTest_Distance"), UEnvQueryTest::StaticClass());
	TestNotNull(TEXT("test class resolves"), TestClass);
	UEnvironmentQueryGraphNode_Option* OptionNode =
		Cast<UEnvironmentQueryGraphNode_Option>(VibeEQS::ResolveNodePath(Graph, TEXT("Option[0]")));
	TestNotNull(TEXT("Option[0] resolves"), OptionNode);
	if (!TestClass || !OptionNode || !AttachTestNodeForTest(Graph, OptionNode, TestClass))
	{
		return false;
	}
	TestEqual(TEXT("commits with a test attached"), VibeEQS::CommitGraph(Query, Graph), FString());

	FEQSQueryInfo Info;
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("one test on the query"), Info.TestCount, 1);
	const TArray<FString> TestClasses = TestClassesAt(UEnvQueryService::GetQuery(Path), 0);
	TestEqual(TEXT("GetQuery reports one test"), TestClasses.Num(), 1);
	TestEqual(TEXT("GetQuery reports its class"),
		TestClasses.Num() ? TestClasses[0] : FString(), FString(TEXT("EnvQueryTest_Distance")));

	// Path resolution reaches it — the "@test[n]" grammar every test-level method addresses through.
	TestNotNull(TEXT("the test resolves by path"),
		VibeEQS::ResolveNodePath(Graph, TEXT("Option[0]/@test[0]")));
	TestNull(TEXT("an out-of-range test index resolves to nothing"),
		VibeEQS::ResolveNodePath(Graph, TEXT("Option[0]/@test[3]")));

	// The replacement. Remove-plus-add would lose the test; this must not.
	TestEqual(TEXT("generator replaced"),
		UEnvQueryService::SetOptionGenerator(Path, TEXT("Option[0]"),
			TEXT("EnvQueryGenerator_OnCircle")), FString());
	TestEqual(TEXT("the new generator is on the option"),
		OptionGeneratorAt(UEnvQueryService::GetQuery(Path), 0),
		FString(TEXT("EnvQueryGenerator_OnCircle")));
	const TArray<FString> SurvivingTests = TestClassesAt(UEnvQueryService::GetQuery(Path), 0);
	TestEqual(TEXT("the test survived the replacement"), SurvivingTests.Num(), 1);
	TestEqual(TEXT("and it is the same test class"),
		SurvivingTests.Num() ? SurvivingTests[0] : FString(), FString(TEXT("EnvQueryTest_Distance")));
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("and is still on the committed query"), Info.TestCount, 1);
	TestEqual(TEXT("the option did not move"), Info.OptionCount, 2);

	const TArray<FString> AfterGenerator = OptionGuidsInOrder(UEnvQueryService::GetQuery(Path));
	TestTrue(TEXT("option order unchanged by the replacement"), AfterGenerator == Initial);

	TestTrue(TEXT("an unknown generator is refused"),
		!UEnvQueryService::SetOptionGenerator(Path, TEXT("Option[0]"),
			TEXT("EnvQueryGenerator_Nonexistent")).IsEmpty());
	TestTrue(TEXT("a test path is not an option path"),
		!UEnvQueryService::SetOptionGenerator(Path, TEXT("Option[0]/@test[0]"),
			TEXT("EnvQueryGenerator_SimpleGrid")).IsEmpty());
	TestEqual(TEXT("the refusals changed nothing"),
		OptionGeneratorAt(UEnvQueryService::GetQuery(Path), 0),
		FString(TEXT("EnvQueryGenerator_OnCircle")));

	TestTrue(TEXT("uasset on disk"), FEQSScopedFixtureReset::FixtureFileExists(Path));
	return true;
#endif
}

// A disabled test is still listed and still addressable, and GetQuery is the only place that says
// it is disabled: bTestEnabled lives on the graph node, not on the UEnvQueryTest, so no property
// reflection over the instance can reach it. Without "enabled", GetQuery's test count and
// GetQueryInfo's TestCount disagree about the same asset with nothing explaining why.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSDisabledTestTest,
	"VibeUE.EQS.Options.DisabledTest", kEQSTestFlags)
bool FVibeEQSDisabledTestTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_DisabledTestTest");
	FEQSScopedFixtureReset Reset(Path);
	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());
	TestFalse(TEXT("option added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1)
			.StartsWith(TEXT("ERROR")));

	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	TestEqual(TEXT("opens for writing"), VibeEQS::OpenWriteGuard(Path, Query, Graph), FString());
	UClass* const TestClass =
		VibeEQS::ResolveClass(TEXT("EnvQueryTest_Distance"), UEnvQueryTest::StaticClass());
	UEnvironmentQueryGraphNode_Option* OptionNode = Graph
		? Cast<UEnvironmentQueryGraphNode_Option>(VibeEQS::ResolveNodePath(Graph, TEXT("Option[0]")))
		: nullptr;
	TestNotNull(TEXT("Option[0] resolves"), OptionNode);
	if (!Query || !Graph || !TestClass || !OptionNode)
	{
		return false;
	}

	// Two tests: the first left enabled, the second switched off — the state a human sets from the
	// node's context menu. Both are built by hand, so what is asserted below is how the READ path
	// reports a disabled test, independently of how it came to be disabled.
	TestNotNull(TEXT("enabled test attached"), AttachTestNodeForTest(Graph, OptionNode, TestClass));
	UEnvironmentQueryGraphNode_Test* DisabledNode =
		AttachTestNodeForTest(Graph, OptionNode, TestClass);
	TestNotNull(TEXT("second test attached"), DisabledNode);
	if (!DisabledNode)
	{
		return false;
	}
	DisabledNode->bTestEnabled = false;

	TestEqual(TEXT("commits"), VibeEQS::CommitGraph(Query, Graph), FString());

	// Both tests are reported, in sub-node order, and the flags say which is which.
	const FString Json = UEnvQueryService::GetQuery(Path);
	const TArray<FString> Classes = TestClassesAt(Json, 0);
	TestEqual(TEXT("both tests are listed"), Classes.Num(), 2);

	const TArray<bool> Enabled = TestEnabledFlagsAt(Json, 0);
	TestEqual(TEXT("a flag per test"), Enabled.Num(), 2);
	if (Enabled.Num() == 2)
	{
		TestTrue(TEXT("the first test reports enabled"), Enabled[0]);
		TestFalse(TEXT("the disabled test reports enabled: false"), Enabled[1]);
	}

	// The disabled one is still addressable — that is the whole reason paths index SubNodes rather
	// than the filtered UEnvQueryOption::Tests.
	TestNotNull(TEXT("the disabled test resolves by path"),
		VibeEQS::ResolveNodePath(Graph, TEXT("Option[0]/@test[1]")));

	// And the documented disagreement: TestCount counts the enabled ones only.
	FEQSQueryInfo Info;
	TestTrue(TEXT("info readable"), UEnvQueryService::GetQueryInfo(Path, Info));
	TestEqual(TEXT("TestCount counts enabled tests only"), Info.TestCount, 1);
	AddInfo(FString::Printf(
		TEXT("GetQuery lists %d test(s), GetQueryInfo counts %d — the disabled one is the difference"),
		Classes.Num(), Info.TestCount));

	// Replacing the generator must not quietly enable, disable or drop anything.
	TestEqual(TEXT("generator replaced"),
		UEnvQueryService::SetOptionGenerator(Path, TEXT("Option[0]"),
			TEXT("EnvQueryGenerator_SimpleGrid")), FString());
	const TArray<bool> AfterReplace = TestEnabledFlagsAt(UEnvQueryService::GetQuery(Path), 0);
	TestEqual(TEXT("both tests survived the replacement"), AfterReplace.Num(), 2);
	if (AfterReplace.Num() == 2)
	{
		TestTrue(TEXT("the enabled test is still enabled"), AfterReplace[0]);
		TestFalse(TEXT("the disabled test is still disabled"), AfterReplace[1]);
	}

	TestTrue(TEXT("uasset on disk"), FEQSScopedFixtureReset::FixtureFileExists(Path));
	return true;
#endif
}

// Test sub-node CRUD, and the ordering that gives a test index meaning: test order is the order the
// tests run in, written straight into UEnvQueryTest::TestOrder by the commit.
//
// This is also the hazard regression for the whole task. AddTest deliberately does NOT call
// UAIGraphNode::AddSubNode, which ends in an immediate UpdateAsset() (AIGraphNode.cpp:297-298) —
// a regeneration of UEnvQuery::Options from a mid-edit graph, before CommitGraph and outside every
// guard it installs. The sequence of adds and removes below, with its option-and-generator
// assertions after each one, is what would catch that: the failure mode is not an error, it is a
// silent commit of whatever the graph happened to look like at that instant.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSTestCrudTest,
	"VibeUE.EQS.Tests.Crud", kEQSTestFlags)
bool FVibeEQSTestCrudTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_TestCrudTest");
	FEQSScopedFixtureReset Reset(Path);
	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());

	// Two options, so every assertion below can also say that the OTHER option was left alone. One
	// option would make an accidental full regeneration indistinguishable from a correct edit.
	TestFalse(TEXT("option 0 added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1)
			.StartsWith(TEXT("ERROR")));
	TestFalse(TEXT("option 1 added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_SimpleGrid"), -1)
			.StartsWith(TEXT("ERROR")));

	// A test on the second option, which nothing below ever touches again. If a stray UpdateAsset
	// ever regenerates the query from a half-finished graph, this is the test that disappears.
	const FString Bystander =
		UEnvQueryService::AddTest(Path, TEXT("Option[1]"), TEXT("EnvQueryTest_Trace"), -1);
	TestEqual(TEXT("bystander test added at the option it names"),
		Bystander, FString(TEXT("Option[1]/@test[0]")));

	// 1) Add, and read back at @test[0].
	const FString First =
		UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryTest_Distance"), -1);
	TestEqual(TEXT("first test reports Option[0]/@test[0]"),
		First, FString(TEXT("Option[0]/@test[0]")));

	FString Json = UEnvQueryService::GetQuery(Path);
	TestEqual(TEXT("one test on option 0"), TestClassesAt(Json, 0).Num(), 1);
	TestEqual(TEXT("and it is the class that was asked for"),
		TestFieldsAt(Json, 0, TEXT("class")).Num() ? TestFieldsAt(Json, 0, TEXT("class"))[0] : FString(),
		FString(TEXT("EnvQueryTest_Distance")));
	TestEqual(TEXT("GetQuery agrees on its path"),
		TestFieldsAt(Json, 0, TEXT("path")).Num() ? TestFieldsAt(Json, 0, TEXT("path"))[0] : FString(),
		First);

	FEQSQueryInfo Info;
	TestTrue(TEXT("info readable"), UEnvQueryService::GetQueryInfo(Path, Info));
	TestEqual(TEXT("the runtime option list still holds both options"), Info.OptionCount, 2);
	TestEqual(TEXT("and both tests reached the runtime test arrays"), Info.TestCount, 2);

	// 2) Insert at 0 must become FIRST, not merely present. Asserted on guids: a path is positional
	//    and could not show that the existing test moved.
	const TArray<FString> BeforeInsert = TestFieldsAt(Json, 0, TEXT("guid"));
	TestEqual(TEXT("one test guid before the insert"), BeforeInsert.Num(), 1);
	if (BeforeInsert.Num() != 1)
	{
		return false;
	}

	const FString Inserted =
		UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryTest_Dot"), 0);
	TestEqual(TEXT("insert at 0 reports Option[0]/@test[0]"),
		Inserted, FString(TEXT("Option[0]/@test[0]")));

	Json = UEnvQueryService::GetQuery(Path);
	const TArray<FString> AfterInsert = TestFieldsAt(Json, 0, TEXT("guid"));
	TestEqual(TEXT("two tests after the insert"), AfterInsert.Num(), 2);
	if (AfterInsert.Num() != 2)
	{
		return false;
	}
	TestNotEqual(TEXT("index 0 holds the newly inserted test"), AfterInsert[0], BeforeInsert[0]);
	TestEqual(TEXT("the previous first test shifted to index 1"), AfterInsert[1], BeforeInsert[0]);

	TArray<FString> Classes = TestClassesAt(Json, 0);
	TestEqual(TEXT("classes are in insert order"), Classes.Num(), 2);
	if (Classes.Num() == 2)
	{
		TestEqual(TEXT("the inserted test runs first"), Classes[0], FString(TEXT("EnvQueryTest_Dot")));
		TestEqual(TEXT("the original test runs second"),
			Classes[1], FString(TEXT("EnvQueryTest_Distance")));
	}

	// The other option is untouched — the assertion an unguarded AddSubNode would break.
	TestEqual(TEXT("option 1 still has exactly its own test"), TestClassesAt(Json, 1).Num(), 1);
	TestEqual(TEXT("option 1's generator survived"), OptionGeneratorAt(Json, 1),
		FString(TEXT("EnvQueryGenerator_SimpleGrid")));
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("still two options"), Info.OptionCount, 2);
	TestEqual(TEXT("three tests on the query"), Info.TestCount, 3);

	// 3) Append lands last, so Index < 0 is not silently "somewhere".
	const FString Appended =
		UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryTest_Trace"), -1);
	TestEqual(TEXT("append reports Option[0]/@test[2]"),
		Appended, FString(TEXT("Option[0]/@test[2]")));
	Json = UEnvQueryService::GetQuery(Path);
	Classes = TestClassesAt(Json, 0);
	TestEqual(TEXT("three tests on option 0"), Classes.Num(), 3);
	if (Classes.Num() == 3)
	{
		TestEqual(TEXT("the appended test is last"), Classes[2], FString(TEXT("EnvQueryTest_Trace")));
	}
	// An index past the end clamps rather than failing or wrapping to the front.
	TestEqual(TEXT("an out-of-range index clamps to the end"),
		UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryTest_Dot"), 99),
		FString(TEXT("Option[0]/@test[3]")));
	TestEqual(TEXT("clamped insert ok"),
		UEnvQueryService::RemoveTest(Path, TEXT("Option[0]/@test[3]")), FString());

	// 4) MoveTest reorders within the option, and only within it.
	const TArray<FString> BeforeMove = TestFieldsAt(UEnvQueryService::GetQuery(Path), 0, TEXT("guid"));
	TestEqual(TEXT("three test guids before the move"), BeforeMove.Num(), 3);
	if (BeforeMove.Num() != 3)
	{
		return false;
	}
	TestEqual(TEXT("move the last test to the front"),
		UEnvQueryService::MoveTest(Path, TEXT("Option[0]/@test[2]"), 0), FString());

	Json = UEnvQueryService::GetQuery(Path);
	const TArray<FString> AfterMove = TestFieldsAt(Json, 0, TEXT("guid"));
	TestEqual(TEXT("still three tests"), AfterMove.Num(), 3);
	if (AfterMove.Num() == 3)
	{
		TestEqual(TEXT("the moved test is now first"), AfterMove[0], BeforeMove[2]);
		TestEqual(TEXT("the first shifted to second"), AfterMove[1], BeforeMove[0]);
		TestEqual(TEXT("the second shifted to third"), AfterMove[2], BeforeMove[1]);
	}
	// A negative index means "last", matching AddTest and MoveOption.
	TestEqual(TEXT("move back to the end"),
		UEnvQueryService::MoveTest(Path, TEXT("Option[0]/@test[0]"), -1), FString());
	TestTrue(TEXT("order restored"),
		TestFieldsAt(UEnvQueryService::GetQuery(Path), 0, TEXT("guid")) == BeforeMove);

	// 5) Remove takes the node the path names, and the survivors renumber.
	const FString RemovedGuid = BeforeMove[1];
	TestEqual(TEXT("remove the middle test"),
		UEnvQueryService::RemoveTest(Path, TEXT("Option[0]/@test[1]")), FString());

	Json = UEnvQueryService::GetQuery(Path);
	const TArray<FString> AfterRemove = TestFieldsAt(Json, 0, TEXT("guid"));
	TestEqual(TEXT("two tests after the removal"), AfterRemove.Num(), 2);
	TestFalse(TEXT("the removed test is gone"), AfterRemove.Contains(RemovedGuid));
	if (AfterRemove.Num() == 2)
	{
		TestEqual(TEXT("the first survivor kept its place"), AfterRemove[0], BeforeMove[0]);
		TestEqual(TEXT("the last test renumbered from @test[2] to @test[1]"),
			AfterRemove[1], BeforeMove[2]);
	}
	const TArray<FString> Paths = TestFieldsAt(Json, 0, TEXT("path"));
	TestEqual(TEXT("and GetQuery reports the renumbered paths"), Paths.Num(), 2);
	if (Paths.Num() == 2)
	{
		TestEqual(TEXT("path 0"), Paths[0], FString(TEXT("Option[0]/@test[0]")));
		TestEqual(TEXT("path 1"), Paths[1], FString(TEXT("Option[0]/@test[1]")));
	}

	// 6) The failures. Each must change nothing.
	TestTrue(TEXT("a test on a nonexistent option errors"),
		UEnvQueryService::AddTest(Path, TEXT("Option[9]"), TEXT("EnvQueryTest_Distance"), -1)
			.StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("an unknown test class errors"),
		UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryTest_Nonexistent"), -1)
			.StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("a generator class is not a test class"),
		UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryGenerator_ActorsOfClass"), -1)
			.StartsWith(TEXT("ERROR")));
	// A meta=(DeprecatedNode) class is unresolvable, and deliberately so: it is not a gap in
	// ResolveClass but the engine's own rule, FGraphNodeClassHelper::FindAllSubClasses skipping
	// `Node->Data.IsDeprecated()` (AIGraphTypes.cpp:399). The editor's own "Add Test..." menu does not
	// offer UEnvQueryTest_Random either, so accepting it here would author something a human could
	// not. Asserted rather than merely known, because the failure it caused once was a confusing
	// "unknown or ambiguous class" for a class that plainly exists.
	TestTrue(TEXT("a deprecated test class is refused"),
		UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryTest_Random"), -1)
			.StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("a test path is not an option path for AddTest"),
		UEnvQueryService::AddTest(Path, TEXT("Option[0]/@test[0]"), TEXT("EnvQueryTest_Distance"), -1)
			.StartsWith(TEXT("ERROR")));
	TestTrue(TEXT("an out-of-range test removal errors"),
		!UEnvQueryService::RemoveTest(Path, TEXT("Option[0]/@test[7]")).IsEmpty());
	TestTrue(TEXT("a malformed test path errors"),
		!UEnvQueryService::RemoveTest(Path, TEXT("Option[0]/@test[two]")).IsEmpty());
	TestTrue(TEXT("an option path is not a test path for RemoveTest"),
		!UEnvQueryService::RemoveTest(Path, TEXT("Option[0]")).IsEmpty());
	TestTrue(TEXT("moving a nonexistent test errors"),
		!UEnvQueryService::MoveTest(Path, TEXT("Option[0]/@test[7]"), 0).IsEmpty());

	Json = UEnvQueryService::GetQuery(Path);
	TestEqual(TEXT("the refusals left option 0 alone"), TestClassesAt(Json, 0).Num(), 2);
	TestEqual(TEXT("and left option 1 alone"), TestClassesAt(Json, 1).Num(), 1);

	// 7) The hazard regression, stated as the end state. Both options, both generators and every
	//    remaining test survived a sequence of five adds, two removes and two moves.
	TestEqual(TEXT("option 0's generator survived it all"), OptionGeneratorAt(Json, 0),
		FString(TEXT("EnvQueryGenerator_ActorsOfClass")));
	TestEqual(TEXT("option 1's generator survived it all"), OptionGeneratorAt(Json, 1),
		FString(TEXT("EnvQueryGenerator_SimpleGrid")));
	TestEqual(TEXT("option 1 still holds its untouched test"),
		TestClassesAt(Json, 1).Num() ? TestClassesAt(Json, 1)[0] : FString(),
		FString(TEXT("EnvQueryTest_Trace")));

	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("two options on the committed query"), Info.OptionCount, 2);
	TestEqual(TEXT("three tests on the committed query"), Info.TestCount, 3);

	// Removing the only test of an option is allowed — unlike the last option, an option with no
	// tests is a valid query.
	TestEqual(TEXT("removing option 1's only test is allowed"),
		UEnvQueryService::RemoveTest(Path, TEXT("Option[1]/@test[0]")), FString());
	Json = UEnvQueryService::GetQuery(Path);
	TestEqual(TEXT("option 1 now has no tests"), TestClassesAt(Json, 1).Num(), 0);
	TestEqual(TEXT("but the option itself is still there"), OptionGeneratorAt(Json, 1),
		FString(TEXT("EnvQueryGenerator_SimpleGrid")));
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("still two options"), Info.OptionCount, 2);
	TestEqual(TEXT("two tests left"), Info.TestCount, 2);

	TestTrue(TEXT("uasset on disk"), FEQSScopedFixtureReset::FixtureFileExists(Path));
	AddInfo(FString::Printf(TEXT("uasset: %s (%lld bytes)"),
		*VibeEQSTest::FixtureFilename(Path), FEQSScopedFixtureReset::FixtureFileSize(Path)));
	return true;
#endif
}

// SetTestEnabled is the only way to write bTestEnabled, which lives on the graph node rather than on
// the UEnvQueryTest — so without it GetQuery's "enabled" field is readable and unsettable, and any
// round-trip of a query with a disabled test would come back all-enabled.
//
// Both halves of what disabling means are asserted, because they are different facts: the sub-node
// stays in the graph (still listed, still addressable, nothing renumbers) while the instance leaves
// UEnvQueryOption::Tests, which is what the query actually executes.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSSetTestEnabledTest,
	"VibeUE.EQS.Tests.SetEnabled", kEQSTestFlags)
bool FVibeEQSSetTestEnabledTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_SetTestEnabledTest");
	FEQSScopedFixtureReset Reset(Path);
	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());
	TestFalse(TEXT("option added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1)
			.StartsWith(TEXT("ERROR")));

	TestEqual(TEXT("test 0 added"),
		UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryTest_Distance"), -1),
		FString(TEXT("Option[0]/@test[0]")));
	TestEqual(TEXT("test 1 added"),
		UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryTest_Trace"), -1),
		FString(TEXT("Option[0]/@test[1]")));

	// A freshly added test is enabled, which is what makes "disabled" a change rather than a default.
	FString Json = UEnvQueryService::GetQuery(Path);
	TArray<bool> Flags = TestEnabledFlagsAt(Json, 0);
	TestEqual(TEXT("a flag per test"), Flags.Num(), 2);
	if (Flags.Num() != 2)
	{
		return false;
	}
	TestTrue(TEXT("new tests are enabled"), Flags[0] && Flags[1]);

	FEQSQueryInfo Info;
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("both run"), Info.TestCount, 2);

	const TArray<FString> GuidsBefore = TestFieldsAt(Json, 0, TEXT("guid"));

	// Disable the FIRST test, not the last: a disabled test that stopped being listed would renumber
	// the one after it, and only a non-terminal test can show that it does not.
	TestEqual(TEXT("disable ok"),
		UEnvQueryService::SetTestEnabled(Path, TEXT("Option[0]/@test[0]"), false), FString());

	Json = UEnvQueryService::GetQuery(Path);
	Flags = TestEnabledFlagsAt(Json, 0);
	TestEqual(TEXT("both tests are still listed"), Flags.Num(), 2);
	if (Flags.Num() == 2)
	{
		TestFalse(TEXT("the disabled test reports enabled: false"), Flags[0]);
		TestTrue(TEXT("the other test is untouched"), Flags[1]);
	}
	TestTrue(TEXT("nothing renumbered"), TestFieldsAt(Json, 0, TEXT("guid")) == GuidsBefore);
	TestEqual(TEXT("and it is still addressable at the same path"),
		TestFieldsAt(Json, 0, TEXT("path")).Num() ? TestFieldsAt(Json, 0, TEXT("path"))[0] : FString(),
		FString(TEXT("Option[0]/@test[0]")));

	// The half that matters at runtime: it left UEnvQueryOption::Tests.
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("only the enabled test runs"), Info.TestCount, 1);
	TestEqual(TEXT("the option is still there"), Info.OptionCount, 1);

	// It survives a further unrelated commit rather than being an in-memory flag the next
	// regeneration forgets.
	TestEqual(TEXT("recompiles"), UEnvQueryService::CompileAndSave(Path), FString());
	Flags = TestEnabledFlagsAt(UEnvQueryService::GetQuery(Path), 0);
	TestEqual(TEXT("still two tests after a recompile"), Flags.Num(), 2);
	if (Flags.Num() == 2)
	{
		TestFalse(TEXT("still disabled after a recompile"), Flags[0]);
	}
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("and still only one runs"), Info.TestCount, 1);

	// Re-enabling puts it back in the runtime list, in its original position.
	TestEqual(TEXT("enable ok"),
		UEnvQueryService::SetTestEnabled(Path, TEXT("Option[0]/@test[0]"), true), FString());
	Json = UEnvQueryService::GetQuery(Path);
	Flags = TestEnabledFlagsAt(Json, 0);
	TestEqual(TEXT("a flag per test"), Flags.Num(), 2);
	if (Flags.Num() == 2)
	{
		TestTrue(TEXT("re-enabled"), Flags[0] && Flags[1]);
	}
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("both run again"), Info.TestCount, 2);
	const TArray<FString> Classes = TestClassesAt(Json, 0);
	TestEqual(TEXT("two tests"), Classes.Num(), 2);
	if (Classes.Num() == 2)
	{
		TestEqual(TEXT("and in the original order"), Classes[0], FString(TEXT("EnvQueryTest_Distance")));
	}

	// Setting the flag it already has is a no-op that still reports success — an idempotent call, not
	// an error, because a caller replaying a desired state must not have to know the current one.
	TestEqual(TEXT("enabling an enabled test is idempotent"),
		UEnvQueryService::SetTestEnabled(Path, TEXT("Option[0]/@test[0]"), true), FString());

	// The failures.
	TestTrue(TEXT("a nonexistent test errors"),
		!UEnvQueryService::SetTestEnabled(Path, TEXT("Option[0]/@test[7]"), false).IsEmpty());
	TestTrue(TEXT("an option path is not a test path"),
		!UEnvQueryService::SetTestEnabled(Path, TEXT("Option[0]"), false).IsEmpty());
	TestTrue(TEXT("a missing asset errors"),
		!UEnvQueryService::SetTestEnabled(FString(kEQSTestDir) / TEXT("EQS_NoSuchAsset"),
			TEXT("Option[0]/@test[0]"), false).IsEmpty());
	UEnvQueryService::GetQueryInfo(Path, Info);
	TestEqual(TEXT("the refusals changed nothing"), Info.TestCount, 2);

	TestTrue(TEXT("uasset on disk"), FEQSScopedFixtureReset::FixtureFileExists(Path));
	return true;
#endif
}

#if WITH_VIBEUE_EQS
#include "AIGraphNode.h"
#include "DataProviders/AIDataProvider.h"
#include "DataProviders/AIDataProvider_QueryParams.h"

/** The FEQSPropertyInfo named Name, or a default-constructed one (empty Name) if there is none. */
static FEQSPropertyInfo FindPropertyInfo(const TArray<FEQSPropertyInfo>& Infos, const TCHAR* Name)
{
	const FEQSPropertyInfo* Found = Infos.FindByPredicate(
		[Name](const FEQSPropertyInfo& Info) { return Info.Name == Name; });
	return Found ? *Found : FEQSPropertyInfo();
}

/**
 * The live UEnvQueryTest behind a committed test path.
 *
 * Only the binding fixture needs this: DataBinding is an Instanced UObject pointer, and there is no
 * text form of "bind this to a provider" a service call could take — the editor sets it through a
 * details-panel combo. Reaching the instance is how a bound property gets built to be refused.
 */
static UEnvQueryTest* FindCommittedTestInstance(const FString& AssetPath, int32 OptionIndex,
	int32 TestIndex)
{
	UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	UEnvironmentQueryGraph* Graph = Query ? Cast<UEnvironmentQueryGraph>(Query->EdGraph) : nullptr;
	if (!Graph)
	{
		return nullptr;
	}

	const TArray<UEnvironmentQueryGraphNode_Option*> Options = VibeEQS::GetOptionNodes(Graph);
	if (!Options.IsValidIndex(OptionIndex) || !Options[OptionIndex]
		|| !Options[OptionIndex]->SubNodes.IsValidIndex(TestIndex))
	{
		return nullptr;
	}

	const UAIGraphNode* SubNode = ToRawPtr(Options[OptionIndex]->SubNodes[TestIndex]);
	return SubNode ? Cast<UEnvQueryTest>(ToRawPtr(SubNode->NodeInstance)) : nullptr;
}
#endif

// Property reflection: what is tunable, what it currently says, and what a plain write does.
//
// The two facts this exists to pin down are encoding and reach. Encoding, because every value here
// is engine property text exported with the instance as its own delta — a data-provider knob reads
// as "(...DefaultValue=0.000000)" and not as "()", which is what a delta against the CDO or against
// null would produce for a struct sitting at zero, and "()" replays as whatever the CDO holds.
// Reach, because an option path describes its GENERATOR (a UEnvQueryOption exposes nothing tunable)
// while UEnvQueryTest::TestOrder is deliberately out of reach in both directions: UpdateAsset
// rewrites it on every commit, so a settable TestOrder would report success and revert.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSPropertyReflectionTest,
	"VibeUE.EQS.Properties.Reflection", kEQSTestFlags)
bool FVibeEQSPropertyReflectionTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_PropertyReflectionTest");
	FEQSScopedFixtureReset Reset(Path);
	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());
	TestFalse(TEXT("option added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1)
			.StartsWith(TEXT("ERROR")));
	const FString TestPath = UEnvQueryService::AddTest(Path, TEXT("Option[0]"),
		TEXT("EnvQueryTest_Distance"), -1);
	TestEqual(TEXT("test added"), TestPath, FString(TEXT("Option[0]/@test[0]")));

	// --- What a test exposes -------------------------------------------------------------------
	const TArray<FEQSPropertyInfo> TestProps = UEnvQueryService::GetPropertyNames(Path, TestPath);
	TestTrue(TEXT("the test exposes properties"), TestProps.Num() > 0);

	const FEQSPropertyInfo ScoringFactor = FindPropertyInfo(TestProps, TEXT("ScoringFactor"));
	TestEqual(TEXT("ScoringFactor is listed"), ScoringFactor.Name, FString(TEXT("ScoringFactor")));
	TestTrue(TEXT("ScoringFactor is a data provider"), ScoringFactor.bIsDataProvider);
	TestEqual(TEXT("and reports its struct type"), ScoringFactor.Type,
		FString(TEXT("FAIDataProviderFloatValue")));

	const FEQSPropertyInfo ScoringEquation = FindPropertyInfo(TestProps, TEXT("ScoringEquation"));
	TestEqual(TEXT("ScoringEquation is listed"), ScoringEquation.Name,
		FString(TEXT("ScoringEquation")));
	TestFalse(TEXT("ScoringEquation is a plain enum, not a provider"),
		ScoringEquation.bIsDataProvider);

	// TestOrder carries no edit specifier and UpdateAsset overwrites it from the sub-node order on
	// every commit (EnvironmentQueryGraph.cpp:95). Listing it would advertise a knob that silently
	// reverts and contradicts MoveTest.
	TestEqual(TEXT("TestOrder is not exposed"),
		FindPropertyInfo(TestProps, TEXT("TestOrder")).Name, FString());

	// --- The export encoding -------------------------------------------------------------------
	// ScoreClampMin is untouched, so every member of the struct sits at its ZERO value. Exported
	// against a null or CDO delta each of those members is elided and the whole property collapses
	// to "()" (FProperty::ExportText_Direct + TProperty_Numeric::Identical's null-B branch); only
	// the self-delta export carries the literal. This assertion is the difference.
	const FString ClampMin = UEnvQueryService::GetPropertyValue(Path, TestPath,
		TEXT("ScoreClampMin"));
	TestTrue(FString::Printf(TEXT("an all-zero provider value still exports its literal (got %s)"),
		*ClampMin), ClampMin.Contains(TEXT("DefaultValue=0")));

	// --- An option path describes its generator ------------------------------------------------
	const TArray<FEQSPropertyInfo> GeneratorProps =
		UEnvQueryService::GetPropertyNames(Path, TEXT("Option[0]"));
	TestEqual(TEXT("the option reports the generator's SearchRadius"),
		FindPropertyInfo(GeneratorProps, TEXT("SearchRadius")).Name, FString(TEXT("SearchRadius")));
	TestEqual(TEXT("and not UEnvQueryOption's own members"),
		FindPropertyInfo(GeneratorProps, TEXT("Generator")).Name, FString());

	// --- The plain-enum write path -------------------------------------------------------------
	FEQSPropertySetResult Set = UEnvQueryService::SetPropertyValue(Path, TestPath,
		TEXT("ScoringEquation"), TEXT("InverseLinear"));
	TestTrue(FString::Printf(TEXT("ScoringEquation written (%s)"), *Set.Error), Set.bSuccess);
	TestEqual(TEXT("the resolved class is the test's"), Set.ResolvedNodeClass,
		FString(TEXT("EnvQueryTest_Distance")));
	TestEqual(TEXT("and it reads back as written"), Set.ValueAfterWrite,
		FString(TEXT("InverseLinear")));
	TestEqual(TEXT("re-read independently"),
		UEnvQueryService::GetPropertyValue(Path, TestPath, TEXT("ScoringEquation")),
		FString(TEXT("InverseLinear")));

	// --- The struct write path, and the rollback -----------------------------------------------
	Set = UEnvQueryService::SetPropertyValue(Path, TestPath, TEXT("ScoringFactor"),
		TEXT("(DefaultValue=2.5)"));
	TestTrue(FString::Printf(TEXT("a full struct literal is accepted (%s)"), *Set.Error),
		Set.bSuccess);
	TestTrue(TEXT("and holds 2.5"), Set.ValueAfterWrite.Contains(TEXT("DefaultValue=2.5")));

	// A MISSPELLED member is the silent one. FProperty::ImportSingleProperty skips an unrecognised
	// name and keeps going (Property.cpp:1656-1661), so the import returns non-null having written
	// nothing at all: no syntax error, no partial write, and — because that branch logs through
	// UE_SUPPRESS(LogExec, Verbose, ...) while LogExec sits at Warning — no message either, unless
	// ImportPropertyValue raises the category to hear it. Reported as success, this is a write that
	// evaporates, and a round-trip harness feeding an exported value back through would never see it.
	Set = UEnvQueryService::SetPropertyValue(Path, TestPath, TEXT("ScoringFactor"),
		TEXT("(DefultValue=9.0)"));
	TestFalse(TEXT("a misspelled struct member is refused"), Set.bSuccess);
	TestTrue(FString::Printf(TEXT("and the error names it (%s)"), *Set.Error),
		Set.Error.Contains(TEXT("DefultValue")));
	TestTrue(TEXT("and the value is untouched"),
		UEnvQueryService::GetPropertyValue(Path, TestPath, TEXT("ScoringFactor"))
			.Contains(TEXT("DefaultValue=2.5")));

	// Setting a property to the value it already holds is NOT the same thing and must still succeed:
	// an agent replaying a desired state cannot be required to know the current one. This is the
	// assertion that rules out "detect a dropped member by diffing the value before and after".
	Set = UEnvQueryService::SetPropertyValue(Path, TestPath, TEXT("ScoringFactor"),
		TEXT("(DefaultValue=2.5)"));
	TestTrue(FString::Printf(TEXT("an idempotent write still succeeds (%s)"), *Set.Error),
		Set.bSuccess);

	// A struct import is applied member by member: this one writes DefaultValue=9.0 and only THEN
	// fails on the missing parenthesis (Class.cpp:3514-3518). Without the by-value pre-image the
	// property would be left holding 9.0 by a call that reported failure.
	Set = UEnvQueryService::SetPropertyValue(Path, TestPath, TEXT("ScoringFactor"),
		TEXT("(DefaultValue=9.0"));
	TestFalse(TEXT("a half-parsable struct literal is refused"), Set.bSuccess);
	TestFalse(TEXT("with a reason"), Set.Error.IsEmpty());
	const FString AfterRollback = UEnvQueryService::GetPropertyValue(Path, TestPath,
		TEXT("ScoringFactor"));
	TestTrue(FString::Printf(TEXT("the partial write was rolled back (got %s)"), *AfterRollback),
		AfterRollback.Contains(TEXT("DefaultValue=2.5")));
	TestFalse(TEXT("and 9.0 did not stick"), AfterRollback.Contains(TEXT("DefaultValue=9")));

	// --- The refusals --------------------------------------------------------------------------
	// "2.5" is what a caller reaches for and it is not a valid FAIDataProviderFloatValue: no opening
	// parenthesis, so the import never starts. Refused rather than silently reinterpreted.
	Set = UEnvQueryService::SetPropertyValue(Path, TestPath, TEXT("ScoringFactor"), TEXT("2.5"));
	TestFalse(TEXT("a bare number is not a data-provider value"), Set.bSuccess);
	TestTrue(TEXT("and the error points at SetDataProviderValue"),
		Set.Error.Contains(TEXT("SetDataProviderValue")));
	TestTrue(TEXT("the refusal changed nothing"),
		UEnvQueryService::GetPropertyValue(Path, TestPath, TEXT("ScoringFactor"))
			.Contains(TEXT("DefaultValue=2.5")));

	Set = UEnvQueryService::SetPropertyValue(Path, TestPath, TEXT("NoSuchProperty"), TEXT("1"));
	TestFalse(TEXT("a nonexistent property is refused"), Set.bSuccess);
	TestFalse(TEXT("with a reason"), Set.Error.IsEmpty());
	TestEqual(TEXT("naming the class that was addressed"), Set.ResolvedNodeClass,
		FString(TEXT("EnvQueryTest_Distance")));

	// The other half of the TestOrder exclusion: unlisted AND unsettable.
	Set = UEnvQueryService::SetPropertyValue(Path, TestPath, TEXT("TestOrder"), TEXT("3"));
	TestFalse(TEXT("TestOrder is not settable"), Set.bSuccess);

	TestTrue(TEXT("a nonexistent node is refused"),
		!UEnvQueryService::SetPropertyValue(Path, TEXT("Option[7]"), TEXT("SearchRadius"),
			TEXT("(DefaultValue=1.0)")).bSuccess);
	TestEqual(TEXT("and lists nothing"),
		UEnvQueryService::GetPropertyNames(Path, TEXT("Option[7]")).Num(), 0);
	TestTrue(TEXT("a missing asset is refused"),
		UEnvQueryService::GetPropertyValue(FString(kEQSTestDir) / TEXT("EQS_NoSuchAsset"),
			TestPath, TEXT("ScoringFactor")).StartsWith(TEXT("ERROR")));

	// --- Consistency with GetQuery -------------------------------------------------------------
	// Both go through the same filter and the same exporter, so an edited property must appear in
	// GetQuery's map spelled exactly as GetPropertyValue reports it.
	const TArray<FString> Enabled = TestFieldsAt(UEnvQueryService::GetQuery(Path), 0, TEXT("class"));
	TestEqual(TEXT("still one test"), Enabled.Num(), 1);
	TSharedPtr<FJsonObject> QueryRoot;
	const TSharedRef<TJsonReader<>> QueryReader =
		TJsonReaderFactory<>::Create(UEnvQueryService::GetQuery(Path));
	// Failing to parse is a failure, not a reason to skip the only cross-check between the two
	// serialisation paths — an unparseable GetQuery must not read as "nothing to compare".
	if (!FJsonSerializer::Deserialize(QueryReader, QueryRoot) || !QueryRoot.IsValid())
	{
		AddError(TEXT("GetQuery returned unparseable JSON"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	const TSharedPtr<FJsonObject>* Option = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Tests = nullptr;
	const TSharedPtr<FJsonObject>* TestObject = nullptr;
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	FString Reported;
	if (QueryRoot->TryGetArrayField(TEXT("options"), Options) && Options->Num() == 1
		&& (*Options)[0]->TryGetObject(Option) && (*Option)->TryGetArrayField(TEXT("tests"), Tests)
		&& Tests->Num() == 1 && (*Tests)[0]->TryGetObject(TestObject)
		&& (*TestObject)->TryGetObjectField(TEXT("properties"), Properties)
		&& (*Properties)->TryGetStringField(TEXT("ScoringFactor"), Reported))
	{
		TestEqual(TEXT("GetQuery reports the same text GetPropertyValue does"), Reported,
			UEnvQueryService::GetPropertyValue(Path, TestPath, TEXT("ScoringFactor")));
	}
	else
	{
		AddError(TEXT("GetQuery did not report an edited ScoringFactor on the only test"));
	}

	TestTrue(TEXT("uasset on disk"), FEQSScopedFixtureReset::FixtureFileExists(Path));
	return true;
#endif
}

// SetDataProviderValue: the reason this service is worth building.
//
// Every scoring knob on a UEnvQueryTest is an FAIDataProviderValue-derived struct rather than a
// number (EnvQueryTest.h:88-139), so the write everyone reaches for — "ScoringFactor" = "2.5" — is
// not a valid value for the property, and the struct may instead be BOUND to a provider field, in
// which case the literal is never read at runtime. This call writes the literal and refuses both
// mistakes: a property that is not one of these structs, and one whose literal would be ignored.
//
// It is deliberately NOT a wrapper that falls back to the generic write. Reverting it to one fails
// this test in two independent places: the bare "2.5" would be rejected by the struct importer, and
// the ScoringEquation refusal below would turn into a success.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSDataProviderValueTest,
	"VibeUE.EQS.Properties.DataProvider", kEQSTestFlags)
bool FVibeEQSDataProviderValueTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_DataProviderValueTest");
	FEQSScopedFixtureReset Reset(Path);
	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());
	TestFalse(TEXT("option added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1)
			.StartsWith(TEXT("ERROR")));
	const FString TestPath = UEnvQueryService::AddTest(Path, TEXT("Option[0]"),
		TEXT("EnvQueryTest_Distance"), -1);
	TestEqual(TEXT("test added"), TestPath, FString(TEXT("Option[0]/@test[0]")));

	// --- The write ------------------------------------------------------------------------------
	FEQSPropertySetResult Set = UEnvQueryService::SetDataProviderValue(Path, TestPath,
		TEXT("ScoringFactor"), TEXT("2.5"));
	TestTrue(FString::Printf(TEXT("ScoringFactor set from a bare number (%s)"), *Set.Error),
		Set.bSuccess);
	TestEqual(TEXT("on the test"), Set.ResolvedNodeClass, FString(TEXT("EnvQueryTest_Distance")));
	TestTrue(FString::Printf(TEXT("and holds the literal (got %s)"), *Set.ValueAfterWrite),
		Set.ValueAfterWrite.Contains(TEXT("DefaultValue=2.5")));

	// ValueAfterWrite is read back from the committed asset, not echoed: what went in was "2.5" and
	// what comes out is the struct's full text. An echo could not tell the two apart.
	TestFalse(TEXT("ValueAfterWrite is not the input"), Set.ValueAfterWrite == TEXT("2.5"));
	TestEqual(TEXT("and matches an independent read"), Set.ValueAfterWrite,
		UEnvQueryService::GetPropertyValue(Path, TestPath, TEXT("ScoringFactor")));

	// It survives a further unrelated commit rather than being an in-memory value the next
	// regeneration forgets.
	TestEqual(TEXT("recompiles"), UEnvQueryService::CompileAndSave(Path), FString());
	TestTrue(TEXT("still 2.5 after a recompile"),
		UEnvQueryService::GetPropertyValue(Path, TestPath, TEXT("ScoringFactor"))
			.Contains(TEXT("DefaultValue=2.5")));

	// --- A zero literal, which is where the export encoding shows -------------------------------
	// BoolValue's class default is true, so false is a real change AND the bool's zero value. Under
	// a delta export every member of the struct would now be at zero and the whole property would
	// collapse to "()" — indistinguishable from the CDO, and a lie.
	Set = UEnvQueryService::SetDataProviderValue(Path, TestPath, TEXT("BoolValue"), TEXT("false"));
	TestTrue(FString::Printf(TEXT("BoolValue set to false (%s)"), *Set.Error), Set.bSuccess);
	TestTrue(FString::Printf(TEXT("and a zero literal is still exported (got %s)"),
		*Set.ValueAfterWrite), Set.ValueAfterWrite.Contains(TEXT("DefaultValue=False")));

	// --- A generator knob, reached through the option path --------------------------------------
	Set = UEnvQueryService::SetDataProviderValue(Path, TEXT("Option[0]"), TEXT("SearchRadius"),
		TEXT("1500.0"));
	TestTrue(FString::Printf(TEXT("the generator's SearchRadius set (%s)"), *Set.Error),
		Set.bSuccess);
	TestEqual(TEXT("resolved to the generator, not the option"), Set.ResolvedNodeClass,
		FString(TEXT("EnvQueryGenerator_ActorsOfClass")));
	TestTrue(TEXT("and holds 1500"), Set.ValueAfterWrite.Contains(TEXT("DefaultValue=1500")));

	// --- The refusal that makes this a separate call --------------------------------------------
	// ScoringEquation is a plain enum. A generic write would happily accept "InverseLinear" here and
	// leave the caller believing it is a data provider.
	Set = UEnvQueryService::SetDataProviderValue(Path, TestPath, TEXT("ScoringEquation"),
		TEXT("InverseLinear"));
	TestFalse(TEXT("a non-provider property is refused"), Set.bSuccess);
	TestTrue(TEXT("and the error says so"), Set.Error.Contains(TEXT("not a data-provider value")));
	TestEqual(TEXT("and nothing was written"),
		UEnvQueryService::GetPropertyValue(Path, TestPath, TEXT("ScoringEquation")),
		FString(TEXT("Linear")));

	Set = UEnvQueryService::SetDataProviderValue(Path, TestPath, TEXT("NoSuchProperty"),
		TEXT("1.0"));
	TestFalse(TEXT("a nonexistent property is refused"), Set.bSuccess);
	TestFalse(TEXT("with a reason"), Set.Error.IsEmpty());

	// --- The refusal that protects a binding ----------------------------------------------------
	UEnvQueryTest* TestInstance = FindCommittedTestInstance(Path, 0, 0);
	if (!TestInstance)
	{
		AddError(TEXT("could not reach the committed UEnvQueryTest to build the binding fixture"));
		return false;
	}

	// Built at the instance because there is no text form of a binding: DataBinding is an Instanced
	// UObject the details panel constructs. This is the shape SetDataProviderValue must refuse.
	TestInstance->ScoringFactor.DataBinding =
		NewObject<UAIDataProvider_QueryParams>(TestInstance);
	TestInstance->ScoringFactor.DataField = FName(TEXT("FloatValue"));

	Set = UEnvQueryService::SetDataProviderValue(Path, TestPath, TEXT("ScoringFactor"),
		TEXT("7.0"));
	TestFalse(TEXT("a bound property is refused"), Set.bSuccess);
	TestTrue(TEXT("and the error says it is bound"), Set.Error.Contains(TEXT("bound to a data provider")));
	TestTrue(TEXT("the binding is intact"),
		ToRawPtr(TestInstance->ScoringFactor.DataBinding) != nullptr);
	TestEqual(TEXT("and DefaultValue was not touched"), TestInstance->ScoringFactor.DefaultValue,
		2.5f);

	// Clearing the binding makes the very same call succeed, which is what proves the refusal was
	// about the binding rather than about the property.
	TestInstance->ScoringFactor.DataBinding = nullptr;
	Set = UEnvQueryService::SetDataProviderValue(Path, TestPath, TEXT("ScoringFactor"),
		TEXT("7.0"));
	TestTrue(FString::Printf(TEXT("unbound, the same write succeeds (%s)"), *Set.Error),
		Set.bSuccess);
	TestTrue(TEXT("and holds 7"), Set.ValueAfterWrite.Contains(TEXT("DefaultValue=7")));

	TestTrue(TEXT("uasset on disk"), FEQSScopedFixtureReset::FixtureFileExists(Path));
	return true;
#endif
}

#if WITH_VIBEUE_EQS
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ActorsOfClass.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_SimpleGrid.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Distance.h"

/** Does any diagnostic contain Fragment? */
static bool HasIssue(const TArray<FString>& Issues, const TCHAR* Fragment)
{
	return Issues.ContainsByPredicate(
		[Fragment](const FString& Issue) { return Issue.Contains(Fragment); });
}

/** Every diagnostic on one line, for a failure message that says what was actually reported. */
static FString JoinIssues(const TArray<FString>& Issues)
{
	return Issues.Num() ? FString::Join(Issues, TEXT(" | ")) : FString(TEXT("<none>"));
}

/**
 * Reduce a graph to its root alone while leaving UEnvQuery::Options populated — the sparse shape.
 *
 * Both halves are required and for different reasons. Breaking the root's links is what makes
 * GetOptionNodes (and therefore every path in this service) see nothing. REMOVING the option nodes
 * from Graph->Nodes is what makes the shape repairable: UEnvironmentQueryGraph::SpawnMissingNodes
 * scans Nodes first and adds every option it finds an existing node for to ExistingNodes, then skips
 * exactly those when rebuilding (EnvironmentQueryGraph.cpp:381-412) — so an option node left in the
 * array but unlinked is never relinked, and the query stays broken through any number of repairs.
 *
 * Deliberately not RemoveNode / DestroyNode / BreakAllNodeLinks: each fires
 * UAIGraphNode::NodeConnectionListChanged -> UpdateAsset(), which would empty Options on the spot and
 * destroy the very fixture being built. BreakAllPinLinks defaults to bNotifyNodes = false.
 */
static void MakeGraphSparseForTest(UEnvironmentQueryGraph* Graph)
{
	if (!Graph)
	{
		return;
	}
	if (UEnvironmentQueryGraphNode_Root* Root = VibeEQS::FindRootNode(Graph))
	{
		if (Root->Pins.Num() > 0 && Root->Pins[0])
		{
			Root->Pins[0]->BreakAllPinLinks();
		}
	}
	Graph->Nodes.RemoveAll([](UEdGraphNode* Node)
	{
		return Node && Node->IsA<UEnvironmentQueryGraphNode_Option>();
	});
}

/**
 * Break one option node's link to the root while LEAVING the node in Graph->Nodes — the partial
 * form of the sparse shape, and the one that is not repairable.
 *
 * The node stays in Nodes, so SpawnMissingNodes finds its option in ExistingNodes and skips it
 * (EnvironmentQueryGraph.cpp:381-413); the root never gets a link back. Meanwhile UpdateAsset
 * rebuilds a SHORTER option list, which is what makes the next open refuse and poison.
 *
 * BreakAllPinLinks defaults to bNotifyNodes = false, so nothing calls UpdateAsset here.
 */
static void UnlinkOptionNodeForTest(UEnvironmentQueryGraphNode_Option* OptionNode)
{
	if (!OptionNode)
	{
		return;
	}
	for (UEdGraphPin* Pin : OptionNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input)
		{
			Pin->BreakAllPinLinks();
		}
	}
}

/** Put an unlinked option node back on the root's output pin. */
static void RelinkOptionNodeForTest(UEnvironmentQueryGraph* Graph,
	UEnvironmentQueryGraphNode_Option* OptionNode)
{
	UEnvironmentQueryGraphNode_Root* Root = VibeEQS::FindRootNode(Graph);
	if (!Root || Root->Pins.Num() == 0 || !Root->Pins[0] || !OptionNode)
	{
		return;
	}
	for (UEdGraphPin* Pin : OptionNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input)
		{
			Root->Pins[0]->MakeLinkTo(Pin);
			break;
		}
	}
}

/** The live editor graph of an already-created fixture, without going through a write path. */
static UEnvironmentQueryGraph* LoadFixtureGraph(const FString& AssetPath)
{
	UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	return Query ? Cast<UEnvironmentQueryGraph>(Query->EdGraph) : nullptr;
}
#endif

// ValidateQuery: every diagnostic built as a real defect, and — the assertion that makes the rest
// worth anything — a healthy query reporting NOTHING.
//
// A validator that always says something is as useless as one that never does, and the specific
// trap here is the context check: a null TSubclassOf<UEnvQueryContext> is not a defect by itself
// (UEnvQueryGenerator_ProjectedPoints::NavDataOverrideContext is legitimately empty on every healthy
// asset), so the check compares against the class default. The SimpleGrid option below exists purely
// to hold such a legitimately-null context while the healthy assertion runs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSValidateTest,
	"VibeUE.EQS.Validate.Diagnostics", kEQSTestFlags)
bool FVibeEQSValidateTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_ValidateTest");
	FEQSScopedFixtureReset Reset(Path);
	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());
	TestFalse(TEXT("option added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1)
			.StartsWith(TEXT("ERROR")));
	TestEqual(TEXT("test added"),
		UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryTest_Distance"), -1),
		FString(TEXT("Option[0]/@test[0]")));
	// The discriminating option: UEnvQueryGenerator_SimpleGrid inherits NavDataOverrideContext, a
	// context reference its CDO leaves null. A bare "context is null" check would report it here.
	TestFalse(TEXT("grid option added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_SimpleGrid"), -1)
			.StartsWith(TEXT("ERROR")));

	// --- Healthy: nothing at all ----------------------------------------------------------------
	TArray<FString> Issues = UEnvQueryService::ValidateQuery(Path);
	TestEqual(FString::Printf(TEXT("a healthy query reports nothing (got: %s)"), *JoinIssues(Issues)),
		Issues.Num(), 0);

	// Reach the live graph. Read-only: nothing below commits, and each defect is undone before the
	// next is built, so every assertion sees exactly one thing wrong.
	UEnvironmentQueryGraph* Graph = LoadFixtureGraph(Path);
	TestNotNull(TEXT("graph reachable"), Graph);
	TArray<UEnvironmentQueryGraphNode_Option*> OptionNodes = VibeEQS::GetOptionNodes(Graph);
	if (!Graph || OptionNodes.Num() != 2 || !OptionNodes[0] || OptionNodes[0]->SubNodes.Num() != 1)
	{
		AddError(TEXT("fixture did not build the expected two options and one test"));
		return false;
	}

	UEnvironmentQueryGraphNode_Option* OptionNode = OptionNodes[0];
	UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(ToRawPtr(OptionNode->NodeInstance));
	UEnvironmentQueryGraphNode* TestNode =
		Cast<UEnvironmentQueryGraphNode>(ToRawPtr(OptionNode->SubNodes[0]));
	UEnvQueryGenerator_ActorsOfClass* Generator = OptionInstance
		? Cast<UEnvQueryGenerator_ActorsOfClass>(ToRawPtr(OptionInstance->Generator)) : nullptr;
	UEnvQueryTest_Distance* TestInstance = TestNode
		? Cast<UEnvQueryTest_Distance>(ToRawPtr(TestNode->NodeInstance)) : nullptr;
	if (!OptionInstance || !TestNode || !Generator || !TestInstance)
	{
		AddError(TEXT("fixture did not expose the generator and test instances"));
		return false;
	}

	// --- Unresolved context, on a generator and on a test ---------------------------------------
	Generator->SearchCenter = nullptr;
	TestInstance->DistanceTo = nullptr;
	Issues = UEnvQueryService::ValidateQuery(Path);
	TestTrue(FString::Printf(TEXT("generator context reported (got: %s)"), *JoinIssues(Issues)),
		HasIssue(Issues, TEXT("Option[0]: unresolved context — 'SearchCenter'")));
	TestTrue(FString::Printf(TEXT("test context reported (got: %s)"), *JoinIssues(Issues)),
		HasIssue(Issues, TEXT("Option[0]/@test[0]: unresolved context — 'DistanceTo'")));
	// The legitimately-null one is still not reported, with the check demonstrably live.
	TestFalse(TEXT("an optional null context stays unreported"),
		HasIssue(Issues, TEXT("NavDataOverrideContext")));
	AddInfo(FString::Printf(TEXT("context diagnostics: %s"), *JoinIssues(Issues)));
	Generator->SearchCenter = UEnvQueryContext_Querier::StaticClass();
	TestInstance->DistanceTo = UEnvQueryContext_Querier::StaticClass();
	TestEqual(TEXT("restored"), UEnvQueryService::ValidateQuery(Path).Num(), 0);

	// --- An option carrying no generator --------------------------------------------------------
	UEnvQueryGenerator* SavedGenerator = ToRawPtr(OptionInstance->Generator);
	OptionInstance->Generator = nullptr;
	Issues = UEnvQueryService::ValidateQuery(Path);
	TestTrue(FString::Printf(TEXT("null generator reported (got: %s)"), *JoinIssues(Issues)),
		HasIssue(Issues, TEXT("Option[0]: no generator")));
	OptionInstance->Generator = SavedGenerator;
	TestEqual(TEXT("restored"), UEnvQueryService::ValidateQuery(Path).Num(), 0);

	// --- A node whose class failed to load: the engine's own ErrorMessage ------------------------
	TestNode->ErrorMessage = TEXT("Class EnvQueryTest_Gone not found");
	Issues = UEnvQueryService::ValidateQuery(Path);
	TestTrue(FString::Printf(TEXT("node error reported (got: %s)"), *JoinIssues(Issues)),
		HasIssue(Issues, TEXT("Option[0]/@test[0]: node class failed to load — Class EnvQueryTest_Gone not found")));
	TestNode->ErrorMessage.Reset();

	// --- ... and its other half: UAIGraphNode::HasErrors() is also true with no instance ---------
	TestNode->NodeInstance = nullptr;
	Issues = UEnvQueryService::ValidateQuery(Path);
	TestTrue(FString::Printf(TEXT("instance-less node reported (got: %s)"), *JoinIssues(Issues)),
		HasIssue(Issues, TEXT("Option[0]/@test[0]: node class failed to load — the node carries no instance")));
	TestNode->NodeInstance = TestInstance;
	TestEqual(TEXT("restored"), UEnvQueryService::ValidateQuery(Path).Num(), 0);

	// --- The cycle guard ------------------------------------------------------------------------
	// A sub-node list containing its own owner. Nothing in UAIGraphNode forbids it, and without the
	// visited set the walk recurses until the stack runs out — on the exact class of malformed graph
	// this call exists to diagnose. Reaching the line after ValidateQuery IS the assertion.
	OptionNode->SubNodes.Add(OptionNode);
	Issues = UEnvQueryService::ValidateQuery(Path);
	OptionNode->SubNodes.Pop();
	AddInfo(FString::Printf(TEXT("cycle walk terminated with %d diagnostic(s)"), Issues.Num()));
	TestEqual(TEXT("the graph is intact after the cycle walk"), OptionNode->SubNodes.Num(), 1);

	// --- The PARTIAL sparse shape: an option the graph can no longer rebuild ---------------------
	// The shape that would otherwise validate clean while every mutator refused. One option node
	// unlinked but still in Graph->Nodes: the whole-graph sparse test sees a non-zero option-node
	// count and stays quiet, the remaining node walks clean, and the caller is told the query is
	// healthy — while the next write rebuilds 1 option over a query that has 2, is refused, and
	// leaves the in-memory copy permanently un-writable.
	UnlinkOptionNodeForTest(OptionNodes[1]);
	TestEqual(TEXT("one option node still linked"), VibeEQS::GetOptionNodes(Graph).Num(), 1);

	Issues = UEnvQueryService::ValidateQuery(Path);
	TestTrue(FString::Printf(TEXT("the orphaned option is named (got: %s)"), *JoinIssues(Issues)),
		HasIssue(Issues,
			TEXT("orphaned option: UEnvQuery::Options[1] (EnvQueryGenerator_SimpleGrid)")));
	// Not the whole-graph diagnostic: one option IS still reachable, and saying the graph is empty
	// would send the caller to RepairGraphFromOptions, which cannot fix this shape.
	TestFalse(TEXT("and it is not reported as a wholly sparse graph"),
		HasIssue(Issues, TEXT("sparse graph")));
	AddInfo(FString::Printf(TEXT("orphan diagnostic: %s"), *JoinIssues(Issues)));

	RelinkOptionNodeForTest(Graph, OptionNodes[1]);
	TestEqual(TEXT("both option nodes linked again"), VibeEQS::GetOptionNodes(Graph).Num(), 2);
	TestEqual(TEXT("restored"), UEnvQueryService::ValidateQuery(Path).Num(), 0);

	// --- The sparse shape, and the read-only requirement -----------------------------------------
	const UEnvQuery* const FixtureQuery =
		LoadObject<UEnvQuery>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const int32 OptionsBeforeValidate = FixtureQuery ? FixtureQuery->GetOptions().Num() : -1;
	TestEqual(TEXT("two options on the query itself"), OptionsBeforeValidate, 2);
	MakeGraphSparseForTest(Graph);
	TestEqual(TEXT("graph really is sparse"), VibeEQS::GetOptionNodes(Graph).Num(), 0);

	Issues = UEnvQueryService::ValidateQuery(Path);
	TestTrue(FString::Printf(TEXT("sparse graph reported (got: %s)"), *JoinIssues(Issues)),
		HasIssue(Issues, TEXT("sparse graph")));
	// One line, not one per option: the total case has a single cause and a single cure, and
	// burying that under an orphan report per option would hide it.
	TestFalse(TEXT("and not also once per option"), HasIssue(Issues, TEXT("orphaned option")));
	AddInfo(FString::Printf(TEXT("sparse diagnostic: %s"), *JoinIssues(Issues)));

	// The whole point of not routing this through OpenWriteGuard: that path's EnsureGraph runs
	// Initialize() -> SpawnMissingNodes(), which would REPAIR the graph as a side effect of being
	// asked about it, and the second call would then report a healthy query.
	TestEqual(TEXT("validating did not rebuild the option nodes"),
		VibeEQS::GetOptionNodes(Graph).Num(), 0);
	TestTrue(TEXT("and says so again on a second read"),
		HasIssue(UEnvQueryService::ValidateQuery(Path), TEXT("sparse graph")));
	const UEnvQuery* AfterValidate =
		LoadObject<UEnvQuery>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	TestEqual(TEXT("the options themselves were never touched"),
		AfterValidate ? AfterValidate->GetOptions().Num() : -1, OptionsBeforeValidate);

	// --- A path that is not a query -------------------------------------------------------------
	const TArray<FString> Missing =
		UEnvQueryService::ValidateQuery(FString(kEQSTestDir) / TEXT("EQS_NoSuchAsset"));
	TestEqual(TEXT("a missing asset is one diagnostic"), Missing.Num(), 1);
	TestTrue(TEXT("naming the asset"), HasIssue(Missing, TEXT("not found")));

	TestTrue(TEXT("uasset on disk"), FEQSScopedFixtureReset::FixtureFileExists(Path));
	return true;
#endif
}

// RepairGraphFromOptions: the cure the sparse diagnostic names, and its refusals.
//
// The finding this test records is that on EQS the reconstruction is NOT exclusive to this call:
// UEnvironmentQueryGraph::Initialize() IS SpawnMissingNodes() among other things
// (EnvironmentQueryGraph.cpp:168-176), so every write path through EnsureGraph heals a sparse
// query. UBehaviorTreeGraph::Initialize() is Super + UpdateInjectedNodes and rebuilds nothing, which
// is the actual BT/EQS divergence — both editors call Initialize() unconditionally. So CompileAndSave
// heals a sparse query too, and that is asserted below rather than papered over: what this call adds
// is the refusals and the before/after report.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSRepairGraphTest,
	"VibeUE.EQS.Validate.RepairGraphFromOptions", kEQSTestFlags)
bool FVibeEQSRepairGraphTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_RepairTest");
	FEQSScopedFixtureReset Reset(Path);
	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());
	TestFalse(TEXT("first option added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1)
			.StartsWith(TEXT("ERROR")));
	TestFalse(TEXT("second option added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_SimpleGrid"), -1)
			.StartsWith(TEXT("ERROR")));
	TestEqual(TEXT("test added"),
		UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryTest_Distance"), -1),
		FString(TEXT("Option[0]/@test[0]")));
	// An authored value, so "the options survived" can mean the objects rather than the count.
	TestTrue(TEXT("scoring factor authored"),
		UEnvQueryService::SetDataProviderValue(Path, TEXT("Option[0]/@test[0]"),
			TEXT("ScoringFactor"), TEXT("3.25")).bSuccess);

	FEQSQueryInfo Before;
	TestTrue(TEXT("info readable"), UEnvQueryService::GetQueryInfo(Path, Before));
	TestEqual(TEXT("two options"), Before.OptionCount, 2);
	TestEqual(TEXT("one test"), Before.TestCount, 1);

	// --- Refused when there is nothing to repair ------------------------------------------------
	const FString HealthyRefusal = UEnvQueryService::RepairGraphFromOptions(Path);
	TestFalse(TEXT("a healthy query is refused"), HealthyRefusal.IsEmpty());
	TestTrue(TEXT("saying the graph already has option nodes"),
		HealthyRefusal.Contains(TEXT("already feeds")));
	AddInfo(FString::Printf(TEXT("healthy refusal: %s"), *HealthyRefusal));

	const FString EmptyPath = FString(kEQSTestDir) / TEXT("EQS_RepairEmptyTest");
	FEQSScopedFixtureReset EmptyReset(EmptyPath);
	TestEqual(TEXT("empty query created"), UEnvQueryService::CreateQuery(EmptyPath), FString());
	const FString EmptyRefusal = UEnvQueryService::RepairGraphFromOptions(EmptyPath);
	TestFalse(TEXT("a query with no options is refused"), EmptyRefusal.IsEmpty());
	TestTrue(TEXT("saying there is nothing to rebuild from"),
		EmptyRefusal.Contains(TEXT("no options")));

	TestFalse(TEXT("a missing asset is refused"),
		UEnvQueryService::RepairGraphFromOptions(FString(kEQSTestDir) / TEXT("EQS_NoSuchAsset"))
			.IsEmpty());

	// --- Identity snapshot, taken before anything is broken -------------------------------------
	UEnvironmentQueryGraph* Graph = LoadFixtureGraph(Path);
	TestNotNull(TEXT("graph reachable"), Graph);
	TArray<UEnvironmentQueryGraphNode_Option*> Nodes = VibeEQS::GetOptionNodes(Graph);
	if (!Graph || Nodes.Num() != 2 || !Nodes[0] || Nodes[0]->SubNodes.Num() != 1)
	{
		AddError(TEXT("fixture did not build the expected two options and one test"));
		return false;
	}
	UEnvQueryOption* const Option0 = Cast<UEnvQueryOption>(ToRawPtr(Nodes[0]->NodeInstance));
	UEnvQueryOption* const Option1 = Cast<UEnvQueryOption>(ToRawPtr(Nodes[1]->NodeInstance));
	const UAIGraphNode* const FirstTestNode = ToRawPtr(Nodes[0]->SubNodes[0]);
	UObject* const Test0 = FirstTestNode ? ToRawPtr(FirstTestNode->NodeInstance) : nullptr;
	TestNotNull(TEXT("option instances captured"), Option0);
	TestNotNull(TEXT("test instance captured"), Test0);

	// --- The world we are actually in: an ordinary write heals it too ---------------------------
	// EnsureGraph -> Initialize() -> SpawnMissingNodes runs on every write path, so a mutator is NOT
	// refused on a sparse query the way the BehaviorTree side would suggest. Asserted, because a
	// silent divergence between the doc comment and the behaviour is the whole failure mode this
	// task exists to close.
	MakeGraphSparseForTest(Graph);
	TestEqual(TEXT("sparse"), VibeEQS::GetOptionNodes(Graph).Num(), 0);
	TestEqual(TEXT("an ordinary commit repairs it as a side effect"),
		UEnvQueryService::CompileAndSave(Path), FString());
	TestEqual(TEXT("and the option nodes came back"), VibeEQS::GetOptionNodes(Graph).Num(), 2);

	// --- The repair proper ----------------------------------------------------------------------
	MakeGraphSparseForTest(Graph);
	TestTrue(TEXT("ValidateQuery calls it sparse"),
		HasIssue(UEnvQueryService::ValidateQuery(Path), TEXT("sparse graph")));
	TestEqual(TEXT("GetQuery sees no options at all"),
		OptionGuidsInOrder(UEnvQueryService::GetQuery(Path)).Num(), 0);

	const FString RepairError = UEnvQueryService::RepairGraphFromOptions(Path);
	TestEqual(FString::Printf(TEXT("repair succeeds (%s)"), *RepairError), RepairError, FString());

	FEQSQueryInfo After;
	TestTrue(TEXT("info readable after repair"), UEnvQueryService::GetQueryInfo(Path, After));
	TestEqual(TEXT("option count restored"), After.OptionCount, Before.OptionCount);
	TestEqual(TEXT("test count restored"), After.TestCount, Before.TestCount);

	const FString Json = UEnvQueryService::GetQuery(Path);
	TestEqual(TEXT("both options addressable again"), OptionGuidsInOrder(Json).Num(), 2);
	TestEqual(TEXT("first generator intact"), OptionGeneratorAt(Json, 0),
		FString(TEXT("EnvQueryGenerator_ActorsOfClass")));
	TestEqual(TEXT("second generator intact"), OptionGeneratorAt(Json, 1),
		FString(TEXT("EnvQueryGenerator_SimpleGrid")));
	TestEqual(TEXT("the test came back with it"), TestClassesAt(Json, 0),
		TArray<FString>{ TEXT("EnvQueryTest_Distance") });
	TestEqual(TEXT("validating a repaired query reports nothing"),
		UEnvQueryService::ValidateQuery(Path).Num(), 0);

	// Identity, not just counts: SpawnMissingNodes assigns MyNode->NodeInstance = OptionInstance
	// (EnvironmentQueryGraph.cpp:438), so the repaired graph must point at the SAME objects. New
	// objects with the same class names would satisfy every assertion above and would have silently
	// discarded every property ever authored.
	UEnvironmentQueryGraph* RepairedGraph = LoadFixtureGraph(Path);
	const TArray<UEnvironmentQueryGraphNode_Option*> Repaired = VibeEQS::GetOptionNodes(RepairedGraph);
	TestEqual(TEXT("two option nodes"), Repaired.Num(), 2);
	if (Repaired.Num() == 2 && Repaired[0] && Repaired[1])
	{
		TestTrue(TEXT("option 0 is the same object"),
			ToRawPtr(Repaired[0]->NodeInstance) == Option0);
		TestTrue(TEXT("option 1 is the same object"),
			ToRawPtr(Repaired[1]->NodeInstance) == Option1);
		TestEqual(TEXT("its test came back"), Repaired[0]->SubNodes.Num(), 1);
		if (Repaired[0]->SubNodes.Num() == 1)
		{
			const UAIGraphNode* Sub = ToRawPtr(Repaired[0]->SubNodes[0]);
			TestTrue(TEXT("the test is the same object"),
				Sub && ToRawPtr(Sub->NodeInstance) == Test0);
		}
	}
	// And the authored value rode along, which is what "reused" means to a caller.
	TestTrue(TEXT("the authored ScoringFactor survived"),
		UEnvQueryService::GetPropertyValue(Path, TEXT("Option[0]/@test[0]"), TEXT("ScoringFactor"))
			.Contains(TEXT("DefaultValue=3.25")));

	// --- An ordinary commit is no longer refused, and a second repair is -------------------------
	TestEqual(TEXT("commit allowed after repair"), UEnvQueryService::CompileAndSave(Path), FString());
	const FString SecondRepair = UEnvQueryService::RepairGraphFromOptions(Path);
	TestFalse(TEXT("a second repair is refused"), SecondRepair.IsEmpty());
	TestEqual(TEXT("so nothing was duplicated"), OptionGuidsInOrder(UEnvQueryService::GetQuery(Path)).Num(), 2);

	// --- A graph with no root: refused, because repairing it would be the damage -----------------
	// EnsureGraph creates a root only when EdGraph is null, so an existing rootless graph keeps
	// none; SpawnMissingNodes would spawn option nodes with nothing to link them to and the commit
	// would rebuild an EMPTY option list, which EnsureGraph then refuses — after poisoning this
	// copy. Asking for a repair would be what broke the asset.
	UEnvironmentQueryGraph* RootlessGraph = LoadFixtureGraph(Path);
	UEnvironmentQueryGraphNode_Root* RootNode = VibeEQS::FindRootNode(RootlessGraph);
	TestNotNull(TEXT("root reachable"), RootNode);
	if (RootlessGraph && RootNode)
	{
		RootlessGraph->Nodes.Remove(RootNode);
		TestNull(TEXT("root really gone"), VibeEQS::FindRootNode(RootlessGraph));

		const FString RootlessError = UEnvQueryService::RepairGraphFromOptions(Path);
		TestFalse(TEXT("a rootless graph is refused"), RootlessError.IsEmpty());
		TestTrue(TEXT("saying the graph has no root node"),
			RootlessError.Contains(TEXT("no root node")));
		AddInfo(FString::Printf(TEXT("rootless refusal: %s"), *RootlessError));

		// The options are still there — count AND identity, because a refusal that left two
		// freshly-made empty options behind would satisfy a count on its own.
		const UEnvQuery* Survivor =
			LoadObject<UEnvQuery>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		TestEqual(TEXT("options survived the refusal"),
			Survivor ? Survivor->GetOptions().Num() : -1, 2);
		if (Survivor && Survivor->GetOptions().Num() == 2)
		{
			TestTrue(TEXT("and they are the same objects"),
				Survivor->GetOptions()[0] == Option0 && Survivor->GetOptions()[1] == Option1);
		}

		// And the copy was not poisoned: put the root back and an ordinary commit still works on
		// this very object, with no reload.
		RootlessGraph->Nodes.Add(RootNode);
		TestEqual(TEXT("nothing was poisoned by the refusal"),
			UEnvQueryService::CompileAndSave(Path), FString());
		TestEqual(TEXT("and the query is intact"),
			OptionGuidsInOrder(UEnvQueryService::GetQuery(Path)).Num(), 2);
	}

	// --- The sticky poison set ------------------------------------------------------------------
	// A repair must not be a way around it, and it cannot be — for any poisoned query, not just the
	// one built below. EnsureGraph poisons on ANY decrease, so a marked copy holds exactly the
	// options its graph could rebuild: either none, and the "no options" refusal fires, or some, and
	// those are by definition carried by root-linked nodes, so the "already feeds" refusal fires.
	// Both sit above OpenWriteGuard, so the repair never reaches the mark, never clears it, and
	// never becomes the retry that commits the loss. The fixture below is the first branch.
	const FString PoisonPath = FString(kEQSTestDir) / TEXT("EQS_RepairPoisonTest");
	FEQSScopedFixtureReset PoisonReset(PoisonPath);
	TestEqual(TEXT("poison fixture created"), UEnvQueryService::CreateQuery(PoisonPath), FString());
	UEnvQuery* PoisonQuery =
		LoadObject<UEnvQuery>(nullptr, *PoisonPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	TestNotNull(TEXT("poison fixture reachable"), PoisonQuery);
	if (PoisonQuery)
	{
		// An option the graph cannot represent: the next open drops it and marks the copy.
		PoisonQuery->GetOptionsMutable().Add(NewObject<UEnvQueryOption>(PoisonQuery));
		UEnvQuery* Ignored = nullptr;
		UEnvironmentQueryGraph* IgnoredGraph = nullptr;
		TestFalse(TEXT("the open that poisons is refused"),
			VibeEQS::OpenWriteGuard(PoisonPath, Ignored, IgnoredGraph).IsEmpty());

		const FString PoisonRepair = UEnvQueryService::RepairGraphFromOptions(PoisonPath);
		TestFalse(TEXT("repair on a poisoned query is refused"), PoisonRepair.IsEmpty());
		TestFalse(TEXT("and the poison is not cleared by it"),
			UEnvQueryService::CompileAndSave(PoisonPath).IsEmpty());
		AddInfo(FString::Printf(TEXT("poisoned repair refusal: %s"), *PoisonRepair));
	}

	TestTrue(TEXT("uasset on disk"), FEQSScopedFixtureReset::FixtureFileExists(Path));
	return true;
#endif
}

#if WITH_VIBEUE_EQS
#include "Serialization/JsonWriter.h"

/** A GetQuery result as an object, or invalid. */
static TSharedPtr<FJsonObject> ParseQueryForTest(const FString& QueryJson)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(QueryJson);
	if (!FJsonSerializer::Deserialize(Reader, Root))
	{
		return nullptr;
	}
	return Root;
}

static FString SerialiseForTest(const TSharedRef<FJsonObject>& Object)
{
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Object, Writer);
	return Out;
}

/** QueryJson with one mutation applied and reserialised — the comparator's negative controls. */
static FString MutatedQueryForTest(const FString& QueryJson,
	TFunctionRef<void(const TSharedRef<FJsonObject>&)> Mutate)
{
	TSharedPtr<FJsonObject> Root = ParseQueryForTest(QueryJson);
	if (!Root.IsValid())
	{
		return FString();
	}
	Mutate(Root.ToSharedRef());
	return SerialiseForTest(Root.ToSharedRef());
}

/** The mutable option object at Index, or invalid. */
static TSharedPtr<FJsonObject> OptionObjectForTest(const TSharedRef<FJsonObject>& Root, int32 Index)
{
	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	if (!Root->TryGetArrayField(TEXT("options"), Options) || Options == nullptr
		|| !Options->IsValidIndex(Index) || !(*Options)[Index].IsValid())
	{
		return nullptr;
	}
	const TSharedPtr<FJsonObject>* Option = nullptr;
	return ((*Options)[Index]->TryGetObject(Option) && Option) ? *Option : nullptr;
}

/** The mutable test object at OptionIndex/TestIndex, or invalid. */
static TSharedPtr<FJsonObject> TestObjectForTest(const TSharedRef<FJsonObject>& Root,
	int32 OptionIndex, int32 TestIndex)
{
	const TSharedPtr<FJsonObject> Option = OptionObjectForTest(Root, OptionIndex);
	const TArray<TSharedPtr<FJsonValue>>* Tests = nullptr;
	if (!Option.IsValid() || !Option->TryGetArrayField(TEXT("tests"), Tests) || Tests == nullptr
		|| !Tests->IsValidIndex(TestIndex) || !(*Tests)[TestIndex].IsValid())
	{
		return nullptr;
	}
	const TSharedPtr<FJsonObject>* TestObject = nullptr;
	return ((*Tests)[TestIndex]->TryGetObject(TestObject) && TestObject) ? *TestObject : nullptr;
}

/** Every guid in a query — options and tests — so two assets can be checked for shared identity. */
static TArray<FString> AllGuidsForTest(const FString& QueryJson)
{
	TArray<FString> Guids = OptionGuidsInOrder(QueryJson);
	// Snapshot the option count before appending: the loop below indexes OPTIONS, and Guids grows.
	const int32 OptionCount = Guids.Num();
	for (int32 Index = 0; Index < OptionCount; ++Index)
	{
		Guids.Append(TestFieldsAt(QueryJson, Index, TEXT("guid")));
	}
	return Guids;
}

/**
 * Compare two "properties" maps in BOTH directions.
 *
 * One direction is not enough and the asymmetry is the whole point: a replay that DROPPED a property
 * passes a forward-only check trivially (the target simply has fewer keys), and a replay that wrote
 * something extra passes a reverse-only one. GetQuery emits only properties differing from the class
 * default, so an extra key means the build wrote a value the source never had.
 */
static bool PropertiesMatchForTest(const TSharedPtr<FJsonObject>* A, const TSharedPtr<FJsonObject>* B,
	const FString& Where, FString& OutDiff)
{
	const TSharedRef<FJsonObject> Empty = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject> Left = (A && A->IsValid()) ? *A : TSharedPtr<FJsonObject>(Empty);
	const TSharedPtr<FJsonObject> Right = (B && B->IsValid()) ? *B : TSharedPtr<FJsonObject>(Empty);

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Left->Values)
	{
		FString LeftValue;
		if (Pair.Value.IsValid())
		{
			Pair.Value->TryGetString(LeftValue);
		}

		FString RightValue;
		if (!Right->TryGetStringField(Pair.Key, RightValue))
		{
			OutDiff = FString::Printf(TEXT("%s: property '%s' is missing from the second query "
				"(first holds %s)"), *Where, *Pair.Key, *LeftValue);
			return false;
		}
		if (LeftValue != RightValue)
		{
			OutDiff = FString::Printf(TEXT("%s: property '%s' is %s in the first query and %s in the "
				"second"), *Where, *Pair.Key, *LeftValue, *RightValue);
			return false;
		}
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Right->Values)
	{
		if (!Left->HasField(Pair.Key))
		{
			FString RightValue;
			if (Pair.Value.IsValid())
			{
				Pair.Value->TryGetString(RightValue);
			}
			OutDiff = FString::Printf(TEXT("%s: the second query has an extra property '%s' (%s)"),
				*Where, *Pair.Key, *RightValue);
			return false;
		}
	}

	return true;
}

/**
 * THE round-trip comparator: two GetQuery results are the same query, ignoring only "guid".
 *
 * Strict on purpose, and every field listed here is one a "both parse and have two options" check
 * would miss while the query did something else at runtime:
 *   - option ORDER and generator class — option order is execution order;
 *   - test ORDER and test class — test order is the order tests run, and a filtering test that
 *     moved to the end costs a query everything it was there to save;
 *   - "enabled" — a graph-node field no property replay can carry, so a build that ignored it
 *     produces a query whose properties all match and whose tests all run;
 *   - "properties", both directions, on options AND tests.
 *
 * Two fields are excluded. "guid" is node identity, and two different assets must not share it
 * (asserted separately). "path" is vacuous by construction — GetQuery derives it from the index this
 * loop is already walking, so it can only ever agree — and comparing it would mask a real reordering
 * behind a path mismatch instead of naming the generator that moved.
 */
static bool QueriesMatchIgnoringGuid(const FString& JsonA, const FString& JsonB, FString& OutDiff)
{
	OutDiff.Reset();

	const TSharedPtr<FJsonObject> RootA = ParseQueryForTest(JsonA);
	const TSharedPtr<FJsonObject> RootB = ParseQueryForTest(JsonB);
	if (!RootA.IsValid() || !RootB.IsValid())
	{
		OutDiff = TEXT("one of the two results is not parseable JSON");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* OptionsA = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* OptionsB = nullptr;
	if (!RootA->TryGetArrayField(TEXT("options"), OptionsA) || OptionsA == nullptr
		|| !RootB->TryGetArrayField(TEXT("options"), OptionsB) || OptionsB == nullptr)
	{
		OutDiff = TEXT("one of the two results has no \"options\" array");
		return false;
	}

	if (OptionsA->Num() != OptionsB->Num())
	{
		OutDiff = FString::Printf(TEXT("option count: %d vs %d"), OptionsA->Num(), OptionsB->Num());
		return false;
	}

	for (int32 Index = 0; Index < OptionsA->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> OptionA = OptionObjectForTest(RootA.ToSharedRef(), Index);
		const TSharedPtr<FJsonObject> OptionB = OptionObjectForTest(RootB.ToSharedRef(), Index);
		if (!OptionA.IsValid() || !OptionB.IsValid())
		{
			OutDiff = FString::Printf(TEXT("option %d is not an object on one side"), Index);
			return false;
		}

		const FString Where = FString::Printf(TEXT("Option[%d]"), Index);

		FString GeneratorA;
		FString GeneratorB;
		OptionA->TryGetStringField(TEXT("generator"), GeneratorA);
		OptionB->TryGetStringField(TEXT("generator"), GeneratorB);
		if (GeneratorA != GeneratorB)
		{
			OutDiff = FString::Printf(TEXT("%s: \"generator\" is '%s' vs '%s'"), *Where,
				*GeneratorA, *GeneratorB);
			return false;
		}

		const TSharedPtr<FJsonObject>* PropertiesA = nullptr;
		const TSharedPtr<FJsonObject>* PropertiesB = nullptr;
		OptionA->TryGetObjectField(TEXT("properties"), PropertiesA);
		OptionB->TryGetObjectField(TEXT("properties"), PropertiesB);
		if (!PropertiesMatchForTest(PropertiesA, PropertiesB, Where, OutDiff))
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* TestsA = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* TestsB = nullptr;
		OptionA->TryGetArrayField(TEXT("tests"), TestsA);
		OptionB->TryGetArrayField(TEXT("tests"), TestsB);
		const int32 CountA = TestsA ? TestsA->Num() : 0;
		const int32 CountB = TestsB ? TestsB->Num() : 0;
		if (CountA != CountB)
		{
			OutDiff = FString::Printf(TEXT("%s: test count %d vs %d"), *Where, CountA, CountB);
			return false;
		}

		for (int32 TestIndex = 0; TestIndex < CountA; ++TestIndex)
		{
			const TSharedPtr<FJsonObject> TestA =
				TestObjectForTest(RootA.ToSharedRef(), Index, TestIndex);
			const TSharedPtr<FJsonObject> TestB =
				TestObjectForTest(RootB.ToSharedRef(), Index, TestIndex);
			if (!TestA.IsValid() || !TestB.IsValid())
			{
				OutDiff = FString::Printf(TEXT("%s/@test[%d] is not an object on one side"),
					*Where, TestIndex);
				return false;
			}

			const FString TestWhere = FString::Printf(TEXT("%s/@test[%d]"), *Where, TestIndex);

			FString ClassA;
			FString ClassB;
			TestA->TryGetStringField(TEXT("class"), ClassA);
			TestB->TryGetStringField(TEXT("class"), ClassB);
			if (ClassA != ClassB)
			{
				OutDiff = FString::Printf(TEXT("%s: \"class\" is '%s' vs '%s'"), *TestWhere,
					*ClassA, *ClassB);
				return false;
			}

			// Missing reads as enabled, which is what a dropped "enabled" replay looks like — so this
			// comparison catches an absent field as well as a flipped one.
			bool EnabledA = true;
			bool EnabledB = true;
			TestA->TryGetBoolField(TEXT("enabled"), EnabledA);
			TestB->TryGetBoolField(TEXT("enabled"), EnabledB);
			if (EnabledA != EnabledB)
			{
				OutDiff = FString::Printf(TEXT("%s: \"enabled\" is %s vs %s"), *TestWhere,
					EnabledA ? TEXT("true") : TEXT("false"), EnabledB ? TEXT("true") : TEXT("false"));
				return false;
			}

			const TSharedPtr<FJsonObject>* TestPropertiesA = nullptr;
			const TSharedPtr<FJsonObject>* TestPropertiesB = nullptr;
			TestA->TryGetObjectField(TEXT("properties"), TestPropertiesA);
			TestB->TryGetObjectField(TEXT("properties"), TestPropertiesB);
			if (!PropertiesMatchForTest(TestPropertiesA, TestPropertiesB, TestWhere, OutDiff))
			{
				return false;
			}
		}
	}

	return true;
}

/**
 * The fixture both the round-trip and the replace test build with the PRIMITIVES, so what BuildQuery
 * is asked to reproduce is a query authored the ordinary way rather than a JSON literal.
 *
 * Two options with different generators (so option order is observable), three tests (so test order
 * is), one of them DISABLED (the field no property replay can carry), and a tuned value on every
 * node — one plain enum and four data-provider literals, which are the two routes BuildQuery has to
 * tell apart. Empty on success, else the first failure.
 */
static FString MakeBuildFixtureQuery(const FString& Path)
{
	const FString CreateError = UEnvQueryService::CreateQuery(Path);
	if (!CreateError.IsEmpty())
	{
		return FString::Printf(TEXT("CreateQuery: %s"), *CreateError);
	}

	if (UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1)
		!= TEXT("Option[0]"))
	{
		return TEXT("AddOption(ActorsOfClass) did not return Option[0]");
	}
	if (!UEnvQueryService::SetDataProviderValue(Path, TEXT("Option[0]"), TEXT("SearchRadius"),
		TEXT("1500.0")).bSuccess)
	{
		return TEXT("SearchRadius");
	}
	if (UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryTest_Distance"), -1)
		!= TEXT("Option[0]/@test[0]"))
	{
		return TEXT("AddTest 0/0");
	}
	if (!UEnvQueryService::SetDataProviderValue(Path, TEXT("Option[0]/@test[0]"),
		TEXT("ScoringFactor"), TEXT("2.5")).bSuccess)
	{
		return TEXT("ScoringFactor 0/0");
	}
	if (UEnvQueryService::AddTest(Path, TEXT("Option[0]"), TEXT("EnvQueryTest_Distance"), -1)
		!= TEXT("Option[0]/@test[1]"))
	{
		return TEXT("AddTest 0/1");
	}
	if (!UEnvQueryService::SetPropertyValue(Path, TEXT("Option[0]/@test[1]"),
		TEXT("ScoringEquation"), TEXT("InverseLinear")).bSuccess)
	{
		return TEXT("ScoringEquation 0/1");
	}
	const FString DisableError =
		UEnvQueryService::SetTestEnabled(Path, TEXT("Option[0]/@test[1]"), false);
	if (!DisableError.IsEmpty())
	{
		return FString::Printf(TEXT("SetTestEnabled: %s"), *DisableError);
	}

	if (UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_SimpleGrid"), -1)
		!= TEXT("Option[1]"))
	{
		return TEXT("AddOption(SimpleGrid) did not return Option[1]");
	}
	if (!UEnvQueryService::SetDataProviderValue(Path, TEXT("Option[1]"), TEXT("GridSize"),
		TEXT("800.0")).bSuccess)
	{
		return TEXT("GridSize");
	}
	if (UEnvQueryService::AddTest(Path, TEXT("Option[1]"), TEXT("EnvQueryTest_Distance"), -1)
		!= TEXT("Option[1]/@test[0]"))
	{
		return TEXT("AddTest 1/0");
	}
	if (!UEnvQueryService::SetDataProviderValue(Path, TEXT("Option[1]/@test[0]"),
		TEXT("ScoringFactor"), TEXT("0.25")).bSuccess)
	{
		return TEXT("ScoringFactor 1/0");
	}

	return FString();
}
#endif

// BuildQuery's round trip: a query authored with the primitives, read with GetQuery, rebuilt into a
// SECOND asset from that JSON alone, and compared field by field.
//
// The comparison is the test. A round-trip check that merely counts options passes while the rebuilt
// query runs its tests in a different order, or runs a test the source had switched off — both of
// which change what the query returns at runtime and neither of which is an error anywhere. So the
// comparator is strict, and the second half of this test proves it discriminates by feeding it
// deliberately broken copies of the very JSON it just accepted.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSBuildRoundTripTest,
	"VibeUE.EQS.Build.RoundTrip", kEQSTestFlags)
bool FVibeEQSBuildRoundTripTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString SourcePath = FString(kEQSTestDir) / TEXT("EQS_BuildSource");
	const FString TargetPath = FString(kEQSTestDir) / TEXT("EQS_BuildTarget");
	FEQSScopedFixtureReset SourceReset(SourcePath);
	FEQSScopedFixtureReset TargetReset(TargetPath);

	const FString FixtureError = MakeBuildFixtureQuery(SourcePath);
	if (!FixtureError.IsEmpty())
	{
		AddError(FString::Printf(TEXT("fixture: %s"), *FixtureError));
		return false;
	}

	const FString SourceJson = UEnvQueryService::GetQuery(SourcePath);
	AddInfo(FString::Printf(TEXT("source: %s"), *SourceJson));

	// The fixture is only worth round-tripping if it actually carries everything the comparator
	// checks — otherwise every assertion below is vacuously true.
	TestEqual(TEXT("fixture has two options"), OptionGuidsInOrder(SourceJson).Num(), 2);
	const TArray<bool> FixtureFlags = TestEnabledFlagsAt(SourceJson, 0);
	TestEqual(TEXT("fixture option 0 has two tests"), FixtureFlags.Num(), 2);
	if (FixtureFlags.Num() == 2)
	{
		TestTrue(TEXT("the first is enabled"), FixtureFlags[0]);
		TestFalse(TEXT("and the second is DISABLED"), FixtureFlags[1]);
	}
	TestTrue(TEXT("fixture tuned a data-provider value"),
		SourceJson.Contains(TEXT("DefaultValue=2.5")));
	TestTrue(TEXT("fixture tuned a plain enum"), SourceJson.Contains(TEXT("InverseLinear")));

	// --- The build ------------------------------------------------------------------------------
	TestEqual(TEXT("target created"), UEnvQueryService::CreateQuery(TargetPath), FString());
	const FEQSBuildResult Build = UEnvQueryService::BuildQuery(TargetPath, SourceJson, false);
	TestTrue(FString::Printf(TEXT("build succeeded (%s)"), *Build.Error), Build.bSuccess);
	TestEqual(TEXT("no whole-build error"), Build.Error, FString());

	// One result per node, in JSON order, each naming its ASSET path.
	TestEqual(TEXT("one node result per node"), Build.Nodes.Num(), 5);
	const TArray<FString> ExpectedPaths = {
		TEXT("Option[0]"), TEXT("Option[0]/@test[0]"), TEXT("Option[0]/@test[1]"),
		TEXT("Option[1]"), TEXT("Option[1]/@test[0]") };
	for (int32 Index = 0; Index < Build.Nodes.Num() && Index < ExpectedPaths.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("node %d path"), Index), Build.Nodes[Index].Path,
			ExpectedPaths[Index]);
		TestTrue(FString::Printf(TEXT("node %d succeeded (%s)"), Index, *Build.Nodes[Index].Error),
			Build.Nodes[Index].bSuccess);
	}

	// --- The comparison -------------------------------------------------------------------------
	const FString TargetJson = UEnvQueryService::GetQuery(TargetPath);
	AddInfo(FString::Printf(TEXT("target: %s"), *TargetJson));

	FString Diff;
	const bool bMatches = QueriesMatchIgnoringGuid(SourceJson, TargetJson, Diff);
	TestTrue(FString::Printf(TEXT("round-trip matches ignoring guid (%s)"), *Diff), bMatches);

	// ...and the two are genuinely different assets. Without this, a comparator bug that compared a
	// string with itself would pass everything above.
	const TArray<FString> SourceGuids = AllGuidsForTest(SourceJson);
	const TArray<FString> TargetGuids = AllGuidsForTest(TargetJson);
	TestEqual(TEXT("same number of nodes"), TargetGuids.Num(), SourceGuids.Num());
	TestEqual(TEXT("and there are five of them"), SourceGuids.Num(), 5);
	bool bSharesAGuid = false;
	for (const FString& Guid : SourceGuids)
	{
		bSharesAGuid |= TargetGuids.Contains(Guid);
	}
	TestFalse(TEXT("no guid is shared between the two assets"), bSharesAGuid);

	// The runtime view, which GetQuery does not show: UEnvQueryOption::Tests is rebuilt from the
	// ENABLED sub-nodes only, so a disabled test that was replayed as enabled would show up here as a
	// higher TestCount even if every property matched.
	FEQSQueryInfo SourceInfo;
	FEQSQueryInfo TargetInfo;
	TestTrue(TEXT("source info"), UEnvQueryService::GetQueryInfo(SourcePath, SourceInfo));
	TestTrue(TEXT("target info"), UEnvQueryService::GetQueryInfo(TargetPath, TargetInfo));
	TestEqual(TEXT("same option count"), TargetInfo.OptionCount, SourceInfo.OptionCount);
	TestEqual(TEXT("same RUNNING test count"), TargetInfo.TestCount, SourceInfo.TestCount);
	TestEqual(TEXT("and that count excludes the disabled test"), TargetInfo.TestCount, 2);

	TestEqual(TEXT("the rebuilt query validates clean"),
		UEnvQueryService::ValidateQuery(TargetPath).Num(), 0);

	// --- Proof the comparator discriminates -----------------------------------------------------
	// Each control is a copy of the query that just MATCHED, broken in exactly one way that a
	// weaker comparator would wave through. If any of these compares equal, every assertion above is
	// worthless — which is why they are in the suite rather than done once by hand.
	struct FControl
	{
		const TCHAR* Name;
		FString Json;
		const TCHAR* ExpectedInDiff;
	};

	const TArray<FControl> Controls = {
		{ TEXT("the disabled test replayed as enabled"),
			MutatedQueryForTest(TargetJson, [](const TSharedRef<FJsonObject>& Root)
			{
				if (const TSharedPtr<FJsonObject> T = TestObjectForTest(Root, 0, 1))
				{
					T->SetBoolField(TEXT("enabled"), true);
				}
			}), TEXT("enabled") },

		{ TEXT("\"enabled\" dropped entirely"),
			MutatedQueryForTest(TargetJson, [](const TSharedRef<FJsonObject>& Root)
			{
				if (const TSharedPtr<FJsonObject> T = TestObjectForTest(Root, 0, 1))
				{
					T->RemoveField(TEXT("enabled"));
				}
			}), TEXT("enabled") },

		{ TEXT("a property holding a different value"),
			MutatedQueryForTest(TargetJson, [](const TSharedRef<FJsonObject>& Root)
			{
				if (const TSharedPtr<FJsonObject> T = TestObjectForTest(Root, 0, 0))
				{
					const TSharedPtr<FJsonObject>* Properties = nullptr;
					if (T->TryGetObjectField(TEXT("properties"), Properties) && Properties)
					{
						(*Properties)->SetStringField(TEXT("ScoringFactor"),
							TEXT("(DefaultValue=9.900000,DataBinding=None,DataField=\"\")"));
					}
				}
			}), TEXT("ScoringFactor") },

		{ TEXT("a property missing altogether"),
			MutatedQueryForTest(TargetJson, [](const TSharedRef<FJsonObject>& Root)
			{
				if (const TSharedPtr<FJsonObject> T = TestObjectForTest(Root, 0, 0))
				{
					const TSharedPtr<FJsonObject>* Properties = nullptr;
					if (T->TryGetObjectField(TEXT("properties"), Properties) && Properties)
					{
						(*Properties)->RemoveField(TEXT("ScoringFactor"));
					}
				}
			}), TEXT("ScoringFactor") },

		{ TEXT("a generator of the wrong class"),
			MutatedQueryForTest(TargetJson, [](const TSharedRef<FJsonObject>& Root)
			{
				if (const TSharedPtr<FJsonObject> O = OptionObjectForTest(Root, 1))
				{
					O->SetStringField(TEXT("generator"), TEXT("EnvQueryGenerator_ActorsOfClass"));
				}
			}), TEXT("generator") },

		{ TEXT("a test dropped"),
			MutatedQueryForTest(TargetJson, [](const TSharedRef<FJsonObject>& Root)
			{
				const TSharedPtr<FJsonObject> O = OptionObjectForTest(Root, 0);
				const TArray<TSharedPtr<FJsonValue>>* Tests = nullptr;
				if (O.IsValid() && O->TryGetArrayField(TEXT("tests"), Tests) && Tests
					&& Tests->Num() > 0)
				{
					TArray<TSharedPtr<FJsonValue>> Fewer = *Tests;
					Fewer.RemoveAt(Fewer.Num() - 1);
					O->SetArrayField(TEXT("tests"), Fewer);
				}
			}), TEXT("test count") },

		{ TEXT("the options in the other order"),
			MutatedQueryForTest(TargetJson, [](const TSharedRef<FJsonObject>& Root)
			{
				const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
				if (Root->TryGetArrayField(TEXT("options"), Options) && Options
					&& Options->Num() == 2)
				{
					TArray<TSharedPtr<FJsonValue>> Swapped = *Options;
					Swapped.Swap(0, 1);
					Root->SetArrayField(TEXT("options"), Swapped);
				}
			}), TEXT("generator") },
	};

	for (const FControl& Control : Controls)
	{
		FString ControlDiff;
		const bool bControlMatches =
			QueriesMatchIgnoringGuid(SourceJson, Control.Json, ControlDiff);
		TestFalse(FString::Printf(TEXT("the comparator rejects: %s"), Control.Name), bControlMatches);
		TestTrue(FString::Printf(TEXT("...naming what differs (%s): %s"), Control.ExpectedInDiff,
			*ControlDiff), ControlDiff.Contains(Control.ExpectedInDiff));
		AddInfo(FString::Printf(TEXT("control [%s] -> %s"), Control.Name, *ControlDiff));
	}

	// And the positive control, so the seven above are not simply a comparator that rejects
	// everything.
	FString SelfDiff;
	TestTrue(TEXT("a query matches itself"),
		QueriesMatchIgnoringGuid(TargetJson, TargetJson, SelfDiff));

	TestTrue(TEXT("uasset on disk"), FEQSScopedFixtureReset::FixtureFileExists(TargetPath));
	return true;
#endif
}

// A build where ONE node cannot be created: the other nodes still report their own outcomes, and the
// query that lands is the buildable part of the JSON rather than nothing.
//
// This is the assertion that a partial failure is not collapsed into one boolean. It also pins the
// path convention: a node that was never created is reported at its position in the SUPPLIED JSON
// ("json:options[1]"), because the options after it shift down and reporting an asset path for it
// would name a different node.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSBuildPartialFailureTest,
	"VibeUE.EQS.Build.PartialFailure", kEQSTestFlags)
bool FVibeEQSBuildPartialFailureTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString SourcePath = FString(kEQSTestDir) / TEXT("EQS_BuildPartialSource");
	const FString TargetPath = FString(kEQSTestDir) / TEXT("EQS_BuildPartialTarget");
	FEQSScopedFixtureReset SourceReset(SourcePath);
	FEQSScopedFixtureReset TargetReset(TargetPath);

	// Three options, one test each, and a distinct tuned value on the two that must survive — so
	// "the right options landed" is about identity rather than about a count.
	TestEqual(TEXT("source created"), UEnvQueryService::CreateQuery(SourcePath), FString());
	TestEqual(TEXT("option 0"),
		UEnvQueryService::AddOption(SourcePath, TEXT("EnvQueryGenerator_ActorsOfClass"), -1),
		FString(TEXT("Option[0]")));
	TestTrue(TEXT("option 0 tuned"), UEnvQueryService::SetDataProviderValue(SourcePath,
		TEXT("Option[0]"), TEXT("SearchRadius"), TEXT("111.0")).bSuccess);
	TestEqual(TEXT("option 0 test"),
		UEnvQueryService::AddTest(SourcePath, TEXT("Option[0]"), TEXT("EnvQueryTest_Distance"), -1),
		FString(TEXT("Option[0]/@test[0]")));
	TestEqual(TEXT("option 1"),
		UEnvQueryService::AddOption(SourcePath, TEXT("EnvQueryGenerator_SimpleGrid"), -1),
		FString(TEXT("Option[1]")));
	TestEqual(TEXT("option 1 test"),
		UEnvQueryService::AddTest(SourcePath, TEXT("Option[1]"), TEXT("EnvQueryTest_Distance"), -1),
		FString(TEXT("Option[1]/@test[0]")));
	TestEqual(TEXT("option 2"),
		UEnvQueryService::AddOption(SourcePath, TEXT("EnvQueryGenerator_SimpleGrid"), -1),
		FString(TEXT("Option[2]")));
	TestTrue(TEXT("option 2 tuned"), UEnvQueryService::SetDataProviderValue(SourcePath,
		TEXT("Option[2]"), TEXT("GridSize"), TEXT("222.0")).bSuccess);
	TestEqual(TEXT("option 2 test"),
		UEnvQueryService::AddTest(SourcePath, TEXT("Option[2]"), TEXT("EnvQueryTest_Distance"), -1),
		FString(TEXT("Option[2]/@test[0]")));

	// The one defect: the MIDDLE option names a generator class that does not exist.
	const FString BrokenJson = MutatedQueryForTest(UEnvQueryService::GetQuery(SourcePath),
		[](const TSharedRef<FJsonObject>& Root)
		{
			if (const TSharedPtr<FJsonObject> Option = OptionObjectForTest(Root, 1))
			{
				Option->SetStringField(TEXT("generator"), TEXT("EnvQueryGenerator_Nonexistent"));
			}
		});
	TestTrue(TEXT("the broken JSON was produced"),
		BrokenJson.Contains(TEXT("EnvQueryGenerator_Nonexistent")));

	TestEqual(TEXT("target created"), UEnvQueryService::CreateQuery(TargetPath), FString());
	const FEQSBuildResult Build = UEnvQueryService::BuildQuery(TargetPath, BrokenJson, false);

	TestFalse(TEXT("the build reports failure"), Build.bSuccess);
	TestTrue(FString::Printf(TEXT("with a whole-build summary: %s"), *Build.Error),
		Build.Error.Contains(TEXT("node(s) failed")));

	// Six nodes asked for, six reported — the failed option's test included, so a caller can find an
	// entry for everything it sent.
	TestEqual(TEXT("one result per node in the JSON"), Build.Nodes.Num(), 6);
	for (const FEQSBuildNodeResult& Node : Build.Nodes)
	{
		AddInfo(FString::Printf(TEXT("node %s -> %s%s"), *Node.Path,
			Node.bSuccess ? TEXT("ok") : TEXT("FAILED: "), Node.bSuccess ? TEXT("") : *Node.Error));
	}

	if (Build.Nodes.Num() == 6)
	{
		// The nodes that worked report their ASSET paths, and option 2 landed at index 1 because the
		// option before it was never created.
		TestEqual(TEXT("option 0 path"), Build.Nodes[0].Path, FString(TEXT("Option[0]")));
		TestTrue(TEXT("option 0 succeeded"), Build.Nodes[0].bSuccess);
		TestEqual(TEXT("its test's path"), Build.Nodes[1].Path,
			FString(TEXT("Option[0]/@test[0]")));
		TestTrue(TEXT("its test succeeded"), Build.Nodes[1].bSuccess);

		// The failure, named at its position in the SUPPLIED JSON.
		TestEqual(TEXT("the failed option is named"), Build.Nodes[2].Path,
			FString(TEXT("json:options[1]")));
		TestFalse(TEXT("and reported as failed"), Build.Nodes[2].bSuccess);
		TestTrue(FString::Printf(TEXT("naming the class that could not resolve: %s"),
			*Build.Nodes[2].Error),
			Build.Nodes[2].Error.Contains(TEXT("EnvQueryGenerator_Nonexistent")));

		// Its test could not be attempted, and says so rather than silently vanishing.
		TestEqual(TEXT("the orphaned test is named"), Build.Nodes[3].Path,
			FString(TEXT("json:options[1].tests[0]")));
		TestFalse(TEXT("and reported as failed"), Build.Nodes[3].bSuccess);
		TestTrue(FString::Printf(TEXT("saying why: %s"), *Build.Nodes[3].Error),
			Build.Nodes[3].Error.Contains(TEXT("not attempted")));

		// And the walk carried on: the option AFTER the failure has its own, successful, outcome.
		TestEqual(TEXT("the option after the failure"), Build.Nodes[4].Path,
			FString(TEXT("Option[1]")));
		TestTrue(FString::Printf(TEXT("still succeeded (%s)"), *Build.Nodes[4].Error),
			Build.Nodes[4].bSuccess);
		TestEqual(TEXT("with its own test"), Build.Nodes[5].Path,
			FString(TEXT("Option[1]/@test[0]")));
		TestTrue(TEXT("also successful"), Build.Nodes[5].bSuccess);
	}

	// The asset holds the buildable part — not nothing, and not a placeholder for the failed option.
	const FString TargetJson = UEnvQueryService::GetQuery(TargetPath);
	AddInfo(FString::Printf(TEXT("partial target: %s"), *TargetJson));
	TestEqual(TEXT("two options landed"), OptionGuidsInOrder(TargetJson).Num(), 2);
	TestEqual(TEXT("the first is the first"), OptionGeneratorAt(TargetJson, 0),
		FString(TEXT("EnvQueryGenerator_ActorsOfClass")));
	TestEqual(TEXT("the second is the THIRD option of the JSON"),
		OptionGeneratorAt(TargetJson, 1), FString(TEXT("EnvQueryGenerator_SimpleGrid")));
	// Identity, not class: both SimpleGrid options are the same class, so the tuned value is the only
	// thing that says which one survived.
	TestTrue(TEXT("and it is the one tuned to 222"),
		UEnvQueryService::GetPropertyValue(TargetPath, TEXT("Option[1]"), TEXT("GridSize"))
			.Contains(TEXT("DefaultValue=222")));
	TestEqual(TEXT("the partial result still validates clean"),
		UEnvQueryService::ValidateQuery(TargetPath).Num(), 0);

	return true;
#endif
}

// bReplaceExisting, both ways. False must refuse a target that already has options and change
// NOTHING; true must leave nothing of the old query behind.
//
// The refusal matters more than it looks: an EQS root feeds N options and every one is removable, so
// a merge would be perfectly possible to implement and completely undetectable afterwards — a query
// holding two old options and three new ones is a valid query that does the wrong thing. The clear
// path is also the one place this service cannot simply "remove everything then add": RemoveOption
// refuses the last option, because CommitGraph's discard guard refuses a query that would rebuild
// none. What is asserted below is that the interleaving used instead really does end with nothing of
// the old query left.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSBuildReplaceExistingTest,
	"VibeUE.EQS.Build.ReplaceExisting", kEQSTestFlags)
bool FVibeEQSBuildReplaceExistingTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString SourcePath = FString(kEQSTestDir) / TEXT("EQS_BuildReplaceSource");
	const FString TargetPath = FString(kEQSTestDir) / TEXT("EQS_BuildReplaceTarget");
	FEQSScopedFixtureReset SourceReset(SourcePath);
	FEQSScopedFixtureReset TargetReset(TargetPath);

	const FString FixtureError = MakeBuildFixtureQuery(SourcePath);
	if (!FixtureError.IsEmpty())
	{
		AddError(FString::Printf(TEXT("fixture: %s"), *FixtureError));
		return false;
	}
	const FString SourceJson = UEnvQueryService::GetQuery(SourcePath);

	// An existing query with a value that appears nowhere in the new one, so "nothing of it remains"
	// is checkable rather than assumed.
	TestEqual(TEXT("target created"), UEnvQueryService::CreateQuery(TargetPath), FString());
	TestEqual(TEXT("existing option 0"),
		UEnvQueryService::AddOption(TargetPath, TEXT("EnvQueryGenerator_SimpleGrid"), -1),
		FString(TEXT("Option[0]")));
	TestTrue(TEXT("existing option 0 tuned"), UEnvQueryService::SetDataProviderValue(TargetPath,
		TEXT("Option[0]"), TEXT("GridSize"), TEXT("777.0")).bSuccess);
	TestEqual(TEXT("existing test"),
		UEnvQueryService::AddTest(TargetPath, TEXT("Option[0]"), TEXT("EnvQueryTest_Distance"), -1),
		FString(TEXT("Option[0]/@test[0]")));
	TestEqual(TEXT("existing option 1"),
		UEnvQueryService::AddOption(TargetPath, TEXT("EnvQueryGenerator_SimpleGrid"), -1),
		FString(TEXT("Option[1]")));

	const FString BeforeJson = UEnvQueryService::GetQuery(TargetPath);
	const TArray<FString> BeforeGuids = AllGuidsForTest(BeforeJson);
	TestEqual(TEXT("three nodes to displace"), BeforeGuids.Num(), 3);

	// --- false: refused, and nothing touched -----------------------------------------------------
	const FEQSBuildResult Refused = UEnvQueryService::BuildQuery(TargetPath, SourceJson, false);
	TestFalse(TEXT("a non-empty target is refused"), Refused.bSuccess);
	TestTrue(FString::Printf(TEXT("saying why: %s"), *Refused.Error),
		Refused.Error.Contains(TEXT("bReplaceExisting")));
	// Nothing was ATTEMPTED, which is the difference between a refusal and a failed build.
	TestEqual(TEXT("and no node was attempted"), Refused.Nodes.Num(), 0);
	// Byte-for-byte, guids included: the strongest available statement that the asset is untouched.
	TestEqual(TEXT("the existing query is exactly as it was"),
		UEnvQueryService::GetQuery(TargetPath), BeforeJson);

	// --- true: replaced wholesale ----------------------------------------------------------------
	const FEQSBuildResult Replaced = UEnvQueryService::BuildQuery(TargetPath, SourceJson, true);
	TestTrue(FString::Printf(TEXT("replace succeeded (%s)"), *Replaced.Error), Replaced.bSuccess);
	for (const FEQSBuildNodeResult& Node : Replaced.Nodes)
	{
		TestTrue(FString::Printf(TEXT("node %s (%s)"), *Node.Path, *Node.Error), Node.bSuccess);
	}

	const FString AfterJson = UEnvQueryService::GetQuery(TargetPath);
	AddInfo(FString::Printf(TEXT("after replace: %s"), *AfterJson));

	FString Diff;
	TestTrue(FString::Printf(TEXT("the target is now the source query (%s)"), *Diff),
		QueriesMatchIgnoringGuid(SourceJson, AfterJson, Diff));

	// Nothing of the old query survives — by identity, and by the one value only it carried.
	const TArray<FString> AfterGuids = AllGuidsForTest(AfterJson);
	bool bKeptAnOldNode = false;
	for (const FString& Guid : BeforeGuids)
	{
		bKeptAnOldNode |= AfterGuids.Contains(Guid);
	}
	TestFalse(TEXT("no node of the old query survived"), bKeptAnOldNode);
	TestFalse(TEXT("and neither did the value only it carried"),
		AfterJson.Contains(TEXT("DefaultValue=777")));

	FEQSQueryInfo Info;
	TestTrue(TEXT("info readable"), UEnvQueryService::GetQueryInfo(TargetPath, Info));
	TestEqual(TEXT("exactly the new option count"), Info.OptionCount, 2);
	TestEqual(TEXT("exactly the new running-test count"), Info.TestCount, 2);
	TestEqual(TEXT("and the replaced query validates clean"),
		UEnvQueryService::ValidateQuery(TargetPath).Num(), 0);

	return true;
#endif
}

// The partial-sparse target: an option present in UEnvQuery::Options whose graph node is unlinked from
// the root. BuildQuery must refuse it BEFORE its first write.
//
// Without that refusal this is not merely a failed build, it is a permanent one. The first AddOption
// commits a graph that rebuilds one FEWER option than the query holds, EnsureGraph's reconstruction
// drops the orphan, the discard guard refuses — and that in-memory UEnvQuery is then marked
// un-writable for the rest of the process, so every later call against it fails too, including calls
// with nothing to do with this build. The asset on disk is fine and the object in memory is not, which
// is the least debuggable failure this service can produce. Refusing up front turns it into a clean,
// repeatable message that names the offending option.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSBuildOrphanRefusalTest,
	"VibeUE.EQS.Build.OrphanedOptionRefusal", kEQSTestFlags)
bool FVibeEQSBuildOrphanRefusalTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_BuildOrphanTarget");
	FEQSScopedFixtureReset Reset(Path);

	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());
	TestEqual(TEXT("option 0"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1),
		FString(TEXT("Option[0]")));
	TestEqual(TEXT("option 1"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_SimpleGrid"), -1),
		FString(TEXT("Option[1]")));

	// Legitimate JSON that would build cleanly into a healthy target, so what is under test is the
	// state of the TARGET rather than the shape of the request.
	const FString BuildJson =
		TEXT("{\"options\":[{\"generator\":\"EnvQueryGenerator_ActorsOfClass\",\"tests\":[]}]}");

	UEnvironmentQueryGraph* Graph = LoadFixtureGraph(Path);
	TestNotNull(TEXT("graph reachable"), Graph);
	TArray<UEnvironmentQueryGraphNode_Option*> OptionNodes =
		Graph ? VibeEQS::GetOptionNodes(Graph) : TArray<UEnvironmentQueryGraphNode_Option*>();
	if (!Graph || OptionNodes.Num() != 2)
	{
		AddError(TEXT("fixture did not build two root-linked options"));
		return false;
	}

	UnlinkOptionNodeForTest(OptionNodes[1]);
	TestEqual(TEXT("one option node still linked"), VibeEQS::GetOptionNodes(Graph).Num(), 1);
	TestTrue(TEXT("and ValidateQuery calls it an orphan"),
		HasIssue(UEnvQueryService::ValidateQuery(Path), TEXT("orphaned option:")));

	const UEnvQuery* const TargetQuery =
		LoadObject<UEnvQuery>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	TestEqual(TEXT("the query itself still holds both options"),
		TargetQuery ? TargetQuery->GetOptions().Num() : -1, 2);
	const FString BeforeJson = UEnvQueryService::GetQuery(Path);

	// --- The refusal ------------------------------------------------------------------------------
	const FEQSBuildResult Refused = UEnvQueryService::BuildQuery(Path, BuildJson, true);
	TestFalse(TEXT("an orphaned option is refused"), Refused.bSuccess);
	TestTrue(FString::Printf(TEXT("naming the diagnostic (%s)"), *Refused.Error),
		Refused.Error.Contains(TEXT("orphaned option:")));
	TestTrue(FString::Printf(TEXT("and saying nothing was written (%s)"), *Refused.Error),
		Refused.Error.Contains(TEXT("Nothing was written")));
	// Refused, not attempted — the difference between this and a build that failed halfway.
	TestEqual(TEXT("no node was attempted"), Refused.Nodes.Num(), 0);

	// --- ... and it really wrote nothing ----------------------------------------------------------
	TestEqual(TEXT("the option list is intact"),
		TargetQuery ? TargetQuery->GetOptions().Num() : -1, 2);
	TestEqual(TEXT("and the readable query is byte-identical"),
		UEnvQueryService::GetQuery(Path), BeforeJson);

	// The proof that the target was not poisoned: repair the orphan and an ordinary write still goes
	// through. Had the build run, this AddOption would be refused like every other call on this object.
	RelinkOptionNodeForTest(Graph, OptionNodes[1]);
	TestEqual(TEXT("both option nodes linked again"), VibeEQS::GetOptionNodes(Graph).Num(), 2);
	TestEqual(TEXT("the repaired query validates clean"),
		UEnvQueryService::ValidateQuery(Path).Num(), 0);
	TestEqual(TEXT("a fresh legitimate write still works"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_SimpleGrid"), -1),
		FString(TEXT("Option[2]")));

	FEQSQueryInfo Info;
	TestTrue(TEXT("info readable"), UEnvQueryService::GetQueryInfo(Path, Info));
	TestEqual(TEXT("and nothing was lost along the way"), Info.OptionCount, 3);

	AddInfo(FString::Printf(TEXT("orphan refusal: %s"), *Refused.Error));
	return true;
#endif
}

// RunQuery's EXECUTION path cannot be covered here, and that is stated rather than worked around:
// UEnvQueryManager::RunInstantQuery needs a live world and a querier actor in it, and the headless
// automation process has neither — GEditor->PlayWorld is null for the whole run, which the first
// assertion below establishes as a fact instead of assuming it.
//
// What IS testable headless is exactly the two refusals, and they are worth testing precisely
// because they are what a caller hits first:
//   - an unrecognised RunMode is refused naming the valid set, never defaulted;
//   - with no play session the call says so, rather than returning an empty result set that reads
//     like "the query matched nothing".
//
// The rest of the refusals (unknown asset, empty querier, querier not found, no AI system) sit
// BEHIND the play-session check by design, so headless every one of them returns the play-session
// message and none of them can be distinguished here. They are not asserted rather than asserted
// falsely. The real run — items, scores, actor names — was verified manually in a PIE session; see
// the RunQuery section of the PR description for what it measured.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSRunRefusalTest,
	"VibeUE.EQS.Run.Refusals", kEQSTestFlags)
bool FVibeEQSRunRefusalTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	// The precondition every assertion below rests on. If a future harness ever runs this suite with
	// a play session up, this fails loudly instead of the tests quietly asserting nothing.
	TestTrue(TEXT("no play session in the headless suite"), !GEditor || !GEditor->PlayWorld);

	const FString AnyPath = FString(kEQSTestDir) / TEXT("EQS_RunRefusalNoSuchAsset");

	// 1) An unrecognised RunMode is refused, and refused FIRST — before the play-session check, which
	//    is the only reason this is reachable at all outside PIE.
	const FEQSRunResult BadMode = UEnvQueryService::RunQuery(AnyPath, TEXT("Querier"), TEXT("Best"));
	TestFalse(TEXT("bad run mode is not a success"), BadMode.bSuccess);
	TestTrue(TEXT("bad run mode explains itself"), !BadMode.Error.IsEmpty());
	TestTrue(TEXT("bad run mode names the offending value"), BadMode.Error.Contains(TEXT("\"Best\"")));
	for (const TCHAR* const Valid :
		{ TEXT("SingleResult"), TEXT("RandomBest5Pct"), TEXT("RandomBest25Pct"), TEXT("AllMatching") })
	{
		TestTrue(FString::Printf(TEXT("bad run mode lists %s"), Valid), BadMode.Error.Contains(Valid));
	}

	// A refusal never ran anything, so it carries no engine status and no items — the field that
	// tells a caller "the query did not execute" apart from "it executed and matched nothing".
	TestEqual(TEXT("bad run mode has no engine status"), BadMode.Status, FString());
	TestEqual(TEXT("bad run mode returns no items"), BadMode.ItemCount, 0);
	TestEqual(TEXT("bad run mode's item array agrees"), BadMode.Items.Num(), 0);

	// 2) A valid RunMode gets PAST the parser and is stopped by the play-session check instead. This
	//    is what proves the two refusals discriminate: same call, same asset, different message
	//    depending only on the mode string.
	const FEQSRunResult NoPie = UEnvQueryService::RunQuery(AnyPath, TEXT("Querier"), TEXT("AllMatching"));
	TestFalse(TEXT("no play session is not a success"), NoPie.bSuccess);
	TestTrue(TEXT("no play session explains itself"), !NoPie.Error.IsEmpty());
	TestTrue(TEXT("no play session says so"), NoPie.Error.Contains(TEXT("Play In Editor")));
	TestEqual(TEXT("no play session has no engine status"), NoPie.Status, FString());
	TestEqual(TEXT("no play session returns no items"), NoPie.ItemCount, 0);

	// The two messages are genuinely different, in both directions.
	TestFalse(TEXT("the mode refusal is not the play-session one"),
		BadMode.Error.Contains(TEXT("Play In Editor")));
	TestFalse(TEXT("the play-session refusal is not the mode one"),
		NoPie.Error.Contains(TEXT("Unknown RunMode")));

	// 3) All four documented modes are accepted by the parser — asserted through the same
	//    discrimination, since none of them can execute here. A mode the table forgot would come back
	//    with the RunMode refusal instead.
	for (const TCHAR* const Valid :
		{ TEXT("SingleResult"), TEXT("RandomBest5Pct"), TEXT("RandomBest25Pct"), TEXT("AllMatching") })
	{
		const FEQSRunResult Accepted = UEnvQueryService::RunQuery(AnyPath, TEXT("Querier"), Valid);
		TestTrue(FString::Printf(TEXT("%s is accepted by the parser"), Valid),
			Accepted.Error.Contains(TEXT("Play In Editor")));
	}

	// Case-insensitive, deliberately: the caller is typing a mode name, not addressing a node.
	TestTrue(TEXT("mode matching ignores case"),
		UEnvQueryService::RunQuery(AnyPath, TEXT("Querier"), TEXT("allmatching"))
			.Error.Contains(TEXT("Play In Editor")));

	// An empty RunMode is a typo, not a request for the default, so it is refused like any other
	// unrecognised value — the default lives in the signature and is only applied when the argument
	// is absent.
	TestTrue(TEXT("empty run mode is refused, not defaulted"),
		UEnvQueryService::RunQuery(AnyPath, TEXT("Querier"), FString())
			.Error.Contains(TEXT("Valid values are")));

	AddInfo(FString::Printf(TEXT("mode refusal: %s"), *BadMode.Error));
	AddInfo(FString::Printf(TEXT("play-session refusal: %s"), *NoPie.Error));
	AddInfo(TEXT("NOT covered here: the execution path (RunInstantQuery needs a live world and a "
				 "querier actor). Verified manually in a PIE session instead."));
	return true;
#endif
}

// Bounds and roots on caller-supplied input. Every case here is one an MCP caller can produce by
// accident, and two of them are fatal rather than erroneous if they reach the engine unchecked:
// FName construction past NAME_SIZE brings the editor down, and it is reached through an object
// path just as easily as through a property name.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSInputBoundsTest,
	"VibeUE.EQS.Hardening.InputBounds", kEQSTestFlags)
bool FVibeEQSInputBoundsTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	// --- Over-long paths are refused, not fatal --------------------------------------------------
	// If this test brings the editor down instead of failing, the guard is gone: that is the whole
	// point of asserting it. NAME_SIZE is the engine's bound; one past it is the first fatal value.
	const FString TooLong = FString(TEXT("/Game/")) + FString::ChrN(NAME_SIZE, TEXT('a'));
	TestTrue(TEXT("over-long CreateQuery path refused"),
		UEnvQueryService::CreateQuery(TooLong).Contains(TEXT("limited to")));
	TestTrue(TEXT("over-long CompileAndSave path refused"),
		UEnvQueryService::CompileAndSave(TooLong).Contains(TEXT("limited to")));
	TestTrue(TEXT("over-long read path refused"),
		UEnvQueryService::GetQuery(TooLong).Contains(TEXT("limited to")));

	// --- Roots these services do not write -------------------------------------------------------
	// All three are refused, but by two different arms, and the split is worth pinning: /Engine is a
	// real mounted content root, so it passes FPackageName::IsValidLongPackageName and reaches the
	// forbidden-root message. /Script and /Temp are READ-ONLY roots, which that same call rejects
	// first — so they refuse as "not a valid asset path". Either way nothing is written, which is
	// the property that matters; asserting only the prettier message would have failed here.
	for (const TCHAR* Forbidden : { TEXT("/Engine/AI/EQS_Nope"), TEXT("/Script/AIModule"),
		TEXT("/Temp/EQS_Nope") })
	{
		TestTrue(FString::Printf(TEXT("%s refused"), Forbidden),
			!UEnvQueryService::CreateQuery(Forbidden).IsEmpty());
	}
	TestTrue(TEXT("engine content names the reason"),
		UEnvQueryService::CreateQuery(TEXT("/Engine/AI/EQS_Nope")).Contains(TEXT("do not write to")));
	// Reading engine content stays legal — the restriction is on writes alone.
	TestFalse(TEXT("engine root is not refused for reads"),
		UEnvQueryService::GetQuery(TEXT("/Engine/AI/EQS_DoesNotExist"))
			.Contains(TEXT("do not write to")));

	// A path no package name grammar accepts, caught before LoadObject sees it.
	TestTrue(TEXT("malformed path refused"),
		UEnvQueryService::CreateQuery(TEXT("not-a-package-path")).Contains(TEXT("Not a valid asset path")));

	// --- Over-long property names ----------------------------------------------------------------
	const FString Path = FString(kEQSTestDir) / TEXT("EQS_InputBoundsTest");
	FEQSScopedFixtureReset Reset(Path);
	TestEqual(TEXT("created"), UEnvQueryService::CreateQuery(Path), FString());
	TestFalse(TEXT("option added"),
		UEnvQueryService::AddOption(Path, TEXT("EnvQueryGenerator_ActorsOfClass"), -1)
			.StartsWith(TEXT("ERROR")));
	const FString TestPath = UEnvQueryService::AddTest(Path, TEXT("Option[0]"),
		TEXT("EnvQueryTest_Distance"), -1);
	TestEqual(TEXT("test added"), TestPath, FString(TEXT("Option[0]/@test[0]")));

	const FString LongName = FString::ChrN(NAME_SIZE, TEXT('p'));
	TestTrue(TEXT("over-long PropertyName refused on read"),
		UEnvQueryService::GetPropertyValue(Path, TestPath, LongName).Contains(TEXT("limited to")));
	const FEQSPropertySetResult Set =
		UEnvQueryService::SetPropertyValue(Path, TestPath, LongName, TEXT("1.0"));
	TestFalse(TEXT("over-long PropertyName refused on write"), Set.bSuccess);
	TestTrue(TEXT("and says why"), Set.Error.Contains(TEXT("limited to")));

	// The refusals above must not have disturbed the asset.
	TestFalse(TEXT("query still readable"), UEnvQueryService::GetQuery(Path).StartsWith(TEXT("ERROR")));
	return true;
#endif
}

#endif // WITH_AUTOMATION_TESTS
