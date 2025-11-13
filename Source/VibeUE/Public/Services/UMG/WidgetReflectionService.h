// Copyright VibeUE 2025

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/UMG/Types/WidgetReflectionTypes.h"
#include "Core/Result.h"

// Forward declarations
class UWidget;

/**
 * @class FWidgetReflectionService
 * @brief Service for widget class discovery and reflection
 * 
 * This service provides reflection-based widget discovery functionality extracted from
 * UMGReflectionCommands.cpp. It handles discovering available widget types, categories,
 * and compatibility information using UClass reflection.
 * 
 * All methods return TResult<T> for type-safe error handling.
 * 
 * @see TResult
 * @see FServiceBase
 */
class VIBEUE_API FWidgetReflectionService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state (can be nullptr)
     */
    explicit FWidgetReflectionService(TSharedPtr<FServiceContext> Context);

    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("WidgetReflectionService"); }

    /**
     * @brief Get all available widget types
     * 
     * @return TResult containing array of widget type names
     */
    TResult<TArray<FString>> GetAvailableWidgetTypes();

    /**
     * @brief Get all widget categories
     * 
     * @return TResult containing array of category names
     */
    TResult<TArray<FString>> GetWidgetCategories();

    /**
     * @brief Get detailed information about available widget classes
     * 
     * @param bIncludeEngine Include engine widget classes
     * @param bIncludeCustom Include custom widget classes
     * @return TResult containing array of FWidgetClassInfo structures
     */
    TResult<TArray<FWidgetClassInfo>> GetAvailableWidgetClasses(bool bIncludeEngine = true, bool bIncludeCustom = true);

    /**
     * @brief Get widgets by category
     * 
     * @param Category Category name (e.g., "Panel", "Common", "Input")
     * @return TResult containing array of widget class names in that category
     */
    TResult<TArray<FString>> GetWidgetsByCategory(const FString& Category);

    /**
     * @brief Get panel widgets (widgets that can contain children)
     * 
     * @return TResult containing array of panel widget class names
     */
    TResult<TArray<FString>> GetPanelWidgets();

    /**
     * @brief Get common widgets (frequently used widgets)
     * 
     * @return TResult containing array of common widget class names
     */
    TResult<TArray<FString>> GetCommonWidgets();

    /**
     * @brief Get widget class information by name
     * 
     * @param WidgetClassName Name of the widget class
     * @return TResult containing FWidgetClassInfo structure or error
     */
    TResult<FWidgetClassInfo> GetWidgetClassInfo(const FString& WidgetClassName);

    /**
     * @brief Check if a widget class supports children
     * 
     * @param WidgetClassName Name of the widget class
     * @return TResult containing true if supports children, false otherwise
     */
    TResult<bool> SupportsChildren(const FString& WidgetClassName);

    /**
     * @brief Check parent-child compatibility
     * 
     * @param ParentClassName Parent widget class name
     * @param ChildClassName Child widget class name
     * @return TResult containing FWidgetCompatibilityInfo structure
     */
    TResult<FWidgetCompatibilityInfo> CheckCompatibility(const FString& ParentClassName, const FString& ChildClassName);

    /**
     * @brief Get maximum children count for a widget class
     * 
     * @param WidgetClassName Name of the widget class
     * @return TResult containing max children count (-1 for unlimited)
     */
    TResult<int32> GetMaxChildrenCount(const FString& WidgetClassName);

    /**
     * @brief Get widget category for a specific widget class
     * 
     * @param WidgetClassName Name of the widget class
     * @return TResult containing category name
     */
    TResult<FString> GetWidgetCategory(const FString& WidgetClassName);

private:
    /**
     * @brief Discover all widget classes using reflection
     * 
     * @param bIncludeEngine Include engine classes
     * @param bIncludeCustom Include custom classes
     * @return Array of UClass pointers
     */
    TArray<UClass*> DiscoverWidgetClasses(bool bIncludeEngine, bool bIncludeCustom);

    /**
     * @brief Get category for a widget UClass
     * 
     * @param WidgetClass Widget UClass
     * @return Category name
     */
    FString GetCategoryForClass(UClass* WidgetClass);

    /**
     * @brief Check if a UClass supports children
     * 
     * @param WidgetClass Widget UClass
     * @return True if supports children
     */
    bool DoesClassSupportChildren(UClass* WidgetClass);

    /**
     * @brief Get maximum children for a UClass
     * 
     * @param WidgetClass Widget UClass
     * @return Max children count
     */
    int32 GetMaxChildrenForClass(UClass* WidgetClass);
};
