#pragma once

#include "CoreMinimal.h"
#include "Json.h"

// Forward declare reflection commands
class FBlueprintReflectionCommands;

/**
 * Handler class for Blueprint Node-related MCP commands
 * Enhanced with reflection-based node discovery and manipulation
 */
class VIBEUE_API FBlueprintNodeCommands
{
public:
    FBlueprintNodeCommands();

    // Handle blueprint node commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Existing blueprint node command handlers
    TSharedPtr<FJsonObject> HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintGetSelfComponentReference(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintEvent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintFunctionCall(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintInputActionNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintSelfReference(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListEventGraphNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetNodeDetails(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListBlueprintFunctions(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListCustomEvents(const TSharedPtr<FJsonObject>& Params);
    
    // NEW: Reflection-based command handlers
    TSharedPtr<FJsonObject> HandleGetAvailableBlueprintNodes(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params);
    
private:
    // Reflection system helper
    TSharedPtr<FBlueprintReflectionCommands> ReflectionCommands;
}; 
