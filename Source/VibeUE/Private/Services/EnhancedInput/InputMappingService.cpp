// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Services/EnhancedInput/InputMappingService.h"
#include "Services/EnhancedInput/EnhancedInputReflectionService.h"
#include "Services/EnhancedInput/EnhancedInputServices.h"
#include "Core/ServiceContext.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "PackageTools.h"
#include "AssetToolsModule.h"
#include "Factories/DataAssetFactory.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"

FInputMappingService::FInputMappingService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

void FInputMappingService::Initialize()
{
	FServiceBase::Initialize();
	LogInfo(TEXT("InputMappingService initialized"));
}

void FInputMappingService::Shutdown()
{
	FServiceBase::Shutdown();
}

TResult<UInputMappingContext*> FInputMappingService::CreateMappingContext(const FString& ContextName, const FString& AssetPath, int32 Priority)
{
	// Create package for the asset
	FString PackageName = AssetPath;
	if (!PackageName.StartsWith(TEXT("/Game")))
	{
		PackageName = TEXT("/Game/") + PackageName;
	}
	
	// Check if asset already exists (including partially loaded assets)
	FString ExistingAssetPath = PackageName + TEXT(".") + ContextName;
	if (UInputMappingContext* ExistingContext = LoadObject<UInputMappingContext>(nullptr, *ExistingAssetPath))
	{
		// Check if fully loaded
		UPackage* ExistingPackage = ExistingContext->GetOutermost();
		if (ExistingPackage && ExistingPackage->HasAnyPackageFlags(PKG_ReloadingForCooker | PKG_NewlyCreated) == false)
		{
			LogInfo(FString::Printf(TEXT("Mapping context already exists, returning existing: %s"), *ExistingAssetPath));
			return TResult<UInputMappingContext*>::Success(ExistingContext);
		}
	}
	
	// Check if package exists on disk but is partially loaded - need to fully load or delete first
	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	if (FPaths::FileExists(PackageFileName))
	{
		// Try to fully load the existing package
		UPackage* ExistingPackage = LoadPackage(nullptr, *PackageName, LOAD_None);
		if (ExistingPackage)
		{
			// Check if it has a valid mapping context
			UInputMappingContext* ExistingContext = FindObject<UInputMappingContext>(ExistingPackage, *ContextName);
			if (ExistingContext)
			{
				LogInfo(FString::Printf(TEXT("Loaded existing mapping context from disk: %s"), *PackageName));
				return TResult<UInputMappingContext*>::Success(ExistingContext);
			}
			// Package exists but doesn't contain the expected asset - this is a problem
			return TResult<UInputMappingContext*>::Error(VibeUE::ErrorCodes::ASSET_CREATE_FAILED,
				FString::Printf(TEXT("Package exists but does not contain mapping context '%s'. Delete the asset first."), *ContextName));
		}
		else
		{
			// Package file exists but couldn't load - corrupted or partially saved
			return TResult<UInputMappingContext*>::Error(VibeUE::ErrorCodes::ASSET_CREATE_FAILED,
				FString::Printf(TEXT("Package file exists but could not be loaded: %s. Delete the file manually or use manage_asset(action='delete')."), *PackageFileName));
		}
	}
	
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return TResult<UInputMappingContext*>::Error(VibeUE::ErrorCodes::ASSET_CREATE_FAILED,
			FString::Printf(TEXT("Failed to create package: %s"), *PackageName));
	}
	
	// Ensure the package is fully loaded before creating objects in it
	if (Package->HasAnyPackageFlags(PKG_ReloadingForCooker))
	{
		return TResult<UInputMappingContext*>::Error(VibeUE::ErrorCodes::ASSET_CREATE_FAILED,
			FString::Printf(TEXT("Package is being reloaded, cannot create asset: %s"), *PackageName));
	}
	
	// Create the Input Mapping Context
	UInputMappingContext* NewContext = NewObject<UInputMappingContext>(Package, *ContextName, RF_Public | RF_Standalone);
	if (!NewContext)
	{
		return TResult<UInputMappingContext*>::Error(VibeUE::ErrorCodes::ASSET_CREATE_FAILED,
			TEXT("Failed to create InputMappingContext object"));
	}
	
	// Mark package dirty and save
	Package->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewContext);
	
	// Save the package
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, NewContext, *PackageFileName, SaveArgs))
	{
		return TResult<UInputMappingContext*>::Error(VibeUE::ErrorCodes::ASSET_SAVE_FAILED,
			TEXT("Failed to save InputMappingContext package"));
	}
	
	return TResult<UInputMappingContext*>::Success(NewContext);
}

// NOTE: DeleteMappingContext removed - use manage_asset(action="delete") instead

TResult<FEnhancedInputMappingInfo> FInputMappingService::GetContextInfo(const FString& ContextPath)
{
	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<FEnhancedInputMappingInfo>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}
	
	FEnhancedInputMappingInfo Info;
	Info.AssetPath = ContextPath;
	Info.AssetName = MappingContext->GetName();
	Info.MappingCount = MappingContext->GetMappings().Num();
	
	return TResult<FEnhancedInputMappingInfo>::Success(Info);
}

TResult<int32> FInputMappingService::AddInputMapping(const FString& ContextPath, const FString& ActionPath, const FString& KeyName,
	bool bShift, bool bCtrl, bool bAlt, bool bCmd)
{
	// Load the mapping context
	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<int32>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}
	
	// Load the input action
	UInputAction* Action = LoadObject<UInputAction>(nullptr, *ActionPath);
	if (!Action)
	{
		return TResult<int32>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Input action not found: %s"), *ActionPath));
	}
	
	// Find the key
	FKey Key = FKey(*KeyName);
	if (!Key.IsValid())
	{
		return TResult<int32>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("Invalid key name: %s"), *KeyName));
	}
	
	// Create the mapping
	FEnhancedActionKeyMapping& NewMapping = MappingContext->MapKey(Action, Key);
	
	// TODO: Add support for modifiers (shift, ctrl, alt, cmd) when needed
	// For now, basic key mappings work without modifiers
	
	// Mark dirty
	MappingContext->MarkPackageDirty();
	
	// Return the index
	return TResult<int32>::Success(MappingContext->GetMappings().Num() - 1);
}

TResult<bool> FInputMappingService::RemoveInputMapping(const FString& ContextPath, int32 MappingIndex)
{
	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}
	
	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	if (!Mappings.IsValidIndex(MappingIndex))
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("Invalid mapping index: %d"), MappingIndex));
	}
	
	// Remove the mapping
	MappingContext->UnmapKey(Mappings[MappingIndex].Action, Mappings[MappingIndex].Key);
	MappingContext->MarkPackageDirty();
	
	return TResult<bool>::Success(true);
}

TResult<TArray<FEnhancedInputPropertyInfo>> FInputMappingService::GetContextMappings(const FString& ContextPath)
{
	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<TArray<FEnhancedInputPropertyInfo>>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}
	
	TArray<FEnhancedInputPropertyInfo> Mappings;
	const TArray<FEnhancedActionKeyMapping>& ContextMappings = MappingContext->GetMappings();
	
	for (int32 i = 0; i < ContextMappings.Num(); ++i)
	{
		const FEnhancedActionKeyMapping& Mapping = ContextMappings[i];
		FEnhancedInputPropertyInfo Info;
		Info.Name = FString::Printf(TEXT("Mapping_%d"), i);
		Info.DisplayName = Mapping.Action ? Mapping.Action->GetName() : TEXT("None");
		Info.TypePath = Mapping.Key.GetDisplayName().ToString();
		Info.DefaultValue = FString::Printf(TEXT("%s -> %s"), *Mapping.Key.ToString(), 
			Mapping.Action ? *Mapping.Action->GetName() : TEXT("None"));
		Mappings.Add(Info);
	}
	
	return TResult<TArray<FEnhancedInputPropertyInfo>>::Success(Mappings);
}

TResult<bool> FInputMappingService::SetContextProperty(const FString& ContextPath, const FString& PropertyName, const FString& PropertyValue)
{
	// Load the mapping context asset
	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}

	// Use reflection to set the property
	FProperty* Property = MappingContext->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("Property '%s' not found on InputMappingContext"), *PropertyName));
	}

	// Set the property value
	void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(MappingContext);
	
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
		// Get the underlying enum
		UEnum* Enum = EnumProp->GetEnum();
		if (!Enum)
		{
			return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
				FString::Printf(TEXT("Invalid enum for property '%s'"), *PropertyName));
		}
		
		// Try to find the enum value by name
		int64 EnumValue = Enum->GetValueByNameString(PropertyValue);
		if (EnumValue == INDEX_NONE)
		{
			// Try by index if name lookup failed
			EnumValue = FCString::Atoi64(*PropertyValue);
		}
		
		// Set the enum value
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		UnderlyingProp->SetIntPropertyValue(PropertyAddress, EnumValue);
	}
	else if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		// Handle byte enums (legacy enum style)
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

	// Mark the asset as modified
	MappingContext->MarkPackageDirty();
	
	return TResult<bool>::Success(true);
}

TResult<FString> FInputMappingService::GetContextProperty(const FString& ContextPath, const FString& PropertyName)
{
	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<FString>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}
	
	FProperty* Property = MappingContext->GetClass()->FindPropertyByName(*PropertyName);
	if (!Property)
	{
		return TResult<FString>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("Property '%s' not found"), *PropertyName));
	}
	
	void* PropertyAddress = Property->ContainerPtrToValuePtr<void>(MappingContext);
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
	else
	{
		return TResult<FString>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Property type not supported"));
	}
	
	return TResult<FString>::Success(ValueStr);
}

TResult<bool> FInputMappingService::ValidateContextConfiguration(const FString& ContextPath)
{
	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}
	
	// Validate that all mapped actions are valid
	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		if (!Mapping.Action)
		{
			return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
				TEXT("Context has mapping with null action"));
		}
		if (!Mapping.Key.IsValid())
		{
			return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID,
				TEXT("Context has mapping with invalid key"));
		}
	}
	
	return TResult<bool>::Success(true);
}

// NOTE: DuplicateMappingContext removed - use manage_asset(action="duplicate") instead

TResult<TArray<FString>> FInputMappingService::FindAllMappingContexts(const FString& SearchCriteria)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	
	// Search for InputMappingContext assets
	FARFilter Filter;
	Filter.ClassPaths.Add(UInputMappingContext::StaticClass()->GetClassPathName());
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

TResult<TArray<FString>> FInputMappingService::GetAvailableInputKeys(const FString& SearchFilter)
{
	TArray<FString> Keys;
	
	// Get all valid keys from the engine
	TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);
	
	for (const FKey& Key : AllKeys)
	{
		FString KeyName = Key.ToString();
		if (SearchFilter.IsEmpty() || KeyName.Contains(SearchFilter))
		{
			Keys.Add(KeyName);
		}
	}
	
	return TResult<TArray<FString>>::Success(Keys);
}

TResult<FEnhancedInputUsageInfo> FInputMappingService::AnalyzeContextUsage(const FString& ContextPath)
{
	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<FEnhancedInputUsageInfo>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND,
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}
	
	FEnhancedInputUsageInfo Info;
	Info.TotalMappings = MappingContext->GetMappings().Num();
	
	// Count unique actions and keys
	TSet<const UInputAction*> UniqueActions;
	TSet<FKey> UniqueKeys;
	
	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		if (Mapping.Action)
		{
			UniqueActions.Add(Mapping.Action.Get());
		}
		if (Mapping.Key.IsValid())
		{
			UniqueKeys.Add(Mapping.Key);
		}
	}
	
	Info.UniqueActions = UniqueActions.Num();
	Info.UniqueKeys = UniqueKeys.Num();
	
	return TResult<FEnhancedInputUsageInfo>::Success(Info);
}

TResult<TArray<FEnhancedInputKeyConflict>> FInputMappingService::DetectKeyConflicts(const TArray<FString>& ContextPaths)
{
	TArray<FEnhancedInputKeyConflict> Conflicts;
	TMap<FKey, TArray<FString>> KeyToActions;
	
	// Gather all key mappings from all contexts
	for (const FString& ContextPath : ContextPaths)
	{
		UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
		if (!MappingContext)
		{
			continue;
		}
		
		for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
		{
			if (Mapping.Key.IsValid() && Mapping.Action)
			{
				FString ActionName = Mapping.Action->GetName();
				KeyToActions.FindOrAdd(Mapping.Key).AddUnique(ActionName);
			}
		}
	}
	
	// Find conflicts (keys used by multiple actions)
	for (const auto& Pair : KeyToActions)
	{
		if (Pair.Value.Num() > 1)
		{
			FEnhancedInputKeyConflict Conflict;
			Conflict.Key = Pair.Key.ToString();
			Conflict.ConflictingActions = Pair.Value;
			Conflicts.Add(Conflict);
		}
	}
	
	return TResult<TArray<FEnhancedInputKeyConflict>>::Success(Conflicts);
}

// ============================================================================
// Modifier Management
// ============================================================================

TResult<bool> FInputMappingService::AddModifierToMapping(const FString& ContextPath, int32 MappingIndex, UInputModifier* Modifier)
{
	if (!Modifier)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Modifier cannot be null"));
	}

	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	if (!Mappings.IsValidIndex(MappingIndex))
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
			FString::Printf(TEXT("Invalid mapping index: %d. Context has %d mappings."), MappingIndex, Mappings.Num()));
	}

	// Get mutable reference to the mapping using the non-const accessor
	FEnhancedActionKeyMapping& Mapping = MappingContext->GetMapping(MappingIndex);
	Mapping.Modifiers.Add(Modifier);
	MappingContext->MarkPackageDirty();

	return TResult<bool>::Success(true);
}

TResult<bool> FInputMappingService::RemoveModifierFromMapping(const FString& ContextPath, int32 MappingIndex, int32 ModifierIndex)
{
	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	if (!Mappings.IsValidIndex(MappingIndex))
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
			FString::Printf(TEXT("Invalid mapping index: %d"), MappingIndex));
	}

	// Get mutable reference
	FEnhancedActionKeyMapping& Mapping = MappingContext->GetMapping(MappingIndex);
	if (!Mapping.Modifiers.IsValidIndex(ModifierIndex))
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
			FString::Printf(TEXT("Invalid modifier index: %d. Mapping has %d modifiers."), ModifierIndex, Mapping.Modifiers.Num()));
	}

	Mapping.Modifiers.RemoveAt(ModifierIndex);
	MappingContext->MarkPackageDirty();

	return TResult<bool>::Success(true);
}

TResult<TArray<FEnhancedInputModifierInstanceInfo>> FInputMappingService::GetMappingModifiers(const FString& ContextPath, int32 MappingIndex)
{
	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<TArray<FEnhancedInputModifierInstanceInfo>>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	if (!Mappings.IsValidIndex(MappingIndex))
	{
		return TResult<TArray<FEnhancedInputModifierInstanceInfo>>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
			FString::Printf(TEXT("Invalid mapping index: %d"), MappingIndex));
	}

	TArray<FEnhancedInputModifierInstanceInfo> ModifierInfos;
	const TArray<TObjectPtr<UInputModifier>>& Modifiers = Mappings[MappingIndex].Modifiers;
	
	for (int32 i = 0; i < Modifiers.Num(); i++)
	{
		const UInputModifier* Modifier = Modifiers[i];
		if (Modifier)
		{
			FEnhancedInputModifierInstanceInfo Info;
			Info.Index = i;
			Info.TypeName = Modifier->GetClass()->GetName();
			Info.DisplayName = Modifier->GetClass()->GetDisplayNameText().ToString();
			
			// Get properties via reflection
			for (TFieldIterator<FProperty> PropIt(Modifier->GetClass()); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;
				if (Property && !Property->HasAnyPropertyFlags(CPF_Transient | CPF_EditorOnly))
				{
					FString ValueStr;
					const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Modifier);
					Property->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
					Info.Properties.Add(Property->GetName(), ValueStr);
				}
			}
			
			ModifierInfos.Add(Info);
		}
	}

	return TResult<TArray<FEnhancedInputModifierInstanceInfo>>::Success(ModifierInfos);
}

TResult<TArray<FString>> FInputMappingService::GetAvailableModifierTypes()
{
	TArray<FString> ModifierTypes;
	
	// Find all UInputModifier subclasses
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UInputModifier::StaticClass()) && 
			Class != UInputModifier::StaticClass() &&
			!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			// Extract clean type name (remove UInputModifier prefix if present)
			FString TypeName = Class->GetName();
			if (TypeName.StartsWith(TEXT("InputModifier")))
			{
				TypeName = TypeName.RightChop(13); // Remove "InputModifier" prefix
			}
			ModifierTypes.Add(TypeName);
		}
	}
	
	ModifierTypes.Sort();
	return TResult<TArray<FString>>::Success(ModifierTypes);
}

TResult<UInputModifier*> FInputMappingService::CreateModifier(const FString& ModifierTypeName)
{
	UClass* ModifierClass = FindModifierClassInternal(ModifierTypeName);
	if (!ModifierClass)
	{
		return TResult<UInputModifier*>::Error(VibeUE::ErrorCodes::TYPE_NOT_FOUND, 
			FString::Printf(TEXT("Modifier type not found: %s"), *ModifierTypeName));
	}

	UInputModifier* NewModifier = NewObject<UInputModifier>(GetTransientPackage(), ModifierClass);
	if (!NewModifier)
	{
		return TResult<UInputModifier*>::Error(VibeUE::ErrorCodes::CREATION_FAILED, 
			FString::Printf(TEXT("Failed to create modifier of type: %s"), *ModifierTypeName));
	}

	return TResult<UInputModifier*>::Success(NewModifier);
}

UClass* FInputMappingService::FindModifierClassInternal(const FString& TypeName)
{
	// Try various naming patterns
	TArray<FString> PossibleNames = {
		FString::Printf(TEXT("InputModifier%s"), *TypeName),
		FString::Printf(TEXT("UInputModifier%s"), *TypeName),
		TypeName,
		FString::Printf(TEXT("U%s"), *TypeName)
	};

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UInputModifier::StaticClass()) && 
			!Class->HasAnyClassFlags(CLASS_Abstract))
		{
			FString ClassName = Class->GetName();
			for (const FString& PossibleName : PossibleNames)
			{
				if (ClassName.Equals(PossibleName, ESearchCase::IgnoreCase))
				{
					return Class;
				}
			}
		}
	}
	
	return nullptr;
}

// ============================================================================
// Trigger Management
// ============================================================================

TResult<bool> FInputMappingService::AddTriggerToMapping(const FString& ContextPath, int32 MappingIndex, UInputTrigger* Trigger)
{
	if (!Trigger)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Trigger cannot be null"));
	}

	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	if (!Mappings.IsValidIndex(MappingIndex))
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
			FString::Printf(TEXT("Invalid mapping index: %d. Context has %d mappings."), MappingIndex, Mappings.Num()));
	}

	// Get mutable reference
	FEnhancedActionKeyMapping& Mapping = MappingContext->GetMapping(MappingIndex);
	Mapping.Triggers.Add(Trigger);
	MappingContext->MarkPackageDirty();

	return TResult<bool>::Success(true);
}

TResult<bool> FInputMappingService::RemoveTriggerFromMapping(const FString& ContextPath, int32 MappingIndex, int32 TriggerIndex)
{
	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	if (!Mappings.IsValidIndex(MappingIndex))
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
			FString::Printf(TEXT("Invalid mapping index: %d"), MappingIndex));
	}

	// Get mutable reference
	FEnhancedActionKeyMapping& Mapping = MappingContext->GetMapping(MappingIndex);
	if (!Mapping.Triggers.IsValidIndex(TriggerIndex))
	{
		return TResult<bool>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
			FString::Printf(TEXT("Invalid trigger index: %d. Mapping has %d triggers."), TriggerIndex, Mapping.Triggers.Num()));
	}

	Mapping.Triggers.RemoveAt(TriggerIndex);
	MappingContext->MarkPackageDirty();

	return TResult<bool>::Success(true);
}

TResult<TArray<FEnhancedInputTriggerInstanceInfo>> FInputMappingService::GetMappingTriggers(const FString& ContextPath, int32 MappingIndex)
{
	UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, *ContextPath);
	if (!MappingContext)
	{
		return TResult<TArray<FEnhancedInputTriggerInstanceInfo>>::Error(VibeUE::ErrorCodes::ASSET_NOT_FOUND, 
			FString::Printf(TEXT("Mapping context not found: %s"), *ContextPath));
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	if (!Mappings.IsValidIndex(MappingIndex))
	{
		return TResult<TArray<FEnhancedInputTriggerInstanceInfo>>::Error(VibeUE::ErrorCodes::PARAM_INVALID, 
			FString::Printf(TEXT("Invalid mapping index: %d"), MappingIndex));
	}

	TArray<FEnhancedInputTriggerInstanceInfo> TriggerInfos;
	const TArray<TObjectPtr<UInputTrigger>>& Triggers = Mappings[MappingIndex].Triggers;
	
	for (int32 i = 0; i < Triggers.Num(); i++)
	{
		const UInputTrigger* Trigger = Triggers[i];
		if (Trigger)
		{
			FEnhancedInputTriggerInstanceInfo Info;
			Info.Index = i;
			Info.TypeName = Trigger->GetClass()->GetName();
			Info.DisplayName = Trigger->GetClass()->GetDisplayNameText().ToString();
			
			// Get properties via reflection
			for (TFieldIterator<FProperty> PropIt(Trigger->GetClass()); PropIt; ++PropIt)
			{
				FProperty* Property = *PropIt;
				if (Property && !Property->HasAnyPropertyFlags(CPF_Transient | CPF_EditorOnly))
				{
					FString ValueStr;
					const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Trigger);
					Property->ExportTextItem_Direct(ValueStr, ValuePtr, nullptr, nullptr, PPF_None);
					Info.Properties.Add(Property->GetName(), ValueStr);
				}
			}
			
			TriggerInfos.Add(Info);
		}
	}

	return TResult<TArray<FEnhancedInputTriggerInstanceInfo>>::Success(TriggerInfos);
}

TResult<TArray<FString>> FInputMappingService::GetAvailableTriggerTypes()
{
	TArray<FString> TriggerTypes;
	
	// Find all UInputTrigger subclasses
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UInputTrigger::StaticClass()) && 
			Class != UInputTrigger::StaticClass() &&
			!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			// Extract clean type name (remove UInputTrigger prefix if present)
			FString TypeName = Class->GetName();
			if (TypeName.StartsWith(TEXT("InputTrigger")))
			{
				TypeName = TypeName.RightChop(12); // Remove "InputTrigger" prefix
			}
			TriggerTypes.Add(TypeName);
		}
	}
	
	TriggerTypes.Sort();
	return TResult<TArray<FString>>::Success(TriggerTypes);
}

TResult<UInputTrigger*> FInputMappingService::CreateTrigger(const FString& TriggerTypeName)
{
	UClass* TriggerClass = FindTriggerClassInternal(TriggerTypeName);
	if (!TriggerClass)
	{
		return TResult<UInputTrigger*>::Error(VibeUE::ErrorCodes::TYPE_NOT_FOUND, 
			FString::Printf(TEXT("Trigger type not found: %s"), *TriggerTypeName));
	}

	UInputTrigger* NewTrigger = NewObject<UInputTrigger>(GetTransientPackage(), TriggerClass);
	if (!NewTrigger)
	{
		return TResult<UInputTrigger*>::Error(VibeUE::ErrorCodes::CREATION_FAILED, 
			FString::Printf(TEXT("Failed to create trigger of type: %s"), *TriggerTypeName));
	}

	return TResult<UInputTrigger*>::Success(NewTrigger);
}

UClass* FInputMappingService::FindTriggerClassInternal(const FString& TypeName)
{
	// Try various naming patterns
	TArray<FString> PossibleNames = {
		FString::Printf(TEXT("InputTrigger%s"), *TypeName),
		FString::Printf(TEXT("UInputTrigger%s"), *TypeName),
		TypeName,
		FString::Printf(TEXT("U%s"), *TypeName)
	};

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UInputTrigger::StaticClass()) && 
			!Class->HasAnyClassFlags(CLASS_Abstract))
		{
			FString ClassName = Class->GetName();
			for (const FString& PossibleName : PossibleNames)
			{
				if (ClassName.Equals(PossibleName, ESearchCase::IgnoreCase))
				{
					return Class;
				}
			}
		}
	}
	
	return nullptr;
}
