#include "Commands/BlueprintComponentReflection.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"

// Phase 4: Include Blueprint Services
#include "Services/Blueprint/BlueprintDiscoveryService.h"
#include "Services/Blueprint/BlueprintComponentService.h"
#include "Services/Blueprint/BlueprintReflectionService.h"
#include "Services/Blueprint/BlueprintPropertyService.h"
#include "Core/ServiceContext.h"
#include "Core/ErrorCodes.h"

// Editor-only utilities and specific component headers guarded to avoid build issues in non-editor contexts
#if WITH_EDITOR
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

FBlueprintComponentReflection::FBlueprintComponentReflection()
{
    // Phase 4: Initialize Blueprint Services
    TSharedPtr<FServiceContext> ServiceContext = MakeShared<FServiceContext>();
    
    DiscoveryService = MakeShared<FBlueprintDiscoveryService>(ServiceContext);
    ComponentService = MakeShared<FBlueprintComponentService>(ServiceContext);
    ReflectionService = MakeShared<FBlueprintReflectionService>(ServiceContext);
    PropertyService = MakeShared<FBlueprintPropertyService>(ServiceContext);
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Log, TEXT("Blueprint Component Reflection: Processing command %s"), *CommandType);

    if (CommandType == TEXT("get_available_components"))
    {
        return HandleGetAvailableComponents(Params);
    }
    else if (CommandType == TEXT("get_component_info"))
    {
        return HandleGetComponentInfo(Params);
    }
    else if (CommandType == TEXT("get_property_metadata"))
    {
        return HandleGetPropertyMetadata(Params);
    }
    else if (CommandType == TEXT("get_component_hierarchy"))
    {
        return HandleGetComponentHierarchy(Params);
    }
    else if (CommandType == TEXT("add_component"))
    {
        return HandleAddComponent(Params);
    }
    else if (CommandType == TEXT("set_component_property"))
    {
        return HandleSetComponentProperty(Params);
    }
    else if (CommandType == TEXT("remove_component"))
    {
        return HandleRemoveComponent(Params);
    }
    else if (CommandType == TEXT("reorder_components"))
    {
        return HandleReorderComponents(Params);
    }
    else if (CommandType == TEXT("get_component_property"))
    {
        return HandleGetComponentProperty(Params);
    }
    else if (CommandType == TEXT("get_all_component_properties"))
    {
        return HandleGetAllComponentProperties(Params);
    }
    else if (CommandType == TEXT("compare_component_properties"))
    {
        return HandleCompareComponentProperties(Params);
    }
    else if (CommandType == TEXT("reparent_component"))
    {
        return HandleReparentComponent(Params);
    }

    return CreateErrorResponse(VibeUE::ErrorCodes::UNKNOWN_COMMAND, 
        FString::Printf(TEXT("Unknown component reflection command: %s"), *CommandType));
}

// Discovery Methods Implementation

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleGetAvailableComponents(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Log, TEXT("Blueprint Component Reflection: Getting available components"));
    
    // Get available component types from ReflectionService
    auto ComponentTypesResult = ReflectionService->GetAvailableComponentTypes();
    
    if (ComponentTypesResult.IsError())
    {
        return CreateErrorResponse(ComponentTypesResult.GetErrorCode(), ComponentTypesResult.GetErrorMessage());
    }

    const TArray<FString>& ComponentTypeNames = ComponentTypesResult.GetValue();
    
    // Check if we want detailed metadata (default: false for performance)
    bool bIncludeDetailedMetadata = false;
    if (Params.IsValid())
    {
        Params->TryGetBoolField(TEXT("detailed_metadata"), bIncludeDetailedMetadata);
    }

    // Build component information array
    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    TSet<FString> Categories;
    
    for (const FString& ComponentTypeName : ComponentTypeNames)
    {
        // Resolve component class
        auto ResolveResult = ReflectionService->ResolveClass(ComponentTypeName);
        if (ResolveResult.IsError())
        {
            continue; // Skip invalid classes
        }
        
        UClass* ComponentClass = ResolveResult.GetValue();
        if (!ComponentClass)
        {
            continue;
        }

        // Create basic component info
        TSharedPtr<FJsonObject> ComponentInfo = MakeShared<FJsonObject>();
        
        // Basic information
        ComponentInfo->SetStringField(TEXT("name"), ComponentClass->GetName());
        ComponentInfo->SetStringField(TEXT("display_name"), ComponentClass->GetDisplayNameText().ToString());
        
        auto PathResult = ReflectionService->GetClassPath(ComponentClass);
        if (PathResult.IsSuccess())
        {
            ComponentInfo->SetStringField(TEXT("class_path"), PathResult.GetValue());
        }
        
        // Component type flags
        ComponentInfo->SetBoolField(TEXT("is_scene_component"), ComponentClass->IsChildOf<USceneComponent>());
        ComponentInfo->SetBoolField(TEXT("is_primitive_component"), ComponentClass->IsChildOf<UPrimitiveComponent>());
        ComponentInfo->SetBoolField(TEXT("is_custom"), !ComponentClass->IsNative());
        ComponentInfo->SetBoolField(TEXT("is_abstract"), ComponentClass->HasAnyClassFlags(CLASS_Abstract));
        
        // Get class category from metadata
        FString Category = TEXT("Miscellaneous");
        if (const FString* CategoryMetadata = ComponentClass->FindMetaData(TEXT("Category")))
        {
            Category = *CategoryMetadata;
        }
        ComponentInfo->SetStringField(TEXT("category"), Category);
        Categories.Add(Category);
        
        // Hierarchy info
        if (ComponentClass->GetSuperClass())
        {
            ComponentInfo->SetStringField(TEXT("base_class"), ComponentClass->GetSuperClass()->GetName());
        }
        
        // Include detailed metadata if requested (uses ReflectionService)
        if (bIncludeDetailedMetadata)
        {
            auto ClassInfoResult = ReflectionService->GetClassInfo(ComponentClass);
            if (ClassInfoResult.IsSuccess())
            {
                const FClassInfo& ClassInfo = ClassInfoResult.GetValue();
                ComponentInfo->SetStringField(TEXT("parent_class"), ClassInfo.ParentClass);
                ComponentInfo->SetBoolField(TEXT("is_blueprint"), ClassInfo.bIsBlueprint);
            }
            
            auto PropertiesResult = ReflectionService->GetClassProperties(ComponentClass);
            if (PropertiesResult.IsSuccess())
            {
                ComponentInfo->SetNumberField(TEXT("property_count"), PropertiesResult.GetValue().Num());
            }
            
            auto FunctionsResult = ReflectionService->GetClassFunctions(ComponentClass);
            if (FunctionsResult.IsSuccess())
            {
                ComponentInfo->SetNumberField(TEXT("function_count"), FunctionsResult.GetValue().Num());
            }
        }

        ComponentsArray.Add(MakeShared<FJsonValueObject>(ComponentInfo));
    }

    // Build success response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetArrayField(TEXT("components"), ComponentsArray);
    Response->SetNumberField(TEXT("total_count"), ComponentsArray.Num());
    
    // Add categories list
    TArray<TSharedPtr<FJsonValue>> CategoriesArray;
    for (const FString& Category : Categories)
    {
        CategoriesArray.Add(MakeShared<FJsonValueString>(Category));
    }
    Response->SetArrayField(TEXT("categories"), CategoriesArray);

    UE_LOG(LogTemp, Log, TEXT("Found %d component types in %d categories"), 
        ComponentsArray.Num(), Categories.Num());
        
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleGetComponentInfo(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString ComponentTypeName;
    if (!Params->TryGetStringField(TEXT("component_type"), ComponentTypeName))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing 'component_type' parameter"));
    }

    // Validate component type using ReflectionService
    auto ValidResult = ReflectionService->IsValidComponentType(ComponentTypeName);
    if (ValidResult.IsError() || !ValidResult.GetValue())
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::COMPONENT_TYPE_INVALID, 
            FString::Printf(TEXT("Component type '%s' not found or invalid"), *ComponentTypeName));
    }

    // Resolve component class
    auto ResolveResult = ReflectionService->ResolveClass(ComponentTypeName);
    if (ResolveResult.IsError())
    {
        return CreateErrorResponse(ResolveResult.GetErrorCode(), ResolveResult.GetErrorMessage());
    }

    UClass* ComponentClass = ResolveResult.GetValue();
    
    // Get class information from ReflectionService
    auto ClassInfoResult = ReflectionService->GetClassInfo(ComponentClass);
    if (ClassInfoResult.IsError())
    {
        return CreateErrorResponse(ClassInfoResult.GetErrorCode(), ClassInfoResult.GetErrorMessage());
    }

    const FClassInfo& ClassInfo = ClassInfoResult.GetValue();
    
    // Build component info response
    TSharedPtr<FJsonObject> ComponentInfo = MakeShared<FJsonObject>();
    ComponentInfo->SetStringField(TEXT("name"), ClassInfo.ClassName);
    ComponentInfo->SetStringField(TEXT("class_path"), ClassInfo.ClassPath);
    ComponentInfo->SetStringField(TEXT("parent_class"), ClassInfo.ParentClass);
    ComponentInfo->SetBoolField(TEXT("is_abstract"), ClassInfo.bIsAbstract);
    ComponentInfo->SetBoolField(TEXT("is_blueprint"), ClassInfo.bIsBlueprint);
    ComponentInfo->SetBoolField(TEXT("is_scene_component"), ComponentClass->IsChildOf<USceneComponent>());
    ComponentInfo->SetBoolField(TEXT("is_primitive_component"), ComponentClass->IsChildOf<UPrimitiveComponent>());
    
    // Get properties
    auto PropertiesResult = ReflectionService->GetClassProperties(ComponentClass);
    if (PropertiesResult.IsSuccess())
    {
        const TArray<FPropertyInfo>& Properties = PropertiesResult.GetValue();
        TArray<TSharedPtr<FJsonValue>> PropertiesArray;
        
        for (const FPropertyInfo& PropInfo : Properties)
        {
            TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
            PropObj->SetStringField(TEXT("name"), PropInfo.PropertyName);
            PropObj->SetStringField(TEXT("type"), PropInfo.PropertyType);
            PropObj->SetBoolField(TEXT("is_editable"), PropInfo.bIsEditable);
            PropObj->SetBoolField(TEXT("is_blueprint_visible"), PropInfo.bIsBlueprintVisible);
            PropertiesArray.Add(MakeShared<FJsonValueObject>(PropObj));
        }
        
        ComponentInfo->SetArrayField(TEXT("properties"), PropertiesArray);
        ComponentInfo->SetNumberField(TEXT("property_count"), Properties.Num());
    }
    
    // Get function count (don't try to extract detailed function info due to struct uncertainties)
    auto FunctionsResult = ReflectionService->GetClassFunctions(ComponentClass);
    if (FunctionsResult.IsSuccess())
    {
        ComponentInfo->SetNumberField(TEXT("function_count"), FunctionsResult.GetValue().Num());
    }

    // Build success response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("component_info"), ComponentInfo);

    UE_LOG(LogTemp, Log, TEXT("Retrieved component info for type: %s"), *ComponentTypeName);

    return Response;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleGetPropertyMetadata(const TSharedPtr<FJsonObject>& Params)
{
    FString ComponentTypeName;
    FString PropertyName;
    
    if (!Params->TryGetStringField(TEXT("component_type"), ComponentTypeName))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing 'component_type' parameter"));
    }
    
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing 'property_name' parameter"));
    }

    UClass* ComponentClass = nullptr;
    if (!ValidateComponentType(ComponentTypeName, ComponentClass))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::COMPONENT_TYPE_INVALID, 
            FString::Printf(TEXT("Component type '%s' not found"), *ComponentTypeName));
    }

    // Find the property
    const FProperty* Property = ComponentClass->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
            FString::Printf(TEXT("Property '%s' not found in component '%s'"), *PropertyName, *ComponentTypeName));
    }

    TSharedPtr<FJsonObject> PropertyInfo = ConvertPropertyToJson(Property);
    if (!PropertyInfo.IsValid())
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PROPERTY_GET_FAILED, 
            FString::Printf(TEXT("Failed to extract metadata for property '%s'"), *PropertyName));
    }

    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("property_metadata"), PropertyInfo);

    UE_LOG(LogTemp, Log, TEXT("Retrieved metadata for property '%s' on component type '%s'"), 
        *PropertyName, *ComponentTypeName);

    return Response;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleGetComponentHierarchy(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return CreateErrorResponse(FindResult.GetErrorCode(), FindResult.GetErrorMessage());
    }

    // Get component list from ComponentService
    auto ListResult = ComponentService->ListComponents(FindResult.GetValue());
    if (ListResult.IsError())
    {
        return CreateErrorResponse(ListResult.GetErrorCode(), ListResult.GetErrorMessage());
    }

    // Build hierarchy response from service result
    const TArray<FComponentInfo>& Components = ListResult.GetValue();
    
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    
    // Build components array
    TArray<TSharedPtr<FJsonValue>> ComponentsArray;
    for (const FComponentInfo& CompInfo : Components)
    {
        TSharedPtr<FJsonObject> ComponentObj = MakeShared<FJsonObject>();
        ComponentObj->SetStringField(TEXT("name"), CompInfo.ComponentName);
        ComponentObj->SetStringField(TEXT("type"), CompInfo.ComponentType);
        ComponentObj->SetStringField(TEXT("parent"), CompInfo.ParentName);
        ComponentObj->SetBoolField(TEXT("is_scene_component"), CompInfo.bIsSceneComponent);
        
        // Add children array
        TArray<TSharedPtr<FJsonValue>> ChildrenArray;
        for (const FString& ChildName : CompInfo.ChildNames)
        {
            ChildrenArray.Add(MakeShared<FJsonValueString>(ChildName));
        }
        ComponentObj->SetArrayField(TEXT("children"), ChildrenArray);
        
        // Add transform if scene component
        if (CompInfo.bIsSceneComponent)
        {
            TSharedPtr<FJsonObject> TransformObj = MakeShared<FJsonObject>();
            
            TArray<TSharedPtr<FJsonValue>> Location;
            Location.Add(MakeShared<FJsonValueNumber>(CompInfo.RelativeTransform.GetLocation().X));
            Location.Add(MakeShared<FJsonValueNumber>(CompInfo.RelativeTransform.GetLocation().Y));
            Location.Add(MakeShared<FJsonValueNumber>(CompInfo.RelativeTransform.GetLocation().Z));
            TransformObj->SetArrayField(TEXT("location"), Location);
            
            TArray<TSharedPtr<FJsonValue>> Rotation;
            Rotation.Add(MakeShared<FJsonValueNumber>(CompInfo.RelativeTransform.GetRotation().Rotator().Pitch));
            Rotation.Add(MakeShared<FJsonValueNumber>(CompInfo.RelativeTransform.GetRotation().Rotator().Yaw));
            Rotation.Add(MakeShared<FJsonValueNumber>(CompInfo.RelativeTransform.GetRotation().Rotator().Roll));
            TransformObj->SetArrayField(TEXT("rotation"), Rotation);
            
            TArray<TSharedPtr<FJsonValue>> Scale;
            Scale.Add(MakeShared<FJsonValueNumber>(CompInfo.RelativeTransform.GetScale3D().X));
            Scale.Add(MakeShared<FJsonValueNumber>(CompInfo.RelativeTransform.GetScale3D().Y));
            Scale.Add(MakeShared<FJsonValueNumber>(CompInfo.RelativeTransform.GetScale3D().Z));
            TransformObj->SetArrayField(TEXT("scale"), Scale);
            
            ComponentObj->SetObjectField(TEXT("relative_transform"), TransformObj);
        }
        
        ComponentsArray.Add(MakeShared<FJsonValueObject>(ComponentObj));
    }
    
    TSharedPtr<FJsonObject> HierarchyObj = MakeShared<FJsonObject>();
    HierarchyObj->SetArrayField(TEXT("components"), ComponentsArray);
    HierarchyObj->SetNumberField(TEXT("total_components"), Components.Num());
    
    Response->SetObjectField(TEXT("hierarchy"), HierarchyObj);

    UE_LOG(LogTemp, Log, TEXT("Successfully retrieved component hierarchy for Blueprint: %s (%d components)"), 
        *BlueprintName, Components.Num());

    return Response;
}

// Manipulation Methods Implementation

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleAddComponent(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString BlueprintName, ComponentType, ComponentName;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
        !Params->TryGetStringField(TEXT("component_type"), ComponentType) ||
        !Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, 
            TEXT("Missing required parameters: blueprint_name, component_type, component_name"));
    }

    // Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return CreateErrorResponse(FindResult.GetErrorCode(), FindResult.GetErrorMessage());
    }

    // Parse parent name and transform
    FString ParentName;
    Params->TryGetStringField(TEXT("parent_name"), ParentName);
    
    // Parse transform for scene components
    FTransform RelativeTransform = FTransform::Identity;
    bool bTransformProvided = false;
    
    const TArray<TSharedPtr<FJsonValue>>* LocationArray;
    if (Params->TryGetArrayField(TEXT("location"), LocationArray) && LocationArray->Num() >= 3)
    {
        FVector Location(
            (*LocationArray)[0]->AsNumber(),
            (*LocationArray)[1]->AsNumber(),
            (*LocationArray)[2]->AsNumber()
        );
        RelativeTransform.SetLocation(Location);
        bTransformProvided = true;
    }

    const TArray<TSharedPtr<FJsonValue>>* RotationArray;
    if (Params->TryGetArrayField(TEXT("rotation"), RotationArray) && RotationArray->Num() >= 3)
    {
        FRotator Rotation(
            (*RotationArray)[0]->AsNumber(),
            (*RotationArray)[1]->AsNumber(),
            (*RotationArray)[2]->AsNumber()
        );
        RelativeTransform.SetRotation(Rotation.Quaternion());
        bTransformProvided = true;
    }

    const TArray<TSharedPtr<FJsonValue>>* ScaleArray;
    if (Params->TryGetArrayField(TEXT("scale"), ScaleArray) && ScaleArray->Num() >= 3)
    {
        FVector Scale(
            (*ScaleArray)[0]->AsNumber(),
            (*ScaleArray)[1]->AsNumber(),
            (*ScaleArray)[2]->AsNumber()
        );
        RelativeTransform.SetScale3D(Scale);
        bTransformProvided = true;
    }

    // Add component using ComponentService
    auto AddResult = ComponentService->AddComponent(
        FindResult.GetValue(),
        ComponentType,
        ComponentName,
        ParentName,
        bTransformProvided ? RelativeTransform : FTransform::Identity
    );

    if (AddResult.IsError())
    {
        return CreateErrorResponse(AddResult.GetErrorCode(), AddResult.GetErrorMessage());
    }

    // Apply initial properties if provided
    const TSharedPtr<FJsonObject>* PropertiesObj;
    if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj) && PropertiesObj->IsValid())
    {
        UActorComponent* Component = AddResult.GetValue();
        if (Component)
        {
            for (const auto& PropertyPair : (*PropertiesObj)->Values)
            {
                const FString& PropertyName = PropertyPair.Key;
                const TSharedPtr<FJsonValue>& PropertyValue = PropertyPair.Value;
                
                // Use PropertyService to set property
                auto SetResult = PropertyService->SetBlueprintProperty(
                    FindResult.GetValue(),
                    PropertyName,
                    PropertyValue->AsString()
                );
                
                // Log warning if property set fails, but don't fail the whole operation
                if (SetResult.IsError())
                {
                    UE_LOG(LogTemp, Warning, TEXT("Failed to set property '%s' on component '%s': %s"), 
                        *PropertyName, *ComponentName, *SetResult.GetErrorMessage());
                }
            }
        }
    }

    // Build success response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Component '%s' added successfully"), *ComponentName));
    Response->SetStringField(TEXT("component_name"), ComponentName);
    Response->SetStringField(TEXT("component_type"), ComponentType);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);

    UE_LOG(LogTemp, Log, TEXT("Added component '%s' of type '%s' to Blueprint '%s'"), 
        *ComponentName, *ComponentType, *BlueprintName);
        
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString BlueprintName, ComponentName, PropertyName;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
        !Params->TryGetStringField(TEXT("component_name"), ComponentName) ||
        !Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, 
            TEXT("Missing required parameters: blueprint_name, component_name, property_name"));
    }

    TSharedPtr<FJsonValue> PropertyValue = Params->TryGetField(TEXT("property_value"));
    if (!PropertyValue.IsValid())
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing 'property_value' parameter"));
    }

    // Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return CreateErrorResponse(FindResult.GetErrorCode(), FindResult.GetErrorMessage());
    }
    
    UBlueprint* Blueprint = FindResult.GetValue();

    // Find the component - check both SCS nodes and inherited components
    UActorComponent* TargetComponent = nullptr;
    UClass* ComponentClass = nullptr;
    bool bFoundInCDO = false;
    
    // First try to find in Simple Construction Script
    if (Blueprint->SimpleConstructionScript)
    {
        USCS_Node* ComponentNode = Blueprint->SimpleConstructionScript->FindSCSNode(*ComponentName);
        if (ComponentNode && ComponentNode->ComponentTemplate)
        {
            TargetComponent = ComponentNode->ComponentTemplate;
            ComponentClass = TargetComponent->GetClass();
        }
    }
    
    // If not found in SCS, look for inherited components in the CDO
    if (!TargetComponent && Blueprint->GeneratedClass)
    {
        UE_LOG(LogTemp, Log, TEXT("Looking for component '%s' in CDO"), *ComponentName);
        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            // Look for component by name in the CDO
            TInlineComponentArray<UActorComponent*> AllComponents;
            CDO->GetComponents(AllComponents);
            
            UE_LOG(LogTemp, Log, TEXT("CDO has %d components"), AllComponents.Num());
            
            for (UActorComponent* Component : AllComponents)
            {
                if (Component)
                {
                    UE_LOG(LogTemp, Log, TEXT("Checking CDO component: %s"), *Component->GetName());
                    if (Component->GetName() == ComponentName)
                    {
                        UE_LOG(LogTemp, Log, TEXT("Found matching component: %s"), *ComponentName);
                        TargetComponent = Component;
                        ComponentClass = Component->GetClass();
                        bFoundInCDO = true;
                        break;
                    }
                }
            }
        }
    }
    
    if (!TargetComponent)
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::COMPONENT_NOT_FOUND, 
            FString::Printf(TEXT("Component '%s' not found in Blueprint"), *ComponentName));
    }

    // Find the property on the component
    const FProperty* Property = ComponentClass->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, 
            FString::Printf(TEXT("Property '%s' not found on component '%s'"), *PropertyName, *ComponentName));
    }

    // Set the property value
    void* PropertyPtr = Property->ContainerPtrToValuePtr<void>(TargetComponent);
    if (!SetPropertyFromJson(Property, PropertyPtr, PropertyValue))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PROPERTY_SET_FAILED, 
            FString::Printf(TEXT("Failed to set property '%s' on component '%s'"), *PropertyName, *ComponentName));
    }

    // Critical: Trigger editor viewport refresh for property changes (especially SkeletalMesh changes)
    #if WITH_EDITOR
    // Create a property changed event to trigger proper editor refresh
    FPropertyChangedEvent PropertyChangedEvent(const_cast<FProperty*>(Property), EPropertyChangeType::ValueSet);
    TargetComponent->PostEditChangeProperty(PropertyChangedEvent);
    
    // Enhanced handling for SkeletalMeshComponent to trigger comprehensive viewport refresh
    if (USkeletalMeshComponent* SkelMeshComp = Cast<USkeletalMeshComponent>(TargetComponent))
    {
        // Always broadcast skeletal mesh property changed for any skeletal mesh property
        if (SkelMeshComp->OnSkeletalMeshPropertyChanged.IsBound())
        {
            SkelMeshComp->OnSkeletalMeshPropertyChanged.Broadcast();
        }
        
        // Force render state refresh for skeletal mesh changes
        SkelMeshComp->MarkRenderStateDirty();
        
        // For skeletal mesh asset changes specifically, recreate render state
        if (PropertyName == TEXT("SkeletalMesh") || PropertyName == TEXT("SkeletalMeshAsset"))
        {
            // Force recreation of render state for immediate viewport update
            SkelMeshComp->RecreateRenderState_Concurrent();

            // Additional safety: compile blueprint to rebuild preview actor (ensures inherited component template changes propagate)
            if (Blueprint)
            {
                UE_LOG(LogTemp, Log, TEXT("Compiling Blueprint %s to force preview rebuild after skeletal mesh change"), *Blueprint->GetName());
                FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipGarbageCollection);
            }
        }
    }
    
    // For any component, mark render state dirty to ensure visual updates
    if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(TargetComponent))
    {
        PrimComp->MarkRenderStateDirty();
    }
    #endif

    // Mark Blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

#if WITH_EDITOR
    // If we updated an inherited (CDO) component, mark structural change so preview rebuilds from class defaults
    if (bFoundInCDO)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    }

    // Nudge the editor UI/viewport to refresh immediately
    if (GEditor)
    {
        GEditor->NoteSelectionChange();
        GEditor->RedrawAllViewports(false);
    }
#endif
    
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Property '%s' set successfully"), *PropertyName));
    Response->SetStringField(TEXT("component_name"), ComponentName);
    Response->SetStringField(TEXT("property_name"), PropertyName);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);

    UE_LOG(LogTemp, Log, TEXT("Set property '%s' on component '%s' in Blueprint '%s'"), 
        *PropertyName, *ComponentName, *BlueprintName);

    return Response;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleRemoveComponent(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString BlueprintName, ComponentName;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
        !Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, 
            TEXT("Missing required parameters: blueprint_name, component_name"));
    }

    // Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return CreateErrorResponse(FindResult.GetErrorCode(), FindResult.GetErrorMessage());
    }

    // Handle children based on remove_children parameter
    bool bRemoveChildren = true;
    Params->TryGetBoolField(TEXT("remove_children"), bRemoveChildren);

    // Remove component using ComponentService
    auto RemoveResult = ComponentService->RemoveComponent(
        FindResult.GetValue(),
        ComponentName,
        bRemoveChildren
    );

    if (RemoveResult.IsError())
    {
        return CreateErrorResponse(RemoveResult.GetErrorCode(), RemoveResult.GetErrorMessage());
    }

    // Build success response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Component '%s' removed successfully"), *ComponentName));
    Response->SetStringField(TEXT("component_name"), ComponentName);
    Response->SetBoolField(TEXT("removed_children"), bRemoveChildren);

    UE_LOG(LogTemp, Log, TEXT("Removed component '%s' from Blueprint '%s'"), *ComponentName, *BlueprintName);

    return Response;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleReorderComponents(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing 'blueprint_name' parameter"));
    }

    const TArray<TSharedPtr<FJsonValue>>* ComponentOrderArray;
    if (!Params->TryGetArrayField(TEXT("component_order"), ComponentOrderArray))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing 'component_order' parameter"));
    }

    // Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return CreateErrorResponse(FindResult.GetErrorCode(), FindResult.GetErrorMessage());
    }

    // Convert JSON array to FString array
    TArray<FString> ComponentNames;
    for (const TSharedPtr<FJsonValue>& Value : *ComponentOrderArray)
    {
        ComponentNames.Add(Value->AsString());
    }

    // Reorder components using ComponentService
    auto ReorderResult = ComponentService->ReorderComponents(FindResult.GetValue(), ComponentNames);
    
    if (ReorderResult.IsError())
    {
        return CreateErrorResponse(ReorderResult.GetErrorCode(), ReorderResult.GetErrorMessage());
    }

    // Build success response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), TEXT("Component reordering completed"));
    Response->SetArrayField(TEXT("final_order"), *ComponentOrderArray);

    UE_LOG(LogTemp, Log, TEXT("Reordered components in Blueprint '%s'"), *BlueprintName);

    return Response;
}

// Core Reflection Engine Implementation

TArray<UClass*> FBlueprintComponentReflection::DiscoverComponentClasses(const TSharedPtr<FJsonObject>& Filters)
{
    TArray<UClass*> ComponentClasses;

    // Use reflection to find all ActorComponent classes
    for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
    {
        UClass* Class = *ClassIterator;
        
        // Only include classes that inherit from UActorComponent
        if (!Class->IsChildOf<UActorComponent>())
            continue;

        // Skip abstract, deprecated, and newer version classes
        if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
            continue;

        // Skip native engine classes that shouldn't be user-accessible
        if (Class->HasAnyClassFlags(CLASS_Hidden))
            continue;

        // Apply filters if provided
        if (Filters.IsValid())
        {
            FString CategoryFilter;
            if (Filters->TryGetStringField(TEXT("category"), CategoryFilter) && !CategoryFilter.IsEmpty())
            {
                FString ComponentCategory = GetComponentCategory(Class);
                if (!ComponentCategory.Equals(CategoryFilter, ESearchCase::IgnoreCase))
                    continue;
            }

            FString BaseClassFilter;
            if (Filters->TryGetStringField(TEXT("base_class"), BaseClassFilter) && !BaseClassFilter.IsEmpty())
            {
                // Find the base class to filter by
                UClass* BaseClass = FindObject<UClass>(nullptr, *BaseClassFilter);
                if (BaseClass && !Class->IsChildOf(BaseClass))
                    continue;
            }

            FString SearchText;
            if (Filters->TryGetStringField(TEXT("search_text"), SearchText) && !SearchText.IsEmpty())
            {
                if (!Class->GetName().Contains(SearchText))
                    continue;
            }

            bool bIncludeAbstract = false;
            Filters->TryGetBoolField(TEXT("include_abstract"), bIncludeAbstract);
            if (!bIncludeAbstract && Class->HasAnyClassFlags(CLASS_Abstract))
                continue;

            bool bIncludeDeprecated = false;
            Filters->TryGetBoolField(TEXT("include_deprecated"), bIncludeDeprecated);
            if (!bIncludeDeprecated && Class->HasAnyClassFlags(CLASS_Deprecated))
                continue;
        }

        ComponentClasses.Add(Class);
    }

    // Sort by name for consistent ordering
    ComponentClasses.Sort([](const UClass& A, const UClass& B) {
        return A.GetName() < B.GetName();
    });

    return ComponentClasses;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::ExtractComponentMetadata(UClass* ComponentClass)
{
    if (!ComponentClass)
        return nullptr;

    TSharedPtr<FJsonObject> Metadata = MakeShareable(new FJsonObject);
    
    // Basic information
    Metadata->SetStringField(TEXT("name"), ComponentClass->GetName());
    Metadata->SetStringField(TEXT("display_name"), GetFriendlyComponentName(ComponentClass));
    Metadata->SetStringField(TEXT("class_path"), ComponentClass->GetPathName());
    Metadata->SetStringField(TEXT("category"), GetComponentCategory(ComponentClass));
    Metadata->SetBoolField(TEXT("is_custom"), !ComponentClass->IsNative());
    Metadata->SetBoolField(TEXT("is_abstract"), ComponentClass->HasAnyClassFlags(CLASS_Abstract));
    Metadata->SetBoolField(TEXT("is_deprecated"), ComponentClass->HasAnyClassFlags(CLASS_Deprecated));

    // Hierarchy information
    if (ComponentClass->GetSuperClass())
    {
        Metadata->SetStringField(TEXT("parent_class"), ComponentClass->GetSuperClass()->GetName());
    }

    // Component-specific metadata
    Metadata->SetBoolField(TEXT("is_scene_component"), ComponentClass->IsChildOf<USceneComponent>());
    Metadata->SetBoolField(TEXT("is_primitive_component"), ComponentClass->IsChildOf<UPrimitiveComponent>());
    Metadata->SetBoolField(TEXT("is_mesh_component"), ComponentClass->GetName().Contains(TEXT("Mesh")));
    Metadata->SetBoolField(TEXT("is_light_component"), ComponentClass->GetName().Contains(TEXT("Light")));

    // Extract properties
    TArray<TSharedPtr<FJsonObject>> Properties = ExtractPropertyMetadata(ComponentClass);
    TArray<TSharedPtr<FJsonValue>> PropertiesArray;
    for (const TSharedPtr<FJsonObject>& Property : Properties)
    {
        PropertiesArray.Add(MakeShareable(new FJsonValueObject(Property)));
    }
    Metadata->SetArrayField(TEXT("properties"), PropertiesArray);

    // Extract methods
    TSharedPtr<FJsonObject> Methods = ExtractMethodMetadata(ComponentClass);
    if (Methods.IsValid())
    {
        Metadata->SetObjectField(TEXT("methods"), Methods);
    }

    // Usage examples
    TArray<FString> UsageExamples = GetComponentUsageExamples(ComponentClass);
    TArray<TSharedPtr<FJsonValue>> ExamplesArray;
    for (const FString& Example : UsageExamples)
    {
        ExamplesArray.Add(MakeShareable(new FJsonValueString(Example)));
    }
    Metadata->SetArrayField(TEXT("usage_examples"), ExamplesArray);

    // Compatibility information
    TArray<UClass*> CompatibleParents = GetCompatibleParents(ComponentClass);
    TArray<TSharedPtr<FJsonValue>> ParentsArray;
    for (UClass* ParentClass : CompatibleParents)
    {
        ParentsArray.Add(MakeShareable(new FJsonValueString(ParentClass->GetName())));
    }
    Metadata->SetArrayField(TEXT("compatible_parents"), ParentsArray);

    TArray<UClass*> CompatibleChildren = GetCompatibleChildren(ComponentClass);
    TArray<TSharedPtr<FJsonValue>> ChildrenArray;
    for (UClass* ChildClass : CompatibleChildren)
    {
        ChildrenArray.Add(MakeShareable(new FJsonValueString(ChildClass->GetName())));
    }
    Metadata->SetArrayField(TEXT("compatible_children"), ChildrenArray);

    return Metadata;
}

TArray<TSharedPtr<FJsonObject>> FBlueprintComponentReflection::ExtractPropertyMetadata(UClass* ComponentClass, bool bIncludeInherited)
{
    TArray<TSharedPtr<FJsonObject>> Properties;

    if (!ComponentClass)
        return Properties;

    // Iterate through all properties
    for (FProperty* Property = ComponentClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
    {
        // Skip inherited properties if not requested
        if (!bIncludeInherited && Property->GetOwnerClass() != ComponentClass)
            continue;

        TSharedPtr<FJsonObject> PropertyInfo = ConvertPropertyToJson(Property);
        if (PropertyInfo.IsValid())
        {
            Properties.Add(PropertyInfo);
        }
    }

    return Properties;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::ExtractMethodMetadata(UClass* ComponentClass)
{
    TSharedPtr<FJsonObject> Methods = MakeShareable(new FJsonObject);
    TArray<TSharedPtr<FJsonValue>> FunctionsArray;

    if (!ComponentClass)
        return Methods;

    // Iterate through all functions
    for (TFieldIterator<UFunction> FunctionIter(ComponentClass); FunctionIter; ++FunctionIter)
    {
        UFunction* Function = *FunctionIter;
        
        // Skip functions that shouldn't be exposed
        if (Function->HasAnyFunctionFlags(FUNC_Private | FUNC_Protected))
            continue;

        TSharedPtr<FJsonObject> FunctionInfo = MakeShareable(new FJsonObject);
        FunctionInfo->SetStringField(TEXT("name"), Function->GetName());
        FunctionInfo->SetStringField(TEXT("display_name"), Function->GetDisplayNameText().ToString());
        FunctionInfo->SetBoolField(TEXT("is_blueprint_callable"), Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
        FunctionInfo->SetBoolField(TEXT("is_blueprint_pure"), Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
        FunctionInfo->SetBoolField(TEXT("is_const"), Function->HasAnyFunctionFlags(FUNC_Const));

        // Extract parameters
        TArray<TSharedPtr<FJsonValue>> ParametersArray;
        for (TFieldIterator<FProperty> ParamIter(Function); ParamIter; ++ParamIter)
        {
            FProperty* Param = *ParamIter;
            TSharedPtr<FJsonObject> ParamInfo = ConvertPropertyToJson(Param);
            if (ParamInfo.IsValid())
            {
                ParamInfo->SetBoolField(TEXT("is_return_param"), Param->HasAnyPropertyFlags(CPF_ReturnParm));
                ParamInfo->SetBoolField(TEXT("is_out_param"), Param->HasAnyPropertyFlags(CPF_OutParm));
                ParametersArray.Add(MakeShareable(new FJsonValueObject(ParamInfo)));
            }
        }
        FunctionInfo->SetArrayField(TEXT("parameters"), ParametersArray);

        FunctionsArray.Add(MakeShareable(new FJsonValueObject(FunctionInfo)));
    }

    Methods->SetArrayField(TEXT("functions"), FunctionsArray);
    return Methods;
}

// Hierarchy Management Implementation

TSharedPtr<FJsonObject> FBlueprintComponentReflection::AnalyzeComponentHierarchy(UBlueprint* Blueprint)
{
    if (!Blueprint)
        return nullptr;

    TSharedPtr<FJsonObject> Hierarchy = MakeShareable(new FJsonObject);
    TArray<TSharedPtr<FJsonValue>> ComponentsArray;

    // First, add inherited components from the Blueprint's Generated Class
    if (Blueprint->GeneratedClass)
    {
        UE_LOG(LogTemp, Log, TEXT("Blueprint has GeneratedClass: %s"), *Blueprint->GeneratedClass->GetName());
        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            UE_LOG(LogTemp, Log, TEXT("CDO found: %s"), *CDO->GetName());
            // Get all inherited components from the CDO
            TInlineComponentArray<UActorComponent*> InheritedComponents;
            CDO->GetComponents(InheritedComponents);
            
            UE_LOG(LogTemp, Log, TEXT("Found %d components in CDO"), InheritedComponents.Num());
            
            for (UActorComponent* Component : InheritedComponents)
            {
                if (Component)
                {
                    UE_LOG(LogTemp, Log, TEXT("Found component: %s (Type: %s)"), *Component->GetName(), *Component->GetClass()->GetName());
                    
                    // Skip components that are added via SCS (we'll add those separately)
                    bool bIsFromSCS = false;
                    if (Blueprint->SimpleConstructionScript)
                    {
                        USCS_Node* SCSNode = Blueprint->SimpleConstructionScript->FindSCSNode(Component->GetFName());
                        if (SCSNode)
                        {
                            bIsFromSCS = true;
                            UE_LOG(LogTemp, Log, TEXT("Component %s is from SCS, skipping"), *Component->GetName());
                        }
                    }
                    
                    if (!bIsFromSCS)
                    {
                        UE_LOG(LogTemp, Log, TEXT("Adding inherited component: %s"), *Component->GetName());
                        TSharedPtr<FJsonObject> ComponentInfo = MakeShareable(new FJsonObject);
                        ComponentInfo->SetStringField(TEXT("name"), Component->GetName());
                        ComponentInfo->SetStringField(TEXT("type"), Component->GetClass()->GetName());
                        ComponentInfo->SetBoolField(TEXT("is_root"), false);
                        ComponentInfo->SetBoolField(TEXT("is_inherited"), true);
                        ComponentInfo->SetBoolField(TEXT("is_scene_component"), Component->IsA<USceneComponent>());
                        ComponentInfo->SetArrayField(TEXT("children"), TArray<TSharedPtr<FJsonValue>>());
                        
                        ComponentsArray.Add(MakeShareable(new FJsonValueObject(ComponentInfo)));
                    }
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("CDO cast failed"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Blueprint has no GeneratedClass"));
    }

    // Then, add components from Simple Construction Script
    if (Blueprint->SimpleConstructionScript)
    {
        USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
        
        // Process root components
        const TArray<USCS_Node*>& RootNodes = SCS->GetRootNodes();
        for (USCS_Node* RootNode : RootNodes)
        {
            TSharedPtr<FJsonObject> ComponentInfo = MakeShareable(new FJsonObject);
            ComponentInfo->SetStringField(TEXT("name"), RootNode->GetVariableName().ToString());
            ComponentInfo->SetStringField(TEXT("type"), RootNode->ComponentClass ? RootNode->ComponentClass->GetName() : TEXT("Unknown"));
            ComponentInfo->SetBoolField(TEXT("is_root"), true);
            ComponentInfo->SetBoolField(TEXT("is_inherited"), false);
            ComponentInfo->SetBoolField(TEXT("is_scene_component"), RootNode->ComponentClass && RootNode->ComponentClass->IsChildOf<USceneComponent>());

            // Add child components recursively
            TArray<TSharedPtr<FJsonValue>> ChildrenArray;
            ProcessChildComponents(RootNode, ChildrenArray);
            ComponentInfo->SetArrayField(TEXT("children"), ChildrenArray);

            ComponentsArray.Add(MakeShareable(new FJsonValueObject(ComponentInfo)));
        }
    }

    Hierarchy->SetBoolField(TEXT("success"), true);
    Hierarchy->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
    Hierarchy->SetArrayField(TEXT("components"), ComponentsArray);
    Hierarchy->SetNumberField(TEXT("total_components"), ComponentsArray.Num());

    return Hierarchy;
}

void FBlueprintComponentReflection::ProcessChildComponents(USCS_Node* ParentNode, TArray<TSharedPtr<FJsonValue>>& ChildrenArray)
{
    if (!ParentNode)
        return;

    const TArray<USCS_Node*>& ChildNodes = ParentNode->GetChildNodes();
    for (USCS_Node* ChildNode : ChildNodes)
    {
        TSharedPtr<FJsonObject> ChildInfo = MakeShareable(new FJsonObject);
        ChildInfo->SetStringField(TEXT("name"), ChildNode->GetVariableName().ToString());
        ChildInfo->SetStringField(TEXT("type"), ChildNode->ComponentClass ? ChildNode->ComponentClass->GetName() : TEXT("Unknown"));
        ChildInfo->SetBoolField(TEXT("is_scene_component"), ChildNode->ComponentClass && ChildNode->ComponentClass->IsChildOf<USceneComponent>());

        // Process grandchildren
        TArray<TSharedPtr<FJsonValue>> GrandChildrenArray;
        ProcessChildComponents(ChildNode, GrandChildrenArray);
        ChildInfo->SetArrayField(TEXT("children"), GrandChildrenArray);

        ChildrenArray.Add(MakeShareable(new FJsonValueObject(ChildInfo)));
    }
}

int32 FBlueprintComponentReflection::CountComponentsRecursive(const TArray<USCS_Node*>& Nodes)
{
    int32 Count = Nodes.Num();
    for (USCS_Node* Node : Nodes)
    {
        Count += CountComponentsRecursive(Node->GetChildNodes());
    }
    return Count;
}

bool FBlueprintComponentReflection::ValidateParentChildCompatibility(UClass* ParentClass, UClass* ChildClass)
{
    if (!ParentClass || !ChildClass)
        return false;

    // Both must be scene components for parent-child relationship
    if (!ParentClass->IsChildOf<USceneComponent>() || !ChildClass->IsChildOf<USceneComponent>())
        return false;

    // Additional compatibility checks can be added here
    return true;
}

TArray<UClass*> FBlueprintComponentReflection::GetCompatibleParents(UClass* ComponentClass)
{
    TArray<UClass*> CompatibleParents;

    if (!ComponentClass || !ComponentClass->IsChildOf<USceneComponent>())
        return CompatibleParents;

    // Find all scene component classes that can be parents
    for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
    {
        UClass* PotentialParent = *ClassIterator;
        
        if (PotentialParent->IsChildOf<USceneComponent>() &&
            !PotentialParent->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated) &&
            ValidateParentChildCompatibility(PotentialParent, ComponentClass))
        {
            CompatibleParents.Add(PotentialParent);
        }
    }

    return CompatibleParents;
}

TArray<UClass*> FBlueprintComponentReflection::GetCompatibleChildren(UClass* ComponentClass)
{
    TArray<UClass*> CompatibleChildren;

    if (!ComponentClass || !ComponentClass->IsChildOf<USceneComponent>())
        return CompatibleChildren;

    // Find all scene component classes that can be children
    for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
    {
        UClass* PotentialChild = *ClassIterator;
        
        if (PotentialChild->IsChildOf<USceneComponent>() &&
            !PotentialChild->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated) &&
            ValidateParentChildCompatibility(ComponentClass, PotentialChild))
        {
            CompatibleChildren.Add(PotentialChild);
        }
    }

    return CompatibleChildren;
}

// Reflection Utilities Implementation

TSharedPtr<FJsonObject> FBlueprintComponentReflection::ConvertPropertyToJson(const FProperty* Property, const void* PropertyValue)
{
    if (!Property)
        return nullptr;

    TSharedPtr<FJsonObject> PropertyInfo = MakeShareable(new FJsonObject);
    
    // Basic property information
    PropertyInfo->SetStringField(TEXT("name"), Property->GetName());
    PropertyInfo->SetStringField(TEXT("display_name"), Property->GetDisplayNameText().ToString());
    PropertyInfo->SetStringField(TEXT("cpp_type"), GetPropertyCPPType(Property));
    PropertyInfo->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
    PropertyInfo->SetStringField(TEXT("tooltip"), Property->GetToolTipText().ToString());

    // Property flags
    PropertyInfo->SetBoolField(TEXT("is_editable"), Property->HasAnyPropertyFlags(CPF_Edit));
    PropertyInfo->SetBoolField(TEXT("is_blueprint_visible"), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
    PropertyInfo->SetBoolField(TEXT("is_blueprint_readonly"), Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
    PropertyInfo->SetBoolField(TEXT("is_instance_editable"), Property->HasAnyPropertyFlags(CPF_InstancedReference));
    PropertyInfo->SetBoolField(TEXT("is_config"), Property->HasAnyPropertyFlags(CPF_Config));
    PropertyInfo->SetBoolField(TEXT("is_transient"), Property->HasAnyPropertyFlags(CPF_Transient));

    // Property constraints and metadata
    TSharedPtr<FJsonObject> Constraints = GetPropertyConstraints(Property);
    if (Constraints.IsValid())
    {
        PropertyInfo->SetObjectField(TEXT("constraints"), Constraints);
    }

    // Property value if provided
    if (PropertyValue)
    {
        // TODO: Convert property value to JSON representation
        PropertyInfo->SetStringField(TEXT("current_value"), TEXT("Value extraction not implemented"));
    }

    return PropertyInfo;
}

bool FBlueprintComponentReflection::SetPropertyFromJson(const FProperty* Property, void* PropertyValue, const TSharedPtr<FJsonValue>& JsonValue)
{
    if (!Property || !PropertyValue || !JsonValue.IsValid())
        return false;

    UE_LOG(LogTemp, Log, TEXT("Setting property %s of type %s"), *Property->GetName(), *Property->GetClass()->GetName());

    // Handle different property types
    if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        bool Value = JsonValue->AsBool();
        BoolProp->SetPropertyValue(PropertyValue, Value);
        UE_LOG(LogTemp, Log, TEXT("Set bool property to %s"), Value ? TEXT("true") : TEXT("false"));
        return true;
    }
    else if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        if (ByteProp->IsEnum())
        {
            // Handle enum by name or value
            FString EnumString = JsonValue->AsString();
            if (!EnumString.IsEmpty())
            {
                int32 EnumValue = ByteProp->Enum->GetValueByName(*EnumString);
                if (EnumValue != INDEX_NONE)
                {
                    ByteProp->SetPropertyValue(PropertyValue, static_cast<uint8>(EnumValue));
                    UE_LOG(LogTemp, Log, TEXT("Set enum property to %s (%d)"), *EnumString, EnumValue);
                    return true;
                }
            }
            // Try as numeric value
            uint8 Value = static_cast<uint8>(JsonValue->AsNumber());
            ByteProp->SetPropertyValue(PropertyValue, Value);
            return true;
        }
        else
        {
            uint8 Value = static_cast<uint8>(JsonValue->AsNumber());
            ByteProp->SetPropertyValue(PropertyValue, Value);
            return true;
        }
    }
    else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        // Handle enum by name or value
        FString EnumString = JsonValue->AsString();
        if (!EnumString.IsEmpty())
        {
            int64 EnumValue = EnumProp->GetEnum()->GetValueByName(*EnumString);
            if (EnumValue != INDEX_NONE)
            {
                EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(PropertyValue, EnumValue);
                UE_LOG(LogTemp, Log, TEXT("Set enum property to %s (%lld)"), *EnumString, EnumValue);
                return true;
            }
        }
        // Try as numeric value
        int64 Value = static_cast<int64>(JsonValue->AsNumber());
        EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(PropertyValue, Value);
        return true;
    }
    else if (const FIntProperty* IntProp = CastField<FIntProperty>(Property))
    {
        int32 Value = static_cast<int32>(JsonValue->AsNumber());
        IntProp->SetPropertyValue(PropertyValue, Value);
        UE_LOG(LogTemp, Log, TEXT("Set int property to %d"), Value);
        return true;
    }
    else if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
    {
        float Value = static_cast<float>(JsonValue->AsNumber());
        FloatProp->SetPropertyValue(PropertyValue, Value);
        UE_LOG(LogTemp, Log, TEXT("Set float property to %f"), Value);
        return true;
    }
    else if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Property))
    {
        double Value = JsonValue->AsNumber();
        DoubleProp->SetPropertyValue(PropertyValue, Value);
        return true;
    }
    else if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        FString Value = JsonValue->AsString();
        StrProp->SetPropertyValue(PropertyValue, Value);
        UE_LOG(LogTemp, Log, TEXT("Set string property to %s"), *Value);
        return true;
    }
    else if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        FName Value(*JsonValue->AsString());
        NameProp->SetPropertyValue(PropertyValue, Value);
        return true;
    }
    else if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        FText Value = FText::FromString(JsonValue->AsString());
        TextProp->SetPropertyValue(PropertyValue, Value);
        return true;
    }
    else if (const FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
    {
        // Handle object references (UObject*, UStaticMesh*, USkeletalMesh*, etc.)
        FString ObjectPath = JsonValue->AsString();
        UE_LOG(LogTemp, Log, TEXT("Setting object property to path: %s"), *ObjectPath);
        
        if (ObjectPath.IsEmpty() || ObjectPath == TEXT("None") || ObjectPath == TEXT("null"))
        {
            ObjectProp->SetObjectPropertyValue(PropertyValue, nullptr);
            UE_LOG(LogTemp, Log, TEXT("Set object property to null"));
            return true;
        }

        // Try to load the object
        UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath);
        if (!Object)
        {
            // Try with different path formats
            if (!ObjectPath.Contains(TEXT("'")))
            {
                // Try as soft object path
                FSoftObjectPath SoftPath(ObjectPath);
                Object = SoftPath.TryLoad();
            }
            
            if (!Object)
            {
                UE_LOG(LogTemp, Warning, TEXT("Failed to load object: %s"), *ObjectPath);
                return false;
            }
        }

        // Verify the object is compatible with the property type
        if (!Object->IsA(ObjectProp->PropertyClass))
        {
            UE_LOG(LogTemp, Warning, TEXT("Object %s is not compatible with property type %s"), 
                   *Object->GetClass()->GetName(), *ObjectProp->PropertyClass->GetName());
            return false;
        }

        ObjectProp->SetObjectPropertyValue(PropertyValue, Object);
        UE_LOG(LogTemp, Log, TEXT("Set object property to %s"), *Object->GetName());
        return true;
    }
    else if (const FSoftObjectProperty* SoftObjectProp = CastField<FSoftObjectProperty>(Property))
    {
        // Handle soft object references
        FString ObjectPath = JsonValue->AsString();
        FSoftObjectPtr SoftPtr;
        
        if (!ObjectPath.IsEmpty() && ObjectPath != TEXT("None") && ObjectPath != TEXT("null"))
        {
            SoftPtr = FSoftObjectPath(ObjectPath);
        }
        
        SoftObjectProp->SetPropertyValue(PropertyValue, SoftPtr);
        return true;
    }
    else if (const FWeakObjectProperty* WeakObjectProp = CastField<FWeakObjectProperty>(Property))
    {
        // Handle weak object references
        FString ObjectPath = JsonValue->AsString();
        FWeakObjectPtr WeakPtr;
        
        if (!ObjectPath.IsEmpty() && ObjectPath != TEXT("None") && ObjectPath != TEXT("null"))
        {
            UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath);
            if (Object)
            {
                WeakPtr = Object;
            }
        }
        
        WeakObjectProp->SetPropertyValue(PropertyValue, WeakPtr);
        return true;
    }
    else if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        UE_LOG(LogTemp, Log, TEXT("Handling struct property: %s"), *StructProp->Struct->GetName());
        
        // Handle common struct types
        if (StructProp->Struct->GetName() == TEXT("Vector"))
        {
            const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
            if (JsonValue->TryGetArray(ArrayValue) && ArrayValue->Num() >= 3)
            {
                FVector* VectorValue = static_cast<FVector*>(PropertyValue);
                VectorValue->X = (*ArrayValue)[0]->AsNumber();
                VectorValue->Y = (*ArrayValue)[1]->AsNumber();
                VectorValue->Z = (*ArrayValue)[2]->AsNumber();
                UE_LOG(LogTemp, Log, TEXT("Set Vector to (%f, %f, %f)"), VectorValue->X, VectorValue->Y, VectorValue->Z);
                return true;
            }
        }
        else if (StructProp->Struct->GetName() == TEXT("Rotator"))
        {
            const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
            if (JsonValue->TryGetArray(ArrayValue) && ArrayValue->Num() >= 3)
            {
                FRotator* RotatorValue = static_cast<FRotator*>(PropertyValue);
                RotatorValue->Pitch = (*ArrayValue)[0]->AsNumber();
                RotatorValue->Yaw = (*ArrayValue)[1]->AsNumber();
                RotatorValue->Roll = (*ArrayValue)[2]->AsNumber();
                UE_LOG(LogTemp, Log, TEXT("Set Rotator to (%f, %f, %f)"), RotatorValue->Pitch, RotatorValue->Yaw, RotatorValue->Roll);
                return true;
            }
        }
        else if (StructProp->Struct->GetName() == TEXT("LinearColor"))
        {
            const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
            if (JsonValue->TryGetArray(ArrayValue) && ArrayValue->Num() >= 4)
            {
                FLinearColor* ColorValue = static_cast<FLinearColor*>(PropertyValue);
                ColorValue->R = (*ArrayValue)[0]->AsNumber();
                ColorValue->G = (*ArrayValue)[1]->AsNumber();
                ColorValue->B = (*ArrayValue)[2]->AsNumber();
                ColorValue->A = (*ArrayValue)[3]->AsNumber();
                UE_LOG(LogTemp, Log, TEXT("Set LinearColor to (%f, %f, %f, %f)"), ColorValue->R, ColorValue->G, ColorValue->B, ColorValue->A);
                return true;
            }
        }
        else if (StructProp->Struct->GetName() == TEXT("Color"))
        {
            const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
            if (JsonValue->TryGetArray(ArrayValue) && ArrayValue->Num() >= 4)
            {
                FColor* ColorValue = static_cast<FColor*>(PropertyValue);
                ColorValue->R = static_cast<uint8>((*ArrayValue)[0]->AsNumber() * 255.0f);
                ColorValue->G = static_cast<uint8>((*ArrayValue)[1]->AsNumber() * 255.0f);
                ColorValue->B = static_cast<uint8>((*ArrayValue)[2]->AsNumber() * 255.0f);
                ColorValue->A = static_cast<uint8>((*ArrayValue)[3]->AsNumber() * 255.0f);
                UE_LOG(LogTemp, Log, TEXT("Set FColor to (%d, %d, %d, %d)"), ColorValue->R, ColorValue->G, ColorValue->B, ColorValue->A);
                return true;
            }
        }
        else if (StructProp->Struct->GetName() == TEXT("Transform"))
        {
            const TSharedPtr<FJsonObject>* ObjectValue;
            if (JsonValue->TryGetObject(ObjectValue))
            {
                FTransform* TransformValue = static_cast<FTransform*>(PropertyValue);
                
                // Handle Location
                const TArray<TSharedPtr<FJsonValue>>* LocationArray;
                if ((*ObjectValue)->TryGetArrayField(TEXT("Location"), LocationArray) && LocationArray->Num() >= 3)
                {
                    FVector Location;
                    Location.X = (*LocationArray)[0]->AsNumber();
                    Location.Y = (*LocationArray)[1]->AsNumber();
                    Location.Z = (*LocationArray)[2]->AsNumber();
                    TransformValue->SetLocation(Location);
                }
                
                // Handle Rotation
                const TArray<TSharedPtr<FJsonValue>>* RotationArray;
                if ((*ObjectValue)->TryGetArrayField(TEXT("Rotation"), RotationArray) && RotationArray->Num() >= 3)
                {
                    FRotator Rotation;
                    Rotation.Pitch = (*RotationArray)[0]->AsNumber();
                    Rotation.Yaw = (*RotationArray)[1]->AsNumber();
                    Rotation.Roll = (*RotationArray)[2]->AsNumber();
                    TransformValue->SetRotation(FQuat(Rotation));
                }
                
                // Handle Scale
                const TArray<TSharedPtr<FJsonValue>>* ScaleArray;
                if ((*ObjectValue)->TryGetArrayField(TEXT("Scale"), ScaleArray) && ScaleArray->Num() >= 3)
                {
                    FVector Scale;
                    Scale.X = (*ScaleArray)[0]->AsNumber();
                    Scale.Y = (*ScaleArray)[1]->AsNumber();
                    Scale.Z = (*ScaleArray)[2]->AsNumber();
                    TransformValue->SetScale3D(Scale);
                }
                
                return true;
            }
        }
        else
        {
            // Try to handle generic struct by JSON object
            const TSharedPtr<FJsonObject>* ObjectValue;
            if (JsonValue->TryGetObject(ObjectValue))
            {
                bool bSuccess = true;
                for (auto& Pair : (*ObjectValue)->Values)
                {
                    const FProperty* ChildProp = StructProp->Struct->FindPropertyByName(*Pair.Key);
                    if (ChildProp)
                    {
                        void* ChildPropertyValue = ChildProp->ContainerPtrToValuePtr<void>(PropertyValue);
                        if (!SetPropertyFromJson(ChildProp, ChildPropertyValue, Pair.Value))
                        {
                            bSuccess = false;
                        }
                    }
                }
                return bSuccess;
            }
        }
    }
    else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
    {
        // Handle arrays
        const TArray<TSharedPtr<FJsonValue>>* ArrayValue;
        if (JsonValue->TryGetArray(ArrayValue))
        {
            FScriptArrayHelper ArrayHelper(ArrayProp, PropertyValue);
            ArrayHelper.EmptyValues();
            
            for (int32 i = 0; i < ArrayValue->Num(); ++i)
            {
                int32 NewIndex = ArrayHelper.AddValue();
                void* ElementPtr = ArrayHelper.GetRawPtr(NewIndex);
                if (!SetPropertyFromJson(ArrayProp->Inner, ElementPtr, (*ArrayValue)[i]))
                {
                    UE_LOG(LogTemp, Warning, TEXT("Failed to set array element %d"), i);
                    return false;
                }
            }
            return true;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Property type %s not supported for JSON conversion"), *Property->GetClass()->GetName());
    return false;
}

FString FBlueprintComponentReflection::GetPropertyCPPType(const FProperty* Property)
{
    if (!Property)
        return TEXT("Unknown");

    // Use GetCPPType instead of ExportCppDeclaration for UE 5.6 compatibility
    return Property->GetCPPType();
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::GetPropertyConstraints(const FProperty* Property)
{
    if (!Property)
        return nullptr;

    TSharedPtr<FJsonObject> Constraints = MakeShareable(new FJsonObject);

    // Numeric constraints
    if (const FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
    {
        FString ClampMin = Property->GetMetaData(TEXT("ClampMin"));
        FString ClampMax = Property->GetMetaData(TEXT("ClampMax"));
        FString UIMin = Property->GetMetaData(TEXT("UIMin"));
        FString UIMax = Property->GetMetaData(TEXT("UIMax"));

        if (!ClampMin.IsEmpty())
            Constraints->SetNumberField(TEXT("clamp_min"), FCString::Atof(*ClampMin));
        if (!ClampMax.IsEmpty())
            Constraints->SetNumberField(TEXT("clamp_max"), FCString::Atof(*ClampMax));
        if (!UIMin.IsEmpty())
            Constraints->SetNumberField(TEXT("ui_min"), FCString::Atof(*UIMin));
        if (!UIMax.IsEmpty())
            Constraints->SetNumberField(TEXT("ui_max"), FCString::Atof(*UIMax));
    }

    // String constraints
    if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        FString MaxLength = Property->GetMetaData(TEXT("MaxLength"));
        if (!MaxLength.IsEmpty())
            Constraints->SetNumberField(TEXT("max_length"), FCString::Atoi(*MaxLength));
    }

    // Array constraints
    if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
    {
        FString ArraySizeMax = Property->GetMetaData(TEXT("ArraySizeMax"));
        if (!ArraySizeMax.IsEmpty())
            Constraints->SetNumberField(TEXT("max_elements"), FCString::Atoi(*ArraySizeMax));
    }

    return Constraints->Values.Num() > 0 ? Constraints : nullptr;
}

// Helper Functions Implementation

bool FBlueprintComponentReflection::ValidateComponentType(const FString& ComponentTypeName, UClass*& OutComponentClass)
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

bool FBlueprintComponentReflection::ValidateComponentName(UBlueprint* Blueprint, const FString& ComponentName)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
        return false;

    return Blueprint->SimpleConstructionScript->FindSCSNode(*ComponentName) == nullptr;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::ValidateHierarchyOperation(UBlueprint* Blueprint, const FString& ComponentName, const FString& ParentComponentName)
{
    TSharedPtr<FJsonObject> ValidationResult = MakeShareable(new FJsonObject);
    ValidationResult->SetBoolField(TEXT("valid"), true);

    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        ValidationResult->SetBoolField(TEXT("valid"), false);
        ValidationResult->SetStringField(TEXT("error"), TEXT("Invalid Blueprint or missing Simple Construction Script"));
        return ValidationResult;
    }

    // Check if parent exists
    if (!ParentComponentName.IsEmpty())
    {
        USCS_Node* ParentNode = Blueprint->SimpleConstructionScript->FindSCSNode(*ParentComponentName);
        if (!ParentNode)
        {
            ValidationResult->SetBoolField(TEXT("valid"), false);
            ValidationResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Parent component '%s' not found"), *ParentComponentName));
            return ValidationResult;
        }

        // Check if parent can have children (must be scene component)
        if (!ParentNode->ComponentClass->IsChildOf<USceneComponent>())
        {
            ValidationResult->SetBoolField(TEXT("valid"), false);
            ValidationResult->SetStringField(TEXT("error"), FString::Printf(TEXT("Parent component '%s' cannot have children (not a scene component)"), *ParentComponentName));
            return ValidationResult;
        }
    }

    return ValidationResult;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::CreateSuccessResponse(const FString& Message) const
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    
    if (!Message.IsEmpty())
    {
        Response->SetStringField(TEXT("message"), Message);
    }

    return Response;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::CreateErrorResponse(const FString& ErrorCode, const FString& ErrorMessage) const
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), ErrorMessage);
    Response->SetStringField(TEXT("error_code"), ErrorCode);
    
    UE_LOG(LogTemp, Error, TEXT("Blueprint Component Reflection Error [%s]: %s"), *ErrorCode, *ErrorMessage);
    return Response;
}

FString FBlueprintComponentReflection::GetFriendlyComponentName(UClass* ComponentClass)
{
    if (!ComponentClass)
        return TEXT("Unknown");

    FString DisplayName = ComponentClass->GetDisplayNameText().ToString();
    if (!DisplayName.IsEmpty())
        return DisplayName;

    // Fallback to class name with some cleanup
    FString ClassName = ComponentClass->GetName();
    
    // Remove common prefixes
    if (ClassName.StartsWith(TEXT("U")))
        ClassName.RemoveFromStart(TEXT("U"));
    
    // Add spaces before capital letters
    FString FriendlyName;
    for (int32 i = 0; i < ClassName.Len(); i++)
    {
        TCHAR Char = ClassName[i];
        if (i > 0 && FChar::IsUpper(Char))
        {
            FriendlyName += TEXT(" ");
        }
        FriendlyName += Char;
    }

    return FriendlyName;
}

FString FBlueprintComponentReflection::GetComponentCategory(UClass* ComponentClass)
{
    if (!ComponentClass)
        return TEXT("Unknown");

    // Check for explicit category metadata first
    FString Category = ComponentClass->GetMetaData(TEXT("Category"));
    if (!Category.IsEmpty())
        return Category;

    // Infer category from class hierarchy to match UE component browser
    FString ClassName = ComponentClass->GetName();
    
    // Audio components (heuristic by name to avoid hard dependency)
    if (ClassName.Contains(TEXT("Audio")) || ClassName.Contains(TEXT("Sound")))
        return TEXT("Audio");
    
    // AI/Perception components  
    if (ClassName.Contains(TEXT("AIPerception")) || ClassName.Contains(TEXT("Pawn")) || ClassName.Contains(TEXT("Blackboard")) || ClassName.Contains(TEXT("BehaviorTree")))
        return TEXT("AI");
    
    // Lighting components (name-based)
    if (ClassName.Contains(TEXT("Light")))
        return TEXT("Lighting");
    
    // Camera components (name-based heuristic)
    if (ClassName.Contains(TEXT("Camera")))
        return TEXT("Camera");
    
    // Physics/Constraint components (check before general Primitive/Scene components)
    if (ClassName.Contains(TEXT("Physics")) || ClassName.Contains(TEXT("Constraint")) || ClassName.Contains(TEXT("Rigid")) || 
        ClassName.Contains(TEXT("Collision")) || ClassName.Contains(TEXT("Force")) || ClassName.Contains(TEXT("Thruster")))
        return TEXT("Physics");
    
    // Movement components (check before general Scene components) 
    if (ClassName.Contains(TEXT("Movement")) || ClassName.Contains(TEXT("Motor")) || ClassName.Contains(TEXT("Control")) ||
        ClassName.Contains(TEXT("Floating")) || ClassName.Contains(TEXT("Character")) || ClassName.Contains(TEXT("Projectile")))
        return TEXT("Movement");
    
    // Mesh and rendering components (including Static Mesh, Skeletal Mesh, etc.)
    if (ClassName.Contains(TEXT("StaticMesh")) || ClassName.Contains(TEXT("SkeletalMesh")) || ClassName.Contains(TEXT("Mesh")) || ClassName.Contains(TEXT("Render")))
        return TEXT("Rendering");
    
    // Primitive components (geometry shapes - Box, Sphere, Capsule, etc.)
    if (ClassName.Contains(TEXT("Primitive")) || ClassName.Contains(TEXT("Box")) || ClassName.Contains(TEXT("Sphere")) || 
        ClassName.Contains(TEXT("Capsule")) || ClassName.Contains(TEXT("Plane")) || ClassName.Contains(TEXT("Cube")))
        return TEXT("Scene");
    
    // Particle/VFX components
    if (ClassName.Contains(TEXT("Particle")) || ClassName.Contains(TEXT("VFX")) || ClassName.Contains(TEXT("Effect")))
        return TEXT("Effects");
    
    // UI/Widget components
    if (ClassName.Contains(TEXT("Widget")) || ClassName.Contains(TEXT("UI")))
        return TEXT("UI");
    
    // Animation components
    if (ClassName.Contains(TEXT("Anim")) || ClassName.Contains(TEXT("Pose")))
        return TEXT("Animation");
    
    // Navigation components
    if (ClassName.Contains(TEXT("Nav")) || ClassName.Contains(TEXT("Spline")))
        return TEXT("Navigation");
    
    // Generic scene components
    if (ComponentClass->IsChildOf<USceneComponent>())
        return TEXT("Scene");
    
    // Non-scene actor components
    if (ComponentClass->IsChildOf<UActorComponent>())
        return TEXT("Gameplay");
    
    return TEXT("Other");
}

TArray<FString> FBlueprintComponentReflection::GetComponentUsageExamples(UClass* ComponentClass)
{
    TArray<FString> Examples;

    if (!ComponentClass)
        return Examples;

    FString ComponentName = ComponentClass->GetName();
    
    // Generate usage examples based on component type
    if (ComponentName.Contains(TEXT("StaticMesh")))
    {
        Examples.Add(TEXT("Use for static geometry like walls, floors, decorative objects"));
        Examples.Add(TEXT("Perfect for non-moving environmental assets"));
        Examples.Add(TEXT("Can be used as collision volumes when configured properly"));
    }
    else if (ComponentName.Contains(TEXT("SkeletalMesh")))
    {
        Examples.Add(TEXT("Use for animated characters and creatures"));
        Examples.Add(TEXT("Perfect for objects that need bone-based animation"));
        Examples.Add(TEXT("Supports physics simulation and cloth simulation"));
    }
    else if (ComponentName.Contains(TEXT("Light")))
    {
        Examples.Add(TEXT("Provides illumination for your scenes"));
        Examples.Add(TEXT("Use for dynamic lighting effects"));
        Examples.Add(TEXT("Configure intensity, color, and shadow settings"));
    }
    else if (ComponentName.Contains(TEXT("Camera")))
    {
        Examples.Add(TEXT("Define viewpoints for players or cinematic shots"));
        Examples.Add(TEXT("Configure field of view and projection settings"));
        Examples.Add(TEXT("Use for security cameras or weapon scopes"));
    }
    else if (ComponentName.Contains(TEXT("Audio")) || ComponentName.Contains(TEXT("Sound")))
    {
        Examples.Add(TEXT("Play sound effects and ambient audio"));
        Examples.Add(TEXT("Configure 3D spatial audio settings"));
        Examples.Add(TEXT("Use for environmental sounds or character voices"));
    }
    else
    {
        Examples.Add(FString::Printf(TEXT("Component of type %s - check documentation for specific usage"), *ComponentName));
    }

    return Examples;
}

void FBlueprintComponentReflection::InitializeCache()
{
    if (bCacheInitialized)
        return;

    UE_LOG(LogTemp, Log, TEXT("Initializing Blueprint Component Reflection cache"));
    
    // Initialize any caching structures here
    CachedComponentsByCategory.Empty();
    CachedComponentMetadata.Empty();
    
    bCacheInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("Blueprint Component Reflection cache initialized successfully"));
}

void FBlueprintComponentReflection::ClearCache()
{
    UE_LOG(LogTemp, Log, TEXT("Clearing Blueprint Component Reflection cache"));
    
    CachedComponentsByCategory.Empty();
    CachedComponentMetadata.Empty();
    bCacheInitialized = false;
}

// NEW: Enhanced FindComponentInBlueprint with optional type validation
// 2-parameter overload - calls 3-parameter version with nullptr for ExpectedClass
UActorComponent* FBlueprintComponentReflection::FindComponentInBlueprint(
    UBlueprint* Blueprint,
    const FString& ComponentName)
{
    return FindComponentInBlueprint(Blueprint, ComponentName, nullptr);
}

// 3-parameter version with type validation
UActorComponent* FBlueprintComponentReflection::FindComponentInBlueprint(
    UBlueprint* Blueprint,
    const FString& ComponentName,
    UClass* ExpectedClass)
{
    if (!Blueprint)
    {
        UE_LOG(LogTemp, Warning, TEXT("FindComponentInBlueprint: Blueprint is null"));
        return nullptr;
    }

    // Search in Simple Construction Script first
    if (Blueprint->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
        for (USCS_Node* Node : AllNodes)
        {
            if (Node && Node->ComponentTemplate)
            {
                FString NodeName = Node->GetVariableName().ToString();
                if (NodeName.Equals(ComponentName, ESearchCase::IgnoreCase))
                {
                    // Validate type if expected class is provided
                    if (ExpectedClass && !Node->ComponentTemplate->IsA(ExpectedClass))
                    {
                        UE_LOG(LogTemp, Warning,
                            TEXT("Found component '%s' but type mismatch: expected %s, got %s"),
                            *ComponentName,
                            *ExpectedClass->GetName(),
                            *Node->ComponentTemplate->GetClass()->GetName());
                        continue;
                    }
                    return Node->ComponentTemplate;
                }
            }
        }
    }

    // Search in Class Default Object (for inherited components)
    if (Blueprint->GeneratedClass)
    {
        AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
        if (CDO)
        {
            TArray<UActorComponent*> Components;
            CDO->GetComponents(Components);

            for (UActorComponent* Component : Components)
            {
                if (Component && Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
                {
                    // Validate type if expected class is provided
                    if (ExpectedClass && !Component->IsA(ExpectedClass))
                    {
                        continue;
                    }
                    return Component;
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Component '%s' not found in Blueprint '%s'"), *ComponentName, *Blueprint->GetName());
    return nullptr;
}

// NEW: Extract property values from component instance
TSharedPtr<FJsonObject> FBlueprintComponentReflection::GetComponentPropertyValues(
    UActorComponent* Component,
    UClass* ComponentClass)
{
    TSharedPtr<FJsonObject> PropertyValues = MakeShareable(new FJsonObject);

    if (!Component || !ComponentClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetComponentPropertyValues: Component or ComponentClass is null"));
        return PropertyValues;
    }

    // Iterate all properties using reflection
    for (TFieldIterator<FProperty> PropIt(ComponentClass); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        if (!Property)
        {
            continue;
        }

        FString PropertyName = Property->GetName();

        // Get property value container (the component instance)
        const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);

        // Convert property value to JSON
        TSharedPtr<FJsonValue> JsonValue = PropertyToJsonValue(Property, ValuePtr);

        if (JsonValue.IsValid())
        {
            PropertyValues->SetField(PropertyName, JsonValue);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Extracted %d property values from component '%s'"), 
        PropertyValues->Values.Num(), *Component->GetName());

    return PropertyValues;
}

// NEW: Convert property value to JSON value using type-safe reflection
TSharedPtr<FJsonValue> FBlueprintComponentReflection::PropertyToJsonValue(
    const FProperty* Property,
    const void* ValuePtr)
{
    if (!Property || !ValuePtr)
    {
        return MakeShareable(new FJsonValueNull());
    }

    // Numeric types
    if (const FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
    {
        if (NumericProp->IsFloatingPoint())
        {
            double Value = NumericProp->GetFloatingPointPropertyValue(ValuePtr);
            return MakeShareable(new FJsonValueNumber(Value));
        }
        else if (NumericProp->IsInteger())
        {
            int64 Value = NumericProp->GetSignedIntPropertyValue(ValuePtr);
            return MakeShareable(new FJsonValueNumber(static_cast<double>(Value)));
        }
    }

    // Boolean
    if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        bool Value = BoolProp->GetPropertyValue(ValuePtr);
        return MakeShareable(new FJsonValueBoolean(Value));
    }

    // String
    if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        FString Value = StrProp->GetPropertyValue(ValuePtr);
        return MakeShareable(new FJsonValueString(Value));
    }

    // Name
    if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        FName Value = NameProp->GetPropertyValue(ValuePtr);
        return MakeShareable(new FJsonValueString(Value.ToString()));
    }

    // Enum
    if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        UEnum* EnumDef = EnumProp->GetEnum();
        int64 EnumValue = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
        FString EnumName = EnumDef->GetNameStringByValue(EnumValue);
        return MakeShareable(new FJsonValueString(EnumName));
    }

    // Byte Enum
    if (const FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        if (ByteProp->Enum)
        {
            uint8 ByteValue = ByteProp->GetPropertyValue(ValuePtr);
            FString EnumName = ByteProp->Enum->GetNameStringByValue(ByteValue);
            return MakeShareable(new FJsonValueString(EnumName));
        }
        else
        {
            uint8 Value = ByteProp->GetPropertyValue(ValuePtr);
            return MakeShareable(new FJsonValueNumber(static_cast<double>(Value)));
        }
    }

    // Struct (special handling for common types)
    if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        UScriptStruct* Struct = StructProp->Struct;

        // FVector
        if (Struct->GetName() == TEXT("Vector"))
        {
            const FVector* VectorValue = static_cast<const FVector*>(ValuePtr);
            TArray<TSharedPtr<FJsonValue>> Array;
            Array.Add(MakeShareable(new FJsonValueNumber(VectorValue->X)));
            Array.Add(MakeShareable(new FJsonValueNumber(VectorValue->Y)));
            Array.Add(MakeShareable(new FJsonValueNumber(VectorValue->Z)));
            return MakeShareable(new FJsonValueArray(Array));
        }

        // FRotator
        if (Struct->GetName() == TEXT("Rotator"))
        {
            const FRotator* RotatorValue = static_cast<const FRotator*>(ValuePtr);
            TArray<TSharedPtr<FJsonValue>> Array;
            Array.Add(MakeShareable(new FJsonValueNumber(RotatorValue->Pitch)));
            Array.Add(MakeShareable(new FJsonValueNumber(RotatorValue->Yaw)));
            Array.Add(MakeShareable(new FJsonValueNumber(RotatorValue->Roll)));
            return MakeShareable(new FJsonValueArray(Array));
        }

        // FColor
        if (Struct->GetName() == TEXT("Color"))
        {
            const FColor* ColorValue = static_cast<const FColor*>(ValuePtr);
            TArray<TSharedPtr<FJsonValue>> Array;
            Array.Add(MakeShareable(new FJsonValueNumber(ColorValue->R)));
            Array.Add(MakeShareable(new FJsonValueNumber(ColorValue->G)));
            Array.Add(MakeShareable(new FJsonValueNumber(ColorValue->B)));
            Array.Add(MakeShareable(new FJsonValueNumber(ColorValue->A)));
            return MakeShareable(new FJsonValueArray(Array));
        }

        // FLinearColor
        if (Struct->GetName() == TEXT("LinearColor"))
        {
            const FLinearColor* ColorValue = static_cast<const FLinearColor*>(ValuePtr);
            TArray<TSharedPtr<FJsonValue>> Array;
            Array.Add(MakeShareable(new FJsonValueNumber(ColorValue->R)));
            Array.Add(MakeShareable(new FJsonValueNumber(ColorValue->G)));
            Array.Add(MakeShareable(new FJsonValueNumber(ColorValue->B)));
            Array.Add(MakeShareable(new FJsonValueNumber(ColorValue->A)));
            return MakeShareable(new FJsonValueArray(Array));
        }

        // FTransform
        if (Struct->GetName() == TEXT("Transform"))
        {
            const FTransform* TransformValue = static_cast<const FTransform*>(ValuePtr);
            TSharedPtr<FJsonObject> TransformObject = MakeShareable(new FJsonObject);

            // Location
            TArray<TSharedPtr<FJsonValue>> LocationArray;
            FVector Location = TransformValue->GetLocation();
            LocationArray.Add(MakeShareable(new FJsonValueNumber(Location.X)));
            LocationArray.Add(MakeShareable(new FJsonValueNumber(Location.Y)));
            LocationArray.Add(MakeShareable(new FJsonValueNumber(Location.Z)));
            TransformObject->SetArrayField(TEXT("Location"), LocationArray);

            // Rotation
            TArray<TSharedPtr<FJsonValue>> RotationArray;
            FRotator Rotation = TransformValue->Rotator();
            RotationArray.Add(MakeShareable(new FJsonValueNumber(Rotation.Pitch)));
            RotationArray.Add(MakeShareable(new FJsonValueNumber(Rotation.Yaw)));
            RotationArray.Add(MakeShareable(new FJsonValueNumber(Rotation.Roll)));
            TransformObject->SetArrayField(TEXT("Rotation"), RotationArray);

            // Scale
            TArray<TSharedPtr<FJsonValue>> ScaleArray;
            FVector Scale = TransformValue->GetScale3D();
            ScaleArray.Add(MakeShareable(new FJsonValueNumber(Scale.X)));
            ScaleArray.Add(MakeShareable(new FJsonValueNumber(Scale.Y)));
            ScaleArray.Add(MakeShareable(new FJsonValueNumber(Scale.Z)));
            TransformObject->SetArrayField(TEXT("Scale"), ScaleArray);

            return MakeShareable(new FJsonValueObject(TransformObject));
        }

        // Generic struct - serialize all properties recursively
        TSharedPtr<FJsonObject> StructObject = MakeShareable(new FJsonObject);
        for (TFieldIterator<FProperty> It(Struct); It; ++It)
        {
            FProperty* StructProperty = *It;
            const void* StructValuePtr = StructProperty->ContainerPtrToValuePtr<void>(ValuePtr);
            TSharedPtr<FJsonValue> StructPropertyValue = PropertyToJsonValue(StructProperty, StructValuePtr);
            if (StructPropertyValue.IsValid())
            {
                StructObject->SetField(StructProperty->GetName(), StructPropertyValue);
            }
        }
        return MakeShareable(new FJsonValueObject(StructObject));
    }

    // Object reference
    if (const FObjectProperty* ObjectProp = CastField<FObjectProperty>(Property))
    {
        UObject* ObjectValue = ObjectProp->GetObjectPropertyValue(ValuePtr);
        if (ObjectValue)
        {
            return MakeShareable(new FJsonValueString(ObjectValue->GetPathName()));
        }
        return MakeShareable(new FJsonValueNull());
    }

    // Array
    if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
    {
        FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
        TArray<TSharedPtr<FJsonValue>> JsonArray;

        for (int32 i = 0; i < ArrayHelper.Num(); ++i)
        {
            const void* ElementPtr = ArrayHelper.GetRawPtr(i);
            TSharedPtr<FJsonValue> ElementValue = PropertyToJsonValue(ArrayProp->Inner, ElementPtr);
            if (ElementValue.IsValid())
            {
                JsonArray.Add(ElementValue);
            }
        }

        return MakeShareable(new FJsonValueArray(JsonArray));
    }

    // Unsupported type - return string representation using ExportTextItem
    FString ExportedValue;
    Property->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);
    return MakeShareable(new FJsonValueString(ExportedValue));
}

// NEW HANDLER IMPLEMENTATIONS for manage_blueprint_component

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleGetComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName, ComponentName, PropertyName;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
        !Params->TryGetStringField(TEXT("component_name"), ComponentName) ||
        !Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing required parameters: blueprint_name, component_name, property_name"));
    }

    // Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return CreateErrorResponse(FindResult.GetErrorCode(), FindResult.GetErrorMessage());
    }
    
    UBlueprint* Blueprint = FindResult.GetValue();

    // Find the component
    UActorComponent* Component = FindComponentInBlueprint(Blueprint, ComponentName);
    if (!Component)
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::COMPONENT_NOT_FOUND, FString::Printf(TEXT("Component '%s' not found in Blueprint '%s'"), *ComponentName, *BlueprintName));
    }

    // Find the property
    UClass* ComponentClass = Component->GetClass();
    const FProperty* Property = ComponentClass->FindPropertyByName(*PropertyName);
    if (!Property)
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PROPERTY_NOT_FOUND, FString::Printf(TEXT("Property '%s' not found in component '%s'"), *PropertyName, *ComponentName));
    }

    // Get property value
    const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);
    TSharedPtr<FJsonValue> PropertyValue = PropertyToJsonValue(Property, ValuePtr);

    if (!PropertyValue.IsValid())
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PROPERTY_GET_FAILED, FString::Printf(TEXT("Failed to read property '%s' value"), *PropertyName));
    }

    // Build response
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("property_name"), PropertyName);
    Response->SetStringField(TEXT("component_name"), ComponentName);
    Response->SetStringField(TEXT("type"), GetPropertyCPPType(Property));
    Response->SetField(TEXT("value"), PropertyValue);

    return Response;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleGetAllComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName, ComponentName;
    bool bIncludeInherited = true;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
        !Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing required parameters: blueprint_name, component_name"));
    }

    Params->TryGetBoolField(TEXT("include_inherited"), bIncludeInherited);

    // Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return CreateErrorResponse(FindResult.GetErrorCode(), FindResult.GetErrorMessage());
    }
    
    UBlueprint* Blueprint = FindResult.GetValue();

    // Find the component
    UActorComponent* Component = FindComponentInBlueprint(Blueprint, ComponentName);
    if (!Component)
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::COMPONENT_NOT_FOUND, FString::Printf(TEXT("Component '%s' not found in Blueprint '%s'"), *ComponentName, *BlueprintName));
    }

    // Get all properties
    UClass* ComponentClass = Component->GetClass();
    TSharedPtr<FJsonObject> PropertiesObject = MakeShareable(new FJsonObject);
    int32 PropertyCount = 0;

    for (TFieldIterator<FProperty> It(ComponentClass); It; ++It)
    {
        FProperty* Property = *It;
        
        // Skip if not including inherited and property is from parent class
        if (!bIncludeInherited && Property->GetOwnerClass() != ComponentClass)
        {
            continue;
        }

        // Skip properties that shouldn't be exposed
        if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient))
        {
            continue;
        }

        const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Component);
        TSharedPtr<FJsonValue> PropertyValue = PropertyToJsonValue(Property, ValuePtr);

        if (PropertyValue.IsValid())
        {
            PropertiesObject->SetField(Property->GetName(), PropertyValue);
            PropertyCount++;
        }
    }

    // Build response
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("component_name"), ComponentName);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetObjectField(TEXT("properties"), PropertiesObject);
    Response->SetNumberField(TEXT("property_count"), PropertyCount);
    Response->SetBoolField(TEXT("include_inherited"), bIncludeInherited);

    return Response;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleCompareComponentProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName, ComponentName, CompareToBlueprint, CompareToComponent;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
        !Params->TryGetStringField(TEXT("component_name"), ComponentName) ||
        !Params->TryGetStringField(TEXT("compare_to_blueprint"), CompareToBlueprint) ||
        !Params->TryGetStringField(TEXT("compare_to_component"), CompareToComponent))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing required parameters: blueprint_name, component_name, compare_to_blueprint, compare_to_component"));
    }

    // Find both Blueprints using DiscoveryService
    auto FindResult1 = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult1.IsError())
    {
        return CreateErrorResponse(FindResult1.GetErrorCode(), FindResult1.GetErrorMessage());
    }
    
    auto FindResult2 = DiscoveryService->FindBlueprint(CompareToBlueprint);
    if (FindResult2.IsError())
    {
        return CreateErrorResponse(FindResult2.GetErrorCode(), FindResult2.GetErrorMessage());
    }
    
    UBlueprint* Blueprint1 = FindResult1.GetValue();
    UBlueprint* Blueprint2 = FindResult2.GetValue();

    // Find both components
    UActorComponent* Component1 = FindComponentInBlueprint(Blueprint1, ComponentName);
    UActorComponent* Component2 = FindComponentInBlueprint(Blueprint2, CompareToComponent);
    
    if (!Component1)
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::COMPONENT_NOT_FOUND, FString::Printf(TEXT("Component '%s' not found in Blueprint '%s'"), *ComponentName, *BlueprintName));
    }
    if (!Component2)
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::COMPONENT_NOT_FOUND, FString::Printf(TEXT("Component '%s' not found in Blueprint '%s'"), *CompareToComponent, *CompareToBlueprint));
    }

    // Ensure components are same type
    if (Component1->GetClass() != Component2->GetClass())
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::COMPONENT_TYPE_INCOMPATIBLE, 
            FString::Printf(TEXT("Component types don't match: '%s' vs '%s'"), 
            *Component1->GetClass()->GetName(), *Component2->GetClass()->GetName()));
    }

    // Compare all properties
    UClass* ComponentClass = Component1->GetClass();
    TArray<TSharedPtr<FJsonValue>> DifferencesArray;
    int32 MatchingCount = 0;
    int32 DifferenceCount = 0;

    for (TFieldIterator<FProperty> It(ComponentClass); It; ++It)
    {
        FProperty* Property = *It;
        
        // Skip transient properties
        if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
        {
            continue;
        }

        const void* ValuePtr1 = Property->ContainerPtrToValuePtr<void>(Component1);
        const void* ValuePtr2 = Property->ContainerPtrToValuePtr<void>(Component2);

        TSharedPtr<FJsonValue> Value1 = PropertyToJsonValue(Property, ValuePtr1);
        TSharedPtr<FJsonValue> Value2 = PropertyToJsonValue(Property, ValuePtr2);

        // Compare values
        bool bValuesMatch = false;
        if (Value1.IsValid() && Value2.IsValid())
        {
            // Simple string comparison of JSON representation
            FString Value1Str, Value2Str;
            TSharedRef<TJsonWriter<>> Writer1 = TJsonWriterFactory<>::Create(&Value1Str);
            TSharedRef<TJsonWriter<>> Writer2 = TJsonWriterFactory<>::Create(&Value2Str);
            FJsonSerializer::Serialize(Value1, TEXT(""), Writer1);
            FJsonSerializer::Serialize(Value2, TEXT(""), Writer2);
            
            bValuesMatch = (Value1Str == Value2Str);
        }

        if (bValuesMatch)
        {
            MatchingCount++;
        }
        else
        {
            DifferenceCount++;
            
            // Add to differences array
            TSharedPtr<FJsonObject> DiffObject = MakeShareable(new FJsonObject);
            DiffObject->SetStringField(TEXT("property"), Property->GetName());
            DiffObject->SetField(TEXT("source_value"), Value1);
            DiffObject->SetField(TEXT("target_value"), Value2);
            DifferencesArray.Add(MakeShareable(new FJsonValueObject(DiffObject)));
        }
    }

    // Build response
    TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetBoolField(TEXT("matches"), DifferenceCount == 0);
    Response->SetNumberField(TEXT("matching_count"), MatchingCount);
    Response->SetNumberField(TEXT("difference_count"), DifferenceCount);
    Response->SetArrayField(TEXT("differences"), DifferencesArray);
    Response->SetStringField(TEXT("source_component"), ComponentName);
    Response->SetStringField(TEXT("target_component"), CompareToComponent);

    return Response;
}

TSharedPtr<FJsonObject> FBlueprintComponentReflection::HandleReparentComponent(const TSharedPtr<FJsonObject>& Params)
{
    // Extract parameters
    FString BlueprintName, ComponentName, ParentName;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) ||
        !Params->TryGetStringField(TEXT("component_name"), ComponentName) ||
        !Params->TryGetStringField(TEXT("parent_name"), ParentName))
    {
        return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, 
            TEXT("Missing required parameters: blueprint_name, component_name, parent_name"));
    }

#if WITH_EDITOR
    // Find Blueprint using DiscoveryService
    auto FindResult = DiscoveryService->FindBlueprint(BlueprintName);
    if (FindResult.IsError())
    {
        return CreateErrorResponse(FindResult.GetErrorCode(), FindResult.GetErrorMessage());
    }

    // Reparent component using ComponentService
    auto ReparentResult = ComponentService->ReparentComponent(
        FindResult.GetValue(),
        ComponentName,
        ParentName
    );

    if (ReparentResult.IsError())
    {
        return CreateErrorResponse(ReparentResult.GetErrorCode(), ReparentResult.GetErrorMessage());
    }

    // Build success response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), TEXT("Component reparented successfully"));
    Response->SetStringField(TEXT("component_name"), ComponentName);
    Response->SetStringField(TEXT("new_parent"), ParentName);

    UE_LOG(LogTemp, Log, TEXT("Reparented component '%s' to '%s' in Blueprint '%s'"), 
        *ComponentName, *ParentName, *BlueprintName);

    return Response;
#else
    return CreateErrorResponse(VibeUE::ErrorCodes::OPERATION_NOT_SUPPORTED, 
        TEXT("Reparent component only available in Editor builds"));
#endif
}
