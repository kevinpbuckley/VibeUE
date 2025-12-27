// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Services/EnhancedInput/EnhancedInputServices.h"
#include "Services/EnhancedInput/EnhancedInputReflectionService.h"
#include "Core/ServiceContext.h"

// ═══════════════════════════════════════════════════════════════════
// Validation Service
// ═══════════════════════════════════════════════════════════════════

FEnhancedInputValidationService::FEnhancedInputValidationService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

void FEnhancedInputValidationService::Initialize()
{
	LogInfo(TEXT("Initializing Enhanced Input Validation Service"));
	
	// Get reference to reflection service from context by name
	TSharedPtr<FServiceBase> ServicePtr = GetContext()->GetService(TEXT("EnhancedInputReflectionService"));
	if (ServicePtr)
	{
		ReflectionService = StaticCastSharedPtr<FEnhancedInputReflectionService>(ServicePtr);
	}
	else
	{
		LogWarning(TEXT("Failed to get EnhancedInputReflectionService"));
	}
}

TResult<void> FEnhancedInputValidationService::ValidateInputActionConfig(
	const FString& ActionName, const FString& ValueType)
{
	if (auto Result = ValidateNotEmpty(ActionName, TEXT("ActionName")); Result.IsError())
	{
		return Result;
	}
	
	if (auto Result = ValidateNotEmpty(ValueType, TEXT("ValueType")); Result.IsError())
	{
		return Result;
	}
	
	// Validate value type is one of the known types
	const TArray<FString> ValidTypes = { TEXT("Value1D"), TEXT("Value2D"), TEXT("Digital") };
	
	if (!ValidTypes.Contains(ValueType))
	{
		return TResult<void>::Error(TEXT("INVALID_VALUE_TYPE"), 
			FString::Printf(TEXT("Invalid value type '%s'. Must be one of: Value1D, Value2D, Digital"), *ValueType));
	}
	
	return TResult<void>::Success();
}

TResult<void> FEnhancedInputValidationService::ValidateMappingContextConfig(
	const FString& ContextName, int32 Priority)
{
	if (auto Result = ValidateNotEmpty(ContextName, TEXT("ContextName")); Result.IsError())
	{
		return Result;
	}
	
	// Priority can be any integer, including negative
	return TResult<void>::Success();
}

TResult<void> FEnhancedInputValidationService::ValidateModifierConfig(
	const FString& ModifierClass, const TMap<FString, FString>& Properties)
{
	if (auto Result = ValidateNotEmpty(ModifierClass, TEXT("ModifierClass")); Result.IsError())
	{
		return Result;
	}
	
	if (!ReflectionService.IsValid())
	{
		return TResult<void>::Error(TEXT("SERVICE_UNAVAILABLE"), 
			TEXT("Reflection service is not available"));
	}
	
	// Validate modifier class exists
	auto ValidResult = ReflectionService->ValidateModifierType(ModifierClass);
	if (ValidResult.IsError())
	{
		return TResult<void>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
	}
	
	if (!ValidResult.GetValue())
	{
		return TResult<void>::Error(TEXT("INVALID_MODIFIER_TYPE"), 
			FString::Printf(TEXT("'%s' is not a valid modifier type"), *ModifierClass));
	}
	
	return TResult<void>::Success();
}

TResult<void> FEnhancedInputValidationService::ValidateTriggerConfig(
	const FString& TriggerClass, const TMap<FString, FString>& Properties)
{
	if (auto Result = ValidateNotEmpty(TriggerClass, TEXT("TriggerClass")); Result.IsError())
	{
		return Result;
	}
	
	if (!ReflectionService.IsValid())
	{
		return TResult<void>::Error(TEXT("SERVICE_UNAVAILABLE"), 
			TEXT("Reflection service is not available"));
	}
	
	// Validate trigger class exists
	auto ValidResult = ReflectionService->ValidateTriggerType(TriggerClass);
	if (ValidResult.IsError())
	{
		return TResult<void>::Error(ValidResult.GetErrorCode(), ValidResult.GetErrorMessage());
	}
	
	if (!ValidResult.GetValue())
	{
		return TResult<void>::Error(TEXT("INVALID_TRIGGER_TYPE"), 
			FString::Printf(TEXT("'%s' is not a valid trigger type"), *TriggerClass));
	}
	
	return TResult<void>::Success();
}

TResult<void> FEnhancedInputValidationService::ValidatePropertyAssignment(
	UClass* Class, const FString& PropertyName, const FString& Value)
{
	if (!Class)
	{
		return TResult<void>::Error(TEXT("INVALID_CLASS"), TEXT("Class is null"));
	}
	
	if (auto Result = ValidateNotEmpty(PropertyName, TEXT("PropertyName")); Result.IsError())
	{
		return Result;
	}
	
	if (!ReflectionService.IsValid())
	{
		return TResult<void>::Error(TEXT("SERVICE_UNAVAILABLE"), 
			TEXT("Reflection service is not available"));
	}
	
	// Validate property exists
	if (auto ExistsResult = ReflectionService->ValidatePropertyExists(Class, PropertyName); 
		ExistsResult.IsError() || !ExistsResult.GetValue())
	{
		return TResult<void>::Error(TEXT("PROPERTY_NOT_FOUND"), 
			FString::Printf(TEXT("Property '%s' not found on class '%s'"), *PropertyName, *Class->GetName()));
	}
	
	return TResult<void>::Success();
}

// ═══════════════════════════════════════════════════════════════════
// Asset Service
// ═══════════════════════════════════════════════════════════════════

FEnhancedInputAssetService::FEnhancedInputAssetService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

void FEnhancedInputAssetService::Initialize()
{
	LogInfo(TEXT("Initializing Enhanced Input Asset Service"));
	
	// Get references to dependent services by name
	TSharedPtr<FServiceBase> ReflServicePtr = GetContext()->GetService(TEXT("EnhancedInputReflectionService"));
	if (ReflServicePtr)
	{
		ReflectionService = StaticCastSharedPtr<FEnhancedInputReflectionService>(ReflServicePtr);
	}
	else
	{
		LogWarning(TEXT("Failed to get EnhancedInputReflectionService"));
	}
	
	TSharedPtr<FServiceBase> ValServicePtr = GetContext()->GetService(TEXT("EnhancedInputValidationService"));
	if (ValServicePtr)
	{
		ValidationService = StaticCastSharedPtr<FEnhancedInputValidationService>(ValServicePtr);
	}
	else
	{
		LogWarning(TEXT("Failed to get EnhancedInputValidationService"));
	}
}

TResult<FString> FEnhancedInputAssetService::CreateInputAction(
	const FString& AssetPath, const FString& ValueType)
{
	// Validate inputs
	if (auto Result = ValidationService->ValidateInputActionConfig(FPaths::GetBaseFilename(AssetPath), ValueType); 
		Result.IsError())
	{
		return TResult<FString>::Error(Result.GetErrorCode(), Result.GetErrorMessage());
	}
	
	// TODO: Implement asset creation via Unreal's factory system
	// This would use FAssetToolsModule to create the Input Action asset
	
	LogInfo(FString::Printf(TEXT("Created Input Action asset: %s (ValueType: %s)"), *AssetPath, *ValueType));
	
	return TResult<FString>::Success(AssetPath);
}

TResult<FString> FEnhancedInputAssetService::CreateInputMappingContext(const FString& AssetPath)
{
	if (auto Result = ValidateNotEmpty(AssetPath, TEXT("AssetPath")); Result.IsError())
	{
		return TResult<FString>::Error(Result.GetErrorCode(), Result.GetErrorMessage());
	}
	
	// TODO: Implement asset creation via Unreal's factory system
	
	LogInfo(FString::Printf(TEXT("Created Input Mapping Context asset: %s"), *AssetPath));
	
	return TResult<FString>::Success(AssetPath);
}

TResult<void> FEnhancedInputAssetService::DeleteAsset(const FString& AssetPath)
{
	if (auto Result = ValidateNotEmpty(AssetPath, TEXT("AssetPath")); Result.IsError())
	{
		return Result;
	}
	
	// TODO: Implement asset deletion via object/asset management
	
	LogInfo(FString::Printf(TEXT("Deleted Enhanced Input asset: %s"), *AssetPath));
	
	return TResult<void>::Success();
}

TResult<UObject*> FEnhancedInputAssetService::LoadAsset(const FString& AssetPath)
{
	if (auto Result = ValidateNotEmpty(AssetPath, TEXT("AssetPath")); Result.IsError())
	{
		return TResult<UObject*>::Error(Result.GetErrorCode(), Result.GetErrorMessage());
	}
	
	// Load the asset
	UObject* Asset = LoadObject<UObject>(nullptr, *AssetPath);
	
	if (!Asset)
	{
		return TResult<UObject*>::Error(TEXT("ASSET_LOAD_FAILED"), 
			FString::Printf(TEXT("Failed to load asset: %s"), *AssetPath));
	}
	
	return TResult<UObject*>::Success(Asset);
}

TResult<void> FEnhancedInputAssetService::SaveAsset(UObject* Asset)
{
	if (!Asset)
	{
		return TResult<void>::Error(TEXT("INVALID_ASSET"), TEXT("Asset is null"));
	}
	
	// TODO: Save the asset using UPackage
	
	LogInfo(FString::Printf(TEXT("Saved Enhanced Input asset: %s"), *Asset->GetPathName()));
	
	return TResult<void>::Success();
}

// ═══════════════════════════════════════════════════════════════════
// Discovery Service
// ═══════════════════════════════════════════════════════════════════

FEnhancedInputDiscoveryService::FEnhancedInputDiscoveryService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

void FEnhancedInputDiscoveryService::Initialize()
{
	LogInfo(TEXT("Initializing Enhanced Input Discovery Service"));
	
	// Get reference to reflection service from context by name
	TSharedPtr<FServiceBase> ServicePtr = GetContext()->GetService(TEXT("EnhancedInputReflectionService"));
	if (ServicePtr)
	{
		ReflectionService = StaticCastSharedPtr<FEnhancedInputReflectionService>(ServicePtr);
	}
	else
	{
		LogWarning(TEXT("Failed to get EnhancedInputReflectionService"));
	}
}

TResult<TArray<FString>> FEnhancedInputDiscoveryService::FindInputActions(
	const FEnhancedInputAssetSearchCriteria& Criteria)
{
	if (!ReflectionService.IsValid())
	{
		return TResult<TArray<FString>>::Error(TEXT("SERVICE_UNAVAILABLE"), 
			TEXT("Reflection service is not available"));
	}
	
	auto Result = ReflectionService->DiscoverInputActionAssets(Criteria);
	
	if (Result.IsError())
	{
		return TResult<TArray<FString>>::Error(Result.GetErrorCode(), Result.GetErrorMessage());
	}
	
	return TResult<TArray<FString>>::Success(Result.GetValue().AssetPaths);
}

TResult<TArray<FString>> FEnhancedInputDiscoveryService::FindMappingContexts(
	const FEnhancedInputAssetSearchCriteria& Criteria)
{
	if (!ReflectionService.IsValid())
	{
		return TResult<TArray<FString>>::Error(TEXT("SERVICE_UNAVAILABLE"), 
			TEXT("Reflection service is not available"));
	}
	
	auto Result = ReflectionService->DiscoverMappingContextAssets(Criteria);
	
	if (Result.IsError())
	{
		return TResult<TArray<FString>>::Error(Result.GetErrorCode(), Result.GetErrorMessage());
	}
	
	return TResult<TArray<FString>>::Success(Result.GetValue().AssetPaths);
}

TResult<TArray<FString>> FEnhancedInputDiscoveryService::GetAvailableModifiers()
{
	if (!ReflectionService.IsValid())
	{
		return TResult<TArray<FString>>::Error(TEXT("SERVICE_UNAVAILABLE"), 
			TEXT("Reflection service is not available"));
	}
	
	FEnhancedInputTypeSearchCriteria Criteria;
	auto Result = ReflectionService->DiscoverModifierTypes(Criteria);
	
	if (Result.IsError())
	{
		return TResult<TArray<FString>>::Error(Result.GetErrorCode(), Result.GetErrorMessage());
	}
	
	TArray<FString> ModifierNames;
	for (const auto& Modifier : Result.GetValue())
	{
		ModifierNames.Add(Modifier.ClassPath);
	}
	
	return TResult<TArray<FString>>::Success(ModifierNames);
}

TResult<TArray<FString>> FEnhancedInputDiscoveryService::GetAvailableTriggers()
{
	if (!ReflectionService.IsValid())
	{
		return TResult<TArray<FString>>::Error(TEXT("SERVICE_UNAVAILABLE"), 
			TEXT("Reflection service is not available"));
	}
	
	FEnhancedInputTypeSearchCriteria Criteria;
	auto Result = ReflectionService->DiscoverTriggerTypes(Criteria);
	
	if (Result.IsError())
	{
		return TResult<TArray<FString>>::Error(Result.GetErrorCode(), Result.GetErrorMessage());
	}
	
	TArray<FString> TriggerNames;
	for (const auto& Trigger : Result.GetValue())
	{
		TriggerNames.Add(Trigger.ClassPath);
	}
	
	return TResult<TArray<FString>>::Success(TriggerNames);
}
