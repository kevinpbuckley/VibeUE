// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UNiagaraEmitterService.generated.h"

/**
 * Niagara Emitter Service - hue rotation of an emitter's colour curve.
 *
 * Trimmed on the 5-9 branch: UE 5.9's NiagaraToolsets (57 tools) covers emitter/module/renderer
 * CRUD, stack input data (what VibeUE called rapid-iteration parameters), emitter properties such
 * as sim target, stack issues and compile state -- and colour-curve keyframes too: a ColorFromCurve
 * dynamic input's `ColorCurve` data interface is read and written with
 * NiagaraToolset_System.GetStackInputData / SetStackInputData (inputNameStack ["Color", "ColorCurve"],
 * value struct NiagaraExt_StackInputData_DataInterface, `propertyValues` a JSON string holding
 * RedCurve/GreenCurve/BlueCurve/AlphaCurve keys). What remains here is the one thing Epic has no
 * tool for: rotating the hue of every key while keeping saturation, value and timing.
 *
 * - shift_color_hue(system_path, emitter_name, degrees[, module_name])
 *
 * Scratch-pad / Custom-HLSL graph authoring is on NiagaraScratchPadService.
 *
 * Python Usage:
 *   import unreal
 *
 *   unreal.NiagaraEmitterService.shift_color_hue("/Game/VFX/NS_Fire", "Flames", 120.0)
 */
UCLASS(BlueprintType)
class VIBEUE_API UNiagaraEmitterService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
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
