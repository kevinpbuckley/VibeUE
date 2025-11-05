#pragma once

#include "CoreMinimal.h"

/**
 * UMG Widget Property Type Definitions
 * 
 * This header contains data structures related to UMG widget properties
 * and events. Note: UMG's FPropertyInfo is in a separate namespace from
 * Blueprint's FPropertyInfo to avoid naming conflicts.
 */

/**
 * @struct FEventInfo
 * @brief Structure holding widget event information
 * 
 * Contains metadata about a widget event including its name, signature,
 * and parameter details.
 */
struct VIBEUE_API FEventInfo
{
	/** The name of the event (e.g., "OnClicked", "OnTextChanged") */
	FString EventName;
	
	/** The component class that owns this event */
	FString ComponentClassName;
	
	/** Human-readable signature of the event */
	FString Signature;
	
	/** Event category (e.g., "Interaction", "Visual", "Data") */
	FString Category;
	
	/** Whether this is a custom user-created event */
	bool bIsCustomEvent;

	FEventInfo()
		: bIsCustomEvent(false)
	{}
};

/**
 * @struct FPropertyInfo
 * @brief Structure containing detailed widget property metadata
 * 
 * Holds comprehensive information about a widget property including
 * its type, constraints, current value, and editability.
 * 
 * Note: This is the UMG-specific version of FPropertyInfo. For Blueprint properties,
 * see Services/Blueprint/Types/PropertyTypes.h
 */
struct VIBEUE_API FPropertyInfo
{
	/** The name of the property */
	FString PropertyName;
	
	/** The type of the property (e.g., "String", "float", "bool", "Struct<FVector2D>") */
	FString PropertyType;
	
	/** The property's class name from reflection */
	FString PropertyClass;
	
	/** Current value of the property as a string */
	FString CurrentValue;
	
	/** Whether the property can be edited */
	bool bIsEditable;
	
	/** Whether the property is blueprint visible */
	bool bIsBlueprintVisible;
	
	/** Whether the property is blueprint read-only */
	bool bIsBlueprintReadOnly;
	
	/** Category for organization */
	FString Category;
	
	/** Tooltip/description text */
	FString Tooltip;
	
	/** Min value for numeric properties */
	FString MinValue;
	
	/** Max value for numeric properties */
	FString MaxValue;
	
	/** Available values for enum properties */
	TArray<FString> EnumValues;

	FPropertyInfo()
		: bIsEditable(false)
		, bIsBlueprintVisible(false)
		, bIsBlueprintReadOnly(false)
	{}
};
