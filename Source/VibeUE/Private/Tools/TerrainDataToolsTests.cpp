// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS
#include "Tools/TerrainDataInternal.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using namespace VibeUE::Terrain;

FParams EnchantedRockTerrainParams()
{
	return {{TEXT("lng"), TEXT("-98.819936")}, {TEXT("lat"), TEXT("30.505025")}, {TEXT("map_size"), TEXT("3")}};
}

bool TerrainBodyMatches(const TSharedPtr<FJsonObject>& Actual, const FString& ExpectedText)
{
	TSharedPtr<FJsonObject> Expected;
	return Actual && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ExpectedText), Expected) &&
		FJsonValue::CompareEqual(FJsonValueObject(Actual), FJsonValueObject(Expected));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeTerrainPlacementTest, "VibeUE.Terrain.EnchantedRock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeTerrainPlacementTest::RunTest(const FString&)
{
	using namespace VibeUE::Terrain;
	const FPlacement Summit = Project(-98.818824, 30.506464, -98.819936, 30.505025, 3, 1009);
	TestEqual(TEXT("Summit fractional column"), Summit.Vertex.X, 539.835480, 0.00001);
	TestEqual(TEXT("Summit fractional row"), Summit.Vertex.Y, 450.176183, 0.00001);
	TestEqual(TEXT("Summit east in cm"), Summit.Centimetres.X, 10665.32, 0.01);
	TestEqual(TEXT("Summit south in cm"), Summit.Centimetres.Y, -16018.99, 0.01);
	const FPlacement AcrossDateline = Project(-179.999, 0, 179.999, 0, 3, 1009);
	TestEqual(TEXT("Unwrap to the east"), AcrossDateline.Centimetres.X, 22263.898, 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeTerrainTileTest, "VibeUE.Terrain.StitchTiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeTerrainTileTest::RunTest(const FString&)
{
	using namespace VibeUE::Terrain;
	// Four distinctly coloured BGRA tiles. The first channel also identifies each pixel
	// within its tile, detecting a flip inside a correctly positioned quadrant.
	for (const int32 Channels : {1, 4})
	{
		TArray<uint8> Canvas;
		Canvas.Init(0, 4 * 4 * Channels);
		for (int32 TileIndex = 0; TileIndex < 4; ++TileIndex)
		{
			TArray<uint8> Tile;
			for (int32 Pixel = 0; Pixel < 4; ++Pixel)
			{
				Tile.Add(static_cast<uint8>(16 * TileIndex + Pixel));
				if (Channels == 4)
				{
					Tile.Add(static_cast<uint8>(50 * TileIndex));
					Tile.Add(static_cast<uint8>(255 - 50 * TileIndex));
					Tile.Add(255);
				}
			}
			TestTrue(TEXT("Valid tile copied"), StitchTile(Canvas, 4, Tile, 2, Channels, TileIndex % 2, TileIndex / 2));
		}
		for (int32 Y = 0; Y < 4; ++Y)
			for (int32 X = 0; X < 4; ++X)
			{
				const int32 Quadrant = (Y / 2) * 2 + X / 2;
				const int32 Offset = (Y * 4 + X) * Channels;
				TestEqual(TEXT("NW-first sample"), int32(Canvas[Offset]), 16 * Quadrant + 2 * (Y % 2) + X % 2);
				if (Channels == 4)
				{
					TestEqual(TEXT("Green"), int32(Canvas[Offset + 1]), 50 * Quadrant);
					TestEqual(TEXT("Red"), int32(Canvas[Offset + 2]), 255 - 50 * Quadrant);
					TestEqual(TEXT("Alpha"), int32(Canvas[Offset + 3]), 255);
				}
			}
		TestFalse(TEXT("Out-of-grid tile rejected"), StitchTile(Canvas, 4, Canvas, 2, Channels, 2, 0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeTerrainRequestsTest, "VibeUE.Terrain.RequestBodies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeTerrainRequestsTest::RunTest(const FString&)
{
	using namespace VibeUE::Terrain;
	FParams Params = EnchantedRockTerrainParams();
	const FString Start = TEXT(R"({"lng":-98.819936,"lat":30.505025,"map_size":3,)");
	TestTrue(TEXT("Old water request has only location"), TerrainBodyMatches(WaterBody(Params),
		TEXT(R"({"lng":-98.819936,"lat":30.505025,"map_size":3})")));
	TestTrue(TEXT("Exact preview by default"), TerrainBodyMatches(PreviewBody(Params), Start + TEXT(R"("exact_center":true})")));
	Params.Add(TEXT("exact_center"), TEXT("false"));
	TestTrue(TEXT("Legacy preview"), TerrainBodyMatches(PreviewBody(Params), Start + TEXT(R"("exact_center":false})")));
	Params.Add(TEXT("detail"), TEXT("high"));
	Params.Add(TEXT("min_spacing_m"), TEXT("0"));
	TestTrue(TEXT("Detailed water request"), TerrainBodyMatches(WaterBody(Params), Start + TEXT(R"("detail":"high","min_spacing_m":0})")));
	Params = EnchantedRockTerrainParams();
	TestTrue(TEXT("Map omits server-selected categories and caps"), TerrainBodyMatches(MapBody(Params), Start + TEXT(R"("prefer":"auto"})")));
	Params.Add(TEXT("max_features"), TEXT("12"));
	Params.Add(TEXT("min_spacing_m"), TEXT("0"));
	Params.Add(TEXT("prefer"), TEXT("tiles"));
	const FString Map = Start + TEXT(R"("prefer":"tiles","categories":["roads","pois"],"min_spacing_m":0,"max_features":12})");
	for (const TCHAR* Selection : {TEXT("roads, pois"), TEXT(R"(["roads","pois"])")})
	{
		Params.Add(TEXT("categories"), Selection);
		TestTrue(TEXT("Comma list and array yield identical wire JSON"), TerrainBodyMatches(MapBody(Params), Map));
	}
	Params.Remove(TEXT("categories"));
	Params.Add(TEXT("ParamsJson"), TEXT(R"({"categories":["roads","pois"]})"));
	TestTrue(TEXT("Nested ParamsJson array"), TerrainBodyMatches(MapBody(Params), Map));
	Params.Add(TEXT("categories"), TEXT("[]"));
	TestTrue(TEXT("Explicit empty category array"), TerrainBodyMatches(MapBody(Params),
		Start + TEXT(R"("prefer":"tiles","categories":[],"min_spacing_m":0,"max_features":12})")));

	Params = EnchantedRockTerrainParams();
	TestTrue(TEXT("Mask manifest"), TerrainBodyMatches(MaskBody(Params, TEXT("manifest")),
		Start + TEXT(R"("resolution":1009,"layer":"manifest","prefer":"auto"})")));
	TestTrue(TEXT("Four default layers"), MaskLayers(Params) == TArray<FString>({TEXT("surface"), TEXT("landcover"), TEXT("parking"), TEXT("road_distance")}));
	Params.Add(TEXT("layers"), TEXT("parking, road_distance"));
	TestTrue(TEXT("Selected layers"), MaskLayers(Params) == TArray<FString>({TEXT("parking"), TEXT("road_distance")}));
	Params.Add(TEXT("resolution"), TEXT("4033"));
	Params.Add(TEXT("prefer"), TEXT("openstreetmap"));
	TestTrue(TEXT("Layer repeats manifest options"), TerrainBodyMatches(MaskBody(Params, TEXT("road_distance")),
		Start + TEXT(R"("resolution":4033,"layer":"road_distance","prefer":"openstreetmap"})")));

	Params = EnchantedRockTerrainParams();
	TestTrue(TEXT("Default RGB band"), OrthoBands(Params) == TArray<FString>({TEXT("rgb")}));
	TestTrue(TEXT("Ortho manifest omits tile"), TerrainBodyMatches(OrthoBody(Params, TEXT("auto"), OrthoBands(Params)[0]),
		Start + TEXT(R"("size":2048,"source":"auto","band":"rgb"})")));
	Params.Add(TEXT("band"), TEXT("both"));
	const auto Bands = OrthoBands(Params);
	if (!TestTrue(TEXT("Both expands client-side"), Bands == TArray<FString>({TEXT("rgb"), TEXT("canopy")}))) return false;
	Params.Add(TEXT("size"), TEXT("4096"));
	const FBody Template = OrthoBody(Params, TEXT("naip"), Bands[0]);
	TestTrue(TEXT("Tile copies resolved template and selects its band and index"),
		TerrainBodyMatches(TileBody(*Template, Bands[1], 1, 0), Start + TEXT(R"("size":4096,"source":"naip","band":"canopy","tile":{"col":1,"row":0}})")));
	TestFalse(TEXT("Template untouched"), Template->HasField(TEXT("tile")));

	Params = EnchantedRockTerrainParams();
	const FString Height = Start + TEXT(R"("format":"png","exact_center":true,"base_level":0,"height_scale":100,"water_depth":40,"gravity_center":0,"level_correction":0,"blur_passes":10,"blur_post_passes":2,"sharpen":true,"draw_streams":true,"stream_depth":7,"plains_height":140)");
	TestTrue(TEXT("Default exact heightmap"), TerrainBodyMatches(HeightBody(Params, false), Height + TEXT("}")));
	TestTrue(TEXT("Default vista inherits the height encoding"), TerrainBodyMatches(HeightBody(Params, true), Height + TEXT(",\"vista_factor\":8}")));
	Params.Add(TEXT("exact_center"), TEXT("false"));
	Params.Add(TEXT("vista_factor"), TEXT("2.5"));
	Params.Add(TEXT("resolution"), TEXT("1009"));
	TestFalse(TEXT("Legacy height crop"), HeightBody(Params, false)->GetBoolField(TEXT("exact_center")));
	TestTrue(TEXT("Vista always exact and preserves fractional factor"), TerrainBodyMatches(HeightBody(Params, true),
		Height + TEXT(",\"vista_factor\":2.5,\"resolution\":1009}")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeTerrainNoKeyTest, "VibeUE.Terrain.MissingKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeTerrainNoKeyTest::RunTest(const FString&)
{
	using namespace VibeUE::Terrain;
	FParams Params = EnchantedRockTerrainParams();
	for (const TCHAR* Action : {TEXT("get_map_features"), TEXT("get_feature_masks"), TEXT("get_ortho_image"), TEXT("generate_vista_heightmap")})
	{
		Params.Add(TEXT("action"), Action);
		TSharedPtr<FJsonObject> Reply;
		// Exercise dispatch and the actual pre-HTTP key guard even if the developer has a key configured.
		if (!TestTrue(Action, FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RunWithoutKey(Params)), Reply))) return false;
		TestEqual(Action, Reply->GetStringField(TEXT("error")), FString(TEXT("NO_API_KEY")));
		TestFalse(Action, Reply->GetBoolField(TEXT("success")));
	}
	return true;
}
#endif // WITH_AUTOMATION_TESTS
