// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UInputService.h"
#include "Application/ThrottleManager.h"
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
// What reads the PIE player input waits for PIE frames as well as seconds. An injection reaches the player input
// only when a PIE world ticks, and UEditorEngine::Tick ticks no PIE world while a Slate responsive-mode request is
// held: in the full suite an earlier test left one held, no frame ran, and "the value is held" failed while the same
// test passed alone (2026-10-03). Seconds only prove that the wall clock moved.
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
		double WorldClock = -1.0;
		int32 Ticks = 0;
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

	// A PIE world ticks at most once per engine frame and a latent command runs once per engine frame, so each change
	// of the world's real-time clock between two runs is one ticked frame. Real time, because a host game that pauses
	// stops GetTimeSeconds while its player controllers still tick and process input.
	auto CountPieTicks = [S]()
	{
		const UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
		if (World && World->GetRealTimeSeconds() != S->WorldClock)
		{
			S->WorldClock = World->GetRealTimeSeconds();
			++S->Ticks;
		}
	};

	// A step that waits for Seconds of wall-clock time and for PIE to tick MinTicks frames, both counted from when the
	// step starts. Without them by MaxSeconds it is an error that names the usual cause, and the steps after it are
	// skipped: they read the player input, which needs the frames.
	auto WaitForPie = [this, S, CountPieTicks](const double Seconds, const int32 MinTicks, const double MaxSeconds, const TCHAR* ToDo)
	{
		struct FWait
		{
			double Started = 0.0;
			int32 TicksAtStart = 0;
		};
		const TSharedRef<FWait> Wait = MakeShared<FWait>();
		return [this, S, CountPieTicks, Wait, Seconds, MinTicks, MaxSeconds, ToDo]()
		{
			if (!S->bReady)
			{
				return true;
			}
			CountPieTicks();
			const double Now = FPlatformTime::Seconds();
			if (Wait->Started == 0.0)
			{
				Wait->Started = Now;
				Wait->TicksAtStart = S->Ticks;
				return false;
			}
			const double Waited = Now - Wait->Started;
			const int32 Ticked = S->Ticks - Wait->TicksAtStart;
			// The cap comes first: frames that arrive after it are too late for the step that follows to be what it says.
			if (Waited > MaxSeconds)
			{
				AddError(FString::Printf(TEXT("PIE ticked %d frame(s) in %.1f s; %d are needed %s within %.1f s.%s"),
					Ticked, Waited, MinTicks, ToDo, MaxSeconds,
					FSlateThrottleManager::Get().IsAllowingExpensiveTasks() ? TEXT("") :
					TEXT(" A Slate responsive-mode request is held (FSlateThrottleManager::IsAllowingExpensiveTasks() is false)")
					TEXT(" and the editor ticks no PIE world while one is: an earlier test left it.")));
				S->bReady = false;
				return true;
			}
			return Waited >= Seconds && Ticked >= MinTicks;
		};
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
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(WaitForPie(0.5, 3, 60.0, TEXT("to start the hold"))));

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
	// The injection is queued when the hold starts, put into the player input on the next PIE tick and processed on the
	// one after: three ticks leave one to spare. The cap keeps the look inside the hold: a third tick that arrives after
	// it is an error, never a late read.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(WaitForPie(0.4, 3, 0.9, TEXT("to read the held value"))));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, S, LivePieSubsystem]()
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = S->bReady ? LivePieSubsystem() : nullptr;
		if (!Subsystem)
		{
			return true;
		}
		TestTrue(TEXT("still injected 0.4 s or more into a 1 s hold"), Subsystem->HasContinuousInputInjectionForAction(S->Action.Get()));
		TestTrue(TEXT("the action's value is held 0.4 s or more in"), Subsystem->GetPlayerInput()->GetActionValue(S->Action.Get()).Get<bool>());
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
	// The release grace is wall-clock time; the value is only cleared on the PIE tick after the one that still saw the
	// injection, so three ticks again.
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(WaitForPie(0.8, 3, 20.0, TEXT("to read the released value"))));
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
