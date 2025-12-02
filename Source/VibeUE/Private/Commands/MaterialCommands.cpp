// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Commands/MaterialCommands.h"
#include "Services/Material/MaterialService.h"
#include "Core/ServiceContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogMaterialCommands, Log, All);

FMaterialCommands::FMaterialCommands()
{
	TSharedPtr<FServiceContext> Context = MakeShared<FServiceContext>();
	Service = MakeShared<FMaterialService>(Context);
	UE_LOG(LogMaterialCommands, Display, TEXT("MaterialCommands: Initialized"));
}

FMaterialCommands::~FMaterialCommands()
{
	Service.Reset();
	UE_LOG(LogMaterialCommands, Display, TEXT("MaterialCommands: Destroyed"));
}

TSharedPtr<FJsonObject> FMaterialCommands::CreateErrorResponse(const FString& Code, const FString& Message)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error_code"), Code);
	Response->SetStringField(TEXT("error"), Message);
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::CreateSuccessResponse()
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), true);
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType != TEXT("manage_material"))
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
	UE_LOG(LogMaterialCommands, Display, TEXT("MaterialCommands: Handling action '%s'"), *Action);

	// Lifecycle actions
	if (Action == TEXT("create"))
	{
		return HandleCreate(Params);
	}
	else if (Action == TEXT("save"))
	{
		return HandleSave(Params);
	}
	else if (Action == TEXT("compile"))
	{
		return HandleCompile(Params);
	}
	else if (Action == TEXT("refresh_editor"))
	{
		return HandleRefreshEditor(Params);
	}
	// Information actions
	else if (Action == TEXT("get_info"))
	{
		return HandleGetInfo(Params);
	}
	else if (Action == TEXT("list_properties"))
	{
		return HandleListProperties(Params);
	}
	// Property actions
	else if (Action == TEXT("get_property"))
	{
		return HandleGetProperty(Params);
	}
	else if (Action == TEXT("get_property_info"))
	{
		return HandleGetPropertyInfo(Params);
	}
	else if (Action == TEXT("set_property"))
	{
		return HandleSetProperty(Params);
	}
	else if (Action == TEXT("set_properties"))
	{
		return HandleSetProperties(Params);
	}
	// Parameter actions
	else if (Action == TEXT("list_parameters"))
	{
		return HandleListParameters(Params);
	}
	else if (Action == TEXT("get_parameter"))
	{
		return HandleGetParameter(Params);
	}
	else if (Action == TEXT("set_parameter_default"))
	{
		return HandleSetParameterDefault(Params);
	}
	else
	{
		return CreateErrorResponse(TEXT("UNKNOWN_ACTION"), 
			FString::Printf(TEXT("Unknown action: %s"), *Action));
	}
}

//-----------------------------------------------------------------------------
// Lifecycle Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommands::HandleCreate(const TSharedPtr<FJsonObject>& Params)
{
	FString DestinationPath;
	FString MaterialName;

	if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("destination_path is required"));
	}
	if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_name is required"));
	}

	FMaterialCreateParams CreateParams;
	CreateParams.DestinationPath = DestinationPath;
	CreateParams.MaterialName = MaterialName;

	// Optional initial properties
	const TSharedPtr<FJsonObject>* InitialPropsObj;
	if (Params->TryGetObjectField(TEXT("initial_properties"), InitialPropsObj))
	{
		for (const auto& Pair : (*InitialPropsObj)->Values)
		{
			FString Value;
			if (Pair.Value->TryGetString(Value))
			{
				CreateParams.InitialProperties.Add(Pair.Key, Value);
			}
		}
	}

	auto Result = Service->CreateMaterial(CreateParams);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("CREATE_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("material_path"), Result.GetValue());
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created material: %s"), *Result.GetValue()));
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleSave(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
	}

	auto Result = Service->SaveMaterial(MaterialPath);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("SAVE_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("material_path"), MaterialPath);
	Response->SetStringField(TEXT("message"), TEXT("Material saved successfully"));
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleCompile(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
	}

	auto Result = Service->CompileMaterial(MaterialPath);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("COMPILE_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("material_path"), MaterialPath);
	Response->SetStringField(TEXT("message"), TEXT("Material compiled successfully"));
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleRefreshEditor(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
	}

	auto Result = Service->RefreshMaterialEditor(MaterialPath);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("REFRESH_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("material_path"), MaterialPath);
	Response->SetStringField(TEXT("message"), TEXT("Material Editor refreshed successfully"));
	return Response;
}

//-----------------------------------------------------------------------------
// Information Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommands::HandleGetInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
	}

	auto Result = Service->GetMaterialInfo(MaterialPath);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("GET_INFO_FAILED"), Result.GetErrorMessage());
	}

	const FMaterialInfo& Info = Result.GetValue();
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("asset_path"), Info.AssetPath);
	Response->SetStringField(TEXT("name"), Info.Name);
	Response->SetStringField(TEXT("material_domain"), Info.MaterialDomain);
	Response->SetStringField(TEXT("blend_mode"), Info.BlendMode);
	Response->SetStringField(TEXT("shading_model"), Info.ShadingModel);
	Response->SetBoolField(TEXT("two_sided"), Info.bTwoSided);
	Response->SetNumberField(TEXT("expression_count"), Info.ExpressionCount);
	Response->SetNumberField(TEXT("texture_sample_count"), Info.TextureSampleCount);
	Response->SetNumberField(TEXT("parameter_count"), Info.ParameterCount);

	// Parameter names array
	TArray<TSharedPtr<FJsonValue>> ParamNames;
	for (const FString& Name : Info.ParameterNames)
	{
		ParamNames.Add(MakeShareable(new FJsonValueString(Name)));
	}
	Response->SetArrayField(TEXT("parameter_names"), ParamNames);

	// Properties array
	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (const FMaterialPropertyInfo& Prop : Info.Properties)
	{
		TSharedPtr<FJsonObject> PropObj = MakeShareable(new FJsonObject);
		PropObj->SetStringField(TEXT("name"), Prop.Name);
		PropObj->SetStringField(TEXT("display_name"), Prop.DisplayName);
		PropObj->SetStringField(TEXT("type"), Prop.Type);
		PropObj->SetStringField(TEXT("category"), Prop.Category);
		PropObj->SetStringField(TEXT("current_value"), Prop.CurrentValue);
		PropObj->SetBoolField(TEXT("is_editable"), Prop.bIsEditable);
		PropObj->SetBoolField(TEXT("is_advanced"), Prop.bIsAdvanced);
		PropsArray.Add(MakeShareable(new FJsonValueObject(PropObj)));
	}
	Response->SetArrayField(TEXT("properties"), PropsArray);

	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleListProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
	}

	bool bIncludeAdvanced = false;
	Params->TryGetBoolField(TEXT("include_advanced"), bIncludeAdvanced);

	auto Result = Service->ListProperties(MaterialPath, bIncludeAdvanced);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("LIST_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("material_path"), MaterialPath);

	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (const FMaterialPropertyInfo& Prop : Result.GetValue())
	{
		TSharedPtr<FJsonObject> PropObj = MakeShareable(new FJsonObject);
		PropObj->SetStringField(TEXT("name"), Prop.Name);
		PropObj->SetStringField(TEXT("display_name"), Prop.DisplayName);
		PropObj->SetStringField(TEXT("type"), Prop.Type);
		PropObj->SetStringField(TEXT("category"), Prop.Category);
		PropObj->SetStringField(TEXT("tooltip"), Prop.Tooltip);
		PropObj->SetStringField(TEXT("current_value"), Prop.CurrentValue);
		PropObj->SetBoolField(TEXT("is_editable"), Prop.bIsEditable);
		PropObj->SetBoolField(TEXT("is_advanced"), Prop.bIsAdvanced);

		// Add object class for object and struct properties
		if (!Prop.ObjectClass.IsEmpty())
		{
			PropObj->SetStringField(TEXT("object_class"), Prop.ObjectClass);
		}

		// Add allowed values for enums
		if (Prop.AllowedValues.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> AllowedArray;
			for (const FString& Val : Prop.AllowedValues)
			{
				AllowedArray.Add(MakeShareable(new FJsonValueString(Val)));
			}
			PropObj->SetArrayField(TEXT("allowed_values"), AllowedArray);
		}

		// Add struct members for struct properties
		if (Prop.StructMembers.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> MembersArray;
			for (const FStructMemberInfo& Member : Prop.StructMembers)
			{
				TSharedPtr<FJsonObject> MemberObj = MakeShareable(new FJsonObject);
				MemberObj->SetStringField(TEXT("name"), Member.Name);
				MemberObj->SetStringField(TEXT("type"), Member.Type);
				MemberObj->SetStringField(TEXT("current_value"), Member.CurrentValue);
				
				if (!Member.ObjectClass.IsEmpty())
				{
					MemberObj->SetStringField(TEXT("object_class"), Member.ObjectClass);
				}
				
				if (Member.AllowedValues.Num() > 0)
				{
					TArray<TSharedPtr<FJsonValue>> MemberAllowedArray;
					for (const FString& Val : Member.AllowedValues)
					{
						MemberAllowedArray.Add(MakeShareable(new FJsonValueString(Val)));
					}
					MemberObj->SetArrayField(TEXT("allowed_values"), MemberAllowedArray);
				}
				
				MembersArray.Add(MakeShareable(new FJsonValueObject(MemberObj)));
			}
			PropObj->SetArrayField(TEXT("struct_members"), MembersArray);
		}

		PropsArray.Add(MakeShareable(new FJsonValueObject(PropObj)));
	}
	Response->SetArrayField(TEXT("properties"), PropsArray);
	Response->SetNumberField(TEXT("count"), PropsArray.Num());

	return Response;
}

//-----------------------------------------------------------------------------
// Property Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommands::HandleGetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath, PropertyName;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
	}
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("property_name is required"));
	}

	auto Result = Service->GetProperty(MaterialPath, PropertyName);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("GET_PROPERTY_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("material_path"), MaterialPath);
	Response->SetStringField(TEXT("property_name"), PropertyName);
	Response->SetStringField(TEXT("value"), Result.GetValue());
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleGetPropertyInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath, PropertyName;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
	}
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("property_name is required"));
	}

	auto Result = Service->GetPropertyInfo(MaterialPath, PropertyName);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("GET_PROPERTY_INFO_FAILED"), Result.GetErrorMessage());
	}

	const FMaterialPropertyInfo& Prop = Result.GetValue();
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("name"), Prop.Name);
	Response->SetStringField(TEXT("display_name"), Prop.DisplayName);
	Response->SetStringField(TEXT("type"), Prop.Type);
	Response->SetStringField(TEXT("category"), Prop.Category);
	Response->SetStringField(TEXT("tooltip"), Prop.Tooltip);
	Response->SetStringField(TEXT("current_value"), Prop.CurrentValue);
	Response->SetBoolField(TEXT("is_editable"), Prop.bIsEditable);
	Response->SetBoolField(TEXT("is_advanced"), Prop.bIsAdvanced);

	if (Prop.AllowedValues.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> AllowedArray;
		for (const FString& Val : Prop.AllowedValues)
		{
			AllowedArray.Add(MakeShareable(new FJsonValueString(Val)));
		}
		Response->SetArrayField(TEXT("allowed_values"), AllowedArray);
	}

	// Add object class for object and struct properties
	if (!Prop.ObjectClass.IsEmpty())
	{
		Response->SetStringField(TEXT("object_class"), Prop.ObjectClass);
	}

	// Add struct members for struct properties
	if (Prop.StructMembers.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> MembersArray;
		for (const FStructMemberInfo& Member : Prop.StructMembers)
		{
			TSharedPtr<FJsonObject> MemberObj = MakeShareable(new FJsonObject);
			MemberObj->SetStringField(TEXT("name"), Member.Name);
			MemberObj->SetStringField(TEXT("type"), Member.Type);
			MemberObj->SetStringField(TEXT("current_value"), Member.CurrentValue);
			
			if (!Member.ObjectClass.IsEmpty())
			{
				MemberObj->SetStringField(TEXT("object_class"), Member.ObjectClass);
			}
			
			if (Member.AllowedValues.Num() > 0)
			{
				TArray<TSharedPtr<FJsonValue>> MemberAllowedArray;
				for (const FString& Val : Member.AllowedValues)
				{
					MemberAllowedArray.Add(MakeShareable(new FJsonValueString(Val)));
				}
				MemberObj->SetArrayField(TEXT("allowed_values"), MemberAllowedArray);
			}
			
			MembersArray.Add(MakeShareable(new FJsonValueObject(MemberObj)));
		}
		Response->SetArrayField(TEXT("struct_members"), MembersArray);
	}

	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleSetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath, PropertyName, Value;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
	}
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("property_name is required"));
	}
	if (!Params->TryGetStringField(TEXT("value"), Value))
	{
		// Try to get as other types
		bool bBoolValue;
		double NumValue;
		if (Params->TryGetBoolField(TEXT("value"), bBoolValue))
		{
			Value = bBoolValue ? TEXT("true") : TEXT("false");
		}
		else if (Params->TryGetNumberField(TEXT("value"), NumValue))
		{
			Value = FString::SanitizeFloat(NumValue);
		}
		else
		{
			return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("value is required"));
		}
	}

	auto Result = Service->SetProperty(MaterialPath, PropertyName, Value);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("SET_PROPERTY_FAILED"), Result.GetErrorMessage());
	}

	// Get the actual value after engine validation (may differ if clamped)
	FString ActualValue = Result.GetValue();

	// Save the material first so changes persist when editor reloads
	Service->SaveMaterial(MaterialPath);
	
	// Automatically refresh the Material Editor if open to show updated values
	Service->RefreshMaterialEditor(MaterialPath);

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("material_path"), MaterialPath);
	Response->SetStringField(TEXT("property_name"), PropertyName);
	Response->SetStringField(TEXT("value"), ActualValue);
	if (ActualValue != Value)
	{
		Response->SetStringField(TEXT("requested_value"), Value);
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set %s = %s (requested %s, clamped by engine)"), *PropertyName, *ActualValue, *Value));
	}
	else
	{
		Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set %s = %s"), *PropertyName, *ActualValue));
	}
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleSetProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
	}

	const TSharedPtr<FJsonObject>* PropsObj;
	if (!Params->TryGetObjectField(TEXT("properties"), PropsObj))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("properties object is required"));
	}

	TMap<FString, FString> Properties;
	for (const auto& Pair : (*PropsObj)->Values)
	{
		FString Value;
		if (Pair.Value->TryGetString(Value))
		{
			Properties.Add(Pair.Key, Value);
		}
		else
		{
			bool bBoolValue;
			double NumValue;
			if (Pair.Value->TryGetBool(bBoolValue))
			{
				Properties.Add(Pair.Key, bBoolValue ? TEXT("true") : TEXT("false"));
			}
			else if (Pair.Value->TryGetNumber(NumValue))
			{
				Properties.Add(Pair.Key, FString::SanitizeFloat(NumValue));
			}
		}
	}

	auto Result = Service->SetProperties(MaterialPath, Properties);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("SET_PROPERTIES_FAILED"), Result.GetErrorMessage());
	}

	// Save the material first so changes persist when editor reloads
	Service->SaveMaterial(MaterialPath);
	
	// Automatically refresh the Material Editor if open to show updated values
	Service->RefreshMaterialEditor(MaterialPath);

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("material_path"), MaterialPath);
	Response->SetNumberField(TEXT("properties_set"), Properties.Num());
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set %d properties"), Properties.Num()));
	return Response;
}

//-----------------------------------------------------------------------------
// Parameter Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommands::HandleListParameters(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
	}

	auto Result = Service->ListParameters(MaterialPath);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("LIST_PARAMS_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("material_path"), MaterialPath);

	TArray<TSharedPtr<FJsonValue>> ParamsArray;
	for (const FVibeMaterialParamInfo& Param : Result.GetValue())
	{
		TSharedPtr<FJsonObject> ParamObj = MakeShareable(new FJsonObject);
		ParamObj->SetStringField(TEXT("name"), Param.Name);
		ParamObj->SetStringField(TEXT("type"), Param.Type);
		ParamObj->SetStringField(TEXT("group"), Param.Group);
		ParamObj->SetStringField(TEXT("current_value"), Param.CurrentValue);
		ParamObj->SetStringField(TEXT("default_value"), Param.DefaultValue);
		ParamObj->SetNumberField(TEXT("sort_priority"), Param.SortPriority);
		ParamObj->SetBoolField(TEXT("is_exposed"), Param.bIsExposed);
		ParamsArray.Add(MakeShareable(new FJsonValueObject(ParamObj)));
	}
	Response->SetArrayField(TEXT("parameters"), ParamsArray);
	Response->SetNumberField(TEXT("count"), ParamsArray.Num());

	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleGetParameter(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath, ParameterName;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
	}
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("parameter_name is required"));
	}

	auto Result = Service->GetParameter(MaterialPath, ParameterName);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("GET_PARAM_FAILED"), Result.GetErrorMessage());
	}

	const FVibeMaterialParamInfo& Param = Result.GetValue();

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("name"), Param.Name);
	Response->SetStringField(TEXT("type"), Param.Type);
	Response->SetStringField(TEXT("group"), Param.Group);
	Response->SetStringField(TEXT("current_value"), Param.CurrentValue);
	Response->SetStringField(TEXT("default_value"), Param.DefaultValue);
	Response->SetNumberField(TEXT("sort_priority"), Param.SortPriority);
	Response->SetBoolField(TEXT("is_exposed"), Param.bIsExposed);
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleSetParameterDefault(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath, ParameterName, Value;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("material_path is required"));
	}
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("parameter_name is required"));
	}
	if (!Params->TryGetStringField(TEXT("value"), Value))
	{
		double NumValue;
		if (Params->TryGetNumberField(TEXT("value"), NumValue))
		{
			Value = FString::SanitizeFloat(NumValue);
		}
		else
		{
			return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("value is required"));
		}
	}

	auto Result = Service->SetParameterDefault(MaterialPath, ParameterName, Value);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("SET_PARAM_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("material_path"), MaterialPath);
	Response->SetStringField(TEXT("parameter_name"), ParameterName);
	Response->SetStringField(TEXT("value"), Value);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set parameter %s = %s"), *ParameterName, *Value));
	return Response;
}
