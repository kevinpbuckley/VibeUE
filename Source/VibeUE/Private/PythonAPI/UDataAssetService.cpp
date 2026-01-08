// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "PythonAPI/UDataAssetService.h"
#include "Engine/DataAsset.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "UObject/UObjectIterator.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

TArray<FString> UDataAssetService::SearchTypes(const FString& SearchFilter)
{
	TArray<FString> DataAssetTypes;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class && Class->IsChildOf(UDataAsset::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			FString ClassName = Class->GetName();

			if (SearchFilter.IsEmpty() || ClassName.Contains(SearchFilter))
			{
				DataAssetTypes.Add(ClassName);
			}
		}
	}

	DataAssetTypes.Sort();
	return DataAssetTypes;
}

TArray<FString> UDataAssetService::ListDataAssets(const FString& ClassName)
{
	TArray<FString> DataAssetPaths;

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;

	if (ClassName.IsEmpty())
	{
		// Get all DataAssets
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine.DataAsset")));
		Filter.bRecursiveClasses = true;
	}
	else
	{
		// Find the specific class
		UClass* TargetClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ClassName));
		if (!TargetClass)
		{
			// Try finding in any package
			for (TObjectIterator<UClass> It; It; ++It)
			{
				if (It->GetName() == ClassName)
				{
					TargetClass = *It;
					break;
				}
			}
		}

		if (TargetClass)
		{
			Filter.ClassPaths.Add(FTopLevelAssetPath(TargetClass->GetPathName()));
			Filter.bRecursiveClasses = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UDataAssetService::ListDataAssets: Class not found: %s"), *ClassName);
			return DataAssetPaths;
		}
	}

	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		DataAssetPaths.Add(AssetData.GetObjectPathString());
	}

	return DataAssetPaths;
}

FString UDataAssetService::GetPropertiesAsJson(const FString& AssetPath)
{
	UDataAsset* DataAsset = Cast<UDataAsset>(UEditorAssetLibrary::LoadAsset(AssetPath));
	if (!DataAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("UDataAssetService::GetPropertiesAsJson: Failed to load DataAsset: %s"), *AssetPath);
		return FString();
	}

	UClass* Class = DataAsset->GetClass();
	if (!Class)
	{
		return FString();
	}

	// Export properties to JSON
	FString JsonString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
	JsonWriter->WriteObjectStart();

	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		FProperty* Property = *It;
		if (Property && !Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
		{
			FString PropertyName = Property->GetName();
			FString PropertyValue;
			Property->ExportTextItem_Direct(PropertyValue, Property->ContainerPtrToValuePtr<void>(DataAsset), nullptr, nullptr, PPF_None);

			JsonWriter->WriteValue(PropertyName, PropertyValue);
		}
	}

	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	return JsonString;
}
