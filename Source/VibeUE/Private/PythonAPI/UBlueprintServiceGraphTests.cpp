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
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_VariableGet.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintVariableNodeSpawner.h"
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

// ============================================================================
// Shared helpers for the batch of graph-authoring regression tests below.
// ============================================================================
namespace VibeBlueprintServiceTestUtil
{
	// Create an in-memory Blueprint under a /Game path and register it with the asset registry so
	// UBlueprintService's path-based API can resolve it. Never saved to disk. The caller runs the
	// cleanup lambda on every exit path.
	static UBlueprint* MakeRegisteredBlueprint(FAutomationTestBase& Test, const FString& PackageName, UClass* ParentClass)
	{
		UPackage* Package = CreatePackage(*PackageName);
		if (!Test.TestNotNull(TEXT("created a package for the transient test Blueprint"), Package))
		{
			return nullptr;
		}

		FString ShortName;
		PackageName.Split(TEXT("/"), nullptr, &ShortName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass, Package, FName(*ShortName), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (!Test.TestNotNull(TEXT("CreateBlueprint returned a Blueprint"), Blueprint))
		{
			return nullptr;
		}

		FAssetRegistryModule::AssetCreated(Blueprint);
		return Blueprint;
	}

	static void ReleaseBlueprint(UBlueprint* Blueprint)
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

	static UEdGraphNode* FindNodeByGuidString(UEdGraph* Graph, const FString& GuidString)
	{
		if (!Graph)
		{
			return nullptr;
		}
		FGuid Parsed;
		if (!FGuid::Parse(GuidString, Parsed))
		{
			return nullptr;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == Parsed)
			{
				return Node;
			}
		}
		return nullptr;
	}

	static UEdGraph* GetEventGraph(UBlueprint* Blueprint)
	{
		if (Blueprint && Blueprint->UbergraphPages.Num() > 0)
		{
			return Blueprint->UbergraphPages[0];
		}
		return nullptr;
	}
}

// ============================================================================
// Item 2: build_graph pin defaults must be able to set a hard class pin (Pin->DefaultObject),
// not just plain string pins. Builds a GetAllActorsOfClass node and sets its ActorClass default
// through build_graph, then asserts the resolved class landed in Pin->DefaultObject.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceBuildGraphClassPinDefaultTest, "VibeUE.BlueprintService.BuildGraphSetsClassPinDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceBuildGraphClassPinDefaultTest::RunTest(const FString&)
{
	using namespace VibeBlueprintServiceTestUtil;

	const FString PackageName = TEXT("/Game/__VibeUETest/BP_ClassPinDefault");
	UBlueprint* Blueprint = MakeRegisteredBlueprint(*this, PackageName, AActor::StaticClass());
	if (!Blueprint)
	{
		return false;
	}
	ON_SCOPE_EXIT { ReleaseBlueprint(Blueprint); };

	const FString Path = PackageName;

	TArray<FGraphNodeDesc> Nodes;
	{
		FGraphNodeDesc Node;
		Node.Ref = TEXT("getall");
		Node.Type = TEXT("function_call");
		Node.Params.Add(TEXT("class"), TEXT("GameplayStatics"));
		Node.Params.Add(TEXT("function"), TEXT("GetAllActorsOfClass"));
		Nodes.Add(Node);
	}

	TArray<FGraphConnectionDesc> Connections;

	TArray<FGraphPinDefaultDesc> PinDefaults;
	{
		FGraphPinDefaultDesc PinDefault;
		PinDefault.NodeRef = TEXT("getall");
		PinDefault.PinName = TEXT("ActorClass");
		PinDefault.Value = TEXT("/Script/Engine.StaticMeshActor");
		PinDefaults.Add(PinDefault);
	}

	const FBuildGraphResult Result = UBlueprintService::BuildGraph(Path, TEXT("EventGraph"), Nodes, Connections, PinDefaults, false, false);

	TestEqual(TEXT("one node created"), Result.NodesCreated, 1);
	TestEqual(TEXT("the class pin default was set (not dropped)"), Result.DefaultsSet, 1);
	TestEqual(TEXT("no pin defaults failed"), Result.DefaultsFailed, 0);

	const FString* NodeGuid = Result.RefToNodeId.Find(TEXT("getall"));
	if (!TestTrue(TEXT("build_graph reported a GUID for the GetAllActorsOfClass node"), NodeGuid != nullptr))
	{
		return false;
	}

	UEdGraphNode* Node = FindNodeByGuidString(GetEventGraph(Blueprint), *NodeGuid);
	if (!TestNotNull(TEXT("resolved the created node by GUID"), Node))
	{
		return false;
	}

	UEdGraphPin* ActorClassPin = Node->FindPin(TEXT("ActorClass"), EGPD_Input);
	if (!TestNotNull(TEXT("GetAllActorsOfClass has an ActorClass input pin"), ActorClassPin))
	{
		return false;
	}

	UClass* ExpectedClass = LoadObject<UClass>(nullptr, TEXT("/Script/Engine.StaticMeshActor"));
	if (!TestNotNull(TEXT("resolved AStaticMeshActor class for comparison"), ExpectedClass))
	{
		return false;
	}

	// The whole point of the fix: a hard class pin stores the resolved class in DefaultObject,
	// which the old TrySetDefaultValue-only path could not write.
	TestTrue(TEXT("ActorClass DefaultObject is the StaticMeshActor class"), ActorClassPin->DefaultObject == ExpectedClass);

	return true;
}

// ============================================================================
// Item 3: get_graph_definition must emit the function entry and result terminals (and their exec
// wire) so a dumped function graph round-trips. Creates a function with one input and one output,
// wires entry -> result, then asserts both terminals and the wire appear in the definition.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceGetGraphDefinitionEntryResultTest, "VibeUE.BlueprintService.GetGraphDefinitionEmitsEntryResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceGetGraphDefinitionEntryResultTest::RunTest(const FString&)
{
	using namespace VibeBlueprintServiceTestUtil;

	const FString PackageName = TEXT("/Game/__VibeUETest/BP_EntryResultRoundTrip");
	UBlueprint* Blueprint = MakeRegisteredBlueprint(*this, PackageName, AActor::StaticClass());
	if (!Blueprint)
	{
		return false;
	}
	ON_SCOPE_EXIT { ReleaseBlueprint(Blueprint); };

	const FString Path = PackageName;
	const FString FuncName = TEXT("VibeRoundTripFunc");

	// Create a user function graph with entry + result terminals.
	UEdGraph* FuncGraph = FBlueprintEditorUtils::CreateNewGraph(
		Blueprint, FName(*FuncName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	if (!TestNotNull(TEXT("created a function graph"), FuncGraph))
	{
		return false;
	}
	FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, FuncGraph, /*bIsUserCreated*/ true, (UClass*)nullptr);

	// One input and one output parameter (the output guarantees a result terminal exists).
	TestTrue(TEXT("added input parameter"),
		UBlueprintService::AddFunctionParameter(Path, FuncName, TEXT("InValue"), TEXT("int"), false, false, TEXT(""), false, TEXT("")));
	TestTrue(TEXT("added output parameter"),
		UBlueprintService::AddFunctionParameter(Path, FuncName, TEXT("OutValue"), TEXT("int"), true, false, TEXT(""), false, TEXT("")));

	// Locate the entry and result terminals and ensure their exec pins are wired entry -> result.
	UK2Node_FunctionEntry* EntryNode = nullptr;
	UK2Node_FunctionResult* ResultNode = nullptr;
	for (UEdGraphNode* Node : FuncGraph->Nodes)
	{
		if (!EntryNode) { EntryNode = Cast<UK2Node_FunctionEntry>(Node); }
		if (!ResultNode) { ResultNode = Cast<UK2Node_FunctionResult>(Node); }
	}
	if (!TestNotNull(TEXT("function graph has an entry terminal"), EntryNode) ||
		!TestNotNull(TEXT("function graph has a result terminal"), ResultNode))
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

	auto FindExecPin = [](UEdGraphNode* Node, EEdGraphPinDirection Dir) -> UEdGraphPin*
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Dir && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
		return nullptr;
	};

	UEdGraphPin* EntryExecOut = FindExecPin(EntryNode, EGPD_Output);
	UEdGraphPin* ResultExecIn = FindExecPin(ResultNode, EGPD_Input);
	if (!TestNotNull(TEXT("entry has an exec output pin"), EntryExecOut) ||
		!TestNotNull(TEXT("result has an exec input pin"), ResultExecIn))
	{
		return false;
	}
	if (EntryExecOut->LinkedTo.Num() == 0)
	{
		const UEdGraphSchema* Schema = FuncGraph->GetSchema();
		Schema->TryCreateConnection(EntryExecOut, ResultExecIn);
	}
	TestTrue(TEXT("entry exec is wired to result exec"), EntryExecOut->LinkedTo.Contains(ResultExecIn));

	// Dump the definition and verify the terminals + their wire survive.
	TArray<FGraphNodeDesc> OutNodes;
	TArray<FGraphConnectionDesc> OutConnections;
	TArray<FGraphPinDefaultDesc> OutPinDefaults;
	FString OutError;
	const bool bDumped = UBlueprintService::GetGraphDefinition(Path, FuncName, OutNodes, OutConnections, OutPinDefaults, OutError);
	if (!TestTrue(FString::Printf(TEXT("GetGraphDefinition succeeded (error: %s)"), *OutError), bDumped))
	{
		return false;
	}

	bool bHasEntry = false;
	bool bHasResult = false;
	for (const FGraphNodeDesc& Desc : OutNodes)
	{
		if (Desc.Ref == TEXT("entry") && Desc.Type == TEXT("function_entry"))
		{
			bHasEntry = true;
			TestTrue(TEXT("entry desc is marked existing"), Desc.Params.Contains(TEXT("existing")));
		}
		if (Desc.Ref == TEXT("result") && Desc.Type == TEXT("function_result"))
		{
			bHasResult = true;
			TestTrue(TEXT("result desc is marked existing"), Desc.Params.Contains(TEXT("existing")));
		}
	}
	TestTrue(TEXT("definition includes the entry terminal as an existing node"), bHasEntry);
	TestTrue(TEXT("definition includes the result terminal as an existing node"), bHasResult);

	bool bHasEntryToResultWire = false;
	for (const FGraphConnectionDesc& Conn : OutConnections)
	{
		if (Conn.From.StartsWith(TEXT("entry.")) && Conn.To.StartsWith(TEXT("result.")))
		{
			bHasEntryToResultWire = true;
			break;
		}
	}
	TestTrue(TEXT("definition includes the entry -> result exec wire"), bHasEntryToResultWire);

	return true;
}

// ============================================================================
// Item 4: create_node_by_key must refuse a foreign variable even when EXACTLY ONE spawner matches
// (the owner check previously ran only for >1 matches). A uniquely-named variable is created on a
// "foreign" Blueprint; spawning its getter on an unrelated target must return an empty id.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceForeignVariableSingleMatchTest, "VibeUE.BlueprintService.CreateNodeByKeyRefusesForeignSingleMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceForeignVariableSingleMatchTest::RunTest(const FString&)
{
	using namespace VibeBlueprintServiceTestUtil;

	const FString ForeignPackage = TEXT("/Game/__VibeUETest/BP_ForeignOwner");
	const FString TargetPackage  = TEXT("/Game/__VibeUETest/BP_ForeignVarTarget");
	const FName   ForeignVarName(TEXT("VibeForeignVarUnique1985"));

	UBlueprint* ForeignBP = MakeRegisteredBlueprint(*this, ForeignPackage, AActor::StaticClass());
	UBlueprint* TargetBP  = MakeRegisteredBlueprint(*this, TargetPackage, AActor::StaticClass());
	if (!ForeignBP || !TargetBP)
	{
		return false;
	}
	ON_SCOPE_EXIT { ReleaseBlueprint(ForeignBP); ReleaseBlueprint(TargetBP); };

	// Add a uniquely-named bool variable to the foreign Blueprint and compile so its spawner primes.
	FEdGraphPinType BoolType;
	BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	if (!TestTrue(TEXT("added the foreign variable"),
		FBlueprintEditorUtils::AddMemberVariable(ForeignBP, ForeignVarName, BoolType)))
	{
		return false;
	}
	FKismetEditorUtilities::CompileBlueprint(ForeignBP);
	FBlueprintActionDatabase::Get().RefreshAssetActions(ForeignBP);

	// Find the foreign variable's getter spawner and read its exact menu name; count matches so we
	// know we are exercising the single-match path.
	FString MenuName;
	int32 MatchCount = 0;
	const FBlueprintActionDatabase::FActionRegistry& Registry = FBlueprintActionDatabase::Get().GetAllActions();
	for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Entry : Registry)
	{
		for (UBlueprintNodeSpawner* Candidate : Entry.Value)
		{
			if (!Candidate || !Candidate->NodeClass || Candidate->NodeClass->GetName() != TEXT("K2Node_VariableGet"))
			{
				continue;
			}
			const UBlueprintVariableNodeSpawner* VarSpawner = Cast<UBlueprintVariableNodeSpawner>(Candidate);
			if (!VarSpawner)
			{
				continue;
			}
			const FProperty* VarProp = VarSpawner->GetVarProperty();
			if (VarProp && VarProp->GetFName() == ForeignVarName)
			{
				MenuName = Candidate->PrimeDefaultUiSpec(nullptr).MenuName.ToString();
			}
		}
	}

	// Recount matches against the resolved menu name exactly the way CreateNodeByKey does.
	if (!MenuName.IsEmpty())
	{
		for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& Entry : Registry)
		{
			for (UBlueprintNodeSpawner* Candidate : Entry.Value)
			{
				if (!Candidate || !Candidate->NodeClass || Candidate->NodeClass->GetName() != TEXT("K2Node_VariableGet"))
				{
					continue;
				}
				if (Candidate->PrimeDefaultUiSpec(nullptr).MenuName.ToString() == MenuName)
				{
					++MatchCount;
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

	if (!TestTrue(TEXT("found the foreign variable's getter spawner menu name"), !MenuName.IsEmpty()))
	{
		return false;
	}
	// A uniquely-named variable on a single Blueprint should produce exactly one matching spawner —
	// the single-match path this test targets (item 4). Reported, not aborted: even if the registry
	// surfaced more than one, they are all foreign and the refusal below must still hold.
	TestEqual(TEXT("exactly one spawner matches the foreign variable's menu name (single-match path)"), MatchCount, 1);

	// Spawning the foreign getter on the unrelated target must be refused (empty id).
	const FString SpawnKey = FString::Printf(TEXT("SPAWN K2Node_VariableGet|%s"), *MenuName);
	const FString NodeId = UBlueprintService::CreateNodeByKey(TargetPackage, TEXT("EventGraph"), SpawnKey, 0.0f, 0.0f);
	TestTrue(TEXT("create_node_by_key refused the single foreign-variable match (empty id)"), NodeId.IsEmpty());

	return true;
}

// ============================================================================
// Item 9: compile_blueprint returns the compiler's error text. Compiles a deliberately broken
// Blueprint — a function-call node in the BeginPlay exec chain whose FunctionReference names a
// function that does not exist — and asserts the result reports errors by count AND message.
//
// Why this fixture: an unresolved UK2Node_CallFunction (GetTargetFunction()==nullptr) is a hard
// compile Error in FKismetCompilerContext (K2Node_CallFunction::ValidateNodeDuringCompilation emits
// MessageLog.Error "Could not find a function named ..."). But AllocateDefaultPins skips
// CreatePinsForFunctionCall when the function is null, so a node created bad has no exec pin and
// would be pruned as isolated. So we spawn a REAL PrintString call (which has exec pins), wire it
// from BeginPlay so it survives pruning, THEN corrupt its FunctionReference to a bogus self member.
// A unique package name keeps the fixture independent of any asset left over from a prior run.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceCompileBlueprintErrorsTest, "VibeUE.BlueprintService.CompileBlueprintReportsErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceCompileBlueprintErrorsTest::RunTest(const FString&)
{
	using namespace VibeBlueprintServiceTestUtil;

	const FString PackageName = FString::Printf(TEXT("/Game/__VibeUETest/BP_CompileErrors_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	UBlueprint* Blueprint = MakeRegisteredBlueprint(*this, PackageName, AActor::StaticClass());
	if (!Blueprint)
	{
		return false;
	}
	ON_SCOPE_EXIT { ReleaseBlueprint(Blueprint); };

	const FString Path = PackageName;

	UEdGraph* EventGraph = GetEventGraph(Blueprint);
	if (!TestNotNull(TEXT("blueprint has an EventGraph"), EventGraph))
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

	// BeginPlay -> PrintString, wired via the service. PrintString gives the call node real exec pins.
	const FString BeginPlayId = UBlueprintService::CreateNodeByKey(Path, TEXT("EventGraph"), TEXT("EVENT Actor::ReceiveBeginPlay"), 0.0f, 0.0f);
	const FString CallId      = UBlueprintService::CreateNodeByKey(Path, TEXT("EventGraph"), TEXT("FUNC KismetSystemLibrary::PrintString"), 320.0f, 0.0f);
	if (!TestFalse(TEXT("BeginPlay node created"), BeginPlayId.IsEmpty()) ||
		!TestFalse(TEXT("PrintString call node created"), CallId.IsEmpty()))
	{
		return false;
	}
	TestTrue(TEXT("wired BeginPlay.then -> Call.execute"),
		UBlueprintService::ConnectNodes(Path, TEXT("EventGraph"), BeginPlayId, TEXT("then"), CallId, TEXT("execute")));

	// Corrupt the call node's function reference to a name that does not exist on this Blueprint's own
	// class. The node stays wired into the exec chain (so it is not pruned), and the compiler cannot
	// resolve the function — a hard error naming the bogus function.
	const FName BogusFunctionName(TEXT("ThisFunctionDoesNotExistZZZ"));
	UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(FindNodeByGuidString(EventGraph, CallId));
	if (!TestNotNull(TEXT("resolved the PrintString call node to corrupt"), CallNode))
	{
		return false;
	}
	CallNode->FunctionReference.SetSelfMember(BogusFunctionName);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

	const FBlueprintCompileResult Result = UBlueprintService::CompileBlueprint(Path);

	TestFalse(TEXT("compile is reported as failed"), Result.bSuccess);
	TestTrue(TEXT("compile reports at least one error (NumErrors > 0)"), Result.NumErrors > 0);
	TestTrue(TEXT("compile returns non-empty error text"), Result.Errors.Num() > 0);

	// The error text should name the missing function.
	bool bErrorMentionsBogusName = false;
	for (const FString& Err : Result.Errors)
	{
		if (Err.Contains(BogusFunctionName.ToString()))
		{
			bErrorMentionsBogusName = true;
			break;
		}
	}
	TestTrue(TEXT("an error message names the missing function"), bErrorMentionsBogusName);

	return true;
}

// ============================================================================
// Item 16: create_node_by_key can spawn a getter for an SCS component variable (previously returned
// an empty id). Adds a StaticMeshComponent and spawns its getter, asserting the node exists and its
// member reference names the component.
// ============================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceComponentGetterSpawnTest, "VibeUE.BlueprintService.CreateNodeByKeySpawnsComponentGetter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceComponentGetterSpawnTest::RunTest(const FString&)
{
	using namespace VibeBlueprintServiceTestUtil;

	const FString PackageName = TEXT("/Game/__VibeUETest/BP_ComponentGetter");
	const FString ComponentName = TEXT("VibeMeshThing");
	UBlueprint* Blueprint = MakeRegisteredBlueprint(*this, PackageName, AActor::StaticClass());
	if (!Blueprint)
	{
		return false;
	}
	ON_SCOPE_EXIT { ReleaseBlueprint(Blueprint); };

	const FString Path = PackageName;

	if (!TestTrue(TEXT("added a StaticMeshComponent to the blueprint"),
		UBlueprintService::AddComponent(Path, TEXT("StaticMeshComponent"), ComponentName, TEXT(""))))
	{
		return false;
	}

	// Spawn the component getter via the key that used to return an empty id.
	const FString SpawnKey = FString::Printf(TEXT("SPAWN K2Node_VariableGet|Get %s"), *ComponentName);
	const FString NodeId = UBlueprintService::CreateNodeByKey(Path, TEXT("EventGraph"), SpawnKey, 0.0f, 0.0f);
	if (!TestFalse(TEXT("component getter spawn returned a non-empty id"), NodeId.IsEmpty()))
	{
		return false;
	}

	UEdGraphNode* Node = FindNodeByGuidString(GetEventGraph(Blueprint), NodeId);
	UK2Node_VariableGet* GetNode = Cast<UK2Node_VariableGet>(Node);
	if (!TestNotNull(TEXT("spawned node is a K2Node_VariableGet"), GetNode))
	{
		return false;
	}

	TestEqual(TEXT("getter's member reference names the component"),
		GetNode->VariableReference.GetMemberName().ToString(), ComponentName);

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
