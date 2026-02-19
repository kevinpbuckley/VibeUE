// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UDataAssetService.h"
#include "Engine/DataAsset.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/DataAssetFactory.h"
#include "UObject/UObjectIterator.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogDataAssetService, Log, All);

// =================================================================
// Helper Methods
// =================================================================

UClass* UDataAssetService::FindDataAssetClass(const FString& ClassName)
{
	if (ClassName.IsEmpty())
	{
		return nullptr;
	}
	
	// Try with and without U prefix
	FString SearchNames[3] = {
		ClassName,
		ClassName.StartsWith(TEXT("U")) ? ClassName.RightChop(1) : FString::Printf(TEXT("U%s"), *ClassName),
		ClassName.StartsWith(TEXT("U")) ? ClassName : FString::Printf(TEXT("U%s"), *ClassName)
	};
	
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class && Class->IsChildOf(UDataAsset::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			for (const FString& SearchName : SearchNames)
			{
				if (Class->GetName().Equals(SearchName, ESearchCase::IgnoreCase))
				{
					return Class;
				}
			}
		}
	}
	
	return nullptr;
}

UDataAsset* UDataAssetService::LoadDataAsset(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return nullptr;
	}
	
	return Cast<UDataAsset>(UEditorAssetLibrary::LoadAsset(AssetPath));
}

bool UDataAssetService::ShouldExposeProperty(FProperty* Property, bool bIncludeAll)
{
	if (!Property)
	{
		return false;
	}
	
	// Skip deprecated and transient properties
	if (Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
	{
		return false;
	}
	
	if (bIncludeAll)
	{
		return true;
	}
	
	// Only expose editable properties by default
	return Property->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible | CPF_SaveGame);
}

FString UDataAssetService::GetPropertyTypeString(FProperty* Property)
{
	if (!Property)
	{
		return TEXT("Unknown");
	}
	
	// Handle common types with friendly names
	if (CastField<FBoolProperty>(Property))
	{
		return TEXT("bool");
	}
	if (CastField<FIntProperty>(Property))
	{
		return TEXT("int32");
	}
	if (CastField<FInt64Property>(Property))
	{
		return TEXT("int64");
	}
	if (CastField<FFloatProperty>(Property))
	{
		return TEXT("float");
	}
	if (CastField<FDoubleProperty>(Property))
	{
		return TEXT("double");
	}
	if (CastField<FStrProperty>(Property))
	{
		return TEXT("FString");
	}
	if (CastField<FNameProperty>(Property))
	{
		return TEXT("FName");
	}
	if (CastField<FTextProperty>(Property))
	{
		return TEXT("FText");
	}
	
	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		if (UEnum* Enum = EnumProp->GetEnum())
		{
			return Enum->GetName();
		}
	}
	
	if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
	{
		if (UEnum* Enum = ByteProp->Enum)
		{
			return Enum->GetName();
		}
		return TEXT("uint8");
	}
	
	if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		if (UScriptStruct* Struct = StructProp->Struct)
		{
			return Struct->GetName();
		}
	}
	
	if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
	{
		if (UClass* PropClass = ObjProp->PropertyClass)
		{
			return FString::Printf(TEXT("%s*"), *PropClass->GetName());
		}
	}
	
	if (FSoftObjectProperty* SoftObjProp = CastField<FSoftObjectProperty>(Property))
	{
		if (UClass* PropClass = SoftObjProp->PropertyClass)
		{
			return FString::Printf(TEXT("TSoftObjectPtr<%s>"), *PropClass->GetName());
		}
	}
	
	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		FString InnerType = GetPropertyTypeString(ArrayProp->Inner);
		return FString::Printf(TEXT("TArray<%s>"), *InnerType);
	}
	
	if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
	{
		FString KeyType = GetPropertyTypeString(MapProp->KeyProp);
		FString ValueType = GetPropertyTypeString(MapProp->ValueProp);
		return FString::Printf(TEXT("TMap<%s, %s>"), *KeyType, *ValueType);
	}
	
	if (FSetProperty* SetProp = CastField<FSetProperty>(Property))
	{
		FString ElementType = GetPropertyTypeString(SetProp->ElementProp);
		return FString::Printf(TEXT("TSet<%s>"), *ElementType);
	}
	
	return Property->GetCPPType();
}

FString UDataAssetService::PropertyToString(FProperty* Property, void* Container)
{
	if (!Property || !Container)
	{
		return TEXT("");
	}
	
	FString Value;
	Property->ExportTextItem_Direct(Value, Property->ContainerPtrToValuePtr<void>(Container), nullptr, nullptr, PPF_None);
	return Value;
}

bool UDataAssetService::SetPropertyFromString(FProperty* Property, void* Container, const FString& Value, FString& OutError)
{
	if (!Property || !Container)
	{
		OutError = TEXT("Invalid property or container");
		return false;
	}
	
	void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);
	
	// Try import from text
	const TCHAR* ImportResult = Property->ImportText_Direct(*Value, ValuePtr, nullptr, PPF_None);
	if (ImportResult == nullptr)
	{
		OutError = FString::Printf(TEXT("Failed to parse value '%s' for property type %s"), *Value, *GetPropertyTypeString(Property));
		return false;
	}
	
	return true;
}

// =================================================================
// Discovery Actions
// =================================================================

TArray<FDataAssetTypeInfo> UDataAssetService::SearchTypes(const FString& SearchFilter)
{
	TArray<FDataAssetTypeInfo> Results;
	
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class || !Class->IsChildOf(UDataAsset::StaticClass()))
		{
			continue;
		}
		
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}
		
		// Skip the base DataAsset class
		if (Class == UDataAsset::StaticClass())
		{
			continue;
		}
		
		FString ClassName = Class->GetName();
		
		// Apply filter
		if (!SearchFilter.IsEmpty() && !ClassName.Contains(SearchFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}
		
		FDataAssetTypeInfo TypeInfo;
		TypeInfo.Name = ClassName;
		TypeInfo.Path = Class->GetPathName();
		TypeInfo.bIsNative = !Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
		
		// Get module name from path
		FString PathStr = Class->GetPathName();
		if (PathStr.StartsWith(TEXT("/Script/")))
		{
			int32 ModuleEnd = PathStr.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromStart, 8);
			if (ModuleEnd != INDEX_NONE)
			{
				TypeInfo.Module = PathStr.Mid(8, ModuleEnd - 8);
			}
		}
		else
		{
			TypeInfo.Module = TEXT("Blueprint");
		}
		
		// Get parent class
		if (UClass* ParentClass = Class->GetSuperClass())
		{
			TypeInfo.ParentClass = ParentClass->GetName();
		}
		
		Results.Add(TypeInfo);
	}
	
	// Sort by name
	Results.Sort([](const FDataAssetTypeInfo& A, const FDataAssetTypeInfo& B) {
		return A.Name < B.Name;
	});
	
	return Results;
}

TArray<FString> UDataAssetService::ListDataAssets(const FString& ClassName, const FString& SearchPath)
{
	TArray<FString> Results;
	
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*SearchPath));
	Filter.bRecursivePaths = true;
	
	if (ClassName.IsEmpty())
	{
		// Get all DataAssets
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/Engine.DataAsset")));
		Filter.bRecursiveClasses = true;
	}
	else
	{
		// Find the specific class
		UClass* TargetClass = FindDataAssetClass(ClassName);
		if (TargetClass)
		{
			Filter.ClassPaths.Add(FTopLevelAssetPath(TargetClass->GetPathName()));
			Filter.bRecursiveClasses = true;
		}
		else
		{
			UE_LOG(LogDataAssetService, Warning, TEXT("ListDataAssets: Class not found: %s"), *ClassName);
			return Results;
		}
	}
	
	TArray<FAssetData> AssetDataList;
	AssetRegistry.GetAssets(Filter, AssetDataList);
	
	for (const FAssetData& AssetData : AssetDataList)
	{
		Results.Add(AssetData.GetObjectPathString());
	}
	
	Results.Sort();
	return Results;
}

FDataAssetClassInfo UDataAssetService::GetClassInfo(const FString& ClassName, bool bIncludeAll)
{
	FDataAssetClassInfo Result;
	
	UClass* AssetClass = FindDataAssetClass(ClassName);
	if (!AssetClass)
	{
		UE_LOG(LogDataAssetService, Warning, TEXT("GetClassInfo: Class not found: %s"), *ClassName);
		return Result;
	}
	
	Result.Name = AssetClass->GetName();
	Result.Path = AssetClass->GetPathName();
	Result.bIsAbstract = AssetClass->HasAnyClassFlags(CLASS_Abstract);
	Result.bIsNative = !AssetClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
	
	// Parent chain
	UClass* CurrentClass = AssetClass->GetSuperClass();
	while (CurrentClass && CurrentClass != UObject::StaticClass())
	{
		Result.ParentClasses.Add(CurrentClass->GetName());
		CurrentClass = CurrentClass->GetSuperClass();
	}
	
	// Properties
	for (TFieldIterator<FProperty> PropIt(AssetClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!ShouldExposeProperty(Property, bIncludeAll))
		{
			continue;
		}
		
		FDataAssetPropertyInfo PropInfo;
		PropInfo.Name = Property->GetName();
		PropInfo.Type = GetPropertyTypeString(Property);
		PropInfo.Category = Property->GetMetaData(TEXT("Category"));
		PropInfo.Description = Property->GetMetaData(TEXT("ToolTip"));
		PropInfo.DefinedIn = Property->GetOwnerClass()->GetName();
		PropInfo.bReadOnly = Property->HasAnyPropertyFlags(CPF_EditConst);
		PropInfo.bIsArray = Property->IsA<FArrayProperty>();
		
		if (bIncludeAll)
		{
			TArray<FString> Flags;
			if (Property->HasAnyPropertyFlags(CPF_Edit)) Flags.Add(TEXT("Edit"));
			if (Property->HasAnyPropertyFlags(CPF_BlueprintVisible)) Flags.Add(TEXT("BlueprintVisible"));
			if (Property->HasAnyPropertyFlags(CPF_SaveGame)) Flags.Add(TEXT("SaveGame"));
			if (Property->HasAnyPropertyFlags(CPF_EditConst)) Flags.Add(TEXT("EditConst"));
			if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate)) Flags.Add(TEXT("Private"));
			if (Property->HasAnyPropertyFlags(CPF_NativeAccessSpecifierProtected)) Flags.Add(TEXT("Protected"));
			PropInfo.Flags = FString::Join(Flags, TEXT(", "));
		}
		
		Result.Properties.Add(PropInfo);
	}
	
	return Result;
}

// =================================================================
// Lifecycle Actions
// =================================================================

FString UDataAssetService::CreateDataAsset(
	const FString& ClassName,
	const FString& AssetPath,
	const FString& AssetName,
	const FString& PropertiesJson)
{
	if (ClassName.IsEmpty())
	{
		UE_LOG(LogDataAssetService, Error, TEXT("CreateDataAsset: ClassName is required"));
		return TEXT("");
	}
	
	if (AssetName.IsEmpty())
	{
		UE_LOG(LogDataAssetService, Error, TEXT("CreateDataAsset: AssetName is required"));
		return TEXT("");
	}
	
	UClass* DataAssetClass = FindDataAssetClass(ClassName);
	if (!DataAssetClass)
	{
		UE_LOG(LogDataAssetService, Error, TEXT("CreateDataAsset: Class not found: %s"), *ClassName);
		return TEXT("");
	}
	
	// Verify it's a data asset class
	if (!DataAssetClass->IsChildOf(UDataAsset::StaticClass()))
	{
		UE_LOG(LogDataAssetService, Error, TEXT("CreateDataAsset: %s is not a DataAsset class"), *ClassName);
		return TEXT("");
	}
	
	// Use default path if not provided
	FString FinalPath = AssetPath.IsEmpty() ? TEXT("/Game/Data") : AssetPath;

	// Check if asset already exists to avoid blocking overwrite dialog
	FString FullAssetPath = FinalPath / AssetName;
	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		UE_LOG(LogDataAssetService, Error, TEXT("CreateDataAsset: Asset '%s' already exists at '%s'. Delete it first or use a different name."), *AssetName, *FullAssetPath);
		return TEXT("");
	}
	
	// Create the asset using asset tools
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	
	// Create factory
	UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
	Factory->DataAssetClass = DataAssetClass;
	
	UObject* NewAsset = AssetTools.CreateAsset(AssetName, FinalPath, DataAssetClass, Factory);
	
	if (!NewAsset)
	{
		UE_LOG(LogDataAssetService, Error, TEXT("CreateDataAsset: Failed to create asset at %s/%s"), *FinalPath, *AssetName);
		return TEXT("");
	}
	
	UDataAsset* DataAsset = Cast<UDataAsset>(NewAsset);
	
	// Apply initial properties if provided
	if (!PropertiesJson.IsEmpty())
	{
		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PropertiesJson);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			for (auto& Pair : JsonObj->Values)
			{
				FProperty* Property = DataAssetClass->FindPropertyByName(*Pair.Key);
				if (Property && ShouldExposeProperty(Property))
				{
					FString ValueStr;
					if (Pair.Value->TryGetString(ValueStr))
					{
						FString Error;
						if (!SetPropertyFromString(Property, DataAsset, ValueStr, Error))
						{
							UE_LOG(LogDataAssetService, Warning, TEXT("CreateDataAsset: Failed to set property %s: %s"), *Pair.Key, *Error);
						}
					}
				}
			}
		}
	}
	
	// Mark dirty
	NewAsset->MarkPackageDirty();
	
	return NewAsset->GetPathName();
}

// =================================================================
// Information Actions
// =================================================================

FDataAssetInstanceInfo UDataAssetService::GetInfo(const FString& AssetPath)
{
	FDataAssetInstanceInfo Result;
	
	UDataAsset* DataAsset = LoadDataAsset(AssetPath);
	if (!DataAsset)
	{
		UE_LOG(LogDataAssetService, Warning, TEXT("GetInfo: Failed to load DataAsset: %s"), *AssetPath);
		return Result;
	}
	
	UClass* AssetClass = DataAsset->GetClass();
	
	Result.Name = DataAsset->GetName();
	Result.Path = DataAsset->GetPathName();
	Result.ClassName = AssetClass->GetName();
	Result.ClassPath = AssetClass->GetPathName();
	
	// Parent class chain
	UClass* CurrentClass = AssetClass->GetSuperClass();
	while (CurrentClass && CurrentClass != UObject::StaticClass())
	{
		Result.ParentClasses.Add(CurrentClass->GetName());
		CurrentClass = CurrentClass->GetSuperClass();
	}
	
	// Export properties to JSON
	FString JsonString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
	JsonWriter->WriteObjectStart();
	
	for (TFieldIterator<FProperty> PropIt(AssetClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!ShouldExposeProperty(Property))
		{
			continue;
		}
		
		FString Value = PropertyToString(Property, DataAsset);
		JsonWriter->WriteValue(Property->GetName(), Value);
	}
	
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	
	Result.PropertiesJson = JsonString;
	
	return Result;
}

TArray<FDataAssetPropertyInfo> UDataAssetService::ListProperties(
	const FString& AssetPath,
	const FString& ClassName,
	bool bIncludeAll)
{
	TArray<FDataAssetPropertyInfo> Results;
	
	UClass* AssetClass = nullptr;
	
	if (!AssetPath.IsEmpty())
	{
		UDataAsset* DataAsset = LoadDataAsset(AssetPath);
		if (DataAsset)
		{
			AssetClass = DataAsset->GetClass();
		}
	}
	else if (!ClassName.IsEmpty())
	{
		AssetClass = FindDataAssetClass(ClassName);
	}
	
	if (!AssetClass)
	{
		UE_LOG(LogDataAssetService, Warning, TEXT("ListProperties: Could not find class. Provide asset_path or class_name"));
		return Results;
	}
	
	for (TFieldIterator<FProperty> PropIt(AssetClass, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!ShouldExposeProperty(Property, bIncludeAll))
		{
			continue;
		}
		
		FDataAssetPropertyInfo PropInfo;
		PropInfo.Name = Property->GetName();
		PropInfo.Type = GetPropertyTypeString(Property);
		PropInfo.Category = Property->GetMetaData(TEXT("Category"));
		PropInfo.Description = Property->GetMetaData(TEXT("ToolTip"));
		PropInfo.DefinedIn = Property->GetOwnerClass()->GetName();
		PropInfo.bReadOnly = Property->HasAnyPropertyFlags(CPF_EditConst);
		PropInfo.bIsArray = Property->IsA<FArrayProperty>();
		
		if (bIncludeAll)
		{
			TArray<FString> Flags;
			if (Property->HasAnyPropertyFlags(CPF_Edit)) Flags.Add(TEXT("Edit"));
			if (Property->HasAnyPropertyFlags(CPF_BlueprintVisible)) Flags.Add(TEXT("BlueprintVisible"));
			if (Property->HasAnyPropertyFlags(CPF_SaveGame)) Flags.Add(TEXT("SaveGame"));
			if (Property->HasAnyPropertyFlags(CPF_EditConst)) Flags.Add(TEXT("EditConst"));
			PropInfo.Flags = FString::Join(Flags, TEXT(", "));
		}
		
		Results.Add(PropInfo);
	}
	
	return Results;
}

// =================================================================
// Property Actions
// =================================================================

FString UDataAssetService::GetProperty(const FString& AssetPath, const FString& PropertyName)
{
	UDataAsset* DataAsset = LoadDataAsset(AssetPath);
	if (!DataAsset)
	{
		UE_LOG(LogDataAssetService, Warning, TEXT("GetProperty: Failed to load DataAsset: %s"), *AssetPath);
		return TEXT("");
	}
	
	UClass* AssetClass = DataAsset->GetClass();
	FProperty* Property = AssetClass->FindPropertyByName(*PropertyName);
	
	if (!Property)
	{
		UE_LOG(LogDataAssetService, Warning, TEXT("GetProperty: Property not found: %s"), *PropertyName);
		return TEXT("");
	}
	
	return PropertyToString(Property, DataAsset);
}

bool UDataAssetService::SetProperty(const FString& AssetPath, const FString& PropertyName, const FString& PropertyValue)
{
	UDataAsset* DataAsset = LoadDataAsset(AssetPath);
	if (!DataAsset)
	{
		UE_LOG(LogDataAssetService, Error, TEXT("SetProperty: Failed to load DataAsset: %s"), *AssetPath);
		return false;
	}
	
	UClass* AssetClass = DataAsset->GetClass();
	FProperty* Property = AssetClass->FindPropertyByName(*PropertyName);
	
	if (!Property)
	{
		UE_LOG(LogDataAssetService, Error, TEXT("SetProperty: Property not found: %s"), *PropertyName);
		return false;
	}
	
	if (!ShouldExposeProperty(Property))
	{
		UE_LOG(LogDataAssetService, Error, TEXT("SetProperty: Property is not editable: %s"), *PropertyName);
		return false;
	}
	
	FString Error;
	if (!SetPropertyFromString(Property, DataAsset, PropertyValue, Error))
	{
		UE_LOG(LogDataAssetService, Error, TEXT("SetProperty: %s"), *Error);
		return false;
	}
	
	DataAsset->MarkPackageDirty();
	return true;
}

FDataAssetSetPropertiesResult UDataAssetService::SetProperties(const FString& AssetPath, const FString& PropertiesJson)
{
	FDataAssetSetPropertiesResult Result;
	
	UDataAsset* DataAsset = LoadDataAsset(AssetPath);
	if (!DataAsset)
	{
		UE_LOG(LogDataAssetService, Error, TEXT("SetProperties: Failed to load DataAsset: %s"), *AssetPath);
		return Result;
	}
	
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PropertiesJson);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogDataAssetService, Error, TEXT("SetProperties: Invalid JSON"));
		return Result;
	}
	
	UClass* AssetClass = DataAsset->GetClass();
	
	for (auto& Pair : JsonObj->Values)
	{
		FProperty* Property = AssetClass->FindPropertyByName(*Pair.Key);
		if (!Property)
		{
			Result.FailedProperties.Add(FString::Printf(TEXT("%s: not found"), *Pair.Key));
			continue;
		}
		
		if (!ShouldExposeProperty(Property))
		{
			Result.FailedProperties.Add(FString::Printf(TEXT("%s: not editable"), *Pair.Key));
			continue;
		}
		
		FString ValueStr;
		if (!Pair.Value->TryGetString(ValueStr))
		{
			// Try to serialize non-string JSON values
			TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ValueStr);
			FJsonSerializer::Serialize(Pair.Value.ToSharedRef(), TEXT(""), Writer);
		}
		
		FString Error;
		if (SetPropertyFromString(Property, DataAsset, ValueStr, Error))
		{
			Result.SuccessProperties.Add(Pair.Key);
		}
		else
		{
			Result.FailedProperties.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *Error));
		}
	}
	
	if (Result.SuccessProperties.Num() > 0)
	{
		DataAsset->MarkPackageDirty();
	}
	
	return Result;
}

// =================================================================
// Legacy Compatibility
// =================================================================

FString UDataAssetService::GetPropertiesAsJson(const FString& AssetPath)
{
	UDataAsset* DataAsset = LoadDataAsset(AssetPath);
	if (!DataAsset)
	{
		UE_LOG(LogDataAssetService, Warning, TEXT("GetPropertiesAsJson: Failed to load DataAsset: %s"), *AssetPath);
		return TEXT("");
	}
	
	UClass* AssetClass = DataAsset->GetClass();
	
	FString JsonString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
	JsonWriter->WriteObjectStart();
	
	for (TFieldIterator<FProperty> PropIt(AssetClass); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (Property && !Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
		{
			FString Value = PropertyToString(Property, DataAsset);
			JsonWriter->WriteValue(Property->GetName(), Value);
		}
	}
	
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	return JsonString;
}

// =================================================================
// Existence Checks
// =================================================================

bool UDataAssetService::DataAssetExists(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return false;
	}
	return UEditorAssetLibrary::DoesAssetExist(AssetPath);
}
