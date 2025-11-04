#pragma once

#include "CoreMinimal.h"
#include "Json.h"

// Forward declarations
class UWidget;
class UWidgetBlueprint;
class UUserWidget;
class FWidgetDiscoveryService;
class FWidgetLifecycleService;
class FWidgetComponentService;
class FWidgetPropertyService;
class FWidgetStyleService;
class FWidgetEventService;
class FWidgetReflectionService;
class FServiceContext;

/**
 * Handles UMG (Widget Blueprint) related MCP commands
 * Thin command handler that delegates to UMG services.
 * Refactored in Phase 4, Task 18 to use service layer.
 */
class VIBEUE_API FUMGCommands
{
public:
    FUMGCommands();

    /**
     * Handle UMG-related commands
     * @param CommandType - The type of command to handle
     * @param Params - JSON parameters for the command
     * @return JSON response with results or error
     */
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    /**
     * Create a new UMG Widget Blueprint
     * @param Params - Must include "name" for the blueprint name
     * @return JSON response with the created blueprint details
     */
    TSharedPtr<FJsonObject> HandleCreateUMGWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);

    // UMG Discovery Methods
    TSharedPtr<FJsonObject> HandleSearchItems(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetWidgetBlueprintInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListWidgetComponents(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetWidgetComponentProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAvailableWidgetTypes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleValidateWidgetHierarchy(const TSharedPtr<FJsonObject>& Params);

    // UMG Layout/Advanced Methods
    TSharedPtr<FJsonObject> HandleAddWidgetSwitcherSlot(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddChildToPanel(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveUMGComponent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetWidgetSlotProperties(const TSharedPtr<FJsonObject>& Params);

    // Property Management Methods
    TSharedPtr<FJsonObject> HandleSetWidgetProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetWidgetProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListWidgetProperties(const TSharedPtr<FJsonObject>& Params);

    // Event and Data Binding Methods
    TSharedPtr<FJsonObject> HandleBindInputEvents(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAvailableEvents(const TSharedPtr<FJsonObject>& Params);

    // Deletion Methods
    TSharedPtr<FJsonObject> HandleDeleteWidgetBlueprint(const TSharedPtr<FJsonObject>& Params);

private:
    // Service instances
    TSharedPtr<FServiceContext> ServiceContext;
    TSharedPtr<FWidgetDiscoveryService> DiscoveryService;
    TSharedPtr<FWidgetLifecycleService> LifecycleService;
    TSharedPtr<FWidgetComponentService> ComponentService;
    TSharedPtr<FWidgetPropertyService> PropertyService;
    TSharedPtr<FWidgetStyleService> StyleService;
    TSharedPtr<FWidgetEventService> EventService;
    TSharedPtr<FWidgetReflectionService> ReflectionService;
    
    // Helper methods for JSON conversion
    TSharedPtr<FJsonObject> CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data = nullptr);
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorCode, const FString& ErrorMessage);
    TSharedPtr<FJsonObject> ComponentToJson(UWidget* Component);
    TArray<TSharedPtr<FJsonValue>> StringArrayToJson(const TArray<FString>& Strings);
    TSharedPtr<FJsonObject> ComponentInfoToJson(const FWidgetComponentInfo& Info);
    TSharedPtr<FJsonObject> PropertyInfoToJson(const FPropertyInfo& Info);
    TSharedPtr<FJsonObject> HandleAddComponentGeneric(const TSharedPtr<FJsonObject>& Params, const FString& ComponentType);
    TSharedPtr<FJsonObject> FindWidgetOrError(const FString& WidgetName, UWidgetBlueprint*& OutWidget);
}; 
