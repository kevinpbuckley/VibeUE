// Copyright Kevin Buckley 2025 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"
#include "Core/ServiceContext.h"
#include "Services/EnhancedInput/Types/EnhancedInputTypes.h"
#include "InputModifiers.h"

class FEnhancedInputReflectionService;
class FInputDiscoveryService;
class FInputValidationService;

/**
 * Advanced Modifier Service
 * 
 * Provides comprehensive input modifier management with reflection-based discovery,
 * dynamic instantiation, advanced configuration, and optimization.
 * 
 * CRITICAL DESIGN PRINCIPLE: ZERO HARDCODING
 * - All modifier types discovered via reflection
 * - All properties accessed via reflection service
 * - No hardcoded modifier lists or property names
 * - Complete reliance on Phase 1 reflection infrastructure
 */
class VIBEUE_API FAdvancedModifierService : public FServiceBase
{
public:
	explicit FAdvancedModifierService(TSharedPtr<class FServiceContext> Context);
	virtual ~FAdvancedModifierService() = default;

	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("AdvancedModifierService"); }
	virtual void Initialize() override;
	virtual void Shutdown() override;

	// ═══════════════════════════════════════════════════════════════════
	// Modifier Type Discovery (Reflection-Based, Zero Hardcoding)
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Discover all available input modifier types via reflection
	 * @return Array of modifier type information
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Modifiers")
	TResult<TArray<FEnhancedInputTypeInfo>> DiscoverModifierTypes();

	/**
	 * Get comprehensive metadata for a specific modifier type
	 * @param ModifierClass - Full class path (e.g., "/Script/EnhancedInput.InputModifierDeadzone")
	 * @return Detailed modifier metadata with property information
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Modifiers")
	TResult<FEnhancedInputTypeInfo> GetModifierMetadata(const FString& ModifierClass);

	/**
	 * Create a modifier instance of any discovered type
	 * @param ModifierClass - Full class path
	 * @param ModifierName - Name for the modifier instance
	 * @return Created modifier instance or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Modifiers")
	TResult<UInputModifier*> CreateModifierInstance(const FString& ModifierClass, const FString& ModifierName);

	/**
	 * Apply advanced configuration to a modifier with full property control
	 * @param Modifier - Target modifier instance
	 * @param PropertyConfigs - Map of property names to values
	 * @return Success or configuration error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Modifiers")
	TResult<bool> ConfigureModifierAdvanced(UInputModifier* Modifier, const TMap<FString, FString>& PropertyConfigs);

	/**
	 * Validate modifier configuration using reflection metadata
	 * @param Modifier - Modifier to validate
	 * @return Validation result with error details
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Modifiers")
	TResult<bool> ValidateModifierSetup(UInputModifier* Modifier);

	/**
	 * Analyze and suggest modifier stack optimizations
	 * @param Modifiers - Current modifier stack
	 * @return Optimization suggestions
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Modifiers")
	TResult<TArray<FString>> OptimizeModifierStack(const TArray<UInputModifier*>& Modifiers);

	/**
	 * Reorder modifiers in a stack
	 * @param ModifierStack - Array of modifiers
	 * @param ModifierOrder - Desired order by index
	 * @return Reordered stack
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Modifiers")
	TResult<TArray<UInputModifier*>> ReorderModifierStack(const TArray<UInputModifier*>& ModifierStack, const TArray<int32>& ModifierOrder);

	/**
	 * Get all properties of a modifier instance
	 * @param Modifier - Target modifier
	 * @return Array of property information
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Modifiers")
	TResult<TArray<FEnhancedInputPropertyInfo>> GetModifierProperties(UInputModifier* Modifier);

	/**
	 * Clone a modifier with all its configuration
	 * @param SourceModifier - Modifier to clone
	 * @param CloneName - Name for the cloned modifier
	 * @return New cloned modifier
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Modifiers")
	TResult<UInputModifier*> CloneModifier(UInputModifier* SourceModifier, const FString& CloneName);

	/**
	 * Compare two modifiers and return differences
	 * @param Modifier1 - First modifier
	 * @param Modifier2 - Second modifier
	 * @return List of property differences
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input|Modifiers")
	TResult<TArray<FString>> CompareModifiers(UInputModifier* Modifier1, UInputModifier* Modifier2);
};
