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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueprintServiceNativeInterfaceTest,
	"VibeUE.Blueprint.Readback.NativeInterface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueprintServiceNativeInterfaceTest::RunTest(const FString&)
{
	TStrongObjectPtr<USceneComponent> Holder(NewObject<USceneComponent>(GetTransientPackage(), NAME_None, RF_Transient));
	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TStrongObjectPtr<UBlueprint> Blueprint(FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(), Holder.Get(), FName(*(TEXT("BP_NativeInterface_") + Suffix)),
		BPTYPE_Normal, TEXT("VibeUEInterfaceTest")));
	TStrongObjectPtr<UBlueprint> Interface(FKismetEditorUtilities::CreateBlueprint(
		UInterface::StaticClass(), Holder.Get(), FName(*(TEXT("BPI_ExistingPath_") + Suffix)),
		BPTYPE_Interface, TEXT("VibeUEInterfaceTest")));
	if (!TestTrue(TEXT("isolated Blueprint and interface fixtures exist"), Blueprint.IsValid() && Interface.IsValid())) return false;
	Blueprint->SetFlags(RF_Transient);
	Blueprint->ClearFlags(RF_Standalone);
	Interface->SetFlags(RF_Transient);
	Interface->ClearFlags(RF_Standalone);
	if (!TestTrue(TEXT("Blueprint interface fixture compiles"), UBlueprintEditorLibrary::CompileBlueprint(Interface.Get()))) return false;
	TestTrue(TEXT("Existing Blueprint asset path remains supported"),
		UBlueprintService::AddInterface(Blueprint->GetPathName(), Interface->GetPathName()));
	TestTrue(TEXT("Compiled class implements the Blueprint interface"), Blueprint->GeneratedClass->ImplementsInterface(Interface->GeneratedClass));
	const FString NativePath(TEXT("/Script/Engine.CurveSourceInterface"));
	UClass* Native = LoadClass<UInterface>(nullptr, *NativePath);
	if (!TestNotNull(TEXT("Native Blueprint-capable interface exists"), Native)) return false;
	if (!TestTrue(TEXT("AddInterface resolves an exact native /Script/ class"),
		UBlueprintService::AddInterface(Blueprint->GetPathName(), NativePath))) return false;
	TestTrue(TEXT("Compiled class implements the native interface"), Blueprint->GeneratedClass->ImplementsInterface(Native));
	const int32 Count = Blueprint->ImplementedInterfaces.Num();
	TestTrue(TEXT("Repeating the native interface is idempotent"), UBlueprintService::AddInterface(Blueprint->GetPathName(), NativePath));
	TestEqual(TEXT("No duplicate interface was created"), Blueprint->ImplementedInterfaces.Num(), Count);
	return TestTrue(TEXT("Both interface paths compile without errors"),
		UBlueprintEditorLibrary::CompileBlueprint(Blueprint.Get()) && Blueprint->Status == BS_UpToDate);
}

#endif
