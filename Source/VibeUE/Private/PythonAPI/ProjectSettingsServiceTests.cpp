// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UProjectSettingsService.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

static const EAutomationTestFlags kSettingsTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

namespace VibeSettingsTest
{
	/**
	 * Puts a config file back byte for byte when the test ends (or deletes it if it did not exist) and, when BranchName
	 * is set, reloads that config branch, so nothing a test wrote stays on disk or in GConfig. Nothing puts the file
	 * back if the editor crashes mid-test, so only the tests that need a real project file (a config branch reload, a
	 * settings class's own Default*.ini) write the host project's Config/DefaultGame.ini; the others use ScratchIni().
	 */
	struct FScopedConfigFileRestore
	{
		FString Path;
		FString BranchName;
		bool bExisted = false;
		TArray<uint8> Bytes;

		FScopedConfigFileRestore(const FString& InPath, const FString& InBranchName)
			: Path(InPath), BranchName(InBranchName)
		{
			bExisted = FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent);
		}

		~FScopedConfigFileRestore()
		{
			if (bExisted)
			{
				FFileHelper::SaveArrayToFile(Bytes, *Path);
			}
			else
			{
				IFileManager::Get().Delete(*Path, /*RequireExists*/ false);
			}
			if (!BranchName.IsEmpty() && GConfig && GConfig->FindBranchWithNoReload(FName(*BranchName), FString()))
			{
				FConfigContext Context = FConfigContext::ForceReloadIntoGConfig();
				Context.bWriteDestIni = false;
				Context.Load(*BranchName);
			}
		}
	};

	static FString DefaultGameIni()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini"));
	}

	/** A new scratch .ini path under the project's Saved folder: no config branch layers it, and the name is unique, so
	 *  GConfig cannot hold a copy of it before the test's own first read */
	static FString ScratchIni()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("VibeUETests")
			/ FString::Printf(TEXT("ProjectSettings_%s.ini"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	static FString ReadDisk(const FString& Path)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *Path);
		return Text;
	}

	static TArray<uint8> ReadDiskBytes(const FString& Path)
	{
		TArray<uint8> Bytes;
		FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent);
		return Bytes;
	}

	static void WriteDisk(const FString& Path, const FString& Text)
	{
		FFileHelper::SaveStringToFile(Text, *Path);
	}
}

// set_ini_value must put the value in the file on disk, not only in GConfig's memory, and keep the rest of the file;
// reads must see the file as it is now, also when they ran before the write (GConfig's NoSave copy of a bare path,
// made by the first read, is never re-read, so a read through it kept returning the old value).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeSettingsIniWriteReachesDiskTest,
	"VibeUE.ProjectSettings.IniWriteReachesDisk", kSettingsTestFlags)
bool FVibeSettingsIniWriteReachesDiskTest::RunTest(const FString&)
{
	using namespace VibeSettingsTest;
	const FString File = ScratchIni();
	FScopedConfigFileRestore Restore(File, FString());

	const FString Section = TEXT("VibeUETests.Scratch");
	const FString Key = TEXT("WrittenByTest");
	const FString Old = TEXT("OldValue");
	const FString Value = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Before = FString::Printf(TEXT("; a comment the write keeps\r\n[VibeUETests.Other]\r\nKeep=Me\r\n\r\n[%s]\r\n%s=%s\r\n"), *Section, *Key, *Old);
	WriteDisk(File, Before);

	// read BEFORE writing
	TestEqual(TEXT("get_ini_value reads the value already in the file"), UProjectSettingsService::GetIniValue(Section, Key, File), Old);
	TestTrue(TEXT("list_ini_keys lists the key"), UProjectSettingsService::ListIniKeys(Section, File).Contains(Key));

	const FProjectSettingResult Result = UProjectSettingsService::SetIniValue(Section, Key, Value, File);
	TestTrue(FString::Printf(TEXT("set_ini_value reports success (%s)"), *Result.ErrorMessage), Result.bSuccess);

	const FString After = ReadDisk(File);
	TestTrue(TEXT("the value is in the file on disk"), After.Contains(Key + TEXT("=") + Value));
	TestFalse(TEXT("the old line is replaced"), After.Contains(Key + TEXT("=") + Old));
	TArray<FString> BeforeLines;
	Before.ParseIntoArrayLines(BeforeLines);
	for (const FString& Line : BeforeLines)
	{
		if (!Line.TrimStartAndEnd().IsEmpty() && !Line.StartsWith(Key + TEXT("=")))
		{
			TestTrue(FString::Printf(TEXT("the file's earlier line is kept: %s"), *Line), After.Contains(Line));
		}
	}
	TestEqual(TEXT("get_ini_value reads the new value after an earlier read"), UProjectSettingsService::GetIniValue(Section, Key, File), Value);

	// a hand edit of the file is seen as well
	const FString HandEdited = TEXT("EditedByHand");
	WriteDisk(File, After.Replace(*(Key + TEXT("=") + Value), *(Key + TEXT("=") + HandEdited)));
	TestEqual(TEXT("get_ini_value sees a hand edit"), UProjectSettingsService::GetIniValue(Section, Key, File), HandEdited);
	return true;
}

// The single value of a key is its plain Key= line, as GConfig resolves it: +Key= / .Key= / -Key= array lines in the
// same section are not it. get_ini_array lists the value lines in file order.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeSettingsIniScalarIgnoresArrayLinesTest,
	"VibeUE.ProjectSettings.IniScalarIgnoresArrayLines", kSettingsTestFlags)
bool FVibeSettingsIniScalarIgnoresArrayLinesTest::RunTest(const FString&)
{
	using namespace VibeSettingsTest;
	const FString File = ScratchIni();
	FScopedConfigFileRestore Restore(File, FString());

	const FString Section = TEXT("VibeUETests.Scratch");
	WriteDisk(File, FString::Printf(TEXT("[%s]\r\nMixed=Plain\r\n+Mixed=Added\r\n.Mixed=AddedUnique\r\n-Mixed=Removed\r\n+ArrayOnly=First\r\n"), *Section));

	TestEqual(TEXT("get_ini_value returns the plain line, not the last array line"), UProjectSettingsService::GetIniValue(Section, TEXT("Mixed"), File), FString(TEXT("Plain")));
	const TArray<FString> Mixed = UProjectSettingsService::GetIniArray(Section, TEXT("Mixed"), File);
	TestEqual(TEXT("get_ini_array lists the three value lines"), Mixed.Num(), 3);
	if (Mixed.Num() == 3)
	{
		TestEqual(TEXT("in file order (1)"), Mixed[0], FString(TEXT("Plain")));
		TestEqual(TEXT("in file order (2)"), Mixed[1], FString(TEXT("Added")));
		TestEqual(TEXT("in file order (3)"), Mixed[2], FString(TEXT("AddedUnique")));
	}

	// writing a plain line next to an existing +Key= line must read back as written
	const FString Value = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FProjectSettingResult Result = UProjectSettingsService::SetIniValue(Section, TEXT("ArrayOnly"), Value, File);
	TestTrue(FString::Printf(TEXT("set_ini_value next to a +Key= line succeeds (%s)"), *Result.ErrorMessage), Result.bSuccess);
	TestEqual(TEXT("get_ini_value reads it back"), UProjectSettingsService::GetIniValue(Section, TEXT("ArrayOnly"), File), Value);
	return true;
}

// set_ini_value refuses a section or key that would inject raw lines, a path that is not an .ini file, and puts the
// file back as it was when the value does not read back as written.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeSettingsIniWriteRefusesBadInputTest,
	"VibeUE.ProjectSettings.IniWriteRefusesBadInput", kSettingsTestFlags)
bool FVibeSettingsIniWriteRefusesBadInputTest::RunTest(const FString&)
{
	using namespace VibeSettingsTest;
	const FString File = ScratchIni();
	FScopedConfigFileRestore Restore(File, FString());
	const FString Section = TEXT("VibeUETests.Scratch");

	const FProjectSettingResult CrLfSection = UProjectSettingsService::SetIniValue(Section + TEXT("]\r\n[Injected"), TEXT("Key"), TEXT("1"), File);
	TestFalse(TEXT("a section with a line break is refused"), CrLfSection.bSuccess);
	const FProjectSettingResult LfKey = UProjectSettingsService::SetIniValue(Section, TEXT("Key\nInjected"), TEXT("1"), File);
	TestFalse(TEXT("a key with a line break is refused"), LfKey.bSuccess);
	const FProjectSettingResult BracketSection = UProjectSettingsService::SetIniValue(Section + TEXT("]Tail"), TEXT("Key"), TEXT("1"), File);
	TestFalse(TEXT("a section with ']' is refused"), BracketSection.bSuccess);
	TestFalse(TEXT("a refusal says why"), BracketSection.ErrorMessage.IsEmpty());
	TestFalse(TEXT("refusals create no file"), IFileManager::Get().FileExists(*File));

	const FString NotIni = FPaths::ChangeExtension(File, TEXT("txt"));
	FScopedConfigFileRestore RestoreNotIni(NotIni, FString());
	TestFalse(TEXT("a path that is not an .ini file is refused"), UProjectSettingsService::SetIniValue(Section, TEXT("Key"), TEXT("1"), NotIni).bSuccess);
	TestFalse(TEXT("and not created"), IFileManager::Get().FileExists(*NotIni));

	// a value the ini format cannot hold as written (a trailing tab is stripped on read) fails the read-back check
	const FString Old = TEXT("OldValue");
	WriteDisk(File, FString::Printf(TEXT("; kept as it was\r\n[%s]\r\nKey=%s\r\n"), *Section, *Old));
	const TArray<uint8> Original = ReadDiskBytes(File);
	const FProjectSettingResult Lossy = UProjectSettingsService::SetIniValue(Section, TEXT("Key"), TEXT("Lossy\t"), File);
	if (Lossy.bSuccess)
	{
		AddInfo(TEXT("this engine round-trips a trailing tab; the put-back path was not exercised"));
	}
	else
	{
		TestTrue(TEXT("a failed read-back puts the file back byte for byte"), ReadDiskBytes(File) == Original);
		TestEqual(TEXT("and the old value reads back"), UProjectSettingsService::GetIniValue(Section, TEXT("Key"), File), Old);

		const FString Fresh = ScratchIni();
		FScopedConfigFileRestore RestoreFresh(Fresh, FString());
		TestFalse(TEXT("a failed read-back into a new file fails"), UProjectSettingsService::SetIniValue(Section, TEXT("Key"), TEXT("Lossy\t"), Fresh).bSuccess);
		TestFalse(TEXT("and removes the file it created"), IFileManager::Get().FileExists(*Fresh));
	}
	return true;
}

// set_ini_value on a project Default*.ini reloads the config branch it layers into, so GConfig (and the next
// get_ini_value, also after an earlier read) sees the new value. This one needs the real DefaultGame.ini.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeSettingsIniWriteReloadsBranchTest,
	"VibeUE.ProjectSettings.IniWriteReloadsBranch", kSettingsTestFlags)
bool FVibeSettingsIniWriteReloadsBranchTest::RunTest(const FString&)
{
	using namespace VibeSettingsTest;
	const FString File = DefaultGameIni();
	FScopedConfigFileRestore Restore(File, TEXT("Game"));

	const FString Section = TEXT("VibeUETests.Scratch");
	const FString Key = TEXT("WrittenByTest");
	const FString First = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Second = FGuid::NewGuid().ToString(EGuidFormats::Digits);

	const FProjectSettingResult FirstResult = UProjectSettingsService::SetIniValue(Section, Key, First, TEXT("DefaultGame.ini"));
	TestTrue(FString::Printf(TEXT("the first set_ini_value succeeds (%s)"), *FirstResult.ErrorMessage), FirstResult.bSuccess);
	TestEqual(TEXT("get_ini_value reads the first value"), UProjectSettingsService::GetIniValue(Section, Key, TEXT("DefaultGame.ini")), First);

	const FProjectSettingResult SecondResult = UProjectSettingsService::SetIniValue(Section, Key, Second, TEXT("DefaultGame.ini"));
	TestTrue(FString::Printf(TEXT("the second set_ini_value succeeds (%s)"), *SecondResult.ErrorMessage), SecondResult.bSuccess);
	TestTrue(TEXT("DefaultGame.ini on disk holds the second value"), ReadDisk(File).Contains(Key + TEXT("=") + Second));
	TestEqual(TEXT("get_ini_value reads the second value after an earlier read"), UProjectSettingsService::GetIniValue(Section, Key, TEXT("DefaultGame.ini")), Second);

	FString FromBranch;
	GConfig->GetString(*Section, *Key, FromBranch, GGameIni);
	TestEqual(TEXT("the Game config branch sees the second value"), FromBranch, Second);
	return true;
}

// set_ini_array may refuse, but when it reports success the lines must be in the file on disk.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeSettingsIniArrayHonestTest,
	"VibeUE.ProjectSettings.IniArrayIsHonest", kSettingsTestFlags)
bool FVibeSettingsIniArrayHonestTest::RunTest(const FString&)
{
	using namespace VibeSettingsTest;
	const FString File = ScratchIni();
	FScopedConfigFileRestore Restore(File, FString());

	const FString Section = TEXT("VibeUETests.Scratch");
	const FString Key = TEXT("ArrayWrittenByTest");
	const FString Marker = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FProjectSettingResult Result = UProjectSettingsService::SetIniArray(Section, Key, { Marker + TEXT("_A"), Marker + TEXT("_B") }, File);

	const FString After = ReadDisk(File);
	if (Result.bSuccess)
	{
		TestTrue(TEXT("a reported success put the first line on disk"), After.Contains(Marker + TEXT("_A")));
		TestTrue(TEXT("a reported success put the second line on disk"), After.Contains(Marker + TEXT("_B")));
	}
	else
	{
		TestFalse(TEXT("a refusal says why"), Result.ErrorMessage.IsEmpty());
		TestFalse(TEXT("a refusal writes nothing"), After.Contains(Marker));
	}
	return true;
}

// set_settings_property changes a settings class's CDO live and saves it the way the Settings window does; here
// GeneralProjectSettings.Description, a defaultconfig property in DefaultGame.ini, put back afterwards.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeSettingsPropertySavesTest,
	"VibeUE.ProjectSettings.SettingsPropertySaves", kSettingsTestFlags)
bool FVibeSettingsPropertySavesTest::RunTest(const FString&)
{
	using namespace VibeSettingsTest;
	const FString File = DefaultGameIni();
	FScopedConfigFileRestore Restore(File, TEXT("Game"));

	const FString Class = TEXT("GeneralProjectSettings");
	const FString Old = UProjectSettingsService::GetSettingsProperty(Class, TEXT("Description"));
	const FString Value = FString(TEXT("VibeUETest")) + FGuid::NewGuid().ToString(EGuidFormats::Digits);

	const FProjectSettingResult Result = UProjectSettingsService::SetSettingsProperty(Class, TEXT("description"), Value);
	TestTrue(FString::Printf(TEXT("set_settings_property succeeded (%s)"), *Result.ErrorMessage), Result.bSuccess);
	TestEqual(TEXT("the class default object holds it"), UProjectSettingsService::GetSettingsProperty(Class, TEXT("Description")), Value);
	TestTrue(TEXT("DefaultGame.ini on disk holds it"), ReadDisk(File).Contains(TEXT("Description=") + Value));

	const FProjectSettingResult Unknown = UProjectSettingsService::SetSettingsProperty(Class, TEXT("NoSuchSetting"), TEXT("1"));
	TestFalse(TEXT("an unknown property is refused"), Unknown.bSuccess);
	const FProjectSettingResult NotAClass = UProjectSettingsService::SetSettingsProperty(TEXT("NoSuchSettingsClass"), TEXT("Description"), TEXT("x"));
	TestFalse(TEXT("an unknown class is refused"), NotAClass.bSuccess);

	// put the live value back too (the file is restored by the guard)
	TestTrue(TEXT("the old value goes back"), UProjectSettingsService::SetSettingsProperty(Class, TEXT("Description"), Old).bSuccess);
	TestEqual(TEXT("the class default object is back"), UProjectSettingsService::GetSettingsProperty(Class, TEXT("Description")), Old);
	return true;
}

// set_ini_array on a settings class's struct array: an element written as a struct literal "(Field=Value,...)" must
// reach the struct parser unquoted (a quoted struct does not parse). ProjectPackagingSettings'
// DirectoriesToAlwaysStageAsNonUFS (TArray<FDirectoryPath>, defaultconfig in DefaultGame.ini), put back afterwards.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeSettingsIniArrayStructElementsTest,
	"VibeUE.ProjectSettings.IniArrayStructElements", kSettingsTestFlags)
bool FVibeSettingsIniArrayStructElementsTest::RunTest(const FString&)
{
	using namespace VibeSettingsTest;
	const FString Class = TEXT("/Script/DeveloperToolSettings.ProjectPackagingSettings");
	const FString Key = TEXT("DirectoriesToAlwaysStageAsNonUFS");
	if (!TestNotNull(TEXT("ProjectPackagingSettings is loaded"), FindObject<UClass>(nullptr, *Class)))
	{
		return false;
	}
	const FString File = DefaultGameIni();
	FScopedConfigFileRestore Restore(File, TEXT("Game"));

	const FString Old = UProjectSettingsService::GetSettingsProperty(Class, Key);
	const FString Marker = FString(TEXT("VibeUETest_")) + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Element = FString::Printf(TEXT("(Path=\"%s/Some Dir\")"), *Marker);

	const FProjectSettingResult Result = UProjectSettingsService::SetIniArray(Class, Key, { Element }, TEXT("DefaultGame.ini"));
	TestTrue(FString::Printf(TEXT("set_ini_array with a struct element succeeds (%s)"), *Result.ErrorMessage), Result.bSuccess);
	TestTrue(TEXT("the class default object holds the element"), UProjectSettingsService::GetSettingsProperty(Class, Key).Contains(Marker + TEXT("/Some Dir")));
	TestTrue(TEXT("DefaultGame.ini on disk holds the element"), ReadDisk(File).Contains(Marker));
	const TArray<FString> Lines = UProjectSettingsService::GetIniArray(Class, Key, TEXT("DefaultGame.ini"));
	TestTrue(TEXT("get_ini_array reads the element back as a struct literal"),
		Lines.ContainsByPredicate([&Marker](const FString& Line) { return Line.StartsWith(TEXT("(")) && Line.Contains(Marker); }));

	// put the live value back too (the file is restored by the guard)
	TestTrue(TEXT("the old value goes back"), UProjectSettingsService::SetSettingsProperty(Class, Key, Old).bSuccess);
	TestEqual(TEXT("the class default object is back"), UProjectSettingsService::GetSettingsProperty(Class, Key), Old);
	return true;
}

// set_ini_array on a settings class's string array: every element is quoted for the string parser (an array element
// of an FString property imports only from a quoted string), plain or not. ProjectPackagingSettings'
// IniSectionDenylist (TArray<FString>, defaultconfig in DefaultGame.ini), put back afterwards.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeSettingsIniArrayStringElementsTest,
	"VibeUE.ProjectSettings.IniArrayStringElements", kSettingsTestFlags)
bool FVibeSettingsIniArrayStringElementsTest::RunTest(const FString&)
{
	using namespace VibeSettingsTest;
	const FString Class = TEXT("/Script/DeveloperToolSettings.ProjectPackagingSettings");
	const FString Key = TEXT("IniSectionDenylist");
	if (!TestNotNull(TEXT("ProjectPackagingSettings is loaded"), FindObject<UClass>(nullptr, *Class)))
	{
		return false;
	}
	const FString File = DefaultGameIni();
	FScopedConfigFileRestore Restore(File, TEXT("Game"));

	const FString Old = UProjectSettingsService::GetSettingsProperty(Class, Key);
	const FString Marker = FString(TEXT("VibeUETest_")) + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString Plain = Marker + TEXT("_Plain");
	const FString Spaced = Marker + TEXT(" with space, comma (and parens)");

	const FProjectSettingResult Result = UProjectSettingsService::SetIniArray(Class, Key, { Plain, Spaced }, TEXT("DefaultGame.ini"));
	TestTrue(FString::Printf(TEXT("set_ini_array with string elements succeeds (%s)"), *Result.ErrorMessage), Result.bSuccess);
	const FString Now = UProjectSettingsService::GetSettingsProperty(Class, Key);
	TestTrue(TEXT("the class default object holds the plain element"), Now.Contains(Plain));
	TestTrue(TEXT("the class default object holds the element with spaces, commas and parentheses"), Now.Contains(Spaced));

	// put the live value back too (the file is restored by the guard)
	TestTrue(TEXT("the old value goes back"), UProjectSettingsService::SetSettingsProperty(Class, Key, Old).bSuccess);
	TestEqual(TEXT("the class default object is back"), UProjectSettingsService::GetSettingsProperty(Class, Key), Old);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
