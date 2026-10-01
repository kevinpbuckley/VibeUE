// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Tools/PythonTools.h"
#include "Tools/PythonTypes.h"
#include "Core/ServiceContext.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Core/ErrorCodes.h"
#include "Utils/VibeUEPythonResultLog.h" // signal-file path for the result JSON (B2 recovery)
#include "HAL/PlatformProcess.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h" // TObjectIterator<UWorld> for the resident-map check
#include "Engine/World.h"
#include "FileHelpers.h" // FEditorFileUtils + UEditorLoadingAndSavingUtils (headless SavePackages)

// Include service headers after PythonTypes
#include "Tools/PythonExecutionService.h"
#include "Tools/PythonDiscoveryService.h"
#include "Tools/PythonSchemaService.h"
#include "WorldPartition/WorldPartition.h"

DEFINE_LOG_CATEGORY_STATIC(LogPythonTools, Log, All);

TArray<FString> UPythonTools::GetResidentMapWorlds()
{
	// A map opened as an ASSET (unreal.load_asset("/Game/Maps/Foo"), EditorAssetLibrary.load_asset,
	// find_object, ...) stays resident afterwards, and a resident map that is not the one currently
	// open makes the engine's own "old level package cleaned up?" check fail the NEXT time any level
	// is loaded. That check is a fatal, not a warning:
	//
	//   EditorServer.cpp:2544  World Memory Leaks: N leaks objects and packages
	//   LogEditorServer: Error: Old level package /Game/Maps/Foo not cleaned up by garbage collection
	//
	// The crash therefore lands minutes later, on whoever calls load_level next, with nothing in the
	// message pointing at the script that actually caused it. Listing the stragglers in the reply of
	// the run that created them turns that into an immediate, attributable warning.
	TArray<FString> Resident;

	if (!GEditor)
	{
		return Resident;
	}

	const UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

	for (TObjectIterator<UWorld> It; It; ++It)
	{
		UWorld* World = *It;
		if (!World || World == EditorWorld || !IsValid(World))
		{
			continue;
		}

		// Only worlds that came from a real map package can block a level load. This drops the
		// transient preview worlds the editor legitimately keeps alive in numbers (Blueprint,
		// material and thumbnail previews all live in the transient package), and PIE worlds, whose
		// lifetime EndPlayMap owns.
		const UPackage* Package = World->GetPackage();
		if (!Package || Package == GetTransientPackage())
		{
			continue;
		}
		if (World->WorldType != EWorldType::Editor && World->WorldType != EWorldType::Inactive)
		{
			continue;
		}
		if (!FPackageName::IsValidLongPackageName(Package->GetName()))
		{
			continue;
		}

		Resident.AddUnique(World->GetPathName());
	}

	return Resident;
}

using namespace VibeUE;

// Track whether the last Python execution crashed (SEH-caught).
// When true, skip auto-save to avoid serializing potentially corrupt assets.
static bool bLastPythonExecutionCrashed = false;

static bool ContainsWorldMutatingPythonPattern(const FString& Code)
{
	const FString CodeLower = Code.ToLower();
	static const TArray<FString> DangerousPatterns = {
		TEXT("destroy_actor("),
		TEXT("delete_landscape("),
		TEXT("create_landscape("),
		TEXT("import_heightmap("),
		TEXT("spawn_actor("),
		TEXT("editoractorsubsystem"),
		TEXT("get_all_level_actors("),
		TEXT("apply_splines_to_landscape(")
	};

	for (const FString& Pattern : DangerousPatterns)
	{
		if (CodeLower.Contains(Pattern))
		{
			return true;
		}
	}

	return false;
}

static bool CanRunWorldMutatingPython(FString& OutReason)
{
	if (!GEditor)
	{
		OutReason = TEXT("Editor is not available yet.");
		return false;
	}

	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	if (!EditorWorld)
	{
		OutReason = TEXT("Editor world is not available yet.");
		return false;
	}

	if (UWorldPartition* WorldPartition = EditorWorld->GetWorldPartition())
	{
		if (!WorldPartition->IsInitialized())
		{
			OutReason = TEXT("World Partition is not initialized yet.");
			return false;
		}

		if (!WorldPartition->GetResolvingDataLayerManager())
		{
			OutReason = TEXT("World Partition data layers are still initializing.");
			return false;
		}
	}

	return true;
}

// Static service instances
static TSharedPtr<FPythonExecutionService> ExecutionServiceInstance;
static TSharedPtr<FPythonDiscoveryService> DiscoveryServiceInstance;
static TSharedPtr<FPythonSchemaService> SchemaServiceInstance;
static TSharedPtr<FServiceContext> ServiceContextInstance;

UPythonTools::UPythonTools()
{
}

TSharedPtr<FPythonExecutionService> UPythonTools::GetExecutionService()
{
	if (!ExecutionServiceInstance.IsValid())
	{
		// Ensure service context exists
		if (!ServiceContextInstance.IsValid())
		{
			ServiceContextInstance = MakeShared<FServiceContext>();
		}
		ExecutionServiceInstance = MakeShared<FPythonExecutionService>(ServiceContextInstance);
		ExecutionServiceInstance->Initialize();
	}
	return ExecutionServiceInstance;
}

TSharedPtr<FPythonDiscoveryService> UPythonTools::GetDiscoveryService()
{
	if (!DiscoveryServiceInstance.IsValid())
	{
		// Ensure service context exists
		if (!ServiceContextInstance.IsValid())
		{
			ServiceContextInstance = MakeShared<FServiceContext>();
		}
		DiscoveryServiceInstance = MakeShared<FPythonDiscoveryService>(ServiceContextInstance, GetExecutionService());
		DiscoveryServiceInstance->Initialize();
	}
	return DiscoveryServiceInstance;
}

TSharedPtr<FPythonSchemaService> UPythonTools::GetSchemaService()
{
	if (!SchemaServiceInstance.IsValid())
	{
		// Ensure service context exists
		if (!ServiceContextInstance.IsValid())
		{
			ServiceContextInstance = MakeShared<FServiceContext>();
		}
		SchemaServiceInstance = MakeShared<FPythonSchemaService>(ServiceContextInstance);
		SchemaServiceInstance->Initialize();
	}
	return SchemaServiceInstance;
}

void UPythonTools::Shutdown()
{
	UE_LOG(LogPythonTools, Log, TEXT("UPythonTools::Shutdown - Releasing Python service instances"));
	
	// Release all Python service instances BEFORE Python shuts down
	// Order matters - discovery depends on execution
	SchemaServiceInstance.Reset();
	DiscoveryServiceInstance.Reset();
	ExecutionServiceInstance.Reset();
	ServiceContextInstance.Reset();
	
	UE_LOG(LogPythonTools, Log, TEXT("UPythonTools::Shutdown - All service instances released"));
}

FString UPythonTools::ExecutePythonCode(const FString& Code, bool bAutoSave)
{
	// Efficient engine readiness check - once ready, never check again
	static bool bEngineReady = false;
	if (!bEngineReady)
	{
		// Check engine, editor, and that we're past initial load phase
		if (!GEngine || !GEditor || GIsInitialLoad)
		{
			return TEXT("Unreal Engine Loading");
		}
		bEngineReady = true;
	}

	if (ContainsWorldMutatingPythonPattern(Code))
	{
		FString BlockReason;
		if (!CanRunWorldMutatingPython(BlockReason))
		{
			TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
			ErrorObj->SetBoolField(TEXT("success"), false);
			ErrorObj->SetStringField(TEXT("error_code"), TEXT("WORLD_NOT_READY"));
			ErrorObj->SetStringField(TEXT("error_message"), FString::Printf(
				TEXT("Blocked world-mutating Python execution: %s Retry in a few seconds after the level finishes loading."),
				*BlockReason));
			FString JsonString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
			FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
			return JsonString;
		}
	}

	// Names of the packages written by the pre-execution auto-save sweep. Reported in the result JSON
	// (saved_packages) so an agent can see, and pass on, exactly what was flushed to disk before the
	// script ran. Empty when auto_save is false, when the sweep is skipped, or when nothing was dirty.
	TArray<FString> SavedPackageNames;

	// Whether the sweep actually ran, and why not when it did not. Reported verbatim in the result
	// JSON: the caller already knows what it PASSED as auto_save, so echoing the argument back tells
	// it nothing — what it cannot otherwise tell is whether its unsaved editor edits reached disk.
	bool bAutoSaveRan = false;
	FString AutoSaveNote;

	// Auto-save all dirty packages (headless) before executing Python code, unless the caller opted
	// out (auto_save=false), the previous run crashed (dirty assets may be corrupt), GEditor is
	// missing, or we're in PIE.
	if (!bAutoSave)
	{
		AutoSaveNote = TEXT("opted_out");
		UE_LOG(LogPythonTools, Verbose, TEXT("Auto-save skipped: auto_save=false — running the script without flushing dirty packages"));
	}
	else
	{
		if (bLastPythonExecutionCrashed)
		{
			AutoSaveNote = TEXT("previous_run_crashed");
			UE_LOG(LogPythonTools, Warning, TEXT("Skipping auto-save: previous Python execution crashed — dirty assets may be corrupt"));
		}
		else if (!GEditor)
		{
			AutoSaveNote = TEXT("editor_unavailable");
			UE_LOG(LogPythonTools, Warning, TEXT("Cannot auto-save: GEditor is not available"));
		}
		// GIsPlayInEditorWorld alone is not enough: the engine sets it only while the PIE world itself
		// ticks (SetPlayInEditorWorld/RestoreEditorWorld), so a call dispatched from editor scope during
		// a PIE or Simulate session sees it false. GEditor->PlayWorld is non-null for the whole session.
		// Same test as the engine's EditorScriptingHelpers::CheckIfInEditorAndPIE.
		else if (GEditor->PlayWorld || GIsPlayInEditorWorld)
		{
			AutoSaveNote = TEXT("pie_active");
			UE_LOG(LogPythonTools, Warning, TEXT("Cannot auto-save: Currently in PIE mode"));
		}
		else
		{
			UE_LOG(LogPythonTools, Verbose, TEXT("Auto-saving dirty packages before Python execution..."));

			// IMPORTANT (issue #433): every call through this path is MCP / tool driven — there is no
			// interactive user. FEditorFileUtils::SaveDirtyPackages can surface a modal PackagesDialog
			// (e.g. on a save warning for a freshly-created asset) which hangs the request, and it can
			// run a Slate thumbnail prepass that stack-overflows on a not-yet-compiled Widget Blueprint
			// (the crash behind issue #435). Use a fully headless package save instead: collect the
			// dirty packages and save them directly, with no dialog and no thumbnail/Slate prepass.
			TArray<UPackage*> DirtyPackages;
			FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);
			FEditorFileUtils::GetDirtyWorldPackages(DirtyPackages);

			// The sweep reached the point of inspecting the editor's dirty set — that is what
			// "it ran" means, whether or not anything was dirty.
			bAutoSaveRan = true;

			if (DirtyPackages.Num() == 0)
			{
				UE_LOG(LogPythonTools, Verbose, TEXT("Auto-save: no dirty packages"));
			}
			else
			{
				// Record the names before the save so the report reflects what the sweep targeted.
				for (const UPackage* DirtyPackage : DirtyPackages)
				{
					if (DirtyPackage)
					{
						SavedPackageNames.Add(DirtyPackage->GetName());
					}
				}

				const bool bSaveSuccess = UEditorLoadingAndSavingUtils::SavePackages(DirtyPackages, /*bOnlyDirty=*/true);
				if (bSaveSuccess)
				{
					UE_LOG(LogPythonTools, Verbose, TEXT("Auto-save completed (headless): %d package(s)"), DirtyPackages.Num());
				}
				else
				{
					// Some or all of the targeted packages did not reach disk. Say so rather than
					// letting SavedPackages imply a clean flush.
					bAutoSaveRan = false;
					AutoSaveNote = TEXT("save_failed");
					UE_LOG(LogPythonTools, Warning, TEXT("Auto-save (headless) completed with warnings or errors"));
				}
			}
		}
	}

	// Attach the auto-save report to any JSON result object returned below, so every reply (success
	// or error) carries auto_save + saved_packages.
	auto AddSaveInfo = [&bAutoSaveRan, &AutoSaveNote, &SavedPackageNames](const TSharedPtr<FJsonObject>& Obj)
	{
		Obj->SetBoolField(TEXT("auto_save"), bAutoSaveRan);
		Obj->SetStringField(TEXT("auto_save_note"), AutoSaveNote);
		TArray<TSharedPtr<FJsonValue>> SavedArray;
		for (const FString& Name : SavedPackageNames)
		{
			SavedArray.Add(MakeShared<FJsonValueString>(Name));
		}
		Obj->SetArrayField(TEXT("saved_packages"), SavedArray);
	};

	auto Service = GetExecutionService();
	if (!Service.IsValid() || !Service.Get())
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), TEXT("PYTHON_SERVICE_UNAVAILABLE"));
		ErrorObj->SetStringField(TEXT("error_message"), TEXT("Python execution service is not available"));
		AddSaveInfo(ErrorObj);
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}

	// Extra safety: verify service context is valid
	if (!ServiceContextInstance.IsValid())
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), TEXT("SERVICE_CONTEXT_INVALID"));
		ErrorObj->SetStringField(TEXT("error_message"), TEXT("Service context is not properly initialized"));
		AddSaveInfo(ErrorObj);
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}

	auto Result = Service->ExecuteCode(Code);

	// Maps this run has left resident (see GetResidentMapWorlds). Computed here, BEFORE the error
	// branch, because the run that strands a map is frequently the same run that raised - that was
	// the shape of the crash this check exists to prevent - so the failing reply must carry it too.
	const TArray<FString> ResidentMaps = GetResidentMapWorlds();
	if (ResidentMaps.Num() > 0)
	{
		UE_LOG(LogPythonTools, Warning,
			TEXT("RESIDENT_MAPS: %d map(s) other than the open level are loaded in memory (%s). Loading any level while they are resident ")
			TEXT("fails the engine's stale-world check and TAKES THE EDITOR DOWN (EditorServer.cpp 'World Memory Leaks'). A map package loads ")
			TEXT("RF_Standalone, and neither collect_garbage() nor EditorLoadingAndSavingUtils.unload_packages() releases it (both verified) - ")
			TEXT("so RESTART THE EDITOR before the next level change, and do not open a map as an asset: read map metadata from the asset ")
			TEXT("registry, and change level with LevelEditorSubsystem.load_level."),
			ResidentMaps.Num(), *FString::Join(ResidentMaps, TEXT(", ")));
	}

	auto AddResidentInfo = [&ResidentMaps](const TSharedPtr<FJsonObject>& Obj)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& MapPath : ResidentMaps)
		{
			Arr.Add(MakeShared<FJsonValueString>(MapPath));
		}
		Obj->SetArrayField(TEXT("resident_maps"), Arr);
	};

	if (Result.IsError())
	{
		// Track crash state so the next auto-save is skipped (the editor may hold half-mutated
		// objects). ONLY a real SEH crash counts: this used to trigger on PYTHON_RUNTIME_ERROR, which
		// is also what an ordinary Python traceback returns, so a trivial AttributeError silently
		// disabled auto-save for the following call (issue #608). A caught exception corrupts
		// nothing — the interpreter handled it and the editor is fine.
		if (Result.GetErrorCode() == FString(ErrorCodes::PYTHON_EDITOR_CRASH))
		{
			bLastPythonExecutionCrashed = true;
		}

		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), Result.GetErrorCode());
		ErrorObj->SetStringField(TEXT("error_message"), Result.GetErrorMessage());
		AddSaveInfo(ErrorObj);
		AddResidentInfo(ErrorObj);
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}

	// Successful execution — safe to auto-save again
	bLastPythonExecutionCrashed = false;

	// Carry the auto-save report through to the success JSON alongside the execution result.
	FPythonExecutionResult Value = Result.GetValue();
	Value.bAutoSave = bAutoSaveRan;
	Value.AutoSaveNote = AutoSaveNote;
	Value.SavedPackages = SavedPackageNames;
	Value.ResidentMaps = ResidentMaps;
	return ConvertExecutionResultToJson(Value);
}

FString UPythonTools::DiscoverPythonModule(const FString& ModuleName)
{
	auto Service = GetDiscoveryService();
	if (!Service.IsValid())
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), TEXT("PYTHON_SERVICE_UNAVAILABLE"));
		ErrorObj->SetStringField(TEXT("error_message"), TEXT("Python discovery service is not available"));
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}

	// Use max_depth=1 and empty filter as defaults
	auto Result = Service->DiscoverUnrealModule(1, TEXT(""));

	if (Result.IsError())
	{
		// Return error as JSON
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), Result.GetErrorCode());
		ErrorObj->SetStringField(TEXT("error_message"), Result.GetErrorMessage());

		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}

	return ConvertModuleInfoToJson(Result.GetValue());
}

FString UPythonTools::DiscoverPythonClass(const FString& ClassName)
{
	auto Service = GetDiscoveryService();
	if (!Service.IsValid())
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), TEXT("PYTHON_SERVICE_UNAVAILABLE"));
		ErrorObj->SetStringField(TEXT("error_message"), TEXT("Python discovery service is not available"));
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}
	
	auto Result = Service->DiscoverClass(ClassName);

	if (Result.IsError())
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), Result.GetErrorCode());
		ErrorObj->SetStringField(TEXT("error_message"), Result.GetErrorMessage());

		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}

	return ConvertClassInfoToJson(Result.GetValue());
}

FString UPythonTools::DiscoverPythonFunction(const FString& FunctionName)
{
	auto Service = GetDiscoveryService();
	if (!Service.IsValid())
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), TEXT("PYTHON_SERVICE_UNAVAILABLE"));
		ErrorObj->SetStringField(TEXT("error_message"), TEXT("Python discovery service is not available"));
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}
	
	auto Result = Service->DiscoverFunction(FunctionName);

	if (Result.IsError())
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), Result.GetErrorCode());
		ErrorObj->SetStringField(TEXT("error_message"), Result.GetErrorMessage());

		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}

	return ConvertFunctionInfoToJson(Result.GetValue());
}

FString UPythonTools::ListPythonSubsystems()
{
	auto Service = GetDiscoveryService();
	if (!Service.IsValid())
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), TEXT("PYTHON_SERVICE_UNAVAILABLE"));
		ErrorObj->SetStringField(TEXT("error_message"), TEXT("Python discovery service is not available"));
		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}
	
	auto Result = Service->ListEditorSubsystems();

	if (Result.IsError())
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), Result.GetErrorCode());
		ErrorObj->SetStringField(TEXT("error_message"), Result.GetErrorMessage());

		FString JsonString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
		FJsonSerializer::Serialize(ErrorObj.ToSharedRef(), Writer);
		return JsonString;
	}

	// Convert subsystems array to JSON
	TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
	ResponseObj->SetBoolField(TEXT("success"), true);

	TArray<TSharedPtr<FJsonValue>> SubsystemsArray;
	for (const FString& Subsystem : Result.GetValue())
	{
		SubsystemsArray.Add(MakeShared<FJsonValueString>(Subsystem));
	}
	ResponseObj->SetArrayField(TEXT("subsystems"), SubsystemsArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(ResponseObj.ToSharedRef(), Writer);
	return JsonString;
}

FString UPythonTools::ConvertExecutionResultToJson(const VibeUE::FPythonExecutionResult& Result)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	JsonObj->SetBoolField(TEXT("success"), Result.bSuccess);
	// run_id lets a client correlate this reply with the persisted python-<pid>-last.json / -runs.jsonl
	// record; on a call that timed out, recover the result with vibeue.last_python_result() (B2).
	JsonObj->SetNumberField(TEXT("run_id"), static_cast<double>(Result.RunId));
	JsonObj->SetStringField(TEXT("output"), Result.Output);
	JsonObj->SetStringField(TEXT("result"), Result.Result);
	
	// Only include error field if there's an actual error message
	if (!Result.ErrorMessage.IsEmpty())
	{
		JsonObj->SetStringField(TEXT("error"), Result.ErrorMessage);
	}

	JsonObj->SetNumberField(TEXT("execution_time_ms"), Result.ExecutionTimeMs);

	// timed_out is true when the run finished successfully but overran the client timeout; the payload
	// is still valid. signal_file_path is where this run's outcome is persisted, so a client that gave
	// up can recover it with vibeue.last_python_result() (B2).
	JsonObj->SetBoolField(TEXT("timed_out"), Result.bTimedOut);
	JsonObj->SetStringField(TEXT("signal_file_path"),
		FVibeUEPythonResultLog::GetLastResultPathForPid(FPlatformProcess::GetCurrentProcessId()));

	// Auto-save report (issue #433 follow-up): whether the pre-execution sweep ran and what it wrote.
	// auto_save is the OUTCOME, not an echo of the argument — auto_save_note names the reason
	// whenever it is false, so "opted out" is never confused with "ran, nothing was dirty".
	JsonObj->SetBoolField(TEXT("auto_save"), Result.bAutoSave);
	JsonObj->SetStringField(TEXT("auto_save_note"), Result.AutoSaveNote);

	// Maps left loaded in memory besides the open level. Non-empty means the NEXT level load will
	// fatal the editor on the engine's stale-world check, so this is a hard warning, not trivia.
	TArray<TSharedPtr<FJsonValue>> ResidentArray;
	for (const FString& MapPath : Result.ResidentMaps)
	{
		ResidentArray.Add(MakeShared<FJsonValueString>(MapPath));
	}
	JsonObj->SetArrayField(TEXT("resident_maps"), ResidentArray);
	TArray<TSharedPtr<FJsonValue>> SavedArray;
	for (const FString& Name : Result.SavedPackages)
	{
		SavedArray.Add(MakeShared<FJsonValueString>(Name));
	}
	JsonObj->SetArrayField(TEXT("saved_packages"), SavedArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	return JsonString;
}

FString UPythonTools::ConvertModuleInfoToJson(const VibeUE::FPythonModuleInfo& Info)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	JsonObj->SetBoolField(TEXT("success"), true);
	JsonObj->SetStringField(TEXT("module_name"), Info.ModuleName);

	// Convert classes array
	TArray<TSharedPtr<FJsonValue>> ClassesArray;
	for (const FString& ClassName : Info.Classes)
	{
		ClassesArray.Add(MakeShared<FJsonValueString>(ClassName));
	}
	JsonObj->SetArrayField(TEXT("classes"), ClassesArray);

	// Convert functions array
	TArray<TSharedPtr<FJsonValue>> FunctionsArray;
	for (const FString& FunctionName : Info.Functions)
	{
		FunctionsArray.Add(MakeShared<FJsonValueString>(FunctionName));
	}
	JsonObj->SetArrayField(TEXT("functions"), FunctionsArray);

	// Convert constants array
	TArray<TSharedPtr<FJsonValue>> ConstantsArray;
	for (const FString& ConstantName : Info.Constants)
	{
		ConstantsArray.Add(MakeShared<FJsonValueString>(ConstantName));
	}
	JsonObj->SetArrayField(TEXT("constants"), ConstantsArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	return JsonString;
}

FString UPythonTools::ConvertClassInfoToJson(const VibeUE::FPythonClassInfo& Info)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	JsonObj->SetBoolField(TEXT("success"), true);
	JsonObj->SetStringField(TEXT("class_name"), Info.Name);
	JsonObj->SetStringField(TEXT("full_path"), Info.FullPath);
	JsonObj->SetStringField(TEXT("doc_string"), Info.Docstring);

	// Convert base classes
	TArray<TSharedPtr<FJsonValue>> BasesArray;
	for (const FString& BaseName : Info.BaseClasses)
	{
		BasesArray.Add(MakeShared<FJsonValueString>(BaseName));
	}
	JsonObj->SetArrayField(TEXT("base_classes"), BasesArray);

	// Convert methods - need to serialize FPythonFunctionInfo structs
	TArray<TSharedPtr<FJsonValue>> MethodsArray;
	for (const VibeUE::FPythonFunctionInfo& Method : Info.Methods)
	{
		TSharedPtr<FJsonObject> MethodObj = MakeShared<FJsonObject>();
		MethodObj->SetStringField(TEXT("name"), Method.Name);
		MethodObj->SetStringField(TEXT("signature"), Method.Signature);
		MethodObj->SetStringField(TEXT("docstring"), Method.Docstring);
		MethodsArray.Add(MakeShared<FJsonValueObject>(MethodObj));
	}
	JsonObj->SetArrayField(TEXT("methods"), MethodsArray);

	// Convert properties
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (const FString& PropName : Info.Properties)
	{
		PropertiesArray.Add(MakeShared<FJsonValueString>(PropName));
	}
	JsonObj->SetArrayField(TEXT("properties"), PropertiesArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	return JsonString;
}

FString UPythonTools::ConvertFunctionInfoToJson(const VibeUE::FPythonFunctionInfo& Info)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();
	JsonObj->SetBoolField(TEXT("success"), true);
	JsonObj->SetStringField(TEXT("function_name"), Info.Name);
	JsonObj->SetStringField(TEXT("signature"), Info.Signature);
	JsonObj->SetStringField(TEXT("doc_string"), Info.Docstring);

	// Convert parameters
	TArray<TSharedPtr<FJsonValue>> ParamsArray;
	for (const FString& ParamName : Info.Parameters)
	{
		ParamsArray.Add(MakeShared<FJsonValueString>(ParamName));
	}
	JsonObj->SetArrayField(TEXT("parameters"), ParamsArray);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
	return JsonString;
}
