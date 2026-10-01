// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UInputService.h"
#include "AIServiceTestFixture.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static const EAutomationTestFlags kInputTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

static const TCHAR* kInputTestDir = TEXT("/Game/Developers/VibeUEInputTests");

// The fixture reset (in-memory and on-disk) is shared with the AI suites; see AIServiceTestFixture.h.
using VibeAITest::FScopedFixtureReset;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeInputTriggerPropertiesTest,
	"VibeUE.Input.TriggerProperties", kInputTestFlags)
bool FVibeInputTriggerPropertiesTest::RunTest(const FString&)
{
	const FString Dir = kInputTestDir;
	const FString ActionPath = Dir / TEXT("IA_VibeTriggerTest");
	const FString ContextPath = Dir / TEXT("IMC_VibeTriggerTest");
	FScopedFixtureReset ResetAction(ActionPath);
	FScopedFixtureReset ResetContext(ContextPath);

	if (!TestTrue(TEXT("action created"), UInputService::CreateAction(TEXT("IA_VibeTriggerTest"), Dir, TEXT("Boolean")).bSuccess) ||
		!TestTrue(TEXT("context created"), UInputService::CreateMappingContext(TEXT("IMC_VibeTriggerTest"), Dir).bSuccess) ||
		!TestTrue(TEXT("key mapped"), UInputService::AddKeyMapping(ContextPath, ActionPath, TEXT("SpaceBar"))))
	{
		return false;
	}

	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, *(ContextPath + TEXT(".IMC_VibeTriggerTest")));
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *(ActionPath + TEXT(".IA_VibeTriggerTest")));
	if (!TestNotNull(TEXT("context loads"), Context) || !TestNotNull(TEXT("action loads"), Action) ||
		!TestEqual(TEXT("one mapping"), Context->GetMappings().Num(), 1))
	{
		return false;
	}

	// Settings by C++ name, including a bool's b prefix.
	TestTrue(TEXT("hold added"), UInputService::AddTrigger(ContextPath, 0, TEXT("Hold"),
		TEXT("{\"HoldTimeThreshold\": 0.75, \"bIsOneShot\": true}")));
	{
		const TArray<TObjectPtr<UInputTrigger>>& Triggers = Context->GetMappings()[0].Triggers;
		const UInputTriggerHold* Hold = Triggers.Num() == 1 ? Cast<UInputTriggerHold>(Triggers[0]) : nullptr;
		if (TestNotNull(TEXT("the mapping holds one Hold trigger"), Hold))
		{
			TestEqual(TEXT("HoldTimeThreshold set"), Hold->HoldTimeThreshold, 0.75f);
			TestTrue(TEXT("bIsOneShot set"), Hold->bIsOneShot);
		}
	}

	// Settings by snake_case name.
	TestTrue(TEXT("tap added"), UInputService::AddTrigger(ContextPath, 0, TEXT("Tap"),
		TEXT("{\"tap_release_time_threshold\": 0.125}")));
	{
		const TArray<TObjectPtr<UInputTrigger>>& Triggers = Context->GetMappings()[0].Triggers;
		const UInputTriggerTap* Tap = Triggers.Num() == 2 ? Cast<UInputTriggerTap>(Triggers[1]) : nullptr;
		if (TestNotNull(TEXT("the mapping's second trigger is a Tap"), Tap))
		{
			TestEqual(TEXT("TapReleaseTimeThreshold set"), Tap->TapReleaseTimeThreshold, 0.125f);
		}
	}

	// A bad setting fails the call and adds nothing.
	AddExpectedMessagePlain(TEXT("has no property 'NoSuchSetting'"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, /*Occurrences*/ 1);
	TestFalse(TEXT("unknown property rejected"), UInputService::AddTrigger(ContextPath, 0, TEXT("Hold"),
		TEXT("{\"NoSuchSetting\": 1}")));
	AddExpectedMessagePlain(TEXT("properties are not a JSON object"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, /*Occurrences*/ 1);
	TestFalse(TEXT("malformed JSON rejected"), UInputService::AddTrigger(ContextPath, 0, TEXT("Hold"), TEXT("{oops")));
	TestEqual(TEXT("still two triggers"), Context->GetMappings()[0].Triggers.Num(), 2);

	// No settings still works as before.
	TestTrue(TEXT("pressed added without settings"), UInputService::AddTrigger(ContextPath, 0, TEXT("Pressed")));
	TestEqual(TEXT("three triggers"), Context->GetMappings()[0].Triggers.Num(), 3);

	// The action's own triggers, with OnTriggersChanged broadcast as for an edit in the editor.
	int32 Broadcasts = 0;
	const FDelegateHandle Handle = Action->OnTriggersChanged.AddLambda([&Broadcasts]() { ++Broadcasts; });
	const FString Reply = UInputService::AddActionTrigger(ActionPath, TEXT("Pulse"),
		TEXT("{\"interval\": 0.25, \"trigger_limit\": 3}"));
	Action->OnTriggersChanged.Remove(Handle);

	TSharedPtr<FJsonObject> Json;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Reply);
	if (TestTrue(TEXT("add_action_trigger returns JSON"), FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid()))
	{
		TestTrue(TEXT("add_action_trigger succeeded"), Json->GetBoolField(TEXT("success")));
		TestEqual(TEXT("trigger_index"), static_cast<int32>(Json->GetNumberField(TEXT("trigger_index"))), 0);
	}
	const UInputTriggerPulse* Pulse = Action->Triggers.Num() == 1 ? Cast<UInputTriggerPulse>(Action->Triggers[0]) : nullptr;
	if (TestNotNull(TEXT("the action holds one Pulse trigger"), Pulse))
	{
		TestEqual(TEXT("Interval set"), Pulse->Interval, 0.25f);
		TestEqual(TEXT("TriggerLimit set"), Pulse->TriggerLimit, 3);
	}
	TestEqual(TEXT("OnTriggersChanged broadcast once"), Broadcasts, 1);

	// Failures come back as error codes and leave the action alone.
	const FString BadReply = UInputService::AddActionTrigger(ActionPath, TEXT("Pulse"), TEXT("{\"NoSuchSetting\": 1}"));
	TestTrue(TEXT("bad property reported"), BadReply.Contains(TEXT("BAD_PROPERTIES")));
	const FString BadType = UInputService::AddActionTrigger(ActionPath, TEXT("NotATrigger"));
	TestTrue(TEXT("unknown trigger type reported"), BadType.Contains(TEXT("TRIGGER_TYPE_NOT_FOUND")));
	TestEqual(TEXT("still one action trigger"), Action->Triggers.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeInputCreateFoldersTest, "VibeUE.Input.CreateFolders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeInputCreateFoldersTest::RunTest(const FString&)
{
	const FString Folder = TEXT("/Game/VibeUETests/InputFolders");
	const FString BareFolder = TEXT("VibeUETests/InputFolders");
	const FString UnmountedFolder = TEXT("/Temp/VibeUETests/InputFolders");
	// Where an unmounted folder used to land once /Game/ was put in front of it.
	const FString PrefixedFolder = TEXT("/Game/Temp/VibeUETests/InputFolders");

	const TArray<FString> Created = {
		Folder / TEXT("IA_VibeFolderBare"), Folder / TEXT("IA_VibeFolderFull"), Folder / TEXT("IMC_VibeFolderBare"),
		PrefixedFolder / TEXT("IA_VibeFolderUnmounted"), PrefixedFolder / TEXT("IMC_VibeFolderUnmounted")};
	auto DeleteCreated = [&Created]()
	{
		for (const FString& Path : Created)
		{
			if (UEditorAssetLibrary::DoesAssetExist(Path))
			{
				UEditorAssetLibrary::DeleteAsset(Path);
			}
		}
	};
	DeleteCreated();

	// A bare folder goes under /Game.
	const FInputCreateResult Bare = UInputService::CreateAction(TEXT("IA_VibeFolderBare"), BareFolder, TEXT("Boolean"));
	TestTrue(TEXT("create_action takes a bare folder"), Bare.bSuccess);
	TestEqual(TEXT("a bare folder goes under /Game"), Bare.AssetPath,
		Folder / TEXT("IA_VibeFolderBare.IA_VibeFolderBare"));

	// A path that starts with its mount point is kept as it is.
	const FInputCreateResult Full = UInputService::CreateAction(TEXT("IA_VibeFolderFull"), Folder, TEXT("Boolean"));
	TestTrue(TEXT("create_action takes a full path"), Full.bSuccess);
	TestEqual(TEXT("a full path is kept"), Full.AssetPath, Folder / TEXT("IA_VibeFolderFull.IA_VibeFolderFull"));

	const FInputCreateResult ContextBare = UInputService::CreateMappingContext(TEXT("IMC_VibeFolderBare"), BareFolder, 0);
	TestTrue(TEXT("create_mapping_context takes a bare folder"), ContextBare.bSuccess);
	TestEqual(TEXT("a bare context folder goes under /Game"), ContextBare.AssetPath,
		Folder / TEXT("IMC_VibeFolderBare.IMC_VibeFolderBare"));

	// A folder under no mounted content root is refused with its reason, and nothing is created anywhere.
	const FInputCreateResult Unmounted =
		UInputService::CreateAction(TEXT("IA_VibeFolderUnmounted"), UnmountedFolder, TEXT("Boolean"));
	TestFalse(TEXT("create_action refuses an unmounted folder"), Unmounted.bSuccess);
	TestTrue(TEXT("the refusal names the folder"), Unmounted.ErrorMessage.Contains(UnmountedFolder));
	TestFalse(TEXT("no action was created under /Game"),
		UEditorAssetLibrary::DoesAssetExist(PrefixedFolder / TEXT("IA_VibeFolderUnmounted")));

	const FInputCreateResult ContextUnmounted =
		UInputService::CreateMappingContext(TEXT("IMC_VibeFolderUnmounted"), UnmountedFolder, 0);
	TestFalse(TEXT("create_mapping_context refuses an unmounted folder"), ContextUnmounted.bSuccess);
	TestTrue(TEXT("the context refusal names the folder"), ContextUnmounted.ErrorMessage.Contains(UnmountedFolder));
	TestFalse(TEXT("no context was created under /Game"),
		UEditorAssetLibrary::DoesAssetExist(PrefixedFolder / TEXT("IMC_VibeFolderUnmounted")));

	DeleteCreated();
	return true;
}

#endif // WITH_AUTOMATION_TESTS
