#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/Blueprint/BlueprintReflectionService.h"
#include "Core/Result.h"

// Forward declarations
class UClass;
class UWidget;
class FProperty;

/**
 * @struct FWidgetTypeInfo
 * @brief Structure holding metadata about a widget type
 * 
 * Contains information about a widget class including its type name,
 * full class path, whether it's a panel type, and other metadata.
 */
struct VIBEUE_API FWidgetTypeInfo
{
    /** The widget type name (e.g., "Button", "TextBlock") */
    FString TypeName;
    
    /** Full class path to the widget class */
    FString ClassPath;
    
    /** Display category for the widget */
    FString Category;
    
    /** Whether this widget type can contain children */
    bool bIsPanelWidget;
    
    /** Whether this is a commonly used widget type */
    bool bIsCommonWidget;
    
    FWidgetTypeInfo()
        : bIsPanelWidget(false)
        , bIsCommonWidget(false)
    {
    }
};

/**
 * @class FWidgetReflectionService
 * @brief Service responsible for UMG widget type discovery and metadata
 * 
 * This service provides focused widget type reflection functionality extracted from
 * UMGCommands.cpp. It handles widget type enumeration, categorization, validation,
 * and metadata extraction.
 * 
 * All methods return TResult<T> for type-safe error handling, avoiding the need
 * for runtime JSON parsing in the service layer.
 * 
 * @note This is part of Phase 3 refactoring (Task 16) to extract UMG domain into
 * focused services as per CPP_REFACTORING_DESIGN.md
 * 
 * @see TResult
 * @see FServiceBase
 * @see Issue #32
 */
class VIBEUE_API FWidgetReflectionService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state
     */
    explicit FWidgetReflectionService(TSharedPtr<FServiceContext> Context);
    
    // ═══════════════════════════════════════════════════════════
    // Type Discovery
    // ═══════════════════════════════════════════════════════════
    
    /**
     * @brief Get list of available widget types
     * 
     * Returns all supported widget types that can be added to widget blueprints.
     * Optionally filter by category.
     * 
     * @param Category Optional category filter (e.g., "Panel", "Common")
     * @return TResult containing array of widget type names
     */
    TResult<TArray<FString>> GetAvailableWidgetTypes(const FString& Category = TEXT(""));
    
    /**
     * @brief Get list of widget categories
     * 
     * Returns all available widget categories (e.g., "Panel", "Common", "Input").
     * 
     * @return TResult containing array of category names
     */
    TResult<TArray<FString>> GetWidgetCategories();
    
    /**
     * @brief Get list of panel widget types
     * 
     * Returns widget types that can contain other widgets (CanvasPanel, VerticalBox, etc.).
     * 
     * @return TResult containing array of panel widget type names
     */
    TResult<TArray<FString>> GetPanelWidgets();
    
    /**
     * @brief Get list of common widget types
     * 
     * Returns frequently used widget types for quick access.
     * 
     * @return TResult containing array of common widget type names
     */
    TResult<TArray<FString>> GetCommonWidgets();
    
    // ═══════════════════════════════════════════════════════════
    // Type Metadata
    // ═══════════════════════════════════════════════════════════
    
    /**
     * @brief Get detailed information about a widget type
     * 
     * Returns metadata including class path, category, and capabilities.
     * 
     * @param WidgetType Widget type name (e.g., "Button")
     * @return TResult containing widget type information
     */
    TResult<FWidgetTypeInfo> GetWidgetTypeInfo(const FString& WidgetType);
    
    /**
     * @brief Get properties available for a widget type
     * 
     * Returns list of editable properties for the specified widget type.
     * Uses FPropertyInfo from BlueprintReflectionService.h
     * 
     * @param WidgetType Widget type name
     * @return TResult containing array of property information
     */
    TResult<TArray<FPropertyInfo>> GetWidgetTypeProperties(const FString& WidgetType);
    
    /**
     * @brief Get events available for a widget type
     * 
     * Returns list of bindable events for the specified widget type.
     * 
     * @param WidgetType Widget type name
     * @return TResult containing array of event names
     */
    TResult<TArray<FString>> GetWidgetTypeEvents(const FString& WidgetType);
    
    // ═══════════════════════════════════════════════════════════
    // Type Validation
    // ═══════════════════════════════════════════════════════════
    
    /**
     * @brief Check if a widget type is valid/supported
     * 
     * Validates that the widget type exists and can be used.
     * 
     * @param WidgetType Widget type name to validate
     * @return TResult containing true if valid, false otherwise
     */
    TResult<bool> IsValidWidgetType(const FString& WidgetType);
    
    /**
     * @brief Check if a widget type is a panel widget
     * 
     * Determines whether the widget type can contain children.
     * 
     * @param WidgetType Widget type name to check
     * @return TResult containing true if panel widget, false otherwise
     */
    TResult<bool> IsPanelWidget(const FString& WidgetType);
    
    /**
     * @brief Check if a widget type can contain children
     * 
     * Determines whether widgets of this type can have child widgets.
     * 
     * @param WidgetType Widget type name to check
     * @return TResult containing true if can contain children, false otherwise
     */
    TResult<bool> CanContainChildren(const FString& WidgetType);
    
    // ═══════════════════════════════════════════════════════════
    // Type Resolution
    // ═══════════════════════════════════════════════════════════
    
    /**
     * @brief Resolve widget type name to UClass
     * 
     * Converts a widget type name to its corresponding UClass pointer.
     * 
     * @param WidgetType Widget type name
     * @return TResult containing UClass pointer
     */
    TResult<UClass*> ResolveWidgetClass(const FString& WidgetType);
    
    /**
     * @brief Get full class path for a widget type
     * 
     * Returns the full Unreal Engine class path for a widget type.
     * 
     * @param WidgetType Widget type name
     * @return TResult containing full class path
     */
    TResult<FString> GetWidgetTypePath(const FString& WidgetType);

protected:
    /**
     * @brief Gets the service name for logging
     * @return The service name "WidgetReflectionService"
     */
    virtual FString GetServiceName() const override { return TEXT("WidgetReflectionService"); }

private:
    /**
     * @brief Initialize widget type catalogs
     * 
     * Builds internal lookup tables for widget types, panels, and common widgets.
     */
    void InitializeWidgetCatalogs();
    
    /**
     * @brief Check if catalogs have been initialized
     * @return True if initialized, false otherwise
     */
    bool AreCatalogsInitialized() const { return bCatalogsInitialized; }
    
    // Widget type catalogs
    TArray<FString> AllWidgetTypes;
    TArray<FString> PanelWidgetTypes;
    TArray<FString> CommonWidgetTypes;
    TMap<FString, FString> WidgetTypeToClassPath;
    TMap<FString, FString> WidgetTypeToCategory;
    TMap<FString, TArray<FString>> WidgetTypeToEvents;
    
    // Initialization flag
    bool bCatalogsInitialized;
};
