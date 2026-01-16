// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "PythonAPI/UActorService.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "LevelEditorViewport.h"
#include "EditorSupportDelegates.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/DirectionalLight.h"
#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "ScopedTransaction.h"
#include "UObject/PropertyIterator.h"

// ═══════════════════════════════════════════════════════════════════
// Helper Functions
// ═══════════════════════════════════════════════════════════════════

UWorld* UActorService::GetEditorWorld()
{
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	return nullptr;
}

AActor* UActorService::FindActorByIdentifier(const FString& NameOrLabel)
{
	UWorld* World = GetEditorWorld();
	if (!World || NameOrLabel.IsEmpty())
	{
		return nullptr;
	}

	FString LowerSearch = NameOrLabel.ToLower();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		// Try exact label match first
		if (Actor->GetActorLabel().ToLower() == LowerSearch)
		{
			return Actor;
		}
		// Then exact name match
		if (Actor->GetName().ToLower() == LowerSearch)
		{
			return Actor;
		}
	}

	// If no exact match, try contains match
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		if (Actor->GetActorLabel().ToLower().Contains(LowerSearch) ||
			Actor->GetName().ToLower().Contains(LowerSearch))
		{
			return Actor;
		}
	}

	return nullptr;
}

void UActorService::PopulateActorInfo(AActor* Actor, FLevelActorInfo& OutInfo)
{
	if (!Actor) return;

	OutInfo.ActorName = Actor->GetName();
	OutInfo.ActorLabel = Actor->GetActorLabel();
	OutInfo.ActorClass = Actor->GetClass()->GetName();
	OutInfo.Location = Actor->GetActorLocation();
	OutInfo.Rotation = Actor->GetActorRotation();
	OutInfo.Scale = Actor->GetActorScale3D();
	OutInfo.FolderPath = Actor->GetFolderPath().ToString();
	OutInfo.bIsHidden = Actor->IsHidden();
	OutInfo.bIsSelected = Actor->IsSelected();
}

UClass* UActorService::FindActorClass(const FString& ClassName)
{
	if (ClassName.IsEmpty()) return nullptr;

	// Try direct lookup first
	UClass* FoundClass = FindObject<UClass>(nullptr, *ClassName);
	if (FoundClass && FoundClass->IsChildOf(AActor::StaticClass()))
	{
		return FoundClass;
	}

	// Try common actor class names
	static TMap<FString, UClass*> CommonClasses;
	if (CommonClasses.Num() == 0)
	{
		CommonClasses.Add(TEXT("pointlight"), APointLight::StaticClass());
		CommonClasses.Add(TEXT("spotlight"), ASpotLight::StaticClass());
		CommonClasses.Add(TEXT("directionallight"), ADirectionalLight::StaticClass());
		CommonClasses.Add(TEXT("staticmeshactor"), AStaticMeshActor::StaticClass());
		CommonClasses.Add(TEXT("actor"), AActor::StaticClass());
	}

	FString LowerName = ClassName.ToLower();
	if (UClass** Found = CommonClasses.Find(LowerName))
	{
		return *Found;
	}

	// Try loading as a blueprint path
	if (ClassName.StartsWith(TEXT("/Game/")))
	{
		FString BlueprintPath = ClassName;
		if (!BlueprintPath.EndsWith(TEXT("_C")))
		{
			BlueprintPath += TEXT("_C");
		}
		FoundClass = LoadClass<AActor>(nullptr, *BlueprintPath);
		if (FoundClass) return FoundClass;
	}

	// Try searching by partial name
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(AActor::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			if (Class->GetName().Contains(ClassName, ESearchCase::IgnoreCase))
			{
				return Class;
			}
		}
	}

	return nullptr;
}

FString UActorService::GetPropertyValueAsString(UObject* Object, FProperty* Property)
{
	if (!Object || !Property) return TEXT("");

	FString Value;
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Object);
	Property->ExportText_Direct(Value, ValuePtr, ValuePtr, Object, PPF_None);
	return Value;
}

void UActorService::BeginTransaction(const FText& Description)
{
	if (GEditor)
	{
		GEditor->BeginTransaction(Description);
	}
}

void UActorService::EndTransaction()
{
	if (GEditor)
	{
		GEditor->EndTransaction();
	}
}

// ═══════════════════════════════════════════════════════════════════
// Discovery Operations
// ═══════════════════════════════════════════════════════════════════

TArray<FLevelActorInfo> UActorService::ListLevelActors(
	const FString& ActorClassFilter,
	bool bIncludeHidden,
	int32 MaxResults)
{
	TArray<FLevelActorInfo> Actors;
	UWorld* World = GetEditorWorld();
	if (!World) return Actors;

	FString LowerFilter = ActorClassFilter.ToLower();
	int32 Count = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;

		// Skip hidden actors if requested
		if (!bIncludeHidden && Actor->IsHidden()) continue;

		// Apply class filter (supports wildcard with *)
		if (!ActorClassFilter.IsEmpty())
		{
			FString ClassName = Actor->GetClass()->GetName().ToLower();
			if (LowerFilter.Contains(TEXT("*")))
			{
				// Wildcard match
				if (!ClassName.MatchesWildcard(LowerFilter))
					continue;
			}
			else
			{
				// Contains match
				if (!ClassName.Contains(LowerFilter))
					continue;
			}
		}

		FLevelActorInfo Info;
		PopulateActorInfo(Actor, Info);
		Actors.Add(Info);

		if (++Count >= MaxResults) break;
	}

	return Actors;
}

TArray<FLevelActorInfo> UActorService::FindActorsByClass(const FString& ClassName)
{
	return ListLevelActors(ClassName, false, 100);
}

bool UActorService::GetActorInfo(const FString& ActorNameOrLabel, FLevelActorInfo& OutInfo)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	PopulateActorInfo(Actor, OutInfo);
	return true;
}

// ═══════════════════════════════════════════════════════════════════
// Lifecycle Operations
// ═══════════════════════════════════════════════════════════════════

FLevelActorInfo UActorService::AddActor(
	const FString& ActorClass,
	FVector Location,
	FRotator Rotation,
	FVector Scale,
	const FString& ActorLabel)
{
	FLevelActorInfo Result;
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("AddActor: No editor world available"));
		return Result;
	}

	UClass* SpawnClass = FindActorClass(ActorClass);
	if (!SpawnClass)
	{
		UE_LOG(LogTemp, Error, TEXT("AddActor: Actor class '%s' not found"), *ActorClass);
		return Result;
	}

	// Determine spawn location
	FVector SpawnLocation = Location;
	if (Location.IsNearlyZero() && GEditor)
	{
		// Spawn in front of viewport
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
			SpawnLocation = ViewLocation + ViewRotation.Vector() * 300.0f;
		}
	}

	BeginTransaction(NSLOCTEXT("ActorService", "AddActor", "Add Actor"));

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FTransform SpawnTransform;
	SpawnTransform.SetLocation(SpawnLocation);
	SpawnTransform.SetRotation(Rotation.Quaternion());
	SpawnTransform.SetScale3D(Scale);

	AActor* NewActor = World->SpawnActor<AActor>(SpawnClass, SpawnTransform, SpawnParams);
	if (!NewActor)
	{
		EndTransaction();
		UE_LOG(LogTemp, Error, TEXT("AddActor: Failed to spawn actor"));
		return Result;
	}

	// Set label if provided
	if (!ActorLabel.IsEmpty())
	{
		NewActor->SetActorLabel(ActorLabel);
	}

	// Apply rotation after spawn (some actors override spawn rotation)
	if (!Rotation.IsZero())
	{
		NewActor->SetActorRotation(Rotation);
	}

	EndTransaction();

	// Refresh viewport
	RefreshViewport();

	PopulateActorInfo(NewActor, Result);
	return Result;
}

bool UActorService::RemoveActor(const FString& ActorNameOrLabel)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveActor: Actor '%s' not found"), *ActorNameOrLabel);
		return false;
	}

	UWorld* World = GetEditorWorld();
	if (!World) return false;

	BeginTransaction(NSLOCTEXT("ActorService", "RemoveActor", "Remove Actor"));

	bool bDestroyed = World->EditorDestroyActor(Actor, true);

	EndTransaction();

	return bDestroyed;
}

// ═══════════════════════════════════════════════════════════════════
// Transform Operations
// ═══════════════════════════════════════════════════════════════════

bool UActorService::GetTransform(const FString& ActorNameOrLabel, FActorTransformData& OutTransform)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	OutTransform.WorldLocation = Actor->GetActorLocation();
	OutTransform.WorldRotation = Actor->GetActorRotation();
	OutTransform.WorldScale = Actor->GetActorScale3D();

	if (USceneComponent* Root = Actor->GetRootComponent())
	{
		OutTransform.RelativeLocation = Root->GetRelativeLocation();
		OutTransform.RelativeRotation = Root->GetRelativeRotation();
		OutTransform.RelativeScale = Root->GetRelativeScale3D();
	}

	OutTransform.Forward = Actor->GetActorForwardVector();
	OutTransform.Right = Actor->GetActorRightVector();
	OutTransform.Up = Actor->GetActorUpVector();

	Actor->GetActorBounds(false, OutTransform.BoundsOrigin, OutTransform.BoundsExtent);

	return true;
}

bool UActorService::SetTransform(
	const FString& ActorNameOrLabel,
	FVector Location,
	FRotator Rotation,
	FVector Scale)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	BeginTransaction(FText::FromString(TEXT("Set Actor Transform")));

	Actor->Modify();
	if (USceneComponent* Root = Actor->GetRootComponent())
	{
		Root->Modify();
	}

	Actor->SetActorLocation(Location);
	Actor->SetActorRotation(Rotation);
	Actor->SetActorScale3D(Scale);

	EndTransaction();

	Actor->MarkPackageDirty();
	RefreshViewport();

	return true;
}

bool UActorService::SetLocation(
	const FString& ActorNameOrLabel,
	FVector Location,
	bool bSweep)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	BeginTransaction(FText::FromString(TEXT("Set Actor Location")));

	Actor->Modify();
	Actor->SetActorLocation(Location, bSweep);

	EndTransaction();

	Actor->MarkPackageDirty();
	RefreshViewport();

	return true;
}

bool UActorService::SetRotation(const FString& ActorNameOrLabel, FRotator Rotation)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	BeginTransaction(FText::FromString(TEXT("Set Actor Rotation")));

	Actor->Modify();
	Actor->SetActorRotation(Rotation);

	EndTransaction();

	Actor->MarkPackageDirty();
	RefreshViewport();

	return true;
}

bool UActorService::SetScale(const FString& ActorNameOrLabel, FVector Scale)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	BeginTransaction(FText::FromString(TEXT("Set Actor Scale")));

	Actor->Modify();
	Actor->SetActorScale3D(Scale);

	EndTransaction();

	Actor->MarkPackageDirty();
	RefreshViewport();

	return true;
}

// ═══════════════════════════════════════════════════════════════════
// Viewport Operations
// ═══════════════════════════════════════════════════════════════════

bool UActorService::FocusActor(const FString& ActorNameOrLabel, bool bInstant)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor || !GEditor) return false;

	GEditor->SelectNone(true, true, false);
	GEditor->SelectActor(Actor, true, true, true);
	GEditor->MoveViewportCamerasToActor(*Actor, bInstant);

	return true;
}

bool UActorService::MoveActorToView(const FString& ActorNameOrLabel)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor || !GEditor) return false;

	USceneComponent* Root = Actor->GetRootComponent();
	if (!Root) return false;

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
	if (!ViewportClient) return false;

	FVector ViewLocation = ViewportClient->GetViewLocation();
	FRotator ViewRotation = ViewportClient->GetViewRotation();

	// Calculate distance based on actor bounds
	FVector Origin, Extent;
	Actor->GetActorBounds(false, Origin, Extent);
	float Distance = FMath::Max(200.0f, Extent.Size() * 2.0f);

	FVector NewLocation = ViewLocation + ViewRotation.Vector() * Distance;

	BeginTransaction(FText::FromString(TEXT("Move Actor to View")));

	Actor->Modify();
	Root->Modify();
	Actor->SetActorLocation(NewLocation);

	EndTransaction();

	Actor->MarkPackageDirty();
	RefreshViewport();

	return true;
}

bool UActorService::RefreshViewport()
{
	if (!GEditor) return false;

	FEditorSupportDelegates::RedrawAllViewports.Broadcast();

	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (Client)
		{
			if (!Client->IsRealtime())
			{
				Client->RequestRealTimeFrames(1);
			}
			Client->Invalidate();
			if (Client->Viewport)
			{
				Client->Viewport->Draw();
			}
			GEditor->UpdateSingleViewportClient(Client, true, false);
		}
	}

	GEditor->RedrawLevelEditingViewports(true);
	return true;
}

// ═══════════════════════════════════════════════════════════════════
// Property Operations
// ═══════════════════════════════════════════════════════════════════

bool UActorService::GetProperty(
	const FString& ActorNameOrLabel,
	const FString& PropertyPath,
	FString& OutValue)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor || PropertyPath.IsEmpty()) return false;

	UObject* TargetObject = Actor;
	FString PropertyName = PropertyPath;

	// Check for component path format: ComponentName.PropertyName
	if (PropertyPath.Contains(TEXT(".")))
	{
		FString ComponentName;
		PropertyPath.Split(TEXT("."), &ComponentName, &PropertyName);

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
			UE_LOG(LogTemp, Warning, TEXT("GetProperty: Component '%s' not found"), *ComponentName);
			return false;
		}
	}
	else
	{
		// Try to find property on components if not found on actor
		FProperty* Property = TargetObject->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Property)
		{
			for (UActorComponent* Comp : Actor->GetComponents())
			{
				if (!Comp) continue;
				FProperty* CompProperty = Comp->GetClass()->FindPropertyByName(FName(*PropertyName));
				if (CompProperty)
				{
					TargetObject = Comp;
					Property = CompProperty;
					break;
				}
			}
		}
	}

	FProperty* Property = TargetObject->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetProperty: Property '%s' not found"), *PropertyName);
		return false;
	}

	OutValue = GetPropertyValueAsString(TargetObject, Property);
	return true;
}

bool UActorService::SetProperty(
	const FString& ActorNameOrLabel,
	const FString& PropertyPath,
	const FString& Value)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor || PropertyPath.IsEmpty()) return false;

	UObject* TargetObject = Actor;
	FString PropertyName = PropertyPath;

	// Check for component path format: ComponentName.PropertyName
	if (PropertyPath.Contains(TEXT(".")))
	{
		FString ComponentName;
		PropertyPath.Split(TEXT("."), &ComponentName, &PropertyName);

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
			UE_LOG(LogTemp, Warning, TEXT("SetProperty: Component '%s' not found"), *ComponentName);
			return false;
		}
	}
	else
	{
		// Try to find property on components if not found on actor
		FProperty* Property = TargetObject->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!Property)
		{
			for (UActorComponent* Comp : Actor->GetComponents())
			{
				if (!Comp) continue;
				FProperty* CompProperty = Comp->GetClass()->FindPropertyByName(FName(*PropertyName));
				if (CompProperty)
				{
					TargetObject = Comp;
					Property = CompProperty;
					break;
				}
			}
		}
	}

	FProperty* Property = TargetObject->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (!Property)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetProperty: Property '%s' not found"), *PropertyName);
		return false;
	}

	if (Property->HasAnyPropertyFlags(CPF_EditConst))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetProperty: Property '%s' is read-only"), *PropertyName);
		return false;
	}

	BeginTransaction(FText::FromString(FString::Printf(TEXT("Set Property: %s"), *PropertyPath)));

	TargetObject->Modify();

	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(TargetObject);
	if (!Property->ImportText_Direct(*Value, ValuePtr, TargetObject, PPF_None))
	{
		EndTransaction();
		UE_LOG(LogTemp, Warning, TEXT("SetProperty: Failed to set property '%s' to '%s'"), *PropertyName, *Value);
		return false;
	}

	FPropertyChangedEvent PropertyChangedEvent(Property);
	TargetObject->PostEditChangeProperty(PropertyChangedEvent);

	EndTransaction();

	Actor->MarkPackageDirty();
	RefreshViewport();

	return true;
}

TArray<FActorPropertyData> UActorService::GetAllProperties(
	const FString& ActorNameOrLabel,
	const FString& ComponentName,
	const FString& CategoryFilter)
{
	TArray<FActorPropertyData> Properties;
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return Properties;

	UObject* TargetObject = Actor;

	if (!ComponentName.IsEmpty())
	{
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp && Comp->GetName() == ComponentName)
			{
				TargetObject = Comp;
				break;
			}
		}
	}

	for (TFieldIterator<FProperty> It(TargetObject->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient)) continue;

#if WITH_EDITORONLY_DATA
		FString Category = Prop->GetMetaData(TEXT("Category"));
		if (!CategoryFilter.IsEmpty() && !Category.Contains(CategoryFilter))
			continue;
#else
		FString Category;
#endif

		FActorPropertyData PropData;
		PropData.Name = Prop->GetName();
		PropData.Value = GetPropertyValueAsString(TargetObject, Prop);
		PropData.Type = Prop->GetCPPType();
		PropData.Category = Category;
		PropData.bIsEditable = !Prop->HasAnyPropertyFlags(CPF_EditConst | CPF_BlueprintReadOnly);

		Properties.Add(PropData);
	}

	return Properties;
}

// ═══════════════════════════════════════════════════════════════════
// Organization Operations
// ═══════════════════════════════════════════════════════════════════

bool UActorService::SetFolder(const FString& ActorNameOrLabel, const FString& FolderPath)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	BeginTransaction(FText::FromString(TEXT("Set Actor Folder")));

	Actor->Modify();
	Actor->SetFolderPath(FName(*FolderPath));

	EndTransaction();

	Actor->MarkPackageDirty();
	return true;
}

bool UActorService::RenameActor(const FString& ActorNameOrLabel, const FString& NewLabel)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor || NewLabel.IsEmpty()) return false;

	BeginTransaction(FText::FromString(TEXT("Rename Actor")));

	Actor->Modify();
	Actor->SetActorLabel(NewLabel);

	EndTransaction();

	Actor->MarkPackageDirty();
	return true;
}

// ═══════════════════════════════════════════════════════════════════
// Hierarchy Operations
// ═══════════════════════════════════════════════════════════════════

bool UActorService::AttachActor(
	const FString& ChildNameOrLabel,
	const FString& ParentNameOrLabel,
	const FString& SocketName)
{
	AActor* ChildActor = FindActorByIdentifier(ChildNameOrLabel);
	AActor* ParentActor = FindActorByIdentifier(ParentNameOrLabel);

	if (!ChildActor || !ParentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("AttachActor: Child or parent actor not found"));
		return false;
	}

	if (ChildActor == ParentActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("AttachActor: Cannot attach actor to itself"));
		return false;
	}

	USceneComponent* ChildRoot = ChildActor->GetRootComponent();
	USceneComponent* ParentRoot = ParentActor->GetRootComponent();

	if (!ChildRoot || !ParentRoot)
	{
		UE_LOG(LogTemp, Warning, TEXT("AttachActor: Both actors must have root components"));
		return false;
	}

	BeginTransaction(FText::FromString(TEXT("Attach Actor")));

	ChildActor->Modify();
	ChildRoot->Modify();

	FAttachmentTransformRules Rules(EAttachmentRule::KeepWorld, true);
	FName Socket = SocketName.IsEmpty() ? NAME_None : FName(*SocketName);

	ChildRoot->AttachToComponent(ParentRoot, Rules, Socket);

	EndTransaction();

	ChildActor->MarkPackageDirty();
	return true;
}

bool UActorService::DetachActor(const FString& ActorNameOrLabel)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	USceneComponent* Root = Actor->GetRootComponent();
	if (!Root)
	{
		UE_LOG(LogTemp, Warning, TEXT("DetachActor: Actor has no root component"));
		return false;
	}

	if (!Root->GetAttachParent())
	{
		UE_LOG(LogTemp, Warning, TEXT("DetachActor: Actor is not attached to anything"));
		return false;
	}

	BeginTransaction(FText::FromString(TEXT("Detach Actor")));

	Actor->Modify();
	Root->Modify();

	FDetachmentTransformRules Rules(EDetachmentRule::KeepWorld, true);
	Root->DetachFromComponent(Rules);

	EndTransaction();

	Actor->MarkPackageDirty();
	return true;
}

// ═══════════════════════════════════════════════════════════════════
// Selection Operations
// ═══════════════════════════════════════════════════════════════════

bool UActorService::SelectActor(const FString& ActorNameOrLabel, bool bAddToSelection)
{
	if (!GEditor) return false;

	// Handle comma-separated list of actors
	TArray<FString> ActorNames;
	ActorNameOrLabel.ParseIntoArray(ActorNames, TEXT(","), true);

	if (!bAddToSelection)
	{
		GEditor->SelectNone(false, true, false);
	}

	bool bAnySelected = false;
	for (const FString& Name : ActorNames)
	{
		FString TrimmedName = Name.TrimStartAndEnd();
		AActor* Actor = FindActorByIdentifier(TrimmedName);
		if (Actor)
		{
			GEditor->SelectActor(Actor, true, true, true);
			bAnySelected = true;
		}
	}

	if (bAnySelected)
	{
		GEditor->NoteSelectionChange();
	}

	return bAnySelected;
}

bool UActorService::DeselectAll()
{
	if (!GEditor) return false;

	GEditor->SelectNone(true, true, false);
	return true;
}
