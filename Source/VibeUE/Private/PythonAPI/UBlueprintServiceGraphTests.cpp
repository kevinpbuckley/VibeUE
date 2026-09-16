// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "PythonAPI/UBlueprintService.h"
#include "PythonAPI/UActorService.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_Timeline.h"
#include "GameFramework/Actor.h"
#include "UObject/Interface.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/TopLevelAssetPath.h"
#include "Misc/PackageName.h"

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

// ─────────────────────────────────────────────────────────────────────────────
// Regression coverage for the 2026-09-15 VibeUE PR-candidates brief, batch items
// 5, 6, 7, 8, 10, 14, 15. Each throwaway Blueprint is created in-memory under a
// /Game/__VibeUETest path, registered with AssetCreated so the service's path-based
// API can resolve it, and unregistered on every exit path. Requires a full editor.
// ─────────────────────────────────────────────────────────────────────────────

namespace VibeUETestHelpers
{
	// Create an in-memory Blueprint under a /Game path and register it so
	// UBlueprintService::LoadBlueprint (UEditorAssetLibrary::LoadAsset) can find it.
	static UBlueprint* MakeBlueprint(UClass* ParentClass, const FString& PackageName, EBlueprintType Type = BPTYPE_Normal)
	{
		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			return nullptr;
		}
		const FName AssetName(*FPackageName::GetShortName(PackageName));
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass, Package, AssetName, Type,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (Blueprint)
		{
			FAssetRegistryModule::AssetCreated(Blueprint);
		}
		return Blueprint;
	}

	static void ForgetBlueprint(UBlueprint* Blueprint)
	{
		if (Blueprint)
		{
			FAssetRegistryModule::AssetDeleted(Blueprint);
			Blueprint->ClearFlags(RF_Standalone | RF_Public);
		}
	}
}

// Item 5: add_member_variable can make a variable instance-editable, and
// set_variable_instance_editable flips CPF_DisableEditOnInstance on the generated FProperty.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceInstanceEditableTest, "VibeUE.BlueprintService.VariableInstanceEditable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceInstanceEditableTest::RunTest(const FString&)
{
	const FString Path = TEXT("/Game/__VibeUETest/BP_InstanceEditable");
	UBlueprint* Blueprint = VibeUETestHelpers::MakeBlueprint(AActor::StaticClass(), Path);
	if (!TestNotNull(TEXT("created the transient Blueprint"), Blueprint))
	{
		return false;
	}
	ON_SCOPE_EXIT{ VibeUETestHelpers::ForgetBlueprint(Blueprint); };

	// Add a variable that should be editable per instance from the start.
	TestTrue(TEXT("add_member_variable with bInstanceEditable=true"),
		UBlueprintService::AddMemberVariable(Path, TEXT("InstEditInt"), TEXT("int"), TEXT(""), false, TEXT(""), /*bInstanceEditable*/true));
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	FProperty* Prop = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->FindPropertyByName(TEXT("InstEditInt")) : nullptr;
	if (!TestNotNull(TEXT("generated FProperty exists for the new variable"), Prop))
	{
		return false;
	}
	TestFalse(TEXT("instance-editable variable lacks CPF_DisableEditOnInstance"),
		Prop->HasAnyPropertyFlags(CPF_DisableEditOnInstance));

	// Flip it to blueprint-only and confirm the flag comes back.
	TestTrue(TEXT("set_variable_instance_editable false"),
		UBlueprintService::SetVariableInstanceEditable(Path, TEXT("InstEditInt"), false));
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	FProperty* Prop2 = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->FindPropertyByName(TEXT("InstEditInt")) : nullptr;
	if (!TestNotNull(TEXT("generated FProperty exists after flip"), Prop2))
	{
		return false;
	}
	TestTrue(TEXT("blueprint-only variable has CPF_DisableEditOnInstance"),
		Prop2->HasAnyPropertyFlags(CPF_DisableEditOnInstance));
	return true;
}

// Item 6: set_variable_default_value writes the CDO default of a variable INHERITED from a parent
// Blueprint (it used to walk NewVariables only and return False for inherited variables).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceInheritedDefaultTest, "VibeUE.BlueprintService.SetInheritedVariableDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceInheritedDefaultTest::RunTest(const FString&)
{
	const FString ParentPath = TEXT("/Game/__VibeUETest/BP_InheritParent");
	const FString ChildPath  = TEXT("/Game/__VibeUETest/BP_InheritChild");

	UBlueprint* Parent = VibeUETestHelpers::MakeBlueprint(AActor::StaticClass(), ParentPath);
	if (!TestNotNull(TEXT("created parent Blueprint"), Parent))
	{
		return false;
	}
	ON_SCOPE_EXIT{ VibeUETestHelpers::ForgetBlueprint(Parent); };

	TestTrue(TEXT("added int variable ParentHealth to parent"),
		UBlueprintService::AddMemberVariable(ParentPath, TEXT("ParentHealth"), TEXT("int"), TEXT("10")));
	FKismetEditorUtilities::CompileBlueprint(Parent);
	if (!TestNotNull(TEXT("parent has a generated class"), Parent->GeneratedClass.Get()))
	{
		return false;
	}

	UBlueprint* Child = VibeUETestHelpers::MakeBlueprint(Parent->GeneratedClass, ChildPath);
	if (!TestNotNull(TEXT("created child Blueprint from the parent's generated class"), Child))
	{
		return false;
	}
	ON_SCOPE_EXIT{ VibeUETestHelpers::ForgetBlueprint(Child); };
	FKismetEditorUtilities::CompileBlueprint(Child);

	// ParentHealth is inherited (not in the child's NewVariables) — the new inherited-CDO path handles it.
	TestTrue(TEXT("set_variable_default_value on the child's inherited variable"),
		UBlueprintService::SetVariableDefaultValue(ChildPath, TEXT("ParentHealth"), TEXT("42")));

	UObject* ChildCDO = Child->GeneratedClass ? Child->GeneratedClass->GetDefaultObject() : nullptr;
	if (!TestNotNull(TEXT("child has a CDO"), ChildCDO))
	{
		return false;
	}
	FIntProperty* IntProp = ChildCDO ? CastField<FIntProperty>(Child->GeneratedClass->FindPropertyByName(TEXT("ParentHealth"))) : nullptr;
	if (!TestNotNull(TEXT("ParentHealth resolves as an int property on the child class"), IntProp))
	{
		return false;
	}
	TestEqual(TEXT("child CDO reads back the inherited default 42"), IntProp->GetPropertyValue_InContainer(ChildCDO), 42);
	return true;
}

// Shared setup for the interface tests (items 7 and 8): a Blueprint Interface with a void function
// (DoThing) and an int-returning function (GetValue), implemented on an AActor Blueprint.
namespace VibeUETestHelpers
{
	static bool BuildInterfaceImplementer(FAutomationTestBase& T, const FString& Tag,
		UBlueprint*& OutInterface, UBlueprint*& OutActor, FString& OutActorPath)
	{
		const FString ItfPath = FString::Printf(TEXT("/Game/__VibeUETest/BPI_%s"), *Tag);
		OutInterface = MakeBlueprint(UInterface::StaticClass(), ItfPath, BPTYPE_Interface);
		if (!T.TestNotNull(TEXT("created interface Blueprint"), OutInterface))
		{
			return false;
		}

		// Void function → implemented as an event on the actor (no return value).
		UEdGraph* VoidGraph = FBlueprintEditorUtils::CreateNewGraph(OutInterface, TEXT("DoThing"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph<UClass>(OutInterface, VoidGraph, /*bIsUserCreated*/true, (UClass*)nullptr);

		// Return-valued function → materialised as a function graph when implemented.
		UEdGraph* RetGraph = FBlueprintEditorUtils::CreateNewGraph(OutInterface, TEXT("GetValue"), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph<UClass>(OutInterface, RetGraph, /*bIsUserCreated*/true, (UClass*)nullptr);

		FKismetEditorUtilities::CompileBlueprint(OutInterface);
		UBlueprintService::AddFunctionParameter(ItfPath, TEXT("GetValue"), TEXT("Value"), TEXT("int"), /*bIsOutput*/true);
		FKismetEditorUtilities::CompileBlueprint(OutInterface);
		if (!T.TestNotNull(TEXT("interface has a generated class"), OutInterface->GeneratedClass.Get()))
		{
			return false;
		}

		OutActorPath = FString::Printf(TEXT("/Game/__VibeUETest/BP_%sImpl"), *Tag);
		OutActor = MakeBlueprint(AActor::StaticClass(), OutActorPath);
		if (!T.TestNotNull(TEXT("created implementer Blueprint"), OutActor))
		{
			return false;
		}

		FBlueprintEditorUtils::ImplementNewInterface(OutActor, FTopLevelAssetPath(OutInterface->GeneratedClass));
		FKismetEditorUtilities::CompileBlueprint(OutActor);
		return true;
	}
}

// Item 7: override_function on a VOID interface function creates the event node in the EventGraph.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceOverrideInterfaceTest, "VibeUE.BlueprintService.OverrideInterfaceFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceOverrideInterfaceTest::RunTest(const FString&)
{
	UBlueprint* Interface = nullptr;
	UBlueprint* Actor = nullptr;
	FString ActorPath;
	const bool bBuilt = VibeUETestHelpers::BuildInterfaceImplementer(*this, TEXT("Override"), Interface, Actor, ActorPath);
	ON_SCOPE_EXIT{ VibeUETestHelpers::ForgetBlueprint(Actor); VibeUETestHelpers::ForgetBlueprint(Interface); };
	if (!bBuilt)
	{
		return false;
	}

	// The void interface function has no override event yet.
	TestTrue(TEXT("override_function creates the interface event"),
		UBlueprintService::OverrideFunction(ActorPath, TEXT("DoThing")));

	// Assert an event node for DoThing now exists in the EventGraph.
	bool bFound = false;
	for (UEdGraph* Ubergraph : Actor->UbergraphPages)
	{
		if (!Ubergraph)
		{
			continue;
		}
		for (UEdGraphNode* Node : Ubergraph->Nodes)
		{
			if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
			{
				if (EventNode->EventReference.GetMemberName() == FName(TEXT("DoThing")))
				{
					bFound = true;
					break;
				}
			}
		}
	}
	TestTrue(TEXT("EventGraph now contains the DoThing interface event node"), bFound);

	// Idempotent — calling again must not fail or duplicate.
	TestTrue(TEXT("override_function is idempotent for the interface event"),
		UBlueprintService::OverrideFunction(ActorPath, TEXT("DoThing")));
	return true;
}

// Item 8: list_graphs surfaces the auto-materialised graph of a return-valued interface function,
// tagged with an "Interface" kind.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceListInterfaceGraphsTest, "VibeUE.BlueprintService.ListInterfaceGraphs",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceListInterfaceGraphsTest::RunTest(const FString&)
{
	UBlueprint* Interface = nullptr;
	UBlueprint* Actor = nullptr;
	FString ActorPath;
	const bool bBuilt = VibeUETestHelpers::BuildInterfaceImplementer(*this, TEXT("ListGraphs"), Interface, Actor, ActorPath);
	ON_SCOPE_EXIT{ VibeUETestHelpers::ForgetBlueprint(Actor); VibeUETestHelpers::ForgetBlueprint(Interface); };
	if (!bBuilt)
	{
		return false;
	}

	const TArray<FBlueprintGraphInfo> Graphs = UBlueprintService::ListGraphs(ActorPath);
	bool bFoundInterfaceGraph = false;
	for (const FBlueprintGraphInfo& G : Graphs)
	{
		if (G.GraphName == TEXT("GetValue") && G.GraphKind.StartsWith(TEXT("Interface")))
		{
			bFoundInterfaceGraph = true;
			break;
		}
	}
	TestTrue(TEXT("list_graphs includes the return-valued interface function graph GetValue with Interface kind"), bFoundInterfaceGraph);
	return true;
}

// Item 10: get_timelines returns FBlueprintTimelineInfo with the name in TimelineName.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceGetTimelinesTest, "VibeUE.BlueprintService.GetTimelinesInfo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceGetTimelinesTest::RunTest(const FString&)
{
	const FString Path = TEXT("/Game/__VibeUETest/BP_TimelineInfo");
	UBlueprint* Blueprint = VibeUETestHelpers::MakeBlueprint(AActor::StaticClass(), Path);
	if (!TestNotNull(TEXT("created the transient Blueprint"), Blueprint))
	{
		return false;
	}
	ON_SCOPE_EXIT{ VibeUETestHelpers::ForgetBlueprint(Blueprint); };

	const FString NodeId = UBlueprintService::AddTimeline(Path, TEXT("EventGraph"), TEXT("LookAtTimeline"), 0.75f);
	TestFalse(TEXT("add_timeline returned a node id (not an ERROR sentinel)"), NodeId.IsEmpty() || NodeId.StartsWith(TEXT("ERROR:")));
	TestTrue(TEXT("add a float track to the timeline"),
		UBlueprintService::AddTimelineFloatTrack(Path, TEXT("LookAtTimeline"), TEXT("Alpha")));

	const TArray<FBlueprintTimelineInfo> Timelines = UBlueprintService::GetTimelines(Path);
	if (!TestEqual(TEXT("exactly one timeline reported"), Timelines.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("TimelineName matches"), Timelines[0].TimelineName, FString(TEXT("LookAtTimeline")));
	TestEqual(TEXT("TrackCount counts the float track"), Timelines[0].TrackCount, 1);
	return true;
}

// Item 14: add_timeline with replace_existing=true succeeds when a leftover UTimelineTemplate
// survives a Timeline-node delete.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceAddTimelineReplaceTest, "VibeUE.BlueprintService.AddTimelineReplaceExisting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceAddTimelineReplaceTest::RunTest(const FString&)
{
	const FString Path = TEXT("/Game/__VibeUETest/BP_TimelineReplace");
	UBlueprint* Blueprint = VibeUETestHelpers::MakeBlueprint(AActor::StaticClass(), Path);
	if (!TestNotNull(TEXT("created the transient Blueprint"), Blueprint))
	{
		return false;
	}
	ON_SCOPE_EXIT{ VibeUETestHelpers::ForgetBlueprint(Blueprint); };

	const FString FirstId = UBlueprintService::AddTimeline(Path, TEXT("EventGraph"), TEXT("PingPong"), 1.0f);
	if (!TestFalse(TEXT("first add_timeline returned a node id"), FirstId.IsEmpty() || FirstId.StartsWith(TEXT("ERROR:"))))
	{
		return false;
	}

	// Delete ONLY the Timeline node, leaving its UTimelineTemplate behind (the bug scenario).
	for (UEdGraph* Ubergraph : Blueprint->UbergraphPages)
	{
		if (!Ubergraph)
		{
			continue;
		}
		TArray<UK2Node_Timeline*> TimelineNodes;
		Ubergraph->GetNodesOfClass(TimelineNodes);
		for (UK2Node_Timeline* Node : TimelineNodes)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, /*bDontRecompile*/true);
		}
	}
	if (!TestNotNull(TEXT("the UTimelineTemplate survives the node delete"),
		Blueprint->FindTimelineTemplateByVariableName(FName(TEXT("PingPong")))))
	{
		return false;
	}

	// With replace_existing=true, add_timeline removes the leftover template and re-adds successfully.
	const FString SecondId = UBlueprintService::AddTimeline(Path, TEXT("EventGraph"), TEXT("PingPong"), 1.0f, false, false, false, 0.0f, 0.0f, /*bReplaceExisting*/true);
	TestFalse(TEXT("second add_timeline with replace_existing=true succeeded (not an ERROR sentinel)"),
		SecondId.IsEmpty() || SecondId.StartsWith(TEXT("ERROR:")));
	return true;
}

// Item 15: ActorService.rerun_construction_scripts finds a placed actor by label and re-runs it.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeActorServiceRerunConstructionTest, "VibeUE.ActorService.RerunConstructionScripts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeActorServiceRerunConstructionTest::RunTest(const FString&)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world is available"), World))
	{
		return false;
	}

	AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
	if (!TestNotNull(TEXT("spawned a StaticMeshActor in the editor world"), Actor))
	{
		return false;
	}
	ON_SCOPE_EXIT{ if (IsValid(Actor)) { World->DestroyActor(Actor); } };

	Actor->SetActorLabel(TEXT("VibeUE_RCS_Probe"));
	TestTrue(TEXT("rerun_construction_scripts by label returns true"),
		UActorService::RerunConstructionScripts(TEXT("VibeUE_RCS_Probe")));
	return true;
}

#endif // WITH_AUTOMATION_TESTS
