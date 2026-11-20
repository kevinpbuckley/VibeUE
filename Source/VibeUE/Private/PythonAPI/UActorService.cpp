// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UActorService.h"
#include "Utils/VibeUEToolError.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "ScopedTransaction.h"

AActor* UActorService::FindActorByIdentifier(const FString& NameOrLabel)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World || NameOrLabel.IsEmpty())
	{
		return nullptr;
	}

	const FString LowerSearch = NameOrLabel.ToLower();

	// Exact label, then exact object name.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		if (Actor->GetActorLabel().ToLower() == LowerSearch || Actor->GetName().ToLower() == LowerSearch)
		{
			return Actor;
		}
	}

	// Then a substring match, so a partial label still resolves.
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		if (Actor->GetActorLabel().ToLower().Contains(LowerSearch) ||
			Actor->GetName().ToLower().Contains(LowerSearch))
		{
			return Actor;
		}
	}

	return nullptr;
}

bool UActorService::RerunConstructionScripts(const FString& ActorLabelOrPath)
{
	// Editor world only — refuse during PIE. RerunConstructionScripts on a play-world actor would
	// stomp gameplay state; the caller wants the editor placement re-evaluated.
	if (GEditor && GEditor->PlayWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("RerunConstructionScripts: refused '%s' — a Play-In-Editor session is running; stop PIE first."), *ActorLabelOrPath);
		return false;
	}

	AActor* Actor = FindActorByIdentifier(ActorLabelOrPath);
	if (!Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("RerunConstructionScripts: actor '%s' not found in the editor world"), *ActorLabelOrPath);
		return false;
	}

	Actor->RerunConstructionScripts();
	UE_LOG(LogTemp, Log, TEXT("RerunConstructionScripts: re-ran construction scripts for '%s'"), *Actor->GetActorLabel());
	return true;
}

bool UActorService::SetActorLockLocation(const FString& ActorNameOrLabel, bool bLocked)
{
	AActor* Actor = FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) return false;

	FBoolProperty* Prop = CastField<FBoolProperty>(AActor::StaticClass()->FindPropertyByName(TEXT("bLockLocation")));
	if (!Prop) return false;

	const FScopedTransaction Transaction(FText::FromString(TEXT("Set Actor Lock Location")));

	Actor->Modify();
	Prop->SetPropertyValue_InContainer(Actor, bLocked);
	Actor->PostEditChange();


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

// =================================================================================================
// Reflected forms (Python and MCP) of the functions that hand their result back through a reference.
// Epic's ToolsetRegistry publishes an output reference parameter as a required INPUT of the MCP tool
// and returns only the return value, so each of these returns its result instead, and fails the way
// Epic's own toolsets do: with a script error (a Python exception, an MCP tool error). Issue #687.
// =================================================================================================

bool UActorService::GetActorLockLocation(const FString& ActorNameOrLabel)
{
	bool Result = false;
	const VibeUE::FToolFailureLog FailureLog;
	if (!GetActorLockLocation(ActorNameOrLabel, Result))
	{
		VibeUE::RaiseToolError(TEXT("GetActorLockLocation"), FString::Printf(TEXT("no actor named or labelled '%s' was found in the editor level"), *ActorNameOrLabel), FailureLog);
	}
	return Result;
}
