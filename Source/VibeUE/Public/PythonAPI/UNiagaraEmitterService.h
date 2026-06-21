// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
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
 * Niagara Emitter Service - Python API for emitter color + module authoring.
 *
 * Methods this service actually exposes (issue #462 — the docstring previously listed
 * renderer/module-input/script-discovery actions that are not bound):
 *
 * Color authoring:
 * - set_color_tint(system_path, emitter_name, color[, hue_shift]) — handles ColorFromCurve
 * - get_color_curve_keys(system_path, emitter_name)
 * - set_color_curve_keys(system_path, emitter_name, keys)
 * - shift_color_hue(system_path, emitter_name, degrees)
 *
 * Modules / parameters:
 * - list_modules(system_path, emitter_name, stage)
 * - add_module(system_path, emitter_name, module_script_path, stage)
 * - get_rapid_iteration_parameters(system_path, emitter_name, stage)
 *
 * For emitter add/copy/duplicate/remove/move and renderer CRUD, use the engine
 * NiagaraToolsets via call_tool (see UNiagaraService notes). Scratch-pad / Custom-HLSL
 * graph authoring is on NiagaraScratchPadService.
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
class VIBEUE_API UNiagaraEmitterService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (AICallable, DisplayName = "Set Color Tint"))
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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (AICallable, DisplayName = "Get Color Curve Keys"))
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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (AICallable, DisplayName = "Set Color Curve Keys"))
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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (AICallable, DisplayName = "Shift Color Hue"))
	static bool ShiftColorHue(
		const FString& SystemPath,
		const FString& EmitterName,
		float HueShiftDegrees,
		const FString& ModuleName = TEXT("ColorFromCurve"));

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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|NiagaraEmitter", meta = (AICallable, DisplayName = "Get Rapid Iteration Parameters"))
	static TArray<FNiagaraModuleInputInfo> GetRapidIterationParameters(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ScriptType = TEXT(""));

private:
	// Helper methods
	static class UNiagaraSystem* LoadNiagaraSystem(const FString& SystemPath);
	static struct FNiagaraEmitterHandle* FindEmitterHandle(class UNiagaraSystem* System, const FString& EmitterName);
	static class UNiagaraDataInterfaceColorCurve* FindColorCurveDataInterface(
		class UNiagaraSystem* System,
		const FString& EmitterName,
		const FString& ModuleName);

	// Internal module helpers (not exposed - required by SetColorTint, which ensures a
	// ScaleColor module exists before setting its rapid-iteration tint value).
	static TArray<FNiagaraModuleInfo_Custom> ListModules(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleType = TEXT(""));
	static bool AddModule(
		const FString& SystemPath,
		const FString& EmitterName,
		const FString& ModuleScriptPath,
		const FString& ModuleType);
};
