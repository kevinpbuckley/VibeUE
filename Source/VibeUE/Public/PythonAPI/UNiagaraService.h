// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UNiagaraService.generated.h"

/**
 * A rapid iteration parameter with its value
 */
USTRUCT(BlueprintType)
struct FNiagaraRIParameterInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ParameterName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ParameterType;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString Value;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ScriptType;  // EmitterSpawn, EmitterUpdate, ParticleSpawn, ParticleUpdate
};

/**
 * Emitter lifecycle settings controlling loop behavior
 */
USTRUCT(BlueprintType)
struct FNiagaraEmitterLifecycleInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString EmitterName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bIsEnabled = true;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString LifeCycleMode;  // Self, System

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString LoopBehavior;  // Once, Multiple, Infinite

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 LoopCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	float LoopDuration = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	float LoopDelay = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bDurationRecalcEachLoop = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bDelayRecalcEachLoop = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bInactiveFromStart = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ScalabilityMode;  // Self, System

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 RIParameterCount = 0;  // Total rapid iteration parameters
};

/**
 * A single difference between two Niagara systems
 */
USTRUCT(BlueprintType)
struct FNiagaraPropertyDifference
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString PropertyName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SourceValue;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString TargetValue;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString EmitterName;  // Empty if system-level property
};

/**
 * Result of comparing two Niagara systems
 */
USTRUCT(BlueprintType)
struct FNiagaraSystemComparison
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SourcePath;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString TargetPath;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bAreEquivalent = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 DifferenceCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	TArray<FNiagaraPropertyDifference> Differences;

	// Summary counts
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 SourceEmitterCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 TargetEmitterCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	TArray<FString> EmittersOnlyInSource;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	TArray<FString> EmittersOnlyInTarget;
};

/**
 * Niagara Service - Python API for Niagara rapid-iteration tuning + diagnostics.
 *
 * This service was TRIMMED (issue #462): system / emitter / user-parameter
 * create / add / copy / duplicate / remove / move / compile are now owned by the
 * engine NiagaraToolsets (NiagaraToolset_System / _Assets / _Component / _Info) and
 * are reached with call_tool — they are NOT methods on this service. Scratch-pad /
 * Custom-HLSL graph authoring lives on NiagaraScratchPadService.
 *
 * Methods this service actually exposes:
 *
 * Rapid-iteration parameters (per-emitter script settings):
 * - list_rapid_iteration_params(system_path, emitter_name)
 * - set_rapid_iteration_param(system_path, emitter_name, param_name, value)
 * - set_rapid_iteration_param_by_stage(system_path, emitter_name, stage, param_name, value)
 *
 * Diagnostics:
 * - compare_systems(source_path, target_path)
 * - get_emitter_lifecycle(system_path, emitter_name)  # info struct or None
 * - debug_activation(system_path)
 *
 * For system/emitter/parameter CRUD + compile, discover the engine tools with
 * list_toolsets() / describe_toolset("NiagaraToolsets.NiagaraToolset_System") and
 * invoke via call_tool.
 *
 * Python Usage:
 *   import unreal
 *
 *   # Discover and tune an emitter's rapid-iteration parameters:
 *   params = unreal.NiagaraService.list_rapid_iteration_params("/Game/VFX/NS_Fire", "Flames")
 *   unreal.NiagaraService.set_rapid_iteration_param(
 *       "/Game/VFX/NS_Fire", "Flames", "Constants.flames.Color.Scale Color", "(0.0, 3.0, 0.0)")
 *
 *   # Diagnose:
 *   print(unreal.NiagaraService.debug_activation("/Game/VFX/NS_Fire"))
 */
UCLASS(BlueprintType)
class VIBEUE_API UNiagaraService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// =================================================================
	// Diagnostic Actions
	// =================================================================

	/**
	 * Compare two Niagara systems and return all differences.
	 * Compares system-level properties, emitter counts, emitter properties,
	 * and rapid iteration parameters.
	 *
	 * @param SourceSystemPath - Path to the source/reference system
	 * @param TargetSystemPath - Path to the target system to compare
	 * @return Comparison result with all differences
	 *
	 * Example:
	 *   comparison = unreal.NiagaraService.compare_systems(
	 *       "/Game/VFX/Source/NS_Fire",
	 *       "/Game/VFX/NS_Fire_Copy"
	 *   )
	 *   print(f"Differences: {comparison.difference_count}")
	 *   for diff in comparison.differences:
	 *       print(f"  {diff.property_name}: {diff.source_value} vs {diff.target_value}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (AICallable, DisplayName = "Compare Systems"))
	static FNiagaraSystemComparison CompareSystems(
		const FString& SourceSystemPath,
		const FString& TargetSystemPath);

	/**
	 * List all rapid iteration parameters for an emitter.
	 * These are the internal module parameters that control behavior like spawn rate, lifetime, etc.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @return Array of rapid iteration parameters with their values
	 *
	 * Example:
	 *   params = unreal.NiagaraService.list_rapid_iteration_params("/Game/VFX/NS_Fire", "Flames")
	 *   for p in params:
	 *       print(f"[{p.script_type}] {p.parameter_name}: {p.value}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (AICallable, DisplayName = "List Rapid Iteration Params"))
	static TArray<FNiagaraRIParameterInfo> ListRapidIterationParams(
		const FString& SystemPath,
		const FString& EmitterName);

	/**
	 * Set a rapid iteration parameter value directly.
	 * This is the easiest way to adjust emitter settings like spawn rate, lifetime, colors, etc.
	 * Use list_rapid_iteration_params() to discover available parameter names.
	 * NOTE: Sets ALL matching parameters across all script stages. Use set_rapid_iteration_param_by_stage
	 * to target a specific stage when the same parameter exists in multiple stages.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param ParameterName - Full parameter name (e.g., "Constants.sparks.SpawnRate.SpawnRate")
	 * @param Value - New value as string
	 * @return True if successful
	 *
	 * Example:
	 *   # List parameters to find the one you want
	 *   params = unreal.NiagaraService.list_rapid_iteration_params("/Game/VFX/NS_Fire", "Sparks")
	 *   # Set spawn rate directly
	 *   unreal.NiagaraService.set_rapid_iteration_param("/Game/VFX/NS_Fire", "Sparks",
	 *       "Constants.sparks.SpawnRate.SpawnRate", "500.0")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (AICallable, DisplayName = "Set Rapid Iteration Param"))
	static bool SetRapidIterationParam(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ParameterName,
		const FString& Value);

	/**
	 * Set a rapid iteration parameter in a specific script stage.
	 * Use when the same parameter name exists in multiple stages (e.g., Color.Scale Color in
	 * both ParticleSpawn and ParticleUpdate) and you need to set one specifically.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param ScriptType - Stage to set: "EmitterSpawn", "EmitterUpdate", "ParticleSpawn", or "ParticleUpdate"
	 * @param ParameterName - Full parameter name (e.g., "Constants.fire.Color.Scale Color")
	 * @param Value - New value as string
	 * @return True if successful
	 *
	 * Example:
	 *   # Set Scale Color only in ParticleUpdate stage (not ParticleSpawn)
	 *   unreal.NiagaraService.set_rapid_iteration_param_by_stage(
	 *       "/Game/VFX/NS_Fire", "fire", "ParticleUpdate",
	 *       "Constants.fire.Color.Scale Color", "0.0, 2.0, 0.0")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (AICallable, DisplayName = "Set Rapid Iteration Param By Stage"))
	static bool SetRapidIterationParamByStage(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ScriptType,
		const FString& ParameterName,
		const FString& Value);

	/**
	 * Get emitter lifecycle settings including loop behavior.
	 * This shows whether the emitter is set to loop infinitely, once, or multiple times.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param OutInfo - Lifecycle information
	 * @return True if successful
	 *
	 * Example:
	 *   info = unreal.NiagaraService.get_emitter_lifecycle("/Game/VFX/NS_Fire", "Flames")  # info struct or None
	 *   print(f"Loop Behavior: {info.loop_behavior}")
	 *   print(f"Life Cycle Mode: {info.life_cycle_mode}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (AICallable, DisplayName = "Get Emitter Lifecycle"))
	static bool GetEmitterLifecycle(
		const FString& SystemPath,
		const FString& EmitterName,
		FNiagaraEmitterLifecycleInfo& OutInfo);

	/**
	 * Debug activation state of a Niagara system by spawning it and checking behavior.
	 * Returns detailed information about why a system might not be playing.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @return Debug string with activation analysis
	 *
	 * Example:
	 *   debug_info = unreal.NiagaraService.debug_activation("/Game/VFX/NS_Fire")
	 *   print(debug_info)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (AICallable, DisplayName = "Debug Activation"))
	static FString DebugActivation(const FString& SystemPath);


private:
	// Helper methods
	static class UNiagaraSystem* LoadNiagaraSystem(const FString& SystemPath);
	static struct FNiagaraEmitterHandle* FindEmitterHandle(class UNiagaraSystem* System, const FString& EmitterName);
	static FString NiagaraVariableToString(const struct FNiagaraVariable& Variable);
	static FString NiagaraTypeToString(const struct FNiagaraTypeDefinition& TypeDef);
};
