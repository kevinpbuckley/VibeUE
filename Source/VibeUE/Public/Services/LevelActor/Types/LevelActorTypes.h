// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Level Actor Type Definitions - Phases 1-4
 * 
 * Core data structures for level actor management.
 * Phase 1: add, remove, list, find, get_info
 * Phase 2: set_transform, get_transform, set_location, set_rotation, set_scale
 * Phase 3: get_property, set_property, get_all_properties
 * Phase 4: set_folder, attach, detach, select, rename
 */

// ═══════════════════════════════════════════════════════════════════
// Property Information
// ═══════════════════════════════════════════════════════════════════

/**
 * @struct FActorPropertyInfo
 * @brief Information about a single actor property
 */
struct VIBEUE_API FActorPropertyInfo
{
	FString Name;
	FString TypeName;
	FString Category;
	FString CurrentValue;
	FString PropertyPath;
	bool bIsEditable = true;
	
	TSharedPtr<FJsonObject> ToJson() const;
};

/**
 * @struct FActorComponentInfo
 * @brief Information about a component attached to an actor
 */
struct VIBEUE_API FActorComponentInfo
{
	FString Name;
	FString ClassName;
	FString ParentName;
	bool bIsRoot = false;
	FVector RelativeLocation = FVector::ZeroVector;
	FRotator RelativeRotation = FRotator::ZeroRotator;
	FVector RelativeScale = FVector::OneVector;
	TArray<FActorPropertyInfo> Properties;
	
	TSharedPtr<FJsonObject> ToJson() const;
};

// ═══════════════════════════════════════════════════════════════════
// Actor Information
// ═══════════════════════════════════════════════════════════════════

/**
 * @struct FActorInfo
 * @brief Information about a level actor
 */
struct VIBEUE_API FActorInfo
{
	FString ActorPath;
	FString ActorLabel;
	FString ActorGuid;
	FString ClassName;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale = FVector::OneVector;
	TArray<FName> Tags;
	bool bIsSelected = false;
	bool bIsHidden = false;
	FString FolderPath;
	TArray<FActorPropertyInfo> Properties;
	TArray<FActorComponentInfo> Components;
	
	TSharedPtr<FJsonObject> ToJson() const;
	TSharedPtr<FJsonObject> ToMinimalJson() const;
};

/**
 * @struct FActorIdentifier
 * @brief Ways to identify an actor
 */
struct VIBEUE_API FActorIdentifier
{
	FString ActorPath;
	FString ActorLabel;
	FString ActorGuid;
	FString ActorTag;
	
	bool IsValid() const
	{
		return !ActorPath.IsEmpty() || !ActorLabel.IsEmpty() || 
		       !ActorGuid.IsEmpty() || !ActorTag.IsEmpty();
	}
	
	static FActorIdentifier FromJson(const TSharedPtr<FJsonObject>& Params);
};

// ═══════════════════════════════════════════════════════════════════
// Query Criteria
// ═══════════════════════════════════════════════════════════════════

/**
 * @struct FActorQueryCriteria
 * @brief Criteria for finding/filtering actors
 */
struct VIBEUE_API FActorQueryCriteria
{
	FString ClassFilter;
	FString LabelFilter;
	TArray<FString> RequiredTags;
	TArray<FString> ExcludedTags;
	bool bSelectedOnly = false;
	int32 MaxResults = 100;
	
	static FActorQueryCriteria FromJson(const TSharedPtr<FJsonObject>& Params);
};

// ═══════════════════════════════════════════════════════════════════
// Add Actor Parameters
// ═══════════════════════════════════════════════════════════════════

/**
 * @struct FActorAddParams
 * @brief Parameters for adding an actor
 */
struct VIBEUE_API FActorAddParams
{
	FString ActorClass;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale = FVector::OneVector;
	FString ActorName;
	TArray<FString> Tags;
	bool bLocationProvided = false;  // If false, spawn at viewport center
	
	static FActorAddParams FromJson(const TSharedPtr<FJsonObject>& Params);
};

// ═══════════════════════════════════════════════════════════════════
// Operation Result
// ═══════════════════════════════════════════════════════════════════

// Forward declare Phase 2 types for FActorOperationResult
struct FActorTransformInfo;

/**
 * @struct FActorOperationResult
 * @brief Result of an actor operation
 */
struct VIBEUE_API FActorOperationResult
{
	bool bSuccess = false;
	FString ErrorMessage;
	FString ErrorCode;
	TOptional<FActorInfo> ActorInfo;
	TArray<FActorInfo> AffectedActors;
	TSharedPtr<FJsonObject> TransformInfo;  // Phase 2: Optional transform data
	TSharedPtr<FJsonObject> CustomJson;     // For minimal responses (e.g., set_property)
	
	static FActorOperationResult Success(const FActorInfo& Info);
	static FActorOperationResult Success(const TArray<FActorInfo>& Actors);
	static FActorOperationResult SuccessWithJson(TSharedPtr<FJsonObject> Json);
	static FActorOperationResult Error(const FString& Code, const FString& Message);
	
	TSharedPtr<FJsonObject> ToJson() const;
};

// ═══════════════════════════════════════════════════════════════════
// Phase 2: Transform Parameters
// ═══════════════════════════════════════════════════════════════════

/**
 * @struct FActorTransformParams
 * @brief Parameters for transform operations
 */
struct VIBEUE_API FActorTransformParams
{
	FActorIdentifier Identifier;
	TOptional<FVector> Location;
	TOptional<FRotator> Rotation;
	TOptional<FVector> Scale;
	bool bWorldSpace = true;  // If false, relative to parent
	bool bSweep = false;      // Test for collision during move
	bool bTeleport = true;    // If sweep, teleport on hit
	
	static FActorTransformParams FromJson(const TSharedPtr<FJsonObject>& Params);
};

/**
 * @struct FActorTransformInfo
 * @brief Transform information returned from get_transform
 */
struct VIBEUE_API FActorTransformInfo
{
	FVector WorldLocation = FVector::ZeroVector;
	FRotator WorldRotation = FRotator::ZeroRotator;
	FVector WorldScale = FVector::OneVector;
	FVector RelativeLocation = FVector::ZeroVector;
	FRotator RelativeRotation = FRotator::ZeroRotator;
	FVector RelativeScale = FVector::OneVector;
	FVector Forward = FVector::ForwardVector;
	FVector Right = FVector::RightVector;
	FVector Up = FVector::UpVector;
	FBox BoundingBox;
	FVector Origin = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	
	TSharedPtr<FJsonObject> ToJson() const;
};

// ═══════════════════════════════════════════════════════════════════
// Phase 3: Property Parameters
// ═══════════════════════════════════════════════════════════════════

/**
 * @struct FActorPropertyParams
 * @brief Parameters for property get/set operations
 */
struct VIBEUE_API FActorPropertyParams
{
	FActorIdentifier Identifier;
	FString PropertyPath;       // Can include component path: "ComponentName.PropertyName"
	FString PropertyValue;      // For set_property - serialized value
	FString ComponentName;      // Optional: target a specific component
	bool bIncludeInherited = true;
	FString CategoryFilter;
	
	static FActorPropertyParams FromJson(const TSharedPtr<FJsonObject>& Params);
};

// ═══════════════════════════════════════════════════════════════════
// Phase 4: Hierarchy & Organization Parameters
// ═══════════════════════════════════════════════════════════════════

/**
 * @struct FActorAttachParams
 * @brief Parameters for attach/detach operations
 */
struct VIBEUE_API FActorAttachParams
{
	FActorIdentifier ChildIdentifier;
	FActorIdentifier ParentIdentifier;
	FString SocketName;
	bool bWeldSimulatedBodies = true;
	
	static FActorAttachParams FromJson(const TSharedPtr<FJsonObject>& Params);
};

/**
 * @struct FActorSelectParams
 * @brief Parameters for selection operations
 */
struct VIBEUE_API FActorSelectParams
{
	TArray<FActorIdentifier> Identifiers;
	bool bAddToSelection = false;   // If false, replace current selection
	bool bDeselect = false;         // If true, deselect the specified actors
	bool bDeselectAll = false;      // If true, deselect all actors
	
	static FActorSelectParams FromJson(const TSharedPtr<FJsonObject>& Params);
};
