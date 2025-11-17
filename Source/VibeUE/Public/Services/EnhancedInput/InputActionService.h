#pragma once

#include "CoreMinimal.h"
#include "Services/Common/ServiceBase.h"
#include "Core/Result.h"
#include "Core/ServiceContext.h"
#include "Services/EnhancedInput/Types/EnhancedInputTypes.h"
#include "InputActionValue.h"

class UInputAction;
class FEnhancedInputReflectionService;
class FInputDiscoveryService;
class FInputValidationService;

/**
 * FInputActionService - Complete Input Action lifecycle management
 * 
 * Handles creation, configuration, deletion, and relationship management of Input Actions
 * using reflection-based property access for maximum flexibility and extensibility.
 * 
 * All operations use Phase 1 reflection infrastructure (zero hardcoding).
 */
class VIBEUE_API FInputActionService : public FServiceBase
{
public:
	explicit FInputActionService(TSharedPtr<class FServiceContext> Context);
	virtual ~FInputActionService() = default;

	// FServiceBase interface
	virtual FString GetServiceName() const override { return TEXT("InputActionService"); }
	virtual void Initialize() override;
	virtual void Shutdown() override;

	/**
	 * Create a new Input Action asset
	 * 
	 * @param AssetName - Name for the new Input Action
	 * @param AssetPath - Content browser path (e.g., "/Game/Input/Actions")
	 * @param ValueType - Type of value (Boolean, Axis1D, Axis2D, Axis3D)
	 * @return Created Input Action or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<UInputAction*> CreateInputAction(const FString& AssetName, const FString& AssetPath, EInputActionValueType ValueType);

	/**
	 * Delete an Input Action asset
	 * 
	 * @param ActionPath - Full path to the Input Action (e.g., "/Game/Input/Actions/IA_Move")
	 * @param bForceDelete - Force delete even if asset has references
	 * @return Success or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<bool> DeleteInputAction(const FString& ActionPath, bool bForceDelete = false);

	/**
	 * Get detailed information about an Input Action
	 * 
	 * @param ActionPath - Full path to the Input Action
	 * @return Detailed action information or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<FEnhancedInputActionInfo> GetActionInfo(const FString& ActionPath);

	/**
	 * Set a property on an Input Action using reflection
	 * 
	 * @param ActionPath - Full path to the Input Action
	 * @param PropertyName - Name of property to set (discovered via reflection)
	 * @param PropertyValue - String value to set (will be converted to proper type)
	 * @return Success or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<bool> SetActionProperty(const FString& ActionPath, const FString& PropertyName, const FString& PropertyValue);

	/**
	 * Get a property value from an Input Action using reflection
	 * 
	 * @param ActionPath - Full path to the Input Action
	 * @param PropertyName - Name of property to get
	 * @return Property value as string or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<FString> GetActionProperty(const FString& ActionPath, const FString& PropertyName);

	/**
	 * Get all properties of an Input Action
	 * 
	 * @param ActionPath - Full path to the Input Action
	 * @return Array of property information or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<TArray<FEnhancedInputPropertyInfo>> GetActionProperties(const FString& ActionPath);

	/**
	 * Validate an Input Action configuration
	 * 
	 * @param ActionPath - Full path to the Input Action
	 * @return Validation result with error codes if invalid
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<bool> ValidateActionConfiguration(const FString& ActionPath);

	/**
	 * Duplicate an existing Input Action
	 * 
	 * @param SourceActionPath - Path to source Input Action
	 * @param DestinationPath - Content browser path for duplicate
	 * @param NewName - Name for the duplicated action
	 * @return Duplicated action or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<UInputAction*> DuplicateInputAction(const FString& SourceActionPath, const FString& DestinationPath, const FString& NewName);

	/**
	 * Find all Input Actions in the project
	 * 
	 * @param SearchCriteria - Optional search filter
	 * @return Array of Input Action paths or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<TArray<FString>> FindAllInputActions(const FString& SearchCriteria = TEXT(""));

	/**
	 * Analyze Input Action usage and relationships
	 * 
	 * @param ActionPath - Full path to the Input Action
	 * @return Usage analysis or error
	 */
	UFUNCTION(BlueprintCallable, Category = "Enhanced Input")
	TResult<FEnhancedInputActionInfo> AnalyzeActionUsage(const FString& ActionPath);

private:
	// Service dependencies (injected via ServiceContext)
	TSharedPtr<FEnhancedInputReflectionService> ReflectionService;
	TSharedPtr<FInputDiscoveryService> DiscoveryService;
	TSharedPtr<FInputValidationService> ValidationService;

	// Helper methods
	UInputAction* LoadActionAsset(const FString& ActionPath);
	bool SaveActionAsset(UInputAction* Action);
	FString GenerateAssetPath(const FString& BasePath, const FString& AssetName);
};
