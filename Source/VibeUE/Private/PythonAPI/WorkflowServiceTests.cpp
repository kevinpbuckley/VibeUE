// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UWorkflowService.h"

#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TSharedPtr<FJsonObject> ParseWorkflowJson(const FString& Text)
	{
		TSharedPtr<FJsonObject> Object;
		return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) ? Object : nullptr;
	}

	FString NewWorkflowAssetPath(const TCHAR* Leaf)
	{
		return FString::Printf(TEXT("/Game/VibeUE_Automation/Workflow/%s_%s"), Leaf,
			*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8));
	}

	UBlueprint* CreateWorkflowBlueprint(const FString& Path, EBlueprintType Type = BPTYPE_Normal)
	{
		const FString Name = FPaths::GetBaseFilename(Path);
		UPackage* Package = CreatePackage(*Path);
		return FKismetEditorUtilities::CreateBlueprint(Type == BPTYPE_Interface ? UInterface::StaticClass() : UObject::StaticClass(),
			Package, *Name, Type, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), TEXT("WorkflowServiceTests"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowEnvironmentTest, "VibeUE.Workflow.Environment.Manifest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowEnvironmentTest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Environment = ParseWorkflowJson(UWorkflowService::GetEnvironment());
	if (!TestTrue(TEXT("manifest is JSON"), Environment.IsValid())) { return false; }
	TestTrue(TEXT("project file is absolute"), !FPaths::IsRelative(Environment->GetStringField(TEXT("projectFile"))));
	TestTrue(TEXT("engine root is absolute"), !FPaths::IsRelative(Environment->GetStringField(TEXT("engineRoot"))));
	TestTrue(TEXT("engine source diagnostic is explicit"), Environment->GetObjectField(TEXT("engineSource"))->HasField(TEXT("available")));
	TestTrue(TEXT("compiler diagnostic is explicit"), Environment->GetObjectField(TEXT("compiler"))->HasField(TEXT("available")));
	TestTrue(TEXT("last build status is explicit"), Environment->GetObjectField(TEXT("lastBuild"))->HasField(TEXT("status")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowJournalTest, "VibeUE.Workflow.Journal.LifecycleAndRedaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowJournalTest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Started = ParseWorkflowJson(UWorkflowService::StartRun(TEXT("Automation run"), TEXT("{\"api_token\":\"secret-value\",\"ticket\":42}")));
	if (!TestTrue(TEXT("run starts"), Started.IsValid() && Started->GetBoolField(TEXT("success")))) { return false; }
	const FString Id = Started->GetStringField(TEXT("runId"));
	TestTrue(TEXT("note accepted"), ParseWorkflowJson(UWorkflowService::AddRunNote(Id, TEXT("compiled successfully"))).IsValid());
	TestTrue(TEXT("artifact accepted"), ParseWorkflowJson(UWorkflowService::AttachRunArtifact(Id, TEXT("Saved/Test.png"), TEXT("capture"))).IsValid());
	const TSharedPtr<FJsonObject> Finished = ParseWorkflowJson(UWorkflowService::FinishRun(Id, TEXT("succeeded"), TEXT("Verified")));
	if (!TestTrue(TEXT("run finishes"), Finished.IsValid())) { return false; }
	TestEqual(TEXT("outcome persisted"), Finished->GetStringField(TEXT("status")), FString(TEXT("succeeded")));
	TestEqual(TEXT("secret redacted"), Finished->GetObjectField(TEXT("metadata"))->GetStringField(TEXT("api_token")), FString(TEXT("[REDACTED]")));
	TestTrue(TEXT("affected-assets array present"), Finished->HasTypedField<EJson::Array>(TEXT("affectedAssets")));
	TestTrue(TEXT("optional GameIQ boundary is explicit"), Finished->HasTypedField<EJson::Object>(TEXT("gameIQ")) &&
		Finished->GetObjectField(TEXT("gameIQ"))->HasField(TEXT("available")));
	TestTrue(TEXT("delete succeeds"), ParseWorkflowJson(UWorkflowService::DeleteRun(Id))->GetBoolField(TEXT("success")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowJournalRecoveryTest, "VibeUE.Workflow.Journal.InterruptedRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowJournalRecoveryTest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Started = ParseWorkflowJson(UWorkflowService::StartRun(TEXT("Interrupted automation run"), TEXT("{}")));
	if (!TestTrue(TEXT("run starts"), Started.IsValid() && Started->GetBoolField(TEXT("success")))) { return false; }
	const FString Id = Started->GetStringField(TEXT("runId"));
	UWorkflowService::ShutdownJournal();
	const TSharedPtr<FJsonObject> Interrupted = ParseWorkflowJson(UWorkflowService::GetRun(Id));
	TestTrue(TEXT("interrupted run remains readable"), Interrupted.IsValid());
	TestEqual(TEXT("shutdown is represented distinctly"), Interrupted->GetStringField(TEXT("status")), FString(TEXT("interrupted")));
	UWorkflowService::InitializeJournal();
	TestTrue(TEXT("interrupted run can be removed"), ParseWorkflowJson(UWorkflowService::DeleteRun(Id))->GetBoolField(TEXT("success")));
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FWaitWorkflowScenario, FAutomationTestBase*, Test, FString, Id, bool, bExpectedPass);
bool FWaitWorkflowScenario::Update()
{
	static TMap<FString, double> Starts;
	double& Start = Starts.FindOrAdd(Id, FPlatformTime::Seconds());
	const TSharedPtr<FJsonObject> Report = ParseWorkflowJson(UWorkflowService::GetScenario(Id));
	if (!Report.IsValid()) { Test->AddError(TEXT("scenario report is invalid")); Starts.Remove(Id); return true; }
	const FString Status = Report->GetStringField(TEXT("status"));
	if (Status == TEXT("running"))
	{
		if (FPlatformTime::Seconds() - Start < 10.0) { return false; }
		Test->AddError(TEXT("scenario timed out")); UWorkflowService::CancelScenario(Id); Starts.Remove(Id); return true;
	}
	Test->TestEqual(TEXT("scenario terminal verdict"), Report->GetBoolField(TEXT("passed")), bExpectedPass);
	Test->TestTrue(TEXT("teardown recorded"), Report->HasField(TEXT("teardownSucceeded")));
	Starts.Remove(Id); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioTest, "VibeUE.Workflow.Scenario.PassingAndFailing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioTest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Passing = ParseWorkflowJson(UWorkflowService::RunScenario(
		TEXT("{\"name\":\"pass\",\"steps\":[{\"action\":\"wait\",\"seconds\":0.01},{\"action\":\"python_assert\",\"expression\":\"1+1\",\"expected\":\"2\"}],\"teardown\":{\"stop_pie\":false}}")));
	const TSharedPtr<FJsonObject> Failing = ParseWorkflowJson(UWorkflowService::RunScenario(
		TEXT("{\"name\":\"fail\",\"steps\":[{\"action\":\"python_assert\",\"expression\":\"1+1\",\"expected\":\"3\"}],\"teardown\":{\"stop_pie\":false}}")));
	if (!TestTrue(TEXT("scenarios queued"), Passing.IsValid() && Failing.IsValid())) { return false; }
	ADD_LATENT_AUTOMATION_COMMAND(FWaitWorkflowScenario(this, Passing->GetStringField(TEXT("scenarioId")), true));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitWorkflowScenario(this, Failing->GetStringField(TEXT("scenarioId")), false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioValidationTest, "VibeUE.Workflow.Scenario.RejectInvalidAssertions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioValidationTest::RunTest(const FString&)
{
	const TArray<FString> Invalid = {
		TEXT(R"({"steps":[{"action":"wait"}]})"),
		TEXT(R"({"steps":[null]})"),
		TEXT(R"({"steps":[{"action":"assert_log"}]})"),
		TEXT(R"({"steps":[{"action":"python_assert","expression":"1"}]})"),
		TEXT(R"({"steps":[{"action":"python_assert_number","expression":"1","operator":"bogus","expected":1}]})"),
		TEXT(R"({"steps":[{"action":"python_assert_number","expression":"1","operator":"eq","expected":1,"tolerance":-1}]})"),
		TEXT(R"({"steps":[{"action":"python_assert_number","expression":"1","operator":"lt","expected":2,"tolerance":1}]})"),
		TEXT(R"({"smoke":"yes","steps":[{"action":"wait"}]})"),
		TEXT(R"({"smoke":true,"steps":[{"action":"wait"}],"dependencies":["Saved/NoSuchScenarioDependency"]})")
	};
	for (const FString& Spec : Invalid)
	{
		const auto Result = ParseWorkflowJson(UWorkflowService::RunScenario(Spec));
		TestTrue(TEXT("invalid scenario rejected before execution: ") + Spec, Result && !Result->GetBoolField(TEXT("success")) && !Result->HasField(TEXT("scenarioId")));
	}
	return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_FOUR_PARAMETER(FVerifyScenarioEvidence, FAutomationTestBase*, Test, FString, Id, FString, ExpectedStatus, FString, Dependency);
bool FVerifyScenarioEvidence::Update()
{
	static TMap<FString, double> Starts;
	const double Start = Starts.FindOrAdd(Id, FPlatformTime::Seconds());
	const auto Report = ParseWorkflowJson(UWorkflowService::GetScenario(Id));
	if (!Report) { Test->AddError(TEXT("invalid evidence JSON")); Starts.Remove(Id); return true; }
	if (Report->GetStringField(TEXT("status")) == TEXT("running"))
	{
		if (FPlatformTime::Seconds() - Start < 10.0) { return false; }
		Test->AddError(TEXT("scenario evidence timed out")); UWorkflowService::CancelScenario(Id); Starts.Remove(Id); return true;
	}
	Starts.Remove(Id);
	Test->TestEqual(TEXT("expected terminal status"), Report->GetStringField(TEXT("status")), ExpectedStatus);
	Test->TestEqual(TEXT("only verified assertions pass"), Report->GetBoolField(TEXT("passed")), ExpectedStatus == TEXT("passed"));
	Test->TestTrue(TEXT("scenario hash recorded"), Report->GetObjectField(TEXT("provenance"))->HasField(TEXT("scenarioSha1")));
	if (ExpectedStatus == TEXT("smoke_passed"))
	{
		Test->TestEqual(TEXT("smoke evaluated no assertions"), Report->GetNumberField(TEXT("assertionsEvaluated")), 0.0);
	}
	if (!Dependency.IsEmpty())
	{
		Test->TestTrue(TEXT("tracked pass is current"), Report->GetBoolField(TEXT("verifiedCurrent")));
		for (const auto& File : Report->GetObjectField(TEXT("provenance"))->GetArrayField(TEXT("files")))
		{
			Test->TestFalse(TEXT("fingerprint paths are absolute"), FPaths::IsRelative(File->AsObject()->GetStringField(TEXT("path"))));
		}
		const FString PersistedId = TEXT("persisted-") + Id;
		const FString PersistedPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VibeUE/Scenarios"), PersistedId + TEXT(".json"));
		FFileHelper::SaveStringToFile(UWorkflowService::GetScenario(Id), *PersistedPath);
		Test->TestEqual(TEXT("all declared assertions evaluated"), Report->GetNumberField(TEXT("assertionsDeclared")), Report->GetNumberField(TEXT("assertionsEvaluated")));
		// Same-length replacement proves freshness uses content, not file size.
		FFileHelper::SaveStringToFile(TEXT("after!"), *Dependency);
		const auto Stale = ParseWorkflowJson(UWorkflowService::GetScenario(Id));
		Test->TestEqual(TEXT("changed dependency invalidates pass"), Stale->GetStringField(TEXT("status")), FString(TEXT("stale")));
		Test->TestFalse(TEXT("stale cannot pass"), Stale->GetBoolField(TEXT("passed")));
		Test->TestTrue(TEXT("historical outcome remains available"), Stale->GetBoolField(TEXT("historicalPassed")));
		const auto Reloaded = ParseWorkflowJson(UWorkflowService::GetScenario(PersistedId));
		Test->TestEqual(TEXT("disk-loaded evidence also becomes stale"), Reloaded->GetStringField(TEXT("status")), FString(TEXT("stale")));
		IFileManager::Get().Delete(*PersistedPath);
		IFileManager::Get().Delete(*Dependency);
		const auto Missing = ParseWorkflowJson(UWorkflowService::GetScenario(Id));
		Test->TestEqual(TEXT("deleted dependency remains stale"), Missing->GetStringField(TEXT("validity")), FString(TEXT("stale")));
	}
	else { Test->TestFalse(TEXT("untracked report never claims current verification"), Report->GetBoolField(TEXT("verifiedCurrent"))); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioEvidenceTest, "VibeUE.Workflow.Scenario.NumericSmokeAndFreshness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioEvidenceTest::RunTest(const FString&)
{
	const FString Leaf = TEXT("VibeUE-scenario-") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".txt");
	const FString Dependency = FPaths::Combine(FPaths::ProjectSavedDir(), Leaf);
	if (!FFileHelper::SaveStringToFile(TEXT("before"), *Dependency)) { AddError(TEXT("cannot create dependency fixture")); return false; }
	const FString Numeric = FString::Printf(TEXT(R"({"dependencies":["Saved/%s"],"steps":[
		{"action":"python_assert_number","expression":"0.1+0.2","operator":"eq","expected":0.3,"tolerance":0.00001},
		{"action":"python_assert_number","expression":"25","operator":"lt","expected":100},
		{"action":"python_assert_number","expression":"25","operator":"le","expected":25},
		{"action":"python_assert_number","expression":"25","operator":"gt","expected":20},
		{"action":"python_assert_number","expression":"25","operator":"ge","expected":25}],"teardown":{"stop_pie":false}})"), *Leaf);
	auto Queue = [this](const FString& Spec, const FString& Status, const FString& File = FString())
	{
		const auto Result = ParseWorkflowJson(UWorkflowService::RunScenario(Spec));
		if (!Result || !Result->HasField(TEXT("scenarioId"))) { AddError(TEXT("scenario was not queued")); return; }
		ADD_LATENT_AUTOMATION_COMMAND(FVerifyScenarioEvidence(this, Result->GetStringField(TEXT("scenarioId")), Status, File));
	};
	Queue(Numeric, TEXT("passed"), Dependency);
	Queue(TEXT(R"({"smoke":true,"steps":[{"action":"wait"}],"teardown":{"stop_pie":false}})"), TEXT("smoke_passed"));
	Queue(TEXT(R"({"steps":[{"action":"python_assert_number","expression":"10","operator":"ge","expected":20}],"teardown":{"stop_pie":false}})"), TEXT("failed"));
	Queue(TEXT(R"({"steps":[{"action":"python_assert_number","expression":"'25garbage'","operator":"eq","expected":25}],"teardown":{"stop_pie":false}})"), TEXT("failed"));
	Queue(TEXT(R"json({"steps":[{"action":"python_assert_number","expression":"float('nan')","operator":"eq","expected":0}],"teardown":{"stop_pie":false}})json"), TEXT("failed"));
	Queue(TEXT(R"({"steps":[{"action":"python_assert","expression":"1","expected":"2"},{"action":"python_assert","expression":"1","expected":"1"}],"teardown":{"stop_pie":false}})"), TEXT("failed"));
	const FString Marker = TEXT("WorkflowLogAssertion-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Queue(FString::Printf(TEXT(R"({"steps":[{"action":"assert_log","contains":"%s","not_contains":"%s"}],"teardown":{"stop_pie":false}})"), *Marker, *Marker), TEXT("failed"));
	Queue(FString::Printf(TEXT(R"({"steps":[{"action":"assert_log","contains":"%s","not_contains":"%s-absent"}],"teardown":{"stop_pie":false}})"), *Marker, *Marker), TEXT("passed"));
	UE_LOG(LogTemp, Display, TEXT("%s"), *Marker);
	GLog->FlushThreadedLogs(); GLog->Flush();
	return true;
}

// Test-first ownership regressions. The scoped seam replaces only editor/asset/I/O
// boundaries; RunScenario, its ticker, CancelScenario and report transitions are real.
// No fixture assigns GEditor->PlayWorld, starts PIE, loads assets or writes reports.
namespace
{
	struct FWorkflowPIEFixture
	{
		FWorkflowScenarioTestState State;
		FScopedWorkflowScenarioTest Scope{State};
		TStrongObjectPtr<UWorld> OwnedWorld{NewObject<UWorld>(GetTransientPackage())};
		TStrongObjectPtr<UWorld> ForeignWorld{NewObject<UWorld>(GetTransientPackage())};
		TStrongObjectPtr<ULevelEditorPlaySettings> ForeignSettings{
			NewObject<ULevelEditorPlaySettings>(GetTransientPackage())};

		void ForeignQueue()
		{
			State.PlayRequest.Emplace();
			State.PlayRequest->EditorPlaySettings = ForeignSettings.Get();
		}

		void ForeignSession(bool bWithWorld = true)
		{
			State.PlaySession.Emplace();
			State.PlaySession->OriginalRequestParams.EditorPlaySettings = ForeignSettings.Get();
			State.PlaySession->PlayRequestStartTime = 99.0;
			State.PlayWorld = bWithWorld ? ForeignWorld.Get() : nullptr;
		}

		void ConsumeOwnedQueue(bool bWithWorld = true)
		{
			check(State.PlayRequest.IsSet());
			State.PlaySession.Emplace();
			State.PlaySession->OriginalRequestParams = State.PlayRequest.GetValue();
			State.PlaySession->PlayRequestStartTime = 1.0;
			State.PlayRequest.Reset();
			State.PlayWorld = bWithWorld ? OwnedWorld.Get() : nullptr;
			if (bWithWorld) { Scope.NotifyPIEStarted(); }
		}

		FString Queue(const FString& Json)
		{
			// Ownership fixtures exercise lifecycle rather than assertions. Upstream
			// requires an explicit smoke marker when no assertion is requested.
			const TSharedPtr<FJsonObject> Spec = ParseWorkflowJson(Json);
			if (!Spec) { return FString(); }
			Spec->SetBoolField(TEXT("smoke"), true);
			FString ScenarioJson;
			FJsonSerializer::Serialize(Spec.ToSharedRef(), TJsonWriterFactory<>::Create(&ScenarioJson));
			const TSharedPtr<FJsonObject> Result = ParseWorkflowJson(UWorkflowService::RunScenario(ScenarioJson));
			FString Id;
			if (Result.IsValid()) { Result->TryGetStringField(TEXT("scenarioId"), Id); }
			return Id;
		}

		TSharedPtr<FJsonObject> Report(const FString& Id)
		{
			return ParseWorkflowJson(UWorkflowService::GetScenario(Id));
		}
	};

	const TCHAR* ExclusivePIESpec = TEXT("{\"exclusive_pie\":true,\"steps\":[{\"action\":\"start_pie\"},{\"action\":\"wait_for_pie\"},{\"action\":\"wait\",\"seconds\":300}]}");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioCapabilitiesTest, "VibeUE.Workflow.Scenario.Ownership.CapabilitiesReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioCapabilitiesTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	F.ForeignQueue(); F.ForeignSession();
	const UFunction* Function = UWorkflowService::StaticClass()->FindFunctionByName(TEXT("GetScenarioCapabilities"));
	TestTrue(TEXT("capabilities is a reflected static callable"), Function &&
		Function->HasAllFunctionFlags(FUNC_Static | FUNC_BlueprintCallable));
	const TSharedPtr<FJsonObject> Result = ParseWorkflowJson(UWorkflowService::GetScenarioCapabilities());
	if (!TestTrue(TEXT("capabilities JSON"), Result.IsValid())) { return false; }
	TestTrue(TEXT("capabilities query succeeds"), Result->GetBoolField(TEXT("success")));
	TestEqual(TEXT("capability schema"), Result->GetStringField(TEXT("schema")), FString(TEXT("vibeue.scenario_capabilities.v1")));
	TestTrue(TEXT("exclusive PIE contract advertised"), Result->GetBoolField(TEXT("exclusive_pie")));
	TestEqual(TEXT("capability lookup never starts PIE"), F.State.StartCalls, 0);
	TestEqual(TEXT("capability lookup never cancels PIE"), F.State.CancelCalls, 0);
	TestEqual(TEXT("capability lookup never ends PIE"), F.State.EndCalls, 0);
	TestEqual(TEXT("capability lookup writes no artifacts"), F.State.ReportWrites, 0);
	TestTrue(TEXT("foreign request remains"), F.State.PlayRequest.IsSet());
	TestTrue(TEXT("foreign world remains"), F.State.PlayWorld.Get() == F.ForeignWorld.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioUnqualifiedGateTest, "VibeUE.Workflow.Scenario.Ownership.UnqualifiedExclusiveDisabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioUnqualifiedGateTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	F.State.bExclusivePIEQualified = false;
	const TSharedPtr<FJsonObject> Capabilities = ParseWorkflowJson(UWorkflowService::GetScenarioCapabilities());
	if (!TestTrue(TEXT("capabilities JSON"), Capabilities.IsValid())) { return false; }
	TestFalse(TEXT("unqualified live ownership is not advertised"), Capabilities->GetBoolField(TEXT("exclusive_pie")));
	const TSharedPtr<FJsonObject> Rejected = ParseWorkflowJson(UWorkflowService::RunScenario(
		TEXT("{\"smoke\":true,\"exclusive_pie\":true,\"preflight\":{\"compile_blueprints\":[\"/Game/NotLoadedByFixture\"]},\"steps\":[{\"action\":\"start_pie\"}]}")));
	if (!TestTrue(TEXT("rejection JSON"), Rejected.IsValid())) { return false; }
	TestFalse(TEXT("unqualified exclusive run rejected"), Rejected->GetBoolField(TEXT("success")));
	TestFalse(TEXT("no scenario reserved"), Rejected->HasField(TEXT("scenarioId")));
	TestEqual(TEXT("no preflight mutation"), F.State.PreflightCalls, 0);
	TestEqual(TEXT("no report written"), F.State.ReportWrites, 0);
	TestEqual(TEXT("no PIE dispatched"), F.State.StartCalls, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioLiveCapabilityGateTest, "VibeUE.Workflow.Scenario.Ownership.LiveExclusiveDisabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioLiveCapabilityGateTest::RunTest(const FString&)
{
	const TSharedPtr<FJsonObject> Capabilities = ParseWorkflowJson(UWorkflowService::GetScenarioCapabilities());
	if (!TestTrue(TEXT("live capabilities JSON"), Capabilities.IsValid())) { return false; }
	TestTrue(TEXT("read-only capability lookup succeeds"), Capabilities->GetBoolField(TEXT("success")));
	TestFalse(TEXT("unqualified real editor must not advertise exclusive PIE"), Capabilities->GetBoolField(TEXT("exclusive_pie")));
	// A wait-only spec cannot launch PIE even if the admission gate regresses.
	const TSharedPtr<FJsonObject> Rejected = ParseWorkflowJson(UWorkflowService::RunScenario(
		TEXT("{\"smoke\":true,\"exclusive_pie\":true,\"steps\":[{\"action\":\"wait\",\"seconds\":0}]}")));
	if (!TestTrue(TEXT("live rejection JSON"), Rejected.IsValid())) { return false; }
	TestFalse(TEXT("unqualified real editor rejects direct exclusive calls"), Rejected->GetBoolField(TEXT("success")));
	TestFalse(TEXT("direct rejection reserves no scenario"), Rejected->HasField(TEXT("scenarioId")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioForeignAdmissionTest, "VibeUE.Workflow.Scenario.Ownership.ForeignAdmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioForeignAdmissionTest::RunTest(const FString&)
{
	for (int32 Mode = 0; Mode != 3; ++Mode)
	{
		FWorkflowPIEFixture F;
		if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
		if (Mode == 0) { F.State.PlayWorld = F.ForeignWorld.Get(); }
		else if (Mode == 1) { F.ForeignQueue(); }
		else { F.ForeignSession(false); } // session startup is busy even without PlayWorld
		const TSharedPtr<FJsonObject> Result = ParseWorkflowJson(UWorkflowService::RunScenario(
			TEXT("{\"smoke\":true,\"exclusive_pie\":true,\"preflight\":{\"compile_blueprints\":[\"/Game/NotLoadedByFixture\"]},\"steps\":[{\"action\":\"start_pie\"}]}")));
		if (!TestTrue(TEXT("admission returns JSON"), Result.IsValid())) { return false; }
		TestFalse(TEXT("foreign active or queued PIE rejected"), Result->GetBoolField(TEXT("success")));
		TestFalse(TEXT("rejection has no scenario id"), Result->HasField(TEXT("scenarioId")));
		TestEqual(TEXT("admission checked before preflight mutation"), F.State.PreflightCalls, 0);
		TestEqual(TEXT("rejected admission creates no report"), F.State.ReportWrites, 0);
		F.Scope.Tick();
		TestEqual(TEXT("foreign PIE never ended"), F.State.EndCalls, 0);
		TestEqual(TEXT("foreign queued request never cancelled"), F.State.CancelCalls, 0);
		TestEqual(TEXT("foreign queued request never overwritten"), F.State.StartCalls, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioExclusiveTypeTest, "VibeUE.Workflow.Scenario.Ownership.ExclusiveFlagType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioExclusiveTypeTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	for (const TCHAR* Value : {TEXT("1"), TEXT("null"), TEXT("\"true\""), TEXT("\"false\"")})
	{
		const TSharedPtr<FJsonObject> Result = ParseWorkflowJson(UWorkflowService::RunScenario(FString::Printf(
			TEXT("{\"smoke\":true,\"exclusive_pie\":%s,\"steps\":[{\"action\":\"start_pie\"}]}"), Value)));
		if (!TestTrue(TEXT("invalid flag yields JSON"), Result.IsValid())) { return false; }
		TestFalse(TEXT("exclusive flag is strictly boolean, not coercible"), Result->GetBoolField(TEXT("success")));
	}
	TestEqual(TEXT("invalid flag never writes or queues a scenario"), F.State.ReportWrites, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioCompetingAdmissionTest, "VibeUE.Workflow.Scenario.Ownership.CompetingScenarios",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioCompetingAdmissionTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	const FString Observer = F.Queue(TEXT("{\"steps\":[{\"action\":\"wait\",\"seconds\":300}]}"));
	TestFalse(TEXT("legacy observer admitted"), Observer.IsEmpty());
	TestTrue(TEXT("exclusive rejects an already active observer"), F.Queue(ExclusivePIESpec).IsEmpty());
	UWorkflowService::CancelScenario(Observer); F.Scope.Tick();
	const FString Exclusive = F.Queue(ExclusivePIESpec);
	TestFalse(TEXT("exclusive admitted after observer is terminal"), Exclusive.IsEmpty());
	TestTrue(TEXT("second exclusive rejected"), F.Queue(ExclusivePIESpec).IsEmpty());
	TestTrue(TEXT("legacy observer cannot enter exclusive reservation"),
		F.Queue(TEXT("{\"steps\":[{\"action\":\"wait\",\"seconds\":0}]}")).IsEmpty());
	UWorkflowService::CancelScenario(Exclusive); F.Scope.Tick();
	TestEqual(TEXT("cancel before first tick never starts PIE"), F.State.StartCalls, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioFirstTickRaceTest, "VibeUE.Workflow.Scenario.Ownership.ForeignBeforeFirstTick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioFirstTickRaceTest::RunTest(const FString&)
{
	for (bool bQueued : {false, true})
	{
		FWorkflowPIEFixture F;
		if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
		const FString Id = F.Queue(ExclusivePIESpec);
		if (!TestFalse(TEXT("scenario queued while idle"), Id.IsEmpty())) { return false; }
		if (bQueued) { F.ForeignQueue(); } else { F.ForeignSession(); }
		F.Scope.Tick(); F.Scope.Tick();
		TestEqual(TEXT("late foreign PIE fails rather than adopts"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("failed")));
		TestEqual(TEXT("start never called after race"), F.State.StartCalls, 0);
		TestEqual(TEXT("late foreign world not stopped"), F.State.EndCalls, 0);
		TestEqual(TEXT("late foreign request not cancelled"), F.State.CancelCalls, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioPendingCancelTest, "VibeUE.Workflow.Scenario.Ownership.CancelBeforeWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioPendingCancelTest::RunTest(const FString&)
{
	for (bool bConsumed : {false, true})
	{
		FWorkflowPIEFixture F;
		if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
		const FString Id = F.Queue(ExclusivePIESpec);
		if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
		F.Scope.Tick();
		if (!TestTrue(TEXT("start enqueued a tagged request"), F.State.PlayRequest.IsSet())) { return false; }
		TestNotNull(TEXT("request carries private settings identity"), F.State.PlayRequest->EditorPlaySettings.Get());
		TestTrue(TEXT("request never uses settings CDO as identity"),
			F.State.PlayRequest->EditorPlaySettings.Get() != GetDefault<ULevelEditorPlaySettings>());
		if (bConsumed) { F.ConsumeOwnedQueue(false); }
		UWorkflowService::CancelScenario(Id); F.Scope.Tick();
		if (bConsumed)
		{
			TestEqual(TEXT("not terminal while an owned startup can still create a world"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("running")));
			TestEqual(TEXT("no unbound EndPlayMap during startup"), F.State.EndCalls, 0);
			F.State.PlayWorld = F.OwnedWorld.Get(); F.Scope.NotifyPIEStarted(); F.Scope.Tick();
		}
		F.Scope.Tick();
		TestEqual(TEXT("cancel reaches terminal after owned cleanup"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("cancelled")));
		TestFalse(TEXT("no request can start PIE after terminal cancellation"), F.State.PlayRequest.IsSet());
		TestFalse(TEXT("no owned startup remains after terminal cancellation"), F.State.PlaySession.IsSet());
		TestEqual(TEXT("only queued start is cancelled"), F.State.CancelCalls, bConsumed ? 0 : 1);
		TestEqual(TEXT("only materialized owned world is stopped"), F.State.EndCalls, bConsumed ? 1 : 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioReplacementTest, "VibeUE.Workflow.Scenario.Ownership.ReplacementWorldAndQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioReplacementTest::RunTest(const FString&)
{
	for (const TCHAR* Action : {TEXT("inject_action"), TEXT("inject_key"), TEXT("console_command"), TEXT("stop_pie")})
	{
		for (int32 Replacement = 0; Replacement != 4; ++Replacement)
		{
			FWorkflowPIEFixture F;
			if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
			// Historical ownership must not authorize any of these actions, even in legacy mode.
			const FString Id = F.Queue(FString::Printf(TEXT("{\"steps\":[{\"action\":\"start_pie\"},{\"action\":\"wait_for_pie\"},{\"action\":\"%s\",\"path\":\"unused\",\"key\":\"W\",\"command\":\"unused\"}]}"), Action));
			if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
			F.Scope.Tick(); F.ConsumeOwnedQueue(); F.Scope.Tick();
			if (Replacement == 0) { F.ForeignSession(); }
			else if (Replacement == 1) { F.State.PlayWorld = F.ForeignWorld.Get(); }
			else if (Replacement == 2) { F.State.PlaySession->PlayRequestStartTime = 2.0; }
			else { F.ForeignQueue(); }
			F.Scope.Tick(); F.Scope.Tick();
			if (Replacement == 3)
			{
				TestEqual(TEXT("owned teardown waits while foreign queue is outstanding"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("running")));
				F.State.PlayRequest.Reset(); F.ForeignSession(); F.Scope.Tick();
			}
			else if (Replacement == 1 || Replacement == 2)
			{
				TestEqual(TEXT("remaining bound world or tagged session keeps cleanup pending"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("running")));
				TestTrue(TEXT("partial identity loss is not a cleanup acknowledgement"), F.Report(Id)->GetBoolField(TEXT("teardownPending")));
				if (Replacement == 1) { F.State.PlaySession.Reset(); }
				else { F.State.PlayWorld = F.ForeignWorld.Get(); }
				F.Scope.Tick();
			}
			TestEqual(TEXT("replacement forbids the step"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("failed")));
			TestEqual(TEXT("no input or command reaches replacement"), F.State.MutationCalls, 0);
			TestEqual(TEXT("no global deferred stop or foreign EndPlayMap"), F.State.EndCalls, 0);
			TestEqual(TEXT("replacement queue is never cancelled"), F.State.CancelCalls, 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioTerminalReplacementTest, "VibeUE.Workflow.Scenario.Ownership.TerminalTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioTerminalReplacementTest::RunTest(const FString&)
{
	for (bool bCancel : {false, true})
	{
		FWorkflowPIEFixture F;
		if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
		const FString Id = F.Queue(TEXT("{\"steps\":[{\"action\":\"start_pie\"},{\"action\":\"wait_for_pie\"}]}"));
		if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
		F.Scope.Tick(); F.ConsumeOwnedQueue(); F.Scope.Tick();
		F.ForeignSession();
		if (bCancel) { UWorkflowService::CancelScenario(Id); }
		F.Scope.Tick(); F.Scope.Tick();
		TestTrue(TEXT("replacement world survives terminal path"), F.State.PlayWorld.Get() == F.ForeignWorld.Get());
		TestEqual(TEXT("terminal teardown never stops replacement"), F.State.EndCalls, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioOwnedStopTest, "VibeUE.Workflow.Scenario.Ownership.OwnedStopAndObservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioOwnedStopTest::RunTest(const FString&)
{
	for (bool bExplicitStop : {false, true})
	{
		FWorkflowPIEFixture F;
		if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
		const FString Id = F.Queue(bExplicitStop
			? TEXT("{\"exclusive_pie\":true,\"steps\":[{\"action\":\"start_pie\"},{\"action\":\"wait_for_pie\"},{\"action\":\"stop_pie\"}]}")
			: TEXT("{\"exclusive_pie\":true,\"steps\":[{\"action\":\"start_pie\"},{\"action\":\"wait_for_pie\"}]}"));
		if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
		F.Scope.Tick(); F.ConsumeOwnedQueue(); F.Scope.Tick();
		F.Scope.Tick(); F.Scope.Tick(); F.Scope.Tick();
		TestEqual(TEXT("owned smoke session finishes"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("smoke_passed")));
		TestTrue(TEXT("teardown acknowledged after readback"), F.Report(Id)->GetBoolField(TEXT("teardownSucceeded")));
		TestEqual(TEXT("owned session ended exactly once"), F.State.EndCalls, 1);
	}
	{
		FWorkflowPIEFixture F;
		if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
		F.ForeignSession();
		const FString Id = F.Queue(TEXT("{\"steps\":[{\"action\":\"start_pie\"},{\"action\":\"wait_for_pie\"}]}"));
		if (!TestFalse(TEXT("legacy observation remains compatible"), Id.IsEmpty())) { return false; }
		F.Scope.Tick(); F.Scope.Tick(); F.Scope.Tick(); F.Scope.Tick();
		TestEqual(TEXT("observation completes as smoke"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("smoke_passed")));
		TestEqual(TEXT("automatic teardown never owns observed PIE"), F.State.EndCalls, 0);
		const FString StopId = F.Queue(TEXT("{\"steps\":[{\"action\":\"stop_pie\"}]}"));
		if (!TestFalse(TEXT("legacy stop scenario admitted for execution-time guard"), StopId.IsEmpty())) { return false; }
		F.Scope.Tick();
		TestEqual(TEXT("explicit foreign stop fails"), F.Report(StopId)->GetStringField(TEXT("status")), FString(TEXT("failed")));
		TestEqual(TEXT("explicit foreign stop does not end PIE"), F.State.EndCalls, 0);
	}
	{
		FWorkflowPIEFixture F;
		if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
		const FString Id = F.Queue(TEXT("{\"exclusive_pie\":true,\"steps\":[{\"action\":\"start_pie\"},{\"action\":\"stop_pie\"}]}"));
		if (!TestFalse(TEXT("pending explicit stop admitted"), Id.IsEmpty())) { return false; }
		F.Scope.Tick(); F.Scope.Tick(); F.Scope.Tick();
		TestEqual(TEXT("explicit stop cancels own queued request"), F.State.CancelCalls, 1);
		TestFalse(TEXT("explicit stop leaves nothing to start later"), F.State.PlayRequest.IsSet());
		TestEqual(TEXT("explicit pending stop completes as smoke"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("smoke_passed")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioQueuedReplacementTest, "VibeUE.Workflow.Scenario.Ownership.QueuedReplacementAndOptOut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioQueuedReplacementTest::RunTest(const FString&)
{
	for (bool bReplace : {false, true})
	{
		FWorkflowPIEFixture F;
		if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
		const FString Id = F.Queue(TEXT("{\"exclusive_pie\":true,\"teardown\":{\"stop_pie\":false},\"steps\":[{\"action\":\"start_pie\"},{\"action\":\"wait_for_pie\"}]}"));
		if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
		F.Scope.Tick();
		if (!TestTrue(TEXT("own request queued"), F.State.PlayRequest.IsSet())) { return false; }
		if (bReplace) { F.ForeignQueue(); }
		UWorkflowService::CancelScenario(Id); F.Scope.Tick();
		TestEqual(TEXT("only own pending request cancelled, including teardown opt-out"), F.State.CancelCalls, bReplace ? 0 : 1);
		TestEqual(TEXT("no world-less stop"), F.State.EndCalls, 0);
		TestEqual(TEXT("foreign queued request preserved"), F.State.PlayRequest.IsSet(), bReplace);
		if (bReplace) { TestTrue(TEXT("foreign queue identity unchanged"), F.State.PlayRequest->EditorPlaySettings.Get() == F.ForeignSettings.Get()); }
		TestEqual(TEXT("only positively cancelled ownership can finish"), F.Report(Id)->GetStringField(TEXT("status")),
			FString(bReplace ? TEXT("running") : TEXT("cancelled")));
		if (bReplace)
		{
			TestTrue(TEXT("unproven replaced request retains cleanup"), F.Report(Id)->GetBoolField(TEXT("teardownPending")));
			TestFalse(TEXT("unproven replaced request has no teardown receipt"), F.Report(Id)->HasField(TEXT("teardownSucceeded")));
			F.State.PlayRequest.Reset(); F.Scope.Tick();
			TestEqual(TEXT("foreign withdrawal cannot prove our request was cancelled"),
				F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("running")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioTapReplacementTest, "VibeUE.Workflow.Scenario.Ownership.ReentrantTapReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioTapReplacementTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	const FString Id = F.Queue(TEXT("{\"exclusive_pie\":true,\"steps\":[{\"action\":\"start_pie\"},{\"action\":\"wait_for_pie\"},{\"action\":\"inject_key\",\"key\":\"W\",\"event\":\"tap\"}]}"));
	if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
	F.Scope.Tick(); F.ConsumeOwnedQueue(); F.Scope.Tick();
	F.State.OnMutation = [&F]() { F.ForeignSession(); };
	F.Scope.Tick(); F.Scope.Tick();
	TestEqual(TEXT("only the owned key-down is delivered, not foreign key-up"), F.State.MutationCalls, 1);
	TestEqual(TEXT("tap fails when ownership changes in callback"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("failed")));
	TestEqual(TEXT("reentrant replacement never stopped"), F.State.EndCalls, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioCancelWithForeignQueueTest, "VibeUE.Workflow.Scenario.Ownership.CancelStartupWithForeignQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioCancelWithForeignQueueTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	const FString Id = F.Queue(ExclusivePIESpec);
	if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
	F.Scope.Tick(); F.ConsumeOwnedQueue(false); F.ForeignQueue();
	UWorkflowService::CancelScenario(Id); F.Scope.Tick();
	F.State.PlayWorld = F.OwnedWorld.Get(); F.Scope.NotifyPIEStarted(); F.Scope.Tick();
	TestEqual(TEXT("foreign queue delays owned teardown, not terminal abandonment"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("running")));
	TestTrue(TEXT("cleanup remains explicitly pending"), F.Report(Id)->GetBoolField(TEXT("teardownPending")));
	TestEqual(TEXT("foreign request not cancelled"), F.State.CancelCalls, 0);
	TestEqual(TEXT("EndPlayMap not called while foreign request is queued"), F.State.EndCalls, 0);
	F.State.PlayRequest.Reset(); // foreign caller withdraws its own request
	F.Scope.Tick();
	TestEqual(TEXT("owned world is cleaned up rather than orphaned"), F.State.EndCalls, 1);
	TestEqual(TEXT("only now cancellation is terminal"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("cancelled")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioPartiallyStartedTest, "VibeUE.Workflow.Scenario.Ownership.CancelBeforePostPIEStarted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioPartiallyStartedTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	const FString Id = F.Queue(TEXT("{\"exclusive_pie\":true,\"teardown\":{\"stop_pie\":false},\"steps\":[{\"action\":\"start_pie\"},{\"action\":\"wait_for_pie\"}]}"));
	if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
	F.Scope.Tick(); F.ConsumeOwnedQueue(false);
	F.State.PlayWorld = F.OwnedWorld.Get(); // world exists, but BeginPlay/startup has not completed
	F.Scope.Tick(); // bind the world without confusing existence with startup completion
	UWorkflowService::CancelScenario(Id); F.Scope.Tick();
	TestEqual(TEXT("no EndPlayMap inside partially initialized PIE"), F.State.EndCalls, 0);
	TestEqual(TEXT("no premature cancellation acknowledgement"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("running")));
	F.Scope.NotifyPIEStarted(); F.Scope.Tick(false);
	TestEqual(TEXT("same-frame modal tick cannot end startup"), F.State.EndCalls, 0);
	F.Scope.Tick();
	TestEqual(TEXT("completed startup is then stopped"), F.State.EndCalls, 1);
	TestEqual(TEXT("owned startup cannot survive terminal cancellation"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("cancelled")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioCleanupReadbackTest, "VibeUE.Workflow.Scenario.Ownership.CleanupReadback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioCleanupReadbackTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	const FString Id = F.Queue(ExclusivePIESpec);
	if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
	F.Scope.Tick(); F.ConsumeOwnedQueue(); F.Scope.Tick();
	F.State.bCompleteEndPlay = false;
	UWorkflowService::CancelScenario(Id); F.Scope.Tick(); F.Scope.Tick();
	TestEqual(TEXT("unacknowledged EndPlayMap is issued only once"), F.State.EndCalls, 1);
	TestEqual(TEXT("successful call is not a cleanup acknowledgement"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("running")));
	TestTrue(TEXT("pending cleanup retains exclusive reservation"), F.Queue(ExclusivePIESpec).IsEmpty());
	F.State.PlaySession.Reset(); F.Scope.Tick();
	TestEqual(TEXT("old PlayWorld must also disappear before acknowledgement"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("running")));
	F.State.PlayWorld.Reset(); F.Scope.Tick();
	TestEqual(TEXT("readback completes cancellation"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("cancelled")));
	TestTrue(TEXT("cleanup acknowledgement is explicit"), F.Report(Id)->GetBoolField(TEXT("teardownSucceeded")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioCleanupIdentityTest, "VibeUE.Workflow.Scenario.Ownership.CleanupIdentityReadback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioCleanupIdentityTest::RunTest(const FString&)
{
	for (bool bReplacementWorld : {false, true})
	{
		for (bool bCancel : {false, true})
		{
			FWorkflowPIEFixture F;
			if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
			const FString Id = F.Queue(bCancel ? ExclusivePIESpec :
				TEXT("{\"exclusive_pie\":true,\"steps\":[{\"action\":\"start_pie\"},{\"action\":\"wait_for_pie\"}]}"));
			if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
			F.Scope.Tick(); F.ConsumeOwnedQueue(); F.Scope.Tick();
			if (bCancel) { UWorkflowService::CancelScenario(Id); }
			else { F.Scope.Tick(); } // all steps passed, but owned cleanup has not run
			if (bReplacementWorld) { F.State.PlayWorld = F.ForeignWorld.Get(); }
			else { F.State.PlaySession.Reset(); }
			F.Scope.Tick(); F.Scope.Tick();
			const TSharedPtr<FJsonObject> Pending = F.Report(Id);
			TestEqual(bReplacementWorld ? TEXT("G2: tagged session prevents terminal cleanup") :
				TEXT("G1: bound current world prevents terminal cleanup"), Pending->GetStringField(TEXT("status")), FString(TEXT("running")));
			TestTrue(TEXT("partial identity loss keeps teardown pending"), Pending->GetBoolField(TEXT("teardownPending")));
			TestFalse(TEXT("partial cleanup has no terminal teardown acknowledgement"), Pending->HasField(TEXT("teardownSucceeded")));
			TestFalse(TEXT("partial cleanup has no finish timestamp"), Pending->HasField(TEXT("finishedAtIso")));
			TestEqual(TEXT("unowned or unbound world is never stopped"), F.State.EndCalls, 0);
			TestEqual(TEXT("cleanup never injects input"), F.State.MutationCalls, 0);
			const FString Observer = F.Queue(TEXT("{\"steps\":[{\"action\":\"wait\",\"seconds\":300}]}"));
			TestTrue(TEXT("pending cleanup retains reservation even against nonexclusive observers"), Observer.IsEmpty());
			if (!Observer.IsEmpty()) { UWorkflowService::CancelScenario(Observer); }

			// Simulate external teardown readback, never stop a replacement to force success.
			if (bReplacementWorld) { F.State.PlaySession.Reset(); }
			else { F.State.PlayWorld.Reset(); }
			F.Scope.Tick();
			const TSharedPtr<FJsonObject> Finished = F.Report(Id);
			TestEqual(TEXT("only reconciled cleanup reaches the intended terminal status"),
				Finished->GetStringField(TEXT("status")), FString(bCancel ? TEXT("cancelled") : TEXT("smoke_passed")));
			TestTrue(TEXT("reconciled cleanup is explicitly acknowledged"), Finished->GetBoolField(TEXT("teardownSucceeded")));
			TestFalse(TEXT("reconciled cleanup is no longer pending"), Finished->GetBoolField(TEXT("teardownPending")));
			TestEqual(TEXT("no teardown of a replacement during reconciliation"), F.State.EndCalls, 0);
			if (bReplacementWorld) { TestTrue(TEXT("replacement world remains untouched"), F.State.PlayWorld.Get() == F.ForeignWorld.Get()); }
			const FString After = F.Queue(TEXT("{\"steps\":[{\"action\":\"wait\",\"seconds\":300}]}"));
			TestFalse(TEXT("reconciled cleanup releases the exclusive reservation"), After.IsEmpty());
			if (!After.IsEmpty()) { UWorkflowService::CancelScenario(After); }
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioStartupGapTest, "VibeUE.Workflow.Scenario.Ownership.CancelInStartupHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioStartupGapTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	const FString Id = F.Queue(ExclusivePIESpec);
	if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
	F.Scope.Tick();
	if (!TestTrue(TEXT("owned request queued"), F.State.PlayRequest.IsSet())) { return false; }
	const FRequestPlaySessionParams StartingRequest = F.State.PlayRequest.GetValue();
	F.Scope.NotifyPIEBegin(); F.State.PlayRequest.Reset(); // engine has a local startup continuation
	UWorkflowService::CancelScenario(Id); F.Scope.Tick(false);
	TestEqual(TEXT("handoff gap is not mistaken for completed cleanup"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("running")));
	TestEqual(TEXT("no cancellation of a consumed request"), F.State.CancelCalls, 0);
	// PreBeginPIE's single-frame barrier is not enough: the queued request can
	// disappear multiple ticks before the matching session/world is observable.
	F.Scope.Tick(); F.Scope.Tick();
	TestEqual(TEXT("multi-frame handoff stays pending"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("running")));
	TestTrue(TEXT("multi-frame handoff retains reservation"), F.Report(Id)->GetBoolField(TEXT("teardownPending")));
	TestFalse(TEXT("no premature multi-frame teardown receipt"), F.Report(Id)->HasField(TEXT("teardownSucceeded")));
	F.State.PlaySession.Emplace();
	F.State.PlaySession->OriginalRequestParams = StartingRequest;
	F.State.PlaySession->PlayRequestStartTime = 1.0;
	F.State.PlayWorld = F.OwnedWorld.Get(); F.Scope.NotifyPIEStarted(); F.Scope.Tick();
	TestEqual(TEXT("continued owned startup is still stopped"), F.State.EndCalls, 1);
	TestEqual(TEXT("only cleaned continuation is terminal"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("cancelled")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioRunningHandoffTest, "VibeUE.Workflow.Scenario.Ownership.StartInMultiFrameHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioRunningHandoffTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	const FString Id = F.Queue(TEXT("{\"exclusive_pie\":true,\"steps\":[{\"action\":\"start_pie\"},{\"action\":\"wait_for_pie\"}]}"));
	if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
	F.Scope.Tick();
	if (!TestTrue(TEXT("owned request queued"), F.State.PlayRequest.IsSet())) { return false; }
	const FRequestPlaySessionParams StartingRequest = F.State.PlayRequest.GetValue();
	F.Scope.NotifyPIEBegin(); F.State.PlayRequest.Reset();
	F.Scope.Tick(false); F.Scope.Tick(); F.Scope.Tick();
	TestEqual(TEXT("startup handoff is still running"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("running")));
	TestFalse(TEXT("temporary identity gap does not request failure cleanup"), F.Report(Id)->HasField(TEXT("teardownPending")));
	F.State.PlaySession.Emplace();
	F.State.PlaySession->OriginalRequestParams = StartingRequest;
	F.State.PlaySession->PlayRequestStartTime = 1.0;
	F.State.PlayWorld = F.OwnedWorld.Get(); F.Scope.NotifyPIEStarted();
	F.Scope.Tick(); F.Scope.Tick(); F.Scope.Tick();
	TestEqual(TEXT("owned continuation completes naturally"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("smoke_passed")));
	TestEqual(TEXT("only the owned world is stopped"), F.State.EndCalls, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioLostRequestTest, "VibeUE.Workflow.Scenario.Ownership.LostRequestUnattributedWorld",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioLostRequestTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	ULevelEditorPlaySettings* DispatchedSettings = nullptr;
	F.State.OnRequestQueued = [&F, &DispatchedSettings]()
	{
		DispatchedSettings = F.State.PlayRequest->EditorPlaySettings.Get();
		F.State.PlayRequest.Reset(); // Request was dispatched but no identity survives the immediate read.
	};
	const FString Id = F.Queue(ExclusivePIESpec);
	if (!TestFalse(TEXT("scenario admitted"), Id.IsEmpty())) { return false; }
	F.Scope.Tick();
	TestEqual(TEXT("request was actually dispatched once"), F.State.StartCalls, 1);
	TestNotNull(TEXT("request carried private settings"), DispatchedSettings);
	const TSharedPtr<FJsonObject> Started = F.Report(Id);
	if (!TestTrue(TEXT("start step reported"), Started.IsValid() &&
		Started->GetArrayField(TEXT("steps")).Num() == 1)) { return false; }
	TestEqual(TEXT("lost immediate tag is not success"),
		Started->GetArrayField(TEXT("steps"))[0]->AsObject()->GetStringField(TEXT("error")),
		FString(TEXT("owned PIE request was not retained")));
	// The engine may not expose either identity until a later tick. Absence of a
	// world now is not proof that the dispatched request cannot create one later.
	F.Scope.Tick();
	const TSharedPtr<FJsonObject> BeforeWorld = F.Report(Id);
	TestEqual(TEXT("unconfirmed dispatch remains pending before a world appears"),
		BeforeWorld->GetStringField(TEXT("status")), FString(TEXT("running")));
	TestTrue(TEXT("unconfirmed dispatch retains teardown reservation"), BeforeWorld->GetBoolField(TEXT("teardownPending")));
	TestFalse(TEXT("no premature teardown receipt before world appears"), BeforeWorld->HasField(TEXT("teardownSucceeded")));
	TestTrue(TEXT("unconfirmed dispatch refuses competing ownership"), F.Queue(ExclusivePIESpec).IsEmpty());
	F.State.PlayWorld = F.ForeignWorld.Get(); // It might have appeared later; never infer ownership.
	UWorkflowService::CancelScenario(Id);
	F.Scope.Tick();
	const TSharedPtr<FJsonObject> Pending = F.Report(Id);
	TestEqual(TEXT("unattributed live world must keep cleanup reserved"),
		Pending->GetStringField(TEXT("status")), FString(TEXT("running")));
	TestTrue(TEXT("cleanup remains pending"), Pending->GetBoolField(TEXT("teardownPending")));
	TestFalse(TEXT("no terminal teardown acknowledgement"), Pending->HasField(TEXT("teardownSucceeded")));
	TestFalse(TEXT("no premature finish timestamp"), Pending->HasField(TEXT("finishedAtIso")));
	TestEqual(TEXT("unattributed world was not stopped"), F.State.EndCalls, 0);
	TestEqual(TEXT("a vanished request was not cancelled"), F.State.CancelCalls, 0);
	TestTrue(TEXT("unattributed world remains untouched"), F.State.PlayWorld.Get() == F.ForeignWorld.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioPreflightFailureTest, "VibeUE.Workflow.Scenario.Ownership.PreflightFailureAndReservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioPreflightFailureTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	F.ForeignSession(); F.State.bPreflightSuccess = false;
	const TSharedPtr<FJsonObject> Failure = ParseWorkflowJson(UWorkflowService::RunScenario(
		TEXT("{\"smoke\":true,\"preflight\":{\"compile_blueprints\":[\"/Game/NotLoadedByFixture\"]},\"steps\":[{\"action\":\"wait\",\"seconds\":0}]}")));
	if (!TestTrue(TEXT("failure report available"), Failure.IsValid())) { return false; }
	const FString Id = Failure->GetStringField(TEXT("id"));
	F.Scope.Tick();
	TestEqual(TEXT("preflight fails through real finalization"), F.Report(Id)->GetStringField(TEXT("status")), FString(TEXT("failed")));
	TestEqual(TEXT("preflight failure does not teardown foreign PIE"), F.State.EndCalls, 0);
	F.State.PlaySession.Reset(); F.State.PlayWorld.Reset(); F.State.bPreflightSuccess = true;
	F.State.OnPreflight = [&F, this](const FString&)
	{
		TestTrue(TEXT("reservation exists during reentrant preflight"), F.Queue(ExclusivePIESpec).IsEmpty());
		F.ForeignQueue();
	};
	const TSharedPtr<FJsonObject> Refused = ParseWorkflowJson(UWorkflowService::RunScenario(
		TEXT("{\"smoke\":true,\"exclusive_pie\":true,\"preflight\":{\"compile_blueprints\":[\"/Game/NotLoadedByFixture\",\"/Game/AlsoNotLoaded\"]},\"steps\":[{\"action\":\"start_pie\"}]}")));
	if (!TestTrue(TEXT("preflight race yields a report"), Refused.IsValid())) { return false; }
	TestEqual(TEXT("preflight race fails closed"), Refused->GetStringField(TEXT("status")), FString(TEXT("failed")));
	F.Scope.Tick(); F.Scope.Tick();
	TestEqual(TEXT("no second compile after foreign request appears"), F.State.PreflightCalls, 2);
	TestEqual(TEXT("preflight race never starts PIE"), F.State.StartCalls, 0);
	TestEqual(TEXT("preflight race preserves foreign queue"), F.State.CancelCalls, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowScenarioPreflightCancelReservationTest, "VibeUE.Workflow.Scenario.Ownership.CancelDuringPreflight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowScenarioPreflightCancelReservationTest::RunTest(const FString&)
{
	FWorkflowPIEFixture F;
	if (!TestTrue(TEXT("isolated fixture bound"), F.Scope.IsBound())) { return false; }
	F.State.OnPreflight = [&F, this](const FString& Id)
	{
		UWorkflowService::CancelScenario(Id);
		TestTrue(TEXT("cancelled preflight retains reservation until stack returns"), F.Queue(ExclusivePIESpec).IsEmpty());
	};
	const TSharedPtr<FJsonObject> Result = ParseWorkflowJson(UWorkflowService::RunScenario(
		TEXT("{\"smoke\":true,\"exclusive_pie\":true,\"preflight\":{\"compile_blueprints\":[\"/Game/NotLoadedByFixture\",\"/Game/AlsoNotLoaded\"]},\"steps\":[{\"action\":\"start_pie\"}]}")));
	if (!TestTrue(TEXT("cancelled preflight returns JSON"), Result.IsValid())) { return false; }
	TestEqual(TEXT("preflight cancellation retained"), Result->GetStringField(TEXT("status")), FString(TEXT("cancelled")));
	TestEqual(TEXT("no further compile after reentrant cancellation"), F.State.PreflightCalls, 1);
	TestFalse(TEXT("reservation releases after preflight returns"), F.Queue(ExclusivePIESpec).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorkflowBulkTest, "VibeUE.Workflow.Bulk.InterfaceAndMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorkflowBulkTest::RunTest(const FString&)
{
	const FString InterfacePath = NewWorkflowAssetPath(TEXT("BPI_Test"));
	const FString APath = NewWorkflowAssetPath(TEXT("BP_A"));
	const FString BPath = NewWorkflowAssetPath(TEXT("BP_B"));
	UBlueprint* Interface = CreateWorkflowBlueprint(InterfacePath, BPTYPE_Interface);
	UBlueprint* A = CreateWorkflowBlueprint(APath); UBlueprint* B = CreateWorkflowBlueprint(BPath);
	if (!TestTrue(TEXT("test assets created"), Interface && A && B)) { return false; }
	FEdGraphPinType StringType; StringType.PinCategory = UEdGraphSchema_K2::PC_String;
	FBlueprintEditorUtils::AddMemberVariable(A, TEXT("Description"), StringType);
	FBlueprintEditorUtils::AddMemberVariable(B, TEXT("Description"), StringType);
	FBlueprintEditorUtils::AddMemberVariable(B, TEXT("Extra"), StringType);
	UEditorAssetLibrary::SaveLoadedAsset(Interface, false); UEditorAssetLibrary::SaveLoadedAsset(A, false); UEditorAssetLibrary::SaveLoadedAsset(B, false);
	const TSharedPtr<FJsonObject> StartedRun = ParseWorkflowJson(UWorkflowService::StartRun(TEXT("Bulk workflow automation"), TEXT("{}")));
	if (!TestTrue(TEXT("bulk journal starts"), StartedRun.IsValid() && StartedRun->GetBoolField(TEXT("success")))) { return false; }
	const FString RunId = StartedRun->GetStringField(TEXT("runId"));

	const FString InterfacePlan = FString::Printf(TEXT("{\"resume_id\":\"workflow-interface-%s\",\"operation\":\"interface_add\",\"path_scope\":\"/Game/VibeUE_Automation\",\"interface\":\"%s\",\"targets\":[\"%s\",\"%s\"]}"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8), *InterfacePath, *APath, *BPath);
	const TSharedPtr<FJsonObject> DryRun = ParseWorkflowJson(UWorkflowService::RunBulkMaintenance(InterfacePlan, false, 1, true));
	TestTrue(TEXT("dry run succeeds"), DryRun.IsValid() && DryRun->GetBoolField(TEXT("success")));
	const TSharedPtr<FJsonObject> Applied = ParseWorkflowJson(UWorkflowService::RunBulkMaintenance(InterfacePlan, true, 1, true));
	TestTrue(TEXT("interface apply succeeds"), Applied.IsValid() && Applied->GetBoolField(TEXT("success")));
	const TSharedPtr<FJsonObject> Resumed = ParseWorkflowJson(UWorkflowService::RunBulkMaintenance(InterfacePlan, true, 1, true));
	TestEqual(TEXT("successful targets are not repeated on resume"), static_cast<int32>(Resumed->GetNumberField(TEXT("skipped"))), 2);

	const FString MetadataPlan = FString::Printf(TEXT("{\"resume_id\":\"workflow-meta-%s\",\"operation\":\"variable_metadata\",\"path_scope\":\"/Game/VibeUE_Automation\",\"targets\":[{\"asset\":\"%s\",\"variable\":\"Description\",\"category\":\"Test\",\"description\":\"A value\"},{\"asset\":\"%s\",\"variable\":\"Description\",\"category\":\"Test\",\"description\":\"B value\"}]}"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8), *APath, *BPath);
	const TSharedPtr<FJsonObject> Metadata = ParseWorkflowJson(UWorkflowService::RunBulkMaintenance(MetadataPlan, true, 2, true));
	TestTrue(TEXT("metadata apply succeeds"), Metadata.IsValid() && Metadata->GetBoolField(TEXT("success")));
	FString Tooltip; TestTrue(TEXT("metadata readback"), FBlueprintEditorUtils::GetBlueprintVariableMetaData(A, TEXT("Description"), nullptr, TEXT("tooltip"), Tooltip));
	TestEqual(TEXT("description preserved"), Tooltip, FString(TEXT("A value")));

	const FString PartialPlan = FString::Printf(TEXT("{\"resume_id\":\"workflow-partial-%s\",\"operation\":\"variable_metadata\",\"path_scope\":\"/Game/VibeUE_Automation\",\"targets\":[{\"asset\":\"%s\",\"variable\":\"Description\",\"category\":\"Changed\",\"description\":\"Should not overwrite\"},{\"asset\":\"%s\",\"variable\":\"Extra\",\"category\":\"Test\",\"description\":\"New metadata\"}]}"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8), *APath, *BPath);
	const TSharedPtr<FJsonObject> Partial = ParseWorkflowJson(UWorkflowService::RunBulkMaintenance(PartialPlan, true, 2, false));
	TestEqual(TEXT("partial failure count"), static_cast<int32>(Partial->GetNumberField(TEXT("failed"))), 1);
	TestEqual(TEXT("partial success count"), static_cast<int32>(Partial->GetNumberField(TEXT("succeeded"))), 1);
	FString PreservedTooltip;
	FBlueprintEditorUtils::GetBlueprintVariableMetaData(A, TEXT("Description"), nullptr, TEXT("tooltip"), PreservedTooltip);
	TestEqual(TEXT("authored metadata survives partial run"), PreservedTooltip, FString(TEXT("A value")));

	const FString CollisionPlan = FString::Printf(TEXT("{\"resume_id\":\"workflow-collision-%s\",\"operation\":\"asset_move\",\"path_scope\":\"/Game/VibeUE_Automation\",\"targets\":[{\"source\":\"%s\",\"destination\":\"%s\"}]}"),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8), *APath, *BPath);
	const TSharedPtr<FJsonObject> Collision = ParseWorkflowJson(UWorkflowService::RunBulkMaintenance(CollisionPlan, true, 1, true));
	TestFalse(TEXT("name collision is refused"), Collision->GetBoolField(TEXT("success")));

	const TSharedPtr<FJsonObject> FinishedRun = ParseWorkflowJson(UWorkflowService::FinishRun(RunId, TEXT("partial"), TEXT("Expected collision and metadata refusal verified")));
	TestTrue(TEXT("bulk reports attach to journal"), FinishedRun->GetArrayField(TEXT("artifacts")).Num() >= 4);

	UEditorAssetLibrary::DeleteAsset(APath); UEditorAssetLibrary::DeleteAsset(BPath); UEditorAssetLibrary::DeleteAsset(InterfacePath);
	UWorkflowService::DeleteRun(RunId);
	return true;
}

#endif
