#include "Commands/BlueprintCommands.h"
#include "Commands/CommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Commands/BlueprintVariableReflectionServices.h"
#include "Services/Blueprint/BlueprintPropertyService.h"
#include "Services/ServiceBase.h"

#include "WidgetBlueprint.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "Blueprint/UserWidget.h"
#include "Components/ActorComponent.h"
#include "Components/Widget.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h" 
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
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
#include "UObject/UObjectGlobals.h"

FBlueprintCommands::FBlueprintCommands()
{
    // Initialize service context and property service
    ServiceContext = MakeShared<FServiceContext>();
    PropertyService = MakeShared<FBlueprintPropertyService>(ServiceContext);
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
    else if (CommandType == TEXT("get_blueprint_property"))
    {
        return HandleGetBlueprintProperty(Params);
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
    else if (CommandType == TEXT("manage_blueprint_variable"))
    {
        return HandleManageBlueprintVariables(Params);
    }
    else if (CommandType == TEXT("get_blueprint_variable_info"))
    {
        return HandleGetBlueprintVariableInfo(Params);
    }
    else if (CommandType == TEXT("get_blueprint_info"))
    {
        return HandleGetBlueprintInfo(Params);
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
    
    return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleCreateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString RawBlueprintName;
    if (!Params->TryGetStringField(TEXT("name"), RawBlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    auto NormalizePackagePath = [](FString InPath) -> FString
    {
        InPath.ReplaceInline(TEXT("\\"), TEXT("/"));
        InPath.TrimStartAndEndInline();
        while (InPath.EndsWith(TEXT("/")))
        {
            InPath.LeftChopInline(1);
        }
        if (!InPath.StartsWith(TEXT("/")) && !InPath.IsEmpty())
        {
            InPath = TEXT("/") + InPath;
        }
        return InPath;
    };

    FString CleanName = RawBlueprintName;
    CleanName.ReplaceInline(TEXT("\\"), TEXT("/"));
    CleanName.TrimStartAndEndInline();

    FString PackagePath;
    FString AssetName;

    if (CleanName.Contains(TEXT("/")))
    {
        FString PackagePart = CleanName;
        FString ObjectName;

        if (CleanName.Contains(TEXT(".")))
        {
            CleanName.Split(TEXT("."), &PackagePart, &ObjectName);
        }

        PackagePart.TrimEndInline();
        while (PackagePart.EndsWith(TEXT("/")))
        {
            PackagePart.LeftChopInline(1);
        }

        int32 LastSlashIndex = INDEX_NONE;
        if (PackagePart.FindLastChar(TEXT('/'), LastSlashIndex))
        {
            AssetName = ObjectName.IsEmpty() ? PackagePart.Mid(LastSlashIndex + 1) : ObjectName;
            PackagePath = PackagePart.Left(LastSlashIndex);
        }
    }

    if (PackagePath.IsEmpty() || AssetName.IsEmpty())
    {
        AssetName = CleanName;
        if (!Params->TryGetStringField(TEXT("path"), PackagePath))
        {
            PackagePath = TEXT("/Game/Blueprints");
        }
    }

    PackagePath = NormalizePackagePath(PackagePath);

    if (PackagePath.IsEmpty())
    {
        PackagePath = TEXT("/Game/Blueprints");
    }

    const FString FullAssetPath = PackagePath + TEXT("/") + AssetName;

    // Check if blueprint already exists
    if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint already exists: %s"), *FullAssetPath));
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
        FString ClassDescriptor = ParentClass;
        ClassDescriptor.TrimStartAndEndInline();
        ClassDescriptor.ReplaceInline(TEXT("\\"), TEXT("/"));

        auto TryLoadParentClass = [](const FString& Descriptor) -> UClass*
        {
            if (Descriptor.IsEmpty())
            {
                return nullptr;
            }

            // Full path descriptors can be loaded directly.
            if (Descriptor.Contains(TEXT("/")))
            {
                if (UClass* Loaded = LoadObject<UClass>(nullptr, *Descriptor))
                {
                    return Loaded;
                }
            }

            // Try existing objects in memory.
            if (UClass* Existing = FindObject<UClass>(ANY_PACKAGE, *Descriptor))
            {
                return Existing;
            }

            // Try loading from common script modules.
            static const TArray<FString> ModuleHints = {
                TEXT("Engine"),
                TEXT("Game"),
                TEXT("PROTEUS")
            };

            FString CandidateBase = Descriptor;

            // Generate a handful of permutations (with/without leading 'A').
            TArray<FString> NamePermutations;
            NamePermutations.Add(CandidateBase);
            if (!CandidateBase.StartsWith(TEXT("A")))
            {
                NamePermutations.Add(TEXT("A") + CandidateBase);
            }

            for (const FString& NameVariant : NamePermutations)
            {
                if (UClass* ExistingVariant = FindObject<UClass>(ANY_PACKAGE, *NameVariant))
                {
                    return ExistingVariant;
                }

                for (const FString& ModuleName : ModuleHints)
                {
                    const FString ModulePath = FString::Printf(TEXT("/Script/%s.%s"), *ModuleName, *NameVariant);
                    if (UClass* LoadedVariant = LoadObject<UClass>(nullptr, *ModulePath))
                    {
                        return LoadedVariant;
                    }
                }
            }

            return nullptr;
        };

        if (UClass* ResolvedParent = TryLoadParentClass(ClassDescriptor))
        {
            SelectedParentClass = ResolvedParent;
        }
    }
    
    Factory->ParentClass = SelectedParentClass;

    // Create the blueprint
    UPackage* Package = CreatePackage(*FullAssetPath);
    UBlueprint* NewBlueprint = Cast<UBlueprint>(Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *AssetName, RF_Standalone | RF_Public, nullptr, GWarn));

    if (NewBlueprint)
    {
        // Notify the asset registry
        FAssetRegistryModule::AssetCreated(NewBlueprint);

        // Mark the package dirty
        Package->MarkPackageDirty();

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("name"), AssetName);
        ResultObj->SetStringField(TEXT("path"), FullAssetPath);
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

TSharedPtr<FJsonObject> FBlueprintCommands::HandleGetBlueprintProperty(const TSharedPtr<FJsonObject>& Params)
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

    // Use the property service to get metadata
    TResult<FPropertyInfo> Result = PropertyService->GetPropertyMetadata(Blueprint, PropertyName);
    
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetError());
    }

    // Convert property info to JSON response
    const FPropertyInfo& Info = Result.GetValue();
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("property_name"), Info.PropertyName);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetStringField(TEXT("type"), Info.PropertyType);
    Response->SetStringField(TEXT("property_class"), Info.PropertyClass);
    Response->SetStringField(TEXT("category"), Info.Category);
    Response->SetStringField(TEXT("tooltip"), Info.Tooltip);
    Response->SetBoolField(TEXT("is_editable"), Info.bIsEditable);
    Response->SetBoolField(TEXT("is_blueprint_visible"), Info.bIsBlueprintVisible);
    Response->SetBoolField(TEXT("is_blueprint_readonly"), Info.bIsBlueprintReadOnly);
    Response->SetStringField(TEXT("current_value"), Info.CurrentValue);
    
    if (!Info.DefaultValue.IsEmpty())
    {
        Response->SetStringField(TEXT("default_value"), Info.DefaultValue);
    }
    
    // Add type-specific metadata
    if (!Info.MinValue.IsEmpty())
    {
        Response->SetStringField(TEXT("min_value"), Info.MinValue);
    }
    if (!Info.MaxValue.IsEmpty())
    {
        Response->SetStringField(TEXT("max_value"), Info.MaxValue);
    }
    if (!Info.UIMin.IsEmpty())
    {
        Response->SetStringField(TEXT("ui_min"), Info.UIMin);
    }
    if (!Info.UIMax.IsEmpty())
    {
        Response->SetStringField(TEXT("ui_max"), Info.UIMax);
    }
    if (!Info.ObjectClass.IsEmpty())
    {
        Response->SetStringField(TEXT("object_class"), Info.ObjectClass);
    }
    if (!Info.ObjectValue.IsEmpty())
    {
        Response->SetStringField(TEXT("object_value"), Info.ObjectValue);
    }
    
    UE_LOG(LogTemp, Log, TEXT("MCP: Retrieved property '%s' from Blueprint '%s': Type=%s"), 
           *PropertyName, *BlueprintName, *Info.PropertyType);
    
    return Response;
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

    // Get property value parameter
    if (!Params->HasField(TEXT("property_value")))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
    }

    TSharedPtr<FJsonValue> JsonValue = Params->Values.FindRef(TEXT("property_value"));
    FString PropertyValue;
    
    // Convert JSON value to string
    if (JsonValue->Type == EJson::String)
    {
        PropertyValue = JsonValue->AsString();
    }
    else if (JsonValue->Type == EJson::Number)
    {
        PropertyValue = FString::Printf(TEXT("%f"), JsonValue->AsNumber());
    }
    else if (JsonValue->Type == EJson::Boolean)
    {
        PropertyValue = JsonValue->AsBool() ? TEXT("true") : TEXT("false");
    }
    else
    {
        // For complex types, fall back to the old implementation
        UObject* DefaultObject = Blueprint->GeneratedClass->GetDefaultObject();
        if (!DefaultObject)
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Failed to get default object"));
        }

        FString ErrorMessage;
        if (FCommonUtils::SetObjectProperty(DefaultObject, PropertyName, JsonValue, ErrorMessage))
        {
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

    // Use the property service to set the value
    TResult<void> Result = PropertyService->SetProperty(Blueprint, PropertyName, PropertyValue);
    
    if (Result.IsError())
    {
        return FCommonUtils::CreateErrorResponse(Result.GetError());
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("property"), PropertyName);
    ResultObj->SetBoolField(TEXT("success"), true);
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

TSharedPtr<FJsonObject> FBlueprintCommands::HandleGetBlueprintVariableInfo(const TSharedPtr<FJsonObject>& Params)
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

    // Build comprehensive response
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetStringField(TEXT("variable_name"), VariableName);
    
    // Get the actual variable value using our enhanced reflection system
    TSharedPtr<FJsonObject> ValueParams = MakeShared<FJsonObject>();
    ValueParams->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ValueParams->SetStringField(TEXT("path"), VariableName);
    TSharedPtr<FJsonObject> ValueResponse = HandleGetVariableProperty(ValueParams);
    
    if (ValueResponse.IsValid() && ValueResponse->GetBoolField(TEXT("success")))
    {
        TSharedPtr<FJsonValue> ValueField = ValueResponse->TryGetField(TEXT("value"));
        if (ValueField.IsValid())
        {
            Response->SetField(TEXT("value"), ValueField);
        }
        else
        {
            Response->SetStringField(TEXT("value"), TEXT("Value not found"));
        }
    }
    else
    {
        FString FallbackValue = VarDesc->DefaultValue;
        if (FallbackValue.IsEmpty())
        {
            FallbackValue = TEXT("None");
        }

        Response->SetStringField(TEXT("value"), FallbackValue);
        if (ValueResponse.IsValid() && ValueResponse->HasField(TEXT("error")))
        {
            Response->SetStringField(TEXT("value_error"), ValueResponse->GetStringField(TEXT("error")));
        }
        else
        {
            Response->SetStringField(TEXT("value_error"), TEXT("Value resolved from blueprint defaults"));
        }
    }

    // Get comprehensive type info using reflection-based reverse mapping
    FString TypeName;
    
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

    Response->SetStringField(TEXT("variable_type"), TypeName);
    Response->SetStringField(TEXT("category"), VarDesc->Category.ToString());
    Response->SetStringField(TEXT("tooltip"), VarDesc->FriendlyName);

    // Get all metadata using our internal metadata system (efficient internal call)
    TSharedPtr<FJsonObject> MetadataParams = MakeShared<FJsonObject>();
    MetadataParams->SetStringField(TEXT("blueprint_name"), BlueprintName);
    MetadataParams->SetStringField(TEXT("variable_name"), VariableName);
    TSharedPtr<FJsonObject> MetadataResponse = GetBlueprintVariableMetadata(MetadataParams);
    
    if (MetadataResponse->GetBoolField(TEXT("success")))
    {
        const TSharedPtr<FJsonObject>* MetadataObj;
        if (MetadataResponse->TryGetObjectField(TEXT("metadata"), MetadataObj))
        {
            Response->SetObjectField(TEXT("metadata"), *MetadataObj);
        }
        else
        {
            Response->SetObjectField(TEXT("metadata"), MakeShared<FJsonObject>());
        }
    }
    else
    {
        Response->SetObjectField(TEXT("metadata"), MakeShared<FJsonObject>());
    }

    // Add array/container information if applicable
    if (VarDesc->VarType.ContainerType != EPinContainerType::None)
    {
        FString ContainerType;
        switch (VarDesc->VarType.ContainerType)
        {
        case EPinContainerType::Array:
            ContainerType = TEXT("Array");
            break;
        case EPinContainerType::Set:
            ContainerType = TEXT("Set");
            break;
        case EPinContainerType::Map:
            ContainerType = TEXT("Map");
            break;
        default:
            ContainerType = TEXT("None");
            break;
        }
        Response->SetStringField(TEXT("container_type"), ContainerType);
    }
    else
    {
        Response->SetStringField(TEXT("container_type"), TEXT("None"));
    }

    // Add property flags information
    TSharedPtr<FJsonObject> FlagsInfo = MakeShared<FJsonObject>();
    FlagsInfo->SetBoolField(TEXT("is_editable"), (VarDesc->PropertyFlags & CPF_Edit) != 0);
    FlagsInfo->SetBoolField(TEXT("is_blueprint_readonly"), (VarDesc->PropertyFlags & CPF_BlueprintReadOnly) != 0);
    FlagsInfo->SetBoolField(TEXT("is_expose_on_spawn"), (VarDesc->PropertyFlags & CPF_ExposeOnSpawn) != 0);
    FlagsInfo->SetBoolField(TEXT("is_private"), (VarDesc->PropertyFlags & CPF_DisableEditOnInstance) != 0);
    Response->SetObjectField(TEXT("property_flags"), FlagsInfo);

    UE_LOG(LogTemp, Warning, TEXT("MCP: Enhanced variable info for '%s': Type=%s, Container=%s"), 
           *VariableName, *TypeName, *Response->GetStringField(TEXT("container_type")));
    
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleGetBlueprintInfo(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    
    // Get blueprint identifier (accepts name or full path)
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        // Try alternates for compatibility
        Params->TryGetStringField(TEXT("blueprint_path"), BlueprintName);
        if (BlueprintName.IsEmpty())
        {
            Params->TryGetStringField(TEXT("object_path"), BlueprintName);
        }
        if (BlueprintName.IsEmpty())
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter (accepts name or full path)"));
        }
    }

    // Find blueprint using reflection
    UBlueprint* Blueprint = FCommonUtils::FindBlueprintByName(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found for '%s'"), *BlueprintName));
    }

    // Create comprehensive blueprint_info object
    TSharedPtr<FJsonObject> BlueprintInfo = MakeShared<FJsonObject>();
    
    // Basic blueprint information
    BlueprintInfo->SetStringField(TEXT("name"), Blueprint->GetName());
    BlueprintInfo->SetStringField(TEXT("path"), Blueprint->GetPathName());
    BlueprintInfo->SetStringField(TEXT("package_path"), Blueprint->GetPackage() ? Blueprint->GetPackage()->GetPathName() : TEXT(""));
    BlueprintInfo->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetName() : TEXT("Unknown"));
    BlueprintInfo->SetStringField(TEXT("blueprint_type"), Blueprint->GetClass()->GetName());
    
    // Check if this is a widget blueprint
    bool bIsWidgetBlueprint = Blueprint->IsA<UWidgetBlueprint>();
    BlueprintInfo->SetBoolField(TEXT("is_widget_blueprint"), bIsWidgetBlueprint);
    
    // Variables - using reflection
    TArray<TSharedPtr<FJsonValue>> VariableArray;
    for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        TSharedPtr<FJsonObject> VarInfo = MakeShared<FJsonObject>();
        VarInfo->SetStringField(TEXT("name"), VarDesc.VarName.ToString());
        
        // Get type info using reflection
        FString TypeName = TEXT("Unknown");
        FString TypePath = TEXT("");  // Add type_path for consistency with manage_blueprint_variable
        
        if (VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
        {
            TypeName = TEXT("Boolean");
            TypePath = TEXT("/Script/CoreUObject.BoolProperty");
        }
        else if (VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Float)
        {
            TypeName = TEXT("Float");
            TypePath = TEXT("/Script/CoreUObject.FloatProperty");
        }
        else if (VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Int)
        {
            TypeName = TEXT("Integer");
            TypePath = TEXT("/Script/CoreUObject.IntProperty");
        }
        else if (VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_String)
        {
            TypeName = TEXT("String");
            TypePath = TEXT("/Script/CoreUObject.StrProperty");
        }
        else if (VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Byte)
        {
            TypeName = TEXT("Byte");
            TypePath = TEXT("/Script/CoreUObject.ByteProperty");
        }
        else if (VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Int64)
        {
            TypeName = TEXT("Int64");
            TypePath = TEXT("/Script/CoreUObject.Int64Property");
        }
        else if (VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Double)
        {
            TypeName = TEXT("Double");
            TypePath = TEXT("/Script/CoreUObject.DoubleProperty");
        }
        else if (VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Name)
        {
            TypeName = TEXT("Name");
            TypePath = TEXT("/Script/CoreUObject.NameProperty");
        }
        else if (VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Text)
        {
            TypeName = TEXT("Text");
            TypePath = TEXT("/Script/CoreUObject.TextProperty");
        }
        else if (VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Struct)
        {
            if (VarDesc.VarType.PinSubCategoryObject == TBaseStructure<FVector>::Get())
            {
                TypeName = TEXT("Vector");
                TypePath = TEXT("/Script/CoreUObject.Vector");
            }
            else if (VarDesc.VarType.PinSubCategoryObject == TBaseStructure<FLinearColor>::Get())
            {
                TypeName = TEXT("LinearColor");
                TypePath = TEXT("/Script/CoreUObject.LinearColor");
            }
            else if (VarDesc.VarType.PinSubCategoryObject.IsValid())
            {
                TypeName = VarDesc.VarType.PinSubCategoryObject->GetName();
                TypePath = VarDesc.VarType.PinSubCategoryObject->GetPathName();
            }
        }
        else if (VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Object || VarDesc.VarType.PinCategory == UEdGraphSchema_K2::PC_Class)
        {
            if (VarDesc.VarType.PinSubCategoryObject.IsValid())
            {
                TypeName = VarDesc.VarType.PinSubCategoryObject->GetName();
                TypePath = VarDesc.VarType.PinSubCategoryObject->GetPathName();
            }
        }
        
        VarInfo->SetStringField(TEXT("type"), TypeName);
        VarInfo->SetStringField(TEXT("type_path"), TypePath);  // ✅ ADD type_path for AI consistency
        VarInfo->SetStringField(TEXT("category"), VarDesc.Category.ToString());
        VarInfo->SetBoolField(TEXT("is_editable"), (VarDesc.PropertyFlags & CPF_Edit) != 0);
        VarInfo->SetBoolField(TEXT("is_blueprint_readonly"), (VarDesc.PropertyFlags & CPF_BlueprintReadOnly) != 0);
        VarInfo->SetBoolField(TEXT("is_expose_on_spawn"), (VarDesc.PropertyFlags & CPF_ExposeOnSpawn) != 0);
        
        // Container type
        FString ContainerType = TEXT("None");
        if (VarDesc.VarType.ContainerType == EPinContainerType::Array)
            ContainerType = TEXT("Array");
        else if (VarDesc.VarType.ContainerType == EPinContainerType::Set)
            ContainerType = TEXT("Set");
        else if (VarDesc.VarType.ContainerType == EPinContainerType::Map)
            ContainerType = TEXT("Map");
        VarInfo->SetStringField(TEXT("container_type"), ContainerType);
        
        VariableArray.Add(MakeShared<FJsonValueObject>(VarInfo));
    }
    BlueprintInfo->SetArrayField(TEXT("variables"), VariableArray);
    
    // Components - using reflection
    TArray<TSharedPtr<FJsonValue>> ComponentArray;
    if (Blueprint->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& AllNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
        for (USCS_Node* Node : AllNodes)
        {
            if (Node && Node->ComponentTemplate)
            {
                TSharedPtr<FJsonObject> CompInfo = MakeShared<FJsonObject>();
                CompInfo->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
                CompInfo->SetStringField(TEXT("type"), Node->ComponentTemplate->GetClass()->GetName());
                CompInfo->SetBoolField(TEXT("is_native"), Node->ComponentTemplate->GetClass()->HasAnyClassFlags(CLASS_Native));
                
                // Parent component - using ParentComponentOrVariableName instead of GetParent()
                if (!Node->ParentComponentOrVariableName.IsNone())
                {
                    CompInfo->SetStringField(TEXT("parent"), Node->ParentComponentOrVariableName.ToString());
                }
                
                ComponentArray.Add(MakeShared<FJsonValueObject>(CompInfo));
            }
        }
    }
    BlueprintInfo->SetArrayField(TEXT("components"), ComponentArray);
    
    // Widget components (if this is a widget blueprint)
    TArray<TSharedPtr<FJsonValue>> WidgetComponentArray;
    if (bIsWidgetBlueprint)
    {
        // For widget blueprints, we'll provide a basic indication but delegate detailed widget info
        // to the existing UMG commands. This keeps separation of concerns clean.
        BlueprintInfo->SetStringField(TEXT("widget_info_note"), TEXT("Use get_widget_blueprint_info for detailed UMG component information"));
    }
    BlueprintInfo->SetArrayField(TEXT("widget_components"), WidgetComponentArray);
    
    // Functions - using reflection
    TArray<TSharedPtr<FJsonValue>> FunctionArray;
    if (Blueprint->FunctionGraphs.Num() > 0)
    {
        for (UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
        {
            if (FunctionGraph)
            {
                TSharedPtr<FJsonObject> FuncInfo = MakeShared<FJsonObject>();
                FuncInfo->SetStringField(TEXT("name"), FunctionGraph->GetName());
                FuncInfo->SetStringField(TEXT("graph_type"), TEXT("Function"));
                
                // Count nodes
                int32 NodeCount = 0;
                for (UEdGraphNode* Node : FunctionGraph->Nodes)
                {
                    if (Node) NodeCount++;
                }
                FuncInfo->SetNumberField(TEXT("node_count"), NodeCount);
                
                FunctionArray.Add(MakeShared<FJsonValueObject>(FuncInfo));
            }
        }
    }
    BlueprintInfo->SetArrayField(TEXT("functions"), FunctionArray);
    
    // Event Graph information
    TArray<TSharedPtr<FJsonValue>> EventArray;
    if (Blueprint->UbergraphPages.Num() > 0)
    {
        for (UEdGraph* EventGraph : Blueprint->UbergraphPages)
        {
            if (EventGraph)
            {
                TSharedPtr<FJsonObject> GraphInfo = MakeShared<FJsonObject>();
                GraphInfo->SetStringField(TEXT("name"), EventGraph->GetName());
                GraphInfo->SetStringField(TEXT("graph_type"), TEXT("EventGraph"));
                
                // Count different node types using reflection
                int32 EventNodeCount = 0;
                int32 FunctionCallCount = 0;
                int32 VariableNodeCount = 0;
                int32 TotalNodeCount = 0;
                
                for (UEdGraphNode* Node : EventGraph->Nodes)
                {
                    if (Node)
                    {
                        TotalNodeCount++;
                        FString NodeClassName = Node->GetClass()->GetName();
                        
                        if (NodeClassName.Contains(TEXT("Event")))
                            EventNodeCount++;
                        else if (NodeClassName.Contains(TEXT("CallFunction")) || NodeClassName.Contains(TEXT("K2Node_CallFunction")))
                            FunctionCallCount++;
                        else if (NodeClassName.Contains(TEXT("Variable")))
                            VariableNodeCount++;
                    }
                }
                
                GraphInfo->SetNumberField(TEXT("total_nodes"), TotalNodeCount);
                GraphInfo->SetNumberField(TEXT("event_nodes"), EventNodeCount);
                GraphInfo->SetNumberField(TEXT("function_calls"), FunctionCallCount);
                GraphInfo->SetNumberField(TEXT("variable_nodes"), VariableNodeCount);
                
                EventArray.Add(MakeShared<FJsonValueObject>(GraphInfo));
            }
        }
    }
    BlueprintInfo->SetArrayField(TEXT("event_graphs"), EventArray);
    
    // Blueprint properties from the Class Default Object
    TArray<TSharedPtr<FJsonValue>> PropertyArray;
    if (Blueprint->GeneratedClass)
    {
        UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
        if (CDO)
        {
            for (TFieldIterator<FProperty> PropertyIt(Blueprint->GeneratedClass, EFieldIteratorFlags::ExcludeSuper); PropertyIt; ++PropertyIt)
            {
                FProperty* Property = *PropertyIt;
                if (Property && Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
                {
                    TSharedPtr<FJsonObject> PropInfo = MakeShared<FJsonObject>();
                    PropInfo->SetStringField(TEXT("name"), Property->GetName());
                    PropInfo->SetStringField(TEXT("type"), Property->GetCPPType());
                    PropInfo->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
                    PropInfo->SetBoolField(TEXT("is_editable"), Property->HasAnyPropertyFlags(CPF_Edit));
                    PropInfo->SetBoolField(TEXT("is_blueprint_visible"), Property->HasAnyPropertyFlags(CPF_BlueprintVisible));
                    PropInfo->SetBoolField(TEXT("is_blueprint_readonly"), Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly));
                    
                    PropertyArray.Add(MakeShared<FJsonValueObject>(PropInfo));
                }
            }
        }
    }
    BlueprintInfo->SetArrayField(TEXT("blueprint_properties"), PropertyArray);
    
    // Success response
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("blueprint_info"), BlueprintInfo);
    
    UE_LOG(LogTemp, Warning, TEXT("MCP: Comprehensive blueprint info for '%s': Type=%s, Variables=%d, Components=%d, Functions=%d"), 
           *BlueprintName, *BlueprintInfo->GetStringField(TEXT("blueprint_type")), 
           VariableArray.Num(), ComponentArray.Num(), FunctionArray.Num());
    
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
    
    // Cache the variable description before removal to avoid dangling pointers
    const FBPVariableDescription RemovedVariable = *VarDesc;

    // Remove the variable from the Blueprint
    Blueprint->NewVariables.RemoveAt(VarIndex);
    
    // Mark Blueprint as dirty and recompile
    Blueprint->MarkPackageDirty();
    FString CompileError;
    FCommonUtils::SafeCompileBlueprint(Blueprint, CompileError);

    // Track cleanup actions
    TSharedPtr<FJsonObject> CleanupInfo = MakeShared<FJsonObject>();
    CleanupInfo->SetStringField(TEXT("action"), TEXT("variable_removed"));
    CleanupInfo->SetStringField(TEXT("variable_name"), VariableName);
    CleanupInfo->SetStringField(TEXT("variable_type"), UEdGraphSchema_K2::TypeToText(RemovedVariable.VarType).ToString());
    CleanupPerformed.Add(MakeShared<FJsonValueObject>(CleanupInfo));
    
    // Build success response
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("variable_name"), VariableName);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetArrayField(TEXT("references"), References);
    Response->SetBoolField(TEXT("force_used"), ForceDelete);
    Response->SetArrayField(TEXT("cleanup_performed"), CleanupPerformed);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Variable '%s' deleted successfully from Blueprint '%s'"), *VariableName, *BlueprintName));
    if (!CompileError.IsEmpty())
    {
        Response->SetStringField(TEXT("compile_warning"), CompileError);
    }
    
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

    static FString ContainerTypeToString(EPinContainerType ContainerType)
    {
        switch (ContainerType)
        {
        case EPinContainerType::Array:
            return TEXT("Array");
        case EPinContainerType::Set:
            return TEXT("Set");
        case EPinContainerType::Map:
            return TEXT("Map");
        default:
            return TEXT("None");
        }
    }

    static FString JsonValueToString(const TSharedPtr<FJsonValue>& JsonValue)
    {
        if (!JsonValue.IsValid())
        {
            return TEXT("");
        }

        switch (JsonValue->Type)
        {
        case EJson::String:
            return JsonValue->AsString();
        case EJson::Number:
            return FString::SanitizeFloat(JsonValue->AsNumber());
        case EJson::Boolean:
            return JsonValue->AsBool() ? TEXT("true") : TEXT("false");
        case EJson::Null:
            return TEXT("null");
        case EJson::Array:
        {
            FString Serialized;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
            FJsonSerializer::Serialize(JsonValue->AsArray(), Writer);
            Writer->Close();
            return Serialized;
        }
        case EJson::Object:
        {
            FString Serialized;
            TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
            TSharedPtr<FJsonObject> ObjectValue = JsonValue->AsObject();
            if (ObjectValue.IsValid())
            {
                FJsonSerializer::Serialize(ObjectValue.ToSharedRef(), Writer);
            }
            Writer->Close();
            return Serialized;
        }
        default:
            return TEXT("");
        }
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
            
            TSharedPtr<FJsonObject> MetadataResponse = GetBlueprintVariableMetadata(MetadataParams);
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
            
            TSharedPtr<FJsonObject> MetadataResponse = SetBlueprintVariableMetadata(MetadataParams);
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

TSharedPtr<FJsonObject> FBlueprintCommands::GetBlueprintVariableMetadata(const TSharedPtr<FJsonObject>& Params)
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

TSharedPtr<FJsonObject> FBlueprintCommands::SetBlueprintVariableMetadata(const TSharedPtr<FJsonObject>& Params)
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

// ============================================================================
// NEW UNIFIED BLUEPRINT VARIABLE MANAGEMENT SYSTEM
// ============================================================================

TSharedPtr<FJsonObject> FBlueprintCommands::HandleManageBlueprintVariables(const TSharedPtr<FJsonObject>& Params)
{
    // Get the action parameter
    FString Action;
    if (!Params->TryGetStringField(TEXT("action"), Action))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'action' parameter. Valid actions: create, delete, modify, list, get_info, get_property, set_property, search_types"));
    }

    // Flip: route to reflection path by default; maintains old path only if explicitly requested
    bool bLegacyPath = false;
    Params->TryGetBoolField(TEXT("use_legacy"), bLegacyPath);
    if (!bLegacyPath)
    {
        return FBlueprintVariableCommandContext::Get().ExecuteCommand(Action, Params);
    }

    // Route to appropriate operation based on action
    if (Action == TEXT("create"))
    {
        return HandleCreateVariableOperation(Params);
    }
    else if (Action == TEXT("delete"))
    {
        return HandleDeleteVariableOperation(Params);
    }
    else if (Action == TEXT("modify"))
    {
        return HandleModifyVariableOperation(Params);
    }
    else if (Action == TEXT("list"))
    {
        return HandleListVariablesOperation(Params);
    }
    else if (Action == TEXT("get_info"))
    {
        return HandleGetVariableInfoOperation(Params);
    }
    else if (Action == TEXT("get_property") || Action == TEXT("set_property") || Action == TEXT("diagnostics") || Action == TEXT("search_types"))
    {
        // Even if legacy is requested, these are best handled by the reflection path
        return FBlueprintVariableCommandContext::Get().ExecuteCommand(Action, Params);
    }
    else if (Action == TEXT("search_types"))
    {
        return HandleSearchTypesOperation(Params);
    }
    else
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown action: %s. Valid actions: create, delete, modify, list, get_info, get_property, set_property, search_types"), *Action));
    }
}

// ============================================================================
// REFLECTION-BASED TYPE DISCOVERY SYSTEM
// ============================================================================

TArray<UClass*> FBlueprintCommands::DiscoverAllVariableTypes()
{
    TArray<UClass*> VariableTypes;
    
    // Iterate through all UClass objects using reflection
    for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
    {
        UClass* Class = *ClassIterator;
        
        // Skip abstract classes and deprecated classes
        if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            continue;
        }
        
        // Check if this class can be used as a Blueprint variable type
        if (IsValidBlueprintVariableType(Class))
        {
            VariableTypes.Add(Class);
        }
    }
    
    return VariableTypes;
}

bool FBlueprintCommands::IsValidBlueprintVariableType(UClass* Class)
{
    if (!Class)
    {
        return false;
    }

    const FString ClassName = Class->GetName();

    if (ClassName.StartsWith(TEXT("SKEL_")) ||
        ClassName.StartsWith(TEXT("REINST_")) ||
        ClassName.StartsWith(TEXT("HOTRELOAD_")) ||
        ClassName.StartsWith(TEXT("TRASHCLASS_")) ||
        ClassName.StartsWith(TEXT("PLACEHOLDER-CLASS")))
    {
        return false;
    }

    if (Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
    {
        return false;
    }

    // Transient types generated during compilation shouldn't appear in search results
    if (Class->HasAnyFlags(RF_Transient) && Class->ClassGeneratedBy == nullptr)
    {
        return false;
    }

    return true;
}

bool FBlueprintCommands::ResolveVariableType(const FString& TypeName, const FString& TypePath, FEdGraphPinType& OutPinType)
{
    OutPinType = FEdGraphPinType();

    const FString NormalizedName = TypeName.TrimStartAndEnd();
    const FString NormalizedPath = TypePath.TrimStartAndEnd();

    auto MatchesName = [&NormalizedName](const FString& Candidate)
    {
        return Candidate.Equals(NormalizedName, ESearchCase::IgnoreCase);
    };

    auto SetStructPin = [&OutPinType](UScriptStruct* Struct) -> bool
    {
        if (!Struct)
        {
            return false;
        }
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
        OutPinType.PinSubCategoryObject = Struct;
        return true;
    };

    auto SetEnumPin = [&OutPinType](UEnum* Enum) -> bool
    {
        if (!Enum)
        {
            return false;
        }
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
        OutPinType.PinSubCategoryObject = Enum;
        return true;
    };

    auto SetClassPin = [&OutPinType](UClass* Class) -> bool
    {
        if (!Class)
        {
            return false;
        }

        if (Class->HasAnyClassFlags(CLASS_Interface))
        {
            OutPinType.PinCategory = UEdGraphSchema_K2::PC_Interface;
        }
        else
        {
            OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
        }
        OutPinType.PinSubCategoryObject = Class;
        return true;
    };

    // Basic types (case-insensitive for convenience)
    if (MatchesName(TEXT("Boolean")) || MatchesName(TEXT("Bool")))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        return true;
    }
    else if (MatchesName(TEXT("Byte")))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
        return true;
    }
    else if (MatchesName(TEXT("Integer")) || MatchesName(TEXT("Int")))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int;
        return true;
    }
    else if (MatchesName(TEXT("Integer64")) || MatchesName(TEXT("Int64")))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
        return true;
    }
    else if (MatchesName(TEXT("Float")))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Float;
        return true;
    }
    else if (MatchesName(TEXT("Double")))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Double;
        return true;
    }
    else if (MatchesName(TEXT("Name")))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
        return true;
    }
    else if (MatchesName(TEXT("String")))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_String;
        return true;
    }
    else if (MatchesName(TEXT("Text")))
    {
        OutPinType.PinCategory = UEdGraphSchema_K2::PC_Text;
        return true;
    }

    // Common engine structs via TBaseStructure (fast path)
    if (MatchesName(TEXT("Vector")))
    {
        return SetStructPin(TBaseStructure<FVector>::Get());
    }
    else if (MatchesName(TEXT("Vector2D")))
    {
        return SetStructPin(TBaseStructure<FVector2D>::Get());
    }
    else if (MatchesName(TEXT("Vector4")))
    {
        return SetStructPin(TBaseStructure<FVector4>::Get());
    }
    else if (MatchesName(TEXT("Rotator")))
    {
        return SetStructPin(TBaseStructure<FRotator>::Get());
    }
    else if (MatchesName(TEXT("Transform")))
    {
        return SetStructPin(TBaseStructure<FTransform>::Get());
    }
    else if (MatchesName(TEXT("Color")))
    {
        return SetStructPin(TBaseStructure<FColor>::Get());
    }
    else if (MatchesName(TEXT("LinearColor")))
    {
        return SetStructPin(TBaseStructure<FLinearColor>::Get());
    }

    // Resolve using explicit path first (structs, enums, classes)
    if (!NormalizedPath.IsEmpty())
    {
        if (UScriptStruct* StructFromPath = UClass::TryFindTypeSlow<UScriptStruct>(NormalizedPath, EFindFirstObjectOptions::EnsureIfAmbiguous))
        {
            return SetStructPin(StructFromPath);
        }

        if (UEnum* EnumFromPath = UClass::TryFindTypeSlow<UEnum>(NormalizedPath, EFindFirstObjectOptions::EnsureIfAmbiguous))
        {
            return SetEnumPin(EnumFromPath);
        }

        if (UClass* ClassFromPath = UClass::TryFindTypeSlow<UClass>(NormalizedPath, EFindFirstObjectOptions::EnsureIfAmbiguous))
        {
            return SetClassPin(ClassFromPath);
        }
    }

    // Resolve structs / enums / classes by name fallback
    if (UScriptStruct* StructByName = FindStructByName(NormalizedName))
    {
        return SetStructPin(StructByName);
    }

    if (UEnum* EnumByName = FindEnumByName(NormalizedName))
    {
        return SetEnumPin(EnumByName);
    }

    if (UClass* ClassByName = FindClassByName(NormalizedName))
    {
        return SetClassPin(ClassByName);
    }

    return false;
}

UClass* FBlueprintCommands::FindClassByName(const FString& ClassName)
{
    if (ClassName.IsEmpty())
    {
        return nullptr;
    }

    if (ClassName.Equals(TEXT("UserWidget"), ESearchCase::IgnoreCase))
    {
        return UUserWidget::StaticClass();
    }
    if (ClassName.Equals(TEXT("Widget"), ESearchCase::IgnoreCase))
    {
        return UWidget::StaticClass();
    }

    static const TMap<FString, FString> PreferredClassPaths = {
        { TEXT("NiagaraSystem"), TEXT("/Script/Niagara.NiagaraSystem") },
        { TEXT("SoundBase"), TEXT("/Script/Engine.SoundBase") },
        { TEXT("SoundWave"), TEXT("/Script/Engine.SoundWave") },
        { TEXT("SoundCue"), TEXT("/Script/Engine.SoundCue") },
        { TEXT("StaticMesh"), TEXT("/Script/Engine.StaticMesh") },
        { TEXT("Material"), TEXT("/Script/Engine.Material") },
        { TEXT("MaterialInstance"), TEXT("/Script/Engine.MaterialInstance") },
        { TEXT("Texture2D"), TEXT("/Script/Engine.Texture2D") },
        { TEXT("Actor"), TEXT("/Script/Engine.Actor") },
        { TEXT("Pawn"), TEXT("/Script/Engine.Pawn") }
    };

    if (const FString* PreferredPath = PreferredClassPaths.Find(ClassName))
    {
        if (UClass* LoadedPreferred = UClass::TryFindTypeSlow<UClass>(*PreferredPath, EFindFirstObjectOptions::EnsureIfAmbiguous))
        {
            return LoadedPreferred;
        }
    }

    if (UClass* LoadedByName = UClass::TryFindTypeSlow<UClass>(ClassName, EFindFirstObjectOptions::EnsureIfAmbiguous))
    {
        if (IsValidBlueprintVariableType(LoadedByName))
        {
            return LoadedByName;
        }
    }

    for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
    {
        UClass* Candidate = *ClassIterator;
        if (!IsValidBlueprintVariableType(Candidate))
        {
            continue;
        }

        if (Candidate->GetName().Equals(ClassName, ESearchCase::IgnoreCase))
        {
            return Candidate;
        }
    }

    return nullptr;
}

UScriptStruct* FBlueprintCommands::FindStructByName(const FString& StructName)
{
    if (StructName.IsEmpty())
    {
        return nullptr;
    }

    if (UScriptStruct* StructFromPath = UClass::TryFindTypeSlow<UScriptStruct>(StructName, EFindFirstObjectOptions::EnsureIfAmbiguous))
    {
        return StructFromPath;
    }

    for (TObjectIterator<UScriptStruct> StructIterator; StructIterator; ++StructIterator)
    {
        if (StructIterator->GetName().Equals(StructName, ESearchCase::IgnoreCase))
        {
            return *StructIterator;
        }
    }

    return nullptr;
}

UEnum* FBlueprintCommands::FindEnumByName(const FString& EnumName)
{
    if (EnumName.IsEmpty())
    {
        return nullptr;
    }

    if (UEnum* EnumFromPath = UClass::TryFindTypeSlow<UEnum>(EnumName, EFindFirstObjectOptions::EnsureIfAmbiguous))
    {
        return EnumFromPath;
    }

    for (TObjectIterator<UEnum> EnumIterator; EnumIterator; ++EnumIterator)
    {
        if (EnumIterator->GetName().Equals(EnumName, ESearchCase::IgnoreCase))
        {
            return *EnumIterator;
        }
    }

    return nullptr;
}

// ============================================================================
// OPERATION HANDLERS
// ============================================================================

TSharedPtr<FJsonObject> FBlueprintCommands::HandleCreateVariableOperation(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }
    
    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }
    
    // Get variable config
    const TSharedPtr<FJsonObject>* VariableConfigPtr;
    if (!Params->TryGetObjectField(TEXT("variable_config"), VariableConfigPtr) || !VariableConfigPtr || !VariableConfigPtr->IsValid())
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing or invalid 'variable_config' parameter"));
    }
    
    TSharedPtr<FJsonObject> VariableConfig = *VariableConfigPtr;
    
    FString VariableType;
    if (!VariableConfig->TryGetStringField(TEXT("type"), VariableType))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'type' in variable_config"));
    }

    FString VariableTypePath;
    VariableConfig->TryGetStringField(TEXT("type_path"), VariableTypePath);
    
    // Find the Blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprintByName(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }
    
    // Resolve the variable type using the enhanced system
    FEdGraphPinType PinType;
    if (!ResolveVariableType(VariableType, VariableTypePath, PinType))
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown or invalid variable type: %s"), *VariableType));
    }
    
    // Get optional parameters
    FString DefaultValue;
    VariableConfig->TryGetStringField(TEXT("default_value"), DefaultValue);
    
    bool bIsEditable = true;
    VariableConfig->TryGetBoolField(TEXT("is_editable"), bIsEditable);
    
    FString Category = TEXT("Default");
    VariableConfig->TryGetStringField(TEXT("category"), Category);
    
    FString Tooltip;
    VariableConfig->TryGetStringField(TEXT("tooltip"), Tooltip);
    
    // Add the variable
    if (FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*VariableName), PinType, DefaultValue))
    {
        // Configure variable properties
        FName VarName(*VariableName);
        for (FBPVariableDescription& Variable : Blueprint->NewVariables)
        {
            if (Variable.VarName == VarName)
            {
                if (bIsEditable)
                {
                    Variable.PropertyFlags |= CPF_Edit;
                    Variable.PropertyFlags |= CPF_BlueprintVisible;
                }
                
                Variable.Category = FText::FromString(Category);
                if (!Tooltip.IsEmpty())
                {
                    // Explicitly set FriendlyName to the tooltip string
                    Variable.FriendlyName = Tooltip;
                }
                break;
            }
        }
        
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("action"), TEXT("create"));
        Response->SetStringField(TEXT("message"), TEXT("Variable created successfully"));
        Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
        Response->SetStringField(TEXT("variable_name"), VariableName);
        Response->SetStringField(TEXT("variable_type"), VariableType);
        Response->SetBoolField(TEXT("is_editable"), bIsEditable);
        Response->SetStringField(TEXT("category"), Category);
        
        // Mark Blueprint as modified and compile to ensure CDO is up to date for subsequent queries
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FString CompileError;
        if (!FCommonUtils::SafeCompileBlueprint(Blueprint, CompileError) && !CompileError.IsEmpty())
        {
            Response->SetStringField(TEXT("compile_warning"), CompileError);
        }
    }
    else
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to create variable in Blueprint"));
    }
    
    return Response;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleSearchTypesOperation(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

    FString CategoryFilter;
    Params->TryGetStringField(TEXT("category"), CategoryFilter);
    CategoryFilter.TrimStartAndEndInline();

    FString SearchText;
    Params->TryGetStringField(TEXT("search_text"), SearchText);
    SearchText.TrimStartAndEndInline();

    bool bIncludeBlueprints = true;
    Params->TryGetBoolField(TEXT("include_blueprints"), bIncludeBlueprints);

    bool bIncludeEngine = true;
    Params->TryGetBoolField(TEXT("include_engine_types"), bIncludeEngine);

    auto IsTransientTypeName = [](const FString& InName) -> bool
    {
        return InName.StartsWith(TEXT("SKEL_")) ||
               InName.StartsWith(TEXT("REINST_")) ||
               InName.StartsWith(TEXT("HOTRELOAD_")) ||
               InName.StartsWith(TEXT("TRASHCLASS_")) ||
               InName.StartsWith(TEXT("TRASHSTRUCT_")) ||
               InName.StartsWith(TEXT("PLACEHOLDER-"));
    };

    auto GetCategoryRank = [](const FString& Category) -> int32
    {
        if (Category.Equals(TEXT("Basic"), ESearchCase::IgnoreCase)) { return 0; }
        if (Category.Equals(TEXT("Structure"), ESearchCase::IgnoreCase)) { return 1; }
        if (Category.Equals(TEXT("Interface"), ESearchCase::IgnoreCase)) { return 2; }
        if (Category.Equals(TEXT("Object Types"), ESearchCase::IgnoreCase)) { return 3; }
        if (Category.Equals(TEXT("Enum"), ESearchCase::IgnoreCase)) { return 4; }
        return 5;
    };

    struct FVariableTypeRecord
    {
        FString Name;
        FString DisplayName;
        FString Category;
        FString TypeKind;
        FString Description;
        FString Path;
        bool bIsBlueprintType = false;
        bool bIsAssetType = false;
        bool bIsEngineType = false;
        bool bSupportsVariables = true;
    };

    TMap<FString, FVariableTypeRecord> RecordsByName;

    auto TryInsertRecord = [&RecordsByName](FVariableTypeRecord&& Record)
    {
        if (Record.DisplayName.IsEmpty())
        {
            Record.DisplayName = Record.Name;
        }
        if (Record.Description.IsEmpty())
        {
            Record.Description = Record.DisplayName;
        }

        if (FVariableTypeRecord* Existing = RecordsByName.Find(Record.Name))
        {
            const bool bExistingIsEngine = Existing->bIsEngineType;
            const bool bNewIsProject = !Record.bIsEngineType;
            const bool bUpgradeBlueprint = !Existing->bIsBlueprintType && Record.bIsBlueprintType;

            if ((bExistingIsEngine && bNewIsProject) || bUpgradeBlueprint)
            {
                *Existing = MoveTemp(Record);
            }
        }
        else
        {
            RecordsByName.Add(Record.Name, MoveTemp(Record));
        }
    };

    auto IsEnginePackage = [](const FString& PackageName) -> bool
    {
        return !PackageName.StartsWith(TEXT("/Game/"));
    };

    // ---------------------------------------------------------------------
    // Basic types (always available)
    // ---------------------------------------------------------------------
    auto AddBasicType = [&TryInsertRecord](const TCHAR* Name, const TCHAR* Description)
    {
        FVariableTypeRecord Record;
        Record.Name = Name;
        Record.DisplayName = Name;
        Record.Description = Description;
        Record.Category = TEXT("Basic");
        Record.TypeKind = TEXT("basic");
        Record.bIsEngineType = false;
        Record.bIsAssetType = false;
        Record.bIsBlueprintType = false;
        TryInsertRecord(MoveTemp(Record));
    };

    AddBasicType(TEXT("Boolean"), TEXT("True/false value"));
    AddBasicType(TEXT("Byte"), TEXT("8-bit unsigned integer (0-255)"));
    AddBasicType(TEXT("Integer"), TEXT("32-bit signed integer"));
    AddBasicType(TEXT("Integer64"), TEXT("64-bit signed integer"));
    AddBasicType(TEXT("Float"), TEXT("32-bit floating point number"));
    AddBasicType(TEXT("Double"), TEXT("64-bit floating point number"));
    AddBasicType(TEXT("Name"), TEXT("Unreal name identifier"));
    AddBasicType(TEXT("String"), TEXT("Text string value"));
    AddBasicType(TEXT("Text"), TEXT("Localizable text value"));

    // ---------------------------------------------------------------------
    // Struct types (native + user defined)
    // ---------------------------------------------------------------------
    for (TObjectIterator<UScriptStruct> StructIterator; StructIterator; ++StructIterator)
    {
        UScriptStruct* Struct = *StructIterator;
        if (!Struct || Struct->HasAnyFlags(RF_Transient))
        {
            continue;
        }

        const FString StructName = Struct->GetName();
        if (IsTransientTypeName(StructName))
        {
            continue;
        }

        const FString PackageName = Struct->GetOutermost()->GetName();
        const bool bIsEngineStruct = IsEnginePackage(PackageName);
        const bool bIsProjectStruct = !bIsEngineStruct;

        if (!bIncludeEngine && bIsEngineStruct)
        {
            continue;
        }

        if (!bIncludeBlueprints && bIsProjectStruct)
        {
            continue;
        }

        const bool bBlueprintVisible = Struct->HasMetaData(TEXT("BlueprintType"));

        if (!bBlueprintVisible)
        {
            continue;
        }

        FVariableTypeRecord Record;
        Record.Name = StructName;
        Record.DisplayName = Struct->GetDisplayNameText().ToString();
        Record.Category = TEXT("Structure");
        Record.TypeKind = TEXT("struct");
        Record.Description = Struct->GetToolTipText().ToString();
        Record.Path = Struct->GetPathName();
        Record.bIsBlueprintType = bIsProjectStruct;
        Record.bIsAssetType = false;
        Record.bIsEngineType = bIsEngineStruct;
        TryInsertRecord(MoveTemp(Record));
    }

    // ---------------------------------------------------------------------
    // Class types (native + blueprint generated)
    // ---------------------------------------------------------------------
    for (TObjectIterator<UClass> ClassIterator; ClassIterator; ++ClassIterator)
    {
        UClass* Class = *ClassIterator;
        if (!IsValidBlueprintVariableType(Class))
        {
            continue;
        }

        const FString ClassName = Class->GetName();
        if (IsTransientTypeName(ClassName))
        {
            continue;
        }

        const bool bIsBlueprintClass = Class->ClassGeneratedBy != nullptr;
        if (!bIncludeBlueprints && bIsBlueprintClass)
        {
            continue;
        }

        const FString PackageName = Class->GetOutermost()->GetName();
        const bool bIsEngineClass = IsEnginePackage(PackageName);
        if (!bIncludeEngine && bIsEngineClass)
        {
            continue;
        }

        FVariableTypeRecord Record;
        Record.Name = ClassName;
        Record.DisplayName = Class->GetDisplayNameText().ToString();
        Record.Description = Class->GetToolTipText().ToString();
        Record.Path = Class->GetPathName();
        Record.bIsBlueprintType = bIsBlueprintClass;
        Record.bIsEngineType = bIsEngineClass;
        Record.bIsAssetType = !Class->IsChildOf(AActor::StaticClass()) && !Class->IsChildOf(UActorComponent::StaticClass());
        Record.Category = Class->HasAnyClassFlags(CLASS_Interface) ? TEXT("Interface") : TEXT("Object Types");
        Record.TypeKind = Class->HasAnyClassFlags(CLASS_Interface) ? TEXT("interface") : TEXT("class");
        TryInsertRecord(MoveTemp(Record));
    }

    // ---------------------------------------------------------------------
    // Enum types (native + user defined)
    // ---------------------------------------------------------------------
    for (TObjectIterator<UEnum> EnumIterator; EnumIterator; ++EnumIterator)
    {
        UEnum* Enum = *EnumIterator;
        if (!Enum || Enum->HasAnyFlags(RF_Transient))
        {
            continue;
        }

        const FString EnumName = Enum->GetName();
        if (IsTransientTypeName(EnumName))
        {
            continue;
        }

        const bool bIsUserDefinedEnum = Enum->IsA<UUserDefinedEnum>();
        const bool bBlueprintVisible = bIsUserDefinedEnum || Enum->HasMetaData(TEXT("BlueprintType"));

        if (!bBlueprintVisible)
        {
            continue;
        }

        if (!bIncludeBlueprints && bIsUserDefinedEnum)
        {
            continue;
        }

        const FString PackageName = Enum->GetOutermost()->GetName();
        const bool bIsEngineEnum = IsEnginePackage(PackageName);
        if (!bIncludeEngine && bIsEngineEnum)
        {
            continue;
        }

        FVariableTypeRecord Record;
        Record.Name = EnumName;
        Record.DisplayName = Enum->GetDisplayNameText().ToString();
        Record.Description = Enum->GetToolTipText().ToString();
        Record.Path = Enum->GetPathName();
        Record.Category = TEXT("Enum");
        Record.TypeKind = TEXT("enum");
        Record.bIsBlueprintType = bIsUserDefinedEnum;
        Record.bIsAssetType = false;
        Record.bIsEngineType = bIsEngineEnum;
        TryInsertRecord(MoveTemp(Record));
    }

    // Convert map to array and sort
    TArray<FVariableTypeRecord> SortedRecords;
    RecordsByName.GenerateValueArray(SortedRecords);

    SortedRecords.Sort([&GetCategoryRank](const FVariableTypeRecord& A, const FVariableTypeRecord& B)
    {
        const int32 RankA = GetCategoryRank(A.Category);
        const int32 RankB = GetCategoryRank(B.Category);

        if (RankA != RankB)
        {
            return RankA < RankB;
        }

        return A.DisplayName.Compare(B.DisplayName, ESearchCase::IgnoreCase) < 0;
    });

    TArray<TSharedPtr<FJsonValue>> TypesArray;
    TSet<FString> Categories;

    const bool bHasCategoryFilter = !CategoryFilter.IsEmpty();
    const bool bHasSearchFilter = !SearchText.IsEmpty();

    for (const FVariableTypeRecord& Record : SortedRecords)
    {
        if (!Record.bSupportsVariables)
        {
            continue;
        }

        if (bHasCategoryFilter && !Record.Category.Equals(CategoryFilter, ESearchCase::IgnoreCase))
        {
            continue;
        }

        if (bHasSearchFilter)
        {
            const bool bMatches =
                Record.Name.Contains(SearchText, ESearchCase::IgnoreCase) ||
                Record.DisplayName.Contains(SearchText, ESearchCase::IgnoreCase) ||
                Record.Description.Contains(SearchText, ESearchCase::IgnoreCase) ||
                Record.Path.Contains(SearchText, ESearchCase::IgnoreCase);

            if (!bMatches)
            {
                continue;
            }
        }

        TSharedPtr<FJsonObject> TypeInfo = MakeShared<FJsonObject>();
        TypeInfo->SetStringField(TEXT("name"), Record.Name);
        TypeInfo->SetStringField(TEXT("display_name"), Record.DisplayName);
        TypeInfo->SetStringField(TEXT("category"), Record.Category);
        TypeInfo->SetStringField(TEXT("description"), Record.Description);
        TypeInfo->SetBoolField(TEXT("is_blueprint_class"), Record.bIsBlueprintType);
        TypeInfo->SetBoolField(TEXT("is_asset_type"), Record.bIsAssetType);
        TypeInfo->SetBoolField(TEXT("supports_variables"), Record.bSupportsVariables);
        TypeInfo->SetBoolField(TEXT("is_engine_type"), Record.bIsEngineType);
        TypeInfo->SetStringField(TEXT("type_kind"), Record.TypeKind);

        if (!Record.Path.IsEmpty())
        {
            TypeInfo->SetStringField(TEXT("type_path"), Record.Path);
        }

        TypesArray.Add(MakeShared<FJsonValueObject>(TypeInfo));
        Categories.Add(Record.Category);
    }

    // Build sorted category list for UI parity
    TArray<FString> SortedCategories = Categories.Array();
    SortedCategories.Sort([&GetCategoryRank](const FString& A, const FString& B)
    {
        const int32 RankA = GetCategoryRank(A);
        const int32 RankB = GetCategoryRank(B);

        if (RankA != RankB)
        {
            return RankA < RankB;
        }

        return A.Compare(B, ESearchCase::IgnoreCase) < 0;
    });

    TArray<TSharedPtr<FJsonValue>> CategoriesArray;
    for (const FString& Category : SortedCategories)
    {
        CategoriesArray.Add(MakeShared<FJsonValueString>(Category));
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("action"), TEXT("search_types"));
    Response->SetArrayField(TEXT("types"), TypesArray);
    Response->SetArrayField(TEXT("categories"), CategoriesArray);
    Response->SetNumberField(TEXT("total_count"), TypesArray.Num());

    return Response;
}

// Placeholder implementations for remaining operations
TSharedPtr<FJsonObject> FBlueprintCommands::HandleDeleteVariableOperation(const TSharedPtr<FJsonObject>& Params)
{
    // Delegate to existing implementation for now
    return HandleDeleteBlueprintVariable(Params);
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleGetVariableInfoOperation(const TSharedPtr<FJsonObject>& Params)
{
    // Delegate to existing implementation for now
    return HandleGetBlueprintVariableInfo(Params);
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleGetPropertyOperation(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    FString PropertyPath;
    if (!Params->TryGetStringField(TEXT("property_path"), PropertyPath))
    {
        Params->TryGetStringField(TEXT("path"), PropertyPath);
    }

    // Build the full path: VariableName.PropertyPath (or just VariableName if no property path)
    FString FullPath = VariableName;
    if (!PropertyPath.IsEmpty())
    {
        FullPath += TEXT(".") + PropertyPath;
    }

    TSharedPtr<FJsonObject> NormalizedParams = MakeShared<FJsonObject>();
    NormalizedParams->SetStringField(TEXT("blueprint_name"), BlueprintName);
    NormalizedParams->SetStringField(TEXT("path"), FullPath);

    return HandleGetVariableProperty(NormalizedParams);
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleSetPropertyOperation(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    FString PropertyPath;
    if (!Params->TryGetStringField(TEXT("property_path"), PropertyPath))
    {
        Params->TryGetStringField(TEXT("path"), PropertyPath);
    }

    TSharedPtr<FJsonValue> ValueField = Params->TryGetField(TEXT("value"));
    if (!ValueField.IsValid())
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'value' parameter"));
    }

    // Build the full path: VariableName.PropertyPath (or just VariableName if no property path)
    FString FullPath = VariableName;
    if (!PropertyPath.IsEmpty())
    {
        FullPath += TEXT(".") + PropertyPath;
    }

    TSharedPtr<FJsonObject> NormalizedParams = MakeShared<FJsonObject>();
    NormalizedParams->SetStringField(TEXT("blueprint_name"), BlueprintName);
    NormalizedParams->SetStringField(TEXT("path"), FullPath);
    NormalizedParams->SetField(TEXT("value"), ValueField);

    return HandleSetVariableProperty(NormalizedParams);
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleModifyVariableOperation(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString VariableName;
    if (!Params->TryGetStringField(TEXT("variable_name"), VariableName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_name' parameter"));
    }

    const TSharedPtr<FJsonObject>* VariableConfig = nullptr;
    if (!Params->TryGetObjectField(TEXT("variable_config"), VariableConfig))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'variable_config' object"));
    }

    UBlueprint* Blueprint = FCommonUtils::FindBlueprintByName(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }

    FName VarName(*VariableName);
    FBPVariableDescription* VarDesc = nullptr;
    for (FBPVariableDescription& Description : Blueprint->NewVariables)
    {
        if (Description.VarName == VarName)
        {
            VarDesc = &Description;
            break;
        }
    }

    if (!VarDesc)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Variable '%s' not found in Blueprint '%s'"), *VariableName, *BlueprintName));
    }

    bool bAnyChanges = false;
    TArray<TSharedPtr<FJsonValue>> UpdatedFields;

    auto TrackChange = [&UpdatedFields](const FString& FieldName)
    {
        UpdatedFields.Add(MakeShared<FJsonValueString>(FieldName));
    };

    FString NewCategory;
    if ((*VariableConfig)->TryGetStringField(TEXT("category"), NewCategory))
    {
        VarDesc->Category = FText::FromString(NewCategory);
        bAnyChanges = true;
        TrackChange(TEXT("category"));
    }

    FString NewTooltip;
    if ((*VariableConfig)->TryGetStringField(TEXT("tooltip"), NewTooltip))
    {
        VarDesc->FriendlyName = NewTooltip;
        bAnyChanges = true;
        TrackChange(TEXT("tooltip"));
    }

    auto SetFlag = [&](uint64 Flag, bool bEnable)
    {
        if (bEnable)
        {
            VarDesc->PropertyFlags |= Flag;
        }
        else
        {
            VarDesc->PropertyFlags &= ~Flag;
        }
    };

    bool bTempBool = false;
    if ((*VariableConfig)->TryGetBoolField(TEXT("is_editable"), bTempBool))
    {
        if (bTempBool)
        {
            VarDesc->PropertyFlags |= CPF_Edit;
            VarDesc->PropertyFlags |= CPF_BlueprintVisible;
            VarDesc->PropertyFlags &= ~CPF_DisableEditOnInstance;
        }
        else
        {
            VarDesc->PropertyFlags &= ~CPF_Edit;
            VarDesc->PropertyFlags &= ~CPF_BlueprintVisible;
            VarDesc->PropertyFlags |= CPF_DisableEditOnInstance;
        }
        bAnyChanges = true;
        TrackChange(TEXT("is_editable"));
    }

    if ((*VariableConfig)->TryGetBoolField(TEXT("is_blueprint_readonly"), bTempBool))
    {
        SetFlag(CPF_BlueprintReadOnly, bTempBool);
        bAnyChanges = true;
        TrackChange(TEXT("is_blueprint_readonly"));
    }

    if ((*VariableConfig)->TryGetBoolField(TEXT("is_expose_on_spawn"), bTempBool))
    {
        SetFlag(CPF_ExposeOnSpawn, bTempBool);
        bAnyChanges = true;
        TrackChange(TEXT("is_expose_on_spawn"));
    }

    if ((*VariableConfig)->TryGetBoolField(TEXT("is_private"), bTempBool))
    {
        SetFlag(CPF_DisableEditOnInstance, bTempBool);
        bAnyChanges = true;
        TrackChange(TEXT("is_private"));
    }

    if ((*VariableConfig)->TryGetBoolField(TEXT("replicated"), bTempBool))
    {
        SetFlag(CPF_Net, bTempBool);
        bAnyChanges = true;
        TrackChange(TEXT("replicated"));
    }

    FString DefaultValue;
    bool bDefaultValueUpdated = false;
    if ((*VariableConfig)->TryGetStringField(TEXT("default_value"), DefaultValue))
    {
        TSharedPtr<FJsonObject> SetParams = MakeShared<FJsonObject>();
        SetParams->SetStringField(TEXT("blueprint_name"), BlueprintName);
        SetParams->SetStringField(TEXT("path"), VariableName);
        SetParams->SetField(TEXT("value"), MakeShared<FJsonValueString>(DefaultValue));
        TSharedPtr<FJsonObject> SetResponse = HandleSetVariableProperty(SetParams);
        if (SetResponse.IsValid() && SetResponse->GetBoolField(TEXT("success")))
        {
            bAnyChanges = true;
            bDefaultValueUpdated = true;
            TrackChange(TEXT("default_value"));
        }
        else
        {
            FString ErrorMessage = TEXT("Failed to update default value for variable");
            if (SetResponse.IsValid() && SetResponse->HasField(TEXT("error")))
            {
                ErrorMessage = SetResponse->GetStringField(TEXT("error"));
            }
            return FCommonUtils::CreateErrorResponse(ErrorMessage);
        }
    }

    const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
    if ((*VariableConfig)->TryGetObjectField(TEXT("metadata"), MetadataObject))
    {
        for (const auto& Pair : (*MetadataObject)->Values)
        {
            FString ValueString = JsonValueToString(Pair.Value);
            FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, VarName, nullptr, FName(*Pair.Key), ValueString);
            TrackChange(FString::Printf(TEXT("metadata.%s"), *Pair.Key));
        }
        bAnyChanges = true;
    }

    if (!bAnyChanges)
    {
        TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), TEXT("No changes were applied"));
        Response->SetArrayField(TEXT("updated_fields"), UpdatedFields);
        Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
        Response->SetStringField(TEXT("variable_name"), VariableName);
        return Response;
    }

    Blueprint->MarkPackageDirty();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    FString CompileError;
    FCommonUtils::SafeCompileBlueprint(Blueprint, CompileError);

    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetStringField(TEXT("variable_name"), VariableName);
    Response->SetArrayField(TEXT("updated_fields"), UpdatedFields);
    Response->SetBoolField(TEXT("default_value_updated"), bDefaultValueUpdated);
    Response->SetStringField(TEXT("message"), TEXT("Variable updated successfully"));
    if (!CompileError.IsEmpty())
    {
        Response->SetStringField(TEXT("compile_warning"), CompileError);
    }

    return Response;
}

TSharedPtr<FJsonObject> FBlueprintCommands::HandleListVariablesOperation(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FCommonUtils::FindBlueprintByName(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }

    FString CategoryFilter;
    FString NameContains;
    bool bIncludePrivate = true;
    bool bIncludeMetadata = false;

    const TSharedPtr<FJsonObject>* ListCriteria = nullptr;
    if (Params->TryGetObjectField(TEXT("list_criteria"), ListCriteria))
    {
        (*ListCriteria)->TryGetStringField(TEXT("category"), CategoryFilter);
        (*ListCriteria)->TryGetStringField(TEXT("name_contains"), NameContains);
        (*ListCriteria)->TryGetBoolField(TEXT("include_private"), bIncludePrivate);
        (*ListCriteria)->TryGetBoolField(TEXT("include_metadata"), bIncludeMetadata);
    }

    TArray<TSharedPtr<FJsonValue>> VariablesArray;

    for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        const FString VarName = VarDesc.VarName.ToString();
        const FString VarCategory = VarDesc.Category.ToString();

        if (!CategoryFilter.IsEmpty() && !VarCategory.Equals(CategoryFilter, ESearchCase::IgnoreCase))
        {
            continue;
        }

        if (!NameContains.IsEmpty() && !VarName.Contains(NameContains, ESearchCase::IgnoreCase))
        {
            continue;
        }

        if (!bIncludePrivate && (VarDesc.PropertyFlags & CPF_DisableEditOnInstance))
        {
            continue;
        }

        TSharedPtr<FJsonObject> VarInfo = MakeShared<FJsonObject>();
        VarInfo->SetStringField(TEXT("name"), VarName);
        VarInfo->SetStringField(TEXT("display_type"), UEdGraphSchema_K2::TypeToText(VarDesc.VarType).ToString());
        VarInfo->SetStringField(TEXT("pin_category"), VarDesc.VarType.PinCategory.ToString());

        if (VarDesc.VarType.PinSubCategoryObject.IsValid())
        {
            VarInfo->SetStringField(TEXT("sub_category_object"), VarDesc.VarType.PinSubCategoryObject->GetPathName());
        }

        VarInfo->SetStringField(TEXT("category"), VarCategory);
        VarInfo->SetStringField(TEXT("tooltip"), VarDesc.FriendlyName);
        VarInfo->SetStringField(TEXT("default_value"), VarDesc.DefaultValue);
        VarInfo->SetStringField(TEXT("container_type"), ContainerTypeToString(VarDesc.VarType.ContainerType));
        VarInfo->SetBoolField(TEXT("is_editable"), (VarDesc.PropertyFlags & CPF_Edit) != 0);
        VarInfo->SetBoolField(TEXT("is_blueprint_readonly"), (VarDesc.PropertyFlags & CPF_BlueprintReadOnly) != 0);
        VarInfo->SetBoolField(TEXT("is_expose_on_spawn"), (VarDesc.PropertyFlags & CPF_ExposeOnSpawn) != 0);
        VarInfo->SetBoolField(TEXT("is_private"), (VarDesc.PropertyFlags & CPF_DisableEditOnInstance) != 0);
        VarInfo->SetBoolField(TEXT("is_replicated"), (VarDesc.PropertyFlags & CPF_Net) != 0);

        if (bIncludeMetadata)
        {
            TSharedPtr<FJsonObject> MetadataParams = MakeShared<FJsonObject>();
            MetadataParams->SetStringField(TEXT("blueprint_name"), BlueprintName);
            MetadataParams->SetStringField(TEXT("variable_name"), VarName);
            TSharedPtr<FJsonObject> MetadataResponse = GetBlueprintVariableMetadata(MetadataParams);
            if (MetadataResponse->GetBoolField(TEXT("success")))
            {
                const TSharedPtr<FJsonObject>* MetadataObjectPtr = nullptr;
                if (MetadataResponse->TryGetObjectField(TEXT("metadata"), MetadataObjectPtr))
                {
                    VarInfo->SetObjectField(TEXT("metadata"), *MetadataObjectPtr);
                }
            }
        }

        VariablesArray.Add(MakeShared<FJsonValueObject>(VarInfo));
    }

    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Response->SetNumberField(TEXT("total_count"), VariablesArray.Num());
    Response->SetArrayField(TEXT("variables"), VariablesArray);

    return Response;
}
