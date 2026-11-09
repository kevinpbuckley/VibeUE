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
 * Niagara Emitter Service - color curve authoring for an emitter's ColorFromCurve module.
 *
 * Trimmed on the 5-9 branch: UE 5.9's NiagaraToolsets (57 tools) covers emitter/module/renderer
 * CRUD, stack input data (what VibeUE called rapid-iteration parameters), emitter properties such
 * as sim target, stack issues and compile state. What remains here is the colour-curve editing
 * Epic's stack-input API does not express directly.
 *
 * - get_color_curve_keys(system_path, emitter_name[, module_name])
 * - set_color_curve_keys(system_path, emitter_name, keys[, module_name])
 * - shift_color_hue(system_path, emitter_name, degrees[, module_name])
 *
 * For emitter/module/renderer CRUD, stack inputs, sim target, solid colour tints and diagnostics,
 * use the engine NiagaraToolsets via call_tool. Scratch-pad / Custom-HLSL graph authoring is on
 * NiagaraScratchPadService.
 *
 * Python Usage:
 *   import unreal
 *
 *   keys = unreal.NiagaraEmitterService.get_color_curve_keys("/Game/VFX/NS_Fire", "Flames")
 *   unreal.NiagaraEmitterService.shift_color_hue("/Game/VFX/NS_Fire", "Flames", 120.0)
 */
UCLASS(BlueprintType)
class VIBEUE_API UNiagaraEmitterService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
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

private:
	// Helper methods
	static class UNiagaraSystem* LoadNiagaraSystem(const FString& SystemPath);
	static struct FNiagaraEmitterHandle* FindEmitterHandle(class UNiagaraSystem* System, const FString& EmitterName);
	static class UNiagaraDataInterfaceColorCurve* FindColorCurveDataInterface(
		class UNiagaraSystem* System,
		const FString& EmitterName,
		const FString& ModuleName);
};
