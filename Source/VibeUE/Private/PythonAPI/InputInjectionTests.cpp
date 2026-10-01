// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UInputService.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

static const EAutomationTestFlags kInjectTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

namespace VibeInjectTest
{
	static TSharedPtr<FJsonObject> ParseJson(const FString& Text)
	{
		TSharedPtr<FJsonObject> Obj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		FJsonSerializer::Deserialize(Reader, Obj);
		return Obj;
	}

	static FString ErrorCode(const FString& Reply)
	{
		const TSharedPtr<FJsonObject> Obj = ParseJson(Reply);
		FString Code;
		if (Obj.IsValid())
		{
			Obj->TryGetStringField(TEXT("error_code"), Code);
		}
		return Code;
	}

	static bool Succeeded(const FString& Reply)
	{
		const TSharedPtr<FJsonObject> Obj = ParseJson(Reply);
		bool bSuccess = false;
		return Obj.IsValid() && Obj->TryGetBoolField(TEXT("success"), bSuccess) && bSuccess;
	}

	static UEnhancedInputLocalPlayerSubsystem* FirstPieSubsystem()
	{
		UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
		UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		ULocalPlayer* LocalPlayer = GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
		return LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
	}

	static int32 ThrottleDelegates()
	{
		return GEditor ? GEditor->ShouldDisableCPUThrottlingDelegates.Num() : 0;
	}
}

// Without PIE: argument checks and the not-running errors, and stop_injection with nothing held.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeInputInjectionGuardsTest,
	"VibeUE.Input.InjectionGuards", kInjectTestFlags)
bool FVibeInputInjectionGuardsTest::RunTest(const FString&)
{
	using namespace VibeInjectTest;
	if (GEditor && GEditor->PlayWorld)
	{
		AddError(TEXT("PIE is running; this test checks the replies without PIE."));
		return false;
	}
	const FString Path = TEXT("/Game/NoSuchFolder/IA_NoSuchAction");

	TestEqual(TEXT("a hold under 0.05 s is refused first"), ErrorCode(UInputService::InjectActionFor(Path, 0.01f)), FString(TEXT("BAD_DURATION")));
	TestEqual(TEXT("a hold over 60 s is refused"), ErrorCode(UInputService::InjectActionFor(Path, 61.0f)), FString(TEXT("BAD_DURATION")));
	TestEqual(TEXT("inject_action_for needs PIE"), ErrorCode(UInputService::InjectActionFor(Path, 1.0f)), FString(TEXT("PIE_NOT_RUNNING")));
	TestEqual(TEXT("inject_action needs PIE"), ErrorCode(UInputService::InjectAction(Path)), FString(TEXT("PIE_NOT_RUNNING")));
	TestEqual(TEXT("inject_key hold needs PIE"), ErrorCode(UInputService::InjectKey(TEXT("SpaceBar"), TEXT("hold"), 0.5f)), FString(TEXT("PIE_NOT_RUNNING")));

	const TSharedPtr<FJsonObject> Stop = ParseJson(UInputService::StopInjection(Path));
	if (TestTrue(TEXT("stop_injection returns JSON"), Stop.IsValid()))
	{
		TestTrue(TEXT("stop_injection with nothing held succeeds"), Stop->GetBoolField(TEXT("success")));
		TestFalse(TEXT("and reports nothing was active"), Stop->GetBoolField(TEXT("was_active")));
	}
	return true;
}

// In PIE: inject_action_for holds the action across frames for its time, then releases it; stop_injection
// releases early, finding the hold by the action and world it names (whatever the path spelling, -1 or the
// instance number); while a hold (or a held key) runs, one extra throttling delegate keeps the editor off its
// background frame rate, and it is gone after the release grace; a second hold of a held key extends it;
// ending PIE drops every hold and the throttling delegate at once.
//
// PIE runs the host project's game code in whatever map is open, and its own Error logs (a game's BeginPlay
// complaints; Proteus' PlaytestSandbox logs five on PIE start) are not what this test is about: every outcome
// here is asserted explicitly, so log errors are not recorded as failures.
class FVibeInputPieTestBase : public FAutomationTestBase
{
public:
	FVibeInputPieTestBase(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}
	virtual bool SuppressLogErrors() override { return true; }
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FVibeInputHoldInPIETest, FVibeInputPieTestBase,
	"VibeUE.Input.HoldInPIE", kInjectTestFlags)
bool FVibeInputHoldInPIETest::RunTest(const FString&)
{
	using namespace VibeInjectTest;
	if (!GEditor || GEditor->PlayWorld)
	{
		AddError(TEXT("Needs the editor with no PIE session running."));
		return false;
	}

	struct FState
	{
		TStrongObjectPtr<UInputAction> Action;
		FString Path;
		FString ObjectPath;
		bool bReady = false;
		int32 Baseline = 0;
		double Started = 0.0;
		double Released = 0.0;
		double EndRequested = 0.0;
	};
	const TSharedRef<FState> S = MakeShared<FState>();

	// A step after PIE is ready must find it still running; if it does not, that is an error, not a skip.
	auto LivePieSubsystem = [this, S]() -> UEnhancedInputLocalPlayerSubsystem*
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = FirstPieSubsystem();
		if (!Subsystem || !Subsystem->GetPlayerInput())
		{
			AddError(TEXT("PIE ended or lost its local player during the test."));
			S->bReady = false;
			return nullptr;
		}
		return Subsystem;
	};

	// An in-memory action: LoadObject finds it by path, and nothing reaches the disk. The name is unique so
	// a second run in the same editor never meets the first run's object.
	const FString Name = FString::Printf(TEXT("IA_VibeHoldTest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	S->Path = FString(TEXT("/Temp/VibeUEInputTests/")) + Name;
	S->ObjectPath = S->Path + TEXT(".") + Name;
	UPackage* Package = CreatePackage(*S->Path);
	S->Action.Reset(NewObject<UInputAction>(Package, *Name, RF_Public | RF_Standalone | RF_Transient));
	S->Action->ValueType = EInputActionValueType::Boolean;

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));

	// PIE starts asynchronously: wait for the local player's Enhanced Input subsystem.
	const double WaitStart = FPlatformTime::Seconds();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, S, WaitStart]()
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = FirstPieSubsystem();
		if (Subsystem && Subsystem->GetPlayerInput())
		{
			S->bReady = true;
			return true;
		}
		if (FPlatformTime::Seconds() - WaitStart > 60.0)
		{
			AddError(TEXT("PIE did not give a local player with Enhanced Input within 60 s."));
			return true;
		}
		return false;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.5f));

	// Hold for 1 s.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, S]()
	{
		if (!S->bReady)
		{
			return true;
		}
		S->Baseline = ThrottleDelegates();
		const TSharedPtr<FJsonObject> Reply = ParseJson(UInputService::InjectActionFor(S->Path, 1.0f));
		if (TestTrue(TEXT("inject_action_for returns JSON"), Reply.IsValid()))
		{
			TestTrue(TEXT("inject_action_for succeeded"), Reply->GetBoolField(TEXT("success")));
			TestEqual(TEXT("pie_instance -1 reports the instance it used (the only PIE world, 0)"),
				static_cast<int32>(Reply->GetNumberField(TEXT("pie_instance"))), 0);
		}
		S->Started = FPlatformTime::Seconds();
		TestEqual(TEXT("one throttling delegate while the hold runs"), ThrottleDelegates(), S->Baseline + 1);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.4f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, S, LivePieSubsystem]()
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = S->bReady ? LivePieSubsystem() : nullptr;
		if (!Subsystem)
		{
			return true;
		}
		TestTrue(TEXT("still injected 0.4 s into a 1 s hold"), Subsystem->HasContinuousInputInjectionForAction(S->Action.Get()));
		TestTrue(TEXT("the action's value is held on 0.4 s in"), Subsystem->GetPlayerInput()->GetActionValue(S->Action.Get()).Get<bool>());
		return true;
	}));

	// It releases on its own at about 1 s.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, S, LivePieSubsystem]()
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = S->bReady ? LivePieSubsystem() : nullptr;
		if (!Subsystem)
		{
			return true;
		}
		const double Elapsed = FPlatformTime::Seconds() - S->Started;
		if (!Subsystem->HasContinuousInputInjectionForAction(S->Action.Get()))
		{
			S->Released = Elapsed;
			return true;
		}
		if (Elapsed > 5.0)
		{
			AddError(TEXT("The 1 s hold was still injected after 5 s."));
			return true;
		}
		return false;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.8f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, S, LivePieSubsystem]()
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = S->bReady ? LivePieSubsystem() : nullptr;
		if (!Subsystem)
		{
			return true;
		}
		AddInfo(FString::Printf(TEXT("A 1 s hold was released %.3f s after inject_action_for returned."), S->Released));
		TestTrue(FString::Printf(TEXT("released at about 1 s (at %.3f s)"), S->Released), S->Released >= 0.9 && S->Released < 2.0);
		TestFalse(TEXT("the action's value is released"), Subsystem->GetPlayerInput()->GetActionValue(S->Action.Get()).Get<bool>());
		TestEqual(TEXT("the throttling delegate is gone after the release grace"), ThrottleDelegates(), S->Baseline);

		// A long hold released early.
		TestTrue(TEXT("a 10 s hold starts"), Succeeded(UInputService::InjectActionFor(S->Path, 10.0f)));
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, S, LivePieSubsystem]()
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = S->bReady ? LivePieSubsystem() : nullptr;
		if (!Subsystem)
		{
			return true;
		}
		// The hold was started as (package path, -1); it is the same hold as (object path, instance 0).
		const TSharedPtr<FJsonObject> Stop = ParseJson(UInputService::StopInjection(S->ObjectPath, 0));
		if (TestTrue(TEXT("stop_injection returns JSON"), Stop.IsValid()))
		{
			TestTrue(TEXT("stop_injection by another spelling and instance 0 finds the hold"), Stop->GetBoolField(TEXT("was_active")));
		}
		TestFalse(TEXT("no longer injected after stop_injection"), Subsystem->HasContinuousInputInjectionForAction(S->Action.Get()));
		const TSharedPtr<FJsonObject> Again = ParseJson(UInputService::StopInjection(S->Path));
		TestFalse(TEXT("a second stop_injection finds nothing"), Again.IsValid() && Again->GetBoolField(TEXT("was_active")));

		// A held key keeps the same throttling scope.
		const TSharedPtr<FJsonObject> Key = ParseJson(UInputService::InjectKey(TEXT("SpaceBar"), TEXT("hold"), 0.3f));
		if (TestTrue(TEXT("inject_key hold returns JSON"), Key.IsValid()))
		{
			TestTrue(TEXT("inject_key hold succeeded"), Key->GetBoolField(TEXT("success")));
			TestEqual(TEXT("inject_key reports hold_seconds"), Key->GetNumberField(TEXT("hold_seconds")), 0.3, 0.001);
			TestFalse(TEXT("a first hold is not an extension"), Key->GetBoolField(TEXT("extended")));
		}
		// Holding it again extends the one hold: no second key-down, one pending release.
		const TSharedPtr<FJsonObject> KeyAgain = ParseJson(UInputService::InjectKey(TEXT("SpaceBar"), TEXT("hold"), 0.3f));
		if (TestTrue(TEXT("a second inject_key hold returns JSON"), KeyAgain.IsValid()))
		{
			TestTrue(TEXT("a second hold of a held key extends it"), KeyAgain->GetBoolField(TEXT("extended")));
			TestFalse(TEXT("...without a second key-down"), KeyAgain->GetBoolField(TEXT("handled_down")));
		}
		TestEqual(TEXT("one throttling delegate while the key is held"), ThrottleDelegates(), S->Baseline + 1);
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, S, LivePieSubsystem]()
	{
		if (!S->bReady)
		{
			return true;
		}
		TestEqual(TEXT("the throttling delegate is gone after the key's release"), ThrottleDelegates(), S->Baseline);

		// Long holds left running when PIE ends.
		if (LivePieSubsystem())
		{
			TestTrue(TEXT("a 10 s action hold starts before PIE ends"), Succeeded(UInputService::InjectActionFor(S->Path, 10.0f)));
			TestTrue(TEXT("a 10 s key hold starts before PIE ends"), Succeeded(UInputService::InjectKey(TEXT("SpaceBar"), TEXT("hold"), 10.0f)));
			TestEqual(TEXT("one throttling delegate for both"), ThrottleDelegates(), S->Baseline + 1);
		}
		return true;
	}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	// Ending PIE drops the holds and the throttling delegate at once, with no release grace.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, S]()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			if (S->EndRequested == 0.0)
			{
				S->EndRequested = FPlatformTime::Seconds();
			}
			if (FPlatformTime::Seconds() - S->EndRequested > 30.0)
			{
				AddError(TEXT("PIE did not end within 30 s."));
				return true;
			}
			return false;
		}
		if (S->bReady)
		{
			TestEqual(TEXT("no throttling delegate once PIE has ended"), ThrottleDelegates(), S->Baseline);
			// Instance 0 explicitly: with no PIE world, -1 resolves to nothing and could never match a leftover hold.
			const TSharedPtr<FJsonObject> Stop = ParseJson(UInputService::StopInjection(S->Path, 0));
			TestFalse(TEXT("a hold from the ended session is not reported active"), Stop.IsValid() && Stop->GetBoolField(TEXT("was_active")));
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([S]()
	{
		if (UInputAction* Action = S->Action.Get())
		{
			Action->ClearFlags(RF_Public | RF_Standalone);
		}
		S->Action.Reset();
		return true;
	}));
	return true;
}

#endif // WITH_AUTOMATION_TESTS
