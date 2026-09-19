// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UPIEActorService.h"

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

// Friend accessor: exercises the pure validators and reads the session serial without a running PIE
// session. These forward to the service's private, non-UFUNCTION test surface.
struct FPIEActorServiceTestAccess
{
	static FString Selector(const FString& S)  { return UPIEActorService::ValidateSelectorCode(S); }
	static FString Collision(const FString& S) { return UPIEActorService::ValidateCollisionCode(S); }
	static FString ClassCode(const FString& S) { return UPIEActorService::ValidateClassCode(S); }
	static int32   Serial()                    { return UPIEActorService::GetSessionSerialForTest(); }
};

namespace
{
	TSharedPtr<FJsonObject> ParsePIEJson(const FString& Text)
	{
		TSharedPtr<FJsonObject> Object;
		return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) ? Object : nullptr;
	}
}

// -----------------------------------------------------------------------------
// ResolveWorld with no PIE running -> PIE_NOT_RUNNING (grammar-valid selector).
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIEActorResolveNoPIETest, "VibeUE.PIEActor.ResolveWorldNoPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIEActorResolveNoPIETest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Result = ParsePIEJson(UPIEActorService::ResolveWorld(TEXT("server")));
	if (!TestTrue(TEXT("result is JSON"), Result.IsValid())) { return false; }
	TestFalse(TEXT("resolve fails without PIE"), Result->GetBoolField(TEXT("success")));
	TestEqual(TEXT("error is PIE_NOT_RUNNING"), Result->GetStringField(TEXT("error_code")), FString(TEXT("PIE_NOT_RUNNING")));
	return true;
}

// -----------------------------------------------------------------------------
// SpawnActor with no PIE -> PIE_NOT_RUNNING, and ListSpawned reports nothing.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIEActorSpawnNoPIETest, "VibeUE.PIEActor.SpawnNoPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIEActorSpawnNoPIETest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Spawn = ParsePIEJson(
		UPIEActorService::SpawnActor(TEXT("server"), TEXT("/Script/Engine.StaticMeshActor"), FTransform::Identity));
	if (!TestTrue(TEXT("spawn result is JSON"), Spawn.IsValid())) { return false; }
	TestFalse(TEXT("spawn fails without PIE"), Spawn->GetBoolField(TEXT("success")));
	TestEqual(TEXT("error is PIE_NOT_RUNNING"), Spawn->GetStringField(TEXT("error_code")), FString(TEXT("PIE_NOT_RUNNING")));

	const TSharedPtr<FJsonObject> List = ParsePIEJson(UPIEActorService::ListSpawned());
	if (!TestTrue(TEXT("list result is JSON"), List.IsValid())) { return false; }
	TestTrue(TEXT("list succeeds"), List->GetBoolField(TEXT("success")));
	TestEqual(TEXT("nothing spawned this session"), (int32)List->GetNumberField(TEXT("count")), 0);
	const TArray<TSharedPtr<FJsonValue>>* Spawned = nullptr;
	if (TestTrue(TEXT("spawned array present"), List->TryGetArrayField(TEXT("spawned"), Spawned)))
	{
		TestEqual(TEXT("spawned array empty"), Spawned->Num(), 0);
	}
	return true;
}

// -----------------------------------------------------------------------------
// Selector grammar: rejects malformed selectors, accepts the valid forms.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIEActorSelectorGrammarTest, "VibeUE.PIEActor.SelectorGrammar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIEActorSelectorGrammarTest::RunTest(const FString&)
{
	const FString Invalid(TEXT("INVALID_SELECTOR"));
	TestEqual(TEXT("empty is invalid"), FPIEActorServiceTestAccess::Selector(TEXT("")), Invalid);
	TestEqual(TEXT("client:0 is invalid"), FPIEActorServiceTestAccess::Selector(TEXT("client:0")), Invalid);
	TestEqual(TEXT("instance:-1 is invalid"), FPIEActorServiceTestAccess::Selector(TEXT("instance:-1")), Invalid);
	TestEqual(TEXT("editor is invalid"), FPIEActorServiceTestAccess::Selector(TEXT("editor")), Invalid);

	TestEqual(TEXT("server is valid"), FPIEActorServiceTestAccess::Selector(TEXT("server")), FString());
	TestEqual(TEXT("client is valid"), FPIEActorServiceTestAccess::Selector(TEXT("client")), FString());
	TestEqual(TEXT("client:1 is valid"), FPIEActorServiceTestAccess::Selector(TEXT("client:1")), FString());
	TestEqual(TEXT("instance:0 is valid"), FPIEActorServiceTestAccess::Selector(TEXT("instance:0")), FString());
	return true;
}

// -----------------------------------------------------------------------------
// Collision policy names: exact set only.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIEActorCollisionPolicyTest, "VibeUE.PIEActor.CollisionPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIEActorCollisionPolicyTest::RunTest(const FString&)
{
	TestEqual(TEXT("AlwaysSpawn valid"), FPIEActorServiceTestAccess::Collision(TEXT("AlwaysSpawn")), FString());
	TestEqual(TEXT("AdjustIfPossibleButAlwaysSpawn valid"),
		FPIEActorServiceTestAccess::Collision(TEXT("AdjustIfPossibleButAlwaysSpawn")), FString());
	TestEqual(TEXT("AdjustIfPossibleButDontSpawnIfColliding valid"),
		FPIEActorServiceTestAccess::Collision(TEXT("AdjustIfPossibleButDontSpawnIfColliding")), FString());
	TestEqual(TEXT("DontSpawnIfColliding valid"),
		FPIEActorServiceTestAccess::Collision(TEXT("DontSpawnIfColliding")), FString());
	TestEqual(TEXT("bogus rejected"),
		FPIEActorServiceTestAccess::Collision(TEXT("Sometimes")), FString(TEXT("INVALID_COLLISION_HANDLING")));
	return true;
}

// -----------------------------------------------------------------------------
// Class validation without PIE (native classes are always loaded).
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIEActorClassValidationTest, "VibeUE.PIEActor.ClassValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIEActorClassValidationTest::RunTest(const FString&)
{
	TestEqual(TEXT("StaticMeshActor is a valid actor class"),
		FPIEActorServiceTestAccess::ClassCode(TEXT("/Script/Engine.StaticMeshActor")), FString());
	TestEqual(TEXT("ALight is abstract"),
		FPIEActorServiceTestAccess::ClassCode(TEXT("/Script/Engine.Light")), FString(TEXT("CLASS_ABSTRACT")));
	TestEqual(TEXT("Texture2D is not an actor"),
		FPIEActorServiceTestAccess::ClassCode(TEXT("/Script/Engine.Texture2D")), FString(TEXT("NOT_AN_ACTOR_CLASS")));
	TestEqual(TEXT("missing path is CLASS_NOT_FOUND"),
		FPIEActorServiceTestAccess::ClassCode(TEXT("/Game/VibeUE_Automation/DoesNotExist_PIEActor")), FString(TEXT("CLASS_NOT_FOUND")));
	return true;
}

// -----------------------------------------------------------------------------
// DestroyActor handle handling: unknown vs. stale (deterministic via the serial accessor).
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIEActorDestroyHandleTest, "VibeUE.PIEActor.DestroyHandle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIEActorDestroyHandleTest::RunTest(const FString&)
{
	// Malformed handle (no serial:guid shape) -> UNKNOWN_HANDLE.
	{
		const TSharedPtr<FJsonObject> R = ParsePIEJson(UPIEActorService::DestroyActor(TEXT("bogus")));
		if (!TestTrue(TEXT("malformed result is JSON"), R.IsValid())) { return false; }
		TestFalse(TEXT("malformed fails"), R->GetBoolField(TEXT("success")));
		TestEqual(TEXT("malformed is UNKNOWN_HANDLE"), R->GetStringField(TEXT("error_code")), FString(TEXT("UNKNOWN_HANDLE")));
	}

	const int32 CurrentSerial = FPIEActorServiceTestAccess::Serial();

	// Well-formed handle with the CURRENT serial but an unknown GUID -> UNKNOWN_HANDLE.
	{
		const FString Handle = FString::Printf(TEXT("%d:00000000-0000-0000-0000-000000000000"), CurrentSerial);
		const TSharedPtr<FJsonObject> R = ParsePIEJson(UPIEActorService::DestroyActor(Handle));
		if (!TestTrue(TEXT("current-serial result is JSON"), R.IsValid())) { return false; }
		TestFalse(TEXT("current-serial unknown fails"), R->GetBoolField(TEXT("success")));
		TestEqual(TEXT("current-serial unknown is UNKNOWN_HANDLE"),
			R->GetStringField(TEXT("error_code")), FString(TEXT("UNKNOWN_HANDLE")));
	}

	// Well-formed handle whose serial is guaranteed to differ from the current one -> STALE_HANDLE.
	{
		const FString Handle = FString::Printf(TEXT("%d:00000000-0000-0000-0000-000000000000"), CurrentSerial + 1);
		const TSharedPtr<FJsonObject> R = ParsePIEJson(UPIEActorService::DestroyActor(Handle));
		if (!TestTrue(TEXT("stale result is JSON"), R.IsValid())) { return false; }
		TestFalse(TEXT("stale fails"), R->GetBoolField(TEXT("success")));
		TestEqual(TEXT("mismatched serial is STALE_HANDLE"),
			R->GetStringField(TEXT("error_code")), FString(TEXT("STALE_HANDLE")));
	}
	return true;
}

// -----------------------------------------------------------------------------
// DestroyAll on an empty registry is a harmless, successful no-op.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIEActorDestroyAllEmptyTest, "VibeUE.PIEActor.DestroyAllEmpty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIEActorDestroyAllEmptyTest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> R = ParsePIEJson(UPIEActorService::DestroyAll());
	if (!TestTrue(TEXT("result is JSON"), R.IsValid())) { return false; }
	TestTrue(TEXT("destroy_all succeeds when empty"), R->GetBoolField(TEXT("success")));
	TestEqual(TEXT("nothing destroyed"), (int32)R->GetNumberField(TEXT("destroyed")), 0);
	TestEqual(TEXT("nothing already gone"), (int32)R->GetNumberField(TEXT("already_gone")), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
