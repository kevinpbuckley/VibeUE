// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"
#include "Core/ServiceContext.h"
#include "Services/EnhancedInput/Types/EnhancedInputTypes.h"
#include "InputTriggers.h"

class FEnhancedInputReflectionService;
class FInputDiscoveryService;
class FInputValidationService;

/**
 * Advanced Trigger Service
 * 
 * Provides comprehensive input trigger management with reflection-based discovery,
 * dynamic instantiation, advanced configuration, and optimization.
 * 
 * CRITICAL DESIGN PRINCIPLE: ZERO HARDCODING
 * - All trigger types discovered via reflection
 * - All properties accessed via reflection service
 * - No hardcoded trigger lists or property names
 * - Complete reliance on Phase 1 reflection infrastructure
 */
class VIBEUE_API FAdvancedTriggerService : public FServiceBase
{
public:
	explicit FAdvancedTriggerService(TSharedPtr<class FServiceContext> Context);
	virtual ~FAdvancedTriggerService() = default;

	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("AdvancedTriggerService"); }
	virtual void Initialize() override;
	virtual void Shutdown() override;

	// ═══════════════════════════════════════════════════════════════════
	// Trigger Type Discovery (Reflection-Based, Zero Hardcoding)
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Discover all available input trigger types via reflection
	 * @return Array of trigger type information
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Triggers")
	TResult<TArray<FEnhancedInputTypeInfo>> DiscoverTriggerTypes();

	/**
	 * Get comprehensive metadata for a specific trigger type
	 * @param TriggerClass - Full class path (e.g., "/Script/EnhancedInput.InputTriggerDown")
	 * @return Detailed trigger metadata with property information
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Triggers")
	TResult<FEnhancedInputTypeInfo> GetTriggerMetadata(const FString& TriggerClass);

	/**
	 * Create a trigger instance of any discovered type
	 * @param TriggerClass - Full class path
	 * @param TriggerName - Name for the trigger instance
	 * @return Created trigger instance or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Triggers")
	TResult<UInputTrigger*> CreateTriggerInstance(const FString& TriggerClass, const FString& TriggerName);

	/**
	 * Apply advanced configuration to a trigger with full property control
	 * @param Trigger - Target trigger instance
	 * @param PropertyConfigs - Map of property names to values
	 * @return Success or configuration error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Triggers")
	TResult<bool> ConfigureTriggerAdvanced(UInputTrigger* Trigger, const TMap<FString, FString>& PropertyConfigs);

	/**
	 * Validate trigger configuration using reflection metadata
	 * @param Trigger - Trigger to validate
	 * @return Validation result with error details
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Triggers")
	TResult<bool> ValidateTriggerSetup(UInputTrigger* Trigger);

	/**
	 * Analyze trigger performance and suggest optimizations
	 * @param Triggers - Trigger array to analyze
	 * @return Performance analysis and suggestions
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Triggers")
	TResult<TArray<FString>> AnalyzeTriggerPerformance(const TArray<UInputTrigger*>& Triggers);

	/**
	 * Get all properties of a trigger instance
	 * @param Trigger - Target trigger
	 * @return Array of property information
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Triggers")
	TResult<TArray<FEnhancedInputPropertyInfo>> GetTriggerProperties(UInputTrigger* Trigger);

	/**
	 * Clone a trigger with all its configuration
	 * @param SourceTrigger - Trigger to clone
	 * @param CloneName - Name for the cloned trigger
	 * @return New cloned trigger
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Triggers")
	TResult<UInputTrigger*> CloneTrigger(UInputTrigger* SourceTrigger, const FString& CloneName);

	/**
	 * Compare two triggers and return differences
	 * @param Trigger1 - First trigger
	 * @param Trigger2 - Second trigger
	 * @return List of property differences
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Triggers")
	TResult<TArray<FString>> CompareTriggers(UInputTrigger* Trigger1, UInputTrigger* Trigger2);

	/**
	 * Detect conflicts between triggers
	 * @param Triggers - Array of triggers to check
	 * @return List of detected conflicts
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Triggers")
	TResult<TArray<FString>> DetectTriggerConflicts(const TArray<UInputTrigger*>& Triggers);
};
