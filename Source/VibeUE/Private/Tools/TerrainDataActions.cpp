// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Tools/TerrainDataInternal.h"

#include "Core/VibeUEToolCancel.h"
#include "Containers/StringConv.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/UnrealMemory.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Logging/LogMacros.h"
#include "HAL/CriticalSection.h"
#include "Math/Color.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/CString.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>

DEFINE_LOG_CATEGORY_STATIC(LogVibeTerrain, Log, All);

namespace VibeUE::Terrain
{
using FValues = TArray<TSharedPtr<FJsonValue>>;

FBody LocationBody(const FParams& Params)
{
	FBody Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("lng"), Number(Params, TEXT("lng"), 0));
	Body->SetNumberField(TEXT("lat"), Number(Params, TEXT("lat"), 0));
	Body->SetNumberField(TEXT("map_size"), Number(Params, TEXT("map_size"), 17.28));
	return Body;
}

FBody PreviewBody(const FParams& Params)
{
	FBody Body = LocationBody(Params);
	Body->SetBoolField(TEXT("exact_center"), Boolean(Params, TEXT("exact_center"), true));
	return Body;
}

FBody WaterBody(const FParams& Params)
{
	FBody Body = LocationBody(Params);
	if (!Param(Params, TEXT("min_spacing_m")).IsEmpty())
		Body->SetNumberField(TEXT("min_spacing_m"), Number(Params, TEXT("min_spacing_m"), 50));
	const FString Detail = Param(Params, TEXT("detail"));
	if (!Detail.IsEmpty()) Body->SetStringField(TEXT("detail"), Detail);
	return Body;
}

FBody HeightBody(const FParams& Params, bool bVista)
{
	FBody Body = LocationBody(Params);
	Body->SetStringField(TEXT("format"), Param(Params, TEXT("format"), TEXT("png")));
	Body->SetBoolField(TEXT("exact_center"), bVista || Boolean(Params, TEXT("exact_center"), true));
	Body->SetNumberField(TEXT("base_level"), Number(Params, TEXT("base_level"), 0));
	Body->SetNumberField(TEXT("height_scale"), Number(Params, TEXT("height_scale"), 100));
	Body->SetNumberField(TEXT("water_depth"), Number(Params, TEXT("water_depth"), 40));
	Body->SetNumberField(TEXT("gravity_center"), Integer(Params, TEXT("gravity_center"), 0));
	Body->SetNumberField(TEXT("level_correction"), Integer(Params, TEXT("level_correction"), 0));
	Body->SetNumberField(TEXT("blur_passes"), Integer(Params, TEXT("blur_passes"), 10));
	Body->SetNumberField(TEXT("blur_post_passes"), Integer(Params, TEXT("blur_post_passes"), 2));
	Body->SetBoolField(TEXT("sharpen"), Boolean(Params, TEXT("sharpen"), true));
	Body->SetBoolField(TEXT("draw_streams"), Boolean(Params, TEXT("draw_streams"), true));
	Body->SetNumberField(TEXT("stream_depth"), Number(Params, TEXT("stream_depth"), 7));
	Body->SetNumberField(TEXT("plains_height"), Number(Params, TEXT("plains_height"), 140));
	const int32 Resolution = Integer(Params, TEXT("resolution"), 0);
	if (Resolution > 0) Body->SetNumberField(TEXT("resolution"), Resolution);
	if (bVista) Body->SetNumberField(TEXT("vista_factor"), Number(Params, TEXT("vista_factor"), 8));
	return Body;
}

static TArray<FString> TerrainCommaList(const FString& Text)
{
	TArray<FString> Names;
	Text.ParseIntoArray(Names, TEXT(","), true);
	for (FString& Name : Names) Name.TrimStartAndEndInline();
	return Names;
}

TSharedPtr<FJsonObject> MapBody(const FParams& Params)
{
	FBody Body = LocationBody(Params);
	Body->SetStringField(TEXT("prefer"), Param(Params, TEXT("prefer"), TEXT("auto")));
	const FString Categories = Param(Params, TEXT("categories")).TrimStartAndEnd();
	if (!Categories.IsEmpty())
	{
		FValues Values;
		if (Categories.StartsWith(TEXT("[")))
		{
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Categories), Values)) return nullptr;
			for (const auto& Value : Values)
				if (!Value || Value->Type != EJson::String) return nullptr;
		}
		else
		{
			for (const FString& Name : TerrainCommaList(Categories))
				Values.Add(MakeShared<FJsonValueString>(Name));
		}
		Body->SetArrayField(TEXT("categories"), MoveTemp(Values));
	}
	for (const TCHAR* Field : {TEXT("min_spacing_m"), TEXT("max_features")})
	{
		if (!Param(Params, Field).IsEmpty()) Body->SetNumberField(Field, Number(Params, Field, 0));
	}
	return Body;
}

TArray<FString> MaskLayers(const FParams& Params)
{
	return TerrainCommaList(Param(Params, TEXT("layers"), TEXT("surface,landcover,parking,road_distance")));
}

FBody MaskBody(const FParams& Params, const FString& Layer)
{
	FBody Body = LocationBody(Params);
	Body->SetNumberField(TEXT("resolution"), Integer(Params, TEXT("resolution"), 1009));
	Body->SetStringField(TEXT("layer"), Layer);
	Body->SetStringField(TEXT("prefer"), Param(Params, TEXT("prefer"), TEXT("auto")));
	return Body;
}

TArray<FString> OrthoBands(const FParams& Params)
{
	const FString Band = Param(Params, TEXT("band"), TEXT("rgb"));
	if (Band == TEXT("both")) return {TEXT("rgb"), TEXT("canopy")};
	return {Band};
}

FBody OrthoBody(const FParams& Params, const FString& Source, const FString& Band)
{
	FBody Body = LocationBody(Params);
	Body->SetNumberField(TEXT("size"), Integer(Params, TEXT("size"), 2048));
	Body->SetStringField(TEXT("source"), Source);
	Body->SetStringField(TEXT("band"), Band);
	return Body;
}

FBody TileBody(const FJsonObject& Template, const FString& Band, int32 Col, int32 Row)
{
	FBody Body = MakeShared<FJsonObject>(Template);
	FBody Tile = MakeShared<FJsonObject>();
	Tile->SetNumberField(TEXT("col"), Col);
	Tile->SetNumberField(TEXT("row"), Row);
	Body->SetObjectField(TEXT("tile"), Tile);
	Body->SetStringField(TEXT("band"), Band);
	return Body;
}

FPlacement Project(double Lng, double Lat, double CenterLng, double CenterLat, double SideKm, int32 Samples)
{
	constexpr double Radius = 6378137.0;
	const double CenterRad = FMath::DegreesToRadians(CenterLat);
	const double GroundSide = SideKm * 1000.0;
	const double ProjectedSide = GroundSide / std::cos(CenterRad);
	const double East = Radius * FMath::DegreesToRadians(std::remainder(Lng - CenterLng, 360.0));
	const double North = Radius * (std::asinh(std::tan(FMath::DegreesToRadians(Lat))) - std::asinh(std::tan(CenterRad)));
	const double Intervals = Samples - 1;
	const FVector2D Vertex((0.5 + East / ProjectedSide) * Intervals, (0.5 - North / ProjectedSide) * Intervals);
	const FVector2D Centre(Intervals / 2, Intervals / 2);
	return {Vertex, (Vertex - Centre) * (100.0 * GroundSide / Intervals)};
}

void AddCoordinates(FJsonObject& Feature, double CenterLng, double CenterLat, double SideKm)
{
	auto ConvertPoint = [=](const TSharedPtr<FJsonValue>& Value) -> TSharedPtr<FJsonValue>
	{
		const TSharedPtr<FJsonObject>* Point;
		double Lng, Lat;
		if (!Value || !Value->TryGetObject(Point) || !(*Point)->TryGetNumberField(TEXT("lng"), Lng) ||
			!(*Point)->TryGetNumberField(TEXT("lat"), Lat)) return nullptr;
		// The sample count cancels in centimetres. Only fractional vertex indices depend on N.
		const FVector2D Position = Project(Lng, Lat, CenterLng, CenterLat, SideKm, 1009).Centimetres;
		FBody World = MakeShared<FJsonObject>();
		World->SetNumberField(TEXT("x"), Position.X);
		World->SetNumberField(TEXT("y"), Position.Y);
		World->SetNumberField(TEXT("z"), 0.0);
		return MakeShared<FJsonValueObject>(World);
	};
	auto ConvertLine = [&ConvertPoint](const FValues& Points)
	{
		FValues Line;
		Line.Reserve(Points.Num());
		for (const auto& Value : Points)
			if (auto Converted = ConvertPoint(Value)) Line.Add(MoveTemp(Converted));
		return Line;
	};

	if (auto Point = ConvertPoint(Feature.TryGetField(TEXT("point"))))
		Feature.SetField(TEXT("ue5_point"), Point);
	const FValues* Points;
	if (Feature.TryGetArrayField(TEXT("points"), Points))
		Feature.SetArrayField(TEXT("ue5_points"), ConvertLine(*Points));
	const FValues* Rings;
	if (Feature.TryGetArrayField(TEXT("rings"), Rings))
	{
		FValues WorldRings;
		for (const auto& Ring : *Rings)
			if (Ring->TryGetArray(Points)) WorldRings.Add(MakeShared<FJsonValueArray>(ConvertLine(*Points)));
		Feature.SetArrayField(TEXT("ue5_rings"), MoveTemp(WorldRings));
	}
}

static TSharedPtr<FJsonObject> ParseTerrainJson(const FString& Text)
{
	TSharedPtr<FJsonObject> Json;
	// Keep numeric tokens (including 64-bit osm_id) lossless when reserializing the response.
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json, FJsonSerializer::EFlags::StoreNumbersAsStrings);
	return Json;
}

struct FTerrainReply
{
	FTerrainHttpResult Http;
	TSharedPtr<FJsonObject> Json;
	FString Failure;
	double RetryAfter = 2;
};

static FTerrainReply RequestTerrain(const TCHAR* Path, const FBody& Body, const FString& ExpectedType)
{
	FTerrainReply Reply;
	Reply.Http = TerrainHttpPost(Path, Serialize(Body), 60.0f, 0.0f);
	const FTerrainHttpResult& Http = Reply.Http;
	if (!Http.bSuccess)
	{
		Reply.Failure = Error(Http.bNoApiKey ? TEXT("NO_API_KEY") : TEXT("HTTP_ERROR"), Http.ErrorMessage);
		return Reply;
	}
	// What a failed request said, for the message when the body is not the contract's {"error": "..."} (a gateway's
	// own 502 / 504 page, for example): the start of the body as text.
	FString ErrorBody;
	if (Http.ResponseCode != 200 && Http.Content.Num())
	{
		const FUTF8ToTCHAR BodyText(reinterpret_cast<const ANSICHAR*>(Http.Content.GetData()), FMath::Min(Http.Content.Num(), 300));
		ErrorBody = FString::ConstructFromPtrSize(BodyText.Get(), BodyText.Length()).Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
	}
	if (Http.HasContentType(TEXT("application/json")) && Http.Content.Num())
	{
		Reply.Http.Content.Add(0);
		Reply.Json = ParseTerrainJson(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Http.Content.GetData())));
		Reply.Http.Content.Empty();
	}
	if (Http.ResponseCode != 200)
	{
		FString Message = FString::Printf(TEXT("Server returned %d"), Http.ResponseCode);
		// The server answers an upstream failure or an exhausted time budget with 503 and JSON (error, code, upstream,
		// retry_after_s). A 502 / 504 without JSON is the edge proxy's own page: the contract says wait 5 s and try again.
		Reply.RetryAfter = (Http.ResponseCode == 502 || Http.ResponseCode == 504) ? 5 : 2;
		if (!Reply.Json || !Reply.Json->HasTypedField<EJson::String>(TEXT("error")))
		{
			if (Http.ResponseCode == 502 || Http.ResponseCode == 503 || Http.ResponseCode == 504)
				Message += TEXT(": the gateway in front of the terrain server answered without detail (an upstream source failed or ran out of time). Try again in a few seconds");
			else if (!ErrorBody.IsEmpty() && !ErrorBody.StartsWith(TEXT("<")))
				Message += TEXT(": ") + ErrorBody;
		}
		if (Reply.Json)
		{
			Reply.Json->TryGetStringField(TEXT("error"), Message);
			Reply.Json->TryGetNumberField(TEXT("retry_after_s"), Reply.RetryAfter);
			Reply.RetryAfter = FMath::Clamp(Reply.RetryAfter, 1.0, 30.0);
		}
		FBody Failure = MakeShared<FJsonObject>();
		Failure->SetBoolField(TEXT("success"), false);
		Failure->SetStringField(TEXT("error"), FString::Printf(TEXT("HTTP_%d"), Http.ResponseCode));
		Failure->SetStringField(TEXT("message"), Message);
		if (Reply.Json)
			for (const TCHAR* Field : {TEXT("code"), TEXT("upstream"), TEXT("upstream_status"), TEXT("retry_after_s"), TEXT("attribution")})
				if (const auto Value = Reply.Json->TryGetField(Field)) Failure->SetField(Field, Value);
		Reply.Failure = Serialize(Failure);
	}
	else if (!Http.HasContentType(ExpectedType) ||
		(ExpectedType == TEXT("application/json") ? !Reply.Json.IsValid() : Http.Content.IsEmpty()))
	{
		Reply.Failure = Error(TEXT("INVALID_RESPONSE"), FString::Printf(TEXT("Expected %s from %s."), *ExpectedType, Path));
	}
	return Reply;
}

static FTerrainReply RequestTerrainUnit(const TCHAR* Route, const FBody& Body, const FString& ExpectedType, const FString& What);

static void CopyTerrainField(FJsonObject& To, const FJsonObject& From, const TCHAR* Field)
{
	if (const auto Value = From.TryGetField(Field)) To.SetField(Field, Value);
}

static FString TerrainStem(const FString& Prefix, const FJsonObject& Body)
{
	return FString::Printf(TEXT("%s_%.4f_%.4f_%gkm"), *Prefix, Body.GetNumberField(TEXT("lat")),
		Body.GetNumberField(TEXT("lng")), Body.GetNumberField(TEXT("map_size")));
}

// A multi-file result belongs to one invocation even when two clients request the same place.
// Only remove files named with this invocation's unique stem on a failed/cancelled download.
struct FTerrainOutputSet
{
	FString Stem;
	TArray<FString> Created;
	bool bComplete = false;

	FTerrainOutputSet(const TCHAR* Prefix, const FJsonObject& Body)
		: Stem(TerrainStem(Prefix, Body) + TEXT("_") + FGuid::NewGuid().ToString()) {}
	~FTerrainOutputSet()
	{
		if (!bComplete)
			for (const FString& File : Created) IFileManager::Get().Delete(*File);
	}
	FString Path(const FString& Suffix)
	{
		FString File = SavePath(FString(), Stem + Suffix);
		Created.Add(File);
		return File;
	}
};

static FString TerrainSaveError(const FString& Path)
{
	return Error(TEXT("SAVE_ERROR"), FString::Printf(TEXT("Could not write %s."), *Path));
}

FString GetMapFeatures(const FParams& Params)
{
	const TSharedPtr<FJsonObject> Body = MapBody(Params);
	if (!Body) return Error(TEXT("INVALID_PARAMS"), TEXT("categories must be a comma list or JSON array of strings."));
	const FTerrainReply Reply = RequestTerrain(TEXT("/api/terrain/map-features"), Body.ToSharedRef(), TEXT("application/json"));
	if (!Reply.Failure.IsEmpty()) return Reply.Failure;
	const TSharedPtr<FJsonObject>* Features;
	if (!Reply.Json->TryGetObjectField(TEXT("features"), Features))
		return Error(TEXT("INVALID_RESPONSE"), TEXT("Map response is missing features."));
	FBody Names = MakeShared<FJsonObject>();
	const double CenterLng = Body->GetNumberField(TEXT("lng"));
	const double CenterLat = Body->GetNumberField(TEXT("lat"));
	const double SideKm = FMath::Max(1.0, Body->GetNumberField(TEXT("map_size")));
	for (const auto& Category : (*Features)->Values)
	{
		const FValues* Items;
		if (!Category.Value->TryGetArray(Items))
			return Error(TEXT("INVALID_RESPONSE"), TEXT("Invalid map feature category."));
		// The first five DISTINCT names: the tile source returns one road as several fragments of the same name.
		FValues Examples;
		TSet<FString> SeenNames;
		for (const auto& Item : *Items)
		{
			const TSharedPtr<FJsonObject>* Feature;
			if (!Item->TryGetObject(Feature)) return Error(TEXT("INVALID_RESPONSE"), TEXT("Invalid map feature."));
			AddCoordinates(**Feature, CenterLng, CenterLat, SideKm);
			FString Name;
			if (Examples.Num() < 5 && (*Feature)->HasTypedField<EJson::String>(TEXT("name")) &&
				(*Feature)->TryGetStringField(TEXT("name"), Name) && !Name.IsEmpty() && !SeenNames.Contains(Name))
			{
				SeenNames.Add(Name);
				Examples.Add(MakeShared<FJsonValueString>(Name));
			}
		}
		Names->SetArrayField(*Category.Key, MoveTemp(Examples));
	}
	const FString Path = SavePath(Param(Params, TEXT("save_path")), TerrainStem(TEXT("mapfeatures"), *Body) + TEXT(".json"));
	if (!SaveText(Serialize(Reply.Json.ToSharedRef()), Path)) return TerrainSaveError(Path);
	FBody Summary = MakeShared<FJsonObject>();
	// Result field names are the ones the UE 5.9 line returns (first_names, manifest_file, metres_per_pixel,
	// uv_formula ...), so a prompt or script written against one line reads the other's replies.
	for (const TCHAR* Field : {TEXT("counts"), TEXT("source"), TEXT("notes"), TEXT("attribution"), TEXT("license"), TEXT("truncated"), TEXT("alignment")})
		CopyTerrainField(*Summary, *Reply.Json, Field);
	Summary->SetBoolField(TEXT("success"), true);
	Summary->SetStringField(TEXT("file"), Path);
	Summary->SetObjectField(TEXT("first_names"), Names);
	Summary->SetStringField(TEXT("ue5_coordinate_note"), TEXT("In the file each feature has ue5_point, ue5_points or ue5_rings: origin-centred centimetres, +X east, +Y south (north is negative Y), z 0. Add the landscape actor's location if it is not at the origin."));
	return Serialize(Summary);
}

static constexpr const TCHAR* TerrainTextureUV =
	TEXT("For landscape vertex (i,j), N vertices and actual texture side M, UV = ")
	TEXT("((0.5+i*(M-1)/(N-1))/M, (0.5+j*(M-1)/(N-1))/M). Row zero is north (minimum Y).");

// Verify the PNG IHDR without expanding or colour-converting independent mask channels.
static bool TerrainMaskMatches(const TArray<uint8>& Png, const FString& Layer, int32 Side)
{
	const uint8 Prefix[] = {137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
	if (Png.Num() < 33 || FMemory::Memcmp(Png.GetData(), Prefix, sizeof(Prefix)) != 0) return false;
	auto Dimension = [&Png](int32 Index)
	{
		return (uint32(Png[Index]) << 24) | (uint32(Png[Index + 1]) << 16) |
			(uint32(Png[Index + 2]) << 8) | uint32(Png[Index + 3]);
	};
	const bool bDistance = Layer == TEXT("road_distance");
	const bool bGray = bDistance || Layer == TEXT("parking");
	return Dimension(16) == uint32(Side) && Dimension(20) == uint32(Side) &&
		Png[24] == (bDistance ? 16 : 8) && Png[25] == (bGray ? 0 : 6);
}

FString GetFeatureMasks(const FParams& Params)
{
	const TArray<FString> Layers = MaskLayers(Params);
	const TArray<FString> Supported = {TEXT("surface"), TEXT("landcover"), TEXT("parking"), TEXT("road_distance")};
	if (Layers.IsEmpty()) return Error(TEXT("INVALID_PARAMS"), TEXT("Select at least one mask layer."));
	for (const FString& Layer : Layers)
		if (!Supported.Contains(Layer)) return Error(TEXT("INVALID_PARAMS"), TEXT("Unknown mask layer: ") + Layer);

	const TCHAR* Route = TEXT("/api/terrain/feature-masks");
	const FBody ManifestBody = MaskBody(Params, TEXT("manifest"));
	const FTerrainReply Manifest = RequestTerrain(Route, ManifestBody, TEXT("application/json"));
	if (!Manifest.Failure.IsEmpty()) return Manifest.Failure;
	const TSharedPtr<FJsonObject>* Channels;
	if (!Manifest.Json->TryGetObjectField(TEXT("layers"), Channels))
		return Error(TEXT("INVALID_RESPONSE"), TEXT("Mask manifest has no channel meanings."));
	FTerrainOutputSet Output(TEXT("masks"), *ManifestBody);
	FBody Paths = MakeShared<FJsonObject>();
	FBody Resolutions = MakeShared<FJsonObject>();
	for (const FString& Layer : Layers)
	{
		if (Paths->HasField(Layer)) continue;
		const FTerrainReply Raster = RequestTerrainUnit(Route, MaskBody(Params, Layer), TEXT("image/png"), TEXT("Mask layer ") + Layer);
		if (!Raster.Failure.IsEmpty()) return Raster.Failure;
		const int32 Side = FCString::Atoi(*Raster.Http.Headers.FindRef(TEXT("X-Mask-Resolution")));
		const FString LayerSource = Raster.Http.Headers.FindRef(TEXT("X-Terrain-Source"));
		if (Side < 33 || Side > 4033 || !TerrainMaskMatches(Raster.Http.Content, Layer, Side) ||
			LayerSource != Manifest.Json->GetStringField(TEXT("source")))
			return Error(TEXT("INVALID_RESPONSE"), TEXT("Mask PNG format, resolution or source is inconsistent with its manifest."));
		const FString Path = Output.Path(TEXT("_") + Layer + TEXT(".png"));
		if (!SaveBytes(Raster.Http.Content, Path)) return TerrainSaveError(Path);
		Paths->SetStringField(Layer, Path);
		Resolutions->SetNumberField(Layer, Side);
	}
	const FString ManifestPath = Output.Path(TEXT("_manifest.json"));
	if (!SaveText(Serialize(Manifest.Json.ToSharedRef()), ManifestPath)) return TerrainSaveError(ManifestPath);
	Output.bComplete = true;
	FBody Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("manifest_file"), ManifestPath);
	Result->SetObjectField(TEXT("files"), Paths);
	Result->SetObjectField(TEXT("resolutions"), Resolutions);
	Result->SetObjectField(TEXT("layers"), *Channels);
	for (const TCHAR* Field : {TEXT("source"), TEXT("counts"), TEXT("notes"), TEXT("truncated"), TEXT("attribution"), TEXT("license")})
		CopyTerrainField(*Result, *Manifest.Json, Field);
	Result->SetStringField(TEXT("import_hints"), TEXT("Import linear with sRGB off and lossless compression. Preserve RGB at zero alpha. Keep road_distance at 16 bits (centimetres)."));
	Result->SetStringField(TEXT("uv_formula"), TerrainTextureUV);
	return Serialize(Result);
}

// North-first row-major copy. Validate dimensions before multiplying or indexing a server-supplied tile.
bool StitchTile(TArrayView64<uint8> Canvas, int32 Side, TArrayView64<const uint8> Tile, int32 TileSide,
	int32 BytesPerPixel, int32 Col, int32 Row)
{
	if (Side < 1 || Side > 8192 || TileSide < 1 || TileSide > Side || Side % TileSide ||
		(BytesPerPixel != 1 && BytesPerPixel != 4) || Col < 0 || Row < 0 ||
		Col >= Side / TileSide || Row >= Side / TileSide ||
		Canvas.Num() != int64(Side) * Side * BytesPerPixel || Tile.Num() != int64(TileSide) * TileSide * BytesPerPixel)
		return false;
	const int32 RowBytes = TileSide * BytesPerPixel;
	for (int32 Y = 0; Y < TileSide; ++Y)
	{
		const int32 Offset = ((Row * TileSide + Y) * Side + Col * TileSide) * BytesPerPixel;
		FMemory::Memcpy(Canvas.GetData() + Offset, Tile.GetData() + Y * RowBytes, RowBytes);
	}
	return true;
}

static constexpr const TCHAR* TerrainOrthoRoute = TEXT("/api/terrain/ortho-image");

static bool WaitTerrainRetry(double Seconds)
{
	const double Deadline = FPlatformTime::Seconds() + FMath::Max(0.0, Seconds);
	while (FPlatformTime::Seconds() < Deadline)
	{
		const FVibeUEToolCancel* Cancel = FVibeUEToolCancel::GetCurrent();
		if (Cancel && Cancel->IsCancelled()) return false;
		if (IsInGameThread()) FHttpModule::Get().GetHttpManager().Tick(0.0f);
		FPlatformProcess::Sleep(0.01f);
	}
	return true;
}

// 503 is the server's "an upstream failed or the time ran out" (JSON, retry_after_s); 502 / 504 the edge proxy's.
static bool IsTerrainUpstreamFailure(int32 Status)
{
	return Status == 502 || Status == 503 || Status == 504;
}

// One unit of a manifest (an ortho tile, a mask layer), asked up to three times. Worth another try: an upstream
// failure status, and a request that never got an answer (the connection dropped, or the wait ran out while the
// upstream was still rendering). Never a cancelled call or a missing key.
static FTerrainReply RequestTerrainUnit(const TCHAR* Route, const FBody& Body, const FString& ExpectedType, const FString& What)
{
	for (int32 Attempt = 1; ; ++Attempt)
	{
		UE_LOG(LogVibeTerrain, Log, TEXT("%s, attempt %d/3"), *What, Attempt);
		FTerrainReply Unit = RequestTerrain(Route, Body, ExpectedType);
		const FVibeUEToolCancel* CancelNow = FVibeUEToolCancel::GetCurrent();
		const bool bNoAnswer = !Unit.Http.bSuccess && !Unit.Http.bNoApiKey && !(CancelNow && CancelNow->IsCancelled());
		if (Attempt == 3 || (!bNoAnswer && !IsTerrainUpstreamFailure(Unit.Http.ResponseCode))) return Unit;
		if (bNoAnswer) Unit.RetryAfter = 5;
		UE_LOG(LogVibeTerrain, Log, TEXT("%s: %s. Trying again in %.0f s"), *What,
			bNoAnswer ? *Unit.Http.ErrorMessage : *FString::Printf(TEXT("HTTP %d"), Unit.Http.ResponseCode), Unit.RetryAfter);
		if (!WaitTerrainRetry(Unit.RetryAfter))
		{
			Unit.Http.ResponseCode = 0;
			Unit.Failure = Error(TEXT("HTTP_ERROR"), TEXT("Request cancelled"));
			return Unit;
		}
	}
}

static FTerrainReply FetchTerrainTile(const FBody& Body, const FString& Band, int32 Index, int32 Count)
{
	return RequestTerrainUnit(TerrainOrthoRoute, Body, Band == TEXT("rgb") ? TEXT("image/jpeg") : TEXT("image/png"),
		FString::Printf(TEXT("Ortho %s tile %d/%d"), *Band, Index, Count));
}

struct FTerrainOrthoBand
{
	TArray64<uint8> Encoded;
	FString Failure;
	bool bUpstreamExhausted = false;
};

static FTerrainOrthoBand StitchTerrainBand(const FJsonObject& Template, const FString& Band, int32 Side, int32 TileSide)
{
	FTerrainOrthoBand Result;
	const bool bRgb = Band == TEXT("rgb");
	const int32 BytesPerPixel = bRgb ? 4 : 1;
	const int64 ByteCount = int64(Side) * Side * BytesPerPixel;
	// SystemMalloc wraps CRT malloc and can return null. UE's normal array allocator treats OOM as fatal.
	uint8* Pixels = static_cast<uint8*>(FMemory::SystemMalloc(ByteCount));
	if (!Pixels)
	{
		Result.Failure = Error(TEXT("ALLOCATION_ERROR"), FString::Printf(TEXT("Could not allocate %lld bytes for the %dx%d %s image. Try a smaller size."), ByteCount, Side, Side, *Band));
		return Result;
	}
	ON_SCOPE_EXIT { FMemory::SystemFree(Pixels); };
	const int32 Across = Side / TileSide;
	const ERawImageFormat::Type Format = bRgb ? ERawImageFormat::BGRA8 : ERawImageFormat::G8;
	for (int32 Row = 0; Row < Across; ++Row)
	{
		for (int32 Col = 0; Col < Across; ++Col)
		{
			const FTerrainReply Tile = FetchTerrainTile(TileBody(Template, Band, Col, Row), Band, Row * Across + Col + 1, Across * Across);
			if (!Tile.Failure.IsEmpty())
			{
				Result.Failure = Tile.Failure;
				Result.bUpstreamExhausted = IsTerrainUpstreamFailure(Tile.Http.ResponseCode);
				return Result;
			}
			if (Tile.Http.Headers.FindRef(TEXT("X-Ortho-Source")) != Template.GetStringField(TEXT("source")))
			{
				Result.Failure = Error(TEXT("INVALID_RESPONSE"), TEXT("Ortho tile source differs from the manifest."));
				return Result;
			}
			FImage Decoded;
			if (!FImageUtils::DecompressImage(Tile.Http.Content.GetData(), Tile.Http.Content.Num(), Decoded) ||
				Decoded.Format != Format || Decoded.SizeX != TileSide || Decoded.SizeY != TileSide || Decoded.NumSlices != 1 ||
				!StitchTile({Pixels, ByteCount}, Side, Decoded.RawData, TileSide, BytesPerPixel, Col, Row))
			{
				Result.Failure = Error(TEXT("INVALID_RESPONSE"), FString::Printf(TEXT("Invalid %s tile (%d,%d)."), *Band, Col, Row));
				return Result;
			}
		}
	}
	const FImageView View(Pixels, Side, Side, 1, Format, bRgb ? EGammaSpace::sRGB : EGammaSpace::Linear);
	if (!FImageUtils::CompressImage(Result.Encoded, bRgb ? TEXT("jpg") : TEXT("png"), View, bRgb ? 92 : 0) || Result.Encoded.IsEmpty())
		Result.Failure = Error(TEXT("ENCODE_ERROR"), TEXT("Could not encode the stitched ortho image."));
	return Result;
}

FString GetOrthoImage(const FParams& Params)
{
	const TArray<FString> Bands = OrthoBands(Params);
	const FString RequestedSource = Param(Params, TEXT("source"), TEXT("auto"));
	FString Source = RequestedSource;
	const int32 Side = Integer(Params, TEXT("size"), 2048);
	if (Side != 1024 && Side != 2048 && Side != 4096 && Side != 8192)
		return Error(TEXT("INVALID_PARAMS"), TEXT("size must be 1024, 2048, 4096 or 8192."));
	for (const FString& Band : Bands)
		if (Band != TEXT("rgb") && Band != TEXT("canopy"))
			return Error(TEXT("INVALID_PARAMS"), TEXT("band must be rgb, canopy or both."));

	for (;;)
	{
		const FBody Body = OrthoBody(Params, Source, Bands[0]);
		const FTerrainReply Manifest = RequestTerrain(TerrainOrthoRoute, Body, TEXT("application/json"));
		if (!Manifest.Failure.IsEmpty()) return Manifest.Failure;
		const FJsonObject& Info = *Manifest.Json;
		const TSharedPtr<FJsonObject>* Grid = nullptr;
		const TSharedPtr<FJsonObject>* Request = nullptr;
		const TSharedPtr<FJsonObject>* Template = nullptr;
		FString Resolved;
		TArray<FString> Available;
		const int32 TileSide = FMath::Min(Side, 2048);
		if (!Info.TryGetObjectField(TEXT("grid"), Grid) || !Info.TryGetObjectField(TEXT("tile_request"), Request) ||
			!(*Request)->TryGetObjectField(TEXT("body"), Template) || !Info.TryGetStringField(TEXT("source"), Resolved) ||
			(Resolved != TEXT("naip") && Resolved != TEXT("mapbox")) || (Source != TEXT("auto") && Source != Resolved) ||
			(*Template)->GetStringField(TEXT("source")) != Resolved || Info.GetIntegerField(TEXT("size")) != Side ||
			(*Grid)->GetIntegerField(TEXT("tile_size_px")) != TileSide || (*Grid)->GetIntegerField(TEXT("cols")) != Side / TileSide ||
			(*Grid)->GetIntegerField(TEXT("rows")) != Side / TileSide || !Info.TryGetStringArrayField(TEXT("available_bands"), Available))
			return Error(TEXT("INVALID_RESPONSE"), TEXT("Invalid orthophoto manifest grid, source or tile request."));
		for (const FString& Band : Bands)
			if (!Available.Contains(Band))
				return Error(TEXT("BAND_UNAVAILABLE"), FString::Printf(TEXT("%s is unavailable from %s. Canopy requires NAIP coverage."), *Band, *Resolved));

		FTerrainOutputSet Output(TEXT("ortho"), *Body);
		FBody Files = MakeShared<FJsonObject>();
		bool bRestart = false;
		for (const FString& Band : Bands)
		{
			const FTerrainOrthoBand Image = StitchTerrainBand(**Template, Band, Side, TileSide);
			if (!Image.Failure.IsEmpty())
			{
				if (Image.bUpstreamExhausted && Source == TEXT("auto") && Resolved == TEXT("naip") &&
					Bands.Num() == 1 && Band == TEXT("rgb"))
				{
					UE_LOG(LogVibeTerrain, Log, TEXT("NAIP retries exhausted. Restarting RGB imagery with a fresh Mapbox manifest."));
					Source = TEXT("mapbox");
					bRestart = true;
					break;
				}
				return Image.Failure;
			}
			const FString Path = Output.Path(Band == TEXT("rgb") ? TEXT("_rgb.jpg") : TEXT("_canopy.png"));
			if (!SaveBytes(Image.Encoded, Path)) return TerrainSaveError(Path);
			Files->SetStringField(Band, Path);
		}
		if (bRestart) continue;
		FBody Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetObjectField(TEXT("files"), Files);
		// metres_per_pixel is the manifest's: ground metres per texel interval, 1000 * map_size / (size - 1)
		for (const TCHAR* Field : {TEXT("size"), TEXT("metres_per_pixel"), TEXT("source"), TEXT("source_reason"), TEXT("acquisition_years"), TEXT("attribution"), TEXT("license_note")})
			CopyTerrainField(*Result, Info, Field);
		if (Source != RequestedSource)
			Result->SetStringField(TEXT("fallback_reason"), TEXT("NAIP tile failed three times. RGB restarted from a new Mapbox manifest."));
		Result->SetStringField(TEXT("import_hints"), TEXT("Import RGB as sRGB and canopy as linear grayscale with sRGB disabled."));
		Result->SetStringField(TEXT("uv_formula"), TerrainTextureUV);
		const FString ManifestPath = Output.Path(TEXT("_manifest.json"));
		if (!SaveText(Serialize(Manifest.Json.ToSharedRef()), ManifestPath)) return TerrainSaveError(ManifestPath);
		Result->SetStringField(TEXT("manifest_file"), ManifestPath);
		Output.bComplete = true;
		return Serialize(Result);
	}
}

// Stage both files before replacing either. Concurrent requests for the same save_path
// publish a matching heightmap and sidecar, and a failed replacement restores the prior pair.
static FString SaveVistaPair(const FString& ImagePath, const TArray<uint8>& Data, const FString& JsonPath, const FString& Json)
{
	const FString Tag = TEXT(".") + FGuid::NewGuid().ToString();
	struct FPart
	{
		FString Path;
		FString Stage;
		FString Backup;
		bool bBackedUp = false;
		bool bPublished = false;
	};
	FPart Parts[] = {
		{ImagePath, ImagePath + Tag + TEXT(".tmp"), ImagePath + Tag + TEXT(".bak")},
		{JsonPath, JsonPath + Tag + TEXT(".tmp"), JsonPath + Tag + TEXT(".bak")}
	};
	IFileManager& Files = IFileManager::Get();
	ON_SCOPE_EXIT { for (const FPart& Part : Parts) Files.Delete(*Part.Stage); };
	if (!SaveBytes(Data, Parts[0].Stage)) return TerrainSaveError(ImagePath);
	if (!SaveText(Json, Parts[1].Stage)) return TerrainSaveError(JsonPath);

	static FCriticalSection VistaPublishLock;
	FScopeLock Lock(&VistaPublishLock);
	FString FailedPath;
	for (FPart& Part : Parts)
	{
		if (Files.FileExists(*Part.Path))
		{
			Part.bBackedUp = Files.Move(*Part.Backup, *Part.Path, false);
			if (!Part.bBackedUp) { FailedPath = Part.Path; break; }
		}
		Part.bPublished = Files.Move(*Part.Path, *Part.Stage, false);
		if (!Part.bPublished) { FailedPath = Part.Path; break; }
	}
	for (const FPart& Part : Parts)
	{
		if (FailedPath.IsEmpty())
		{
			if (Part.bBackedUp) Files.Delete(*Part.Backup);
		}
		else
		{
			if (Part.bPublished) Files.Delete(*Part.Path);
			if (Part.bBackedUp && !Files.Move(*Part.Path, *Part.Backup, false))
				FailedPath += TEXT(" (previous file retained at ") + Part.Backup + TEXT(")");
		}
	}
	return FailedPath.IsEmpty() ? FString() : TerrainSaveError(FailedPath);
}

FString GenerateVista(const FParams& Params)
{
	const FBody Body = HeightBody(Params, true);
	const FString Format = Body->GetStringField(TEXT("format"));
	if (Format != TEXT("png") && Format != TEXT("raw") && Format != TEXT("zip"))
		return Error(TEXT("INVALID_PARAMS"), TEXT("Vista format must be png, raw or zip."));
	const TCHAR* Mime = Format == TEXT("png") ? TEXT("image/png") :
		(Format == TEXT("raw") ? TEXT("application/octet-stream") : TEXT("application/zip"));
	const FTerrainReply Reply = RequestTerrain(TEXT("/api/terrain/vista-heightmap"), Body, Mime);
	if (!Reply.Failure.IsEmpty()) return Reply.Failure;
	const auto Alignment = ParseTerrainJson(Reply.Http.Headers.FindRef(TEXT("X-Vista-Alignment")));
	double Spacing, HeightScale;
	if (!Alignment || !Alignment->TryGetNumberField(TEXT("landscape_metres_per_pixel"), Spacing) || Spacing <= 0 ||
		!Alignment->TryGetNumberField(TEXT("height_scale"), HeightScale) || HeightScale <= 0 ||
		!Alignment->HasTypedField<EJson::Object>(TEXT("inner_rect")) || !Alignment->HasTypedField<EJson::Object>(TEXT("clipping")))
		return Error(TEXT("INVALID_RESPONSE"), TEXT("Missing or invalid X-Vista-Alignment header."));
	const FString Stem = TerrainStem(TEXT("vista"), *Body) + TEXT("_") + FGuid::NewGuid().ToString();
	const FString Path = SavePath(Param(Params, TEXT("save_path")), Stem + TEXT(".") + Format);
	const FString Metadata = FPaths::SetExtension(Path, TEXT("json"));
	if (Path.Equals(Metadata, ESearchCase::IgnoreCase))
		return Error(TEXT("INVALID_PARAMS"), TEXT("The heightmap and JSON sidecar need distinct paths."));
	const FString SaveFailure = SaveVistaPair(Path, Reply.Http.Content, Metadata, Serialize(Alignment.ToSharedRef()));
	if (!SaveFailure.IsEmpty()) return SaveFailure;
	FBody Result = MakeShared<FJsonObject>(*Alignment);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("file"), Path);
	Result->SetStringField(TEXT("format"), Format);
	Result->SetNumberField(TEXT("size_bytes"), Reply.Http.Content.Num());
	const FString Dimensions = Reply.Http.Headers.FindRef(TEXT("X-Heightmap-Size"));
	if (!Dimensions.IsEmpty()) Result->SetStringField(TEXT("dimensions"), Dimensions);
	Result->SetStringField(TEXT("alignment_file"), Metadata);
	Result->SetNumberField(TEXT("suggested_xy_scale"), Spacing * 100.0);
	Result->SetNumberField(TEXT("suggested_z_scale"), 20000.0 / HeightScale);
	const FString Credit = Reply.Http.Headers.FindRef(TEXT("X-Terrain-Attribution"));
	if (!Credit.IsEmpty()) Result->SetStringField(TEXT("attribution"), Credit);
	Result->SetStringField(TEXT("message"), TEXT("Place the vista around the playable landscape as a backdrop with the same centre and vertical mapping, cutting out the inclusive inner_rect."));
	return Serialize(Result);
}
} // namespace VibeUE::Terrain
