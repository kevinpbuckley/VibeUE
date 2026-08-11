// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UBlackboardService.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"

static const EAutomationTestFlags kBBTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

static const TCHAR* kBBTestDir = TEXT("/Game/Developers/VibeUEBTTests");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeBBKeyCrudTest,
	"VibeUE.Blackboard.Keys.Crud", kBBTestFlags)
bool FVibeBBKeyCrudTest::RunTest(const FString&)
{
	const FString Path = FString(kBBTestDir) / TEXT("BB_VibeTest");

	TestTrue(TEXT("created"), UBlackboardService::CreateBlackboard(Path, FString()));

	// A fresh blackboard has no keys.
	TestEqual(TEXT("empty on create"), UBlackboardService::GetBlackboardKeys(Path).Num(), 0);

	// One key of each scalar type.
	for (const TCHAR* Type : { TEXT("Bool"), TEXT("Int"), TEXT("Float"), TEXT("Vector"),
	                           TEXT("Name"), TEXT("String"), TEXT("Rotator") })
	{
		const FString Err = UBlackboardService::AddBlackboardKey(
			Path, FString::Printf(TEXT("Key%s"), Type), Type, false);
		TestEqual(FString::Printf(TEXT("add %s"), Type), Err, FString());
	}

	TArray<FBBKeyInfo> Keys = UBlackboardService::GetBlackboardKeys(Path);
	TestEqual(TEXT("seven keys"), Keys.Num(), 7);

	const FBBKeyInfo* Bool = Keys.FindByPredicate(
		[](const FBBKeyInfo& K){ return K.Name == TEXT("KeyBool"); });
	TestNotNull(TEXT("KeyBool present"), Bool);
	if (Bool)
	{
		TestEqual(TEXT("KeyBool type"), Bool->Type, FString(TEXT("Bool")));
		TestFalse(TEXT("KeyBool not synced"), Bool->bInstanceSynced);
	}

	// An unknown type is a reported error, not a silently broken key.
	const FString BadErr = UBlackboardService::AddBlackboardKey(
		Path, TEXT("KeyBogus"), TEXT("NotAType"), false);
	TestTrue(TEXT("unknown type rejected"), !BadErr.IsEmpty());
	TestEqual(TEXT("no key added"), UBlackboardService::GetBlackboardKeys(Path).Num(), 7);

	// A duplicate name is rejected.
	TestTrue(TEXT("duplicate rejected"),
		!UBlackboardService::AddBlackboardKey(Path, TEXT("KeyBool"), TEXT("Bool"), false).IsEmpty());

	// Object key carries a base class.
	TestEqual(TEXT("add object key"),
		UBlackboardService::AddBlackboardKey(Path, TEXT("KeyActor"), TEXT("Object"), true), FString());
	TestEqual(TEXT("set base class"),
		UBlackboardService::SetBlackboardKeyObjectClass(Path, TEXT("KeyActor"), TEXT("/Script/Engine.Actor")),
		FString());
	Keys = UBlackboardService::GetBlackboardKeys(Path);
	const FBBKeyInfo* Actor = Keys.FindByPredicate(
		[](const FBBKeyInfo& K){ return K.Name == TEXT("KeyActor"); });
	TestNotNull(TEXT("KeyActor present"), Actor);
	if (Actor)
	{
		TestTrue(TEXT("KeyActor synced"), Actor->bInstanceSynced);
		TestTrue(TEXT("KeyActor base class"), Actor->ObjectClassPath.Contains(TEXT("Actor")));
	}

	// Remove.
	TestEqual(TEXT("remove"), UBlackboardService::RemoveBlackboardKey(Path, TEXT("KeyBool")), FString());
	TestEqual(TEXT("seven after remove"), UBlackboardService::GetBlackboardKeys(Path).Num(), 7);
	TestTrue(TEXT("removing a missing key errors"),
		!UBlackboardService::RemoveBlackboardKey(Path, TEXT("KeyBool")).IsEmpty());

	return true;
}

#endif // WITH_AUTOMATION_TESTS
