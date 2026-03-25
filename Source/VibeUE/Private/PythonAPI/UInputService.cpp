// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UInputService.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"

// Enhanced Input includes
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "EnhancedInputDeveloperSettings.h"

// =================================================================
// Helper Methods
// =================================================================

UInputAction* UInputService::LoadInputAction(const FString& ActionPath)
{
	UInputAction* Action = Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(ActionPath));
	if (!Action)
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService: Failed to load Input Action: %s"), *ActionPath);
	}
	return Action;
}

UInputMappingContext* UInputService::LoadMappingContext(const FString& ContextPath)
{
	UInputMappingContext* Context = Cast<UInputMappingContext>(UEditorAssetLibrary::LoadAsset(ContextPath));
	if (!Context)
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService: Failed to load Mapping Context: %s"), *ContextPath);
	}
	return Context;
}

FKey UInputService::FindKeyByName(const FString& KeyName)
{
	// Try to find the key by name
	FKey Key = FKey(*KeyName);
	if (Key.IsValid())
	{
		return Key;
	}
	
	// Try with "Keys::" prefix removed
	FString CleanName = KeyName;
	CleanName.RemoveFromStart(TEXT("Keys::"));
	CleanName.RemoveFromStart(TEXT("EKeys::"));
	
	Key = FKey(*CleanName);
	return Key;
}

// =================================================================
// Reflection
// =================================================================

FInputTypeDiscoveryResult UInputService::DiscoverTypes()
{
	FInputTypeDiscoveryResult Result;
	
	// Action value types
	Result.ActionValueTypes.Add(TEXT("Boolean"));
	Result.ActionValueTypes.Add(TEXT("Axis1D"));
	Result.ActionValueTypes.Add(TEXT("Axis2D"));
	Result.ActionValueTypes.Add(TEXT("Axis3D"));
	
	// Discover modifier types
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UInputModifier::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			FString ClassName = Class->GetName();
			ClassName.RemoveFromStart(TEXT("InputModifier"));
			if (!ClassName.IsEmpty())
			{
				Result.ModifierTypes.Add(ClassName);
			}
		}
	}
	
	// Discover trigger types
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UInputTrigger::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			FString ClassName = Class->GetName();
			ClassName.RemoveFromStart(TEXT("InputTrigger"));
			if (!ClassName.IsEmpty())
			{
				Result.TriggerTypes.Add(ClassName);
			}
		}
	}
	
	return Result;
}

// =================================================================
// Action Management
// =================================================================

FInputCreateResult UInputService::CreateAction(
	const FString& ActionName,
	const FString& AssetPath,
	const FString& ValueType)
{
	FInputCreateResult Result;
	
	// Normalize path
	FString BasePath = AssetPath;
	if (!BasePath.StartsWith(TEXT("/Game")))
	{
		BasePath = TEXT("/Game/") + BasePath;
	}
	if (BasePath.EndsWith(TEXT("/")))
	{
		BasePath = BasePath.LeftChop(1);
	}
	
	FString FullPath = BasePath / ActionName;
	
	// Check if already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Input Action '%s' already exists"), *FullPath);
		return Result;
	}
	
	// Create the asset
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UInputAction* NewAction = Cast<UInputAction>(AssetTools.CreateAsset(ActionName, BasePath, UInputAction::StaticClass(), nullptr));
	
	if (!NewAction)
	{
		Result.ErrorMessage = TEXT("Failed to create Input Action asset");
		return Result;
	}
	
	// Set value type
	if (ValueType.Equals(TEXT("Boolean"), ESearchCase::IgnoreCase) || ValueType.Equals(TEXT("Digital"), ESearchCase::IgnoreCase))
	{
		NewAction->ValueType = EInputActionValueType::Boolean;
	}
	else if (ValueType.Equals(TEXT("Axis1D"), ESearchCase::IgnoreCase))
	{
		NewAction->ValueType = EInputActionValueType::Axis1D;
	}
	else if (ValueType.Equals(TEXT("Axis2D"), ESearchCase::IgnoreCase))
	{
		NewAction->ValueType = EInputActionValueType::Axis2D;
	}
	else if (ValueType.Equals(TEXT("Axis3D"), ESearchCase::IgnoreCase))
	{
		NewAction->ValueType = EInputActionValueType::Axis3D;
	}
	
	// Save the asset
	UEditorAssetLibrary::SaveAsset(FullPath, false);
	
	Result.bSuccess = true;
	Result.AssetPath = FullPath + TEXT(".") + ActionName;
	
	return Result;
}

TArray<FString> UInputService::ListInputActions()
{
	TArray<FString> ActionPaths;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/EnhancedInput.InputAction")));

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		ActionPaths.Add(AssetData.GetObjectPathString());
	}

	return ActionPaths;
}

bool UInputService::GetInputActionInfo(const FString& ActionPath, FInputActionDetailedInfo& OutInfo)
{
	UInputAction* InputAction = LoadInputAction(ActionPath);
	if (!InputAction)
	{
		return false;
	}

	OutInfo.ActionName = InputAction->GetName();
	OutInfo.ActionPath = ActionPath;
	OutInfo.bConsumeInput = InputAction->bConsumeInput;
	OutInfo.bTriggerWhenPaused = InputAction->bTriggerWhenPaused;
	OutInfo.Description = InputAction->ActionDescription.ToString();

	// Get value type
	switch (InputAction->ValueType)
	{
	case EInputActionValueType::Boolean:
		OutInfo.ValueType = TEXT("Boolean");
		break;
	case EInputActionValueType::Axis1D:
		OutInfo.ValueType = TEXT("Axis1D");
		break;
	case EInputActionValueType::Axis2D:
		OutInfo.ValueType = TEXT("Axis2D");
		break;
	case EInputActionValueType::Axis3D:
		OutInfo.ValueType = TEXT("Axis3D");
		break;
	default:
		OutInfo.ValueType = TEXT("Unknown");
		break;
	}

	return true;
}

bool UInputService::ConfigureAction(
	const FString& ActionPath,
	bool bConsumeInput,
	bool bTriggerWhenPaused,
	const FString& Description)
{
	UInputAction* InputAction = LoadInputAction(ActionPath);
	if (!InputAction)
	{
		return false;
	}

	InputAction->Modify();
	InputAction->bConsumeInput = bConsumeInput;
	InputAction->bTriggerWhenPaused = bTriggerWhenPaused;
	
	if (!Description.IsEmpty())
	{
		InputAction->ActionDescription = FText::FromString(Description);
	}

	// Save the asset
	UPackage* Package = InputAction->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	return true;
}

// =================================================================
// Mapping Context Management
// =================================================================

FInputCreateResult UInputService::CreateMappingContext(
	const FString& ContextName,
	const FString& AssetPath,
	int32 Priority)
{
	FInputCreateResult Result;
	
	// Normalize path
	FString BasePath = AssetPath;
	if (!BasePath.StartsWith(TEXT("/Game")))
	{
		BasePath = TEXT("/Game/") + BasePath;
	}
	if (BasePath.EndsWith(TEXT("/")))
	{
		BasePath = BasePath.LeftChop(1);
	}
	
	FString FullPath = BasePath / ContextName;
	
	// Check if already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Mapping Context '%s' already exists"), *FullPath);
		return Result;
	}
	
	// Create the asset
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UInputMappingContext* NewContext = Cast<UInputMappingContext>(AssetTools.CreateAsset(ContextName, BasePath, UInputMappingContext::StaticClass(), nullptr));
	
	if (!NewContext)
	{
		Result.ErrorMessage = TEXT("Failed to create Mapping Context asset");
		return Result;
	}
	
	// Save the asset
	UEditorAssetLibrary::SaveAsset(FullPath, false);
	
	Result.bSuccess = true;
	Result.AssetPath = FullPath + TEXT(".") + ContextName;
	
	return Result;
}

TArray<FString> UInputService::ListMappingContexts()
{
	TArray<FString> ContextPaths;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/EnhancedInput.InputMappingContext")));

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		ContextPaths.Add(AssetData.GetObjectPathString());
	}

	return ContextPaths;
}

bool UInputService::GetMappingContextInfo(const FString& ContextPath, FMappingContextDetailedInfo& OutInfo)
{
	UInputMappingContext* MappingContext = LoadMappingContext(ContextPath);
	if (!MappingContext)
	{
		return false;
	}

	OutInfo.ContextName = MappingContext->GetName();
	OutInfo.ContextPath = ContextPath;

	// Get all mapped actions
	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		if (Mapping.Action)
		{
			FString ActionInfo = FString::Printf(TEXT("%s -> %s"),
				*Mapping.Action->GetName(),
				*Mapping.Key.ToString());
			OutInfo.MappedActions.Add(ActionInfo);
		}
	}

	return true;
}

TArray<FKeyMappingInfo> UInputService::GetMappings(const FString& ContextPath)
{
	TArray<FKeyMappingInfo> MappingInfos;
	
	UInputMappingContext* MappingContext = LoadMappingContext(ContextPath);
	if (!MappingContext)
	{
		return MappingInfos;
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	for (int32 i = 0; i < Mappings.Num(); ++i)
	{
		const FEnhancedActionKeyMapping& Mapping = Mappings[i];
		
		FKeyMappingInfo Info;
		Info.MappingIndex = i;
		Info.KeyName = Mapping.Key.ToString();
		
		if (Mapping.Action)
		{
			Info.ActionName = Mapping.Action->GetName();
			Info.ActionPath = Mapping.Action->GetPathName();
		}
		
		Info.ModifierCount = Mapping.Modifiers.Num();
		Info.TriggerCount = Mapping.Triggers.Num();
		
		MappingInfos.Add(Info);
	}

	return MappingInfos;
}

bool UInputService::AddKeyMapping(
	const FString& ContextPath,
	const FString& ActionPath,
	const FString& KeyName)
{
	UInputMappingContext* MappingContext = LoadMappingContext(ContextPath);
	if (!MappingContext)
	{
		return false;
	}

	UInputAction* InputAction = LoadInputAction(ActionPath);
	if (!InputAction)
	{
		return false;
	}

	FKey Key = FindKeyByName(KeyName);
	if (!Key.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService::AddKeyMapping: Invalid key name: %s"), *KeyName);
		return false;
	}

	MappingContext->Modify();
	
	FEnhancedActionKeyMapping NewMapping;
	NewMapping.Action = InputAction;
	NewMapping.Key = Key;
	
	MappingContext->MapKey(InputAction, Key);

	// Save
	UPackage* Package = MappingContext->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	return true;
}

bool UInputService::RemoveMapping(const FString& ContextPath, int32 MappingIndex)
{
	UInputMappingContext* MappingContext = LoadMappingContext(ContextPath);
	if (!MappingContext)
	{
		return false;
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	if (MappingIndex < 0 || MappingIndex >= Mappings.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService::RemoveMapping: Invalid mapping index: %d"), MappingIndex);
		return false;
	}

	MappingContext->Modify();
	MappingContext->UnmapKey(Mappings[MappingIndex].Action, Mappings[MappingIndex].Key);

	// Save
	UPackage* Package = MappingContext->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	return true;
}

TArray<FString> UInputService::GetAvailableKeys(const FString& Filter)
{
	TArray<FString> Keys;
	
	// Get all registered keys
	TArray<FKey> AllKeys;
	EKeys::GetAllKeys(AllKeys);
	
	for (const FKey& Key : AllKeys)
	{
		FString KeyName = Key.ToString();
		
		// Apply filter if provided
		if (Filter.IsEmpty() || KeyName.Contains(Filter, ESearchCase::IgnoreCase))
		{
			Keys.Add(KeyName);
		}
	}
	
	return Keys;
}

// =================================================================
// Modifier Management
// =================================================================

bool UInputService::AddModifier(
	const FString& ContextPath,
	int32 MappingIndex,
	const FString& ModifierType)
{
	UInputMappingContext* MappingContext = LoadMappingContext(ContextPath);
	if (!MappingContext)
	{
		return false;
	}

	TArray<FEnhancedActionKeyMapping>& Mappings = const_cast<TArray<FEnhancedActionKeyMapping>&>(MappingContext->GetMappings());
	if (MappingIndex < 0 || MappingIndex >= Mappings.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService::AddModifier: Invalid mapping index: %d"), MappingIndex);
		return false;
	}

	// Find the modifier class
	FString ClassName = TEXT("InputModifier") + ModifierType;
	UClass* ModifierClass = nullptr;
	
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UInputModifier::StaticClass()) && 
			!Class->HasAnyClassFlags(CLASS_Abstract) &&
			(Class->GetName().Equals(ClassName, ESearchCase::IgnoreCase) ||
			 Class->GetName().Equals(ModifierType, ESearchCase::IgnoreCase)))
		{
			ModifierClass = Class;
			break;
		}
	}
	
	if (!ModifierClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService::AddModifier: Modifier type not found: %s"), *ModifierType);
		return false;
	}

	MappingContext->Modify();
	
	UInputModifier* NewModifier = NewObject<UInputModifier>(MappingContext, ModifierClass);
	if (NewModifier)
	{
		Mappings[MappingIndex].Modifiers.Add(NewModifier);
	}

	// Save
	UPackage* Package = MappingContext->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	return true;
}

bool UInputService::RemoveModifier(
	const FString& ContextPath,
	int32 MappingIndex,
	int32 ModifierIndex)
{
	UInputMappingContext* MappingContext = LoadMappingContext(ContextPath);
	if (!MappingContext)
	{
		return false;
	}

	TArray<FEnhancedActionKeyMapping>& Mappings = const_cast<TArray<FEnhancedActionKeyMapping>&>(MappingContext->GetMappings());
	if (MappingIndex < 0 || MappingIndex >= Mappings.Num())
	{
		return false;
	}

	if (ModifierIndex < 0 || ModifierIndex >= Mappings[MappingIndex].Modifiers.Num())
	{
		return false;
	}

	MappingContext->Modify();
	Mappings[MappingIndex].Modifiers.RemoveAt(ModifierIndex);

	// Save
	UPackage* Package = MappingContext->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	return true;
}

TArray<FInputModifierInfo> UInputService::GetModifiers(
	const FString& ContextPath,
	int32 MappingIndex)
{
	TArray<FInputModifierInfo> ModifierInfos;
	
	UInputMappingContext* MappingContext = LoadMappingContext(ContextPath);
	if (!MappingContext)
	{
		return ModifierInfos;
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	if (MappingIndex < 0 || MappingIndex >= Mappings.Num())
	{
		return ModifierInfos;
	}

	const TArray<TObjectPtr<UInputModifier>>& Modifiers = Mappings[MappingIndex].Modifiers;
	for (int32 i = 0; i < Modifiers.Num(); ++i)
	{
		if (Modifiers[i])
		{
			FInputModifierInfo Info;
			Info.ModifierIndex = i;
			Info.TypeName = Modifiers[i]->GetClass()->GetName();
			Info.DisplayName = Info.TypeName;
			Info.DisplayName.RemoveFromStart(TEXT("InputModifier"));
			ModifierInfos.Add(Info);
		}
	}

	return ModifierInfos;
}

TArray<FString> UInputService::GetAvailableModifierTypes()
{
	TArray<FString> Types;
	
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UInputModifier::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			FString ClassName = Class->GetName();
			ClassName.RemoveFromStart(TEXT("InputModifier"));
			if (!ClassName.IsEmpty())
			{
				Types.Add(ClassName);
			}
		}
	}
	
	return Types;
}

// =================================================================
// Trigger Management
// =================================================================

bool UInputService::AddTrigger(
	const FString& ContextPath,
	int32 MappingIndex,
	const FString& TriggerType)
{
	UInputMappingContext* MappingContext = LoadMappingContext(ContextPath);
	if (!MappingContext)
	{
		return false;
	}

	TArray<FEnhancedActionKeyMapping>& Mappings = const_cast<TArray<FEnhancedActionKeyMapping>&>(MappingContext->GetMappings());
	if (MappingIndex < 0 || MappingIndex >= Mappings.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService::AddTrigger: Invalid mapping index: %d"), MappingIndex);
		return false;
	}

	// Find the trigger class
	FString ClassName = TEXT("InputTrigger") + TriggerType;
	UClass* TriggerClass = nullptr;
	
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UInputTrigger::StaticClass()) && 
			!Class->HasAnyClassFlags(CLASS_Abstract) &&
			(Class->GetName().Equals(ClassName, ESearchCase::IgnoreCase) ||
			 Class->GetName().Equals(TriggerType, ESearchCase::IgnoreCase)))
		{
			TriggerClass = Class;
			break;
		}
	}
	
	if (!TriggerClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService::AddTrigger: Trigger type not found: %s"), *TriggerType);
		return false;
	}

	MappingContext->Modify();
	
	UInputTrigger* NewTrigger = NewObject<UInputTrigger>(MappingContext, TriggerClass);
	if (NewTrigger)
	{
		Mappings[MappingIndex].Triggers.Add(NewTrigger);
	}

	// Save
	UPackage* Package = MappingContext->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	return true;
}

bool UInputService::RemoveTrigger(
	const FString& ContextPath,
	int32 MappingIndex,
	int32 TriggerIndex)
{
	UInputMappingContext* MappingContext = LoadMappingContext(ContextPath);
	if (!MappingContext)
	{
		return false;
	}

	TArray<FEnhancedActionKeyMapping>& Mappings = const_cast<TArray<FEnhancedActionKeyMapping>&>(MappingContext->GetMappings());
	if (MappingIndex < 0 || MappingIndex >= Mappings.Num())
	{
		return false;
	}

	if (TriggerIndex < 0 || TriggerIndex >= Mappings[MappingIndex].Triggers.Num())
	{
		return false;
	}

	MappingContext->Modify();
	Mappings[MappingIndex].Triggers.RemoveAt(TriggerIndex);

	// Save
	UPackage* Package = MappingContext->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	return true;
}

TArray<FInputTriggerInfo> UInputService::GetTriggers(
	const FString& ContextPath,
	int32 MappingIndex)
{
	TArray<FInputTriggerInfo> TriggerInfos;
	
	UInputMappingContext* MappingContext = LoadMappingContext(ContextPath);
	if (!MappingContext)
	{
		return TriggerInfos;
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	if (MappingIndex < 0 || MappingIndex >= Mappings.Num())
	{
		return TriggerInfos;
	}

	const TArray<TObjectPtr<UInputTrigger>>& Triggers = Mappings[MappingIndex].Triggers;
	for (int32 i = 0; i < Triggers.Num(); ++i)
	{
		if (Triggers[i])
		{
			FInputTriggerInfo Info;
			Info.TriggerIndex = i;
			Info.TypeName = Triggers[i]->GetClass()->GetName();
			Info.DisplayName = Info.TypeName;
			Info.DisplayName.RemoveFromStart(TEXT("InputTrigger"));
			TriggerInfos.Add(Info);
		}
	}

	return TriggerInfos;
}

TArray<FString> UInputService::GetAvailableTriggerTypes()
{
	TArray<FString> Types;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UInputTrigger::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			FString ClassName = Class->GetName();
			ClassName.RemoveFromStart(TEXT("InputTrigger"));
			if (!ClassName.IsEmpty())
			{
				Types.Add(ClassName);
			}
		}
	}

	return Types;
}

// =================================================================
// Existence Checks
// =================================================================

bool UInputService::InputActionExists(const FString& ActionPath)
{
	if (ActionPath.IsEmpty())
	{
		return false;
	}
	return UEditorAssetLibrary::DoesAssetExist(ActionPath);
}

bool UInputService::MappingContextExists(const FString& ContextPath)
{
	if (ContextPath.IsEmpty())
	{
		return false;
	}
	return UEditorAssetLibrary::DoesAssetExist(ContextPath);
}

bool UInputService::KeyMappingExists(const FString& ContextPath, const FString& ActionPath)
{
	if (ContextPath.IsEmpty() || ActionPath.IsEmpty())
	{
		return false;
	}

	UInputMappingContext* MappingContext = LoadMappingContext(ContextPath);
	if (!MappingContext)
	{
		return false;
	}

	const TArray<FEnhancedActionKeyMapping>& Mappings = MappingContext->GetMappings();
	for (const FEnhancedActionKeyMapping& Mapping : Mappings)
	{
		if (Mapping.Action)
		{
			FString MappedActionPath = Mapping.Action->GetPathName();
			if (MappedActionPath.Equals(ActionPath, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
	}

	return false;
}