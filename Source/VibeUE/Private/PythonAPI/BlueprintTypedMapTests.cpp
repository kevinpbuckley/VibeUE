// Copyright Buckley Builds LLC 2026 All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "PythonAPI/BlueprintTypeParser.h"
#include "EdGraphSchema_K2.h"
#include "PythonAPI/UBlueprintService.h"
#include "EditorAssetLibrary.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PackageTools.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"

#if WITH_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeTypedMapTest, "VibeUE.BlueprintService.TypedMaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeTypedMapTest::RunTest(const FString&)
{
	FString Error;
	FEdGraphPinType Map;
	TestTrue(TEXT("name-to-int map parses"), FBlueprintTypeParser::ParseTypeString(TEXT("TMap<FName, int>"), Map, false, TEXT(""), Error));
	TestTrue(TEXT("Map container, key, and value"), Map.IsMap() && Map.PinCategory == UEdGraphSchema_K2::PC_Name && Map.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Int);
	TestTrue(TEXT("full struct and class paths"), FBlueprintTypeParser::ParseTypeString(TEXT("TMap<FName, TSubclassOf</Script/Engine.Actor>>"), Map, false, TEXT(""), Error));
	TestTrue(TEXT("value is a class"), Map.PinValueType.TerminalCategory == UEdGraphSchema_K2::PC_Class && Map.PinValueType.TerminalSubCategoryObject.IsValid());
	TestFalse(TEXT("engine rejects bool keys"), FBlueprintTypeParser::ParseTypeString(TEXT("TMap<bool, int>"), Map, false, TEXT(""), Error));
	const FString Root = TEXT("/Game/VibeUETests/Names_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString ShortName = TEXT("MapTerminal") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	UPackage* Left = CreatePackage(*(Root / TEXT("Left") / ShortName));
	UPackage* Right = CreatePackage(*(Root / TEXT("Right") / ShortName));
	UScriptStruct* First = NewObject<UScriptStruct>(Left, FName(*ShortName));
	UScriptStruct* Second = NewObject<UScriptStruct>(Right, FName(*ShortName));
	TestFalse(TEXT("ambiguous short name refused"), FBlueprintTypeParser::ParseTypeString(FString::Printf(TEXT("TMap<FName,%s>"), *ShortName), Map, false, FString(), Error));
	TestTrue(TEXT("both matching types identified"), Error.Contains(First->GetPathName()) && Error.Contains(Second->GetPathName()));
	TestTrue(TEXT("qualified path resolves the requested type"), FBlueprintTypeParser::ParseTypeString(FString::Printf(TEXT("TMap<FName,%s>"), *Second->GetPathName()), Map, false, FString(), Error) && Map.PinValueType.TerminalSubCategoryObject.Get() == Second);
	TestFalse(TEXT("nested container rejected"), FBlueprintTypeParser::ParseTypeString(TEXT("TMap<int, TMap<int, int>>"), Map, false, TEXT(""), Error));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeTypedMapReloadTest, "VibeUE.BlueprintService.TypedMapReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeTypedMapReloadTest::RunTest(const FString&)
{
	const FString AssetName = TEXT("BP_Map_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Path = TEXT("/Game/VibeUETests/") + AssetName;
	UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), CreatePackage(*Path), FName(*AssetName), BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	if (!TestNotNull(TEXT("fixture blueprint"), BP)) return false;
	FAssetRegistryModule::AssetCreated(BP);
	ON_SCOPE_EXIT { UEditorAssetLibrary::DeleteAsset(Path); };
	const FString Spec = TEXT("TMap</Script/Engine.ECollisionChannel, /Script/CoreUObject.Vector>");
	const FString RefSpec = TEXT("TMap<FString, /Script/Engine.MaterialInterface>");
	if (!TestTrue(TEXT("authored enum-to-struct"), UBlueprintService::AddMemberVariable(Path, TEXT("Vectors"), Spec)) ||
		!TestTrue(TEXT("authored string-to-object"), UBlueprintService::AddMemberVariable(Path, TEXT("Materials"), RefSpec))) return false;
	FKismetEditorUtilities::CompileBlueprint(BP);
	TestTrue(TEXT("compiled cleanly"), BP->Status == BS_UpToDate);
	FBlueprintVariableDetailedInfo Before;
	TestTrue(TEXT("descriptor available"), UBlueprintService::GetVariableInfo(Path, TEXT("Vectors"), Before) && !Before.MapTypeDescriptor.IsEmpty());
	TestTrue(TEXT("descriptor includes exact value struct"), Before.MapTypeDescriptor.Contains(TEXT("/Script/CoreUObject.Vector")));
	if (!TestTrue(TEXT("saved"), UEditorAssetLibrary::SaveLoadedAsset(BP, false))) return false;
	FText ReloadError;
	if (!TestTrue(TEXT("reloaded"), UPackageTools::ReloadPackages({BP->GetPackage()}, ReloadError, EReloadPackagesInteractionMode::AssumePositive))) return false;
	FBlueprintVariableDetailedInfo After;
	TestTrue(TEXT("read reloaded variable"), UBlueprintService::GetVariableInfo(Path, TEXT("Vectors"), After));
	TestEqual(TEXT("lossless descriptor survives reload"), After.MapTypeDescriptor, Before.MapTypeDescriptor);
	FEdGraphPinType Expected, Actual;
	FString Error;
	TestTrue(TEXT("authored type reparses"), FBlueprintTypeParser::ParseTypeString(Spec, Expected, false, FString(), Error));
	TestTrue(TEXT("descriptor decodes"), FEdGraphPinType::StaticStruct()->ImportText(*After.MapTypeDescriptor, &Actual, nullptr, PPF_None, GWarn, TEXT("EdGraphPinType")) != nullptr);
	TestTrue(TEXT("all pin fields agree"), Actual == Expected);
	return true;
}
#endif
