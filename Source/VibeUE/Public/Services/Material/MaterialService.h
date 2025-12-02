// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

class UMaterial;
class UMaterialInterface;
class UMaterialInstance;

/**
 * @struct FStructMemberInfo
 * @brief Information about a struct member property
 */
struct FStructMemberInfo
{
    /** Member name */
    FString Name;
    
    /** Member type */
    FString Type;
    
    /** For object properties: expected class type */
    FString ObjectClass;
    
    /** For enums: allowed values */
    TArray<FString> AllowedValues;
    
    /** Current value as string */
    FString CurrentValue;
};

/**
 * @struct FMaterialPropertyInfo
 * @brief Information about a material property
 */
struct FMaterialPropertyInfo
{
    /** Property name */
    FString Name;
    
    /** Property display name */
    FString DisplayName;
    
    /** Property type (bool, float, enum, etc.) */
    FString Type;
    
    /** Property category */
    FString Category;
    
    /** Property tooltip/description */
    FString Tooltip;
    
    /** Current value as string */
    FString CurrentValue;
    
    /** For enums: list of allowed values */
    TArray<FString> AllowedValues;
    
    /** For structs: list of member properties */
    TArray<FStructMemberInfo> StructMembers;
    
    /** For object properties: expected class type */
    FString ObjectClass;
    
    /** Whether property is editable */
    bool bIsEditable = true;
    
    /** Whether property is advanced (usually hidden) */
    bool bIsAdvanced = false;
};

/**
 * @struct FMaterialInfo
 * @brief Comprehensive information about a material
 */
struct FMaterialInfo
{
    /** Material asset path */
    FString AssetPath;
    
    /** Material name */
    FString Name;
    
    /** Material domain (Surface, DeferredDecal, etc.) */
    FString MaterialDomain;
    
    /** Blend mode */
    FString BlendMode;
    
    /** Shading model */
    FString ShadingModel;
    
    /** Whether two-sided rendering is enabled */
    bool bTwoSided = false;
    
    /** Number of expressions in the material graph */
    int32 ExpressionCount = 0;
    
    /** Number of texture samples */
    int32 TextureSampleCount = 0;
    
    /** Number of parameters */
    int32 ParameterCount = 0;
    
    /** List of parameter names */
    TArray<FString> ParameterNames;
    
    /** All editable properties */
    TArray<FMaterialPropertyInfo> Properties;
};

/**
 * @struct FVibeMaterialParamInfo
 * @brief Information about a material parameter (renamed to avoid UE5 collision)
 */
struct FVibeMaterialParamInfo
{
    /** Parameter name */
    FString Name;
    
    /** Parameter type (Scalar, Vector, Texture, etc.) */
    FString Type;
    
    /** Parameter group/category */
    FString Group;
    
    /** Current value as string */
    FString CurrentValue;
    
    /** Default value as string */
    FString DefaultValue;
    
    /** Sort priority within group */
    int32 SortPriority = 0;
    
    /** Whether parameter is exposed to instances */
    bool bIsExposed = true;
};

/**
 * @struct FMaterialCreateParams
 * @brief Parameters for creating a new material
 */
struct FMaterialCreateParams
{
    /** Destination path in content browser */
    FString DestinationPath;
    
    /** Material name */
    FString MaterialName;
    
    /** Initial properties to set (optional) */
    TMap<FString, FString> InitialProperties;
};

/**
 * @class FMaterialService
 * @brief Service for material lifecycle and property management
 * 
 * Provides comprehensive material management including:
 * - Creating new materials
 * - Getting/setting material properties via reflection
 * - Managing material parameters
 * - Substrate/Nanite settings support
 * 
 * All methods return TResult<T> for type-safe error handling.
 * 
 * @see TResult
 * @see FServiceBase
 */
class VIBEUE_API FMaterialService : public FServiceBase
{
public:
    /**
     * @brief Constructor with dependency injection
     * @param InContext Shared service context
     */
    explicit FMaterialService(TSharedPtr<FServiceContext> InContext);

    //-------------------------------------------------------------------------
    // FServiceBase Interface
    //-------------------------------------------------------------------------

    /**
     * @brief Get the service name for logging and identification
     * @return Service name string
     */
    virtual FString GetServiceName() const override { return TEXT("MaterialService"); }

    //-------------------------------------------------------------------------
    // Material Lifecycle
    //-------------------------------------------------------------------------

    /**
     * @brief Create a new material asset
     * @param Params Creation parameters
     * @return Result containing the new material's asset path
     */
    TResult<FString> CreateMaterial(const FMaterialCreateParams& Params);

    /**
     * @brief Load a material by path
     * @param MaterialPath Full asset path to the material
     * @return Result containing the loaded material pointer
     */
    TResult<UMaterial*> LoadMaterial(const FString& MaterialPath);

    /**
     * @brief Save a material to disk
     * @param MaterialPath Full asset path to the material
     * @return Success or error
     */
    TResult<void> SaveMaterial(const FString& MaterialPath);

    /**
     * @brief Compile/rebuild a material's shaders
     * @param MaterialPath Full asset path to the material
     * @return Success or error
     */
    TResult<void> CompileMaterial(const FString& MaterialPath);

    /**
     * @brief Refresh an open Material Editor to show latest property values
     * This closes and reopens the Material Editor to force a UI refresh.
     * Use this after setting properties externally when the Details panel doesn't update.
     * @param MaterialPath Full asset path to the material
     * @return Success or error
     */
    TResult<void> RefreshMaterialEditor(const FString& MaterialPath);

    //-------------------------------------------------------------------------
    // Material Information
    //-------------------------------------------------------------------------

    /**
     * @brief Get comprehensive information about a material
     * @param MaterialPath Full asset path to the material
     * @return Result containing material information
     */
    TResult<FMaterialInfo> GetMaterialInfo(const FString& MaterialPath);

    /**
     * @brief List all editable properties of a material via reflection
     * @param MaterialPath Full asset path to the material
     * @param bIncludeAdvanced Whether to include advanced/hidden properties
     * @return Result containing list of properties
     */
    TResult<TArray<FMaterialPropertyInfo>> ListProperties(const FString& MaterialPath, bool bIncludeAdvanced = false);

    //-------------------------------------------------------------------------
    // Property Management (Reflection-based)
    //-------------------------------------------------------------------------

    /**
     * @brief Get a material property value
     * @param MaterialPath Full asset path to the material
     * @param PropertyName Name of the property to get
     * @return Result containing the property value as string
     */
    TResult<FString> GetProperty(const FString& MaterialPath, const FString& PropertyName);

    /**
     * @brief Get detailed property metadata
     * @param MaterialPath Full asset path to the material
     * @param PropertyName Name of the property
     * @return Result containing property information
     */
    TResult<FMaterialPropertyInfo> GetPropertyInfo(const FString& MaterialPath, const FString& PropertyName);

    /**
     * @brief Set a material property value
     * @param MaterialPath Full asset path to the material
     * @param PropertyName Name of the property to set
     * @param Value New value as string (will be converted based on property type)
     * @return Success with actual value after engine validation, or error
     */
    TResult<FString> SetProperty(const FString& MaterialPath, const FString& PropertyName, const FString& Value);

    /**
     * @brief Set multiple material properties at once
     * @param MaterialPath Full asset path to the material
     * @param Properties Map of property names to values
     * @return Success or error (with details of any failed properties)
     */
    TResult<void> SetProperties(const FString& MaterialPath, const TMap<FString, FString>& Properties);

    //-------------------------------------------------------------------------
    // Parameter Management
    //-------------------------------------------------------------------------

    /**
     * @brief List all parameters in a material
     * @param MaterialPath Full asset path to the material
     * @return Result containing list of parameters
     */
    TResult<TArray<FVibeMaterialParamInfo>> ListParameters(const FString& MaterialPath);

    /**
     * @brief Get a specific parameter's value
     * @param MaterialPath Full asset path to the material
     * @param ParameterName Name of the parameter
     * @return Result containing parameter info with current value
     */
    TResult<FVibeMaterialParamInfo> GetParameter(const FString& MaterialPath, const FString& ParameterName);

    /**
     * @brief Set a parameter's default value
     * @param MaterialPath Full asset path to the material
     * @param ParameterName Name of the parameter
     * @param Value New default value as string
     * @return Success or error
     */
    TResult<void> SetParameterDefault(const FString& MaterialPath, const FString& ParameterName, const FString& Value);

private:
    /**
     * @brief Helper to convert property value to string based on type
     * @param Property The property
     * @param Container Pointer to the container object
     * @return Value as string
     */
    FString PropertyToString(const FProperty* Property, const void* Container) const;

    /**
     * @brief Helper to set property value from string based on type
     * @param Property The property
     * @param Container Pointer to the container object
     * @param Value String value to convert and set
     * @return Success or error
     */
    TResult<void> StringToProperty(FProperty* Property, void* Container, const FString& Value) const;

    /**
     * @brief Get property category from metadata
     * @param Property The property
     * @return Category name
     */
    FString GetPropertyCategory(const FProperty* Property) const;

    /**
     * @brief Get enum values as strings
     * @param EnumProperty The enum property
     * @return Array of enum value names
     */
    TArray<FString> GetEnumValues(const FEnumProperty* EnumProperty) const;

    /**
     * @brief Mark material as dirty and update editor
     * @param Material The material to mark
     */
    void MarkMaterialDirty(UMaterial* Material) const;
};
