// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UWorkflowService.h"

#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputKeyEventArgs.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformOutputDevices.h"
#include "IPythonScriptPlugin.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/EngineVersion.h"
#include "Modules/ModuleManager.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UObject/StrongObjectPtr.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
	struct FWorkflowScenario
	{
		~FWorkflowScenario() { UnbindPIEStarted(); }
		void UnbindPIEStarted()
		{
			if (PIEBeginHandle.IsValid()) { FEditorDelegates::PreBeginPIE.Remove(PIEBeginHandle); PIEBeginHandle.Reset(); }
			if (PIEStartedHandle.IsValid()) { FEditorDelegates::PostPIEStarted.Remove(PIEStartedHandle); PIEStartedHandle.Reset(); }
		}
		FString Id;
		TSharedRef<FJsonObject> Spec = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Steps;
		TArray<TSharedPtr<FJsonValue>> Results;
		int32 StepIndex = 0;
		double StartedSeconds = 0.0;
		double StepStartedSeconds = 0.0;
		double WaitUntil = 0.0;
		int32 LogStartChars = -1;
		// A private settings object tags both the queued request and OriginalRequestParams.
		// Keep it alive: pointer identity must not be recycled while a request can still start.
		TStrongObjectPtr<ULevelEditorPlaySettings> PIERequestSettings;
		TWeakObjectPtr<UWorld> OwnedWorld;
		double OwnedSessionStarted = 0.0;
		FDelegateHandle PIEStartedHandle;
		FDelegateHandle PIEBeginHandle;
		TOptional<uint64> PIEBeginFrame;
		uint64 PIEReadyFrame = 0;
		bool bPIEReady = false;
		bool bSessionBound = false;
		bool bWorldBound = false;
		bool bExclusivePIE = false;
		bool bStopPIE = true;
		bool bPreflighting = true;
		bool bFinishRequested = false;
		// A dispatched request is unresolved until its world is bound or its
		// still-tagged queue is synchronously cancelled and read back.
		bool bPIEHandoffPending = false;
		bool bCancelPendingStart = false;
		bool bEndInvoked = false;
		bool bTerminal = false;
		int32 AssertionsDeclared = 0;
		int32 AssertionsEvaluated = 0;
		bool bSmoke = false;
		FString FinishStatus;
		FString FinishError;
	};

	TMap<FString, TSharedPtr<FWorkflowScenario>> GWorkflowScenarios;
	bool GWorkflowScenarioTicking = false;
#if WITH_DEV_AUTOMATION_TESTS
	FWorkflowScenarioTestState* GScenarioTestState = nullptr;
	TMap<FString, TSharedPtr<FWorkflowScenario>> GSavedScenarioHistory;
#endif

	struct FScenarioPIEState
	{
		bool bEditorAvailable = false;
		TOptional<FRequestPlaySessionParams> Request;
		TOptional<FPlayInEditorSessionInfo> Session;
		TWeakObjectPtr<UWorld> World;
		bool IsBusy() const { return Request.IsSet() || Session.IsSet() || World.IsValid(); }
	};

	FScenarioPIEState ReadPIEState()
	{
		FScenarioPIEState State;
#if WITH_DEV_AUTOMATION_TESTS
		if (GScenarioTestState)
		{
			State.bEditorAvailable = GScenarioTestState->bEditorAvailable;
			State.Request = GScenarioTestState->PlayRequest;
			State.Session = GScenarioTestState->PlaySession;
			State.World = GScenarioTestState->PlayWorld;
			return State;
		}
#endif
		if (GEditor)
		{
			State.bEditorAvailable = true;
			// UE EditorEngine.h: these include startup BEFORE PlayWorld is created.
			State.Request = GEditor->GetPlaySessionRequest();
			State.Session = GEditor->GetPlayInEditorSessionInfo();
			State.World = GEditor->PlayWorld.Get();
		}
		return State;
	}

	uint64 ScenarioFrame()
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GScenarioTestState) { return GScenarioTestState->Frame; }
#endif
		return GFrameCounter;
	}

	bool IsPIEReady(const FWorkflowScenario& Scenario)
	{
		// A modal/reentrant core tick within the startup frame is not safe for EndPlayMap.
		return Scenario.bPIEReady && ScenarioFrame() > Scenario.PIEReadyFrame &&
			(!Scenario.PIEBeginFrame.IsSet() || ScenarioFrame() > Scenario.PIEBeginFrame.GetValue());
	}

	void RecordPIEBegin(const TSharedPtr<FWorkflowScenario>& Scenario)
	{
		// Defer for ANY startup this frame, including the request -> session handoff
		// where neither public optional need be populated yet. This never acquires ownership.
		if (!Scenario->bTerminal) { Scenario->PIEBeginFrame = ScenarioFrame(); }
	}

	bool OwnsQueuedPIE(const FWorkflowScenario& Scenario, const FScenarioPIEState& State)
	{
		return Scenario.PIERequestSettings.IsValid() && State.Request.IsSet() &&
			State.Request->EditorPlaySettings.Get() == Scenario.PIERequestSettings.Get();
	}

	bool OwnsPIESession(const FWorkflowScenario& Scenario, const FScenarioPIEState& State)
	{
		return Scenario.PIERequestSettings.IsValid() && State.Session.IsSet() &&
			State.Session->OriginalRequestParams.EditorPlaySettings.Get() == Scenario.PIERequestSettings.Get() &&
			(!Scenario.bSessionBound || State.Session->PlayRequestStartTime == Scenario.OwnedSessionStarted);
	}

	void BindPIEWorld(FWorkflowScenario& Scenario, const FScenarioPIEState& State)
	{
		if (!OwnsPIESession(Scenario, State)) { return; }
		if (!Scenario.bSessionBound)
		{
			Scenario.OwnedSessionStarted = State.Session->PlayRequestStartTime;
			Scenario.bSessionBound = true;
		}
		if (!Scenario.bWorldBound && State.World.IsValid())
		{
			Scenario.OwnedWorld = State.World;
			Scenario.bWorldBound = true;
			Scenario.bPIEHandoffPending = false;
		}
	}

	bool OwnsPIEWorld(const FWorkflowScenario& Scenario, const FScenarioPIEState& State)
	{
		return Scenario.bWorldBound && Scenario.OwnedWorld.IsValid() &&
			Scenario.OwnedWorld == State.World && OwnsPIESession(Scenario, State);
	}

	bool HasForeignPIE(const FWorkflowScenario& Scenario, const FScenarioPIEState& State)
	{
		return (State.Request.IsSet() && !OwnsQueuedPIE(Scenario, State)) ||
			(State.Session.IsSet() && !OwnsPIESession(Scenario, State)) ||
			(State.World.IsValid() && !OwnsPIEWorld(Scenario, State));
	}

	void RecordPIEStarted(const TSharedPtr<FWorkflowScenario>& Scenario)
	{
		if (Scenario->bTerminal) { return; }
		const FScenarioPIEState State = ReadPIEState();
		BindPIEWorld(*Scenario, State);
		if (OwnsPIEWorld(*Scenario, State) && !Scenario->bPIEReady)
		{
			Scenario->bPIEReady = true; Scenario->PIEReadyFrame = ScenarioFrame();
		}
	}

	void ReleasePIEOwnership(FWorkflowScenario& Scenario)
	{
		Scenario.UnbindPIEStarted();
		Scenario.PIERequestSettings.Reset(); Scenario.OwnedWorld.Reset();
		Scenario.bSessionBound = false; Scenario.bWorldBound = false; Scenario.bEndInvoked = false;
		Scenario.bPIEReady = false;
		Scenario.PIEBeginFrame.Reset();
	}

	enum class EScenarioCleanup { Complete, Pending, Failed };

	EScenarioCleanup CleanupOwnedPIE(FWorkflowScenario& Scenario, bool bStopLiveWorld, FString& OutError)
	{
		// Only called by the non-reentrant core ticker, never from a Python/gameplay callback.
		check(GWorkflowScenarioTicking && IsInGameThread());
		if (Scenario.PIEBeginFrame.IsSet() && ScenarioFrame() <= Scenario.PIEBeginFrame.GetValue()) { return EScenarioCleanup::Pending; }
		FScenarioPIEState State = ReadPIEState();
		BindPIEWorld(Scenario, State);
		if (Scenario.bPIEHandoffPending && !OwnsQueuedPIE(Scenario, State))
		{
			// A consumed request can have a local startup continuation even if
			// no session/world appears for multiple frames.
			return EScenarioCleanup::Pending;
		}
		if (OwnsQueuedPIE(Scenario, State))
		{
#if WITH_DEV_AUTOMATION_TESTS
			if (GScenarioTestState) { ++GScenarioTestState->CancelCalls; GScenarioTestState->PlayRequest.Reset(); }
			else
#endif
			{ GEditor->CancelRequestPlaySession(); }
			State = ReadPIEState(); // cancellation delegates may have queued a different request
			if (OwnsQueuedPIE(Scenario, State)) { return EScenarioCleanup::Pending; }
			BindPIEWorld(Scenario, State);
			if (Scenario.bPIEHandoffPending)
			{
				if (State.Session.IsSet() || State.World.IsValid()) { return EScenarioCleanup::Pending; }
				// We cancelled the still-tagged queue on this game-thread tick,
				// and neither a session nor world was produced by cancellation.
				Scenario.bPIEHandoffPending = false;
			}
		}
		if (OwnsPIESession(Scenario, State))
		{
			if (!Scenario.bWorldBound && !State.World.IsValid()) { return EScenarioCleanup::Pending; }
			if (!IsPIEReady(Scenario)) { return EScenarioCleanup::Pending; }
			// EndPlayMap may also affect the editor's queued request. Do not touch a
			// foreign queue and do not abandon an owned startup: keep cleanup reserved
			// until the foreign request is withdrawn or has replaced our session.
			if (State.Request.IsSet() && !OwnsQueuedPIE(Scenario, State)) { return EScenarioCleanup::Pending; }
			if (!OwnsPIEWorld(Scenario, State))
			{
				OutError = TEXT("PIE ownership changed; refusing teardown of a replacement world or queued session");
				// The tagged session still exists. Refuse its replacement world but
				// retain the reservation until external teardown is actually observed.
				return EScenarioCleanup::Pending;
			}
			// An explicit teardown opt-out transfers the live session to the caller.
			// Retain its settings in terminal history; the engine stores a non-UPROPERTY
			// request struct and must not be left with a collectable settings object.
			if (!bStopLiveWorld) { return EScenarioCleanup::Complete; }
			if (!Scenario.bEndInvoked)
			{
				Scenario.bEndInvoked = true;
				// RequestEndPlayMap is a global next-tick flag (EditorEngine.h), not a
				// world-targeted stop. End synchronously on this safe ticker stack instead.
#if WITH_DEV_AUTOMATION_TESTS
				if (GScenarioTestState)
				{
					++GScenarioTestState->EndCalls;
					if (GScenarioTestState->bCompleteEndPlay) { GScenarioTestState->PlayWorld.Reset(); GScenarioTestState->PlaySession.Reset(); }
				}
				else
#endif
				{ GEditor->EndPlayMap(); }
			}
			State = ReadPIEState();
			if (OwnsPIESession(Scenario, State) || OwnsQueuedPIE(Scenario, State)) { return EScenarioCleanup::Pending; }
		}
		// Session metadata can disappear before the bound current world does.
		// Its continued presence is pending cleanup even when we never called EndPlayMap.
		if (Scenario.OwnedWorld.IsValid() && State.World == Scenario.OwnedWorld) { return EScenarioCleanup::Pending; }
		ReleasePIEOwnership(Scenario);
		return EScenarioCleanup::Complete;
	}

	bool CanMutatePIE(const FWorkflowScenario& Scenario, FString& OutError)
	{
		const FScenarioPIEState State = ReadPIEState();
		if (!Scenario.bFinishRequested && IsPIEReady(Scenario) && OwnsPIEWorld(Scenario, State) && !HasForeignPIE(Scenario, State)) { return true; }
		OutError = TEXT("scenario does not own the current PIE world/session; mutation refused");
		return false;
	}

	bool DispatchPIEMutation(const FWorkflowScenario& Scenario, FString& OutError,
		TFunctionRef<bool(UWorld*, FString&)> Mutation)
	{
		if (!CanMutatePIE(Scenario, OutError)) { return false; }
#if WITH_DEV_AUTOMATION_TESTS
		if (GScenarioTestState)
		{
			++GScenarioTestState->MutationCalls;
			if (GScenarioTestState->OnMutation) { GScenarioTestState->OnMutation(); }
			return true;
		}
#endif
		return Mutation(Scenario.OwnedWorld.Get(), OutError);
	}

	FString ScenarioDir() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VibeUE/Scenarios")); }
	FString ScenarioPath(const FString& Id) { return FPaths::Combine(ScenarioDir(), Id + TEXT(".json")); }
	FString ProjectLogPath() { return FPlatformOutputDevices::GetAbsoluteLogFilename(); }

	FString SerializeScenario(const TSharedRef<FJsonObject>& Object)
	{
		FString Out; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out)); return Out;
	}

	FString ScenarioError(const FString& Message)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("success"), false);
		Root->SetStringField(TEXT("error"), Message); return SerializeScenario(Root);
	}

	bool FingerprintFile(const FString& Path, FString& Hash)
	{
		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path));
		if (!Reader) { return false; }
		FSHA1 Digest;
		uint8 Buffer[65536];
		while (Reader->Tell() < Reader->TotalSize() && !Reader->IsError())
		{
			const int64 Count = FMath::Min<int64>(sizeof(Buffer), Reader->TotalSize() - Reader->Tell());
			Reader->Serialize(Buffer, Count); Digest.Update(Buffer, static_cast<uint32>(Count));
		}
		if (Reader->IsError()) { return false; }
		Digest.Final(); uint8 Bytes[20]; Digest.GetHash(Bytes); Hash = BytesToHex(Bytes, 20); return true;
	}

	bool IsAssertion(const FString& Action)
	{
		return Action == TEXT("assert_log") || Action == TEXT("python_assert") || Action == TEXT("python_assert_number");
	}

	FString HashText(const FString& Text)
	{
		FTCHARToUTF8 Utf8(*Text); uint8 Bytes[20];
		FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Bytes); return BytesToHex(Bytes, 20);
	}

	// Historical outcome is preserved on disk; callers receive a current validity assessment.
	FString CurrentScenarioReport(const TSharedRef<FJsonObject>& Report)
	{
		TSharedPtr<FJsonObject> Copy;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SerializeScenario(Report)), Copy);
		if (Copy->GetStringField(TEXT("status")) == TEXT("running")) { return SerializeScenario(Copy.ToSharedRef()); }
		const TSharedPtr<FJsonObject>* Provenance = nullptr;
		FString Validity = TEXT("untracked");
		if (Copy->TryGetObjectField(TEXT("provenance"), Provenance))
		{
			const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
			if ((*Provenance)->TryGetArrayField(TEXT("files"), Files) && Files->Num() > 0)
			{
				Validity = TEXT("current");
				for (const auto& Value : *Files)
				{
					const auto File = Value->AsObject(); FString Hash;
					if (!File || !FingerprintFile(File->GetStringField(TEXT("path")), Hash) || Hash != File->GetStringField(TEXT("sha1")))
					{ Validity = TEXT("stale"); break; }
				}
				if ((*Provenance)->GetStringField(TEXT("engineVersion")) != FEngineVersion::Current().ToString()) { Validity = TEXT("stale"); }
			}
		}
		Copy->SetStringField(TEXT("validity"), Validity);
		bool Passed = false; Copy->TryGetBoolField(TEXT("passed"), Passed);
		Copy->SetBoolField(TEXT("historicalPassed"), Passed);
		Copy->SetBoolField(TEXT("verifiedCurrent"), Passed && Validity == TEXT("current"));
		if (Passed && Validity == TEXT("stale"))
		{
			Copy->SetBoolField(TEXT("passed"), false); Copy->SetStringField(TEXT("status"), TEXT("stale"));
		}
		return SerializeScenario(Copy.ToSharedRef());
	}

	void SaveScenario(const FWorkflowScenario& Scenario)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GScenarioTestState) { ++GScenarioTestState->ReportWrites; return; }
#endif
		IFileManager::Get().MakeDirectory(*ScenarioDir(), true);
		const FString Path = ScenarioPath(Scenario.Id), Temp = Path + TEXT(".tmp");
		FFileHelper::SaveStringToFile(SerializeScenario(Scenario.Report), *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		IFileManager::Get().Move(*Path, *Temp, true, true, false, true);
	}

	FString ReadScenarioLogDelta(const FWorkflowScenario& Scenario, bool* bReadable = nullptr)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (GScenarioTestState) { if (bReadable) { *bReadable = false; } return FString(); }
#endif
		if (GLog) { GLog->FlushThreadedLogs(); GLog->Flush(); }
		FString Log;
		const bool bOk = FFileHelper::LoadFileToString(Log, *ProjectLogPath(), FFileHelper::EHashOptions::None, FILEREAD_AllowWrite) &&
			Scenario.LogStartChars >= 0 && Scenario.LogStartChars <= Log.Len();
		if (bReadable) { *bReadable = bOk; }
		return bOk ? Log.Mid(Scenario.LogStartChars) : FString();
	}

	void FinalizeScenario(const TSharedPtr<FWorkflowScenario>& Scenario, bool bTeardownOk, const FString& TeardownError)
	{
		Scenario->UnbindPIEStarted();
		Scenario->bTerminal = true;
		const bool bVerified = Scenario->AssertionsDeclared > 0 && Scenario->AssertionsEvaluated == Scenario->AssertionsDeclared;
		const FString FinalStatus = !bTeardownOk ? TEXT("failed") : Scenario->FinishStatus == TEXT("passed") && !bVerified
			? (Scenario->bSmoke ? TEXT("smoke_passed") : TEXT("failed")) : Scenario->FinishStatus;
		Scenario->Report->SetBoolField(TEXT("teardownPending"), false);
		Scenario->Report->SetStringField(TEXT("status"), FinalStatus);
		Scenario->Report->SetBoolField(TEXT("passed"), FinalStatus == TEXT("passed"));
		Scenario->Report->SetNumberField(TEXT("assertionsDeclared"), Scenario->AssertionsDeclared);
		Scenario->Report->SetNumberField(TEXT("assertionsEvaluated"), Scenario->AssertionsEvaluated);
		Scenario->Report->SetArrayField(TEXT("steps"), Scenario->Results);
		Scenario->Report->SetStringField(TEXT("finishedAtIso"), FDateTime::UtcNow().ToIso8601());
		Scenario->Report->SetNumberField(TEXT("durationMs"), (FPlatformTime::Seconds() - Scenario->StartedSeconds) * 1000.0);
		Scenario->Report->SetBoolField(TEXT("teardownSucceeded"), bTeardownOk);
		if (!Scenario->FinishError.IsEmpty()) { Scenario->Report->SetStringField(TEXT("error"), Scenario->FinishError); }
		if (!TeardownError.IsEmpty()) { Scenario->Report->SetStringField(TEXT("teardownError"), TeardownError); }
		Scenario->Report->SetStringField(TEXT("logDelta"), ReadScenarioLogDelta(*Scenario).Right(50000));
		SaveScenario(*Scenario);
#if WITH_DEV_AUTOMATION_TESTS
		if (GScenarioTestState) { return; }
#endif
		UWorkflowService::AttachActiveRunArtifact(ScenarioPath(Scenario->Id), TEXT("pie-scenario"));
	}

	void FinishScenario(const TSharedPtr<FWorkflowScenario>& Scenario, const FString& Status, const FString& Error)
	{
		if (Scenario->bTerminal || Scenario->bFinishRequested) { return; }
		Scenario->bFinishRequested = true;
		Scenario->FinishStatus = Status; Scenario->FinishError = Error;
		Scenario->bCancelPendingStart = Scenario->PIERequestSettings.IsValid() && !IsPIEReady(*Scenario);
		Scenario->Report->SetBoolField(TEXT("teardownPending"), true);
		// A terminal status is a cleanup acknowledgement, not merely a stop request.
		// Never EndPlayMap on a caller's potentially reentrant/gameplay/Python stack.
		if (!Scenario->PIERequestSettings.IsValid()) { FinalizeScenario(Scenario, true, FString()); }
		else { SaveScenario(*Scenario); }
	}

	void TickScenarioFinish(const TSharedPtr<FWorkflowScenario>& Scenario)
	{
		FString Error;
		const EScenarioCleanup Result = CleanupOwnedPIE(*Scenario, Scenario->bStopPIE || Scenario->bCancelPendingStart, Error);
		if (Result != EScenarioCleanup::Pending) { FinalizeScenario(Scenario, Result == EScenarioCleanup::Complete, Error); }
	}

	void AddScenarioStepResult(const TSharedPtr<FWorkflowScenario>& Scenario, const FString& Action, bool bPassed,
		const FString& Error = FString(), const FString& Actual = FString(), const FString& Expected = FString())
	{
		if (Scenario->bFinishRequested) { return; }
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("index"), Scenario->StepIndex); Result->SetStringField(TEXT("action"), Action);
		if (IsAssertion(Action)) { ++Scenario->AssertionsEvaluated; }
		Result->SetStringField(TEXT("status"), bPassed ? TEXT("passed") : TEXT("failed"));
		Result->SetNumberField(TEXT("durationMs"), (FPlatformTime::Seconds() - Scenario->StepStartedSeconds) * 1000.0);
		if (!Error.IsEmpty()) { Result->SetStringField(TEXT("error"), Error); }
		if (!Actual.IsEmpty()) { Result->SetStringField(TEXT("actual"), Actual); }
		if (!Expected.IsEmpty()) { Result->SetStringField(TEXT("expected"), Expected); }
		Scenario->Results.Add(MakeShared<FJsonValueObject>(Result)); Scenario->Report->SetArrayField(TEXT("steps"), Scenario->Results);
		SaveScenario(*Scenario);
		if (!bPassed) { FinishScenario(Scenario, TEXT("failed"), Error); }
		else { ++Scenario->StepIndex; Scenario->StepStartedSeconds = 0.0; Scenario->WaitUntil = 0.0; }
	}

	bool TickWorkflowScenario(float)
	{
		if (!IsInGameThread() || GWorkflowScenarioTicking) { return true; }
		TGuardValue<bool> TickGuard(GWorkflowScenarioTicking, true);
		TArray<FString> Ids; GWorkflowScenarios.GetKeys(Ids);
		for (const FString& Id : Ids)
		{
			const TSharedPtr<FWorkflowScenario> Scenario = GWorkflowScenarios.FindChecked(Id);
			if (Scenario->bTerminal || Scenario->bPreflighting) { continue; }
			if (Scenario->bFinishRequested) { TickScenarioFinish(Scenario); continue; }
			const FScenarioPIEState State = ReadPIEState();
			BindPIEWorld(*Scenario, State);
			if ((Scenario->bExclusivePIE && (!State.bEditorAvailable || HasForeignPIE(*Scenario, State))) ||
				(Scenario->PIERequestSettings.IsValid() &&
					(HasForeignPIE(*Scenario, State) ||
						(!OwnsQueuedPIE(*Scenario, State) && !OwnsPIESession(*Scenario, State) && !Scenario->bPIEHandoffPending) ||
					(Scenario->bWorldBound && !OwnsPIEWorld(*Scenario, State)))))
			{
				FinishScenario(Scenario, TEXT("failed"), TEXT("PIE ownership changed or foreign active/queued PIE appeared"));
				continue;
			}
			if (Scenario->StepIndex >= Scenario->Steps.Num()) { FinishScenario(Scenario, TEXT("passed"), FString()); continue; }
			const TSharedPtr<FJsonObject> Step = Scenario->Steps[Scenario->StepIndex]->AsObject();
			if (!Step.IsValid()) { FinishScenario(Scenario, TEXT("failed"), TEXT("step must be a JSON object")); continue; }
			const FString Action = Step->GetStringField(TEXT("action")).ToLower();
			const double Now = FPlatformTime::Seconds();
			if (Scenario->StepStartedSeconds == 0.0) { Scenario->StepStartedSeconds = Now; }

			if (Action == TEXT("start_pie"))
			{
				if (State.World.IsValid() && (OwnsPIEWorld(*Scenario, State) || !Scenario->bExclusivePIE))
				{
					AddScenarioStepResult(Scenario, Action, true, FString(), TEXT("already running (no ownership acquired)"));
					continue;
				}
				if (!State.bEditorAvailable || State.IsBusy())
				{
					AddScenarioStepResult(Scenario, Action, false, TEXT("PIE start refused: editor unavailable or active/queued session"));
					continue;
				}
				Scenario->PIERequestSettings.Reset(DuplicateObject<ULevelEditorPlaySettings>(
					GetMutableDefault<ULevelEditorPlaySettings>(), GetTransientPackage()));
				// Bound scenarios are single-client, in-process. Never spawn untracked external servers.
				Scenario->PIERequestSettings->SetPlayNetMode(PIE_Standalone);
				Scenario->PIERequestSettings->SetRunUnderOneProcess(true);
				Scenario->PIERequestSettings->SetPlayNumberOfClients(1);
				Scenario->PIERequestSettings->bLaunchSeparateServer = false;
				FRequestPlaySessionParams Request;
				Request.SessionDestination = EPlaySessionDestinationType::InProcess;
				Request.WorldType = EPlaySessionWorldType::PlayInEditor;
				Request.EditorPlaySettings = Scenario->PIERequestSettings.Get();
				Request.bAllowOnlineSubsystem = false;
				// Object duplication/settings hooks can reenter: check again immediately before queue mutation.
				const FScenarioPIEState BeforeRequest = ReadPIEState();
				if (Scenario->bFinishRequested || !BeforeRequest.bEditorAvailable || BeforeRequest.IsBusy())
				{
					AddScenarioStepResult(Scenario, Action, false, TEXT("PIE became busy before start request")); continue;
				}
#if WITH_DEV_AUTOMATION_TESTS
				if (GScenarioTestState)
				{
					++GScenarioTestState->StartCalls;
					Scenario->bPIEHandoffPending = true;
					GScenarioTestState->PlayRequest = Request;
					if (GScenarioTestState->OnRequestQueued) { GScenarioTestState->OnRequestQueued(); }
				}
				else
#endif
				{
					const TWeakPtr<FWorkflowScenario> WeakScenario = Scenario;
					Scenario->PIEBeginHandle = FEditorDelegates::PreBeginPIE.AddLambda([WeakScenario](bool)
					{
						if (const TSharedPtr<FWorkflowScenario> Pinned = WeakScenario.Pin()) { RecordPIEBegin(Pinned); }
					});
					Scenario->PIEStartedHandle = FEditorDelegates::PostPIEStarted.AddLambda([WeakScenario](bool)
					{
						if (const TSharedPtr<FWorkflowScenario> Pinned = WeakScenario.Pin()) { RecordPIEStarted(Pinned); }
					});
					Scenario->bPIEHandoffPending = true;
					GEditor->RequestPlaySession(Request);
				}
				const FScenarioPIEState RequestedState = ReadPIEState();
				BindPIEWorld(*Scenario, RequestedState);
				const bool bAccepted = OwnsQueuedPIE(*Scenario, RequestedState) || OwnsPIESession(*Scenario, RequestedState);
				AddScenarioStepResult(Scenario, Action, bAccepted, bAccepted ? FString() : TEXT("owned PIE request was not retained"));
			}
			else if (Action == TEXT("wait_for_pie"))
			{
				if (State.World.IsValid() &&
					(Scenario->PIERequestSettings.IsValid() ? IsPIEReady(*Scenario) && OwnsPIEWorld(*Scenario, State) : !Scenario->bExclusivePIE))
				{ AddScenarioStepResult(Scenario, Action, true); }
				else
				{
					double Timeout = 30.0; Step->TryGetNumberField(TEXT("timeout_seconds"), Timeout);
					if (Now - Scenario->StepStartedSeconds > Timeout) { AddScenarioStepResult(Scenario, Action, false, TEXT("timed out waiting for PIE readiness")); }
				}
			}
			else if (Action == TEXT("wait"))
			{
				double Seconds = 0.0; Step->TryGetNumberField(TEXT("seconds"), Seconds);
				if (Scenario->WaitUntil == 0.0) { Scenario->WaitUntil = Now + FMath::Clamp(Seconds, 0.0, 300.0); }
				if (Now >= Scenario->WaitUntil) { AddScenarioStepResult(Scenario, Action, true); }
			}
			else if (Action == TEXT("inject_action"))
			{
				double X = 1.0, Y = 0.0, Z = 0.0; Step->TryGetNumberField(TEXT("x"), X); Step->TryGetNumberField(TEXT("y"), Y); Step->TryGetNumberField(TEXT("z"), Z);
				FString Error;
				const bool bOk = DispatchPIEMutation(*Scenario, Error, [&](UWorld*, FString& OutError)
				{
					FString Path = Step->GetStringField(TEXT("path"));
					if (!Path.Contains(TEXT("."))) { Path += TEXT(".") + FPaths::GetBaseFilename(Path); }
					const UInputAction* Input = LoadObject<UInputAction>(nullptr, *Path);
					// Loading can invoke callbacks; never resolve a replacement world after it.
					if (!CanMutatePIE(*Scenario, OutError)) { return false; }
					APlayerController* PC = Scenario->OwnedWorld->GetFirstPlayerController();
					ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
					UEnhancedInputLocalPlayerSubsystem* Subsystem = Player ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player) : nullptr;
					if (!Input || !Subsystem) { OutError = TEXT("input action or owned local player is unavailable"); return false; }
					FInputActionValue Value;
					switch (Input->ValueType)
					{
					case EInputActionValueType::Boolean: Value = FInputActionValue(X != 0.0); break;
					case EInputActionValueType::Axis1D: Value = FInputActionValue(static_cast<float>(X)); break;
					case EInputActionValueType::Axis2D: Value = FInputActionValue(FVector2D(X, Y)); break;
					case EInputActionValueType::Axis3D: Value = FInputActionValue(FVector(X, Y, Z)); break;
					default: OutError = TEXT("unsupported input action value type"); return false;
					}
					Subsystem->InjectInputForAction(Input, Value, TArray<UInputModifier*>(), TArray<UInputTrigger*>());
					return true;
				});
				AddScenarioStepResult(Scenario, Action, bOk, Error);
			}
			else if (Action == TEXT("inject_key"))
			{
				FString Event = TEXT("tap"); Step->TryGetStringField(TEXT("event"), Event); FString Error;
				FString Name = Step->GetStringField(TEXT("key")); Name.RemoveFromStart(TEXT("Keys::")); Name.RemoveFromStart(TEXT("EKeys::"));
				const FKey Key(*Name);
				const bool bDown = Event.Equals(TEXT("down"), ESearchCase::IgnoreCase) || Event.Equals(TEXT("tap"), ESearchCase::IgnoreCase);
				const bool bUp = Event.Equals(TEXT("up"), ESearchCase::IgnoreCase) || Event.Equals(TEXT("tap"), ESearchCase::IgnoreCase);
				auto SendKey = [&](EInputEvent InputEvent)
				{
					return DispatchPIEMutation(*Scenario, Error, [&](UWorld* World, FString& OutError)
					{
						APlayerController* PC = World->GetFirstPlayerController();
						if (!PC) { OutError = TEXT("owned PIE player controller is unavailable"); return false; }
						// Global Slate focus can change during a key-down callback. Target the
						// owned controller and validate again before the key-up half of a tap.
						PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, InputEvent, InputEvent == IE_Released ? 0.0f : 1.0f));
						return true;
					});
				};
				bool bOk = Key.IsValid() && (bDown || bUp);
				if (!bOk) { Error = TEXT("unknown key or event (use tap/down/up)"); }
				if (bOk && bDown) { bOk = SendKey(IE_Pressed); }
				if (bOk && bUp) { bOk = SendKey(IE_Released); }
				AddScenarioStepResult(Scenario, Action, bOk, Error);
			}
			else if (Action == TEXT("assert_log"))
			{
				FString Contains, NotContains; Step->TryGetStringField(TEXT("contains"), Contains); Step->TryGetStringField(TEXT("not_contains"), NotContains);
				bool bReadable = false; const FString Delta = ReadScenarioLogDelta(*Scenario, &bReadable);
				const bool bOk = bReadable && (Contains.IsEmpty() || Delta.Contains(Contains)) && (NotContains.IsEmpty() || !Delta.Contains(NotContains));
				const FString Expected = TEXT("contains: ") + Contains + TEXT("; does not contain: ") + NotContains;
				AddScenarioStepResult(Scenario, Action, bOk, bOk ? FString() : bReadable ? TEXT("log assertion failed") : TEXT("scenario log unavailable or truncated"), Delta.Right(2000), Expected);
			}
			else if (Action == TEXT("python_assert"))
			{
				FString Expression, Expected; Step->TryGetStringField(TEXT("expression"), Expression); Step->TryGetStringField(TEXT("expected"), Expected);
				FPythonCommandEx Command; Command.Command = Expression; Command.ExecutionMode = EPythonCommandExecutionMode::EvaluateStatement;
				IPythonScriptPlugin* Python = IPythonScriptPlugin::Get(); const bool bExecuted = Python && Python->ExecPythonCommandEx(Command);
				const bool bOk = bExecuted && Command.CommandResult == Expected;
				AddScenarioStepResult(Scenario, Action, bOk, bOk ? FString() : TEXT("Python assertion failed"), Command.CommandResult, Expected);
			}
			else if (Action == TEXT("python_assert_number"))
			{
				FPythonCommandEx Command; Command.Command = Step->GetStringField(TEXT("expression"));
				Command.ExecutionMode = EPythonCommandExecutionMode::EvaluateStatement;
				IPythonScriptPlugin* Python = IPythonScriptPlugin::Get();
				const bool bExecuted = Python && Python->ExecPythonCommandEx(Command);
				double Actual = 0.0; const double Expected = Step->GetNumberField(TEXT("expected"));
				double Tolerance = 0.0; Step->TryGetNumberField(TEXT("tolerance"), Tolerance);
				const FString Op = Step->GetStringField(TEXT("operator"));
				// Unreal's JSON reader expects a container at the root. Wrapping also rejects
				// Python strings, booleans, NaN/Inf and multiple values instead of coercing them.
				TArray<TSharedPtr<FJsonValue>> Numbers;
				const bool bNumeric = bExecuted && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Command.CommandResult + TEXT("]")), Numbers) &&
					Numbers.Num() == 1 && Numbers[0]->Type == EJson::Number && Numbers[0]->TryGetNumber(Actual) && FMath::IsFinite(Actual);
				const bool bOk = bNumeric && (Op == TEXT("eq") ? FMath::Abs(Actual - Expected) <= Tolerance :
					Op == TEXT("lt") ? Actual < Expected : Op == TEXT("le") ? Actual <= Expected :
					Op == TEXT("gt") ? Actual > Expected : Actual >= Expected);
				AddScenarioStepResult(Scenario, Action, bOk, bOk ? FString() : TEXT("numeric Python assertion failed"), Command.CommandResult,
					FString::Printf(TEXT("%s %.17g (tolerance %.17g)"), *Op, Expected, Tolerance));
			}
			else if (Action == TEXT("capture_game"))
			{
				FString Name = FString::Printf(TEXT("scenario-%s-step-%d"), *Scenario->Id, Scenario->StepIndex); Step->TryGetStringField(TEXT("name"), Name);
				const FString Path = FPaths::Combine(ScenarioDir(), Scenario->Id + TEXT("-") + Name + TEXT(".png"));
				FScreenshotRequest::RequestScreenshot(Path, true, false, false, FIntRect(), true);
				TArray<TSharedPtr<FJsonValue>> Captures = Scenario->Report->GetArrayField(TEXT("captures")); Captures.Add(MakeShared<FJsonValueString>(Path)); Scenario->Report->SetArrayField(TEXT("captures"), Captures);
				AddScenarioStepResult(Scenario, Action, true, FString(), Path);
			}
			else if (Action == TEXT("console_command"))
			{
				FString Command, Error; Step->TryGetStringField(TEXT("command"), Command);
				const bool bOk = DispatchPIEMutation(*Scenario, Error, [&](UWorld* World, FString& OutError)
				{
					if (!GEngine || Command.IsEmpty()) { OutError = TEXT("no engine or empty command"); return false; }
					GEngine->Exec(World, *Command); return true;
				});
				AddScenarioStepResult(Scenario, Action, bOk, Error);
			}
			else if (Action == TEXT("stop_pie"))
			{
				if (HasForeignPIE(*Scenario, State)) { AddScenarioStepResult(Scenario, Action, false, TEXT("refusing to stop unowned PIE")); continue; }
				FString Error; const EScenarioCleanup Result = CleanupOwnedPIE(*Scenario, true, Error);
				if (Result != EScenarioCleanup::Pending) { AddScenarioStepResult(Scenario, Action, Result == EScenarioCleanup::Complete, Error); }
			}
			else { AddScenarioStepResult(Scenario, Action, false, TEXT("unsupported scenario action: ") + Action); }
		}
		return true;
	}

	bool IsExclusivePIEQualified()
	{
		// Public UE 5.8 state cannot yet prove the first world binding or an atomic
		// cancellation receipt. Keep live use off until those ownership gaps close.
#if WITH_DEV_AUTOMATION_TESTS
		return GScenarioTestState && GScenarioTestState->bExclusivePIEQualified;
#else
		return false;
#endif
	}

	FTSTicker::FDelegateHandle GScenarioTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateStatic(&TickWorkflowScenario), 0.01f);
}

FString UWorkflowService::GetScenarioCapabilities()
{
	// No editor access, scenario reservation, asset load, journal or disk write.
	return IsExclusivePIEQualified()
		? TEXT("{\"success\":true,\"schema\":\"vibeue.scenario_capabilities.v1\",\"exclusive_pie\":true}")
		: TEXT("{\"success\":true,\"schema\":\"vibeue.scenario_capabilities.v1\",\"exclusive_pie\":false}");
}

FString UWorkflowService::RunScenario(const FString& ScenarioJson)
{
	if (!IsInGameThread()) { return ScenarioError(TEXT("RunScenario requires the game thread")); }
	TSharedPtr<FJsonObject> Spec;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ScenarioJson), Spec) || !Spec.IsValid()) { return ScenarioError(TEXT("scenario_json must be an object")); }
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!Spec->TryGetArrayField(TEXT("steps"), Steps) || Steps->Num() == 0) { return ScenarioError(TEXT("scenario requires a non-empty steps array")); }
	int32 AssertionsDeclared = 0;
	for (const auto& Value : *Steps)
	{
		if (!Value || Value->Type != EJson::Object) { return ScenarioError(TEXT("each step must be an object")); }
		const auto Step = Value->AsObject(); FString Action;
		if (!Step || !Step->TryGetStringField(TEXT("action"), Action)) { return ScenarioError(TEXT("each step requires an action")); }
		Action = Action.ToLower();
		if (!IsAssertion(Action)) { continue; }
		++AssertionsDeclared;
		FString Expression, Expected, Contains, NotContains;
		if (Action == TEXT("assert_log"))
		{
			Step->TryGetStringField(TEXT("contains"), Contains); Step->TryGetStringField(TEXT("not_contains"), NotContains);
			if (Contains.IsEmpty() && NotContains.IsEmpty()) { return ScenarioError(TEXT("assert_log requires contains or not_contains")); }
		}
		else
		{
			if (!Step->TryGetStringField(TEXT("expression"), Expression) || Expression.TrimStartAndEnd().IsEmpty()) { return ScenarioError(TEXT("assertion requires expression")); }
			if (Action == TEXT("python_assert"))
			{
				if (!Step->TryGetStringField(TEXT("expected"), Expected)) { return ScenarioError(TEXT("python_assert requires string expected")); }
			}
			else
			{
				double Number, Tolerance = 0.0; FString Op;
				if (!Step->HasTypedField<EJson::Number>(TEXT("expected")) || !Step->TryGetNumberField(TEXT("expected"), Number) || !FMath::IsFinite(Number) ||
					!Step->TryGetStringField(TEXT("operator"), Op) || !(Op == TEXT("eq") || Op == TEXT("lt") || Op == TEXT("le") || Op == TEXT("gt") || Op == TEXT("ge")))
				{ return ScenarioError(TEXT("numeric assertion requires finite expected and operator eq/lt/le/gt/ge")); }
				if (Step->HasField(TEXT("tolerance")) && (!Step->HasTypedField<EJson::Number>(TEXT("tolerance")) || !Step->TryGetNumberField(TEXT("tolerance"), Tolerance) || !FMath::IsFinite(Tolerance) || Tolerance < 0 || Op != TEXT("eq")))
				{ return ScenarioError(TEXT("tolerance must be finite, nonnegative, and only used with eq")); }
			}
		}
	}
	bool bSmoke = false;
	if (Spec->HasField(TEXT("smoke")) && (!Spec->HasTypedField<EJson::Boolean>(TEXT("smoke")) || !Spec->TryGetBoolField(TEXT("smoke"), bSmoke))) { return ScenarioError(TEXT("smoke must be boolean")); }
	if (AssertionsDeclared == 0 && !bSmoke) { return ScenarioError(TEXT("scenario requires an assertion; use smoke:true for boot-only checks")); }
	TArray<TSharedPtr<FJsonValue>> Fingerprints;
	const TArray<TSharedPtr<FJsonValue>>* Dependencies = nullptr;
	if (Spec->HasField(TEXT("dependencies")))
	{
		if (!Spec->TryGetArrayField(TEXT("dependencies"), Dependencies)) { return ScenarioError(TEXT("dependencies must be an array of file paths")); }
		for (const auto& Value : *Dependencies)
		{
			FString Path, Hash;
			if (!Value || Value->Type != EJson::String || !Value->TryGetString(Path) || Path.IsEmpty()) { return ScenarioError(TEXT("dependency must be a file path")); }
			if (FPaths::IsRelative(Path)) { Path = FPaths::Combine(FPaths::ProjectDir(), Path); }
			Path = FPaths::ConvertRelativePathToFull(Path);
			if (!FingerprintFile(Path, Hash)) { return ScenarioError(TEXT("cannot fingerprint dependency: ") + Path); }
			auto File = MakeShared<FJsonObject>(); File->SetStringField(TEXT("path"), Path); File->SetStringField(TEXT("sha1"), Hash);
			Fingerprints.Add(MakeShared<FJsonValueObject>(File));
		}
	}
	bool bExclusive = false;
	if (Spec->HasField(TEXT("exclusive_pie")) &&
		(!Spec->HasTypedField<EJson::Boolean>(TEXT("exclusive_pie")) || !Spec->TryGetBoolField(TEXT("exclusive_pie"), bExclusive)))
	{
		return ScenarioError(TEXT("exclusive_pie must be a boolean"));
	}
	if (bExclusive && !IsExclusivePIEQualified())
	{
		return ScenarioError(TEXT("exclusive PIE disabled pending live ownership qualification"));
	}
	for (const auto& Entry : GWorkflowScenarios)
	{
		if ((!Entry.Value->bTerminal || Entry.Value->bPreflighting) && (bExclusive || Entry.Value->bExclusivePIE))
		{
			return ScenarioError(TEXT("exclusive PIE conflicts with an active scenario"));
		}
	}
	const FScenarioPIEState InitialState = ReadPIEState();
	if (bExclusive && (!InitialState.bEditorAvailable || InitialState.IsBusy()))
	{
		return ScenarioError(TEXT("exclusive PIE requires an idle editor with no active or queued play session"));
	}
	TSharedPtr<FWorkflowScenario> Scenario = MakeShared<FWorkflowScenario>();
	Scenario->AssertionsDeclared = AssertionsDeclared; Scenario->bSmoke = bSmoke;
	auto Provenance = MakeShared<FJsonObject>(); Provenance->SetArrayField(TEXT("files"), Fingerprints);
	Provenance->SetStringField(TEXT("engineVersion"), FEngineVersion::Current().ToString());
	Provenance->SetStringField(TEXT("scenarioSha1"), HashText(ScenarioJson));
	Scenario->Report->SetObjectField(TEXT("provenance"), Provenance);
	Scenario->Id = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")) + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	Scenario->Spec = Spec.ToSharedRef(); Scenario->Steps = *Steps; Scenario->StartedSeconds = FPlatformTime::Seconds();
	Scenario->bExclusivePIE = bExclusive;
#if WITH_DEV_AUTOMATION_TESTS
	if (!GScenarioTestState)
#endif
	{
	if (GLog) { GLog->FlushThreadedLogs(); GLog->Flush(); }
	FString CurrentLog;
	if (FFileHelper::LoadFileToString(CurrentLog, *ProjectLogPath(), FFileHelper::EHashOptions::None, FILEREAD_AllowWrite)) { Scenario->LogStartChars = CurrentLog.Len(); }
	}
	const TSharedPtr<FJsonObject>* Teardown = nullptr; if (Spec->TryGetObjectField(TEXT("teardown"), Teardown)) { (*Teardown)->TryGetBoolField(TEXT("stop_pie"), Scenario->bStopPIE); }
	Scenario->Report->SetStringField(TEXT("schema"), TEXT("vibeue.scenario.v1")); Scenario->Report->SetStringField(TEXT("id"), Scenario->Id);
	FString Name = Scenario->Id; Spec->TryGetStringField(TEXT("name"), Name); Scenario->Report->SetStringField(TEXT("name"), Name);
	Scenario->Report->SetStringField(TEXT("status"), TEXT("running")); Scenario->Report->SetStringField(TEXT("startedAtIso"), FDateTime::UtcNow().ToIso8601());
	Scenario->Report->SetArrayField(TEXT("steps"), {}); Scenario->Report->SetArrayField(TEXT("captures"), {});
	Scenario->Report->SetBoolField(TEXT("exclusive_pie"), bExclusive);
	// Reserve BEFORE any preflight loads/compiles/saves can invoke reentrant callbacks.
	GWorkflowScenarios.Add(Scenario->Id, Scenario);
	auto CanPreflight = [&]()
	{
		const FScenarioPIEState State = ReadPIEState();
		return !Scenario->bFinishRequested && (!bExclusive || (State.bEditorAvailable && !State.IsBusy()));
	};
	TArray<TSharedPtr<FJsonValue>> CompileResults;
	bool bCompileOk = true;
	const TSharedPtr<FJsonObject>* Preflight = nullptr;
	if (Spec->TryGetObjectField(TEXT("preflight"), Preflight))
	{
		bool bSave = false; (*Preflight)->TryGetBoolField(TEXT("save_dirty_assets"), bSave);
		const TArray<TSharedPtr<FJsonValue>>* Blueprints = nullptr;
		if ((*Preflight)->TryGetArrayField(TEXT("compile_blueprints"), Blueprints))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Blueprints)
			{
				if (!CanPreflight()) { bCompileOk = false; break; }
				const FString Path = Value->AsString(); FString ObjectPath = Path;
				if (!ObjectPath.Contains(TEXT("."))) { ObjectPath += TEXT(".") + FPaths::GetBaseFilename(Path); }
				TSharedRef<FJsonObject> Compile = MakeShared<FJsonObject>(); Compile->SetStringField(TEXT("path"), Path);
#if WITH_DEV_AUTOMATION_TESTS
				if (GScenarioTestState)
				{
					++GScenarioTestState->PreflightCalls;
					if (GScenarioTestState->OnPreflight) { GScenarioTestState->OnPreflight(Scenario->Id); }
					Compile->SetBoolField(TEXT("success"), GScenarioTestState->bPreflightSuccess);
					bCompileOk &= GScenarioTestState->bPreflightSuccess;
				}
				else
#endif
				{
					UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
					if (!Blueprint) { Compile->SetBoolField(TEXT("success"), false); Compile->SetStringField(TEXT("error"), TEXT("Blueprint not found")); bCompileOk = false; }
					else if (!CanPreflight()) { bCompileOk = false; }
					else
					{
						FKismetEditorUtilities::CompileBlueprint(Blueprint);
						const bool bOk = Blueprint->Status != BS_Error; Compile->SetBoolField(TEXT("success"), bOk); bCompileOk &= bOk;
						if (bSave && bOk && CanPreflight()) { Compile->SetBoolField(TEXT("saved"), UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false)); }
					}
				}
				CompileResults.Add(MakeShared<FJsonValueObject>(Compile));
				if (!CanPreflight()) { bCompileOk = false; break; }
			}
		}
	}
	// Capture after preflight saves, so the baseline describes the files actually tested.
	for (const auto& Value : Fingerprints)
	{
		const auto File = Value->AsObject(); FString Hash;
		if (!FingerprintFile(File->GetStringField(TEXT("path")), Hash)) { bCompileOk = false; }
		else { File->SetStringField(TEXT("sha1"), Hash); }
	}
	if (Fingerprints.Num() > 0)
	{
		const FString ModulePath = FPaths::ConvertRelativePathToFull(FModuleManager::Get().GetModuleFilename(TEXT("VibeUE"))); FString Hash;
		if (!FingerprintFile(ModulePath, Hash)) { bCompileOk = false; }
		else
		{
			auto File = MakeShared<FJsonObject>(); File->SetStringField(TEXT("path"), ModulePath); File->SetStringField(TEXT("sha1"), Hash);
			Fingerprints.Add(MakeShared<FJsonValueObject>(File));
		}
	}
	Provenance->SetArrayField(TEXT("files"), Fingerprints);
	Scenario->Report->SetArrayField(TEXT("compileResults"), CompileResults); SaveScenario(*Scenario);
	Scenario->bPreflighting = false;
	if (Scenario->bFinishRequested) { return SerializeScenario(Scenario->Report); }
	if (!bCompileOk) { FinishScenario(Scenario, TEXT("failed"), TEXT("preflight compile or provenance capture failed")); return SerializeScenario(Scenario->Report); }
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("success"), true); Root->SetStringField(TEXT("scenarioId"), Scenario->Id);
	Root->SetStringField(TEXT("status"), TEXT("running")); Root->SetStringField(TEXT("reportPath"), ScenarioPath(Scenario->Id));
	return SerializeScenario(Root);
}

FString UWorkflowService::GetScenario(const FString& ScenarioId)
{
	if (!IsInGameThread()) { return ScenarioError(TEXT("GetScenario requires the game thread")); }
	if (const TSharedPtr<FWorkflowScenario>* Found = GWorkflowScenarios.Find(ScenarioId)) { return CurrentScenarioReport((*Found)->Report); }
#if WITH_DEV_AUTOMATION_TESTS
	if (GScenarioTestState) { return ScenarioError(TEXT("scenario not found in fixture")); }
#endif
	FString Text; TSharedPtr<FJsonObject> Report;
	if (!FFileHelper::LoadFileToString(Text, *ScenarioPath(ScenarioId)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Report) || !Report)
	{ return ScenarioError(TEXT("scenario not found or invalid")); }
	return CurrentScenarioReport(Report.ToSharedRef());
}

FString UWorkflowService::CancelScenario(const FString& ScenarioId)
{
	if (!IsInGameThread()) { return ScenarioError(TEXT("CancelScenario requires the game thread")); }
	const TSharedPtr<FWorkflowScenario>* Found = GWorkflowScenarios.Find(ScenarioId);
	if (!Found) { return ScenarioError(TEXT("scenario not found")); }
	// Hold a shared pointer across callbacks; TMap entries can move during reentrant RunScenario.
	const TSharedPtr<FWorkflowScenario> Scenario = *Found;
	FinishScenario(Scenario, TEXT("cancelled"), TEXT("cancelled by caller")); return SerializeScenario(Scenario->Report);
}

#if WITH_DEV_AUTOMATION_TESTS
FScopedWorkflowScenarioTest::FScopedWorkflowScenarioTest(FWorkflowScenarioTestState& State)
{
	if (!IsInGameThread() || GScenarioTestState || GWorkflowScenarioTicking) { return; }
	for (const auto& Entry : GWorkflowScenarios) { if (!Entry.Value->bTerminal || Entry.Value->bPreflighting) { return; } }
	GSavedScenarioHistory = MoveTemp(GWorkflowScenarios);
	GWorkflowScenarios.Reset(); GScenarioTestState = &State; bBound = true;
}

FScopedWorkflowScenarioTest::~FScopedWorkflowScenarioTest()
{
	if (!bBound) { return; }
	// Fixture requests/worlds are not editor state. Drop them without touching real PIE.
	GWorkflowScenarios.Reset(); GWorkflowScenarios = MoveTemp(GSavedScenarioHistory); GScenarioTestState = nullptr;
}

void FScopedWorkflowScenarioTest::Tick(bool bAdvanceFrame)
{
	if (bBound)
	{
		if (bAdvanceFrame) { ++GScenarioTestState->Frame; }
		TickWorkflowScenario(0.01f);
	}
}

void FScopedWorkflowScenarioTest::NotifyPIEBegin()
{
	if (!bBound) { return; }
	for (const auto& Entry : GWorkflowScenarios) { RecordPIEBegin(Entry.Value); }
}

void FScopedWorkflowScenarioTest::NotifyPIEStarted()
{
	if (!bBound) { return; }
	for (const auto& Entry : GWorkflowScenarios) { RecordPIEStarted(Entry.Value); }
}
#endif
