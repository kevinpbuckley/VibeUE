// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/UMG/Types/WidgetTypes.h"
#include "Core/Result.h"

// Forward declarations
class UWidget;
class UWidgetBlueprint;

/**
 * @class FWidgetHierarchyService
 * @brief Service for widget hierarchy navigation and validation
 * 
 * This service provides widget tree navigation and validation functionality
 * extracted from UMGCommands.cpp. It handles getting widget hierarchies,
 * validating parent-child relationships, and navigating widget trees.
 * 
 * All methods return TResult<T> for type-safe error handling.
 * 
 * @see TResult
 * @see FServiceBase
 */
class VIBEUE_API FWidgetHierarchyService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state (can be nullptr)
     */
    explicit FWidgetHierarchyService(TSharedPtr<FServiceContext> Context);

    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("WidgetHierarchyService"); }

    /**
     * @brief Get complete widget hierarchy for a widget blueprint
     * 
     * @param WidgetBlueprint Widget blueprint to get hierarchy from
     * @return TResult containing FWidgetHierarchy structure or error
     */
    TResult<FWidgetHierarchy> GetWidgetHierarchy(UWidgetBlueprint* WidgetBlueprint);

    /**
     * @brief List all widget components in a blueprint
     * 
     * @param WidgetBlueprint Widget blueprint to list components from
     * @return TResult containing array of FWidgetInfo structures or error
     */
    TResult<TArray<FWidgetInfo>> ListWidgetComponents(UWidgetBlueprint* WidgetBlueprint);

    /**
     * @brief Get information about a specific widget
     * 
     * @param Widget Widget to get information from
     * @return TResult containing FWidgetInfo structure or error
     */
    TResult<FWidgetInfo> GetWidgetInfo(UWidget* Widget);

    /**
     * @brief Get children of a widget
     * 
     * @param WidgetBlueprint Widget blueprint containing the widget
     * @param WidgetName Name of widget to get children for
     * @return TResult containing array of child widget names or error
     */
    TResult<TArray<FString>> GetWidgetChildren(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName);

    /**
     * @brief Get parent of a widget
     * 
     * @param WidgetBlueprint Widget blueprint containing the widget
     * @param WidgetName Name of widget to get parent for
     * @return TResult containing parent widget name (empty if root) or error
     */
    TResult<FString> GetWidgetParent(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName);

    /**
     * @brief Validate widget hierarchy
     * 
     * @param WidgetBlueprint Widget blueprint to validate
     * @return TResult containing validation errors (empty if valid)
     */
    TResult<TArray<FString>> ValidateWidgetHierarchy(UWidgetBlueprint* WidgetBlueprint);

    /**
     * @brief Get root widget of a blueprint
     * 
     * @param WidgetBlueprint Widget blueprint
     * @return TResult containing root widget pointer or error
     */
    TResult<UWidget*> GetRootWidget(UWidgetBlueprint* WidgetBlueprint);

    /**
     * @brief Get widget depth in hierarchy
     * 
     * @param WidgetBlueprint Widget blueprint containing the widget
     * @param WidgetName Name of widget
     * @return TResult containing depth (0 for root) or error
     */
    TResult<int32> GetWidgetDepth(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName);

private:
    /**
     * @brief Build widget hierarchy recursively
     * 
     * @param Widget Current widget
     * @param OutHierarchy Output hierarchy structure
     */
    void BuildHierarchyRecursive(UWidget* Widget, FWidgetHierarchy& OutHierarchy);

    /**
     * @brief Get all widgets in tree recursively
     * 
     * @param Widget Current widget
     * @param OutWidgets Output array of widgets
     */
    void GetAllWidgetsRecursive(UWidget* Widget, TArray<UWidget*>& OutWidgets);
};
