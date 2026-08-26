// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UWorkflowService.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Utils/VibeUEEnvironment.h"

namespace
{
	FString GActiveWorkflowRun;
	FDelegateHandle GObjectModifiedHandle;
	bool GWritingWorkflowRun = false;

	FString RunsDir()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VibeUE/Runs"));
	}

	bool SafeWorkflowId(const FString& Id)
	{
		if (Id.IsEmpty()) { return false; }
		for (TCHAR C : Id) { if (!(FChar::IsAlnum(C) || C == '-' || C == '_')) { return false; } }
		return true;
	}

	FString WorkflowRunPath(const FString& Id) { return FPaths::Combine(RunsDir(), Id + TEXT(".json")); }
	FString WorkflowRunMarkdownPath(const FString& Id) { return FPaths::Combine(RunsDir(), Id + TEXT(".md")); }

	FString InvokeOptionalGameIQ(const FName FunctionName, const TMap<FName, FString>& Strings,
		const TMap<FName, int32>& Integers = {})
	{
		UClass* ServiceClass = FindObject<UClass>(nullptr, TEXT("/Script/GameIQ.GameIQService"));
		if (!ServiceClass) { return FString(); }
		UFunction* Function = ServiceClass->FindFunctionByName(FunctionName);
		UObject* Service = ServiceClass->GetDefaultObject();
		if (!Function || !Service) { return FString(); }

		FStructOnScope Parameters(Function);
		uint8* Memory = Parameters.GetStructMemory();
		for (TFieldIterator<FProperty> It(Function); It; ++It)
		{
			FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm)) { continue; }
			if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
			{
				if (const FString* Value = Strings.Find(Property->GetFName()))
				{
					StringProperty->SetPropertyValue_InContainer(Memory, *Value);
				}
			}
			else if (FIntProperty* IntProperty = CastField<FIntProperty>(Property))
			{
				if (const int32* Value = Integers.Find(Property->GetFName()))
				{
					IntProperty->SetPropertyValue_InContainer(Memory, *Value);
				}
			}
		}
		Service->ProcessEvent(Function, Memory);
		for (TFieldIterator<FProperty> It(Function); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				if (const FStrProperty* ReturnProperty = CastField<FStrProperty>(*It))
				{
					return ReturnProperty->GetPropertyValue_InContainer(Memory);
				}
			}
		}
		return FString();
	}

	TSharedPtr<FJsonObject> ParseWorkflowObject(const FString& Text)
	{
		TSharedPtr<FJsonObject> Object;
		return !Text.IsEmpty() && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) ? Object : nullptr;
	}

	FString SerializeWorkflow(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out));
		return Out;
	}

	FString WorkflowError(const FString& Message)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("success"), false);
		Root->SetStringField(TEXT("error"), Message);
		return SerializeWorkflow(Root);
	}

	TSharedPtr<FJsonObject> LoadWorkflowRun(const FString& Id)
	{
		if (!SafeWorkflowId(Id)) { return nullptr; }
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *WorkflowRunPath(Id))) { return nullptr; }
		TSharedPtr<FJsonObject> Object;
		return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Object) ? Object : nullptr;
	}

	bool AtomicWriteWorkflow(const FString& Path, const FString& Text)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		const FString Temp = Path + TEXT(".tmp");
		if (!FFileHelper::SaveStringToFile(Text, *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) { return false; }
		return IFileManager::Get().Move(*Path, *Temp, true, true, false, true);
	}

	TSharedPtr<FJsonValue> RedactedWorkflowValue(const TSharedPtr<FJsonValue>& Value)
	{
		if (!Value.IsValid()) { return MakeShared<FJsonValueNull>(); }
		if (Value->Type == EJson::Object)
		{
			TSharedRef<FJsonObject> Redacted = MakeShared<FJsonObject>();
			for (const auto& Pair : Value->AsObject()->Values)
			{
				const FString Key = FString(Pair.Key).ToLower();
				if (Key.Contains(TEXT("token")) || Key.Contains(TEXT("secret")) || Key.Contains(TEXT("password")) ||
					Key.Contains(TEXT("authorization")) || Key.Contains(TEXT("api_key")))
				{
					Redacted->SetStringField(Pair.Key, TEXT("[REDACTED]"));
				}
				else { Redacted->SetField(Pair.Key, RedactedWorkflowValue(Pair.Value)); }
			}
			return MakeShared<FJsonValueObject>(Redacted);
		}
		else if (Value->Type == EJson::Array)
		{
			TArray<TSharedPtr<FJsonValue>> Redacted;
			for (const TSharedPtr<FJsonValue>& Item : Value->AsArray()) { Redacted.Add(RedactedWorkflowValue(Item)); }
			return MakeShared<FJsonValueArray>(Redacted);
		}
		return Value;
	}

	FString WorkflowMarkdown(const TSharedRef<FJsonObject>& Run)
	{
		FString Out = FString::Printf(TEXT("# %s\n\n- Run: `%s`\n- Status: **%s**\n- Started: %s\n"),
			*Run->GetStringField(TEXT("name")), *Run->GetStringField(TEXT("id")), *Run->GetStringField(TEXT("status")),
			*Run->GetStringField(TEXT("startedAtIso")));
		FString Finished;
		if (Run->TryGetStringField(TEXT("finishedAtIso"), Finished)) { Out += TEXT("- Finished: ") + Finished + TEXT("\n"); }
		FString Summary;
		if (Run->TryGetStringField(TEXT("summary"), Summary) && !Summary.IsEmpty()) { Out += TEXT("\n## Summary\n\n") + Summary + TEXT("\n"); }
		Out += TEXT("\n## Affected assets\n\n");
		const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
		if (Run->TryGetArrayField(TEXT("affectedAssets"), Assets))
		{
			for (const TSharedPtr<FJsonValue>& Asset : *Assets) { Out += TEXT("- `") + Asset->AsString() + TEXT("`\n"); }
		}
		Out += TEXT("\n## Events\n\n");
		const TArray<TSharedPtr<FJsonValue>>* Events = nullptr;
		if (Run->TryGetArrayField(TEXT("events"), Events))
		{
			for (const TSharedPtr<FJsonValue>& Event : *Events)
			{
				const TSharedPtr<FJsonObject> E = Event->AsObject();
				Out += FString::Printf(TEXT("- %s — **%s**"), *E->GetStringField(TEXT("atIso")), *E->GetStringField(TEXT("type")));
				FString Text; if (E->TryGetStringField(TEXT("text"), Text)) { Out += TEXT(": ") + Text; }
				Out += TEXT("\n");
			}
		}
		Out += TEXT("\n> Coverage: VibeUE observes changed UObject packages and explicit notes/artifacts while a run is active. Calls made outside the editor, secret arguments, and method names from native Epic toolsets may not be observable.\n");
		return Out;
	}

	bool SaveWorkflowRun(const TSharedRef<FJsonObject>& Run)
	{
		TGuardValue<bool> Guard(GWritingWorkflowRun, true);
		const FString Id = Run->GetStringField(TEXT("id"));
		return AtomicWriteWorkflow(WorkflowRunPath(Id), SerializeWorkflow(Run)) &&
			AtomicWriteWorkflow(WorkflowRunMarkdownPath(Id), WorkflowMarkdown(Run));
	}

	void AppendWorkflowEvent(const TSharedRef<FJsonObject>& Run, const FString& Type, const FString& Text,
		const TSharedPtr<FJsonObject>& Data = nullptr)
	{
		TArray<TSharedPtr<FJsonValue>> Events = Run->GetArrayField(TEXT("events"));
		TSharedRef<FJsonObject> Event = MakeShared<FJsonObject>();
		Event->SetStringField(TEXT("atIso"), FDateTime::UtcNow().ToIso8601());
		Event->SetStringField(TEXT("type"), Type);
		if (!Text.IsEmpty()) { Event->SetStringField(TEXT("text"), Text); }
		if (Data.IsValid()) { Event->SetObjectField(TEXT("data"), Data.ToSharedRef()); }
		Events.Add(MakeShared<FJsonValueObject>(Event));
		Run->SetArrayField(TEXT("events"), Events);
	}

	void ObserveModifiedObject(UObject* Object)
	{
		if (GWritingWorkflowRun || GActiveWorkflowRun.IsEmpty() || !Object) { return; }
		const UPackage* Package = Object->GetOutermost();
		if (!Package || Package == GetTransientPackage()) { return; }
		const FString Path = Package->GetName();
		if (!(Path.StartsWith(TEXT("/Game/")) || Path.StartsWith(TEXT("/Engine/")) || Path.StartsWith(TEXT("/Plugins/")))) { return; }
		const TSharedPtr<FJsonObject> Run = LoadWorkflowRun(GActiveWorkflowRun);
		if (!Run.IsValid() || Run->GetStringField(TEXT("status")) != TEXT("running")) { return; }
		TArray<TSharedPtr<FJsonValue>> Assets = Run->GetArrayField(TEXT("affectedAssets"));
		for (const TSharedPtr<FJsonValue>& Asset : Assets) { if (Asset->AsString() == Path) { return; } }
		Assets.Add(MakeShared<FJsonValueString>(Path));
		Assets.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B) { return A->AsString() < B->AsString(); });
		Run->SetArrayField(TEXT("affectedAssets"), Assets);
		AppendWorkflowEvent(Run.ToSharedRef(), TEXT("object_modified"), Path);
		SaveWorkflowRun(Run.ToSharedRef());
	}
}

FString UWorkflowService::GetEnvironment()
{
	return FVibeUEEnvironment::BuildJson();
}

FString UWorkflowService::StartRun(const FString& Name, const FString& MetadataJson)
{
	TSharedPtr<FJsonObject> Metadata;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MetadataJson.IsEmpty() ? TEXT("{}") : MetadataJson), Metadata) || !Metadata.IsValid())
	{
		return WorkflowError(TEXT("metadata_json must be a JSON object"));
	}
	const TSharedPtr<FJsonValue> MetadataValue = RedactedWorkflowValue(MakeShared<FJsonValueObject>(Metadata.ToSharedRef()));
	const FString Id = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")) + TEXT("-") +
		FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
	TSharedRef<FJsonObject> Run = MakeShared<FJsonObject>();
	Run->SetStringField(TEXT("schema"), TEXT("vibeue.run.v1"));
	Run->SetStringField(TEXT("id"), Id);
	Run->SetStringField(TEXT("name"), Name.IsEmpty() ? Id : Name);
	Run->SetStringField(TEXT("status"), TEXT("running"));
	Run->SetStringField(TEXT("startedAtIso"), FDateTime::UtcNow().ToIso8601());
	Run->SetObjectField(TEXT("metadata"), MetadataValue->AsObject().ToSharedRef());
	Run->SetArrayField(TEXT("events"), {});
	Run->SetArrayField(TEXT("affectedAssets"), {});
	Run->SetArrayField(TEXT("artifacts"), {});
	Run->SetStringField(TEXT("coverage"), TEXT("UObject package changes plus explicit workflow notes/artifacts; external calls are not intercepted"));
	AppendWorkflowEvent(Run, TEXT("run_started"), TEXT("Run journal opened"));
	TSharedRef<FJsonObject> GameIQ = MakeShared<FJsonObject>();
	const bool bGameIQAvailable = FindObject<UClass>(nullptr, TEXT("/Script/GameIQ.GameIQService")) != nullptr;
	GameIQ->SetBoolField(TEXT("available"), bGameIQAvailable);
	GameIQ->SetStringField(TEXT("freshnessBoundary"), TEXT("GameIQ save-hook/index freshness; unsaved or unsupported changes may be incomplete"));
	if (bGameIQAvailable)
	{
		const FString SnapshotJson = InvokeOptionalGameIQ(TEXT("CreateSnapshot"), {
			{TEXT("Name"), TEXT("VibeUE before ") + Id},
			{TEXT("MetadataJson"), FString::Printf(TEXT("{\"vibeueRunId\":\"%s\"}"), *Id)}
		});
		if (const TSharedPtr<FJsonObject> Snapshot = ParseWorkflowObject(SnapshotJson))
		{
			if (Snapshot->HasTypedField<EJson::Object>(TEXT("result")))
			{
				const TSharedPtr<FJsonObject> Result = Snapshot->GetObjectField(TEXT("result"));
				GameIQ->SetStringField(TEXT("preSnapshotId"), Result->GetStringField(TEXT("id")));
				GameIQ->SetObjectField(TEXT("preSnapshot"), Result.ToSharedRef());
			}
			else
			{
				FString Error; Snapshot->TryGetStringField(TEXT("error"), Error);
				GameIQ->SetStringField(TEXT("warning"), Error.IsEmpty() ? TEXT("GameIQ snapshot returned no result") : Error);
			}
		}
		else { GameIQ->SetStringField(TEXT("warning"), TEXT("GameIQ snapshot call returned invalid JSON")); }
	}
	else { GameIQ->SetStringField(TEXT("warning"), TEXT("GameIQ is unavailable; semantic pre/post evidence is omitted")); }
	Run->SetObjectField(TEXT("gameIQ"), GameIQ);
	if (!SaveWorkflowRun(Run)) { return WorkflowError(TEXT("failed to write run journal")); }
	GActiveWorkflowRun = Id;
	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true); Result->SetStringField(TEXT("runId"), Id);
	Result->SetStringField(TEXT("jsonPath"), WorkflowRunPath(Id)); Result->SetStringField(TEXT("markdownPath"), WorkflowRunMarkdownPath(Id));
	return SerializeWorkflow(Result);
}

FString UWorkflowService::AddRunNote(const FString& RunId, const FString& Text)
{
	const TSharedPtr<FJsonObject> Run = LoadWorkflowRun(RunId);
	if (!Run.IsValid()) { return WorkflowError(TEXT("run not found")); }
	AppendWorkflowEvent(Run.ToSharedRef(), TEXT("note"), Text);
	if (!SaveWorkflowRun(Run.ToSharedRef())) { return WorkflowError(TEXT("failed to update run")); }
	return SerializeWorkflow(Run.ToSharedRef());
}

FString UWorkflowService::AttachRunArtifact(const FString& RunId, const FString& PathOrId, const FString& Kind)
{
	const TSharedPtr<FJsonObject> Run = LoadWorkflowRun(RunId);
	if (!Run.IsValid()) { return WorkflowError(TEXT("run not found")); }
	TArray<TSharedPtr<FJsonValue>> Artifacts = Run->GetArrayField(TEXT("artifacts"));
	TSharedRef<FJsonObject> Artifact = MakeShared<FJsonObject>();
	Artifact->SetStringField(TEXT("kind"), Kind); Artifact->SetStringField(TEXT("pathOrId"), PathOrId);
	Artifact->SetStringField(TEXT("attachedAtIso"), FDateTime::UtcNow().ToIso8601());
	Artifacts.Add(MakeShared<FJsonValueObject>(Artifact)); Run->SetArrayField(TEXT("artifacts"), Artifacts);
	AppendWorkflowEvent(Run.ToSharedRef(), TEXT("artifact_attached"), PathOrId);
	if (!SaveWorkflowRun(Run.ToSharedRef())) { return WorkflowError(TEXT("failed to update run")); }
	return SerializeWorkflow(Run.ToSharedRef());
}

FString UWorkflowService::FinishRun(const FString& RunId, const FString& Outcome, const FString& Summary)
{
	const TSharedPtr<FJsonObject> Run = LoadWorkflowRun(RunId);
	if (!Run.IsValid()) { return WorkflowError(TEXT("run not found")); }
	if (Run->HasTypedField<EJson::Object>(TEXT("gameIQ")))
	{
		const TSharedPtr<FJsonObject> GameIQ = Run->GetObjectField(TEXT("gameIQ"));
		FString PreSnapshotId;
		if (GameIQ->TryGetStringField(TEXT("preSnapshotId"), PreSnapshotId) && !PreSnapshotId.IsEmpty())
		{
			const FString ChangesJson = InvokeOptionalGameIQ(TEXT("Changes"), {
				{TEXT("SinceSnapshot"), PreSnapshotId}, {TEXT("UntilSnapshot"), TEXT("current")},
				{TEXT("Kind"), TEXT("")}, {TEXT("PathPrefix"), TEXT("")}
			}, {{TEXT("Limit"), 500}, {TEXT("Offset"), 0}});
			if (const TSharedPtr<FJsonObject> Changes = ParseWorkflowObject(ChangesJson))
			{
				if (Changes->HasTypedField<EJson::Object>(TEXT("result")))
				{
					GameIQ->SetObjectField(TEXT("changes"), Changes->GetObjectField(TEXT("result")).ToSharedRef());
				}
				else
				{
					FString Error; Changes->TryGetStringField(TEXT("error"), Error);
					GameIQ->SetStringField(TEXT("finishWarning"), Error.IsEmpty() ? TEXT("GameIQ changes returned no result") : Error);
				}
			}
			else { GameIQ->SetStringField(TEXT("finishWarning"), TEXT("GameIQ changes call returned invalid JSON")); }
		}
	}
	Run->SetStringField(TEXT("status"), Outcome.IsEmpty() ? TEXT("completed") : Outcome.ToLower());
	Run->SetStringField(TEXT("summary"), Summary);
	Run->SetStringField(TEXT("finishedAtIso"), FDateTime::UtcNow().ToIso8601());
	AppendWorkflowEvent(Run.ToSharedRef(), TEXT("run_finished"), Outcome);
	if (!SaveWorkflowRun(Run.ToSharedRef())) { return WorkflowError(TEXT("failed to finish run")); }
	if (GActiveWorkflowRun == RunId) { GActiveWorkflowRun.Reset(); }
	return SerializeWorkflow(Run.ToSharedRef());
}

FString UWorkflowService::GetRun(const FString& RunId)
{
	const TSharedPtr<FJsonObject> Run = LoadWorkflowRun(RunId);
	return Run.IsValid() ? SerializeWorkflow(Run.ToSharedRef()) : WorkflowError(TEXT("run not found"));
}

FString UWorkflowService::ListRuns(int32 Limit)
{
	TArray<FString> Files; IFileManager::Get().FindFiles(Files, *FPaths::Combine(RunsDir(), TEXT("*.json")), true, false);
	Files.Sort([](const FString& A, const FString& B) { return A > B; });
	TArray<TSharedPtr<FJsonValue>> Runs;
	for (int32 Index = 0; Index < Files.Num() && Runs.Num() < FMath::Clamp(Limit, 1, 500); ++Index)
	{
		const TSharedPtr<FJsonObject> Run = LoadWorkflowRun(FPaths::GetBaseFilename(Files[Index]));
		if (Run.IsValid()) { Runs.Add(MakeShared<FJsonValueObject>(Run.ToSharedRef())); }
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("success"), true);
	Root->SetArrayField(TEXT("runs"), Runs); Root->SetNumberField(TEXT("total"), Files.Num());
	return SerializeWorkflow(Root);
}

FString UWorkflowService::DeleteRun(const FString& RunId)
{
	if (!SafeWorkflowId(RunId)) { return WorkflowError(TEXT("invalid run id")); }
	if (GActiveWorkflowRun == RunId) { return WorkflowError(TEXT("cannot delete the active run; finish it first")); }
	const bool bJson = IFileManager::Get().Delete(*WorkflowRunPath(RunId), true, true, true);
	const bool bMarkdown = IFileManager::Get().Delete(*WorkflowRunMarkdownPath(RunId), false, true, true);
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("success"), bJson && bMarkdown);
	Root->SetStringField(TEXT("runId"), RunId); return SerializeWorkflow(Root);
}

void UWorkflowService::InitializeJournal()
{
	if (!GObjectModifiedHandle.IsValid())
	{
		GObjectModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddStatic(&ObserveModifiedObject);
	}
	TArray<FString> Files; IFileManager::Get().FindFiles(Files, *FPaths::Combine(RunsDir(), TEXT("*.json")), true, false);
	for (const FString& File : Files)
	{
		const TSharedPtr<FJsonObject> Run = LoadWorkflowRun(FPaths::GetBaseFilename(File));
		if (Run.IsValid() && Run->GetStringField(TEXT("status")) == TEXT("running"))
		{
			Run->SetStringField(TEXT("status"), TEXT("interrupted"));
			Run->SetStringField(TEXT("finishedAtIso"), FDateTime::UtcNow().ToIso8601());
			AppendWorkflowEvent(Run.ToSharedRef(), TEXT("interrupted_recovery"), TEXT("Editor exited before finish_run"));
			SaveWorkflowRun(Run.ToSharedRef());
		}
	}
}

void UWorkflowService::ShutdownJournal()
{
	if (!GActiveWorkflowRun.IsEmpty())
	{
		const TSharedPtr<FJsonObject> Run = LoadWorkflowRun(GActiveWorkflowRun);
		if (Run.IsValid())
		{
			Run->SetStringField(TEXT("status"), TEXT("interrupted"));
			Run->SetStringField(TEXT("finishedAtIso"), FDateTime::UtcNow().ToIso8601());
			AppendWorkflowEvent(Run.ToSharedRef(), TEXT("editor_shutdown"), TEXT("Editor shut down with run active"));
			SaveWorkflowRun(Run.ToSharedRef());
		}
		GActiveWorkflowRun.Reset();
	}
	if (GObjectModifiedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectModified.Remove(GObjectModifiedHandle); GObjectModifiedHandle.Reset();
	}
}

FString UWorkflowService::GetActiveRunId()
{
	return GActiveWorkflowRun;
}

void UWorkflowService::AttachActiveRunArtifact(const FString& PathOrId, const FString& Kind)
{
	if (!GActiveWorkflowRun.IsEmpty()) { AttachRunArtifact(GActiveWorkflowRun, PathOrId, Kind); }
}

FString UWorkflowService::GetOptionalGameIQImpact(const FString& TopicOrId)
{
	return InvokeOptionalGameIQ(TEXT("ExportGraph"), {
		{TEXT("TopicOrId"), TopicOrId}, {TEXT("Format"), TEXT("json")},
		{TEXT("Direction"), TEXT("both")}, {TEXT("EdgeTypes"), TEXT("")}, {TEXT("Kinds"), TEXT("")}
	}, {{TEXT("Depth"), 1}, {TEXT("MaxNodes"), 50}});
}
