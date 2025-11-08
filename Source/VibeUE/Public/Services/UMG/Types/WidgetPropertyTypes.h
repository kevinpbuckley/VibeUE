#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Optional.h"

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

/**
 * @struct FWidgetPropertyValue
 * @brief Union-style container for string or JSON property values
 */
struct VIBEUE_API FWidgetPropertyValue
{
    bool bHasStringValue = false;
    FString StringValue;
    TSharedPtr<FJsonValue> JsonValue;

    bool HasString() const { return bHasStringValue; }
    bool HasJson() const { return JsonValue.IsValid(); }
    bool HasValue() const { return bHasStringValue || JsonValue.IsValid(); }

    void SetString(const FString& InValue)
    {
        bHasStringValue = true;
        StringValue = InValue;
    }

    void SetJson(const TSharedPtr<FJsonValue>& InValue)
    {
        JsonValue = InValue;
    }
};

/**
 * @struct FWidgetCollectionOperation
 * @brief Describes a collection manipulation request (arrays, etc.)
 */
struct VIBEUE_API FWidgetCollectionOperation
{
    FString Operation;
    TOptional<int32> Index;
};

/**
 * @struct FWidgetPropertySetRequest
 * @brief Request payload for setting a widget property via service layer
 */
struct VIBEUE_API FWidgetPropertySetRequest
{
    FString PropertyPath;
    FWidgetPropertyValue Value;
    TOptional<FWidgetCollectionOperation> CollectionOperation;
};

/**
 * @struct FWidgetPropertySetResult
 * @brief Result payload for property mutation operations
 */
struct VIBEUE_API FWidgetPropertySetResult
{
    FString PropertyPath;
    FWidgetPropertyValue AppliedValue;
    FString Note;
    FString CollectionOperation;
    bool bStructuralChange = false;
    bool bChildOrderUpdated = false;
    int32 ChildOrderValue = INDEX_NONE;
};

/**
 * @struct FWidgetPropertyGetResult
 * @brief Result payload for property query operations
 */
struct VIBEUE_API FWidgetPropertyGetResult
{
    FString PropertyPath;
    FWidgetPropertyValue Value;
    FString PropertyType;
    TSharedPtr<FJsonObject> Constraints;
    TSharedPtr<FJsonObject> Schema;
    bool bIsEditable = false;
    FString SlotClass;
    bool bIsChildOrder = false;
    int32 ChildOrderValue = INDEX_NONE;
    int32 ChildCount = 0;
};
