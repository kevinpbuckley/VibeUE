// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Fab/FabLibraryClient.h"
#include "Fab/FabEndpoints.h"
#include "PythonAPI/UFabService.h"
#include "Json.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeFabFreeCatalogEndpointsTest, "VibeUE.Fab.FreeCatalog.Endpoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeFabFreeCatalogEndpointsTest::RunTest(const FString&)
{
	const FString Search = VibeUE::Fab::CatalogSearchUrl(TEXT("https://www.fab.com"),
		TEXT("forest rock"), TEXT("Quixel Megascans"), TEXT("next+page"));
	TestTrue(TEXT("search is constrained to free"), Search.Contains(TEXT("is_free=1")));
	TestTrue(TEXT("query is URL encoded"), Search.Contains(TEXT("q=forest%20rock")));
	TestTrue(TEXT("seller is URL encoded"), Search.Contains(TEXT("seller=Quixel%20Megascans")));
	TestTrue(TEXT("cursor is URL encoded"), Search.Contains(TEXT("cursor=next%2Bpage")));

	const FString Download = VibeUE::Fab::CatalogDownloadInfoUrl(TEXT("https://www.fab.com"),
		TEXT("listing-id"), TEXT("gltf"), TEXT("file-id"), TEXT("Windows"));
	TestEqual(TEXT("download-info endpoint"), Download,
		TEXT("https://www.fab.com/i/listings/listing-id/asset-formats/gltf/files/file-id/download-info?platform=Windows"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeFabFreeImportEulaGuardTest, "VibeUE.Fab.FreeCatalog.EulaGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeFabFreeImportEulaGuardTest::RunTest(const FString&)
{
	const FString Response = UFabService::ImportFreeAsset(TEXT("test-listing"), TEXT("High"),
		TEXT("gltf"), TEXT("personal"), false);
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);
	TestTrue(TEXT("guard returns JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (Root.IsValid())
	{
		TestFalse(TEXT("import is refused without acceptance"), Root->GetBoolField(TEXT("success")));
		TestEqual(TEXT("guard error code"), Root->GetStringField(TEXT("error_code")),
			TEXT("EULA_ACCEPTANCE_REQUIRED"));
		TestFalse(TEXT("library remains unchanged"), Root->GetBoolField(TEXT("library_changed")));
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
