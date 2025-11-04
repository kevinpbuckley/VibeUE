#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Engine/Blueprint.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"

// Forward declarations for editor-only types
class USCS_Node;

/**
 * Handler class for Blueprint Component Reflection MCP commands
 * Phase 4: Refactored to use service layer architecture
 * Delegates component operations to BlueprintComponentService and BlueprintReflectionService
 */
class VIBEUE_API FBlueprintComponentReflection
{
public:
    FBlueprintComponentReflection();

    // Handle component reflection commands
    TSharedPtr<FJsonObject> HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
    // Discovery Methods (100% Reflection-Based)
    TSharedPtr<FJsonObject> HandleGetAvailableComponents(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetComponentInfo(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetPropertyMetadata(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetComponentHierarchy(const TSharedPtr<FJsonObject>& Params);

    // Manipulation Methods (100% Reflection-Based)
    TSharedPtr<FJsonObject> HandleAddComponent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleRemoveComponent(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReorderComponents(const TSharedPtr<FJsonObject>& Params);
    
    // Property Reading Methods (NEW for manage_blueprint_component)
    TSharedPtr<FJsonObject> HandleGetComponentProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAllComponentProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCompareComponentProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReparentComponent(const TSharedPtr<FJsonObject>& Params);

    // Helper Functions for TResult to JSON conversion
    TSharedPtr<FJsonObject> CreateSuccessResponse(const FString& Message = TEXT("")) const;
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorCode, const FString& ErrorMessage) const;

    // Phase 4: Blueprint Services (replacing inline logic)
    TSharedPtr<class FBlueprintDiscoveryService> DiscoveryService;
    TSharedPtr<class FBlueprintComponentService> ComponentService;
    TSharedPtr<class FBlueprintReflectionService> ReflectionService;
    TSharedPtr<class FBlueprintPropertyService> PropertyService;
};