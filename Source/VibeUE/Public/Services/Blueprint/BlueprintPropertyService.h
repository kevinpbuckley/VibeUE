#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

// Forward declarations
class UBlueprint;

/**
 * Property information structure
 */
struct VIBEUE_API FPropertyInfo
{
    FString PropertyName;
    FString PropertyType;
    FString PropertyClass;
    FString Category;
    FString Tooltip;
    FString CurrentValue;
    FString DefaultValue;
    bool bIsEditable;
    bool bIsBlueprintVisible;
    bool bIsBlueprintReadOnly;
    
    // Type-specific metadata
    FString MinValue;
    FString MaxValue;
    FString UIMin;
    FString UIMax;
    FString ObjectClass;
    FString ObjectValue;
};

/**
 * Blueprint Property Service
 * Handles getting and setting properties on Blueprint default objects
 */
class VIBEUE_API FBlueprintPropertyService : public FServiceBase
{
public:
    explicit FBlueprintPropertyService(TSharedPtr<FServiceContext> Context);
    
    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("BlueprintPropertyService"); }
    
    /**
     * Get a property value from a Blueprint's default object
     * @param Blueprint The Blueprint to query
     * @param PropertyName The name of the property to get
     * @return Result containing the property value as a string, or an error message
     */
    TResult<FString> GetProperty(UBlueprint* Blueprint, const FString& PropertyName);
    
    /**
     * Set a property value on a Blueprint's default object
     * @param Blueprint The Blueprint to modify
     * @param PropertyName The name of the property to set
     * @param PropertyValue The value to set (as a string)
     * @return Result indicating success or error
     */
    TResult<void> SetProperty(UBlueprint* Blueprint, const FString& PropertyName, const FString& PropertyValue);
    
    /**
     * List all properties available on a Blueprint's default object
     * @param Blueprint The Blueprint to query
     * @return Result containing an array of property information, or an error message
     */
    TResult<TArray<FPropertyInfo>> ListProperties(UBlueprint* Blueprint);
    
    /**
     * Get detailed metadata for a specific property
     * @param Blueprint The Blueprint to query
     * @param PropertyName The name of the property
     * @return Result containing property metadata, or an error message
     */
    TResult<FPropertyInfo> GetPropertyMetadata(UBlueprint* Blueprint, const FString& PropertyName);

private:
    /**
     * Helper to populate property metadata
     */
    void PopulatePropertyInfo(FProperty* Property, UObject* DefaultObject, FPropertyInfo& OutInfo);
};
