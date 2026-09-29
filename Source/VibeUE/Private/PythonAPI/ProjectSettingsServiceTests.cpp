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

static const EAutomationTestFlags kSettingsTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

namespace VibeSettingsTest
{
	/**
	 * These tests write the host project's Config/DefaultGame.ini. This puts the file back byte for byte (or
	 * deletes it if it did not exist) and reloads the Game config branch, so nothing a test wrote stays on disk
	 * or in GConfig.
	 */
	struct FScopedConfigFileRestore
	{
		FString Path;
		bool bExisted = false;
		TArray<uint8> Bytes;

		explicit FScopedConfigFileRestore(const FString& InPath)
			: Path(InPath)
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
			if (GConfig && GConfig->FindBranchWithNoReload(FName(TEXT("Game")), FString()))
			{
				FConfigContext Context = FConfigContext::ForceReloadIntoGConfig();
				Context.bWriteDestIni = false;
				Context.Load(TEXT("Game"));
			}
		}
	};

	static FString DefaultGameIni()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("DefaultGame.ini"));
	}

	static FString ReadDisk(const FString& Path)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *Path);
		return Text;
	}
}

// set_ini_value must put the value in the file on disk, not only in GConfig's memory, and keep the rest of the file.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeSettingsIniWriteReachesDiskTest,
	"VibeUE.ProjectSettings.IniWriteReachesDisk", kSettingsTestFlags)
bool FVibeSettingsIniWriteReachesDiskTest::RunTest(const FString&)
{
	using namespace VibeSettingsTest;
	const FString File = DefaultGameIni();
	FScopedConfigFileRestore Restore(File);
	const FString Before = ReadDisk(File);

	const FString Section = TEXT("VibeUETests.Scratch");
	const FString Key = TEXT("WrittenByTest");
	const FString Value = FGuid::NewGuid().ToString(EGuidFormats::Digits);

	const FProjectSettingResult Result = UProjectSettingsService::SetIniValue(Section, Key, Value, TEXT("DefaultGame.ini"));
	TestTrue(TEXT("set_ini_value reports success"), Result.bSuccess);

	const FString After = ReadDisk(File);
	TestTrue(TEXT("the value is in the file on disk"), After.Contains(Key + TEXT("=") + Value));
	TArray<FString> BeforeLines;
	Before.ParseIntoArrayLines(BeforeLines);
	for (const FString& Line : BeforeLines)
	{
		if (!Line.TrimStartAndEnd().IsEmpty())
		{
			TestTrue(FString::Printf(TEXT("the file's earlier line is kept: %s"), *Line), After.Contains(Line));
		}
	}
	TestEqual(TEXT("get_ini_value reads it back"), UProjectSettingsService::GetIniValue(Section, Key, TEXT("DefaultGame.ini")), Value);
	return true;
}

// set_ini_array may refuse, but when it reports success the lines must be in the file on disk.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeSettingsIniArrayHonestTest,
	"VibeUE.ProjectSettings.IniArrayIsHonest", kSettingsTestFlags)
bool FVibeSettingsIniArrayHonestTest::RunTest(const FString&)
{
	using namespace VibeSettingsTest;
	const FString File = DefaultGameIni();
	FScopedConfigFileRestore Restore(File);

	const FString Section = TEXT("VibeUETests.Scratch");
	const FString Key = TEXT("ArrayWrittenByTest");
	const FString Marker = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FProjectSettingResult Result = UProjectSettingsService::SetIniArray(Section, Key, { Marker + TEXT("_A"), Marker + TEXT("_B") }, TEXT("DefaultGame.ini"));

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
	FScopedConfigFileRestore Restore(File);

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

#endif // WITH_AUTOMATION_TESTS
