// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UNiagaraService.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraComponent.h"
#include "NiagaraTypes.h"
#include "NiagaraParameterStore.h"
#include "NiagaraDataInterface.h"
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
						if (UEnum* Enum = TypeDef.GetEnum())
						{
							FText DisplayName = Enum->GetDisplayNameTextByValue(Val);
							Info.Value = !DisplayName.IsEmpty() ? DisplayName.ToString() : Enum->GetNameStringByValue(Val);
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