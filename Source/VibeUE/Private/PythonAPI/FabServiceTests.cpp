// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Fab/FabLibraryClient.h"

#if WITH_AUTOMATION_TESTS

// Pure discovery logic (engine-version compat, type routing, version rollup) is factored onto
// FFabLibraryAsset so it can be verified headlessly, with no editor/auth/network. Test path prefix
// VibeUE.Fab.*. Live auth + library + import are exercised by test_prompts/fab/fab_tests.md.

namespace
{
	FFabProjectVersion MakePV(const TArray<FString>& Engines)
	{
		FFabProjectVersion PV;
		PV.EngineVersions = Engines;
		return PV;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeFabSupportsEngineTest, "VibeUE.Fab.Compat.SupportsEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeFabSupportsEngineTest::RunTest(const FString&)
{
	FFabLibraryAsset A;
	A.ProjectVersions.Add(MakePV({TEXT("5.7"), TEXT("5.8")}));

	TestTrue(TEXT("exact 5.8 supported"), A.SupportsEngine(TEXT("5.8")));
	TestTrue(TEXT("5.7 supported"), A.SupportsEngine(TEXT("5.7")));
	TestFalse(TEXT("5.9 not supported"), A.SupportsEngine(TEXT("5.9")));
	TestTrue(TEXT("empty query = compatible"), A.SupportsEngine(TEXT("")));

	// Lenient suffix match: a "UE_5.8" token should satisfy a "5.8" query.
	FFabLibraryAsset B;
	B.ProjectVersions.Add(MakePV({TEXT("UE_5.8")}));
	TestTrue(TEXT("UE_5.8 satisfies 5.8"), B.SupportsEngine(TEXT("5.8")));

	// No projectVersions → unknown → treated as compatible (don't hide items on sparse data).
	FFabLibraryAsset C;
	TestTrue(TEXT("no versions = compatible (unknown)"), C.SupportsEngine(TEXT("5.8")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeFabDeriveTypeTest, "VibeUE.Fab.Compat.DeriveAssetType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeFabDeriveTypeTest::RunTest(const FString&)
{
	{
		FFabLibraryAsset A; A.Source = TEXT("quixel");
		TestEqual(TEXT("quixel source"), A.DeriveAssetType(), TEXT("quixel"));
	}
	{
		FFabLibraryAsset A; A.Source = TEXT("megascans");
		TestEqual(TEXT("megascans source"), A.DeriveAssetType(), TEXT("quixel"));
	}
	{
		FFabLibraryAsset A; A.DistributionMethod = TEXT("engine_plugin");
		TestEqual(TEXT("plugin dist"), A.DeriveAssetType(), TEXT("plugin"));
	}
	{
		FFabLibraryAsset A; A.DistributionMethod = TEXT("asset_pack");
		TestEqual(TEXT("asset pack dist"), A.DeriveAssetType(), TEXT("unreal-engine"));
	}
	{
		FFabLibraryAsset A; A.DistributionMethod = TEXT("complete_project");
		TestEqual(TEXT("complete project dist"), A.DeriveAssetType(), TEXT("complete-project"));
	}
	{
		FFabLibraryAsset A;   // nothing set → generic model
		TestEqual(TEXT("default = model"), A.DeriveAssetType(), TEXT("model"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeFabAllEngineVersionsTest, "VibeUE.Fab.Compat.AllEngineVersions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeFabAllEngineVersionsTest::RunTest(const FString&)
{
	FFabLibraryAsset A;
	A.ProjectVersions.Add(MakePV({TEXT("5.7"), TEXT("5.8")}));
	A.ProjectVersions.Add(MakePV({TEXT("5.8"), TEXT("5.6")}));   // 5.8 duplicated across versions

	const TArray<FString> All = A.AllEngineVersions();
	TestEqual(TEXT("de-duplicated count"), All.Num(), 3);
	TestTrue(TEXT("has 5.6"), All.Contains(TEXT("5.6")));
	TestTrue(TEXT("has 5.7"), All.Contains(TEXT("5.7")));
	TestTrue(TEXT("has 5.8"), All.Contains(TEXT("5.8")));
	return true;
}

#endif // WITH_AUTOMATION_TESTS
