// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

// Forward declarations
class UWidgetBlueprint;
class UWidget;
class FProperty;

/**
 * @struct FPropertyInfo
 * @brief Structure containing detailed property metadata
 * 
 * Holds comprehensive information about a widget property including
 * its type, constraints, current value, and editability.
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

/**
 * @class FWidgetPropertyService
 * @brief Service for managing UMG widget properties
 * 
 * This service provides focused property management functionality extracted from
 * UMGCommands.cpp. It handles getting, setting, discovering, and validating properties
 * on UMG widget components within widget blueprints.
 * 
 * All methods return TResult<T> for type-safe error handling, avoiding the need
 * for runtime JSON parsing in the service layer.
 * 
 * Supports:
 * - Simple property access (get/set)
 * - Complex property paths with dot notation (e.g., "Slot.Padding")
 * - Property discovery and enumeration
 * - Property validation
 * - Type-specific handling for primitives, structs, arrays, maps, and sets
 * 
 * @note This is part of Phase 3 refactoring (Task 13) to extract UMG property
 * management into a focused service as per CPP_REFACTORING_DESIGN.md
 * 
 * @see TResult
 * @see FServiceBase
 * @see FPropertyInfo
 * @see Issue #35
 */
class VIBEUE_API FWidgetPropertyService : public FServiceBase
{
public:
	/**
	 * @brief Constructor
	 * @param Context Service context for shared state (can be nullptr)
	 */
	explicit FWidgetPropertyService(TSharedPtr<FServiceContext> Context);

	/**
	 * @brief Get a property value from a widget component
	 * 
	 * Retrieves the current value of a property on a specific widget component.
	 * Supports simple properties and complex paths using dot notation.
	 * 
	 * Examples:
	 * - "Text" - simple property
	 * - "Slot.Padding" - slot property with path
	 * - "RenderTransform.Translation" - struct field access
	 * 
	 * @param Widget The widget blueprint containing the component
	 * @param ComponentName The name of the widget component
	 * @param PropertyName The property name or path (supports dot notation)
	 * @return TResult containing the property value as a string, or error code and message on failure
	 */
	TResult<FString> GetProperty(UWidgetBlueprint* Widget, const FString& ComponentName,
	                             const FString& PropertyName);

	/**
	 * @brief Set a property value on a widget component
	 * 
	 * Sets a property value on a specific widget component. Marks the blueprint as modified.
	 * Supports simple properties and complex paths using dot notation.
	 * 
	 * @param Widget The widget blueprint containing the component
	 * @param ComponentName The name of the widget component
	 * @param PropertyName The property name or path (supports dot notation)
	 * @param Value The value to set (as a string, will be converted based on property type)
	 * @return TResult indicating success or error
	 */
	TResult<void> SetProperty(UWidgetBlueprint* Widget, const FString& ComponentName,
	                         const FString& PropertyName, const FString& Value);

	/**
	 * @brief Get all properties from a widget component
	 * 
	 * Retrieves all accessible properties and their current values from a widget component.
	 * Filters out private/protected properties.
	 * 
	 * @param Widget The widget blueprint containing the component
	 * @param ComponentName The name of the widget component
	 * @return TResult containing a map of property names to values, or error code and message on failure
	 */
	TResult<TMap<FString, FString>> GetAllProperties(UWidgetBlueprint* Widget, 
	                                                const FString& ComponentName);

	/**
	 * @brief List all properties available on a widget component
	 * 
	 * Discovers all properties on a widget component and returns detailed metadata
	 * about each property including type, constraints, and current value.
	 * 
	 * @param Widget The widget blueprint containing the component
	 * @param ComponentName The name of the widget component
	 * @return TResult containing an array of FPropertyInfo structures, or error code and message on failure
	 */
	TResult<TArray<FPropertyInfo>> ListProperties(UWidgetBlueprint* Widget, 
	                                             const FString& ComponentName);

	/**
	 * @brief Get detailed metadata for a specific property
	 * 
	 * Retrieves comprehensive metadata about a specific property including type information,
	 * constraints, current value, and editability flags.
	 * 
	 * @param Widget The widget blueprint containing the component
	 * @param ComponentName The name of the widget component
	 * @param PropertyName The property name to query
	 * @return TResult containing property metadata, or error code and message on failure
	 */
	TResult<FPropertyInfo> GetPropertyMetadata(UWidgetBlueprint* Widget, 
	                                          const FString& ComponentName,
	                                          const FString& PropertyName);

	/**
	 * @brief Check if a property exists and is accessible
	 * 
	 * Validates that a property exists on the widget component and can be accessed.
	 * 
	 * @param Widget The widget blueprint containing the component
	 * @param ComponentName The name of the widget component
	 * @param PropertyName The property name to validate
	 * @return TResult containing true if property is valid, false otherwise
	 */
	TResult<bool> IsValidProperty(UWidgetBlueprint* Widget, const FString& ComponentName,
	                              const FString& PropertyName);

	/**
	 * @brief Validate that a value is compatible with a property type
	 * 
	 * Checks if a given string value can be converted to match the specified property type.
	 * 
	 * @param PropertyType The type of the property (e.g., "float", "bool", "int")
	 * @param Value The value to validate
	 * @return TResult containing true if value is valid for the type, false otherwise
	 */
	TResult<bool> ValidatePropertyValue(const FString& PropertyType, const FString& Value);

protected:
	/**
	 * @brief Gets the service name for logging
	 * @return The service name
	 */
	virtual FString GetServiceName() const override { return TEXT("WidgetPropertyService"); }

private:
	/**
	 * @brief Find a widget component by name in a widget blueprint
	 * @param Widget The widget blueprint to search
	 * @param ComponentName The name of the component to find
	 * @return The widget component or nullptr if not found
	 */
	UWidget* FindWidgetComponent(UWidgetBlueprint* Widget, const FString& ComponentName);

	/**
	 * @brief Populate property info structure from a property and object
	 * @param Property The property to analyze
	 * @param WidgetObject The widget object containing the property
	 * @param OutInfo The FPropertyInfo structure to populate
	 */
	void PopulatePropertyInfo(FProperty* Property, UWidget* WidgetObject, FPropertyInfo& OutInfo);

	/**
	 * @brief Extract property value as a string
	 * @param Property The property to read
	 * @param Container The container pointer holding the property value
	 * @param OutValue The output string value
	 * @return true if value was successfully extracted, false otherwise
	 */
	bool ExtractPropertyValue(FProperty* Property, void* Container, FString& OutValue);

	/**
	 * @brief Set property value from a string
	 * @param Property The property to set
	 * @param Container The container pointer holding the property
	 * @param Value The string value to convert and set
	 * @return true if value was successfully set, false otherwise
	 */
	bool SetPropertyValue(FProperty* Property, void* Container, const FString& Value);
};
