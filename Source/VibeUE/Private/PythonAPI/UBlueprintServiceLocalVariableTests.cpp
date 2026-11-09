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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintServiceFunctionLocalWritableTest,
	"VibeUE.Blueprint.Readback.FunctionLocalWritable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintServiceFunctionLocalWritableTest::RunTest(const FString&)
{
	TStrongObjectPtr<USceneComponent> Holder(NewObject<USceneComponent>(GetTransientPackage(), NAME_None, RF_Transient));
	const FName BlueprintName(*FString::Printf(TEXT("BP_LocalWritable_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), Holder.Get(), BlueprintName, BPTYPE_Normal, TEXT("VibeUELocalWritableTest")));
	if (!TestTrue(TEXT("canonical transient local-variable fixture has generated classes"),
		Blueprint.IsValid() && Blueprint->GeneratedClass && Blueprint->SkeletonGeneratedClass))
	{
		return false;
	}
	Blueprint->SetFlags(RF_Transient);
	Blueprint->ClearFlags(RF_Standalone);
	const FString BlueprintPath = Blueprint->GetPathName();
	const FString FunctionName(TEXT("LocalWritable"));
	const FName LocalName(TEXT("LoadingResult"));
	UEdGraph* Graph = UBlueprintEditorLibrary::AddFunctionGraph(Blueprint.Get(), FunctionName);
	if (!TestNotNull(TEXT("function graph exists"), Graph))
	{
		return false;
	}
	TArray<UK2Node_FunctionEntry*> Entries;
	Graph->GetNodesOfClass(Entries);
	if (!TestEqual(TEXT("function has one entry"), Entries.Num(), 1))
	{
		return false;
	}
	Entries[0]->SetExtraFlags(FUNC_Public | FUNC_BlueprintCallable | FUNC_BlueprintPure | FUNC_Const);
	if (!TestTrue(TEXT("add bool return through the service"), UBlueprintService::AddFunctionParameter(
		BlueprintPath, FunctionName, TEXT("ReturnValue"), TEXT("bool"), true)) ||
		!TestTrue(TEXT("add local through the service"), UBlueprintService::AddFunctionLocalVariable(
			BlueprintPath, FunctionName, LocalName.ToString(), TEXT("bool"), TEXT("false"))))
	{
		return false;
	}
	const FBPVariableDescription* Local = Entries[0]->LocalVariables.FindByPredicate(
		[LocalName](const FBPVariableDescription& Item) { return Item.VarName == LocalName; });
	if (!TestNotNull(TEXT("service authored the function-local description"), Local))
	{
		return false;
	}
	TestTrue(TEXT("local description is Blueprint visible"), (Local->PropertyFlags & CPF_BlueprintVisible) != 0);
	TestFalse(TEXT("local description is not Blueprint read-only"), (Local->PropertyFlags & CPF_BlueprintReadOnly) != 0);
	TArray<UK2Node_FunctionResult*> Returns;
	Graph->GetNodesOfClass(Returns);
	if (!TestEqual(TEXT("function has one return"), Returns.Num(), 1))
	{
		return false;
	}
	TArray<FGraphNodeDesc> Descriptions;
	for (const TCHAR* Kind : {TEXT("variable_set"), TEXT("variable_get")})
	{
		FGraphNodeDesc& Description = Descriptions.AddDefaulted_GetRef();
		Description.Ref = Kind;
		Description.Type = Kind;
		Description.Params.Add(TEXT("variable"), LocalName.ToString());
	}
	const FBuildGraphResult Built = UBlueprintService::BuildGraph(BlueprintPath, FunctionName, Descriptions, {}, {}, false, false);
	if (!TestTrue(TEXT("service creates local setter/getter nodes"), Built.bSuccess && Built.Errors.IsEmpty() && Built.NodesCreated == 2))
	{
		return false;
	}
	const FString* Setter = Built.RefToNodeId.Find(TEXT("variable_set"));
	const FString* Getter = Built.RefToNodeId.Find(TEXT("variable_get"));
	if (!TestTrue(TEXT("both local node IDs exist"), Setter && Getter))
	{
		return false;
	}
	const FString LocalReference = FString::Printf(TEXT("(MemberName=\"%s\",MemberScope=\"%s\",bSelfContext=False)"),
		*LocalName.ToString(), *FunctionName);
	for (const FString* NodeId : {Setter, Getter})
	{
		if (!TestTrue(TEXT("bind the authored node to the function local"), UBlueprintService::ConfigureNode(
			BlueprintPath, FunctionName, *NodeId, TEXT("VariableReference"), LocalReference)) ||
			!TestTrue(TEXT("refresh local binding pins"), UBlueprintService::RefreshNode(BlueprintPath, FunctionName, *NodeId, false)))
		{
			return false;
		}
	}
	const FString EntryId = Entries[0]->NodeGuid.ToString();
	const FString ReturnId = Returns[0]->NodeGuid.ToString();
	if (!TestTrue(TEXT("setter writes true"), UBlueprintService::SetNodePinValue(BlueprintPath, FunctionName, *Setter, LocalName.ToString(), TEXT("true"))) ||
		!TestTrue(TEXT("entry executes the setter"), UBlueprintService::ConnectNodes(BlueprintPath, FunctionName, EntryId, TEXT("then"), *Setter, TEXT("execute"))) ||
		!TestTrue(TEXT("setter reaches return"), UBlueprintService::ConnectNodes(BlueprintPath, FunctionName, *Setter, TEXT("then"), ReturnId, TEXT("execute"))) ||
		!TestTrue(TEXT("return reads the local"), UBlueprintService::ConnectNodes(BlueprintPath, FunctionName, *Getter, LocalName.ToString(), ReturnId, TEXT("ReturnValue"))))
	{
		return false;
	}
	if (!TestTrue(TEXT("pure const function with a local setter compiles"), UBlueprintEditorLibrary::CompileBlueprint(Blueprint.Get())))
	{
		return false;
	}
	UFunction* Function = Blueprint->GeneratedClass->FindFunctionByName(*FunctionName);
	if (!TestNotNull(TEXT("compiled function exists"), Function))
	{
		return false;
	}
	TestTrue(TEXT("compiled function remains pure and const"), Function->HasAllFunctionFlags(FUNC_BlueprintPure | FUNC_Const));
	const FBoolProperty* CompiledLocal = FindFProperty<FBoolProperty>(Function, LocalName);
	const FBoolProperty* ReturnProperty = CastField<FBoolProperty>(Function->GetReturnProperty());
	if (!TestTrue(TEXT("compiled local and bool return exist"), CompiledLocal && ReturnProperty))
	{
		return false;
	}
	TestTrue(TEXT("compiled local is Blueprint visible"), CompiledLocal->HasAllPropertyFlags(CPF_BlueprintVisible));
	TestFalse(TEXT("compiled local is writable"), CompiledLocal->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
	FStructOnScope Parameters(Function);
	FEditorScriptExecutionGuard ScriptGuard;
	Blueprint->GeneratedClass->GetDefaultObject()->ProcessEvent(Function, Parameters.GetStructMemory());
	TestTrue(TEXT("compiled setter changes false local to true and returns it"),
		ReturnProperty->GetPropertyValue_InContainer(Parameters.GetStructMemory()));
	return true;
}

#endif
