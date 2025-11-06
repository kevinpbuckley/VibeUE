#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/UMG/Types/WidgetPropertyTypes.h"
#include "Core/Result.h"

// Forward declarations
class UWidget;
class UWidgetBlueprint;
class FProperty;

/**
 * @class FWidgetPropertyService
 * @brief Service for widget property management
 * 
 * This service provides property get/set functionality extracted from UMGCommands.cpp.
 * It handles reading and writing widget properties, including nested properties,
 * slot properties, and property validation.
 * 
 * All methods return TResult<T> for type-safe error handling.
 * 
 * @see TResult
 * @see FServiceBase
 */
class VIBEUE_API FWidgetPropertyService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state (can be nullptr)
     */
    explicit FWidgetPropertyService(TSharedPtr<FServiceContext> Context);

    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("WidgetPropertyService"); }

    /**
     * @brief Get a widget property value
     * 
     * @param Widget Widget to get property from
     * @param PropertyPath Property path (supports nested like "Slot.Padding")
     * @return TResult containing property value as string or error
     */
    TResult<FString> GetWidgetProperty(UWidget* Widget, const FString& PropertyPath);

    /**
     * @brief Set a widget property value
     * 
     * @param Widget Widget to set property on
     * @param PropertyPath Property path (supports nested like "Slot.Padding")
     * @param Value New value as string
     * @return TResult indicating success or error
     */
    TResult<void> SetWidgetProperty(UWidget* Widget, const FString& PropertyPath, const FString& Value);

    /**
     * @brief List all properties for a widget
     * 
     * @param Widget Widget to list properties for
     * @param bIncludeSlotProperties Include slot properties if widget is in a slot
     * @return TResult containing array of FWidgetPropertyInfo structures
     */
    TResult<TArray<FWidgetPropertyInfo>> ListWidgetProperties(UWidget* Widget, bool bIncludeSlotProperties = true);

    /**
     * @brief Get detailed property descriptor with constraints
     * 
     * @param Widget Widget to get property descriptor from
     * @param PropertyPath Property path
     * @return TResult containing FWidgetPropertyDescriptor structure
     */
    TResult<FWidgetPropertyDescriptor> GetPropertyDescriptor(UWidget* Widget, const FString& PropertyPath);

    /**
     * @brief Validate a property value before setting
     * 
     * @param Widget Widget to validate property for
     * @param PropertyPath Property path
     * @param Value Value to validate
     * @return TResult indicating if value is valid
     */
    TResult<bool> ValidatePropertyValue(UWidget* Widget, const FString& PropertyPath, const FString& Value);

    /**
     * @brief Set multiple properties in a batch
     * 
     * @param Widget Widget to set properties on
     * @param Updates Array of property updates
     * @return TResult with array of error messages (empty if all succeeded)
     */
    TResult<TArray<FString>> SetPropertiesBatch(UWidget* Widget, const TArray<FWidgetPropertyUpdate>& Updates);

    /**
     * @brief Get slot properties for a widget
     * 
     * @param Widget Widget to get slot properties from
     * @return TResult containing array of FWidgetPropertyInfo structures for slot
     */
    TResult<TArray<FWidgetPropertyInfo>> GetSlotProperties(UWidget* Widget);

    /**
     * @brief Set slot property value
     * 
     * @param Widget Widget whose slot to update
     * @param PropertyPath Slot property path
     * @param Value New value as string
     * @return TResult indicating success or error
     */
    TResult<void> SetSlotProperty(UWidget* Widget, const FString& PropertyPath, const FString& Value);

private:
    /**
     * @brief Find a property by path
     * 
     * @param Object Object to search
     * @param PropertyPath Property path
     * @param OutProperty Output property
     * @param OutContainer Output container address
     * @return True if found
     */
    bool FindPropertyByPath(UObject* Object, const FString& PropertyPath, FProperty*& OutProperty, void*& OutContainer);

    /**
     * @brief Convert property value to string
     * 
     * @param Property Property definition
     * @param ValuePtr Pointer to value
     * @return String representation
     */
    FString PropertyValueToString(FProperty* Property, const void* ValuePtr);

    /**
     * @brief Set property value from string
     * 
     * @param Property Property definition
     * @param ValuePtr Pointer to value
     * @param ValueString String to parse
     * @return True if successful
     */
    bool SetPropertyValueFromString(FProperty* Property, void* ValuePtr, const FString& ValueString);
};
