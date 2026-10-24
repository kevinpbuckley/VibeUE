// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UPIEActorService.h"
#include "PythonAPI/PIEActorServiceCallTestTarget.h"

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

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

// =============================================================================
// call_function / console_command — headless (no PIE). The marshalling + call core is exercised on a
// transient UVibeUEPIECallTestTarget through UPIEActorService::CallFunctionOnObject.
// =============================================================================

namespace
{
	/** Call through the testable core and parse the reply. Null when the reply is not JSON. */
	TSharedPtr<FJsonObject> CallOn(UObject* Target, const TCHAR* Function, const TCHAR* Args)
	{
		FString Reply;
		UPIEActorService::CallFunctionOnObject(Target, Function, Args, Reply);
		return ParsePIEJson(Reply);
	}

	FString ErrorCodeOf(const TSharedPtr<FJsonObject>& Reply)
	{
		FString Code;
		if (Reply.IsValid())
		{
			Reply->TryGetStringField(TEXT("error_code"), Code);
		}
		return Code;
	}
}

// -----------------------------------------------------------------------------
// Return values, out params, string/object/default arguments round-trip.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIECallFunctionRoundTripTest, "VibeUE.PIEActorService.CallFunction.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIECallFunctionRoundTripTest::RunTest(const FString&)
{
	UVibeUEPIECallTestTarget* Target = NewObject<UVibeUEPIECallTestTarget>(GetTransientPackage());
	UVibeUEPIECallTestTarget* Other = NewObject<UVibeUEPIECallTestTarget>(GetTransientPackage());
	if (!TestNotNull(TEXT("target created"), Target) || !TestNotNull(TEXT("other created"), Other))
	{
		return false;
	}

	// int32 Add(int32, int32): return value, local callspace (a plain UObject is never remote).
	// Parameter names match case-insensitively ("b").
	{
		const TSharedPtr<FJsonObject> R = CallOn(Target, TEXT("Add"), TEXT("{\"A\": 2, \"b\": 3}"));
		if (!TestTrue(TEXT("Add reply is JSON"), R.IsValid())) { return false; }
		TestTrue(TEXT("Add succeeds"), R->GetBoolField(TEXT("success")));
		TestEqual(TEXT("Add returns 5"), (int32)R->GetNumberField(TEXT("return")), 5);
		TestEqual(TEXT("Add callspace is local"), R->GetStringField(TEXT("callspace")), FString(TEXT("local")));
		TestTrue(TEXT("Add executed locally"), R->GetBoolField(TEXT("executed_locally")));
		TestEqual(TEXT("function name echoed"), R->GetStringField(TEXT("function")), FString(TEXT("Add")));
	}

	// void SetLabel(const FString&): side effect lands, return is null.
	{
		const TSharedPtr<FJsonObject> R = CallOn(Target, TEXT("SetLabel"), TEXT("{\"InLabel\": \"hello world\"}"));
		if (!TestTrue(TEXT("SetLabel reply is JSON"), R.IsValid())) { return false; }
		TestTrue(TEXT("SetLabel succeeds"), R->GetBoolField(TEXT("success")));
		TestEqual(TEXT("label stored"), Target->Label, FString(TEXT("hello world")));
		TestTrue(TEXT("void return is null"), R->HasTypedField<EJson::Null>(TEXT("return")));
	}

	// void WithOut(int32 In, int32& Out, FString& OutText): out-only params need no input.
	{
		const TSharedPtr<FJsonObject> R = CallOn(Target, TEXT("WithOut"), TEXT("{\"In\": 7}"));
		if (!TestTrue(TEXT("WithOut reply is JSON"), R.IsValid())) { return false; }
		TestTrue(TEXT("WithOut succeeds"), R->GetBoolField(TEXT("success")));
		const TSharedPtr<FJsonObject>* OutParams = nullptr;
		if (TestTrue(TEXT("out_params present"), R->TryGetObjectField(TEXT("out_params"), OutParams)))
		{
			TestEqual(TEXT("Out = 14"), (int32)(*OutParams)->GetNumberField(TEXT("Out")), 14);
			TestEqual(TEXT("OutText round-trips"), (*OutParams)->GetStringField(TEXT("OutText")), FString(TEXT("in=7")));
			TestFalse(TEXT("input param not echoed as out"), (*OutParams)->HasField(TEXT("In")));
		}
	}

	// FString Describe(UObject*): object argument by full path, and "None" as null.
	{
		const FString Args = FString::Printf(TEXT("{\"Other\": \"%s\"}"), *Other->GetPathName());
		const TSharedPtr<FJsonObject> R = CallOn(Target, TEXT("Describe"), *Args);
		if (!TestTrue(TEXT("Describe reply is JSON"), R.IsValid())) { return false; }
		TestTrue(TEXT("Describe succeeds"), R->GetBoolField(TEXT("success")));
		TestEqual(TEXT("object path resolved"), R->GetStringField(TEXT("return")), Other->GetName());

		const TSharedPtr<FJsonObject> RNull = CallOn(Target, TEXT("Describe"), TEXT("{\"Other\": \"None\"}"));
		if (!TestTrue(TEXT("Describe(None) reply is JSON"), RNull.IsValid())) { return false; }
		TestEqual(TEXT("None is null"), RNull->GetStringField(TEXT("return")), FString(TEXT("null")));
	}

	// int32 Scale(int32 Value, int32 Factor = 3): a missing input takes its declared C++ default.
	{
		const TSharedPtr<FJsonObject> R = CallOn(Target, TEXT("Scale"), TEXT("{\"Value\": 4}"));
		if (!TestTrue(TEXT("Scale reply is JSON"), R.IsValid())) { return false; }
		TestTrue(TEXT("Scale succeeds with default"), R->GetBoolField(TEXT("success")));
		TestEqual(TEXT("default Factor=3 used"), (int32)R->GetNumberField(TEXT("return")), 12);
	}

	// An empty args string is the same as {}.
	{
		const TSharedPtr<FJsonObject> R = CallOn(Target, TEXT("GuardWasLowered"), TEXT(""));
		if (!TestTrue(TEXT("empty-args reply is JSON"), R.IsValid())) { return false; }
		TestTrue(TEXT("empty args accepted"), R->GetBoolField(TEXT("success")));
	}
	return true;
}

// -----------------------------------------------------------------------------
// Rejections happen before anything is called, each with its own error_code.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIECallFunctionErrorsTest, "VibeUE.PIEActorService.CallFunction.Errors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIECallFunctionErrorsTest::RunTest(const FString&)
{
	UVibeUEPIECallTestTarget* Target = NewObject<UVibeUEPIECallTestTarget>(GetTransientPackage());
	if (!TestNotNull(TEXT("target created"), Target))
	{
		return false;
	}

	TestEqual(TEXT("unknown function"),
		ErrorCodeOf(CallOn(Target, TEXT("NoSuchFunction_VibeUE"), TEXT("{}"))), FString(TEXT("FUNCTION_NOT_FOUND")));
	TestEqual(TEXT("malformed JSON"),
		ErrorCodeOf(CallOn(Target, TEXT("Add"), TEXT("{\"A\": 1,"))), FString(TEXT("INVALID_ARGS_JSON")));
	TestEqual(TEXT("top-level array is not an args object"),
		ErrorCodeOf(CallOn(Target, TEXT("Add"), TEXT("[1, 2]"))), FString(TEXT("INVALID_ARGS_JSON")));
	TestEqual(TEXT("missing required input"),
		ErrorCodeOf(CallOn(Target, TEXT("SetLabel"), TEXT("{}"))), FString(TEXT("MISSING_ARG")));
	TestEqual(TEXT("missing second input"),
		ErrorCodeOf(CallOn(Target, TEXT("Add"), TEXT("{\"A\": 1}"))), FString(TEXT("MISSING_ARG")));
	TestEqual(TEXT("unknown argument key"),
		ErrorCodeOf(CallOn(Target, TEXT("Add"), TEXT("{\"A\": 1, \"B\": 2, \"C\": 3}"))), FString(TEXT("UNKNOWN_ARG")));
	TestEqual(TEXT("return value is not an input"),
		ErrorCodeOf(CallOn(Target, TEXT("Add"), TEXT("{\"A\": 1, \"B\": 2, \"ReturnValue\": 3}"))), FString(TEXT("UNKNOWN_ARG")));
	TestEqual(TEXT("fraction into an int"),
		ErrorCodeOf(CallOn(Target, TEXT("Add"), TEXT("{\"A\": 1.5, \"B\": 2}"))), FString(TEXT("INVALID_ARG_VALUE")));
	TestEqual(TEXT("JSON array value is unsupported"),
		ErrorCodeOf(CallOn(Target, TEXT("Add"), TEXT("{\"A\": [1], \"B\": 2}"))), FString(TEXT("UNSUPPORTED_ARG_TYPE")));
	TestEqual(TEXT("unresolvable object argument"),
		ErrorCodeOf(CallOn(Target, TEXT("Describe"), TEXT("{\"Other\": \"NoSuchObject_VibeUE\"}"))), FString(TEXT("INVALID_ARG_VALUE")));
	TestEqual(TEXT("null target"),
		ErrorCodeOf(CallOn(nullptr, TEXT("Add"), TEXT("{}"))), FString(TEXT("OBJECT_NOT_FOUND")));

	TestEqual(TEXT("rejected calls never ran"), Target->Label, FString());
	return true;
}

// -----------------------------------------------------------------------------
// The whole point: GAllowActorScriptExecutionInEditor reads FALSE inside the call even when the caller
// (like every Python call, via FEditorScriptExecutionGuard) has it raised, and is restored after.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIECallFunctionGuardLoweredTest, "VibeUE.PIEActorService.CallFunction.GuardLowered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIECallFunctionGuardLoweredTest::RunTest(const FString&)
{
	UVibeUEPIECallTestTarget* Target = NewObject<UVibeUEPIECallTestTarget>(GetTransientPackage());
	if (!TestNotNull(TEXT("target created"), Target))
	{
		return false;
	}

	const bool bOutsideValue = GAllowActorScriptExecutionInEditor;
	{
		TGuardValue<bool> PythonLikeGuard(GAllowActorScriptExecutionInEditor, true);
		const TSharedPtr<FJsonObject> R = CallOn(Target, TEXT("GuardWasLowered"), TEXT("{}"));
		if (!TestTrue(TEXT("reply is JSON"), R.IsValid())) { return false; }
		TestTrue(TEXT("call succeeds"), R->GetBoolField(TEXT("success")));
		TestTrue(TEXT("function saw the guard lowered"), R->GetBoolField(TEXT("return")));
		TestFalse(TEXT("recorded value inside the call was false"), Target->bGuardSeenDuringCall);
		TestTrue(TEXT("caller's raised value is restored after the call"), GAllowActorScriptExecutionInEditor);
	}
	TestEqual(TEXT("global back to its outside value"), GAllowActorScriptExecutionInEditor, bOutsideValue);
	return true;
}

// -----------------------------------------------------------------------------
// The UFUNCTION surface with no PIE running: PIE_NOT_RUNNING (INVALID_SELECTOR checked first).
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIECallFunctionNoPIETest, "VibeUE.PIEActorService.CallFunction.NoPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIECallFunctionNoPIETest::RunTest(const FString&)
{
	TestEqual(TEXT("call_function without PIE"),
		ErrorCodeOf(ParsePIEJson(UPIEActorService::CallFunction(TEXT("server"), TEXT("BP_Test_C_0"), TEXT("Add"), TEXT("{}")))),
		FString(TEXT("PIE_NOT_RUNNING")));
	TestEqual(TEXT("call_function bad selector"),
		ErrorCodeOf(ParsePIEJson(UPIEActorService::CallFunction(TEXT("editor"), TEXT("BP_Test_C_0"), TEXT("Add"), TEXT("{}")))),
		FString(TEXT("INVALID_SELECTOR")));
	TestEqual(TEXT("console_command without PIE"),
		ErrorCodeOf(ParsePIEJson(UPIEActorService::ConsoleCommand(TEXT("client:1"), TEXT("stat fps")))),
		FString(TEXT("PIE_NOT_RUNNING")));
	return true;
}

// -----------------------------------------------------------------------------
// Value checks: integer ranges per property size, numbers-as-strings must be whole numbers, enum
// integers/names must name a real enumerator (never _MAX).
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIECallFunctionValueChecksTest, "VibeUE.PIEActorService.CallFunction.ValueChecks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIECallFunctionValueChecksTest::RunTest(const FString&)
{
	UVibeUEPIECallTestTarget* Target = NewObject<UVibeUEPIECallTestTarget>(GetTransientPackage());
	if (!TestNotNull(TEXT("target created"), Target))
	{
		return false;
	}
	const FString Invalid(TEXT("INVALID_ARG_VALUE"));

	{
		const TSharedPtr<FJsonObject> R = CallOn(Target, TEXT("SmallInts"), TEXT("{\"Small\": 127, \"Medium\": 65535}"));
		if (!TestTrue(TEXT("in-range reply is JSON"), R.IsValid())) { return false; }
		TestTrue(TEXT("int8 127 / uint16 65535 accepted"), R->GetBoolField(TEXT("success")));
		TestEqual(TEXT("sum"), (int32)R->GetNumberField(TEXT("return")), 127 + 65535);
	}
	TestEqual(TEXT("int8 128 refused"),
		ErrorCodeOf(CallOn(Target, TEXT("SmallInts"), TEXT("{\"Small\": 128, \"Medium\": 0}"))), Invalid);
	TestEqual(TEXT("int8 -129 refused"),
		ErrorCodeOf(CallOn(Target, TEXT("SmallInts"), TEXT("{\"Small\": -129, \"Medium\": 0}"))), Invalid);
	TestEqual(TEXT("uint16 -1 refused"),
		ErrorCodeOf(CallOn(Target, TEXT("SmallInts"), TEXT("{\"Small\": 0, \"Medium\": -1}"))), Invalid);
	TestEqual(TEXT("int32 2^31 refused"),
		ErrorCodeOf(CallOn(Target, TEXT("Add"), TEXT("{\"A\": 2147483648, \"B\": 0}"))), Invalid);

	{
		const TSharedPtr<FJsonObject> R = CallOn(Target, TEXT("Add"), TEXT("{\"A\": \" 12 \", \"B\": 1}"));
		if (!TestTrue(TEXT("numeric-string reply is JSON"), R.IsValid())) { return false; }
		TestTrue(TEXT("numeric string accepted"), R->GetBoolField(TEXT("success")));
		TestEqual(TEXT("numeric string parsed"), (int32)R->GetNumberField(TEXT("return")), 13);
	}
	TestEqual(TEXT("trailing junk refused"),
		ErrorCodeOf(CallOn(Target, TEXT("Add"), TEXT("{\"A\": \"12abc\", \"B\": 1}"))), Invalid);
	TestEqual(TEXT("non-number string refused"),
		ErrorCodeOf(CallOn(Target, TEXT("Add"), TEXT("{\"A\": \"abc\", \"B\": 1}"))), Invalid);

	{
		const TSharedPtr<FJsonObject> R = CallOn(Target, TEXT("PickEnum"), TEXT("{\"Choice\": \"Beta\"}"));
		if (!TestTrue(TEXT("enum-name reply is JSON"), R.IsValid())) { return false; }
		TestTrue(TEXT("enum by name accepted"), R->GetBoolField(TEXT("success")));
		TestEqual(TEXT("Beta is 1"), (int32)R->GetNumberField(TEXT("return")), 1);
	}
	{
		const TSharedPtr<FJsonObject> R = CallOn(Target, TEXT("PickEnum"), TEXT("{\"Choice\": 1}"));
		if (!TestTrue(TEXT("enum-int reply is JSON"), R.IsValid())) { return false; }
		TestTrue(TEXT("enum by valid integer accepted"), R->GetBoolField(TEXT("success")));
	}
	TestEqual(TEXT("enum _MAX integer refused"),
		ErrorCodeOf(CallOn(Target, TEXT("PickEnum"), TEXT("{\"Choice\": 2}"))), Invalid);
	TestEqual(TEXT("enum unknown integer refused"),
		ErrorCodeOf(CallOn(Target, TEXT("PickEnum"), TEXT("{\"Choice\": 7}"))), Invalid);
	TestEqual(TEXT("enum _MAX integer as text refused"),
		ErrorCodeOf(CallOn(Target, TEXT("PickEnum"), TEXT("{\"Choice\": \"2\"}"))), Invalid);
	TestEqual(TEXT("enum unknown name refused"),
		ErrorCodeOf(CallOn(Target, TEXT("PickEnum"), TEXT("{\"Choice\": \"Gamma\"}"))), Invalid);
	return true;
}

// -----------------------------------------------------------------------------
// An object argument that names an unloaded MAP is refused without loading it (a map loaded as an
// asset stays resident and makes the next load_level fatal).
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIECallFunctionMapPathRefusedTest, "VibeUE.PIEActorService.CallFunction.MapPathRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIECallFunctionMapPathRefusedTest::RunTest(const FString&)
{
	UVibeUEPIECallTestTarget* Target = NewObject<UVibeUEPIECallTestTarget>(GetTransientPackage());
	if (!TestNotNull(TEXT("target created"), Target))
	{
		return false;
	}

	const TCHAR* MapPackage = TEXT("/Engine/Maps/Entry");
	if (FindPackage(nullptr, MapPackage))
	{
		AddInfo(TEXT("/Engine/Maps/Entry is already loaded in this session; the not-loaded refusal cannot be exercised."));
		return true;
	}

	TestEqual(TEXT("unloaded map object path refused"),
		ErrorCodeOf(CallOn(Target, TEXT("Describe"), TEXT("{\"Other\": \"/Engine/Maps/Entry.Entry\"}"))),
		FString(TEXT("MAP_PATH_REFUSED")));
	TestEqual(TEXT("actor path inside an unloaded map refused"),
		ErrorCodeOf(CallOn(Target, TEXT("Describe"), TEXT("{\"Other\": \"/Engine/Maps/Entry.Entry:PersistentLevel.Foo\"}"))),
		FString(TEXT("MAP_PATH_REFUSED")));
	TestNull(TEXT("the map package was not loaded"), FindPackage(nullptr, MapPackage));
	return true;
}

// -----------------------------------------------------------------------------
// spawn_actor refuses landscape proxies (destroying a GUID-less ALandscape leaves a dangling
// ULandscapeSubsystem entry that crashes its Tick). Class validation only: nothing is spawned.
// -----------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPIEActorLandscapeRefusedTest, "VibeUE.PIEActor.LandscapeClassRefused",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPIEActorLandscapeRefusedTest::RunTest(const FString&)
{
	const FString Unsupported(TEXT("UNSUPPORTED_CLASS"));
	TestEqual(TEXT("ALandscape refused"),
		FPIEActorServiceTestAccess::ClassCode(TEXT("/Script/Landscape.Landscape")), Unsupported);
	TestEqual(TEXT("ALandscapeStreamingProxy refused"),
		FPIEActorServiceTestAccess::ClassCode(TEXT("/Script/Landscape.LandscapeStreamingProxy")), Unsupported);
	TestEqual(TEXT("ordinary actor still accepted"),
		FPIEActorServiceTestAccess::ClassCode(TEXT("/Script/Engine.StaticMeshActor")), FString());

	// The public path checks the PIE world first (selector -> world -> class), so without PIE the reply
	// is PIE_NOT_RUNNING and nothing is spawned or registered.
	const TSharedPtr<FJsonObject> Spawn = ParsePIEJson(
		UPIEActorService::SpawnActor(TEXT("server"), TEXT("/Script/Landscape.Landscape"), FTransform::Identity));
	if (!TestTrue(TEXT("spawn result is JSON"), Spawn.IsValid())) { return false; }
	TestFalse(TEXT("landscape spawn fails"), Spawn->GetBoolField(TEXT("success")));
	TestEqual(TEXT("no PIE reported first"), Spawn->GetStringField(TEXT("error_code")), FString(TEXT("PIE_NOT_RUNNING")));

	const TSharedPtr<FJsonObject> List = ParsePIEJson(UPIEActorService::ListSpawned());
	if (!TestTrue(TEXT("list result is JSON"), List.IsValid())) { return false; }
	TestEqual(TEXT("nothing registered"), (int32)List->GetNumberField(TEXT("count")), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
