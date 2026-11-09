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
#include "Engine/Level.h"          // ULevel::OwningWorld, for the sublevel filter in GetResidentMaps
#include "Engine/LevelStreaming.h" // ULevelStreaming::GetLoadedLevel
#include "FileHelpers.h" // FEditorFileUtils + UEditorLoadingAndSavingUtils (headless SavePackages)
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "UObject/GarbageCollection.h"

// Include service headers after PythonTypes
#include "Tools/PythonExecutionService.h"
#include "Settings/VibeUEEditorSettings.h"
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

TArray<FString> UPythonTools::GetResidentMaps()
{
	// Narrower than the resident_maps reply field (which keeps GetResidentMapWorlds' rule unchanged):
	// a legacy streaming sublevel of the open map is a separate map package that is resident by design,
	// and the level change unloads it with its persistent world. Reporting it here would make
	// vibeue.safe_load_level refuse forever on any map that has sublevels.
	TArray<FString> Resident = GetResidentMapWorlds();
	const UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!EditorWorld || Resident.Num() == 0)
	{
		return Resident;
	}

	TSet<FString> Sublevels;
	for (const ULevelStreaming* Streaming : EditorWorld->GetStreamingLevels())
	{
		const ULevel* Loaded = Streaming ? Streaming->GetLoadedLevel() : nullptr;
		const UWorld* SublevelWorld = Loaded ? Loaded->GetTypedOuter<UWorld>() : nullptr;
		if (SublevelWorld && SublevelWorld != EditorWorld)
		{
			Sublevels.Add(SublevelWorld->GetPathName());
		}
	}
	for (TObjectIterator<UWorld> It; It; ++It)
	{
		const UWorld* World = *It;
		if (World && World != EditorWorld && World->PersistentLevel && World->PersistentLevel->OwningWorld == EditorWorld)
		{
			Sublevels.Add(World->GetPathName());
		}
	}
	Resident.RemoveAll([&Sublevels](const FString& Path) { return Sublevels.Contains(Path); });
	return Resident;
}

TArray<FString> UPythonTools::GetDirtyPackages(bool bIncludeMaps)
{
	TArray<FString> Names;
	if (!GEditor)
	{
		return Names;
	}

	// The same enumeration as the pre-run auto-save sweep, so this is exactly what an auto_save=true
	// call would write.
	TArray<UPackage*> DirtyPackages;
	FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);
	if (bIncludeMaps)
	{
		FEditorFileUtils::GetDirtyWorldPackages(DirtyPackages);
	}

	// A TSet, not AddUnique: a World Partition map can have thousands of dirty external-actor packages,
	// and this runs twice per execute_python_code call.
	TSet<FString> Unique;
	Unique.Reserve(DirtyPackages.Num());
	for (const UPackage* DirtyPackage : DirtyPackages)
	{
		if (DirtyPackage)
		{
			Unique.Add(DirtyPackage->GetName());
		}
	}
	Names = Unique.Array();
	Names.Sort();
	return Names;
}

void UPythonTools::ComputeDirtyPackageDelta(const TArray<FString>& Before, const TArray<FString>& After,
	TArray<FString>& OutDirtied, TArray<FString>& OutCleaned)
{
	OutDirtied.Reset();
	OutCleaned.Reset();

	const TSet<FString> BeforeSet(Before);
	const TSet<FString> AfterSet(After);
	for (const FString& Name : AfterSet)
	{
		if (!BeforeSet.Contains(Name))
		{
			OutDirtied.Add(Name);
		}
	}
	for (const FString& Name : BeforeSet)
	{
		if (!AfterSet.Contains(Name))
		{
			OutCleaned.Add(Name);
		}
	}
	OutDirtied.Sort();
	OutCleaned.Sort();
}

using namespace VibeUE;

// Write a string list as a JSON array field (an empty list still writes the key: reply fields are
// always present, so a caller can index them without a presence check).
static void SetStringArrayField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* FieldName, const TArray<FString>& Values)
{
	TArray<TSharedPtr<FJsonValue>> Arr;
	Arr.Reserve(Values.Num());
	for (const FString& Value : Values)
	{
		Arr.Add(MakeShared<FJsonValueString>(Value));
	}
	Obj->SetArrayField(FieldName, Arr);
}

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

static TSharedPtr<FPythonExecutionService> GetOrCreateExecutionService()
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

namespace
{
	/** What a run reports besides the script's own outcome. Filled before the script starts. */
	struct FPythonRunContext
	{
		// Names of the packages written by the pre-execution auto-save sweep. Reported in the result JSON
		// (saved_packages) so an agent can see, and pass on, exactly what was flushed to disk before the
		// script ran. Empty when auto_save is false, when the sweep is skipped, or when nothing was dirty.
		TArray<FString> SavedPackageNames;

		// Whether the sweep actually ran, and why not when it did not. Reported verbatim in the result
		// JSON: the caller already knows what it PASSED as auto_save, so echoing the argument back tells
		// it nothing — what it cannot otherwise tell is whether its unsaved editor edits reached disk.
		bool bAutoSaveRan = false;
		FString AutoSaveNote;

		TArray<FString> DirtyBeforeRun;
	};

	/** A script paused at a top-level await, which the core ticker advances once per frame (TickAwaitRuns). */
	struct FPythonAwaitRun
	{
		FPythonRunContext Context;
		int64 RunId = 0;
		FString CancelKey;
		FDateTime StartedUtc;
		double StartSeconds = 0.0;
		// Everything the script has printed so far, each step's output appended, so its end reads as one command's.
		FPythonCommandEx Output;
		TFunction<void(const FString&)> OnDone;
		bool bCancelRequested = false;
		bool bCancelSent = false;
	};

	constexpr int32 MaxAwaitRuns = 8;
	TArray<TUniquePtr<FPythonAwaitRun>> GAwaitRuns; // Game thread only
	FTSTicker::FDelegateHandle GAwaitTickerHandle;
	bool bSteppingAwaitRuns = false;
	int32 GSyncPythonRunDepth = 0;
}

static FString SerializeReply(const TSharedPtr<FJsonObject>& Obj)
{
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return JsonString;
}

// Attach the auto-save report to a JSON reply, so every reply (success or error) carries auto_save +
// saved_packages.
static void AddSaveInfo(const FPythonRunContext& Run, const TSharedPtr<FJsonObject>& Obj)
{
	Obj->SetBoolField(TEXT("auto_save"), Run.bAutoSaveRan);
	Obj->SetStringField(TEXT("auto_save_note"), Run.AutoSaveNote);
	SetStringArrayField(Obj, TEXT("saved_packages"), Run.SavedPackageNames);
}

UPythonTools::UPythonTools()
{
}

TSharedPtr<FPythonExecutionService> UPythonTools::GetExecutionService()
{
	return GetOrCreateExecutionService();
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

// The sweep and the snapshot before a script, shared by both ways of running one. False means the
// script must not run, and OutEarlyReply is the reply.
static bool BeginPythonRun(const FString& Code, bool bAutoSave, FPythonRunContext& Run, FString& OutEarlyReply)
{
	// Efficient engine readiness check - once ready, never check again
	static bool bEngineReady = false;
	if (!bEngineReady)
	{
		// Check engine, editor, and that we're past initial load phase
		if (!GEngine || !GEditor || GIsInitialLoad)
		{
			OutEarlyReply = TEXT("Unreal Engine Loading");
			return false;
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
			// Nothing ran, so nothing was dirtied or cleaned; the keys are still present.
			SetStringArrayField(ErrorObj, TEXT("dirtied_packages"), TArray<FString>());
			SetStringArrayField(ErrorObj, TEXT("cleaned_packages"), TArray<FString>());
			OutEarlyReply = SerializeReply(ErrorObj);
			return false;
		}
	}

	// Auto-save all dirty packages (headless) before executing Python code, unless the caller opted
	// out (auto_save=false), the previous run crashed (dirty assets may be corrupt), GEditor is
	// missing, we're in PIE, or a script is paused at an await (its edits are half done).
	if (!bAutoSave)
	{
		Run.AutoSaveNote = TEXT("opted_out");
		UE_LOG(LogPythonTools, Verbose, TEXT("Auto-save skipped: auto_save=false — running the script without flushing dirty packages"));
	}
	else if (GetDefault<UVibeUEEditorSettings>()->bDisablePythonAutoSave)
	{
		Run.AutoSaveNote = TEXT("disabled_by_setting");
		UE_LOG(LogPythonTools, Verbose, TEXT("Auto-save skipped: disabled in Editor Preferences > Plugins > VibeUE"));
	}
	else
	{
		if (bLastPythonExecutionCrashed)
		{
			Run.AutoSaveNote = TEXT("previous_run_crashed");
			UE_LOG(LogPythonTools, Warning, TEXT("Skipping auto-save: previous Python execution crashed — dirty assets may be corrupt"));
		}
		else if (GAwaitRuns.Num() > 0)
		{
			Run.AutoSaveNote = TEXT("await_run_in_progress");
			UE_LOG(LogPythonTools, Verbose, TEXT("Skipping auto-save: %d script(s) paused at an await"), GAwaitRuns.Num());
		}
		else if (!GEditor)
		{
			Run.AutoSaveNote = TEXT("editor_unavailable");
			UE_LOG(LogPythonTools, Warning, TEXT("Cannot auto-save: GEditor is not available"));
		}
		// GIsPlayInEditorWorld alone is not enough: the engine sets it only while the PIE world itself
		// ticks (SetPlayInEditorWorld/RestoreEditorWorld), so a call dispatched from editor scope during
		// a PIE or Simulate session sees it false. GEditor->PlayWorld is non-null for the whole session.
		// Same test as the engine's EditorScriptingHelpers::CheckIfInEditorAndPIE.
		else if (GEditor->PlayWorld || GIsPlayInEditorWorld)
		{
			Run.AutoSaveNote = TEXT("pie_active");
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
			Run.bAutoSaveRan = true;

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
						Run.SavedPackageNames.Add(DirtyPackage->GetName());
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
					Run.bAutoSaveRan = false;
					Run.AutoSaveNote = TEXT("save_failed");
					UE_LOG(LogPythonTools, Warning, TEXT("Auto-save (headless) completed with warnings or errors"));
				}
			}
		}
	}

	auto RefuseWith = [&Run, &OutEarlyReply](const TCHAR* ErrorCode, const TCHAR* Message)
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), ErrorCode);
		ErrorObj->SetStringField(TEXT("error_message"), Message);
		AddSaveInfo(Run, ErrorObj);
		SetStringArrayField(ErrorObj, TEXT("dirtied_packages"), TArray<FString>());
		SetStringArrayField(ErrorObj, TEXT("cleaned_packages"), TArray<FString>());
		OutEarlyReply = SerializeReply(ErrorObj);
		return false;
	};

	TSharedPtr<FPythonExecutionService> Service = GetOrCreateExecutionService();
	if (!Service.IsValid() || !Service.Get())
	{
		return RefuseWith(TEXT("PYTHON_SERVICE_UNAVAILABLE"), TEXT("Python execution service is not available"));
	}

	// Extra safety: verify service context is valid
	if (!ServiceContextInstance.IsValid())
	{
		return RefuseWith(TEXT("SERVICE_CONTEXT_INVALID"), TEXT("Service context is not properly initialized"));
	}

	// Snapshot the dirty set immediately around the script. The auto-save sweep above has already
	// run, so its saves are not attributed to the script, and dirt that predates this call never shows
	// up as "dirtied". Taken on the error path too: a script that raised half-way through an edit is
	// exactly the one whose stray dirt a later auto_save=true call would otherwise flush to disk.
	Run.DirtyBeforeRun = UPythonTools::GetDirtyPackages(/*bIncludeMaps=*/true);
	return true;
}

// The reply for a script that has ended, with what it left behind.
static FString FinishPythonRun(const FPythonRunContext& Run, const TResult<FPythonExecutionResult>& Result)
{
	TArray<FString> DirtiedPackageNames;
	TArray<FString> CleanedPackageNames;

	// After an SEH-caught crash the editor may hold half-torn objects, and the after-snapshot walks every
	// UPackage outside the crash guard - so skip it and report empty lists rather than risk a second crash.
	if (!(Result.IsError() && Result.GetErrorCode() == FString(ErrorCodes::PYTHON_EDITOR_CRASH)))
	{
		UPythonTools::ComputeDirtyPackageDelta(Run.DirtyBeforeRun, UPythonTools::GetDirtyPackages(/*bIncludeMaps=*/true), DirtiedPackageNames, CleanedPackageNames);
	}

	// Maps this run has left resident (see GetResidentMapWorlds). Computed here, BEFORE the error
	// branch, because the run that strands a map is frequently the same run that raised - that was
	// the shape of the crash this check exists to prevent - so the failing reply must carry it too.
	const TArray<FString> ResidentMaps = UPythonTools::GetResidentMapWorlds();
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
		AddSaveInfo(Run, ErrorObj);
		SetStringArrayField(ErrorObj, TEXT("resident_maps"), ResidentMaps);
		SetStringArrayField(ErrorObj, TEXT("dirtied_packages"), DirtiedPackageNames);
		SetStringArrayField(ErrorObj, TEXT("cleaned_packages"), CleanedPackageNames);
		return SerializeReply(ErrorObj);
	}

	// Successful execution — safe to auto-save again
	bLastPythonExecutionCrashed = false;

	// Carry the auto-save report through to the success JSON alongside the execution result.
	FPythonExecutionResult Value = Result.GetValue();
	Value.bAutoSave = Run.bAutoSaveRan;
	Value.AutoSaveNote = Run.AutoSaveNote;
	Value.SavedPackages = Run.SavedPackageNames;
	Value.ResidentMaps = ResidentMaps;
	Value.DirtiedPackages = DirtiedPackageNames;
	Value.CleanedPackages = CleanedPackageNames;
	return UPythonTools::ConvertExecutionResultToJson(Value);
}

FString UPythonTools::ExecutePythonCode(const FString& Code, bool bAutoSave, bool bFreshNamespace)
{
	FPythonRunContext Run;
	FString EarlyReply;
	if (!BeginPythonRun(Code, bAutoSave, Run, EarlyReply))
	{
		return EarlyReply;
	}

	++GSyncPythonRunDepth;
	const TResult<FPythonExecutionResult> Result = GetExecutionService()->ExecuteCode(Code, EPythonFileExecutionScope::Private, 30000, bFreshNamespace);
	--GSyncPythonRunDepth;
	return FinishPythonRun(Run, Result);
}

// One await script's end: its outcome is recorded like any other run's, and its caller answered.
static void EndAwaitRun(FPythonExecutionService& Service, FPythonAwaitRun& Run, bool bSuccess, bool bCrashed, const FString& CrashMessage, bool bCancelled)
{
	const double ExecutionTimeMs = (FPlatformTime::Seconds() - Run.StartSeconds) * 1000.0;
	const TResult<FPythonExecutionResult> Result = Service.RecordRunOutcome(
		Run.Output, bSuccess, bCrashed, CrashMessage, ExecutionTimeMs, Run.RunId, Run.StartedUtc, /*TimeoutMs=*/0, bCancelled);
	Run.OnDone(FinishPythonRun(Run.Context, Result));
}

// One iteration of one await script. True when it has ended and its caller has been answered.
static bool StepAwaitRun(FPythonExecutionService& Service, FPythonAwaitRun& Run)
{
	double IgnoredMs = 0.0;
	bool bCrashed = false;
	FString CrashMessage;

	if (Run.bCancelRequested && !Run.bCancelSent)
	{
		Run.bCancelSent = true;
		FPythonCommandEx CancelCommand;
		CancelCommand.Command = FPythonExecutionService::MakeAwaitRunCommand(TEXT("cancel"), Run.RunId);
		CancelCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
		CancelCommand.Flags = EPythonCommandFlags::None;
		Service.RunGuardedCommand(CancelCommand, IgnoredMs, bCrashed, CrashMessage);
	}

	FPythonCommandEx Command;
	bool bSuccess = false;
	if (!bCrashed)
	{
		Command.Command = FPythonExecutionService::MakeAwaitRunCommand(TEXT("step"), Run.RunId);
		Command.ExecutionMode = EPythonCommandExecutionMode::EvaluateStatement;
		Command.Flags = EPythonCommandFlags::None;
		bSuccess = Service.RunGuardedCommand(Command, IgnoredMs, bCrashed, CrashMessage);
		Run.Output.LogOutput.Append(Command.LogOutput);
	}

	if (!bCrashed && bSuccess && Command.CommandResult.Contains(TEXT("running")))
	{
		return false;
	}

	if (bCrashed || !bSuccess)
	{
		// The traceback, which RecordRunOutcome reads as it does a one-command run's.
		Run.Output.CommandResult = Command.CommandResult;

		FPythonCommandEx DiscardCommand;
		DiscardCommand.Command = FPythonExecutionService::MakeAwaitRunCommand(TEXT("discard"), Run.RunId);
		DiscardCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
		DiscardCommand.Flags = EPythonCommandFlags::None;
		bool bDiscardCrashed = false;
		FString IgnoredMessage;
		Service.RunGuardedCommand(DiscardCommand, IgnoredMs, bDiscardCrashed, IgnoredMessage);
	}

	const bool bCancelled = !bCrashed && bSuccess && Command.CommandResult.Contains(TEXT("cancelled"));
	EndAwaitRun(Service, Run, bSuccess, bCrashed, CrashMessage, bCancelled);
	return true;
}

// Each frame on the core ticker, where a script may do what it may do in a one-command run (a task on
// the game thread, by contrast, may not load or import). A script stays paused while anything else has
// the editor in a state it could not safely run in: another Python run pumping the ticker from inside,
// garbage collection, a package save, a slow task or a modal dialog.
static bool TickAwaitRuns(float /*DeltaTime*/)
{
	const bool bModal = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetActiveModalWindow().IsValid();
	if (bSteppingAwaitRuns || GSyncPythonRunDepth > 0 || IsGarbageCollecting() || UE::IsSavingPackage() || GIsSlowTask || bModal)
	{
		return true;
	}

	if (ExecutionServiceInstance.IsValid())
	{
		TGuardValue<bool> Stepping(bSteppingAwaitRuns, true);
		TSharedPtr<FPythonExecutionService> Service = ExecutionServiceInstance;
		for (int32 Index = GAwaitRuns.Num() - 1; Index >= 0; --Index)
		{
			if (StepAwaitRun(*Service, *GAwaitRuns[Index]))
			{
				GAwaitRuns.RemoveAt(Index);
			}
		}
	}

	if (GAwaitRuns.Num() == 0)
	{
		GAwaitTickerHandle.Reset();
		return false;
	}
	return true;
}

bool UPythonTools::NeedsAwaitRun(const FString& Code)
{
	// Almost no script mentions either word, and those that do not stay one command cheaper.
	if (!Code.Contains(TEXT("await")) && !Code.Contains(TEXT("async")))
	{
		return false;
	}

	TSharedPtr<FPythonExecutionService> Service = GetExecutionService();
	if (!Service.IsValid() || Service->IsPythonAvailable().IsError())
	{
		return false;
	}

	FPythonCommandEx Command;
	Command.Command = FPythonExecutionService::MakeNeedsAwaitCommand(Code);
	Command.ExecutionMode = EPythonCommandExecutionMode::EvaluateStatement;
	Command.Flags = EPythonCommandFlags::None;
	double IgnoredMs = 0.0;
	bool bCrashed = false;
	FString IgnoredMessage;
	const bool bSuccess = Service->RunGuardedCommand(Command, IgnoredMs, bCrashed, IgnoredMessage);
	return bSuccess && !bCrashed && Command.CommandResult.TrimStartAndEnd() == TEXT("True");
}

void UPythonTools::StartAwaitRun(const FString& Code, bool bAutoSave, bool bFreshNamespace, const FString& CancelKey, TFunction<void(const FString&)> OnDone)
{
	check(IsInGameThread());

	if (GAwaitRuns.Num() >= MaxAwaitRuns)
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), TEXT("BUSY"));
		ErrorObj->SetStringField(TEXT("error_message"), FString::Printf(
			TEXT("The script was not run: %d scripts are already paused at an await, the most the editor takes at once. Try again when one has finished."),
			MaxAwaitRuns));
		OnDone(SerializeReply(ErrorObj));
		return;
	}

	TUniquePtr<FPythonAwaitRun> Run = MakeUnique<FPythonAwaitRun>();
	FString EarlyReply;
	if (!BeginPythonRun(Code, bAutoSave, Run->Context, EarlyReply))
	{
		OnDone(EarlyReply);
		return;
	}

	Run->OnDone = MoveTemp(OnDone);
	FString BlockedPattern;
	FString BlockedReason;
	if (FPythonExecutionService::ContainsUnsafePattern(Code, BlockedPattern, BlockedReason))
	{
		Run->OnDone(FinishPythonRun(Run->Context, TResult<FPythonExecutionResult>::Error(
			ErrorCodes::PYTHON_UNSAFE_CODE,
			FString::Printf(TEXT("Blocked unsafe Python code: %s. %s"), *BlockedPattern, *BlockedReason))));
		return;
	}

	Run->RunId = FVibeUEPythonResultLog::NextRunId();
	Run->CancelKey = CancelKey;
	Run->StartedUtc = FDateTime::UtcNow();
	Run->StartSeconds = FPlatformTime::Seconds();

	TSharedPtr<FPythonExecutionService> Service = GetExecutionService();
	FPythonCommandEx Command;
	Command.Command = FPythonExecutionService::MakeAwaitBeginCommand(Run->RunId, Code, bFreshNamespace);
	Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	Command.FileExecutionScope = EPythonFileExecutionScope::Private;
	Command.Flags = EPythonCommandFlags::None;
	double IgnoredMs = 0.0;
	bool bCrashed = false;
	FString CrashMessage;
	const bool bSuccess = Service->RunGuardedCommand(Command, IgnoredMs, bCrashed, CrashMessage);
	Run->Output.LogOutput.Append(Command.LogOutput);
	if (bCrashed || !bSuccess)
	{
		Run->Output.CommandResult = Command.CommandResult;
		EndAwaitRun(*Service, *Run, bSuccess, bCrashed, CrashMessage, /*bCancelled=*/false);
		return;
	}

	GAwaitRuns.Add(MoveTemp(Run));
	if (!GAwaitTickerHandle.IsValid())
	{
		GAwaitTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickAwaitRuns), 0.0f);
	}
}

void UPythonTools::CancelAwaitRuns(const FString& CancelKey)
{
	check(IsInGameThread());
	for (const TUniquePtr<FPythonAwaitRun>& Run : GAwaitRuns)
	{
		if (Run->CancelKey == CancelKey)
		{
			Run->bCancelRequested = true;
		}
	}
}

void UPythonTools::AbandonAwaitRuns()
{
	check(IsInGameThread());
	if (GAwaitTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GAwaitTickerHandle);
		GAwaitTickerHandle.Reset();
	}

	// Called at editor exit, when Python may be partly shut down, so the scripts are not told: they end
	// with the interpreter. Their callers still get an answer.
	TArray<TUniquePtr<FPythonAwaitRun>> Abandoned = MoveTemp(GAwaitRuns);
	GAwaitRuns.Reset();
	for (const TUniquePtr<FPythonAwaitRun>& Run : Abandoned)
	{
		TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
		ErrorObj->SetBoolField(TEXT("success"), false);
		ErrorObj->SetStringField(TEXT("error_code"), ErrorCodes::OPERATION_CANCELLED);
		ErrorObj->SetStringField(TEXT("error_message"), TEXT("The editor is shutting down; the script stopped at an await."));
		Run->OnDone(SerializeReply(ErrorObj));
	}
}

int32 UPythonTools::GetAwaitRunCount()
{
	return GAwaitRuns.Num();
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

	// What the script itself dirtied and cleaned (snapshots taken after the auto-save sweep).
	SetStringArrayField(JsonObj, TEXT("dirtied_packages"), Result.DirtiedPackages);
	SetStringArrayField(JsonObj, TEXT("cleaned_packages"), Result.CleanedPackages);

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
