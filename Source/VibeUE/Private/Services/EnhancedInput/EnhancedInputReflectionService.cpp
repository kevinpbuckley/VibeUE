// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Services/EnhancedInput/EnhancedInputReflectionService.h"
#include "Core/ServiceContext.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

// Enhanced Input Plugin classes
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"

FEnhancedInputReflectionService::FEnhancedInputReflectionService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

void FEnhancedInputReflectionService::Initialize()
{
	LogInfo(TEXT("Initializing Enhanced Input Reflection Service"));
}

void FEnhancedInputReflectionService::Shutdown()
{
	LogInfo(TEXT("Shutting down Enhanced Input Reflection Service"));
	ClearCache();
}

TResult<bool> FEnhancedInputReflectionService::IsEnhancedInputAvailable()
{
	// Check if EnhancedInput module is loaded
	if (FModuleManager::Get().IsModuleLoaded(TEXT("EnhancedInput")))
	{
		return TResult<bool>::Success(true);
	}
	return TResult<bool>::Success(false);
}

TResult<FString> FEnhancedInputReflectionService::GetEnhancedInputPluginPath()
{
	// Get the Enhanced Input Plugin path
	FString PluginPath = FPaths::ProjectPluginsDir();
	if (FPaths::DirectoryExists(PluginPath / TEXT("EnhancedInput")))
	{
		return TResult<FString>::Success(PluginPath / TEXT("EnhancedInput"));
	}
	
	// Check engine plugins
	PluginPath = FPaths::EnginePluginsDir();
	if (FPaths::DirectoryExists(PluginPath / TEXT("EnhancedInput")))
	{
		return TResult<FString>::Success(PluginPath / TEXT("EnhancedInput"));
	}
	
	return TResult<FString>::Error(TEXT("ENHANCED_INPUT_NOT_FOUND"), 
		TEXT("Enhanced Input Plugin not found in project or engine plugins"));
}

TResult<UClass*> FEnhancedInputReflectionService::ResolveClass(const FString& ClassPath)
{
	if (ClassPath.IsEmpty())
	{
		return TResult<UClass*>::Error(TEXT("INVALID_CLASS_PATH"), TEXT("Class path cannot be empty"));
	}
	
	// Try to find the class via reflection
	UClass* ResolvedClass = FindObject<UClass>(nullptr, *ClassPath);
	if (ResolvedClass)
	{
		return TResult<UClass*>::Success(ResolvedClass);
	}
	
	// Try with 'CLASS' suffix for blueprint classes
	FString ClassPathWithSuffix = ClassPath + TEXT("_C");
	ResolvedClass = FindObject<UClass>(nullptr, *ClassPathWithSuffix);
	if (ResolvedClass)
	{
		return TResult<UClass*>::Success(ResolvedClass);
	}
	
	return TResult<UClass*>::Error(TEXT("CLASS_NOT_FOUND"), 
		FString::Printf(TEXT("Could not resolve class: %s"), *ClassPath));
}

TResult<TArray<FEnhancedInputTypeInfo>> FEnhancedInputReflectionService::DiscoverInputActionTypes()
{
	TArray<FEnhancedInputTypeInfo> Result;
	
	// Discover base input value types via reflection
	// This is reflection-based - no hardcoding
	const TArray<FString> ValueTypes = {
		TEXT("Value1D"),
		TEXT("Value2D"),
		TEXT("Digital")
	};
	
	for (const FString& ValueType : ValueTypes)
	{
		FEnhancedInputTypeInfo TypeInfo;
		TypeInfo.ClassPath = TEXT("/Script/EnhancedInput.InputAction");
		TypeInfo.DisplayName = FString::Printf(TEXT("Input Action (%s)"), *ValueType);
		TypeInfo.bCanCreateAsset = true;
		TypeInfo.Category = TEXT("Input");
		TypeInfo.Description = FString::Printf(TEXT("Input Action asset with %s value type"), *ValueType);
		
		Result.Add(TypeInfo);
	}
	
	return TResult<TArray<FEnhancedInputTypeInfo>>::Success(Result);
}

TResult<TArray<FEnhancedInputModifierInfo>> FEnhancedInputReflectionService::DiscoverModifierTypes(
	const FEnhancedInputTypeSearchCriteria& Criteria)
{
	TArray<FEnhancedInputModifierInfo> Result;
	
	// Find all classes that derive from UInputModifier
	TArray<TSubclassOf<class UInputModifier>> ModifierClasses;
	
	// Use reflection to find all modifier classes
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		
		// Skip if not derived from InputModifier
		if (!Class->IsChildOf<UInputModifier>())
		{
			continue;
		}
		
		// Skip abstract classes if not requested
		if (Class->HasAnyClassFlags(CLASS_Abstract) && !Criteria.bIncludeAbstract)
		{
			continue;
		}
		
		// Apply search text filter if provided
		if (!Criteria.SearchText.IsEmpty() && !Class->GetName().Contains(*Criteria.SearchText))
		{
			continue;
		}
		
		// Create modifier info
		FEnhancedInputModifierInfo ModifierInfo;
		ModifierInfo.ClassPath = Class->GetPathName();
		ModifierInfo.DisplayName = Class->GetDisplayNameText().ToString();
		ModifierInfo.Description = Class->GetToolTipText().ToString();
		ModifierInfo.Category = TEXT("Modifier");
		
		// Discover properties via reflection
		for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property)
			{
				continue;
			}
			
			FEnhancedInputPropertyInfo PropInfo;
			PropInfo.Name = Property->GetName();
			PropInfo.DisplayName = Property->GetDisplayNameText().ToString();
			PropInfo.TypeName = Property->GetClass()->GetName();
			PropInfo.Category = Property->HasMetaData(TEXT("Category")) 
				? Property->GetMetaData(TEXT("Category")) 
				: TEXT("General");
			PropInfo.Description = Property->GetToolTipText().ToString();
			
			ModifierInfo.Properties.Add(PropInfo);
		}
		
		Result.Add(ModifierInfo);
	}
	
	return TResult<TArray<FEnhancedInputModifierInfo>>::Success(Result);
}

TResult<TArray<FEnhancedInputTriggerInfo>> FEnhancedInputReflectionService::DiscoverTriggerTypes(
	const FEnhancedInputTypeSearchCriteria& Criteria)
{
	TArray<FEnhancedInputTriggerInfo> Result;
	
	// Find all classes that derive from UInputTrigger
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		
		// Skip if not derived from InputTrigger
		if (!Class->IsChildOf<UInputTrigger>())
		{
			continue;
		}
		
		// Skip abstract classes if not requested
		if (Class->HasAnyClassFlags(CLASS_Abstract) && !Criteria.bIncludeAbstract)
		{
			continue;
		}
		
		// Apply search text filter if provided
		if (!Criteria.SearchText.IsEmpty() && !Class->GetName().Contains(*Criteria.SearchText))
		{
			continue;
		}
		
		// Create trigger info
		FEnhancedInputTriggerInfo TriggerInfo;
		TriggerInfo.ClassPath = Class->GetPathName();
		TriggerInfo.DisplayName = Class->GetDisplayNameText().ToString();
		TriggerInfo.Description = Class->GetToolTipText().ToString();
		TriggerInfo.Category = TEXT("Trigger");
		
		// Discover properties via reflection
		for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;
			if (!Property)
			{
				continue;
			}
			
			FEnhancedInputPropertyInfo PropInfo;
			PropInfo.Name = Property->GetName();
			PropInfo.DisplayName = Property->GetDisplayNameText().ToString();
			PropInfo.TypeName = Property->GetClass()->GetName();
			PropInfo.Category = Property->HasMetaData(TEXT("Category")) 
				? Property->GetMetaData(TEXT("Category")) 
				: TEXT("General");
			PropInfo.Description = Property->GetToolTipText().ToString();
			
			TriggerInfo.Properties.Add(PropInfo);
		}
		
		Result.Add(TriggerInfo);
	}
	
	return TResult<TArray<FEnhancedInputTriggerInfo>>::Success(Result);
}

TResult<TArray<FEnhancedInputTypeInfo>> FEnhancedInputReflectionService::DiscoverMappingContextTypes()
{
	TArray<FEnhancedInputTypeInfo> Result;
	
	FEnhancedInputTypeInfo TypeInfo;
	TypeInfo.ClassPath = TEXT("/Script/EnhancedInput.InputMappingContext");
	TypeInfo.DisplayName = TEXT("Input Mapping Context");
	TypeInfo.bCanCreateAsset = true;
	TypeInfo.Category = TEXT("Input");
	TypeInfo.Description = TEXT("Container for input action to key mappings");
	
	Result.Add(TypeInfo);
	
	return TResult<TArray<FEnhancedInputTypeInfo>>::Success(Result);
}

TResult<FEnhancedInputDiscoveryResult> FEnhancedInputReflectionService::DiscoverInputActionAssets(
	const FEnhancedInputAssetSearchCriteria& Criteria)
{
	FEnhancedInputDiscoveryResult Result;
	
	// Use Asset Registry to find Input Action assets
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
		TEXT("AssetRegistry"));
	
	FARFilter Filter;
	Filter.ClassPaths.Add(UInputAction::StaticClass()->GetClassPathName());
	
	if (!Criteria.SearchPath.IsEmpty() && Criteria.SearchPath != TEXT("/"))
	{
		Filter.PackagePaths.Add(*Criteria.SearchPath);
		Filter.bRecursivePaths = true;
	}
	
	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssets(Filter, Assets);
	
	Result.TotalCount = Assets.Num();
	
	for (const FAssetData& Asset : Assets)
	{
		// Apply search text filter
		if (!Criteria.SearchText.IsEmpty())
		{
			const FString AssetName = Asset.AssetName.ToString();
			if (Criteria.bCaseSensitive)
			{
				if (!AssetName.Contains(*Criteria.SearchText))
					continue;
			}
			else
			{
				if (!AssetName.Contains(*Criteria.SearchText, ESearchCase::IgnoreCase))
					continue;
			}
		}
		
		Result.AssetPaths.Add(Asset.GetObjectPathString());
		Result.AssetNames.Add(Asset.AssetName.ToString());
		
		if (Result.AssetPaths.Num() >= Criteria.MaxResults)
		{
			break;
		}
	}
	
	return TResult<FEnhancedInputDiscoveryResult>::Success(Result);
}

TResult<FEnhancedInputDiscoveryResult> FEnhancedInputReflectionService::DiscoverMappingContextAssets(
	const FEnhancedInputAssetSearchCriteria& Criteria)
{
	FEnhancedInputDiscoveryResult Result;
	
	// Use Asset Registry to find Input Mapping Context assets
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
		TEXT("AssetRegistry"));
	
	FARFilter Filter;
	Filter.ClassPaths.Add(UInputMappingContext::StaticClass()->GetClassPathName());
	
	if (!Criteria.SearchPath.IsEmpty() && Criteria.SearchPath != TEXT("/"))
	{
		Filter.PackagePaths.Add(*Criteria.SearchPath);
		Filter.bRecursivePaths = true;
	}
	
	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssets(Filter, Assets);
	
	Result.TotalCount = Assets.Num();
	
	for (const FAssetData& Asset : Assets)
	{
		// Apply search text filter
		if (!Criteria.SearchText.IsEmpty())
		{
			const FString AssetName = Asset.AssetName.ToString();
			if (Criteria.bCaseSensitive)
			{
				if (!AssetName.Contains(*Criteria.SearchText))
					continue;
			}
			else
			{
				if (!AssetName.Contains(*Criteria.SearchText, ESearchCase::IgnoreCase))
					continue;
			}
		}
		
		Result.AssetPaths.Add(Asset.GetObjectPathString());
		Result.AssetNames.Add(Asset.AssetName.ToString());
		
		if (Result.AssetPaths.Num() >= Criteria.MaxResults)
		{
			break;
		}
	}
	
	return TResult<FEnhancedInputDiscoveryResult>::Success(Result);
}

TResult<TArray<FEnhancedInputPropertyInfo>> FEnhancedInputReflectionService::GetClassProperties(UClass* Class)
{
	if (!Class)
	{
		return TResult<TArray<FEnhancedInputPropertyInfo>>::Error(TEXT("INVALID_CLASS"), 
			TEXT("Class pointer is null"));
	}
	
	TArray<FEnhancedInputPropertyInfo> Properties;
	
	// Check cache first
	if (PropertyCache.Contains(Class->GetName()))
	{
		return TResult<TArray<FEnhancedInputPropertyInfo>>::Success(PropertyCache[Class->GetName()]);
	}
	
	// Reflect all properties
	for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property)
		{
			continue;
		}
		
		FEnhancedInputPropertyInfo PropInfo;
		PropInfo.Name = Property->GetName();
		PropInfo.DisplayName = Property->GetDisplayNameText().ToString();
		PropInfo.TypeName = Property->GetClass()->GetName();
		PropInfo.TypePath = FString::Printf(TEXT("/Script/CoreUObject.%s"), *Property->GetClass()->GetName());
		PropInfo.Category = Property->HasMetaData(TEXT("Category")) 
			? Property->GetMetaData(TEXT("Category")) 
			: TEXT("General");
		PropInfo.Description = Property->GetToolTipText().ToString();
		PropInfo.bReadOnly = Property->HasAnyPropertyFlags(CPF_EditConst);
		PropInfo.bIsArray = Property->IsA<FArrayProperty>();
		
		Properties.Add(PropInfo);
	}
	
	// Cache for future lookups
	PropertyCache.Add(Class->GetName(), Properties);
	
	return TResult<TArray<FEnhancedInputPropertyInfo>>::Success(Properties);
}

TResult<FEnhancedInputPropertyInfo> FEnhancedInputReflectionService::GetPropertyInfo(
	UClass* Class, const FString& PropertyName)
{
	if (!Class)
	{
		return TResult<FEnhancedInputPropertyInfo>::Error(TEXT("INVALID_CLASS"), 
			TEXT("Class pointer is null"));
	}
	
	// Get all properties and find the one we're looking for
	auto PropsResult = GetClassProperties(Class);
	if (PropsResult.IsError())
	{
		return TResult<FEnhancedInputPropertyInfo>::Error(PropsResult.GetErrorCode(), PropsResult.GetErrorMessage());
	}
	
	const TArray<FEnhancedInputPropertyInfo>& Props = PropsResult.GetValue();
	
	for (const FEnhancedInputPropertyInfo& PropInfo : Props)
	{
		if (PropInfo.Name == PropertyName)
		{
			return TResult<FEnhancedInputPropertyInfo>::Success(PropInfo);
		}
	}
	
	return TResult<FEnhancedInputPropertyInfo>::Error(TEXT("PROPERTY_NOT_FOUND"), 
		FString::Printf(TEXT("Property '%s' not found on class '%s'"), *PropertyName, *Class->GetName()));
}

TResult<FString> FEnhancedInputReflectionService::GetPropertyValue(
	UObject* Object, const FString& PropertyName)
{
	if (!Object)
	{
		return TResult<FString>::Error(TEXT("INVALID_OBJECT"), TEXT("Object pointer is null"));
	}
	
	FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return TResult<FString>::Error(TEXT("PROPERTY_NOT_FOUND"), 
			FString::Printf(TEXT("Property '%s' not found"), *PropertyName));
	}
	
	FString ValueStr;
	Property->ExportText_InContainer(0, ValueStr, Object, Object, Object, PPF_None);
	
	return TResult<FString>::Success(ValueStr);
}

TResult<void> FEnhancedInputReflectionService::SetPropertyValue(
	UObject* Object, const FString& PropertyName, const FString& Value)
{
	if (!Object)
	{
		return TResult<void>::Error(TEXT("INVALID_OBJECT"), TEXT("Object pointer is null"));
	}
	
	FProperty* Property = Object->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return TResult<void>::Error(TEXT("PROPERTY_NOT_FOUND"), 
			FString::Printf(TEXT("Property '%s' not found"), *PropertyName));
	}
	
	if (Property->HasAnyPropertyFlags(CPF_EditConst))
	{
		return TResult<void>::Error(TEXT("PROPERTY_READ_ONLY"), 
			FString::Printf(TEXT("Property '%s' is read-only"), *PropertyName));
	}
	
	// Import the text value into the property
	Property->ImportText_InContainer(*Value, Object, Object, 0);
	
	return TResult<void>::Success();
}

TResult<bool> FEnhancedInputReflectionService::ValidateInputActionType(const FString& ClassPath)
{
	auto ClassResult = ResolveClass(ClassPath);
	if (ClassResult.IsError())
	{
		return TResult<bool>::Success(false);
	}
	
	UClass* Class = ClassResult.GetValue();
	return TResult<bool>::Success(Class->IsChildOf<UInputAction>());
}

TResult<bool> FEnhancedInputReflectionService::ValidateModifierType(const FString& ClassPath)
{
	auto ClassResult = ResolveClass(ClassPath);
	if (ClassResult.IsError())
	{
		return TResult<bool>::Success(false);
	}
	
	UClass* Class = ClassResult.GetValue();
	return TResult<bool>::Success(Class->IsChildOf<UInputModifier>());
}

TResult<bool> FEnhancedInputReflectionService::ValidateTriggerType(const FString& ClassPath)
{
	auto ClassResult = ResolveClass(ClassPath);
	if (ClassResult.IsError())
	{
		return TResult<bool>::Success(false);
	}
	
	UClass* Class = ClassResult.GetValue();
	return TResult<bool>::Success(Class->IsChildOf<UInputTrigger>());
}

TResult<bool> FEnhancedInputReflectionService::ValidatePropertyExists(
	UClass* Class, const FString& PropertyName)
{
	if (!Class)
	{
		return TResult<bool>::Success(false);
	}
	
	return TResult<bool>::Success(Class->FindPropertyByName(*PropertyName) != nullptr);
}

void FEnhancedInputReflectionService::CacheType(const FString& Key, const FEnhancedInputTypeInfo& TypeInfo)
{
	TypeCache.Add(Key, TypeInfo);
}

TResult<FEnhancedInputTypeInfo> FEnhancedInputReflectionService::GetOrDiscoverType(const FString& ClassPath)
{
	if (TypeCache.Contains(ClassPath))
	{
		return TResult<FEnhancedInputTypeInfo>::Success(TypeCache[ClassPath]);
	}
	
	// Discover the type via reflection
	FEnhancedInputTypeInfo TypeInfo;
	TypeInfo.ClassPath = ClassPath;
	
	auto ClassResult = ResolveClass(ClassPath);
	if (ClassResult.IsSuccess())
	{
		UClass* Class = ClassResult.GetValue();
		TypeInfo.DisplayName = Class->GetDisplayNameText().ToString();
		TypeInfo.Description = Class->GetToolTipText().ToString();
		
		// Determine type category
		if (Class->IsChildOf<UInputAction>())
		{
			TypeInfo.Category = TEXT("InputAction");
			TypeInfo.bCanCreateAsset = true;
		}
		else if (Class->IsChildOf<UInputMappingContext>())
		{
			TypeInfo.Category = TEXT("MappingContext");
			TypeInfo.bCanCreateAsset = true;
		}
		else if (Class->IsChildOf<UInputModifier>())
		{
			TypeInfo.Category = TEXT("Modifier");
		}
		else if (Class->IsChildOf<UInputTrigger>())
		{
			TypeInfo.Category = TEXT("Trigger");
		}
		
		CacheType(ClassPath, TypeInfo);
		return TResult<FEnhancedInputTypeInfo>::Success(TypeInfo);
	}
	
	return TResult<FEnhancedInputTypeInfo>::Error(TEXT("CLASS_RESOLUTION_FAILED"), 
		FString::Printf(TEXT("Could not resolve class: %s"), *ClassPath));
}

void FEnhancedInputReflectionService::ClearCache()
{
	TypeCache.Empty();
	PropertyCache.Empty();
}
