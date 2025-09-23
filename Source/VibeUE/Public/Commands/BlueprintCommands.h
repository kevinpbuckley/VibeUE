#pragma once

#include "CoreMinimal.h"
#include "Json.h"

/**
 * Handler class for Blueprint-related MCP commands
 */
class VIBEUE_API FBlueprintCommands
{
public:
    FBlueprintCommands();

    // Handle blueprint commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Specific blueprint command handlers
    TSharedPtr<FJsonObject> HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleAddComponentToBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetBlueprintProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPawnProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReparentBlueprint(const TSharedPtr<FJsonObject>& Params);

    // Variable management commands
    TSharedPtr<FJsonObject> HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetBlueprintVariable(const TSharedPtr<FJsonObject>& Params);  
    TSharedPtr<FJsonObject> HandleSetBlueprintVariable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteBlueprintVariable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAvailableBlueprintVariableTypes(const TSharedPtr<FJsonObject>& Params);

    // Helper functions
    TSharedPtr<FJsonObject> AddComponentToBlueprint(const FString& BlueprintName, const FString& ComponentType, 
                                                   const FString& ComponentName, const FString& MeshType,
                                                   const TArray<float>& Location, const TArray<float>& Rotation,
                                                   const TArray<float>& Scale, const TSharedPtr<FJsonObject>& ComponentProperties);
}; 
