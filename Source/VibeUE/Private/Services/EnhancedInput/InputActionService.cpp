// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Services/EnhancedInput/InputActionService.h"
#include "Services/EnhancedInput/EnhancedInputReflectionService.h"
#include "Services/EnhancedInput/EnhancedInputServices.h"
#include "Core/ServiceContext.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "PackageTools.h"
#include "AssetToolsModule.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "EditorAssetLibrary.h"

FInputActionService::FInputActionService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

void FInputActionService::Initialize()
{
	FServiceBase::Initialize();
	LogInfo(TEXT("InputActionService initialized"));
}

void FInputActionService::Shutdown()
{
	FServiceBase::Shutdown();
}

TResult<UInputAction*> FInputActionService::CreateInputAction(const FString& AssetName, const FString& AssetPath, EInputActionValueType ValueType)
{
	// Validate inputs
	if (AssetName.IsEmpty())
	{
		return TResult<UInputAction*>::Error(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("AssetName cannot be empty"));
	}
	if (AssetPath.IsEmpty())
	{
		return TResult<UInputAction*>::Error(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("AssetPath cannot be empty"));
	}
	
	// Ensure path starts with /Game
	FString PackageName = AssetPath;
	if (!PackageName.StartsWith(TEXT("/Game")))
	{
		PackageName = TEXT("/Game/") + PackageName;
	}
	
	// Create package for the asset
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return TResult<UInputAction*>::Error(VibeUE::ErrorCodes::ASSET_CREATE_FAILED,
			FString::Printf(TEXT("Failed to create package: %s"), *PackageName));
	}
	
	// Create the Input Action
	UInputAction* NewAction = NewObject<UInputAction>(Package, *AssetName, RF_Public | RF_Standalone);
	if (!NewAction)
	{
		return TResult<UInputAction*>::Error(VibeUE::ErrorCodes::ASSET_CREATE_FAILED,
			TEXT("Failed to create InputAction object"));
	}
	
	// Set the value type
	NewAction->ValueType = ValueType;
	
	// Mark package dirty and register with asset registry
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewAction);
	
	// Save the package
	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, NewAction, *PackageFileName, SaveArgs))
	{
		return TResult<UInputAction*>::Error(VibeUE::ErrorCodes::ASSET_SAVE_FAILED,
			TEXT("Failed to save InputAction package"));
	}
	
	LogInfo(FString::Printf(TEXT("Created InputAction: %s"), *PackageName));
	
	return TResult<UInputAction*>::Success(NewAction);
}

// NOTE: DeleteInputAction removed - use manage_asset(action="delete") instead

TResult<FEnhancedInputActionInfo> FInputActionService::GetActionInfo(const FString& ActionPath)
{
	if (ActionPath.IsEmpty())
	{
		return TResult<FEnhancedInputActionInfo>::Error(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("ActionPath cannot be empty"));
	}
	
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!Action)
	{
		return TResult<FEnhancedInputActionInfo>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Input action not found: %s"), *ActionPath));
	}
	
	FEnhancedInputActionInfo Info;
	Info.AssetPath = ActionPath;
	Info.AssetName = Action->GetName();
	
	// Get value type as string
	switch (Action->ValueType)
	{
		case EInputActionValueType::Boolean:
			Info.ValueType = TEXT("Boolean");
			break;
		case EInputActionValueType::Axis1D:
			Info.ValueType = TEXT("Axis1D");
			break;
		case EInputActionValueType::Axis2D:
			Info.ValueType = TEXT("Axis2D");
			break;
		case EInputActionValueType::Axis3D:
			Info.ValueType = TEXT("Axis3D");
			break;
		default:
			Info.ValueType = TEXT("Unknown");
	}
	
	// Count modifiers and triggers
	Info.ModifierCount = Action->Modifiers.Num();
	Info.TriggerCount = Action->Triggers.Num();
	
	return TResult<FEnhancedInputActionInfo>::Success(Info);
}

TResult<bool> FInputActionService::SetActionProperty(const FString& ActionPath, const FString& PropertyName, const FString& PropertyValue)
{
	if (ActionPath.IsEmpty() || PropertyName.IsEmpty())
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("ActionPath and PropertyName are required"));
	}
	
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!Action)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Input action not found: %s"), *ActionPath));
	}
	
	// Use reflection to find the property
	FProperty* Property = Action->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("Property '%s' not found on InputAction"), *PropertyName));
	}
	
	// Set the property value using reflection
	void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(Action);
	
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		StrProp->SetPropertyValue(PropertyAddress, PropertyValue);
	}
	else if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		TextProp->SetPropertyValue(PropertyAddress, FText::FromString(PropertyValue));
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		IntProp->SetPropertyValue(PropertyAddress, FCString::Atoi(*PropertyValue));
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		FloatProp->SetPropertyValue(PropertyAddress, FCString::Atof(*PropertyValue));
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		BoolProp->SetPropertyValue(PropertyAddress, PropertyValue.ToBool());
	}
	else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		UEnum* Enum = EnumProp->GetEnum();
		if (Enum)
		{
			int64 EnumValue = Enum->GetValueByNameString(PropertyValue);
			if (EnumValue == INDEX_NONE)
			{
				EnumValue = FCString::Atoi64(*PropertyValue);
			}
			FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
			UnderlyingProp->SetIntPropertyValue(PropertyAddress, EnumValue);
		}
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (ByteProp->Enum)
		{
			int64 EnumValue = ByteProp->Enum->GetValueByNameString(PropertyValue);
			if (EnumValue == INDEX_NONE)
			{
				EnumValue = FCString::Atoi64(*PropertyValue);
			}
			ByteProp->SetPropertyValue(PropertyAddress, static_cast<uint8>(EnumValue));
		}
		else
		{
			ByteProp->SetPropertyValue(PropertyAddress, static_cast<uint8>(FCString::Atoi(*PropertyValue)));
		}
	}
	else
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("Property type not supported for '%s'"), *PropertyName));
	}
	
	// Mark dirty
	Action->MarkPackageDirty();
	
	return TResult<bool>::Success(true);
}

TResult<FString> FInputActionService::GetActionProperty(const FString& ActionPath, const FString& PropertyName)
{
	if (ActionPath.IsEmpty() || PropertyName.IsEmpty())
	{
		return TResult<FString>::Error(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("ActionPath and PropertyName are required"));
	}
	
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!Action)
	{
		return TResult<FString>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Input action not found: %s"), *ActionPath));
	}
	
	FProperty* Property = Action->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return TResult<FString>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("Property '%s' not found"), *PropertyName));
	}
	
	void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(Action);
	FString ValueStr;
	
	if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		ValueStr = StrProp->GetPropertyValue(PropertyAddress);
	}
	else if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		ValueStr = TextProp->GetPropertyValue(PropertyAddress).ToString();
	}
	else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
	{
		ValueStr = FString::FromInt(IntProp->GetPropertyValue(PropertyAddress));
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
	{
		ValueStr = FString::SanitizeFloat(FloatProp->GetPropertyValue(PropertyAddress));
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		ValueStr = BoolProp->GetPropertyValue(PropertyAddress) ? TEXT("true") : TEXT("false");
	}
	else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		UEnum* Enum = EnumProp->GetEnum();
		if (Enum)
		{
			FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
			int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(PropertyAddress);
			ValueStr = Enum->GetNameStringByValue(EnumValue);
		}
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (ByteProp->Enum)
		{
			uint8 ByteValue = ByteProp->GetPropertyValue(PropertyAddress);
			ValueStr = ByteProp->Enum->GetNameStringByValue(ByteValue);
		}
		else
		{
			ValueStr = FString::FromInt(ByteProp->GetPropertyValue(PropertyAddress));
		}
	}
	else
	{
		return TResult<FString>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Property type not supported"));
	}
	
	return TResult<FString>::Success(ValueStr);
}

TResult<TArray<FEnhancedInputPropertyInfo>> FInputActionService::GetActionProperties(const FString& ActionPath)
{
	if (ActionPath.IsEmpty())
	{
		return TResult<TArray<FEnhancedInputPropertyInfo>>::Error(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("ActionPath cannot be empty"));
	}
	
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!Action)
	{
		return TResult<TArray<FEnhancedInputPropertyInfo>>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Input action not found: %s"), *ActionPath));
	}
	
	TArray<FEnhancedInputPropertyInfo> Properties;
	
	// Iterate through all properties using reflection
	for (TFieldIterator<FProperty> PropIt(Action->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		
		// Skip internal/private properties
		if (Property->HasAnyPropertyFlags(CPF_DisableEditOnInstance | CPF_Deprecated))
		{
			continue;
		}
		
		FEnhancedInputPropertyInfo Info;
		Info.Name = Property->GetName();
		Info.DisplayName = Property->GetDisplayNameText().ToString();
		Info.TypeName = Property->GetCPPType();
		
		// Get the current value
		void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(Action);
		
		if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
		{
			Info.DefaultValue = StrProp->GetPropertyValue(PropertyAddress);
		}
		else if (FIntProperty* IntProp = CastField<FIntProperty>(Property))
		{
			Info.DefaultValue = FString::FromInt(IntProp->GetPropertyValue(PropertyAddress));
		}
		else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Property))
		{
			Info.DefaultValue = FString::SanitizeFloat(FloatProp->GetPropertyValue(PropertyAddress));
		}
		else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
		{
			Info.DefaultValue = BoolProp->GetPropertyValue(PropertyAddress) ? TEXT("true") : TEXT("false");
		}
		else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			UEnum* Enum = EnumProp->GetEnum();
			if (Enum)
			{
				FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
				int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(PropertyAddress);
				Info.DefaultValue = Enum->GetNameStringByValue(EnumValue);
			}
		}
		else
		{
			Info.DefaultValue = TEXT("<complex>");
		}
		
		Properties.Add(Info);
	}
	
	return TResult<TArray<FEnhancedInputPropertyInfo>>::Success(Properties);
}

TResult<bool> FInputActionService::ValidateActionConfiguration(const FString& ActionPath)
{
	if (ActionPath.IsEmpty())
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_MISSING, TEXT("ActionPath cannot be empty"));
	}
	
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!Action)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Input action not found: %s"), *ActionPath));
	}
	
	// Validate the action configuration
	// Check that the ValueType is valid
	if (Action->ValueType == EInputActionValueType::Boolean ||
		Action->ValueType == EInputActionValueType::Axis1D ||
		Action->ValueType == EInputActionValueType::Axis2D ||
		Action->ValueType == EInputActionValueType::Axis3D)
	{
		return TResult<bool>::Success(true);
	}
	
	return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Invalid ValueType configuration"));
}

// NOTE: DuplicateInputAction removed - use manage_asset(action="duplicate") instead

TResult<TArray<FString>> FInputActionService::FindAllInputActions(const FString& SearchCriteria)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	// Search for InputAction assets
	FARFilter Filter;
	Filter.ClassPaths.Add(UInputAction::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add("/Game");
	Filter.bRecursivePaths = true;
	
	TArray<FAssetData> AssetData;
	AssetRegistry.GetAssets(Filter, AssetData);
	
	TArray<FString> Results;
	for (const FAssetData& Asset : AssetData)
	{
		FString AssetPath = Asset.GetObjectPathString();
		if (SearchCriteria.IsEmpty() || AssetPath.Contains(SearchCriteria))
		{
			Results.Add(AssetPath);
		}
	}
	
	return TResult<TArray<FString>>::Success(Results);
}

TResult<FEnhancedInputActionInfo> FInputActionService::AnalyzeActionUsage(const FString& ActionPath)
{
	// Get the action info first
	auto InfoResult = GetActionInfo(ActionPath);
	if (InfoResult.IsError())
	{
		return InfoResult;
	}
	
	FEnhancedInputActionInfo Info = InfoResult.GetValue();
	
	// Search for mapping contexts that reference this action
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	FARFilter Filter;
	Filter.ClassPaths.Add(UInputMappingContext::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add("/Game");
	Filter.bRecursivePaths = true;
	
	TArray<FAssetData> ContextAssets;
	AssetRegistry.GetAssets(Filter, ContextAssets);
	
	int32 ActionUsageCount = 0;
	for (const FAssetData& ContextAsset : ContextAssets)
	{
		UInputMappingContext* MappingContext = Cast<UInputMappingContext>(ContextAsset.GetAsset());
		if (MappingContext)
		{
			for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
			{
				if (Mapping.Action && Mapping.Action->GetPathName() == ActionPath)
				{
					ActionUsageCount++;
				}
			}
		}
	}
	
	Info.UsageCount = ActionUsageCount;
	
	return TResult<FEnhancedInputActionInfo>::Success(Info);
}

