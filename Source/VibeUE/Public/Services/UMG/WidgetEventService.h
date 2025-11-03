#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/TResult.h"

// Forward declarations
class UWidgetBlueprint;
class UFunction;

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
 * @class FWidgetEventService
 * @brief Service responsible for widget event binding and discovery
 * 
 * This service provides focused widget event management functionality extracted from
 * UMGCommands.cpp. It handles event binding, unbinding, discovery, and validation
 * for UMG widget components.
 * 
 * All methods return TResult<T> for type-safe error handling, avoiding the need
 * for runtime JSON parsing in the service layer.
 * 
 * @note This is part of Phase 3 refactoring (Task 15) to extract UMG event operations
 * into a focused service as per CPP_REFACTORING_DESIGN.md
 * 
 * @see TResult
 * @see FServiceBase
 * @see Issue #35
 */
class VIBEUE_API FWidgetEventService : public FServiceBase
{
public:
	/**
	 * @brief Constructor
	 * @param Context Service context for shared state
	 */
	explicit FWidgetEventService(TSharedPtr<FServiceContext> Context);

	/**
	 * @brief Bind an event to a function in a widget blueprint
	 * 
	 * Creates an event node in the widget blueprint's event graph that calls
	 * the specified function when the event fires.
	 * 
	 * @param Widget The widget blueprint to modify
	 * @param ComponentName Name of the component that owns the event
	 * @param EventName Name of the event to bind (e.g., "OnClicked")
	 * @param FunctionName Name of the function to call when event fires
	 * @return Success or error result
	 */
	TResult<void> BindEvent(UWidgetBlueprint* Widget, const FString& ComponentName,
						   const FString& EventName, const FString& FunctionName);

	/**
	 * @brief Unbind an event from a widget blueprint
	 * 
	 * Removes the event binding node from the widget blueprint's event graph.
	 * 
	 * @param Widget The widget blueprint to modify
	 * @param ComponentName Name of the component that owns the event
	 * @param EventName Name of the event to unbind
	 * @return Success or error result
	 */
	TResult<void> UnbindEvent(UWidgetBlueprint* Widget, const FString& ComponentName,
							 const FString& EventName);

	/**
	 * @brief Get all bound events for a component
	 * 
	 * Returns a map of event names to function names for all currently
	 * bound events on the specified component.
	 * 
	 * @param Widget The widget blueprint to query
	 * @param ComponentName Name of the component to check
	 * @return Map of event names to bound function names, or error
	 */
	TResult<TMap<FString, FString>> GetBoundEvents(UWidgetBlueprint* Widget, 
												   const FString& ComponentName);

	/**
	 * @brief Get available events for a component
	 * 
	 * Returns list of event names that can be bound on the specified component.
	 * 
	 * @param Widget The widget blueprint to query
	 * @param ComponentName Name of the component to check
	 * @return Array of available event names, or error
	 */
	TResult<TArray<FString>> GetAvailableEvents(UWidgetBlueprint* Widget, 
											   const FString& ComponentName);

	/**
	 * @brief Get detailed event information for a component
	 * 
	 * Returns comprehensive information about all available events including
	 * signatures, categories, and parameter details.
	 * 
	 * @param Widget The widget blueprint to query
	 * @param ComponentName Name of the component to check
	 * @return Array of event details, or error
	 */
	TResult<TArray<FEventInfo>> GetEventDetails(UWidgetBlueprint* Widget,
											   const FString& ComponentName);

	/**
	 * @brief Check if an event name is valid for a component
	 * 
	 * Validates that the specified event exists on the component class.
	 * 
	 * @param Widget The widget blueprint to check
	 * @param ComponentName Name of the component to check
	 * @param EventName Name of the event to validate
	 * @return True if event is valid, false otherwise, or error
	 */
	TResult<bool> IsValidEvent(UWidgetBlueprint* Widget, const FString& ComponentName,
							  const FString& EventName);

	/**
	 * @brief Check if an event can be bound to a function
	 * 
	 * Validates that the event exists, the function exists, and their
	 * signatures are compatible.
	 * 
	 * @param Widget The widget blueprint to check
	 * @param ComponentName Name of the component that owns the event
	 * @param EventName Name of the event to bind
	 * @param FunctionName Name of the function to call
	 * @return True if binding is valid, false otherwise, or error
	 */
	TResult<bool> CanBindEvent(UWidgetBlueprint* Widget, const FString& ComponentName,
							  const FString& EventName, const FString& FunctionName);

protected:
	/** @brief Gets the service name for logging */
	virtual FString GetServiceName() const override { return TEXT("WidgetEventService"); }

private:
	/**
	 * @brief Find a widget component by name in the widget tree
	 * @param Widget The widget blueprint to search
	 * @param ComponentName Name of the component to find
	 * @return The component widget, or nullptr if not found
	 */
	class UWidget* FindComponent(UWidgetBlueprint* Widget, const FString& ComponentName);

	/**
	 * @brief Get all multicast delegate events from a widget class
	 * @param WidgetClass The widget class to inspect
	 * @param OutEvents Array to populate with event information
	 */
	void GetWidgetEvents(UClass* WidgetClass, TArray<FEventInfo>& OutEvents);

	/**
	 * @brief Check if a function signature matches an event signature
	 * @param EventFunction The event's signature function
	 * @param TargetFunction The function to check compatibility
	 * @return True if signatures are compatible
	 */
	bool SignaturesMatch(UFunction* EventFunction, UFunction* TargetFunction);
};
