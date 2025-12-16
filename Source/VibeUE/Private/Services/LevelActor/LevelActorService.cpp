// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Services/LevelActor/LevelActorService.h"
#include "Services/LevelActor/Types/LevelActorTypes.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"
#include "LevelEditorViewport.h"
#include "EditorViewportClient.h"
#include "EditorSupportDelegates.h"

// ═══════════════════════════════════════════════════════════════════
// Type Implementation: JSON Serialization
// ═══════════════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FActorPropertyInfo::ToJson() const
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);
	Json->SetStringField(TEXT("name"), Name);
	Json->SetStringField(TEXT("type"), TypeName);
	Json->SetStringField(TEXT("category"), Category);
	Json->SetStringField(TEXT("value"), CurrentValue);
	Json->SetStringField(TEXT("path"), PropertyPath);
	Json->SetBoolField(TEXT("is_editable"), bIsEditable);
	return Json;
}

TSharedPtr<FJsonObject> FActorComponentInfo::ToJson() const
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);
	Json->SetStringField(TEXT("name"), Name);
	Json->SetStringField(TEXT("class_name"), ClassName);
	Json->SetStringField(TEXT("parent"), ParentName);
	Json->SetBoolField(TEXT("is_root"), bIsRoot);
	
	// Location
	TSharedPtr<FJsonObject> LocJson = MakeShareable(new FJsonObject);
	LocJson->SetNumberField(TEXT("x"), RelativeLocation.X);
	LocJson->SetNumberField(TEXT("y"), RelativeLocation.Y);
	LocJson->SetNumberField(TEXT("z"), RelativeLocation.Z);
	Json->SetObjectField(TEXT("relative_location"), LocJson);
	
	// Properties
	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (const FActorPropertyInfo& Prop : Properties)
	{
		PropsArray.Add(MakeShareable(new FJsonValueObject(Prop.ToJson())));
	}
	Json->SetArrayField(TEXT("properties"), PropsArray);
	
	return Json;
}

TSharedPtr<FJsonObject> FActorInfo::ToJson() const
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);
	Json->SetStringField(TEXT("actor_path"), ActorPath);
	Json->SetStringField(TEXT("actor_label"), ActorLabel);
	Json->SetStringField(TEXT("actor_guid"), ActorGuid);
	Json->SetStringField(TEXT("class_name"), ClassName);
	
	// Transform
	TSharedPtr<FJsonObject> LocJson = MakeShareable(new FJsonObject);
	LocJson->SetNumberField(TEXT("x"), Location.X);
	LocJson->SetNumberField(TEXT("y"), Location.Y);
	LocJson->SetNumberField(TEXT("z"), Location.Z);
	Json->SetObjectField(TEXT("location"), LocJson);
	
	TSharedPtr<FJsonObject> RotJson = MakeShareable(new FJsonObject);
	RotJson->SetNumberField(TEXT("pitch"), Rotation.Pitch);
	RotJson->SetNumberField(TEXT("yaw"), Rotation.Yaw);
	RotJson->SetNumberField(TEXT("roll"), Rotation.Roll);
	Json->SetObjectField(TEXT("rotation"), RotJson);
	
	TSharedPtr<FJsonObject> ScaleJson = MakeShareable(new FJsonObject);
	ScaleJson->SetNumberField(TEXT("x"), Scale.X);
	ScaleJson->SetNumberField(TEXT("y"), Scale.Y);
	ScaleJson->SetNumberField(TEXT("z"), Scale.Z);
	Json->SetObjectField(TEXT("scale"), ScaleJson);
	
	// Tags
	TArray<TSharedPtr<FJsonValue>> TagsArray;
	for (const FName& Tag : Tags)
	{
		TagsArray.Add(MakeShareable(new FJsonValueString(Tag.ToString())));
	}
	Json->SetArrayField(TEXT("tags"), TagsArray);
	
	Json->SetBoolField(TEXT("is_selected"), bIsSelected);
	Json->SetBoolField(TEXT("is_hidden"), bIsHidden);
	Json->SetStringField(TEXT("folder_path"), FolderPath);
	
	// Properties
	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (const FActorPropertyInfo& Prop : Properties)
	{
		PropsArray.Add(MakeShareable(new FJsonValueObject(Prop.ToJson())));
	}
	Json->SetArrayField(TEXT("properties"), PropsArray);
	
	// Components
	TArray<TSharedPtr<FJsonValue>> CompsArray;
	for (const FActorComponentInfo& Comp : Components)
	{
		CompsArray.Add(MakeShareable(new FJsonValueObject(Comp.ToJson())));
	}
	Json->SetArrayField(TEXT("components"), CompsArray);
	
	return Json;
}

TSharedPtr<FJsonObject> FActorInfo::ToMinimalJson() const
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);
	Json->SetStringField(TEXT("actor_path"), ActorPath);
	Json->SetStringField(TEXT("actor_label"), ActorLabel);
	Json->SetStringField(TEXT("class_name"), ClassName);
	
	TArray<TSharedPtr<FJsonValue>> LocArray;
	LocArray.Add(MakeShareable(new FJsonValueNumber(Location.X)));
	LocArray.Add(MakeShareable(new FJsonValueNumber(Location.Y)));
	LocArray.Add(MakeShareable(new FJsonValueNumber(Location.Z)));
	Json->SetArrayField(TEXT("location"), LocArray);
	
	return Json;
}

FActorIdentifier FActorIdentifier::FromJson(const TSharedPtr<FJsonObject>& Params)
{
	FActorIdentifier Id;
	if (Params)
	{
		Params->TryGetStringField(TEXT("actor_path"), Id.ActorPath);
		Params->TryGetStringField(TEXT("actor_label"), Id.ActorLabel);
		Params->TryGetStringField(TEXT("actor_guid"), Id.ActorGuid);
		Params->TryGetStringField(TEXT("actor_tag"), Id.ActorTag);
	}
	return Id;
}

FActorQueryCriteria FActorQueryCriteria::FromJson(const TSharedPtr<FJsonObject>& Params)
{
	FActorQueryCriteria Criteria;
	if (Params)
	{
		Params->TryGetStringField(TEXT("class_filter"), Criteria.ClassFilter);
		Params->TryGetStringField(TEXT("label_filter"), Criteria.LabelFilter);
		Params->TryGetBoolField(TEXT("selected_only"), Criteria.bSelectedOnly);
		
		int32 MaxResults;
		if (Params->TryGetNumberField(TEXT("max_results"), MaxResults))
		{
			Criteria.MaxResults = MaxResults;
		}
		
		const TArray<TSharedPtr<FJsonValue>>* TagsArray;
		if (Params->TryGetArrayField(TEXT("required_tags"), TagsArray))
		{
			for (const auto& Val : *TagsArray)
			{
				Criteria.RequiredTags.Add(Val->AsString());
			}
		}
		if (Params->TryGetArrayField(TEXT("excluded_tags"), TagsArray))
		{
			for (const auto& Val : *TagsArray)
			{
				Criteria.ExcludedTags.Add(Val->AsString());
			}
		}
	}
	return Criteria;
}

FActorAddParams FActorAddParams::FromJson(const TSharedPtr<FJsonObject>& Params)
{
	FActorAddParams AddParams;
	if (Params)
	{
		Params->TryGetStringField(TEXT("actor_class"), AddParams.ActorClass);
		Params->TryGetStringField(TEXT("actor_name"), AddParams.ActorName);
		
		const TArray<TSharedPtr<FJsonValue>>* LocArray;
		if (Params->TryGetArrayField(TEXT("spawn_location"), LocArray) && LocArray->Num() >= 3)
		{
			AddParams.Location = FVector(
				(*LocArray)[0]->AsNumber(),
				(*LocArray)[1]->AsNumber(),
				(*LocArray)[2]->AsNumber()
			);
			AddParams.bLocationProvided = true;  // Explicit location given
		}
		
		const TArray<TSharedPtr<FJsonValue>>* RotArray;
		if (Params->TryGetArrayField(TEXT("spawn_rotation"), RotArray) && RotArray->Num() >= 3)
		{
			AddParams.Rotation = FRotator(
				(*RotArray)[0]->AsNumber(),
				(*RotArray)[1]->AsNumber(),
				(*RotArray)[2]->AsNumber()
			);
		}
		
		const TArray<TSharedPtr<FJsonValue>>* ScaleArray;
		if (Params->TryGetArrayField(TEXT("spawn_scale"), ScaleArray) && ScaleArray->Num() >= 3)
		{
			AddParams.Scale = FVector(
				(*ScaleArray)[0]->AsNumber(),
				(*ScaleArray)[1]->AsNumber(),
				(*ScaleArray)[2]->AsNumber()
			);
		}
		
		const TArray<TSharedPtr<FJsonValue>>* TagsArray;
		if (Params->TryGetArrayField(TEXT("tags"), TagsArray))
		{
			for (const auto& Val : *TagsArray)
			{
				AddParams.Tags.Add(Val->AsString());
			}
		}
	}
	return AddParams;
}

FActorOperationResult FActorOperationResult::Success(const FActorInfo& Info)
{
	FActorOperationResult Result;
	Result.bSuccess = true;
	Result.ActorInfo = Info;
	return Result;
}

FActorOperationResult FActorOperationResult::Success(const TArray<FActorInfo>& Actors)
{
	FActorOperationResult Result;
	Result.bSuccess = true;
	Result.AffectedActors = Actors;
	return Result;
}

FActorOperationResult FActorOperationResult::Error(const FString& Code, const FString& Message)
{
	FActorOperationResult Result;
	Result.bSuccess = false;
	Result.ErrorCode = Code;
	Result.ErrorMessage = Message;
	return Result;
}

TSharedPtr<FJsonObject> FActorOperationResult::ToJson() const
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);
	Json->SetBoolField(TEXT("success"), bSuccess);
	
	if (!bSuccess)
	{
		Json->SetStringField(TEXT("error_code"), ErrorCode);
		Json->SetStringField(TEXT("error"), ErrorMessage);
	}
	else if (ActorInfo.IsSet())
	{
		Json->SetObjectField(TEXT("actor"), ActorInfo.GetValue().ToJson());
		
		// Phase 2: Include transform info if present
		if (TransformInfo.IsValid())
		{
			Json->SetObjectField(TEXT("transform"), TransformInfo);
		}
	}
	else if (AffectedActors.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ActorsArray;
		for (const FActorInfo& Info : AffectedActors)
		{
			ActorsArray.Add(MakeShareable(new FJsonValueObject(Info.ToMinimalJson())));
		}
		Json->SetArrayField(TEXT("actors"), ActorsArray);
		Json->SetNumberField(TEXT("count"), AffectedActors.Num());
	}
	
	return Json;
}

// ═══════════════════════════════════════════════════════════════════
// Service Implementation
// ═══════════════════════════════════════════════════════════════════

FLevelActorService::FLevelActorService()
{
	UE_LOG(LogTemp, Display, TEXT("LevelActorService: Initialized"));
}

FLevelActorService::~FLevelActorService()
{
	UE_LOG(LogTemp, Display, TEXT("LevelActorService: Destroyed"));
}

UWorld* FLevelActorService::GetEditorWorld() const
{
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	return nullptr;
}

void FLevelActorService::BeginTransaction(const FText& Description)
{
	if (GEditor)
	{
		GEditor->BeginTransaction(Description);
	}
}

void FLevelActorService::EndTransaction()
{
	if (GEditor)
	{
		GEditor->EndTransaction();
	}
}

AActor* FLevelActorService::FindActorByIdentifier(const FActorIdentifier& Identifier) const
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return nullptr;
	}
	
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		
		// Match by path
		if (!Identifier.ActorPath.IsEmpty())
		{
			if (Actor->GetPathName() == Identifier.ActorPath)
			{
				return Actor;
			}
		}
		
		// Match by label
		if (!Identifier.ActorLabel.IsEmpty())
		{
			if (Actor->GetActorLabel() == Identifier.ActorLabel)
			{
				return Actor;
			}
		}
		
		// Match by GUID
		if (!Identifier.ActorGuid.IsEmpty())
		{
			FGuid Guid;
			FGuid::Parse(Identifier.ActorGuid, Guid);
			if (Actor->GetActorGuid() == Guid)
			{
				return Actor;
			}
		}
		
		// Match by tag
		if (!Identifier.ActorTag.IsEmpty())
		{
			if (Actor->Tags.Contains(FName(*Identifier.ActorTag)))
			{
				return Actor;
			}
		}
	}
	
	return nullptr;
}

UClass* FLevelActorService::FindActorClass(const FString& ClassNameOrPath) const
{
	// Try to find by path first (for already loaded classes)
	UClass* Class = FindObject<UClass>(nullptr, *ClassNameOrPath);
	if (Class && Class->IsChildOf(AActor::StaticClass()))
	{
		return Class;
	}
	
	// Try loading Blueprint class if path looks like a content path
	if (ClassNameOrPath.StartsWith(TEXT("/Game/")) || ClassNameOrPath.StartsWith(TEXT("/Script/")))
	{
		// Try loading with _C suffix if not present
		FString ClassPath = ClassNameOrPath;
		if (!ClassPath.EndsWith(TEXT("_C")))
		{
			// Extract the class name from path and add _C
			FString ClassName = FPaths::GetBaseFilename(ClassPath);
			ClassPath = ClassPath + TEXT(".") + ClassName + TEXT("_C");
		}
		
		Class = LoadClass<AActor>(nullptr, *ClassPath);
		if (Class)
		{
			return Class;
		}
		
		// Try without modification
		Class = LoadClass<AActor>(nullptr, *ClassNameOrPath);
		if (Class)
		{
			return Class;
		}
	}
	
	// Try common prefixes for native classes
	TArray<FString> Prefixes = { TEXT("A"), TEXT(""), TEXT("BP_") };
	for (const FString& Prefix : Prefixes)
	{
		FString FullName = Prefix + ClassNameOrPath;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (It->IsChildOf(AActor::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
			{
				if (It->GetName() == FullName || It->GetName() == ClassNameOrPath)
				{
					return *It;
				}
			}
		}
	}
	
	return nullptr;
}

bool FLevelActorService::MatchesWildcard(const FString& Value, const FString& Pattern) const
{
	if (Pattern.IsEmpty())
	{
		return true;
	}
	
	// Simple wildcard matching
	if (Pattern.StartsWith(TEXT("*")) && Pattern.EndsWith(TEXT("*")))
	{
		FString Middle = Pattern.Mid(1, Pattern.Len() - 2);
		return Value.Contains(Middle, ESearchCase::IgnoreCase);
	}
	else if (Pattern.StartsWith(TEXT("*")))
	{
		FString Suffix = Pattern.Mid(1);
		return Value.EndsWith(Suffix, ESearchCase::IgnoreCase);
	}
	else if (Pattern.EndsWith(TEXT("*")))
	{
		FString Prefix = Pattern.Left(Pattern.Len() - 1);
		return Value.StartsWith(Prefix, ESearchCase::IgnoreCase);
	}
	else
	{
		return Value.Equals(Pattern, ESearchCase::IgnoreCase);
	}
}

bool FLevelActorService::MatchesCriteria(AActor* Actor, const FActorQueryCriteria& Criteria) const
{
	if (!Actor)
	{
		return false;
	}
	
	// Class filter
	if (!Criteria.ClassFilter.IsEmpty())
	{
		if (!MatchesWildcard(Actor->GetClass()->GetName(), Criteria.ClassFilter))
		{
			return false;
		}
	}
	
	// Label filter
	if (!Criteria.LabelFilter.IsEmpty())
	{
		if (!MatchesWildcard(Actor->GetActorLabel(), Criteria.LabelFilter))
		{
			return false;
		}
	}
	
	// Selected only
	if (Criteria.bSelectedOnly && !Actor->IsSelected())
	{
		return false;
	}
	
	// Required tags
	for (const FString& Tag : Criteria.RequiredTags)
	{
		if (!Actor->Tags.Contains(FName(*Tag)))
		{
			return false;
		}
	}
	
	// Excluded tags
	for (const FString& Tag : Criteria.ExcludedTags)
	{
		if (Actor->Tags.Contains(FName(*Tag)))
		{
			return false;
		}
	}
	
	return true;
}

FString FLevelActorService::GetPropertyValueAsString(UObject* Object, FProperty* Property) const
{
	if (!Object || !Property)
	{
		return TEXT("");
	}
	
	FString Value;
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
	Property->ExportTextItem_Direct(Value, ValuePtr, nullptr, Object, PPF_None);
	return Value;
}

TArray<FActorPropertyInfo> FLevelActorService::GetObjectProperties(UObject* Object, bool bIncludeInherited, const FString& CategoryFilter) const
{
	TArray<FActorPropertyInfo> Properties;
	if (!Object)
	{
		return Properties;
	}
	
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		FProperty* Property = *It;
		
		// Skip if not inherited and we don't want inherited
		if (!bIncludeInherited && It.GetStruct() != Object->GetClass())
		{
			continue;
		}
		
		// Skip deprecated and transient
		if (Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
		{
			continue;
		}
		
		FString Category = Property->GetMetaData(TEXT("Category"));
		
		// Category filter
		if (!CategoryFilter.IsEmpty() && !Category.Contains(CategoryFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		
		FActorPropertyInfo PropInfo;
		PropInfo.Name = Property->GetName();
		PropInfo.TypeName = Property->GetCPPType();
		PropInfo.Category = Category;
		PropInfo.CurrentValue = GetPropertyValueAsString(Object, Property);
		PropInfo.PropertyPath = Property->GetName();
		PropInfo.bIsEditable = !Property->HasAnyPropertyFlags(CPF_EditConst);
		
		Properties.Add(PropInfo);
	}
	
	return Properties;
}

FActorComponentInfo FLevelActorService::BuildComponentInfo(UActorComponent* Component, bool bIncludeProperties, const FString& CategoryFilter) const
{
	FActorComponentInfo Info;
	if (!Component)
	{
		return Info;
	}
	
	Info.Name = Component->GetName();
	Info.ClassName = Component->GetClass()->GetName();
	
	if (USceneComponent* SceneComp = Cast<USceneComponent>(Component))
	{
		if (USceneComponent* Parent = SceneComp->GetAttachParent())
		{
			Info.ParentName = Parent->GetName();
		}
		Info.RelativeLocation = SceneComp->GetRelativeLocation();
		Info.RelativeRotation = SceneComp->GetRelativeRotation();
		Info.RelativeScale = SceneComp->GetRelativeScale3D();
	}
	
	if (AActor* Owner = Component->GetOwner())
	{
		Info.bIsRoot = (Component == Owner->GetRootComponent());
	}
	
	if (bIncludeProperties)
	{
		Info.Properties = GetObjectProperties(Component, true, CategoryFilter);
		// Prefix property paths with component name
		for (FActorPropertyInfo& Prop : Info.Properties)
		{
			Prop.PropertyPath = Info.Name + TEXT(".") + Prop.PropertyPath;
		}
	}
	
	return Info;
}

FActorInfo FLevelActorService::BuildActorInfo(AActor* Actor, bool bIncludeComponents, bool bIncludeProperties, const FString& CategoryFilter) const
{
	FActorInfo Info;
	if (!Actor)
	{
		return Info;
	}
	
	Info.ActorPath = Actor->GetPathName();
	Info.ActorLabel = Actor->GetActorLabel();
	Info.ActorGuid = Actor->GetActorGuid().ToString();
	Info.ClassName = Actor->GetClass()->GetName();
	Info.Location = Actor->GetActorLocation();
	Info.Rotation = Actor->GetActorRotation();
	Info.Scale = Actor->GetActorScale3D();
	Info.Tags = Actor->Tags;
	Info.bIsSelected = Actor->IsSelected();
	Info.bIsHidden = Actor->IsHidden();
	Info.FolderPath = Actor->GetFolderPath().ToString();
	
	if (bIncludeProperties)
	{
		Info.Properties = GetObjectProperties(Actor, true, CategoryFilter);
	}
	
	if (bIncludeComponents)
	{
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		for (UActorComponent* Comp : Components)
		{
			Info.Components.Add(BuildComponentInfo(Comp, bIncludeProperties, CategoryFilter));
		}
	}
	
	return Info;
}

// ═══════════════════════════════════════════════════════════════════
// Public API: Phase 1 Actions
// ═══════════════════════════════════════════════════════════════════

FActorOperationResult FLevelActorService::AddActor(const FActorAddParams& Params)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FActorOperationResult::Error(TEXT("NO_WORLD"), TEXT("No editor world available"));
	}
	
	if (Params.ActorClass.IsEmpty())
	{
		return FActorOperationResult::Error(TEXT("MISSING_CLASS"), TEXT("actor_class is required"));
	}
	
	UClass* ActorClass = FindActorClass(Params.ActorClass);
	if (!ActorClass)
	{
		return FActorOperationResult::Error(TEXT("CLASS_NOT_FOUND"), 
			FString::Printf(TEXT("Actor class '%s' not found"), *Params.ActorClass));
	}
	
	// Determine spawn location - use viewport center if no location provided
	FVector SpawnLocation = Params.Location;
	if (!Params.bLocationProvided && GEditor)
	{
		// Get the current viewport camera position and direction
		FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
		if (!ViewportClient)
		{
			for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
			{
				if (Client && Client->IsPerspective())
				{
					ViewportClient = Client;
					break;
				}
			}
		}
		
		if (ViewportClient)
		{
			FVector ViewLocation = ViewportClient->GetViewLocation();
			FRotator ViewRotation = ViewportClient->GetViewRotation();
			FVector ForwardVector = ViewRotation.Vector();
			// Spawn 300 units in front of the camera
			SpawnLocation = ViewLocation + ForwardVector * 300.0f;
		}
	}
	
	// Begin transaction for undo
	BeginTransaction(NSLOCTEXT("LevelActorService", "AddActor", "Add Actor"));
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Don't set SpawnParams.Name - let Unreal auto-generate unique internal name
	// We'll set the actor label (display name) after spawning
	
	FTransform SpawnTransform;
	SpawnTransform.SetLocation(SpawnLocation);
	SpawnTransform.SetRotation(Params.Rotation.Quaternion());
	SpawnTransform.SetScale3D(Params.Scale);
	
	AActor* NewActor = World->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
	
	if (!NewActor)
	{
		EndTransaction();
		return FActorOperationResult::Error(TEXT("SPAWN_FAILED"), TEXT("Failed to spawn actor"));
	}
	
	// Add tags
	for (const FString& Tag : Params.Tags)
	{
		NewActor->Tags.Add(FName(*Tag));
	}
	
	// Set label if name was provided
	if (!Params.ActorName.IsEmpty())
	{
		NewActor->SetActorLabel(Params.ActorName);
	}
	
	// Explicitly set rotation after spawn (spawn transform may be overridden by actor defaults)
	if (!Params.Rotation.IsZero())
	{
		NewActor->SetActorRotation(Params.Rotation);
	}
	
	EndTransaction();
	
	// Force viewport refresh so the new actor is visible immediately
	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (Client)
		{
			Client->Invalidate();
			if (Client->Viewport)
			{
				Client->RedrawRequested(Client->Viewport);
			}
		}
	}
	GEditor->RedrawLevelEditingViewports(true);
	
	FActorInfo Info = BuildActorInfo(NewActor, true, false, TEXT(""));
	return FActorOperationResult::Success(Info);
}

FActorOperationResult FLevelActorService::RemoveActor(const FActorIdentifier& Identifier, bool bWithUndo)
{
	if (!Identifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_IDENTIFIER"), TEXT("No actor identifier provided"));
	}
	
	AActor* Actor = FindActorByIdentifier(Identifier);
	if (!Actor)
	{
		return FActorOperationResult::Error(TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"));
	}
	
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FActorOperationResult::Error(TEXT("NO_WORLD"), TEXT("No editor world available"));
	}
	
	// Store info before destroying
	FActorInfo Info = BuildActorInfo(Actor, false, false, TEXT(""));
	
	if (bWithUndo)
	{
		BeginTransaction(NSLOCTEXT("LevelActorService", "RemoveActor", "Remove Actor"));
	}
	
	bool bDestroyed = World->EditorDestroyActor(Actor, true);
	
	if (bWithUndo)
	{
		EndTransaction();
	}
	
	if (!bDestroyed)
	{
		return FActorOperationResult::Error(TEXT("DESTROY_FAILED"), TEXT("Failed to destroy actor"));
	}
	
	return FActorOperationResult::Success(Info);
}

FActorOperationResult FLevelActorService::ListActors(const FActorQueryCriteria& Criteria)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return FActorOperationResult::Error(TEXT("NO_WORLD"), TEXT("No editor world available"));
	}
	
	TArray<FActorInfo> Actors;
	int32 Count = 0;
	
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		
		if (!MatchesCriteria(Actor, Criteria))
		{
			continue;
		}
		
		Actors.Add(BuildActorInfo(Actor, false, false, TEXT("")));
		Count++;
		
		if (Count >= Criteria.MaxResults)
		{
			break;
		}
	}
	
	return FActorOperationResult::Success(Actors);
}

FActorOperationResult FLevelActorService::FindActors(const FActorQueryCriteria& Criteria)
{
	return ListActors(Criteria);
}

FActorOperationResult FLevelActorService::GetActorInfo(
	const FActorIdentifier& Identifier,
	bool bIncludeComponents,
	bool bIncludeProperties,
	const FString& CategoryFilter)
{
	if (!Identifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_IDENTIFIER"), TEXT("No actor identifier provided"));
	}
	
	AActor* Actor = FindActorByIdentifier(Identifier);
	if (!Actor)
	{
		return FActorOperationResult::Error(TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"));
	}
	
	FActorInfo Info = BuildActorInfo(Actor, bIncludeComponents, bIncludeProperties, CategoryFilter);
	return FActorOperationResult::Success(Info);
}

// ═══════════════════════════════════════════════════════════════════
// Phase 2: Transform Operations
// ═══════════════════════════════════════════════════════════════════

TSharedPtr<FJsonObject> FActorTransformInfo::ToJson() const
{
	TSharedPtr<FJsonObject> Json = MakeShareable(new FJsonObject);
	
	// World transform
	TSharedPtr<FJsonObject> WorldLocJson = MakeShareable(new FJsonObject);
	WorldLocJson->SetNumberField(TEXT("x"), WorldLocation.X);
	WorldLocJson->SetNumberField(TEXT("y"), WorldLocation.Y);
	WorldLocJson->SetNumberField(TEXT("z"), WorldLocation.Z);
	Json->SetObjectField(TEXT("world_location"), WorldLocJson);
	
	TSharedPtr<FJsonObject> WorldRotJson = MakeShareable(new FJsonObject);
	WorldRotJson->SetNumberField(TEXT("pitch"), WorldRotation.Pitch);
	WorldRotJson->SetNumberField(TEXT("yaw"), WorldRotation.Yaw);
	WorldRotJson->SetNumberField(TEXT("roll"), WorldRotation.Roll);
	Json->SetObjectField(TEXT("world_rotation"), WorldRotJson);
	
	TSharedPtr<FJsonObject> WorldScaleJson = MakeShareable(new FJsonObject);
	WorldScaleJson->SetNumberField(TEXT("x"), WorldScale.X);
	WorldScaleJson->SetNumberField(TEXT("y"), WorldScale.Y);
	WorldScaleJson->SetNumberField(TEXT("z"), WorldScale.Z);
	Json->SetObjectField(TEXT("world_scale"), WorldScaleJson);
	
	// Relative transform
	TSharedPtr<FJsonObject> RelLocJson = MakeShareable(new FJsonObject);
	RelLocJson->SetNumberField(TEXT("x"), RelativeLocation.X);
	RelLocJson->SetNumberField(TEXT("y"), RelativeLocation.Y);
	RelLocJson->SetNumberField(TEXT("z"), RelativeLocation.Z);
	Json->SetObjectField(TEXT("relative_location"), RelLocJson);
	
	TSharedPtr<FJsonObject> RelRotJson = MakeShareable(new FJsonObject);
	RelRotJson->SetNumberField(TEXT("pitch"), RelativeRotation.Pitch);
	RelRotJson->SetNumberField(TEXT("yaw"), RelativeRotation.Yaw);
	RelRotJson->SetNumberField(TEXT("roll"), RelativeRotation.Roll);
	Json->SetObjectField(TEXT("relative_rotation"), RelRotJson);
	
	TSharedPtr<FJsonObject> RelScaleJson = MakeShareable(new FJsonObject);
	RelScaleJson->SetNumberField(TEXT("x"), RelativeScale.X);
	RelScaleJson->SetNumberField(TEXT("y"), RelativeScale.Y);
	RelScaleJson->SetNumberField(TEXT("z"), RelativeScale.Z);
	Json->SetObjectField(TEXT("relative_scale"), RelScaleJson);
	
	// Direction vectors
	TSharedPtr<FJsonObject> ForwardJson = MakeShareable(new FJsonObject);
	ForwardJson->SetNumberField(TEXT("x"), Forward.X);
	ForwardJson->SetNumberField(TEXT("y"), Forward.Y);
	ForwardJson->SetNumberField(TEXT("z"), Forward.Z);
	Json->SetObjectField(TEXT("forward"), ForwardJson);
	
	TSharedPtr<FJsonObject> RightJson = MakeShareable(new FJsonObject);
	RightJson->SetNumberField(TEXT("x"), Right.X);
	RightJson->SetNumberField(TEXT("y"), Right.Y);
	RightJson->SetNumberField(TEXT("z"), Right.Z);
	Json->SetObjectField(TEXT("right"), RightJson);
	
	TSharedPtr<FJsonObject> UpJson = MakeShareable(new FJsonObject);
	UpJson->SetNumberField(TEXT("x"), Up.X);
	UpJson->SetNumberField(TEXT("y"), Up.Y);
	UpJson->SetNumberField(TEXT("z"), Up.Z);
	Json->SetObjectField(TEXT("up"), UpJson);
	
	// Bounds
	TSharedPtr<FJsonObject> OriginJson = MakeShareable(new FJsonObject);
	OriginJson->SetNumberField(TEXT("x"), Origin.X);
	OriginJson->SetNumberField(TEXT("y"), Origin.Y);
	OriginJson->SetNumberField(TEXT("z"), Origin.Z);
	Json->SetObjectField(TEXT("origin"), OriginJson);
	
	TSharedPtr<FJsonObject> ExtentJson = MakeShareable(new FJsonObject);
	ExtentJson->SetNumberField(TEXT("x"), Extent.X);
	ExtentJson->SetNumberField(TEXT("y"), Extent.Y);
	ExtentJson->SetNumberField(TEXT("z"), Extent.Z);
	Json->SetObjectField(TEXT("extent"), ExtentJson);
	
	return Json;
}

FActorTransformParams FActorTransformParams::FromJson(const TSharedPtr<FJsonObject>& Params)
{
	FActorTransformParams TransformParams;
	if (!Params) return TransformParams;
	
	TransformParams.Identifier = FActorIdentifier::FromJson(Params);
	
	// Parse location if present
	if (Params->HasField(TEXT("location")))
	{
		const TArray<TSharedPtr<FJsonValue>>* LocArray;
		if (Params->TryGetArrayField(TEXT("location"), LocArray) && LocArray->Num() >= 3)
		{
			TransformParams.Location = FVector(
				(*LocArray)[0]->AsNumber(),
				(*LocArray)[1]->AsNumber(),
				(*LocArray)[2]->AsNumber()
			);
		}
	}
	
	// Parse rotation if present
	if (Params->HasField(TEXT("rotation")))
	{
		const TArray<TSharedPtr<FJsonValue>>* RotArray;
		if (Params->TryGetArrayField(TEXT("rotation"), RotArray) && RotArray->Num() >= 3)
		{
			TransformParams.Rotation = FRotator(
				(*RotArray)[0]->AsNumber(),
				(*RotArray)[1]->AsNumber(),
				(*RotArray)[2]->AsNumber()
			);
		}
	}
	
	// Parse scale if present
	if (Params->HasField(TEXT("scale")))
	{
		const TArray<TSharedPtr<FJsonValue>>* ScaleArray;
		if (Params->TryGetArrayField(TEXT("scale"), ScaleArray) && ScaleArray->Num() >= 3)
		{
			TransformParams.Scale = FVector(
				(*ScaleArray)[0]->AsNumber(),
				(*ScaleArray)[1]->AsNumber(),
				(*ScaleArray)[2]->AsNumber()
			);
		}
	}
	
	// Parse options
	Params->TryGetBoolField(TEXT("world_space"), TransformParams.bWorldSpace);
	Params->TryGetBoolField(TEXT("sweep"), TransformParams.bSweep);
	Params->TryGetBoolField(TEXT("teleport"), TransformParams.bTeleport);
	
	return TransformParams;
}

FActorOperationResult FLevelActorService::SetTransform(const FActorTransformParams& Params)
{
	if (!Params.Identifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_IDENTIFIER"), TEXT("No actor identifier provided"));
	}
	
	AActor* Actor = FindActorByIdentifier(Params.Identifier);
	if (!Actor)
	{
		return FActorOperationResult::Error(TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"));
	}
	
	USceneComponent* RootComponent = Actor->GetRootComponent();
	if (!RootComponent)
	{
		return FActorOperationResult::Error(TEXT("NO_ROOT_COMPONENT"), TEXT("Actor has no root component"));
	}
	
	BeginTransaction(FText::FromString(TEXT("Set Actor Transform")));
	
	Actor->Modify();
	RootComponent->Modify();
	
	// Set location if provided
	if (Params.Location.IsSet())
	{
		if (Params.bWorldSpace)
		{
			Actor->SetActorLocation(Params.Location.GetValue(), Params.bSweep, nullptr, 
				Params.bTeleport ? ETeleportType::TeleportPhysics : ETeleportType::None);
		}
		else
		{
			RootComponent->SetRelativeLocation(Params.Location.GetValue());
		}
	}
	
	// Set rotation if provided
	if (Params.Rotation.IsSet())
	{
		if (Params.bWorldSpace)
		{
			Actor->SetActorRotation(Params.Rotation.GetValue());
		}
		else
		{
			RootComponent->SetRelativeRotation(Params.Rotation.GetValue());
		}
	}
	
	// Set scale if provided
	if (Params.Scale.IsSet())
	{
		Actor->SetActorScale3D(Params.Scale.GetValue());
	}
	
	EndTransaction();
	
	// Mark package dirty for save
	Actor->MarkPackageDirty();
	
	// Force viewport refresh
	GEditor->RedrawLevelEditingViewports(true);
	
	FActorInfo Info = BuildActorInfo(Actor, false, false, TEXT(""));
	return FActorOperationResult::Success(Info);
}

FActorOperationResult FLevelActorService::GetTransform(const FActorIdentifier& Identifier)
{
	if (!Identifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_IDENTIFIER"), TEXT("No actor identifier provided"));
	}
	
	AActor* Actor = FindActorByIdentifier(Identifier);
	if (!Actor)
	{
		return FActorOperationResult::Error(TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"));
	}
	
	FActorTransformInfo TransformInfo;
	
	// World transform
	TransformInfo.WorldLocation = Actor->GetActorLocation();
	TransformInfo.WorldRotation = Actor->GetActorRotation();
	TransformInfo.WorldScale = Actor->GetActorScale3D();
	
	// Relative transform (if has root component)
	if (USceneComponent* RootComponent = Actor->GetRootComponent())
	{
		TransformInfo.RelativeLocation = RootComponent->GetRelativeLocation();
		TransformInfo.RelativeRotation = RootComponent->GetRelativeRotation();
		TransformInfo.RelativeScale = RootComponent->GetRelativeScale3D();
	}
	
	// Direction vectors
	TransformInfo.Forward = Actor->GetActorForwardVector();
	TransformInfo.Right = Actor->GetActorRightVector();
	TransformInfo.Up = Actor->GetActorUpVector();
	
	// Bounds
	Actor->GetActorBounds(false, TransformInfo.Origin, TransformInfo.Extent);
	
	// Build result with transform info
	FActorInfo Info = BuildActorInfo(Actor, false, false, TEXT(""));
	
	FActorOperationResult Result;
	Result.bSuccess = true;
	Result.ActorInfo = Info;
	Result.TransformInfo = TransformInfo.ToJson();
	return Result;
}

FActorOperationResult FLevelActorService::SetLocation(
	const FActorIdentifier& Identifier,
	const FVector& Location,
	bool bWorldSpace,
	bool bSweep)
{
	FActorTransformParams Params;
	Params.Identifier = Identifier;
	Params.Location = Location;
	Params.bWorldSpace = bWorldSpace;
	Params.bSweep = bSweep;
	
	return SetTransform(Params);
}

FActorOperationResult FLevelActorService::SetRotation(
	const FActorIdentifier& Identifier,
	const FRotator& Rotation,
	bool bWorldSpace)
{
	FActorTransformParams Params;
	Params.Identifier = Identifier;
	Params.Rotation = Rotation;
	Params.bWorldSpace = bWorldSpace;
	
	return SetTransform(Params);
}

FActorOperationResult FLevelActorService::SetScale(
	const FActorIdentifier& Identifier,
	const FVector& Scale)
{
	FActorTransformParams Params;
	Params.Identifier = Identifier;
	Params.Scale = Scale;
	
	return SetTransform(Params);
}

// ═══════════════════════════════════════════════════════════════════
// Editor View Operations
// ═══════════════════════════════════════════════════════════════════

FActorOperationResult FLevelActorService::FocusActor(const FActorIdentifier& Identifier, bool bInstant)
{
	if (!Identifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_IDENTIFIER"), TEXT("No actor identifier provided"));
	}
	
	AActor* Actor = FindActorByIdentifier(Identifier);
	if (!Actor)
	{
		return FActorOperationResult::Error(TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"));
	}
	
	// Use GEditor to focus on the actor
	if (GEditor)
	{
		// Select the actor and focus on it
		GEditor->SelectNone(true, true, false);
		GEditor->SelectActor(Actor, true, true, true);
		GEditor->MoveViewportCamerasToActor(*Actor, bInstant);
		
		FActorInfo Info = BuildActorInfo(Actor, false, false, TEXT(""));
		return FActorOperationResult::Success(Info);
	}
	
	return FActorOperationResult::Error(TEXT("NO_EDITOR"), TEXT("Editor not available"));
}

FActorOperationResult FLevelActorService::MoveActorToView(const FActorIdentifier& Identifier)
{
	if (!Identifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_IDENTIFIER"), TEXT("No actor identifier provided"));
	}
	
	AActor* Actor = FindActorByIdentifier(Identifier);
	if (!Actor)
	{
		return FActorOperationResult::Error(TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"));
	}
	
	USceneComponent* RootComponent = Actor->GetRootComponent();
	if (!RootComponent)
	{
		return FActorOperationResult::Error(TEXT("NO_ROOT_COMPONENT"), TEXT("Actor has no root component"));
	}
	
	// Get the current level editing viewport client (the one user is actively using)
	FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	
	if (!ViewportClient)
	{
		// Fallback: try to get the active viewport
		if (GEditor && GEditor->GetActiveViewport())
		{
			ViewportClient = static_cast<FLevelEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
		}
	}
	
	if (!ViewportClient)
	{
		// Last resort: get any perspective viewport
		for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
		{
			if (Client && Client->IsPerspective())
			{
				ViewportClient = Client;
				break;
			}
		}
	}
	
	if (!ViewportClient)
	{
		return FActorOperationResult::Error(TEXT("NO_VIEWPORT"), TEXT("No active viewport found"));
	}
	
	// Get viewport camera location and direction - use GetViewTransform for most up-to-date info
	FVector ViewLocation = ViewportClient->GetViewLocation();
	FRotator ViewRotation = ViewportClient->GetViewRotation();
	
	// Calculate a position in front of the camera
	// Use a reasonable distance based on actor bounds
	FVector Origin, Extent;
	Actor->GetActorBounds(false, Origin, Extent);
	float ActorRadius = Extent.Size();
	float Distance = FMath::Max(200.0f, ActorRadius * 2.0f);
	
	FVector ForwardVector = ViewRotation.Vector();
	FVector NewLocation = ViewLocation + ForwardVector * Distance;
	
	// Move the actor
	BeginTransaction(FText::FromString(TEXT("Move Actor to View")));
	
	Actor->Modify();
	RootComponent->Modify();
	
	Actor->SetActorLocation(NewLocation);
	
	EndTransaction();
	
	Actor->MarkPackageDirty();
	
	// Update actor transform immediately
	Actor->UpdateComponentTransforms();
	
	// Force viewport refresh - invalidate all viewports and request immediate redraw
	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (Client)
		{
			Client->Invalidate();
			Client->RedrawRequested(Client->Viewport);
		}
	}
	GEditor->RedrawLevelEditingViewports(true);
	
	FActorInfo Info = BuildActorInfo(Actor, false, false, TEXT(""));
	return FActorOperationResult::Success(Info);
}

FActorOperationResult FLevelActorService::RefreshViewport()
{
	if (!GEditor)
	{
		return FActorOperationResult::Error(TEXT("NO_EDITOR"), TEXT("Editor not available"));
	}
	
	// Use the delegate broadcast which is the standard UE way to refresh all viewports after changes
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	
	// Force all level editing viewports to render a frame immediately
	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (Client)
		{
			// Request at least one real-time frame even if viewport is not in realtime mode
			// This ensures the viewport actually renders the changes
			if (!Client->IsRealtime())
			{
				Client->RequestRealTimeFrames(1);
			}
			
			Client->Invalidate();
			
			if (Client->Viewport)
			{
				// Force immediate draw
				Client->Viewport->Draw();
			}
			
			// Use the editor's update function which handles non-realtime viewports properly
			GEditor->UpdateSingleViewportClient(Client, /*bInAllowNonRealtimeViewportToDraw=*/ true, /*bLinkedOrthoMovement=*/ false);
		}
	}
	
	// Force full redraw with hit proxy invalidation
	GEditor->RedrawLevelEditingViewports(true);
	
	FActorOperationResult Result;
	Result.bSuccess = true;
	return Result;
}

// ═══════════════════════════════════════════════════════════════════
// Phase 3: Property Operations
// ═══════════════════════════════════════════════════════════════════

FActorPropertyParams FActorPropertyParams::FromJson(const TSharedPtr<FJsonObject>& Params)
{
	FActorPropertyParams PropertyParams;
	if (!Params) return PropertyParams;
	
	PropertyParams.Identifier = FActorIdentifier::FromJson(Params);
	Params->TryGetStringField(TEXT("property_path"), PropertyParams.PropertyPath);
	Params->TryGetStringField(TEXT("property_value"), PropertyParams.PropertyValue);
	Params->TryGetStringField(TEXT("component_name"), PropertyParams.ComponentName);
	Params->TryGetBoolField(TEXT("include_inherited"), PropertyParams.bIncludeInherited);
	Params->TryGetStringField(TEXT("category_filter"), PropertyParams.CategoryFilter);
	
	return PropertyParams;
}

FActorOperationResult FLevelActorService::GetProperty(const FActorPropertyParams& Params)
{
	if (!Params.Identifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_IDENTIFIER"), TEXT("No actor identifier provided"));
	}
	
	if (Params.PropertyPath.IsEmpty())
	{
		return FActorOperationResult::Error(TEXT("MISSING_PROPERTY"), TEXT("property_path is required"));
	}
	
	AActor* Actor = FindActorByIdentifier(Params.Identifier);
	if (!Actor)
	{
		return FActorOperationResult::Error(TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"));
	}
	
	// Determine target object (actor or component)
	UObject* TargetObject = Actor;
	FString PropertyName = Params.PropertyPath;
	
	// Check if targeting a component (format: "ComponentName.PropertyName" or just use component_name param)
	if (!Params.ComponentName.IsEmpty())
	{
		UActorComponent* Component = Actor->GetComponentByClass(UActorComponent::StaticClass());
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp && Comp->GetName() == Params.ComponentName)
			{
				TargetObject = Comp;
				break;
			}
		}
		if (TargetObject == Actor)
		{
			return FActorOperationResult::Error(TEXT("COMPONENT_NOT_FOUND"), 
				FString::Printf(TEXT("Component '%s' not found"), *Params.ComponentName));
		}
	}
	else if (PropertyName.Contains(TEXT(".")))
	{
		// Parse "ComponentName.PropertyName" format
		FString ComponentName;
		PropertyName.Split(TEXT("."), &ComponentName, &PropertyName);
		
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp && Comp->GetName() == ComponentName)
			{
				TargetObject = Comp;
				break;
			}
		}
		if (TargetObject == Actor)
		{
			return FActorOperationResult::Error(TEXT("COMPONENT_NOT_FOUND"), 
				FString::Printf(TEXT("Component '%s' not found"), *ComponentName));
		}
	}
	
	// Find the property
	FProperty* Property = TargetObject->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		return FActorOperationResult::Error(TEXT("PROPERTY_NOT_FOUND"), 
			FString::Printf(TEXT("Property '%s' not found"), *PropertyName));
	}
	
	// Get property value
	FString Value = GetPropertyValueAsString(TargetObject, Property);
	
	// Build result
	FActorInfo Info = BuildActorInfo(Actor, false, false, TEXT(""));
	FActorOperationResult Result = FActorOperationResult::Success(Info);
	
	// Add property info to result
	TSharedPtr<FJsonObject> PropertyJson = MakeShareable(new FJsonObject);
	PropertyJson->SetStringField(TEXT("name"), PropertyName);
	PropertyJson->SetStringField(TEXT("value"), Value);
	PropertyJson->SetStringField(TEXT("type"), Property->GetCPPType());
	PropertyJson->SetBoolField(TEXT("is_editable"), !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_BlueprintReadOnly));
	
#if WITH_EDITORONLY_DATA
	PropertyJson->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
#endif
	
	Result.TransformInfo = PropertyJson;  // Reuse TransformInfo for property data
	return Result;
}

FActorOperationResult FLevelActorService::SetProperty(const FActorPropertyParams& Params)
{
	if (!Params.Identifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_IDENTIFIER"), TEXT("No actor identifier provided"));
	}
	
	if (Params.PropertyPath.IsEmpty())
	{
		return FActorOperationResult::Error(TEXT("MISSING_PROPERTY"), TEXT("property_path is required"));
	}
	
	AActor* Actor = FindActorByIdentifier(Params.Identifier);
	if (!Actor)
	{
		return FActorOperationResult::Error(TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"));
	}
	
	// Determine target object (actor or component)
	UObject* TargetObject = Actor;
	FString PropertyName = Params.PropertyPath;
	
	// Check if targeting a component
	if (!Params.ComponentName.IsEmpty())
	{
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp && Comp->GetName() == Params.ComponentName)
			{
				TargetObject = Comp;
				break;
			}
		}
		if (TargetObject == Actor)
		{
			return FActorOperationResult::Error(TEXT("COMPONENT_NOT_FOUND"), 
				FString::Printf(TEXT("Component '%s' not found"), *Params.ComponentName));
		}
	}
	else if (PropertyName.Contains(TEXT(".")))
	{
		// Parse "ComponentName.PropertyName" format
		FString ComponentName;
		PropertyName.Split(TEXT("."), &ComponentName, &PropertyName);
		
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp && Comp->GetName() == ComponentName)
			{
				TargetObject = Comp;
				break;
			}
		}
		if (TargetObject == Actor)
		{
			return FActorOperationResult::Error(TEXT("COMPONENT_NOT_FOUND"), 
				FString::Printf(TEXT("Component '%s' not found"), *ComponentName));
		}
	}
	
	// Check for array index syntax: PropertyName[index]
	int32 ArrayIndex = INDEX_NONE;
	FString BasePropertyName = PropertyName;
	if (PropertyName.Contains(TEXT("[")))
	{
		FString IndexStr;
		if (PropertyName.Split(TEXT("["), &BasePropertyName, &IndexStr))
		{
			IndexStr.RemoveFromEnd(TEXT("]"));
			ArrayIndex = FCString::Atoi(*IndexStr);
		}
	}
	
	// Find the property
	FProperty* Property = TargetObject->GetClass()->FindPropertyByName(FName(*BasePropertyName));
	if (!Property)
	{
		return FActorOperationResult::Error(TEXT("PROPERTY_NOT_FOUND"), 
			FString::Printf(TEXT("Property '%s' not found"), *BasePropertyName));
	}
	
	// Check if property is editable
	if (Property->HasAnyPropertyFlags(CPF_EditConst))
	{
		return FActorOperationResult::Error(TEXT("PROPERTY_READONLY"), 
			FString::Printf(TEXT("Property '%s' is read-only"), *BasePropertyName));
	}
	
	BeginTransaction(FText::FromString(FString::Printf(TEXT("Set Property: %s"), *PropertyName)));
	
	TargetObject->Modify();
	
	// Handle TArray properties with index
	FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property);
	if (ArrayProp && ArrayIndex != INDEX_NONE)
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(TargetObject));
		
		// Resize array if needed
		if (ArrayIndex >= ArrayHelper.Num())
		{
			ArrayHelper.Resize(ArrayIndex + 1);
		}
		
		void* ElementPtr = ArrayHelper.GetRawPtr(ArrayIndex);
		FProperty* InnerProp = ArrayProp->Inner;
		
		// Handle object reference arrays (e.g., TArray<UMaterialInterface*>)
		FObjectProperty* ObjProp = CastField<FObjectProperty>(InnerProp);
		if (ObjProp)
		{
			// Load the object by path
			UObject* LoadedObject = StaticLoadObject(ObjProp->PropertyClass, nullptr, *Params.PropertyValue);
			if (!LoadedObject)
			{
				EndTransaction();
				return FActorOperationResult::Error(TEXT("OBJECT_NOT_FOUND"), 
					FString::Printf(TEXT("Could not load object: %s"), *Params.PropertyValue));
			}
			ObjProp->SetObjectPropertyValue(ElementPtr, LoadedObject);
		}
		else
		{
			// Use ImportText for other types
			if (!InnerProp->ImportText_Direct(*Params.PropertyValue, ElementPtr, TargetObject, PPF_None))
			{
				EndTransaction();
				return FActorOperationResult::Error(TEXT("INVALID_VALUE"), 
					FString::Printf(TEXT("Failed to set array element '%s[%d]' to '%s'"), *BasePropertyName, ArrayIndex, *Params.PropertyValue));
			}
		}
	}
	else
	{
		// Check for special case: StaticMesh property on StaticMeshComponent
		// Must use native SetStaticMesh() function instead of reflection to trigger proper render state updates
		UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(TargetObject);
		if (StaticMeshComp && BasePropertyName == TEXT("StaticMesh"))
		{
			UStaticMesh* NewMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Params.PropertyValue));
			if (!NewMesh && !Params.PropertyValue.IsEmpty() && Params.PropertyValue != TEXT("None"))
			{
				EndTransaction();
				return FActorOperationResult::Error(TEXT("OBJECT_NOT_FOUND"), 
					FString::Printf(TEXT("Could not load StaticMesh: %s"), *Params.PropertyValue));
			}
			StaticMeshComp->SetStaticMesh(NewMesh);
		}
		else
		{
			// Set the property value using ImportText
			void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
			if (!Property->ImportText_Direct(*Params.PropertyValue, ValuePtr, TargetObject, PPF_None))
			{
				EndTransaction();
				return FActorOperationResult::Error(TEXT("INVALID_VALUE"), 
					FString::Printf(TEXT("Failed to set property '%s' to '%s'"), *PropertyName, *Params.PropertyValue));
			}
		}
	}
	
	// Notify property changed
	FPropertyChangedEvent PropertyChangedEvent(Property);
	TargetObject->PostEditChangeProperty(PropertyChangedEvent);
	
	// Force visual update for primitive components (mesh, material changes)
	if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(TargetObject))
	{
		// Check if render state is created before calling these methods
		if (PrimComp->IsRenderStateCreated())
		{
			PrimComp->MarkRenderStateDirty();
		}
		// Update bounds for any visual changes
		PrimComp->UpdateBounds();
		
		// Force re-register component to trigger immediate visual update
		// This is necessary for static mesh changes to be reflected immediately
		PrimComp->RecreateRenderState_Concurrent();
	}
	
	// Broadcast viewport redraw to force immediate visual update
	FEditorSupportDelegates::RedrawAllViewports.Broadcast();
	
	// Force all level editing viewports to render immediately
	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (Client)
		{
			if (!Client->IsRealtime())
			{
				Client->RequestRealTimeFrames(1);
			}
			Client->Invalidate();
			GEditor->UpdateSingleViewportClient(Client, /*bInAllowNonRealtimeViewportToDraw=*/ true, /*bLinkedOrthoMovement=*/ false);
		}
	}
	
	EndTransaction();
	
	Actor->MarkPackageDirty();
	
	FActorInfo Info = BuildActorInfo(Actor, false, false, TEXT(""));
	return FActorOperationResult::Success(Info);
}

FActorOperationResult FLevelActorService::GetAllProperties(const FActorPropertyParams& Params)
{
	if (!Params.Identifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_IDENTIFIER"), TEXT("No actor identifier provided"));
	}
	
	AActor* Actor = FindActorByIdentifier(Params.Identifier);
	if (!Actor)
	{
		return FActorOperationResult::Error(TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"));
	}
	
	// Determine target object (actor or component)
	UObject* TargetObject = Actor;
	
	if (!Params.ComponentName.IsEmpty())
	{
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp && Comp->GetName() == Params.ComponentName)
			{
				TargetObject = Comp;
				break;
			}
		}
		if (TargetObject == Actor && !Params.ComponentName.IsEmpty())
		{
			return FActorOperationResult::Error(TEXT("COMPONENT_NOT_FOUND"), 
				FString::Printf(TEXT("Component '%s' not found"), *Params.ComponentName));
		}
	}
	
	// Get all properties
	TArray<FActorPropertyInfo> Properties = GetObjectProperties(TargetObject, Params.bIncludeInherited, Params.CategoryFilter);
	
	// Build result with full property info
	FActorInfo Info = BuildActorInfo(Actor, false, false, TEXT(""));
	Info.Properties = Properties;
	
	return FActorOperationResult::Success(Info);
}

// ═══════════════════════════════════════════════════════════════════
// Phase 4: Hierarchy & Organization
// ═══════════════════════════════════════════════════════════════════

FActorAttachParams FActorAttachParams::FromJson(const TSharedPtr<FJsonObject>& Params)
{
	FActorAttachParams AttachParams;
	if (!Params) return AttachParams;
	
	// Child identifier
	AttachParams.ChildIdentifier = FActorIdentifier::FromJson(Params);
	
	// Parent identifier - prefixed fields
	FString ParentPath, ParentLabel, ParentGuid, ParentTag;
	Params->TryGetStringField(TEXT("parent_path"), ParentPath);
	Params->TryGetStringField(TEXT("parent_label"), ParentLabel);
	Params->TryGetStringField(TEXT("parent_guid"), ParentGuid);
	Params->TryGetStringField(TEXT("parent_tag"), ParentTag);
	
	AttachParams.ParentIdentifier.ActorPath = ParentPath;
	AttachParams.ParentIdentifier.ActorLabel = ParentLabel;
	AttachParams.ParentIdentifier.ActorGuid = ParentGuid;
	AttachParams.ParentIdentifier.ActorTag = ParentTag;
	
	Params->TryGetStringField(TEXT("socket_name"), AttachParams.SocketName);
	Params->TryGetBoolField(TEXT("weld_simulated_bodies"), AttachParams.bWeldSimulatedBodies);
	
	return AttachParams;
}

FActorSelectParams FActorSelectParams::FromJson(const TSharedPtr<FJsonObject>& Params)
{
	FActorSelectParams SelectParams;
	if (!Params) return SelectParams;
	
	// Single actor identifier
	FActorIdentifier SingleId = FActorIdentifier::FromJson(Params);
	if (SingleId.IsValid())
	{
		SelectParams.Identifiers.Add(SingleId);
	}
	
	// Multiple actors via "actors" array
	const TArray<TSharedPtr<FJsonValue>>* ActorsArray;
	if (Params->TryGetArrayField(TEXT("actors"), ActorsArray))
	{
		for (const auto& ActorValue : *ActorsArray)
		{
			if (const TSharedPtr<FJsonObject>* ActorObj = nullptr; ActorValue->TryGetObject(ActorObj))
			{
				FActorIdentifier Id = FActorIdentifier::FromJson(*ActorObj);
				if (Id.IsValid())
				{
					SelectParams.Identifiers.Add(Id);
				}
			}
			else if (FString Label; ActorValue->TryGetString(Label))
			{
				// Simple string = actor label
				FActorIdentifier Id;
				Id.ActorLabel = Label;
				SelectParams.Identifiers.Add(Id);
			}
		}
	}
	
	Params->TryGetBoolField(TEXT("add_to_selection"), SelectParams.bAddToSelection);
	Params->TryGetBoolField(TEXT("deselect"), SelectParams.bDeselect);
	Params->TryGetBoolField(TEXT("deselect_all"), SelectParams.bDeselectAll);
	
	return SelectParams;
}

FActorOperationResult FLevelActorService::SetFolder(const FActorIdentifier& Identifier, const FString& FolderPath)
{
	if (!Identifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_IDENTIFIER"), TEXT("No actor identifier provided"));
	}
	
	AActor* Actor = FindActorByIdentifier(Identifier);
	if (!Actor)
	{
		return FActorOperationResult::Error(TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"));
	}
	
	BeginTransaction(FText::FromString(TEXT("Set Actor Folder")));
	
	Actor->Modify();
	Actor->SetFolderPath(FName(*FolderPath));
	
	EndTransaction();
	
	Actor->MarkPackageDirty();
	
	FActorInfo Info = BuildActorInfo(Actor, false, false, TEXT(""));
	return FActorOperationResult::Success(Info);
}

FActorOperationResult FLevelActorService::AttachActor(const FActorAttachParams& Params)
{
	if (!Params.ChildIdentifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_CHILD"), TEXT("No child actor identifier provided"));
	}
	
	if (!Params.ParentIdentifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_PARENT"), TEXT("No parent actor identifier provided"));
	}
	
	AActor* ChildActor = FindActorByIdentifier(Params.ChildIdentifier);
	if (!ChildActor)
	{
		return FActorOperationResult::Error(TEXT("CHILD_NOT_FOUND"), TEXT("Child actor not found"));
	}
	
	AActor* ParentActor = FindActorByIdentifier(Params.ParentIdentifier);
	if (!ParentActor)
	{
		return FActorOperationResult::Error(TEXT("PARENT_NOT_FOUND"), TEXT("Parent actor not found"));
	}
	
	if (ChildActor == ParentActor)
	{
		return FActorOperationResult::Error(TEXT("SELF_ATTACH"), TEXT("Cannot attach actor to itself"));
	}
	
	USceneComponent* ChildRoot = ChildActor->GetRootComponent();
	USceneComponent* ParentRoot = ParentActor->GetRootComponent();
	
	if (!ChildRoot || !ParentRoot)
	{
		return FActorOperationResult::Error(TEXT("NO_ROOT_COMPONENT"), TEXT("Both actors must have root components"));
	}
	
	BeginTransaction(FText::FromString(TEXT("Attach Actor")));
	
	ChildActor->Modify();
	ChildRoot->Modify();
	
	FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, Params.bWeldSimulatedBodies);
	FName SocketName = Params.SocketName.IsEmpty() ? NAME_None : FName(*Params.SocketName);
	
	ChildRoot->AttachToComponent(ParentRoot, AttachRules, SocketName);
	
	EndTransaction();
	
	ChildActor->MarkPackageDirty();
	
	FActorInfo Info = BuildActorInfo(ChildActor, false, false, TEXT(""));
	return FActorOperationResult::Success(Info);
}

FActorOperationResult FLevelActorService::DetachActor(const FActorIdentifier& Identifier)
{
	if (!Identifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_IDENTIFIER"), TEXT("No actor identifier provided"));
	}
	
	AActor* Actor = FindActorByIdentifier(Identifier);
	if (!Actor)
	{
		return FActorOperationResult::Error(TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"));
	}
	
	USceneComponent* RootComponent = Actor->GetRootComponent();
	if (!RootComponent)
	{
		return FActorOperationResult::Error(TEXT("NO_ROOT_COMPONENT"), TEXT("Actor has no root component"));
	}
	
	if (!RootComponent->GetAttachParent())
	{
		return FActorOperationResult::Error(TEXT("NOT_ATTACHED"), TEXT("Actor is not attached to anything"));
	}
	
	BeginTransaction(FText::FromString(TEXT("Detach Actor")));
	
	Actor->Modify();
	RootComponent->Modify();
	
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	RootComponent->DetachFromComponent(DetachRules);
	
	EndTransaction();
	
	Actor->MarkPackageDirty();
	
	FActorInfo Info = BuildActorInfo(Actor, false, false, TEXT(""));
	return FActorOperationResult::Success(Info);
}

FActorOperationResult FLevelActorService::SelectActors(const FActorSelectParams& Params)
{
	if (!GEditor)
	{
		return FActorOperationResult::Error(TEXT("NO_EDITOR"), TEXT("Editor not available"));
	}
	
	// Handle deselect all first
	if (Params.bDeselectAll)
	{
		GEditor->SelectNone(true, true, false);
		
		FActorOperationResult Result;
		Result.bSuccess = true;
		return Result;
	}
	
	// If not adding to selection and not deselecting, clear current selection
	if (!Params.bAddToSelection && !Params.bDeselect)
	{
		GEditor->SelectNone(false, true, false);
	}
	
	TArray<FActorInfo> AffectedActors;
	
	for (const FActorIdentifier& Identifier : Params.Identifiers)
	{
		AActor* Actor = FindActorByIdentifier(Identifier);
		if (Actor)
		{
			bool bSelect = !Params.bDeselect;
			GEditor->SelectActor(Actor, bSelect, true, true);
			AffectedActors.Add(BuildActorInfo(Actor, false, false, TEXT("")));
		}
	}
	
	// Notify selection changed
	GEditor->NoteSelectionChange();
	
	return FActorOperationResult::Success(AffectedActors);
}

FActorOperationResult FLevelActorService::RenameActor(const FActorIdentifier& Identifier, const FString& NewLabel)
{
	if (!Identifier.IsValid())
	{
		return FActorOperationResult::Error(TEXT("INVALID_IDENTIFIER"), TEXT("No actor identifier provided"));
	}
	
	if (NewLabel.IsEmpty())
	{
		return FActorOperationResult::Error(TEXT("EMPTY_LABEL"), TEXT("New label cannot be empty"));
	}
	
	AActor* Actor = FindActorByIdentifier(Identifier);
	if (!Actor)
	{
		return FActorOperationResult::Error(TEXT("ACTOR_NOT_FOUND"), TEXT("Actor not found"));
	}
	
	BeginTransaction(FText::FromString(TEXT("Rename Actor")));
	
	Actor->Modify();
	Actor->SetActorLabel(NewLabel);
	
	EndTransaction();
	
	Actor->MarkPackageDirty();
	
	FActorInfo Info = BuildActorInfo(Actor, false, false, TEXT(""));
	return FActorOperationResult::Success(Info);
}
