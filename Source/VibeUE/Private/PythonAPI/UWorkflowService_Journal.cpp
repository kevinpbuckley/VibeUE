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



