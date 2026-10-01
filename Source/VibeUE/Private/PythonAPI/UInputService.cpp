// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UInputService.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BehaviorTreeServiceInternal.h" // VibeBT::CheckWritableAssetPath / CheckNameLength, the shared create-path rule

// Enhanced Input includes
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "EnhancedInputDeveloperSettings.h"

// PIE input injection (issue #550)
#include "Editor.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Framework/Application/SlateApplication.h"
#include "Containers/Ticker.h"
#include "Editor/EditorEngine.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/PackageName.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Trigger settings
#include "JsonObjectConverter.h"
#include "UObject/UnrealType.h"

namespace VibeUEInputTriggers
{
	// Property names match the C++ name or its snake_case form ("hold_time_threshold" for
	// HoldTimeThreshold, "is_one_shot" for bIsOneShot), case-insensitively.
	static FString NormalizeName(const FString& In)
	{
		return In.Replace(TEXT("_"), TEXT("")).ToLower();
	}

	// A trigger setting is what the Details panel lets a user change: EditAnywhere / EditInstanceOnly, not
	// EditConst, not Transient (not saved with the asset). Runtime state the trigger keeps for itself
	// (HeldDuration, LastValue, bShouldAlwaysTick, the Repeated Tap counters) has no Edit flag and is refused,
	// so it can never be written into the asset.
	static bool IsTriggerSetting(const FProperty* Prop)
	{
		return Prop->HasAnyPropertyFlags(CPF_Edit) &&
			!Prop->HasAnyPropertyFlags(CPF_EditConst | CPF_Transient | CPF_Deprecated);
	}

	// An editable match wins over a non-editable one of the same normalized name.
	static FProperty* FindPropertyLoose(UClass* Class, const FString& Key)
	{
		const FString Wanted = NormalizeName(Key);
		FProperty* NonSetting = nullptr;
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			const FString Name = It->GetName();
			const bool bMatches = NormalizeName(Name) == Wanted ||
				(It->IsA<FBoolProperty>() && Name.StartsWith(TEXT("b")) && NormalizeName(Name.RightChop(1)) == Wanted);
			if (!bMatches)
			{
				continue;
			}
			if (IsTriggerSetting(*It))
			{
				return *It;
			}
			NonSetting = NonSetting ? NonSetting : *It;
		}
		return NonSetting;
	}

	// The editor clamps a numeric setting to its ClampMin / ClampMax metadata (HoldTimeThreshold >= 0,
	// NumberOfTapsWhichTriggerRepeat >= 1); a value outside that range is refused rather than saved.
	static bool CheckClampMetadata(const FProperty* Prop, const void* ValuePtr, FString& OutError)
	{
		const FNumericProperty* Numeric = CastField<FNumericProperty>(Prop);
		if (!Numeric || Numeric->IsEnum())
		{
			return true;
		}
		const FString MinText = Prop->GetMetaData(TEXT("ClampMin"));
		const FString MaxText = Prop->GetMetaData(TEXT("ClampMax"));
		if (MinText.IsEmpty() && MaxText.IsEmpty())
		{
			return true;
		}
		const bool bFloat = Numeric->IsFloatingPoint();
		const double Value = bFloat ? Numeric->GetFloatingPointPropertyValue(ValuePtr) : static_cast<double>(Numeric->GetSignedIntPropertyValue(ValuePtr));
		const bool bBelow = !MinText.IsEmpty() && Value < FCString::Atod(*MinText);
		const bool bAbove = !MaxText.IsEmpty() && Value > FCString::Atod(*MaxText);
		if (!bBelow && !bAbove)
		{
			return true;
		}
		const FString Range = MinText.IsEmpty() ? FString::Printf(TEXT("<= %s"), *MaxText)
			: MaxText.IsEmpty() ? FString::Printf(TEXT(">= %s"), *MinText)
			: FString::Printf(TEXT("between %s and %s"), *MinText, *MaxText);
		const FString ValueText = bFloat ? FString::SanitizeFloat(Value) : FString::Printf(TEXT("%lld"), Numeric->GetSignedIntPropertyValue(ValuePtr));
		OutError = FString::Printf(TEXT("%s = %s is out of range: it must be %s (ClampMin/ClampMax, as the editor enforces)"),
			*Prop->GetName(), *ValueText, *Range);
		return false;
	}

	// Apply {"Property": value, ...} to a freshly created trigger by reflection. An unknown name, a
	// property that is not an editable setting, a value of the wrong type or a value outside the
	// property's ClampMin/ClampMax fails the whole call.
	static bool ApplyPropertiesJson(UObject* Target, const FString& PropertiesJson, FString& OutError)
	{
		if (PropertiesJson.TrimStartAndEnd().IsEmpty())
		{
			return true;
		}
		TSharedPtr<FJsonObject> Obj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PropertiesJson);
		if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
		{
			OutError = FString::Printf(TEXT("properties are not a JSON object: %s"), *PropertiesJson);
			return false;
		}
		for (const auto& Pair : Obj->Values)
		{
			const FString Key = *Pair.Key;
			FProperty* Prop = FindPropertyLoose(Target->GetClass(), Key);
			if (!Prop)
			{
				OutError = FString::Printf(TEXT("%s has no property '%s'"), *Target->GetClass()->GetName(), *Key);
				return false;
			}
			if (!IsTriggerSetting(Prop))
			{
				OutError = FString::Printf(TEXT("%s.%s is not a trigger setting: it is runtime state or read-only in the editor, and cannot be set"),
					*Target->GetClass()->GetName(), *Prop->GetName());
				return false;
			}
			FText Reason;
			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Target);
			if (!FJsonObjectConverter::JsonValueToUProperty(Pair.Value, Prop, ValuePtr, 0, 0, false, &Reason))
			{
				OutError = FString::Printf(TEXT("could not set %s: %s"), *Prop->GetName(), *Reason.ToString());
				return false;
			}
			if (!CheckClampMetadata(Prop, ValuePtr, OutError))
			{
				return false;
			}
		}
		return true;
	}

	static UClass* FindTriggerClass(const FString& TriggerType)
	{
		const FString ClassName = TEXT("InputTrigger") + TriggerType;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (Class->IsChildOf(UInputTrigger::StaticClass()) &&
				!Class->HasAnyClassFlags(CLASS_Abstract) &&
				(Class->GetName().Equals(ClassName, ESearchCase::IgnoreCase) ||
				 Class->GetName().Equals(TriggerType, ESearchCase::IgnoreCase)))
			{
				return Class;
			}
		}
		return nullptr;
	}
}

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

// The folder a create_* call writes into. A path that starts with its mount point (/Game/X, /MyPlugin/X) is kept;
// only a bare folder goes under /Game. A folder under no mounted content root (/Temp/X) is refused with the reason:
// passed on, AssetTools would open a modal "Path does not start with a valid root" message, which blocks every
// MCP call until someone closes it.
static bool NormalizeCreateFolder(const FString& AssetPath, FString& OutFolder, FString& OutError)
{
	OutFolder = AssetPath.StartsWith(TEXT("/")) ? AssetPath : TEXT("/Game/") + AssetPath;
	if (OutFolder.EndsWith(TEXT("/")))
	{
		OutFolder = OutFolder.LeftChop(1);
	}

	FText Reason;
	if (!FPackageName::IsValidLongPackageName(OutFolder, false, &Reason))
	{
		OutError = FString::Printf(TEXT("'%s' is not a content folder: %s"), *OutFolder, *Reason.ToString());
		return false;
	}
	return true;
}

namespace VibeUEInputCreate
{
	// The new asset's name and full path, checked before AssetTools sees them: AssetTools answers an invalid
	// name with a modal "invalid name" dialog (CanCreateAsset), which wedges an unattended editor. The name
	// becomes both the object name and the last package path segment, so it must be valid as either.
	// /Engine is a writable mount point but the installed engine's content, so the shared VibeUE rule
	// (no /Engine, /Script, /Temp; plugin roots allowed) applies to the full path.
	static bool CheckNewAssetTarget(const FString& AssetName, const FString& FullPath, FString& OutError)
	{
		if (AssetName.TrimStartAndEnd().IsEmpty())
		{
			OutError = TEXT("The asset name is empty.");
			return false;
		}
		OutError = VibeBT::CheckNameLength(AssetName, TEXT("The asset name"));
		if (!OutError.IsEmpty())
		{
			return false;
		}
		FText Reason;
		if (!FName::IsValidXName(AssetName, FString(INVALID_OBJECTNAME_CHARACTERS) + INVALID_LONGPACKAGE_CHARACTERS, &Reason))
		{
			OutError = FString::Printf(TEXT("'%s' is not a valid asset name: %s"), *AssetName, *Reason.ToString());
			return false;
		}
		OutError = VibeBT::CheckWritableAssetPath(FullPath);
		return OutError.IsEmpty();
	}
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

	FString BasePath;
	if (!NormalizeCreateFolder(AssetPath, BasePath, Result.ErrorMessage))
	{
		return Result;
	}

	FString FullPath = BasePath / ActionName;
	if (!VibeUEInputCreate::CheckNewAssetTarget(ActionName, FullPath, Result.ErrorMessage))
	{
		return Result;
	}

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
	Result.bSuccess = UEditorAssetLibrary::SaveAsset(FullPath, false);
	if (!Result.bSuccess)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to save asset '%s'"), *FullPath);
		return Result;
	}

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

	FString BasePath;
	if (!NormalizeCreateFolder(AssetPath, BasePath, Result.ErrorMessage))
	{
		return Result;
	}

	FString FullPath = BasePath / ContextName;
	if (!VibeUEInputCreate::CheckNewAssetTarget(ContextName, FullPath, Result.ErrorMessage))
	{
		return Result;
	}

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
	Result.bSuccess = UEditorAssetLibrary::SaveAsset(FullPath, false);
	if (!Result.bSuccess)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to save asset '%s'"), *FullPath);
		return Result;
	}

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
	const FString& TriggerType,
	const FString& PropertiesJson)
{
	UInputMappingContext* MappingContext = LoadMappingContext(ContextPath);
	if (!MappingContext)
	{
		return false;
	}

	if (MappingIndex < 0 || MappingIndex >= MappingContext->GetMappings().Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService::AddTrigger: Invalid mapping index: %d"), MappingIndex);
		return false;
	}

	UClass* TriggerClass = VibeUEInputTriggers::FindTriggerClass(TriggerType);
	if (!TriggerClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService::AddTrigger: Trigger type not found: %s"), *TriggerType);
		return false;
	}

	// Settings go on the new trigger before it is added, so a bad property changes nothing.
	UInputTrigger* NewTrigger = NewObject<UInputTrigger>(MappingContext, TriggerClass, NAME_None, RF_Transactional);
	FString PropertyError;
	if (!NewTrigger || !VibeUEInputTriggers::ApplyPropertiesJson(NewTrigger, PropertiesJson, PropertyError))
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService::AddTrigger: %s"), *PropertyError);
		return false;
	}

	MappingContext->Modify();
	MappingContext->GetMapping(MappingIndex).Triggers.Add(NewTrigger);
	MappingContext->PostEditChange();

	// Save
	UPackage* Package = MappingContext->GetOutermost();
	if (Package)
	{
		Package->MarkPackageDirty();
	}

	return true;
}

FString UInputService::AddActionTrigger(
	const FString& ActionPath,
	const FString& TriggerType,
	const FString& PropertiesJson)
{
	// An Input Action's own triggers apply to every mapping of the action.
	auto Fail = [](const TCHAR* Code, const FString& Message)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetBoolField(TEXT("success"), false);
		Obj->SetStringField(TEXT("error_code"), Code);
		Obj->SetStringField(TEXT("error_message"), Message);
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	};

	// UEditorAssetLibrary::LoadAsset refuses during PIE, which would otherwise surface as ACTION_NOT_FOUND
	// for an action that exists. Same refusal and code as BlueprintService's LoadBlueprint.
	if (GEditor && GEditor->PlayWorld)
	{
		return Fail(TEXT("PIE_ACTIVE"), FString::Printf(TEXT("add_action_trigger refused '%s' — a Play-In-Editor session is running; stop PIE before editing Input Action assets."), *ActionPath));
	}

	UInputAction* Action = LoadInputAction(ActionPath);
	if (!Action)
	{
		return Fail(TEXT("ACTION_NOT_FOUND"), FString::Printf(TEXT("Input Action not found: %s"), *ActionPath));
	}
	UClass* TriggerClass = VibeUEInputTriggers::FindTriggerClass(TriggerType);
	if (!TriggerClass)
	{
		return Fail(TEXT("TRIGGER_TYPE_NOT_FOUND"), FString::Printf(TEXT("Trigger type not found: %s (see get_available_trigger_types)"), *TriggerType));
	}

	UInputTrigger* NewTrigger = NewObject<UInputTrigger>(Action, TriggerClass, NAME_None, RF_Transactional);
	FString PropertyError;
	if (!NewTrigger || !VibeUEInputTriggers::ApplyPropertiesJson(NewTrigger, PropertiesJson, PropertyError))
	{
		return Fail(TEXT("BAD_PROPERTIES"), PropertyError);
	}

	Action->Modify();
	const int32 TriggerIndex = Action->Triggers.Add(NewTrigger);
	// As an edit in the editor would: Enhanced Input's Blueprint nodes listen for OnTriggersChanged.
	FProperty* TriggersProperty = FindFProperty<FProperty>(UInputAction::StaticClass(), GET_MEMBER_NAME_CHECKED(UInputAction, Triggers));
	FPropertyChangedEvent ChangedEvent(TriggersProperty, EPropertyChangeType::ArrayAdd);
	Action->PostEditChangeProperty(ChangedEvent);
	Action->GetOutermost()->MarkPackageDirty();

	TSharedRef<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), true);
	R->SetStringField(TEXT("action"), Action->GetName());
	R->SetStringField(TEXT("trigger"), TriggerClass->GetName());
	R->SetNumberField(TEXT("trigger_index"), TriggerIndex);
	R->SetStringField(TEXT("note"), TEXT("The asset is modified, not saved. A running PIE session keeps its copy of the triggers: restart PIE to see the change."));
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(R, Writer);
	return Out;
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
// =================================================================
// PIE Input Injection (issue #550)
// =================================================================

static FString InjectionErrorJson(const FString& Code, const FString& Message)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), false);
	Obj->SetStringField(TEXT("error_code"), Code);
	Obj->SetStringField(TEXT("error_message"), Message);
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
	return Out;
}

static FString InjectionOkJson(const TSharedRef<FJsonObject>& Obj)
{
	Obj->SetBoolField(TEXT("success"), true);
	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Obj, Writer);
	return Out;
}

namespace VibeUEInputInjection
{
	static ULocalPlayer* FirstLocalPlayerOf(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
	}

	// One PIE world by its PIE instance number. -1 is the first PIE world that has a local player (the lowest
	// instance number among them): the server's window in a listen-server session, client 1 under a
	// dedicated server. Not GEditor->PlayWorld, which the editor re-points at each PIE world in turn every
	// tick, so it names whichever world happened to tick last. Falls back to the lowest-numbered PIE world
	// when none has a local player yet, so the caller gets NO_LOCAL_PLAYER rather than NO_PIE_INSTANCE.
	static UWorld* FindPieWorld(int32 PieInstance, int32& OutInstance)
	{
		UWorld* Found = nullptr;
		bool bFoundHasPlayer = false;
		OutInstance = PieInstance;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType != EWorldType::PIE || !World)
			{
				continue;
			}
			if (PieInstance >= 0)
			{
				if (Context.PIEInstance == PieInstance)
				{
					return World;
				}
				continue;
			}
			const bool bHasPlayer = FirstLocalPlayerOf(World) != nullptr;
			const bool bBetter = !Found || (bHasPlayer && !bFoundHasPlayer) ||
				(bHasPlayer == bFoundHasPlayer && Context.PIEInstance < OutInstance);
			if (bBetter)
			{
				Found = World;
				bFoundHasPlayer = bHasPlayer;
				OutInstance = Context.PIEInstance;
			}
		}
		return Found;
	}

	// The local player's input subsystem of one PIE world (see FindPieWorld; OutInstance is the instance
	// actually used). The local player comes from that world's game instance, never from "the first player
	// controller", which on a server world need not be a local one. The subsystem is only returned once that
	// player has a controller with Enhanced Input's player input: before that (early PIE, a client still
	// joining) InjectInputForAction does nothing at all, so reporting success would be a lie.
	static UEnhancedInputLocalPlayerSubsystem* ResolvePieSubsystem(int32 PieInstance, int32& OutInstance, FString& OutCode, FString& OutMessage)
	{
		OutInstance = PieInstance;
		if (!GEditor || !GEditor->PlayWorld)
		{
			OutCode = TEXT("PIE_NOT_RUNNING");
			OutMessage = TEXT("PIE is not running — start it first (injection drives the live PIE session).");
			return nullptr;
		}
		UWorld* World = FindPieWorld(PieInstance, OutInstance);
		if (!World)
		{
			OutCode = TEXT("NO_PIE_INSTANCE");
			OutMessage = FString::Printf(TEXT("No PIE world with instance %d."), PieInstance);
			return nullptr;
		}
		ULocalPlayer* LocalPlayer = FirstLocalPlayerOf(World);
		if (!LocalPlayer)
		{
			OutCode = TEXT("NO_LOCAL_PLAYER");
			OutMessage = FString::Printf(TEXT("PIE instance %d has no local player yet — PIE start is asynchronous, retry on a later tick (a dedicated server world never has one: pass a client's pie_instance)."), OutInstance);
			return nullptr;
		}
		APlayerController* PlayerController = LocalPlayer->GetPlayerController(World);
		if (!PlayerController || !PlayerController->PlayerInput)
		{
			OutCode = TEXT("NO_PLAYER_CONTROLLER");
			OutMessage = FString::Printf(TEXT("PIE instance %d's local player has no player controller with input yet — PIE start (or a client's join) is asynchronous, retry on a later tick."), OutInstance);
			return nullptr;
		}
		UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);
		if (!Subsystem || !Subsystem->GetPlayerInput())
		{
			OutCode = TEXT("NO_ENHANCED_INPUT");
			OutMessage = Subsystem
				? FString::Printf(TEXT("The player controller's input is %s, not EnhancedPlayerInput — set Default Player Input Class to EnhancedPlayerInput."), *PlayerController->PlayerInput->GetClass()->GetName())
				: FString(TEXT("EnhancedInputLocalPlayerSubsystem unavailable — is the project using Enhanced Input?"));
			return nullptr;
		}
		return Subsystem;
	}

	// UEditorAssetLibrary::LoadAsset returns null during PIE, so resolve the object path directly
	// (same pattern as WidgetService::SpawnWidgetInPIE).
	static const UInputAction* LoadActionForPie(const FString& ActionPath, uint32 LoadFlags = LOAD_None)
	{
		FString ObjectPath = ActionPath;
		if (!ObjectPath.Contains(TEXT(".")))
		{
			ObjectPath = FString::Printf(TEXT("%s.%s"), *ActionPath, *FPackageName::GetShortName(ActionPath));
		}
		return LoadObject<UInputAction>(nullptr, *ObjectPath, {}, LoadFlags);
	}

	static FInputActionValue MakeValue(const UInputAction* Action, float X, float Y, float Z)
	{
		switch (Action->ValueType)
		{
		case EInputActionValueType::Boolean: return FInputActionValue(X != 0.0f);
		case EInputActionValueType::Axis1D:  return FInputActionValue(X);
		case EInputActionValueType::Axis2D:  return FInputActionValue(FVector2D(X, Y));
		case EInputActionValueType::Axis3D:  return FInputActionValue(FVector(X, Y, Z));
		default:                             return FInputActionValue(X);
		}
	}

	// Held injections and held keys. While any is active, a scoped entry in
	// ShouldDisableCPUThrottlingDelegates keeps the editor off its background frame rate (about 3 FPS,
	// measured 2026-09-26), without touching the user's saved "use less CPU in the background" setting.
	// For the same span an EndPIE handler is bound, so nothing outlives the PIE session it was made for.
	struct FHeldInjection
	{
		TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> Subsystem;
		TWeakObjectPtr<const UInputAction> Action;
		FTSTicker::FDelegateHandle Release;
	};
	// Keyed as the engine keys its continuous injections: by the action object (its path name, so
	// /Game/IA_X and /Game/IA_X.IA_X are one hold), in the PIE instance actually used (so -1 and 0 are one
	// hold when they resolve to the same world).
	static TMap<FString, FHeldInjection> GHeldInjections;
	// One pending release per held key: holding a key that is already held extends that hold.
	static TMap<FKey, FTSTicker::FDelegateHandle> GHeldKeys;

	// Runs Release once Seconds of wall-clock time have passed, checked every frame. A ticker delay is not
	// used: it counts from the ticker clock at the START of the frame that made the hold, so on slow PIE
	// frames (~250 ms measured on a busy level, 2026-10-01) a 1 s hold was released at 0.74 s.
	static FTSTicker::FDelegateHandle AddWallClockRelease(float Seconds, TFunction<void()> Release)
	{
		const double Deadline = FPlatformTime::Seconds() + Seconds;
		return FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Deadline, Release = MoveTemp(Release)](float)
		{
			if (FPlatformTime::Seconds() < Deadline)
			{
				return true;
			}
			Release();
			return false;
		}));
	}
	static FDelegateHandle GThrottleHandle;
	static FDelegateHandle GEndPieHandle;
	// Half a second at full rate after a release, so the release itself is processed promptly: measured
	// 2026-09-26, a minimized editor dropped back to 3 FPS at the release and the action stayed active ~1 s longer.
	// Timed only by the core ticker that times the holds: compared with wall-clock time, a fixed timestep
	// could end the grace ticker first and leave the override in place with nothing left to remove it.
	static bool GReleaseGraceActive = false;
	static FTSTicker::FDelegateHandle GReleaseGraceTicker;

	static void OnEndPie(const bool bIsSimulating);

	static void UpdateThrottleScope()
	{
		if (!GEditor)
		{
			return;
		}
		const bool bActive = GHeldInjections.Num() > 0 || GHeldKeys.Num() > 0 || GReleaseGraceActive;
		if (bActive && !GThrottleHandle.IsValid())
		{
			UEditorEngine::FShouldDisableCPUThrottling Delegate = UEditorEngine::FShouldDisableCPUThrottling::CreateLambda([]() { return true; });
			GThrottleHandle = Delegate.GetHandle();
			GEditor->ShouldDisableCPUThrottlingDelegates.Add(MoveTemp(Delegate));
		}
		else if (!bActive && GThrottleHandle.IsValid())
		{
			const FDelegateHandle Handle = GThrottleHandle;
			GEditor->ShouldDisableCPUThrottlingDelegates.RemoveAll(
				[Handle](const UEditorEngine::FShouldDisableCPUThrottling& D) { return D.GetHandle() == Handle; });
			GThrottleHandle.Reset();
		}
		// Removing it from inside the EndPIE broadcast is safe: the delegate only unbinds the entry mid-broadcast.
		if (bActive && !GEndPieHandle.IsValid())
		{
			GEndPieHandle = FEditorDelegates::EndPIE.AddStatic(&OnEndPie);
		}
		else if (!bActive && GEndPieHandle.IsValid())
		{
			FEditorDelegates::EndPIE.Remove(GEndPieHandle);
			GEndPieHandle.Reset();
		}
	}

	static void StartReleaseGrace()
	{
		GReleaseGraceActive = true;
		if (GReleaseGraceTicker.IsValid())
		{
			FTSTicker::RemoveTicker(GReleaseGraceTicker);
		}
		GReleaseGraceTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			GReleaseGraceTicker.Reset();
			GReleaseGraceActive = false;
			UpdateThrottleScope();
			return false;
		}), 0.5f);
	}

	static FString HoldId(const UInputAction* Action, int32 PieInstance)
	{
		return FString::Printf(TEXT("%s#%d"), *Action->GetPathName(), PieInstance);
	}

	// bFromTicker: called by the release ticker itself, which removes itself by returning false.
	static bool ReleaseHeld(const FString& Id, bool bFromTicker = false)
	{
		FHeldInjection Held;
		if (!GHeldInjections.RemoveAndCopyValue(Id, Held))
		{
			return false;
		}
		if (!bFromTicker && Held.Release.IsValid())
		{
			FTSTicker::RemoveTicker(Held.Release);
		}
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = Held.Subsystem.Get())
		{
			if (const UInputAction* Action = Held.Action.Get())
			{
				Subsystem->StopContinuousInputInjectionForAction(Action);
			}
		}
		StartReleaseGrace();
		UpdateThrottleScope();
		return true;
	}

	// Ends a held key. bSendKeyUp: send its key-up to the PIE game viewport, and only while PIE runs — with no
	// session it would land on whichever editor widget has focus. Without it the caller sends its own key-up.
	static bool ReleaseHeldKey(const FKey& Key, bool bSendKeyUp, bool bFromTicker = false)
	{
		FTSTicker::FDelegateHandle Release;
		if (!GHeldKeys.RemoveAndCopyValue(Key, Release))
		{
			return false;
		}
		if (!bFromTicker && Release.IsValid())
		{
			FTSTicker::RemoveTicker(Release);
		}
		if (bSendKeyUp && GEditor && GEditor->PlayWorld && FSlateApplication::IsInitialized())
		{
			FSlateApplication& SlateApp = FSlateApplication::Get();
			SlateApp.SetAllUserFocusToGameViewport();
			FKeyEvent UpEvent(Key, FModifierKeysState(), /*UserIndex=*/0, /*bIsRepeat=*/false, /*CharacterCode=*/0, /*KeyCode=*/0);
			SlateApp.ProcessKeyUpEvent(UpEvent);
		}
		StartReleaseGrace();
		UpdateThrottleScope();
		return true;
	}

	// PIE ended, and every hold belonged to it: stop the injections, cancel the pending releases (no key-up is
	// sent: the game viewport is going away) and drop the throttle override now, with no grace — there is no
	// session left to process a release. A later stop_injection then truthfully reports was_active: false.
	static void OnEndPie(const bool /*bIsSimulating*/)
	{
		for (const TPair<FString, FHeldInjection>& Pair : GHeldInjections)
		{
			if (Pair.Value.Release.IsValid())
			{
				FTSTicker::RemoveTicker(Pair.Value.Release);
			}
			UEnhancedInputLocalPlayerSubsystem* Subsystem = Pair.Value.Subsystem.Get();
			const UInputAction* Action = Pair.Value.Action.Get();
			if (Subsystem && Action)
			{
				Subsystem->StopContinuousInputInjectionForAction(Action);
			}
		}
		GHeldInjections.Empty();
		for (const TPair<FKey, FTSTicker::FDelegateHandle>& Pair : GHeldKeys)
		{
			if (Pair.Value.IsValid())
			{
				FTSTicker::RemoveTicker(Pair.Value);
			}
		}
		GHeldKeys.Empty();
		if (GReleaseGraceTicker.IsValid())
		{
			FTSTicker::RemoveTicker(GReleaseGraceTicker);
			GReleaseGraceTicker.Reset();
		}
		GReleaseGraceActive = false;
		UpdateThrottleScope();
	}
}

FString UInputService::InjectAction(const FString& ActionPath, float X, float Y, float Z, int32 PieInstance)
{
	// The PIE world's local player (PieInstance), not the first player controller
	FString Code, Message;
	int32 UsedInstance = PieInstance;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = VibeUEInputInjection::ResolvePieSubsystem(PieInstance, UsedInstance, Code, Message);
	if (!Subsystem)
	{
		return InjectionErrorJson(Code, Message);
	}
	const UInputAction* Action = VibeUEInputInjection::LoadActionForPie(ActionPath);
	if (!Action)
	{
		return InjectionErrorJson(TEXT("ACTION_NOT_FOUND"), FString::Printf(TEXT("Input Action not found: %s"), *ActionPath));
	}

	Subsystem->InjectInputForAction(Action, VibeUEInputInjection::MakeValue(Action, X, Y, Z), TArray<UInputModifier*>(), TArray<UInputTrigger*>());

	TSharedRef<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("queued"), true);
	R->SetStringField(TEXT("action"), Action->GetName());
	R->SetStringField(TEXT("value_type"), UEnum::GetValueAsString(Action->ValueType));
	TArray<TSharedPtr<FJsonValue>> Injected;
	Injected.Add(MakeShared<FJsonValueNumber>(X));
	Injected.Add(MakeShared<FJsonValueNumber>(Y));
	Injected.Add(MakeShared<FJsonValueNumber>(Z));
	R->SetArrayField(TEXT("injected"), Injected);
	R->SetNumberField(TEXT("pie_instance"), UsedInstance);
	R->SetStringField(TEXT("note"), TEXT("Queued for the next input tick and released on the tick after; nothing runs while this call blocks the game thread. Read the result in a later call, two or more frames on. To hold an action use inject_action_for."));
	return InjectionOkJson(R);
}

FString UInputService::InjectActionFor(const FString& ActionPath, float Seconds, float X, float Y, float Z, int32 PieInstance)
{
	// Holds the value with Enhanced Input's continuous injection, then releases it.
	if (!(Seconds >= 0.05f && Seconds <= 60.0f))
	{
		return InjectionErrorJson(TEXT("BAD_DURATION"), TEXT("seconds must be between 0.05 and 60."));
	}
	FString Code, Message;
	int32 UsedInstance = PieInstance;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = VibeUEInputInjection::ResolvePieSubsystem(PieInstance, UsedInstance, Code, Message);
	if (!Subsystem)
	{
		return InjectionErrorJson(Code, Message);
	}
	const UInputAction* Action = VibeUEInputInjection::LoadActionForPie(ActionPath);
	if (!Action)
	{
		return InjectionErrorJson(TEXT("ACTION_NOT_FOUND"), FString::Printf(TEXT("Input Action not found: %s"), *ActionPath));
	}

	const FString Id = VibeUEInputInjection::HoldId(Action, UsedInstance);
	VibeUEInputInjection::ReleaseHeld(Id); // a second call restarts the hold

	Subsystem->StartContinuousInputInjectionForAction(Action, VibeUEInputInjection::MakeValue(Action, X, Y, Z), TArray<UInputModifier*>(), TArray<UInputTrigger*>());

	VibeUEInputInjection::FHeldInjection Held;
	Held.Subsystem = Subsystem;
	Held.Action = Action;
	Held.Release = VibeUEInputInjection::AddWallClockRelease(Seconds, [Id]()
	{
		VibeUEInputInjection::ReleaseHeld(Id, /*bFromTicker=*/true);
	});
	VibeUEInputInjection::GHeldInjections.Add(Id, Held);
	VibeUEInputInjection::UpdateThrottleScope();

	TSharedRef<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("action"), Action->GetName());
	R->SetNumberField(TEXT("seconds"), Seconds);
	R->SetNumberField(TEXT("pie_instance"), UsedInstance);
	R->SetStringField(TEXT("note"), TEXT("Held from the next input tick for the given real time, then released. Read game state in later calls; stop early with stop_injection. Ending PIE drops the hold."));
	return InjectionOkJson(R);
}

FString UInputService::StopInjection(const FString& ActionPath, int32 PieInstance)
{
	// The hold is found the way inject_action_for filed it: by the action object and the PIE instance -1
	// resolves to, so any spelling of the path that names the same action finds it.
	bool bWasActive = false;
	int32 UsedInstance = PieInstance;
	if (VibeUEInputInjection::GHeldInjections.Num() > 0)
	{
		if (PieInstance < 0 && GEngine)
		{
			VibeUEInputInjection::FindPieWorld(PieInstance, UsedInstance);
		}
		// A held action is in memory; a quiet load finds it without warning about one that does not exist.
		if (const UInputAction* Action = VibeUEInputInjection::LoadActionForPie(ActionPath, LOAD_NoWarn | LOAD_Quiet))
		{
			bWasActive = VibeUEInputInjection::ReleaseHeld(VibeUEInputInjection::HoldId(Action, UsedInstance));
		}
	}
	TSharedRef<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("action"), ActionPath);
	R->SetBoolField(TEXT("was_active"), bWasActive);
	R->SetNumberField(TEXT("pie_instance"), UsedInstance);
	return InjectionOkJson(R);
}

FString UInputService::InjectKey(const FString& KeyName, const FString& EventType, float HoldSeconds)
{
	if (!GEditor || !GEditor->PlayWorld)
	{
		return InjectionErrorJson(TEXT("PIE_NOT_RUNNING"), TEXT("PIE is not running — inject_key targets the PIE game viewport."));
	}
	// Simulate In Editor also sets PlayWorld, but it has no player game viewport for Slate to focus:
	// the key events below would be dropped while this still reported success.
	if (GEditor->IsSimulatingInEditor())
	{
		return InjectionErrorJson(TEXT("SIMULATE_NOT_PLAY"), TEXT("The session is Simulate In Editor, which has no player game viewport — inject_key needs Play In Editor. Stop the session and use StartPIE."));
	}
	if (!FSlateApplication::IsInitialized())
	{
		return InjectionErrorJson(TEXT("NO_SLATE"), TEXT("Slate is not initialized."));
	}

	const FKey Key = FindKeyByName(KeyName);
	if (!Key.IsValid())
	{
		return InjectionErrorJson(TEXT("UNKNOWN_KEY"), FString::Printf(TEXT("Unknown key '%s' — see get_available_keys."), *KeyName));
	}

	// "hold" = down now, up after HoldSeconds (a ticker sends it; this call returns at once)
	const bool bHold = EventType.Equals(TEXT("hold"), ESearchCase::IgnoreCase);
	const bool bDown = bHold || EventType.Equals(TEXT("down"), ESearchCase::IgnoreCase) || EventType.Equals(TEXT("tap"), ESearchCase::IgnoreCase);
	const bool bUp = EventType.Equals(TEXT("up"), ESearchCase::IgnoreCase) || EventType.Equals(TEXT("tap"), ESearchCase::IgnoreCase);
	if (!bDown && !bUp)
	{
		return InjectionErrorJson(TEXT("BAD_EVENT"), FString::Printf(TEXT("Unknown event_type '%s' — use 'tap', 'down', 'up', or 'hold'."), *EventType));
	}
	if (bHold && !(HoldSeconds >= 0.05f && HoldSeconds <= 60.0f))
	{
		return InjectionErrorJson(TEXT("BAD_DURATION"), TEXT("hold_seconds must be between 0.05 and 60 for a hold."));
	}

	FSlateApplication& Slate = FSlateApplication::Get();
	// Route the synthesized event to the game: Slate focus is app-internal, so this works even when
	// the OS focus is elsewhere — the whole point versus SendKeys.
	Slate.SetAllUserFocusToGameViewport();

	// A hold of a key that is already held extends that hold: no second key-down (the game would see a
	// second press), and its one pending release moves to HoldSeconds from now — two timers would let the
	// first release the key early. An explicit "up" or "tap" ends a pending hold; this call sends the key-up.
	bool bExtended = false;
	if (bHold)
	{
		if (const FTSTicker::FDelegateHandle* Pending = VibeUEInputInjection::GHeldKeys.Find(Key))
		{
			FTSTicker::RemoveTicker(*Pending);
			VibeUEInputInjection::GHeldKeys.Remove(Key);
			bExtended = true;
		}
	}
	else if (bUp)
	{
		VibeUEInputInjection::ReleaseHeldKey(Key, /*bSendKeyUp=*/false);
	}

	bool bHandledDown = false;
	bool bHandledUp = false;
	if (bDown && !bExtended)
	{
		const FKeyEvent DownEvent(Key, FModifierKeysState(), /*UserIndex=*/0, /*bIsRepeat=*/false, /*CharacterCode=*/0, /*KeyCode=*/0);
		bHandledDown = Slate.ProcessKeyDownEvent(const_cast<FKeyEvent&>(DownEvent));
	}
	if (bUp)
	{
		const FKeyEvent UpEvent(Key, FModifierKeysState(), /*UserIndex=*/0, /*bIsRepeat=*/false, /*CharacterCode=*/0, /*KeyCode=*/0);
		bHandledUp = Slate.ProcessKeyUpEvent(const_cast<FKeyEvent&>(UpEvent));
	}
	if (bHold)
	{
		// The release, later. The editor stays off its background frame rate meanwhile, and ending PIE first
		// cancels it (see OnEndPie).
		const FTSTicker::FDelegateHandle Release = VibeUEInputInjection::AddWallClockRelease(HoldSeconds, [Key]()
		{
			VibeUEInputInjection::ReleaseHeldKey(Key, /*bSendKeyUp=*/true, /*bFromTicker=*/true);
		});
		VibeUEInputInjection::GHeldKeys.Add(Key, Release);
		VibeUEInputInjection::UpdateThrottleScope();
	}

	TSharedRef<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("key"), Key.GetFName().ToString());
	R->SetStringField(TEXT("event"), EventType.ToLower());
	R->SetBoolField(TEXT("handled_down"), bHandledDown);
	R->SetBoolField(TEXT("handled_up"), bHandledUp);
	if (bHold)
	{
		R->SetBoolField(TEXT("extended"), bExtended);
		R->SetNumberField(TEXT("hold_seconds"), HoldSeconds);
	}
	return InjectionOkJson(R);
}
