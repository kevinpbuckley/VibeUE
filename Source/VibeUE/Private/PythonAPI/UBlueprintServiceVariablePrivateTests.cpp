// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "PythonAPI/UBlueprintService.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "UObject/Package.h"

#if WITH_AUTOMATION_TESTS

// Regression coverage for get_variable_info is_private. My Blueprint's Private checkbox writes the
// BlueprintPrivate metadata (BlueprintDetailsCustomization.cpp); is_private was read from
// VarType.bIsConst, which is false on every member variable, so a variable ticked Private read False.
// The test ticks Private the way the checkbox does. Drives the public API only, so it compiles
// against the unfixed plugin. The Blueprint lives in-memory under /Game/__VibeUETest, registered
// with AssetCreated so LoadBlueprint can find it, never saved.

namespace VibeVariablePrivateTestUtil
{
	static UBlueprint* MakeRegisteredActorBlueprint(const FString& PackageName)
	{
		UPackage* Package = CreatePackage(*PackageName);
		const FString Name = FPackageName::GetShortName(PackageName);
		// CreateBlueprint check()s that the name is free in Outer; refuse here instead of asserting.
		if (!Package || FindObject<UObject>(Package, *Name))
		{
			return nullptr;
		}
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(), Package, FName(*Name), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		if (Blueprint)
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
			FAssetRegistryModule::AssetCreated(Blueprint);
		}
		return Blueprint;
	}

	static void Release(UBlueprint* Blueprint)
	{
		if (Blueprint)
		{
			FAssetRegistryModule::AssetDeleted(Blueprint);
			Blueprint->ClearFlags(RF_Standalone | RF_Public);
			// Never-saved test package: leave it clean so no later auto-save sweep writes it.
			Blueprint->GetPackage()->SetDirtyFlag(false);
		}
	}
}

// On 5-8 the failing assertion is "is_private is True for the variable ticked Private".
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBlueprintServiceVariablePrivateTest, "VibeUE.BlueprintService.GetVariableInfoReportsPrivate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeBlueprintServiceVariablePrivateTest::RunTest(const FString&)
{
	using namespace VibeVariablePrivateTestUtil;

	const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12);
	const FString Path = FString::Printf(TEXT("/Game/__VibeUETest/BP_VariablePrivate_%s"), *Suffix);
	const FString PrivateName = TEXT("VibePrivateVar");
	const FString PublicName = TEXT("VibePublicVar");

	UBlueprint* Blueprint = MakeRegisteredActorBlueprint(Path);
	ON_SCOPE_EXIT { Release(Blueprint); };
	if (!TestNotNull(TEXT("created the test Blueprint"), Blueprint) ||
		!TestTrue(TEXT("added the Private variable"), UBlueprintService::AddMemberVariable(Path, PrivateName, TEXT("float"))) ||
		!TestTrue(TEXT("added the public variable"), UBlueprintService::AddMemberVariable(Path, PublicName, TEXT("float"))))
	{
		return false;
	}

	// What My Blueprint's Private checkbox does (FBlueprintVarActionDetails::OnPrivateChanged).
	FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, FName(*PrivateName), nullptr, FBlueprintMetadata::MD_Private, TEXT("true"));
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

	FBlueprintVariableDetailedInfo PrivateInfo;
	FBlueprintVariableDetailedInfo PublicInfo;
	if (!TestTrue(TEXT("get_variable_info found the Private variable"), UBlueprintService::GetVariableInfo(Path, PrivateName, PrivateInfo)) ||
		!TestTrue(TEXT("get_variable_info found the public variable"), UBlueprintService::GetVariableInfo(Path, PublicName, PublicInfo)))
	{
		return false;
	}

	TestTrue(TEXT("is_private is True for the variable ticked Private"), PrivateInfo.bIsPrivate);
	TestFalse(TEXT("is_private is False for the public variable"), PublicInfo.bIsPrivate);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
