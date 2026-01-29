// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UEngineSettingsService.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UObject/UObjectIterator.h"
#include "Engine/RendererSettings.h"
#include "Engine/Engine.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "AudioDeviceManager.h"
#include "Sound/AudioSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "Scalability.h"
#include "HAL/IConsoleManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogEngineSettingsService, Log, All);

// =================================================================
// Category Mapping System
// =================================================================

namespace
{
	struct FEngineCategoryMapping
	{
		FString CategoryId;
		FString DisplayName;
		FString Description;
		FString SettingsClassName;
		FString ConfigSection;
		FString ConfigFile;
	};

	// Predefined engine category mappings
	static const TArray<FEngineCategoryMapping> EngineCategories = {
		{
			TEXT("rendering"),
			TEXT("Rendering Settings"),
			TEXT("Graphics, shaders, ray tracing, reflections, and visual quality settings"),
			TEXT("RendererSettings"),
			TEXT("/Script/Engine.RendererSettings"),
			TEXT("DefaultEngine.ini")
		},
		{
			TEXT("physics"),
			TEXT("Physics Settings"),
			TEXT("Physics simulation, collision, and dynamics settings"),
			TEXT("PhysicsSettings"),
			TEXT("/Script/Engine.PhysicsSettings"),
			TEXT("DefaultEngine.ini")
		},
		{
			TEXT("audio"),
			TEXT("Audio Settings"),
			TEXT("Sound, audio device, spatialization, and mixing settings"),
			TEXT("AudioSettings"),
			TEXT("/Script/Engine.AudioSettings"),
			TEXT("DefaultEngine.ini")
		},
		{
			TEXT("engine"),
			TEXT("Core Engine Settings"),
			TEXT("Engine core configuration, near clip plane, tick rates"),
			TEXT(""),
			TEXT("/Script/Engine.Engine"),
			TEXT("DefaultEngine.ini")
		},
		{
			TEXT("gc"),
			TEXT("Garbage Collection"),
			TEXT("Memory management, garbage collection timing and behavior"),
			TEXT("GarbageCollectionSettings"),
			TEXT("/Script/Engine.GarbageCollectionSettings"),
			TEXT("DefaultEngine.ini")
		},
		{
			TEXT("streaming"),
			TEXT("Streaming Settings"),
			TEXT("Level streaming, texture streaming, and asset loading settings"),
			TEXT("StreamingSettings"),
			TEXT("/Script/Engine.StreamingSettings"),
			TEXT("DefaultEngine.ini")
		},
		{
			TEXT("network"),
			TEXT("Network Settings"),
			TEXT("Networking, replication, and multiplayer settings"),
			TEXT(""),
			TEXT("/Script/Engine.NetworkSettings"),
			TEXT("DefaultEngine.ini")
		},
		{
			TEXT("collision"),
			TEXT("Collision Profiles"),
			TEXT("Collision channels, profiles, and object type definitions"),
			TEXT(""),
			TEXT("/Script/Engine.CollisionProfile"),
			TEXT("DefaultEngine.ini")
		},
		{
			TEXT("platform_windows"),
			TEXT("Windows Platform"),
			TEXT("Windows-specific settings, graphics API, shaders"),
			TEXT("WindowsTargetSettings"),
			TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"),
			TEXT("DefaultEngine.ini")
		},
		{
			TEXT("hardware"),
			TEXT("Hardware Targeting"),
			TEXT("Target hardware class and graphics performance tier"),
			TEXT("HardwareTargetingSettings"),
			TEXT("/Script/HardwareTargeting.HardwareTargetingSettings"),
			TEXT("DefaultEngine.ini")
		},
		{
			TEXT("ai"),
			TEXT("AI System"),
			TEXT("AI module settings, behavior trees, navigation"),
			TEXT("AISystem"),
			TEXT("/Script/AIModule.AISystem"),
			TEXT("DefaultEngine.ini")
		},
		{
			TEXT("input"),
			TEXT("Input Settings"),
			TEXT("Input bindings, axis mappings, and input configuration"),
			TEXT("InputSettings"),
			TEXT("/Script/Engine.InputSettings"),
			TEXT("DefaultInput.ini")
		},
		{
			TEXT("cvar"),
			TEXT("Console Variables"),
			TEXT("Direct access to console variables (cvars) - use get/set_console_variable methods"),
			TEXT(""),
			TEXT(""),
			TEXT("")
		}
	};

	const FEngineCategoryMapping* FindEngineCategory(const FString& CategoryId)
	{
		for (const FEngineCategoryMapping& Mapping : EngineCategories)
		{
			if (Mapping.CategoryId.Equals(CategoryId, ESearchCase::IgnoreCase))
			{
				return &Mapping;
			}
		}
		return nullptr;
	}

	FString GetEngineConfigFilePath(const FString& ConfigFile)
	{
		if (ConfigFile.IsEmpty())
		{
			return FString();
		}

		// Check if already an absolute path
		if (!FPaths::IsRelative(ConfigFile))
		{
			return ConfigFile;
		}

		// Handle "DefaultXXX.ini" format - project config
		if (ConfigFile.StartsWith(TEXT("Default")))
		{
			FString ProjectConfigDir = FPaths::ProjectConfigDir();
			return ProjectConfigDir / ConfigFile;
		}

		// Handle base engine config names
		FString ProjectConfigDir = FPaths::ProjectConfigDir();
		FString FullPath = ProjectConfigDir / TEXT("Default") + ConfigFile;

		if (FPaths::FileExists(FullPath))
		{
			return FullPath;
		}

		// Try as-is
		return ProjectConfigDir / ConfigFile;
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

		if (CastField<FArrayProperty>(Property))
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

UObject* UEngineSettingsService::GetSettingsObjectForCategory(const FString& CategoryId)
{
	const FEngineCategoryMapping* Mapping = FindEngineCategory(CategoryId);
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

FString UEngineSettingsService::GetConfigSectionForCategory(const FString& CategoryId)
{
	const FEngineCategoryMapping* Mapping = FindEngineCategory(CategoryId);
	return Mapping ? Mapping->ConfigSection : FString();
}

FString UEngineSettingsService::GetConfigFileForCategory(const FString& CategoryId)
{
	const FEngineCategoryMapping* Mapping = FindEngineCategory(CategoryId);
	return Mapping ? Mapping->ConfigFile : FString();
}

FString UEngineSettingsService::PropertyToString(FProperty* Property, const void* Container)
{
	return PropertyValueToString(Property, Container);
}

bool UEngineSettingsService::StringToProperty(FProperty* Property, void* Container, const FString& Value, FString& OutError)
{
	return StringToPropertyValue(Property, Container, Value, OutError);
}

FString UEngineSettingsService::GetPropertyType(FProperty* Property)
{
	return GetPropertyTypeString(Property);
}

bool UEngineSettingsService::ValidateCategoryId(const FString& CategoryId, FString& OutError)
{
	if (CategoryId.IsEmpty())
	{
		OutError = TEXT("Category ID cannot be empty");
		return false;
	}

	if (FindEngineCategory(CategoryId))
	{
		return true;
	}

	OutError = FString::Printf(TEXT("Unknown engine category: %s"), *CategoryId);
	return false;
}

FString UEngineSettingsService::GetCVarFlagsString(IConsoleVariable* CVar)
{
	if (!CVar)
	{
		return TEXT("");
	}

	TArray<FString> Flags;

	EConsoleVariableFlags CVarFlags = CVar->GetFlags();

	if (EnumHasAnyFlags(CVarFlags, ECVF_RenderThreadSafe))
	{
		Flags.Add(TEXT("RenderThreadSafe"));
	}
	if (EnumHasAnyFlags(CVarFlags, ECVF_Scalability))
	{
		Flags.Add(TEXT("Scalability"));
	}
	if (EnumHasAnyFlags(CVarFlags, ECVF_ReadOnly))
	{
		Flags.Add(TEXT("ReadOnly"));
	}
	if (EnumHasAnyFlags(CVarFlags, ECVF_Cheat))
	{
		Flags.Add(TEXT("Cheat"));
	}

	return FString::Join(Flags, TEXT(", "));
}

FString UEngineSettingsService::GetCVarTypeString(IConsoleVariable* CVar)
{
	if (!CVar)
	{
		return TEXT("unknown");
	}

	// Try to determine type from the cvar
	if (CVar->IsVariableInt())
	{
		return TEXT("int");
	}
	if (CVar->IsVariableFloat())
	{
		return TEXT("float");
	}
	if (CVar->IsVariableString())
	{
		return TEXT("string");
	}
	if (CVar->IsVariableBool())
	{
		return TEXT("bool");
	}

	return TEXT("unknown");
}

// =================================================================
// Category Operations
// =================================================================

TArray<FEngineSettingCategory> UEngineSettingsService::ListCategories()
{
	TArray<FEngineSettingCategory> Categories;

	for (const FEngineCategoryMapping& Mapping : EngineCategories)
	{
		FEngineSettingCategory Category;
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
		else if (Mapping.CategoryId == TEXT("cvar"))
		{
			// Special case for cvars - don't count (too many)
			Category.SettingCount = -1; // Indicates "many"
		}

		Categories.Add(Category);
	}

	UE_LOG(LogEngineSettingsService, Log, TEXT("Listed %d engine categories"), Categories.Num());
	return Categories;
}

// =================================================================
// Settings Discovery
// =================================================================

TArray<FEngineSettingInfo> UEngineSettingsService::ListSettings(const FString& CategoryId)
{
	TArray<FEngineSettingInfo> Settings;

	if (CategoryId.Equals(TEXT("cvar"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogEngineSettingsService, Log, TEXT("CVar category - use SearchConsoleVariables or ListConsoleVariablesWithPrefix"));
		return Settings;
	}

	const FEngineCategoryMapping* Mapping = FindEngineCategory(CategoryId);
	if (!Mapping)
	{
		UE_LOG(LogEngineSettingsService, Warning, TEXT("Unknown engine category: %s"), *CategoryId);
		return Settings;
	}

	UObject* SettingsObj = GetSettingsObjectForCategory(CategoryId);

	if (!SettingsObj)
	{
		// For categories without a settings object, read from INI directly
		if (!Mapping->ConfigSection.IsEmpty() && !Mapping->ConfigFile.IsEmpty())
		{
			FString ConfigPath = GetEngineConfigFilePath(Mapping->ConfigFile);
			TArray<FString> KeyValuePairs;

			if (GConfig && GConfig->GetSection(*Mapping->ConfigSection, KeyValuePairs, ConfigPath))
			{
				for (const FString& Pair : KeyValuePairs)
				{
					int32 EqualsIndex;
					if (Pair.FindChar(TEXT('='), EqualsIndex))
					{
						FEngineSettingInfo Info;
						Info.Key = Pair.Left(EqualsIndex);
						Info.Value = Pair.RightChop(EqualsIndex + 1);
						Info.DisplayName = Info.Key;
						Info.ConfigSection = Mapping->ConfigSection;
						Info.ConfigFile = Mapping->ConfigFile;
						Info.Type = TEXT("string"); // INI values are strings

						// Handle array syntax
						if (Info.Key.StartsWith(TEXT("+")))
						{
							Info.Key = Info.Key.RightChop(1);
							Info.Type = TEXT("array_element");
						}

						Settings.Add(Info);
					}
				}
			}
		}

		UE_LOG(LogEngineSettingsService, Log, TEXT("Listed %d settings for category: %s (from INI)"), Settings.Num(), *CategoryId);
		return Settings;
	}

	UClass* SettingsClass = SettingsObj->GetClass();

	for (TFieldIterator<FProperty> It(SettingsClass); It; ++It)
	{
		FProperty* Property = *It;
		if (!ShouldExposeProperty(Property))
		{
			continue;
		}

		FEngineSettingInfo Info;
		Info.Key = Property->GetName();
		Info.DisplayName = Property->GetName();
		Info.Type = GetPropertyTypeString(Property);
		Info.Value = PropertyValueToString(Property, SettingsObj);
		Info.ConfigSection = Mapping->ConfigSection;
		Info.ConfigFile = Mapping->ConfigFile;

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

		// Check console variable association
		if (Property->HasMetaData(TEXT("ConsoleVariable")))
		{
			Info.bIsConsoleVariable = true;
		}

		// Some settings require restart
		if (Property->HasMetaData(TEXT("ConfigRestartRequired")))
		{
			Info.bRequiresRestart = true;
		}

		Settings.Add(Info);
	}

	UE_LOG(LogEngineSettingsService, Log, TEXT("Listed %d settings for category: %s"), Settings.Num(), *CategoryId);
	return Settings;
}

bool UEngineSettingsService::GetSettingInfo(const FString& CategoryId, const FString& Key, FEngineSettingInfo& OutInfo)
{
	TArray<FEngineSettingInfo> Settings = ListSettings(CategoryId);

	for (const FEngineSettingInfo& Setting : Settings)
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

FString UEngineSettingsService::GetSetting(const FString& CategoryId, const FString& Key)
{
	if (CategoryId.Equals(TEXT("cvar"), ESearchCase::IgnoreCase))
	{
		return GetConsoleVariable(Key);
	}

	const FEngineCategoryMapping* Mapping = FindEngineCategory(CategoryId);
	if (!Mapping)
	{
		UE_LOG(LogEngineSettingsService, Warning, TEXT("Unknown engine category: %s"), *CategoryId);
		return FString();
	}

	UObject* SettingsObj = GetSettingsObjectForCategory(CategoryId);
	
	if (SettingsObj)
	{
		FProperty* Property = SettingsObj->GetClass()->FindPropertyByName(FName(*Key));
		if (Property)
		{
			return PropertyValueToString(Property, SettingsObj);
		}
	}

	// Try INI lookup
	if (!Mapping->ConfigSection.IsEmpty() && !Mapping->ConfigFile.IsEmpty())
	{
		FString ConfigPath = GetEngineConfigFilePath(Mapping->ConfigFile);
		FString Value;
		if (GConfig && GConfig->GetString(*Mapping->ConfigSection, *Key, Value, ConfigPath))
		{
			return Value;
		}
	}

	UE_LOG(LogEngineSettingsService, Warning, TEXT("Setting not found: %s.%s"), *CategoryId, *Key);
	return FString();
}

FEngineSettingResult UEngineSettingsService::SetSetting(const FString& CategoryId, const FString& Key, const FString& Value)
{
	FEngineSettingResult Result;

	if (CategoryId.Equals(TEXT("cvar"), ESearchCase::IgnoreCase))
	{
		return SetConsoleVariable(Key, Value);
	}

	const FEngineCategoryMapping* Mapping = FindEngineCategory(CategoryId);
	if (!Mapping)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Unknown engine category: %s"), *CategoryId);
		return Result;
	}

	UObject* SettingsObj = GetSettingsObjectForCategory(CategoryId);

	if (SettingsObj)
	{
		FProperty* Property = SettingsObj->GetClass()->FindPropertyByName(FName(*Key));
		if (Property)
		{
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
			// 1. Applies any runtime effects specific to this setting
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

			UE_LOG(LogEngineSettingsService, Log, TEXT("Set engine setting: %s.%s = %s"), *CategoryId, *Key, *Value);
			return Result;
		}
	}

	// Try INI set
	if (!Mapping->ConfigSection.IsEmpty() && !Mapping->ConfigFile.IsEmpty())
	{
		FString ConfigPath = GetEngineConfigFilePath(Mapping->ConfigFile);
		GConfig->SetString(*Mapping->ConfigSection, *Key, *Value, ConfigPath);
		GConfig->Flush(false, ConfigPath);

		Result.bSuccess = true;
		Result.ModifiedSettings.Add(FString::Printf(TEXT("[%s] %s"), *Mapping->ConfigSection, *Key));

		UE_LOG(LogEngineSettingsService, Log, TEXT("Set engine INI: [%s] %s = %s"), *Mapping->ConfigSection, *Key, *Value);
		return Result;
	}

	Result.ErrorMessage = FString::Printf(TEXT("Could not set setting: %s.%s"), *CategoryId, *Key);
	return Result;
}

// =================================================================
// Console Variables (CVars)
// =================================================================

FString UEngineSettingsService::GetConsoleVariable(const FString& Name)
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!CVar)
	{
		UE_LOG(LogEngineSettingsService, Warning, TEXT("Console variable not found: %s"), *Name);
		return FString();
	}

	return CVar->GetString();
}

FEngineSettingResult UEngineSettingsService::SetConsoleVariable(const FString& Name, const FString& Value)
{
	FEngineSettingResult Result;

	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!CVar)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Console variable not found: %s"), *Name);
		return Result;
	}

	if (CVar->TestFlags(ECVF_ReadOnly))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Console variable is read-only: %s"), *Name);
		return Result;
	}

	CVar->Set(*Value, ECVF_SetByCode);

	// Persist the cvar to config file for it to survive restart
	FString ConfigPath = GetEngineConfigFilePath(TEXT("DefaultEngine.ini"));
	GConfig->SetString(TEXT("ConsoleVariables"), *Name, *Value, ConfigPath);
	GConfig->Flush(false, ConfigPath);

	Result.bSuccess = true;
	Result.ModifiedSettings.Add(Name);

	UE_LOG(LogEngineSettingsService, Log, TEXT("Set console variable: %s = %s (saved to config)"), *Name, *Value);
	return Result;
}

bool UEngineSettingsService::GetConsoleVariableInfo(const FString& Name, FConsoleVariableInfo& OutInfo)
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!CVar)
	{
		return false;
	}

	OutInfo.Name = Name;
	OutInfo.Value = CVar->GetString();
	OutInfo.Description = CVar->GetHelp();
	OutInfo.Type = GetCVarTypeString(CVar);
	OutInfo.Flags = GetCVarFlagsString(CVar);
	OutInfo.bIsReadOnly = CVar->TestFlags(ECVF_ReadOnly);

	// Try to get default value
	OutInfo.DefaultValue = TEXT(""); // CVars don't always expose default

	return true;
}

TArray<FConsoleVariableInfo> UEngineSettingsService::SearchConsoleVariables(const FString& SearchTerm, int32 MaxResults)
{
	TArray<FConsoleVariableInfo> Results;

	FString SearchLower = SearchTerm.ToLower();

	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(
		FConsoleObjectVisitor::CreateLambda([&](const TCHAR* Name, IConsoleObject* Obj)
		{
			if (MaxResults > 0 && Results.Num() >= MaxResults)
			{
				return;
			}

			IConsoleVariable* CVar = Obj->AsVariable();
			if (!CVar)
			{
				return;
			}

			FString NameStr(Name);
			FString HelpStr = CVar->GetHelp();

			// Search in name and description
			if (NameStr.ToLower().Contains(SearchLower) || HelpStr.ToLower().Contains(SearchLower))
			{
				FConsoleVariableInfo Info;
				Info.Name = NameStr;
				Info.Value = CVar->GetString();
				Info.Description = HelpStr;
				Info.Type = GetCVarTypeString(CVar);
				Info.Flags = GetCVarFlagsString(CVar);
				Info.bIsReadOnly = CVar->TestFlags(ECVF_ReadOnly);
				Results.Add(Info);
			}
		}),
		TEXT("") // Start from empty to iterate all
	);

	UE_LOG(LogEngineSettingsService, Log, TEXT("Found %d console variables matching '%s'"), Results.Num(), *SearchTerm);
	return Results;
}

TArray<FConsoleVariableInfo> UEngineSettingsService::ListConsoleVariablesWithPrefix(const FString& Prefix, int32 MaxResults)
{
	TArray<FConsoleVariableInfo> Results;

	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(
		FConsoleObjectVisitor::CreateLambda([&](const TCHAR* Name, IConsoleObject* Obj)
		{
			if (MaxResults > 0 && Results.Num() >= MaxResults)
			{
				return;
			}

			IConsoleVariable* CVar = Obj->AsVariable();
			if (!CVar)
			{
				return;
			}

			FConsoleVariableInfo Info;
			Info.Name = Name;
			Info.Value = CVar->GetString();
			Info.Description = CVar->GetHelp();
			Info.Type = GetCVarTypeString(CVar);
			Info.Flags = GetCVarFlagsString(CVar);
			Info.bIsReadOnly = CVar->TestFlags(ECVF_ReadOnly);
			Results.Add(Info);
		}),
		*Prefix
	);

	UE_LOG(LogEngineSettingsService, Log, TEXT("Found %d console variables with prefix '%s'"), Results.Num(), *Prefix);
	return Results;
}

// =================================================================
// Batch Operations
// =================================================================

FString UEngineSettingsService::GetCategorySettingsAsJson(const FString& CategoryId)
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();

	TArray<FEngineSettingInfo> Settings = ListSettings(CategoryId);
	for (const FEngineSettingInfo& Setting : Settings)
	{
		JsonObj->SetStringField(Setting.Key, Setting.Value);
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);

	return JsonString;
}

FEngineSettingResult UEngineSettingsService::SetCategorySettingsFromJson(const FString& CategoryId, const FString& SettingsJson)
{
	FEngineSettingResult Result;

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
			FEngineSettingResult SingleResult = SetSetting(CategoryId, Pair.Key, Value);
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

FEngineSettingResult UEngineSettingsService::SetConsoleVariablesFromJson(const FString& SettingsJson)
{
	FEngineSettingResult Result;

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
			FEngineSettingResult SingleResult = SetConsoleVariable(Pair.Key, Value);
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
		Result.ErrorMessage = TEXT("Some console variables failed to update");
	}

	return Result;
}

// =================================================================
// Direct Engine INI Access
// =================================================================

TArray<FString> UEngineSettingsService::ListEngineSections(const FString& ConfigFile, bool bIncludeBase)
{
	TArray<FString> Sections;

	FString ConfigPath = GetEngineConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		return Sections;
	}

	// Read the INI file directly to extract sections
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *ConfigPath))
	{
		UE_LOG(LogEngineSettingsService, Warning, TEXT("Failed to read config file: %s"), *ConfigPath);
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

FString UEngineSettingsService::GetEngineIniValue(const FString& Section, const FString& Key, const FString& ConfigFile)
{
	FString ConfigPath = GetEngineConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		return FString();
	}

	FString Value;
	if (GConfig && GConfig->GetString(*Section, *Key, Value, ConfigPath))
	{
		return Value;
	}

	return FString();
}

FEngineSettingResult UEngineSettingsService::SetEngineIniValue(const FString& Section, const FString& Key, const FString& Value, const FString& ConfigFile)
{
	FEngineSettingResult Result;

	FString ConfigPath = GetEngineConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		Result.ErrorMessage = FString::Printf(TEXT("Invalid config file: %s"), *ConfigFile);
		return Result;
	}

	GConfig->SetString(*Section, *Key, *Value, ConfigPath);
	GConfig->Flush(false, ConfigPath);

	Result.bSuccess = true;
	Result.ModifiedSettings.Add(FString::Printf(TEXT("[%s] %s"), *Section, *Key));

	UE_LOG(LogEngineSettingsService, Log, TEXT("Set engine INI value: [%s] %s = %s in %s"), *Section, *Key, *Value, *ConfigFile);
	return Result;
}

TArray<FString> UEngineSettingsService::GetEngineIniArray(const FString& Section, const FString& Key, const FString& ConfigFile)
{
	TArray<FString> Values;

	FString ConfigPath = GetEngineConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		return Values;
	}

	if (GConfig)
	{
		GConfig->GetArray(*Section, *Key, Values, ConfigPath);
	}

	return Values;
}

FEngineSettingResult UEngineSettingsService::SetEngineIniArray(const FString& Section, const FString& Key, const TArray<FString>& Values, const FString& ConfigFile)
{
	FEngineSettingResult Result;

	FString ConfigPath = GetEngineConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		Result.ErrorMessage = FString::Printf(TEXT("Invalid config file: %s"), *ConfigFile);
		return Result;
	}

	GConfig->SetArray(*Section, *Key, Values, ConfigPath);
	GConfig->Flush(false, ConfigPath);

	Result.bSuccess = true;
	Result.ModifiedSettings.Add(FString::Printf(TEXT("[%s] %s (%d values)"), *Section, *Key, Values.Num()));

	UE_LOG(LogEngineSettingsService, Log, TEXT("Set engine INI array: [%s] %s with %d values in %s"), *Section, *Key, Values.Num(), *ConfigFile);
	return Result;
}

// =================================================================
// Scalability Settings
// =================================================================

FString UEngineSettingsService::GetScalabilitySettings()
{
	TSharedPtr<FJsonObject> JsonObj = MakeShared<FJsonObject>();

	Scalability::FQualityLevels QualityLevels = Scalability::GetQualityLevels();

	JsonObj->SetNumberField(TEXT("ResolutionQuality"), QualityLevels.ResolutionQuality);
	JsonObj->SetNumberField(TEXT("ViewDistanceQuality"), QualityLevels.ViewDistanceQuality);
	JsonObj->SetNumberField(TEXT("AntiAliasingQuality"), QualityLevels.AntiAliasingQuality);
	JsonObj->SetNumberField(TEXT("ShadowQuality"), QualityLevels.ShadowQuality);
	JsonObj->SetNumberField(TEXT("GlobalIlluminationQuality"), QualityLevels.GlobalIlluminationQuality);
	JsonObj->SetNumberField(TEXT("ReflectionQuality"), QualityLevels.ReflectionQuality);
	JsonObj->SetNumberField(TEXT("PostProcessQuality"), QualityLevels.PostProcessQuality);
	JsonObj->SetNumberField(TEXT("TextureQuality"), QualityLevels.TextureQuality);
	JsonObj->SetNumberField(TEXT("EffectsQuality"), QualityLevels.EffectsQuality);
	JsonObj->SetNumberField(TEXT("FoliageQuality"), QualityLevels.FoliageQuality);
	JsonObj->SetNumberField(TEXT("ShadingQuality"), QualityLevels.ShadingQuality);

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);

	return JsonString;
}

FEngineSettingResult UEngineSettingsService::SetScalabilityLevel(const FString& GroupName, int32 QualityLevel)
{
	FEngineSettingResult Result;

	Scalability::FQualityLevels QualityLevels = Scalability::GetQualityLevels();

	bool bFound = true;
	if (GroupName.Equals(TEXT("ViewDistance"), ESearchCase::IgnoreCase))
	{
		QualityLevels.ViewDistanceQuality = QualityLevel;
	}
	else if (GroupName.Equals(TEXT("AntiAliasing"), ESearchCase::IgnoreCase))
	{
		QualityLevels.AntiAliasingQuality = QualityLevel;
	}
	else if (GroupName.Equals(TEXT("Shadow"), ESearchCase::IgnoreCase))
	{
		QualityLevels.ShadowQuality = QualityLevel;
	}
	else if (GroupName.Equals(TEXT("GlobalIllumination"), ESearchCase::IgnoreCase))
	{
		QualityLevels.GlobalIlluminationQuality = QualityLevel;
	}
	else if (GroupName.Equals(TEXT("Reflection"), ESearchCase::IgnoreCase))
	{
		QualityLevels.ReflectionQuality = QualityLevel;
	}
	else if (GroupName.Equals(TEXT("PostProcess"), ESearchCase::IgnoreCase))
	{
		QualityLevels.PostProcessQuality = QualityLevel;
	}
	else if (GroupName.Equals(TEXT("Texture"), ESearchCase::IgnoreCase))
	{
		QualityLevels.TextureQuality = QualityLevel;
	}
	else if (GroupName.Equals(TEXT("Effects"), ESearchCase::IgnoreCase))
	{
		QualityLevels.EffectsQuality = QualityLevel;
	}
	else if (GroupName.Equals(TEXT("Foliage"), ESearchCase::IgnoreCase))
	{
		QualityLevels.FoliageQuality = QualityLevel;
	}
	else if (GroupName.Equals(TEXT("Shading"), ESearchCase::IgnoreCase))
	{
		QualityLevels.ShadingQuality = QualityLevel;
	}
	else
	{
		bFound = false;
	}

	if (!bFound)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Unknown scalability group: %s"), *GroupName);
		return Result;
	}

	Scalability::SetQualityLevels(QualityLevels);

	// Save scalability settings to config for persistence
	Scalability::SaveState(GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	Result.bSuccess = true;
	Result.ModifiedSettings.Add(FString::Printf(TEXT("%s = %d"), *GroupName, QualityLevel));

	UE_LOG(LogEngineSettingsService, Log, TEXT("Set scalability: %s = %d (saved to config)"), *GroupName, QualityLevel);
	return Result;
}

FEngineSettingResult UEngineSettingsService::SetOverallScalabilityLevel(int32 QualityLevel)
{
	FEngineSettingResult Result;

	Scalability::FQualityLevels QualityLevels;
	QualityLevels.SetFromSingleQualityLevel(QualityLevel);
	Scalability::SetQualityLevels(QualityLevels);

	// Save scalability settings to config for persistence
	Scalability::SaveState(GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	Result.bSuccess = true;
	Result.ModifiedSettings.Add(FString::Printf(TEXT("OverallQuality = %d"), QualityLevel));

	UE_LOG(LogEngineSettingsService, Log, TEXT("Set overall scalability level: %d (saved to config)"), QualityLevel);
	return Result;
}

// =================================================================
// Persistence
// =================================================================

bool UEngineSettingsService::SaveAllEngineConfig()
{
	GConfig->Flush(false);
	UE_LOG(LogEngineSettingsService, Log, TEXT("Saved all engine config files"));
	return true;
}

bool UEngineSettingsService::SaveEngineConfig(const FString& ConfigFile)
{
	FString ConfigPath = GetEngineConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		UE_LOG(LogEngineSettingsService, Warning, TEXT("Invalid config file: %s"), *ConfigFile);
		return false;
	}

	GConfig->Flush(false, ConfigPath);
	UE_LOG(LogEngineSettingsService, Log, TEXT("Saved engine config file: %s"), *ConfigFile);
	return true;
}
