#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/UMG/WidgetDiscoveryService.h"
#include "Core/Result.h"

// Forward declarations
class UWidgetBlueprint;
class UWidget;

/**
 * @class FWidgetLifecycleService
 * @brief Service responsible for UMG Widget lifecycle operations
 * 
 * This service provides focused widget lifecycle functionality including validation,
 * editor operations (opening/closing), and widget information retrieval.
 * 
 * Extracted from UMGCommands.cpp as part of Phase 3 refactoring (Task 11) to create
 * focused services that operate with TResult instead of JSON for type safety.
 * 
 * All methods return TResult<T> for type-safe error handling.
 * 
 * @note This is part of Phase 3 refactoring (Task 11) to extract UMG domain into
 * focused services as per CPP_REFACTORING_DESIGN.md
 * 
 * @see TResult
 * @see FServiceBase
 * @see FWidgetDiscoveryService
 * @see Issue #32
 */
class VIBEUE_API FWidgetLifecycleService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state
     */
    explicit FWidgetLifecycleService(TSharedPtr<FServiceContext> Context);

    // ========================================================================
    // Editor Operations
    // ========================================================================
    
    /**
     * @brief Open a widget blueprint in the editor
     * 
     * Opens the specified widget blueprint in the Unreal Engine widget editor.
     * 
     * @param WidgetName Name or path of the widget blueprint to open
     * @return Success or error result
     * 
     * @note Currently a stub - full implementation requires IAssetEditorInstance integration
     */
    TResult<void> OpenWidgetInEditor(const FString& WidgetName);
    
    /**
     * @brief Check if a widget blueprint is currently open in the editor
     * 
     * Verifies whether the specified widget blueprint has an active editor instance.
     * 
     * @param WidgetName Name or path of the widget blueprint
     * @return TResult containing true if widget is open, false otherwise
     * 
     * @note Currently a stub - full implementation requires IAssetEditorInstance integration
     */
    TResult<bool> IsWidgetOpen(const FString& WidgetName);
    
    /**
     * @brief Close a widget blueprint editor
     * 
     * Closes the editor instance for the specified widget blueprint.
     * 
     * @param WidgetName Name or path of the widget blueprint to close
     * @return Success or error result
     * 
     * @note Currently a stub - full implementation requires IAssetEditorInstance integration
     */
    TResult<void> CloseWidget(const FString& WidgetName);
    
    // ========================================================================
    // Validation
    // ========================================================================
    
    /**
     * @brief Validate a widget blueprint's hierarchy
     * 
     * Performs comprehensive validation of the widget blueprint's hierarchy including:
     * - Widget tree existence and structure
     * - Root widget validation
     * - Child widget compatibility
     * - Circular reference detection
     * 
     * @param Widget The widget blueprint to validate
     * @return TResult containing array of validation error messages (empty if valid)
     */
    TResult<TArray<FString>> ValidateWidget(UWidgetBlueprint* Widget);
    
    /**
     * @brief Check if a widget blueprint is valid
     * 
     * Quick validation check to determine if a widget blueprint is structurally valid.
     * 
     * @param Widget The widget blueprint to validate
     * @return TResult containing true if valid, false otherwise
     */
    TResult<bool> IsWidgetValid(UWidgetBlueprint* Widget);
    
    /**
     * @brief Validate widget hierarchy structure
     * 
     * Detailed validation of widget hierarchy ensuring proper parent-child relationships
     * and detecting any structural issues.
     * 
     * @param Widget The widget blueprint to validate
     * @return Success or error result
     */
    TResult<void> ValidateHierarchy(UWidgetBlueprint* Widget);
    
    // ========================================================================
    // Info
    // ========================================================================
    
    /**
     * @brief Get detailed information about a widget blueprint
     * 
     * Retrieves comprehensive widget blueprint metadata including name, path,
     * parent class, and type information.
     * 
     * @param Widget The widget blueprint to query
     * @return TResult containing FWidgetInfo structure
     */
    TResult<FWidgetInfo> GetWidgetInfo(UWidgetBlueprint* Widget);
    
    /**
     * @brief Get widget categories/tags
     * 
     * Retrieves categorization metadata for the widget blueprint.
     * 
     * @param Widget The widget blueprint to query
     * @return TResult containing array of category names
     */
    TResult<TArray<FString>> GetWidgetCategories(UWidgetBlueprint* Widget);

protected:
    virtual FString GetServiceName() const override { return TEXT("WidgetLifecycleService"); }

private:
    /**
     * @brief Validate individual widget in hierarchy
     * @param Widget The widget to validate
     * @param Errors Output array for validation errors
     */
    void ValidateWidgetRecursive(UWidget* Widget, TArray<FString>& Errors);
    
    /**
     * @brief Check for circular references in widget hierarchy
     * @param Widget The widget to check
     * @param Visited Set of already visited widgets
     * @return True if circular reference detected
     */
    bool DetectCircularReference(UWidget* Widget, TSet<UWidget*>& Visited);
};
