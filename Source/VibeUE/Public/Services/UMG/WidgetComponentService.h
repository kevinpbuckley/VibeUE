#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/UMG/Types/WidgetTypes.h"
#include "Core/Result.h"

// Forward declarations
class UWidget;
class UWidgetBlueprint;
class UPanelWidget;

/**
 * @class FWidgetComponentService
 * @brief Service for adding and removing widget components
 * 
 * This service provides widget component management functionality extracted from
 * UMGCommands.cpp and UMGReflectionCommands.cpp. It handles adding widgets to
 * blueprints, removing widgets, and managing parent-child relationships.
 * 
 * All methods return TResult<T> for type-safe error handling.
 * 
 * @see TResult
 * @see FServiceBase
 */
class VIBEUE_API FWidgetComponentService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state (can be nullptr)
     */
    explicit FWidgetComponentService(TSharedPtr<FServiceContext> Context);

    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("WidgetComponentService"); }

    /**
     * @brief Add a widget component to a widget blueprint
     * 
     * @param WidgetBlueprint Target widget blueprint
     * @param WidgetClassName Class name of widget to add (e.g., "Button", "TextBlock")
     * @param WidgetName Name for the new widget
     * @param ParentName Name of parent widget (empty for root)
     * @param bIsVariable Whether to expose as a variable
     * @return TResult containing the created widget pointer or error
     */
    TResult<UWidget*> AddWidgetComponent(
        UWidgetBlueprint* WidgetBlueprint,
        const FString& WidgetClassName,
        const FString& WidgetName,
        const FString& ParentName = TEXT(""),
        bool bIsVariable = true
    );

    /**
     * @brief Remove a widget component from a widget blueprint
     * 
     * @param WidgetBlueprint Target widget blueprint
     * @param WidgetName Name of widget to remove
     * @return TResult indicating success or error
     */
    TResult<void> RemoveWidgetComponent(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName);

    /**
     * @brief Add a child widget to a panel widget
     * 
     * @param WidgetBlueprint Target widget blueprint
     * @param ChildName Name of child widget
     * @param ParentName Name of parent panel widget
     * @return TResult indicating success or error
     */
    TResult<void> AddChildToPanel(
        UWidgetBlueprint* WidgetBlueprint,
        const FString& ChildName,
        const FString& ParentName
    );

    /**
     * @brief Create and add a specific widget type with properties
     * 
     * @param WidgetBlueprint Target widget blueprint
     * @param WidgetClassName Widget class name
     * @param WidgetName Widget name
     * @param ParentName Parent widget name
     * @param bIsVariable Whether to expose as variable
     * @param InitialProperties Map of initial property values
     * @return TResult containing the created widget or error
     */
    TResult<UWidget*> CreateAndAddWidget(
        UWidgetBlueprint* WidgetBlueprint,
        const FString& WidgetClassName,
        const FString& WidgetName,
        const FString& ParentName,
        bool bIsVariable,
        const TMap<FString, FString>& InitialProperties
    );

    /**
     * @brief Validate widget creation parameters
     * 
     * @param WidgetBlueprint Target widget blueprint
     * @param WidgetClassName Widget class to validate
     * @param WidgetName Proposed widget name
     * @param ParentName Parent widget name
     * @return TResult indicating if parameters are valid
     */
    TResult<bool> ValidateWidgetCreation(
        UWidgetBlueprint* WidgetBlueprint,
        const FString& WidgetClassName,
        const FString& WidgetName,
        const FString& ParentName
    );

    /**
     * @brief Get parent panel widget for a widget
     * 
     * @param WidgetBlueprint Widget blueprint containing the widget
     * @param WidgetName Name of widget to get parent for
     * @return TResult containing parent panel widget or error
     */
    TResult<UPanelWidget*> GetParentPanel(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName);

private:
    /**
     * @brief Get widget class from name
     * 
     * @param WidgetClassName Class name
     * @return UClass pointer or nullptr
     */
    UClass* GetWidgetClass(const FString& WidgetClassName);

    /**
     * @brief Check if widget name is unique in blueprint
     * 
     * @param WidgetBlueprint Widget blueprint
     * @param WidgetName Name to check
     * @return True if unique
     */
    bool IsWidgetNameUnique(UWidgetBlueprint* WidgetBlueprint, const FString& WidgetName);
};
