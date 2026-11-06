// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBlueprintService.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "BlueprintEditorLibrary.h"
#include "Components/SceneComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Select.h"
#include "K2Node_Self.h"
#include "K2Node_SpawnActorFromClass.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Serialization/ObjectWriter.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Script.h"
#include "UObject/Interface.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintServiceCompleteGraphInventoryTest,
	"VibeUE.Blueprint.Readback.CompleteGraphInventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintServiceCompleteGraphInventoryTest::RunTest(const FString&)
{
	TStrongObjectPtr<USceneComponent> Holder(NewObject<USceneComponent>(GetTransientPackage(), NAME_None, RF_Transient));
	const FName BlueprintName(*(TEXT("BP_GraphInventory_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), Holder.Get(), BlueprintName, BPTYPE_Normal, TEXT("VibeUEGraphInventoryTest")));
	if (!TestTrue(TEXT("transient fixture has generated classes"), Blueprint.IsValid()
		&& Blueprint->GeneratedClass && Blueprint->SkeletonGeneratedClass)) { return false; }
	Blueprint->SetFlags(RF_Transient);
	Blueprint->ClearFlags(RF_Standalone);
	const auto NewGraph = [](UObject* Outer, const TCHAR* Name)
	{
		UEdGraph* Graph = NewObject<UEdGraph>(Outer, Name, RF_Transient);
		Graph->Schema = UEdGraphSchema_K2::StaticClass();
		Graph->GraphGuid = FGuid::NewGuid();
		return Graph;
	};
	UEdGraph* Event = NewGraph(Blueprint.Get(), TEXT("InventoryEvent"));
	UEdGraph* Function = NewGraph(Blueprint.Get(), TEXT("InventoryFunction"));
	UEdGraph* Macro = NewGraph(Blueprint.Get(), TEXT("InventoryMacro"));
	UEdGraph* Delegate = NewGraph(Blueprint.Get(), TEXT("InventoryDelegate"));
	UEdGraph* Interface = NewGraph(Blueprint.Get(), TEXT("InventoryInterface"));
	UEdGraph* Child = NewGraph(Event, TEXT("InventoryChild"));
	UEdGraph* SameNameA = NewGraph(Event, TEXT("RepeatedName"));
	UEdGraph* SameNameB = NewGraph(Function, TEXT("RepeatedName"));
	Blueprint->UbergraphPages.Add(Event);
	Blueprint->FunctionGraphs.Add(Function);
	Blueprint->MacroGraphs.Add(Macro);
	Blueprint->DelegateSignatureGraphs.Add(Delegate);
	Event->SubGraphs.Add(Child);
	Event->SubGraphs.Add(SameNameA);
	Function->SubGraphs.Add(SameNameB);
	FBPInterfaceDescription InterfaceDescription;
	InterfaceDescription.Interface = UInterface::StaticClass();
	InterfaceDescription.Graphs.Add(Interface);
	Blueprint->ImplementedInterfaces.Add(InterfaceDescription);
	// Repeat the exact pointer via another collection; this is not a second graph.
	Blueprint->DelegateSignatureGraphs.Add(Function);
	const auto AddSentinel = [](UEdGraph* Graph)
	{
		UK2Node_Self* Node = NewObject<UK2Node_Self>(Graph, NAME_None, RF_Transient);
		Node->CreateNewGuid();
		Graph->AddNode(Node, false, false);
		Node->AllocateDefaultPins();
		return Node;
	};
	UK2Node_Self* InterfaceNode = AddSentinel(Interface);
	UK2Node_Self* ChildNode = AddSentinel(Child);

	// Reproduce the previous four-array algorithm without mutating the fixture.
	TArray<UEdGraph*> Legacy;
	Legacy.Append(Blueprint->UbergraphPages);
	Legacy.Append(Blueprint->FunctionGraphs);
	Legacy.Append(Blueprint->MacroGraphs);
	Legacy.Append(Blueprint->DelegateSignatureGraphs);
	TestFalse(TEXT("old inventory omitted implemented interface graph"), Legacy.Contains(Interface));
	TestFalse(TEXT("old inventory omitted nested graph"), Legacy.Contains(Child));
	TArray<UEdGraph*> Expected;
	TMap<UEdGraph*, FString> LegacyKinds;
	const auto RecordLegacy = [&Expected, &LegacyKinds](const TArray<UEdGraph*>& Collection, const TCHAR* Kind)
	{
		for (UEdGraph* Graph : Collection)
		{
			if (Graph && !LegacyKinds.Contains(Graph))
			{
				Expected.Add(Graph);
				LegacyKinds.Add(Graph, Kind);
			}
		}
	};
	RecordLegacy(Blueprint->UbergraphPages, TEXT("Ubergraph"));
	RecordLegacy(Blueprint->FunctionGraphs, TEXT("Function"));
	RecordLegacy(Blueprint->MacroGraphs, TEXT("Macro"));
	RecordLegacy(Blueprint->DelegateSignatureGraphs, TEXT("DelegateSignature"));
	const int32 LegacyCount = Expected.Num();
	// ListGraphs lists implemented-interface graphs right after the four collections.
	for (const FBPInterfaceDescription& Intf : Blueprint->ImplementedInterfaces)
	{
		for (UEdGraph* Graph : Intf.Graphs) { if (Graph) { Expected.AddUnique(Graph); } }
	}
	TArray<UEdGraph*> AllGraphs;
	Blueprint->GetAllGraphs(AllGraphs);
	for (UEdGraph* Graph : AllGraphs) { if (Graph) { Expected.AddUnique(Graph); } }

	TMap<UObject*, TArray<uint8>> Before;
	const auto RecordObject = [&Before](UObject* Object)
	{
		TArray<uint8>& Bytes = Before.Add(Object);
		FObjectWriter Writer(Object, Bytes, false, false, false, PPF_DuplicateVerbatim);
	};
	RecordObject(Blueprint.Get());
	for (UEdGraph* Graph : Expected)
	{
		RecordObject(Graph);
		for (UEdGraphNode* Node : Graph->Nodes) { if (Node) { RecordObject(Node); } }
	}
	const bool bDirtyBefore = Blueprint->GetPackage()->IsDirty();
	const auto StatusBefore = Blueprint->Status;
	const FString Path = Blueprint->GetPathName();
	const TArray<FBlueprintGraphInfo> Listed = UBlueprintService::ListGraphs(Path);
	if (TestEqual(TEXT("inventory contains each reachable graph object once"), Listed.Num(), Expected.Num()))
	{
		for (int32 Index = 0; Index < Expected.Num(); ++Index)
		{
			TestEqual(TEXT("legacy prefix and appended Engine order preserved"), Listed[Index].GraphName, Expected[Index]->GetName());
			TestEqual(TEXT("node count belongs to listed graph"), Listed[Index].NodeCount, Expected[Index]->Nodes.Num());
			if (Index < LegacyCount)
			{
				TestEqual(TEXT("legacy kind preserved"), Listed[Index].GraphKind, LegacyKinds[Expected[Index]]);
			}
			else
			{
				if (Expected[Index] == Interface)
				{
					TestTrue(TEXT("interface graph tagged with its interface"), Listed[Index].GraphKind.StartsWith(TEXT("Interface (")));
				}
				else
				{
					TestEqual(TEXT("missing graph classified by membership"), Listed[Index].GraphKind, FString(TEXT("Subgraph")));
				}
			}
		}
	}
	TestEqual(TEXT("duplicate pointer listed once"), Listed.FilterByPredicate(
		[Function](const FBlueprintGraphInfo& Info) { return Info.GraphName == Function->GetName(); }).Num(), 1);
	TestEqual(TEXT("distinct graph objects sharing a short name are not collapsed"), Listed.FilterByPredicate(
		[](const FBlueprintGraphInfo& Info) { return Info.GraphName == TEXT("RepeatedName"); }).Num(), 2);
	for (UK2Node_Self* Sentinel : {InterfaceNode, ChildNode})
	{
		const TArray<FBlueprintNodeInfo> Nodes = UBlueprintService::GetNodesInGraph(
			Path, Sentinel->GetGraph()->GetName(), 10, TEXT(""), true);
		TestTrue(TEXT("explicit lookup reaches the newly inventoried graph sentinel"), Nodes.ContainsByPredicate(
			[Sentinel](const FBlueprintNodeInfo& Info) { return Info.NodeId == Sentinel->NodeGuid.ToString(); }));
	}
	for (const TPair<UObject*, TArray<uint8>>& Entry : Before)
	{
		TArray<uint8> After;
		FObjectWriter Writer(Entry.Key, After, false, false, false, PPF_DuplicateVerbatim);
		TestTrue(TEXT("listing/readback preserves Blueprint, graph and node state"), Entry.Value == After);
	}
	TestEqual(TEXT("listing preserves Blueprint compile status"), Blueprint->Status, StatusBefore);
	TestEqual(TEXT("listing preserves existing package dirty state"), Blueprint->GetPackage()->IsDirty(), bDirtyBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintServiceUniqueFunctionResultPinsTest,
	"VibeUE.Blueprint.Readback.UniqueFunctionResultPins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintServiceUniqueFunctionResultPinsTest::RunTest(const FString&)
{
	TStrongObjectPtr<USceneComponent> Holder(NewObject<USceneComponent>(GetTransientPackage(), NAME_None, RF_Transient));
	const FName Name(*(TEXT("BP_UniqueResult_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), Holder.Get(), Name, BPTYPE_Normal, TEXT("VibeUEUniqueResultTest")));
	if (!TestTrue(TEXT("isolated Blueprint has generated classes"), Blueprint.IsValid()
		&& Blueprint->GeneratedClass && Blueprint->SkeletonGeneratedClass)) { return false; }
	Blueprint->SetFlags(RF_Transient);
	Blueprint->ClearFlags(RF_Standalone);
	const FString Path = Blueprint->GetPathName();
	const FString FunctionName(TEXT("UniqueResult"));
	UEdGraph* Graph = UBlueprintEditorLibrary::AddFunctionGraph(Blueprint.Get(), FunctionName);
	if (!TestNotNull(TEXT("function graph created"), Graph)) { return false; }
	TArray<UK2Node_FunctionEntry*> Entries;
	Graph->GetNodesOfClass(Entries);
	if (!TestEqual(TEXT("one entry"), Entries.Num(), 1)) { return false; }
	if (!TestTrue(TEXT("service adds the bool output"), UBlueprintService::AddFunctionParameter(
		Path, FunctionName, TEXT("ReturnValue"), TEXT("bool"), true))) { return false; }
	TArray<UK2Node_FunctionResult*> Returns;
	Graph->GetNodesOfClass(Returns);
	if (!TestEqual(TEXT("one result node"), Returns.Num(), 1)) { return false; }
	UK2Node_FunctionResult* Result = Returns[0];
	const auto CheckRawPins = [this, Result](const TCHAR* Stage)
	{
		int32 ExecCount = 0;
		int32 ReturnCount = 0;
		for (UEdGraphPin* Pin : Result->Pins)
		{
			if (!Pin) { continue; }
			if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) { ++ExecCount; }
			if (Pin->Direction == EGPD_Input && Pin->PinName == TEXT("ReturnValue")
				&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean) { ++ReturnCount; }
		}
		const bool bExec = TestEqual(FString(Stage) + TEXT(" has exactly one raw input exec"), ExecCount, 1);
		const bool bReturn = TestEqual(FString(Stage) + TEXT(" has exactly one raw bool ReturnValue"), ReturnCount, 1);
		const bool bTotal = TestEqual(FString(Stage) + TEXT(" has exactly the two expected pins"), Result->Pins.Num(), 2);
		return bExec && bReturn && bTotal;
	};
	// Count the raw array before any name-based API can hide duplicate names.
	if (!CheckRawPins(TEXT("immediately after AddFunctionParameter"))) { return false; }
	const FString EntryId = Entries[0]->NodeGuid.ToString();
	const FString ReturnId = Result->NodeGuid.ToString();
	const FString Flags = FString::FromInt(FUNC_Public | FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const);
	if (!TestTrue(TEXT("Writer-order flag configuration succeeds"), UBlueprintService::ConfigureNode(
		Path, FunctionName, EntryId, TEXT("ExtraFlags"), Flags)) ||
		!TestTrue(TEXT("Writer-order entry refresh succeeds"), UBlueprintService::RefreshNode(
			Path, FunctionName, EntryId, false))) { return false; }
	if (!CheckRawPins(TEXT("after entry configuration and refresh"))) { return false; }
	if (!TestTrue(TEXT("return literal is true"), UBlueprintService::SetNodePinValue(
		Path, FunctionName, ReturnId, TEXT("ReturnValue"), TEXT("true"))) ||
		!TestTrue(TEXT("entry connects to the only return exec"), UBlueprintService::ConnectNodes(
			Path, FunctionName, EntryId, TEXT("then"), ReturnId, TEXT("execute")))) { return false; }
	UEdGraphPin* Exec = Result->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	if (!TestNotNull(TEXT("return exec found"), Exec)) { return false; }
	TestEqual(TEXT("return exec has exactly one incoming connection"), Exec->LinkedTo.Num(), 1);
	TestTrue(TEXT("return exec comes from the intended entry"), Exec->LinkedTo.Num() == 1
		&& Exec->LinkedTo[0]->GetOwningNode() == Entries[0]);
	if (!TestTrue(TEXT("fixture compiles"), UBlueprintEditorLibrary::CompileBlueprint(Blueprint.Get()))) { return false; }
	TestTrue(TEXT("fixture compiles without the unconnected-return warning"), Blueprint->Status == BS_UpToDate);
	if (!CheckRawPins(TEXT("after compile"))) { return false; }
	Exec = Result->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
	TestTrue(TEXT("compile preserves the exact entry-to-return connection"), Exec && Exec->LinkedTo.Num() == 1
		&& Exec->LinkedTo[0]->GetOwningNode() == Entries[0]);
	UFunction* Function = Blueprint->GeneratedClass->FindFunctionByName(*FunctionName);
	if (!TestNotNull(TEXT("compiled function exists"), Function)) { return false; }
	const FBoolProperty* ReturnProperty = CastField<FBoolProperty>(Function->GetReturnProperty());
	if (!TestNotNull(TEXT("compiled function returns bool"), ReturnProperty)) { return false; }
	FStructOnScope Parameters(Function);
	FEditorScriptExecutionGuard ScriptGuard;
	Blueprint->GeneratedClass->GetDefaultObject()->ProcessEvent(Function, Parameters.GetStructMemory());
	TestTrue(TEXT("executed Blueprint returns true"), ReturnProperty->GetPropertyValue_InContainer(Parameters.GetStructMemory()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintServiceExactGeneratedClassPathTest,
	"VibeUE.Blueprint.Readback.ExactGeneratedClassPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintServiceExactGeneratedClassPathTest::RunTest(const FString&)
{
	TStrongObjectPtr<USceneComponent> Holder(NewObject<USceneComponent>(GetTransientPackage(), NAME_None, RF_Transient));
	const FName Name(*(TEXT("BP_ExactClass_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), Holder.Get(), Name, BPTYPE_Normal, TEXT("VibeUEExactClassTest")));
	if (!TestTrue(TEXT("isolated Blueprint exists"), Blueprint.IsValid())) { return false; }
	Blueprint->SetFlags(RF_Transient);
	Blueprint->ClearFlags(RF_Standalone);
	const FString CalleeName(TEXT("ExactPathCallee"));
	if (!TestNotNull(TEXT("callee graph exists"), UBlueprintEditorLibrary::AddFunctionGraph(Blueprint.Get(), CalleeName))
		|| !TestTrue(TEXT("callee compiled before resolution"), UBlueprintEditorLibrary::CompileBlueprint(Blueprint.Get()))) { return false; }
	UFunction* Expected = Blueprint->GeneratedClass->FindFunctionByName(*CalleeName);
	if (!TestNotNull(TEXT("exact generated-class function exists"), Expected)) { return false; }
	FGraphNodeDesc Node;
	Node.Ref = TEXT("ExactClassCall");
	Node.Type = TEXT("function_call");
	Node.Params.Add(TEXT("class"), Blueprint->GeneratedClass->GetPathName());
	Node.Params.Add(TEXT("function"), CalleeName);
	const FBuildGraphResult Result = UBlueprintService::BuildGraph(
		Blueprint->GetPathName(), TEXT("EventGraph"), {Node}, {}, {}, false, false);
	if (!TestTrue(TEXT("BuildGraph resolves the exact loaded BlueprintGeneratedClass path"), Result.bSuccess)) { return false; }
	UK2Node_CallFunction* Call = nullptr;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		for (UEdGraphNode* Candidate : Graph->Nodes)
		{
			if (UK2Node_CallFunction* Typed = Cast<UK2Node_CallFunction>(Candidate))
			{
				UFunction* Actual = Typed->GetTargetFunction();
				UClass* Owner = Actual ? Actual->GetOwnerClass() : nullptr;
				AddInfo(FString::Printf(TEXT("ExactClassCall actual=%s expected=%s owner=%s generated_by=%s"),
					*GetPathNameSafe(Actual), *GetPathNameSafe(Expected), *GetPathNameSafe(Owner),
					*GetPathNameSafe(Owner ? Owner->ClassGeneratedBy.Get() : nullptr)));
				// Editing nodes resolve through the owning Blueprint's skeleton;
				// compare exact Blueprint identity and member, not two phase-specific UFunctions.
				if (Actual && Actual->GetFName() == Expected->GetFName() && Owner
					&& Owner->ClassGeneratedBy == Blueprint.Get()
					&& (Owner == Blueprint->GeneratedClass || Owner == Blueprint->SkeletonGeneratedClass)) { Call = Typed; }
			}
		}
	}
	if (!TestNotNull(TEXT("call resolves the exact Blueprint and function member"), Call)) { return false; }
	return TestTrue(TEXT("resolved call compiles without warnings"),
		UBlueprintEditorLibrary::CompileBlueprint(Blueprint.Get()) && Blueprint->Status == BS_UpToDate);
}

#endif
