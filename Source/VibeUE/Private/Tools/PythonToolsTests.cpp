// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Tools/PythonTools.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "Misc/ScopeExit.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "UObject/UObjectGlobals.h"


// A map opened as an asset stays resident, and the NEXT level load then fails the engine's
// stale-world check and FATALS the editor ("World Memory Leaks", EditorServer.cpp) - naming
// neither the script nor the tool that left it behind. GetResidentMapWorlds surfaces those
// stragglers in the reply of the run that created them.
//
// The fixture is a scratch UWorld in its own /Game package, NOT a real project map: loading one of
// those pulls in World Partition, and releasing it again trips
// `InitState == EWorldPartitionInitState::Uninitialized`. A bare NewObject<UWorld> reproduces
// exactly what the detector looks at (a non-transient package, an inactive world) with nothing to
// tear down.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonResidentMapsTest, "VibeUE.Python.ResidentMapDetection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonResidentMapsTest::RunTest(const FString&)
{
	if (!GEditor)
	{
		AddWarning(TEXT("No GEditor; skipping the resident-map test."));
		return true;
	}

	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

	// The open level must never be flagged - it is not a straggler, and reporting it would fire a
	// scary warning on every single execute_python_code call.
	if (EditorWorld)
	{
		TestFalse(TEXT("the open editor world is not reported as resident"),
			UPythonTools::GetResidentMapWorlds().Contains(EditorWorld->GetPathName()));
	}

	// A world in a real (non-transient) package, inactive, exactly like a map opened as an asset.
	UPackage* Package = CreatePackage(TEXT("/Game/__VibeUETest/W_ResidentProbe"));
	if (!TestNotNull(TEXT("created the scratch package"), Package))
	{
		return false;
	}
	UWorld* Probe = NewObject<UWorld>(Package, TEXT("W_ResidentProbe"), RF_Public | RF_Standalone);
	if (!TestNotNull(TEXT("created the scratch world"), Probe))
	{
		return false;
	}
	Probe->WorldType = EWorldType::Inactive;
	const FString ProbePath = Probe->GetPathName();

	ON_SCOPE_EXIT
	{
		if (UWorld* Leftover = FindObject<UWorld>(Package, TEXT("W_ResidentProbe")))
		{
			Leftover->ClearFlags(RF_Public | RF_Standalone);
			Leftover->MarkAsGarbage();
		}
	};

	// It must be reported while it is alive, and the open level still must not be.
	const TArray<FString> During = UPythonTools::GetResidentMapWorlds();
	TestTrue(*FString::Printf(TEXT("the resident world %s is reported"), *ProbePath),
		During.Contains(ProbePath));
	if (EditorWorld)
	{
		TestFalse(TEXT("the open editor world is still not reported"),
			During.Contains(EditorWorld->GetPathName()));
	}

	// Once released it must stop being reported, so the warning clears instead of sticking forever.
	Probe->ClearFlags(RF_Public | RF_Standalone);
	Probe->MarkAsGarbage();
	Probe = nullptr;

	TestFalse(*FString::Printf(TEXT("%s is no longer reported once released"), *ProbePath),
		UPythonTools::GetResidentMapWorlds().Contains(ProbePath));

	return true;
}
#endif // WITH_AUTOMATION_TESTS
