// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UWorkflowService.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "Misc/Paths.h"
#include "PythonAPI/UBlueprintService.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	FString SerializeBulk(const TSharedRef<FJsonObject>& Object)
	{
		FString Out; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out)); return Out;
	}

	FString BulkError(const FString& Error)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>(); Root->SetBoolField(TEXT("success"), false);
		Root->SetStringField(TEXT("error"), Error); return SerializeBulk(Root);
	}

	FString BulkDir() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VibeUE/Bulk")); }

	UBlueprint* LoadBulkBlueprint(const FString& Path)
	{
		FString ObjectPath = Path;
		if (!ObjectPath.Contains(TEXT("."))) { ObjectPath += TEXT(".") + FPaths::GetBaseFilename(Path); }
		return LoadObject<UBlueprint>(nullptr, *ObjectPath);
	}

	bool InBulkScope(const FString& Path, const FString& Scope)
	{
		return Scope.IsEmpty() || Path.StartsWith(Scope, ESearchCase::IgnoreCase);
	}

	void SaveBulkReport(const FString& Id, const TSharedRef<FJsonObject>& Report)
	{
		IFileManager::Get().MakeDirectory(*BulkDir(), true);
		const FString Path = FPaths::Combine(BulkDir(), Id + TEXT(".json")), Temp = Path + TEXT(".tmp");
		FFileHelper::SaveStringToFile(SerializeBulk(Report), *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		IFileManager::Get().Move(*Path, *Temp, true, true, false, true);
	}
}

FString UWorkflowService::RunBulkMaintenance(const FString& PlanJson, bool bApply, int32 BatchSize, bool bStopOnError)
{
	TSharedPtr<FJsonObject> Plan;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PlanJson), Plan) || !Plan.IsValid()) { return BulkError(TEXT("plan_json must be an object")); }
	FString Operation; if (!Plan->TryGetStringField(TEXT("operation"), Operation)) { return BulkError(TEXT("plan requires operation")); }
	Operation = Operation.ToLower();
	const TArray<TSharedPtr<FJsonValue>>* Targets = nullptr;
	if (!Plan->TryGetArrayField(TEXT("targets"), Targets) || Targets->Num() == 0) { return BulkError(TEXT("an explicit non-empty targets array is required")); }
	FString Scope; Plan->TryGetStringField(TEXT("path_scope"), Scope);
	const int32 Cap = FMath::Clamp(BatchSize, 1, 100);
	FString PlanId; Plan->TryGetStringField(TEXT("resume_id"), PlanId);
	if (PlanId.IsEmpty()) { PlanId = FMD5::HashAnsiString(*PlanJson).Left(16); }
	for (TCHAR C : PlanId) { if (!(FChar::IsAlnum(C) || C == '-' || C == '_')) { return BulkError(TEXT("resume_id contains invalid characters")); } }

	TSet<FString> PreviouslyCompleted;
	const FString ExistingPath = FPaths::Combine(BulkDir(), PlanId + TEXT(".json"));
	FString ExistingText; TSharedPtr<FJsonObject> Existing;
	if (bApply && FFileHelper::LoadFileToString(ExistingText, *ExistingPath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ExistingText), Existing) && Existing.IsValid() &&
		Existing->GetStringField(TEXT("mode")) == TEXT("apply"))
	{
		const TArray<TSharedPtr<FJsonValue>>* ExistingResults = nullptr;
		if (Existing->TryGetArrayField(TEXT("results"), ExistingResults))
		{
			for (const TSharedPtr<FJsonValue>& Value : *ExistingResults)
			{
				const TSharedPtr<FJsonObject> Result = Value->AsObject(); bool bSucceeded = false;
				if (Result.IsValid() && Result->TryGetBoolField(TEXT("success"), bSucceeded) && bSucceeded) { PreviouslyCompleted.Add(Result->GetStringField(TEXT("targetKey"))); }
			}
		}
	}

	TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	Report->SetStringField(TEXT("schema"), TEXT("vibeue.bulk.v1")); Report->SetStringField(TEXT("id"), PlanId);
	Report->SetStringField(TEXT("operation"), Operation); Report->SetStringField(TEXT("mode"), bApply ? TEXT("apply") : TEXT("dry-run"));
	Report->SetStringField(TEXT("startedAtIso"), FDateTime::UtcNow().ToIso8601()); Report->SetNumberField(TEXT("batchSize"), Cap);
	Report->SetBoolField(TEXT("gameIQAvailable"), FindObject<UClass>(nullptr, TEXT("/Script/GameIQ.GameIQService")) != nullptr);
	if (!Report->GetBoolField(TEXT("gameIQAvailable"))) { Report->SetStringField(TEXT("safetyWarning"), TEXT("GameIQ is unavailable; dependency/impact evidence is not attached.")); }
	else
	{
		TArray<TSharedPtr<FJsonValue>> ImpactEvidence;
		const int32 EvidenceCap = FMath::Min(Targets->Num(), 100);
		for (int32 Index = 0; Index < EvidenceCap; ++Index)
		{
			const TSharedPtr<FJsonValue>& Target = (*Targets)[Index];
			FString AssetPath;
			if (Target->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> TargetObject = Target->AsObject();
				TargetObject->TryGetStringField(TEXT("asset"), AssetPath);
				if (AssetPath.IsEmpty()) { TargetObject->TryGetStringField(TEXT("source"), AssetPath); }
			}
			else { AssetPath = Target->AsString(); }
			TSharedRef<FJsonObject> Evidence = MakeShared<FJsonObject>();
			Evidence->SetStringField(TEXT("target"), AssetPath);
			const FString ImpactJson = UWorkflowService::GetOptionalGameIQImpact(AssetPath);
			TSharedPtr<FJsonObject> Impact;
			if (!ImpactJson.IsEmpty() && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ImpactJson), Impact) && Impact.IsValid())
			{
				Evidence->SetObjectField(TEXT("graph"), Impact.ToSharedRef());
			}
			else { Evidence->SetStringField(TEXT("warning"), TEXT("GameIQ impact query returned no valid JSON")); }
			ImpactEvidence.Add(MakeShared<FJsonValueObject>(Evidence));
		}
		Report->SetArrayField(TEXT("gameIQImpactEvidence"), ImpactEvidence);
		Report->SetBoolField(TEXT("gameIQImpactTruncated"), Targets->Num() > EvidenceCap);
	}
	TArray<TSharedPtr<FJsonValue>> Results;
	int32 Succeeded = 0, Failed = 0, Skipped = 0;

	for (int32 Index = 0; Index < Targets->Num(); ++Index)
	{
		const TSharedPtr<FJsonValue>& TargetValue = (*Targets)[Index];
		FString AssetPath, TargetKey;
		const TSharedPtr<FJsonObject> TargetObject = TargetValue->Type == EJson::Object ? TargetValue->AsObject() : nullptr;
		if (TargetObject.IsValid()) { TargetObject->TryGetStringField(TEXT("asset"), AssetPath); if (AssetPath.IsEmpty()) { TargetObject->TryGetStringField(TEXT("source"), AssetPath); } }
		else { AssetPath = TargetValue->AsString(); }
		TargetKey = TargetObject.IsValid() ? SerializeBulk(TargetObject.ToSharedRef()) : AssetPath;
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>(); Item->SetNumberField(TEXT("index"), Index); Item->SetStringField(TEXT("targetKey"), TargetKey); Item->SetStringField(TEXT("asset"), AssetPath);
		if (PreviouslyCompleted.Contains(TargetKey))
		{
			Item->SetBoolField(TEXT("success"), true); Item->SetStringField(TEXT("status"), TEXT("resumed-skip")); ++Skipped;
			Results.Add(MakeShared<FJsonValueObject>(Item)); continue;
		}
		if (!InBulkScope(AssetPath, Scope))
		{
			Item->SetBoolField(TEXT("success"), false); Item->SetStringField(TEXT("error"), TEXT("target is outside path_scope")); ++Failed;
			Results.Add(MakeShared<FJsonValueObject>(Item)); if (bStopOnError) { break; } continue;
		}
		if (!bApply)
		{
			Item->SetBoolField(TEXT("success"), true); Item->SetStringField(TEXT("status"), TEXT("would-change")); ++Succeeded;
			Results.Add(MakeShared<FJsonValueObject>(Item)); continue;
		}

		const FText TransactionText = FText::FromString(FString::Printf(TEXT("VibeUE bulk %s: %s"), *Operation, *AssetPath));
		if (GEditor) { GEditor->BeginTransaction(TransactionText); }
		bool bOk = false, bAttemptedMutation = false; FString Error;
		if (Operation == TEXT("interface_add") || Operation == TEXT("interface_remove"))
		{
			FString InterfacePath; Plan->TryGetStringField(TEXT("interface"), InterfacePath);
			if (InterfacePath.IsEmpty()) { Error = TEXT("plan requires interface"); }
			else
			{
				bAttemptedMutation = true;
				bOk = Operation == TEXT("interface_add") ? UBlueprintService::AddInterface(AssetPath, InterfacePath) : UBlueprintService::RemoveInterface(AssetPath, InterfacePath);
				if (bOk) { bOk = UEditorAssetLibrary::SaveAsset(AssetPath, false); if (!bOk) { Error = TEXT("interface changed but save failed"); } }
				else { Error = TEXT("interface mutation or compile failed"); }
			}
		}
		else if (Operation == TEXT("variable_metadata"))
		{
			FString Variable, Category, Description; bool bOverwrite = false;
			if (TargetObject.IsValid()) { TargetObject->TryGetStringField(TEXT("variable"), Variable); TargetObject->TryGetStringField(TEXT("category"), Category); TargetObject->TryGetStringField(TEXT("description"), Description); TargetObject->TryGetBoolField(TEXT("overwrite"), bOverwrite); }
			UBlueprint* Blueprint = LoadBulkBlueprint(AssetPath);
			if (!Blueprint || Variable.IsEmpty()) { Error = TEXT("Blueprint or variable not found"); }
			else
			{
				FString ExistingTooltip; const bool bHasTooltip = FBlueprintEditorUtils::GetBlueprintVariableMetaData(Blueprint, *Variable, nullptr, TEXT("tooltip"), ExistingTooltip);
				const FBPVariableDescription* ExistingVariable = Blueprint->NewVariables.FindByPredicate([&Variable](const FBPVariableDescription& V) { return V.VarName == *Variable; });
				if (!ExistingVariable) { Error = TEXT("variable not found"); }
				else
				{
					const FString ExistingCategory = ExistingVariable->Category.ToString();
					const bool bAuthoredTooltip = bHasTooltip && !ExistingTooltip.TrimStartAndEnd().IsEmpty() &&
						!ExistingTooltip.Equals(Variable, ESearchCase::IgnoreCase);
					const bool bAuthoredCategory = !ExistingCategory.IsEmpty() && !ExistingCategory.Equals(TEXT("Default"), ESearchCase::IgnoreCase);
					if (!bOverwrite && ((!Description.IsEmpty() && bAuthoredTooltip) || (!Category.IsEmpty() && bAuthoredCategory)))
					{
						Error = TEXT("existing authored metadata preserved; set overwrite=true to replace it");
					}
					else
					{
						bAttemptedMutation = true;
						if (!Description.IsEmpty()) { FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, *Variable, nullptr, TEXT("tooltip"), Description); }
						if (!Category.IsEmpty()) { FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, *Variable, nullptr, FText::FromString(Category), true); }
						FKismetEditorUtilities::CompileBlueprint(Blueprint); bOk = Blueprint->Status != BS_Error && UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false);
						if (!bOk) { Error = TEXT("metadata changed but compile/save verification failed"); }
					}
				}
			}
		}
		else if (Operation == TEXT("asset_move"))
		{
			FString Destination; if (TargetObject.IsValid()) { TargetObject->TryGetStringField(TEXT("destination"), Destination); }
			if (Destination.IsEmpty() || !InBulkScope(Destination, Scope)) { Error = TEXT("destination missing or outside path_scope"); }
			else if (UEditorAssetLibrary::DoesAssetExist(Destination)) { Error = TEXT("destination collision"); }
			else { bAttemptedMutation = true; bOk = UEditorAssetLibrary::RenameAsset(AssetPath, Destination); if (!bOk) { Error = TEXT("asset move failed"); } }
		}
		else if (Operation == TEXT("cleanup_review"))
		{
			Error = TEXT("cleanup_review is report-only; deletion requires a separate explicit approved operation");
		}
		else { Error = TEXT("unsupported operation"); }
		if (GEditor) { GEditor->EndTransaction(); if (!bOk && bAttemptedMutation) { GEditor->UndoTransaction(); Item->SetStringField(TEXT("rollback"), TEXT("transaction undone")); } }
		Item->SetBoolField(TEXT("success"), bOk); Item->SetStringField(TEXT("status"), bOk ? TEXT("changed") : TEXT("failed"));
		if (!Error.IsEmpty()) { Item->SetStringField(TEXT("error"), Error); }
		bOk ? ++Succeeded : ++Failed; Results.Add(MakeShared<FJsonValueObject>(Item));
		Report->SetArrayField(TEXT("results"), Results); SaveBulkReport(PlanId, Report);
		if (!bOk && bStopOnError) { break; }
		if ((Index + 1) % Cap == 0) { CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS); }
	}
	Report->SetArrayField(TEXT("results"), Results); Report->SetNumberField(TEXT("succeeded"), Succeeded);
	Report->SetNumberField(TEXT("failed"), Failed); Report->SetNumberField(TEXT("skipped"), Skipped);
	Report->SetBoolField(TEXT("success"), Failed == 0); Report->SetBoolField(TEXT("resumable"), true);
	Report->SetStringField(TEXT("finishedAtIso"), FDateTime::UtcNow().ToIso8601());
	Report->SetStringField(TEXT("reportPath"), FPaths::Combine(BulkDir(), PlanId + TEXT(".json"))); SaveBulkReport(PlanId, Report);
	UWorkflowService::AttachActiveRunArtifact(FPaths::Combine(BulkDir(), PlanId + TEXT(".json")), TEXT("bulk-maintenance"));
	return SerializeBulk(Report);
}
