// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/LevelActor/Types/LevelActorTypes.h"

class AActor;
class UWorld;
class ULevel;

/**
 * Level Actor Service - Phases 1-4
 * 
 * Core actor management operations for level design.
 * Uses editor APIs (not runtime spawning) for full undo/redo support.
 * 
 * Phase 1 Actions: add, remove, list, find, get_info
 * Phase 2 Actions: set_transform, get_transform, set_location, set_rotation, set_scale
 * Phase 3 Actions: get_property, set_property, get_all_properties
 * Phase 4 Actions: set_folder, attach, detach, select, rename
 */
class VIBEUE_API FLevelActorService
{
public:
	FLevelActorService();
	~FLevelActorService();

	// ═══════════════════════════════════════════════════════════════════
	// Phase 1: Core Actor Operations
	// ═══════════════════════════════════════════════════════════════════

	/** Add a new actor to the current level */
	FActorOperationResult AddActor(const FActorAddParams& Params);

	/** Remove an actor from the level */
	FActorOperationResult RemoveActor(const FActorIdentifier& Identifier, bool bWithUndo = true);

	/** List all actors in the current level with optional filtering */
	FActorOperationResult ListActors(const FActorQueryCriteria& Criteria = FActorQueryCriteria());

	/** Find actors matching specific criteria */
	FActorOperationResult FindActors(const FActorQueryCriteria& Criteria);

	/** Get comprehensive information about a specific actor */
	FActorOperationResult GetActorInfo(
		const FActorIdentifier& Identifier,
		bool bIncludeComponents = true,
		bool bIncludeProperties = true,
		const FString& CategoryFilter = TEXT(""));

	// ═══════════════════════════════════════════════════════════════════
	// Phase 2: Transform Operations
	// ═══════════════════════════════════════════════════════════════════

	/** Set complete transform (location, rotation, scale) */
	FActorOperationResult SetTransform(const FActorTransformParams& Params);

	/** Get comprehensive transform information */
	FActorOperationResult GetTransform(const FActorIdentifier& Identifier);

	/** Set actor location only */
	FActorOperationResult SetLocation(
		const FActorIdentifier& Identifier,
		const FVector& Location,
		bool bWorldSpace = true,
		bool bSweep = false);

	/** Set actor rotation only */
	FActorOperationResult SetRotation(
		const FActorIdentifier& Identifier,
		const FRotator& Rotation,
		bool bWorldSpace = true);

	/** Set actor scale only */
	FActorOperationResult SetScale(
		const FActorIdentifier& Identifier,
		const FVector& Scale);

	// ═══════════════════════════════════════════════════════════════════
	// Editor View Operations
	// ═══════════════════════════════════════════════════════════════════

	/** Focus the viewport camera on an actor */
	FActorOperationResult FocusActor(const FActorIdentifier& Identifier, bool bInstant = false);

	/** Move an actor to the center of the current viewport */
	FActorOperationResult MoveActorToView(const FActorIdentifier& Identifier);

	/** Force refresh of all level editing viewports */
	FActorOperationResult RefreshViewport();

	// ═══════════════════════════════════════════════════════════════════
	// Phase 3: Property Operations
	// ═══════════════════════════════════════════════════════════════════

	/** Get a single property value from an actor or component */
	FActorOperationResult GetProperty(const FActorPropertyParams& Params);

	/** Set a single property value on an actor or component */
	FActorOperationResult SetProperty(const FActorPropertyParams& Params);

	/** Get all properties from an actor with optional filtering */
	FActorOperationResult GetAllProperties(const FActorPropertyParams& Params);

	// ═══════════════════════════════════════════════════════════════════
	// Phase 4: Hierarchy & Organization
	// ═══════════════════════════════════════════════════════════════════

	/** Set the folder path for an actor in World Outliner */
	FActorOperationResult SetFolder(const FActorIdentifier& Identifier, const FString& FolderPath);

	/** Attach an actor to a parent actor */
	FActorOperationResult AttachActor(const FActorAttachParams& Params);

	/** Detach an actor from its parent */
	FActorOperationResult DetachActor(const FActorIdentifier& Identifier);

	/** Select or deselect actors in the editor */
	FActorOperationResult SelectActors(const FActorSelectParams& Params);

	/** Rename an actor's label */
	FActorOperationResult RenameActor(const FActorIdentifier& Identifier, const FString& NewLabel);

private:
	/** Get the current editor world */
	UWorld* GetEditorWorld() const;

	/** Find an actor by identifier */
	AActor* FindActorByIdentifier(const FActorIdentifier& Identifier) const;

	/** Find actor class by name or path */
	UClass* FindActorClass(const FString& ClassNameOrPath) const;

	/** Build FActorInfo from an actor */
	FActorInfo BuildActorInfo(AActor* Actor, bool bIncludeComponents, bool bIncludeProperties, const FString& CategoryFilter) const;

	/** Build FActorComponentInfo from a component */
	FActorComponentInfo BuildComponentInfo(UActorComponent* Component, bool bIncludeProperties, const FString& CategoryFilter) const;

	/** Get all properties from an object via reflection */
	TArray<FActorPropertyInfo> GetObjectProperties(UObject* Object, bool bIncludeInherited, const FString& CategoryFilter) const;

	/** Get a property value as string */
	FString GetPropertyValueAsString(UObject* Object, FProperty* Property) const;

	/** Check if an actor matches query criteria */
	bool MatchesCriteria(AActor* Actor, const FActorQueryCriteria& Criteria) const;

	/** Check if a string matches a wildcard filter */
	bool MatchesWildcard(const FString& Value, const FString& Pattern) const;

	/** Begin a scoped transaction for undo support */
	void BeginTransaction(const FText& Description);

	/** End the current transaction */
	void EndTransaction();
};
