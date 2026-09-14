// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "PythonAPI/UBlueprintService.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#if WITH_AUTOMATION_TESTS

// Regression coverage for item A9 of the 2026-09-09 VibeUE PR-candidates brief. Two symptoms were
// recorded with no confirmed cause: get_connections did not report an event's exec edge in a widget
// Blueprint, and disconnect_pin(node, "then") appeared to also sever that node's exec INPUT. This
// test pins the ground-truth semantics of GetConnections and DisconnectPin against the service's own
// node-creation and wiring API, so any future regression of either surfaces here.
//
// EditorContext: the service resolves Blueprints through the Content Browser asset registry, so the
// test creates an in-memory Blueprint under a /Game path and registers it with AssetCreated (never
// saved to disk) so UBlueprintService::LoadBlueprint can find it. Requires a full editor; it is not
// a commandlet/headless test.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceDisconnectPinTest, "VibeUE.BlueprintService.DisconnectPinKeepsInputEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceDisconnectPinTest::RunTest(const FString&)
{
	const FString PackageName = TEXT("/Game/__VibeUETest/BP_A9DisconnectRegression");
	const FName   AssetName(TEXT("BP_A9DisconnectRegression"));

	UPackage* Package = CreatePackage(*PackageName);
	if (!TestNotNull(TEXT("created a package for the transient test Blueprint"), Package))
	{
		return false;
	}

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), Package, AssetName, BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!TestNotNull(TEXT("CreateBlueprint returned a Blueprint"), Blueprint))
	{
		return false;
	}

	// Make the in-memory asset discoverable so the service's path-based API can resolve it.
	FAssetRegistryModule::AssetCreated(Blueprint);

	// Cleanup runs on every exit path: unregister and drop the standalone flags so GC reclaims the
	// never-saved asset instead of leaving a phantom entry in the Content Browser for the session.
	ON_SCOPE_EXIT
	{
		if (Blueprint)
		{
			FAssetRegistryModule::AssetDeleted(Blueprint);
			Blueprint->ClearFlags(RF_Standalone | RF_Public);
		}
	};

	// The service resolves by package path (UEditorAssetLibrary::LoadAsset), not object path.
	const FString Path = PackageName;

	// Guard: if the editor's asset registry did not surface the in-memory asset, the service cannot
	// load it and every step below would fail confusingly. Say so plainly and stop.
	const TArray<FBlueprintGraphInfo> Graphs = UBlueprintService::ListGraphs(Path);
	bool bHasEventGraph = false;
	for (const FBlueprintGraphInfo& G : Graphs)
	{
		if (G.GraphName == TEXT("EventGraph"))
		{
			bHasEventGraph = true;
			break;
		}
	}
	if (!TestTrue(TEXT("service resolved the transient Blueprint and it has an EventGraph (needs a full editor with an active asset registry)"), bHasEventGraph))
	{
		return false;
	}

	// Build: Event BeginPlay -> A (PrintString) -> B (PrintString), all via the service's own API.
	const FString BeginPlayId = UBlueprintService::CreateNodeByKey(Path, TEXT("EventGraph"), TEXT("EVENT Actor::ReceiveBeginPlay"), 0.0f, 0.0f);
	const FString AId         = UBlueprintService::CreateNodeByKey(Path, TEXT("EventGraph"), TEXT("FUNC KismetSystemLibrary::PrintString"), 320.0f, 0.0f);
	const FString BId         = UBlueprintService::CreateNodeByKey(Path, TEXT("EventGraph"), TEXT("FUNC KismetSystemLibrary::PrintString"), 640.0f, 0.0f);

	TestFalse(TEXT("BeginPlay event node was created"), BeginPlayId.IsEmpty());
	TestFalse(TEXT("function-call node A was created"), AId.IsEmpty());
	TestFalse(TEXT("function-call node B was created"), BId.IsEmpty());
	if (BeginPlayId.IsEmpty() || AId.IsEmpty() || BId.IsEmpty())
	{
		return false;
	}

	TestTrue(TEXT("connect BeginPlay.then -> A.execute"),
		UBlueprintService::ConnectNodes(Path, TEXT("EventGraph"), BeginPlayId, TEXT("then"), AId, TEXT("execute")));
	TestTrue(TEXT("connect A.then -> B.execute"),
		UBlueprintService::ConnectNodes(Path, TEXT("EventGraph"), AId, TEXT("then"), BId, TEXT("execute")));

	// Ground truth: an edge is Source(node,pin) -> Target(node,pin), matched case-insensitively on pins.
	auto HasEdge = [](const TArray<FBlueprintConnectionInfo>& Edges,
		const FString& SrcId, const FString& SrcPin, const FString& TgtId, const FString& TgtPin) -> bool
	{
		for (const FBlueprintConnectionInfo& E : Edges)
		{
			if (E.SourceNodeId == SrcId && E.SourcePinName.Equals(SrcPin, ESearchCase::IgnoreCase) &&
				E.TargetNodeId == TgtId && E.TargetPinName.Equals(TgtPin, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	};
	auto CountEdgesFrom = [](const TArray<FBlueprintConnectionInfo>& Edges, const FString& SrcId, const FString& SrcPin) -> int32
	{
		int32 Count = 0;
		for (const FBlueprintConnectionInfo& E : Edges)
		{
			if (E.SourceNodeId == SrcId && E.SourcePinName.Equals(SrcPin, ESearchCase::IgnoreCase))
			{
				++Count;
			}
		}
		return Count;
	};

	// GetConnections must report exactly the two exec edges we just made — this is the leg that the
	// widget-Blueprint symptom said was missing.
	{
		const TArray<FBlueprintConnectionInfo> Edges = UBlueprintService::GetConnections(Path, TEXT("EventGraph"));
		TestEqual(TEXT("exactly two exec edges reported after wiring"), Edges.Num(), 2);
		TestTrue(TEXT("GetConnections reports BeginPlay.then -> A.execute"),
			HasEdge(Edges, BeginPlayId, TEXT("then"), AId, TEXT("execute")));
		TestTrue(TEXT("GetConnections reports A.then -> B.execute"),
			HasEdge(Edges, AId, TEXT("then"), BId, TEXT("execute")));
	}

	// Disconnect only A's OUTPUT exec pin ("then"). A's INPUT exec ("execute") must be untouched.
	TestTrue(TEXT("DisconnectPin(A, 'then') succeeds"),
		UBlueprintService::DisconnectPin(Path, TEXT("EventGraph"), AId, TEXT("then")));

	{
		const TArray<FBlueprintConnectionInfo> Edges = UBlueprintService::GetConnections(Path, TEXT("EventGraph"));
		// The exec INPUT edge into A must survive — this is the leg the disconnect symptom claimed was severed.
		TestTrue(TEXT("BeginPlay.then -> A.execute still connected after disconnecting A.then"),
			HasEdge(Edges, BeginPlayId, TEXT("then"), AId, TEXT("execute")));
		// Only A's output edge should be gone.
		TestEqual(TEXT("no edges remain out of A.then"), CountEdgesFrom(Edges, AId, TEXT("then")), 0);
		TestFalse(TEXT("A.then -> B.execute is gone"),
			HasEdge(Edges, AId, TEXT("then"), BId, TEXT("execute")));
		TestEqual(TEXT("exactly one exec edge remains"), Edges.Num(), 1);
	}

	return true;
}

#endif // WITH_AUTOMATION_TESTS
