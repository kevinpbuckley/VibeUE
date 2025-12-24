// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Commands/DataAssetCommands.h"
#include "Core/ServiceContext.h"
#include "Utils/HelpFileReader.h"
#include "Utils/ParamValidation.h"
#include "Engine/DataAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataAssetFactory.h"
#include "UObject/SavePackage.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectIterator.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataAssetCommands, Log, All);

FDataAssetCommands::FDataAssetCommands()
{
    ServiceContext = MakeShared<FServiceContext>();
}

TSharedPtr<FJsonObject> FDataAssetCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType != TEXT("manage_data_asset"))
    {
        return CreateErrorResponse(FString::Printf(TEXT("Unknown command type: %s"), *CommandType));
    }

    if (!Params.IsValid())
    {
        return CreateErrorResponse(TEXT("Parameters are required"));
    }

    FString Action;
    if (!Params->TryGetStringField(TEXT("action"), Action))
    {
        return CreateErrorResponse(TEXT("action parameter is required"));
    }

    Action = Action.ToLower();
    UE_LOG(LogDataAssetCommands, Display, TEXT("DataAssetCommands: Handling action '%s'"), *Action);

    // Route to action handlers
    if (Action == TEXT("help"))
    {
        return HandleHelp(Params);
    }
    else if (Action == TEXT("search_types") || Action == TEXT("list_types") || Action == TEXT("get_available_types"))
    {
        return HandleSearchTypes(Params);
    }
    else if (Action == TEXT("list"))
    {
        return HandleList(Params);
    }
    else if (Action == TEXT("create"))
    {
        return HandleCreate(Params);
    }
    else if (Action == TEXT("get_info"))
    {
        return HandleGetInfo(Params);
    }
    else if (Action == TEXT("list_properties"))
    {
        return HandleListProperties(Params);
    }
    else if (Action == TEXT("get_property"))
    {
        return HandleGetProperty(Params);
    }
    else if (Action == TEXT("set_property"))
    {
        return HandleSetProperty(Params);
    }
    else if (Action == TEXT("set_properties"))
    {
        return HandleSetProperties(Params);
    }
    else if (Action == TEXT("get_class_info"))
    {
        return HandleGetClassInfo(Params);
    }
    else
    {
        return CreateErrorResponse(FString::Printf(TEXT("Unknown action: %s. Use action='help' for available actions."), *Action));
    }
}

// ========== Help ==========

TSharedPtr<FJsonObject> FDataAssetCommands::HandleHelp(const TSharedPtr<FJsonObject>& Params)
{
    return FHelpFileReader::HandleHelp(TEXT("manage_data_asset"), Params);
}

// ========== Discovery Actions ==========

TSharedPtr<FJsonObject> FDataAssetCommands::HandleSearchTypes(const TSharedPtr<FJsonObject>& Params)
{
    FString SearchFilter;
    Params->TryGetStringField(TEXT("search_filter"), SearchFilter);
    Params->TryGetStringField(TEXT("search_text"), SearchFilter); // Alias
    
    TArray<TSharedPtr<FJsonValue>> TypesArray;
    
    // Get all classes derived from UDataAsset
    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Class = *It;
        
        // Must be a subclass of UDataAsset but not UDataAsset itself
        if (!Class->IsChildOf(UDataAsset::StaticClass()))
        {
            continue;
        }
        
        // Skip abstract classes
        if (Class->HasAnyClassFlags(CLASS_Abstract))
        {
            continue;
        }
        
        // Skip deprecated classes
        if (Class->HasAnyClassFlags(CLASS_Deprecated))
        {
            continue;
        }
        
        FString ClassName = Class->GetName();
        FString ClassPath = Class->GetPathName();
        
        // Apply search filter if provided
        if (!SearchFilter.IsEmpty())
        {
            if (!ClassName.Contains(SearchFilter, ESearchCase::IgnoreCase) &&
                !ClassPath.Contains(SearchFilter, ESearchCase::IgnoreCase))
            {
                continue;
            }
        }
        
        TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
        TypeObj->SetStringField(TEXT("name"), ClassName);
        TypeObj->SetStringField(TEXT("path"), ClassPath);
        
        // Get module info
        UPackage* Package = Class->GetOuterUPackage();
        if (Package)
        {
            TypeObj->SetStringField(TEXT("module"), Package->GetName());
        }
        
        // Check if it's a native C++ class or Blueprint class
        TypeObj->SetBoolField(TEXT("is_native"), !Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint));
        
        // Get parent class
        if (UClass* Super = Class->GetSuperClass())
        {
            TypeObj->SetStringField(TEXT("parent_class"), Super->GetName());
        }
        
        TypesArray.Add(MakeShared<FJsonValueObject>(TypeObj));
    }
    
    TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
    Response->SetArrayField(TEXT("types"), TypesArray);
    Response->SetNumberField(TEXT("count"), TypesArray.Num());
    
    if (!SearchFilter.IsEmpty())
    {
        Response->SetStringField(TEXT("filter"), SearchFilter);
    }
    
    return Response;
}

TSharedPtr<FJsonObject> FDataAssetCommands::HandleList(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetType;
    FString SearchPath = TEXT("/Game");
    
    Params->TryGetStringField(TEXT("asset_type"), AssetType);
    Params->TryGetStringField(TEXT("class_name"), AssetType); // Alias
    Params->TryGetStringField(TEXT("path"), SearchPath);
    
    // Get asset registry
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
    
    TArray<FAssetData> AssetDataList;
    
    // If specific type provided, filter by it
    if (!AssetType.IsEmpty())
    {
        UClass* FilterClass = FindDataAssetClass(AssetType);
        if (!FilterClass)
        {
            return CreateErrorResponse(FString::Printf(TEXT("Could not find data asset class: %s"), *AssetType));
        }
        
        // Use native class name for filtering
        FTopLevelAssetPath ClassPath(FilterClass->GetClassPathName());
        AssetRegistry.GetAssetsByClass(ClassPath, AssetDataList, true);
    }
    else
    {
        // Get all UDataAsset subclasses
        FTopLevelAssetPath DataAssetPath(UDataAsset::StaticClass()->GetClassPathName());
        AssetRegistry.GetAssetsByClass(DataAssetPath, AssetDataList, true);
    }
    
    TArray<TSharedPtr<FJsonValue>> AssetsArray;
    
    for (const FAssetData& AssetData : AssetDataList)
    {
        // Filter by path if specified
        FString AssetPath = AssetData.GetObjectPathString();
        if (!SearchPath.IsEmpty() && !AssetPath.StartsWith(SearchPath))
        {
            continue;
        }
        
        TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
        AssetObj->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
        AssetObj->SetStringField(TEXT("path"), AssetPath);
        AssetObj->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
        
        AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
    }
    
    TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
    Response->SetArrayField(TEXT("assets"), AssetsArray);
    Response->SetNumberField(TEXT("count"), AssetsArray.Num());
    
    return Response;
}

// ========== Asset Lifecycle ==========

TSharedPtr<FJsonObject> FDataAssetCommands::HandleCreate(const TSharedPtr<FJsonObject>& Params)
{
    static const TArray<FString> ValidParams = {
        TEXT("class_name"), TEXT("asset_type"), TEXT("asset_path"), TEXT("destination_path"), 
        TEXT("asset_name"), TEXT("properties")
    };
    
    FString ClassName;
    FString AssetPath;
    FString AssetName;
    
    // Get class name (required)
    if (!Params->TryGetStringField(TEXT("class_name"), ClassName))
    {
        Params->TryGetStringField(TEXT("asset_type"), ClassName);
    }
    
    if (ClassName.IsEmpty())
    {
        return CreateErrorResponseWithParams(TEXT("class_name or asset_type is required"), ValidParams);
    }
    
    // Get asset path and name
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        Params->TryGetStringField(TEXT("destination_path"), AssetPath);
    }
    
    Params->TryGetStringField(TEXT("asset_name"), AssetName);
    
    // If full path with asset name provided
    if (!AssetPath.IsEmpty() && AssetName.IsEmpty())
    {
        // Check if path includes asset name
        int32 LastSlash;
        if (AssetPath.FindLastChar('/', LastSlash))
        {
            FString PotentialName = AssetPath.Mid(LastSlash + 1);
            if (!PotentialName.IsEmpty() && !PotentialName.Contains(TEXT(".")))
            {
                AssetName = PotentialName;
                AssetPath = AssetPath.Left(LastSlash);
            }
        }
    }
    
    if (AssetPath.IsEmpty())
    {
        AssetPath = TEXT("/Game/Data");
    }
    
    if (AssetName.IsEmpty())
    {
        return CreateErrorResponseWithParams(TEXT("asset_name is required (or include it in asset_path)"), ValidParams);
    }
    
    // Find the class
    UClass* DataAssetClass = FindDataAssetClass(ClassName);
    if (!DataAssetClass)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Could not find data asset class: %s. Use search_types action to find available classes."), *ClassName));
    }
    
    // Verify it's a data asset class
    if (!DataAssetClass->IsChildOf(UDataAsset::StaticClass()))
    {
        return CreateErrorResponse(FString::Printf(TEXT("%s is not a DataAsset class"), *ClassName));
    }
    
    // Create the asset using asset tools
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    
    // Create factory
    UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
    Factory->DataAssetClass = DataAssetClass;
    
    FString FullPath = AssetPath / AssetName;
    FString PackagePath = AssetPath;
    
    UObject* NewAsset = AssetTools.CreateAsset(AssetName, PackagePath, DataAssetClass, Factory);
    
    if (!NewAsset)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Failed to create data asset at %s"), *FullPath));
    }
    
    UDataAsset* DataAsset = Cast<UDataAsset>(NewAsset);
    
    // Apply initial properties if provided
    const TSharedPtr<FJsonObject>* PropertiesObj;
    if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
    {
        for (auto& Pair : (*PropertiesObj)->Values)
        {
            FProperty* Property = DataAssetClass->FindPropertyByName(*Pair.Key);
            if (Property && ShouldExposeProperty(Property))
            {
                FString Error;
                if (!JsonToProperty(Property, DataAsset, Pair.Value, Error))
                {
                    UE_LOG(LogDataAssetCommands, Warning, TEXT("Failed to set initial property %s: %s"), *Pair.Key, *Error);
                }
            }
        }
    }
    
    // Mark dirty
    NewAsset->MarkPackageDirty();
    
    TSharedPtr<FJsonObject> Response = CreateSuccessResponse(FString::Printf(TEXT("Created data asset: %s"), *FullPath));
    Response->SetStringField(TEXT("asset_path"), NewAsset->GetPathName());
    Response->SetStringField(TEXT("asset_name"), AssetName);
    Response->SetStringField(TEXT("class_name"), DataAssetClass->GetName());
    
    return Response;
}

// ========== Property Reflection ==========

TSharedPtr<FJsonObject> FDataAssetCommands::HandleGetInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return CreateErrorResponse(TEXT("asset_path is required"));
    }
    
    UDataAsset* DataAsset = LoadDataAsset(AssetPath);
    if (!DataAsset)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Could not find data asset: %s"), *AssetPath));
    }
    
    UClass* AssetClass = DataAsset->GetClass();
    
    TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
    Response->SetStringField(TEXT("name"), DataAsset->GetName());
    Response->SetStringField(TEXT("path"), DataAsset->GetPathName());
    Response->SetStringField(TEXT("class"), AssetClass->GetName());
    Response->SetStringField(TEXT("class_path"), AssetClass->GetPathName());
    
    // Get parent class chain
    TArray<TSharedPtr<FJsonValue>> ParentChain;
    UClass* CurrentClass = AssetClass->GetSuperClass();
    while (CurrentClass && CurrentClass != UObject::StaticClass())
    {
        ParentChain.Add(MakeShared<FJsonValueString>(CurrentClass->GetName()));
        CurrentClass = CurrentClass->GetSuperClass();
    }
    Response->SetArrayField(TEXT("parent_classes"), ParentChain);
    
    // Get all properties with values
    TSharedPtr<FJsonObject> PropertiesObj = MakeShared<FJsonObject>();
    
    for (TFieldIterator<FProperty> PropIt(AssetClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        if (!ShouldExposeProperty(Property))
        {
            continue;
        }
        
        TSharedPtr<FJsonValue> Value = PropertyToJson(Property, DataAsset);
        if (Value.IsValid())
        {
            PropertiesObj->SetField(Property->GetName(), Value);
        }
    }
    
    Response->SetObjectField(TEXT("properties"), PropertiesObj);
    
    return Response;
}

TSharedPtr<FJsonObject> FDataAssetCommands::HandleListProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    FString ClassName;
    bool bIncludeAll = false;
    
    Params->TryGetStringField(TEXT("asset_path"), AssetPath);
    Params->TryGetStringField(TEXT("class_name"), ClassName);
    Params->TryGetBoolField(TEXT("include_all"), bIncludeAll);
    
    UClass* AssetClass = nullptr;
    
    if (!AssetPath.IsEmpty())
    {
        UDataAsset* DataAsset = LoadDataAsset(AssetPath);
        if (!DataAsset)
        {
            return CreateErrorResponse(FString::Printf(TEXT("Could not find data asset: %s"), *AssetPath));
        }
        AssetClass = DataAsset->GetClass();
    }
    else if (!ClassName.IsEmpty())
    {
        AssetClass = FindDataAssetClass(ClassName);
        if (!AssetClass)
        {
            return CreateErrorResponse(FString::Printf(TEXT("Could not find class: %s"), *ClassName));
        }
    }
    else
    {
        return CreateErrorResponse(TEXT("Either asset_path or class_name is required"));
    }
    
    TArray<TSharedPtr<FJsonValue>> PropertiesArray;
    
    for (TFieldIterator<FProperty> PropIt(AssetClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        if (!ShouldExposeProperty(Property, bIncludeAll))
        {
            continue;
        }
        
        TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
        PropObj->SetStringField(TEXT("name"), Property->GetName());
        PropObj->SetStringField(TEXT("type"), GetPropertyTypeString(Property));
        PropObj->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
        
        // Get tooltip/description
        FString Tooltip = Property->GetMetaData(TEXT("ToolTip"));
        if (!Tooltip.IsEmpty())
        {
            PropObj->SetStringField(TEXT("description"), Tooltip);
        }
        
        // Check if read-only
        PropObj->SetBoolField(TEXT("read_only"), Property->HasAnyPropertyFlags(CPF_EditConst));
        
        // Check if it's an array
        PropObj->SetBoolField(TEXT("is_array"), Property->IsA<FArrayProperty>());
        
        // Get owning class
        PropObj->SetStringField(TEXT("defined_in"), Property->GetOwnerClass()->GetName());
        
        // Add property flags info when including all
        if (bIncludeAll)
        {
            TArray<FString> Flags;
            if (Property->HasAnyPropertyFlags(CPF_Edit)) Flags.Add(TEXT("Edit"));
            if (Property->HasAnyPropertyFlags(CPF_BlueprintVisible)) Flags.Add(TEXT("BlueprintVisible"));
            if (Property->HasAnyPropertyFlags(CPF_SaveGame)) Flags.Add(TEXT("SaveGame"));
            if (Property->HasAnyPropertyFlags(CPF_EditConst)) Flags.Add(TEXT("EditConst"));
            if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate)) Flags.Add(TEXT("Private"));
            if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierProtected)) Flags.Add(TEXT("Protected"));
            if (Property->HasAnyPropertyFlags(CPF_Transient)) Flags.Add(TEXT("Transient"));
            PropObj->SetStringField(TEXT("flags"), FString::Join(Flags, TEXT(", ")));
        }
        
        PropertiesArray.Add(MakeShared<FJsonValueObject>(PropObj));
    }
    
    TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
    Response->SetArrayField(TEXT("properties"), PropertiesArray);
    Response->SetNumberField(TEXT("count"), PropertiesArray.Num());
    Response->SetStringField(TEXT("class"), AssetClass->GetName());
    if (bIncludeAll)
    {
        Response->SetBoolField(TEXT("include_all"), true);
        Response->SetStringField(TEXT("note"), TEXT("Showing all properties including non-editable. Only properties with Edit/BlueprintVisible/SaveGame flags can be modified."));
    }
    
    return Response;
}

TSharedPtr<FJsonObject> FDataAssetCommands::HandleGetProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    FString PropertyName;
    
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return CreateErrorResponse(TEXT("asset_path is required"));
    }
    
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return CreateErrorResponse(TEXT("property_name is required"));
    }
    
    UDataAsset* DataAsset = LoadDataAsset(AssetPath);
    if (!DataAsset)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Could not find data asset: %s"), *AssetPath));
    }
    
    UClass* AssetClass = DataAsset->GetClass();
    FProperty* Property = AssetClass->FindPropertyByName(*PropertyName);
    
    if (!Property)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Property not found: %s"), *PropertyName));
    }
    
    TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
    Response->SetStringField(TEXT("property_name"), PropertyName);
    Response->SetStringField(TEXT("type"), GetPropertyTypeString(Property));
    Response->SetField(TEXT("value"), PropertyToJson(Property, DataAsset));
    
    return Response;
}

TSharedPtr<FJsonObject> FDataAssetCommands::HandleSetProperty(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    FString PropertyName;
    
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return CreateErrorResponse(TEXT("asset_path is required"));
    }
    
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return CreateErrorResponse(TEXT("property_name is required"));
    }
    
    UDataAsset* DataAsset = LoadDataAsset(AssetPath);
    if (!DataAsset)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Could not find data asset: %s"), *AssetPath));
    }
    
    UClass* AssetClass = DataAsset->GetClass();
    FProperty* Property = AssetClass->FindPropertyByName(*PropertyName);
    
    if (!Property)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Property not found: %s"), *PropertyName));
    }
    
    if (!ShouldExposeProperty(Property))
    {
        return CreateErrorResponse(FString::Printf(TEXT("Property is not editable: %s"), *PropertyName));
    }
    
    // Get the value to set
    TSharedPtr<FJsonValue> Value = Params->TryGetField(TEXT("property_value"));
    if (!Value.IsValid())
    {
        Value = Params->TryGetField(TEXT("value"));
    }
    
    if (!Value.IsValid())
    {
        return CreateErrorResponse(TEXT("property_value is required"));
    }
    
    FString Error;
    if (!JsonToProperty(Property, DataAsset, Value, Error))
    {
        return CreateErrorResponse(FString::Printf(TEXT("Failed to set property: %s"), *Error));
    }
    
    DataAsset->MarkPackageDirty();
    
    TSharedPtr<FJsonObject> Response = CreateSuccessResponse(FString::Printf(TEXT("Set property %s"), *PropertyName));
    Response->SetStringField(TEXT("property_name"), PropertyName);
    Response->SetField(TEXT("new_value"), PropertyToJson(Property, DataAsset));
    
    return Response;
}

TSharedPtr<FJsonObject> FDataAssetCommands::HandleSetProperties(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return CreateErrorResponse(TEXT("asset_path is required"));
    }
    
    const TSharedPtr<FJsonObject>* PropertiesObj;
    if (!Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
    {
        return CreateErrorResponse(TEXT("properties object is required"));
    }
    
    UDataAsset* DataAsset = LoadDataAsset(AssetPath);
    if (!DataAsset)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Could not find data asset: %s"), *AssetPath));
    }
    
    UClass* AssetClass = DataAsset->GetClass();
    
    TArray<FString> SuccessProperties;
    TArray<FString> FailedProperties;
    
    for (auto& Pair : (*PropertiesObj)->Values)
    {
        FProperty* Property = AssetClass->FindPropertyByName(*Pair.Key);
        if (!Property)
        {
            FailedProperties.Add(FString::Printf(TEXT("%s: not found"), *Pair.Key));
            continue;
        }
        
        if (!ShouldExposeProperty(Property))
        {
            FailedProperties.Add(FString::Printf(TEXT("%s: not editable"), *Pair.Key));
            continue;
        }
        
        FString Error;
        if (JsonToProperty(Property, DataAsset, Pair.Value, Error))
        {
            SuccessProperties.Add(Pair.Key);
        }
        else
        {
            FailedProperties.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Error));
        }
    }
    
    DataAsset->MarkPackageDirty();
    
    TSharedPtr<FJsonObject> Response = CreateSuccessResponse(FString::Printf(TEXT("Set %d properties"), SuccessProperties.Num()));
    
    TArray<TSharedPtr<FJsonValue>> SuccessArray;
    for (const FString& Name : SuccessProperties)
    {
        SuccessArray.Add(MakeShared<FJsonValueString>(Name));
    }
    Response->SetArrayField(TEXT("success"), SuccessArray);
    
    if (FailedProperties.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> FailedArray;
        for (const FString& Name : FailedProperties)
        {
            FailedArray.Add(MakeShared<FJsonValueString>(Name));
        }
        Response->SetArrayField(TEXT("failed"), FailedArray);
    }
    
    return Response;
}

TSharedPtr<FJsonObject> FDataAssetCommands::HandleGetClassInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString ClassName;
    bool bIncludeAll = false;
    
    if (!Params->TryGetStringField(TEXT("class_name"), ClassName))
    {
        return CreateErrorResponse(TEXT("class_name is required"));
    }
    Params->TryGetBoolField(TEXT("include_all"), bIncludeAll);
    
    UClass* AssetClass = FindDataAssetClass(ClassName);
    if (!AssetClass)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Could not find class: %s"), *ClassName));
    }
    
    TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
    Response->SetStringField(TEXT("name"), AssetClass->GetName());
    Response->SetStringField(TEXT("path"), AssetClass->GetPathName());
    Response->SetBoolField(TEXT("is_abstract"), AssetClass->HasAnyClassFlags(CLASS_Abstract));
    Response->SetBoolField(TEXT("is_native"), !AssetClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint));
    
    // Parent chain
    TArray<TSharedPtr<FJsonValue>> ParentChain;
    UClass* CurrentClass = AssetClass->GetSuperClass();
    while (CurrentClass && CurrentClass != UObject::StaticClass())
    {
        ParentChain.Add(MakeShared<FJsonValueString>(CurrentClass->GetName()));
        CurrentClass = CurrentClass->GetSuperClass();
    }
    Response->SetArrayField(TEXT("parent_classes"), ParentChain);
    
    // Properties
    TArray<TSharedPtr<FJsonValue>> PropertiesArray;
    for (TFieldIterator<FProperty> PropIt(AssetClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
    {
        FProperty* Property = *PropIt;
        if (!ShouldExposeProperty(Property, bIncludeAll))
        {
            continue;
        }
        
        TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
        PropObj->SetStringField(TEXT("name"), Property->GetName());
        PropObj->SetStringField(TEXT("type"), GetPropertyTypeString(Property));
        PropObj->SetStringField(TEXT("defined_in"), Property->GetOwnerClass()->GetName());
        
        if (bIncludeAll)
        {
            TArray<FString> Flags;
            if (Property->HasAnyPropertyFlags(CPF_Edit)) Flags.Add(TEXT("Edit"));
            if (Property->HasAnyPropertyFlags(CPF_BlueprintVisible)) Flags.Add(TEXT("BlueprintVisible"));
            if (Property->HasAnyPropertyFlags(CPF_SaveGame)) Flags.Add(TEXT("SaveGame"));
            if (Property->HasAnyPropertyFlags(CPF_EditConst)) Flags.Add(TEXT("EditConst"));
            if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate)) Flags.Add(TEXT("Private"));
            if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierProtected)) Flags.Add(TEXT("Protected"));
            if (Property->HasAnyPropertyFlags(CPF_Transient)) Flags.Add(TEXT("Transient"));
            PropObj->SetStringField(TEXT("flags"), FString::Join(Flags, TEXT(", ")));
        }
        
        PropertiesArray.Add(MakeShared<FJsonValueObject>(PropObj));
    }
    Response->SetArrayField(TEXT("properties"), PropertiesArray);
    
    if (bIncludeAll && PropertiesArray.Num() == 0)
    {
        Response->SetStringField(TEXT("note"), TEXT("This class has no properties at all. It may use custom serialization or internal data structures not exposed via UPROPERTY."));
    }
    
    return Response;
}

// ========== Helper Functions ==========

UDataAsset* FDataAssetCommands::LoadDataAsset(const FString& AssetPath)
{
    // Try direct load first
    UDataAsset* DataAsset = Cast<UDataAsset>(StaticLoadObject(UDataAsset::StaticClass(), nullptr, *AssetPath));
    
    if (!DataAsset)
    {
        // Try adding .AssetName suffix
        FString FullPath = AssetPath;
        if (!FullPath.Contains(TEXT(".")))
        {
            int32 LastSlash;
            if (FullPath.FindLastChar('/', LastSlash))
            {
                FString AssetName = FullPath.Mid(LastSlash + 1);
                FullPath = FullPath + TEXT(".") + AssetName;
                DataAsset = Cast<UDataAsset>(StaticLoadObject(UDataAsset::StaticClass(), nullptr, *FullPath));
            }
        }
    }
    
    if (!DataAsset)
    {
        // Search by name
        IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
        TArray<FAssetData> AssetDataList;
        AssetRegistry.GetAssetsByClass(FTopLevelAssetPath(UDataAsset::StaticClass()->GetClassPathName()), AssetDataList, true);
        
        FString SearchName = FPackageName::GetShortName(AssetPath);
        
        for (const FAssetData& AssetData : AssetDataList)
        {
            if (AssetData.AssetName.ToString().Equals(SearchName, ESearchCase::IgnoreCase))
            {
                DataAsset = Cast<UDataAsset>(AssetData.GetAsset());
                break;
            }
        }
    }
    
    return DataAsset;
}

UClass* FDataAssetCommands::FindDataAssetClass(const FString& ClassNameOrPath)
{
    FString SearchName = ClassNameOrPath;
    
    // Try direct find
    UClass* FoundClass = FindObject<UClass>(nullptr, *SearchName);
    
    if (!FoundClass)
    {
        // Try with /Script/ prefix
        if (!SearchName.StartsWith(TEXT("/")))
        {
            // Search all classes
            for (TObjectIterator<UClass> It; It; ++It)
            {
                UClass* Class = *It;
                if (Class->IsChildOf(UDataAsset::StaticClass()))
                {
                    if (Class->GetName().Equals(SearchName, ESearchCase::IgnoreCase))
                    {
                        FoundClass = Class;
                        break;
                    }
                }
            }
        }
    }
    
    if (!FoundClass)
    {
        // Try loading
        FoundClass = LoadObject<UClass>(nullptr, *SearchName);
    }
    
    return FoundClass;
}

TSharedPtr<FJsonValue> FDataAssetCommands::PropertyToJson(FProperty* Property, void* Container)
{
    if (!Property || !Container)
    {
        return MakeShared<FJsonValueNull>();
    }
    
    void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
    
    // Numeric types
    if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
    {
        if (NumericProp->IsFloatingPoint())
        {
            double Value = 0.0;
            NumericProp->GetValue_InContainer(Container, &Value);
            return MakeShared<FJsonValueNumber>(Value);
        }
        else if (NumericProp->IsInteger())
        {
            int64 Value = 0;
            NumericProp->GetValue_InContainer(Container, &Value);
            return MakeShared<FJsonValueNumber>(static_cast<double>(Value));
        }
    }
    
    // Bool
    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
    }
    
    // String types
    if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValuePtr));
    }
    
    if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
    }
    
    if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
    }
    
    // Enum
    if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        UEnum* Enum = EnumProp->GetEnum();
        FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
        int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
        FString EnumName = Enum->GetNameStringByValue(EnumValue);
        return MakeShared<FJsonValueString>(EnumName);
    }
    
    if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        if (UEnum* Enum = ByteProp->Enum)
        {
            uint8 Value = ByteProp->GetPropertyValue(ValuePtr);
            FString EnumName = Enum->GetNameStringByValue(Value);
            return MakeShared<FJsonValueString>(EnumName);
        }
        else
        {
            return MakeShared<FJsonValueNumber>(ByteProp->GetPropertyValue(ValuePtr));
        }
    }
    
    // Object reference
    if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
    {
        UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
        if (Obj)
        {
            return MakeShared<FJsonValueString>(Obj->GetPathName());
        }
        return MakeShared<FJsonValueNull>();
    }
    
    // Soft object reference
    if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
    {
        FSoftObjectPtr* SoftPtr = static_cast<FSoftObjectPtr*>(ValuePtr);
        return MakeShared<FJsonValueString>(SoftPtr->ToString());
    }
    
    // Array
    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
    {
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
        
        for (int32 i = 0; i < ArrayHelper.Num(); ++i)
        {
            // For array elements, we need to pass the element pointer directly
            void* ElementPtr = ArrayHelper.GetRawPtr(i);
            
            // Create a temporary "container" that points to the element
            // For inner property, the container IS the value ptr
            TSharedPtr<FJsonValue> ElementValue = PropertyToJson(ArrayProp->Inner, ElementPtr);
            JsonArray.Add(ElementValue);
        }
        
        return MakeShared<FJsonValueArray>(JsonArray);
    }
    
    // Struct
    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        TSharedPtr<FJsonObject> StructObj = MakeShared<FJsonObject>();
        UScriptStruct* Struct = StructProp->Struct;
        
        for (TFieldIterator<FProperty> It(Struct); It; ++It)
        {
            FProperty* InnerProp = *It;
            TSharedPtr<FJsonValue> InnerValue = PropertyToJson(InnerProp, ValuePtr);
            StructObj->SetField(InnerProp->GetName(), InnerValue);
        }
        
        return MakeShared<FJsonValueObject>(StructObj);
    }
    
    // Map
    if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
    {
        TSharedPtr<FJsonObject> MapObj = MakeShared<FJsonObject>();
        FScriptMapHelper MapHelper(MapProp, ValuePtr);
        
        for (int32 i = 0; i < MapHelper.Num(); ++i)
        {
            if (MapHelper.IsValidIndex(i))
            {
                void* KeyPtr = MapHelper.GetKeyPtr(i);
                void* ValPtr = MapHelper.GetValuePtr(i);
                
                // Get key as string
                FString KeyStr;
                MapProp->KeyProp->ExportTextItem_Direct(KeyStr, KeyPtr, nullptr, nullptr, PPF_None);
                
                TSharedPtr<FJsonValue> Value = PropertyToJson(MapProp->ValueProp, ValPtr);
                MapObj->SetField(KeyStr, Value);
            }
        }
        
        return MakeShared<FJsonValueObject>(MapObj);
    }
    
    // Fallback: export as text
    FString ExportedText;
    Property->ExportTextItem_Direct(ExportedText, ValuePtr, nullptr, nullptr, PPF_None);
    return MakeShared<FJsonValueString>(ExportedText);
}

bool FDataAssetCommands::JsonToProperty(FProperty* Property, void* Container, const TSharedPtr<FJsonValue>& Value, FString& OutError)
{
    if (!Property || !Container || !Value.IsValid())
    {
        OutError = TEXT("Invalid parameters");
        return false;
    }
    
    void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
    
    // Numeric types
    if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
    {
        double NumValue;
        if (!Value->TryGetNumber(NumValue))
        {
            OutError = TEXT("Expected numeric value");
            return false;
        }
        
        if (NumericProp->IsFloatingPoint())
        {
            NumericProp->SetFloatingPointPropertyValue(ValuePtr, NumValue);
        }
        else
        {
            NumericProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumValue));
        }
        return true;
    }
    
    // Bool
    if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
    {
        bool BoolValue;
        if (!Value->TryGetBool(BoolValue))
        {
            OutError = TEXT("Expected boolean value");
            return false;
        }
        BoolProp->SetPropertyValue(ValuePtr, BoolValue);
        return true;
    }
    
    // String types
    if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
    {
        FString StrValue;
        if (!Value->TryGetString(StrValue))
        {
            OutError = TEXT("Expected string value");
            return false;
        }
        StrProp->SetPropertyValue(ValuePtr, StrValue);
        return true;
    }
    
    if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
    {
        FString StrValue;
        if (!Value->TryGetString(StrValue))
        {
            OutError = TEXT("Expected string value for FName");
            return false;
        }
        NameProp->SetPropertyValue(ValuePtr, FName(*StrValue));
        return true;
    }
    
    if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
    {
        FString StrValue;
        if (!Value->TryGetString(StrValue))
        {
            OutError = TEXT("Expected string value for FText");
            return false;
        }
        TextProp->SetPropertyValue(ValuePtr, FText::FromString(StrValue));
        return true;
    }
    
    // Enum
    if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        FString EnumStr;
        if (Value->TryGetString(EnumStr))
        {
            UEnum* Enum = EnumProp->GetEnum();
            int64 EnumValue = Enum->GetValueByNameString(EnumStr);
            if (EnumValue == INDEX_NONE)
            {
                OutError = FString::Printf(TEXT("Invalid enum value: %s"), *EnumStr);
                return false;
            }
            EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumValue);
            return true;
        }
        
        double NumValue;
        if (Value->TryGetNumber(NumValue))
        {
            EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, static_cast<int64>(NumValue));
            return true;
        }
        
        OutError = TEXT("Expected string or number for enum");
        return false;
    }
    
    // Object reference (as path string)
    if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
    {
        FString PathStr;
        if (Value->IsNull())
        {
            ObjProp->SetObjectPropertyValue(ValuePtr, nullptr);
            return true;
        }
        
        if (!Value->TryGetString(PathStr))
        {
            OutError = TEXT("Expected string path for object reference");
            return false;
        }
        
        UObject* Obj = StaticLoadObject(ObjProp->PropertyClass, nullptr, *PathStr);
        if (!Obj && !PathStr.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Could not load object: %s"), *PathStr);
            return false;
        }
        
        ObjProp->SetObjectPropertyValue(ValuePtr, Obj);
        return true;
    }
    
    // Soft object reference
    if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
    {
        FString PathStr;
        if (!Value->TryGetString(PathStr))
        {
            OutError = TEXT("Expected string path for soft object reference");
            return false;
        }
        
        FSoftObjectPtr* SoftPtr = static_cast<FSoftObjectPtr*>(ValuePtr);
        *SoftPtr = FSoftObjectPath(PathStr);
        return true;
    }
    
    // Array
    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
    {
        const TArray<TSharedPtr<FJsonValue>>* JsonArray;
        if (!Value->TryGetArray(JsonArray))
        {
            OutError = TEXT("Expected array value");
            return false;
        }
        
        FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
        ArrayHelper.EmptyValues();
        ArrayHelper.AddValues(JsonArray->Num());
        
        for (int32 i = 0; i < JsonArray->Num(); ++i)
        {
            FString ElementError;
            void* ElementPtr = ArrayHelper.GetRawPtr(i);
            
            // For setting array elements, we pass the element ptr directly
            if (!JsonToProperty(ArrayProp->Inner, ElementPtr, (*JsonArray)[i], ElementError))
            {
                OutError = FString::Printf(TEXT("Array element %d: %s"), i, *ElementError);
                return false;
            }
        }
        return true;
    }
    
    // Struct - try object format first, then string import
    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        const TSharedPtr<FJsonObject>* JsonObj;
        if (Value->TryGetObject(JsonObj))
        {
            UScriptStruct* Struct = StructProp->Struct;
            
            for (auto& Pair : (*JsonObj)->Values)
            {
                FProperty* InnerProp = Struct->FindPropertyByName(*Pair.Key);
                if (InnerProp)
                {
                    FString InnerError;
                    if (!JsonToProperty(InnerProp, ValuePtr, Pair.Value, InnerError))
                    {
                        UE_LOG(LogDataAssetCommands, Warning, TEXT("Failed to set struct member %s: %s"), *Pair.Key, *InnerError);
                    }
                }
            }
            return true;
        }
        
        // Try string import
        FString StrValue;
        if (Value->TryGetString(StrValue))
        {
            if (StructProp->ImportText_Direct(*StrValue, ValuePtr, nullptr, PPF_None))
            {
                return true;
            }
            OutError = FString::Printf(TEXT("Failed to import struct from string: %s"), *StrValue);
            return false;
        }
        
        OutError = TEXT("Expected object or string for struct");
        return false;
    }
    
    // Fallback: try ImportText
    FString StrValue;
    if (Value->TryGetString(StrValue))
    {
        if (Property->ImportText_Direct(*StrValue, ValuePtr, nullptr, PPF_None))
        {
            return true;
        }
    }
    
    OutError = TEXT("Could not convert JSON value to property");
    return false;
}

FString FDataAssetCommands::GetPropertyTypeString(FProperty* Property)
{
    if (!Property)
    {
        return TEXT("Unknown");
    }
    
    if (FNumericProperty* NumericProp = CastField<FNumericProperty>(Property))
    {
        if (NumericProp->IsFloatingPoint())
        {
            if (CastField<FFloatProperty>(Property)) return TEXT("float");
            if (CastField<FDoubleProperty>(Property)) return TEXT("double");
        }
        else
        {
            if (CastField<FIntProperty>(Property)) return TEXT("int32");
            if (CastField<FInt64Property>(Property)) return TEXT("int64");
            if (CastField<FUInt32Property>(Property)) return TEXT("uint32");
            if (CastField<FUInt64Property>(Property)) return TEXT("uint64");
            if (CastField<FInt16Property>(Property)) return TEXT("int16");
            if (CastField<FUInt16Property>(Property)) return TEXT("uint16");
            if (CastField<FInt8Property>(Property)) return TEXT("int8");
        }
        return TEXT("numeric");
    }
    
    if (CastField<FBoolProperty>(Property)) return TEXT("bool");
    if (CastField<FStrProperty>(Property)) return TEXT("FString");
    if (CastField<FNameProperty>(Property)) return TEXT("FName");
    if (CastField<FTextProperty>(Property)) return TEXT("FText");
    
    if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
    {
        if (UEnum* Enum = EnumProp->GetEnum())
        {
            return Enum->GetName();
        }
        return TEXT("Enum");
    }
    
    if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
    {
        if (UEnum* Enum = ByteProp->Enum)
        {
            return Enum->GetName();
        }
        return TEXT("uint8");
    }
    
    if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
    {
        return FString::Printf(TEXT("%s*"), *ObjProp->PropertyClass->GetName());
    }
    
    if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
    {
        return FString::Printf(TEXT("TSoftObjectPtr<%s>"), *SoftObjProp->PropertyClass->GetName());
    }
    
    if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
    {
        return FString::Printf(TEXT("TSubclassOf<%s>"), *ClassProp->MetaClass->GetName());
    }
    
    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
    {
        return FString::Printf(TEXT("TArray<%s>"), *GetPropertyTypeString(ArrayProp->Inner));
    }
    
    if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
    {
        return FString::Printf(TEXT("TSet<%s>"), *GetPropertyTypeString(SetProp->ElementProp));
    }
    
    if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
    {
        return FString::Printf(TEXT("TMap<%s, %s>"), 
            *GetPropertyTypeString(MapProp->KeyProp),
            *GetPropertyTypeString(MapProp->ValueProp));
    }
    
    if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
    {
        return StructProp->Struct->GetName();
    }
    
    return Property->GetCPPType();
}

bool FDataAssetCommands::ShouldExposeProperty(FProperty* Property, bool bIncludeAll)
{
    if (!Property)
    {
        return false;
    }
    
    // If including all, just skip null and deprecated
    if (bIncludeAll)
    {
        // Still skip deprecated
        if (Property->HasMetaData(TEXT("DeprecatedProperty")))
        {
            return false;
        }
        return true;
    }
    
    // Must be editable in some way - check this FIRST
    bool bIsEditable = Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible | CPF_SaveGame);
    
    // If property is marked as editable (Edit flag), allow it even if it's private
    // Many engine classes use private properties with UPROPERTY(EditAnywhere)
    if (!bIsEditable)
    {
        // Not editable at all - check access specifiers
        if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected))
        {
            return false;
        }
        // Still not editable
        return false;
    }
    
    // Skip deprecated
    if (Property->HasMetaData(TEXT("DeprecatedProperty")))
    {
        return false;
    }
    
    return true;
}

TSharedPtr<FJsonObject> FDataAssetCommands::CreateSuccessResponse(const FString& Message)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), true);
    if (!Message.IsEmpty())
    {
        Response->SetStringField(TEXT("message"), Message);
    }
    return Response;
}

TSharedPtr<FJsonObject> FDataAssetCommands::CreateErrorResponse(const FString& ErrorMessage, const FString& ErrorCode)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), ErrorMessage);
    Response->SetStringField(TEXT("error_code"), ErrorCode);
    return Response;
}

TSharedPtr<FJsonObject> FDataAssetCommands::CreateErrorResponseWithParams(const FString& ErrorMessage, const TArray<FString>& ValidParams)
{
    TSharedPtr<FJsonObject> Response = CreateErrorResponse(ErrorMessage, TEXT("MISSING_PARAMS"));
    
    TArray<TSharedPtr<FJsonValue>> ParamsArray;
    for (const FString& Param : ValidParams)
    {
        ParamsArray.Add(MakeShared<FJsonValueString>(Param));
    }
    Response->SetArrayField(TEXT("valid_params"), ParamsArray);
    
    return Response;
}
