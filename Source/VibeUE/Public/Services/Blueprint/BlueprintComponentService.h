#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/Blueprint/Types/ComponentTypes.h"
#include "Core/Result.h"
#include "Engine/Blueprint.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBlueprintComponentService, Log, All);

/**
 * Service for Blueprint component CRUD operations
 * Extracted from BlueprintComponentReflection.cpp for focused, testable implementation
 */
class VIBEUE_API FBlueprintComponentService : public FServiceBase
{
public:
    explicit FBlueprintComponentService(TSharedPtr<FServiceContext> Context);
    ~FBlueprintComponentService() = default;

    // FServiceBase interface
    virtual FString GetServiceName() const override { return TEXT("BlueprintComponentService"); }

    /**
     * Add a new component to a Blueprint
     * @param Blueprint Target blueprint
     * @param ComponentType Component class name or path
     * @param ComponentName Name for the new component
     * @param ParentName Optional parent component name
     * @param RelativeTransform Optional transform for scene components
     * @return Result containing the created component or error
     */
    TResult<UActorComponent*> AddComponent(
        UBlueprint* Blueprint, 
        const FString& ComponentType, 
        const FString& ComponentName,
        const FString& ParentName = FString(),
        const FTransform& RelativeTransform = FTransform::Identity);

    /**
     * Remove a component from a Blueprint
     * @param Blueprint Target blueprint
     * @param ComponentName Name of component to remove
     * @param bRemoveChildren If true, remove child components; if false, reparent them
     * @return Result indicating success or error
     */
    TResult<void> RemoveComponent(
        UBlueprint* Blueprint, 
        const FString& ComponentName,
        bool bRemoveChildren = true);

    /**
     * List all components in a Blueprint with hierarchy information
     * @param Blueprint Target blueprint
     * @return Result containing array of component information
     */
    TResult<TArray<FComponentInfo>> ListComponents(UBlueprint* Blueprint);

    /**
     * Reorder components in the Blueprint's construction script
     * @param Blueprint Target blueprint
     * @param ComponentOrder Ordered array of component names
     * @return Result indicating success or error
     */
    TResult<void> ReorderComponents(
        UBlueprint* Blueprint, 
        const TArray<FString>& ComponentOrder);

    /**
     * Change the parent of a component
     * @param Blueprint Target blueprint
     * @param ComponentName Component to reparent
     * @param NewParentName New parent component name
     * @return Result indicating success or error
     */
    TResult<void> ReparentComponent(
        UBlueprint* Blueprint, 
        const FString& ComponentName, 
        const FString& NewParentName);

    /**
     * Get all available component events using reflection
     * @param Blueprint Target blueprint
     * @param ComponentNameFilter Optional filter for specific component (empty = all components)
     * @return Result containing component events grouped by component name
     */
    TResult<FComponentEventsResult> GetComponentEvents(
        UBlueprint* Blueprint,
        const FString& ComponentNameFilter = FString());

    /**
     * Create a component event binding node
     * @param Blueprint Target blueprint
     * @param ComponentName Component with the delegate
     * @param DelegateName Delegate property name
     * @param Position Position for the event node
     * @return Result containing created event node information
     */
    TResult<FComponentEventResult> CreateComponentEvent(
        UBlueprint* Blueprint,
        const FString& ComponentName,
        const FString& DelegateName,
        const FVector2D& Position);

private:
    /**
     * Validate and resolve a component type string to a UClass
     * @param ComponentType Component type name or path
     * @param OutComponentClass Resolved component class
     * @return True if valid component type
     */
    bool ValidateComponentType(const FString& ComponentType, UClass*& OutComponentClass);

    /**
     * Check if a component name already exists in the Blueprint
     * @param Blueprint Target blueprint
     * @param ComponentName Name to check
     * @return True if name is unique (valid)
     */
    bool ValidateComponentName(UBlueprint* Blueprint, const FString& ComponentName);

    /**
     * Recursively collect component information from SCS nodes
     * @param Nodes Array of SCS nodes to process
     * @param OutComponents Output array for component information
     */
    void CollectComponentInfo(const TArray<class USCS_Node*>& Nodes, TArray<FComponentInfo>& OutComponents);

    /**
     * Process a single SCS node and its children
     * @param Node Node to process
     * @param OutComponents Output array for component information
     */
    void ProcessComponentNode(class USCS_Node* Node, TArray<FComponentInfo>& OutComponents);
};
