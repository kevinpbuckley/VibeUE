#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "Engine/Blueprint.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"

/**
 * Handler class for Blueprint Component Reflection MCP commands
 * Implements 100% reflection-based component discovery and manipulation
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
    
    // Property Reading Methods (NEW for manage_blueprint_components)
    TSharedPtr<FJsonObject> HandleGetComponentProperty(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleGetAllComponentProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleCompareComponentProperties(const TSharedPtr<FJsonObject>& Params);
    TSharedPtr<FJsonObject> HandleReparentComponent(const TSharedPtr<FJsonObject>& Params);

    // Core Reflection Engine
    TArray<UClass*> DiscoverComponentClasses(const TSharedPtr<FJsonObject>& Filters = nullptr);
    TSharedPtr<FJsonObject> ExtractComponentMetadata(UClass* ComponentClass);
    TArray<TSharedPtr<FJsonObject>> ExtractPropertyMetadata(UClass* ComponentClass, bool bIncludeInherited = true);
    TSharedPtr<FJsonObject> ExtractMethodMetadata(UClass* ComponentClass);
    
    // Hierarchy Management
    TSharedPtr<FJsonObject> AnalyzeComponentHierarchy(UBlueprint* Blueprint);
    bool ValidateParentChildCompatibility(UClass* ParentClass, UClass* ChildClass);
    TArray<UClass*> GetCompatibleParents(UClass* ComponentClass);
    TArray<UClass*> GetCompatibleChildren(UClass* ComponentClass);

    // Reflection Utilities
    TSharedPtr<FJsonObject> ConvertPropertyToJson(const FProperty* Property, const void* PropertyValue = nullptr);
    bool SetPropertyFromJson(const FProperty* Property, void* PropertyValue, const TSharedPtr<FJsonValue>& JsonValue);
    FString GetPropertyCPPType(const FProperty* Property);
    TSharedPtr<FJsonObject> GetPropertyConstraints(const FProperty* Property);

    // Component Instance Management
    UActorComponent* FindComponentInBlueprint(UBlueprint* Blueprint, const FString& ComponentName);
    UActorComponent* FindComponentInBlueprint(UBlueprint* Blueprint, const FString& ComponentName, UClass* ExpectedClass);
    bool AttachComponentToParent(USceneComponent* ChildComponent, USceneComponent* ParentComponent, 
                               const FString& AttachmentRule, const FTransform& RelativeTransform);

    // Property Value Extraction (NEW)
    TSharedPtr<FJsonObject> GetComponentPropertyValues(UActorComponent* Component, UClass* ComponentClass);
    TSharedPtr<FJsonValue> PropertyToJsonValue(const FProperty* Property, const void* ValuePtr);

    // Validation and Safety
    bool ValidateComponentType(const FString& ComponentTypeName, UClass*& OutComponentClass);
    bool ValidateComponentName(UBlueprint* Blueprint, const FString& ComponentName);
    TSharedPtr<FJsonObject> ValidateHierarchyOperation(UBlueprint* Blueprint, const FString& ComponentName, 
                                                     const FString& ParentComponentName);

    // Helper Functions
    TSharedPtr<FJsonObject> CreateSuccessResponse(const FString& Message = TEXT(""));
    TSharedPtr<FJsonObject> CreateErrorResponse(const FString& ErrorMessage, const FString& ErrorCode = TEXT(""));
    FString GetFriendlyComponentName(UClass* ComponentClass);
    FString GetComponentCategory(UClass* ComponentClass);
    TArray<FString> GetComponentUsageExamples(UClass* ComponentClass);
    
    // Hierarchy Helper Functions
    void ProcessChildComponents(USCS_Node* ParentNode, TArray<TSharedPtr<FJsonValue>>& ChildrenArray);
    int32 CountComponentsRecursive(const TArray<USCS_Node*>& Nodes);

    // Caching for Performance
    mutable TMap<FString, TArray<UClass*>> CachedComponentsByCategory;
    mutable TMap<UClass*, TSharedPtr<FJsonObject>> CachedComponentMetadata;
    mutable bool bCacheInitialized = false;
    
    void InitializeCache();
    void ClearCache();
};