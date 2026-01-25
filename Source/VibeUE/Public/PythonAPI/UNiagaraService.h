// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UNiagaraService.generated.h"

/**
 * Information about a Niagara parameter
 */
USTRUCT(BlueprintType)
struct FNiagaraParameterInfo_Custom
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ParameterName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ParameterType;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString Namespace;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString CurrentValue;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString DefaultValue;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bIsOverridden = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bIsUserExposed = false;
};

/**
 * Information about an emitter in a Niagara system
 */
USTRUCT(BlueprintType)
struct FNiagaraEmitterInfo_Custom
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString EmitterName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString UniqueEmitterName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SourceAssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bIsEnabled = true;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bIsIsolated = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SimTarget;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 ParameterCount = 0;
};

/**
 * Comprehensive Niagara system information
 */
USTRUCT(BlueprintType)
struct FNiagaraSystemInfo_Custom
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SystemName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SystemPath;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bIsValid = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 EmitterCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	TArray<FNiagaraEmitterInfo_Custom> Emitters;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	TArray<FNiagaraParameterInfo_Custom> UserParameters;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bNeedsRecompile = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString FixedBounds;
};

/**
 * Result of a Niagara system compilation
 */
USTRUCT(BlueprintType)
struct FNiagaraCompilationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SystemPath;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 ErrorCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 WarningCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	TArray<FString> Errors;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	TArray<FString> Warnings;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	float CompilationTimeSeconds = 0.0f;
};

/**
 * Detailed system-level properties for diagnostics
 */
USTRUCT(BlueprintType)
struct FNiagaraSystemPropertiesInfo
{
	GENERATED_BODY()

	// Basic Info
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SystemName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SystemPath;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 EmitterCount = 0;

	// Effect Type
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString EffectTypeName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString EffectTypePath;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString CullReaction;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString UpdateFrequency;

	// Determinism
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bDeterminism = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 RandomSeed = 0;

	// Warmup
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	float WarmupTime = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 WarmupTickCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	float WarmupTickDelta = 0.0f;

	// Bounds
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bFixedBounds = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString FixedBoundsValue;

	// Rendering
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bSupportLargeWorldCoordinates = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bCastShadow = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bReceivesDecals = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bRenderCustomDepth = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 TranslucencySortPriority = 0;

	// Performance
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bBakeOutRapidIteration = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bCompressAttributes = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bTrimAttributes = false;

	// Scalability
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bOverrideScalabilitySettings = false;

	// Debug
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bDumpDebugSystemInfo = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bDumpDebugEmitterInfo = false;
};

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
 * Result of Niagara system creation operations
 */
USTRUCT(BlueprintType)
struct FNiagaraCreateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ErrorMessage;
};

/**
 * AI-friendly summary of a Niagara system
 */
USTRUCT(BlueprintType)
struct FNiagaraSystemSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SystemPath;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SystemName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 EmitterCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	TArray<FString> EmitterNames;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 UserParameterCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	TArray<FString> UserParameterNames;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bHasGPUEmitters = false;
};

/**
 * Niagara Service - Python API for Niagara System manipulation
 *
 * Provides Niagara system management actions:
 *
 * Lifecycle:
 * - create_system: Create a new Niagara System asset
 * - save_system: Save system to disk
 * - compile_system: Compile/rebuild system
 * - compile_with_results: Compile and get detailed error/warning messages
 * - open_in_editor: Open system in Niagara Editor
 *
 * Information:
 * - get_system_info: Get comprehensive system information
 * - summarize: Get AI-friendly system summary
 * - list_emitters: List all emitters in system
 *
 * Emitter Management:
 * - add_emitter: Add emitter to system (from template or minimal)
 * - list_emitter_templates: List available emitter templates
 * - copy_emitter: Copy emitter from one system to another
 * - remove_emitter: Remove emitter from system
 * - enable_emitter: Enable/disable emitter
 * - duplicate_emitter: Duplicate an emitter within system
 * - rename_emitter: Rename an emitter
 * - move_emitter: Reorder/reposition an emitter in the graph
 *
 * Parameter Management:
 * - list_parameters: List all user-exposed parameters
 * - get_parameter: Get a parameter value
 * - set_parameter: Set a parameter value
 * - add_user_parameter: Add a new user-exposed parameter
 * - remove_user_parameter: Remove a user parameter
 *
 * Existence Checks:
 * - system_exists: Check if system exists
 * - emitter_exists: Check if emitter exists in system
 * - parameter_exists: Check if parameter exists
 *
 * Search:
 * - search_systems: Search for Niagara systems
 * - search_emitters: Search for standalone emitter assets
 *
 * Python Usage:
 *   import unreal
 *
 *   # Create a system
 *   result = unreal.NiagaraService.create_system("NS_Fire", "/Game/VFX")
 *
 *   # Add minimal emitter (like UI's "Add Minimal Emitter" button)
 *   unreal.NiagaraService.add_emitter(result.asset_path, "minimal", "Flames")
 *
 *   # Or add existing emitter template
 *   templates = unreal.NiagaraService.list_emitter_templates("", "Fountain")
 *   unreal.NiagaraService.add_emitter(result.asset_path, templates[0], "Fountain")
 *
 *   # Set user parameter
 *   unreal.NiagaraService.set_parameter(result.asset_path, "User.SpawnRate", "100.0")
 */
UCLASS(BlueprintType)
class VIBEUE_API UNiagaraService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Lifecycle Actions
	// =================================================================

	/**
	 * Create a new Niagara System asset.
	 *
	 * @param SystemName - Name for the new system (e.g., "NS_Fire")
	 * @param DestinationPath - Path where to create (e.g., "/Game/VFX")
	 * @param TemplateAssetPath - Optional Niagara System or Emitter asset to use as a template
	 * @return Create result with asset path
	 *
	 * Example:
	 *   # Empty system
	 *   result = unreal.NiagaraService.create_system("NS_Fire", "/Game/VFX")
	 *
	 *   # From template system
	 *   result = unreal.NiagaraService.create_system(
	 *       "NS_Fire", "/Game/VFX", "/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Create Niagara System"))
	static FNiagaraCreateResult CreateSystem(
		const FString& SystemName,
		const FString& DestinationPath,
		const FString& TemplateAssetPath = TEXT(""));

	/**
	 * Save a Niagara System to disk.
	 *
	 * @param SystemPath - Full path to the system
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraService.save_system("/Game/VFX/NS_Fire")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Save Niagara System"))
	static bool SaveSystem(const FString& SystemPath);

	/**
	 * Compile a Niagara System.
	 *
	 * @param SystemPath - Full path to the system
	 * @param bWaitForCompletion - Whether to wait for compilation to finish
	 * @return True if compilation started/completed successfully
	 *
	 * Example:
	 *   unreal.NiagaraService.compile_system("/Game/VFX/NS_Fire")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Compile Niagara System"))
	static bool CompileSystem(const FString& SystemPath, bool bWaitForCompletion = true);

	/**
	 * Compile a Niagara System and return detailed compilation results with errors.
	 * This method waits for compilation to complete and captures all error and warning messages.
	 *
	 * @param SystemPath - Full path to the system
	 * @return Compilation result with success status, errors, and warnings
	 *
	 * Example:
	 *   result = unreal.NiagaraService.compile_with_results("/Game/VFX/NS_Fire")
	 *   if not result.success:
	 *       print(f"Compilation failed with {result.error_count} errors:")
	 *       for error in result.errors:
	 *           print(f"  - {error}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Compile Niagara System With Results"))
	static FNiagaraCompilationResult CompileWithResults(const FString& SystemPath);

	/**
	 * Open a Niagara System in the Niagara Editor.
	 *
	 * @param SystemPath - Full path to the system
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraService.open_in_editor("/Game/VFX/NS_Fire")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Open In Niagara Editor"))
	static bool OpenInEditor(const FString& SystemPath);

	/**
	 * Copy system-level properties from a source system to a target system.
	 * Copies effect type, determinism, warmup settings, bounds, and rendering properties.
	 * Does NOT copy emitters - only system-level settings.
	 *
	 * @param TargetSystemPath - Full path to the target system
	 * @param SourceSystemPath - Full path to the source system to copy from
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraService.copy_system_properties("/Game/VFX/NS_Fire", "/Game/VFX/Source/NS_Fire_Original")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Copy System Properties"))
	static bool CopySystemProperties(
		const FString& TargetSystemPath,
		const FString& SourceSystemPath);

	// =================================================================
	// Information Actions
	// =================================================================

	/**
	 * Get comprehensive Niagara System information.
	 *
	 * @param SystemPath - Full path to the system
	 * @param OutInfo - Structure containing all system details
	 * @return True if successful
	 *
	 * Example:
	 *   success, info = unreal.NiagaraService.get_system_info("/Game/VFX/NS_Fire")
	 *   print(f"Emitter count: {info.emitter_count}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Get Niagara System Info"))
	static bool GetSystemInfo(const FString& SystemPath, FNiagaraSystemInfo_Custom& OutInfo);

	/**
	 * Get an AI-friendly summary of a Niagara System.
	 *
	 * @param SystemPath - Full path to the system
	 * @param OutSummary - AI-friendly system summary
	 * @return True if successful
	 *
	 * Example:
	 *   success, summary = unreal.NiagaraService.summarize("/Game/VFX/NS_Fire")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Summarize Niagara System"))
	static bool Summarize(const FString& SystemPath, FNiagaraSystemSummary& OutSummary);

	/**
	 * List all emitters in a Niagara System.
	 *
	 * @param SystemPath - Full path to the system
	 * @return Array of emitter information
	 *
	 * Example:
	 *   emitters = unreal.NiagaraService.list_emitters("/Game/VFX/NS_Fire")
	 *   for e in emitters:
	 *       print(e.emitter_name)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "List Emitters"))
	static TArray<FNiagaraEmitterInfo_Custom> ListEmitters(const FString& SystemPath);

	// =================================================================
	// Emitter Management Actions
	// =================================================================

	/**
	 * Add an emitter to a Niagara System.
	 * Works like Unreal's "Add Emitter to your System" dialog.
	 *
	 * @param SystemPath - Full path to the system
	 * @param EmitterAssetPath - Path to emitter asset, or empty/"minimal" for a minimal empty emitter
	 * @param EmitterName - Optional custom name for the emitter
	 * @return Name of the added emitter (empty on failure)
	 *
	 * Example:
	 *   # Add existing emitter
	 *   name = unreal.NiagaraService.add_emitter("/Game/VFX/NS_Fire", "/Game/VFX/NE_Sparks")
	 *
	 *   # Add minimal empty emitter (like UI's "Add Minimal Emitter" button)
	 *   name = unreal.NiagaraService.add_emitter("/Game/VFX/NS_Fire", "minimal")
	 *   name = unreal.NiagaraService.add_emitter("/Game/VFX/NS_Fire", "")  # Also creates minimal
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Add Emitter"))
	static FString AddEmitter(
		const FString& SystemPath,
		const FString& EmitterAssetPath,
		const FString& EmitterName = TEXT(""));

	/**
	 * List available Niagara emitter templates from the asset registry.
	 * Returns emitters that can be added to systems.
	 *
	 * @param SearchPath - Path to search in (default: searches all paths including engine content)
	 * @param NameFilter - Optional filter for emitter names
	 * @return Array of emitter asset paths
	 *
	 * Example:
	 *   # List all emitters
	 *   emitters = unreal.NiagaraService.list_emitter_templates()
	 *
	 *   # List emitters in specific path
	 *   emitters = unreal.NiagaraService.list_emitter_templates("/Game/VFX")
	 *
	 *   # Filter by name
	 *   emitters = unreal.NiagaraService.list_emitter_templates("", "Smoke")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "List Emitter Templates"))
	static TArray<FString> ListEmitterTemplates(
		const FString& SearchPath = TEXT(""),
		const FString& NameFilter = TEXT(""));

	/**
	 * Copy an emitter from one Niagara System to another.
	 * Works like copy/paste in the Niagara Editor - copies the emitter with all its
	 * modules, renderers, and settings to the target system.
	 *
	 * @param SourceSystemPath - Full path to the source system
	 * @param SourceEmitterName - Name of emitter to copy
	 * @param TargetSystemPath - Full path to the target system
	 * @param NewEmitterName - Optional new name for the copied emitter (uses original name if empty)
	 * @return Name of the added emitter (empty on failure)
	 *
	 * Example:
	 *   # Copy emitter from one system to another
	 *   name = unreal.NiagaraService.copy_emitter(
	 *       "/Game/VFX/NS_Source", "Flames",
	 *       "/Game/VFX/NS_Target", "CopiedFlames")
	 *
	 *   # Copy with same name
	 *   name = unreal.NiagaraService.copy_emitter(
	 *       "/Game/VFX/NS_Source", "Flames",
	 *       "/Game/VFX/NS_Target")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Copy Emitter"))
	static FString CopyEmitter(
		const FString& SourceSystemPath,
		const FString& SourceEmitterName,
		const FString& TargetSystemPath,
		const FString& NewEmitterName = TEXT(""));

	/**
	 * Remove an emitter from a Niagara System.
	 *
	 * @param SystemPath - Full path to the system
	 * @param EmitterName - Name of emitter to remove
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraService.remove_emitter("/Game/VFX/NS_Fire", "Sparks")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Remove Emitter"))
	static bool RemoveEmitter(const FString& SystemPath, const FString& EmitterName);

	/**
	 * Enable or disable an emitter.
	 *
	 * @param SystemPath - Full path to the system
	 * @param EmitterName - Name of emitter
	 * @param bEnabled - Whether to enable or disable
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraService.enable_emitter("/Game/VFX/NS_Fire", "Sparks", False)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Enable Emitter"))
	static bool EnableEmitter(const FString& SystemPath, const FString& EmitterName, bool bEnabled);

	/**
	 * Duplicate an emitter within the same system.
	 *
	 * @param SystemPath - Full path to the system
	 * @param SourceEmitterName - Name of emitter to duplicate
	 * @param NewEmitterName - Name for the duplicate
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraService.duplicate_emitter("/Game/VFX/NS_Fire", "Sparks", "Sparks2")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Duplicate Emitter"))
	static bool DuplicateEmitter(
		const FString& SystemPath,
		const FString& SourceEmitterName,
		const FString& NewEmitterName);

	/**
	 * Rename an emitter in a system.
	 *
	 * @param SystemPath - Full path to the system
	 * @param CurrentName - Current emitter name
	 * @param NewName - New emitter name
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraService.rename_emitter("/Game/VFX/NS_Fire", "Sparks", "FireSparks")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Rename Emitter"))
	static bool RenameEmitter(
		const FString& SystemPath,
		const FString& CurrentName,
		const FString& NewName);

	/**
	 * Move/reorder an emitter to a new position in the system.
	 * Changes the visual order of emitters in the Niagara Editor graph.
	 *
	 * @param SystemPath - Full path to the system
	 * @param EmitterName - Name of emitter to move
	 * @param NewIndex - Target index (0-based). Use 0 to move to top, or large number to move to bottom
	 * @return True if successful
	 *
	 * Example:
	 *   # Move "green_smoke" to position 0 (top)
	 *   unreal.NiagaraService.move_emitter("/Game/VFX/NS_Fire", "green_smoke", 0)
	 *
	 *   # Move "Sparks" to position 2
	 *   unreal.NiagaraService.move_emitter("/Game/VFX/NS_Fire", "Sparks", 2)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Move Emitter"))
	static bool MoveEmitter(
		const FString& SystemPath,
		const FString& EmitterName,
		int32 NewIndex);

	/**
	 * Get the visual position of an emitter in the Niagara graph editor.
	 *
	 * @param SystemPath - Full path to the system
	 * @param EmitterName - Name of emitter
	 * @param OutX - Output X position
	 * @param OutY - Output Y position
	 * @return True if successful
	 *
	 * Example:
	 *   # Get position (Python returns tuple of (x, y))
	 *   pos = unreal.NiagaraService.get_emitter_graph_position("/Game/VFX/NS_Fire", "green_smoke")
	 *   print(f"Position: ({pos[0]}, {pos[1]})")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Get Emitter Graph Position"))
	static bool GetEmitterGraphPosition(
		const FString& SystemPath,
		const FString& EmitterName,
		float& OutX,
		float& OutY);

	/**
	 * Set the visual position of an emitter in the Niagara graph editor.
	 * Moves the emitter node to a new XY position in the graph.
	 *
	 * @param SystemPath - Full path to the system
	 * @param EmitterName - Name of emitter to move
	 * @param X - New X position
	 * @param Y - New Y position
	 * @return True if successful
	 *
	 * Example:
	 *   # Get current position
	 *   success, x, y = unreal.NiagaraService.get_emitter_graph_position("/Game/VFX/NS_Fire", "green_smoke")
	 *   
	 *   # Move 100 pixels to the right
	 *   unreal.NiagaraService.set_emitter_graph_position("/Game/VFX/NS_Fire", "green_smoke", x + 100, y)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Set Emitter Graph Position"))
	static bool SetEmitterGraphPosition(
		const FString& SystemPath,
		const FString& EmitterName,
		float X,
		float Y);

	// =================================================================
	// Parameter Management Actions
	// =================================================================

	/**
	 * List all user-exposed parameters in a Niagara System.
	 *
	 * @param SystemPath - Full path to the system
	 * @return Array of parameter information
	 *
	 * Example:
	 *   params = unreal.NiagaraService.list_parameters("/Game/VFX/NS_Fire")
	 *   for p in params:
	 *       print(f"{p.parameter_name}: {p.current_value}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "List Parameters"))
	static TArray<FNiagaraParameterInfo_Custom> ListParameters(const FString& SystemPath);

	/**
	 * Get a parameter value from a Niagara System.
	 *
	 * @param SystemPath - Full path to the system
	 * @param ParameterName - Full parameter name (e.g., "User.SpawnRate")
	 * @param OutInfo - Parameter information with current value
	 * @return True if found
	 *
	 * Example:
	 *   success, info = unreal.NiagaraService.get_parameter("/Game/VFX/NS_Fire", "User.SpawnRate")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Get Parameter"))
	static bool GetParameter(
		const FString& SystemPath,
		const FString& ParameterName,
		FNiagaraParameterInfo_Custom& OutInfo);

	/**
	 * Set a parameter value on a Niagara System.
	 *
	 * @param SystemPath - Full path to the system
	 * @param ParameterName - Full parameter name (e.g., "User.SpawnRate")
	 * @param Value - New value as string
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraService.set_parameter("/Game/VFX/NS_Fire", "User.SpawnRate", "100.0")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Set Parameter"))
	static bool SetParameter(
		const FString& SystemPath,
		const FString& ParameterName,
		const FString& Value);

	/**
	 * Add a user-exposed parameter to a Niagara System.
	 *
	 * @param SystemPath - Full path to the system
	 * @param ParameterName - Name for the parameter (without namespace)
	 * @param ParameterType - Type (Float, Int, Bool, Vector, Color, etc.)
	 * @param DefaultValue - Default value as string
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraService.add_user_parameter("/Game/VFX/NS_Fire", "SpawnRate", "Float", "50.0")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Add User Parameter"))
	static bool AddUserParameter(
		const FString& SystemPath,
		const FString& ParameterName,
		const FString& ParameterType,
		const FString& DefaultValue = TEXT(""));

	/**
	 * Remove a user parameter from a Niagara System.
	 *
	 * @param SystemPath - Full path to the system
	 * @param ParameterName - Parameter name to remove
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraService.remove_user_parameter("/Game/VFX/NS_Fire", "SpawnRate")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Remove User Parameter"))
	static bool RemoveUserParameter(
		const FString& SystemPath,
		const FString& ParameterName);

	// =================================================================
	// Existence Checks
	// =================================================================

	/**
	 * Check if a Niagara System exists at the given path.
	 *
	 * @param SystemPath - Full path to the system
	 * @return True if system exists
	 *
	 * Example:
	 *   if unreal.NiagaraService.system_exists("/Game/VFX/NS_Fire"):
	 *       print("System exists!")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Exists")
	static bool SystemExists(const FString& SystemPath);

	/**
	 * Check if an emitter exists in a Niagara System.
	 *
	 * @param SystemPath - Full path to the system
	 * @param EmitterName - Name of the emitter
	 * @return True if emitter exists in system
	 *
	 * Example:
	 *   if unreal.NiagaraService.emitter_exists("/Game/VFX/NS_Fire", "Sparks"):
	 *       print("Emitter exists!")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Exists")
	static bool EmitterExists(const FString& SystemPath, const FString& EmitterName);

	/**
	 * Check if a parameter exists in a Niagara System.
	 *
	 * @param SystemPath - Full path to the system
	 * @param ParameterName - Full parameter name
	 * @return True if parameter exists
	 *
	 * Example:
	 *   if unreal.NiagaraService.parameter_exists("/Game/VFX/NS_Fire", "User.SpawnRate"):
	 *       print("Parameter exists!")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Exists")
	static bool ParameterExists(const FString& SystemPath, const FString& ParameterName);

	// =================================================================
	// Search Actions
	// =================================================================

	/**
	 * Search for Niagara Systems in the project.
	 *
	 * @param SearchPath - Path to search in (default: /Game)
	 * @param NameFilter - Optional filter for system names
	 * @return Array of system paths
	 *
	 * Example:
	 *   systems = unreal.NiagaraService.search_systems("/Game/VFX", "Fire")
	 *   for s in systems:
	 *       print(s)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Search Systems"))
	static TArray<FString> SearchSystems(
		const FString& SearchPath = TEXT("/Game"),
		const FString& NameFilter = TEXT(""));

	/**
	 * Search for standalone Niagara Emitter assets.
	 *
	 * @param SearchPath - Path to search in (default: /Game)
	 * @param NameFilter - Optional filter for emitter names
	 * @return Array of emitter asset paths
	 *
	 * Example:
	 *   emitters = unreal.NiagaraService.search_emitter_assets("/Game/VFX", "Smoke")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara", meta = (DisplayName = "Search Emitter Assets"))
	static TArray<FString> SearchEmitterAssets(
		const FString& SearchPath = TEXT("/Game"),
		const FString& NameFilter = TEXT(""));

	// =================================================================
	// Diagnostic Actions
	// =================================================================

	/**
	 * Get detailed system-level properties for diagnostics.
	 * Returns comprehensive information about system settings including
	 * effect type, determinism, warmup, bounds, rendering, and performance.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param OutProperties - Detailed system properties
	 * @return True if successful
	 *
	 * Example:
	 *   success, props = unreal.NiagaraService.get_system_properties("/Game/VFX/NS_Fire")
	 *   print(f"Effect Type: {props.effect_type_name}")
	 *   print(f"Determinism: {props.determinism}")
	 *   print(f"Warmup: {props.warmup_time}s")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (DisplayName = "Get System Properties"))
	static bool GetSystemProperties(
		const FString& SystemPath,
		FNiagaraSystemPropertiesInfo& OutProperties);

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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (DisplayName = "Compare Systems"))
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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (DisplayName = "List Rapid Iteration Params"))
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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (DisplayName = "Set Rapid Iteration Param"))
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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (DisplayName = "Set Rapid Iteration Param By Stage"))
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
	 *   success, info = unreal.NiagaraService.get_emitter_lifecycle("/Game/VFX/NS_Fire", "Flames")
	 *   print(f"Loop Behavior: {info.loop_behavior}")
	 *   print(f"Life Cycle Mode: {info.life_cycle_mode}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (DisplayName = "Get Emitter Lifecycle"))
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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Niagara|Diagnostics", meta = (DisplayName = "Debug Activation"))
	static FString DebugActivation(const FString& SystemPath);

private:
	// Helper methods
	static class UNiagaraSystem* LoadNiagaraSystem(const FString& SystemPath);
	static class UNiagaraEmitter* LoadNiagaraEmitter(const FString& EmitterPath);
	static struct FNiagaraEmitterHandle* FindEmitterHandle(class UNiagaraSystem* System, const FString& EmitterName);
	static FString NiagaraVariableToString(const struct FNiagaraVariable& Variable);
	static FString NiagaraTypeToString(const struct FNiagaraTypeDefinition& TypeDef);
};
