// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UProjectSettingsService.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UObject/UObjectIterator.h"
#include "Engine/DeveloperSettings.h"
#include "GameMapsSettings.h"
#include "GeneralProjectSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectSettingsService, Log, All);

// =================================================================
// Category Mapping System
// =================================================================

namespace
{
	struct FCategoryMapping
	{
		FString CategoryId;
		FString DisplayName;
		FString Description;
		FString SettingsClassName;
		FString ConfigSection;
		FString ConfigFile;
	};

	// Predefined category mappings
	static const TArray<FCategoryMapping> PredefinedCategories = {
		{
			TEXT("general"),
			TEXT("General Project Settings"),
			TEXT("Project name, company, description, and legal information"),
			TEXT("GeneralProjectSettings"),
			TEXT("/Script/EngineSettings.GeneralProjectSettings"),
			TEXT("DefaultGame.ini")
		},
		{
			TEXT("maps"),
			TEXT("Maps & Modes"),
			TEXT("Default maps, game modes, and level transitions"),
			TEXT("GameMapsSettings"),
			TEXT("/Script/EngineSettings.GameMapsSettings"),
			TEXT("DefaultEngine.ini")
		},
		{
			TEXT("custom"),
			TEXT("Custom INI"),
			TEXT("Direct access to any config section/key in any INI file"),
			TEXT(""),
			TEXT(""),
			TEXT("")
		}
	};

	const FCategoryMapping* FindPredefinedCategory(const FString& CategoryId)
	{
		for (const FCategoryMapping& Mapping : PredefinedCategories)
		{
			if (Mapping.CategoryId.Equals(CategoryId, ESearchCase::IgnoreCase))
			{
				return &Mapping;
			}
		}
		return nullptr;
	}

	FString GetConfigFilePath(const FString& ConfigFile)
	{
		if (ConfigFile.IsEmpty())
		{
			return FString();
		}

		// Check if already an absolute path
		if (FPaths::IsRelative(ConfigFile) == false)
		{
			return ConfigFile;
		}

		// Standard config file names
		FString ProjectConfigDir = FPaths::ProjectConfigDir();
		FString FullPath = ProjectConfigDir / ConfigFile;

		return FullPath;
	}

	bool ShouldExposeProperty(FProperty* Property)
	{
		if (!Property)
		{
			return false;
		}

		// Skip deprecated, transient, and non-config properties
		if (Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_Transient))
		{
			return false;
		}

		// Only expose config properties
		return Property->HasAnyPropertyFlags(CPF_Config | CPF_GlobalConfig | CPF_Edit);
	}

	FString GetPropertyTypeString(FProperty* Property)
	{
		if (!Property)
		{
			return TEXT("unknown");
		}

		if (CastField<FBoolProperty>(Property))
		{
			return TEXT("bool");
		}
		if (CastField<FIntProperty>(Property))
		{
			return TEXT("int");
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
			return TEXT("string");
		}
		if (CastField<FNameProperty>(Property))
		{
			return TEXT("name");
		}
		if (CastField<FTextProperty>(Property))
		{
			return TEXT("text");
		}

		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
		{
			if (UEnum* Enum = EnumProp->GetEnum())
			{
				return FString::Printf(TEXT("enum:%s"), *Enum->GetName());
			}
		}

		if (FByteProperty* ByteProp = CastField<FByteProperty>(Property))
		{
			if (UEnum* Enum = ByteProp->Enum)
			{
				return FString::Printf(TEXT("enum:%s"), *Enum->GetName());
			}
			return TEXT("byte");
		}

		if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			return TEXT("array");
		}

		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			if (UScriptStruct* Struct = StructProp->Struct)
			{
				return FString::Printf(TEXT("struct:%s"), *Struct->GetName());
			}
			return TEXT("struct");
		}

		if (CastField<FObjectProperty>(Property) || CastField<FSoftObjectProperty>(Property))
		{
			return TEXT("object");
		}

		if (CastField<FClassProperty>(Property) || CastField<FSoftClassProperty>(Property))
		{
			return TEXT("class");
		}

		return TEXT("unknown");
	}

	FString PropertyValueToString(FProperty* Property, const void* Container)
	{
		if (!Property || !Container)
		{
			return TEXT("");
		}

		FString Value;
		Property->ExportTextItem_Direct(Value, Property->ContainerPtrToValuePtr<void>(Container), nullptr, nullptr, PPF_None);
		return Value;
	}

	bool StringToPropertyValue(FProperty* Property, void* Container, const FString& Value, FString& OutError)
	{
		if (!Property || !Container)
		{
			OutError = TEXT("Invalid property or container");
			return false;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

		const TCHAR* ImportResult = Property->ImportText_Direct(*Value, ValuePtr, nullptr, PPF_None);
		if (ImportResult == nullptr)
		{
			OutError = FString::Printf(TEXT("Failed to parse value '%s' for property type %s"), *Value, *GetPropertyTypeString(Property));
			return false;
		}

		return true;
	}
}

// =================================================================
// Private Helper Methods
// =================================================================

FString UProjectSettingsService::GetConfigFilePath(const FString& ConfigFile)
{
	return ::GetConfigFilePath(ConfigFile);
}

UObject* UProjectSettingsService::GetSettingsObjectForCategory(const FString& CategoryId)
{
	const FCategoryMapping* Mapping = FindPredefinedCategory(CategoryId);
	if (!Mapping || Mapping->SettingsClassName.IsEmpty())
	{
		return nullptr;
	}

	// Try to find and get default object for the settings class
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class && Class->GetName().Equals(Mapping->SettingsClassName, ESearchCase::IgnoreCase))
		{
			return Class->GetDefaultObject();
		}
	}

	return nullptr;
}

FString UProjectSettingsService::GetConfigSectionForCategory(const FString& CategoryId)
{
	const FCategoryMapping* Mapping = FindPredefinedCategory(CategoryId);
	return Mapping ? Mapping->ConfigSection : FString();
}

FString UProjectSettingsService::GetConfigFileForCategory(const FString& CategoryId)
{
	const FCategoryMapping* Mapping = FindPredefinedCategory(CategoryId);
	return Mapping ? Mapping->ConfigFile : FString();
}

FString UProjectSettingsService::PropertyToString(FProperty* Property, const void* Container)
{
	return PropertyValueToString(Property, Container);
}

bool UProjectSettingsService::StringToProperty(FProperty* Property, void* Container, const FString& Value, FString& OutError)
{
	return StringToPropertyValue(Property, Container, Value, OutError);
}

FString UProjectSettingsService::GetPropertyType(FProperty* Property)
{
	return GetPropertyTypeString(Property);
}

bool UProjectSettingsService::ValidateCategoryId(const FString& CategoryId, FString& OutError)
{
	if (CategoryId.IsEmpty())
	{
		OutError = TEXT("Category ID cannot be empty");
		return false;
	}

	// Check predefined categories
	if (FindPredefinedCategory(CategoryId))
	{
		return true;
	}

	// Check if it's a dynamically discovered settings class
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class && Class->IsChildOf(UDeveloperSettings::StaticClass()))
		{
			if (Class->GetName().Equals(CategoryId, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
	}

	OutError = FString::Printf(TEXT("Unknown category: %s"), *CategoryId);
	return false;
}

// =================================================================
// Category Operations
// =================================================================

TArray<FProjectSettingCategory> UProjectSettingsService::ListCategories()
{
	TArray<FProjectSettingCategory> Categories;

	// Add predefined categories
	for (const FCategoryMapping& Mapping : PredefinedCategories)
	{
		FProjectSettingCategory Category;
		Category.CategoryId = Mapping.CategoryId;
		Category.DisplayName = Mapping.DisplayName;
		Category.Description = Mapping.Description;
		Category.SettingsClassName = Mapping.SettingsClassName;
		Category.ConfigFile = Mapping.ConfigFile;

		// Count settings for this category
		if (!Mapping.SettingsClassName.IsEmpty())
		{
			UObject* SettingsObj = GetSettingsObjectForCategory(Mapping.CategoryId);
			if (SettingsObj)
			{
				int32 Count = 0;
				for (TFieldIterator<FProperty> It(SettingsObj->GetClass()); It; ++It)
				{
					if (ShouldExposeProperty(*It))
					{
						Count++;
					}
				}
				Category.SettingCount = Count;
			}
		}

		Categories.Add(Category);
	}

	// Discover UDeveloperSettings subclasses
	TSet<FString> AddedClasses;
	for (const FCategoryMapping& Mapping : PredefinedCategories)
	{
		AddedClasses.Add(Mapping.SettingsClassName);
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class || !Class->IsChildOf(UDeveloperSettings::StaticClass()))
		{
			continue;
		}

		if (Class == UDeveloperSettings::StaticClass())
		{
			continue;
		}

		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}

		FString ClassName = Class->GetName();
		if (AddedClasses.Contains(ClassName))
		{
			continue;
		}

		AddedClasses.Add(ClassName);

		FProjectSettingCategory Category;
		Category.CategoryId = ClassName;
		Category.DisplayName = ClassName;
		Category.SettingsClassName = Class->GetPathName();

		// Get description from class metadata if available
		if (Class->HasMetaData(TEXT("DisplayName")))
		{
			Category.DisplayName = Class->GetMetaData(TEXT("DisplayName"));
		}
		if (Class->HasMetaData(TEXT("ToolTip")))
		{
			Category.Description = Class->GetMetaData(TEXT("ToolTip"));
		}

		// Count configurable properties
		UObject* CDO = Class->GetDefaultObject();
		if (CDO)
		{
			int32 Count = 0;
			for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
			{
				if (ShouldExposeProperty(*PropIt))
				{
					Count++;
				}
			}
			Category.SettingCount = Count;
		}

		Categories.Add(Category);
	}

	UE_LOG(LogProjectSettingsService, Log, TEXT("Listed %d categories"), Categories.Num());
	return Categories;
}

// =================================================================
// Settings Discovery
// =================================================================

TArray<FSettingsClassInfo> UProjectSettingsService::DiscoverSettingsClasses()
{
	TArray<FSettingsClassInfo> Classes;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class)
		{
			continue;
		}

		// Check for UDeveloperSettings or common settings base classes
		bool bIsSettingsClass = Class->IsChildOf(UDeveloperSettings::StaticClass());

		// Also check for classes ending in "Settings" that have config properties
		if (!bIsSettingsClass && Class->GetName().EndsWith(TEXT("Settings")))
		{
			for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
			{
				if ((*PropIt)->HasAnyPropertyFlags(CPF_Config | CPF_GlobalConfig))
				{
					bIsSettingsClass = true;
					break;
				}
			}
		}

		if (!bIsSettingsClass)
		{
			continue;
		}

		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}

		FSettingsClassInfo Info;
		Info.ClassName = Class->GetName();
		Info.ClassPath = Class->GetPathName();
		Info.bIsDeveloperSettings = Class->IsChildOf(UDeveloperSettings::StaticClass());

		// Get config file info if available
		if (UObject* CDO = Class->GetDefaultObject())
		{
			// Try to determine config file from class
			FString ConfigName = Class->ClassConfigName != NAME_None ? Class->ClassConfigName.ToString() : TEXT("");
			if (!ConfigName.IsEmpty())
			{
				Info.ConfigFile = ConfigName + TEXT(".ini");
			}
		}

		// Count configurable properties
		int32 Count = 0;
		for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
		{
			if (ShouldExposeProperty(*PropIt))
			{
				Count++;
			}
		}
		Info.PropertyCount = Count;

		// Build config section from class path
		Info.ConfigSection = FString::Printf(TEXT("/Script/%s.%s"), *Class->GetOutermost()->GetName(), *Class->GetName());

		Classes.Add(Info);
	}

	// Sort by class name
	Classes.Sort([](const FSettingsClassInfo& A, const FSettingsClassInfo& B) {
		return A.ClassName < B.ClassName;
	});

	UE_LOG(LogProjectSettingsService, Log, TEXT("Discovered %d settings classes"), Classes.Num());
	return Classes;
}

TArray<FProjectSettingInfo> UProjectSettingsService::ListSettings(const FString& CategoryId)
{
	TArray<FProjectSettingInfo> Settings;

	if (CategoryId.Equals(TEXT("custom"), ESearchCase::IgnoreCase))
	{
		// Custom category doesn't list settings - use direct INI access
		UE_LOG(LogProjectSettingsService, Log, TEXT("Custom category - use GetIniValue/SetIniValue for direct access"));
		return Settings;
	}

	// Try predefined category first
	UObject* SettingsObj = GetSettingsObjectForCategory(CategoryId);

	// If not found, try as a class name
	if (!SettingsObj)
	{
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (Class && Class->GetName().Equals(CategoryId, ESearchCase::IgnoreCase))
			{
				SettingsObj = Class->GetDefaultObject();
				break;
			}
		}
	}

	if (!SettingsObj)
	{
		UE_LOG(LogProjectSettingsService, Warning, TEXT("Settings object not found for category: %s"), *CategoryId);
		return Settings;
	}

	UClass* SettingsClass = SettingsObj->GetClass();
	FString ConfigSection = GetConfigSectionForCategory(CategoryId);
	FString ConfigFile = GetConfigFileForCategory(CategoryId);

	// If not a predefined category, derive config info from class
	if (ConfigSection.IsEmpty())
	{
		ConfigSection = FString::Printf(TEXT("/Script/%s.%s"), *SettingsClass->GetOutermost()->GetName(), *SettingsClass->GetName());
	}

	for (TFieldIterator<FProperty> It(SettingsClass); It; ++It)
	{
		FProperty* Property = *It;
		if (!ShouldExposeProperty(Property))
		{
			continue;
		}

		FProjectSettingInfo Info;
		Info.Key = Property->GetName();
		Info.DisplayName = Property->GetName();
		Info.Type = GetPropertyTypeString(Property);
		Info.Value = PropertyValueToString(Property, SettingsObj);
		Info.ConfigSection = ConfigSection;
		Info.ConfigFile = ConfigFile;

		// Get metadata
		if (Property->HasMetaData(TEXT("DisplayName")))
		{
			Info.DisplayName = Property->GetMetaData(TEXT("DisplayName"));
		}
		if (Property->HasMetaData(TEXT("ToolTip")))
		{
			Info.Description = Property->GetMetaData(TEXT("ToolTip"));
		}

		// Check if read-only
		Info.bReadOnly = Property->HasAnyPropertyFlags(CPF_EditConst);

		// Some settings require restart
		if (Property->HasMetaData(TEXT("ConfigRestartRequired")))
		{
			Info.bRequiresRestart = true;
		}

		Settings.Add(Info);
	}

	UE_LOG(LogProjectSettingsService, Log, TEXT("Listed %d settings for category: %s"), Settings.Num(), *CategoryId);
	return Settings;
}

bool UProjectSettingsService::GetSettingInfo(const FString& CategoryId, const FString& Key, FProjectSettingInfo& OutInfo)
{
	TArray<FProjectSettingInfo> Settings = ListSettings(CategoryId);

	for (const FProjectSettingInfo& Setting : Settings)
	{
		if (Setting.Key.Equals(Key, ESearchCase::IgnoreCase))
		{
			OutInfo = Setting;
			return true;
		}
	}

	return false;
}

// =================================================================
// Get/Set Individual Settings
// =================================================================

FString UProjectSettingsService::GetSetting(const FString& CategoryId, const FString& Key)
{
	if (CategoryId.Equals(TEXT("custom"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogProjectSettingsService, Warning, TEXT("Use GetIniValue for custom category"));
		return FString();
	}

	UObject* SettingsObj = GetSettingsObjectForCategory(CategoryId);
	if (!SettingsObj)
	{
		// Try as class name
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (Class && Class->GetName().Equals(CategoryId, ESearchCase::IgnoreCase))
			{
				SettingsObj = Class->GetDefaultObject();
				break;
			}
		}
	}

	if (!SettingsObj)
	{
		UE_LOG(LogProjectSettingsService, Warning, TEXT("Settings object not found for category: %s"), *CategoryId);
		return FString();
	}

	FProperty* Property = SettingsObj->GetClass()->FindPropertyByName(FName(*Key));
	if (!Property)
	{
		UE_LOG(LogProjectSettingsService, Warning, TEXT("Property not found: %s.%s"), *CategoryId, *Key);
		return FString();
	}

	return PropertyValueToString(Property, SettingsObj);
}

FProjectSettingResult UProjectSettingsService::SetSetting(const FString& CategoryId, const FString& Key, const FString& Value)
{
	FProjectSettingResult Result;

	if (CategoryId.Equals(TEXT("custom"), ESearchCase::IgnoreCase))
	{
		Result.ErrorMessage = TEXT("Use SetIniValue for custom category");
		return Result;
	}

	UObject* SettingsObj = GetSettingsObjectForCategory(CategoryId);
	if (!SettingsObj)
	{
		// Try as class name
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (Class && Class->GetName().Equals(CategoryId, ESearchCase::IgnoreCase))
			{
				SettingsObj = Class->GetDefaultObject();
				break;
			}
		}
	}

	if (!SettingsObj)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Settings object not found for category: %s"), *CategoryId);
		return Result;
	}

	FProperty* Property = SettingsObj->GetClass()->FindPropertyByName(FName(*Key));
	if (!Property)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Property not found: %s.%s"), *CategoryId, *Key);
		return Result;
	}

	if (Property->HasAnyPropertyFlags(CPF_EditConst))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Property is read-only: %s.%s"), *CategoryId, *Key);
		return Result;
	}

#if WITH_EDITOR
	// Notify before change - this is what the editor does
	SettingsObj->PreEditChange(Property);
#endif

	FString Error;
	if (!StringToPropertyValue(Property, SettingsObj, Value, Error))
	{
		Result.ErrorMessage = Error;
		return Result;
	}

#if WITH_EDITOR
	// Notify after change - this triggers PostEditChangeProperty which:
	// 1. Applies any runtime effects (like ApplicationScale -> FSlateApplication::SetApplicationScale)
	// 2. Broadcasts change events so listeners can react
	// 3. Calls SaveConfig() to persist the change
	// This is exactly what the editor's property panel does.
	FPropertyChangedEvent PropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
	SettingsObj->PostEditChangeProperty(PropertyChangedEvent);
#else
	// In non-editor builds, fall back to manual save
	SettingsObj->SaveConfig();
#endif

	Result.bSuccess = true;
	Result.ModifiedSettings.Add(FString::Printf(TEXT("%s.%s"), *CategoryId, *Key));

	UE_LOG(LogProjectSettingsService, Log, TEXT("Set setting: %s.%s = %s"), *CategoryId, *Key, *Value);
	return Result;
}

// =================================================================
// Batch Operations
// =================================================================

FString UProjectSettingsService::GetCategorySettingsAsJson(const FString& CategoryId)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();

	TArray<FProjectSettingInfo> Settings = ListSettings(CategoryId);
	for (const FProjectSettingInfo& Setting : Settings)
	{
		JsonObj->SetStringField(Setting.Key, Setting.Value);
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);

	return JsonString;
}

FProjectSettingResult UProjectSettingsService::SetCategorySettingsFromJson(const FString& CategoryId, const FString& SettingsJson)
{
	FProjectSettingResult Result;

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SettingsJson);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		Result.ErrorMessage = TEXT("Failed to parse JSON");
		return Result;
	}

	for (const auto& Pair : JsonObj->Values)
	{
		FString Value;
		if (Pair.Value->TryGetString(Value))
		{
			FProjectSettingResult SingleResult = SetSetting(CategoryId, Pair.Key, Value);
			if (SingleResult.bSuccess)
			{
				Result.ModifiedSettings.Add(Pair.Key);
			}
			else
			{
				Result.FailedSettings.Add(FString::Printf(TEXT("%s: %s"), *Pair.Key, *SingleResult.ErrorMessage));
			}
		}
	}

	Result.bSuccess = Result.FailedSettings.Num() == 0;
	if (!Result.bSuccess && Result.ModifiedSettings.Num() > 0)
	{
		Result.ErrorMessage = TEXT("Some settings failed to update");
	}

	return Result;
}

// =================================================================
// Direct INI Access
// =================================================================

TArray<FString> UProjectSettingsService::ListIniSections(const FString& ConfigFile)
{
	TArray<FString> Sections;

	FString ConfigPath = ::GetConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		return Sections;
	}

	// Read the INI file directly to extract sections
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *ConfigPath))
	{
		UE_LOG(LogProjectSettingsService, Warning, TEXT("Failed to read config file: %s"), *ConfigPath);
		return Sections;
	}

	TArray<FString> Lines;
	FileContent.ParseIntoArrayLines(Lines);

	for (const FString& Line : Lines)
	{
		FString TrimmedLine = Line.TrimStartAndEnd();
		if (TrimmedLine.StartsWith(TEXT("[")) && TrimmedLine.EndsWith(TEXT("]")))
		{
			FString Section = TrimmedLine.Mid(1, TrimmedLine.Len() - 2);
			Sections.AddUnique(Section);
		}
	}

	return Sections;
}

TArray<FString> UProjectSettingsService::ListIniKeys(const FString& Section, const FString& ConfigFile)
{
	TArray<FString> Keys;

	FString ConfigPath = ::GetConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		return Keys;
	}

	TArray<FString> KeyValuePairs;
	if (GConfig->GetSection(*Section, KeyValuePairs, ConfigPath))
	{
		for (const FString& Pair : KeyValuePairs)
		{
			int32 EqualsIndex;
			if (Pair.FindChar(TEXT('='), EqualsIndex))
			{
				FString Key = Pair.Left(EqualsIndex);
				// Handle array syntax (+Key=Value)
				if (Key.StartsWith(TEXT("+")))
				{
					Key = Key.RightChop(1);
				}
				Keys.AddUnique(Key);
			}
		}
	}

	return Keys;
}

FString UProjectSettingsService::GetIniValue(const FString& Section, const FString& Key, const FString& ConfigFile)
{
	FString ConfigPath = ::GetConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		return FString();
	}

	FString Value;
	if (GConfig->GetString(*Section, *Key, Value, ConfigPath))
	{
		return Value;
	}

	return FString();
}

FProjectSettingResult UProjectSettingsService::SetIniValue(const FString& Section, const FString& Key, const FString& Value, const FString& ConfigFile)
{
	FProjectSettingResult Result;

	FString ConfigPath = ::GetConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		Result.ErrorMessage = FString::Printf(TEXT("Invalid config file: %s"), *ConfigFile);
		return Result;
	}

	GConfig->SetString(*Section, *Key, *Value, ConfigPath);
	GConfig->Flush(false, ConfigPath);

	Result.bSuccess = true;
	Result.ModifiedSettings.Add(FString::Printf(TEXT("[%s] %s"), *Section, *Key));

	UE_LOG(LogProjectSettingsService, Log, TEXT("Set INI value: [%s] %s = %s in %s"), *Section, *Key, *Value, *ConfigFile);
	return Result;
}

TArray<FString> UProjectSettingsService::GetIniArray(const FString& Section, const FString& Key, const FString& ConfigFile)
{
	TArray<FString> Values;

	FString ConfigPath = ::GetConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		return Values;
	}

	GConfig->GetArray(*Section, *Key, Values, ConfigPath);
	return Values;
}

FProjectSettingResult UProjectSettingsService::SetIniArray(const FString& Section, const FString& Key, const TArray<FString>& Values, const FString& ConfigFile)
{
	FProjectSettingResult Result;

	FString ConfigPath = ::GetConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		Result.ErrorMessage = FString::Printf(TEXT("Invalid config file: %s"), *ConfigFile);
		return Result;
	}

	GConfig->SetArray(*Section, *Key, Values, ConfigPath);
	GConfig->Flush(false, ConfigPath);

	Result.bSuccess = true;
	Result.ModifiedSettings.Add(FString::Printf(TEXT("[%s] %s (%d values)"), *Section, *Key, Values.Num()));

	UE_LOG(LogProjectSettingsService, Log, TEXT("Set INI array: [%s] %s with %d values in %s"), *Section, *Key, Values.Num(), *ConfigFile);
	return Result;
}

// =================================================================
// Persistence
// =================================================================

bool UProjectSettingsService::SaveAllConfig()
{
	GConfig->Flush(false);
	UE_LOG(LogProjectSettingsService, Log, TEXT("Saved all config files"));
	return true;
}

bool UProjectSettingsService::SaveConfig(const FString& ConfigFile)
{
	FString ConfigPath = ::GetConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		UE_LOG(LogProjectSettingsService, Warning, TEXT("Invalid config file: %s"), *ConfigFile);
		return false;
	}

	GConfig->Flush(false, ConfigPath);
	UE_LOG(LogProjectSettingsService, Log, TEXT("Saved config file: %s"), *ConfigFile);
	return true;
}
