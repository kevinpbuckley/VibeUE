// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/EnhancedInput/Types/EnhancedInputTypes.h"
#include "Core/Result.h"
#include "UObject/Class.h"

// Forward declarations
class UInputAction;
class UInputMappingContext;

/**
 * Enhanced Input Reflection Service
 * 
 * Provides reflection-based discovery and metadata extraction for Enhanced Input System.
 * 
 * CRITICAL DESIGN PRINCIPLE: ZERO HARDCODING
 * - All types discovered dynamically via UE5 reflection
 * - No hardcoded type lists or property names
 * - All properties configurable through reflection
 * - Automatic extensibility for custom types
 * 
 * Core Capabilities:
 * - Type discovery (Input Actions, Modifiers, Triggers, Mapping Contexts)
 * - Dynamic property reflection (no hardcoded property lists)
 * - Asset Registry integration for asset discovery
 * - Type validation through reflection
 * - Generic property access patterns
 */
class VIBEUE_API FEnhancedInputReflectionService : public FServiceBase
{
public:
	explicit FEnhancedInputReflectionService(TSharedPtr<FServiceContext> Context);
	
	virtual ~FEnhancedInputReflectionService() = default;
	
	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("EnhancedInputReflectionService"); }
	virtual void Initialize() override;
	virtual void Shutdown() override;
	
	// ═══════════════════════════════════════════════════════════════════
	// Type Discovery (Reflection-Based, Zero Hardcoding)
	// ═══════════════════════════════════════════════════════════════════
	
	/**
	 * Discover all available Input Action types
	 * 
	 * @return Result containing array of Input Action type information
	 */
	TResult<TArray<FEnhancedInputTypeInfo>> DiscoverInputActionTypes();
	
	/**
	 * Discover all available Modifier types
	 * 
	 * @param Criteria Optional search criteria to filter results
	 * @return Result containing array of modifier type information
	 */
	TResult<TArray<FEnhancedInputModifierInfo>> DiscoverModifierTypes(
		const FEnhancedInputTypeSearchCriteria& Criteria = FEnhancedInputTypeSearchCriteria());
	
	/**
	 * Discover all available Trigger types
	 * 
	 * @param Criteria Optional search criteria to filter results
	 * @return Result containing array of trigger type information
	 */
	TResult<TArray<FEnhancedInputTriggerInfo>> DiscoverTriggerTypes(
		const FEnhancedInputTypeSearchCriteria& Criteria = FEnhancedInputTypeSearchCriteria());
	
	/**
	 * Discover all available Input Mapping Context types
	 * 
	 * @return Result containing array of mapping context type information
	 */
	TResult<TArray<FEnhancedInputTypeInfo>> DiscoverMappingContextTypes();
	
	// ═══════════════════════════════════════════════════════════════════
	// Asset Discovery (Asset Registry Integration)
	// ═══════════════════════════════════════════════════════════════════
	
	/**
	 * Discover Input Action assets in the project
	 * 
	 * @param Criteria Search criteria for asset discovery
	 * @return Result containing discovered assets
	 */
	TResult<FEnhancedInputDiscoveryResult> DiscoverInputActionAssets(
		const FEnhancedInputAssetSearchCriteria& Criteria);
	
	/**
	 * Discover Input Mapping Context assets in the project
	 * 
	 * @param Criteria Search criteria for asset discovery
	 * @return Result containing discovered assets
	 */
	TResult<FEnhancedInputDiscoveryResult> DiscoverMappingContextAssets(
		const FEnhancedInputAssetSearchCriteria& Criteria);
	
	// ═══════════════════════════════════════════════════════════════════
	// Generic Property Reflection
	// ═══════════════════════════════════════════════════════════════════
	
	/**
	 * Get all properties for a class via reflection
	 * 
	 * Uses reflection to dynamically discover all available properties.
	 * No hardcoded property lists.
	 * 
	 * @param Class The class to reflect on
	 * @return Result containing array of property info
	 */
	TResult<TArray<FEnhancedInputPropertyInfo>> GetClassProperties(UClass* Class);
	
	/**
	 * Get property metadata by name
	 * 
	 * @param Class The class to reflect on
	 * @param PropertyName The property name to retrieve metadata for
	 * @return Result containing property metadata
	 */
	TResult<FEnhancedInputPropertyInfo> GetPropertyInfo(UClass* Class, const FString& PropertyName);
	
	/**
	 * Get property value from an object via reflection
	 * 
	 * @param Object The object instance
	 * @param PropertyName The property name
	 * @return Result containing property value as string
	 */
	TResult<FString> GetPropertyValue(UObject* Object, const FString& PropertyName);
	
	/**
	 * Set property value on an object via reflection
	 * 
	 * @param Object The object instance
	 * @param PropertyName The property name
	 * @param Value The value to set (as string)
	 * @return Result indicating success or failure
	 */
	TResult<void> SetPropertyValue(UObject* Object, const FString& PropertyName, const FString& Value);
	
	// ═══════════════════════════════════════════════════════════════════
	// Type Validation
	// ═══════════════════════════════════════════════════════════════════
	
	/**
	 * Validate if a class path represents a valid Input Action type
	 * 
	 * @param ClassPath The class path to validate (e.g., "/Script/EnhancedInput.InputAction")
	 * @return Result containing true if valid
	 */
	TResult<bool> ValidateInputActionType(const FString& ClassPath);
	
	/**
	 * Validate if a class path represents a valid Modifier type
	 * 
	 * @param ClassPath The class path to validate
	 * @return Result containing true if valid
	 */
	TResult<bool> ValidateModifierType(const FString& ClassPath);
	
	/**
	 * Validate if a class path represents a valid Trigger type
	 * 
	 * @param ClassPath The class path to validate
	 * @return Result containing true if valid
	 */
	TResult<bool> ValidateTriggerType(const FString& ClassPath);
	
	/**
	 * Validate if a property exists on a class
	 * 
	 * @param Class The class to check
	 * @param PropertyName The property name
	 * @return Result containing true if property exists
	 */
	TResult<bool> ValidatePropertyExists(UClass* Class, const FString& PropertyName);
	
	// ═══════════════════════════════════════════════════════════════════
	// Type Resolution and Lookup
	// ═══════════════════════════════════════════════════════════════════
	
	/**
	 * Resolve a class path string to an actual UClass pointer
	 * 
	 * @param ClassPath The class path (e.g., "/Script/EnhancedInput.InputAction")
	 * @return Result containing the resolved UClass pointer
	 */
	TResult<UClass*> ResolveClass(const FString& ClassPath);
	
	/**
	 * Get the Enhanced Input Plugin's root path
	 * 
	 * @return Result containing the plugin root path
	 */
	TResult<FString> GetEnhancedInputPluginPath();
	
	/**
	 * Check if Enhanced Input Plugin is available
	 * 
	 * @return Result containing true if plugin is loaded
	 */
	TResult<bool> IsEnhancedInputAvailable();

protected:
	// Cache for discovered types
	TMap<FString, FEnhancedInputTypeInfo> TypeCache;
	TMap<FString, TArray<FEnhancedInputPropertyInfo>> PropertyCache;
	
	/**
	 * Cache a discovered type
	 */
	void CacheType(const FString& Key, const FEnhancedInputTypeInfo& TypeInfo);
	
	/**
	 * Get cached type or discover if not cached
	 */
	TResult<FEnhancedInputTypeInfo> GetOrDiscoverType(const FString& ClassPath);
	
	/**
	 * Clear all caches
	 */
	void ClearCache();
};
