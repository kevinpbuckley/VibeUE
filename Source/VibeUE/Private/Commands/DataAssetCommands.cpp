// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Commands/DataAssetCommands.h"
#include "Core/ServiceContext.h"
#include "Utils/HelpFileReader.h"
#include "Utils/ParamValidation.h"
#include "Services/DataAsset/DataAssetDiscoveryService.h"
#include "Services/DataAsset/DataAssetPropertyService.h"
#include "Services/DataAsset/DataAssetLifecycleService.h"
#include "Engine/DataAsset.h"

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
	
	FDataAssetDiscoveryService Service(ServiceContext);
	auto Result = Service.SearchTypes(SearchFilter);
	
	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorMessage(), Result.GetErrorCode());
	}
	
	TArray<TSharedPtr<FJsonValue>> TypesArray;
	for (const FDataAssetTypeInfo& TypeInfo : Result.GetValue())
	{
		TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
		TypeObj->SetStringField(TEXT("name"), TypeInfo.Name);
		TypeObj->SetStringField(TEXT("path"), TypeInfo.Path);
		TypeObj->SetStringField(TEXT("module"), TypeInfo.Module);
		TypeObj->SetBoolField(TEXT("is_native"), TypeInfo.bIsNative);
		TypeObj->SetStringField(TEXT("parent_class"), TypeInfo.ParentClass);
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
	
	FDataAssetDiscoveryService Service(ServiceContext);
	auto Result = Service.ListAssets(AssetType, SearchPath);
	
	if (Result.IsError())
	{
		return CreateErrorResponse(Result.GetErrorMessage(), Result.GetErrorCode());
	}
	
	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	for (const FDataAssetInfo& Info : Result.GetValue())
	{
		TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
		AssetObj->SetStringField(TEXT("name"), Info.Name);
		AssetObj->SetStringField(TEXT("path"), Info.Path);
		AssetObj->SetStringField(TEXT("class"), Info.ClassName);
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
	FDataAssetDiscoveryService DiscoveryService(ServiceContext);
	auto ClassResult = DiscoveryService.FindDataAssetClass(ClassName);
	if (ClassResult.IsError())
	{
		return CreateErrorResponse(
			FString::Printf(TEXT("%s. Use search_types action to find available classes."), *ClassResult.GetErrorMessage()),
			ClassResult.GetErrorCode());
	}
	
	// Get initial properties if provided
	TSharedPtr<FJsonObject> InitialProperties;
	const TSharedPtr<FJsonObject>* PropertiesObj;
	if (Params->TryGetObjectField(TEXT("properties"), PropertiesObj))
	{
		InitialProperties = *PropertiesObj;
	}
	
	// Create the asset
	FDataAssetLifecycleService LifecycleService(ServiceContext);
	auto CreateResult = LifecycleService.CreateDataAsset(ClassResult.GetValue(), AssetPath, AssetName, InitialProperties);
	
	if (CreateResult.IsError())
	{
		return CreateErrorResponse(CreateResult.GetErrorMessage(), CreateResult.GetErrorCode());
	}
	
	const FDataAssetCreateResult& Result = CreateResult.GetValue();
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse(FString::Printf(TEXT("Created data asset: %s"), *Result.AssetPath));
	Response->SetStringField(TEXT("asset_path"), Result.AssetPath);
	Response->SetStringField(TEXT("asset_name"), Result.AssetName);
	Response->SetStringField(TEXT("class_name"), Result.ClassName);
	
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
	
	FDataAssetDiscoveryService DiscoveryService(ServiceContext);
	auto LoadResult = DiscoveryService.LoadDataAsset(AssetPath);
	if (LoadResult.IsError())
	{
		return CreateErrorResponse(LoadResult.GetErrorMessage(), LoadResult.GetErrorCode());
	}
	
	FDataAssetPropertyService PropertyService(ServiceContext);
	auto InfoResult = PropertyService.GetAssetInfo(LoadResult.GetValue());
	if (InfoResult.IsError())
	{
		return CreateErrorResponse(InfoResult.GetErrorMessage(), InfoResult.GetErrorCode());
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	// Merge info result into response
	for (auto& Pair : InfoResult.GetValue()->Values)
	{
		Response->SetField(Pair.Key, Pair.Value);
	}
	
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
	FDataAssetDiscoveryService DiscoveryService(ServiceContext);
	
	if (!AssetPath.IsEmpty())
	{
		auto LoadResult = DiscoveryService.LoadDataAsset(AssetPath);
		if (LoadResult.IsError())
		{
			return CreateErrorResponse(LoadResult.GetErrorMessage(), LoadResult.GetErrorCode());
		}
		AssetClass = LoadResult.GetValue()->GetClass();
	}
	else if (!ClassName.IsEmpty())
	{
		auto ClassResult = DiscoveryService.FindDataAssetClass(ClassName);
		if (ClassResult.IsError())
		{
			return CreateErrorResponse(ClassResult.GetErrorMessage(), ClassResult.GetErrorCode());
		}
		AssetClass = ClassResult.GetValue();
	}
	else
	{
		return CreateErrorResponse(TEXT("Either asset_path or class_name is required"));
	}
	
	FDataAssetPropertyService PropertyService(ServiceContext);
	auto PropsResult = PropertyService.ListProperties(AssetClass, bIncludeAll);
	if (PropsResult.IsError())
	{
		return CreateErrorResponse(PropsResult.GetErrorMessage(), PropsResult.GetErrorCode());
	}
	
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (const FDataAssetPropertyInfo& PropInfo : PropsResult.GetValue())
	{
		TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
		PropObj->SetStringField(TEXT("name"), PropInfo.Name);
		PropObj->SetStringField(TEXT("type"), PropInfo.Type);
		PropObj->SetStringField(TEXT("category"), PropInfo.Category);
		if (!PropInfo.Description.IsEmpty())
		{
			PropObj->SetStringField(TEXT("description"), PropInfo.Description);
		}
		PropObj->SetBoolField(TEXT("read_only"), PropInfo.bReadOnly);
		PropObj->SetBoolField(TEXT("is_array"), PropInfo.bIsArray);
		PropObj->SetStringField(TEXT("defined_in"), PropInfo.DefinedIn);
		if (bIncludeAll && !PropInfo.Flags.IsEmpty())
		{
			PropObj->SetStringField(TEXT("flags"), PropInfo.Flags);
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
	
	FDataAssetDiscoveryService DiscoveryService(ServiceContext);
	auto LoadResult = DiscoveryService.LoadDataAsset(AssetPath);
	if (LoadResult.IsError())
	{
		return CreateErrorResponse(LoadResult.GetErrorMessage(), LoadResult.GetErrorCode());
	}
	
	FDataAssetPropertyService PropertyService(ServiceContext);
	auto PropResult = PropertyService.GetProperty(LoadResult.GetValue(), PropertyName);
	if (PropResult.IsError())
	{
		return CreateErrorResponse(PropResult.GetErrorMessage(), PropResult.GetErrorCode());
	}
	
	UClass* AssetClass = LoadResult.GetValue()->GetClass();
	FProperty* Property = AssetClass->FindPropertyByName(*PropertyName);
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("property_name"), PropertyName);
	Response->SetStringField(TEXT("type"), PropertyService.GetPropertyTypeString(Property));
	Response->SetField(TEXT("value"), PropResult.GetValue());
	
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
	
	FDataAssetDiscoveryService DiscoveryService(ServiceContext);
	auto LoadResult = DiscoveryService.LoadDataAsset(AssetPath);
	if (LoadResult.IsError())
	{
		return CreateErrorResponse(LoadResult.GetErrorMessage(), LoadResult.GetErrorCode());
	}
	
	FDataAssetPropertyService PropertyService(ServiceContext);
	auto SetResult = PropertyService.SetProperty(LoadResult.GetValue(), PropertyName, Value);
	if (SetResult.IsError())
	{
		return CreateErrorResponse(SetResult.GetErrorMessage(), SetResult.GetErrorCode());
	}
	
	// Get the new value for response
	auto NewValueResult = PropertyService.GetProperty(LoadResult.GetValue(), PropertyName);
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse(FString::Printf(TEXT("Set property %s"), *PropertyName));
	Response->SetStringField(TEXT("property_name"), PropertyName);
	if (NewValueResult.IsSuccess())
	{
		Response->SetField(TEXT("new_value"), NewValueResult.GetValue());
	}
	
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
	
	FDataAssetDiscoveryService DiscoveryService(ServiceContext);
	auto LoadResult = DiscoveryService.LoadDataAsset(AssetPath);
	if (LoadResult.IsError())
	{
		return CreateErrorResponse(LoadResult.GetErrorMessage(), LoadResult.GetErrorCode());
	}
	
	FDataAssetPropertyService PropertyService(ServiceContext);
	auto SetResult = PropertyService.SetProperties(LoadResult.GetValue(), *PropertiesObj);
	if (SetResult.IsError())
	{
		return CreateErrorResponse(SetResult.GetErrorMessage(), SetResult.GetErrorCode());
	}
	
	const FSetPropertiesResult& Result = SetResult.GetValue();
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse(FString::Printf(TEXT("Set %d properties"), Result.SuccessProperties.Num()));
	
	TArray<TSharedPtr<FJsonValue>> SuccessArray;
	for (const FString& Name : Result.SuccessProperties)
	{
		SuccessArray.Add(MakeShared<FJsonValueString>(Name));
	}
	Response->SetArrayField(TEXT("success"), SuccessArray);
	
	if (Result.FailedProperties.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> FailedArray;
		for (const FString& Name : Result.FailedProperties)
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
	
	FDataAssetDiscoveryService DiscoveryService(ServiceContext);
	auto ClassResult = DiscoveryService.FindDataAssetClass(ClassName);
	if (ClassResult.IsError())
	{
		return CreateErrorResponse(ClassResult.GetErrorMessage(), ClassResult.GetErrorCode());
	}
	
	FDataAssetPropertyService PropertyService(ServiceContext);
	auto InfoResult = PropertyService.GetClassInfo(ClassResult.GetValue(), bIncludeAll);
	if (InfoResult.IsError())
	{
		return CreateErrorResponse(InfoResult.GetErrorMessage(), InfoResult.GetErrorCode());
	}
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	// Merge info result into response
	for (auto& Pair : InfoResult.GetValue()->Values)
	{
		Response->SetField(Pair.Key, Pair.Value);
	}
	
	return Response;
}

// ========== Response Helpers ==========

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
