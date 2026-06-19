// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UActorService.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "LevelEditorViewport.h"
#include "EditorSupportDelegates.h"
#include "ScopedTransaction.h"
#include "UObject/PropertyIterator.h"

// =================================================================
// Helper Functions
// =================================================================

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

// =================================================================
// Transform Lock / Constraint Operations
// =================================================================

bool UActorService::SetActorLockLocation(const FString& ActorNameOrLabel, bool bLocked)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	FBoolProperty* Prop = CastField<FBoolProperty>(AActor::StaticClass()->FindPropertyByName(TEXT("bLockLocation")));
	if (!Prop) return false;

	BeginTransaction(FText::FromString(TEXT("Set Actor Lock Location")));

	Actor->Modify();
	Prop->SetPropertyValue_InContainer(Actor, bLocked);
	Actor->PostEditChange();

	EndTransaction();

	Actor->MarkPackageDirty();
	return true;
}

bool UActorService::GetActorLockLocation(const FString& ActorNameOrLabel, bool& OutLocked)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	FBoolProperty* Prop = CastField<FBoolProperty>(AActor::StaticClass()->FindPropertyByName(TEXT("bLockLocation")));
	if (!Prop) return false;

	OutLocked = Prop->GetPropertyValue_InContainer(Actor);
	return true;
}

bool UActorService::SetAbsoluteTransform(
	const FString& ActorNameOrLabel,
	bool bAbsoluteLocation,
	bool bAbsoluteRotation,
	bool bAbsoluteScale)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	USceneComponent* Root = Actor->GetRootComponent();
	if (!Root) return false;

	BeginTransaction(FText::FromString(TEXT("Set Absolute Transform Flags")));

	Root->Modify();
	Root->SetAbsolute(bAbsoluteLocation, bAbsoluteRotation, bAbsoluteScale);

	EndTransaction();

	Actor->MarkPackageDirty();
	RefreshViewport();

	return true;
}

bool UActorService::GetAbsoluteTransform(
	const FString& ActorNameOrLabel,
	bool& OutAbsoluteLocation,
	bool& OutAbsoluteRotation,
	bool& OutAbsoluteScale)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	USceneComponent* Root = Actor->GetRootComponent();
	if (!Root) return false;

	OutAbsoluteLocation = Root->IsUsingAbsoluteLocation();
	OutAbsoluteRotation = Root->IsUsingAbsoluteRotation();
	OutAbsoluteScale = Root->IsUsingAbsoluteScale();

	return true;
}

bool UActorService::SetPreserveScaleRatio(bool bPreserve)
{
	GConfig->SetBool(TEXT("SelectionDetails"), TEXT("PreserveScaleRatio"), bPreserve, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
	return true;
}

bool UActorService::GetPreserveScaleRatio()
{
	bool bPreserve = true;
	GConfig->GetBool(TEXT("SelectionDetails"), TEXT("PreserveScaleRatio"), bPreserve, GEditorPerProjectIni);
	return bPreserve;
}

// =================================================================
// Viewport Helpers
// =================================================================

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

// =================================================================
// Camera View Operations
// =================================================================

FLevelEditorViewportClient* UActorService::GetPerspectiveViewportClient()
{
	if (!GEditor) return nullptr;

	FLevelEditorViewportClient* ViewportClient = GCurrentLevelEditingViewportClient;
	if (ViewportClient && ViewportClient->IsPerspective())
	{
		return ViewportClient;
	}

	for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
	{
		if (Client && Client->IsPerspective())
		{
			return Client;
		}
	}
	return nullptr;
}

FCameraViewInfo UActorService::CalculateViewForActor(AActor* Actor, EViewDirection Direction, float PaddingMultiplier)
{
	FCameraViewInfo Result;
	if (!Actor) return Result;

	// Get actor bounds
	FVector Origin, Extent;
	Actor->GetActorBounds(false, Origin, Extent);

	// Ensure minimum extent so we don't get degenerate views for flat objects
	float MinExtent = 100.0f;
	Extent.X = FMath::Max(Extent.X, MinExtent);
	Extent.Y = FMath::Max(Extent.Y, MinExtent);
	Extent.Z = FMath::Max(Extent.Z, MinExtent);

	// Calculate view distance based on the face of the bounding box the camera sees
	// Use a 60-degree FOV assumption (half-angle = 30 degrees, tan(30) ~= 0.577)
	float HalfFOVTangent = 0.577f; // tan(30 degrees)

	float ViewDistance = 0.0f;
	FVector CameraDirection = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;

	switch (Direction)
	{
	case EViewDirection::Top:
		// Looking down from above: camera sees XY extent
		ViewDistance = FMath::Max(Extent.X, Extent.Y) / HalfFOVTangent;
		CameraDirection = FVector(0, 0, 1);  // Camera is above, offset in +Z
		CameraRotation = FRotator(-90, 0, 0); // Pitch straight down
		break;

	case EViewDirection::Bottom:
		// Looking up from below: camera sees XY extent
		ViewDistance = FMath::Max(Extent.X, Extent.Y) / HalfFOVTangent;
		CameraDirection = FVector(0, 0, -1); // Camera is below, offset in -Z
		CameraRotation = FRotator(90, 0, 0); // Pitch straight up
		break;

	case EViewDirection::Left:
		// Looking from left side (-Y): camera sees XZ extent
		ViewDistance = FMath::Max(Extent.X, Extent.Z) / HalfFOVTangent;
		CameraDirection = FVector(0, -1, 0); // Camera is to the left (-Y)
		CameraRotation = FRotator(0, 90, 0); // Yaw to look toward +Y
		break;

	case EViewDirection::Right:
		// Looking from right side (+Y): camera sees XZ extent
		ViewDistance = FMath::Max(Extent.X, Extent.Z) / HalfFOVTangent;
		CameraDirection = FVector(0, 1, 0); // Camera is to the right (+Y)
		CameraRotation = FRotator(0, -90, 0); // Yaw to look toward -Y
		break;

	case EViewDirection::Front:
		// Looking from front (+X): camera sees YZ extent
		ViewDistance = FMath::Max(Extent.Y, Extent.Z) / HalfFOVTangent;
		CameraDirection = FVector(1, 0, 0); // Camera is in front (+X)
		CameraRotation = FRotator(0, 180, 0); // Yaw to look toward -X
		break;

	case EViewDirection::Back:
		// Looking from back (-X): camera sees YZ extent
		ViewDistance = FMath::Max(Extent.Y, Extent.Z) / HalfFOVTangent;
		CameraDirection = FVector(-1, 0, 0); // Camera is behind (-X)
		CameraRotation = FRotator(0, 0, 0); // Yaw to look toward +X
		break;
	}

	// Apply padding multiplier
	ViewDistance *= FMath::Max(PaddingMultiplier, 0.5f);

	// Ensure minimum distance
	ViewDistance = FMath::Max(ViewDistance, 500.0f);

	Result.bSuccess = true;
	Result.CameraLocation = Origin + CameraDirection * ViewDistance;
	Result.CameraRotation = CameraRotation;
	Result.ViewDirection = Direction;
	Result.ActorCenter = Origin;
	Result.ActorExtent = Extent;
	Result.ViewDistance = ViewDistance;

	return Result;
}

bool UActorService::SetViewportCamera(FVector Location, FRotator Rotation)
{
	FLevelEditorViewportClient* ViewportClient = GetPerspectiveViewportClient();
	if (!ViewportClient) return false;

	ViewportClient->SetViewLocation(Location);
	ViewportClient->SetViewRotation(Rotation);

	RefreshViewport();
	return true;
}

FCameraViewInfo UActorService::GetActorViewCamera(
	const FString& ActorNameOrLabel,
	EViewDirection Direction,
	float PaddingMultiplier)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	FCameraViewInfo ViewInfo = CalculateViewForActor(Actor, Direction, PaddingMultiplier);

	if (ViewInfo.bSuccess)
	{
		// Apply the calculated camera position to the viewport
		SetViewportCamera(ViewInfo.CameraLocation, ViewInfo.CameraRotation);
	}

	return ViewInfo;
}

FCameraViewInfo UActorService::CalculateActorView(
	const FString& ActorNameOrLabel,
	EViewDirection Direction,
	float PaddingMultiplier)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	return CalculateViewForActor(Actor, Direction, PaddingMultiplier);
}

// =================================================================
// Property Operations
// =================================================================

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
