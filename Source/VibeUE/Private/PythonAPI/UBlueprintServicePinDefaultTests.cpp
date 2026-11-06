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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintServiceDefaultValueTest,
	"VibeUE.Blueprint.Readback.ClassObjectAndScalarDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintServiceDefaultValueTest::RunTest(const FString&)
{
	// The canonical factory creates valid generated/skeleton classes in memory.
	// Nested transient ownership keeps the service's ':' loader; no asset is saved.
	TStrongObjectPtr<USceneComponent> Holder(NewObject<USceneComponent>(GetTransientPackage(), NAME_None, RF_Transient));
	const FName BlueprintName(*FString::Printf(TEXT("BP_LiteralReadback_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), Holder.Get(), BlueprintName, BPTYPE_Normal, TEXT("VibeUEReadbackTest")));
	if (!TestTrue(TEXT("canonical transient Blueprint has generated classes"),
		Blueprint.IsValid() && Blueprint->GeneratedClass && Blueprint->SkeletonGeneratedClass))
	{
		return false;
	}
	Blueprint->SetFlags(RF_Transient);
	Blueprint->ClearFlags(RF_Standalone);
	UEdGraph* Graph = NewObject<UEdGraph>(Blueprint.Get(), TEXT("LiteralReadback"), RF_Transient);
	Graph->Schema = UEdGraphSchema_K2::StaticClass();
	Blueprint->UbergraphPages.Add(Graph);
	UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph, NAME_None, RF_Transient);
	Node->CreateNewGuid();
	Node->SetFromFunction(UGameplayStatics::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UGameplayStatics, BeginDeferredActorSpawnFromClass)));
	Graph->AddNode(Node, false, false);
	Node->AllocateDefaultPins();
	UEdGraphPin* ClassPin = Node->FindPin(TEXT("ActorClass"));
	UEdGraphPin* ObjectPin = Node->FindPin(TEXT("Owner"));
	UEdGraphPin* ScalarPin = Node->FindPin(TEXT("CollisionHandlingOverride"));
	if (!TestTrue(TEXT("real spawn function exposes fixture pins"), ClassPin && ObjectPin && ScalarPin))
	{
		return false;
	}
	ClassPin->DefaultValue.Reset();
	ClassPin->DefaultObject = AActor::StaticClass();
	ObjectPin->DefaultValue.Reset();
	ObjectPin->DefaultObject = AActor::StaticClass()->GetDefaultObject();
	ScalarPin->DefaultValue = TEXT("AlwaysSpawn");
	const FString BlueprintPath = Blueprint->GetPathName();
	TestTrue(TEXT("fixture has a transient subobject path"), BlueprintPath.Contains(TEXT(":")));
	const FString NodeId = Node->NodeGuid.ToString();
	TMap<FString, FString> Expected = {
		{TEXT("ActorClass"), AActor::StaticClass()->GetPathName()},
		{TEXT("Owner"), AActor::StaticClass()->GetDefaultObject()->GetPathName()},
		{TEXT("CollisionHandlingOverride"), TEXT("AlwaysSpawn")}
	};

	const auto CheckPins = [this, &Expected](const TCHAR* API, const auto& Pins)
	{
		for (const TPair<FString, FString>& Pair : Expected)
		{
			const auto* Info = Pins.FindByPredicate([&Pair](const auto& Item) { return Item.PinName == Pair.Key; });
			const FString Label = FString::Printf(TEXT("%s %s"), API, *Pair.Key);
			if (TestNotNull(Label + TEXT(" is present"), Info))
			{
				TestEqual(Label + TEXT(" retains the complete literal"), Info->DefaultValue, Pair.Value);
			}
		}
	};
	const TArray<FBlueprintNodeInfo> Nodes = UBlueprintService::GetNodesInGraph(
		BlueprintPath, Graph->GetName(), 10, TEXT(""), true);
	const FBlueprintNodeInfo* NodeInfo = Nodes.FindByPredicate(
		[&NodeId](const FBlueprintNodeInfo& Item) { return Item.NodeId == NodeId; });
	if (TestNotNull(TEXT("GetNodesInGraph returns the fixture"), NodeInfo))
	{
		CheckPins(TEXT("GetNodesInGraph"), NodeInfo->Pins);
	}
	CheckPins(TEXT("GetNodePins"), UBlueprintService::GetNodePins(BlueprintPath, Graph->GetName(), NodeId));
	FBlueprintNodeDetailedInfo Details;
	if (TestTrue(TEXT("GetNodeDetails succeeds"), UBlueprintService::GetNodeDetails(
		BlueprintPath, Graph->GetName(), NodeId, Details)))
	{
		CheckPins(TEXT("GetNodeDetails"), Details.InputPins);
	}
	for (const FString& NullValue : {FString(TEXT("None")), FString()})
	{
		TestTrue(TEXT("restore a class reference before clearing"), UBlueprintService::SetNodePinValue(
			BlueprintPath, Graph->GetName(), NodeId, TEXT("ActorClass"), AActor::StaticClass()->GetPathName()));
		TestTrue(TEXT("restore an object reference before clearing"), UBlueprintService::SetNodePinValue(
			BlueprintPath, Graph->GetName(), NodeId, TEXT("Owner"), AActor::StaticClass()->GetDefaultObject()->GetPathName()));
		for (const TCHAR* ReferencePinName : {TEXT("ActorClass"), TEXT("Owner")})
		{
			TestTrue(FString::Printf(TEXT("clear %s using '%s'"), ReferencePinName, *NullValue),
				UBlueprintService::SetNodePinValue(BlueprintPath, Graph->GetName(), NodeId, ReferencePinName, NullValue));
			const UEdGraphPin* ClearedPin = Node->FindPin(ReferencePinName);
			TestTrue(TEXT("native reference default and string readback are empty"),
				ClearedPin && !ClearedPin->DefaultObject && ClearedPin->GetDefaultAsString().IsEmpty());
		}
		Expected[TEXT("ActorClass")].Empty();
		Expected[TEXT("Owner")].Empty();
		const TArray<FBlueprintNodeInfo> ClearedNodes = UBlueprintService::GetNodesInGraph(
			BlueprintPath, Graph->GetName(), 10, TEXT(""), true);
		const FBlueprintNodeInfo* ClearedInfo = ClearedNodes.FindByPredicate(
			[&NodeId](const FBlueprintNodeInfo& Item) { return Item.NodeId == NodeId; });
		if (TestNotNull(TEXT("GetNodesInGraph returns cleared references"), ClearedInfo))
		{
			CheckPins(TEXT("GetNodesInGraph clear"), ClearedInfo->Pins);
		}
		CheckPins(TEXT("GetNodePins clear"), UBlueprintService::GetNodePins(BlueprintPath, Graph->GetName(), NodeId));
		if (TestTrue(TEXT("GetNodeDetails reads cleared references"), UBlueprintService::GetNodeDetails(
			BlueprintPath, Graph->GetName(), NodeId, Details)))
		{
			CheckPins(TEXT("GetNodeDetails clear"), Details.InputPins);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintServiceSpawnMetadataAndPlacementTest,
	"VibeUE.Blueprint.Readback.SpawnMetadataAndNodePlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintServiceSpawnMetadataAndPlacementTest::RunTest(const FString&)
{
	TStrongObjectPtr<USceneComponent> Holder(NewObject<USceneComponent>(GetTransientPackage(), NAME_None, RF_Transient));
	const FName BlueprintName(*FString::Printf(TEXT("BP_SpawnReadback_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), Holder.Get(), BlueprintName, BPTYPE_Normal, TEXT("VibeUESpawnReadbackTest")));
	if (!TestTrue(TEXT("canonical transient spawn fixture has generated classes"),
		Blueprint.IsValid() && Blueprint->GeneratedClass && Blueprint->SkeletonGeneratedClass))
	{
		return false;
	}
	Blueprint->SetFlags(RF_Transient);
	Blueprint->ClearFlags(RF_Standalone);
	const FString BlueprintPath = Blueprint->GetPathName();
	TestTrue(TEXT("spawn fixture is a transient subobject, never a disk asset"), BlueprintPath.Contains(TEXT(":")));
	const FName VariableName(TEXT("SpawnValue"));
	if (!TestTrue(TEXT("add an authored scalar variable"), UBlueprintService::AddMemberVariable(
		BlueprintPath, VariableName.ToString(), TEXT("int"), TEXT("17"), false, TEXT(""))))
	{
		return false;
	}
	UBlueprintEditorLibrary::SetBlueprintVariableInstanceEditable(Blueprint.Get(), VariableName, true);
	FBlueprintVariableDetailedInfo Info;
	TestTrue(TEXT("ordinary variable can be read"), UBlueprintService::GetVariableInfo(BlueprintPath, VariableName.ToString(), Info));
	TestFalse(TEXT("ordinary variable is not exposed on spawn"), Info.bIsExposeOnSpawn);
	UBlueprintEditorLibrary::SetBlueprintVariableExposeOnSpawn(Blueprint.Get(), VariableName, true);
	const FBPVariableDescription* Description = Blueprint->NewVariables.FindByPredicate(
		[VariableName](const FBPVariableDescription& Item) { return Item.VarName == VariableName; });
	if (!TestNotNull(TEXT("authored variable description exists"), Description))
	{
		return false;
	}
	TestTrue(TEXT("Editor library authored ExposeOnSpawn metadata"), Description->HasMetaData(FBlueprintMetadata::MD_ExposeOnSpawn));
	TestFalse(TEXT("Editor library did not set the description flag"), (Description->PropertyFlags & CPF_ExposeOnSpawn) != 0);
	TestTrue(TEXT("metadata-authored variable can be read"), UBlueprintService::GetVariableInfo(BlueprintPath, VariableName.ToString(), Info));
	TestTrue(TEXT("readback recognizes the real metadata exposure"), Info.bIsExposeOnSpawn);
	TestTrue(TEXT("readback preserves instance editing"), Info.bIsInstanceEditable);
	if (!TestTrue(TEXT("metadata-authored Blueprint compiles"), UBlueprintEditorLibrary::CompileBlueprint(Blueprint.Get())))
	{
		return false;
	}
	const FIntProperty* Property = FindFProperty<FIntProperty>(Blueprint->GeneratedClass, VariableName);
	if (!TestNotNull(TEXT("compiled spawn property exists"), Property))
	{
		return false;
	}
	TestTrue(TEXT("compiler creates the real ExposeOnSpawn property flag"), Property->HasAllPropertyFlags(CPF_ExposeOnSpawn));
	TestTrue(TEXT("compiled property uses the Engine spawn exposure predicate"), UEdGraphSchema_K2::IsPropertyExposedOnSpawn(Property));
	TestEqual(TEXT("compiled CDO retains the authored value"), Property->GetPropertyValue_InContainer(Blueprint->GeneratedClass->GetDefaultObject()), 17);

	UEdGraph* Graph = NewObject<UEdGraph>(Blueprint.Get(), TEXT("SpawnPlacement"), RF_Transient);
	Graph->Schema = UEdGraphSchema_K2::StaticClass();
	Blueprint->UbergraphPages.Add(Graph);
	const FString GraphName = Graph->GetName();
	const TArray<UClass*> NodeClasses = {UK2Node_SpawnActorFromClass::StaticClass(), UK2Node_Self::StaticClass(), UK2Node_Select::StaticClass()};
	TArray<FString> CreatedIds;
	const auto CheckNode = [this, &Blueprint, &BlueprintPath, &GraphName, &VariableName](const FString& NodeId, UClass* NodeClass)
	{
		FBlueprintNodeDetailedInfo Details;
		if (!TestTrue(TEXT("public node details are available"), UBlueprintService::GetNodeDetails(BlueprintPath, GraphName, NodeId, Details)))
		{
			return;
		}
		TestEqual(TEXT("requested generic node class survives initialization"), Details.NodeClass, NodeClass->GetName());
		if (NodeClass == UK2Node_SpawnActorFromClass::StaticClass())
		{
			// UE 5.8 PostPlacedNewNode reads this allocated pin and sets this exact default.
			const FBlueprintPinDetailedInfo* Scale = Details.InputPins.FindByPredicate(
				[](const FBlueprintPinDetailedInfo& Item) { return Item.PinName == TEXT("TransformScaleMethod"); });
			if (TestNotNull(TEXT("SpawnActor scale pin exists before placement callback"), Scale))
			{
				TestEqual(TEXT("placement callback ran after pin allocation"), Scale->DefaultValue, FString(TEXT("MultiplyWithRoot")));
			}
			TestTrue(TEXT("public Class pin setter accepts the generated actor class"), UBlueprintService::SetNodePinValue(
				BlueprintPath, GraphName, NodeId, TEXT("Class"), Blueprint->GeneratedClass->GetPathName()));
			if (TestTrue(TEXT("typed spawn pins can be read"), UBlueprintService::GetNodeDetails(BlueprintPath, GraphName, NodeId, Details)))
			{
				const FBlueprintPinDetailedInfo* SpawnValue = Details.InputPins.FindByPredicate(
					[&VariableName](const FBlueprintPinDetailedInfo& Item) { return Item.PinName == VariableName.ToString(); });
				if (TestNotNull(TEXT("real SpawnActor exposes the metadata-authored variable"), SpawnValue))
				{
					TestEqual(TEXT("spawn pin retains scalar type"), SpawnValue->PinCategory, FString(TEXT("int")));
					TestEqual(TEXT("spawn pin defaults to the compiled CDO value"), SpawnValue->DefaultValue, FString(TEXT("17")));
				}
			}
		}
	};
	for (UClass* NodeClass : NodeClasses)
	{
		const FString NodeId = UBlueprintService::CreateNodeByKey(BlueprintPath, GraphName,
			TEXT("NODE ") + NodeClass->GetName(), 10.0f, 20.0f);
		if (!TestFalse(TEXT("CreateNodeByKey returns an owned node ID"), NodeId.IsEmpty()))
		{
			return false;
		}
		CreatedIds.Add(NodeId);
		CheckNode(NodeId, NodeClass);
	}
	TArray<FGraphNodeDesc> Descriptions;
	for (UClass* NodeClass : NodeClasses)
	{
		FGraphNodeDesc& Item = Descriptions.AddDefaulted_GetRef();
		Item.Ref = NodeClass->GetName();
		Item.Type = TEXT("spawner_key");
		Item.Params.Add(TEXT("key"), TEXT("NODE ") + NodeClass->GetName());
	}
	const FBuildGraphResult Result = UBlueprintService::BuildGraph(BlueprintPath, GraphName, Descriptions, {}, {}, false, false);
	if (!TestTrue(TEXT("BuildGraph initializes all generic nodes without a placement failure"), Result.bSuccess && Result.Errors.IsEmpty() && Result.NodesCreated == NodeClasses.Num()))
	{
		return false;
	}
	for (UClass* NodeClass : NodeClasses)
	{
		const FString* NodeId = Result.RefToNodeId.Find(NodeClass->GetName());
		if (TestNotNull(TEXT("BuildGraph returns an owned node ID"), NodeId))
		{
			CreatedIds.Add(*NodeId);
			CheckNode(*NodeId, NodeClass);
		}
	}
	for (const FString& NodeId : CreatedIds)
	{
		TestTrue(TEXT("remove only owned transient fixture nodes"), UBlueprintService::DeleteNode(BlueprintPath, GraphName, NodeId));
	}
	TestTrue(TEXT("fixture graph is empty after exact cleanup"), Graph->Nodes.IsEmpty());
	UBlueprintEditorLibrary::SetBlueprintVariableExposeOnSpawn(Blueprint.Get(), VariableName, false);
	TestTrue(TEXT("disabled exposure can be read"), UBlueprintService::GetVariableInfo(BlueprintPath, VariableName.ToString(), Info));
	TestFalse(TEXT("removing authored metadata disables exposure readback"), Info.bIsExposeOnSpawn);
	return true;
}

#endif
