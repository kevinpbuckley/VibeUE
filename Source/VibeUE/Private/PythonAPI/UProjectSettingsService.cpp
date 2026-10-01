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
#include "UObject/TextProperty.h"
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

	/** The single value of Key in one on-disk file section, resolved the way GConfig resolves it in its copy of a single
	 *  file (FConfigCacheIni::GetString: FConfigSection::Find on the plain key). A file read without its commands keeps
	 *  array lines under their prefixed names (+Key, .Key, -Key, !Key), so those are not this value. nullptr when the
	 *  section has no plain Key= line. */
	const FConfigValue* SettingsIniScalar(const FConfigFile& File, const FString& Section, const FString& Key)
	{
		const FConfigSection* Found = File.FindSection(*Section);
		return Found ? Found->Find(FName(*Key)) : nullptr;
	}

	/** The array values of Key in one on-disk file section, in file order: Key=, +Key= and .Key= lines (one layer, so
	 *  this file's own lines; -Key= removals and !Key= clears are not values) */
	TArray<FString> SettingsIniArrayLines(const FConfigFile& File, const FString& Section, const FString& Key)
	{
		TArray<FString> Values;
		if (const FConfigSection* Found = File.FindSection(*Section))
		{
			const FName Plain(*Key);
			const FName Add(*(TEXT("+") + Key));
			const FName AddUnique(*(TEXT(".") + Key));
			for (const TPair<FName, FConfigValue>& Pair : *Found)
			{
				if (Pair.Key == Plain || Pair.Key == Add || Pair.Key == AddUnique)
				{
					Values.Add(Pair.Value.GetValue());
				}
			}
		}
		return Values;
	}

	/** The name GConfig holds ConfigPath under when the engine keeps that file current itself: a config branch, or a
	 *  file GConfig loaded in order to save it. Empty otherwise, in particular for the NoSave copy GConfig::Find makes
	 *  of a bare path such as Config/DefaultGame.ini on its first read: that copy is never re-read from disk
	 *  (ini.TimeToUnloadConfig defaults to 0, and reloading a branch does not touch it), so reading through it returned
	 *  the old value after set_ini_value, set_settings_property or a hand edit changed the file. Such files are read
	 *  from disk instead, which is what GConfig's copy held when it was made. */
	FString SettingsIniLiveName(const FString& ConfigPath)
	{
		if (!GConfig || ConfigPath.IsEmpty())
		{
			return FString();
		}
		for (const FString& Name : { ConfigPath, FConfigCacheIni::NormalizeConfigIniPath(ConfigPath) })
		{
			// FindBranchWithNoReload never loads a file, unlike the GConfig getters
			const FConfigBranch* Branch = GConfig->FindBranchWithNoReload(NAME_None, Name);
			if (Branch && Branch->IniPath == Name && (Branch->bIsHierarchical || !Branch->InMemoryFile.NoSave))
			{
				return Name;
			}
		}
		return FString();
	}

	/** Why a section or key cannot be written as raw ini text: the engine writes both verbatim, so a line break would
	 *  inject lines into the file and a ']' would end the [Section] header early. Empty when the text is safe. */
	FString SettingsIniBadName(const FString& Section, const FString& Key)
	{
		if (Section.Contains(TEXT("\r")) || Section.Contains(TEXT("\n")) || Key.Contains(TEXT("\r")) || Key.Contains(TEXT("\n")))
		{
			return TEXT("a section or key cannot contain a line break");
		}
		if (Section.Contains(TEXT("]")))
		{
			return TEXT("a section name cannot contain ']'");
		}
		return FString();
	}

	/** Puts a file back as it was before a failed write: its original bytes, or no file when there was none */
	bool SettingsIniRestore(const FString& DiskPath, bool bExisted, const TArray<uint8>& OriginalBytes)
	{
		return bExisted
			? FFileHelper::SaveArrayToFile(OriginalBytes, *DiskPath)
			: IFileManager::Get().Delete(*DiskPath, /*RequireExists*/ false, /*EvenReadOnly*/ false, /*Quiet*/ true);
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
	const FString LiveName = SettingsIniLiveName(ConfigPath);
	if (!LiveName.IsEmpty() && GConfig->GetSection(*Section, KeyValuePairs, LiveName))
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
		// Any other file (see ResolveIni, SettingsIniLiveName): list what is in the file on disk now
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
	const FString LiveName = SettingsIniLiveName(ConfigPath);
	if (!LiveName.IsEmpty() && GConfig->GetString(*Section, *Key, Value, LiveName))
	{
		return Value;
	}

	// Any other file (see SettingsIniLiveName): read the file on disk now
	FConfigFile OnDisk;
	if (ReadDiskIni(ResolveIni(ConfigFile).DiskPath, OnDisk))
	{
		if (const FConfigValue* Found = SettingsIniScalar(OnDisk, Section, Key))
		{
			return Found->GetValue();
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
	const FString BadName = SettingsIniBadName(Section, Key);
	if (!BadName.IsEmpty())
	{
		Result.ErrorMessage = FString::Printf(TEXT("Invalid section or key for %s: %s"), *Ini.DiskPath, *BadName);
		return Result;
	}
	// a mistyped path would otherwise create an arbitrary file
	if (!FPaths::GetExtension(Ini.DiskPath).Equals(TEXT("ini"), ESearchCase::IgnoreCase))
	{
		Result.ErrorMessage = FString::Printf(TEXT("'%s' is not an .ini file"), *Ini.DiskPath);
		return Result;
	}
	if (IFileManager::Get().FileExists(*Ini.DiskPath) && IFileManager::Get().IsReadOnly(*Ini.DiskPath))
	{
		Result.ErrorMessage = FString::Printf(TEXT("%s is read-only"), *Ini.DiskPath);
		return Result;
	}

	// keep the file as it is now, to put it back if the write does not land
	const bool bExisted = IFileManager::Get().FileExists(*Ini.DiskPath);
	TArray<uint8> OriginalBytes;
	if (bExisted && !FFileHelper::LoadFileToArray(OriginalBytes, *Ini.DiskPath, FILEREAD_Silent))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Could not read %s to keep a copy before writing it"), *Ini.DiskPath);
		return Result;
	}

	FConfigFile Pending;
	Pending.SetString(*Section, *Key, *Value);
	if (!Pending.UpdateSinglePropertyInSection(*Ini.DiskPath, *Key, *Section))
	{
		const bool bRestored = SettingsIniRestore(Ini.DiskPath, bExisted, OriginalBytes);
		Result.ErrorMessage = FString::Printf(TEXT("The engine could not write [%s] %s to %s%s"), *Section, *Key, *Ini.DiskPath,
			bRestored ? TEXT("") : TEXT(" (and the file could not be put back as it was)"));
		return Result;
	}
	ReloadBranch(Ini.BranchBaseName);

	// compare the text as written (GetSavedValue), not with %PLACEHOLDERS% expanded (GetValue)
	FConfigFile OnDisk;
	const FConfigValue* Written = ReadDiskIni(Ini.DiskPath, OnDisk) ? SettingsIniScalar(OnDisk, Section, Key) : nullptr;
	if (!Written || !Written->GetSavedValue().Equals(Value, ESearchCase::CaseSensitive))
	{
		const FString Found = Written ? Written->GetSavedValue() : FString(TEXT("nothing"));
		const bool bRestored = SettingsIniRestore(Ini.DiskPath, bExisted, OriginalBytes);
		ReloadBranch(Ini.BranchBaseName);
		Result.ErrorMessage = FString::Printf(TEXT("[%s] %s did not read back as '%s' from %s (found: %s); %s"),
			*Section, *Key, *Value, *Ini.DiskPath, *Found,
			bRestored ? TEXT("the file was put back as it was") : TEXT("the file could NOT be put back as it was"));
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

	const FString LiveName = SettingsIniLiveName(ConfigPath);
	if (!LiveName.IsEmpty())
	{
		GConfig->GetArray(*Section, *Key, Values, LiveName);
	}
	if (Values.Num() == 0)
	{
		// Any other file (see SettingsIniLiveName): the file's own lines on disk now (one layer, so the
		// +/- commands of this file are listed as written, not merged with the lower layers)
		FConfigFile OnDisk;
		if (ReadDiskIni(ResolveIni(ConfigFile).DiskPath, OnDisk))
		{
			Values = SettingsIniArrayLines(OnDisk, Section, Key);
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
		// the array's element property, to tell a struct literal from a string that merely looks like one
		const FProperty* Target = FindSettingsProperty(Class, Key);
		const FProperty* Element = nullptr;
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Target))
		{
			Element = ArrayProperty->Inner;
		}
		else if (const FSetProperty* SetProperty = CastField<FSetProperty>(Target))
		{
			Element = SetProperty->ElementProp;
		}
		// FStrProperty / FTextProperty import an array element (PPF_Delimited) only from a quoted string
		const bool bQuotedElements = Element && (Element->IsA<FStrProperty>() || Element->IsA<FTextProperty>());
		const bool bTextElements = bQuotedElements || (Element && Element->IsA<FNameProperty>());

		TArray<FString> Items;
		for (const FString& Item : Values)
		{
			// a struct element "(Name=...,Value=...)" goes in as written: UScriptStruct::ImportText needs its leading
			// '(' and a quoted struct does not parse (+Profiles=, +ActiveClassRedirects= style arrays)
			const FString Trimmed = Item.TrimStartAndEnd();
			if (!bTextElements && Trimmed.StartsWith(TEXT("(")) && Trimmed.EndsWith(TEXT(")")))
			{
				Items.Add(Trimmed);
				continue;
			}
			// quote a string element, and anything else the struct/array text parser would split on
			const bool bNeedsQuotes = bQuotedElements || Item.IsEmpty() || Item.Contains(TEXT(",")) || Item.Contains(TEXT("(")) || Item.Contains(TEXT(")"))
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
