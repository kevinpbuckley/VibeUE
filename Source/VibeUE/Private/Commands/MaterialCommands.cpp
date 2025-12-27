// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Commands/MaterialCommands.h"
#include "Services/Material/MaterialService.h"
#include "Core/ServiceContext.h"
#include "Core/JsonValueHelper.h"
#include "Utils/HelpFileReader.h"
#include "Utils/ParamValidation.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

DEFINE_LOG_CATEGORY_STATIC(LogMaterialCommands, Log, All);

// Parameter validation for manage_material actions
namespace MaterialParams
{
	inline const TArray<FString>& CreateParams()
	{
		static const TArray<FString> Params = {
			TEXT("destination_path"), TEXT("material_name"), TEXT("initial_properties")
		};
		return Params;
	}
	
	inline const TArray<FString>& CreateInstanceParams()
	{
		static const TArray<FString> Params = {
			TEXT("parent_material_path"), TEXT("destination_path"), TEXT("instance_name"),
			TEXT("scalar_parameters"), TEXT("vector_parameters"), TEXT("texture_parameters")
		};
		return Params;
	}
	
	inline const TArray<FString>& MaterialPathParams()
	{
		static const TArray<FString> Params = {
			TEXT("material_path")
		};
		return Params;
	}
	
	inline const TArray<FString>& PropertyParams()
	{
		static const TArray<FString> Params = {
			TEXT("material_path"), TEXT("property_name"), TEXT("property_value")
		};
		return Params;
	}
	
	inline const TArray<FString>& ParameterParams()
	{
		static const TArray<FString> Params = {
			TEXT("material_path"), TEXT("parameter_name"), TEXT("default_value")
		};
		return Params;
	}
	
	inline const TArray<FString>& InstanceParams()
	{
		static const TArray<FString> Params = {
			TEXT("instance_path"), TEXT("parameter_name"), TEXT("value")
		};
		return Params;
	}
}

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

	// Help action
	if (Action == TEXT("help"))
	{
		return HandleHelp(Params);
	}

	// Lifecycle actions
	if (Action == TEXT("create"))
	{
		return HandleCreate(Params);
	}
	else if (Action == TEXT("create_instance"))
	{
		return HandleCreateInstance(Params);
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
	else if (Action == TEXT("open") || Action == TEXT("open_in_editor"))
	{
		return HandleOpenInEditor(Params);
	}
	// Information actions
	else if (Action == TEXT("get_info"))
	{
		return HandleGetInfo(Params);
	}
	else if (Action == TEXT("summarize"))
	{
		return HandleSummarize(Params);
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
	// Instance information actions
	else if (Action == TEXT("get_instance_info"))
	{
		return HandleGetInstanceInfo(Params);
	}
	else if (Action == TEXT("list_instance_properties"))
	{
		return HandleListInstanceProperties(Params);
	}
	// Instance property actions
	else if (Action == TEXT("get_instance_property"))
	{
		return HandleGetInstanceProperty(Params);
	}
	else if (Action == TEXT("set_instance_property"))
	{
		return HandleSetInstanceProperty(Params);
	}
	// Instance parameter actions
	else if (Action == TEXT("list_instance_parameters"))
	{
		return HandleListInstanceParameters(Params);
	}
	else if (Action == TEXT("set_instance_scalar_parameter"))
	{
		return HandleSetInstanceScalarParameter(Params);
	}
	else if (Action == TEXT("set_instance_vector_parameter"))
	{
		return HandleSetInstanceVectorParameter(Params);
	}
	else if (Action == TEXT("set_instance_texture_parameter"))
	{
		return HandleSetInstanceTextureParameter(Params);
	}
	else if (Action == TEXT("clear_instance_parameter_override"))
	{
		return HandleClearInstanceParameterOverride(Params);
	}
	else if (Action == TEXT("save_instance"))
	{
		return HandleSaveInstance(Params);
	}
	else
	{
		// Enhanced error response with available commands
		TSharedPtr<FJsonObject> Response = CreateErrorResponse(TEXT("UNKNOWN_ACTION"), 
			FString::Printf(TEXT("Unknown action: %s"), *Action));
		
		// Add array of available commands
		TArray<TSharedPtr<FJsonValue>> AvailableCommands;
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("help")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("create")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("create_instance")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("save")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("compile")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("refresh_editor")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("open")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("get_info")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("summarize")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("list_properties")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("get_property")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("get_property_info")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("set_property")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("set_properties")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("list_parameters")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("get_parameter")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("set_parameter_default")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("get_instance_info")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("list_instance_properties")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("get_instance_property")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("set_instance_property")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("list_instance_parameters")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("set_instance_scalar_parameter")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("set_instance_vector_parameter")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("set_instance_texture_parameter")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("clear_instance_parameter_override")));
		AvailableCommands.Add(MakeShared<FJsonValueString>(TEXT("save_instance")));
		Response->SetArrayField(TEXT("available_commands"), AvailableCommands);
		
		Response->SetStringField(TEXT("action"), Action);
		Response->SetStringField(TEXT("tool"), TEXT("manage_material"));
		Response->SetStringField(TEXT("help_tip"), TEXT("Use manage_material(action='help') to see all available actions and their parameters."));
		
		return Response;
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
		return ParamValidation::MissingParamsError(
			TEXT("destination_path is required"),
			MaterialParams::CreateParams());
	}
	if (!Params->TryGetStringField(TEXT("material_name"), MaterialName))
	{
		return ParamValidation::MissingParamsError(
			TEXT("material_name is required"),
			MaterialParams::CreateParams());
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

TSharedPtr<FJsonObject> FMaterialCommands::HandleCreateInstance(const TSharedPtr<FJsonObject>& Params)
{
	FString ParentMaterialPath;
	FString DestinationPath;
	FString InstanceName;

	if (!Params->TryGetStringField(TEXT("parent_material_path"), ParentMaterialPath))
	{
		return ParamValidation::MissingParamsError(
			TEXT("parent_material_path is required"),
			MaterialParams::CreateInstanceParams());
	}
	if (!Params->TryGetStringField(TEXT("destination_path"), DestinationPath))
	{
		return ParamValidation::MissingParamsError(
			TEXT("destination_path is required"),
			MaterialParams::CreateInstanceParams());
	}
	if (!Params->TryGetStringField(TEXT("instance_name"), InstanceName))
	{
		return ParamValidation::MissingParamsError(
			TEXT("instance_name is required"),
			MaterialParams::CreateInstanceParams());
	}

	FMaterialInstanceCreateParams CreateParams;
	CreateParams.ParentMaterialPath = ParentMaterialPath;
	CreateParams.DestinationPath = DestinationPath;
	CreateParams.InstanceName = InstanceName;

	// Optional scalar parameter overrides
	const TSharedPtr<FJsonObject>* ScalarParamsObj;
	if (Params->TryGetObjectField(TEXT("scalar_parameters"), ScalarParamsObj))
	{
		for (const auto& Pair : (*ScalarParamsObj)->Values)
		{
			double Value;
			if (Pair.Value->TryGetNumber(Value))
			{
				CreateParams.ScalarParameters.Add(Pair.Key, static_cast<float>(Value));
			}
		}
	}

	// Optional vector parameter overrides
	const TSharedPtr<FJsonObject>* VectorParamsObj;
	if (Params->TryGetObjectField(TEXT("vector_parameters"), VectorParamsObj))
	{
		for (const auto& Pair : (*VectorParamsObj)->Values)
		{
			// Use FJsonValueHelper for robust color parsing (arrays, objects, hex, named colors)
			FLinearColor Color;
			if (FJsonValueHelper::TryGetLinearColor(Pair.Value, Color))
			{
				CreateParams.VectorParameters.Add(Pair.Key, Color);
			}
		}
	}

	// Optional texture parameter overrides
	const TSharedPtr<FJsonObject>* TextureParamsObj;
	if (Params->TryGetObjectField(TEXT("texture_parameters"), TextureParamsObj))
	{
		for (const auto& Pair : (*TextureParamsObj)->Values)
		{
			FString Value;
			if (Pair.Value->TryGetString(Value))
			{
				CreateParams.TextureParameters.Add(Pair.Key, Value);
			}
		}
	}

	auto Result = Service->CreateMaterialInstance(CreateParams);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("CREATE_INSTANCE_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("instance_path"), Result.GetValue());
	Response->SetStringField(TEXT("parent_material_path"), ParentMaterialPath);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Created material instance: %s"), *Result.GetValue()));
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleSave(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return ParamValidation::MissingParamsError(
			TEXT("material_path is required"),
			MaterialParams::MaterialPathParams());
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
		return ParamValidation::MissingParamsError(
			TEXT("material_path is required"),
			MaterialParams::MaterialPathParams());
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
		return ParamValidation::MissingParamsError(
			TEXT("material_path is required"),
			MaterialParams::MaterialPathParams());
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

TSharedPtr<FJsonObject> FMaterialCommands::HandleOpenInEditor(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return ParamValidation::MissingParamsError(
			TEXT("material_path is required"),
			MaterialParams::MaterialPathParams());
	}

	// Load the material asset
	UObject* Asset = StaticLoadObject(UMaterial::StaticClass(), nullptr, *MaterialPath);
	if (!Asset)
	{
		// Try loading as material instance
		Asset = StaticLoadObject(UMaterialInstance::StaticClass(), nullptr, *MaterialPath);
	}
	
	if (!Asset)
	{
		return CreateErrorResponse(TEXT("MATERIAL_NOT_FOUND"), 
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	// Open in asset editor
	if (GEditor)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Asset);
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("material_path"), MaterialPath);
	Response->SetStringField(TEXT("message"), TEXT("Material opened in editor"));
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
		return ParamValidation::MissingParamsError(
			TEXT("material_path is required"),
			MaterialParams::MaterialPathParams());
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

TSharedPtr<FJsonObject> FMaterialCommands::HandleSummarize(const TSharedPtr<FJsonObject>& Params)
{
	// Comprehensive material summary designed for AI understanding
	// Returns everything needed in ONE call with clear categorization
	
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return ParamValidation::MissingParamsError(
			TEXT("material_path is required"),
			MaterialParams::MaterialPathParams());
	}

	auto InfoResult = Service->GetMaterialInfo(MaterialPath);
	if (!InfoResult.IsSuccess())
	{
		return CreateErrorResponse(TEXT("GET_INFO_FAILED"), InfoResult.GetErrorMessage());
	}

	const FMaterialInfo& Info = InfoResult.GetValue();
	
	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("material_path"), MaterialPath);
	Response->SetStringField(TEXT("name"), Info.Name);
	
	// Current material configuration (most important for understanding state)
	TSharedPtr<FJsonObject> ConfigObj = MakeShareable(new FJsonObject);
	ConfigObj->SetStringField(TEXT("domain"), Info.MaterialDomain);
	ConfigObj->SetStringField(TEXT("blend_mode"), Info.BlendMode);
	ConfigObj->SetStringField(TEXT("shading_model"), Info.ShadingModel);
	ConfigObj->SetBoolField(TEXT("two_sided"), Info.bTwoSided);
	Response->SetObjectField(TEXT("current_config"), ConfigObj);
	
	// Separate properties into categories for AI clarity
	TArray<TSharedPtr<FJsonValue>> EditableProps;      // Properties that CAN be set with set_property
	TArray<TSharedPtr<FJsonValue>> ReadOnlyProps;      // Informational only
	
	// Key editable properties to highlight (most commonly used)
	TArray<FString> KeyEditableProps = {
		TEXT("TwoSided"), TEXT("BlendMode"), TEXT("ShadingModel"), TEXT("MaterialDomain"),
		TEXT("OpacityMaskClipValue"), TEXT("bCastDynamicShadowAsMasked"), 
		TEXT("DitheredLODTransition"), TEXT("bTangentSpaceNormal")
	};
	
	TArray<TSharedPtr<FJsonValue>> KeyProps;
	
	for (const FMaterialPropertyInfo& Prop : Info.Properties)
	{
		TSharedPtr<FJsonObject> PropObj = MakeShareable(new FJsonObject);
		PropObj->SetStringField(TEXT("name"), Prop.Name);
		PropObj->SetStringField(TEXT("type"), Prop.Type);
		PropObj->SetStringField(TEXT("value"), Prop.CurrentValue);
		
		// Add enum options if available
		if (Prop.AllowedValues.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> AllowedArray;
			for (const FString& Val : Prop.AllowedValues)
			{
				AllowedArray.Add(MakeShareable(new FJsonValueString(Val)));
			}
			PropObj->SetArrayField(TEXT("options"), AllowedArray);
		}
		
		if (Prop.bIsEditable)
		{
			PropObj->SetStringField(TEXT("how_to_modify"), 
				TEXT("Use set_property action with property_name and property_value"));
			
			// Add to key props if it's a commonly used property
			if (KeyEditableProps.Contains(Prop.Name))
			{
				KeyProps.Add(MakeShareable(new FJsonValueObject(PropObj)));
			}
			else
			{
				EditableProps.Add(MakeShareable(new FJsonValueObject(PropObj)));
			}
		}
		else
		{
			ReadOnlyProps.Add(MakeShareable(new FJsonValueObject(PropObj)));
		}
	}
	
	// Key properties section (most commonly used)
	TSharedPtr<FJsonObject> KeySection = MakeShareable(new FJsonObject);
	KeySection->SetStringField(TEXT("description"), 
		TEXT("Most commonly used properties - can be changed with set_property action"));
	KeySection->SetArrayField(TEXT("properties"), KeyProps);
	KeySection->SetNumberField(TEXT("count"), KeyProps.Num());
	Response->SetObjectField(TEXT("key_properties"), KeySection);
	
	// All other editable properties
	TSharedPtr<FJsonObject> EditableSection = MakeShareable(new FJsonObject);
	EditableSection->SetStringField(TEXT("description"), 
		TEXT("Additional properties that can be changed with set_property action"));
	EditableSection->SetArrayField(TEXT("properties"), EditableProps);
	EditableSection->SetNumberField(TEXT("count"), EditableProps.Num());
	Response->SetObjectField(TEXT("other_editable_properties"), EditableSection);
	
	// Material inputs - these are the graph connections (hardcoded list of common inputs)
	// These are NOT in Info.Properties because they're not CPF_Edit, but they ARE queryable
	TArray<TSharedPtr<FJsonValue>> MaterialInputs;
	
	// Common material inputs that need graph nodes to modify
	TArray<FString> CommonInputNames = {
		TEXT("BaseColor"), TEXT("Metallic"), TEXT("Specular"), TEXT("Roughness"),
		TEXT("Anisotropy"), TEXT("EmissiveColor"), TEXT("Opacity"), TEXT("OpacityMask"),
		TEXT("Normal"), TEXT("Tangent"), TEXT("WorldPositionOffset"), TEXT("SubsurfaceColor"),
		TEXT("ClearCoat"), TEXT("ClearCoatRoughness"), TEXT("AmbientOcclusion"),
		TEXT("Refraction"), TEXT("PixelDepthOffset"), TEXT("ShadingModelFromMaterialExpression"),
		TEXT("FrontMaterial"), TEXT("Displacement")
	};
	
	for (const FString& InputName : CommonInputNames)
	{
		auto PropResult = Service->GetPropertyInfo(MaterialPath, InputName);
		if (PropResult.IsSuccess())
		{
			const FMaterialPropertyInfo& Prop = PropResult.GetValue();
			TSharedPtr<FJsonObject> InputObj = MakeShareable(new FJsonObject);
			InputObj->SetStringField(TEXT("name"), Prop.Name);
			InputObj->SetStringField(TEXT("type"), Prop.Type);
			InputObj->SetStringField(TEXT("current_value"), Prop.CurrentValue);
			InputObj->SetBoolField(TEXT("is_connected"), !Prop.CurrentValue.IsEmpty() && Prop.CurrentValue != TEXT("()"));
			InputObj->SetStringField(TEXT("how_to_modify"), 
				TEXT("Create a Constant or Parameter node with manage_material_node and connect it to this input"));
			MaterialInputs.Add(MakeShareable(new FJsonValueObject(InputObj)));
		}
	}
	
	TSharedPtr<FJsonObject> InputsSection = MakeShareable(new FJsonObject);
	InputsSection->SetStringField(TEXT("description"), 
		TEXT("Material graph inputs - these CANNOT be set with set_property. Use manage_material_node tool to create Constant or Parameter nodes and connect them."));
	InputsSection->SetArrayField(TEXT("inputs"), MaterialInputs);
	InputsSection->SetNumberField(TEXT("count"), MaterialInputs.Num());
	InputsSection->SetStringField(TEXT("important"), 
		TEXT("To set BaseColor, Roughness, Metallic etc., you must create material graph nodes!"));
	InputsSection->SetStringField(TEXT("example_roughness"), 
		TEXT("(1) manage_material_node(action='create', expression_class='MaterialExpressionConstant', material_path=...) (2) set its R value (3) connect output to Roughness"));
	InputsSection->SetStringField(TEXT("example_basecolor"), 
		TEXT("(1) manage_material_node(action='create', expression_class='MaterialExpressionConstant3Vector', material_path=...) (2) set RGB values (3) connect output to BaseColor"));
	Response->SetObjectField(TEXT("material_inputs"), InputsSection);
	
	// Parameters (runtime-controllable)
	TSharedPtr<FJsonObject> ParamsSection = MakeShareable(new FJsonObject);
	ParamsSection->SetStringField(TEXT("description"), 
		TEXT("Material parameters exposed for runtime control and material instances"));
	TArray<TSharedPtr<FJsonValue>> ParamNames;
	for (const FString& Name : Info.ParameterNames)
	{
		ParamNames.Add(MakeShareable(new FJsonValueString(Name)));
	}
	ParamsSection->SetArrayField(TEXT("parameter_names"), ParamNames);
	ParamsSection->SetNumberField(TEXT("count"), ParamNames.Num());
	ParamsSection->SetStringField(TEXT("tip"), 
		TEXT("Use list_parameters for full parameter details including types and default values"));
	Response->SetObjectField(TEXT("parameters"), ParamsSection);
	
	// Graph statistics
	TSharedPtr<FJsonObject> GraphStats = MakeShareable(new FJsonObject);
	GraphStats->SetNumberField(TEXT("expression_count"), Info.ExpressionCount);
	GraphStats->SetNumberField(TEXT("texture_sample_count"), Info.TextureSampleCount);
	Response->SetObjectField(TEXT("graph_stats"), GraphStats);
	
	// Usage guidance - clear examples
	TSharedPtr<FJsonObject> UsageGuide = MakeShareable(new FJsonObject);
	UsageGuide->SetStringField(TEXT("change_blend_mode"), TEXT("manage_material(action='set_property', material_path='...', property_name='BlendMode', property_value='BLEND_Masked')"));
	UsageGuide->SetStringField(TEXT("enable_two_sided"), TEXT("manage_material(action='set_property', material_path='...', property_name='TwoSided', property_value='true')"));
	UsageGuide->SetStringField(TEXT("set_opacity_clip"), TEXT("manage_material(action='set_property', material_path='...', property_name='OpacityMaskClipValue', property_value='0.5')"));
	UsageGuide->SetStringField(TEXT("set_roughness"), TEXT("Use manage_material_node: create MaterialExpressionConstant, connect to Roughness input"));
	UsageGuide->SetStringField(TEXT("set_base_color"), TEXT("Use manage_material_node: create MaterialExpressionConstant3Vector, connect to BaseColor input"));
	Response->SetObjectField(TEXT("usage_guide"), UsageGuide);

	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleListProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString MaterialPath;
	if (!Params->TryGetStringField(TEXT("material_path"), MaterialPath))
	{
		return ParamValidation::MissingParamsError(
			TEXT("material_path is required"),
			MaterialParams::MaterialPathParams());
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
		return ParamValidation::MissingParamsError(
			TEXT("material_path is required"),
			MaterialParams::PropertyParams());
	}
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return ParamValidation::MissingParamsError(
			TEXT("property_name is required"),
			MaterialParams::PropertyParams());
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
		return ParamValidation::MissingParamsError(
			TEXT("material_path is required"),
			MaterialParams::PropertyParams());
	}
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return ParamValidation::MissingParamsError(
			TEXT("property_name is required"),
			MaterialParams::PropertyParams());
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
	// Try both "value" and "property_value" parameters (property_value is the documented parameter)
	if (!Params->TryGetStringField(TEXT("value"), Value) && !Params->TryGetStringField(TEXT("property_value"), Value))
	{
		// Try to get as other types from both parameter names
		bool bBoolValue;
		double NumValue;
		if (Params->TryGetBoolField(TEXT("value"), bBoolValue) || Params->TryGetBoolField(TEXT("property_value"), bBoolValue))
		{
			Value = bBoolValue ? TEXT("true") : TEXT("false");
		}
		else if (Params->TryGetNumberField(TEXT("value"), NumValue) || Params->TryGetNumberField(TEXT("property_value"), NumValue))
		{
			Value = FString::SanitizeFloat(NumValue);
		}
		else
		{
			return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("value or property_value is required"));
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

	TMap<FString, FString> Properties;
	
	// Try to get properties as an object first: {"properties": {"BlendMode": "Masked", "TwoSided": true}}
	const TSharedPtr<FJsonObject>* PropsObj;
	if (Params->TryGetObjectField(TEXT("properties"), PropsObj))
	{
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
	}
	// Also try array format: {"properties": [{"name": "BlendMode", "value": "Masked"}, ...]}
	else
	{
		const TArray<TSharedPtr<FJsonValue>>* PropsArray;
		if (Params->TryGetArrayField(TEXT("properties"), PropsArray))
		{
			for (const TSharedPtr<FJsonValue>& ArrayItem : *PropsArray)
			{
				const TSharedPtr<FJsonObject>* PropObj;
				if (ArrayItem->TryGetObject(PropObj))
				{
					FString PropName, PropValue;
					if ((*PropObj)->TryGetStringField(TEXT("name"), PropName))
					{
						// Get value as string, bool, or number
						if ((*PropObj)->TryGetStringField(TEXT("value"), PropValue))
						{
							Properties.Add(PropName, PropValue);
						}
						else
						{
							bool bBoolValue;
							double NumValue;
							if ((*PropObj)->TryGetBoolField(TEXT("value"), bBoolValue))
							{
								Properties.Add(PropName, bBoolValue ? TEXT("true") : TEXT("false"));
							}
							else if ((*PropObj)->TryGetNumberField(TEXT("value"), NumValue))
							{
								Properties.Add(PropName, FString::SanitizeFloat(NumValue));
							}
						}
					}
				}
			}
		}
		else
		{
			return CreateErrorResponse(TEXT("MISSING_PARAM"), 
				TEXT("properties is required. Accepts object format {\"BlendMode\": \"Masked\"} or array format [{\"name\": \"BlendMode\", \"value\": \"Masked\"}]"));
		}
	}
	
	if (Properties.Num() == 0)
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("No valid properties found to set"));
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
	// Try both "value" and "property_value" parameters
	if (!Params->TryGetStringField(TEXT("value"), Value) && !Params->TryGetStringField(TEXT("property_value"), Value))
	{
		double NumValue;
		if (Params->TryGetNumberField(TEXT("value"), NumValue) || Params->TryGetNumberField(TEXT("property_value"), NumValue))
		{
			Value = FString::SanitizeFloat(NumValue);
		}
		else
		{
			return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("value or property_value is required"));
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

//-----------------------------------------------------------------------------
// Instance Information Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommands::HandleGetInstanceInfo(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath;
	if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("instance_path is required"));
	}

	auto Result = Service->GetInstanceInfo(InstancePath);
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
	Response->SetBoolField(TEXT("two_sided"), Info.bTwoSided);
	Response->SetNumberField(TEXT("parameter_count"), Info.ParameterCount);
	
	// Parameter names (includes parent info and override details)
	TArray<TSharedPtr<FJsonValue>> ParamNamesJson;
	for (const FString& ParamName : Info.ParameterNames)
	{
		ParamNamesJson.Add(MakeShareable(new FJsonValueString(ParamName)));
	}
	Response->SetArrayField(TEXT("parameter_info"), ParamNamesJson);

	// Properties
	TArray<TSharedPtr<FJsonValue>> PropsJson;
	for (const FMaterialPropertyInfo& PropInfo : Info.Properties)
	{
		TSharedPtr<FJsonObject> PropObj = MakeShareable(new FJsonObject);
		PropObj->SetStringField(TEXT("name"), PropInfo.Name);
		PropObj->SetStringField(TEXT("display_name"), PropInfo.DisplayName);
		PropObj->SetStringField(TEXT("type"), PropInfo.Type);
		PropObj->SetStringField(TEXT("category"), PropInfo.Category);
		PropObj->SetStringField(TEXT("current_value"), PropInfo.CurrentValue);
		PropObj->SetBoolField(TEXT("is_editable"), PropInfo.bIsEditable);
		PropObj->SetBoolField(TEXT("is_advanced"), PropInfo.bIsAdvanced);
		PropsJson.Add(MakeShareable(new FJsonValueObject(PropObj)));
	}
	Response->SetArrayField(TEXT("properties"), PropsJson);

	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleListInstanceProperties(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath;
	if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("instance_path is required"));
	}

	bool bIncludeAdvanced = true;
	Params->TryGetBoolField(TEXT("include_advanced"), bIncludeAdvanced);

	auto Result = Service->ListInstanceProperties(InstancePath, bIncludeAdvanced);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("LIST_PROPS_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("instance_path"), InstancePath);
	
	TArray<TSharedPtr<FJsonValue>> PropsJson;
	for (const FMaterialPropertyInfo& PropInfo : Result.GetValue())
	{
		TSharedPtr<FJsonObject> PropObj = MakeShareable(new FJsonObject);
		PropObj->SetStringField(TEXT("name"), PropInfo.Name);
		PropObj->SetStringField(TEXT("display_name"), PropInfo.DisplayName);
		PropObj->SetStringField(TEXT("type"), PropInfo.Type);
		PropObj->SetStringField(TEXT("category"), PropInfo.Category);
		PropObj->SetStringField(TEXT("current_value"), PropInfo.CurrentValue);
		PropObj->SetBoolField(TEXT("is_editable"), PropInfo.bIsEditable);
		PropObj->SetBoolField(TEXT("is_advanced"), PropInfo.bIsAdvanced);
		
		if (!PropInfo.Tooltip.IsEmpty())
		{
			PropObj->SetStringField(TEXT("tooltip"), PropInfo.Tooltip);
		}
		
		if (!PropInfo.ObjectClass.IsEmpty())
		{
			PropObj->SetStringField(TEXT("object_class"), PropInfo.ObjectClass);
		}
		
		if (PropInfo.AllowedValues.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> AllowedJson;
			for (const FString& Val : PropInfo.AllowedValues)
			{
				AllowedJson.Add(MakeShareable(new FJsonValueString(Val)));
			}
			PropObj->SetArrayField(TEXT("allowed_values"), AllowedJson);
		}
		
		PropsJson.Add(MakeShareable(new FJsonValueObject(PropObj)));
	}
	
	Response->SetArrayField(TEXT("properties"), PropsJson);
	Response->SetNumberField(TEXT("count"), PropsJson.Num());
	return Response;
}

//-----------------------------------------------------------------------------
// Instance Property Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommands::HandleGetInstanceProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath, PropertyName;
	if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("instance_path is required"));
	}
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("property_name is required"));
	}

	auto Result = Service->GetInstanceProperty(InstancePath, PropertyName);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("GET_PROP_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("instance_path"), InstancePath);
	Response->SetStringField(TEXT("property_name"), PropertyName);
	Response->SetStringField(TEXT("value"), Result.GetValue());
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleSetInstanceProperty(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath, PropertyName, Value;
	if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("instance_path is required"));
	}
	if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("property_name is required"));
	}
	// Try both "value" and "property_value" parameters
	if (!Params->TryGetStringField(TEXT("value"), Value) && !Params->TryGetStringField(TEXT("property_value"), Value))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("value or property_value is required"));
	}

	auto Result = Service->SetInstanceProperty(InstancePath, PropertyName, Value);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("SET_PROP_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("instance_path"), InstancePath);
	Response->SetStringField(TEXT("property_name"), PropertyName);
	Response->SetStringField(TEXT("value"), Value);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set %s = %s"), *PropertyName, *Value));
	return Response;
}

//-----------------------------------------------------------------------------
// Instance Parameter Actions
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommands::HandleListInstanceParameters(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath;
	if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("instance_path is required"));
	}

	auto Result = Service->ListInstanceParameters(InstancePath);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("LIST_PARAMS_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("instance_path"), InstancePath);
	
	TArray<TSharedPtr<FJsonValue>> ParamsJson;
	for (const FVibeMaterialParamInfo& ParamInfo : Result.GetValue())
	{
		TSharedPtr<FJsonObject> ParamObj = MakeShareable(new FJsonObject);
		ParamObj->SetStringField(TEXT("name"), ParamInfo.Name);
		ParamObj->SetStringField(TEXT("type"), ParamInfo.Type);
		ParamObj->SetStringField(TEXT("current_value"), ParamInfo.CurrentValue);
		ParamsJson.Add(MakeShareable(new FJsonValueObject(ParamObj)));
	}
	
	Response->SetArrayField(TEXT("parameters"), ParamsJson);
	Response->SetNumberField(TEXT("count"), ParamsJson.Num());
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleSetInstanceScalarParameter(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath, ParameterName;
	double Value;
	
	if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("instance_path is required"));
	}
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("parameter_name is required"));
	}
	if (!Params->TryGetNumberField(TEXT("value"), Value))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("value (number) is required"));
	}

	auto Result = Service->SetInstanceScalarParameter(InstancePath, ParameterName, (float)Value);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("SET_PARAM_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("instance_path"), InstancePath);
	Response->SetStringField(TEXT("parameter_name"), ParameterName);
	Response->SetNumberField(TEXT("value"), Value);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set scalar parameter %s = %f"), *ParameterName, Value));
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleSetInstanceVectorParameter(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath, ParameterName;
	
	if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("instance_path is required"));
	}
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("parameter_name is required"));
	}
	
	// Use FJsonValueHelper for robust color parsing (arrays, objects, hex, named colors)
	FLinearColor ColorValue = FLinearColor::Black;
	
	// Try to get value from 'value' field first
	TSharedPtr<FJsonValue> ValueField = Params->TryGetField(TEXT("value"));
	if (ValueField.IsValid() && FJsonValueHelper::TryGetLinearColor(ValueField, ColorValue))
	{
		// Successfully parsed from value field
	}
	else
	{
		// Fallback: Try to get individual R, G, B, A fields directly from params
		double R, G, B, A = 1.0;
		if (Params->TryGetNumberField(TEXT("r"), R) && Params->TryGetNumberField(TEXT("g"), G) && Params->TryGetNumberField(TEXT("b"), B))
		{
			Params->TryGetNumberField(TEXT("a"), A);
			ColorValue = FLinearColor(R, G, B, A);
		}
		else
		{
			return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("value is required. Supports: arrays [r,g,b,a], objects {R,G,B,A}, hex '#RRGGBB', or named colors like 'red', 'orange'"));
		}
	}

	auto Result = Service->SetInstanceVectorParameter(InstancePath, ParameterName, ColorValue);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("SET_PARAM_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("instance_path"), InstancePath);
	Response->SetStringField(TEXT("parameter_name"), ParameterName);
	Response->SetStringField(TEXT("value"), FString::Printf(TEXT("(%f,%f,%f,%f)"), ColorValue.R, ColorValue.G, ColorValue.B, ColorValue.A));
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set vector parameter %s"), *ParameterName));
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleSetInstanceTextureParameter(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath, ParameterName, TexturePath;
	
	if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("instance_path is required"));
	}
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("parameter_name is required"));
	}
	if (!Params->TryGetStringField(TEXT("texture_path"), TexturePath))
	{
		TexturePath = TEXT(""); // Allow clearing to None
	}

	auto Result = Service->SetInstanceTextureParameter(InstancePath, ParameterName, TexturePath);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("SET_PARAM_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("instance_path"), InstancePath);
	Response->SetStringField(TEXT("parameter_name"), ParameterName);
	Response->SetStringField(TEXT("texture_path"), TexturePath);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Set texture parameter %s = %s"), *ParameterName, *TexturePath));
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleClearInstanceParameterOverride(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath, ParameterName;
	
	if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("instance_path is required"));
	}
	if (!Params->TryGetStringField(TEXT("parameter_name"), ParameterName))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("parameter_name is required"));
	}

	auto Result = Service->ClearInstanceParameterOverride(InstancePath, ParameterName);
	if (!Result.IsSuccess())
	{
		return CreateErrorResponse(TEXT("CLEAR_OVERRIDE_FAILED"), Result.GetErrorMessage());
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("instance_path"), InstancePath);
	Response->SetStringField(TEXT("parameter_name"), ParameterName);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Cleared parameter override: %s"), *ParameterName));
	return Response;
}

TSharedPtr<FJsonObject> FMaterialCommands::HandleSaveInstance(const TSharedPtr<FJsonObject>& Params)
{
	FString InstancePath;
	if (!Params->TryGetStringField(TEXT("instance_path"), InstancePath))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAM"), TEXT("instance_path is required"));
	}

	// Load instance and save its package
	auto LoadResult = Service->LoadMaterialInstance(InstancePath);
	if (!LoadResult.IsSuccess())
	{
		return CreateErrorResponse(TEXT("LOAD_FAILED"), LoadResult.GetErrorMessage());
	}

	UMaterialInstanceConstant* Instance = LoadResult.GetValue();
	UPackage* Package = Instance->GetOutermost();

	FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.Error = GWarn;
	
	FSavePackageResultStruct Result = UPackage::Save(Package, Instance, *PackageFileName, SaveArgs);
	
	if (Result.Result != ESavePackageResult::Success)
	{
		return CreateErrorResponse(TEXT("SAVE_FAILED"), FString::Printf(TEXT("Failed to save instance: %s"), *InstancePath));
	}

	TSharedPtr<FJsonObject> Response = CreateSuccessResponse();
	Response->SetStringField(TEXT("instance_path"), InstancePath);
	Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Saved material instance: %s"), *InstancePath));
	return Response;
}

//-----------------------------------------------------------------------------
// Help Action
//-----------------------------------------------------------------------------

TSharedPtr<FJsonObject> FMaterialCommands::HandleHelp(const TSharedPtr<FJsonObject>& Params)
{
	// Use file-based help system
	return FHelpFileReader::HandleHelp(TEXT("manage_material"), Params);
}