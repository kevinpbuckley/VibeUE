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






// =================================================================
// Batch Operations
// =================================================================


// =================================================================
// Direct Engine INI Access
// =================================================================






// =================================================================
// Scalability Settings
// =================================================================




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


