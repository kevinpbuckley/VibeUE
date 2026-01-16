// Copyright Buckley Builds LLC 2026 All Rights Reserved.

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
	FString CurrentValue;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString DefaultValue;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	bool bIsOverridden = false;
};

/**
 * Information about a material property
 */
USTRUCT(BlueprintType)
struct FMaterialPropertyInfo_Custom
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString PropertyName;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString PropertyType;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString CurrentValue;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	TArray<FString> AllowedValues;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	bool bIsEditable = true;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	bool bIsAdvanced = false;
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
	FString MaterialDomain;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString BlendMode;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString ShadingModel;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	bool bIsMaterialInstance = false;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	bool bTwoSided = false;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	int32 ExpressionCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	int32 TextureSampleCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	TArray<FMaterialParameterInfo_Custom> Parameters;
};

/**
 * Material instance information (VibeUE custom)
 */
USTRUCT(BlueprintType)
struct FVibeUEMaterialInstanceInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString InstanceName;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString InstancePath;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString ParentMaterialPath;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString ParentMaterialName;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	TArray<FMaterialParameterInfo_Custom> ScalarParameters;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	TArray<FMaterialParameterInfo_Custom> VectorParameters;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	TArray<FMaterialParameterInfo_Custom> TextureParameters;
};

/**
 * Result of material creation operations
 */
USTRUCT(BlueprintType)
struct FMaterialCreateResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString ErrorMessage;
};

/**
 * Material summary for AI understanding
 */
USTRUCT(BlueprintType)
struct FMaterialSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString MaterialPath;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString MaterialName;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString MaterialDomain;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString BlendMode;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	FString ShadingModel;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	bool bTwoSided = false;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	int32 ExpressionCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	int32 ParameterCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	TArray<FString> ParameterNames;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	TArray<FMaterialPropertyInfo_Custom> KeyProperties;

	UPROPERTY(BlueprintReadWrite, Category = "Material")
	TArray<FMaterialPropertyInfo_Custom> EditableProperties;
};

/**
 * Material service exposed directly to Python.
 *
 * Provides 26 material management actions:
 *
 * Lifecycle:
 * - create: Create a new material asset
 * - create_instance: Create a material instance from a parent
 * - save: Save material to disk
 * - compile: Compile/rebuild material shaders
 * - refresh_editor: Refresh open Material Editor
 * - open: Open material in editor
 *
 * Information:
 * - get_info: Get comprehensive material information
 * - summarize: Get AI-friendly material summary
 * - list_properties: List all editable properties
 * - get_property: Get a property value
 * - get_property_info: Get detailed property metadata
 *
 * Property Management:
 * - set_property: Set a single property value
 * - set_properties: Set multiple properties at once
 *
 * Parameter Management:
 * - list_parameters: List all material parameters
 * - get_parameter: Get a parameter's value and info
 * - set_parameter_default: Set a parameter's default value
 *
 * Instance Information:
 * - get_instance_info: Get material instance information
 * - list_instance_properties: List instance editable properties
 * - get_instance_property: Get an instance property value
 * - set_instance_property: Set an instance property value
 *
 * Instance Parameters:
 * - list_instance_parameters: List instance parameter overrides
 * - set_instance_scalar_parameter: Set scalar parameter override
 * - set_instance_vector_parameter: Set vector/color parameter override
 * - set_instance_texture_parameter: Set texture parameter override
 * - clear_instance_parameter_override: Clear parameter override
 * - save_instance: Save material instance to disk
 *
 * Python Usage:
 *   import unreal
 *
 *   # Create a material
 *   result = unreal.MaterialService.create_material("M_NewMaterial", "/Game/Materials")
 *
 *   # Create material instance
 *   result = unreal.MaterialService.create_instance("/Game/Materials/M_Base", "MI_Red", "/Game/Materials")
 *
 *   # Set instance parameter
 *   unreal.MaterialService.set_instance_vector_parameter("/Game/MI_Red", "BaseColor", 1.0, 0.0, 0.0, 1.0)
 *
 * @note This replaces the JSON-based manage_material MCP tool
 */
UCLASS(BlueprintType)
class VIBEUE_API UMaterialService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Lifecycle Actions
	// =================================================================

	/**
	 * Create a new material asset.
	 * Maps to action="create"
	 *
	 * @param MaterialName - Name for the new material
	 * @param DestinationPath - Path where to create the asset (e.g., "/Game/Materials")
	 * @return Create result with asset path
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Create Material"))
	static FMaterialCreateResult CreateMaterial(
		const FString& MaterialName,
		const FString& DestinationPath);

	/**
	 * Create a material instance from a parent material.
	 * Maps to action="create_instance"
	 *
	 * @param ParentMaterialPath - Full path to the parent material
	 * @param InstanceName - Name for the new instance
	 * @param DestinationPath - Path where to create the instance
	 * @return Create result with asset path
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Create Material Instance"))
	static FMaterialCreateResult CreateInstance(
		const FString& ParentMaterialPath,
		const FString& InstanceName,
		const FString& DestinationPath);

	/**
	 * Save a material to disk.
	 * Maps to action="save"
	 *
	 * @param MaterialPath - Full path to the material
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Save Material"))
	static bool SaveMaterial(const FString& MaterialPath);

	/**
	 * Compile/rebuild a material's shaders.
	 * Maps to action="compile"
	 *
	 * @param MaterialPath - Full path to the material
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Compile Material"))
	static bool CompileMaterial(const FString& MaterialPath);

	/**
	 * Refresh an open Material Editor to show latest property values.
	 * Maps to action="refresh_editor"
	 *
	 * @param MaterialPath - Full path to the material
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Refresh Material Editor"))
	static bool RefreshEditor(const FString& MaterialPath);

	/**
	 * Open a material in the Material Editor.
	 * Maps to action="open"
	 *
	 * @param MaterialPath - Full path to the material or instance
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Open Material In Editor"))
	static bool OpenInEditor(const FString& MaterialPath);

	// =================================================================
	// Information Actions
	// =================================================================

	/**
	 * Get comprehensive material information.
	 * Maps to action="get_info"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param OutInfo - Structure containing all material details
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Get Material Info"))
	static bool GetMaterialInfo(const FString& MaterialPath, FMaterialDetailedInfo& OutInfo);

	/**
	 * Get an AI-friendly summary of a material.
	 * Maps to action="summarize"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param OutSummary - AI-friendly material summary
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Summarize Material"))
	static bool Summarize(const FString& MaterialPath, FMaterialSummary& OutSummary);

	/**
	 * List all editable properties of a material.
	 * Maps to action="list_properties"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param bIncludeAdvanced - Whether to include advanced/hidden properties
	 * @return Array of property information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "List Material Properties"))
	static TArray<FMaterialPropertyInfo_Custom> ListProperties(
		const FString& MaterialPath,
		bool bIncludeAdvanced = false);

	/**
	 * Get a material property value.
	 * Maps to action="get_property"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param PropertyName - Name of the property
	 * @return Property value as string
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Get Material Property"))
	static FString GetProperty(const FString& MaterialPath, const FString& PropertyName);

	/**
	 * Get detailed property metadata.
	 * Maps to action="get_property_info"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param PropertyName - Name of the property
	 * @param OutInfo - Detailed property information
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Get Material Property Info"))
	static bool GetPropertyInfo(
		const FString& MaterialPath,
		const FString& PropertyName,
		FMaterialPropertyInfo_Custom& OutInfo);

	// =================================================================
	// Property Management
	// =================================================================

	/**
	 * Set a material property value.
	 * Maps to action="set_property"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param PropertyName - Name of the property
	 * @param PropertyValue - New value as string
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Set Material Property"))
	static bool SetProperty(
		const FString& MaterialPath,
		const FString& PropertyName,
		const FString& PropertyValue);

	/**
	 * Set multiple material properties at once.
	 * Maps to action="set_properties"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param Properties - Map of property names to values
	 * @return Number of properties successfully set
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Set Material Properties"))
	static int32 SetProperties(
		const FString& MaterialPath,
		const TMap<FString, FString>& Properties);

	// =================================================================
	// Parameter Management
	// =================================================================

	/**
	 * List all parameters in a material.
	 * Maps to action="list_parameters"
	 *
	 * @param MaterialPath - Full path to the material
	 * @return Array of parameter information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "List Material Parameters"))
	static TArray<FMaterialParameterInfo_Custom> ListParameters(const FString& MaterialPath);

	/**
	 * Get a specific parameter's value and info.
	 * Maps to action="get_parameter"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param ParameterName - Name of the parameter
	 * @param OutInfo - Parameter information
	 * @return True if found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Get Material Parameter"))
	static bool GetParameter(
		const FString& MaterialPath,
		const FString& ParameterName,
		FMaterialParameterInfo_Custom& OutInfo);

	/**
	 * Set a parameter's default value.
	 * Maps to action="set_parameter_default"
	 *
	 * @param MaterialPath - Full path to the material
	 * @param ParameterName - Name of the parameter
	 * @param DefaultValue - New default value as string
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Set Parameter Default"))
	static bool SetParameterDefault(
		const FString& MaterialPath,
		const FString& ParameterName,
		const FString& DefaultValue);

	// =================================================================
	// Instance Information Actions
	// =================================================================

	/**
	 * Get information about a material instance.
	 * Maps to action="get_instance_info"
	 *
	 * @param InstancePath - Full path to the material instance
	 * @param OutInfo - Instance information
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Get Instance Info"))
	static bool GetInstanceInfo(const FString& InstancePath, FVibeUEMaterialInstanceInfo& OutInfo);

	/**
	 * List all editable properties of a material instance.
	 * Maps to action="list_instance_properties"
	 *
	 * @param InstancePath - Full path to the material instance
	 * @param bIncludeAdvanced - Whether to include advanced properties
	 * @return Array of property information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "List Instance Properties"))
	static TArray<FMaterialPropertyInfo_Custom> ListInstanceProperties(
		const FString& InstancePath,
		bool bIncludeAdvanced = false);

	/**
	 * Get a material instance property value.
	 * Maps to action="get_instance_property"
	 *
	 * @param InstancePath - Full path to the material instance
	 * @param PropertyName - Name of the property
	 * @return Property value as string
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Get Instance Property"))
	static FString GetInstanceProperty(const FString& InstancePath, const FString& PropertyName);

	/**
	 * Set a material instance property value.
	 * Maps to action="set_instance_property"
	 *
	 * @param InstancePath - Full path to the material instance
	 * @param PropertyName - Name of the property
	 * @param PropertyValue - New value as string
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Set Instance Property"))
	static bool SetInstanceProperty(
		const FString& InstancePath,
		const FString& PropertyName,
		const FString& PropertyValue);

	// =================================================================
	// Instance Parameter Actions
	// =================================================================

	/**
	 * List all parameter overrides in a material instance.
	 * Maps to action="list_instance_parameters"
	 *
	 * @param InstancePath - Full path to the material instance
	 * @return Array of parameter information with override status
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "List Instance Parameters"))
	static TArray<FMaterialParameterInfo_Custom> ListInstanceParameters(const FString& InstancePath);

	/**
	 * Set a scalar parameter override on a material instance.
	 * Maps to action="set_instance_scalar_parameter"
	 *
	 * @param InstancePath - Full path to the material instance
	 * @param ParameterName - Name of the parameter
	 * @param Value - New scalar value
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Set Instance Scalar Parameter"))
	static bool SetInstanceScalarParameter(
		const FString& InstancePath,
		const FString& ParameterName,
		float Value);

	/**
	 * Set a vector/color parameter override on a material instance.
	 * Maps to action="set_instance_vector_parameter"
	 *
	 * @param InstancePath - Full path to the material instance
	 * @param ParameterName - Name of the parameter
	 * @param R - Red component (0.0-1.0)
	 * @param G - Green component (0.0-1.0)
	 * @param B - Blue component (0.0-1.0)
	 * @param A - Alpha component (0.0-1.0)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Set Instance Vector Parameter"))
	static bool SetInstanceVectorParameter(
		const FString& InstancePath,
		const FString& ParameterName,
		float R, float G, float B, float A = 1.0f);

	/**
	 * Set a texture parameter override on a material instance.
	 * Maps to action="set_instance_texture_parameter"
	 *
	 * @param InstancePath - Full path to the material instance
	 * @param ParameterName - Name of the parameter
	 * @param TexturePath - Path to the texture asset (empty to clear)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Set Instance Texture Parameter"))
	static bool SetInstanceTextureParameter(
		const FString& InstancePath,
		const FString& ParameterName,
		const FString& TexturePath);

	/**
	 * Clear a parameter override on a material instance.
	 * Maps to action="clear_instance_parameter_override"
	 *
	 * @param InstancePath - Full path to the material instance
	 * @param ParameterName - Name of the parameter to clear
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Clear Instance Parameter Override"))
	static bool ClearInstanceParameterOverride(
		const FString& InstancePath,
		const FString& ParameterName);

	/**
	 * Save a material instance to disk.
	 * Maps to action="save_instance"
	 *
	 * @param InstancePath - Full path to the material instance
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Materials", meta = (DisplayName = "Save Material Instance"))
	static bool SaveInstance(const FString& InstancePath);

private:
	// Helper methods
	static class UMaterial* LoadMaterialAsset(const FString& MaterialPath);
	static class UMaterialInstance* LoadMaterialInstanceAsset(const FString& InstancePath);
	static class UMaterialInstanceConstant* LoadMaterialInstanceConstant(const FString& InstancePath);
	static FString PropertyValueToString(const FProperty* Property, const void* Container);
	static bool StringToPropertyValue(FProperty* Property, void* Container, const FString& Value);
	static TArray<FString> GetEnumPropertyValues(const FEnumProperty* EnumProp);
};
