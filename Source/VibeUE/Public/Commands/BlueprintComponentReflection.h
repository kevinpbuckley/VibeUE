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

    // Component Discovery and Reflection Helper Methods
    TArray<UClass*> DiscoverComponentClasses(const TSharedPtr<FJsonObject>& Filters);
    TSharedPtr<FJsonObject> ExtractComponentMetadata(UClass* ComponentClass);
    TArray<TSharedPtr<FJsonObject>> ExtractPropertyMetadata(UClass* ComponentClass, bool bIncludeInherited = true);
    TSharedPtr<FJsonObject> ExtractMethodMetadata(UClass* ComponentClass);
    
    // Hierarchy Analysis Helper Methods
    TSharedPtr<FJsonObject> AnalyzeComponentHierarchy(UBlueprint* Blueprint);
    void ProcessChildComponents(USCS_Node* ParentNode, TArray<TSharedPtr<FJsonValue>>& ChildrenArray);
    int32 CountComponentsRecursive(const TArray<USCS_Node*>& Nodes);
    
    // Validation Helper Methods
    bool ValidateComponentType(const FString& ComponentTypeName, UClass*& OutComponentClass);
    bool ValidateComponentName(UBlueprint* Blueprint, const FString& ComponentName);
    bool ValidateParentChildCompatibility(UClass* ParentClass, UClass* ChildClass);
    TSharedPtr<FJsonObject> ValidateHierarchyOperation(UBlueprint* Blueprint, const FString& ComponentName, const FString& ParentComponentName);
    
    // Compatibility Helper Methods
    TArray<UClass*> GetCompatibleParents(UClass* ComponentClass);
    TArray<UClass*> GetCompatibleChildren(UClass* ComponentClass);
    
    // Property Conversion Helper Methods
    TSharedPtr<FJsonObject> ConvertPropertyToJson(const FProperty* Property, const void* PropertyValue = nullptr);
    bool SetPropertyFromJson(const FProperty* Property, void* PropertyValue, const TSharedPtr<FJsonValue>& JsonValue);
    TSharedPtr<FJsonValue> PropertyToJsonValue(const FProperty* Property, const void* ValuePtr);
    
    // Property Metadata Helper Methods
    FString GetPropertyCPPType(const FProperty* Property);
    TSharedPtr<FJsonObject> GetPropertyConstraints(const FProperty* Property);
    
    // Component Utility Helper Methods
    FString GetFriendlyComponentName(UClass* ComponentClass);
    FString GetComponentCategory(UClass* ComponentClass);
    TArray<FString> GetComponentUsageExamples(UClass* ComponentClass);
    
    // Component Search Helper Methods
    UActorComponent* FindComponentInBlueprint(UBlueprint* Blueprint, const FString& ComponentName);
    UActorComponent* FindComponentInBlueprint(UBlueprint* Blueprint, const FString& ComponentName, UClass* ExpectedClass);
    TSharedPtr<FJsonObject> GetComponentPropertyValues(UActorComponent* Component, UClass* ComponentClass);
    
    // Cache Management Helper Methods
    void InitializeCache();
    void ClearCache();
    
    // Cache Members
    TMap<FString, TArray<UClass*>> CachedComponentsByCategory;
    TMap<UClass*, TSharedPtr<FJsonObject>> CachedComponentMetadata;
    bool bCacheInitialized = false;

    // Phase 4: Blueprint Services (replacing inline logic)
    TSharedPtr<class FBlueprintDiscoveryService> DiscoveryService;
    TSharedPtr<class FBlueprintComponentService> ComponentService;
    TSharedPtr<class FBlueprintReflectionService> ReflectionService;
    TSharedPtr<class FBlueprintPropertyService> PropertyService;
};