// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UNiagaraService.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraComponent.h"
#include "NiagaraTypes.h"
#include "NiagaraParameterStore.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraEffectType.h"
#include "NiagaraScript.h"
#include "NiagaraEditorModule.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraOverviewNode.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"

// =================================================================
// Helper Methods
// =================================================================

UNiagaraSystem* UNiagaraService::LoadNiagaraSystem(const FString& SystemPath)
{
	if (SystemPath.IsEmpty())
	{
		return nullptr;
	}

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(SystemPath);
	if (!LoadedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService: Failed to load Niagara system: %s"), *SystemPath);
		return nullptr;
	}

	UNiagaraSystem* System = Cast<UNiagaraSystem>(LoadedObject);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService: Object is not a Niagara system: %s"), *SystemPath);
		return nullptr;
	}

	return System;
}

UNiagaraEmitter* UNiagaraService::LoadNiagaraEmitter(const FString& EmitterPath)
{
	if (EmitterPath.IsEmpty())
	{
		return nullptr;
	}

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(EmitterPath);
	if (!LoadedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService: Failed to load Niagara emitter: %s"), *EmitterPath);
		return nullptr;
	}

	UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(LoadedObject);
	if (!Emitter)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService: Object is not a Niagara emitter: %s"), *EmitterPath);
		return nullptr;
	}

	return Emitter;
}

FNiagaraEmitterHandle* UNiagaraService::FindEmitterHandle(UNiagaraSystem* System, const FString& EmitterName)
{
	if (!System)
	{
		return nullptr;
	}

	TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
	for (FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (Handle.GetName().ToString().Equals(EmitterName, ESearchCase::IgnoreCase) ||
			Handle.GetUniqueInstanceName().Equals(EmitterName, ESearchCase::IgnoreCase))
		{
			return &Handle;
		}
	}

	return nullptr;
}

FString UNiagaraService::NiagaraTypeToString(const FNiagaraTypeDefinition& TypeDef)
{
	if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
	{
		return TEXT("Float");
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
	{
		return TEXT("Int");
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
	{
		return TEXT("Bool");
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
	{
		return TEXT("Vector2");
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
	{
		return TEXT("Vector");
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())
	{
		return TEXT("Vector4");
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
	{
		return TEXT("Color");
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetQuatDef())
	{
		return TEXT("Quat");
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetMatrix4Def())
	{
		return TEXT("Matrix");
	}
	else if (TypeDef.IsEnum())
	{
		return TEXT("Enum");
	}

	return TypeDef.GetName();
}

FString UNiagaraService::NiagaraVariableToString(const FNiagaraVariable& Variable)
{
	const FNiagaraTypeDefinition& TypeDef = Variable.GetType();

	// Safety check: ensure the variable has allocated data before reading
	if (!Variable.IsDataAllocated())
	{
		return TEXT("(uninitialized)");
	}

	if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
	{
		return FString::Printf(TEXT("%f"), Variable.GetValue<float>());
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
	{
		return FString::Printf(TEXT("%d"), Variable.GetValue<int32>());
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
	{
		return Variable.GetValue<bool>() ? TEXT("true") : TEXT("false");
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetVec2Def())
	{
		FVector2f Vec = Variable.GetValue<FVector2f>();
		return FString::Printf(TEXT("(X=%f,Y=%f)"), Vec.X, Vec.Y);
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
	{
		FVector3f Vec = Variable.GetValue<FVector3f>();
		return FString::Printf(TEXT("(X=%f,Y=%f,Z=%f)"), Vec.X, Vec.Y, Vec.Z);
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetVec4Def())
	{
		FVector4f Vec = Variable.GetValue<FVector4f>();
		return FString::Printf(TEXT("(X=%f,Y=%f,Z=%f,W=%f)"), Vec.X, Vec.Y, Vec.Z, Vec.W);
	}
	else if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
	{
		FLinearColor Color = Variable.GetValue<FLinearColor>();
		return FString::Printf(TEXT("(R=%f,G=%f,B=%f,A=%f)"), Color.R, Color.G, Color.B, Color.A);
	}

	return TEXT("");
}

// =================================================================
// Lifecycle Actions
// =================================================================

FNiagaraCreateResult UNiagaraService::CreateSystem(
	const FString& SystemName,
	const FString& DestinationPath,
	const FString& TemplateAssetPath)
{
	FNiagaraCreateResult Result;
	Result.bSuccess = false;

	if (SystemName.IsEmpty())
	{
		Result.ErrorMessage = TEXT("System name cannot be empty");
		return Result;
	}

	if (DestinationPath.IsEmpty())
	{
		Result.ErrorMessage = TEXT("Destination path cannot be empty");
		return Result;
	}

	// Construct the full asset path
	FString CleanPath = DestinationPath;
	if (!CleanPath.StartsWith(TEXT("/Game")))
	{
		CleanPath = TEXT("/Game/") + CleanPath;
	}
	if (CleanPath.EndsWith(TEXT("/")))
	{
		CleanPath = CleanPath.LeftChop(1);
	}

	FString FullAssetPath = CleanPath / SystemName;

	// Check if already exists - but also verify we can actually load it
	// to handle ghost references in the asset registry
	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		// Try to actually load the asset to verify it's not a ghost reference
		UObject* ExistingAsset = UEditorAssetLibrary::LoadAsset(FullAssetPath);
		if (ExistingAsset)
		{
			Result.ErrorMessage = FString::Printf(TEXT("Niagara system already exists at: %s"), *FullAssetPath);
			return Result;
		}
		else
		{
			// Ghost reference detected - asset registry says it exists but we can't load it
			UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::CreateSystem: Ghost reference detected at %s, will overwrite"), *FullAssetPath);
			// Delete the ghost reference entry
			UEditorAssetLibrary::DeleteAsset(FullAssetPath);
		}
	}

	auto CreateEmptySystem = [&]() -> UNiagaraSystem*
	{
		FString PackagePath = FullAssetPath;
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			Result.ErrorMessage = TEXT("Failed to create package");
			return nullptr;
		}

		UNiagaraSystemFactoryNew* Factory = NewObject<UNiagaraSystemFactoryNew>();
		if (!Factory)
		{
			Result.ErrorMessage = TEXT("Failed to create Niagara system factory");
			return nullptr;
		}

		UNiagaraSystem* NewSystem = Cast<UNiagaraSystem>(Factory->FactoryCreateNew(
			UNiagaraSystem::StaticClass(),
			Package,
			*SystemName,
			RF_Public | RF_Standalone,
			nullptr,
			GWarn));

		if (!NewSystem)
		{
			Result.ErrorMessage = TEXT("Failed to create Niagara system");
			return nullptr;
		}

		Package->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(NewSystem);

		FString PackageFilename = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, NewSystem, *PackageFilename, SaveArgs);

		return NewSystem;
	};

	// If a template is provided, create from template just like the editor
	if (!TemplateAssetPath.IsEmpty())
	{
		UObject* TemplateAsset = UEditorAssetLibrary::LoadAsset(TemplateAssetPath);
		if (!TemplateAsset)
		{
			Result.ErrorMessage = FString::Printf(TEXT("Template asset not found: %s"), *TemplateAssetPath);
			return Result;
		}

		if (UNiagaraSystem* TemplateSystem = Cast<UNiagaraSystem>(TemplateAsset))
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			UObject* DuplicatedAsset = AssetToolsModule.Get().DuplicateAsset(SystemName, CleanPath, TemplateSystem);
			UNiagaraSystem* NewSystem = Cast<UNiagaraSystem>(DuplicatedAsset);
			if (!NewSystem)
			{
				Result.ErrorMessage = TEXT("Failed to duplicate Niagara system template");
				return Result;
			}

			NewSystem->MarkPackageDirty();
			UEditorAssetLibrary::SaveAsset(FullAssetPath, true);

			Result.bSuccess = true;
			Result.AssetPath = FullAssetPath;
			return Result;
		}

		if (UNiagaraEmitter* TemplateEmitter = Cast<UNiagaraEmitter>(TemplateAsset))
		{
			UNiagaraSystem* NewSystem = CreateEmptySystem();
			if (!NewSystem)
			{
				return Result;
			}

			// Add emitter using editor utility path for parity with editor behavior
			FString EmitterPath = TemplateEmitter->GetPathName();
			AddEmitter(FullAssetPath, EmitterPath, FString());
			NewSystem->RequestCompile(false);
			UEditorAssetLibrary::SaveAsset(FullAssetPath, true);

			Result.bSuccess = true;
			Result.AssetPath = FullAssetPath;
			return Result;
		}

		Result.ErrorMessage = TEXT("Template asset must be a NiagaraSystem or NiagaraEmitter");
		return Result;
	}

	UNiagaraSystem* NewSystem = CreateEmptySystem();
	if (!NewSystem)
	{
		return Result;
	}

	Result.bSuccess = true;
	Result.AssetPath = FullAssetPath;
	return Result;
}

bool UNiagaraService::SaveSystem(const FString& SystemPath)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	// Mark package dirty first
	System->MarkPackageDirty();

	// Use EditorAssetLibrary for safe saving
	return UEditorAssetLibrary::SaveAsset(SystemPath, true);
}

bool UNiagaraService::CompileSystem(const FString& SystemPath, bool bWaitForCompletion)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	// Request compilation
	System->RequestCompile(false);

	if (bWaitForCompletion)
	{
		// Wait for compilation to complete
		System->WaitForCompilationComplete();
	}

	return true;
}

FNiagaraCompilationResult UNiagaraService::CompileWithResults(const FString& SystemPath)
{
	FNiagaraCompilationResult Result;
	Result.SystemPath = SystemPath;
	
	const double StartTime = FPlatformTime::Seconds();

	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		Result.bSuccess = false;
		Result.Errors.Add(TEXT("Failed to load Niagara system"));
		Result.ErrorCount = 1;
		return Result;
	}

	// Request compilation
	System->RequestCompile(false);
	System->WaitForCompilationComplete();

	// Check compilation status using system validity
	// In UE 5.7, we check if the system is ready for simulation after compile
	bool bHasErrors = false;

	// Check if system has valid compiled data
	if (!System->IsValid())
	{
		Result.Errors.Add(TEXT("System is invalid after compilation"));
		Result.ErrorCount++;
		bHasErrors = true;
	}

	// Check if system is ready for simulation (indicates successful compile)
	if (!System->IsReadyToRun())
	{
		Result.Errors.Add(TEXT("System is not ready to run after compilation - likely has compile errors"));
		Result.ErrorCount++;
		bHasErrors = true;
	}

	// Check emitter handles for enabled but invalid emitters
	const TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
	for (int32 i = 0; i < EmitterHandles.Num(); i++)
	{
		const FNiagaraEmitterHandle& EmitterHandle = EmitterHandles[i];
		if (EmitterHandle.GetIsEnabled())
		{
			// Safely check if emitter data exists
			const FVersionedNiagaraEmitter& VersionedEmitter = EmitterHandle.GetInstance();
			if (VersionedEmitter.Emitter != nullptr)
			{
				FVersionedNiagaraEmitterData* EmitterData = EmitterHandle.GetEmitterData();
				if (EmitterData != nullptr && !EmitterData->IsReadyToRun())
				{
					FString EmitterName = EmitterHandle.GetUniqueInstanceName();
					Result.Errors.Add(FString::Printf(TEXT("Emitter '%s' is not ready to run"), *EmitterName));
					Result.ErrorCount++;
					bHasErrors = true;
				}
			}
			else
			{
				FString EmitterName = EmitterHandle.GetUniqueInstanceName();
				Result.Errors.Add(FString::Printf(TEXT("Emitter '%s' has null emitter data"), *EmitterName));
				Result.ErrorCount++;
				bHasErrors = true;
			}
		}
	}

	// Determine success based on error count
	Result.bSuccess = !bHasErrors;
	
	// Calculate compilation time
	Result.CompilationTimeSeconds = FPlatformTime::Seconds() - StartTime;

	if (!Result.bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::CompileWithResults - System '%s' failed to compile with %d error(s)"), 
			*SystemPath, Result.ErrorCount);
	}

	return Result;
}

bool UNiagaraService::OpenInEditor(const FString& SystemPath)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	if (GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			return AssetEditorSubsystem->OpenEditorForAsset(System);
		}
	}

	return false;
}

bool UNiagaraService::CopySystemProperties(
	const FString& TargetSystemPath,
	const FString& SourceSystemPath)
{
	UNiagaraSystem* TargetSystem = LoadNiagaraSystem(TargetSystemPath);
	UNiagaraSystem* SourceSystem = LoadNiagaraSystem(SourceSystemPath);

	if (!TargetSystem || !SourceSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::CopySystemProperties: Failed to load systems"));
		return false;
	}

	TargetSystem->Modify();

	// Copy Effect Type - THE MOST CRITICAL PROPERTY
	UNiagaraEffectType* SourceEffectType = SourceSystem->GetEffectType();
	if (SourceEffectType)
	{
		TargetSystem->SetEffectType(SourceEffectType);
		UE_LOG(LogTemp, Log, TEXT("UNiagaraService: Copied effect type: %s"), *SourceEffectType->GetName());
	}

	// Copy Warmup settings
	TargetSystem->SetWarmupTime(SourceSystem->GetWarmupTime());
	TargetSystem->SetWarmupTickDelta(SourceSystem->GetWarmupTickDelta());
	if (FProperty* WarmupTickCountProp = TargetSystem->GetClass()->FindPropertyByName(TEXT("WarmupTickCount")))
	{
		WarmupTickCountProp->CopyCompleteValue_InContainer(TargetSystem, SourceSystem);
	}
	else if (FProperty* WarmupTickCountDeprecatedProp = TargetSystem->GetClass()->FindPropertyByName(TEXT("WarmupTickCount_DEPRECATED")))
	{
		WarmupTickCountDeprecatedProp->CopyCompleteValue_InContainer(TargetSystem, SourceSystem);
	}
	else
	{
		UE_LOG(LogTemp, Verbose, TEXT("UNiagaraService::CopySystemProperties: WarmupTickCount property not found"));
	}

	// Copy determinism + random seed (these are not covered elsewhere)
	if (FProperty* DeterminismProp = TargetSystem->GetClass()->FindPropertyByName(TEXT("bDeterminism")))
	{
		DeterminismProp->CopyCompleteValue_InContainer(TargetSystem, SourceSystem);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::CopySystemProperties: Missing property bDeterminism"));
	}

	if (FProperty* RandomSeedProp = TargetSystem->GetClass()->FindPropertyByName(TEXT("RandomSeed")))
	{
		RandomSeedProp->CopyCompleteValue_InContainer(TargetSystem, SourceSystem);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::CopySystemProperties: Missing property RandomSeed"));
	}

	// Copy Fixed Bounds
	FBox SourceBounds = SourceSystem->GetFixedBounds();
	bool bSourceHasFixedBounds = SourceBounds.IsValid != 0;
	if (bSourceHasFixedBounds)
	{
		TargetSystem->SetFixedBounds(SourceBounds);
	}

	// Copy public rendering properties
	TargetSystem->bSupportLargeWorldCoordinates = SourceSystem->bSupportLargeWorldCoordinates;
	TargetSystem->bCastShadow = SourceSystem->bCastShadow;
	TargetSystem->bReceivesDecals = SourceSystem->bReceivesDecals;
	TargetSystem->bRenderCustomDepth = SourceSystem->bRenderCustomDepth;
	TargetSystem->TranslucencySortPriority = SourceSystem->TranslucencySortPriority;

	// Copy debug settings
	TargetSystem->bDumpDebugSystemInfo = SourceSystem->bDumpDebugSystemInfo;
	TargetSystem->bDumpDebugEmitterInfo = SourceSystem->bDumpDebugEmitterInfo;

	// Request compile
	TargetSystem->RequestCompile(false);
	TargetSystem->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("UNiagaraService: Copied system properties from %s to %s"), 
		*SourceSystemPath, *TargetSystemPath);

	return true;
}

// =================================================================
// Information Actions
// =================================================================

bool UNiagaraService::GetSystemInfo(const FString& SystemPath, FNiagaraSystemInfo_Custom& OutInfo)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	OutInfo.SystemName = System->GetName();
	OutInfo.SystemPath = SystemPath;
	OutInfo.bIsValid = System->IsValid();

	// Get emitter information
	const TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
	OutInfo.EmitterCount = EmitterHandles.Num();

	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		FNiagaraEmitterInfo_Custom EmitterInfo;
		EmitterInfo.EmitterName = Handle.GetName().ToString();
		EmitterInfo.UniqueEmitterName = Handle.GetUniqueInstanceName();
		EmitterInfo.bIsEnabled = Handle.GetIsEnabled();

		FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (EmitterData)
		{
			EmitterInfo.SimTarget = EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim
				? TEXT("GPUComputeSim") : TEXT("CPUSim");
		}

		OutInfo.Emitters.Add(EmitterInfo);
	}

	// Get user parameters
	const FNiagaraUserRedirectionParameterStore& UserParamStore = System->GetExposedParameters();
	TArray<FNiagaraVariable> UserParams;
	UserParamStore.GetParameters(UserParams);

	for (const FNiagaraVariable& Param : UserParams)
	{
		FNiagaraParameterInfo_Custom ParamInfo;
		ParamInfo.ParameterName = Param.GetName().ToString();
		ParamInfo.ParameterType = NiagaraTypeToString(Param.GetType());
		ParamInfo.Namespace = TEXT("User");
		ParamInfo.bIsUserExposed = true;
		ParamInfo.CurrentValue = NiagaraVariableToString(Param);
		OutInfo.UserParameters.Add(ParamInfo);
	}

	OutInfo.bNeedsRecompile = System->HasOutstandingCompilationRequests();

	return true;
}

bool UNiagaraService::Summarize(const FString& SystemPath, FNiagaraSystemSummary& OutSummary)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	OutSummary.SystemPath = SystemPath;
	OutSummary.SystemName = System->GetName();

	const TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
	OutSummary.EmitterCount = EmitterHandles.Num();
	OutSummary.bHasGPUEmitters = false;

	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		OutSummary.EmitterNames.Add(Handle.GetName().ToString());

		FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (EmitterData && EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim)
		{
			OutSummary.bHasGPUEmitters = true;
		}
	}

	// Get user parameters
	const FNiagaraUserRedirectionParameterStore& UserParamStore = System->GetExposedParameters();
	TArray<FNiagaraVariable> UserParams;
	UserParamStore.GetParameters(UserParams);
	OutSummary.UserParameterCount = UserParams.Num();

	for (const FNiagaraVariable& Param : UserParams)
	{
		OutSummary.UserParameterNames.Add(Param.GetName().ToString());
	}

	return true;
}

TArray<FNiagaraEmitterInfo_Custom> UNiagaraService::ListEmitters(const FString& SystemPath)
{
	TArray<FNiagaraEmitterInfo_Custom> Result;

	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return Result;
	}

	const TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		FNiagaraEmitterInfo_Custom EmitterInfo;
		EmitterInfo.EmitterName = Handle.GetName().ToString();
		EmitterInfo.UniqueEmitterName = Handle.GetUniqueInstanceName();
		EmitterInfo.bIsEnabled = Handle.GetIsEnabled();

		FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (EmitterData)
		{
			EmitterInfo.SimTarget = EmitterData->SimTarget == ENiagaraSimTarget::GPUComputeSim
				? TEXT("GPUComputeSim") : TEXT("CPUSim");
		}

		Result.Add(EmitterInfo);
	}

	return Result;
}

// =================================================================
// Emitter Management Actions
// =================================================================

FString UNiagaraService::AddEmitter(
	const FString& SystemPath,
	const FString& EmitterAssetPath,
	const FString& EmitterName)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return FString();
	}

	System->Modify();

	UNiagaraEmitter* SourceEmitter = nullptr;
	FGuid EmitterVersion;
	bool bIsMinimalEmitter = false;

	// Check if this is a minimal emitter request
	if (EmitterAssetPath.IsEmpty() || EmitterAssetPath.Equals(TEXT("minimal"), ESearchCase::IgnoreCase))
	{
		bIsMinimalEmitter = true;

		// Try to load the default empty emitter from editor settings (like the UI does)
		const UNiagaraEditorSettings* EditorSettings = GetDefault<UNiagaraEditorSettings>();
		if (EditorSettings && !EditorSettings->DefaultEmptyEmitter.IsNull())
		{
			SourceEmitter = Cast<UNiagaraEmitter>(EditorSettings->DefaultEmptyEmitter.TryLoad());
			if (SourceEmitter)
			{
				EmitterVersion = SourceEmitter->GetExposedVersion().VersionGuid;
				UE_LOG(LogTemp, Log, TEXT("UNiagaraService: Using configured minimal emitter: %s"), *EditorSettings->DefaultEmptyEmitter.ToString());
			}
		}

		// If no configured emitter or failed to load, create a truly empty emitter
		if (!SourceEmitter)
		{
			SourceEmitter = NewObject<UNiagaraEmitter>(GetTransientPackage());
			UNiagaraEmitterFactoryNew::InitializeEmitter(SourceEmitter, false);  // false = no default modules
			EmitterVersion = SourceEmitter->GetExposedVersion().VersionGuid;
			UE_LOG(LogTemp, Log, TEXT("UNiagaraService: Creating truly empty minimal emitter"));
		}
	}
	else
	{
		// Load existing emitter asset
		SourceEmitter = LoadNiagaraEmitter(EmitterAssetPath);
		if (!SourceEmitter)
		{
			UE_LOG(LogTemp, Warning, TEXT("UNiagaraService: Failed to load emitter asset: %s"), *EmitterAssetPath);
			return FString();
		}
		EmitterVersion = SourceEmitter->GetExposedVersion().VersionGuid;
	}

	// Use FNiagaraEditorUtilities::AddEmitterToSystem - same as the Niagara Editor UI
	const FGuid NewEmitterHandleId = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *SourceEmitter, EmitterVersion, false);

	if (!NewEmitterHandleId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService: Failed to add emitter to system"));
		return FString();
	}

	// Rename if a custom name was specified
	FString ResultName;
	if (!EmitterName.IsEmpty())
	{
		for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			if (Handle.GetId() == NewEmitterHandleId)
			{
				Handle.SetName(FName(*EmitterName), *System);
				ResultName = EmitterName;
				break;
			}
		}
	}

	if (ResultName.IsEmpty())
	{
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
		{
			if (Handle.GetId() == NewEmitterHandleId)
			{
				ResultName = Handle.GetName().ToString();
				break;
			}
		}
	}

	// Sync the overview graph with the system - this updates the UI
	if (UNiagaraSystemEditorData* SystemEditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData()))
	{
		SystemEditorData->SynchronizeOverviewGraphWithSystem(*System);
	}

	// Request compile to make the emitter valid
	System->RequestCompile(false);

	// Refresh the view model if one exists (this updates the emitter handle view models)
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = FNiagaraEditorModule::Get().GetExistingViewModelForSystem(System);
	if (SystemViewModel.IsValid())
	{
		SystemViewModel->RefreshAll();
	}

	// Mark dirty
	System->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("UNiagaraService: Added emitter '%s' (minimal: %s)"), *ResultName, bIsMinimalEmitter ? TEXT("yes") : TEXT("no"));

	return ResultName;
}

TArray<FString> UNiagaraService::ListEmitterTemplates(
	const FString& SearchPath,
	const FString& NameFilter)
{
	TArray<FString> Result;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UNiagaraEmitter::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;

	// If no search path specified, search everywhere including engine content
	if (!SearchPath.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*SearchPath));
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		FString AssetPath = AssetData.GetObjectPathString();
		FString AssetName = AssetData.AssetName.ToString();

		// Apply name filter if specified
		if (!NameFilter.IsEmpty())
		{
			if (!AssetName.Contains(NameFilter, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		Result.Add(AssetPath);
	}

	// Sort alphabetically
	Result.Sort();

	return Result;
}

FString UNiagaraService::CopyEmitter(
	const FString& SourceSystemPath,
	const FString& SourceEmitterName,
	const FString& TargetSystemPath,
	const FString& NewEmitterName)
{
	// Load source system
	UNiagaraSystem* SourceSystem = LoadNiagaraSystem(SourceSystemPath);
	if (!SourceSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::CopyEmitter: Failed to load source system: %s"), *SourceSystemPath);
		return FString();
	}

	// Find the source emitter
	const FNiagaraEmitterHandle* SourceHandle = FindEmitterHandle(SourceSystem, SourceEmitterName);
	if (!SourceHandle || !SourceHandle->GetInstance().Emitter)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::CopyEmitter: Could not find emitter '%s' in source system"), *SourceEmitterName);
		return FString();
	}

	// Get the source emitter
	UNiagaraEmitter* SourceEmitter = SourceHandle->GetInstance().Emitter.Get();
	if (!SourceEmitter)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::CopyEmitter: Source emitter is invalid"));
		return FString();
	}

	// Load target system
	UNiagaraSystem* TargetSystem = LoadNiagaraSystem(TargetSystemPath);
	if (!TargetSystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::CopyEmitter: Failed to load target system: %s"), *TargetSystemPath);
		return FString();
	}

	// Determine the emitter name
	FString EmitterName = NewEmitterName.IsEmpty() ? SourceHandle->GetName().ToString() : NewEmitterName;
	FGuid EmitterVersion = SourceHandle->GetInstance().Version;

	TargetSystem->Modify();

	// Use the same editor utility as the Niagara Editor UI
	const FGuid NewEmitterHandleId = FNiagaraEditorUtilities::AddEmitterToSystem(*TargetSystem, *SourceEmitter, EmitterVersion, false);

	if (!NewEmitterHandleId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::CopyEmitter: Failed to add emitter to target system"));
		return FString();
	}

	// Rename if custom name provided
	FString ResultName;
	if (!EmitterName.IsEmpty())
	{
		for (FNiagaraEmitterHandle& Handle : TargetSystem->GetEmitterHandles())
		{
			if (Handle.GetId() == NewEmitterHandleId)
			{
				Handle.SetName(FName(*EmitterName), *TargetSystem);
				ResultName = EmitterName;
				break;
			}
		}
	}

	if (ResultName.IsEmpty())
	{
		for (const FNiagaraEmitterHandle& Handle : TargetSystem->GetEmitterHandles())
		{
			if (Handle.GetId() == NewEmitterHandleId)
			{
				ResultName = Handle.GetName().ToString();
				break;
			}
		}
	}

	// Sync the overview graph with the system - this updates the UI
	if (UNiagaraSystemEditorData* SystemEditorData = Cast<UNiagaraSystemEditorData>(TargetSystem->GetEditorData()))
	{
		SystemEditorData->SynchronizeOverviewGraphWithSystem(*TargetSystem);
	}

	// Request compile to make the emitter valid
	TargetSystem->RequestCompile(false);

	// Refresh the view model if one exists
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = FNiagaraEditorModule::Get().GetExistingViewModelForSystem(TargetSystem);
	if (SystemViewModel.IsValid())
	{
		SystemViewModel->RefreshAll();
	}

	// Mark dirty
	TargetSystem->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("UNiagaraService::CopyEmitter: Copied emitter '%s' from '%s' to '%s' as '%s'"),
		*SourceEmitterName, *SourceSystemPath, *TargetSystemPath, *ResultName);

	return ResultName;
}

bool UNiagaraService::RemoveEmitter(const FString& SystemPath, const FString& EmitterName)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService: Emitter not found: %s"), *EmitterName);
		return false;
	}

	// Get the handle ID
	FGuid HandleId = Handle->GetId();

	// Remove by ID
	TSet<FGuid> IdsToRemove;
	IdsToRemove.Add(HandleId);
	System->RemoveEmitterHandlesById(IdsToRemove);

	// Recompile after removal to sync internal state
	// This prevents crashes when saving/validating with stale emitter count
	System->RequestCompile(false);
	System->WaitForCompilationComplete();

	// Mark dirty
	System->MarkPackageDirty();

	return true;
}

bool UNiagaraService::EnableEmitter(const FString& SystemPath, const FString& EmitterName, bool bEnabled)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		return false;
	}

	Handle->SetIsEnabled(bEnabled, *System, false);
	System->MarkPackageDirty();

	return true;
}

bool UNiagaraService::DuplicateEmitter(
	const FString& SystemPath,
	const FString& SourceEmitterName,
	const FString& NewEmitterName)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraEmitterHandle* SourceHandle = FindEmitterHandle(System, SourceEmitterName);
	if (!SourceHandle)
	{
		return false;
	}

	// Duplicate the emitter handle
	FNiagaraEmitterHandle DuplicatedHandle = System->DuplicateEmitterHandle(*SourceHandle, FName(*NewEmitterName));

	if (!DuplicatedHandle.IsValid())
	{
		return false;
	}

	System->MarkPackageDirty();
	return true;
}

bool UNiagaraService::RenameEmitter(
	const FString& SystemPath,
	const FString& CurrentName,
	const FString& NewName)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, CurrentName);
	if (!Handle)
	{
		return false;
	}

	Handle->SetName(FName(*NewName), *System);
	System->MarkPackageDirty();

	return true;
}

bool UNiagaraService::MoveEmitter(
	const FString& SystemPath,
	const FString& EmitterName,
	int32 NewIndex)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	// Get the emitter handles array
	TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();
	
	// Find the current index of the emitter
	int32 CurrentIndex = INDEX_NONE;
	for (int32 i = 0; i < EmitterHandles.Num(); i++)
	{
		const FNiagaraEmitterHandle& Handle = EmitterHandles[i];
		if (Handle.GetName().ToString().Equals(EmitterName, ESearchCase::IgnoreCase) ||
			Handle.GetUniqueInstanceName().Equals(EmitterName, ESearchCase::IgnoreCase))
		{
			CurrentIndex = i;
			break;
		}
	}

	if (CurrentIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::MoveEmitter - Emitter not found: %s"), *EmitterName);
		return false;
	}

	// Clamp target index to valid range
	NewIndex = FMath::Clamp(NewIndex, 0, EmitterHandles.Num() - 1);

	// If already at target position, nothing to do
	if (CurrentIndex == NewIndex)
	{
		return true;
	}

	// Remove from current position and insert at new position
	FNiagaraEmitterHandle EmitterToMove = EmitterHandles[CurrentIndex];
	EmitterHandles.RemoveAt(CurrentIndex);
	EmitterHandles.Insert(EmitterToMove, NewIndex);

	System->MarkPackageDirty();
	
	UE_LOG(LogTemp, Log, TEXT("UNiagaraService::MoveEmitter - Moved '%s' from index %d to %d"), 
		*EmitterName, CurrentIndex, NewIndex);

	return true;
}

bool UNiagaraService::GetEmitterGraphPosition(
	const FString& SystemPath,
	const FString& EmitterName,
	float& OutX,
	float& OutY)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	// Get the system editor data
	UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData());
	if (!EditorData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::GetEmitterGraphPosition - No editor data found"));
		return false;
	}

	// Find the emitter handle
	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::GetEmitterGraphPosition - Emitter not found: %s"), *EmitterName);
		return false;
	}

	// Get the overview graph
	UEdGraph* OverviewGraph = EditorData->GetSystemOverviewGraph();
	if (!OverviewGraph)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::GetEmitterGraphPosition - No overview graph found"));
		return false;
	}

	// Find the overview node for this emitter
	FGuid EmitterGuid = Handle->GetId();
	for (UEdGraphNode* Node : OverviewGraph->Nodes)
	{
		UNiagaraOverviewNode* OverviewNode = Cast<UNiagaraOverviewNode>(Node);
		if (OverviewNode && OverviewNode->GetEmitterHandleGuid() == EmitterGuid)
		{
			OutX = OverviewNode->NodePosX;
			OutY = OverviewNode->NodePosY;
			UE_LOG(LogTemp, Log, TEXT("UNiagaraService::GetEmitterGraphPosition - '%s' at (%.1f, %.1f)"), 
				*EmitterName, OutX, OutY);
			return true;
		}
	}

	// If no node found, return default
	UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::GetEmitterGraphPosition - No overview node found for emitter: %s"), *EmitterName);
	OutX = 0.0f;
	OutY = 0.0f;
	return false;
}

bool UNiagaraService::SetEmitterGraphPosition(
	const FString& SystemPath,
	const FString& EmitterName,
	float X,
	float Y)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	// Get the system editor data
	UNiagaraSystemEditorData* EditorData = Cast<UNiagaraSystemEditorData>(System->GetEditorData());
	if (!EditorData)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::SetEmitterGraphPosition - No editor data found"));
		return false;
	}

	// Find the emitter handle
	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::SetEmitterGraphPosition - Emitter not found: %s"), *EmitterName);
		return false;
	}

	// Get the overview graph
	UEdGraph* OverviewGraph = EditorData->GetSystemOverviewGraph();
	if (!OverviewGraph)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::SetEmitterGraphPosition - No overview graph found"));
		return false;
	}

	// Find the overview node for this emitter and set its position
	FGuid EmitterGuid = Handle->GetId();
	for (UEdGraphNode* Node : OverviewGraph->Nodes)
	{
		UNiagaraOverviewNode* OverviewNode = Cast<UNiagaraOverviewNode>(Node);
		if (OverviewNode && OverviewNode->GetEmitterHandleGuid() == EmitterGuid)
		{
			OverviewNode->NodePosX = X;
			OverviewNode->NodePosY = Y;
			System->MarkPackageDirty();
			
			UE_LOG(LogTemp, Log, TEXT("UNiagaraService::SetEmitterGraphPosition - Set '%s' to position (%.1f, %.1f)"), 
				*EmitterName, X, Y);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::SetEmitterGraphPosition - No overview node found for emitter: %s"), *EmitterName);
	return false;
}

// =================================================================
// Parameter Management Actions
// =================================================================

TArray<FNiagaraParameterInfo_Custom> UNiagaraService::ListParameters(const FString& SystemPath)
{
	TArray<FNiagaraParameterInfo_Custom> Result;

	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return Result;
	}

	const FNiagaraUserRedirectionParameterStore& UserParamStore = System->GetExposedParameters();
	TArray<FNiagaraVariable> UserParams;
	UserParamStore.GetParameters(UserParams);

	for (const FNiagaraVariable& Param : UserParams)
	{
		FNiagaraParameterInfo_Custom ParamInfo;
		ParamInfo.ParameterName = Param.GetName().ToString();
		ParamInfo.ParameterType = NiagaraTypeToString(Param.GetType());
		ParamInfo.Namespace = TEXT("User");
		ParamInfo.bIsUserExposed = true;
		
		// Read the value directly from the parameter store
		const FNiagaraTypeDefinition& TypeDef = Param.GetType();
		int32 ParamOffset = UserParamStore.IndexOf(Param);
		if (ParamOffset == INDEX_NONE)
		{
			ParamInfo.CurrentValue = TEXT("(unset)");
		}
		else if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
		{
			float Val = UserParamStore.GetParameterValue<float>(Param);
			ParamInfo.CurrentValue = FString::Printf(TEXT("%f"), Val);
		}
		else if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
		{
			int32 Val = UserParamStore.GetParameterValue<int32>(Param);
			ParamInfo.CurrentValue = FString::Printf(TEXT("%d"), Val);
		}
		else if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
		{
			bool Val = UserParamStore.GetParameterValue<bool>(Param);
			ParamInfo.CurrentValue = Val ? TEXT("true") : TEXT("false");
		}
		else
		{
			ParamInfo.CurrentValue = NiagaraVariableToString(Param);
		}
		
		Result.Add(ParamInfo);
	}

	return Result;
}

bool UNiagaraService::GetParameter(
	const FString& SystemPath,
	const FString& ParameterName,
	FNiagaraParameterInfo_Custom& OutInfo)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	const FNiagaraUserRedirectionParameterStore& UserParamStore = System->GetExposedParameters();
	TArray<FNiagaraVariable> UserParams;
	UserParamStore.GetParameters(UserParams);

	for (const FNiagaraVariable& Param : UserParams)
	{
		FString ParamName = Param.GetName().ToString();
		if (ParamName.Equals(ParameterName, ESearchCase::IgnoreCase) ||
			ParamName.EndsWith(ParameterName, ESearchCase::IgnoreCase))
		{
			OutInfo.ParameterName = ParamName;
			OutInfo.ParameterType = NiagaraTypeToString(Param.GetType());
			OutInfo.Namespace = TEXT("User");
			OutInfo.bIsUserExposed = true;
			OutInfo.CurrentValue = NiagaraVariableToString(Param);
			return true;
		}
	}

	return false;
}

bool UNiagaraService::SetParameter(
	const FString& SystemPath,
	const FString& ParameterName,
	const FString& Value)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraUserRedirectionParameterStore& UserParamStore = System->GetExposedParameters();
	TArray<FNiagaraVariable> UserParams;
	UserParamStore.GetParameters(UserParams);

	for (FNiagaraVariable& Param : UserParams)
	{
		FString ParamName = Param.GetName().ToString();
		if (ParamName.Equals(ParameterName, ESearchCase::IgnoreCase) ||
			ParamName.EndsWith(ParameterName, ESearchCase::IgnoreCase))
		{
			const FNiagaraTypeDefinition& TypeDef = Param.GetType();

			if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
			{
				float FloatValue = FCString::Atof(*Value);
				UserParamStore.SetParameterValue(FloatValue, Param);
			}
			else if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
			{
				int32 IntValue = FCString::Atoi(*Value);
				UserParamStore.SetParameterValue(IntValue, Param);
			}
			else if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
			{
				bool BoolValue = Value.ToBool() || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value.Equals(TEXT("1"));
				UserParamStore.SetParameterValue(BoolValue, Param);
			}
			else if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
			{
				FLinearColor Color;
				Color.InitFromString(Value);
				UserParamStore.SetParameterValue(Color, Param);
			}
			else if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
			{
				FVector3f Vec;
				// Parse vector manually (FDefaultValueHelper moved in UE 5.7)
				FString TrimmedValue = Value.TrimStartAndEnd();
				TrimmedValue.RemoveFromStart(TEXT("("));
				TrimmedValue.RemoveFromEnd(TEXT(")"));
				TArray<FString> Components;
				TrimmedValue.ParseIntoArray(Components, TEXT(","));
				if (Components.Num() >= 3)
				{
					Vec.X = FCString::Atof(*Components[0].TrimStartAndEnd());
					Vec.Y = FCString::Atof(*Components[1].TrimStartAndEnd());
					Vec.Z = FCString::Atof(*Components[2].TrimStartAndEnd());
				}
				UserParamStore.SetParameterValue(Vec, Param);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("UNiagaraService: Unsupported parameter type for: %s"), *ParameterName);
				return false;
			}

			System->MarkPackageDirty();
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UNiagaraService: Parameter not found: %s"), *ParameterName);
	return false;
}

bool UNiagaraService::AddUserParameter(
	const FString& SystemPath,
	const FString& ParameterName,
	const FString& ParameterType,
	const FString& DefaultValue)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	// Determine the type
	FNiagaraTypeDefinition TypeDef;
	if (ParameterType.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
	{
		TypeDef = FNiagaraTypeDefinition::GetFloatDef();
	}
	else if (ParameterType.Equals(TEXT("Int"), ESearchCase::IgnoreCase) || ParameterType.Equals(TEXT("Int32"), ESearchCase::IgnoreCase))
	{
		TypeDef = FNiagaraTypeDefinition::GetIntDef();
	}
	else if (ParameterType.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
	{
		TypeDef = FNiagaraTypeDefinition::GetBoolDef();
	}
	else if (ParameterType.Equals(TEXT("Vector"), ESearchCase::IgnoreCase) || ParameterType.Equals(TEXT("Vector3"), ESearchCase::IgnoreCase))
	{
		TypeDef = FNiagaraTypeDefinition::GetVec3Def();
	}
	else if (ParameterType.Equals(TEXT("Vector2"), ESearchCase::IgnoreCase))
	{
		TypeDef = FNiagaraTypeDefinition::GetVec2Def();
	}
	else if (ParameterType.Equals(TEXT("Vector4"), ESearchCase::IgnoreCase))
	{
		TypeDef = FNiagaraTypeDefinition::GetVec4Def();
	}
	else if (ParameterType.Equals(TEXT("Color"), ESearchCase::IgnoreCase) || ParameterType.Equals(TEXT("LinearColor"), ESearchCase::IgnoreCase))
	{
		TypeDef = FNiagaraTypeDefinition::GetColorDef();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService: Unknown parameter type: %s"), *ParameterType);
		return false;
	}

	// Create the parameter with User namespace (avoid double prefix)
	FString FullName;
	if (ParameterName.StartsWith(TEXT("User."), ESearchCase::IgnoreCase))
	{
		FullName = ParameterName;
	}
	else
	{
		FullName = FString::Printf(TEXT("User.%s"), *ParameterName);
	}
	FNiagaraVariable NewVariable(TypeDef, FName(*FullName));

	// Set default value if provided
	if (!DefaultValue.IsEmpty())
	{
		if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
		{
			float Val = FCString::Atof(*DefaultValue);
			NewVariable.SetValue(Val);
		}
		else if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
		{
			int32 Val = FCString::Atoi(*DefaultValue);
			NewVariable.SetValue(Val);
		}
		else if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
		{
			bool Val = DefaultValue.ToBool() || DefaultValue.Equals(TEXT("true"), ESearchCase::IgnoreCase);
			NewVariable.SetValue(Val);
		}
		else if (TypeDef == FNiagaraTypeDefinition::GetColorDef())
		{
			FLinearColor Val;
			Val.InitFromString(DefaultValue);
			NewVariable.SetValue(Val);
		}
	}

	// Add to exposed parameters
	FNiagaraUserRedirectionParameterStore& UserParamStore = System->GetExposedParameters();
	UserParamStore.AddParameter(NewVariable, true);

	System->MarkPackageDirty();
	return true;
}

bool UNiagaraService::RemoveUserParameter(
	const FString& SystemPath,
	const FString& ParameterName)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraUserRedirectionParameterStore& UserParamStore = System->GetExposedParameters();
	TArray<FNiagaraVariable> UserParams;
	UserParamStore.GetParameters(UserParams);

	for (const FNiagaraVariable& Param : UserParams)
	{
		FString ParamName = Param.GetName().ToString();
		if (ParamName.Equals(ParameterName, ESearchCase::IgnoreCase) ||
			ParamName.EndsWith(ParameterName, ESearchCase::IgnoreCase))
		{
			UserParamStore.RemoveParameter(Param);
			System->MarkPackageDirty();
			return true;
		}
	}

	return false;
}

// =================================================================
// Existence Checks
// =================================================================

bool UNiagaraService::SystemExists(const FString& SystemPath)
{
	if (SystemPath.IsEmpty())
	{
		return false;
	}

	if (!UEditorAssetLibrary::DoesAssetExist(SystemPath))
	{
		return false;
	}

	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	return System != nullptr;
}

bool UNiagaraService::EmitterExists(const FString& SystemPath, const FString& EmitterName)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	return FindEmitterHandle(System, EmitterName) != nullptr;
}

bool UNiagaraService::ParameterExists(const FString& SystemPath, const FString& ParameterName)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	const FNiagaraUserRedirectionParameterStore& UserParamStore = System->GetExposedParameters();
	TArray<FNiagaraVariable> UserParams;
	UserParamStore.GetParameters(UserParams);

	for (const FNiagaraVariable& Param : UserParams)
	{
		FString ParamName = Param.GetName().ToString();
		if (ParamName.Equals(ParameterName, ESearchCase::IgnoreCase) ||
			ParamName.EndsWith(ParameterName, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}

// =================================================================
// Search Actions
// =================================================================

TArray<FString> UNiagaraService::SearchSystems(
	const FString& SearchPath,
	const FString& NameFilter)
{
	TArray<FString> Result;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;

	if (!SearchPath.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*SearchPath));
	}

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	for (const FAssetData& Asset : Assets)
	{
		FString AssetName = Asset.AssetName.ToString();
		if (NameFilter.IsEmpty() || AssetName.Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			Result.Add(Asset.GetObjectPathString());
		}
	}

	return Result;
}

TArray<FString> UNiagaraService::SearchEmitterAssets(
	const FString& SearchPath,
	const FString& NameFilter)
{
	TArray<FString> Result;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UNiagaraEmitter::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;

	if (!SearchPath.IsEmpty())
	{
		Filter.PackagePaths.Add(FName(*SearchPath));
	}

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	for (const FAssetData& Asset : Assets)
	{
		FString AssetName = Asset.AssetName.ToString();
		if (NameFilter.IsEmpty() || AssetName.Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			Result.Add(Asset.GetObjectPathString());
		}
	}

	return Result;
}

// =================================================================
// Diagnostic Actions
// =================================================================

bool UNiagaraService::GetSystemProperties(
	const FString& SystemPath,
	FNiagaraSystemPropertiesInfo& OutProperties)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetSystemProperties: Failed to load system: %s"), *SystemPath);
		return false;
	}

	// Basic info
	OutProperties.SystemName = System->GetName();
	OutProperties.SystemPath = SystemPath;
	OutProperties.EmitterCount = System->GetEmitterHandles().Num();

	// Effect Type
	UNiagaraEffectType* EffectType = System->GetEffectType();
	if (EffectType)
	{
		OutProperties.EffectTypeName = EffectType->GetName();
		OutProperties.EffectTypePath = EffectType->GetPathName();
		OutProperties.UpdateFrequency = StaticEnum<ENiagaraScalabilityUpdateFrequency>()->GetNameStringByValue((int64)EffectType->UpdateFrequency);
		OutProperties.CullReaction = StaticEnum<ENiagaraCullReaction>()->GetNameStringByValue((int64)EffectType->CullReaction);
	}
	else
	{
		OutProperties.EffectTypeName = TEXT("None");
		OutProperties.EffectTypePath = TEXT("");
		OutProperties.UpdateFrequency = TEXT("N/A");
		OutProperties.CullReaction = TEXT("N/A");
	}

	// Determinism
	OutProperties.bDeterminism = System->NeedsDeterminism();
	OutProperties.RandomSeed = System->GetRandomSeed();

	// Warmup
	OutProperties.WarmupTime = System->GetWarmupTime();
	OutProperties.WarmupTickCount = System->GetWarmupTickCount();
	OutProperties.WarmupTickDelta = System->GetWarmupTickDelta();

	// Bounds - access via public methods
	FBox FixedBounds = System->GetFixedBounds();
	OutProperties.bFixedBounds = FixedBounds.IsValid != 0;
	if (OutProperties.bFixedBounds)
	{
		OutProperties.FixedBoundsValue = FString::Printf(TEXT("Min(%f,%f,%f) Max(%f,%f,%f)"),
			FixedBounds.Min.X, FixedBounds.Min.Y, FixedBounds.Min.Z,
			FixedBounds.Max.X, FixedBounds.Max.Y, FixedBounds.Max.Z);
	}
	else
	{
		OutProperties.FixedBoundsValue = TEXT("Dynamic");
	}

	// Rendering - these are public properties
	OutProperties.bSupportLargeWorldCoordinates = System->bSupportLargeWorldCoordinates;
	OutProperties.bCastShadow = System->bCastShadow;
	OutProperties.bReceivesDecals = System->bReceivesDecals;
	OutProperties.bRenderCustomDepth = System->bRenderCustomDepth;
	OutProperties.TranslucencySortPriority = System->TranslucencySortPriority;

	// Performance - these properties are protected, so we skip them
	OutProperties.bBakeOutRapidIteration = false;  // Protected - not accessible
	OutProperties.bCompressAttributes = false;     // Protected - not accessible
	OutProperties.bTrimAttributes = false;         // Protected - not accessible

	// Scalability - protected property
	OutProperties.bOverrideScalabilitySettings = false;  // Protected - not accessible

	// Debug
	OutProperties.bDumpDebugSystemInfo = System->bDumpDebugSystemInfo;
	OutProperties.bDumpDebugEmitterInfo = System->bDumpDebugEmitterInfo;

	return true;
}

// Helper to compare rapid iteration parameters between two emitters
static void CompareEmitterRapidIterationParams(
	UNiagaraSystem* SourceSystem,
	UNiagaraSystem* TargetSystem,
	const FString& EmitterName,
	TArray<FNiagaraPropertyDifference>& OutDifferences)
{
	// Find emitters in both systems
	FNiagaraEmitterHandle* SourceHandle = nullptr;
	FNiagaraEmitterHandle* TargetHandle = nullptr;

	for (FNiagaraEmitterHandle& Handle : SourceSystem->GetEmitterHandles())
	{
		if (Handle.GetUniqueInstanceName() == EmitterName)
		{
			SourceHandle = &Handle;
			break;
		}
	}

	for (FNiagaraEmitterHandle& Handle : TargetSystem->GetEmitterHandles())
	{
		if (Handle.GetUniqueInstanceName() == EmitterName)
		{
			TargetHandle = &Handle;
			break;
		}
	}

	if (!SourceHandle || !TargetHandle)
	{
		return;
	}

	FVersionedNiagaraEmitterData* SourceData = SourceHandle->GetEmitterData();
	FVersionedNiagaraEmitterData* TargetData = TargetHandle->GetEmitterData();

	if (!SourceData || !TargetData)
	{
		return;
	}

	// Compare emitter-level properties
	if (SourceData->SimTarget != TargetData->SimTarget)
	{
		FNiagaraPropertyDifference Diff;
		Diff.Category = TEXT("Emitter");
		Diff.PropertyName = TEXT("SimTarget");
		Diff.SourceValue = StaticEnum<ENiagaraSimTarget>()->GetNameStringByValue((int64)SourceData->SimTarget);
		Diff.TargetValue = StaticEnum<ENiagaraSimTarget>()->GetNameStringByValue((int64)TargetData->SimTarget);
		Diff.EmitterName = EmitterName;
		OutDifferences.Add(Diff);
	}

	if (SourceData->bLocalSpace != TargetData->bLocalSpace)
	{
		FNiagaraPropertyDifference Diff;
		Diff.Category = TEXT("Emitter");
		Diff.PropertyName = TEXT("bLocalSpace");
		Diff.SourceValue = SourceData->bLocalSpace ? TEXT("true") : TEXT("false");
		Diff.TargetValue = TargetData->bLocalSpace ? TEXT("true") : TEXT("false");
		Diff.EmitterName = EmitterName;
		OutDifferences.Add(Diff);
	}

	if (SourceData->bDeterminism != TargetData->bDeterminism)
	{
		FNiagaraPropertyDifference Diff;
		Diff.Category = TEXT("Emitter");
		Diff.PropertyName = TEXT("bDeterminism");
		Diff.SourceValue = SourceData->bDeterminism ? TEXT("true") : TEXT("false");
		Diff.TargetValue = TargetData->bDeterminism ? TEXT("true") : TEXT("false");
		Diff.EmitterName = EmitterName;
		OutDifferences.Add(Diff);
	}

	// Compare scripts' rapid iteration parameters
	auto CompareScriptParams = [&](UNiagaraScript* SourceScript, UNiagaraScript* TargetScript, const FString& ScriptType)
	{
		if (!SourceScript || !TargetScript)
		{
			return;
		}

		const FNiagaraParameterStore& SourceStore = SourceScript->RapidIterationParameters;
		const FNiagaraParameterStore& TargetStore = TargetScript->RapidIterationParameters;

		TArray<FNiagaraVariable> SourceParams, TargetParams;
		SourceStore.GetParameters(SourceParams);
		TargetStore.GetParameters(TargetParams);

		// Build maps for comparison
		TMap<FName, FNiagaraVariable> SourceMap, TargetMap;
		for (const FNiagaraVariable& Var : SourceParams)
		{
			SourceMap.Add(Var.GetName(), Var);
		}
		for (const FNiagaraVariable& Var : TargetParams)
		{
			TargetMap.Add(Var.GetName(), Var);
		}

		// Check for differences
		for (const auto& Pair : SourceMap)
		{
			FNiagaraVariable* TargetVar = TargetMap.Find(Pair.Key);
			if (!TargetVar)
			{
				FNiagaraPropertyDifference Diff;
				Diff.Category = TEXT("RapidIteration");
				Diff.PropertyName = FString::Printf(TEXT("[%s] %s"), *ScriptType, *Pair.Key.ToString());
				Diff.SourceValue = TEXT("(exists)");
				Diff.TargetValue = TEXT("(missing)");
				Diff.EmitterName = EmitterName;
				OutDifferences.Add(Diff);
			}
			else
			{
				// Compare values - get raw data and compare
				const FNiagaraTypeDefinition& TypeDef = Pair.Value.GetType();
				int32 SourceOffset = SourceStore.IndexOf(Pair.Value);
				int32 TargetOffset = TargetStore.IndexOf(*TargetVar);

				if (SourceOffset != INDEX_NONE && TargetOffset != INDEX_NONE)
				{
					const uint8* SourceData = SourceStore.GetParameterData(SourceOffset, TypeDef);
					const uint8* TargetData = TargetStore.GetParameterData(TargetOffset, TypeDef);

					int32 Size = TypeDef.GetSize();
					if (SourceData && TargetData && FMemory::Memcmp(SourceData, TargetData, Size) != 0)
					{
						FNiagaraPropertyDifference Diff;
						Diff.Category = TEXT("RapidIteration");
						Diff.PropertyName = FString::Printf(TEXT("[%s] %s"), *ScriptType, *Pair.Key.ToString());

						// Try to format values based on type
						if (Size == sizeof(float))
						{
							float SourceVal, TargetVal;
							FMemory::Memcpy(&SourceVal, SourceData, sizeof(float));
							FMemory::Memcpy(&TargetVal, TargetData, sizeof(float));
							Diff.SourceValue = FString::Printf(TEXT("%f"), SourceVal);
							Diff.TargetValue = FString::Printf(TEXT("%f"), TargetVal);
						}
						else if (Size == sizeof(int32))
						{
							int32 SourceVal, TargetVal;
							FMemory::Memcpy(&SourceVal, SourceData, sizeof(int32));
							FMemory::Memcpy(&TargetVal, TargetData, sizeof(int32));
							Diff.SourceValue = FString::Printf(TEXT("%d"), SourceVal);
							Diff.TargetValue = FString::Printf(TEXT("%d"), TargetVal);
						}
						else if (Size == 1)
						{
							Diff.SourceValue = (*SourceData != 0) ? TEXT("true") : TEXT("false");
							Diff.TargetValue = (*TargetData != 0) ? TEXT("true") : TEXT("false");
						}
						else
						{
							Diff.SourceValue = TEXT("(differs)");
							Diff.TargetValue = TEXT("(differs)");
						}

						Diff.EmitterName = EmitterName;
						OutDifferences.Add(Diff);
					}
				}
			}
		}

		// Check for params in target but not source
		for (const auto& Pair : TargetMap)
		{
			if (!SourceMap.Contains(Pair.Key))
			{
				FNiagaraPropertyDifference Diff;
				Diff.Category = TEXT("RapidIteration");
				Diff.PropertyName = FString::Printf(TEXT("[%s] %s"), *ScriptType, *Pair.Key.ToString());
				Diff.SourceValue = TEXT("(missing)");
				Diff.TargetValue = TEXT("(exists)");
				Diff.EmitterName = EmitterName;
				OutDifferences.Add(Diff);
			}
		}
	};

	// Compare all script types
	CompareScriptParams(SourceData->EmitterSpawnScriptProps.Script, TargetData->EmitterSpawnScriptProps.Script, TEXT("EmitterSpawn"));
	CompareScriptParams(SourceData->EmitterUpdateScriptProps.Script, TargetData->EmitterUpdateScriptProps.Script, TEXT("EmitterUpdate"));
	CompareScriptParams(SourceData->SpawnScriptProps.Script, TargetData->SpawnScriptProps.Script, TEXT("ParticleSpawn"));
	CompareScriptParams(SourceData->UpdateScriptProps.Script, TargetData->UpdateScriptProps.Script, TEXT("ParticleUpdate"));
}

FNiagaraSystemComparison UNiagaraService::CompareSystems(
	const FString& SourceSystemPath,
	const FString& TargetSystemPath)
{
	FNiagaraSystemComparison Result;
	Result.SourcePath = SourceSystemPath;
	Result.TargetPath = TargetSystemPath;
	Result.bAreEquivalent = true;

	UNiagaraSystem* SourceSystem = LoadNiagaraSystem(SourceSystemPath);
	UNiagaraSystem* TargetSystem = LoadNiagaraSystem(TargetSystemPath);

	if (!SourceSystem || !TargetSystem)
	{
		Result.bAreEquivalent = false;
		FNiagaraPropertyDifference Diff;
		Diff.Category = TEXT("System");
		Diff.PropertyName = TEXT("LoadError");
		Diff.SourceValue = SourceSystem ? TEXT("Loaded") : TEXT("Failed to load");
		Diff.TargetValue = TargetSystem ? TEXT("Loaded") : TEXT("Failed to load");
		Result.Differences.Add(Diff);
		Result.DifferenceCount = 1;
		return Result;
	}

	// Compare emitter counts
	Result.SourceEmitterCount = SourceSystem->GetEmitterHandles().Num();
	Result.TargetEmitterCount = TargetSystem->GetEmitterHandles().Num();

	// Build emitter name sets
	TSet<FString> SourceEmitters, TargetEmitters;
	for (const FNiagaraEmitterHandle& Handle : SourceSystem->GetEmitterHandles())
	{
		SourceEmitters.Add(Handle.GetUniqueInstanceName());
	}
	for (const FNiagaraEmitterHandle& Handle : TargetSystem->GetEmitterHandles())
	{
		TargetEmitters.Add(Handle.GetUniqueInstanceName());
	}

	// Find emitters only in source
	for (const FString& Name : SourceEmitters)
	{
		if (!TargetEmitters.Contains(Name))
		{
			Result.EmittersOnlyInSource.Add(Name);
			Result.bAreEquivalent = false;

			FNiagaraPropertyDifference Diff;
			Diff.Category = TEXT("Emitter");
			Diff.PropertyName = TEXT("Exists");
			Diff.SourceValue = TEXT("Present");
			Diff.TargetValue = TEXT("Missing");
			Diff.EmitterName = Name;
			Result.Differences.Add(Diff);
		}
	}

	// Find emitters only in target
	for (const FString& Name : TargetEmitters)
	{
		if (!SourceEmitters.Contains(Name))
		{
			Result.EmittersOnlyInTarget.Add(Name);
			Result.bAreEquivalent = false;

			FNiagaraPropertyDifference Diff;
			Diff.Category = TEXT("Emitter");
			Diff.PropertyName = TEXT("Exists");
			Diff.SourceValue = TEXT("Missing");
			Diff.TargetValue = TEXT("Present");
			Diff.EmitterName = Name;
			Result.Differences.Add(Diff);
		}
	}

	// Compare system-level properties
	auto CompareSystemProperty = [&](const FString& PropName, const FString& SourceVal, const FString& TargetVal)
	{
		if (SourceVal != TargetVal)
		{
			Result.bAreEquivalent = false;
			FNiagaraPropertyDifference Diff;
			Diff.Category = TEXT("System");
			Diff.PropertyName = PropName;
			Diff.SourceValue = SourceVal;
			Diff.TargetValue = TargetVal;
			Result.Differences.Add(Diff);
		}
	};

	// Effect Type
	UNiagaraEffectType* SourceEffect = SourceSystem->GetEffectType();
	UNiagaraEffectType* TargetEffect = TargetSystem->GetEffectType();
	FString SourceEffectName = SourceEffect ? SourceEffect->GetName() : TEXT("None");
	FString TargetEffectName = TargetEffect ? TargetEffect->GetName() : TEXT("None");
	CompareSystemProperty(TEXT("EffectType"), SourceEffectName, TargetEffectName);

	// Determinism
	CompareSystemProperty(TEXT("bDeterminism"),
		SourceSystem->NeedsDeterminism() ? TEXT("true") : TEXT("false"),
		TargetSystem->NeedsDeterminism() ? TEXT("true") : TEXT("false"));

	CompareSystemProperty(TEXT("RandomSeed"),
		FString::FromInt(SourceSystem->GetRandomSeed()),
		FString::FromInt(TargetSystem->GetRandomSeed()));

	// Warmup
	CompareSystemProperty(TEXT("WarmupTime"),
		FString::Printf(TEXT("%f"), SourceSystem->GetWarmupTime()),
		FString::Printf(TEXT("%f"), TargetSystem->GetWarmupTime()));

	CompareSystemProperty(TEXT("WarmupTickCount"),
		FString::FromInt(SourceSystem->GetWarmupTickCount()),
		FString::FromInt(TargetSystem->GetWarmupTickCount()));

	// Rendering
	CompareSystemProperty(TEXT("bSupportLargeWorldCoordinates"),
		SourceSystem->bSupportLargeWorldCoordinates ? TEXT("true") : TEXT("false"),
		TargetSystem->bSupportLargeWorldCoordinates ? TEXT("true") : TEXT("false"));

	CompareSystemProperty(TEXT("bCastShadow"),
		SourceSystem->bCastShadow ? TEXT("true") : TEXT("false"),
		TargetSystem->bCastShadow ? TEXT("true") : TEXT("false"));

	// Note: bBakeOutRapidIteration is protected - skipping comparison

	// Compare emitters that exist in both
	for (const FString& EmitterName : SourceEmitters)
	{
		if (TargetEmitters.Contains(EmitterName))
		{
			CompareEmitterRapidIterationParams(SourceSystem, TargetSystem, EmitterName, Result.Differences);
		}
	}

	// Update equivalent flag and count
	Result.DifferenceCount = Result.Differences.Num();
	if (Result.DifferenceCount > 0)
	{
		Result.bAreEquivalent = false;
	}

	return Result;
}
// =================================================================
// New Diagnostic Methods
// =================================================================

TArray<FNiagaraRIParameterInfo> UNiagaraService::ListRapidIterationParams(
	const FString& SystemPath,
	const FString& EmitterName)
{
	TArray<FNiagaraRIParameterInfo> Result;

	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return Result;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService: Emitter '%s' not found in system"), *EmitterName);
		return Result;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return Result;
	}

	// Lambda to extract RI params from a script
	auto ExtractRIParams = [&Result](UNiagaraScript* Script, const FString& ScriptType)
	{
		if (!Script)
		{
			return;
		}

		const FNiagaraParameterStore& Store = Script->RapidIterationParameters;
		TArray<FNiagaraVariable> Params;
		Store.GetParameters(Params);

		for (const FNiagaraVariable& Var : Params)
		{
			FNiagaraRIParameterInfo Info;
			Info.ParameterName = Var.GetName().ToString();
			Info.ParameterType = NiagaraTypeToString(Var.GetType());
			Info.ScriptType = ScriptType;

			// Get the value
			int32 Offset = Store.IndexOf(Var);
			if (Offset != INDEX_NONE)
			{
				const FNiagaraTypeDefinition& TypeDef = Var.GetType();
				const uint8* Data = Store.GetParameterData(Offset, TypeDef);
				int32 Size = TypeDef.GetSize();

				if (Data)
				{
					if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
					{
						float Val;
						FMemory::Memcpy(&Val, Data, sizeof(float));
						Info.Value = FString::Printf(TEXT("%f"), Val);
					}
					else if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
					{
						int32 Val;
						FMemory::Memcpy(&Val, Data, sizeof(int32));
						Info.Value = FString::Printf(TEXT("%d"), Val);
					}
					else if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
					{
						Info.Value = (*Data != 0) ? TEXT("true") : TEXT("false");
					}
					else if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
					{
						FVector3f Val;
						FMemory::Memcpy(&Val, Data, sizeof(FVector3f));
						Info.Value = FString::Printf(TEXT("(%f, %f, %f)"), Val.X, Val.Y, Val.Z);
					}
					else if (TypeDef == FNiagaraTypeDefinition::GetVec4Def() || TypeDef == FNiagaraTypeDefinition::GetColorDef())
					{
						FVector4f Val;
						FMemory::Memcpy(&Val, Data, sizeof(FVector4f));
						Info.Value = FString::Printf(TEXT("(%f, %f, %f, %f)"), Val.X, Val.Y, Val.Z, Val.W);
					}
					else if (TypeDef.IsEnum())
					{
						int32 Val;
						FMemory::Memcpy(&Val, Data, sizeof(int32));
						// Try to get enum name
						if (UEnum* Enum = TypeDef.GetEnum())
						{
							Info.Value = Enum->GetNameStringByValue(Val);
						}
						else
						{
							Info.Value = FString::Printf(TEXT("%d"), Val);
						}
					}
					else
					{
						Info.Value = FString::Printf(TEXT("(raw %d bytes)"), Size);
					}
				}
			}

			Result.Add(Info);
		}
	};

	// Extract from all script types
	ExtractRIParams(EmitterData->EmitterSpawnScriptProps.Script, TEXT("EmitterSpawn"));
	ExtractRIParams(EmitterData->EmitterUpdateScriptProps.Script, TEXT("EmitterUpdate"));
	ExtractRIParams(EmitterData->SpawnScriptProps.Script, TEXT("ParticleSpawn"));
	ExtractRIParams(EmitterData->UpdateScriptProps.Script, TEXT("ParticleUpdate"));

	return Result;
}

bool UNiagaraService::SetRapidIterationParam(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& ParameterName,
	const FString& Value)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::SetRapidIterationParam - System not found: %s"), *SystemPath);
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::SetRapidIterationParam - Emitter '%s' not found"), *EmitterName);
		return false;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return false;
	}

	// Try to set the parameter in each script's rapid iteration parameter store
	bool bSuccess = false;
	FName ParamName(*ParameterName);

	auto TrySetInScript = [&](UNiagaraScript* Script, const FString& ScriptType) -> bool
	{
		if (!Script)
		{
			return false;
		}

		FNiagaraParameterStore& Store = Script->RapidIterationParameters;
		TArray<FNiagaraVariable> Params;
		Store.GetParameters(Params);

		for (const FNiagaraVariable& Var : Params)
		{
			if (Var.GetName() == ParamName)
			{
				int32 Offset = Store.IndexOf(Var);
				if (Offset == INDEX_NONE)
				{
					continue;
				}

				const FNiagaraTypeDefinition& TypeDef = Var.GetType();
				uint8* Data = const_cast<uint8*>(Store.GetParameterData(Offset, TypeDef));

				if (!Data)
				{
					continue;
				}

				// Parse and set the value based on type
				if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
				{
					float Val = FCString::Atof(*Value);
					FMemory::Memcpy(Data, &Val, sizeof(float));
					bSuccess = true;
				}
				else if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
				{
					int32 Val = FCString::Atoi(*Value);
					FMemory::Memcpy(Data, &Val, sizeof(int32));
					bSuccess = true;
				}
				else if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
				{
					bool Val = Value.ToBool();
					*Data = Val ? 1 : 0;
					bSuccess = true;
				}
				else if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
				{
					FVector3f Val;
					// Parse (X, Y, Z) format
					FString CleanValue = Value.Replace(TEXT("("), TEXT("")).Replace(TEXT(")"), TEXT(""));
					TArray<FString> Components;
					CleanValue.ParseIntoArray(Components, TEXT(","));
					if (Components.Num() == 3)
					{
						Val.X = FCString::Atof(*Components[0].TrimStartAndEnd());
						Val.Y = FCString::Atof(*Components[1].TrimStartAndEnd());
						Val.Z = FCString::Atof(*Components[2].TrimStartAndEnd());
						FMemory::Memcpy(Data, &Val, sizeof(FVector3f));
						bSuccess = true;
					}
				}
				else if (TypeDef == FNiagaraTypeDefinition::GetVec4Def() || TypeDef == FNiagaraTypeDefinition::GetColorDef())
				{
					FVector4f Val;
					// Parse (X, Y, Z, W) format
					FString CleanValue = Value.Replace(TEXT("("), TEXT("")).Replace(TEXT(")"), TEXT(""));
					TArray<FString> Components;
					CleanValue.ParseIntoArray(Components, TEXT(","));
					if (Components.Num() == 4)
					{
						Val.X = FCString::Atof(*Components[0].TrimStartAndEnd());
						Val.Y = FCString::Atof(*Components[1].TrimStartAndEnd());
						Val.Z = FCString::Atof(*Components[2].TrimStartAndEnd());
						Val.W = FCString::Atof(*Components[3].TrimStartAndEnd());
						FMemory::Memcpy(Data, &Val, sizeof(FVector4f));
						bSuccess = true;
					}
				}
				else if (TypeDef.IsEnum())
				{
					int32 Val = FCString::Atoi(*Value);
					FMemory::Memcpy(Data, &Val, sizeof(int32));
					bSuccess = true;
				}

				if (bSuccess)
				{
					UE_LOG(LogTemp, Log, TEXT("UNiagaraService::SetRapidIterationParam - Set %s = %s in %s"), *ParameterName, *Value, *ScriptType);
					return true;
				}
			}
		}
		return false;
	};

	// Try setting in all script types - use & to set in ALL scripts, not just the first match
	bool bSpawn = TrySetInScript(EmitterData->EmitterSpawnScriptProps.Script, TEXT("EmitterSpawn"));
	bool bEmitterUpdate = TrySetInScript(EmitterData->EmitterUpdateScriptProps.Script, TEXT("EmitterUpdate"));
	bool bParticleSpawn = TrySetInScript(EmitterData->SpawnScriptProps.Script, TEXT("ParticleSpawn"));
	bool bParticleUpdate = TrySetInScript(EmitterData->UpdateScriptProps.Script, TEXT("ParticleUpdate"));
	bSuccess = bSpawn || bEmitterUpdate || bParticleSpawn || bParticleUpdate;

	if (bSuccess)
	{
		// Mark as dirty - rapid iteration params don't need recompilation
		// DO NOT call ForceGraphToRecompileOnNextCheck() here as it can cause
		// crashes when the Niagara editor is open (race condition with PropertyEditor)
		System->Modify();
		System->MarkPackageDirty();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::SetRapidIterationParam - Parameter '%s' not found in any script"), *ParameterName);
	}

	return bSuccess;
}

bool UNiagaraService::SetRapidIterationParamByStage(
	const FString& SystemPath,
	const FString& EmitterName,
	const FString& ScriptType,
	const FString& ParameterName,
	const FString& Value)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::SetRapidIterationParamByStage - System not found: %s"), *SystemPath);
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::SetRapidIterationParamByStage - Emitter '%s' not found"), *EmitterName);
		return false;
	}

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return false;
	}

	// Get the target script based on ScriptType
	UNiagaraScript* TargetScript = nullptr;
	FString NormalizedType = ScriptType.ToLower();
	
	if (NormalizedType == TEXT("emitterspawn"))
	{
		TargetScript = EmitterData->EmitterSpawnScriptProps.Script;
	}
	else if (NormalizedType == TEXT("emitterupdate"))
	{
		TargetScript = EmitterData->EmitterUpdateScriptProps.Script;
	}
	else if (NormalizedType == TEXT("particlespawn"))
	{
		TargetScript = EmitterData->SpawnScriptProps.Script;
	}
	else if (NormalizedType == TEXT("particleupdate"))
	{
		TargetScript = EmitterData->UpdateScriptProps.Script;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::SetRapidIterationParamByStage - Invalid ScriptType '%s'. Use EmitterSpawn, EmitterUpdate, ParticleSpawn, or ParticleUpdate"), *ScriptType);
		return false;
	}

	if (!TargetScript)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::SetRapidIterationParamByStage - Script not found for stage '%s'"), *ScriptType);
		return false;
	}

	// Find and set the parameter
	bool bSuccess = false;
	FName ParamName(*ParameterName);
	FNiagaraParameterStore& Store = TargetScript->RapidIterationParameters;
	TArray<FNiagaraVariable> Params;
	Store.GetParameters(Params);

	for (const FNiagaraVariable& Var : Params)
	{
		if (Var.GetName() == ParamName)
		{
			int32 Offset = Store.IndexOf(Var);
			if (Offset == INDEX_NONE)
			{
				continue;
			}

			const FNiagaraTypeDefinition& TypeDef = Var.GetType();
			uint8* Data = const_cast<uint8*>(Store.GetParameterData(Offset, TypeDef));

			if (!Data)
			{
				continue;
			}

			// Parse and set the value based on type
			if (TypeDef == FNiagaraTypeDefinition::GetFloatDef())
			{
				float Val = FCString::Atof(*Value);
				FMemory::Memcpy(Data, &Val, sizeof(float));
				bSuccess = true;
			}
			else if (TypeDef == FNiagaraTypeDefinition::GetIntDef())
			{
				int32 Val = FCString::Atoi(*Value);
				FMemory::Memcpy(Data, &Val, sizeof(int32));
				bSuccess = true;
			}
			else if (TypeDef == FNiagaraTypeDefinition::GetBoolDef())
			{
				bool Val = Value.ToBool();
				*Data = Val ? 1 : 0;
				bSuccess = true;
			}
			else if (TypeDef == FNiagaraTypeDefinition::GetVec3Def())
			{
				FVector3f Val;
				FString CleanValue = Value.Replace(TEXT("("), TEXT("")).Replace(TEXT(")"), TEXT(""));
				TArray<FString> Components;
				CleanValue.ParseIntoArray(Components, TEXT(","));
				if (Components.Num() == 3)
				{
					Val.X = FCString::Atof(*Components[0].TrimStartAndEnd());
					Val.Y = FCString::Atof(*Components[1].TrimStartAndEnd());
					Val.Z = FCString::Atof(*Components[2].TrimStartAndEnd());
					FMemory::Memcpy(Data, &Val, sizeof(FVector3f));
					bSuccess = true;
				}
			}
			else if (TypeDef == FNiagaraTypeDefinition::GetVec4Def() || TypeDef == FNiagaraTypeDefinition::GetColorDef())
			{
				FVector4f Val;
				FString CleanValue = Value.Replace(TEXT("("), TEXT("")).Replace(TEXT(")"), TEXT(""));
				TArray<FString> Components;
				CleanValue.ParseIntoArray(Components, TEXT(","));
				if (Components.Num() == 4)
				{
					Val.X = FCString::Atof(*Components[0].TrimStartAndEnd());
					Val.Y = FCString::Atof(*Components[1].TrimStartAndEnd());
					Val.Z = FCString::Atof(*Components[2].TrimStartAndEnd());
					Val.W = FCString::Atof(*Components[3].TrimStartAndEnd());
					FMemory::Memcpy(Data, &Val, sizeof(FVector4f));
					bSuccess = true;
				}
			}
			else if (TypeDef.IsEnum())
			{
				int32 Val = FCString::Atoi(*Value);
				FMemory::Memcpy(Data, &Val, sizeof(int32));
				bSuccess = true;
			}

			if (bSuccess)
			{
				UE_LOG(LogTemp, Log, TEXT("UNiagaraService::SetRapidIterationParamByStage - Set %s = %s in %s"), *ParameterName, *Value, *ScriptType);
				break;
			}
		}
	}

	if (bSuccess)
	{
		// Mark as dirty - rapid iteration params don't need recompilation
		// DO NOT call ForceGraphToRecompileOnNextCheck() here as it can cause
		// crashes when the Niagara editor is open (race condition with PropertyEditor)
		System->Modify();
		System->MarkPackageDirty();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService::SetRapidIterationParamByStage - Parameter '%s' not found in %s"), *ParameterName, *ScriptType);
	}

	return bSuccess;
}

bool UNiagaraService::GetEmitterLifecycle(
	const FString& SystemPath,
	const FString& EmitterName,
	FNiagaraEmitterLifecycleInfo& OutInfo)
{
	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		return false;
	}

	FNiagaraEmitterHandle* Handle = FindEmitterHandle(System, EmitterName);
	if (!Handle)
	{
		UE_LOG(LogTemp, Warning, TEXT("UNiagaraService: Emitter '%s' not found in system"), *EmitterName);
		return false;
	}

	OutInfo.EmitterName = EmitterName;
	OutInfo.bIsEnabled = Handle->GetIsEnabled();

	FVersionedNiagaraEmitterData* EmitterData = Handle->GetEmitterData();
	if (!EmitterData)
	{
		return false;
	}

	// Get lifecycle parameters from EmitterUpdate script's RI params
	// These are typically named like "EmitterState.LoopBehavior", "EmitterState.LoopDuration", etc.
	UNiagaraScript* UpdateScript = EmitterData->EmitterUpdateScriptProps.Script;
	if (UpdateScript)
	{
		const FNiagaraParameterStore& Store = UpdateScript->RapidIterationParameters;
		TArray<FNiagaraVariable> Params;
		Store.GetParameters(Params);

		for (const FNiagaraVariable& Var : Params)
		{
			FString ParamName = Var.GetName().ToString();
			int32 Offset = Store.IndexOf(Var);
			if (Offset == INDEX_NONE)
			{
				continue;
			}

			const uint8* Data = Store.GetParameterData(Offset, Var.GetType());
			if (!Data)
			{
				continue;
			}

			// Look for lifecycle-related params (they often contain EmitterState in the name)
			if (ParamName.Contains(TEXT("LoopBehavior")))
			{
				int32 Val;
				FMemory::Memcpy(&Val, Data, sizeof(int32));
				// ENiagaraLoopBehavior: Once=0, Multiple=1, Infinite=2
				switch (Val)
				{
				case 0: OutInfo.LoopBehavior = TEXT("Once"); break;
				case 1: OutInfo.LoopBehavior = TEXT("Multiple"); break;
				case 2: OutInfo.LoopBehavior = TEXT("Infinite"); break;
				default: OutInfo.LoopBehavior = FString::Printf(TEXT("Unknown(%d)"), Val);
				}
			}
			else if (ParamName.Contains(TEXT("LoopCount")))
			{
				int32 Val;
				FMemory::Memcpy(&Val, Data, sizeof(int32));
				OutInfo.LoopCount = Val;
			}
			else if (ParamName.Contains(TEXT("LoopDuration")) && !ParamName.Contains(TEXT("Recalc")))
			{
				float Val;
				FMemory::Memcpy(&Val, Data, sizeof(float));
				OutInfo.LoopDuration = Val;
			}
			else if (ParamName.Contains(TEXT("LoopDelay")) && !ParamName.Contains(TEXT("Recalc")))
			{
				float Val;
				FMemory::Memcpy(&Val, Data, sizeof(float));
				OutInfo.LoopDelay = Val;
			}
			else if (ParamName.Contains(TEXT("LifeCycleMode")))
			{
				int32 Val;
				FMemory::Memcpy(&Val, Data, sizeof(int32));
				// ENiagaraEmitterInactiveMode: Self=0, System=1
				OutInfo.LifeCycleMode = (Val == 0) ? TEXT("Self") : TEXT("System");
			}
			else if (ParamName.Contains(TEXT("InactiveResponse")) || ParamName.Contains(TEXT("Inactive From Start")))
			{
				OutInfo.bInactiveFromStart = (*Data != 0);
			}
			else if (ParamName.Contains(TEXT("ScalabilityMode")))
			{
				int32 Val;
				FMemory::Memcpy(&Val, Data, sizeof(int32));
				OutInfo.ScalabilityMode = (Val == 0) ? TEXT("Self") : TEXT("System");
			}
		}

		OutInfo.RIParameterCount = Params.Num();
	}

	// Default values if not found in RI params
	if (OutInfo.LoopBehavior.IsEmpty())
	{
		OutInfo.LoopBehavior = TEXT("(default - check EmitterState module)");
	}
	if (OutInfo.LifeCycleMode.IsEmpty())
	{
		OutInfo.LifeCycleMode = TEXT("(default - Self)");
	}

	return true;
}

FString UNiagaraService::DebugActivation(const FString& SystemPath)
{
	FString Result;
	Result += FString::Printf(TEXT("=== Debug Activation for %s ===\n"), *SystemPath);

	UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
	if (!System)
	{
		Result += TEXT("ERROR: Failed to load system\n");
		return Result;
	}

	Result += FString::Printf(TEXT("System Name: %s\n"), *System->GetName());
	Result += FString::Printf(TEXT("Is Valid: %s\n"), System->IsValid() ? TEXT("Yes") : TEXT("No"));
	Result += FString::Printf(TEXT("Needs Recompile: %s\n"), System->HasOutstandingCompilationRequests() ? TEXT("Yes") : TEXT("No"));

	// Check effect type
	UNiagaraEffectType* EffectType = System->GetEffectType();
	Result += FString::Printf(TEXT("Effect Type: %s\n"), EffectType ? *EffectType->GetName() : TEXT("None"));

	// Check all emitters
	Result += FString::Printf(TEXT("\n--- Emitters (%d total) ---\n"), System->GetEmitterHandles().Num());

	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		FString EmitterName = Handle.GetUniqueInstanceName();
		bool bEnabled = Handle.GetIsEnabled();
		
		Result += FString::Printf(TEXT("\n[%s] Enabled: %s\n"), *EmitterName, bEnabled ? TEXT("Yes") : TEXT("No"));

		FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (EmitterData)
		{
			Result += FString::Printf(TEXT("  SimTarget: %s\n"), 
				EmitterData->SimTarget == ENiagaraSimTarget::CPUSim ? TEXT("CPU") : TEXT("GPU"));
			Result += FString::Printf(TEXT("  LocalSpace: %s\n"), EmitterData->bLocalSpace ? TEXT("Yes") : TEXT("No"));
			Result += FString::Printf(TEXT("  Determinism: %s\n"), EmitterData->bDeterminism ? TEXT("Yes") : TEXT("No"));

			// Check for EmitterState module in EmitterUpdate script
			UNiagaraScript* UpdateScript = EmitterData->EmitterUpdateScriptProps.Script;
			if (UpdateScript)
			{
				const FNiagaraParameterStore& Store = UpdateScript->RapidIterationParameters;
				TArray<FNiagaraVariable> Params;
				Store.GetParameters(Params);

				Result += FString::Printf(TEXT("  EmitterUpdate RI Params: %d\n"), Params.Num());

				// Look for key lifecycle params
				for (const FNiagaraVariable& Var : Params)
				{
					FString ParamName = Var.GetName().ToString();
					if (ParamName.Contains(TEXT("LoopBehavior")) || 
						ParamName.Contains(TEXT("LifeCycleMode")) ||
						ParamName.Contains(TEXT("Inactive")))
					{
						int32 Offset = Store.IndexOf(Var);
						if (Offset != INDEX_NONE)
						{
							const uint8* Data = Store.GetParameterData(Offset, Var.GetType());
							if (Data)
							{
								int32 Val;
								FMemory::Memcpy(&Val, Data, FMath::Min(Var.GetType().GetSize(), (int32)sizeof(int32)));
								Result += FString::Printf(TEXT("    %s = %d\n"), *ParamName, Val);
							}
						}
					}
				}
			}
			else
			{
				Result += TEXT("  WARNING: No EmitterUpdate script!\n");
			}

			// Check SpawnBurst/SpawnRate in EmitterUpdate
			if (UpdateScript)
			{
				const FNiagaraParameterStore& Store = UpdateScript->RapidIterationParameters;
				TArray<FNiagaraVariable> Params;
				Store.GetParameters(Params);

				for (const FNiagaraVariable& Var : Params)
				{
					FString ParamName = Var.GetName().ToString();
					if (ParamName.Contains(TEXT("SpawnRate")) || ParamName.Contains(TEXT("SpawnCount")))
					{
						int32 Offset = Store.IndexOf(Var);
						if (Offset != INDEX_NONE)
						{
							const uint8* Data = Store.GetParameterData(Offset, Var.GetType());
							if (Data)
							{
								float Val;
								FMemory::Memcpy(&Val, Data, sizeof(float));
								Result += FString::Printf(TEXT("    %s = %f\n"), *ParamName, Val);
							}
						}
					}
				}
			}
		}
	}

	// Summary
	Result += TEXT("\n--- Activation Analysis ---\n");
	if (!System->IsValid())
	{
		Result += TEXT("ISSUE: System is not valid - needs compilation or has errors\n");
	}
	if (System->GetEmitterHandles().Num() == 0)
	{
		Result += TEXT("ISSUE: No emitters in system\n");
	}

	bool bAnyEnabled = false;
	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (Handle.GetIsEnabled())
		{
			bAnyEnabled = true;
			break;
		}
	}
	if (!bAnyEnabled)
	{
		Result += TEXT("ISSUE: No enabled emitters\n");
	}

	return Result;
}