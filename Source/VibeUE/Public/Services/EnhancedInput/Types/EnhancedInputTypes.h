// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

/**
 * Enhanced Input System Type Definitions
 * 
 * Data structures for Enhanced Input reflection and asset management.
 * All types are populated via reflection discovery.
 */

// ═══════════════════════════════════════════════════════════════════
// Type Information Structures
// ═══════════════════════════════════════════════════════════════════

/**
 * @struct FEnhancedInputTypeInfo
 * @brief Metadata about an Enhanced Input type discovered via reflection
 */
struct FEnhancedInputTypeInfo
{
	/** Full class path (e.g., "/Script/EnhancedInput.InputAction") */
	FString ClassPath;
	
	/** Display name for the type */
	FString DisplayName;
	
	/** Whether this type can be instantiated as an asset */
	bool bCanCreateAsset = false;
	
	/** Whether this type can be used as a component */
	bool bCanCreateComponent = false;
	
	/** Category this type belongs to */
	FString Category;
	
	/** Description of the type */
	FString Description;
};

/**
 * @struct FEnhancedInputPropertyInfo
 * @brief Metadata about an Enhanced Input property
 */
struct FEnhancedInputPropertyInfo
{
	/** Property name */
	FString Name;
	
	/** Display name for the property */
	FString DisplayName;
	
	/** Property type name */
	FString TypeName;
	
	/** Full property type path */
	FString TypePath;
	
	/** Category this property belongs to */
	FString Category;
	
	/** Property description/tooltip */
	FString Description;
	
	/** Whether this property is read-only */
	bool bReadOnly = false;
	
	/** Whether this property is an array */
	bool bIsArray = false;
	
	/** Default value as string */
	FString DefaultValue;
	
	/** Min value (for numeric types) */
	double MinValue = 0.0;
	
	/** Max value (for numeric types) */
	double MaxValue = 0.0;
};

/**
 * @struct FEnhancedInputModifierInfo
 * @brief Metadata about an Input Modifier type
 */
struct FEnhancedInputModifierInfo
{
	/** Modifier class path */
	FString ClassPath;
	
	/** Display name */
	FString DisplayName;
	
	/** Description */
	FString Description;
	
	/** Category (e.g., "Deadzone", "Scaling", "Smoothing") */
	FString Category;
	
	/** Available properties for this modifier */
	TArray<FEnhancedInputPropertyInfo> Properties;
};

/**
 * @struct FEnhancedInputTriggerInfo
 * @brief Metadata about an Input Trigger type
 */
struct FEnhancedInputTriggerInfo
{
	/** Trigger class path */
	FString ClassPath;
	
	/** Display name */
	FString DisplayName;
	
	/** Description */
	FString Description;
	
	/** Category (e.g., "Duration", "Chord", "Tap") */
	FString Category;
	
	/** Available properties for this trigger */
	TArray<FEnhancedInputPropertyInfo> Properties;
};

/**
 * @struct FEnhancedInputActionInfo
 * @brief Metadata about an Input Action asset
 */
struct FEnhancedInputActionInfo
{
	/** Asset path (e.g., "/Game/Input/IA_Move") */
	FString AssetPath;
	
	/** Asset name */
	FString AssetName;
	
	/** Value type (e.g., "Value1D", "Value2D", "Digital") */
	FString ValueType;
	
	/** Whether this is a digital action */
	bool bIsDigital = false;
	
	/** Associated modifiers */
	TArray<FEnhancedInputModifierInfo> Modifiers;
	
	/** Associated triggers */
	TArray<FEnhancedInputTriggerInfo> Triggers;
};

/**
 * @struct FEnhancedInputMappingInfo
 * @brief Metadata about an Input Mapping Context asset
 */
struct FEnhancedInputMappingInfo
{
	/** Asset path */
	FString AssetPath;
	
	/** Asset name */
	FString AssetName;
	
	/** Associated input actions and their mappings */
	TArray<FEnhancedInputActionInfo> ActionMappings;
	
	/** Priority value */
	int32 Priority = 0;
};

/**
 * @struct FEnhancedInputDiscoveryResult
 * @brief Result of asset discovery operation
 */
struct FEnhancedInputDiscoveryResult
{
	/** Discovered asset paths */
	TArray<FString> AssetPaths;
	
	/** Discovered asset names */
	TArray<FString> AssetNames;
	
	/** Total count of discovered assets */
	int32 TotalCount = 0;
};

// ═══════════════════════════════════════════════════════════════════
// Search/Filter Criteria
// ═══════════════════════════════════════════════════════════════════

/**
 * @struct FEnhancedInputTypeSearchCriteria
 * @brief Criteria for searching Enhanced Input types
 */
struct FEnhancedInputTypeSearchCriteria
{
	/** Search text to match in type names */
	FString SearchText;
	
	/** Filter by category */
	FString Category;
	
	/** Filter by base class */
	FString BaseClass;
	
	/** Include abstract types */
	bool bIncludeAbstract = false;
	
	/** Include deprecated types */
	bool bIncludeDeprecated = false;
};

/**
 * @struct FEnhancedInputAssetSearchCriteria
 * @brief Criteria for searching Enhanced Input assets
 */
struct FEnhancedInputAssetSearchCriteria
{
	/** Search path (default: /Game) */
	FString SearchPath = TEXT("/Game");
	
	/** Search text to match in asset names */
	FString SearchText;
	
	/** Filter by asset type (e.g., "InputAction", "InputMappingContext") */
	FString AssetType;
	
	/** Include only digital actions */
	bool bDigitalOnly = false;
	
	/** Maximum results to return */
	int32 MaxResults = 100;
	
	/** Case sensitive search */
	bool bCaseSensitive = false;
};
