// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Commands/MaterialNodeCommands.h"
#include "Services/Material/MaterialNodeService.h"
#include "Core/ServiceContext.h"
#include "Materials/Material.h"
#include "AssetRegistry/AssetRegistryModule.h"

DEFINE_LOG_CATEGORY_STATIC(LogMaterialNodeCommands, Log, All);

FMaterialNodeCommands::FMaterialNodeCommands()
{
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	Service = MakeShared<FMaterialNodeService>(Context);
	UE_LOG(LogMaterialNodeCommands, Display, TEXT("MaterialNodeCommands: Initialized"));
}

FMaterialNodeCommands::~FMaterialNodeCommands()
{
	Service.Reset();
	UE_LOG(LogMaterialNodeCommands, Display, TEXT("MaterialNodeCommands: Destroyed"));
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::CreateErrorResponse(const FString& Code, const FString& Message)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error_code"), Code);
	Response->SetStringField(TEXT("error"), Message);
	return Response;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::CreateSuccessResponse()
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), true);
	return Response;
}

UMaterial* FMaterialNodeCommands::LoadMaterialFromParams(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& OutError)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		OutError = CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
		return nullptr;
	}

	UMaterial* Material = LoadObject<UMaterial>(nullptr, *MaterialPath);
	if (!Material)
	{
		OutError = CreateErrorResponse(TEXT("MATERIAL_NOT_FOUND"), 
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
		return nullptr;
	}

	return Material;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::ExpressionInfoToJson(const FMaterialExpressionInfo& Info)
{
	TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
	Obj->SetStringField(TEXT("id"), Info.Id);
	Obj->SetStringField(TEXT("class_name"), Info.ClassName);
	Obj->SetStringField(TEXT("display_name"), Info.DisplayName);
	Obj->SetStringField(TEXT("category"), Info.Category);
	Obj->SetNumberField(TEXT("pos_x"), Info.PosX);
	Obj->SetNumberField(TEXT("pos_y"), Info.PosY);
	Obj->SetStringField(TEXT("description"), Info.Description);
	Obj->SetBoolField(TEXT("is_parameter"), Info.bIsParameter);
	Obj->SetStringField(TEXT("parameter_name"), Info.ParameterName);

	TArray<TSharedPtr<FJsonValue>> InputsArray;
	for (const FString& InputName : Info.InputNames)
	{
		InputsArray.Add(MakeShareable(new FJsonValueString(InputName)));
	}
	Obj->SetArrayField(TEXT("inputs"), InputsArray);

	TArray<TSharedPtr<FJsonValue>> OutputsArray;
	for (const FString& OutputName : Info.OutputNames)
	{
		OutputsArray.Add(MakeShareable(new FJsonValueString(OutputName)));
	}
	Obj->SetArrayField(TEXT("outputs"), OutputsArray);

	return Obj;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::TypeInfoToJson(const FMaterialExpressionTypeInfo& Info)
{
	TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
	Obj->SetStringField(TEXT("class_name"), Info.ClassName);
	Obj->SetStringField(TEXT("display_name"), Info.DisplayName);
	Obj->SetStringField(TEXT("category"), Info.Category);
	Obj->SetStringField(TEXT("description"), Info.Description);
	Obj->SetBoolField(TEXT("is_parameter"), Info.bIsParameter);

	TArray<TSharedPtr<FJsonValue>> KeywordsArray;
	for (const FString& Keyword : Info.Keywords)
	{
		KeywordsArray.Add(MakeShareable(new FJsonValueString(Keyword)));
	}
	Obj->SetArrayField(TEXT("keywords"), KeywordsArray);

	TArray<TSharedPtr<FJsonValue>> InputsArray;
	for (const FString& InputName : Info.InputNames)
	{
		InputsArray.Add(MakeShareable(new FJsonValueString(InputName)));
	}
	Obj->SetArrayField(TEXT("inputs"), InputsArray);

	TArray<TSharedPtr<FJsonValue>> OutputsArray;
	for (const FString& OutputName : Info.OutputNames)
	{
		OutputsArray.Add(MakeShareable(new FJsonValueString(OutputName)));
	}
	Obj->SetArrayField(TEXT("outputs"), OutputsArray);

	return Obj;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::PinInfoToJson(const FMaterialPinInfo& Info)
{
	TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
	Obj->SetStringField(TEXT("name"), Info.Name);
	Obj->SetNumberField(TEXT("index"), Info.Index);
	Obj->SetStringField(TEXT("direction"), Info.Direction);
	Obj->SetStringField(TEXT("value_type"), Info.ValueType);
	Obj->SetBoolField(TEXT("is_connected"), Info.bIsConnected);
	Obj->SetStringField(TEXT("connected_expression_id"), Info.ConnectedExpressionId);
	Obj->SetNumberField(TEXT("connected_output_index"), Info.ConnectedOutputIndex);
	Obj->SetStringField(TEXT("default_value"), Info.DefaultValue);
	return Obj;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::ConnectionInfoToJson(const FMaterialConnectionInfo& Info)
{
	TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
	Obj->SetStringField(TEXT("source_expression_id"), Info.SourceExpressionId);
	Obj->SetStringField(TEXT("source_output"), Info.SourceOutput);
	Obj->SetStringField(TEXT("target_expression_id"), Info.TargetExpressionId);
	Obj->SetStringField(TEXT("target_input"), Info.TargetInput);
	return Obj;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType != TEXT("manage_material_node"))
	{
		return CreateErrorResponse(TEXT("INVALID_COMMAND"), 
			FString::Printf(TEXT("Unknown command: %s"), *CommandType));
	}

	if (!Params.IsValid())
	{
		return CreateErrorResponse(TEXT("INVALID_PARAMS"), TEXT("Parameters are required"));
	}

	FString Action;
	if (!Params->TryGetStringField(TEXT("action"), Action))
	{
		return CreateErrorResponse(TEXT("MISSING_ACTION"), TEXT("action parameter is required"));
	}

	Action = Action.ToLower();
	UE_LOG(LogMaterialNodeCommands, Display, TEXT("MaterialNodeCommands: Handling action '%s'"), *Action);

	// Discovery actions
	if (Action == TEXT("discover_types"))
	{
		return HandleDiscoverTypes(Params);
	}
	else if (Action == TEXT("get_categories"))
	{
		return HandleGetCategories(Params);
	}
	// Expression lifecycle actions
	else if (Action == TEXT("create"))
	{
		return HandleCreate(Params);
	}
	else if (Action == TEXT("delete"))
	{
		return HandleDelete(Params);
	}
	else if (Action == TEXT("move"))
	{
		return HandleMove(Params);
	}
	// Expression information actions
	else if (Action == TEXT("list"))
	{
		return HandleList(Params);
	}
	else if (Action == TEXT("get_details"))
	{
		return HandleGetDetails(Params);
	}
	else if (Action == TEXT("get_pins"))
	{
		return HandleGetPins(Params);
	}
	// Connection actions
	else if (Action == TEXT("connect"))
	{
		return HandleConnect(Params);
	}
	else if (Action == TEXT("disconnect"))
	{
		return HandleDisconnect(Params);
	}
	else if (Action == TEXT("connect_to_output"))
	{
		return HandleConnectToOutput(Params);
	}
	else if (Action == TEXT("disconnect_output"))
	{
		return HandleDisconnectOutput(Params);
	}
	else if (Action == TEXT("list_connections"))
	{
		return HandleListConnections(Params);
	}
	// Expression property actions
	else if (Action == TEXT("get_property"))
	{
		return HandleGetProperty(Params);
	}
	else if (Action == TEXT("set_property"))
	{
		return HandleSetProperty(Params);
	}
	else if (Action == TEXT("list_properties"))
	{
		return HandleListProperties(Params);
	}
	// Parameter actions
	else if (Action == TEXT("promote_to_parameter"))
	{
		return HandlePromoteToParameter(Params);
	}
	else if (Action == TEXT("create_parameter"))
	{
		return HandleCreateParameter(Params);
	}
	else if (Action == TEXT("set_parameter_metadata"))
	{
		return HandleSetParameterMetadata(Params);
	}
	// Material output actions
	else if (Action == TEXT("get_output_properties"))
	{
		return HandleGetOutputProperties(Params);
	}
	else if (Action == TEXT("get_output_connections"))
	{
		return HandleGetOutputConnections(Params);
	}
	else
	{
		return CreateErrorResponse(TEXT("UNKNOWN_ACTION"), 
			FString::Printf(TEXT("Unknown action: %s"), *Action));
	}
}

//-----------------------------------------------------------------------------
// Discovery Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleDiscoverTypes(const TSharedPtr<FJsonObject>& Params)
{
	FString Category;
	Params->TryGetStringField(TEXT("category"), Category);

	FString SearchTerm;
	Params->TryGetStringField(TEXT("search_term"), SearchTerm);

	int32 MaxResults = 100;
	Params->TryGetNumberField(TEXT("max_results"), MaxResults);

	auto Result = Service->DiscoverExpressionTypes(Category, SearchTerm, MaxResults);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	TArray<TSharedPtr<FJsonValue>> TypesArray;
	for (const FMaterialExpressionTypeInfo& TypeInfo : Result.GetValue())
	{
		TypesArray.Add(MakeShareable(new FJsonValueObject(TypeInfoToJson(TypeInfo))));
	}
	Response->SetArrayField(TEXT("expression_types"), TypesArray);
	Response->SetNumberField(TEXT("count"), TypesArray.Num());

	return Response;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleGetCategories(const TSharedPtr<FJsonObject>& Params)
{
	auto Result = Service->GetExpressionCategories();
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	TArray<TSharedPtr<FJsonValue>> CategoriesArray;
	for (const FString& Category : Result.GetValue())
	{
		CategoriesArray.Add(MakeShareable(new FJsonValueString(Category)));
	}
	Response->SetArrayField(TEXT("categories"), CategoriesArray);

	return Response;
}

//-----------------------------------------------------------------------------
// Expression Lifecycle Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleCreate(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ExpressionClass;
	if (!Params->TryGetStringField(TEXT("expression_class"), ExpressionClass))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("expression_class is required"));
	}

	int32 PosX = 0, PosY = 0;
	Params->TryGetNumberField(TEXT("pos_x"), PosX);
	Params->TryGetNumberField(TEXT("pos_y"), PosY);

	auto Result = Service->CreateExpression(Material, ExpressionClass, PosX, PosY);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetObjectField(TEXT("expression"), ExpressionInfoToJson(Result.GetValue()));
	Response->SetStringField(TEXT("expression_id"), Result.GetValue().Id);

	return Response;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleDelete(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ExpressionId;
	if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("expression_id is required"));
	}

	auto Result = Service->DeleteExpression(Material, ExpressionId);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	return CreateSuccessResponse();
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleMove(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ExpressionId;
	if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("expression_id is required"));
	}

	int32 PosX = 0, PosY = 0;
	if (!Params->TryGetNumberField(TEXT("pos_x"), PosX) || !Params->TryGetNumberField(TEXT("pos_y"), PosY))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("pos_x and pos_y are required"));
	}

	auto Result = Service->MoveExpression(Material, ExpressionId, PosX, PosY);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	return CreateSuccessResponse();
}

//-----------------------------------------------------------------------------
// Expression Information Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleList(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	auto Result = Service->ListExpressions(Material);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	TArray<TSharedPtr<FJsonValue>> ExpressionsArray;
	for (const FMaterialExpressionInfo& ExprInfo : Result.GetValue())
	{
		ExpressionsArray.Add(MakeShareable(new FJsonValueObject(ExpressionInfoToJson(ExprInfo))));
	}
	Response->SetArrayField(TEXT("expressions"), ExpressionsArray);
	Response->SetNumberField(TEXT("count"), ExpressionsArray.Num());

	return Response;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleGetDetails(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ExpressionId;
	if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("expression_id is required"));
	}

	auto Result = Service->GetExpressionDetails(Material, ExpressionId);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetObjectField(TEXT("expression"), ExpressionInfoToJson(Result.GetValue()));

	return Response;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleGetPins(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ExpressionId;
	if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("expression_id is required"));
	}

	auto Result = Service->GetExpressionPins(Material, ExpressionId);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	TArray<TSharedPtr<FJsonValue>> PinsArray;
	for (const FMaterialPinInfo& PinInfo : Result.GetValue())
	{
		PinsArray.Add(MakeShareable(new FJsonValueObject(PinInfoToJson(PinInfo))));
	}
	Response->SetArrayField(TEXT("pins"), PinsArray);

	return Response;
}

//-----------------------------------------------------------------------------
// Connection Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleConnect(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString SourceExpressionId, SourceOutputName, TargetExpressionId, TargetInputName;
	if (!Params->TryGetStringField(TEXT("source_expression_id"), SourceExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("source_expression_id is required"));
	}
	Params->TryGetStringField(TEXT("source_output"), SourceOutputName);
	if (!Params->TryGetStringField(TEXT("target_expression_id"), TargetExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("target_expression_id is required"));
	}
	if (!Params->TryGetStringField(TEXT("target_input"), TargetInputName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("target_input is required"));
	}

	auto Result = Service->ConnectExpressions(Material, SourceExpressionId, SourceOutputName, TargetExpressionId, TargetInputName);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	return CreateSuccessResponse();
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleDisconnect(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ExpressionId, InputName;
	if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("expression_id is required"));
	}
	if (!Params->TryGetStringField(TEXT("input_name"), InputName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("input_name is required"));
	}

	auto Result = Service->DisconnectInput(Material, ExpressionId, InputName);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	return CreateSuccessResponse();
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleConnectToOutput(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ExpressionId, OutputName, MaterialProperty;
	if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("expression_id is required"));
	}
	Params->TryGetStringField(TEXT("output_name"), OutputName);
	if (!Params->TryGetStringField(TEXT("material_property"), MaterialProperty))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_property is required"));
	}

	auto Result = Service->ConnectToMaterialProperty(Material, ExpressionId, OutputName, MaterialProperty);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	return CreateSuccessResponse();
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleDisconnectOutput(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString MaterialProperty;
	if (!Params->TryGetStringField(TEXT("material_property"), MaterialProperty))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_property is required"));
	}

	auto Result = Service->DisconnectMaterialProperty(Material, MaterialProperty);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	return CreateSuccessResponse();
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleListConnections(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	auto Result = Service->ListConnections(Material);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
	for (const FMaterialConnectionInfo& ConnInfo : Result.GetValue())
	{
		ConnectionsArray.Add(MakeShareable(new FJsonValueObject(ConnectionInfoToJson(ConnInfo))));
	}
	Response->SetArrayField(TEXT("connections"), ConnectionsArray);
	Response->SetNumberField(TEXT("count"), ConnectionsArray.Num());

	return Response;
}

//-----------------------------------------------------------------------------
// Expression Property Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleGetProperty(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ExpressionId, PropertyName;
	if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("expression_id is required"));
	}
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("property_name is required"));
	}

	auto Result = Service->GetExpressionProperty(Material, ExpressionId, PropertyName);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("property_name"), PropertyName);
	Response->SetStringField(TEXT("value"), Result.GetValue());

	return Response;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleSetProperty(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ExpressionId, PropertyName, Value;
	if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("expression_id is required"));
	}
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("property_name is required"));
	}
	if (!Params->TryGetStringField(TEXT("value"), Value))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("value is required"));
	}

	auto Result = Service->SetExpressionProperty(Material, ExpressionId, PropertyName, Value);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	return CreateSuccessResponse();
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleListProperties(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ExpressionId;
	if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("expression_id is required"));
	}

	auto Result = Service->ListExpressionProperties(Material, ExpressionId);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (const auto& Pair : Result.GetValue())
	{
		TSharedPtr<FJsonObject> PropObj = MakeShareable(new FJsonObject);
		PropObj->SetStringField(TEXT("name"), Pair.Key);
		PropObj->SetStringField(TEXT("value"), Pair.Value);
		PropertiesArray.Add(MakeShareable(new FJsonValueObject(PropObj)));
	}
	Response->SetArrayField(TEXT("properties"), PropertiesArray);

	return Response;
}

//-----------------------------------------------------------------------------
// Parameter Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandlePromoteToParameter(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ExpressionId, ParameterName, GroupName;
	if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("expression_id is required"));
	}
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("parameter_name is required"));
	}
	Params->TryGetStringField(TEXT("group_name"), GroupName);

	auto Result = Service->PromoteToParameter(Material, ExpressionId, ParameterName, GroupName);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetObjectField(TEXT("parameter"), ExpressionInfoToJson(Result.GetValue()));
	Response->SetStringField(TEXT("expression_id"), Result.GetValue().Id);

	return Response;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleCreateParameter(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ParameterType, ParameterName, GroupName, DefaultValue;
	if (!Params->TryGetStringField(TEXT("parameter_type"), ParameterType))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("parameter_type is required"));
	}
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("parameter_name is required"));
	}
	Params->TryGetStringField(TEXT("group_name"), GroupName);
	Params->TryGetStringField(TEXT("default_value"), DefaultValue);

	int32 PosX = 0, PosY = 0;
	Params->TryGetNumberField(TEXT("pos_x"), PosX);
	Params->TryGetNumberField(TEXT("pos_y"), PosY);

	auto Result = Service->CreateParameter(Material, ParameterType, ParameterName, GroupName, DefaultValue, PosX, PosY);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetObjectField(TEXT("parameter"), ExpressionInfoToJson(Result.GetValue()));
	Response->SetStringField(TEXT("expression_id"), Result.GetValue().Id);

	return Response;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleSetParameterMetadata(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	FString ExpressionId, GroupName;
	if (!Params->TryGetStringField(TEXT("expression_id"), ExpressionId))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("expression_id is required"));
	}
	Params->TryGetStringField(TEXT("group_name"), GroupName);

	int32 SortPriority = 0;
	Params->TryGetNumberField(TEXT("sort_priority"), SortPriority);

	auto Result = Service->SetParameterMetadata(Material, ExpressionId, GroupName, SortPriority);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	return CreateSuccessResponse();
}

//-----------------------------------------------------------------------------
// Material Output Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleGetOutputProperties(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	auto Result = Service->GetMaterialOutputProperties(Material);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	TArray<TSharedPtr<FJsonValue>> PropertiesArray;
	for (const FString& Prop : Result.GetValue())
	{
		PropertiesArray.Add(MakeShareable(new FJsonValueString(Prop)));
	}
	Response->SetArrayField(TEXT("output_properties"), PropertiesArray);

	return Response;
}

TSharedPtr<FJsonObject> FMaterialNodeCommands::HandleGetOutputConnections(const TSharedPtr<FJsonObject>& Params)
{
	TSharedPtr<FJsonObject> Error;
	UMaterial* Material = LoadMaterialFromParams(Params, Error);
	if (!Material)
	{
		return Error;
	}

	auto Result = Service->GetMaterialOutputConnections(Material);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	
	TSharedPtr<FJsonObject> ConnectionsObj = MakeShareable(new FJsonObject);
	for (const auto& Pair : Result.GetValue())
	{
		ConnectionsObj->SetStringField(Pair.Key, Pair.Value);
	}
	Response->SetObjectField(TEXT("output_connections"), ConnectionsObj);

	return Response;
}
