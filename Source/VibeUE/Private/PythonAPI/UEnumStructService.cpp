// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UEnumStructService.h"
#include "PythonAPI/BlueprintTypeParser.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/EnumFactory.h"
#include "Factories/StructureFactory.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Kismet2/StructureEditorUtils.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "UObject/UObjectIterator.h"
#include "UObject/StructOnScope.h"
#include "EdGraph/EdGraphPin.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnumStructService, Log, All);

// =============================================================================
// HELPER METHODS
// =============================================================================

UUserDefinedEnum* UEnumStructService::LoadUserDefinedEnum(const FString& EnumPathOrName)
{
	if (EnumPathOrName.IsEmpty())
	{
		return nullptr;
	}

	// Try loading by path first
	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(EnumPathOrName);
	if (UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(LoadedAsset))
	{
		return Enum;
	}

	// Try finding by name
	UEnum* FoundEnum = FindEnum(EnumPathOrName);
	return Cast<UUserDefinedEnum>(FoundEnum);
}

UEnum* UEnumStructService::FindEnum(const FString& EnumPathOrName)
{
	if (EnumPathOrName.IsEmpty())
	{
		return nullptr;
	}

	// Try loading by path first
	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(EnumPathOrName);
	if (UEnum* Enum = Cast<UEnum>(LoadedAsset))
	{
		return Enum;
	}

	// Use BlueprintTypeParser's FindEnumByName
	return FBlueprintTypeParser::FindEnumByName(EnumPathOrName);
}

UUserDefinedStruct* UEnumStructService::LoadUserDefinedStruct(const FString& StructPathOrName)
{
	if (StructPathOrName.IsEmpty())
	{
		return nullptr;
	}

	// Try loading by path first
	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(StructPathOrName);
	if (UUserDefinedStruct* Struct = Cast<UUserDefinedStruct>(LoadedAsset))
	{
		return Struct;
	}

	// Try finding by name
	UScriptStruct* FoundStruct = FindStruct(StructPathOrName);
	return Cast<UUserDefinedStruct>(FoundStruct);
}

UScriptStruct* UEnumStructService::FindStruct(const FString& StructPathOrName)
{
	if (StructPathOrName.IsEmpty())
	{
		return nullptr;
	}

	// Try loading by path first
	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(StructPathOrName);
	if (UScriptStruct* Struct = Cast<UScriptStruct>(LoadedAsset))
	{
		return Struct;
	}

	// Use BlueprintTypeParser's FindStructByName
	return FBlueprintTypeParser::FindStructByName(StructPathOrName);
}

FString UEnumStructService::GetPropertyTypeString(FProperty* Property)
{
	if (!Property)
	{
		return TEXT("Unknown");
	}

	// Handle common types with friendly names
	if (CastField<FBoolProperty>(Property)) return TEXT("bool");
	if (CastField<FIntProperty>(Property)) return TEXT("int32");
	if (CastField<FInt64Property>(Property)) return TEXT("int64");
	if (CastField<FFloatProperty>(Property)) return TEXT("float");
	if (CastField<FDoubleProperty>(Property)) return TEXT("double");
	if (CastField<FStrProperty>(Property)) return TEXT("FString");
	if (CastField<FNameProperty>(Property)) return TEXT("FName");
	if (CastField<FTextProperty>(Property)) return TEXT("FText");

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
			return PropClass->GetName();
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

int32 UEnumStructService::FindEnumValueIndex(UEnum* Enum, const FString& ValueName)
{
	if (!Enum)
	{
		return INDEX_NONE;
	}

	// Try exact match first
	for (int32 i = 0; i < Enum->NumEnums(); ++i)
	{
		FString Name = Enum->GetNameStringByIndex(i);
		// Check both with and without enum prefix (e.g., "MyEnum::Value" vs "Value")
		if (Name.Equals(ValueName, ESearchCase::IgnoreCase))
		{
			return i;
		}
		// Check short name
		FString ShortName = Name;
		int32 ColonIndex;
		if (ShortName.FindLastChar(TEXT(':'), ColonIndex))
		{
			ShortName = ShortName.RightChop(ColonIndex + 1);
		}
		if (ShortName.Equals(ValueName, ESearchCase::IgnoreCase))
		{
			return i;
		}
	}

	return INDEX_NONE;
}

FGuid UEnumStructService::FindPropertyGuid(UUserDefinedStruct* Struct, const FString& PropertyName)
{
	if (!Struct)
	{
		return FGuid();
	}

	// Iterate through variable descriptions to find the GUID
	const TArray<FStructVariableDescription>& VarDescriptions = FStructureEditorUtils::GetVarDesc(Struct);
	for (const FStructVariableDescription& Desc : VarDescriptions)
	{
		if (Desc.FriendlyName.Equals(PropertyName, ESearchCase::IgnoreCase) ||
			Desc.VarName.ToString().Equals(PropertyName, ESearchCase::IgnoreCase))
		{
			return Desc.VarGuid;
		}
	}

	return FGuid();
}

// =============================================================================
// ENUM DISCOVERY
// =============================================================================

TArray<FEnumSearchResult> UEnumStructService::SearchEnums(
	const FString& SearchFilter,
	bool bUserDefinedOnly,
	int32 MaxResults)
{
	TArray<FEnumSearchResult> Results;

	for (TObjectIterator<UEnum> It; It; ++It)
	{
		UEnum* Enum = *It;
		if (!Enum)
		{
			continue;
		}

		FString EnumName = Enum->GetName();

		// Skip MAX entries and internal enums
		if (EnumName.Contains(TEXT("_MAX")) || EnumName.StartsWith(TEXT("E_")))
		{
			continue;
		}

		bool bIsUserDefined = Enum->IsA<UUserDefinedEnum>();

		// Filter by user-defined only
		if (bUserDefinedOnly && !bIsUserDefined)
		{
			continue;
		}

		// Apply search filter
		if (!SearchFilter.IsEmpty() && !EnumName.Contains(SearchFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		FEnumSearchResult Result;
		Result.Name = EnumName;
		Result.Path = Enum->GetPathName();
		Result.bIsUserDefined = bIsUserDefined;
		Result.ValueCount = FMath::Max(0, Enum->NumEnums() - 1); // Exclude _MAX
		Results.Add(Result);

		if (MaxResults > 0 && Results.Num() >= MaxResults)
		{
			break;
		}
	}

	// Sort alphabetically
	Results.Sort([](const FEnumSearchResult& A, const FEnumSearchResult& B) {
		return A.Name < B.Name;
	});

	return Results;
}

bool UEnumStructService::GetEnumInfo(const FString& EnumPathOrName, FEnumInfo& OutInfo)
{
	UEnum* Enum = FindEnum(EnumPathOrName);
	if (!Enum)
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("GetEnumInfo: Enum not found: %s"), *EnumPathOrName);
		return false;
	}

	OutInfo.Name = Enum->GetName();
	OutInfo.Path = Enum->GetPathName();
	OutInfo.bIsUserDefined = Enum->IsA<UUserDefinedEnum>();
	OutInfo.bIsBitFlags = Enum->HasAnyEnumFlags(EEnumFlags::Flags);
	OutInfo.Module = Enum->GetOutermost()->GetName();

	// Get all values (excluding _MAX)
	int32 NumEnums = Enum->NumEnums();
	OutInfo.ValueCount = 0;

	for (int32 i = 0; i < NumEnums; ++i)
	{
		FString ValueName = Enum->GetNameStringByIndex(i);

		// Skip _MAX value
		if (ValueName.EndsWith(TEXT("_MAX")))
		{
			continue;
		}

		FEnumValueInfo ValueInfo;
		ValueInfo.Name = ValueName;
		ValueInfo.Value = Enum->GetValueByIndex(i);
		ValueInfo.Index = i;

		// Get display name
		ValueInfo.DisplayName = Enum->GetDisplayNameTextByIndex(i).ToString();
		if (ValueInfo.DisplayName.IsEmpty())
		{
			// Extract short name for display
			FString ShortName = ValueName;
			int32 ColonIndex;
			if (ShortName.FindLastChar(TEXT(':'), ColonIndex))
			{
				ShortName = ShortName.RightChop(ColonIndex + 1);
			}
			ValueInfo.DisplayName = ShortName;
		}

		// Get description/tooltip if available
		ValueInfo.Description = Enum->GetToolTipTextByIndex(i).ToString();

		OutInfo.Values.Add(ValueInfo);
		OutInfo.ValueCount++;
	}

	return true;
}

TArray<FString> UEnumStructService::GetEnumValues(const FString& EnumPathOrName)
{
	TArray<FString> Values;

	UEnum* Enum = FindEnum(EnumPathOrName);
	if (!Enum)
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("GetEnumValues: Enum not found: %s"), *EnumPathOrName);
		return Values;
	}

	for (int32 i = 0; i < Enum->NumEnums() - 1; ++i) // -1 to skip _MAX
	{
		FString ValueName = Enum->GetNameStringByIndex(i);
		// Extract short name
		int32 ColonIndex;
		if (ValueName.FindLastChar(TEXT(':'), ColonIndex))
		{
			ValueName = ValueName.RightChop(ColonIndex + 1);
		}
		Values.Add(ValueName);
	}

	return Values;
}

// =============================================================================
// ENUM LIFECYCLE
// =============================================================================

FString UEnumStructService::CreateEnum(const FString& AssetPath, const FString& EnumName)
{
	if (EnumName.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("CreateEnum: EnumName is required"));
		return TEXT("");
	}

	// Ensure E prefix
	FString FinalName = EnumName;
	if (!FinalName.StartsWith(TEXT("E")))
	{
		FinalName = TEXT("E") + FinalName;
	}

	// Normalize path
	FString NormalizedPath = AssetPath.IsEmpty() ? TEXT("/Game/Enums") : AssetPath;
	if (!NormalizedPath.StartsWith(TEXT("/")))
	{
		NormalizedPath = TEXT("/Game/") + NormalizedPath;
	}

	// Check if asset already exists to avoid blocking overwrite dialog
	FString FullAssetPath = NormalizedPath / FinalName;
	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		UE_LOG(LogEnumStructService, Error, TEXT("CreateEnum: Enum '%s' already exists at '%s'. Delete it first or use a different name."), *FinalName, *FullAssetPath);
		return TEXT("");
	}

	// Create the asset using asset tools
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UEnumFactory* Factory = NewObject<UEnumFactory>();
	UObject* NewAsset = AssetTools.CreateAsset(FinalName, NormalizedPath, UUserDefinedEnum::StaticClass(), Factory);

	if (!NewAsset)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("CreateEnum: Failed to create enum at %s/%s"), *NormalizedPath, *FinalName);
		return TEXT("");
	}

	// Register with asset registry
	FAssetRegistryModule::AssetCreated(NewAsset);
	NewAsset->MarkPackageDirty();

	UE_LOG(LogEnumStructService, Log, TEXT("CreateEnum: Created enum at %s"), *NewAsset->GetPathName());

	return NewAsset->GetPathName();
}

bool UEnumStructService::DeleteEnum(const FString& EnumPath)
{
	if (EnumPath.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("DeleteEnum: EnumPath is required"));
		return false;
	}

	UUserDefinedEnum* Enum = LoadUserDefinedEnum(EnumPath);
	if (!Enum)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("DeleteEnum: UserDefinedEnum not found: %s"), *EnumPath);
		return false;
	}

	// Delete the asset
	bool bDeleted = UEditorAssetLibrary::DeleteAsset(Enum->GetPathName());
	if (!bDeleted)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("DeleteEnum: Failed to delete enum: %s"), *EnumPath);
		return false;
	}

	UE_LOG(LogEnumStructService, Log, TEXT("DeleteEnum: Deleted enum: %s"), *EnumPath);
	return true;
}

// =============================================================================
// ENUM VALUE OPERATIONS
// =============================================================================

bool UEnumStructService::AddEnumValue(
	const FString& EnumPath,
	const FString& ValueName,
	const FString& DisplayName)
{
	if (ValueName.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("AddEnumValue: ValueName is required"));
		return false;
	}

	UUserDefinedEnum* Enum = LoadUserDefinedEnum(EnumPath);
	if (!Enum)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("AddEnumValue: UserDefinedEnum not found: %s"), *EnumPath);
		return false;
	}

	// Check if value already exists
	if (FindEnumValueIndex(Enum, ValueName) != INDEX_NONE)
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("AddEnumValue: Value '%s' already exists in enum"), *ValueName);
		return false;
	}

	// Add the enumerator
	FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(Enum);

	// Get the index of the newly added value (before _MAX)
	int32 NewIndex = Enum->NumEnums() - 2;

	if (NewIndex < 0)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("AddEnumValue: Failed to add value to enum"));
		return false;
	}

	// Set the display name
	FText DisplayText = FText::FromString(DisplayName.IsEmpty() ? ValueName : DisplayName);
	FEnumEditorUtils::SetEnumeratorDisplayName(Enum, NewIndex, DisplayText);

	Enum->MarkPackageDirty();

	UE_LOG(LogEnumStructService, Log, TEXT("AddEnumValue: Added value '%s' to enum %s"), *ValueName, *EnumPath);
	return true;
}

bool UEnumStructService::RemoveEnumValue(const FString& EnumPath, const FString& ValueName)
{
	if (ValueName.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("RemoveEnumValue: ValueName is required"));
		return false;
	}

	UUserDefinedEnum* Enum = LoadUserDefinedEnum(EnumPath);
	if (!Enum)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("RemoveEnumValue: UserDefinedEnum not found: %s"), *EnumPath);
		return false;
	}

	// Find the value index
	int32 ValueIndex = FindEnumValueIndex(Enum, ValueName);
	if (ValueIndex == INDEX_NONE)
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("RemoveEnumValue: Value '%s' not found in enum"), *ValueName);
		return false;
	}

	// Remove the enumerator
	FEnumEditorUtils::RemoveEnumeratorFromUserDefinedEnum(Enum, ValueIndex);

	Enum->MarkPackageDirty();

	UE_LOG(LogEnumStructService, Log, TEXT("RemoveEnumValue: Removed value '%s' from enum %s"), *ValueName, *EnumPath);
	return true;
}

bool UEnumStructService::RenameEnumValue(
	const FString& EnumPath,
	const FString& OldValueName,
	const FString& NewValueName)
{
	if (OldValueName.IsEmpty() || NewValueName.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("RenameEnumValue: Both old and new value names are required"));
		return false;
	}

	UUserDefinedEnum* Enum = LoadUserDefinedEnum(EnumPath);
	if (!Enum)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("RenameEnumValue: UserDefinedEnum not found: %s"), *EnumPath);
		return false;
	}

	// Find the value index
	int32 ValueIndex = FindEnumValueIndex(Enum, OldValueName);
	if (ValueIndex == INDEX_NONE)
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("RenameEnumValue: Value '%s' not found in enum"), *OldValueName);
		return false;
	}

	// Set the new display name (this is how UE handles enum value "renaming")
	FText NewDisplayText = FText::FromString(NewValueName);
	FEnumEditorUtils::SetEnumeratorDisplayName(Enum, ValueIndex, NewDisplayText);

	Enum->MarkPackageDirty();

	UE_LOG(LogEnumStructService, Log, TEXT("RenameEnumValue: Renamed value '%s' to '%s' in enum %s"),
		*OldValueName, *NewValueName, *EnumPath);
	return true;
}

bool UEnumStructService::SetEnumValueDisplayName(
	const FString& EnumPath,
	const FString& ValueName,
	const FString& DisplayName)
{
	if (ValueName.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("SetEnumValueDisplayName: ValueName is required"));
		return false;
	}

	UUserDefinedEnum* Enum = LoadUserDefinedEnum(EnumPath);
	if (!Enum)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("SetEnumValueDisplayName: UserDefinedEnum not found: %s"), *EnumPath);
		return false;
	}

	// Find the value index
	int32 ValueIndex = FindEnumValueIndex(Enum, ValueName);
	if (ValueIndex == INDEX_NONE)
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("SetEnumValueDisplayName: Value '%s' not found in enum"), *ValueName);
		return false;
	}

	// Set the display name
	FText DisplayText = FText::FromString(DisplayName);
	FEnumEditorUtils::SetEnumeratorDisplayName(Enum, ValueIndex, DisplayText);

	Enum->MarkPackageDirty();

	UE_LOG(LogEnumStructService, Log, TEXT("SetEnumValueDisplayName: Set display name for '%s' to '%s'"),
		*ValueName, *DisplayName);
	return true;
}

// =============================================================================
// STRUCT DISCOVERY
// =============================================================================

TArray<FStructSearchResult> UEnumStructService::SearchStructs(
	const FString& SearchFilter,
	bool bUserDefinedOnly,
	int32 MaxResults)
{
	TArray<FStructSearchResult> Results;

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (!Struct)
		{
			continue;
		}

		FString StructName = Struct->GetName();

		bool bIsUserDefined = Struct->IsA<UUserDefinedStruct>();

		// Filter by user-defined only
		if (bUserDefinedOnly && !bIsUserDefined)
		{
			continue;
		}

		// Apply search filter
		if (!SearchFilter.IsEmpty() && !StructName.Contains(SearchFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		FStructSearchResult Result;
		Result.Name = StructName;
		Result.Path = Struct->GetPathName();
		Result.bIsUserDefined = bIsUserDefined;

		// Count properties
		Result.PropertyCount = 0;
		for (TFieldIterator<FProperty> PropIt(Struct); PropIt; ++PropIt)
		{
			Result.PropertyCount++;
		}

		Results.Add(Result);

		if (MaxResults > 0 && Results.Num() >= MaxResults)
		{
			break;
		}
	}

	// Sort alphabetically
	Results.Sort([](const FStructSearchResult& A, const FStructSearchResult& B) {
		return A.Name < B.Name;
	});

	return Results;
}

bool UEnumStructService::GetStructInfo(const FString& StructPathOrName, FStructInfo& OutInfo)
{
	UScriptStruct* Struct = FindStruct(StructPathOrName);
	if (!Struct)
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("GetStructInfo: Struct not found: %s"), *StructPathOrName);
		return false;
	}

	OutInfo.Name = Struct->GetName();
	OutInfo.Path = Struct->GetPathName();
	OutInfo.bIsUserDefined = Struct->IsA<UUserDefinedStruct>();
	OutInfo.Module = Struct->GetOutermost()->GetName();
	OutInfo.StructureSize = Struct->GetStructureSize();

	// Get parent struct
	if (UScriptStruct* SuperStruct = Cast<UScriptStruct>(Struct->GetSuperStruct()))
	{
		OutInfo.ParentStruct = SuperStruct->GetName();
	}

	// Get all properties
	OutInfo.PropertyCount = 0;
	int32 PropertyIndex = 0;

	FStructOnScope DefaultStructScope(Struct);
	void* DefaultData = DefaultStructScope.GetStructMemory();

	for (TFieldIterator<FProperty> PropIt(Struct, EFieldIteratorFlags::ExcludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property)
		{
			continue;
		}

		FStructPropertyInfo PropInfo;
		PropInfo.Name = Property->GetName();
		PropInfo.Type = GetPropertyTypeString(Property);
		PropInfo.TypePath = Property->GetCPPType();
		PropInfo.Index = PropertyIndex++;

		// Get category
		PropInfo.Category = Property->GetMetaData(TEXT("Category"));

		// Get description/tooltip
		PropInfo.Description = Property->GetToolTipText().ToString();

		// Check container types
		PropInfo.bIsArray = Property->IsA<FArrayProperty>();
		PropInfo.bIsMap = Property->IsA<FMapProperty>();
		PropInfo.bIsSet = Property->IsA<FSetProperty>();

		// For UserDefinedStruct, get the GUID
		if (UUserDefinedStruct* UDStruct = Cast<UUserDefinedStruct>(Struct))
		{
			FGuid PropertyGuid = FindPropertyGuid(UDStruct, Property->GetName());
			if (PropertyGuid.IsValid())
			{
				PropInfo.Guid = PropertyGuid.ToString();
			}
		}

		// Try to get default value
		if (DefaultData)
		{
			FString DefaultValueStr;
			Property->ExportTextItem_Direct(DefaultValueStr, Property->ContainerPtrToValuePtr<void>(DefaultData), nullptr, nullptr, PPF_None);
			PropInfo.DefaultValue = DefaultValueStr;
		}

		OutInfo.Properties.Add(PropInfo);
		OutInfo.PropertyCount++;
	}

	return true;
}

// =============================================================================
// STRUCT LIFECYCLE
// =============================================================================

FString UEnumStructService::CreateStruct(const FString& AssetPath, const FString& StructName)
{
	if (StructName.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("CreateStruct: StructName is required"));
		return TEXT("");
	}

	// Ensure F prefix
	FString FinalName = StructName;
	if (!FinalName.StartsWith(TEXT("F")))
	{
		FinalName = TEXT("F") + FinalName;
	}

	// Normalize path
	FString NormalizedPath = AssetPath.IsEmpty() ? TEXT("/Game/Structs") : AssetPath;
	if (!NormalizedPath.StartsWith(TEXT("/")))
	{
		NormalizedPath = TEXT("/Game/") + NormalizedPath;
	}

	// Check if asset already exists to avoid blocking overwrite dialog
	FString FullAssetPath = NormalizedPath / FinalName;
	if (UEditorAssetLibrary::DoesAssetExist(FullAssetPath))
	{
		UE_LOG(LogEnumStructService, Error, TEXT("CreateStruct: Struct '%s' already exists at '%s'. Delete it first or use a different name."), *FinalName, *FullAssetPath);
		return TEXT("");
	}

	// Create the asset using asset tools
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	UStructureFactory* Factory = NewObject<UStructureFactory>();
	UObject* NewAsset = AssetTools.CreateAsset(FinalName, NormalizedPath, UUserDefinedStruct::StaticClass(), Factory);

	if (!NewAsset)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("CreateStruct: Failed to create struct at %s/%s"), *NormalizedPath, *FinalName);
		return TEXT("");
	}

	// Register with asset registry
	FAssetRegistryModule::AssetCreated(NewAsset);
	NewAsset->MarkPackageDirty();

	UE_LOG(LogEnumStructService, Log, TEXT("CreateStruct: Created struct at %s"), *NewAsset->GetPathName());

	return NewAsset->GetPathName();
}

bool UEnumStructService::DeleteStruct(const FString& StructPath)
{
	if (StructPath.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("DeleteStruct: StructPath is required"));
		return false;
	}

	UUserDefinedStruct* Struct = LoadUserDefinedStruct(StructPath);
	if (!Struct)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("DeleteStruct: UserDefinedStruct not found: %s"), *StructPath);
		return false;
	}

	// Delete the asset
	bool bDeleted = UEditorAssetLibrary::DeleteAsset(Struct->GetPathName());
	if (!bDeleted)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("DeleteStruct: Failed to delete struct: %s"), *StructPath);
		return false;
	}

	UE_LOG(LogEnumStructService, Log, TEXT("DeleteStruct: Deleted struct: %s"), *StructPath);
	return true;
}

// =============================================================================
// STRUCT PROPERTY OPERATIONS
// =============================================================================

bool UEnumStructService::AddStructProperty(
	const FString& StructPath,
	const FString& PropertyName,
	const FString& PropertyType,
	const FString& DefaultValue,
	const FString& ContainerType)
{
	if (PropertyName.IsEmpty() || PropertyType.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("AddStructProperty: PropertyName and PropertyType are required"));
		return false;
	}

	UUserDefinedStruct* Struct = LoadUserDefinedStruct(StructPath);
	if (!Struct)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("AddStructProperty: UserDefinedStruct not found: %s"), *StructPath);
		return false;
	}

	// Check if property already exists
	if (FindPropertyGuid(Struct, PropertyName).IsValid())
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("AddStructProperty: Property '%s' already exists"), *PropertyName);
		return false;
	}

	// Parse the property type using BlueprintTypeParser
	FEdGraphPinType PinType;
	FString ErrorMessage;
	bool bIsArray = ContainerType.Equals(TEXT("Array"), ESearchCase::IgnoreCase);

	if (!FBlueprintTypeParser::ParseTypeString(PropertyType, PinType, bIsArray, ContainerType, ErrorMessage))
	{
		UE_LOG(LogEnumStructService, Error, TEXT("AddStructProperty: Invalid type '%s': %s"), *PropertyType, *ErrorMessage);
		return false;
	}

	// Add the variable
	bool bAdded = FStructureEditorUtils::AddVariable(Struct, PinType);
	if (!bAdded)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("AddStructProperty: Failed to add property '%s'"), *PropertyName);
		return false;
	}

	FGuid NewVarGuid;
	const TArray<FStructVariableDescription>& VarDescriptions = FStructureEditorUtils::GetVarDesc(Struct);
	if (VarDescriptions.Num() > 0)
	{
		NewVarGuid = VarDescriptions.Last().VarGuid;
	}

	if (!NewVarGuid.IsValid())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("AddStructProperty: Failed to resolve property GUID for '%s'"), *PropertyName);
		return false;
	}

	// Rename to desired name
	bool bRenamed = FStructureEditorUtils::RenameVariable(Struct, NewVarGuid, PropertyName);
	if (!bRenamed)
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("AddStructProperty: Failed to rename property to '%s'"), *PropertyName);
	}

	// Set default value if provided
	if (!DefaultValue.IsEmpty())
	{
		FStructureEditorUtils::ChangeVariableDefaultValue(Struct, NewVarGuid, DefaultValue);
	}

	Struct->MarkPackageDirty();

	UE_LOG(LogEnumStructService, Log, TEXT("AddStructProperty: Added property '%s' of type '%s' to struct %s"),
		*PropertyName, *PropertyType, *StructPath);
	return true;
}

bool UEnumStructService::RemoveStructProperty(const FString& StructPath, const FString& PropertyName)
{
	if (PropertyName.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("RemoveStructProperty: PropertyName is required"));
		return false;
	}

	UUserDefinedStruct* Struct = LoadUserDefinedStruct(StructPath);
	if (!Struct)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("RemoveStructProperty: UserDefinedStruct not found: %s"), *StructPath);
		return false;
	}

	// Find the property GUID
	FGuid PropertyGuid = FindPropertyGuid(Struct, PropertyName);
	if (!PropertyGuid.IsValid())
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("RemoveStructProperty: Property '%s' not found"), *PropertyName);
		return false;
	}

	// Remove the variable
	bool bRemoved = FStructureEditorUtils::RemoveVariable(Struct, PropertyGuid);
	if (!bRemoved)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("RemoveStructProperty: Failed to remove property '%s'"), *PropertyName);
		return false;
	}

	Struct->MarkPackageDirty();

	UE_LOG(LogEnumStructService, Log, TEXT("RemoveStructProperty: Removed property '%s' from struct %s"),
		*PropertyName, *StructPath);
	return true;
}

bool UEnumStructService::RenameStructProperty(
	const FString& StructPath,
	const FString& OldPropertyName,
	const FString& NewPropertyName)
{
	if (OldPropertyName.IsEmpty() || NewPropertyName.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("RenameStructProperty: Both old and new property names are required"));
		return false;
	}

	UUserDefinedStruct* Struct = LoadUserDefinedStruct(StructPath);
	if (!Struct)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("RenameStructProperty: UserDefinedStruct not found: %s"), *StructPath);
		return false;
	}

	// Find the property GUID
	FGuid PropertyGuid = FindPropertyGuid(Struct, OldPropertyName);
	if (!PropertyGuid.IsValid())
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("RenameStructProperty: Property '%s' not found"), *OldPropertyName);
		return false;
	}

	// Rename the variable
	bool bRenamed = FStructureEditorUtils::RenameVariable(Struct, PropertyGuid, NewPropertyName);
	if (!bRenamed)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("RenameStructProperty: Failed to rename property '%s' to '%s'"),
			*OldPropertyName, *NewPropertyName);
		return false;
	}

	Struct->MarkPackageDirty();

	UE_LOG(LogEnumStructService, Log, TEXT("RenameStructProperty: Renamed property '%s' to '%s' in struct %s"),
		*OldPropertyName, *NewPropertyName, *StructPath);
	return true;
}

bool UEnumStructService::ChangeStructPropertyType(
	const FString& StructPath,
	const FString& PropertyName,
	const FString& NewPropertyType)
{
	if (PropertyName.IsEmpty() || NewPropertyType.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("ChangeStructPropertyType: PropertyName and NewPropertyType are required"));
		return false;
	}

	UUserDefinedStruct* Struct = LoadUserDefinedStruct(StructPath);
	if (!Struct)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("ChangeStructPropertyType: UserDefinedStruct not found: %s"), *StructPath);
		return false;
	}

	// Find the property GUID
	FGuid PropertyGuid = FindPropertyGuid(Struct, PropertyName);
	if (!PropertyGuid.IsValid())
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("ChangeStructPropertyType: Property '%s' not found"), *PropertyName);
		return false;
	}

	// Parse the new property type
	FEdGraphPinType NewPinType;
	FString ErrorMessage;

	if (!FBlueprintTypeParser::ParseTypeString(NewPropertyType, NewPinType, false, TEXT(""), ErrorMessage))
	{
		UE_LOG(LogEnumStructService, Error, TEXT("ChangeStructPropertyType: Invalid type '%s': %s"), *NewPropertyType, *ErrorMessage);
		return false;
	}

	// Change the variable type
	bool bChanged = FStructureEditorUtils::ChangeVariableType(Struct, PropertyGuid, NewPinType);
	if (!bChanged)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("ChangeStructPropertyType: Failed to change property '%s' to type '%s'"),
			*PropertyName, *NewPropertyType);
		return false;
	}

	Struct->MarkPackageDirty();

	UE_LOG(LogEnumStructService, Log, TEXT("ChangeStructPropertyType: Changed property '%s' to type '%s' in struct %s"),
		*PropertyName, *NewPropertyType, *StructPath);
	return true;
}

bool UEnumStructService::SetStructPropertyDefault(
	const FString& StructPath,
	const FString& PropertyName,
	const FString& DefaultValue)
{
	if (PropertyName.IsEmpty())
	{
		UE_LOG(LogEnumStructService, Error, TEXT("SetStructPropertyDefault: PropertyName is required"));
		return false;
	}

	UUserDefinedStruct* Struct = LoadUserDefinedStruct(StructPath);
	if (!Struct)
	{
		UE_LOG(LogEnumStructService, Error, TEXT("SetStructPropertyDefault: UserDefinedStruct not found: %s"), *StructPath);
		return false;
	}

	// Find the property GUID
	FGuid PropertyGuid = FindPropertyGuid(Struct, PropertyName);
	if (!PropertyGuid.IsValid())
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("SetStructPropertyDefault: Property '%s' not found"), *PropertyName);
		return false;
	}

	// Set the default value
	bool bChanged = FStructureEditorUtils::ChangeVariableDefaultValue(Struct, PropertyGuid, DefaultValue);
	if (!bChanged)
	{
		UE_LOG(LogEnumStructService, Warning, TEXT("SetStructPropertyDefault: Failed to set default value for '%s'"), *PropertyName);
		return false;
	}

	Struct->MarkPackageDirty();

	UE_LOG(LogEnumStructService, Log, TEXT("SetStructPropertyDefault: Set default value for '%s' to '%s' in struct %s"),
		*PropertyName, *DefaultValue, *StructPath);
	return true;
}

// =============================================================================
// EXISTENCE CHECKS
// =============================================================================

bool UEnumStructService::EnumExists(const FString& EnumPathOrName)
{
	return FindEnum(EnumPathOrName) != nullptr;
}

bool UEnumStructService::StructExists(const FString& StructPathOrName)
{
	return FindStruct(StructPathOrName) != nullptr;
}
