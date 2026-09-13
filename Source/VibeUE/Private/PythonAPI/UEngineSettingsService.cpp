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
#include "Settings/LevelEditorPlaySettings.h"
#include "HAL/IConsoleManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogEngineSettingsService, Log, All);

// =================================================================
// File-local helpers
// =================================================================

namespace
{
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

	// EPlayNetMode -> stable Python-facing string.
	FString PlayNetModeToString(EPlayNetMode Mode)
	{
		switch (Mode)
		{
		case PIE_Standalone:   return TEXT("Standalone");
		case PIE_ListenServer: return TEXT("ListenServer");
		case PIE_Client:       return TEXT("Client");
		default:               return TEXT("Standalone");
		}
	}

	// Parse a Python-facing net-mode string (case-insensitive). "DedicatedServer" is accepted as an
	// alias for "Client" because dedicated-server PIE IS PIE_Client on 5.8. Returns false on unknown.
	bool StringToPlayNetMode(const FString& In, EPlayNetMode& OutMode)
	{
		const FString S = In.TrimStartAndEnd();
		if (S.Equals(TEXT("Standalone"), ESearchCase::IgnoreCase))   { OutMode = PIE_Standalone;   return true; }
		if (S.Equals(TEXT("ListenServer"), ESearchCase::IgnoreCase)) { OutMode = PIE_ListenServer; return true; }
		if (S.Equals(TEXT("Client"), ESearchCase::IgnoreCase) ||
			S.Equals(TEXT("DedicatedServer"), ESearchCase::IgnoreCase)) { OutMode = PIE_Client; return true; }
		return false;
	}

	// TEnumAsByte<EPlayModeType> -> string via reflection (cheap, no hand-maintained switch).
	FString PlayModeTypeToString(EPlayModeType Mode)
	{
		if (const UEnum* EnumPtr = StaticEnum<EPlayModeType>())
		{
			const FString Name = EnumPtr->GetNameStringByValue(static_cast<int64>(Mode));
			if (!Name.IsEmpty())
			{
				return Name;
			}
		}
		return FString();
	}
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
			FEngineSettingResult SingleResult = SetConsoleVariable(*Pair.Key, Value);
			if (SingleResult.bSuccess)
			{
				Result.ModifiedSettings.Add(*Pair.Key);
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
// Play-In-Editor (PIE) net-mode settings
// =================================================================

FPIESettingsInfo UEngineSettingsService::GetPIESettings()
{
	FPIESettingsInfo Info;

	const ULevelEditorPlaySettings* Settings = GetDefault<ULevelEditorPlaySettings>();
	if (!Settings)
	{
		Info.ErrorMessage = TEXT("ULevelEditorPlaySettings is unavailable");
		UE_LOG(LogEngineSettingsService, Warning, TEXT("GetPIESettings: %s"), *Info.ErrorMessage);
		return Info;
	}

	EPlayNetMode NetMode = PIE_Standalone;
	Settings->GetPlayNetMode(NetMode);
	Info.NetMode = PlayNetModeToString(NetMode);

	int32 NumClients = 0;
	Settings->GetPlayNumberOfClients(NumClients);
	Info.NumClients = NumClients;

	bool bRunUnderOneProcess = false;
	Settings->GetRunUnderOneProcess(bRunUnderOneProcess);
	Info.bRunUnderOneProcess = bRunUnderOneProcess;

	Info.bLaunchSeparateServer = Settings->bLaunchSeparateServer;
	Info.LastPlayMode = PlayModeTypeToString(Settings->LastExecutedPlayModeType.GetValue());

	Info.bSuccess = true;
	return Info;
}

bool UEngineSettingsService::SetPIESettings(const FString& NetMode, int32 NumClients, bool bRunUnderOneProcess)
{
	EPlayNetMode TargetMode = PIE_Standalone;
	if (!StringToPlayNetMode(NetMode, TargetMode))
	{
		UE_LOG(LogEngineSettingsService, Warning,
			TEXT("SetPIESettings: unknown net mode '%s'. Valid: Standalone, ListenServer, Client (alias DedicatedServer)."),
			*NetMode);
		return false;
	}

	// Header ClampMin=1 / ClampMax=10. Refuse out of range so a mismatch on readback can only mean
	// the write itself failed, not a silent clamp.
	if (NumClients < 1 || NumClients > 10)
	{
		UE_LOG(LogEngineSettingsService, Warning,
			TEXT("SetPIESettings: NumClients %d out of range; must be 1-10."), NumClients);
		return false;
	}

	ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
	if (!Settings)
	{
		UE_LOG(LogEngineSettingsService, Warning, TEXT("SetPIESettings: ULevelEditorPlaySettings is unavailable"));
		return false;
	}

	Settings->SetPlayNetMode(TargetMode);
	Settings->SetPlayNumberOfClients(NumClients);   // clamps PrimaryPIEClientIndex etc. internally
	Settings->SetRunUnderOneProcess(bRunUnderOneProcess);

	// Persist to EditorPerProjectUserSettings.ini (the class's config file) so the values survive a
	// restart — this is the file the manual quit/edit/relaunch workaround used to touch by hand.
	Settings->SaveConfig();

	// Verify by readback, never by return value: the setters are void, and only a round-trip proves
	// the config actually holds what we asked for.
	EPlayNetMode ReadMode = PIE_Standalone;
	Settings->GetPlayNetMode(ReadMode);
	int32 ReadNumClients = 0;
	Settings->GetPlayNumberOfClients(ReadNumClients);
	bool bReadRunUnderOneProcess = false;
	Settings->GetRunUnderOneProcess(bReadRunUnderOneProcess);

	if (ReadMode != TargetMode || ReadNumClients != NumClients || bReadRunUnderOneProcess != bRunUnderOneProcess)
	{
		UE_LOG(LogEngineSettingsService, Warning,
			TEXT("SetPIESettings: readback mismatch. Requested [NetMode=%s, NumClients=%d, RunUnderOneProcess=%s] ")
			TEXT("but read [NetMode=%s, NumClients=%d, RunUnderOneProcess=%s]."),
			*PlayNetModeToString(TargetMode), NumClients, bRunUnderOneProcess ? TEXT("true") : TEXT("false"),
			*PlayNetModeToString(ReadMode), ReadNumClients, bReadRunUnderOneProcess ? TEXT("true") : TEXT("false"));
		return false;
	}

	UE_LOG(LogEngineSettingsService, Log,
		TEXT("SetPIESettings: NetMode=%s, NumClients=%d, RunUnderOneProcess=%s (saved to EditorPerProjectUserSettings.ini)"),
		*PlayNetModeToString(TargetMode), NumClients, bRunUnderOneProcess ? TEXT("true") : TEXT("false"));
	return true;
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
