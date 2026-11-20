// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Tools/PythonTools.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace VibePythonHostReleaseTest
{
	/** What Python prints for host_releases_resident_maps(): the reply's output, or the reply itself when it has none */
	FString PythonSays()
	{
		const FString Reply = UPythonTools::ExecutePythonCode(
			TEXT("import unreal\nprint('HostReleases', unreal.PythonTools.host_releases_resident_maps())\n"),
			/*bAutoSave=*/false, /*bFreshNamespace=*/true);
		TSharedPtr<FJsonObject> Json;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Reply), Json);
		return Json.IsValid() && Json->HasField(TEXT("output")) ? Json->GetStringField(TEXT("output")) : Reply;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonHostReleasesResidentMapsTest, "VibeUE.Python.HostReleasesResidentMaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonHostReleasesResidentMapsTest::RunTest(const FString&)
{
	using namespace VibePythonHostReleaseTest;

	// A host editor that releases maps loaded as assets before every map load says so through this variable. VibeUE
	// owns it, off by default, so a host that does nothing keeps safe_load_level's refusals and the RESIDENT_MAPS warning.
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("VibeUE.HostReleasesResidentMaps"));
	if (!TestNotNull(TEXT("VibeUE registers the console variable VibeUE.HostReleasesResidentMaps"), Variable))
	{
		return false;
	}

	// Set as the console does: a lower priority than a console command run earlier in this editor (the test prompt's)
	// is ignored, and the test would read that value instead of its own.
	const bool bWas = Variable->GetBool();

	Variable->Set(false, ECVF_SetByConsole);
	const FString Off = PythonSays();
	TestTrue(FString::Printf(TEXT("with the variable off, unreal.PythonTools.host_releases_resident_maps() is False: %s"), *Off),
		Off.Contains(TEXT("HostReleases False")));

	Variable->Set(true, ECVF_SetByConsole);
	const FString On = PythonSays();
	TestTrue(FString::Printf(TEXT("with the variable on, it is True: %s"), *On), On.Contains(TEXT("HostReleases True")));

	Variable->Set(bWas, ECVF_SetByConsole);
	return true;
}

#endif
