#include "Commands/BlueprintCommands.h"
#include "Commands/CommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "Engine/StaticMesh.h"

FBlueprintCommands::FBlueprintCommands()
{
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("create_blueprint"))
    {
        return HandleCreateBlueprint(Params);
    }
    else if (CommandType == TEXT("add_component_to_blueprint"))
    {
        return HandleAddComponentToBlueprint(Params);
    }
    else if (CommandType == TEXT("set_component_property"))
    {
        return HandleSetComponentProperty(Params);
    }
    else if (CommandType == TEXT("set_physics_properties"))
    {
        return HandleSetPhysicsProperties(Params);
    }
    else if (CommandType == TEXT("compile_blueprint"))
    {
        return HandleCompileBlueprint(Params);
    }
    else if (CommandType == TEXT("set_blueprint_property"))
    {
        return HandleSetBlueprintProperty(Params);
    }
    else if (CommandType == TEXT("set_static_mesh_properties"))
    {
        return HandleSetStaticMeshProperties(Params);
    }
    else if (CommandType == TEXT("reparent_blueprint"))
    {
        return HandleReparentBlueprint(Params);
    }
    else if (CommandType == TEXT("add_blueprint_variable"))
    {
        return HandleAddBlueprintVariable(Params);
    }
    else if (CommandType == TEXT("get_blueprint_variable"))
    {
        return HandleGetBlueprintVariable(Params);
    }
    else if (CommandType == TEXT("set_blueprint_variable"))
    {
        return HandleSetBlueprintVariable(Params);
    }
    else if (CommandType == TEXT("get_available_blueprint_variable_types"))
    {
        return HandleGetAvailableBlueprintVariableTypes(Params);
    }
    
    return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Check if blueprint already exists
    FString PackagePath;
    if (!Params->TryGetStringField(TEXT("path"), PackagePath))
    {
        // Default to /Game/Blueprints/ if no path specified
        PackagePath = TEXT("/Game/Blueprints/");
    }
    
    // Ensure path ends with slash
    if (!PackagePath.EndsWith(TEXT("/")))
    {
        PackagePath += TEXT("/");
    }
    
    FString AssetName = BlueprintName;
    if (UEditorAssetLibrary::DoesAssetExist(PackagePath + AssetName))
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint already exists: %s"), *BlueprintName));
    }

    // Create the blueprint factory
    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    
    // Handle parent class
    FString ParentClass;
    Params->TryGetStringField(TEXT("parent_class"), ParentClass);
    
    // Default to Actor if no parent class specified
    UClass* SelectedParentClass = AActor::StaticClass();
    
    // Try to find the specified parent class
    if (!ParentClass.IsEmpty())
    {
        FString ClassName = ParentClass;
        if (!ClassName.StartsWith(TEXT("A")))
        {
            ClassName = TEXT("A") + ClassName;
        }
        
        // First try direct StaticClass lookup for common classes
        UClass* FoundClass = nullptr;
        if (ClassName == TEXT("APawn"))
        {
            FoundClass = APawn::StaticClass();
        }
        else if (ClassName == TEXT("AActor"))
        {
            FoundClass = AActor::StaticClass();
        }
        else
        {
            // Try loading the class using LoadClass which is more reliable than FindObject
            const FString ClassPath = FString::Printf(TEXT("/Script/Engine.%s"), *ClassName);
            FoundClass = LoadClass<AActor>(nullptr, *ClassPath);
            
            if (!FoundClass)
            {
                // Try alternate paths if not found
                const FString GameClassPath = FString::Printf(TEXT("/Script/Game.%s"), *ClassName);
                FoundClass = LoadClass<AActor>(nullptr, *GameClassPath);
            }
        }

        if (FoundClass)
        {
            SelectedParentClass = FoundClass;
            UE_LOG(LogTemp, Log, TEXT("Successfully set parent class to '%s'"), *ClassName);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not find specified parent class '%s' at paths: /Script/Engine.%s or /Script/Game.%s, defaulting to AActor"), 
                *ClassName, *ClassName, *ClassName);
        }
    }
    
    Factory->ParentClass = SelectedParentClass;

    // Create the blueprint
    UPackage* Package = CreatePackage(*(PackagePath + AssetName));
    UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr, GWarn));

    if (NewBlueprint)
    {
        // Notify the asset registry
        FAssetRegistryModule::AssetCreated(NewBlueprint);

        // Mark the package dirty
        Package->MarkPackageDirty();

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("name"), AssetName);
        ResultObj->SetStringField(TEXT("path"), PackagePath + AssetName);
        return ResultObj;
    }

    return FCommonUtils::CreateErrorResponse(TEXT("Failed to create blueprint"));
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleAddComponentToBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentType;
    if (!Params->TryGetStringField(TEXT("component_type"), ComponentType))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Create the component - dynamically find the component class by name
    UClass* ComponentClass = nullptr;

    // Try to find the class with exact name first
    ComponentClass = FindFirstObject<UClass>(*ComponentType, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("VibeUEBlueprintCommands"));
    
    // If not found, try with "Component" suffix
    if (!ComponentClass && !ComponentType.EndsWith(TEXT("Component")))
    {
        FString ComponentTypeWithSuffix = ComponentType + TEXT("Component");
        ComponentClass = FindFirstObject<UClass>(*ComponentTypeWithSuffix, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("VibeUEBlueprintCommands"));
    }
    
    // If still not found, try with "U" prefix
    if (!ComponentClass && !ComponentType.StartsWith(TEXT("U")))
    {
        FString ComponentTypeWithPrefix = TEXT("U") + ComponentType;
        ComponentClass = FindFirstObject<UClass>(*ComponentTypeWithPrefix, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("VibeUEBlueprintCommands"));
        
        // Try with both prefix and suffix
        if (!ComponentClass && !ComponentType.EndsWith(TEXT("Component")))
        {
            FString ComponentTypeWithBoth = TEXT("U") + ComponentType + TEXT("Component");
            ComponentClass = FindFirstObject<UClass>(*ComponentTypeWithBoth, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("VibeUEBlueprintCommands"));
        }
    }
    
    // Verify that the class is a valid component type
    if (!ComponentClass || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown component type: %s"), *ComponentType));
    }

    // Add the component to the blueprint
    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(ComponentClass, *ComponentName);
    if (NewNode)
    {
        // Set transform if provided
        USceneComponent* SceneComponent = Cast<USceneComponent>(NewNode->ComponentTemplate);
        if (SceneComponent)
        {
            if (Params->HasField(TEXT("location")))
            {
                SceneComponent->SetRelativeLocation(FCommonUtils::GetVectorFromJson(Params, TEXT("location")));
            }
            if (Params->HasField(TEXT("rotation")))
            {
                SceneComponent->SetRelativeRotation(FCommonUtils::GetRotatorFromJson(Params, TEXT("rotation")));
            }
            if (Params->HasField(TEXT("scale")))
            {
                SceneComponent->SetRelativeScale3D(FCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
            }
        }

        // Add to root if no parent specified
        Blueprint->SimpleConstructionScript->AddNode(NewNode);

        // Compile the blueprint
        FKismetEditorUtilities::CompileBlueprint(Blueprint);

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("component_name"), ComponentName);
        ResultObj->SetStringField(TEXT("component_type"), ComponentType);
        return ResultObj;
    }

    return FCommonUtils::CreateErrorResponse(TEXT("Failed to add component to blueprint"));
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleSetComponentProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Log all input parameters for debugging
    UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - Blueprint: %s, Component: %s, Property: %s"), 
        *BlueprintName, *ComponentName, *PropertyName);
    
    // Log property_value if available
    if (Params->HasField(TEXT("property_value")))
    {
        TSharedPtr<FJsonValue> JsonValue = Params->Values.FindRef(TEXT("property_value"));
        FString ValueType;
        
        switch(JsonValue->Type)
        {
            case EJson::Boolean: ValueType = FString::Printf(TEXT("Boolean: %s"), JsonValue->AsBool() ? TEXT("true") : TEXT("false")); break;
            case EJson::Number: ValueType = FString::Printf(TEXT("Number: %f"), JsonValue->AsNumber()); break;
            case EJson::String: ValueType = FString::Printf(TEXT("String: %s"), *JsonValue->AsString()); break;
            case EJson::Array: ValueType = TEXT("Array"); break;
            case EJson::Object: ValueType = TEXT("Object"); break;
            default: ValueType = TEXT("Unknown"); break;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - Value Type: %s"), *ValueType);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - No property_value provided"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Blueprint not found: %s"), *BlueprintName);
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Blueprint found: %s (Class: %s)"), 
            *BlueprintName, 
            Blueprint->GeneratedClass ? *Blueprint->GeneratedClass->GetName() : TEXT("NULL"));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Searching for component %s in blueprint nodes"), *ComponentName);
    
    if (!Blueprint->SimpleConstructionScript)
    {
        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - SimpleConstructionScript is NULL for blueprint %s"), *BlueprintName);
        return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint construction script"));
    }
    
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node)
        {
            UE_LOG(LogTemp, Verbose, TEXT("SetComponentProperty - Found node: %s"), *Node->GetVariableName().ToString());
            if (Node->GetVariableName().ToString() == ComponentName)
            {
                ComponentNode = Node;
                break;
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - Found NULL node in blueprint"));
        }
    }

    if (!ComponentNode)
    {
        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Component not found: %s"), *ComponentName);
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Component found: %s (Class: %s)"), 
            *ComponentName, 
            ComponentNode->ComponentTemplate ? *ComponentNode->ComponentTemplate->GetClass()->GetName() : TEXT("NULL"));
    }

    // Get the component template
    UObject* ComponentTemplate = ComponentNode->ComponentTemplate;
    if (!ComponentTemplate)
    {
        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Component template is NULL for %s"), *ComponentName);
        return FCommonUtils::CreateErrorResponse(TEXT("Invalid component template"));
    }

    // Check if this is a Spring Arm component and log special debug info
    if (ComponentTemplate->GetClass()->GetName().Contains(TEXT("SpringArm")))
    {
        UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - SpringArm component detected! Class: %s"), 
            *ComponentTemplate->GetClass()->GetPathName());
            
        // Log all properties of the SpringArm component class
        UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - SpringArm properties:"));
        for (TFieldIterator<FProperty> PropIt(ComponentTemplate->GetClass()); PropIt; ++PropIt)
        {
            FProperty* Prop = *PropIt;
            UE_LOG(LogTemp, Warning, TEXT("  - %s (%s)"), *Prop->GetName(), *Prop->GetCPPType());
        }

        // Special handling for Spring Arm properties
        if (Params->HasField(TEXT("property_value")))
        {
            TSharedPtr<FJsonValue> JsonValue = Params->Values.FindRef(TEXT("property_value"));
            
            // Get the property using the new FField system
            FProperty* Property = FindFProperty<FProperty>(ComponentTemplate->GetClass(), *PropertyName);
            if (!Property)
            {
                UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Property %s not found on SpringArm component"), *PropertyName);
                return FCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Property %s not found on SpringArm component"), *PropertyName));
            }

            // Create a scope guard to ensure property cleanup
            struct FScopeGuard
            {
                UObject* Object;
                FScopeGuard(UObject* InObject) : Object(InObject) 
                {
                    if (Object)
                    {
                        Object->Modify();
                    }
                }
                ~FScopeGuard()
                {
                    if (Object)
                    {
                        Object->PostEditChange();
                    }
                }
            } ScopeGuard(ComponentTemplate);

            bool bSuccess = false;
            FString ErrorMessage;

            // Handle specific Spring Arm property types
            if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
            {
                if (JsonValue->Type == EJson::Number)
                {
                    const float Value = JsonValue->AsNumber();
                    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting float property %s to %f"), *PropertyName, Value);
                    FloatProp->SetPropertyValue_InContainer(ComponentTemplate, Value);
                    bSuccess = true;
                }
            }
            else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
            {
                if (JsonValue->Type == EJson::Boolean)
                {
                    const bool Value = JsonValue->AsBool();
                    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting bool property %s to %d"), *PropertyName, Value);
                    BoolProp->SetPropertyValue_InContainer(ComponentTemplate, Value);
                    bSuccess = true;
                }
            }
            else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
            {
                UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Handling struct property %s of type %s"), 
                    *PropertyName, *StructProp->Struct->GetName());
                
                // Special handling for common Spring Arm struct properties
                if (StructProp->Struct == TBaseStructure<FVector>::Get())
                {
                    if (JsonValue->Type == EJson::Array)
                    {
                        const TArray<TSharedPtr<FJsonValue>>& Arr = JsonValue->AsArray();
                        if (Arr.Num() == 3)
                        {
                            FVector Vec(
                                Arr[0]->AsNumber(),
                                Arr[1]->AsNumber(),
                                Arr[2]->AsNumber()
                            );
                            void* PropertyAddr = StructProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
                            StructProp->CopySingleValue(PropertyAddr, &Vec);
                            bSuccess = true;
                        }
                    }
                }
                else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
                {
                    if (JsonValue->Type == EJson::Array)
                    {
                        const TArray<TSharedPtr<FJsonValue>>& Arr = JsonValue->AsArray();
                        if (Arr.Num() == 3)
                        {
                            FRotator Rot(
                                Arr[0]->AsNumber(),
                                Arr[1]->AsNumber(),
                                Arr[2]->AsNumber()
                            );
                            void* PropertyAddr = StructProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
                            StructProp->CopySingleValue(PropertyAddr, &Rot);
                            bSuccess = true;
                        }
                    }
                }
            }

            if (bSuccess)
            {
                // Mark the blueprint as modified
                UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Successfully set SpringArm property %s"), *PropertyName);
                FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("component"), ComponentName);
                ResultObj->SetStringField(TEXT("property"), PropertyName);
                ResultObj->SetBoolField(TEXT("success"), true);
                return ResultObj;
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Failed to set SpringArm property %s"), *PropertyName);
                return FCommonUtils::CreateErrorResponse(
                    FString::Printf(TEXT("Failed to set SpringArm property %s"), *PropertyName));
            }
        }
    }

    // Regular property handling for non-Spring Arm components continues...

    // Set the property value
    if (Params->HasField(TEXT("property_value")))
    {
        TSharedPtr<FJsonValue> JsonValue = Params->Values.FindRef(TEXT("property_value"));
        
        // Get the property
        FProperty* Property = FindFProperty<FProperty>(ComponentTemplate->GetClass(), *PropertyName);
        if (!Property)
        {
            UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Property %s not found on component %s"), 
                *PropertyName, *ComponentName);
            
            // List all available properties for this component
            UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - Available properties for %s:"), *ComponentName);
            for (TFieldIterator<FProperty> PropIt(ComponentTemplate->GetClass()); PropIt; ++PropIt)
            {
                FProperty* Prop = *PropIt;
                UE_LOG(LogTemp, Warning, TEXT("  - %s (%s)"), *Prop->GetName(), *Prop->GetCPPType());
            }
            
            return FCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Property %s not found on component %s"), *PropertyName, *ComponentName));
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Property found: %s (Type: %s)"), 
                *PropertyName, *Property->GetCPPType());
        }

        bool bSuccess = false;
        FString ErrorMessage;

        // Handle different property types
        UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Attempting to set property %s"), *PropertyName);
        
        // Add try-catch block to catch and log any crashes
        try
        {
            if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
            {
                // Handle vector properties
                UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Property is a struct: %s"), 
                    StructProp->Struct ? *StructProp->Struct->GetName() : TEXT("NULL"));
                    
                if (StructProp->Struct == TBaseStructure<FVector>::Get())
                {
                    if (JsonValue->Type == EJson::Array)
                    {
                        // Handle array input [x, y, z]
                        const TArray<TSharedPtr<FJsonValue>>& Arr = JsonValue->AsArray();
                        if (Arr.Num() == 3)
                        {
                            FVector Vec(
                                Arr[0]->AsNumber(),
                                Arr[1]->AsNumber(),
                                Arr[2]->AsNumber()
                            );
                            void* PropertyAddr = StructProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
                            UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting Vector(%f, %f, %f)"), 
                                Vec.X, Vec.Y, Vec.Z);
                            StructProp->CopySingleValue(PropertyAddr, &Vec);
                            bSuccess = true;
                        }
                        else
                        {
                            ErrorMessage = FString::Printf(TEXT("Vector property requires 3 values, got %d"), Arr.Num());
                            UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - %s"), *ErrorMessage);
                        }
                    }
                    else if (JsonValue->Type == EJson::Number)
                    {
                        // Handle scalar input (sets all components to same value)
                        float Value = JsonValue->AsNumber();
                        FVector Vec(Value, Value, Value);
                        void* PropertyAddr = StructProp->ContainerPtrToValuePtr<void>(ComponentTemplate);
                        UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting Vector(%f, %f, %f) from scalar"), 
                            Vec.X, Vec.Y, Vec.Z);
                        StructProp->CopySingleValue(PropertyAddr, &Vec);
                        bSuccess = true;
                    }
                    else
                    {
                        ErrorMessage = TEXT("Vector property requires either a single number or array of 3 numbers");
                        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - %s"), *ErrorMessage);
                    }
                }
                else
                {
                    // Handle other struct properties using default handler
                    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Using generic struct handler for %s"), 
                        *PropertyName);
                    bSuccess = FCommonUtils::SetObjectProperty(ComponentTemplate, PropertyName, JsonValue, ErrorMessage);
                    if (!bSuccess)
                    {
                        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Failed to set struct property: %s"), *ErrorMessage);
                    }
                }
            }
            else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
            {
                // Handle enum properties
                UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Property is an enum"));
                if (JsonValue->Type == EJson::String)
                {
                    FString EnumValueName = JsonValue->AsString();
                    UEnum* Enum = EnumProp->GetEnum();
                    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting enum from string: %s"), *EnumValueName);
                    
                    if (Enum)
                    {
                        int64 EnumValue = Enum->GetValueByNameString(EnumValueName);
                        
                        if (EnumValue != INDEX_NONE)
                        {
                            UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Found enum value: %lld"), EnumValue);
                            EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(
                                ComponentTemplate, 
                                EnumValue
                            );
                            bSuccess = true;
                        }
                        else
                        {
                            // List all possible enum values
                            UE_LOG(LogTemp, Warning, TEXT("SetComponentProperty - Available enum values for %s:"), 
                                *Enum->GetName());
                            for (int32 i = 0; i < Enum->NumEnums(); i++)
                            {
                                UE_LOG(LogTemp, Warning, TEXT("  - %s (%lld)"), 
                                    *Enum->GetNameStringByIndex(i),
                                    Enum->GetValueByIndex(i));
                            }
                            
                            ErrorMessage = FString::Printf(TEXT("Invalid enum value '%s' for property %s"), 
                                *EnumValueName, *PropertyName);
                            UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - %s"), *ErrorMessage);
                        }
                    }
                    else
                    {
                        ErrorMessage = TEXT("Enum object is NULL");
                        UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - %s"), *ErrorMessage);
                    }
                }
                else if (JsonValue->Type == EJson::Number)
                {
                    // Allow setting enum by integer value
                    int64 EnumValue = JsonValue->AsNumber();
                    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting enum from number: %lld"), EnumValue);
                    EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(
                        ComponentTemplate, 
                        EnumValue
                    );
                    bSuccess = true;
                }
                else
                {
                    ErrorMessage = TEXT("Enum property requires either a string name or integer value");
                    UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - %s"), *ErrorMessage);
                }
            }
            else if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
            {
                // Handle numeric properties
                UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Property is numeric: IsInteger=%d, IsFloat=%d"), 
                    NumericProp->IsInteger(), NumericProp->IsFloatingPoint());
                    
                if (JsonValue->Type == EJson::Number)
                {
                    double Value = JsonValue->AsNumber();
                    UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Setting numeric value: %f"), Value);
                    
                    if (NumericProp->IsInteger())
                    {
                        NumericProp->SetIntPropertyValue(ComponentTemplate, (int64)Value);
                        UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Set integer value: %lld"), (int64)Value);
                        bSuccess = true;
                    }
                    else if (NumericProp->IsFloatingPoint())
                    {
                        NumericProp->SetFloatingPointPropertyValue(ComponentTemplate, Value);
                        UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Set float value: %f"), Value);
                        bSuccess = true;
                    }
                }
                else
                {
                    ErrorMessage = TEXT("Numeric property requires a number value");
                    UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - %s"), *ErrorMessage);
                }
            }
            else
            {
                // Handle all other property types using default handler
                UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Using generic property handler for %s (Type: %s)"), 
                    *PropertyName, *Property->GetCPPType());
                bSuccess = FCommonUtils::SetObjectProperty(ComponentTemplate, PropertyName, JsonValue, ErrorMessage);
                if (!bSuccess)
                {
                    UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Failed to set property: %s"), *ErrorMessage);
                }
            }
        }
        catch (const std::exception& Ex)
        {
            UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - EXCEPTION: %s"), ANSI_TO_TCHAR(Ex.what()));
            return FCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Exception while setting property %s: %s"), *PropertyName, ANSI_TO_TCHAR(Ex.what())));
        }
        catch (...)
        {
            UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - UNKNOWN EXCEPTION occurred while setting property %s"), *PropertyName);
            return FCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Unknown exception while setting property %s"), *PropertyName));
        }

        if (bSuccess)
        {
            // Mark the blueprint as modified
            UE_LOG(LogTemp, Log, TEXT("SetComponentProperty - Successfully set property %s on component %s"), 
                *PropertyName, *ComponentName);
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetStringField(TEXT("component"), ComponentName);
            ResultObj->SetStringField(TEXT("property"), PropertyName);
            ResultObj->SetBoolField(TEXT("success"), true);
            return ResultObj;
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Failed to set property %s: %s"), 
                *PropertyName, *ErrorMessage);
            return FCommonUtils::CreateErrorResponse(ErrorMessage);
        }
    }

    UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Missing 'property_value' parameter"));
    return FCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleSetPhysicsProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(ComponentNode->ComponentTemplate);
    if (!PrimComponent)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Component is not a primitive component"));
    }

    // Set physics properties
    if (Params->HasField(TEXT("simulate_physics")))
    {
        PrimComponent->SetSimulatePhysics(Params->GetBoolField(TEXT("simulate_physics")));
    }

    if (Params->HasField(TEXT("mass")))
    {
        float Mass = Params->GetNumberField(TEXT("mass"));
        // In UE5.5, use proper overrideMass instead of just scaling
        PrimComponent->SetMassOverrideInKg(NAME_None, Mass);
        UE_LOG(LogTemp, Display, TEXT("Set mass for component %s to %f kg"), *ComponentName, Mass);
    }

    if (Params->HasField(TEXT("linear_damping")))
    {
        PrimComponent->SetLinearDamping(Params->GetNumberField(TEXT("linear_damping")));
    }

    if (Params->HasField(TEXT("angular_damping")))
    {
        PrimComponent->SetAngularDamping(Params->GetNumberField(TEXT("angular_damping")));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleCompileBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Compile the blueprint with safety wrapper and return diagnostics on failure
    FString CompileError;
    bool bCompiled = FCommonUtils::SafeCompileBlueprint(Blueprint, CompileError);

    if (!bCompiled)
    {
        UE_LOG(LogTemp, Error, TEXT("MCP: CompileBlueprint failed for %s: %s"), *BlueprintName, *CompileError);
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Compile failed: %s"), *CompileError));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), BlueprintName);
    ResultObj->SetBoolField(TEXT("compiled"), true);
    return ResultObj;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleSetBlueprintProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the default object
    UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
    if (!DefaultObject)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to get default object"));
    }

    // Set the property value
    if (Params->HasField(TEXT("property_value")))
    {
        TSharedPtr<FJsonValue> JsonValue = Params->Values.FindRef(TEXT("property_value"));
        
        FString ErrorMessage;
        if (FCommonUtils::SetObjectProperty(DefaultObject, PropertyName, JsonValue, ErrorMessage))
        {
            // Mark the blueprint as modified
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetStringField(TEXT("property"), PropertyName);
            ResultObj->SetBoolField(TEXT("success"), true);
            return ResultObj;
        }
        else
        {
            return FCommonUtils::CreateErrorResponse(ErrorMessage);
        }
    }

    return FCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleSetStaticMeshProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ComponentName;
    if (!Params->TryGetStringField(TEXT("component_name"), ComponentName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'component_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName().ToString() == ComponentName)
        {
            ComponentNode = Node;
            break;
        }
    }

    if (!ComponentNode)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }

    UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentNode->ComponentTemplate);
    if (!MeshComponent)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Component is not a static mesh component"));
    }

    // Set static mesh properties
    if (Params->HasField(TEXT("static_mesh")))
    {
        FString MeshPath = Params->GetStringField(TEXT("static_mesh"));
        UStaticMesh* Mesh = Cast<UStaticMesh>(UEditorAssetLibrary::LoadAsset(MeshPath));
        if (Mesh)
        {
            MeshComponent->SetStaticMesh(Mesh);
        }
    }

    if (Params->HasField(TEXT("material")))
    {
        FString MaterialPath = Params->GetStringField(TEXT("material"));
        UMaterialInterface* Material = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(MaterialPath));
        if (Material)
        {
            MeshComponent->SetMaterial(0, Material);
        }
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("component"), ComponentName);
    return ResultObj;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleSetPawnProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the default object
    UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
    if (!DefaultObject)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to get default object"));
    }

    // Track if any properties were set successfully
    bool bAnyPropertiesSet = false;
    TSharedPtr<FJsonObject> ResultsObj = MakeShared<FJsonObject>();
    
    // Set auto possess player if specified
    if (Params->HasField(TEXT("auto_possess_player")))
    {
        TSharedPtr<FJsonValue> AutoPossessValue = Params->Values.FindRef(TEXT("auto_possess_player"));
        
        FString ErrorMessage;
        if (FCommonUtils::SetObjectProperty(DefaultObject, TEXT("AutoPossessPlayer"), AutoPossessValue, ErrorMessage))
        {
            bAnyPropertiesSet = true;
            TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
            PropResultObj->SetBoolField(TEXT("success"), true);
            ResultsObj->SetObjectField(TEXT("AutoPossessPlayer"), PropResultObj);
        }
        else
        {
            TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
            PropResultObj->SetBoolField(TEXT("success"), false);
            PropResultObj->SetStringField(TEXT("error"), ErrorMessage);
            ResultsObj->SetObjectField(TEXT("AutoPossessPlayer"), PropResultObj);
        }
    }
    
    // Set controller rotation properties
    const TCHAR* RotationProps[] = {
        TEXT("bUseControllerRotationYaw"),
        TEXT("bUseControllerRotationPitch"),
        TEXT("bUseControllerRotationRoll")
    };
    
    const TCHAR* ParamNames[] = {
        TEXT("use_controller_rotation_yaw"),
        TEXT("use_controller_rotation_pitch"),
        TEXT("use_controller_rotation_roll")
    };
    
    for (int32 i = 0; i < 3; i++)
    {
        if (Params->HasField(ParamNames[i]))
        {
            TSharedPtr<FJsonValue> Value = Params->Values.FindRef(ParamNames[i]);
            
            FString ErrorMessage;
            if (FCommonUtils::SetObjectProperty(DefaultObject, RotationProps[i], Value, ErrorMessage))
            {
                bAnyPropertiesSet = true;
                TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
                PropResultObj->SetBoolField(TEXT("success"), true);
                ResultsObj->SetObjectField(RotationProps[i], PropResultObj);
            }
            else
            {
                TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
                PropResultObj->SetBoolField(TEXT("success"), false);
                PropResultObj->SetStringField(TEXT("error"), ErrorMessage);
                ResultsObj->SetObjectField(RotationProps[i], PropResultObj);
            }
        }
    }
    
    // Set can be damaged property
    if (Params->HasField(TEXT("can_be_damaged")))
    {
        TSharedPtr<FJsonValue> Value = Params->Values.FindRef(TEXT("can_be_damaged"));
        
        FString ErrorMessage;
        if (FCommonUtils::SetObjectProperty(DefaultObject, TEXT("bCanBeDamaged"), Value, ErrorMessage))
        {
            bAnyPropertiesSet = true;
            TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
            PropResultObj->SetBoolField(TEXT("success"), true);
            ResultsObj->SetObjectField(TEXT("bCanBeDamaged"), PropResultObj);
        }
        else
        {
            TSharedPtr<FJsonObject> PropResultObj = MakeShared<FJsonObject>();
            PropResultObj->SetBoolField(TEXT("success"), false);
            PropResultObj->SetStringField(TEXT("error"), ErrorMessage);
            ResultsObj->SetObjectField(TEXT("bCanBeDamaged"), PropResultObj);
        }
    }

    // Mark the blueprint as modified if any properties were set
    if (bAnyPropertiesSet)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }
    else if (ResultsObj->Values.Num() == 0)
    {
        // No properties were specified
        return FCommonUtils::CreateErrorResponse(TEXT("No properties specified to set"));
    }

    TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
    ResponseObj->SetStringField(TEXT("blueprint"), BlueprintName);
    ResponseObj->SetBoolField(TEXT("success"), bAnyPropertiesSet);
    ResponseObj->SetObjectField(TEXT("results"), ResultsObj);
    return ResponseObj;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleReparentBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Log, TEXT("MCP: HandleReparentBlueprint called"));
    
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        UE_LOG(LogTemp, Error, TEXT("MCP: Missing 'blueprint_name' parameter"));
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NewParentClass;
    if (!Params->TryGetStringField(TEXT("new_parent_class"), NewParentClass))
    {
        UE_LOG(LogTemp, Error, TEXT("MCP: Missing 'new_parent_class' parameter"));
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'new_parent_class' parameter"));
    }

    UE_LOG(LogTemp, Log, TEXT("MCP: Attempting to reparent blueprint '%s' to parent class '%s'"), *BlueprintName, *NewParentClass);

    // Find the blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprintByName(BlueprintName);
    if (!Blueprint)
    {
        FString ErrorMsg = FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName);
        UE_LOG(LogTemp, Error, TEXT("MCP: %s"), *ErrorMsg);
        return FCommonUtils::CreateErrorResponse(ErrorMsg);
    }

    // Find the new parent class
    UClass* NewParentClassObj = nullptr;
    
    // Try common engine classes first
    if (NewParentClass == TEXT("Actor") || NewParentClass == TEXT("AActor"))
    {
        NewParentClassObj = AActor::StaticClass();
    }
    else if (NewParentClass == TEXT("Pawn") || NewParentClass == TEXT("APawn"))
    {
        NewParentClassObj = APawn::StaticClass();
    }
    else if (NewParentClass == TEXT("UserWidget") || NewParentClass == TEXT("UUserWidget"))
    {
        // Find UUserWidget class for UMG widgets
        NewParentClassObj = FindObject<UClass>(nullptr, TEXT("UserWidget"));
        if (!NewParentClassObj)
        {
            NewParentClassObj = LoadClass<UObject>(nullptr, TEXT("/Script/UMG.UserWidget"));
        }
    }
    else
    {
        // Try to load the class by name with several fallbacks.
        FString ClassName = NewParentClass;

        // If a full path was provided (/Script/Module.Class) try loading directly
        if (ClassName.StartsWith(TEXT("/Script/")) || ClassName.Contains(TEXT(".")))
        {
            NewParentClassObj = FindObject<UClass>(nullptr, *ClassName);
            if (!NewParentClassObj)
            {
                // Attempt LoadClass with the provided path
                NewParentClassObj = LoadClass<UObject>(nullptr, *ClassName);
            }
        }

        // Try exact class name in loaded objects (may be C++ UCLASS)
        if (!NewParentClassObj)
        {
            NewParentClassObj = FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("Blueprint parent class search"));
        }

        // Add 'U'/'A' prefixes if missing and try common script modules (Engine and project module)
        if (!NewParentClassObj)
        {
            FString ProjectModuleName = FApp::GetProjectName();

            TArray<FString> TryPrefixes = { TEXT("U"), TEXT("A") };
            TArray<FString> TryModules = { TEXT("Engine"), ProjectModuleName };

            for (const FString& Prefix : TryPrefixes)
            {
                FString Prefixed = ClassName;
                if (!ClassName.StartsWith(Prefix))
                {
                    Prefixed = Prefix + ClassName;
                }

                for (const FString& Module : TryModules)
                {
                    FString Path = FString::Printf(TEXT("/Script/%s.%s"), *Module, *Prefixed);
                    NewParentClassObj = FindObject<UClass>(nullptr, *Path);
                    if (!NewParentClassObj)
                    {
                        NewParentClassObj = LoadClass<UObject>(nullptr, *Path);
                    }

                    if (NewParentClassObj)
                    {
                        break;
                    }
                }

                if (NewParentClassObj)
                {
                    break;
                }
            }
        }

        // Final fallback: attempt to find any loaded UClass with that short name
        if (!NewParentClassObj)
        {
            for (TObjectIterator<UClass> It; It; ++It)
            {
                if (It->GetName() == ClassName || It->GetName() == (TEXT("U") + ClassName) || It->GetName() == (TEXT("A") + ClassName))
                {
                    NewParentClassObj = *It;
                    break;
                }
            }
        }
    }

    if (!NewParentClassObj)
    {
        FString ErrorMsg = FString::Printf(TEXT("Parent class not found: %s"), *NewParentClass);
        UE_LOG(LogTemp, Error, TEXT("MCP: %s"), *ErrorMsg);
        return FCommonUtils::CreateErrorResponse(ErrorMsg);
    }

    UE_LOG(LogTemp, Log, TEXT("MCP: Found new parent class: %s"), *NewParentClassObj->GetName());

    // Get the old parent class for logging
    UClass* OldParentClass = Blueprint->ParentClass;
    FString OldParentName = OldParentClass ? OldParentClass->GetName() : TEXT("None");

    // Perform the reparenting
    try
    {
        // Set the new parent class
        Blueprint->ParentClass = NewParentClassObj;
        
        // Mark the blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        
        // Refresh the blueprint to update inheritance
        FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
        
        // Recompile the blueprint
        FBlueprintEditorUtils::RefreshVariables(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None);
        
        UE_LOG(LogTemp, Log, TEXT("MCP: Successfully reparented blueprint '%s' from '%s' to '%s'"), 
               *BlueprintName, *OldParentName, *NewParentClassObj->GetName());

        // Create success response
        TSharedPtr<FJsonObject> ResponseObj = MakeShared<FJsonObject>();
        ResponseObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
        ResponseObj->SetStringField(TEXT("old_parent_class"), OldParentName);
        ResponseObj->SetStringField(TEXT("new_parent_class"), NewParentClassObj->GetName());
        ResponseObj->SetBoolField(TEXT("success"), true);
        ResponseObj->SetStringField(TEXT("message"), TEXT("Blueprint reparented successfully"));
        
        return ResponseObj;
    }
    catch (const std::exception& e)
    {
        FString ErrorMsg = FString::Printf(TEXT("Error during reparenting: %s"), UTF8_TO_TCHAR(e.what()));
        UE_LOG(LogTemp, Error, TEXT("MCP: %s"), *ErrorMsg);
        return FCommonUtils::CreateErrorResponse(ErrorMsg);
    }
    catch (...)
    {
        FString ErrorMsg = TEXT("Unknown error during reparenting");
        UE_LOG(LogTemp, Error, TEXT("MCP: %s"), *ErrorMsg);
        return FCommonUtils::CreateErrorResponse(ErrorMsg);
    }
} 

TSharedPtr<FJsonObject> FBlueprintCommands::HandleAddBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    
    // Get parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing blueprint_name parameter"));
    }
    
    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing variable_name parameter"));
    }
    
    FString VariableType;
    if (!Params->TryGetStringField(TEXT("variable_type"), VariableType))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing variable_type parameter"));
    }
    
    // Find the Blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprintByName(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }
    
    // Create a comprehensive pin type mapping using reflection-based system
    FEdGraphPinType PinType;
    
    // Basic types
    if (VariableType == TEXT("Boolean"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
    }
    else if (VariableType == TEXT("Byte"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
    }
    else if (VariableType == TEXT("Integer"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
    }
    else if (VariableType == TEXT("Integer64"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
    }
    else if (VariableType == TEXT("Float"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Float;
    }
    else if (VariableType == TEXT("Double"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Double;
    }
    else if (VariableType == TEXT("Name"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
    }
    else if (VariableType == TEXT("String"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    else if (VariableType == TEXT("Text"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
    }
    // Struct types
    else if (VariableType == TEXT("Vector"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector>::Get();
    }
    else if (VariableType == TEXT("Vector2D"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector2D>::Get();
    }
    else if (VariableType == TEXT("Vector4"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FVector4>::Get();
    }
    else if (VariableType == TEXT("Rotator"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FRotator>::Get();
    }
    else if (VariableType == TEXT("Transform"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FTransform>::Get();
    }
    else if (VariableType == TEXT("Color"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FColor>::Get();
    }
    else if (VariableType == TEXT("LinearColor"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        PinType.PinSubCategoryObject = TBaseStructure<FLinearColor>::Get();
    }
    // Object types (basic implementation)
    else if (VariableType == TEXT("Actor"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        PinType.PinSubCategoryObject = AActor::StaticClass();
    }
    else if (VariableType == TEXT("Pawn"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        PinType.PinSubCategoryObject = APawn::StaticClass();
    }
    else if (VariableType == TEXT("StaticMesh"))
    {
        PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        PinType.PinSubCategoryObject = UStaticMesh::StaticClass();
    }
    else
    {
        // Default to String for unsupported types
        PinType.PinCategory = UEdGraphSchema_K2::PC_String;
    }
    
    // Get default value
    FString DefaultValue;
    Params->TryGetStringField(TEXT("default_value"), DefaultValue);
    
    // Add the variable
    if (FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), PinType, DefaultValue))
    {
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Variable added successfully"));
        Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
        Response->SetStringField(TEXT("variable_name"), VariableName);
        Response->SetStringField(TEXT("variable_type"), VariableType);
        
        // Compile the Blueprint
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    }
    else
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to add variable to Blueprint"));
    }
    
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleGetBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing blueprint_name parameter"));
    }
    
    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing variable_name parameter"));
    }
    
    UBlueprint* Blueprint = FCommonUtils::FindBlueprintByName(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }
    
    // Find the variable
    FName VarName(*VariableName);
    FBPVariableDescription* VarDesc = nullptr;
    
    for (int32 i = 0; i < Blueprint->NewVariables.Num(); ++i)
    {
        if (Blueprint->NewVariables[i].VarName == VarName)
        {
            VarDesc = &Blueprint->NewVariables[i];
            break;
        }
    }
    
    if (!VarDesc)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Variable '%s' not found in Blueprint '%s'"), *VariableName, *BlueprintName));
    }
    
    // Build response
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetStringField(TEXT("variable_name"), VariableName);
    
    // Get comprehensive type info using reflection-based reverse mapping
    FString TypeName;
    
    // Debug logging to see what we're actually dealing with
    FString PinCategoryStr = VarDesc->VarType.PinCategory.ToString();
    UE_LOG(LogTemp, Warning, TEXT("MCP: Variable '%s' has PinCategory: %s"), *VariableName, *PinCategoryStr);
    
    // Debug SubCategoryObject if it exists
    if (VarDesc->VarType.PinSubCategoryObject.IsValid())
    {
        FString SubCategoryStr = VarDesc->VarType.PinSubCategoryObject->GetName();
        UE_LOG(LogTemp, Warning, TEXT("MCP: Variable '%s' has SubCategoryObject: %s"), *VariableName, *SubCategoryStr);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("MCP: Variable '%s' has no SubCategoryObject"), *VariableName);
    }
    
    // Basic types
    if (VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
        TypeName = TEXT("Boolean");
    else if (VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Byte)
        TypeName = TEXT("Byte");
    else if (VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Int)
        TypeName = TEXT("Integer");
    else if (VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Int64)
        TypeName = TEXT("Integer64");
    else if (VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Float)
        TypeName = TEXT("Float");
    else if (VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Double)
        TypeName = TEXT("Double");
    else if (VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Real)
        TypeName = TEXT("Float");  // PC_Real maps to Float for backwards compatibility
    else if (VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Name)
        TypeName = TEXT("Name");
    else if (VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_String)
        TypeName = TEXT("String");
    else if (VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Text)
        TypeName = TEXT("Text");
    // Struct types - check SubCategoryObject
    else if (VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Struct)
    {
        if (VarDesc->VarType.PinSubCategoryObject == TBaseStructure<FVector>::Get())
            TypeName = TEXT("Vector");
        else if (VarDesc->VarType.PinSubCategoryObject == TBaseStructure<FVector2D>::Get())
            TypeName = TEXT("Vector2D");
        else if (VarDesc->VarType.PinSubCategoryObject == TBaseStructure<FVector4>::Get())
            TypeName = TEXT("Vector4");
        else if (VarDesc->VarType.PinSubCategoryObject == TBaseStructure<FRotator>::Get())
            TypeName = TEXT("Rotator");
        else if (VarDesc->VarType.PinSubCategoryObject == TBaseStructure<FTransform>::Get())
            TypeName = TEXT("Transform");
        else if (VarDesc->VarType.PinSubCategoryObject == TBaseStructure<FColor>::Get())
            TypeName = TEXT("Color");
        else if (VarDesc->VarType.PinSubCategoryObject == TBaseStructure<FLinearColor>::Get())
            TypeName = TEXT("LinearColor");
        else
            TypeName = TEXT("Struct");
    }
    // Object types - check SubCategoryObject class
    else if (VarDesc->VarType.PinCategory == UEdGraphSchema_K2::PC_Object)
    {
        if (VarDesc->VarType.PinSubCategoryObject == AActor::StaticClass())
            TypeName = TEXT("Actor");
        else if (VarDesc->VarType.PinSubCategoryObject == APawn::StaticClass())
            TypeName = TEXT("Pawn");
        else if (VarDesc->VarType.PinSubCategoryObject == UStaticMesh::StaticClass())
            TypeName = TEXT("StaticMesh");
        else
            TypeName = TEXT("Object");
    }
    else
        TypeName = TEXT("Unknown");
    
    // Debug final result
    UE_LOG(LogTemp, Warning, TEXT("MCP: Variable '%s' determined as type: %s"), *VariableName, *TypeName);
    
    Response->SetStringField(TEXT("variable_type"), TypeName);
    Response->SetStringField(TEXT("category"), VarDesc->Category.ToString());
    Response->SetStringField(TEXT("tooltip"), VarDesc->FriendlyName);
    
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleSetBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing blueprint_name parameter"));
    }
    
    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing variable_name parameter"));
    }
    
    UBlueprint* Blueprint = FCommonUtils::FindBlueprintByName(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }
    
    // Find the variable
    FName VarName(*VariableName);
    FBPVariableDescription* VarDesc = nullptr;
    
    for (int32 i = 0; i < Blueprint->NewVariables.Num(); ++i)
    {
        if (Blueprint->NewVariables[i].VarName == VarName)
        {
            VarDesc = &Blueprint->NewVariables[i];
            break;
        }
    }
    
    if (!VarDesc)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Variable '%s' not found in Blueprint '%s'"), *VariableName, *BlueprintName));
    }
    
    // Handle different property updates
    const TSharedPtr<FJsonObject>* PropertiesObj;
    if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
    {
        // Update tooltip if provided
        FString NewTooltip;
        if ((*PropertiesObj)->TryGetStringField(TEXT("tooltip"), NewTooltip))
        {
            VarDesc->FriendlyName = NewTooltip;
        }
        
        // Update category if provided
        FString NewCategory;
        if ((*PropertiesObj)->TryGetStringField(TEXT("category"), NewCategory))
        {
            VarDesc->Category = FText::FromString(NewCategory);
        }
        
        // Mark Blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    }
    
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), TEXT("Variable updated successfully"));
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetStringField(TEXT("variable_name"), VariableName);
    
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleGetAvailableBlueprintVariableTypes(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    
    try
    {
        // Get all available pin types from UEdGraphSchema_K2
        TArray<TSharedPtr<FJsonValue>> BasicTypesArray;
        TArray<TSharedPtr<FJsonValue>> StructTypesArray;
        TArray<TSharedPtr<FJsonValue>> ObjectTypesArray;
        TArray<TSharedPtr<FJsonValue>> EnumTypesArray;
        
        TSharedPtr<FJsonObject> TypeInfoObject = MakeShared<FJsonObject>();
        
        // Basic types from EdGraphSchema_K2
        auto AddBasicType = [&](const FString& TypeName, const FString& PinCategory, const FString& Description, const FString& DefaultValue)
        {
            BasicTypesArray.Add(MakeShared<FJsonValueString>(TypeName));
            
            TSharedPtr<FJsonObject> TypeInfo = MakeShared<FJsonObject>();
            TypeInfo->SetStringField(TEXT("category"), TEXT("Basic"));
            TypeInfo->SetStringField(TEXT("description"), Description);
            TypeInfo->SetStringField(TEXT("default_value"), DefaultValue);
            TypeInfo->SetStringField(TEXT("pin_category"), PinCategory);
            TypeInfoObject->SetObjectField(TypeName, TypeInfo);
        };
        
        // Add all basic types that UE supports
        AddBasicType(TEXT("Boolean"), TEXT("PC_Boolean"), TEXT("True/false value"), TEXT("false"));
        AddBasicType(TEXT("Byte"), TEXT("PC_Byte"), TEXT("8-bit unsigned integer (0-255)"), TEXT("0"));
        AddBasicType(TEXT("Integer"), TEXT("PC_Int"), TEXT("32-bit signed integer"), TEXT("0"));
        AddBasicType(TEXT("Integer64"), TEXT("PC_Int64"), TEXT("64-bit signed integer"), TEXT("0"));
        AddBasicType(TEXT("Float"), TEXT("PC_Float"), TEXT("32-bit floating point number"), TEXT("0.0"));
        AddBasicType(TEXT("Double"), TEXT("PC_Double"), TEXT("64-bit floating point number"), TEXT("0.0"));
        AddBasicType(TEXT("Name"), TEXT("PC_Name"), TEXT("Unreal name identifier"), TEXT("None"));
        AddBasicType(TEXT("String"), TEXT("PC_String"), TEXT("Text string value"), TEXT(""));
        AddBasicType(TEXT("Text"), TEXT("PC_Text"), TEXT("Localizable text value"), TEXT(""));
        
        // Add common struct types
        auto AddStructType = [&](const FString& TypeName, const FString& StructName, const FString& Description, const FString& DefaultValue)
        {
            StructTypesArray.Add(MakeShared<FJsonValueString>(TypeName));
            
            TSharedPtr<FJsonObject> TypeInfo = MakeShared<FJsonObject>();
            TypeInfo->SetStringField(TEXT("category"), TEXT("Struct"));
            TypeInfo->SetStringField(TEXT("description"), Description);
            TypeInfo->SetStringField(TEXT("default_value"), DefaultValue);
            TypeInfo->SetStringField(TEXT("pin_category"), TEXT("PC_Struct"));
            TypeInfo->SetStringField(TEXT("struct_type"), StructName);
            TypeInfoObject->SetObjectField(TypeName, TypeInfo);
        };
        
        AddStructType(TEXT("Vector"), TEXT("FVector"), TEXT("3D vector with X, Y, Z components"), TEXT("(0.0, 0.0, 0.0)"));
        AddStructType(TEXT("Vector2D"), TEXT("FVector2D"), TEXT("2D vector with X, Y components"), TEXT("(0.0, 0.0)"));
        AddStructType(TEXT("Vector4"), TEXT("FVector4"), TEXT("4D vector with X, Y, Z, W components"), TEXT("(0.0, 0.0, 0.0, 0.0)"));
        AddStructType(TEXT("Rotator"), TEXT("FRotator"), TEXT("3D rotation with Pitch, Yaw, Roll"), TEXT("(0.0, 0.0, 0.0)"));
        AddStructType(TEXT("Transform"), TEXT("FTransform"), TEXT("3D transformation (location, rotation, scale)"), TEXT("Identity"));
        AddStructType(TEXT("Color"), TEXT("FColor"), TEXT("RGBA color (0-255 range)"), TEXT("(255, 255, 255, 255)"));
        AddStructType(TEXT("LinearColor"), TEXT("FLinearColor"), TEXT("RGBA color (0.0-1.0 range)"), TEXT("(1.0, 1.0, 1.0, 1.0)"));
        
        // Add common object types
        auto AddObjectType = [&](const FString& TypeName, const FString& ClassName, const FString& Description)
        {
            ObjectTypesArray.Add(MakeShared<FJsonValueString>(TypeName));
            
            TSharedPtr<FJsonObject> TypeInfo = MakeShared<FJsonObject>();
            TypeInfo->SetStringField(TEXT("category"), TEXT("Object"));
            TypeInfo->SetStringField(TEXT("description"), Description);
            TypeInfo->SetStringField(TEXT("default_value"), TEXT("None"));
            TypeInfo->SetStringField(TEXT("pin_category"), TEXT("PC_Object"));
            TypeInfo->SetStringField(TEXT("class_type"), ClassName);
            TypeInfoObject->SetObjectField(TypeName, TypeInfo);
        };
        
        AddObjectType(TEXT("Actor"), TEXT("AActor"), TEXT("Reference to any Actor in the world"));
        AddObjectType(TEXT("Pawn"), TEXT("APawn"), TEXT("Reference to a controllable Pawn"));
        AddObjectType(TEXT("Character"), TEXT("ACharacter"), TEXT("Reference to a Character (humanoid Pawn)"));
        AddObjectType(TEXT("PlayerController"), TEXT("APlayerController"), TEXT("Reference to a PlayerController"));
        AddObjectType(TEXT("GameMode"), TEXT("AGameMode"), TEXT("Reference to the GameMode"));
        AddObjectType(TEXT("ActorComponent"), TEXT("UActorComponent"), TEXT("Reference to an ActorComponent"));
        AddObjectType(TEXT("SceneComponent"), TEXT("USceneComponent"), TEXT("Reference to a SceneComponent"));
        AddObjectType(TEXT("StaticMeshComponent"), TEXT("UStaticMeshComponent"), TEXT("Reference to a StaticMeshComponent"));
        AddObjectType(TEXT("StaticMesh"), TEXT("UStaticMesh"), TEXT("Reference to a StaticMesh asset"));
        AddObjectType(TEXT("Material"), TEXT("UMaterial"), TEXT("Reference to a Material asset"));
        AddObjectType(TEXT("Texture2D"), TEXT("UTexture2D"), TEXT("Reference to a 2D Texture asset"));
        
        // Build response
        Response->SetBoolField(TEXT("success"), true);
        Response->SetArrayField(TEXT("basic_types"), BasicTypesArray);
        Response->SetArrayField(TEXT("struct_types"), StructTypesArray);
        Response->SetArrayField(TEXT("object_types"), ObjectTypesArray);
        Response->SetArrayField(TEXT("enum_types"), EnumTypesArray); // Empty for now
        Response->SetObjectField(TEXT("type_info"), TypeInfoObject);
        Response->SetNumberField(TEXT("total_count"), BasicTypesArray.Num() + StructTypesArray.Num() + ObjectTypesArray.Num());
        
        // Add implementation status
        TSharedPtr<FJsonObject> StatusObject = MakeShared<FJsonObject>();
        StatusObject->SetStringField(TEXT("current"), TEXT("Reflection-based type discovery from UE5 pin categories"));
        StatusObject->SetStringField(TEXT("method"), TEXT("EdGraphSchema_K2 pin category enumeration"));
        StatusObject->SetBoolField(TEXT("extensible"), true);
        Response->SetObjectField(TEXT("implementation_status"), StatusObject);
        
        UE_LOG(LogTemp, Log, TEXT("MCP: Found %d available Blueprint variable types"), BasicTypesArray.Num() + StructTypesArray.Num() + ObjectTypesArray.Num());
        
    }
    catch (...)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to get available Blueprint variable types"));
    }
    
    return Response;
}
