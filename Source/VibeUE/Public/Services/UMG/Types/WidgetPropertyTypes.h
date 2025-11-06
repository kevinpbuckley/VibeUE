#pragma once

#include "CoreMinimal.h"

/**
 * Widget Property Type Definitions
 * 
 * This header contains property-related data structures for widget components.
 */

/**
 * @struct FWidgetPropertyInfo
 * @brief Information about a widget property
 */
struct VIBEUE_API FWidgetPropertyInfo
{
    /** Property name */
    FString PropertyName;
    
    /** Property type (e.g., "FText", "FLinearColor", "bool") */
    FString PropertyType;
    
    /** Property category */
    FString Category;
    
    /** Default value as string */
    FString DefaultValue;
    
    /** Current value as string */
    FString CurrentValue;
    
    /** Whether the property can be edited */
    bool bIsEditable;
    
    /** Whether the property is a blueprint variable */
    bool bIsBlueprintVisible;

    FWidgetPropertyInfo()
        : bIsEditable(true)
        , bIsBlueprintVisible(false)
    {}
};

/**
 * @struct FWidgetPropertyDescriptor
 * @brief Extended property descriptor with constraints
 */
struct VIBEUE_API FWidgetPropertyDescriptor
{
    /** Property information */
    FWidgetPropertyInfo Info;
    
    /** Minimum value (for numeric properties) */
    FString MinValue;
    
    /** Maximum value (for numeric properties) */
    FString MaxValue;
    
    /** Allowed enum values (for enum properties) */
    TArray<FString> EnumValues;
    
    /** Whether this property has constraints */
    bool bHasConstraints;

    FWidgetPropertyDescriptor()
        : bHasConstraints(false)
    {}
};

/**
 * @struct FWidgetPropertyUpdate
 * @brief Property update request
 */
struct VIBEUE_API FWidgetPropertyUpdate
{
    /** Widget name to update */
    FString WidgetName;
    
    /** Property path (supports nested properties like "Slot.Padding") */
    FString PropertyPath;
    
    /** New value as JSON-serializable string */
    FString NewValue;
    
    /** Property type hint */
    FString PropertyType;
};
