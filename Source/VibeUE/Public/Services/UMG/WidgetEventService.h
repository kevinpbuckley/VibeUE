#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"

// Forward declarations
class UWidget;
class UWidgetBlueprint;

/**
 * @class FWidgetEventService
 * @brief Service for widget event binding and management
 * 
 * This service provides event binding functionality extracted from UMGCommands.cpp.
 * It handles binding widget events, getting available events, and managing event
 * callbacks.
 * 
 * All methods return TResult<T> for type-safe error handling.
 * 
 * @see TResult
 * @see FServiceBase
 */
class VIBEUE_API FWidgetEventService : public FServiceBase
{
public:
    /**
     * @brief Constructor
     * @param Context Service context for shared state (can be nullptr)
     */
    explicit FWidgetEventService(TSharedPtr<FServiceContext> Context);

    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("WidgetEventService"); }

    /**
     * @brief Get available events for a widget
     * 
     * @param Widget Widget to get events for
     * @return TResult containing array of event names or error
     */
    TResult<TArray<FString>> GetAvailableEvents(UWidget* Widget);

    /**
     * @brief Bind an event for a widget
     * 
     * @note Current implementation validates event existence only. Full event binding
     * requires creating event graph nodes and function delegates, which is deferred
     * to future implementation. Callers should check return value for validation.
     * 
     * @param WidgetBlueprint Widget blueprint containing the widget
     * @param WidgetName Name of widget to bind event for
     * @param EventName Name of event to bind (e.g., "OnClicked")
     * @param FunctionName Name of function to bind to
     * @return TResult indicating success or error
     */
    TResult<void> BindEvent(
        UWidgetBlueprint* WidgetBlueprint,
        const FString& WidgetName,
        const FString& EventName,
        const FString& FunctionName
    );

    /**
     * @brief Get bound events for a widget
     * 
     * @note Current implementation returns empty map. Full implementation requires
     * inspecting event graph nodes for bound events, which is deferred to future work.
     * 
     * @param WidgetBlueprint Widget blueprint containing the widget
     * @param WidgetName Name of widget to get bound events for
     * @return TResult containing map of event name to function name or error
     */
    TResult<TMap<FString, FString>> GetBoundEvents(
        UWidgetBlueprint* WidgetBlueprint,
        const FString& WidgetName
    );

    /**
     * @brief Unbind an event
     * 
     * @note Current implementation validates parameters only. Full unbinding requires
     * removing event graph nodes, which is deferred to future implementation.
     * 
     * @param WidgetBlueprint Widget blueprint containing the widget
     * @param WidgetName Name of widget
     * @param EventName Name of event to unbind
     * @return TResult indicating success or error
     */
    TResult<void> UnbindEvent(
        UWidgetBlueprint* WidgetBlueprint,
        const FString& WidgetName,
        const FString& EventName
    );

    /**
     * @brief Check if an event is bound
     * 
     * @note Current implementation always returns false. Full implementation requires
     * checking event graph nodes, which is deferred to future work.
     * 
     * @param WidgetBlueprint Widget blueprint containing the widget
     * @param WidgetName Name of widget
     * @param EventName Name of event to check
     * @return TResult containing true if bound, false otherwise
     */
    TResult<bool> IsEventBound(
        UWidgetBlueprint* WidgetBlueprint,
        const FString& WidgetName,
        const FString& EventName
    );

private:
    /**
     * @brief Get multicast delegate property for event
     * 
     * @param Widget Widget to get property from
     * @param EventName Event name
     * @return Property pointer or nullptr
     */
    FMulticastDelegateProperty* GetEventProperty(UWidget* Widget, const FString& EventName);
};
