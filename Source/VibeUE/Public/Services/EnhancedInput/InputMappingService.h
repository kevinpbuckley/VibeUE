#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"
#include "Core/ServiceContext.h"
#include "Services/EnhancedInput/Types/EnhancedInputTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"

class FEnhancedInputReflectionService;
class FInputDiscoveryService;
class FInputValidationService;

/**
 * FInputMappingService - Complete Input Mapping Context management
 * 
 * Handles creation, configuration, and lifecycle management of Input Mapping Contexts
 * and their associated input mappings using reflection-based architecture.
 * 
 * All operations use Phase 1 reflection infrastructure (zero hardcoding).
 */
class VIBEUE_API FInputMappingService : public FServiceBase
{
public:
	explicit FInputMappingService(TSharedPtr<class FServiceContext> Context);
	virtual ~FInputMappingService() = default;

	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("InputMappingService"); }
	virtual void Initialize() override;
	virtual void Shutdown() override;

	/**
	 * Create a new Input Mapping Context asset
	 * 
	 * @param ContextName - Name for the new Mapping Context
	 * @param AssetPath - Content browser path (e.g., "/Game/Input/Contexts")
	 * @param Priority - Priority for this context in the input stack (higher = earlier processing)
	 * @return Created Mapping Context or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<UInputMappingContext*> CreateMappingContext(const FString& ContextName, const FString& AssetPath, int32 Priority = 0);

	/**
	 * Delete an Input Mapping Context asset
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @param bForceDelete - Force delete even if context has references
	 * @return Success or error
	 */
	// NOTE: For deleting Mapping Contexts, use manage_asset(action="delete", asset_path="/Game/Input/IMC_Name")

	/**
	 * Get detailed information about a Mapping Context
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @return Detailed context information or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<FEnhancedInputMappingInfo> GetContextInfo(const FString& ContextPath);

	/**
	 * Add an input mapping to a context
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @param ActionPath - Full path to the Input Action to map
	 * @param KeyName - Name of the key to map (e.g., "W", "Space", "Mouse X")
	 * @param bShift - Require shift modifier
	 * @param bCtrl - Require ctrl modifier
	 * @param bAlt - Require alt modifier
	 * @param bCmd - Require cmd modifier (Mac)
	 * @return Index of the added mapping or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<int32> AddInputMapping(const FString& ContextPath, const FString& ActionPath, const FString& KeyName,
		bool bShift = false, bool bCtrl = false, bool bAlt = false, bool bCmd = false);

	/**
	 * Remove an input mapping from a context
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of mapping to remove (from GetContextInfo)
	 * @return Success or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<bool> RemoveInputMapping(const FString& ContextPath, int32 MappingIndex);

	/**
	 * Get all mappings in a context
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @return Array of mapping information or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<TArray<FEnhancedInputPropertyInfo>> GetContextMappings(const FString& ContextPath);

	/**
	 * Set a property on a Mapping Context using reflection
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @param PropertyName - Name of property to set (discovered via reflection)
	 * @param PropertyValue - String value to set
	 * @return Success or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<bool> SetContextProperty(const FString& ContextPath, const FString& PropertyName, const FString& PropertyValue);

	/**
	 * Get a property value from a Mapping Context
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @param PropertyName - Name of property to get
	 * @return Property value as string or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<FString> GetContextProperty(const FString& ContextPath, const FString& PropertyName);

	/**
	 * Validate a Mapping Context configuration
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @return Validation result with error codes if invalid
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<bool> ValidateContextConfiguration(const FString& ContextPath);

	// NOTE: For duplicating Mapping Contexts, use manage_asset(action="duplicate", asset_path="...", destination_path="...", new_name="...")

	/**
	 * Find all Mapping Contexts in the project
	 * 
	 * @param SearchCriteria - Optional search filter
	 * @return Array of Mapping Context paths or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<TArray<FString>> FindAllMappingContexts(const FString& SearchCriteria = TEXT(""));

	/**
	 * Get input keys available for mapping
	 * 
	 * @param SearchFilter - Optional filter for key names
	 * @return Array of available key names or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<TArray<FString>> GetAvailableInputKeys(const FString& SearchFilter = TEXT(""));

	/**
	 * Analyze Mapping Context usage and conflicts
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @return Usage analysis or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<FEnhancedInputUsageInfo> AnalyzeContextUsage(const FString& ContextPath);

	/**
	 * Check for key conflicts between contexts
	 * 
	 * @param ContextPaths - Paths to contexts to check
	 * @return Conflict analysis or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<TArray<FEnhancedInputKeyConflict>> DetectKeyConflicts(const TArray<FString>& ContextPaths);

	// ═══════════════════════════════════════════════════════════════════
	// Modifier Management - Add/Remove modifiers on mappings
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Add a modifier to a specific mapping in a context
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping (from GetContextMappings)
	 * @param Modifier - The modifier object to add (create with CreateModifier)
	 * @return Success or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<bool> AddModifierToMapping(const FString& ContextPath, int32 MappingIndex, UInputModifier* Modifier);

	/**
	 * Remove a modifier from a mapping
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping
	 * @param ModifierIndex - Index of the modifier to remove
	 * @return Success or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<bool> RemoveModifierFromMapping(const FString& ContextPath, int32 MappingIndex, int32 ModifierIndex);

	/**
	 * Get all modifiers on a mapping
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping
	 * @return Array of modifier instance info or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<TArray<FEnhancedInputModifierInstanceInfo>> GetMappingModifiers(const FString& ContextPath, int32 MappingIndex);

	/**
	 * Get available modifier types
	 * 
	 * @return Array of available modifier type names
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<TArray<FString>> GetAvailableModifierTypes();

	// ═══════════════════════════════════════════════════════════════════
	// Trigger Management - Add/Remove triggers on mappings
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Add a trigger to a specific mapping in a context
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping (from GetContextMappings)
	 * @param Trigger - The trigger object to add (create with CreateTrigger)
	 * @return Success or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<bool> AddTriggerToMapping(const FString& ContextPath, int32 MappingIndex, UInputTrigger* Trigger);

	/**
	 * Remove a trigger from a mapping
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping
	 * @param TriggerIndex - Index of the trigger to remove
	 * @return Success or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<bool> RemoveTriggerFromMapping(const FString& ContextPath, int32 MappingIndex, int32 TriggerIndex);

	/**
	 * Get all triggers on a mapping
	 * 
	 * @param ContextPath - Full path to the Mapping Context
	 * @param MappingIndex - Index of the mapping
	 * @return Array of trigger instance info or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<TArray<FEnhancedInputTriggerInstanceInfo>> GetMappingTriggers(const FString& ContextPath, int32 MappingIndex);

	/**
	 * Get available trigger types
	 * 
	 * @return Array of available trigger type names
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<TArray<FString>> GetAvailableTriggerTypes();

	/**
	 * Create a trigger instance of specified type
	 * 
	 * @param TriggerTypeName - Type name (e.g., "Down", "Pressed", "Hold")
	 * @return Created trigger or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<UInputTrigger*> CreateTrigger(const FString& TriggerTypeName);

	/**
	 * Create a modifier instance of specified type
	 * 
	 * @param ModifierTypeName - Type name (e.g., "Negate", "Swizzle", "DeadZone")
	 * @return Created modifier or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<UInputModifier*> CreateModifier(const FString& ModifierTypeName);

private:
	// Service dependencies (injected via ServiceContext)
	TSharedPtr<FEnhancedInputReflectionService> ReflectionService;
	TSharedPtr<FInputDiscoveryService> DiscoveryService;
	TSharedPtr<FInputValidationService> ValidationService;

	// Helper methods
	UInputMappingContext* LoadContextAsset(const FString& ContextPath);
	bool SaveContextAsset(UInputMappingContext* Context);
	FString GenerateAssetPath(const FString& BasePath, const FString& AssetName);
	FKey FindKeyByName(const FString& KeyName);
	
	// Class finder helpers for modifier/trigger creation
	UClass* FindModifierClassInternal(const FString& TypeName);
	UClass* FindTriggerClassInternal(const FString& TypeName);
};
