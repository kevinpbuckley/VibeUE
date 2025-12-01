// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Commands/LevelActorCommands.h"
#include "Services/LevelActor/LevelActorService.h"
#include "Services/LevelActor/Types/LevelActorTypes.h"

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
	else if (Action == TEXT("set_folder"))
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
	FActorAddParams AddParams = FActorAddParams::FromJson(Params);
	FActorOperationResult Result = Service->AddActor(AddParams);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleRemove(const TSharedPtr<FJsonObject>& Params)
{
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	bool bWithUndo = true;
	Params->TryGetBoolField(TEXT("with_undo"), bWithUndo);
	
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
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	bool bIncludeComponents = true;
	bool bIncludeProperties = true;
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
	FActorTransformParams TransformParams = FActorTransformParams::FromJson(Params);
	FActorOperationResult Result = Service->SetTransform(TransformParams);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleGetTransform(const TSharedPtr<FJsonObject>& Params)
{
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	FActorOperationResult Result = Service->GetTransform(Identifier);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleSetLocation(const TSharedPtr<FJsonObject>& Params)
{
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	// Parse location array
	FVector Location = FVector::ZeroVector;
	const TArray<TSharedPtr<FJsonValue>>* LocationArray;
	if (Params->TryGetArrayField(TEXT("location"), LocationArray) && LocationArray->Num() >= 3)
	{
		Location.X = (*LocationArray)[0]->AsNumber();
		Location.Y = (*LocationArray)[1]->AsNumber();
		Location.Z = (*LocationArray)[2]->AsNumber();
	}
	else
	{
		return CreateErrorResponse(TEXT("MISSING_LOCATION"), TEXT("location parameter is required as [X, Y, Z] array"));
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
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	// Parse rotation array [Pitch, Yaw, Roll]
	FRotator Rotation = FRotator::ZeroRotator;
	const TArray<TSharedPtr<FJsonValue>>* RotationArray;
	if (Params->TryGetArrayField(TEXT("rotation"), RotationArray) && RotationArray->Num() >= 3)
	{
		Rotation.Pitch = (*RotationArray)[0]->AsNumber();
		Rotation.Yaw = (*RotationArray)[1]->AsNumber();
		Rotation.Roll = (*RotationArray)[2]->AsNumber();
	}
	else
	{
		return CreateErrorResponse(TEXT("MISSING_ROTATION"), TEXT("rotation parameter is required as [Pitch, Yaw, Roll] array"));
	}
	
	bool bWorldSpace = true;
	Params->TryGetBoolField(TEXT("world_space"), bWorldSpace);
	
	FActorOperationResult Result = Service->SetRotation(Identifier, Rotation, bWorldSpace);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleSetScale(const TSharedPtr<FJsonObject>& Params)
{
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	// Parse scale array [X, Y, Z]
	FVector Scale = FVector::OneVector;
	const TArray<TSharedPtr<FJsonValue>>* ScaleArray;
	if (Params->TryGetArrayField(TEXT("scale"), ScaleArray) && ScaleArray->Num() >= 3)
	{
		Scale.X = (*ScaleArray)[0]->AsNumber();
		Scale.Y = (*ScaleArray)[1]->AsNumber();
		Scale.Z = (*ScaleArray)[2]->AsNumber();
	}
	else
	{
		return CreateErrorResponse(TEXT("MISSING_SCALE"), TEXT("scale parameter is required as [X, Y, Z] array"));
	}
	
	FActorOperationResult Result = Service->SetScale(Identifier, Scale);
	return Result.ToJson();
}

// ============================================================================
// Editor View Operations
// ============================================================================

TSharedPtr<FJsonObject> FLevelActorCommands::HandleFocus(const TSharedPtr<FJsonObject>& Params)
{
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	bool bInstant = false;
	Params->TryGetBoolField(TEXT("instant"), bInstant);
	
	FActorOperationResult Result = Service->FocusActor(Identifier, bInstant);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleMoveToView(const TSharedPtr<FJsonObject>& Params)
{
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
	FActorPropertyParams PropertyParams = FActorPropertyParams::FromJson(Params);
	FActorOperationResult Result = Service->GetProperty(PropertyParams);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleSetProperty(const TSharedPtr<FJsonObject>& Params)
{
	FActorPropertyParams PropertyParams = FActorPropertyParams::FromJson(Params);
	FActorOperationResult Result = Service->SetProperty(PropertyParams);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleGetAllProperties(const TSharedPtr<FJsonObject>& Params)
{
	FActorPropertyParams PropertyParams = FActorPropertyParams::FromJson(Params);
	FActorOperationResult Result = Service->GetAllProperties(PropertyParams);
	return Result.ToJson();
}

// ============================================================================
// Phase 4: Hierarchy & Organization
// ============================================================================

TSharedPtr<FJsonObject> FLevelActorCommands::HandleSetFolder(const TSharedPtr<FJsonObject>& Params)
{
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	FString FolderPath;
	Params->TryGetStringField(TEXT("folder_path"), FolderPath);
	
	FActorOperationResult Result = Service->SetFolder(Identifier, FolderPath);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleAttach(const TSharedPtr<FJsonObject>& Params)
{
	FActorAttachParams AttachParams = FActorAttachParams::FromJson(Params);
	FActorOperationResult Result = Service->AttachActor(AttachParams);
	return Result.ToJson();
}

TSharedPtr<FJsonObject> FLevelActorCommands::HandleDetach(const TSharedPtr<FJsonObject>& Params)
{
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
	FActorIdentifier Identifier = FActorIdentifier::FromJson(Params);
	
	FString NewLabel;
	Params->TryGetStringField(TEXT("new_label"), NewLabel);
	
	FActorOperationResult Result = Service->RenameActor(Identifier, NewLabel);
	return Result.ToJson();
}
