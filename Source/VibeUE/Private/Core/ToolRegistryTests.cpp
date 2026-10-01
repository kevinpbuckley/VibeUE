// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Core/ToolRegistry.h"

#if WITH_AUTOMATION_TESTS

// VibeUE.RefreshTools calls FToolRegistry::Refresh(), which empties the registry and runs Initialize() again.
// Initialize() only re-registers what is still queued in PendingRegistrations, and the first Initialize() emptied
// that queue, so every refresh after startup left zero tools. Test path prefix VibeUE.Registry.*

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeUERegistryRefreshKeepsToolsTest, "VibeUE.Registry.RefreshKeepsTools",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeUERegistryRefreshKeepsToolsTest::RunTest(const FString& Parameters)
{
	FToolRegistry& Registry = FToolRegistry::Get();
	const int32 Before = Registry.GetAllTools().Num();
	TestTrue(TEXT("the registry has tools before the refresh"), Before > 0);

	Registry.Refresh();

	TestEqual(TEXT("the refresh keeps every tool"), Registry.GetAllTools().Num(), Before);
	TestNotNull(TEXT("execute_python_code is still registered"), Registry.FindTool(TEXT("execute_python_code")));
	return true;
}

#endif // WITH_AUTOMATION_TESTS
