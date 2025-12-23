// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Commands/LevelActorCommands.h"
#include "Services/LevelActor/LevelActorService.h"
#include "Services/LevelActor/Types/LevelActorTypes.h"
#include "Utils/HelpFileReader.h"
#include "Utils/ParamValidation.h"
#include "Core/JsonValueHelper.h"

// ============================================================================
// Level Actor Parameter Sets
// ============================================================================

namespace LevelActorParams
{
	// Actor identifier params - at least one required for most actions
	static const TArray<FString> ActorIdentifierParams = {
		TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag")
	};
	
	// Check if any identifier is present
	static bool HasActorIdentifier(const TSharedPtr<FJsonObject>& Params)
	{
		return ParamValidation::HasAnyStringParam(Params, ActorIdentifierParams);
	}
	
	// Reuse ParamValidation helpers
	static bool HasAnyParam(const TSharedPtr<FJsonObject>& Params, const TArray<FString>& ParamNames)
	{
		return ParamValidation::HasAnyParam(Params, ParamNames);
	}
	
	// Build error message listing valid params
	static FString BuildMissingParamsError(const FString& Description, const TArray<FString>& ValidParams)
	{
		return ParamValidation::BuildError(Description, ValidParams);
	}
}

FLevelActorCommands::FLevelActorCommands()
{
	Service = MakeShared<FLevelActorService>();
	UE_LOG(LogTemp, Display, TEXT("LevelActorCommands: Initialized"));
}

FLevelActorCommands::~FLevelActorCommands()
{
	Service.Reset();
	UE_LOG(LogTemp, Display, TEXT("LevelActorCommands: Destroyed"));
}

TSharedPtr<FJsonObject> FLevelActorCommands::CreateErrorResponse(const FString& Code, const FString& Message)
{
	TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error_code"), Code);
	Response->SetStringField(TEXT("error"), Message);
	return Response;
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType != TEXT("manage_level_actors"))
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
	UE_LOG(LogTemp, Display, TEXT("LevelActorCommands: Handling action '%s'"), *Action);

	// Handle help action
	if (Action == TEXT("help"))
	{
		return HandleHelp(Params);
	}

	// Phase 1: Basic actor operations
	if (Action == TEXT("add"))
	{
		return HandleAdd(Params);
	}
	else if (Action == TEXT("remove"))
	{
		return HandleRemove(Params);
	}
	else if (Action == TEXT("list"))
	{
		return HandleList(Params);
	}
	else if (Action == TEXT("find"))
	{
		return HandleFind(Params);
	}
	else if (Action == TEXT("get_info"))
	{
		return HandleGetInfo(Params);
	}
	// Phase 2: Transform operations
	else if (Action == TEXT("set_transform"))
	{
		return HandleSetTransform(Params);
	}
	else if (Action == TEXT("get_transform"))
	{
		return HandleGetTransform(Params);
	}
	else if (Action == TEXT("set_location"))
	{
		return HandleSetLocation(Params);
	}
	else if (Action == TEXT("set_rotation"))
	{
		return HandleSetRotation(Params);
	}
	else if (Action == TEXT("set_scale"))
	{
		return HandleSetScale(Params);
	}
	// Editor view operations
	else if (Action == TEXT("focus"))
	{
		return HandleFocus(Params);
	}
	else if (Action == TEXT("move_to_view"))
	{
		return HandleMoveToView(Params);
	}
	else if (Action == TEXT("refresh_viewport"))
	{
		return HandleRefreshViewport(Params);
	}
	// Phase 3: Property operations
	else if (Action == TEXT("get_property"))
	{
		return HandleGetProperty(Params);
	}
	else if (Action == TEXT("set_property"))
	{
		return HandleSetProperty(Params);
	}
	else if (Action == TEXT("get_all_properties"))
	{
		return HandleGetAllProperties(Params);
	}
	// Phase 4: Hierarchy & Organization
	else if (Action == TEXT("set_folder") || Action == TEXT("create_folder"))
	{
		return HandleSetFolder(Params);
	}
	else if (Action == TEXT("attach"))
	{
		return HandleAttach(Params);
	}
	else if (Action == TEXT("detach"))
	{
		return HandleDetach(Params);
	}
	else if (Action == TEXT("select"))
	{
		return HandleSelect(Params);
	}
	else if (Action == TEXT("rename"))
	{
		return HandleRename(Params);
	}
	else
	{
		return CreateErrorResponse(TEXT("UNKNOWN_ACTION"), 
			FString::Printf(TEXT("Unknown action: %s"), *Action));
	}
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleAdd(const TSharedPtr<FJsonObject>& Params)
{
	// Validate required param: actor_class
	FString ActorClass;
	if (!Params->TryGetStringField(TEXT("actor_class"), ActorClass) || ActorClass.IsEmpty())
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_class"), TEXT("actor_name"), TEXT("actor_label"), 
			TEXT("location"), TEXT("rotation"), TEXT("scale"), TEXT("tags")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("actor_class is required"), ValidParams));
	}
	
	FActorAddParams AddParams = FActorAddParams::FromJson(Params);
	FActorOperationResult Result = Service->AddActor(AddParams);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleRemove(const TSharedPtr<FJsonObject>& Params)
{
	bool bWithUndo = true;
	Params->TryGetBoolField(TEXT("with_undo"), bWithUndo);
	
	// Check for multiple actors first - support both actor_labels and actor_paths
	const TArray<TSharedPtr<FJsonValue>>* ActorArray = nullptr;
	bool bUsingPaths = false;
	
	if (!Params->TryGetArrayField(TEXT("actor_labels"), ActorArray) || !ActorArray || ActorArray->Num() == 0)
	{
		// Try actor_paths as an alias
		if (Params->TryGetArrayField(TEXT("actor_paths"), ActorArray) && ActorArray && ActorArray->Num() > 0)
		{
			bUsingPaths = true;
		}
	}
	
	if (ActorArray && ActorArray->Num() > 0)
	{
		// Batch removal mode
		TSharedPtr<FJsonObject> Response = MakeShareable(new FJsonObject);
		TArray<TSharedPtr<FJsonValue>> RemovedArray;
		TArray<TSharedPtr<FJsonValue>> FailedArray;
		
		for (const TSharedPtr<FJsonValue>& Value : *ActorArray)
		{
			FString Identifier;
			if (Value->TryGetString(Identifier) && !Identifier.IsEmpty())
			{
				FActorIdentifier ActorId;
				if (bUsingPaths)
				{
					ActorId.ActorPath = Identifier;
				}
				else
				{
					ActorId.ActorLabel = Identifier;
				}
				
				FActorOperationResult Result = Service->RemoveActor(ActorId, bWithUndo);
				
				TSharedPtr<FJsonObject> ItemResult = MakeShareable(new FJsonObject);
				ItemResult->SetStringField(bUsingPaths ? TEXT("actor_path") : TEXT("actor_label"), Identifier);
				ItemResult->SetBoolField(TEXT("success"), Result.bSuccess);
				
				if (Result.bSuccess)
				{
					RemovedArray.Add(MakeShareable(new FJsonValueObject(ItemResult)));
				}
				else
				{
					ItemResult->SetStringField(TEXT("error"), Result.ErrorMessage);
					FailedArray.Add(MakeShareable(new FJsonValueObject(ItemResult)));
				}
			}
		}
		
		Response->SetBoolField(TEXT("success"), FailedArray.Num() == 0);
		Response->SetArrayField(TEXT("removed"), RemovedArray);
		if (FailedArray.Num() > 0)
		{
			Response->SetArrayField(TEXT("failed"), FailedArray);
		}
		Response->SetNumberField(TEXT("removed_count"), RemovedArray.Num());
		Response->SetNumberField(TEXT("failed_count"), FailedArray.Num());
		
		return Response;
	}
	
	// Single actor removal (existing behavior)
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	// Validate - need either batch array or single identifier
	if (!Identifier.IsValid())
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("actor_labels"), TEXT("actor_paths"), TEXT("with_undo")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorOperationResult Result = Service->RemoveActor(Identifier, bWithUndo);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleList(const TSharedPtr<FJsonObject>& Params)
{
	FActorQueryCriteria Criteria = FActorQueryCriteria::FromJson(Params);
	FActorOperationResult Result = Service->ListActors(Criteria);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleFind(const TSharedPtr<FJsonObject>& Params)
{
	FActorQueryCriteria Criteria = FActorQueryCriteria::FromJson(Params);
	FActorOperationResult Result = Service->FindActors(Criteria);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleGetInfo(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("include_components"), TEXT("include_properties"), TEXT("category_filter")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	// Default to minimal response - user must opt-in for components/properties
	bool bIncludeComponents = false;
	bool bIncludeProperties = false;
	FString CategoryFilter;
	
	Params->TryGetBoolField(TEXT("include_components"), bIncludeComponents);
	Params->TryGetBoolField(TEXT("include_properties"), bIncludeProperties);
	Params->TryGetStringField(TEXT("category_filter"), CategoryFilter);
	
	FActorOperationResult Result = Service->GetActorInfo(
		Identifier, bIncludeComponents, bIncludeProperties, CategoryFilter);
	return Result.ToJson();
}

// ============================================================================
// Phase 2: Transform Operations
// ============================================================================

TSharedPtr<FJsonObject> FLevelActorCommands::HandleSetTransform(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("location"), TEXT("rotation"), TEXT("scale"), TEXT("world_space"), TEXT("sweep")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorTransformParams TransformParams = FActorTransformParams::FromJson(Params);
	FActorOperationResult Result = Service->SetTransform(TransformParams);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleGetTransform(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	FActorOperationResult Result = Service->GetTransform(Identifier);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleSetLocation(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("location"), TEXT("world_space"), TEXT("sweep")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	// Parse location using helper - handles arrays, objects, and string-encoded JSON
	FVector Location = FVector::ZeroVector;
	const TSharedPtr<FJsonValue>* LocationValue = Params->Values.Find(TEXT("location"));
	if (!LocationValue || !FJsonValueHelper::TryGetVector(*LocationValue, Location))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("location"), TEXT("world_space"), TEXT("sweep")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("location is required as {x, y, z} or [X, Y, Z]"), ValidParams));
	}
	
	bool bWorldSpace = true;
	bool bSweep = false;
	Params->TryGetBoolField(TEXT("world_space"), bWorldSpace);
	Params->TryGetBoolField(TEXT("sweep"), bSweep);
	
	FActorOperationResult Result = Service->SetLocation(Identifier, Location, bWorldSpace, bSweep);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleSetRotation(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("rotation"), TEXT("world_space")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	// Parse rotation using helper - handles arrays, objects, and string-encoded JSON
	FRotator Rotation = FRotator::ZeroRotator;
	const TSharedPtr<FJsonValue>* RotationValue = Params->Values.Find(TEXT("rotation"));
	if (!RotationValue || !FJsonValueHelper::TryGetRotator(*RotationValue, Rotation))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("rotation"), TEXT("world_space")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("rotation is required as {pitch, yaw, roll} or [Pitch, Yaw, Roll]"), ValidParams));
	}
	
	bool bWorldSpace = true;
	Params->TryGetBoolField(TEXT("world_space"), bWorldSpace);
	
	FActorOperationResult Result = Service->SetRotation(Identifier, Rotation, bWorldSpace);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleSetScale(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("scale")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	// Parse scale using helper - handles arrays, objects, and string-encoded JSON
	FVector Scale = FVector::OneVector;
	const TSharedPtr<FJsonValue>* ScaleValue = Params->Values.Find(TEXT("scale"));
	if (!ScaleValue || !FJsonValueHelper::TryGetVector(*ScaleValue, Scale))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("scale")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("scale is required as {x, y, z} or [X, Y, Z]"), ValidParams));
	}
	
	FActorOperationResult Result = Service->SetScale(Identifier, Scale);
	return Result.ToJson();
}

// ============================================================================
// Editor View Operations
// ============================================================================

TSharedPtr<FJsonObject> FLevelActorCommands::HandleFocus(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("instant")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	bool bInstant = false;
	Params->TryGetBoolField(TEXT("instant"), bInstant);
	
	FActorOperationResult Result = Service->FocusActor(Identifier, bInstant);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleMoveToView(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	FActorOperationResult Result = Service->MoveActorToView(Identifier);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleRefreshViewport(const TSharedPtr<FJsonObject>& Params)
{
	FActorOperationResult Result = Service->RefreshViewport();
	return Result.ToJson();
}

// ============================================================================
// Phase 3: Property Operations
// ============================================================================

TSharedPtr<FJsonObject> FLevelActorCommands::HandleGetProperty(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("property_path"), TEXT("component_name")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	// Validate property_path
	if (!LevelActorParams::HasAnyParam(Params, {TEXT("property_path"), TEXT("property_name")}))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("property_path"), TEXT("component_name")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("property_path is required"), ValidParams));
	}
	
	FActorPropertyParams PropertyParams = FActorPropertyParams::FromJson(Params);
	FActorOperationResult Result = Service->GetProperty(PropertyParams);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleSetProperty(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("property_path"), TEXT("property_value"), TEXT("component_name")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	// Validate property_path
	if (!LevelActorParams::HasAnyParam(Params, {TEXT("property_path"), TEXT("property_name")}))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("property_path"), TEXT("property_value"), TEXT("component_name")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("property_path is required"), ValidParams));
	}
	
	// Validate property_value
	if (!Params->HasField(TEXT("property_value")))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("property_path"), TEXT("property_value"), TEXT("component_name")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("property_value is required"), ValidParams));
	}
	
	FActorPropertyParams PropertyParams = FActorPropertyParams::FromJson(Params);
	FActorOperationResult Result = Service->SetProperty(PropertyParams);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleGetAllProperties(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("component_name"), TEXT("category_filter"), TEXT("include_inherited")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorPropertyParams PropertyParams = FActorPropertyParams::FromJson(Params);
	FActorOperationResult Result = Service->GetAllProperties(PropertyParams);
	return Result.ToJson();
}

// ============================================================================
// Phase 4: Hierarchy & Organization
// ============================================================================

TSharedPtr<FJsonObject> FLevelActorCommands::HandleSetFolder(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("folder_path"), TEXT("folder_name"), TEXT("folder")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	FString FolderPath;
	// Accept folder_path, folder_name, or folder as the path
	if (!Params->TryGetStringField(TEXT("folder_path"), FolderPath))
	{
		if (!Params->TryGetStringField(TEXT("folder_name"), FolderPath))
		{
			Params->TryGetStringField(TEXT("folder"), FolderPath);
		}
	}
	
	FActorOperationResult Result = Service->SetFolder(Identifier, FolderPath);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleAttach(const TSharedPtr<FJsonObject>& Params)
{
	// Validate - need both child and parent identifiers
	static const TArray<FString> ValidParams = {
		TEXT("child_label"), TEXT("child_path"), TEXT("child_guid"),
		TEXT("parent_label"), TEXT("parent_path"), TEXT("parent_guid"),
		TEXT("socket_name"), TEXT("weld_simulated_bodies")
	};
	
	// Check for child identifier
	if (!LevelActorParams::HasAnyParam(Params, {TEXT("child_label"), TEXT("child_path"), TEXT("child_guid"), TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid")}))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Child actor identifier required"), ValidParams));
	}
	
	// Check for parent identifier
	if (!LevelActorParams::HasAnyParam(Params, {TEXT("parent_label"), TEXT("parent_path"), TEXT("parent_guid")}))
	{
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Parent actor identifier required"), ValidParams));
	}
	
	FActorAttachParams AttachParams = FActorAttachParams::FromJson(Params);
	FActorOperationResult Result = Service->AttachActor(AttachParams);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleDetach(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	FActorOperationResult Result = Service->DetachActor(Identifier);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleSelect(const TSharedPtr<FJsonObject>& Params)
{
	FActorSelectParams SelectParams = FActorSelectParams::FromJson(Params);
	FActorOperationResult Result = Service->SelectActors(SelectParams);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleRename(const TSharedPtr<FJsonObject>& Params)
{
	// Validate actor identifier
	if (!LevelActorParams::HasActorIdentifier(Params))
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("new_label"), TEXT("new_actor_label"), TEXT("new_name")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("Actor identifier required"), ValidParams));
	}
	
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	// Accept multiple parameter name variations
	FString NewLabel;
	if (!Params->TryGetStringField(TEXT("new_label"), NewLabel))
	{
		if (!Params->TryGetStringField(TEXT("new_actor_label"), NewLabel))
		{
			Params->TryGetStringField(TEXT("new_name"), NewLabel);
		}
	}
	
	// Validate new_label
	if (NewLabel.IsEmpty())
	{
		static const TArray<FString> ValidParams = {
			TEXT("actor_label"), TEXT("actor_path"), TEXT("actor_guid"), TEXT("actor_tag"),
			TEXT("new_label"), TEXT("new_actor_label"), TEXT("new_name")
		};
		return CreateErrorResponse(TEXT("MISSING_PARAMS"), 
			LevelActorParams::BuildMissingParamsError(TEXT("new_label is required"), ValidParams));
	}
	
	FActorOperationResult Result = Service->RenameActor(Identifier, NewLabel);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleHelp(const TSharedPtr<FJsonObject>& Params)
{
	return FHelpFileReader::HandleHelp(TEXT("manage_level_actors"), Params);
}
