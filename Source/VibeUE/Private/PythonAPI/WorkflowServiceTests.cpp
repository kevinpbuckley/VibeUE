// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UWorkflowService.h"

#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
