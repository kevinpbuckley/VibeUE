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
