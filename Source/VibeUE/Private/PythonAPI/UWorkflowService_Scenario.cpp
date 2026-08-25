// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UWorkflowService.h"

#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "IPythonScriptPlugin.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PythonAPI/UInputService.h"
#include "PythonAPI/UPerformanceService.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace
{
	struct FWorkflowScenario
	{
		FString Id;
		TSharedRef<FJsonObject> Spec = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Steps;
		TArray<TSharedPtr<FJsonValue>> Results;
		int32 StepIndex = 0;
		double StartedSeconds = 0.0;
		double StepStartedSeconds = 0.0;
		double WaitUntil = 0.0;
		int32 LogStartChars = 0;
		bool bOwnsPIE = false;
		bool bStopPIE = true;
		bool bTerminal = false;
	};

	TMap<FString, TSharedPtr<FWorkflowScenario>> GWorkflowScenarios;

	FString ScenarioDir() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VibeUE/Scenarios")); }
	FString ScenarioPath(const FString& Id) { return FPaths::Combine(ScenarioDir(), Id + TEXT(".json")); }
	FString ProjectLogPath() { return FPaths::Combine(FPaths::ProjectLogDir(), FString(FApp::GetProjectName()) + TEXT(".log")); }

	FString SerializeScenario(const TSharedRef<FJsonObject>& Object)
	{
		FString Out; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out)); return Out;
	}

	FString ScenarioError(const FString& Message)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("success"), false);
		Root->SetStringField(TEXT("error"), Message); return SerializeScenario(Root);
	}

	void SaveScenario(const FWorkflowScenario& Scenario)
	{
		IFileManager::Get().MakeDirectory(*ScenarioDir(), true);
		const FString Path = ScenarioPath(Scenario.Id), Temp = Path + TEXT(".tmp");
		FFileHelper::SaveStringToFile(SerializeScenario(Scenario.Report), *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		IFileManager::Get().Move(*Path, *Temp, true, true, false, true);
	}

	FString ReadScenarioLogDelta(const FWorkflowScenario& Scenario)
	{
		FString Log; FFileHelper::LoadFileToString(Log, *ProjectLogPath());
		return Scenario.LogStartChars <= Log.Len() ? Log.Mid(Scenario.LogStartChars) : Log;
	}

	bool JsonSuccess(const FString& Json, FString& OutError)
	{
		TSharedPtr<FJsonObject> Object;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) || !Object.IsValid())
		{
			OutError = TEXT("operation returned invalid JSON"); return false;
		}
		bool bSuccess = false;
		if (!Object->TryGetBoolField(TEXT("success"), bSuccess)) { bSuccess = !Object->HasField(TEXT("error")); }
		if (!bSuccess)
		{
			if (!Object->TryGetStringField(TEXT("error_message"), OutError)) { Object->TryGetStringField(TEXT("error"), OutError); }
		}
		return bSuccess;
	}

	void FinishScenario(const TSharedPtr<FWorkflowScenario>& Scenario, const FString& Status, const FString& Error)
	{
		if (Scenario->bTerminal) { return; }
		Scenario->bTerminal = true;
		bool bTeardownOk = true;
		if (Scenario->bStopPIE && GEditor && GEditor->PlayWorld)
		{
			FString StopError; bTeardownOk = JsonSuccess(UPerformanceService::StopPIE(), StopError);
		}
		Scenario->Report->SetStringField(TEXT("status"), bTeardownOk ? Status : TEXT("failed"));
		Scenario->Report->SetBoolField(TEXT("passed"), Status == TEXT("passed") && bTeardownOk);
		Scenario->Report->SetArrayField(TEXT("steps"), Scenario->Results);
		Scenario->Report->SetStringField(TEXT("finishedAtIso"), FDateTime::UtcNow().ToIso8601());
		Scenario->Report->SetNumberField(TEXT("durationMs"), (FPlatformTime::Seconds() - Scenario->StartedSeconds) * 1000.0);
		Scenario->Report->SetBoolField(TEXT("teardownSucceeded"), bTeardownOk);
		if (!Error.IsEmpty()) { Scenario->Report->SetStringField(TEXT("error"), Error); }
		Scenario->Report->SetStringField(TEXT("logDelta"), ReadScenarioLogDelta(*Scenario).Right(50000));
		SaveScenario(*Scenario);
		UWorkflowService::AttachActiveRunArtifact(ScenarioPath(Scenario->Id), TEXT("pie-scenario"));
	}

	void AddScenarioStepResult(const TSharedPtr<FWorkflowScenario>& Scenario, const FString& Action, bool bPassed,
		const FString& Error = FString(), const FString& Actual = FString(), const FString& Expected = FString())
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("index"), Scenario->StepIndex); Result->SetStringField(TEXT("action"), Action);
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
		TArray<FString> Ids; GWorkflowScenarios.GetKeys(Ids);
		for (const FString& Id : Ids)
		{
			const TSharedPtr<FWorkflowScenario> Scenario = GWorkflowScenarios.FindChecked(Id);
			if (Scenario->bTerminal) { continue; }
			if (Scenario->StepIndex >= Scenario->Steps.Num()) { FinishScenario(Scenario, TEXT("passed"), FString()); continue; }
			const TSharedPtr<FJsonObject> Step = Scenario->Steps[Scenario->StepIndex]->AsObject();
			if (!Step.IsValid()) { FinishScenario(Scenario, TEXT("failed"), TEXT("step must be a JSON object")); continue; }
			const FString Action = Step->GetStringField(TEXT("action")).ToLower();
			const double Now = FPlatformTime::Seconds();
			if (Scenario->StepStartedSeconds == 0.0) { Scenario->StepStartedSeconds = Now; }

			if (Action == TEXT("start_pie"))
			{
				if (GEditor && GEditor->PlayWorld) { AddScenarioStepResult(Scenario, Action, true, FString(), TEXT("already running")); }
				else { FString Error; const bool bOk = JsonSuccess(UPerformanceService::StartPIE(), Error); Scenario->bOwnsPIE = bOk; AddScenarioStepResult(Scenario, Action, bOk, Error); }
			}
			else if (Action == TEXT("wait_for_pie"))
			{
				if (GEditor && GEditor->PlayWorld) { AddScenarioStepResult(Scenario, Action, true); }
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
				FString Error; const bool bOk = JsonSuccess(UInputService::InjectAction(Step->GetStringField(TEXT("path")), X, Y, Z), Error);
				AddScenarioStepResult(Scenario, Action, bOk, Error);
			}
			else if (Action == TEXT("inject_key"))
			{
				FString Event = TEXT("tap"); Step->TryGetStringField(TEXT("event"), Event); FString Error;
				const bool bOk = JsonSuccess(UInputService::InjectKey(Step->GetStringField(TEXT("key")), Event), Error);
				AddScenarioStepResult(Scenario, Action, bOk, Error);
			}
			else if (Action == TEXT("assert_log"))
			{
				FString Contains, NotContains; Step->TryGetStringField(TEXT("contains"), Contains); Step->TryGetStringField(TEXT("not_contains"), NotContains);
				const FString Delta = ReadScenarioLogDelta(*Scenario);
				const bool bOk = (!Contains.IsEmpty() && Delta.Contains(Contains)) || (!NotContains.IsEmpty() && !Delta.Contains(NotContains));
				const FString Expected = !Contains.IsEmpty() ? TEXT("contains: ") + Contains : TEXT("does not contain: ") + NotContains;
				AddScenarioStepResult(Scenario, Action, bOk, bOk ? FString() : TEXT("log assertion failed"), Delta.Right(2000), Expected);
			}
			else if (Action == TEXT("python_assert"))
			{
				FString Expression, Expected; Step->TryGetStringField(TEXT("expression"), Expression); Step->TryGetStringField(TEXT("expected"), Expected);
				FPythonCommandEx Command; Command.Command = Expression; Command.ExecutionMode = EPythonCommandExecutionMode::EvaluateStatement;
				IPythonScriptPlugin* Python = IPythonScriptPlugin::Get(); const bool bExecuted = Python && Python->ExecPythonCommandEx(Command);
				const bool bOk = bExecuted && Command.CommandResult == Expected;
				AddScenarioStepResult(Scenario, Action, bOk, bOk ? FString() : TEXT("Python assertion failed"), Command.CommandResult, Expected);
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
				FString Command; Step->TryGetStringField(TEXT("command"), Command); UWorld* World = GEditor && GEditor->PlayWorld ? GEditor->PlayWorld.Get() : (GEditor ? GEditor->GetEditorWorldContext().World() : nullptr);
				const bool bOk = GEngine && World && !Command.IsEmpty(); if (bOk) { GEngine->Exec(World, *Command); }
				AddScenarioStepResult(Scenario, Action, bOk, bOk ? FString() : TEXT("no world or empty command"));
			}
			else if (Action == TEXT("stop_pie"))
			{
				if (!GEditor || !GEditor->PlayWorld) { AddScenarioStepResult(Scenario, Action, true); }
				else { FString Error; AddScenarioStepResult(Scenario, Action, JsonSuccess(UPerformanceService::StopPIE(), Error), Error); }
			}
			else { AddScenarioStepResult(Scenario, Action, false, TEXT("unsupported scenario action: ") + Action); }
		}
		return true;
	}

	FTSTicker::FDelegateHandle GScenarioTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateStatic(&TickWorkflowScenario), 0.01f);
}

FString UWorkflowService::RunScenario(const FString& ScenarioJson)
{
	TSharedPtr<FJsonObject> Spec;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ScenarioJson), Spec) || !Spec.IsValid()) { return ScenarioError(TEXT("scenario_json must be an object")); }
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!Spec->TryGetArrayField(TEXT("steps"), Steps) || Steps->Num() == 0) { return ScenarioError(TEXT("scenario requires a non-empty steps array")); }
	TSharedPtr<FWorkflowScenario> Scenario = MakeShared<FWorkflowScenario>();
	Scenario->Id = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")) + TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	Scenario->Spec = Spec.ToSharedRef(); Scenario->Steps = *Steps; Scenario->StartedSeconds = FPlatformTime::Seconds();
	FString CurrentLog; FFileHelper::LoadFileToString(CurrentLog, *ProjectLogPath()); Scenario->LogStartChars = CurrentLog.Len();
	const TSharedPtr<FJsonObject>* Teardown = nullptr; if (Spec->TryGetObjectField(TEXT("teardown"), Teardown)) { (*Teardown)->TryGetBoolField(TEXT("stop_pie"), Scenario->bStopPIE); }
	Scenario->Report->SetStringField(TEXT("schema"), TEXT("vibeue.scenario.v1")); Scenario->Report->SetStringField(TEXT("id"), Scenario->Id);
	FString Name = Scenario->Id; Spec->TryGetStringField(TEXT("name"), Name); Scenario->Report->SetStringField(TEXT("name"), Name);
	Scenario->Report->SetStringField(TEXT("status"), TEXT("running")); Scenario->Report->SetStringField(TEXT("startedAtIso"), FDateTime::UtcNow().ToIso8601());
	Scenario->Report->SetArrayField(TEXT("steps"), {}); Scenario->Report->SetArrayField(TEXT("captures"), {});
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
				const FString Path = Value->AsString(); FString ObjectPath = Path;
				if (!ObjectPath.Contains(TEXT("."))) { ObjectPath += TEXT(".") + FPaths::GetBaseFilename(Path); }
				UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
				TSharedRef<FJsonObject> Compile = MakeShared<FJsonObject>(); Compile->SetStringField(TEXT("path"), Path);
				if (!Blueprint) { Compile->SetBoolField(TEXT("success"), false); Compile->SetStringField(TEXT("error"), TEXT("Blueprint not found")); bCompileOk = false; }
				else
				{
					FKismetEditorUtilities::CompileBlueprint(Blueprint);
					const bool bOk = Blueprint->Status != BS_Error; Compile->SetBoolField(TEXT("success"), bOk); bCompileOk &= bOk;
					if (bSave && bOk) { Compile->SetBoolField(TEXT("saved"), UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false)); }
				}
				CompileResults.Add(MakeShared<FJsonValueObject>(Compile));
			}
		}
	}
	Scenario->Report->SetArrayField(TEXT("compileResults"), CompileResults); SaveScenario(*Scenario);
	GWorkflowScenarios.Add(Scenario->Id, Scenario);
	if (!bCompileOk) { FinishScenario(Scenario, TEXT("failed"), TEXT("Blueprint preflight compile failed")); return SerializeScenario(Scenario->Report); }
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("success"), true); Root->SetStringField(TEXT("scenarioId"), Scenario->Id);
	Root->SetStringField(TEXT("status"), TEXT("running")); Root->SetStringField(TEXT("reportPath"), ScenarioPath(Scenario->Id));
	return SerializeScenario(Root);
}

FString UWorkflowService::GetScenario(const FString& ScenarioId)
{
	if (const TSharedPtr<FWorkflowScenario>* Found = GWorkflowScenarios.Find(ScenarioId)) { return SerializeScenario((*Found)->Report); }
	FString Text; return FFileHelper::LoadFileToString(Text, *ScenarioPath(ScenarioId)) ? Text : ScenarioError(TEXT("scenario not found"));
}

FString UWorkflowService::CancelScenario(const FString& ScenarioId)
{
	const TSharedPtr<FWorkflowScenario>* Found = GWorkflowScenarios.Find(ScenarioId);
	if (!Found) { return ScenarioError(TEXT("scenario not found")); }
	FinishScenario(*Found, TEXT("cancelled"), TEXT("cancelled by caller")); return SerializeScenario((*Found)->Report);
}
