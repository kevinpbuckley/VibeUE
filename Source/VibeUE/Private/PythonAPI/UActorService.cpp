// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "PythonAPI/UActorService.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"

namespace
{
	UWorld* GetEditorWorld()
	{
		if (GEditor)
		{
			return GEditor->GetEditorWorldContext().World();
		}
		return nullptr;
	}

	void PopulateActorInfo(AActor* Actor, FLevelActorInfo& OutInfo)
	{
		if (!Actor)
		{
			return;
		}

		OutInfo.ActorName = Actor->GetName();
		OutInfo.ActorLabel = Actor->GetActorLabel();
		OutInfo.ActorClass = Actor->GetClass()->GetName();
		OutInfo.Location = Actor->GetActorLocation();
		OutInfo.Rotation = Actor->GetActorRotation();
		OutInfo.bIsHidden = Actor->IsHidden();
	}
}

TArray<FLevelActorInfo> UActorService::ListLevelActors(const FString& ActorClassFilter, bool bIncludeHidden)
{
	TArray<FLevelActorInfo> Actors;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UActorService::ListLevelActors: No editor world found"));
		return Actors;
	}

	FString LowerFilter = ActorClassFilter.ToLower();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		// Skip hidden actors if requested
		if (!bIncludeHidden && Actor->IsHidden())
		{
			continue;
		}

		// Apply class filter
		if (!ActorClassFilter.IsEmpty())
		{
			FString ClassName = Actor->GetClass()->GetName().ToLower();
			if (!ClassName.Contains(LowerFilter))
			{
				continue;
			}
		}

		FLevelActorInfo Info;
		PopulateActorInfo(Actor, Info);
		Actors.Add(Info);
	}

	return Actors;
}

TArray<FLevelActorInfo> UActorService::FindActorsByClass(const FString& ClassName)
{
	return ListLevelActors(ClassName, false);
}

bool UActorService::GetActorInfo(const FString& ActorNameOrLabel, FLevelActorInfo& OutInfo)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return false;
	}

	FString LowerSearch = ActorNameOrLabel.ToLower();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		FString ActorName = Actor->GetName().ToLower();
		FString ActorLabel = Actor->GetActorLabel().ToLower();

		if (ActorName.Contains(LowerSearch) || ActorLabel.Contains(LowerSearch))
		{
			PopulateActorInfo(Actor, OutInfo);
			return true;
		}
	}

	return false;
}
