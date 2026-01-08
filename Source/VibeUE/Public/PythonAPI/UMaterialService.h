// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UMaterialService.generated.h"

/**
 * Information about a material parameter
 */
USTRUCT(BlueprintType)
struct FMaterialParameterInfo_Custom
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString ParameterName;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString ParameterType;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString Group;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString DefaultValue;
};

/**
 * Comprehensive material information
 */
USTRUCT(BlueprintType)
struct FMaterialDetailedInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString MaterialName;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString MaterialPath;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString ParentMaterial;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	bool bIsMaterialInstance = false;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	TArray<FMaterialParameterInfo_Custom> Parameters;
};

/**
 * Material service exposed directly to Python.
 *
 * Python Usage:
 *   import unreal
 *
 *   # Get material info
 *   info = unreal.MaterialDetailedInfo()
 *   if unreal.MaterialService.get_material_info("/Game/Materials/M_Base", info):
 *       print(f"Material: {info.material_name}")
 *       for param in info.parameters:
 *           print(f"  {param.parameter_name}: {param.parameter_type}")
 *
 * @note This replaces the JSON-based manage_material MCP tool
 */
UCLASS(BlueprintType)
class VIBEUE_API UMaterialService : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Get comprehensive material information including parameters.
	 *
	 * @param MaterialPath - Full path to the material
	 * @param OutInfo - Structure containing all material details
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials")
	static bool GetMaterialInfo(const FString& MaterialPath, FMaterialDetailedInfo& OutInfo);

	/**
	 * List all parameters in a material.
	 *
	 * @param MaterialPath - Full path to the material
	 * @return Array of parameter information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials")
	static TArray<FMaterialParameterInfo_Custom> ListParameters(const FString& MaterialPath);

	/**
	 * Get a specific parameter value from a material instance.
	 *
	 * @param MaterialPath - Full path to the material instance
	 * @param ParameterName - Name of the parameter
	 * @return Parameter value as string, or empty if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials")
	static FString GetParameterValue(const FString& MaterialPath, const FString& ParameterName);
};
