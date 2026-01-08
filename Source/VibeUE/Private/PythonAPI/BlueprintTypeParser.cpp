// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "PythonAPI/BlueprintTypeParser.h"
#include "EdGraphSchema_K2.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"
#include "UObject/StructOnScope.h"

const TMap<FString, FName>& FBlueprintTypeParser::GetBasicTypeMap()
{
	static TMap<FString, FName> BasicTypeMap;
	if (BasicTypeMap.Num() == 0)
	{
		BasicTypeMap.Add(TEXT("bool"), UEdGraphSchema_K2::PC_Boolean);
		BasicTypeMap.Add(TEXT("byte"), UEdGraphSchema_K2::PC_Byte);
		BasicTypeMap.Add(TEXT("int"), UEdGraphSchema_K2::PC_Int);
		BasicTypeMap.Add(TEXT("int32"), UEdGraphSchema_K2::PC_Int);
		BasicTypeMap.Add(TEXT("int64"), UEdGraphSchema_K2::PC_Int64);
		BasicTypeMap.Add(TEXT("float"), UEdGraphSchema_K2::PC_Real);  // UE 5.x uses PC_Real with subcategory
		BasicTypeMap.Add(TEXT("double"), UEdGraphSchema_K2::PC_Real); // UE 5.x uses PC_Real with subcategory
		BasicTypeMap.Add(TEXT("FName"), UEdGraphSchema_K2::PC_Name);
		BasicTypeMap.Add(TEXT("FString"), UEdGraphSchema_K2::PC_String);
		BasicTypeMap.Add(TEXT("FText"), UEdGraphSchema_K2::PC_Text);
	}
	return BasicTypeMap;
}

const TMap<FString, FString>& FBlueprintTypeParser::GetTypeAliases()
{
	static TMap<FString, FString> TypeAliases;
	if (TypeAliases.Num() == 0)
	{
		// Case-insensitive basic types
		TypeAliases.Add(TEXT("Bool"), TEXT("bool"));
		TypeAliases.Add(TEXT("Int"), TEXT("int"));
		TypeAliases.Add(TEXT("Float"), TEXT("float"));
		TypeAliases.Add(TEXT("String"), TEXT("FString"));
		TypeAliases.Add(TEXT("Name"), TEXT("FName"));
		TypeAliases.Add(TEXT("Text"), TEXT("FText"));

		// Common abbreviations
		TypeAliases.Add(TEXT("Vec"), TEXT("FVector"));
		TypeAliases.Add(TEXT("Vector"), TEXT("FVector"));
		TypeAliases.Add(TEXT("Rot"), TEXT("FRotator"));
		TypeAliases.Add(TEXT("Rotator"), TEXT("FRotator"));
		TypeAliases.Add(TEXT("Transform"), TEXT("FTransform"));

		// Actor shortcuts
		TypeAliases.Add(TEXT("Actor"), TEXT("AActor"));
		TypeAliases.Add(TEXT("Pawn"), TEXT("APawn"));
		TypeAliases.Add(TEXT("Character"), TEXT("ACharacter"));
		TypeAliases.Add(TEXT("PlayerController"), TEXT("APlayerController"));

		// Component shortcuts
		TypeAliases.Add(TEXT("StaticMeshComponent"), TEXT("UStaticMeshComponent"));
		TypeAliases.Add(TEXT("SkeletalMeshComponent"), TEXT("USkeletalMeshComponent"));
	}
	return TypeAliases;
}

FString FBlueprintTypeParser::ResolveTypeAlias(const FString& TypeString)
{
	const TMap<FString, FString>& Aliases = GetTypeAliases();
	if (const FString* Resolved = Aliases.Find(TypeString))
	{
		return *Resolved;
	}
	return TypeString;
}

bool FBlueprintTypeParser::IsBasicType(const FString& TypeString)
{
	return GetBasicTypeMap().Contains(TypeString);
}

bool FBlueprintTypeParser::IsStructType(const FString& TypeString)
{
	return TypeString.StartsWith(TEXT("F")) && !IsBasicType(TypeString);
}

bool FBlueprintTypeParser::IsObjectType(const FString& TypeString)
{
	return (TypeString.StartsWith(TEXT("U")) || TypeString.StartsWith(TEXT("A"))) && !IsClassType(TypeString);
}

bool FBlueprintTypeParser::IsClassType(const FString& TypeString)
{
	return TypeString.StartsWith(TEXT("TSubclassOf<")) || TypeString.StartsWith(TEXT("SubclassOf<"));
}

bool FBlueprintTypeParser::IsEnumType(const FString& TypeString)
{
	return TypeString.StartsWith(TEXT("E"));
}

UScriptStruct* FBlueprintTypeParser::FindStructByName(const FString& StructName)
{
	// Try direct find first
	UScriptStruct* FoundStruct = FindObject<UScriptStruct>(nullptr, *StructName);
	if (FoundStruct)
	{
		return FoundStruct;
	}

	// Search all loaded structs
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (Struct->GetName() == StructName || Struct->GetPrefixCPP() + Struct->GetName() == StructName)
		{
			return Struct;
		}
	}

	return nullptr;
}

UClass* FBlueprintTypeParser::FindClassByName(const FString& ClassName)
{
	// Try direct find first
	UClass* FoundClass = FindObject<UClass>(nullptr, *ClassName);
	if (FoundClass)
	{
		return FoundClass;
	}

	// Search all loaded classes
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->GetName() == ClassName || Class->GetPrefixCPP() + Class->GetName() == ClassName)
		{
			return Class;
		}
	}

	return nullptr;
}

UEnum* FBlueprintTypeParser::FindEnumByName(const FString& EnumName)
{
	// Try direct find first
	UEnum* FoundEnum = FindObject<UEnum>(nullptr, *EnumName);
	if (FoundEnum)
	{
		return FoundEnum;
	}

	// Search all loaded enums
	for (TObjectIterator<UEnum> It; It; ++It)
	{
		UEnum* Enum = *It;
		if (Enum->GetName() == EnumName)
		{
			return Enum;
		}
	}

	return nullptr;
}

bool FBlueprintTypeParser::ParseBasicType(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutErrorMessage)
{
	const TMap<FString, FName>& TypeMap = GetBasicTypeMap();
	if (const FName* Category = TypeMap.Find(TypeString))
	{
		OutPinType.PinCategory = *Category;
		
		// UE 5.x requires PinSubCategory for PC_Real types (float/double)
		if (*Category == UEdGraphSchema_K2::PC_Real)
		{
			if (TypeString == TEXT("float"))
			{
				OutPinType.PinSubCategory = TEXT("float");
			}
			else if (TypeString == TEXT("double"))
			{
				OutPinType.PinSubCategory = TEXT("double");
			}
		}
		
		return true;
	}

	OutErrorMessage = FString::Printf(TEXT("Unknown basic type '%s'"), *TypeString);
	return false;
}

bool FBlueprintTypeParser::ParseStructType(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutErrorMessage)
{
	UScriptStruct* Struct = FindStructByName(TypeString);
	if (!Struct)
	{
		OutErrorMessage = FString::Printf(TEXT("Struct '%s' not found. Check spelling and ensure the struct is loaded."), *TypeString);
		return false;
	}

	OutPinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	OutPinType.PinSubCategoryObject = Struct;
	return true;
}

bool FBlueprintTypeParser::ParseObjectType(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutErrorMessage)
{
	UClass* Class = FindClassByName(TypeString);
	if (!Class)
	{
		OutErrorMessage = FString::Printf(TEXT("Class '%s' not found. Check spelling and module dependencies."), *TypeString);
		return false;
	}

	OutPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
	OutPinType.PinSubCategoryObject = Class;
	return true;
}

bool FBlueprintTypeParser::ParseClassType(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutErrorMessage)
{
	// Extract inner type from TSubclassOf<T> or SubclassOf<T>
	FString InnerType;
	int32 StartIndex = TypeString.Find(TEXT("<"));
	int32 EndIndex = TypeString.Find(TEXT(">"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	if (StartIndex == INDEX_NONE || EndIndex == INDEX_NONE || EndIndex <= StartIndex)
	{
		OutErrorMessage = FString::Printf(TEXT("Invalid TSubclassOf syntax: '%s'. Expected format: 'TSubclassOf<AActor>'"), *TypeString);
		return false;
	}

	InnerType = TypeString.Mid(StartIndex + 1, EndIndex - StartIndex - 1).TrimStartAndEnd();

	UClass* Class = FindClassByName(InnerType);
	if (!Class)
	{
		OutErrorMessage = FString::Printf(TEXT("Class '%s' not found in TSubclassOf<%s>"), *InnerType, *InnerType);
		return false;
	}

	OutPinType.PinCategory = UEdGraphSchema_K2::PC_Class;
	OutPinType.PinSubCategoryObject = Class;
	return true;
}

bool FBlueprintTypeParser::ParseEnumType(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutErrorMessage)
{
	UEnum* Enum = FindEnumByName(TypeString);
	if (!Enum)
	{
		OutErrorMessage = FString::Printf(TEXT("Enum '%s' not found. Check spelling and ensure enum is Blueprint-exposed."), *TypeString);
		return false;
	}

	OutPinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
	OutPinType.PinSubCategoryObject = Enum;
	return true;
}

EPinContainerType FBlueprintTypeParser::GetContainerTypeEnum(const FString& ContainerString)
{
	if (ContainerString.IsEmpty())
	{
		return EPinContainerType::None;
	}

	if (ContainerString.Equals(TEXT("Array"), ESearchCase::IgnoreCase))
	{
		return EPinContainerType::Array;
	}

	if (ContainerString.Equals(TEXT("Set"), ESearchCase::IgnoreCase))
	{
		return EPinContainerType::Set;
	}

	if (ContainerString.Equals(TEXT("Map"), ESearchCase::IgnoreCase))
	{
		return EPinContainerType::Map;
	}

	return EPinContainerType::None;
}

bool FBlueprintTypeParser::ParseTypeString(
	const FString& TypeString,
	FEdGraphPinType& OutPinType,
	bool bIsArray,
	const FString& ContainerType,
	FString& OutErrorMessage)
{
	if (TypeString.IsEmpty())
	{
		OutErrorMessage = TEXT("Type string cannot be empty");
		return false;
	}

	// Resolve aliases
	FString ResolvedType = ResolveTypeAlias(TypeString);

	// Reset pin type
	OutPinType = FEdGraphPinType();

	// Parse the base type
	bool bSuccess = false;

	if (IsBasicType(ResolvedType))
	{
		bSuccess = ParseBasicType(ResolvedType, OutPinType, OutErrorMessage);
	}
	else if (IsClassType(ResolvedType))
	{
		bSuccess = ParseClassType(ResolvedType, OutPinType, OutErrorMessage);
	}
	else if (IsStructType(ResolvedType))
	{
		bSuccess = ParseStructType(ResolvedType, OutPinType, OutErrorMessage);
	}
	else if (IsObjectType(ResolvedType))
	{
		bSuccess = ParseObjectType(ResolvedType, OutPinType, OutErrorMessage);
	}
	else if (IsEnumType(ResolvedType))
	{
		bSuccess = ParseEnumType(ResolvedType, OutPinType, OutErrorMessage);
	}
	else
	{
		OutErrorMessage = FString::Printf(TEXT("Unknown type '%s' - not a basic type, struct, class, object, or enum"), *ResolvedType);
		return false;
	}

	if (!bSuccess)
	{
		return false;
	}

	// Set container type
	if (bIsArray || !ContainerType.IsEmpty())
	{
		EPinContainerType Container = GetContainerTypeEnum(ContainerType);
		if (Container == EPinContainerType::None && bIsArray)
		{
			Container = EPinContainerType::Array;
		}

		if (Container == EPinContainerType::Map)
		{
			OutErrorMessage = TEXT("Map container type requires explicit key and value types (not yet fully supported via this API)");
			return false;
		}

		OutPinType.ContainerType = Container;
	}

	return true;
}

FString FBlueprintTypeParser::GetFriendlyTypeName(const FEdGraphPinType& PinType)
{
	FString TypeName;

	// Get base category name
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean) TypeName = TEXT("bool");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte) TypeName = TEXT("byte");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int) TypeName = TEXT("int");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64) TypeName = TEXT("int64");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		// UE 5.x: PC_Real uses PinSubCategory to distinguish float vs double
		if (PinType.PinSubCategory == TEXT("double"))
			TypeName = TEXT("double");
		else
			TypeName = TEXT("float");
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Double) TypeName = TEXT("double");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name) TypeName = TEXT("FName");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_String) TypeName = TEXT("FString");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text) TypeName = TEXT("FText");
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && PinType.PinSubCategoryObject.IsValid())
	{
		TypeName = PinType.PinSubCategoryObject->GetName();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object && PinType.PinSubCategoryObject.IsValid())
	{
		TypeName = PinType.PinSubCategoryObject->GetName();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Class && PinType.PinSubCategoryObject.IsValid())
	{
		TypeName = FString::Printf(TEXT("TSubclassOf<%s>"), *PinType.PinSubCategoryObject->GetName());
	}
	else
	{
		TypeName = PinType.PinCategory.ToString();
	}

	// Add container wrapper
	if (PinType.ContainerType == EPinContainerType::Array)
	{
		TypeName = FString::Printf(TEXT("TArray<%s>"), *TypeName);
	}
	else if (PinType.ContainerType == EPinContainerType::Set)
	{
		TypeName = FString::Printf(TEXT("TSet<%s>"), *TypeName);
	}
	else if (PinType.ContainerType == EPinContainerType::Map)
	{
		TypeName = FString::Printf(TEXT("TMap<?, %s>"), *TypeName);
	}

	return TypeName;
}
