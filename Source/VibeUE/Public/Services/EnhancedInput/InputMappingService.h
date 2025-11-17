#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"
#include "Core/ServiceContext.h"
#include "Services/EnhancedInput/Types/EnhancedInputTypes.h"
#include "InputMappingContext.h"

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
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<bool> DeleteMappingContext(const FString& ContextPath, bool bForceDelete = false);

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

	/**
	 * Duplicate a Mapping Context with all its mappings
	 * 
	 * @param SourceContextPath - Path to source Mapping Context
	 * @param DestinationPath - Content browser path for duplicate
	 * @param NewName - Name for the duplicated context
	 * @return Duplicated context or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<UInputMappingContext*> DuplicateMappingContext(const FString& SourceContextPath, const FString& DestinationPath, const FString& NewName);

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
	TResult<FEnhancedInputMappingInfo> AnalyzeContextUsage(const FString& ContextPath);

	/**
	 * Check for key conflicts between contexts
	 * 
	 * @param ContextPaths - Paths to contexts to check
	 * @return Conflict analysis or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<TArray<FString>> DetectKeyConflicts(const TArray<FString>& ContextPaths);

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
};
