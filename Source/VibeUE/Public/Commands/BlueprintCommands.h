#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Core/Result.h"

/**
 * Handler class for Blueprint-related MCP commands
 * Phase 4: Refactored to use service layer architecture
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
    TSharedPtr<FJsonObject> HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetBlueprintProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetBlueprintProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPawnProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReparentBlueprint(const TSharedPtr<FJsonObject>& Params);

    // Variable management commands (internal helpers for HandleManageBlueprintVariables)
    TSharedPtr<FJsonObject> HandleGetBlueprintVariableInfo(const TSharedPtr<FJsonObject>& Params);  
    TSharedPtr<FJsonObject> HandleDeleteBlueprintVariable(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAvailableBlueprintVariableTypes(const TSharedPtr<FJsonObject>& Params);

    // NEW: Unified Variable Management System
    TSharedPtr<FJsonObject> HandleManageBlueprintVariables(const TSharedPtr<FJsonObject>& Params);

    // Comprehensive Blueprint information
    TSharedPtr<FJsonObject> HandleGetBlueprintInfo(const TSharedPtr<FJsonObject>& Params);

    // Reflection-based variable property access (two-method API)
    TSharedPtr<FJsonObject> HandleGetVariableProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetVariableProperty(const TSharedPtr<FJsonObject>& Params);
    
    // Internal Blueprint variable metadata management
    TSharedPtr<FJsonObject> GetBlueprintVariableMetadata(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> SetBlueprintVariableMetadata(const TSharedPtr<FJsonObject>& Params);

    // NEW: Variable Management System Support Functions
    TSharedPtr<FJsonObject> HandleCreateVariableOperation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleDeleteVariableOperation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleModifyVariableOperation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleListVariablesOperation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetVariableInfoOperation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetPropertyOperation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSetPropertyOperation(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleSearchTypesOperation(const TSharedPtr<FJsonObject>& Params);

    // NEW: Reflection-based Type Discovery System
    TArray<UClass*> DiscoverAllVariableTypes();
    TArray<UClass*> FilterVariableTypes(const TSharedPtr<FJsonObject>& SearchCriteria);
    TSharedPtr<FJsonObject> GetTypeMetadata(UClass* VariableClass);
    bool IsValidBlueprintVariableType(UClass* Class);
    TArray<FAssetData> FindBlueprintAssetClasses(UClass* BaseClass);
    
    // NEW: Enhanced Type Resolution System
    bool ResolveVariableType(const FString& TypeName, const FString& TypePath, FEdGraphPinType& OutPinType);
    FString GetTypeDisplayName(const FEdGraphPinType& PinType);
    UClass* FindClassByName(const FString& ClassName);
    UScriptStruct* FindStructByName(const FString& StructName);
    UEnum* FindEnumByName(const FString& EnumName);

    // Helper functions
    TSharedPtr<FJsonObject> AddComponentToBlueprint(const FString& BlueprintName, const FString& ComponentType, 
                                                   const FString& ComponentName, const FString& MeshType,
                                                   const TArray<float>& Location, const TArray<float>& Rotation,
                                                   const TArray<float>& Scale, const TSharedPtr<FJsonObject>& ComponentProperties);

private:
    // Helper methods to convert TResult to JSON
    TSharedPtr<FJsonObject> CreateSuccessResponse() const;
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorCode, const FString& ErrorMessage) const;
    
    // Phase 4: Blueprint Services (replacing inline logic)
    TSharedPtr<class FBlueprintDiscoveryService> DiscoveryService;
    TSharedPtr<class FBlueprintLifecycleService> LifecycleService;
    TSharedPtr<class FBlueprintPropertyService> PropertyService;
    TSharedPtr<class FBlueprintComponentService> ComponentService;
    TSharedPtr<class FBlueprintFunctionService> FunctionService;
    TSharedPtr<class FBlueprintGraphService> GraphService;
    TSharedPtr<class FBlueprintReflectionService> ReflectionService;
}; 
