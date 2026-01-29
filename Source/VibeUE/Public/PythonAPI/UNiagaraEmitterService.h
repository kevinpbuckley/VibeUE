// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UNiagaraEmitterService.generated.h"

/**
 * A single keyframe in a color curve (RGBA values at a specific time)
 */
USTRUCT(BlueprintType)
struct FNiagaraColorCurveKey
{
	GENERATED_BODY()

	/** Time position of this keyframe (typically 0.0 to 1.0 for particle lifetime) */
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	float Time = 0.0f;

	/** Red channel value (can be >1.0 for HDR) */
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	float R = 1.0f;

	/** Green channel value (can be >1.0 for HDR) */
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	float G = 1.0f;

	/** Blue channel value (can be >1.0 for HDR) */
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	float B = 1.0f;

	/** Alpha channel value (0.0 to 1.0) */
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	float A = 1.0f;
};

/**
 * Information about a module input
 */
USTRUCT(BlueprintType)
struct FNiagaraModuleInputInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString InputName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString InputType;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString CurrentValue;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString DefaultValue;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bIsLinked = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString LinkedSource;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bIsEditable = true;
};

/**
 * Information about a module in an emitter
 */
USTRUCT(BlueprintType)
struct FNiagaraModuleInfo_Custom
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ModuleName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ModuleType;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ScriptAssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 ModuleIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bIsEnabled = true;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	TArray<FNiagaraModuleInputInfo> Inputs;
};

/**
 * Information about a renderer in an emitter
 */
USTRUCT(BlueprintType)
struct FNiagaraRendererInfo_Custom
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString RendererName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString RendererType;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 RendererIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bIsEnabled = true;
};

/**
 * Detailed renderer information including material and texture bindings
 */
USTRUCT(BlueprintType)
struct FNiagaraRendererDetailedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString RendererName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString RendererType;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 RendererIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bIsEnabled = true;

	// Material info
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString MaterialPath;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bHasMaterial = false;

	// Sprite-specific
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SubImageSize;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString Alignment;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString FacingMode;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SortMode;

	// Mesh-specific
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString MeshPath;

	// Ribbon-specific
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString RibbonShape;
};

/**
 * Emitter lifecycle and property information
 */
USTRUCT(BlueprintType)
struct FNiagaraEmitterPropertiesInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString EmitterName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bIsEnabled = true;

	// Simulation settings
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString SimTarget;  // "CPUSim" or "GPUComputeSim"

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bLocalSpace = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	bool bDeterminism = false;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 RandomSeed = 0;

	// Bounds
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString CalculateBoundsMode;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString FixedBounds;

	// Allocation
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString AllocationMode;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	int32 PreAllocationCount = 0;

	// Lifecycle from EmitterState module
	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString LoopBehavior;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString LoopDuration;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString InactiveResponse;
};

/**
 * Information about a Niagara script asset
 */
USTRUCT(BlueprintType)
struct FNiagaraScriptInfo_Custom
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ScriptName;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ScriptPath;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString ScriptUsage;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	FString Description;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	TArray<FString> InputNames;

	UPROPERTY(BlueprintReadWrite, Category = "Niagara")
	TArray<FString> OutputNames;
};

/**
 * Niagara Emitter Service - Python API for module-level emitter manipulation
 *
 * Provides 17 module and renderer management actions:
 *
 * Module Management:
 * - list_modules: List all modules in an emitter
 * - get_module_info: Get detailed module information
 * - add_module: Add a module to an emitter
 * - remove_module: Remove a module from an emitter
 * - enable_module: Enable/disable a module
 * - set_module_input: Set a module input value
 * - get_module_input: Get a module input value
 * - reorder_module: Reorder a module within its stack
 * - set_color_tint: Set color tint (handles ColorFromCurve automatically)
 *
 * Renderer Management:
 * - list_renderers: List all renderers in an emitter
 * - add_renderer: Add a renderer to an emitter
 * - remove_renderer: Remove a renderer from an emitter
 * - enable_renderer: Enable/disable a renderer
 * - set_renderer_property: Set a renderer property
 *
 * Script Discovery:
 * - search_module_scripts: Search for module scripts
 * - get_script_info: Get script asset information
 * - list_builtin_modules: List built-in module scripts
 *
 * Python Usage:
 *   import unreal
 *
 *   # List modules
 *   modules = unreal.NiagaraEmitterService.list_modules("/Game/VFX/NS_Fire", "Sparks", "Update")
 *
 *   # Add a module
 *   unreal.NiagaraEmitterService.add_module("/Game/VFX/NS_Fire", "Sparks",
 *       "/Niagara/Modules/Update/Size/ScaleSpriteSize", "Update")
 *
 *   # Set color tint (works even with ColorFromCurve)
 *   unreal.NiagaraEmitterService.set_color_tint("/Game/VFX/NS_Fire", "Flames", "(0.0, 3.0, 0.0)")
 */
UCLASS(BlueprintType)
class VIBEUE_API UNiagaraEmitterService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Module Management Actions
	// =================================================================

	/**
	 * List all modules in an emitter.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param ModuleType - Filter by type: "Spawn", "Update", "Event", or empty for all
	 * @return Array of module information
	 *
	 * Example:
	 *   modules = unreal.NiagaraEmitterService.list_modules("/Game/VFX/NS_Fire", "Sparks", "Update")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "List Modules"))
	static TArray<FNiagaraModuleInfo_Custom> ListModules(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleType = TEXT(""));

	/**
	 * Get detailed information about a specific module.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param ModuleName - Name of the module
	 * @param OutInfo - Module information
	 * @return True if found
	 *
	 * Example:
	 *   success, info = unreal.NiagaraEmitterService.get_module_info("/Game/VFX/NS_Fire", "Sparks", "Initialize Particle")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Get Module Info"))
	static bool GetModuleInfo(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		FNiagaraModuleInfo_Custom& OutInfo);

	/**
	 * Add a module to an emitter.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param ModuleScriptPath - Path to the module script asset
	 * @param ModuleType - Target stack: "Spawn", "Update", "Event"
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraEmitterService.add_module("/Game/VFX/NS_Fire", "Sparks",
	 *       "/Niagara/Modules/Update/Size/ScaleSpriteSize", "Update")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Add Module"))
	static bool AddModule(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleScriptPath,
		const FString& ModuleType);

	/**
	 * Remove a module from an emitter.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param ModuleName - Name of the module to remove
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraEmitterService.remove_module("/Game/VFX/NS_Fire", "Sparks", "Scale Sprite Size")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Remove Module"))
	static bool RemoveModule(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName);

	/**
	 * Enable or disable a module.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param ModuleName - Name of the module
	 * @param bEnabled - Whether to enable or disable
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraEmitterService.enable_module("/Game/VFX/NS_Fire", "Sparks", "Scale Sprite Size", False)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Enable Module"))
	static bool EnableModule(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		bool bEnabled);

	/**
	 * Set a module input value.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param ModuleName - Name of the module
	 * @param InputName - Name of the input parameter
	 * @param Value - New value as string
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraEmitterService.set_module_input("/Game/VFX/NS_Fire", "Sparks",
	 *       "Scale Sprite Size", "Scale Factor", "(X=2,Y=2)")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Set Module Input"))
	static bool SetModuleInput(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& InputName,
		const FString& Value);

	/**
	 * Get a module input value.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param ModuleName - Name of the module
	 * @param InputName - Name of the input parameter
	 * @return Value as string (empty if not found)
	 *
	 * Example:
	 *   value = unreal.NiagaraEmitterService.get_module_input("/Game/VFX/NS_Fire", "Sparks",
	 *       "Scale Sprite Size", "Scale Factor")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Get Module Input"))
	static FString GetModuleInput(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		const FString& InputName);

	/**
	 * Reorder a module within its stack.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param ModuleName - Name of the module
	 * @param NewIndex - New position index
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraEmitterService.reorder_module("/Game/VFX/NS_Fire", "Sparks", "Scale Sprite Size", 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Reorder Module"))
	static bool ReorderModule(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName,
		int32 NewIndex);

	/**
	 * Set a color tint on an emitter, handling ColorFromCurve modules automatically.
	 *
	 * This method adds a ScaleColor module (if needed) and sets its Scale RGB value.
	 * Works even when ColorFromCurve is present - the tint multiplies with the curve output.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param RGB - Color as "(R, G, B)" string. Values >1 make colors brighter.
	 * @param Alpha - Optional alpha scale (default 1.0)
	 * @return True if successful
	 *
	 * Example:
	 *   # Make fire green (works even with ColorFromCurve)
	 *   unreal.NiagaraEmitterService.set_color_tint("/Game/VFX/NS_Fire", "Flames", "(0.0, 3.0, 0.0)")
	 *
	 *   # Make smoke purple with 50% alpha
	 *   unreal.NiagaraEmitterService.set_color_tint("/Game/VFX/NS_Fire", "Smoke", "(2.0, 0.0, 2.0)", 0.5)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Set Color Tint"))
	static bool SetColorTint(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& RGB,
		float Alpha = 1.0f);

	/**
	 * Get the color curve keyframes from a ColorFromCurve module.
	 *
	 * Returns an array of keyframes, each containing time and RGBA values.
	 * This allows reading the actual curve data for analysis or modification.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter containing the ColorFromCurve module
	 * @param ModuleName - Name of the ColorFromCurve module (default "ColorFromCurve")
	 * @return Array of color curve keyframes
	 *
	 * Example:
	 *   keys = unreal.NiagaraEmitterService.get_color_curve_keys("/Game/VFX/NS_Fire", "Flames")
	 *   for key in keys:
	 *       print(f"Time {key.time}: R={key.r}, G={key.g}, B={key.b}, A={key.a}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Get Color Curve Keys"))
	static TArray<FNiagaraColorCurveKey> GetColorCurveKeys(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleName = TEXT("ColorFromCurve"));

	/**
	 * Set the color curve keyframes on a ColorFromCurve module.
	 *
	 * Replaces all existing keyframes with the provided array.
	 * Each keyframe must have time and RGBA values.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter containing the ColorFromCurve module
	 * @param Keys - Array of color curve keyframes to set
	 * @param ModuleName - Name of the ColorFromCurve module (default "ColorFromCurve")
	 * @return True if successful
	 *
	 * Example:
	 *   # Read, modify, write back
	 *   keys = unreal.NiagaraEmitterService.get_color_curve_keys("/Game/VFX/NS_Fire", "Flames")
	 *   # ... modify keys ...
	 *   unreal.NiagaraEmitterService.set_color_curve_keys("/Game/VFX/NS_Fire", "Flames", keys)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Set Color Curve Keys"))
	static bool SetColorCurveKeys(
		const FString& SystemPath,
		const FString& EmitterName,
		const TArray<FNiagaraColorCurveKey>& Keys,
		const FString& ModuleName = TEXT("ColorFromCurve"));

	/**
	 * Shift the hue of a ColorFromCurve module while preserving luminosity and saturation.
	 *
	 * This is the recommended method for artistic color changes (e.g., orange fire to green fire)
	 * as it preserves all the detail and gradients in the original effect.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter containing the ColorFromCurve module
	 * @param HueShiftDegrees - Amount to shift hue (0-360). Examples: 120=orange->green, 240=orange->blue
	 * @param ModuleName - Name of the ColorFromCurve module (default "ColorFromCurve")
	 * @return True if successful
	 *
	 * Example:
	 *   # Shift orange fire to green (preserves all gradients and detail)
	 *   unreal.NiagaraEmitterService.shift_color_hue("/Game/VFX/NS_Fire", "Flames", 120)
	 *
	 *   # Shift to blue
	 *   unreal.NiagaraEmitterService.shift_color_hue("/Game/VFX/NS_Fire", "Flames", 240)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Shift Color Hue"))
	static bool ShiftColorHue(
		const FString& SystemPath,
		const FString& EmitterName,
		float HueShiftDegrees,
		const FString& ModuleName = TEXT("ColorFromCurve"));

	// =================================================================
	// Renderer Management Actions
	// =================================================================

	/**
	 * List all renderers in an emitter.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @return Array of renderer information
	 *
	 * Example:
	 *   renderers = unreal.NiagaraEmitterService.list_renderers("/Game/VFX/NS_Fire", "Sparks")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "List Renderers"))
	static TArray<FNiagaraRendererInfo_Custom> ListRenderers(
		const FString& SystemPath,
		const FString& EmitterName);

	/**
	 * Add a renderer to an emitter.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param RendererType - Type: "Sprite", "Mesh", "Ribbon", "Light", "Component"
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraEmitterService.add_renderer("/Game/VFX/NS_Fire", "Sparks", "Ribbon")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Add Renderer"))
	static bool AddRenderer(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& RendererType);

	/**
	 * Remove a renderer from an emitter.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param RendererIndex - Index of the renderer to remove
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraEmitterService.remove_renderer("/Game/VFX/NS_Fire", "Sparks", 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Remove Renderer"))
	static bool RemoveRenderer(
		const FString& SystemPath,
		const FString& EmitterName,
		int32 RendererIndex);

	/**
	 * Enable or disable a renderer.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param RendererIndex - Index of the renderer
	 * @param bEnabled - Whether to enable or disable
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraEmitterService.enable_renderer("/Game/VFX/NS_Fire", "Sparks", 0, False)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Enable Renderer"))
	static bool EnableRenderer(
		const FString& SystemPath,
		const FString& EmitterName,
		int32 RendererIndex,
		bool bEnabled);

	/**
	 * Set a renderer property.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param RendererIndex - Index of the renderer
	 * @param PropertyName - Name of the property
	 * @param Value - New value as string
	 * @return True if successful
	 *
	 * Example:
	 *   unreal.NiagaraEmitterService.set_renderer_property("/Game/VFX/NS_Fire", "Sparks", 0, "Material", "/Game/Materials/M_Particle")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Set Renderer Property"))
	static bool SetRendererProperty(
		const FString& SystemPath,
		const FString& EmitterName,
		int32 RendererIndex,
		const FString& PropertyName,
		const FString& Value);

	// =================================================================
	// Script Discovery Actions
	// =================================================================

	/**
	 * Search for Niagara module scripts.
	 *
	 * @param NameFilter - Filter for script names
	 * @param ModuleType - Filter by type: "Spawn", "Update", "Event", or empty for all
	 * @return Array of script paths
	 *
	 * Example:
	 *   scripts = unreal.NiagaraEmitterService.search_module_scripts("Scale", "Update")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Search Module Scripts"))
	static TArray<FString> SearchModuleScripts(
		const FString& NameFilter = TEXT(""),
		const FString& ModuleType = TEXT(""));

	/**
	 * Get information about a Niagara script asset.
	 *
	 * @param ScriptPath - Full path to the script asset
	 * @param OutInfo - Script information
	 * @return True if found
	 *
	 * Example:
	 *   success, info = unreal.NiagaraEmitterService.get_script_info("/Niagara/Modules/Update/Size/ScaleSpriteSize")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Get Script Info"))
	static bool GetScriptInfo(
		const FString& ScriptPath,
		FNiagaraScriptInfo_Custom& OutInfo);

	/**
	 * List common built-in module scripts by type.
	 *
	 * @param ModuleType - Type: "Spawn", "Update", "Event"
	 * @return Array of script paths
	 *
	 * Example:
	 *   scripts = unreal.NiagaraEmitterService.list_builtin_modules("Spawn")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "List Built-in Modules"))
	static TArray<FString> ListBuiltinModules(const FString& ModuleType);

	// =================================================================
	// Diagnostic Actions
	// =================================================================

	/**
	 * Get detailed renderer information including material and texture bindings.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param RendererIndex - Index of the renderer
	 * @param OutInfo - Detailed renderer information
	 * @return True if found
	 *
	 * Example:
	 *   success, info = unreal.NiagaraEmitterService.get_renderer_details("/Game/VFX/NS_Fire", "Sparks", 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Get Renderer Details"))
	static bool GetRendererDetails(
		const FString& SystemPath,
		const FString& EmitterName,
		int32 RendererIndex,
		FNiagaraRendererDetailedInfo& OutInfo);

	/**
	 * Get emitter lifecycle and property information.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param OutInfo - Emitter properties information
	 * @return True if found
	 *
	 * Example:
	 *   success, info = unreal.NiagaraEmitterService.get_emitter_properties("/Game/VFX/NS_Fire", "Sparks")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Get Emitter Properties"))
	static bool GetEmitterProperties(
		const FString& SystemPath,
		const FString& EmitterName,
		FNiagaraEmitterPropertiesInfo& OutInfo);

	/**
	 * Get all rapid iteration parameters from an emitter's scripts.
	 * Rapid iteration parameters are module input values that can be changed at runtime.
	 *
	 * @param SystemPath - Full path to the Niagara system
	 * @param EmitterName - Name of the emitter
	 * @param ScriptType - Optional: Filter by script type (EmitterSpawn, EmitterUpdate, ParticleSpawn, ParticleUpdate, or empty for all)
	 * @return Array of parameter info structs with name, type, and value
	 *
	 * Example:
	 *   params = unreal.NiagaraEmitterService.get_rapid_iteration_parameters("/Game/VFX/NS_Fire", "Sparks", "EmitterUpdate")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (DisplayName = "Get Rapid Iteration Parameters"))
	static TArray<FNiagaraModuleInputInfo> GetRapidIterationParameters(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ScriptType = TEXT(""));

private:
	// Helper methods
	static class UNiagaraSystem* LoadNiagaraSystem(const FString& SystemPath);
	static struct FNiagaraEmitterHandle* FindEmitterHandle(class UNiagaraSystem* System, const FString& EmitterName);
	static FString GetRendererTypeName(class UNiagaraRendererProperties* Renderer);
	static class UNiagaraDataInterfaceColorCurve* FindColorCurveDataInterface(
		class UNiagaraSystem* System,
		const FString& EmitterName,
		const FString& ModuleName);
};
