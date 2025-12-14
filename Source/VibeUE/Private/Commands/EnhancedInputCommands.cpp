// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Commands/EnhancedInputCommands.h"
#include "Services/EnhancedInput/EnhancedInputReflectionService.h"
#include "Services/EnhancedInput/InputActionService.h"
#include "Services/EnhancedInput/InputMappingService.h"
#include "Core/ErrorCodes.h"
#include "Dom/JsonObject.h"

FEnhancedInputCommands::FEnhancedInputCommands()
{
	// Create a service context for Enhanced Input services
	TSharedPtr<FServiceContext> LocalServiceContext = MakeShared<FServiceContext>();

	// Initialize Enhanced Input services with local service context
	ReflectionService = MakeShared<FEnhancedInputReflectionService>(LocalServiceContext);
	ActionService = MakeShared<FInputActionService>(LocalServiceContext);
	MappingService = MakeShared<FInputMappingService>(LocalServiceContext);

	UE_LOG(LogTemp, Display, TEXT("EnhancedInputCommands: Initialized with 3 core services"));
}

FEnhancedInputCommands::~FEnhancedInputCommands()
{
	// Services cleanup
	ReflectionService.Reset();
	ActionService.Reset();
	MappingService.Reset();

	UE_LOG(LogTemp, Display, TEXT("EnhancedInputCommands: Cleaned up"));
}

TSharedPtr<FJsonObject> FEnhancedInputCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType != TEXT("manage_enhanced_input"))
	{
		UE_LOG(LogTemp, Warning, TEXT("EnhancedInputCommands: Invalid command type: %s"), *CommandType);
		return CreateErrorResponse(VibeUE::ErrorCodes::UNKNOWN_COMMAND, FString::Printf(TEXT("Expected 'manage_enhanced_input', got '%s'"), *CommandType));
	}

	if (!Params.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("EnhancedInputCommands: Null parameters"));
		return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Parameters object is null"));
	}

	// Extract action and service from params
	FString Action = Params->GetStringField(TEXT("action"));
	FString Service = Params->GetStringField(TEXT("service"));

	if (Action.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("EnhancedInputCommands: Missing 'action' parameter"));
		return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing 'action' parameter"));
	}

	if (Service.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("EnhancedInputCommands: Missing 'service' parameter"));
		return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("Missing 'service' parameter"));
	}

	// Normalize to lowercase
	Action = Action.ToLower();
	Service = Service.ToLower();

	UE_LOG(LogTemp, Display, TEXT("EnhancedInputCommands: Routing action='%s' service='%s'"), *Action, *Service);

	// Route to appropriate service handler
	return RouteByService(Service, Action, Params);
}

TSharedPtr<FJsonObject> FEnhancedInputCommands::RouteByService(const FString& Service, const FString& Action, const TSharedPtr<FJsonObject>& Params)
{
	if (Service == TEXT("reflection"))
	{
		return HandleReflectionService(Action, Params);
	}
	else if (Service == TEXT("action"))
	{
		return HandleActionService(Action, Params);
	}
	else if (Service == TEXT("mapping"))
	{
		return HandleMappingService(Action, Params);
	}
	else if (Service == TEXT("modifier"))
	{
		return HandleModifierService(Action, Params);
	}
	else if (Service == TEXT("trigger"))
	{
		return HandleTriggerService(Action, Params);
	}
	else if (Service == TEXT("ai"))
	{
		return HandleAIService(Action, Params);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("EnhancedInputCommands: Unknown service: %s"), *Service);
		return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_INVALID, FString::Printf(TEXT("Unknown service: %s"), *Service));
	}
}

TSharedPtr<FJsonObject> FEnhancedInputCommands::HandleReflectionService(const FString& Action, const TSharedPtr<FJsonObject>& Params)
{
	if (!ReflectionService.IsValid())
	{
		return CreateErrorResponse(VibeUE::ErrorCodes::INTERNAL_ERROR, TEXT("Reflection service not initialized"));
	}

	if (Action == TEXT("reflection_discover_types"))
	{
		// Discover all available Enhanced Input types
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("action"), Action);
		Response->SetStringField(TEXT("service"), TEXT("reflection"));
		
		// Discover different type categories
		TArray<TSharedPtr<FJsonValue>> ActionTypesArray;
		TArray<TSharedPtr<FJsonValue>> ModifierTypesArray;
		TArray<TSharedPtr<FJsonValue>> TriggerTypesArray;
		
		// Input Action types
		auto ActionTypesResult = ReflectionService->DiscoverInputActionTypes();
		if (ActionTypesResult.IsSuccess())
		{
			for (const auto& TypeInfo : ActionTypesResult.GetValue())
			{
				TSharedPtr<FJsonObject> TypeObj = MakeShared<FJsonObject>();
				TypeObj->SetStringField(TEXT("name"), TypeInfo.DisplayName);
				TypeObj->SetStringField(TEXT("path"), TypeInfo.ClassPath);
				ActionTypesArray.Add(MakeShared<FJsonValueObject>(TypeObj));
			}
		}
		
		// Modifier types
		auto ModifierTypesResult = ReflectionService->DiscoverModifierTypes();
		if (ModifierTypesResult.IsSuccess())
		{
			for (const auto& ModifierInfo : ModifierTypesResult.GetValue())
			{
				TSharedPtr<FJsonObject> ModTypeObj = MakeShared<FJsonObject>();
				ModTypeObj->SetStringField(TEXT("name"), ModifierInfo.DisplayName);
				ModTypeObj->SetStringField(TEXT("category"), ModifierInfo.Category);
				ModifierTypesArray.Add(MakeShared<FJsonValueObject>(ModTypeObj));
			}
		}
		
		// Trigger types
		auto TriggerTypesResult = ReflectionService->DiscoverTriggerTypes();
		if (TriggerTypesResult.IsSuccess())
		{
			for (const auto& TriggerInfo : TriggerTypesResult.GetValue())
			{
				TSharedPtr<FJsonObject> TrigTypeObj = MakeShared<FJsonObject>();
				TrigTypeObj->SetStringField(TEXT("name"), TriggerInfo.DisplayName);
				TrigTypeObj->SetStringField(TEXT("category"), TriggerInfo.Category);
				TriggerTypesArray.Add(MakeShared<FJsonValueObject>(TrigTypeObj));
			}
		}
		
		Response->SetArrayField(TEXT("action_types"), ActionTypesArray);
		Response->SetArrayField(TEXT("modifier_types"), ModifierTypesArray);
		Response->SetArrayField(TEXT("trigger_types"), TriggerTypesArray);
		
		return Response;
	}
	else if (Action == TEXT("reflection_get_metadata"))
	{
		FString InputType = Params->GetStringField(TEXT("input_type"));
		if (InputType.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("input_type parameter required"));
		}

		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("action"), Action);
		Response->SetStringField(TEXT("service"), TEXT("reflection"));
		Response->SetStringField(TEXT("input_type"), InputType);
		
		// Get metadata would be implemented here by reflection service
		TSharedPtr<FJsonObject> MetadataObj = MakeShared<FJsonObject>();
		MetadataObj->SetStringField(TEXT("name"), InputType);
		MetadataObj->SetStringField(TEXT("category"), TEXT("enhanced_input"));
		Response->SetObjectField(TEXT("metadata"), MetadataObj);
		
		return Response;
	}
	else
	{
		return CreateErrorResponse(VibeUE::ErrorCodes::ACTION_UNSUPPORTED, FString::Printf(TEXT("Unknown reflection action: %s"), *Action));
	}
}

TSharedPtr<FJsonObject> FEnhancedInputCommands::HandleActionService(const FString& Action, const TSharedPtr<FJsonObject>& Params)
{
	if (!ActionService.IsValid())
	{
		return CreateErrorResponse(VibeUE::ErrorCodes::INTERNAL_ERROR, TEXT("Action service not initialized"));
	}

	if (Action == TEXT("action_create"))
	{
		FString ActionName = Params->GetStringField(TEXT("action_name"));
		FString AssetPath = Params->GetStringField(TEXT("asset_path"));
		FString ValueTypeStr = Params->GetStringField(TEXT("value_type"));

		if (ActionName.IsEmpty() || AssetPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("action_name and asset_path required"));
		}

		// Default to Axis1D, allow customization via parameters
		EInputActionValueType ValueType = EInputActionValueType::Axis1D;

		auto Result = ActionService->CreateInputAction(ActionName, AssetPath, ValueType);
		
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		if (Result.IsSuccess())
		{
			// Build the actual asset path that can be used in subsequent operations
			// Format: /Game/Path/ActionName.ActionName (package path with asset name)
			FString BasePackagePath = AssetPath;
			if (!BasePackagePath.StartsWith(TEXT("/Game")))
			{
				BasePackagePath = TEXT("/Game/") + BasePackagePath;
			}
			if (BasePackagePath.EndsWith(TEXT("/")))
			{
				BasePackagePath = BasePackagePath.LeftChop(1);
			}
			FString FullAssetPath = BasePackagePath / ActionName + TEXT(".") + ActionName;
			
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("action"));
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Input action '%s' created successfully"), *ActionName));
			Response->SetStringField(TEXT("asset_path"), FullAssetPath);
			Response->SetStringField(TEXT("usage_hint"), TEXT("Use this asset_path for mapping_add_key_mapping action_path parameter"));
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
		
		return Response;
	}
	else if (Action == TEXT("action_list"))
	{
		auto Result = ActionService->FindAllInputActions();
		
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		if (Result.IsSuccess())
		{
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("action"));
			
			// Serialize action paths
			TArray<TSharedPtr<FJsonValue>> ActionsArray;
			for (const FString& ActionPath : Result.GetValue())
			{
				ActionsArray.Add(MakeShared<FJsonValueString>(ActionPath));
			}
			Response->SetArrayField(TEXT("actions"), ActionsArray);
			Response->SetNumberField(TEXT("count"), ActionsArray.Num());
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
		
		return Response;
	}
	else if (Action == TEXT("action_get_properties"))
	{
		FString ActionPath = Params->GetStringField(TEXT("action_path"));

		if (ActionPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("action_path required"));
		}

		auto Result = ActionService->GetActionProperties(ActionPath);
		
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		if (Result.IsSuccess())
		{
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("action"));
			
			// Serialize properties
			TArray<TSharedPtr<FJsonValue>> PropertiesArray;
			for (const auto& PropInfo : Result.GetValue())
			{
				TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
				PropObj->SetStringField(TEXT("name"), PropInfo.Name);
				PropObj->SetStringField(TEXT("type"), PropInfo.TypeName);
				PropertiesArray.Add(MakeShared<FJsonValueObject>(PropObj));
			}
			Response->SetArrayField(TEXT("properties"), PropertiesArray);
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
		
		return Response;
	}
	else if (Action == TEXT("action_configure"))
	{
		FString ActionPath = Params->GetStringField(TEXT("action_path"));
		FString PropertyName = Params->GetStringField(TEXT("property_name"));
		FString PropertyValue = Params->GetStringField(TEXT("property_value"));

		if (ActionPath.IsEmpty() || PropertyName.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("action_path and property_name required"));
		}

		auto Result = ActionService->SetActionProperty(ActionPath, PropertyName, PropertyValue);
		
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		if (Result.IsSuccess())
		{
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("action"));
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Property '%s' configured"), *PropertyName));
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
		
		return Response;
	}
	else
	{
		return CreateErrorResponse(VibeUE::ErrorCodes::ACTION_UNSUPPORTED, FString::Printf(TEXT("Unknown action service action: %s"), *Action));
	}
}

TSharedPtr<FJsonObject> FEnhancedInputCommands::HandleMappingService(const FString& Action, const TSharedPtr<FJsonObject>& Params)
{
	if (!MappingService.IsValid())
	{
		return CreateErrorResponse(VibeUE::ErrorCodes::INTERNAL_ERROR, TEXT("Mapping service not initialized"));
	}

	if (Action == TEXT("mapping_create_context"))
	{
		FString ContextName = Params->GetStringField(TEXT("context_name"));
		// Accept both context_path (primary, consistent with other mapping_* ops) and asset_path (legacy)
		FString AssetPath = Params->GetStringField(TEXT("context_path"));
		if (AssetPath.IsEmpty())
		{
			AssetPath = Params->GetStringField(TEXT("asset_path"));
		}
		int32 Priority = static_cast<int32>(Params->GetNumberField(TEXT("priority")));

		if (ContextName.IsEmpty() || AssetPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_name and context_path required"));
		}

		auto Result = MappingService->CreateMappingContext(ContextName, AssetPath, Priority);
		
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		if (Result.IsSuccess())
		{
			// Build the actual asset path that can be used in subsequent operations
			FString BasePackagePath = AssetPath;
			if (!BasePackagePath.StartsWith(TEXT("/Game")))
			{
				BasePackagePath = TEXT("/Game/") + BasePackagePath;
			}
			if (BasePackagePath.EndsWith(TEXT("/")))
			{
				BasePackagePath = BasePackagePath.LeftChop(1);
			}
			FString FullAssetPath = BasePackagePath / ContextName + TEXT(".") + ContextName;
			
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Mapping context '%s' created"), *ContextName));
			Response->SetStringField(TEXT("context_path"), FullAssetPath);
			Response->SetNumberField(TEXT("priority"), Priority);
			Response->SetStringField(TEXT("usage_hint"), TEXT("Use this context_path for mapping operations like mapping_add_key_mapping"));
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
		
		return Response;
	}
	else if (Action == TEXT("mapping_list_contexts"))
	{
		auto Result = MappingService->FindAllMappingContexts();
		
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		if (Result.IsSuccess())
		{
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			
			TArray<TSharedPtr<FJsonValue>> ContextsArray;
			for (const FString& ContextPath : Result.GetValue())
			{
				ContextsArray.Add(MakeShared<FJsonValueString>(ContextPath));
			}
			Response->SetArrayField(TEXT("contexts"), ContextsArray);
			Response->SetNumberField(TEXT("count"), ContextsArray.Num());
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
		
		return Response;
	}
	else if (Action == TEXT("mapping_add_key_mapping"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));
		FString ActionPath = Params->GetStringField(TEXT("action_path"));
		FString KeyName = Params->GetStringField(TEXT("key"));

		if (ContextPath.IsEmpty() || ActionPath.IsEmpty() || KeyName.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path, action_path, and key required"));
		}

		auto Result = MappingService->AddInputMapping(ContextPath, ActionPath, KeyName);
		
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		if (Result.IsSuccess())
		{
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Key mapping for '%s' added"), *KeyName));
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
		
		return Response;
	}
	else if (Action == TEXT("mapping_update_context"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));
		FString PropertyName = Params->GetStringField(TEXT("property_name"));
		FString PropertyValue = Params->GetStringField(TEXT("property_value"));

		if (ContextPath.IsEmpty() || PropertyName.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path and property_name required"));
		}

		auto Result = MappingService->SetContextProperty(ContextPath, PropertyName, PropertyValue);
		
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		if (Result.IsSuccess())
		{
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Property '%s' updated for context"), *PropertyName));
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
		
		return Response;
	}
	else if (Action == TEXT("mapping_get_properties"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));

		if (ContextPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path required"));
		}

		// Load the mapping context
		UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
		if (!MappingContext)
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
				FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
		}

		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("action"), Action);
		Response->SetStringField(TEXT("service"), TEXT("mapping"));
		Response->SetStringField(TEXT("context_path"), ContextPath);

		// Get all properties using reflection
		TArray<TSharedPtr<FJsonValue>> PropertiesArray;
		for (TFieldIterator<FProperty> PropIt(MappingContext->GetClass()); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property) continue;

			TSharedPtr<FJsonObject> PropObj = MakeShared<FJsonObject>();
			PropObj->SetStringField(TEXT("name"), Property->GetName());
			PropObj->SetStringField(TEXT("type"), Property->GetCPPType());
			PropObj->SetStringField(TEXT("display_name"), Property->GetDisplayNameText().ToString());

			// Get current value
			void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(MappingContext);
			FString ValueStr;
			
			if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
			{
				ValueStr = StrProp->GetPropertyValue(PropertyAddress);
			}
			else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
			{
				ValueStr = FString::FromInt(IntProp->GetPropertyValue(PropertyAddress));
			}
			else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
			{
				ValueStr = FString::SanitizeFloat(FloatProp->GetPropertyValue(PropertyAddress));
			}
			else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
			{
				ValueStr = BoolProp->GetPropertyValue(PropertyAddress) ? TEXT("true") : TEXT("false");
			}
			else if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
			{
				ValueStr = TextProp->GetPropertyValue(PropertyAddress).ToString();
			}
			else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
			{
				UEnum* Enum = EnumProp->GetEnum();
				if (Enum)
				{
					FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
					int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(PropertyAddress);
					ValueStr = Enum->GetNameStringByValue(EnumValue);
					
					// Also add valid values list
					TArray<TSharedPtr<FJsonValue>> ValidValues;
					for (int32 i = 0; i < Enum->NumEnums() - 1; ++i)
					{
						ValidValues.Add(MakeShared<FJsonValueString>(Enum->GetNameStringByIndex(i)));
					}
					PropObj->SetArrayField(TEXT("valid_values"), ValidValues);
				}
				else
				{
					ValueStr = TEXT("<unknown enum>");
				}
			}
			else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
			{
				if (ByteProp->Enum)
				{
					uint8 ByteValue = ByteProp->GetPropertyValue(PropertyAddress);
					ValueStr = ByteProp->Enum->GetNameStringByValue(ByteValue);
					
					// Also add valid values list
					TArray<TSharedPtr<FJsonValue>> ValidValues;
					for (int32 i = 0; i < ByteProp->Enum->NumEnums() - 1; ++i)
					{
						ValidValues.Add(MakeShared<FJsonValueString>(ByteProp->Enum->GetNameStringByIndex(i)));
					}
					PropObj->SetArrayField(TEXT("valid_values"), ValidValues);
				}
				else
				{
					ValueStr = FString::FromInt(ByteProp->GetPropertyValue(PropertyAddress));
				}
			}
			else
			{
				ValueStr = TEXT("<complex type>");
			}

			PropObj->SetStringField(TEXT("current_value"), ValueStr);
			PropertiesArray.Add(MakeShared<FJsonValueObject>(PropObj));
		}

		Response->SetArrayField(TEXT("properties"), PropertiesArray);
		Response->SetNumberField(TEXT("count"), PropertiesArray.Num());
		
		return Response;
	}
	else if (Action == TEXT("mapping_get_mappings"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));

		if (ContextPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path required"));
		}

		auto Result = MappingService->GetContextMappings(ContextPath);
		
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		if (Result.IsSuccess())
		{
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			
			TArray<TSharedPtr<FJsonValue>> MappingsArray;
			for (const auto& MappingInfo : Result.GetValue())
			{
				TSharedPtr<FJsonObject> MappingObj = MakeShared<FJsonObject>();
				MappingObj->SetStringField(TEXT("name"), MappingInfo.Name);
				MappingObj->SetStringField(TEXT("action"), MappingInfo.DisplayName);  // Action name
				MappingObj->SetStringField(TEXT("key"), MappingInfo.TypePath);        // Key display name
				MappingObj->SetStringField(TEXT("detail"), MappingInfo.DefaultValue); // Full "Key -> Action" string
				MappingsArray.Add(MakeShared<FJsonValueObject>(MappingObj));
			}
			Response->SetArrayField(TEXT("mappings"), MappingsArray);
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
		
		return Response;
	}
	else if (Action == TEXT("mapping_remove_mapping"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));
		int32 MappingIndex = Params->GetIntegerField(TEXT("mapping_index"));

		if (ContextPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path required"));
		}

		auto Result = MappingService->RemoveInputMapping(ContextPath, MappingIndex);
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			Response->SetStringField(TEXT("message"), TEXT("Input mapping removed successfully"));
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	else if (Action == TEXT("mapping_get_property"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));
		FString PropertyName = Params->GetStringField(TEXT("property_name"));

		if (ContextPath.IsEmpty() || PropertyName.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path and property_name required"));
		}

		auto Result = MappingService->GetContextProperty(ContextPath, PropertyName);
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			Response->SetStringField(TEXT("property_name"), PropertyName);
			Response->SetStringField(TEXT("value"), Result.GetValue());
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	else if (Action == TEXT("mapping_validate_context"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));

		if (ContextPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path required"));
		}

		auto Result = MappingService->ValidateContextConfiguration(ContextPath);
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			Response->SetBoolField(TEXT("is_valid"), true);
			Response->SetStringField(TEXT("message"), TEXT("Context configuration is valid"));
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	// NOTE: mapping_duplicate_context removed - use manage_asset(action="duplicate") instead
	else if (Action == TEXT("mapping_get_available_keys"))
	{
		FString Filter = Params->GetStringField(TEXT("filter"));
		
		auto Result = MappingService->GetAvailableInputKeys(Filter);
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			
			TArray<TSharedPtr<FJsonValue>> KeysArray;
			for (const FString& Key : Result.GetValue())
			{
				KeysArray.Add(MakeShared<FJsonValueString>(Key));
			}
			Response->SetArrayField(TEXT("available_keys"), KeysArray);
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	else if (Action == TEXT("mapping_analyze_usage"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));

		if (ContextPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path required"));
		}

		auto Result = MappingService->AnalyzeContextUsage(ContextPath);
		
		if (Result.IsSuccess())
		{
			auto Usage = Result.GetValue();
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			Response->SetNumberField(TEXT("total_mappings"), Usage.TotalMappings);
			Response->SetNumberField(TEXT("unique_actions"), Usage.UniqueActions);
			Response->SetNumberField(TEXT("unique_keys"), Usage.UniqueKeys);
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	else if (Action == TEXT("mapping_detect_conflicts"))
	{
		// For single context, create array with one element
		FString ContextPath = Params->GetStringField(TEXT("context_path"));
		TArray<FString> ContextPaths;
		
		if (!ContextPath.IsEmpty())
		{
			ContextPaths.Add(ContextPath);
		}
		else
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path required"));
		}

		auto Result = MappingService->DetectKeyConflicts(ContextPaths);
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			
			TArray<TSharedPtr<FJsonValue>> ConflictsArray;
			for (const auto& Conflict : Result.GetValue())
			{
				TSharedPtr<FJsonObject> ConflictObj = MakeShared<FJsonObject>();
				ConflictObj->SetStringField(TEXT("key"), Conflict.Key);
				
				TArray<TSharedPtr<FJsonValue>> ActionsArray;
				for (const FString& ActionName : Conflict.ConflictingActions)
				{
					ActionsArray.Add(MakeShared<FJsonValueString>(ActionName));
				}
				ConflictObj->SetArrayField(TEXT("conflicting_actions"), ActionsArray);
				ConflictsArray.Add(MakeShared<FJsonValueObject>(ConflictObj));
			}
			Response->SetArrayField(TEXT("conflicts"), ConflictsArray);
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	// ═══════════════════════════════════════════════════════════════════
	// Modifier Management - Add/Remove/List modifiers on mappings
	// ═══════════════════════════════════════════════════════════════════
	else if (Action == TEXT("mapping_add_modifier"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));
		int32 MappingIndex = Params->GetIntegerField(TEXT("mapping_index"));
		FString ModifierType = Params->GetStringField(TEXT("modifier_type"));

		if (ContextPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path required"));
		}
		if (ModifierType.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("modifier_type required"));
		}

		// Create the modifier instance
		auto CreateResult = MappingService->CreateModifier(ModifierType);
		if (!CreateResult.IsSuccess())
		{
			return CreateErrorResponse(CreateResult.GetErrorCode(), CreateResult.GetErrorMessage());
		}

		// Add to mapping
		auto Result = MappingService->AddModifierToMapping(ContextPath, MappingIndex, CreateResult.GetValue());
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Modifier '%s' added to mapping %d"), *ModifierType, MappingIndex));
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	else if (Action == TEXT("mapping_remove_modifier"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));
		int32 MappingIndex = Params->GetIntegerField(TEXT("mapping_index"));
		int32 ModifierIndex = Params->GetIntegerField(TEXT("modifier_index"));

		if (ContextPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path required"));
		}

		auto Result = MappingService->RemoveModifierFromMapping(ContextPath, MappingIndex, ModifierIndex);
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Modifier at index %d removed from mapping %d"), ModifierIndex, MappingIndex));
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	else if (Action == TEXT("mapping_get_modifiers"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));
		int32 MappingIndex = Params->GetIntegerField(TEXT("mapping_index"));

		if (ContextPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path required"));
		}

		auto Result = MappingService->GetMappingModifiers(ContextPath, MappingIndex);
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			
			TArray<TSharedPtr<FJsonValue>> ModifiersArray;
			for (const auto& ModInfo : Result.GetValue())
			{
				TSharedPtr<FJsonObject> ModObj = MakeShared<FJsonObject>();
				ModObj->SetNumberField(TEXT("index"), ModInfo.Index);
				ModObj->SetStringField(TEXT("type_name"), ModInfo.TypeName);
				ModObj->SetStringField(TEXT("display_name"), ModInfo.DisplayName);
				
				TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
				for (const auto& Prop : ModInfo.Properties)
				{
					PropsObj->SetStringField(Prop.Key, Prop.Value);
				}
				ModObj->SetObjectField(TEXT("properties"), PropsObj);
				
				ModifiersArray.Add(MakeShared<FJsonValueObject>(ModObj));
			}
			Response->SetArrayField(TEXT("modifiers"), ModifiersArray);
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	else if (Action == TEXT("mapping_get_available_modifier_types"))
	{
		auto Result = MappingService->GetAvailableModifierTypes();
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			
			TArray<TSharedPtr<FJsonValue>> TypesArray;
			for (const FString& Type : Result.GetValue())
			{
				TypesArray.Add(MakeShared<FJsonValueString>(Type));
			}
			Response->SetArrayField(TEXT("modifier_types"), TypesArray);
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	// ═══════════════════════════════════════════════════════════════════
	// Trigger Management - Add/Remove/List triggers on mappings
	// ═══════════════════════════════════════════════════════════════════
	else if (Action == TEXT("mapping_add_trigger"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));
		int32 MappingIndex = Params->GetIntegerField(TEXT("mapping_index"));
		FString TriggerType = Params->GetStringField(TEXT("trigger_type"));

		if (ContextPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path required"));
		}
		if (TriggerType.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("trigger_type required"));
		}

		// Create the trigger instance
		auto CreateResult = MappingService->CreateTrigger(TriggerType);
		if (!CreateResult.IsSuccess())
		{
			return CreateErrorResponse(CreateResult.GetErrorCode(), CreateResult.GetErrorMessage());
		}

		// Add to mapping
		auto Result = MappingService->AddTriggerToMapping(ContextPath, MappingIndex, CreateResult.GetValue());
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Trigger '%s' added to mapping %d"), *TriggerType, MappingIndex));
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	else if (Action == TEXT("mapping_remove_trigger"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));
		int32 MappingIndex = Params->GetIntegerField(TEXT("mapping_index"));
		int32 TriggerIndex = Params->GetIntegerField(TEXT("trigger_index"));

		if (ContextPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path required"));
		}

		auto Result = MappingService->RemoveTriggerFromMapping(ContextPath, MappingIndex, TriggerIndex);
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Trigger at index %d removed from mapping %d"), TriggerIndex, MappingIndex));
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	else if (Action == TEXT("mapping_get_triggers"))
	{
		FString ContextPath = Params->GetStringField(TEXT("context_path"));
		int32 MappingIndex = Params->GetIntegerField(TEXT("mapping_index"));

		if (ContextPath.IsEmpty())
		{
			return CreateErrorResponse(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("context_path required"));
		}

		auto Result = MappingService->GetMappingTriggers(ContextPath, MappingIndex);
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			
			TArray<TSharedPtr<FJsonValue>> TriggersArray;
			for (const auto& TrigInfo : Result.GetValue())
			{
				TSharedPtr<FJsonObject> TrigObj = MakeShared<FJsonObject>();
				TrigObj->SetNumberField(TEXT("index"), TrigInfo.Index);
				TrigObj->SetStringField(TEXT("type_name"), TrigInfo.TypeName);
				TrigObj->SetStringField(TEXT("display_name"), TrigInfo.DisplayName);
				
				TSharedPtr<FJsonObject> PropsObj = MakeShared<FJsonObject>();
				for (const auto& Prop : TrigInfo.Properties)
				{
					PropsObj->SetStringField(Prop.Key, Prop.Value);
				}
				TrigObj->SetObjectField(TEXT("properties"), PropsObj);
				
				TriggersArray.Add(MakeShared<FJsonValueObject>(TrigObj));
			}
			Response->SetArrayField(TEXT("triggers"), TriggersArray);
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	else if (Action == TEXT("mapping_get_available_trigger_types"))
	{
		auto Result = MappingService->GetAvailableTriggerTypes();
		
		if (Result.IsSuccess())
		{
			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("action"), Action);
			Response->SetStringField(TEXT("service"), TEXT("mapping"));
			
			TArray<TSharedPtr<FJsonValue>> TypesArray;
			for (const FString& Type : Result.GetValue())
			{
				TypesArray.Add(MakeShared<FJsonValueString>(Type));
			}
			Response->SetArrayField(TEXT("trigger_types"), TypesArray);
			return Response;
		}
		else
		{
			return CreateErrorResponse(Result.GetErrorCode(), Result.GetErrorMessage());
		}
	}
	else
	{
		return CreateErrorResponse(VibeUE::ErrorCodes::ACTION_UNSUPPORTED, FString::Printf(TEXT("Unknown mapping action: %s"), *Action));
	}
}

TSharedPtr<FJsonObject> FEnhancedInputCommands::HandleModifierService(const FString& Action, const TSharedPtr<FJsonObject>& Params)
{
	// Modifier management has been consolidated into mapping service
	// Use mapping_add_modifier, mapping_remove_modifier, mapping_get_modifiers actions
	return CreateErrorResponse(VibeUE::ErrorCodes::ACTION_UNSUPPORTED, 
		FString::Printf(TEXT("Modifier service deprecated. Use mapping service actions: mapping_add_modifier, mapping_remove_modifier, mapping_get_modifiers, mapping_get_available_modifier_types")));
}

TSharedPtr<FJsonObject> FEnhancedInputCommands::HandleTriggerService(const FString& Action, const TSharedPtr<FJsonObject>& Params)
{
	// Trigger management has been consolidated into mapping service
	// Use mapping_add_trigger, mapping_remove_trigger, mapping_get_triggers actions
	return CreateErrorResponse(VibeUE::ErrorCodes::ACTION_UNSUPPORTED, 
		FString::Printf(TEXT("Trigger service deprecated. Use mapping service actions: mapping_add_trigger, mapping_remove_trigger, mapping_get_triggers, mapping_get_available_trigger_types")));
}

TSharedPtr<FJsonObject> FEnhancedInputCommands::HandleAIService(const FString& Action, const TSharedPtr<FJsonObject>& Params)
{
	// AI configuration service has been deprecated
	// Enhanced Input setup should be done through action and mapping services directly
	return CreateErrorResponse(VibeUE::ErrorCodes::ACTION_UNSUPPORTED, 
		TEXT("AI service deprecated. Use action and mapping services directly for Enhanced Input setup."));
}

TSharedPtr<FJsonObject> FEnhancedInputCommands::CreateErrorResponse(const FString& ErrorCode, const FString& ErrorMessage)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error_code"), ErrorCode);
	Response->SetStringField(TEXT("error"), ErrorMessage);
	return Response;
}

TSharedPtr<FJsonObject> FEnhancedInputCommands::CreateSuccessResponse(const TSharedPtr<FJsonObject>& Data)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	
	// Just return the data as-is if it exists
	if (Data.IsValid())
	{
		return Data;
	}
	
	return Response;
}

