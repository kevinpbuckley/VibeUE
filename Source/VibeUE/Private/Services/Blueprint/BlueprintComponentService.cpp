#include "Services/Blueprint/BlueprintComponentService.h"
#include "Core/ErrorCodes.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#endif

DEFINE_LOG_CATEGORY(LogBlueprintComponentService);

FBlueprintComponentService::FBlueprintComponentService()
{
}

FBlueprintComponentService::~FBlueprintComponentService()
{
}

TResult<UActorComponent*> FBlueprintComponentService::AddComponent(
    UBlueprint* Blueprint, 
    const FString& ComponentType, 
    const FString& ComponentName,
    const FString& ParentName,
    const FTransform& RelativeTransform)
{
    if (!Blueprint)
    {
        return TResult<UActorComponent*>::Error(
            VibeUEErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null"));
    }

    // Validate component type
    UClass* ComponentClass = nullptr;
    if (!ValidateComponentType(ComponentType, ComponentClass))
    {
        return TResult<UActorComponent*>::Error(
            VibeUEErrorCodes::COMPONENT_TYPE_INVALID,
            FString::Printf(TEXT("Invalid component type: %s"), *ComponentType));
    }

    // Validate component name is unique
    if (!ValidateComponentName(Blueprint, ComponentName))
    {
        return TResult<UActorComponent*>::Error(
            VibeUEErrorCodes::COMPONENT_NAME_EXISTS,
            FString::Printf(TEXT("Component name '%s' already exists in Blueprint"), *ComponentName));
    }

    // Get Simple Construction Script
    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        return TResult<UActorComponent*>::Error(
            VibeUEErrorCodes::SCS_NOT_AVAILABLE,
            TEXT("Blueprint does not have a Simple Construction Script"));
    }

    // Create a new SCS node for the component
    USCS_Node* NewNode = SCS->CreateNode(ComponentClass, *ComponentName);
    if (!NewNode)
    {
        return TResult<UActorComponent*>::Error(
            VibeUEErrorCodes::COMPONENT_CREATE_FAILED,
            FString::Printf(TEXT("Failed to create component node for '%s'"), *ComponentName));
    }

    // Handle parent attachment if specified
    if (!ParentName.IsEmpty())
    {
        USCS_Node* ParentNode = SCS->FindSCSNode(*ParentName);
        if (ParentNode)
        {
            ParentNode->AddChildNode(NewNode);
        }
        else
        {
            UE_LOG(LogBlueprintComponentService, Warning, TEXT("Parent component '%s' not found, adding to root"), *ParentName);
            SCS->AddNode(NewNode);
        }
    }
    else
    {
        SCS->AddNode(NewNode);
    }

    // Apply transform if this is a scene component
    if (ComponentClass->IsChildOf<USceneComponent>())
    {
        USceneComponent* SceneComponentTemplate = Cast<USceneComponent>(NewNode->ComponentTemplate);
        if (SceneComponentTemplate)
        {
            SceneComponentTemplate->SetRelativeTransform(RelativeTransform);
        }
    }

#if WITH_EDITOR
    // Mark Blueprint as modified and recompile
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
#endif

    UE_LOG(LogBlueprintComponentService, Log, TEXT("Added component '%s' of type '%s' to Blueprint '%s'"), 
        *ComponentName, *ComponentType, *Blueprint->GetName());

    return TResult<UActorComponent*>::Success(NewNode->ComponentTemplate);
}

TResult<void> FBlueprintComponentService::RemoveComponent(
    UBlueprint* Blueprint, 
    const FString& ComponentName,
    bool bRemoveChildren)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(
            VibeUEErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null"));
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        return TResult<void>::Error(
            VibeUEErrorCodes::SCS_NOT_AVAILABLE,
            TEXT("Blueprint does not have a Simple Construction Script"));
    }

    USCS_Node* ComponentNode = SCS->FindSCSNode(*ComponentName);
    if (!ComponentNode)
    {
        return TResult<void>::Error(
            VibeUEErrorCodes::COMPONENT_NOT_FOUND,
            FString::Printf(TEXT("Component '%s' not found in Blueprint"), *ComponentName));
    }

    // Handle children based on remove_children parameter
    TArray<USCS_Node*> ChildNodes = ComponentNode->GetChildNodes();
    if (!bRemoveChildren && ChildNodes.Num() > 0)
    {
        // Reparent children to root
        for (USCS_Node* ChildNode : ChildNodes)
        {
            ComponentNode->RemoveChildNode(ChildNode);
            SCS->AddNode(ChildNode);
        }
    }

    // Remove the component node
    SCS->RemoveNode(ComponentNode);

#if WITH_EDITOR
    // Mark Blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
#endif

    UE_LOG(LogBlueprintComponentService, Log, TEXT("Removed component '%s' from Blueprint '%s'"), 
        *ComponentName, *Blueprint->GetName());

    return TResult<void>::Success();
}

TResult<TArray<FComponentInfo>> FBlueprintComponentService::ListComponents(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return TResult<TArray<FComponentInfo>>::Error(
            VibeUEErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null"));
    }

    TArray<FComponentInfo> Components;

    // Get components from Simple Construction Script
    if (Blueprint->SimpleConstructionScript)
    {
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        
        // Process root components and their children
        const TArray<USCS_Node*>& RootNodes = SCS->GetRootNodes();
        CollectComponentInfo(RootNodes, Components);
    }

    return TResult<TArray<FComponentInfo>>::Success(Components);
}

TResult<void> FBlueprintComponentService::ReorderComponents(
    UBlueprint* Blueprint, 
    const TArray<FString>& ComponentOrder)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(
            VibeUEErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null"));
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (!SCS)
    {
        return TResult<void>::Error(
            VibeUEErrorCodes::SCS_NOT_AVAILABLE,
            TEXT("Blueprint does not have a Simple Construction Script"));
    }

    // Component reordering is not fully implemented yet
    UE_LOG(LogBlueprintComponentService, Warning, TEXT("Component reordering not fully implemented yet"));

#if WITH_EDITOR
    // Mark Blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
#endif

    return TResult<void>::Success();
}

TResult<void> FBlueprintComponentService::ReparentComponent(
    UBlueprint* Blueprint, 
    const FString& ComponentName, 
    const FString& NewParentName)
{
#if WITH_EDITOR
    if (!Blueprint)
    {
        return TResult<void>::Error(
            VibeUEErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null"));
    }

    // Find the component to reparent
    USCS_Node* ChildNode = nullptr;
    if (Blueprint->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
        for (USCS_Node* Node : AllNodes)
        {
            if (Node && Node->GetVariableName() == *ComponentName)
            {
                ChildNode = Node;
                break;
            }
        }
    }

    if (!ChildNode)
    {
        return TResult<void>::Error(
            VibeUEErrorCodes::COMPONENT_NOT_FOUND,
            FString::Printf(TEXT("Component '%s' not found in Blueprint"), *ComponentName));
    }

    // Find the new parent component
    USCS_Node* NewParentNode = nullptr;
    if (Blueprint->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
        for (USCS_Node* Node : AllNodes)
        {
            if (Node && Node->GetVariableName() == *NewParentName)
            {
                NewParentNode = Node;
                break;
            }
        }
    }

    if (!NewParentNode)
    {
        return TResult<void>::Error(
            VibeUEErrorCodes::PARENT_COMPONENT_NOT_FOUND,
            FString::Printf(TEXT("Parent component '%s' not found in Blueprint"), *NewParentName));
    }

    // Validate that new parent is a SceneComponent
    if (!NewParentNode->ComponentTemplate || !NewParentNode->ComponentTemplate->IsA<USceneComponent>())
    {
        return TResult<void>::Error(
            VibeUEErrorCodes::PARENT_NOT_SCENE_COMPONENT,
            FString::Printf(TEXT("Parent component '%s' is not a SceneComponent"), *NewParentName));
    }

    // Reparent using UE's built-in SetParent method
    ChildNode->SetParent(NewParentNode);

    // Mark Blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    UE_LOG(LogBlueprintComponentService, Log, TEXT("Reparented component '%s' to '%s' in Blueprint '%s'"), 
        *ComponentName, *NewParentName, *Blueprint->GetName());

    return TResult<void>::Success();
#else
    return TResult<void>::Error(
        VibeUEErrorCodes::NOT_IMPLEMENTED,
        TEXT("Reparent component only available in Editor builds"));
#endif
}

bool FBlueprintComponentService::ValidateComponentType(const FString& ComponentTypeName, UClass*& OutComponentClass)
{
    OutComponentClass = nullptr;

    // Try to find the class by name
    for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
    {
        UClass* Class = *ClassIterator;
        if (Class->IsChildOf<UActorComponent>() && 
            (Class->GetName() == ComponentTypeName || 
             Class->GetDisplayNameText().ToString() == ComponentTypeName))
        {
            OutComponentClass = Class;
            return true;
        }
    }

    return false;
}

bool FBlueprintComponentService::ValidateComponentName(UBlueprint* Blueprint, const FString& ComponentName)
{
    // Returns true if the name is valid (unique)
    // Returns false if Blueprint is null, SCS is null, or name already exists
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
        return false;

    // Name is valid if it doesn't already exist
    return Blueprint->SimpleConstructionScript->FindSCSNode(*ComponentName) == nullptr;
}

void FBlueprintComponentService::CollectComponentInfo(const TArray<USCS_Node*>& Nodes, TArray<FComponentInfo>& OutComponents)
{
    for (USCS_Node* Node : Nodes)
    {
        if (Node)
        {
            ProcessComponentNode(Node, OutComponents);
        }
    }
}

void FBlueprintComponentService::ProcessComponentNode(USCS_Node* Node, TArray<FComponentInfo>& OutComponents)
{
    if (!Node)
        return;

    FComponentInfo Info;
    Info.ComponentName = Node->GetVariableName().ToString();
    Info.ComponentType = Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("Unknown");
    Info.ParentName = Node->ParentComponentOrVariableName.ToString();
    Info.bIsSceneComponent = Node->ComponentClass && Node->ComponentClass->IsChildOf<USceneComponent>();

    // Get transform if it's a scene component
    if (Info.bIsSceneComponent && Node->ComponentTemplate)
    {
        USceneComponent* SceneComp = Cast<USceneComponent>(Node->ComponentTemplate);
        if (SceneComp)
        {
            Info.RelativeTransform = SceneComp->GetRelativeTransform();
        }
    }

    // Collect child names
    const TArray<USCS_Node*>& ChildNodes = Node->GetChildNodes();
    for (USCS_Node* ChildNode : ChildNodes)
    {
        if (ChildNode)
        {
            Info.ChildNames.Add(ChildNode->GetVariableName().ToString());
        }
    }

    OutComponents.Add(Info);

    // Process children recursively
    CollectComponentInfo(ChildNodes, OutComponents);
}
