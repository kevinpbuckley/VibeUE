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
// INI writes that reach the disk, and settings saved the way the Settings window saves them (see the header)
#include "HAL/FileManager.h"
#include "Misc/ConfigContext.h"
#include "Misc/OutputDeviceNull.h"
#include "Misc/StringOutputDevice.h"
#include "UObject/Package.h"   // Class->GetOutermost() below; unity builds hid the missing include
#include "UObject/UnrealType.h"

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

	// ---- Project config files on disk ---------------------------------------------------------------------------
	// GConfig saves only the files of its config branches (GEngineIni, GEditorIni, ...: the merged hierarchy). A
	// bare project path like ".../Config/DefaultEditor.ini" is not one of them: GConfig::Find loads it as a single
	// file marked NoSave ("should never be saved"), or not at all when the file does not exist, so SetString and
	// Flush on it never reach the disk. Project files are therefore read and written ON DISK here, with the
	// engine's own single-property writer.

	/** A config file argument resolved to its file on disk, plus the config branch it layers into ("Editor" for the
	 *  project's DefaultEditor.ini), empty when it is not a project Default*.ini */
	struct FResolvedIni
	{
		FString DiskPath;
		FString BranchBaseName;
	};

	FResolvedIni ResolveIni(const FString& ConfigFile)
	{
		FResolvedIni Resolved;
		const FString Path = GetConfigFilePath(ConfigFile);
		if (Path.IsEmpty())
		{
			return Resolved;
		}
		Resolved.DiskPath = FPaths::ConvertRelativePathToFull(Path);
		FPaths::NormalizeFilename(Resolved.DiskPath);

		FString Dir = FPaths::GetPath(Resolved.DiskPath);
		FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir());
		FPaths::NormalizeDirectoryName(Dir);
		FPaths::NormalizeDirectoryName(ProjectDir);
		const FString Clean = FPaths::GetCleanFilename(Resolved.DiskPath);
		if (FPaths::IsSamePath(Dir, ProjectDir) && Clean.StartsWith(TEXT("Default")) && Clean.EndsWith(TEXT(".ini")) && Clean.Len() > 11)
		{
			Resolved.BranchBaseName = Clean.Mid(7, Clean.Len() - 11);   // "DefaultEditor.ini" -> "Editor"
		}
		return Resolved;
	}

	/** The file exactly as it is on disk (one layer, no hierarchy); false when it does not exist */
	bool ReadDiskIni(const FString& DiskPath, FConfigFile& OutFile)
	{
		if (DiskPath.IsEmpty() || !IFileManager::Get().FileExists(*DiskPath))
		{
			return false;
		}
		OutFile.Read(DiskPath);
		return true;
	}

	/** Values of Key in one on-disk file section; array lines may be stored with their +/./- command prefix */
	TArray<FString> DiskValues(const FConfigFile& File, const FString& Section, const FString& Key)
	{
		TArray<FString> Values;
		if (const FConfigSection* Found = File.FindSection(*Section))
		{
			for (const TCHAR* Prefix : { TEXT(""), TEXT("+"), TEXT(".") })
			{
				TArray<FString> Part;
				Found->MultiFind(FName(*(FString(Prefix) + Key)), Part, /*bMaintainOrder*/ true);
				Values.Append(Part);
			}
		}
		return Values;
	}

	/** Makes GConfig's merged copy of a branch re-read its files (what UObject::UpdateSingleSectionOfConfigFile does
	 *  after writing a Default*.ini), flushing pending user-layer writes first so they are not lost */
	void ReloadBranch(const FString& BranchBaseName)
	{
		if (BranchBaseName.IsEmpty() || !GConfig || !GConfig->FindBranchWithNoReload(FName(*BranchBaseName), FString()))
		{
			return;
		}
		GConfig->Flush(false, GConfig->GetConfigFilename(*BranchBaseName));
		FConfigContext Context = FConfigContext::ForceReloadIntoGConfig();
		Context.bWriteDestIni = false;
		Context.Load(*BranchBaseName);
	}

	/** A settings class by path ("/Script/UnrealEd.EditorProjectAppearanceSettings") or name (a leading U is accepted) */
	UClass* FindSettingsClass(const FString& InName)
	{
		const FString Name = InName.TrimStartAndEnd();
		if (Name.IsEmpty())
		{
			return nullptr;
		}
		if (Name.StartsWith(TEXT("/")))
		{
			return FindObject<UClass>(nullptr, *Name);
		}
		if (UClass* Found = FindFirstObject<UClass>(*Name, EFindFirstObjectOptions::ExactClass))
		{
			return Found;
		}
		if (Name.Len() > 1 && Name[0] == TEXT('U'))
		{
			return FindFirstObject<UClass>(*Name.Mid(1), EFindFirstObjectOptions::ExactClass);
		}
		return nullptr;
	}

	/** A property by its C++ name, case-insensitively, or by its Python name (display_units -> bDisplayUnits) */
	FProperty* FindSettingsProperty(UClass* Class, const FString& Name)
	{
		if (FProperty* Exact = FindFProperty<FProperty>(Class, *Name))
		{
			return Exact;
		}
		auto Normalize = [](FString Text) { Text.ReplaceInline(TEXT("_"), TEXT("")); return Text.ToLower(); };
		const FString Wanted = Normalize(Name);
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			const FString Candidate = Normalize(It->GetName());
			if (Candidate == Wanted || (CastField<FBoolProperty>(*It) && Candidate.StartsWith(TEXT("b")) && Candidate.Mid(1) == Wanted))
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** A property value parsed into scratch memory owned by this object (a bad value then changes nothing) */
	struct FScratchValue
	{
		const FProperty* Property;
		void* Memory;
		explicit FScratchValue(const FProperty* InProperty)
			: Property(InProperty), Memory(FMemory::Malloc(InProperty->GetSize(), InProperty->GetMinAlignment()))
		{
			Property->InitializeValue(Memory);
		}
		~FScratchValue()
		{
			Property->DestroyValue(Memory);
			FMemory::Free(Memory);
		}
		FScratchValue(const FScratchValue&) = delete;
		FScratchValue& operator=(const FScratchValue&) = delete;
	};

	/** True when the value the config system now holds for the class (GConfig, reloaded from disk by the save) equals
	 *  Current. OutNote explains a mismatch or a missing line. */
	bool ConfigHoldsValue(UClass* Class, const FProperty* Property, const void* Current, FString& OutNote)
	{
		const FString Section = Class->GetPathName();
		const FString Key = Property->GetName();
		const FString IniName = Class->GetConfigName();
		FScratchValue FromConfig(Property);
		FOutputDeviceNull Quiet;
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			TArray<FString> Lines;
			GConfig->GetArray(*Section, *Key, Lines, IniName);
			FScriptArrayHelper Helper(ArrayProperty, FromConfig.Memory);
			for (const FString& Line : Lines)
			{
				const int32 Index = Helper.AddValue();
				ArrayProperty->Inner->ImportText_Direct(*Line, Helper.GetRawPtr(Index), nullptr, PPF_None, &Quiet);
			}
		}
		else
		{
			FString Text;
			if (!GConfig->GetString(*Section, *Key, Text, IniName))
			{
				OutNote = TEXT("no config line in any layer: the C++ default applies at the next start");
				return false;
			}
			Property->ImportText_Direct(*Text, FromConfig.Memory, nullptr, PPF_None, &Quiet);
		}
		if (Property->Identical(Current, FromConfig.Memory, PPF_None))
		{
			return true;
		}
		FString Held;
		Property->ExportTextItem_Direct(Held, FromConfig.Memory, nullptr, nullptr, PPF_None);
		OutNote = FString::Printf(TEXT("the config layers still resolve to %s (a higher layer, e.g. the user's Saved/Config, overrides it?)"), *Held);
		return false;
	}
	// ---- end of the project config file helpers ------------------------------------------------------------------

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
	else
	{
		// A project file GConfig does not know (see ResolveIni): list what is in the file on disk
		FConfigFile OnDisk;
		if (ReadDiskIni(ResolveIni(ConfigFile).DiskPath, OnDisk))
		{
			if (const FConfigSection* Found = OnDisk.FindSection(*Section))
			{
				for (const TPair<FName, FConfigValue>& Pair : *Found)
				{
					FString Key = Pair.Key.ToString();
					Key.RemoveFromStart(TEXT("+"));
					Key.RemoveFromStart(TEXT("."));
					Keys.AddUnique(Key);
				}
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

	// A project file GConfig does not know: read the file on disk
	FConfigFile OnDisk;
	if (ReadDiskIni(ResolveIni(ConfigFile).DiskPath, OnDisk))
	{
		const TArray<FString> Values = DiskValues(OnDisk, Section, Key);
		if (Values.Num() > 0)
		{
			return Values.Last();
		}
	}

	return FString();
}

FProjectSettingResult UProjectSettingsService::SetIniValue(const FString& Section, const FString& Key, const FString& Value, const FString& ConfigFile)
{
	FProjectSettingResult Result;

	// Written ON DISK with the engine's single-property writer (keeps the rest of the file and its comments), then
	// the branch is reloaded and the file read back. GConfig->SetString + Flush on a bare path reported success but
	// never reached the disk (see ResolveIni).
	const FResolvedIni Ini = ResolveIni(ConfigFile);
	if (Ini.DiskPath.IsEmpty() || Section.IsEmpty() || Key.IsEmpty())
	{
		Result.ErrorMessage = FString::Printf(TEXT("Invalid config file, section or key: '%s' [%s] %s"), *ConfigFile, *Section, *Key);
		return Result;
	}
	if (IFileManager::Get().FileExists(*Ini.DiskPath) && IFileManager::Get().IsReadOnly(*Ini.DiskPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("%s is read-only"), *Ini.DiskPath);
		return Result;
	}

	FConfigFile Pending;
	Pending.SetString(*Section, *Key, *Value);
	if (!Pending.UpdateSinglePropertyInSection(*Ini.DiskPath, *Key, *Section))
	{
		Result.ErrorMessage = FString::Printf(TEXT("The engine could not write [%s] %s to %s"), *Section, *Key, *Ini.DiskPath);
		return Result;
	}
	ReloadBranch(Ini.BranchBaseName);

	FConfigFile OnDisk;
	const TArray<FString> Written = ReadDiskIni(Ini.DiskPath, OnDisk) ? DiskValues(OnDisk, Section, Key) : TArray<FString>();
	if (Written.Num() == 0 || Written.Last() != Value)
	{
		Result.ErrorMessage = FString::Printf(TEXT("[%s] %s did not read back as '%s' from %s (found: %s)"),
			*Section, *Key, *Value, *Ini.DiskPath, Written.Num() ? *Written.Last() : TEXT("nothing"));
		return Result;
	}

	Result.bSuccess = true;
	// a settings object read its config at startup: it sees the new value after a restart (or use SetSettingsProperty)
	Result.bRequiresRestart = true;
	Result.ModifiedSettings.Add(FString::Printf(TEXT("[%s] %s = %s (%s)"), *Section, *Key, *Value, *Ini.DiskPath));

	UE_LOG(LogProjectSettingsService, Log, TEXT("Set INI value: [%s] %s = %s in %s (verified on disk)"), *Section, *Key, *Value, *Ini.DiskPath);
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
	if (Values.Num() == 0)
	{
		// A project file GConfig does not know: the file's own lines on disk (one layer, so the
		// +/- commands of this file are listed as written, not merged with the lower layers)
		FConfigFile OnDisk;
		if (ReadDiskIni(ResolveIni(ConfigFile).DiskPath, OnDisk))
		{
			Values = DiskValues(OnDisk, Section, Key);
		}
	}
	return Values;
}

FProjectSettingResult UProjectSettingsService::SetIniArray(const FString& Section, const FString& Key, const TArray<FString>& Values, const FString& ConfigFile)
{
	FProjectSettingResult Result;

	// Arrays in layered project files need !Key=ClearArray / +Key=... relative to the lower layers,
	// which the engine only writes correctly through a settings object (UObject::UpdateSingleSectionOfConfigFile).
	// A settings class section therefore goes through SetSettingsProperty; anything else fails honestly.
	if (UClass* Class = Section.StartsWith(TEXT("/Script/")) ? FindSettingsClass(Section) : nullptr)
	{
		TArray<FString> Items;
		for (const FString& Item : Values)
		{
			// quote anything the struct/array text parser would split on
			const bool bNeedsQuotes = Item.IsEmpty() || Item.Contains(TEXT(",")) || Item.Contains(TEXT("(")) || Item.Contains(TEXT(")"))
				|| Item.Contains(TEXT("\"")) || Item.Contains(TEXT(" "));
			Items.Add(bNeedsQuotes ? FString::Printf(TEXT("\"%s\""), *Item.ReplaceCharWithEscapedChar()) : Item);
		}
		Result = SetSettingsProperty(Class->GetPathName(), Key, FString::Printf(TEXT("(%s)"), *FString::Join(Items, TEXT(","))));
		const FResolvedIni Asked = ResolveIni(ConfigFile);
		const FString Owner = Class->HasAnyClassFlags(CLASS_DefaultConfig) ? Class->GetDefaultObject()->GetDefaultConfigFilename() : Class->GetConfigName();
		if (Result.bSuccess && !Asked.DiskPath.IsEmpty() && !FPaths::IsSamePath(FPaths::ConvertRelativePathToFull(Owner), Asked.DiskPath))
		{
			Result.ModifiedSettings.Add(FString::Printf(TEXT("note: saved to the class's own config file %s, not %s"), *Owner, *ConfigFile));
		}
		return Result;
	}

	Result.ErrorMessage = FString::Printf(TEXT("SetIniArray: [%s] is not a settings class section, and an array in a layered project ini needs "
		"engine array commands that cannot be written safely here. Use SetSettingsProperty on the owning settings class, or edit %s by hand."),
		*Section, *ConfigFile);
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
	// A branch name ("Engine", "Editor", ...) flushes that branch's user layer (Saved/Config)
	if (!ConfigFile.IsEmpty() && !ConfigFile.Contains(TEXT(".")) && GConfig->FindBranchWithNoReload(FName(*ConfigFile), FString()))
	{
		GConfig->Flush(false, GConfig->GetConfigFilename(*ConfigFile));
		UE_LOG(LogProjectSettingsService, Log, TEXT("Flushed config branch: %s"), *ConfigFile);
		return true;
	}

	FString ConfigPath = ::GetConfigFilePath(ConfigFile);
	if (ConfigPath.IsEmpty())
	{
		UE_LOG(LogProjectSettingsService, Warning, TEXT("Invalid config file: %s"), *ConfigFile);
		return false;
	}

	// Project Default*.ini writes (SetIniValue, SetSettingsProperty) are already on disk
	if (!ResolveIni(ConfigFile).BranchBaseName.IsEmpty())
	{
		UE_LOG(LogProjectSettingsService, Log, TEXT("Nothing pending for %s: writes go to disk immediately"), *ConfigFile);
		return true;
	}

	GConfig->Flush(false, ConfigPath);
	UE_LOG(LogProjectSettingsService, Log, TEXT("Saved config file: %s"), *ConfigFile);
	return true;
}

// =================================================================
// Settings objects
// =================================================================

FProjectSettingResult UProjectSettingsService::SetSettingsProperty(const FString& SettingsClass, const FString& PropertyName, const FString& Value)
{
	FProjectSettingResult Result;

	UClass* Class = FindSettingsClass(SettingsClass);
	if (!Class || !Class->HasAnyClassFlags(CLASS_Config))
	{
		Result.ErrorMessage = FString::Printf(TEXT("'%s' is not a loaded config (settings) class"), *SettingsClass);
		return Result;
	}
	if (Class->HasAnyClassFlags(CLASS_PerObjectConfig))
	{
		Result.ErrorMessage = FString::Printf(TEXT("%s uses per-object config: its CDO is not what gets saved"), *Class->GetName());
		return Result;
	}
	FProperty* Property = FindSettingsProperty(Class, PropertyName);
	if (!Property || !Property->HasAnyPropertyFlags(CPF_Config | CPF_GlobalConfig))
	{
		Result.ErrorMessage = FString::Printf(TEXT("%s has no config property '%s'"), *Class->GetName(), *PropertyName);
		return Result;
	}

	UObject* Settings = Class->GetDefaultObject();
	void* Target = Property->ContainerPtrToValuePtr<void>(Settings);

	// parse first: a value that doesn't parse changes nothing
	FScratchValue Parsed(Property);
	FStringOutputDevice ParseErrors;
	const TCHAR* End = Property->ImportText_Direct(*Value, Parsed.Memory, Settings, PPF_None, &ParseErrors);
	if (!End || !ParseErrors.IsEmpty())
	{
		Result.ErrorMessage = FString::Printf(TEXT("'%s' is not a valid %s value for %s: %s"), *Value, *Property->GetCPPType(), *Property->GetName(), *ParseErrors);
		return Result;
	}

	FString OldText;
	Property->ExportTextItem_Direct(OldText, Target, nullptr, Settings, PPF_None);

	// apply live, like an edit in the Settings window's details view
	Settings->PreEditChange(Property);
	Property->CopyCompleteValue(Target, Parsed.Memory);
	FPropertyChangedEvent ChangedEvent(Property, EPropertyChangeType::ValueSet);
	Settings->PostEditChangeProperty(ChangedEvent);

	// save exactly like FSettingsSection::Save() (Developer/Settings/Private/SettingsSection.cpp)
	FString SavedTo;
	if (Class->HasAnyClassFlags(CLASS_DefaultConfig))
	{
		SavedTo = Settings->GetDefaultConfigFilename();
		if (!Settings->TryUpdateDefaultConfigFile())
		{
			Result.ErrorMessage = FString::Printf(TEXT("Applied live but NOT saved: %s is read-only"), *SavedTo);
			return Result;
		}
	}
	else if (Class->HasAnyClassFlags(CLASS_GlobalUserConfig))
	{
		SavedTo = Settings->GetGlobalUserConfigFilename();
		Settings->UpdateGlobalUserConfigFile();
	}
	else if (Class->HasAnyClassFlags(CLASS_ProjectUserConfig))
	{
		SavedTo = Settings->GetProjectUserConfigFilename();
		Settings->UpdateProjectUserConfigFile();
	}
	else
	{
		SavedTo = Class->GetConfigName();
		Settings->SaveConfig();
	}

	FString NewText;
	Property->ExportTextItem_Direct(NewText, Target, nullptr, Settings, PPF_None);

	// verify against what the config system will hand the class at the next start
	FString Note;
	if (!ConfigHoldsValue(Class, Property, Target, Note))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Applied live and written to %s, but %s"), *SavedTo, *Note);
		Result.FailedSettings.Add(FString::Printf(TEXT("[%s] %s"), *Class->GetPathName(), *Property->GetName()));
		return Result;
	}

	Result.bSuccess = true;
	Result.ModifiedSettings.Add(FString::Printf(TEXT("[%s] %s: %s -> %s (%s)"), *Class->GetPathName(), *Property->GetName(), *OldText, *NewText, *SavedTo));
	UE_LOG(LogProjectSettingsService, Log, TEXT("Set settings property %s.%s = %s, saved to %s (verified)"), *Class->GetName(), *Property->GetName(), *NewText, *SavedTo);
	return Result;
}

FString UProjectSettingsService::GetSettingsProperty(const FString& SettingsClass, const FString& PropertyName)
{
	UClass* Class = FindSettingsClass(SettingsClass);
	FProperty* Property = Class ? FindSettingsProperty(Class, PropertyName) : nullptr;
	if (!Property)
	{
		return FString();
	}
	UObject* Settings = Class->GetDefaultObject();
	FString Text;
	Property->ExportTextItem_Direct(Text, Property->ContainerPtrToValuePtr<void>(Settings), nullptr, Settings, PPF_None);
	return Text;
}
