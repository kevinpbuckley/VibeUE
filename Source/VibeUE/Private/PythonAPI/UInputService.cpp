// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "PythonAPI/UInputService.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

// Enhanced Input includes
#include "InputAction.h"
#include "InputMappingContext.h"

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

bool UInputService::GetInputActionInfo(const FString& ActionPath, FInputActionDetailedInfo& OutInfo)
{
	UInputAction* InputAction = Cast<UInputAction>(UEditorAssetLibrary::LoadAsset(ActionPath));
	if (!InputAction)
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService::GetInputActionInfo: Failed to load Input Action: %s"), *ActionPath);
		return false;
	}

	OutInfo.ActionName = InputAction->GetName();
	OutInfo.ActionPath = ActionPath;

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

bool UInputService::GetMappingContextInfo(const FString& ContextPath, FMappingContextDetailedInfo& OutInfo)
{
	UInputMappingContext* MappingContext = Cast<UInputMappingContext>(UEditorAssetLibrary::LoadAsset(ContextPath));
	if (!MappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputService::GetMappingContextInfo: Failed to load Mapping Context: %s"), *ContextPath);
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
