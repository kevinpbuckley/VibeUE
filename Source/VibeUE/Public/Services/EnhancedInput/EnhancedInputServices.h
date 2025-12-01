// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Services/EnhancedInput/Types/EnhancedInputTypes.h"
#include "Core/Result.h"

class FEnhancedInputReflectionService;

/**
 * Enhanced Input Validation Service
 * 
 * Provides reflection-based validation for Enhanced Input configurations.
 * Ensures all property values and type combinations are valid before creation.
 * 
 * KEY FEATURE: All validation is reflection-based
 * - No hardcoded validation rules
 * - Validates against actual UE5 property constraints
 * - Supports custom types automatically
 */
class VIBEUE_API FEnhancedInputValidationService : public FServiceBase
{
public:
	explicit FEnhancedInputValidationService(TSharedPtr<FServiceContext> Context);
	
	virtual ~FEnhancedInputValidationService() = default;
	
	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("EnhancedInputValidationService"); }
	virtual void Initialize() override;
	
	/**
	 * Validate Input Action configuration
	 * 
	 * @param ActionName Name for the input action
	 * @param ValueType Type of input (e.g., "Value1D", "Value2D", "Digital")
	 * @return Result indicating validation status
	 */
	TResult<void> ValidateInputActionConfig(const FString& ActionName, const FString& ValueType);
	
	/**
	 * Validate Mapping Context configuration
	 * 
	 * @param ContextName Name for the mapping context
	 * @param Priority Priority value
	 * @return Result indicating validation status
	 */
	TResult<void> ValidateMappingContextConfig(const FString& ContextName, int32 Priority);
	
	/**
	 * Validate Modifier configuration
	 * 
	 * @param ModifierClass Class path of the modifier
	 * @param Properties Properties to set on the modifier
	 * @return Result indicating validation status
	 */
	TResult<void> ValidateModifierConfig(const FString& ModifierClass, const TMap<FString, FString>& Properties);
	
	/**
	 * Validate Trigger configuration
	 * 
	 * @param TriggerClass Class path of the trigger
	 * @param Properties Properties to set on the trigger
	 * @return Result indicating validation status
	 */
	TResult<void> ValidateTriggerConfig(const FString& TriggerClass, const TMap<FString, FString>& Properties);
	
	/**
	 * Validate property assignment
	 * 
	 * @param Class The class containing the property
	 * @param PropertyName The property name
	 * @param Value The value to validate
	 * @return Result indicating if value is valid for the property
	 */
	TResult<void> ValidatePropertyAssignment(UClass* Class, const FString& PropertyName, const FString& Value);

private:
	TSharedPtr<FEnhancedInputReflectionService> ReflectionService;
};

/**
 * Enhanced Input Asset Management Service
 * 
 * Manages Enhanced Input asset lifecycle and Asset Registry integration.
 * 
 * Responsibilities:
 * - Asset creation with validation
 * - Asset modification and configuration
 * - Asset deletion with cleanup
 * - Asset Registry synchronization
 * - Dependency management
 */
class VIBEUE_API FEnhancedInputAssetService : public FServiceBase
{
public:
	explicit FEnhancedInputAssetService(TSharedPtr<FServiceContext> Context);
	
	virtual ~FEnhancedInputAssetService() = default;
	
	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("EnhancedInputAssetService"); }
	virtual void Initialize() override;
	
	/**
	 * Create a new Input Action asset
	 * 
	 * @param AssetPath Path where to create the asset (e.g., "/Game/Input/IA_Move")
	 * @param ValueType Type of input (e.g., "Value1D", "Value2D", "Digital")
	 * @return Result containing the created asset path
	 */
	TResult<FString> CreateInputAction(const FString& AssetPath, const FString& ValueType);
	
	/**
	 * Create a new Input Mapping Context asset
	 * 
	 * @param AssetPath Path where to create the asset
	 * @return Result containing the created asset path
	 */
	TResult<FString> CreateInputMappingContext(const FString& AssetPath);
	
	/**
	 * Delete an Enhanced Input asset
	 * 
	 * @param AssetPath Path of the asset to delete
	 * @return Result indicating success or failure
	 */
	TResult<void> DeleteAsset(const FString& AssetPath);
	
	/**
	 * Load an asset from disk
	 * 
	 * @param AssetPath Path of the asset to load
	 * @return Result containing the loaded UObject
	 */
	TResult<UObject*> LoadAsset(const FString& AssetPath);
	
	/**
	 * Save an asset to disk
	 * 
	 * @param Asset The asset to save
	 * @return Result indicating success or failure
	 */
	TResult<void> SaveAsset(UObject* Asset);

private:
	TSharedPtr<FEnhancedInputReflectionService> ReflectionService;
	TSharedPtr<FEnhancedInputValidationService> ValidationService;
};

/**
 * Enhanced Input Discovery Service
 * 
 * Integrates with Asset Registry to discover Enhanced Input assets.
 * 
 * Responsibilities:
 * - Input Action asset discovery
 * - Input Mapping Context discovery
 * - Modifier/Trigger type discovery
 * - Filtering and search
 */
class VIBEUE_API FEnhancedInputDiscoveryService : public FServiceBase
{
public:
	explicit FEnhancedInputDiscoveryService(TSharedPtr<FServiceContext> Context);
	
	virtual ~FEnhancedInputDiscoveryService() = default;
	
	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("EnhancedInputDiscoveryService"); }
	virtual void Initialize() override;
	
	/**
	 * Find Input Action assets
	 * 
	 * @param Criteria Search criteria
	 * @return Result containing discovered asset paths
	 */
	TResult<TArray<FString>> FindInputActions(const FEnhancedInputAssetSearchCriteria& Criteria);
	
	/**
	 * Find Input Mapping Context assets
	 * 
	 * @param Criteria Search criteria
	 * @return Result containing discovered asset paths
	 */
	TResult<TArray<FString>> FindMappingContexts(const FEnhancedInputAssetSearchCriteria& Criteria);
	
	/**
	 * Get all available modifier types
	 * 
	 * @return Result containing available modifier types
	 */
	TResult<TArray<FString>> GetAvailableModifiers();
	
	/**
	 * Get all available trigger types
	 * 
	 * @return Result containing available trigger types
	 */
	TResult<TArray<FString>> GetAvailableTriggers();

private:
	TSharedPtr<FEnhancedInputReflectionService> ReflectionService;
};
