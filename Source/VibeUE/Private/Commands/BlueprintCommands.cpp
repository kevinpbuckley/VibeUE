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
#include "Dom/JsonValue.h" 
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/UserDefinedStruct.h"
#include "Engine/UserDefinedEnum.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "Engine/StaticMesh.h"
#include "UObject/UnrealType.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Math/Color.h"

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
    else if (CommandType == TEXT("compile_blueprint"))
    {
        return HandleCompileBlueprint(Params);
    }
    else if (CommandType == TEXT("set_blueprint_property"))
    {
        return HandleSetBlueprintProperty(Params);
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
    else if (CommandType == TEXT("delete_blueprint_variable"))
    {
        return HandleDeleteBlueprintVariable(Params);
    }
    else if (CommandType == TEXT("get_available_blueprint_variable_types"))
    {
        return HandleGetAvailableBlueprintVariableTypes(Params);
    }
    else if (CommandType == TEXT("get_variable_property"))
    {
        return HandleGetVariableProperty(Params);
    }
    else if (CommandType == TEXT("set_variable_property"))
    {
        return HandleSetVariableProperty(Params);
    }
    else if (CommandType == TEXT("get_blueprint_variable_metadata"))
    {
        return HandleGetBlueprintVariableMetadata(Params);
    }
    else if (CommandType == TEXT("set_blueprint_variable_metadata"))
    {
        return HandleSetBlueprintVariableMetadata(Params);
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
        }
        else
        {
            /* cleanup: keep defaulting silently when class not found */
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
        
    
    }
    else
    {
    
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
        
    }

    // Find the component
    USCS_Node* ComponentNode = nullptr;
    
    
    if (!Blueprint->SimpleConstructionScript)
    {
    UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - SimpleConstructionScript is NULL for blueprint %s"), *BlueprintName);
        return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint construction script"));
    }
    
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node)
        {
            
            if (Node->GetVariableName().ToString() == ComponentName)
            {
                ComponentNode = Node;
                break;
            }
        }
        else
        {
            
        }
    }

    if (!ComponentNode)
    {
    UE_LOG(LogTemp, Error, TEXT("SetComponentProperty - Component not found: %s"), *ComponentName);
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Component not found: %s"), *ComponentName));
    }
    else
    {
        
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
        
            
        // Log all properties of the SpringArm component class
    
        for (TFieldIterator<FProperty> PropIt(ComponentTemplate->GetClass()); PropIt; ++PropIt)
        {
            FProperty* Prop = *PropIt;
            
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
                    
                    FloatProp->SetPropertyValue_InContainer(ComponentTemplate, Value);
                    bSuccess = true;
                }
            }
            else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
            {
                if (JsonValue->Type == EJson::Boolean)
                {
                    const bool Value = JsonValue->AsBool();
                    
                    BoolProp->SetPropertyValue_InContainer(ComponentTemplate, Value);
                    bSuccess = true;
                }
            }
            else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
            {
                
                
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
            
            for (TFieldIterator<FProperty> PropIt(ComponentTemplate->GetClass()); PropIt; ++PropIt)
            {
                FProperty* Prop = *PropIt;
                
            }
            
            return FCommonUtils::CreateErrorResponse(
                FString::Printf(TEXT("Property %s not found on component %s"), *PropertyName, *ComponentName));
        }
        else
        {
            
        }

        bool bSuccess = false;
        FString ErrorMessage;

        // Handle different property types
    
        
        // Add try-catch block to catch and log any crashes
        try
        {
            if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
            {
                // Handle vector properties
                
                    
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
                
                if (JsonValue->Type == EJson::String)
                {
                    FString EnumValueName = JsonValue->AsString();
                    UEnum* Enum = EnumProp->GetEnum();
                    
                    
                    if (Enum)
                    {
                        int64 EnumValue = Enum->GetValueByNameString(EnumValueName);
                        
                        if (EnumValue != INDEX_NONE)
                        {
                            
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
    
    // Get is_exposed parameter
    bool bIsExposed = false;
    Params->TryGetBoolField(TEXT("is_exposed"), bIsExposed);
    
    // Add the variable
    if (FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), PinType, DefaultValue))
    {
        // Set the Instance Editable flag if specified
        if (bIsExposed)
        {
            FName VarName(*VariableName);
            for (FBPVariableDescription& Variable : Blueprint->NewVariables)
            {
                if (Variable.VarName == VarName)
                {
                    Variable.PropertyFlags |= CPF_Edit;
                    Variable.PropertyFlags |= CPF_BlueprintVisible;
                    Variable.RepNotifyFunc = FName(); // Clear rep notify
                    break;
                }
            }
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("Variable added successfully"));
        Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
        Response->SetStringField(TEXT("variable_name"), VariableName);
        Response->SetStringField(TEXT("variable_type"), VariableType);
        Response->SetBoolField(TEXT("is_exposed"), bIsExposed);
        
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

TSharedPtr<FJsonObject> FBlueprintCommands::HandleDeleteBlueprintVariable(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    FString VariableName;
    bool ForceDelete = false;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing blueprint_name parameter"));
    }
    
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing variable_name parameter"));
    }
    
    // Optional force_delete parameter
    Params->TryGetBoolField(TEXT("force_delete"), ForceDelete);
    
    UBlueprint* Blueprint = FCommonUtils::FindBlueprintByName(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }
    
    FName VarName(*VariableName);
    FBPVariableDescription* VarDesc = nullptr;
    int32 VarIndex = -1;
    
    // Find the variable in the Blueprint's variable list
    for (int32 i = 0; i < Blueprint->NewVariables.Num(); ++i)
    {
        if (Blueprint->NewVariables[i].VarName == VarName)
        {
            VarDesc = &Blueprint->NewVariables[i];
            VarIndex = i;
            break;
        }
    }
    
    if (!VarDesc)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Variable '%s' not found in Blueprint '%s'"), *VariableName, *BlueprintName));
    }
    
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> References;
    TArray<TSharedPtr<FJsonValue>> CleanupPerformed;
    
    // TODO: Add reference detection by scanning Blueprint graphs
    // For now, we'll implement basic deletion without reference checking
    // Future enhancement: Scan all graphs for variable usage nodes
    
    // Check if force_delete is needed (simplified check for now)
    bool ReferencesFound = false; // Will be enhanced with actual reference detection
    
    if (ReferencesFound && !ForceDelete)
    {
        // Return error with references found
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Variable '%s' has references. Use force_delete=true to remove automatically."), *VariableName));
        Response->SetArrayField(TEXT("references"), References);
        Response->SetStringField(TEXT("suggestion"), TEXT("Use force_delete=true to remove all references automatically"));
        return Response;
    }
    
    // Remove the variable from the Blueprint
    Blueprint->NewVariables.RemoveAt(VarIndex);
    
    // Mark Blueprint as dirty and recompile
    Blueprint->MarkPackageDirty();
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    
    // Track cleanup actions
    TSharedPtr<FJsonObject> CleanupInfo = MakeShared<FJsonObject>();
    CleanupInfo->SetStringField(TEXT("action"), TEXT("variable_removed"));
    CleanupInfo->SetStringField(TEXT("variable_name"), VariableName);
    CleanupInfo->SetStringField(TEXT("variable_type"), VarDesc->VarType.PinCategory.ToString());
    CleanupPerformed.Add(MakeShared<FJsonValueObject>(CleanupInfo));
    
    // Build success response
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("variable_name"), VariableName);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetArrayField(TEXT("references"), References);
    Response->SetBoolField(TEXT("force_used"), ForceDelete);
    Response->SetArrayField(TEXT("cleanup_performed"), CleanupPerformed);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Variable '%s' deleted successfully from Blueprint '%s'"), *VariableName, *BlueprintName));
    
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

// --- Reflection-based variable property access ---
namespace
{
    static bool SplitVarPath(const FString& In, FString& OutVar, TArray<FString>& OutSegs)
    {
        OutVar.Empty();
        OutSegs.Reset();
        TArray<FString> Parts; In.ParseIntoArray(Parts, TEXT("."), true);
        if (Parts.Num() == 0) return false;
        OutVar = Parts[0];
        for (int32 i=1;i<Parts.Num();++i) OutSegs.Add(Parts[i]);
        return true;
    }

    struct FResolvedVarProp { FProperty* Prop=nullptr; void* Ptr=nullptr; FString Canonical; };

    static bool ResolveOnCDO(UObject* CDO, const FString& Var, const TArray<FString>& Segs, FResolvedVarProp& Out)
    {
        if (!CDO) return false;
        FProperty* P = CDO->GetClass()->FindPropertyByName(*Var);
        if (!P) return false;
        void* Cur = P->ContainerPtrToValuePtr<void>(CDO);
        FString Canonical = Var;
        for (const FString& Seg : Segs)
        {
            if (FArrayProperty* AP = CastField<FArrayProperty>(P))
            {
                FScriptArrayHelper H(AP, Cur);
                int32 Idx = FCString::Atoi(*Seg);
                if (!H.IsValidIndex(Idx)) return false;
                Cur = H.GetRawPtr(Idx);
                P = AP->Inner;
                Canonical += FString::Printf(TEXT(".%d"), Idx);
                continue;
            }
            if (FMapProperty* MP = CastField<FMapProperty>(P))
            {
                FScriptMapHelper H(MP, Cur);
                // Only support string/int/name/enum keys via text coercion
                TArray<uint8> Key; Key.SetNumUninitialized(MP->KeyProp->GetSize());
                MP->KeyProp->InitializeValue(Key.GetData());
                bool bOk = false;
                if (FStrProperty* SP = CastField<FStrProperty>(MP->KeyProp)) { FString S=Seg; SP->CopyCompleteValue(Key.GetData(), &S); bOk=true; }
                else if (FNameProperty* NP = CastField<FNameProperty>(MP->KeyProp)) { FName N(*Seg); NP->CopyCompleteValue(Key.GetData(), &N); bOk=true; }
                else if (FIntProperty* IP = CastField<FIntProperty>(MP->KeyProp)) { int32 V=FCString::Atoi(*Seg); IP->CopyCompleteValue(Key.GetData(), &V); bOk=true; }
                else if (FByteProperty* BP = CastField<FByteProperty>(MP->KeyProp)) { uint8 V=0; if (UEnum* E=BP->Enum){int64 EV=E->GetValueByNameString(Seg); V=(EV==INDEX_NONE)?(uint8)FCString::Atoi(*Seg):(uint8)EV;} else {V=(uint8)FCString::Atoi(*Seg);} BP->CopyCompleteValue(Key.GetData(), &V); bOk=true; }
                if (!bOk) { MP->KeyProp->DestroyValue(Key.GetData()); return false; }
                int32 PairIdx = INDEX_NONE;
                for (int32 It=0; It<H.GetMaxIndex(); ++It) { if (!H.IsValidIndex(It)) continue; uint8* Pair=(uint8*)H.GetPairPtr(It); if (MP->KeyProp->Identical(Pair, Key.GetData())) { PairIdx=It; break; } }
                MP->KeyProp->DestroyValue(Key.GetData());
                if (PairIdx==INDEX_NONE) return false;
                Cur = H.GetPairPtr(PairIdx) + MP->MapLayout.ValueOffset;
                P = MP->ValueProp;
                Canonical += TEXT(".") + Seg;
                continue;
            }
            if (FSetProperty* SP = CastField<FSetProperty>(P))
            {
                // Expose whole set only; traversal into elements unsupported here
                return false;
            }
            if (FStructProperty* StP = CastField<FStructProperty>(P))
            {
                FProperty* Inner = StP->Struct->FindPropertyByName(*Seg);
                if (!Inner) return false;
                Cur = Inner->ContainerPtrToValuePtr<void>(Cur);
                P = Inner;
                Canonical += TEXT(".") + Seg;
                continue;
            }
            // not traversable further
            return false;
        }
        Out.Prop = P; Out.Ptr = Cur; Out.Canonical = Canonical; return true;
    }

    // --- JSON Serialization Helpers ---
    static FString EnumToString(FProperty* Prop, void* Ptr)
    {
        if (FEnumProperty* EP = CastField<FEnumProperty>(Prop))
        {
            UEnum* E = EP->GetEnum();
            int64 V = EP->GetUnderlyingProperty()->GetSignedIntPropertyValue(Ptr);
            return E ? E->GetNameStringByValue(V) : FString::FromInt((int32)V);
        }
        if (FByteProperty* BP = CastField<FByteProperty>(Prop))
        {
            if (UEnum* E = BP->Enum)
            {
                uint8 V = *(uint8*)Ptr;
                return E->GetNameStringByValue(V);
            }
        }
        return TEXT("");
    }

    static FString KeyToString(FProperty* KeyProp, void* KeyPtr)
    {
        if (FStrProperty* SP = CastField<FStrProperty>(KeyProp)) return SP->GetPropertyValue(KeyPtr);
        if (FNameProperty* NP = CastField<FNameProperty>(KeyProp)) return NP->GetPropertyValue(KeyPtr).ToString();
        if (FIntProperty* IP = CastField<FIntProperty>(KeyProp)) return FString::FromInt(IP->GetPropertyValue(KeyPtr));
        if (FByteProperty* BP = CastField<FByteProperty>(KeyProp))
        {
            if (BP->Enum) return EnumToString(KeyProp, KeyPtr);
            return FString::FromInt((int32)*(uint8*)KeyPtr);
        }
        if (FEnumProperty* EP = CastField<FEnumProperty>(KeyProp)) return EnumToString(KeyProp, KeyPtr);
        return TEXT("");
    }

    static TSharedPtr<FJsonValue> SerializeProperty(FProperty* Prop, void* Ptr);

    static TSharedPtr<FJsonValue> SerializeStruct(FStructProperty* SP, void* Ptr)
    {
        UScriptStruct* SS = SP->Struct;
        // Common structs with compact representations
        if (SS->GetFName() == NAME_Vector)
        {
            FVector V = *(FVector*)Ptr;
            TArray<TSharedPtr<FJsonValue>> Arr{ MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z) };
            return MakeShared<FJsonValueArray>(Arr);
        }
        if (SS->GetFName() == NAME_Vector2D)
        {
            FVector2D V = *(FVector2D*)Ptr;
            TArray<TSharedPtr<FJsonValue>> Arr{ MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y) };
            return MakeShared<FJsonValueArray>(Arr);
        }
        if (SS->GetFName() == NAME_Vector4)
        {
            FVector4d V = *(FVector4d*)Ptr;
            TArray<TSharedPtr<FJsonValue>> Arr{ MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z), MakeShared<FJsonValueNumber>(V.W) };
            return MakeShared<FJsonValueArray>(Arr);
        }
        if (SS->GetFName() == NAME_Rotator)
        {
            FRotator R = *(FRotator*)Ptr;
            TArray<TSharedPtr<FJsonValue>> Arr{ MakeShared<FJsonValueNumber>(R.Pitch), MakeShared<FJsonValueNumber>(R.Yaw), MakeShared<FJsonValueNumber>(R.Roll) };
            return MakeShared<FJsonValueArray>(Arr);
        }
        if (SS->GetFName() == NAME_Transform)
        {
            FTransform T = *(FTransform*)Ptr;
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            FVector L = T.GetLocation(); FRotator R = T.Rotator(); FVector S = T.GetScale3D();
            Obj->SetArrayField(TEXT("location"), { MakeShared<FJsonValueNumber>(L.X), MakeShared<FJsonValueNumber>(L.Y), MakeShared<FJsonValueNumber>(L.Z) });
            Obj->SetArrayField(TEXT("rotation"), { MakeShared<FJsonValueNumber>(R.Pitch), MakeShared<FJsonValueNumber>(R.Yaw), MakeShared<FJsonValueNumber>(R.Roll) });
            Obj->SetArrayField(TEXT("scale"), { MakeShared<FJsonValueNumber>(S.X), MakeShared<FJsonValueNumber>(S.Y) , MakeShared<FJsonValueNumber>(S.Z) });
            return MakeShared<FJsonValueObject>(Obj);
        }
        if (SS->GetFName() == NAME_Color)
        {
            FColor C = *(FColor*)Ptr;
            TArray<TSharedPtr<FJsonValue>> Arr{ MakeShared<FJsonValueNumber>((double)C.R), MakeShared<FJsonValueNumber>((double)C.G), MakeShared<FJsonValueNumber>((double)C.B), MakeShared<FJsonValueNumber>((double)C.A) };
            return MakeShared<FJsonValueArray>(Arr);
        }
        if (SS->GetFName() == NAME_LinearColor)
        {
            FLinearColor C = *(FLinearColor*)Ptr;
            TArray<TSharedPtr<FJsonValue>> Arr{ MakeShared<FJsonValueNumber>(C.R), MakeShared<FJsonValueNumber>(C.G), MakeShared<FJsonValueNumber>(C.B), MakeShared<FJsonValueNumber>(C.A) };
            return MakeShared<FJsonValueArray>(Arr);
        }
        // Generic struct to object
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        for (TFieldIterator<FProperty> It(SS); It; ++It)
        {
            FProperty* Inner = *It;
            void* InnerPtr = (uint8*)Ptr + Inner->GetOffset_ForInternal();
            Obj->SetField(Inner->GetName(), SerializeProperty(Inner, InnerPtr));
        }
        return MakeShared<FJsonValueObject>(Obj);
    }

    static TSharedPtr<FJsonValue> SerializeProperty(FProperty* Prop, void* Ptr)
    {
        if (FIntProperty* IP = CastField<FIntProperty>(Prop)) return MakeShared<FJsonValueNumber>((double)IP->GetPropertyValue(Ptr));
        if (FFloatProperty* FP = CastField<FFloatProperty>(Prop)) return MakeShared<FJsonValueNumber>((double)FP->GetFloatingPointPropertyValue(Ptr));
        if (FDoubleProperty* DP = CastField<FDoubleProperty>(Prop)) return MakeShared<FJsonValueNumber>(DP->GetFloatingPointPropertyValue(Ptr));
        if (FBoolProperty* BP = CastField<FBoolProperty>(Prop)) return MakeShared<FJsonValueBoolean>(BP->GetPropertyValue(Ptr));
        if (FStrProperty* SP = CastField<FStrProperty>(Prop)) return MakeShared<FJsonValueString>(SP->GetPropertyValue(Ptr));
        if (FNameProperty* NP = CastField<FNameProperty>(Prop)) return MakeShared<FJsonValueString>(NP->GetPropertyValue(Ptr).ToString());
        if (FTextProperty* TP = CastField<FTextProperty>(Prop)) return MakeShared<FJsonValueString>(TP->GetPropertyValue(Ptr).ToString());
        if (FEnumProperty* EP = CastField<FEnumProperty>(Prop)) return MakeShared<FJsonValueString>(EnumToString(Prop, Ptr));
        if (FByteProperty* BP2 = CastField<FByteProperty>(Prop))
        {
            if (BP2->Enum) return MakeShared<FJsonValueString>(EnumToString(Prop, Ptr));
            return MakeShared<FJsonValueNumber>((double)*(uint8*)Ptr);
        }
        if (FArrayProperty* AP = CastField<FArrayProperty>(Prop))
        {
            FScriptArrayHelper H(AP, Ptr);
            TArray<TSharedPtr<FJsonValue>> Arr;
            for (int32 i=0;i<H.Num();++i) Arr.Add(SerializeProperty(AP->Inner, H.GetRawPtr(i)));
            return MakeShared<FJsonValueArray>(Arr);
        }
        if (FMapProperty* MP = CastField<FMapProperty>(Prop))
        {
            FScriptMapHelper H(MP, Ptr);
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            for (int32 It=0; It<H.GetMaxIndex(); ++It)
            {
                if (!H.IsValidIndex(It)) continue;
                uint8* Pair = (uint8*)H.GetPairPtr(It);
                void* KeyPtr = Pair; void* ValPtr = Pair + MP->MapLayout.ValueOffset;
                FString K = KeyToString(MP->KeyProp, KeyPtr);
                Obj->SetField(K, SerializeProperty(MP->ValueProp, ValPtr));
            }
            return MakeShared<FJsonValueObject>(Obj);
        }
        if (FSetProperty* SetP = CastField<FSetProperty>(Prop))
        {
            FScriptSetHelper H(SetP, Ptr);
            TArray<TSharedPtr<FJsonValue>> Arr;
            for (int32 It=0; It<H.GetMaxIndex(); ++It)
            {
                if (!H.IsValidIndex(It)) continue;
                Arr.Add(SerializeProperty(SetP->ElementProp, H.GetElementPtr(It)));
            }
            return MakeShared<FJsonValueArray>(Arr);
        }
        if (FStructProperty* StP = CastField<FStructProperty>(Prop))
        {
            return SerializeStruct(StP, Ptr);
        }
        // unsupported object refs for variable CDOs return string path if non-null
        if (FObjectProperty* OP = CastField<FObjectProperty>(Prop))
        {
            UObject* const* ObjPtr = (UObject* const*)Ptr;
            return MakeShared<FJsonValueString>((*ObjPtr) ? (*ObjPtr)->GetPathName() : FString("None"));
        }
        return MakeShared<FJsonValueNull>();
    }

    static bool ApplyJsonToProperty(const TSharedPtr<FJsonValue>& J, FProperty* Prop, void* Ptr);

    static bool ApplyJsonToStruct(const TSharedPtr<FJsonValue>& J, FStructProperty* SP, void* Ptr)
    {
        UScriptStruct* SS = SP->Struct;
        if (SS->GetFName() == NAME_Vector)
        {
            const TArray<TSharedPtr<FJsonValue>>* A; if (!J->TryGetArray(A) || A->Num()<3) return false;
            FVector V((float)(*A)[0]->AsNumber(), (float)(*A)[1]->AsNumber(), (float)(*A)[2]->AsNumber());
            *(FVector*)Ptr = V; return true;
        }
        if (SS->GetFName() == NAME_Vector2D)
        {
            const TArray<TSharedPtr<FJsonValue>>* A; if (!J->TryGetArray(A) || A->Num()<2) return false;
            FVector2D V((float)(*A)[0]->AsNumber(), (float)(*A)[1]->AsNumber());
            *(FVector2D*)Ptr = V; return true;
        }
        if (SS->GetFName() == NAME_Vector4)
        {
            const TArray<TSharedPtr<FJsonValue>>* A; if (!J->TryGetArray(A) || A->Num()<4) return false;
            FVector4d V((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber(), (*A)[3]->AsNumber());
            *(FVector4d*)Ptr = V; return true;
        }
        if (SS->GetFName() == NAME_Rotator)
        {
            const TArray<TSharedPtr<FJsonValue>>* A; if (!J->TryGetArray(A) || A->Num()<3) return false;
            FRotator R((float)(*A)[0]->AsNumber(), (float)(*A)[1]->AsNumber(), (float)(*A)[2]->AsNumber());
            *(FRotator*)Ptr = R; return true;
        }
        if (SS->GetFName() == NAME_Transform)
        {
            const TSharedPtr<FJsonObject>* Obj; if (!J->TryGetObject(Obj)) return false;
            FVector L(0); FRotator R(0); FVector S(1);
            const TArray<TSharedPtr<FJsonValue>>* LA; if ((*Obj)->TryGetArrayField(TEXT("location"), LA) && LA->Num()>=3) { L=FVector((float)(*LA)[0]->AsNumber(), (float)(*LA)[1]->AsNumber(), (float)(*LA)[2]->AsNumber()); }
            const TArray<TSharedPtr<FJsonValue>>* RA; if ((*Obj)->TryGetArrayField(TEXT("rotation"), RA) && RA->Num()>=3) { R=FRotator((float)(*RA)[0]->AsNumber(), (float)(*RA)[1]->AsNumber(), (float)(*RA)[2]->AsNumber()); }
            const TArray<TSharedPtr<FJsonValue>>* SA; if ((*Obj)->TryGetArrayField(TEXT("scale"), SA) && SA->Num()>=3) { S=FVector((float)(*SA)[0]->AsNumber(), (float)(*SA)[1]->AsNumber(), (float)(*SA)[2]->AsNumber()); }
            *(FTransform*)Ptr = FTransform(R, L, S); return true;
        }
        if (SS->GetFName() == NAME_Color)
        {
            const TArray<TSharedPtr<FJsonValue>>* A; if (!J->TryGetArray(A) || A->Num()<4) return false;
            FColor C((uint8)(*A)[0]->AsNumber(), (uint8)(*A)[1]->AsNumber(), (uint8)(*A)[2]->AsNumber(), (uint8)(*A)[3]->AsNumber());
            *(FColor*)Ptr = C; return true;
        }
        if (SS->GetFName() == NAME_LinearColor)
        {
            const TArray<TSharedPtr<FJsonValue>>* A; if (!J->TryGetArray(A) || A->Num()<4) return false;
            FLinearColor C((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber(), (*A)[3]->AsNumber());
            *(FLinearColor*)Ptr = C; return true;
        }
        // Generic by field names
        const TSharedPtr<FJsonObject>* Obj; if (!J->TryGetObject(Obj)) return false;
        for (TFieldIterator<FProperty> It(SS); It; ++It)
        {
            FProperty* Inner = *It;
            if (!(*Obj)->HasField(Inner->GetName())) continue;
            void* InnerPtr = (uint8*)Ptr + Inner->GetOffset_ForInternal();
            if (!ApplyJsonToProperty((*Obj)->TryGetField(Inner->GetName()), Inner, InnerPtr)) return false;
        }
        return true;
    }

    static bool ApplyJsonToProperty(const TSharedPtr<FJsonValue>& J, FProperty* Prop, void* Ptr)
    {
        if (FIntProperty* IP = CastField<FIntProperty>(Prop)) { if (J->Type==EJson::Number){ IP->SetPropertyValue(Ptr, (int32)J->AsNumber()); return true; } return false; }
        if (FFloatProperty* FP = CastField<FFloatProperty>(Prop)) { if (J->Type==EJson::Number){ FP->SetFloatingPointPropertyValue(Ptr, (float)J->AsNumber()); return true; } return false; }
        if (FDoubleProperty* DP = CastField<FDoubleProperty>(Prop)) { if (J->Type==EJson::Number){ DP->SetFloatingPointPropertyValue(Ptr, J->AsNumber()); return true; } return false; }
        if (FBoolProperty* BP = CastField<FBoolProperty>(Prop)) { if (J->Type==EJson::Boolean){ BP->SetPropertyValue(Ptr, J->AsBool()); return true; } return false; }
        if (FStrProperty* SP = CastField<FStrProperty>(Prop)) { if (J->Type==EJson::String){ SP->SetPropertyValue(Ptr, J->AsString()); return true; } return false; }
        if (FNameProperty* NP = CastField<FNameProperty>(Prop)) { if (J->Type==EJson::String){ NP->SetPropertyValue(Ptr, FName(*J->AsString())); return true; } return false; }
        if (FTextProperty* TP = CastField<FTextProperty>(Prop)) { if (J->Type==EJson::String){ TP->SetPropertyValue(Ptr, FText::FromString(J->AsString())); return true; } return false; }
        if (FEnumProperty* EP = CastField<FEnumProperty>(Prop))
        {
            if (J->Type==EJson::String)
            {
                UEnum* E = EP->GetEnum(); int64 V = E?E->GetValueByNameString(J->AsString()):INDEX_NONE; if (V==INDEX_NONE) V=(int64)FCString::Atoi(*J->AsString());
                EP->GetUnderlyingProperty()->SetIntPropertyValue(Ptr, V); return true;
            }
            if (J->Type==EJson::Number)
            {
                EP->GetUnderlyingProperty()->SetIntPropertyValue(Ptr, (int64)J->AsNumber()); return true;
            }
            return false;
        }
        if (FByteProperty* BP2 = CastField<FByteProperty>(Prop))
        {
            if (BP2->Enum)
            {
                if (J->Type==EJson::String){ int64 V = BP2->Enum->GetValueByNameString(J->AsString()); if (V==INDEX_NONE) V=(int64)FCString::Atoi(*J->AsString()); *(uint8*)Ptr=(uint8)V; return true; }
                if (J->Type==EJson::Number){ *(uint8*)Ptr=(uint8)J->AsNumber(); return true; }
                return false;
            }
            if (J->Type==EJson::Number){ *(uint8*)Ptr=(uint8)J->AsNumber(); return true; }
            return false;
        }
        if (FArrayProperty* AP = CastField<FArrayProperty>(Prop))
        {
            const TArray<TSharedPtr<FJsonValue>>* A; if (!J->TryGetArray(A)) return false;
            FScriptArrayHelper H(AP, Ptr);
            H.EmptyValues();
            for (const auto& Elem : *A)
            {
                int32 NewIdx = H.AddValue();
                void* ElemPtr = H.GetRawPtr(NewIdx);
                if (!ApplyJsonToProperty(Elem, AP->Inner, ElemPtr)) return false;
            }
            return true;
        }
        if (FMapProperty* MP = CastField<FMapProperty>(Prop))
        {
            const TSharedPtr<FJsonObject>* Obj; if (!J->TryGetObject(Obj)) return false;
            FScriptMapHelper H(MP, Ptr);
            H.EmptyValues();
            for (const auto& Pair : (*Obj)->Values)
            {
                int32 PairIdx = H.AddDefaultValue_Invalid_NeedsRehash();
                uint8* PairPtr = (uint8*)H.GetPairPtr(PairIdx);
                // key
                if (FStrProperty* SP2 = CastField<FStrProperty>(MP->KeyProp)) { FString S = Pair.Key; SP2->CopyCompleteValue(PairPtr, &S); }
                else if (FNameProperty* NP2 = CastField<FNameProperty>(MP->KeyProp)) { FName N(*Pair.Key); NP2->CopyCompleteValue(PairPtr, &N); }
                else if (FIntProperty* IP2 = CastField<FIntProperty>(MP->KeyProp)) { int32 I = FCString::Atoi(*Pair.Key); IP2->CopyCompleteValue(PairPtr, &I); }
                else if (FByteProperty* BP3 = CastField<FByteProperty>(MP->KeyProp)) { uint8 B=0; if (BP3->Enum){ int64 EV=BP3->Enum->GetValueByNameString(Pair.Key); B=(EV==INDEX_NONE)?(uint8)FCString::Atoi(*Pair.Key):(uint8)EV; } else { B=(uint8)FCString::Atoi(*Pair.Key);} BP3->CopyCompleteValue(PairPtr, &B); }
                else if (FEnumProperty* EP2 = CastField<FEnumProperty>(MP->KeyProp)) { int64 EV=0; if (Pair.Value->Type==EJson::String){ if (UEnum* E=EP2->GetEnum()){ EV=E->GetValueByNameString(Pair.Key); if (EV==INDEX_NONE) EV=(int64)FCString::Atoi(*Pair.Key);} } else { EV=(int64)FCString::Atoi(*Pair.Key);} EP2->GetUnderlyingProperty()->SetIntPropertyValue(PairPtr, EV); }
                else { return false; }
                // value
                void* ValPtr = PairPtr + MP->MapLayout.ValueOffset;
                if (!ApplyJsonToProperty(Pair.Value, MP->ValueProp, ValPtr)) return false;
            }
            H.Rehash();
            return true;
        }
        if (FSetProperty* SetP = CastField<FSetProperty>(Prop))
        {
            const TArray<TSharedPtr<FJsonValue>>* A; if (!J->TryGetArray(A)) return false;
            FScriptSetHelper H(SetP, Ptr);
            H.EmptyElements();
            for (const auto& Elem : *A)
            {
                int32 Idx = H.AddDefaultValue_Invalid_NeedsRehash();
                void* ElemPtr = H.GetElementPtr(Idx);
                if (!ApplyJsonToProperty(Elem, SetP->ElementProp, ElemPtr)) return false;
            }
            H.Rehash();
            return true;
        }
        if (FStructProperty* StP = CastField<FStructProperty>(Prop))
        {
            return ApplyJsonToStruct(J, StP, Ptr);
        }
        if (FObjectProperty* OP = CastField<FObjectProperty>(Prop))
        {
            if (J->Type==EJson::String)
            {
                FString Path = J->AsString();
                UObject* Obj = Path.Equals(TEXT("None"), ESearchCase::IgnoreCase) ? nullptr : StaticLoadObject(OP->PropertyClass, nullptr, *Path);
                OP->SetObjectPropertyValue(Ptr, Obj);
                return true;
            }
            return false;
        }
        return false;
    }
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleGetVariableProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName, Path; if (!Params->TryGetStringField(TEXT("blueprint_name"), BPName) || !Params->TryGetStringField(TEXT("path"), Path))
    { return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' or 'path'")); }

    // Check if this is a metadata path (e.g., "MyVar.@metadata.instance_editable")
    if (Path.Contains(TEXT(".@metadata.")))
    {
        FString VarName, MetadataKey;
        if (Path.Split(TEXT(".@metadata."), &VarName, &MetadataKey))
        {
            // Get variable metadata
            TSharedPtr<FJsonObject> MetadataParams = MakeShared<FJsonObject>();
            MetadataParams->SetStringField(TEXT("blueprint_name"), BPName);
            MetadataParams->SetStringField(TEXT("variable_name"), VarName);
            
            TSharedPtr<FJsonObject> MetadataResponse = HandleGetBlueprintVariableMetadata(MetadataParams);
            if (MetadataResponse->GetBoolField(TEXT("success")))
            {
                const TSharedPtr<FJsonObject>* MetadataObj;
                if (MetadataResponse->TryGetObjectField(TEXT("metadata"), MetadataObj))
                {
                    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                    Result->SetBoolField(TEXT("success"), true);
                    Result->SetStringField(TEXT("canonical_path"), Path);
                    Result->SetField(TEXT("value"), (*MetadataObj)->TryGetField(MetadataKey));
                    return Result;
                }
            }
            return FCommonUtils::CreateErrorResponse(TEXT("Failed to get metadata"));
        }
    }

    UBlueprint* BP = FCommonUtils::FindBlueprint(BPName);
    if (!BP) { return FCommonUtils::CreateErrorResponse(TEXT("Blueprint not found")); }
    if (!BP->GeneratedClass) { FKismetEditorUtilities::CompileBlueprint(BP); }
    if (!BP->GeneratedClass) { return FCommonUtils::CreateErrorResponse(TEXT("Failed to compile blueprint")); }
    UObject* CDO = BP->GeneratedClass->GetDefaultObject();

    FString Var; TArray<FString> Segs; if (!SplitVarPath(Path, Var, Segs)) { return FCommonUtils::CreateErrorResponse(TEXT("Invalid path format")); }
    FResolvedVarProp Res; if (!ResolveOnCDO(CDO, Var, Segs, Res)) { return FCommonUtils::CreateErrorResponse(TEXT("Failed to resolve property path")); }

    TSharedPtr<FJsonValue> JVal = SerializeProperty(Res.Prop, Res.Ptr);

    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), true);
    Out->SetStringField(TEXT("canonical_path"), Res.Canonical);
    Out->SetField(TEXT("value"), JVal);
    return Out;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleSetVariableProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName, Path; if (!Params->TryGetStringField(TEXT("blueprint_name"), BPName) || !Params->TryGetStringField(TEXT("path"), Path))
    { return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' or 'path'")); }
    if (!Params->HasField(TEXT("value"))) { return FCommonUtils::CreateErrorResponse(TEXT("Missing 'value'")); }
    
    // Check if this is a metadata path (e.g., "MyVar.@metadata.instance_editable")
    if (Path.Contains(TEXT(".@metadata.")))
    {
        FString VarName, MetadataKey;
        if (Path.Split(TEXT(".@metadata."), &VarName, &MetadataKey))
        {
            // Create metadata object with single field
            TSharedPtr<FJsonObject> MetadataUpdate = MakeShared<FJsonObject>();
            MetadataUpdate->SetField(MetadataKey, Params->Values.FindRef(TEXT("value")));
            
            TSharedPtr<FJsonObject> MetadataParams = MakeShared<FJsonObject>();
            MetadataParams->SetStringField(TEXT("blueprint_name"), BPName);
            MetadataParams->SetStringField(TEXT("variable_name"), VarName);
            MetadataParams->SetObjectField(TEXT("metadata"), MetadataUpdate);
            
            TSharedPtr<FJsonObject> MetadataResponse = HandleSetBlueprintVariableMetadata(MetadataParams);
            if (MetadataResponse->GetBoolField(TEXT("success")))
            {
                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetBoolField(TEXT("success"), true);
                Result->SetStringField(TEXT("canonical_path"), Path);
                Result->SetField(TEXT("normalized_value"), Params->Values.FindRef(TEXT("value")));
                return Result;
            }
            return FCommonUtils::CreateErrorResponse(TEXT("Failed to set metadata"));
        }
    }
    
    // Get the value - it might be a JSON value or a string that needs parsing
    TSharedPtr<FJsonValue> InVal = Params->Values.FindRef(TEXT("value"));
    
    // If the value is a string, try to parse it as JSON for arrays/objects, or convert to appropriate type
    if (InVal->Type == EJson::String)
    {
        FString ValueStr = InVal->AsString();
        
        // Try to parse as JSON if it looks like JSON (starts with [ or {)
        if (ValueStr.StartsWith(TEXT("[")) || ValueStr.StartsWith(TEXT("{")))
        {
            TSharedPtr<FJsonValue> ParsedValue;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ValueStr);
            if (FJsonSerializer::Deserialize(Reader, ParsedValue) && ParsedValue.IsValid())
            {
                InVal = ParsedValue;
            }
        }
        // Try to convert numeric strings to numbers
        else if (ValueStr.IsNumeric())
        {
            if (ValueStr.Contains(TEXT(".")))
            {
                // Float/Double
                double NumValue = FCString::Atod(*ValueStr);
                InVal = MakeShared<FJsonValueNumber>(NumValue);
            }
            else
            {
                // Integer
                int32 IntValue = FCString::Atoi(*ValueStr);
                InVal = MakeShared<FJsonValueNumber>((double)IntValue);
            }
        }
        // Try to convert boolean strings
        else if (ValueStr.Equals(TEXT("true"), ESearchCase::IgnoreCase))
        {
            InVal = MakeShared<FJsonValueBoolean>(true);
        }
        else if (ValueStr.Equals(TEXT("false"), ESearchCase::IgnoreCase))
        {
            InVal = MakeShared<FJsonValueBoolean>(false);
        }
    }

    UBlueprint* BP = FCommonUtils::FindBlueprint(BPName);
    if (!BP) { return FCommonUtils::CreateErrorResponse(TEXT("Blueprint not found")); }
    if (!BP->GeneratedClass) { FKismetEditorUtilities::CompileBlueprint(BP); }
    if (!BP->GeneratedClass) { return FCommonUtils::CreateErrorResponse(TEXT("Failed to compile blueprint")); }
    UObject* CDO = BP->GeneratedClass->GetDefaultObject();

    FString Var; TArray<FString> Segs; if (!SplitVarPath(Path, Var, Segs)) { return FCommonUtils::CreateErrorResponse(TEXT("Invalid path format")); }
    FResolvedVarProp Res; if (!ResolveOnCDO(CDO, Var, Segs, Res)) { return FCommonUtils::CreateErrorResponse(TEXT("Failed to resolve property path")); }

    auto Fail = [&](const TCHAR* Msg){ return FCommonUtils::CreateErrorResponse(Msg); };

    if (!ApplyJsonToProperty(InVal, Res.Prop, Res.Ptr)) { return Fail(TEXT("Unsupported property type or value kind")); }

    // Compile to propagate CDO changes
    FString CompileError; FCommonUtils::SafeCompileBlueprint(BP, CompileError);

    // Return normalized value via get
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetBoolField(TEXT("success"), true);
    Out->SetStringField(TEXT("canonical_path"), Res.Canonical);
    // reuse getter to normalize
    TSharedPtr<FJsonObject> GetParams = MakeShared<FJsonObject>();
    GetParams->SetStringField(TEXT("blueprint_name"), BPName);
    GetParams->SetStringField(TEXT("path"), Res.Canonical);
    TSharedPtr<FJsonObject> Norm = HandleGetVariableProperty(GetParams);
    if (Norm.IsValid() && Norm->HasField(TEXT("value")))
    {
        Out->SetField(TEXT("normalized_value"), Norm->TryGetField(TEXT("value")));
    }
    return Out;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleGetBlueprintVariableMetadata(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName, VarName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BPName) || !Params->TryGetStringField(TEXT("variable_name"), VarName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' or 'variable_name'"));
    }

    UBlueprint* BP = FCommonUtils::FindBlueprint(BPName);
    if (!BP) { return FCommonUtils::CreateErrorResponse(TEXT("Blueprint not found")); }

    // Find the variable in Blueprint's NewVariables
    FName VarFName(*VarName);
    FBPVariableDescription* VarDesc = nullptr;
    for (FBPVariableDescription& Var : BP->NewVariables)
    {
        if (Var.VarName == VarFName)
        {
            VarDesc = &Var;
            break;
        }
    }

    if (!VarDesc)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Variable not found"));
    }

    // Build metadata response
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("blueprint_name"), BPName);
    Response->SetStringField(TEXT("variable_name"), VarName);
    
    TSharedPtr<FJsonObject> Metadata = MakeShared<FJsonObject>();
    
    // Basic properties
    Metadata->SetBoolField(TEXT("instance_editable"), (VarDesc->PropertyFlags & CPF_Edit) != 0);
    Metadata->SetBoolField(TEXT("blueprint_readonly"), (VarDesc->PropertyFlags & CPF_BlueprintReadOnly) != 0);
    Metadata->SetBoolField(TEXT("expose_on_spawn"), (VarDesc->PropertyFlags & CPF_ExposeOnSpawn) != 0);
    Metadata->SetBoolField(TEXT("private"), (VarDesc->PropertyFlags & CPF_DisableEditOnInstance) != 0);
    Metadata->SetBoolField(TEXT("expose_to_matinee"), (VarDesc->PropertyFlags & CPF_Interp) != 0);
    
    // Category and tooltip
    Metadata->SetStringField(TEXT("category"), VarDesc->Category.ToString());
    Metadata->SetStringField(TEXT("tooltip"), VarDesc->FriendlyName);
    
    // Replication
    Metadata->SetBoolField(TEXT("replicated"), (VarDesc->PropertyFlags & CPF_Net) != 0);
    Metadata->SetStringField(TEXT("replication_condition"), 
        (VarDesc->PropertyFlags & CPF_RepNotify) ? TEXT("RepNotify") : TEXT("None"));
    
    // Slider settings (if applicable)
    if (VarDesc->HasMetaData(TEXT("UIMin")))
    {
        Metadata->SetStringField(TEXT("slider_min"), VarDesc->GetMetaData(TEXT("UIMin")));
    }
    if (VarDesc->HasMetaData(TEXT("UIMax")))
    {
        Metadata->SetStringField(TEXT("slider_max"), VarDesc->GetMetaData(TEXT("UIMax")));
    }
    
    Response->SetObjectField(TEXT("metadata"), Metadata);
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleSetBlueprintVariableMetadata(const TSharedPtr<FJsonObject>& Params)
{
    FString BPName, VarName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BPName) || !Params->TryGetStringField(TEXT("variable_name"), VarName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' or 'variable_name'"));
    }

    const TSharedPtr<FJsonObject>* MetadataObj;
    if (!Params->TryGetObjectField(TEXT("metadata"), MetadataObj))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'metadata' object"));
    }

    UBlueprint* BP = FCommonUtils::FindBlueprint(BPName);
    if (!BP) { return FCommonUtils::CreateErrorResponse(TEXT("Blueprint not found")); }

    // Find the variable
    FName VarFName(*VarName);
    FBPVariableDescription* VarDesc = nullptr;
    for (FBPVariableDescription& Var : BP->NewVariables)
    {
        if (Var.VarName == VarFName)
        {
            VarDesc = &Var;
            break;
        }
    }

    if (!VarDesc)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Variable not found"));
    }

    // Apply metadata changes
    bool bChanged = false;

    // Instance Editable
    if ((*MetadataObj)->HasField(TEXT("instance_editable")))
    {
        bool bInstanceEditable = (*MetadataObj)->GetBoolField(TEXT("instance_editable"));
        if (bInstanceEditable)
        {
            VarDesc->PropertyFlags |= CPF_Edit;
            VarDesc->PropertyFlags |= CPF_BlueprintVisible;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_Edit;
        }
        bChanged = true;
    }

    // Blueprint Read Only
    if ((*MetadataObj)->HasField(TEXT("blueprint_readonly")))
    {
        bool bReadOnly = (*MetadataObj)->GetBoolField(TEXT("blueprint_readonly"));
        if (bReadOnly)
        {
            VarDesc->PropertyFlags |= CPF_BlueprintReadOnly;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_BlueprintReadOnly;
        }
        bChanged = true;
    }

    // Expose on Spawn
    if ((*MetadataObj)->HasField(TEXT("expose_on_spawn")))
    {
        bool bExposeOnSpawn = (*MetadataObj)->GetBoolField(TEXT("expose_on_spawn"));
        if (bExposeOnSpawn)
        {
            VarDesc->PropertyFlags |= CPF_ExposeOnSpawn;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_ExposeOnSpawn;
        }
        bChanged = true;
    }

    // Private
    if ((*MetadataObj)->HasField(TEXT("private")))
    {
        bool bPrivate = (*MetadataObj)->GetBoolField(TEXT("private"));
        if (bPrivate)
        {
            VarDesc->PropertyFlags |= CPF_DisableEditOnInstance;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_DisableEditOnInstance;
        }
        bChanged = true;
    }

    // Category
    if ((*MetadataObj)->HasField(TEXT("category")))
    {
        FString Category = (*MetadataObj)->GetStringField(TEXT("category"));
        VarDesc->Category = FText::FromString(Category);
        bChanged = true;
    }

    // Tooltip
    if ((*MetadataObj)->HasField(TEXT("tooltip")))
    {
        FString Tooltip = (*MetadataObj)->GetStringField(TEXT("tooltip"));
        VarDesc->FriendlyName = Tooltip;
        bChanged = true;
    }

    // Slider range
    if ((*MetadataObj)->HasField(TEXT("slider_min")))
    {
        FString SliderMin = (*MetadataObj)->GetStringField(TEXT("slider_min"));
        VarDesc->SetMetaData(TEXT("UIMin"), *SliderMin);
        bChanged = true;
    }
    if ((*MetadataObj)->HasField(TEXT("slider_max")))
    {
        FString SliderMax = (*MetadataObj)->GetStringField(TEXT("slider_max"));
        VarDesc->SetMetaData(TEXT("UIMax"), *SliderMax);
        bChanged = true;
    }

    if (bChanged)
    {
        // Mark Blueprint as modified and recompile
        BP->MarkPackageDirty();
        FKismetEditorUtilities::CompileBlueprint(BP);
    }

    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("blueprint_name"), BPName);
    Response->SetStringField(TEXT("variable_name"), VarName);
    Response->SetStringField(TEXT("message"), TEXT("Variable metadata updated successfully"));
    
    return Response;
}
