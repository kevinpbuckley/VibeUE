// Copyright Buckley Builds LLC 2026 All Rights Reserved.
//
// TerrainDataTools.cpp
// MCP tool: terrain_data — generate heightmaps and map images from real-world terrain data.
// Calls vibeue.com terrain API endpoints authenticated with the user's VibeUE API key.

#include "Core/ToolRegistry.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static FString ExtractTerrainParam(const TMap<FString, FString>& Params, const FString& FieldName, const FString& Default = FString())
{
	// Check direct key
	const FString* Direct = Params.Find(FieldName);
	if (Direct) return *Direct;

	// Check capitalized (MCP server sometimes capitalizes first letter)
	FString Cap = FieldName;
	if (Cap.Len() > 0) Cap[0] = FChar::ToUpper(Cap[0]);
	Direct = Params.Find(Cap);
	if (Direct) return *Direct;

	// Fallback to ParamsJson
	const FString* ParamsJsonStr = Params.Find(TEXT("ParamsJson"));
	if (ParamsJsonStr)
	{
		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*ParamsJsonStr);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			FString Value;
			if (JsonObj->TryGetStringField(FieldName, Value))
				return Value;
			// Numbers (lat, lng, height_scale, etc.) arrive as JSON number values
			double NumValue;
			if (JsonObj->TryGetNumberField(FieldName, NumValue))
				return FString::Printf(TEXT("%.10g"), NumValue);
			// Booleans (sharpen, draw_streams, etc.) arrive as JSON bool values
			bool BoolValue;
			if (JsonObj->TryGetBoolField(FieldName, BoolValue))
				return BoolValue ? TEXT("true") : TEXT("false");
		}
	}

	return Default;
}

static double ExtractTerrainDouble(const TMap<FString, FString>& Params, const FString& Name, double Default)
{
	FString V = ExtractTerrainParam(Params, Name);
	return V.IsEmpty() ? Default : FCString::Atod(*V);
}

static int32 ExtractTerrainInt(const TMap<FString, FString>& Params, const FString& Name, int32 Default)
{
	FString V = ExtractTerrainParam(Params, Name);
	return V.IsEmpty() ? Default : FCString::Atoi(*V);
}

static bool ExtractTerrainBool(const TMap<FString, FString>& Params, const FString& Name, bool Default)
{
	FString V = ExtractTerrainParam(Params, Name);
	if (V.IsEmpty()) return Default;
	return V.Equals(TEXT("true"), ESearchCase::IgnoreCase) || V.Equals(TEXT("1"));
}

static FString GetVibeUEApiKey()
{
	FString Key;
	GConfig->GetString(TEXT("VibeUE"), TEXT("VibeUEApiKey"), Key, GEditorPerProjectIni);
	return Key;
}

static FString GetTerrainBaseUrl()
{
	FString Url = TEXT("https://www.vibeue.com");
	GConfig->GetString(TEXT("VibeUE.Terrain"), TEXT("ApiBaseUrl"), Url, GEngineIni);
	return Url;
}

static FString BuildErrorJson(const FString& Code, const FString& Message)
{
	return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\",\"message\":\"%s\"}"), *Code, *Message);
}

static FString BuildSuccessJson(const TSharedRef<FJsonObject>& Data)
{
	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Data, Writer);
	return Out;
}

// ---------------------------------------------------------------------------
// HTTP helper — blocking POST with JSON body, returns response bytes
// ---------------------------------------------------------------------------
struct FTerrainHttpResult
{
	bool bSuccess = false;
	int32 ResponseCode = 0;
	TArray<uint8> Content;
	TMap<FString, FString> Headers;
	FString ContentType;
	FString ErrorMessage;
};

static FTerrainHttpResult TerrainHttpPost(
	const FString& Url,
	const FString& ApiKey,
	const FString& JsonBody,
	float TimeoutSeconds = 30.0f)
{
	FTerrainHttpResult Result;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-API-Key"), ApiKey);
	Request->SetContentAsString(JsonBody);

	bool bComplete = false;
	Request->OnProcessRequestComplete().BindLambda(
		[&Result, &bComplete](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			// Write all fields BEFORE setting bComplete — game thread polls bComplete
			// and will return (destroying Result) the moment it sees true.
			if (!bConnected || !Resp.IsValid())
			{
				Result.ErrorMessage = TEXT("Connection failed");
			}
			else
			{
				Result.bSuccess = true;
				Result.ResponseCode = Resp->GetResponseCode();
				Result.Content = Resp->GetContent();
				Result.ContentType = Resp->GetContentType();
				Result.Headers.Add(TEXT("X-Heightmap-Min-Height"), Resp->GetHeader(TEXT("X-Heightmap-Min-Height")));
				Result.Headers.Add(TEXT("X-Heightmap-Max-Height"), Resp->GetHeader(TEXT("X-Heightmap-Max-Height")));
				Result.Headers.Add(TEXT("X-Heightmap-Size"),       Resp->GetHeader(TEXT("X-Heightmap-Size")));
			}
			bComplete = true; // signal last — Result is fully written
		}
	);

	Request->ProcessRequest();

	const double StartTime = FPlatformTime::Seconds();
	while (!bComplete)
	{
		// Tick the HTTP manager so it can dispatch the callback on the game thread.
		FHttpModule::Get().GetHttpManager().Tick(0.0f);
		FPlatformProcess::Sleep(0.01f);
		if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
		{
			Request->CancelRequest();
			Result.ErrorMessage = TEXT("Request timed out");
			return Result;
		}
	}

	return Result;
}

static FTerrainHttpResult TerrainHttpGet(const FString& Url, const FString& ApiKey, float TimeoutSeconds = 15.0f)
{
	FTerrainHttpResult Result;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	if (!ApiKey.IsEmpty())
		Request->SetHeader(TEXT("X-API-Key"), ApiKey);

	bool bComplete = false;
	Request->OnProcessRequestComplete().BindLambda(
		[&Result, &bComplete](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				Result.ErrorMessage = TEXT("Connection failed");
			}
			else
			{
				Result.bSuccess = true;
				Result.ResponseCode = Resp->GetResponseCode();
				Result.Content = Resp->GetContent();
				Result.ContentType = Resp->GetContentType();
			}
			bComplete = true; // signal last
		}
	);

	Request->ProcessRequest();

	const double StartTime = FPlatformTime::Seconds();
	while (!bComplete)
	{
		FHttpModule::Get().GetHttpManager().Tick(0.0f);
		FPlatformProcess::Sleep(0.01f);
		if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds)
		{
			Request->CancelRequest();
			Result.ErrorMessage = TEXT("Request timed out");
			return Result;
		}
	}

	return Result;
}

// ---------------------------------------------------------------------------
// Save path resolution
// ---------------------------------------------------------------------------
static FString ResolveSavePath(const FString& RequestedPath, const FString& Filename)
{
	if (!RequestedPath.IsEmpty()) return RequestedPath;
	FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Terrain"));
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.DirectoryExists(*Dir)) PF.CreateDirectory(*Dir);
	return FPaths::Combine(Dir, Filename);
}

// ---------------------------------------------------------------------------
// Action: generate_heightmap
// ---------------------------------------------------------------------------
static FString ActionGenerateHeightmap(const TMap<FString, FString>& Params)
{
	const FString ApiKey = GetVibeUEApiKey();
	if (ApiKey.IsEmpty())
		return BuildErrorJson(TEXT("NO_API_KEY"), TEXT("No VibeUE API key configured. Set it in VibeUE chat settings."));

	const double Lng = ExtractTerrainDouble(Params, TEXT("lng"), 0.0);
	const double Lat = ExtractTerrainDouble(Params, TEXT("lat"), 0.0);
	if (Lat == 0.0 && Lng == 0.0 && ExtractTerrainParam(Params, TEXT("lat")).IsEmpty())
		return BuildErrorJson(TEXT("MISSING_PARAMS"), TEXT("lat and lng are required."));

	const FString Format        = ExtractTerrainParam(Params, TEXT("format"), TEXT("png"));
	const double MapSize        = ExtractTerrainDouble(Params, TEXT("map_size"),         17.28);
	const double BaseLevel      = ExtractTerrainDouble(Params, TEXT("base_level"),        0.0);
	const int32  HeightScale    = ExtractTerrainInt   (Params, TEXT("height_scale"),      100);
	const int32  WaterDepth     = ExtractTerrainInt   (Params, TEXT("water_depth"),        40);
	const int32  GravityCenter  = ExtractTerrainInt   (Params, TEXT("gravity_center"),      0);
	const int32  LevelCorr      = ExtractTerrainInt   (Params, TEXT("level_correction"),    0);
	const int32  BlurPasses     = ExtractTerrainInt   (Params, TEXT("blur_passes"),         10);
	const int32  BlurPostPasses = ExtractTerrainInt   (Params, TEXT("blur_post_passes"),     2);
	const bool   bSharpen       = ExtractTerrainBool  (Params, TEXT("sharpen"),            true);
	const bool   bDrawStreams   = ExtractTerrainBool  (Params, TEXT("draw_streams"),       true);
	const int32  StreamDepth    = ExtractTerrainInt   (Params, TEXT("stream_depth"),          7);
	const int32  PlainsHeight   = ExtractTerrainInt   (Params, TEXT("plains_height"),       140);
	const FString SavePath      = ExtractTerrainParam (Params, TEXT("save_path"));
	const int32  Resolution     = ExtractTerrainInt   (Params, TEXT("resolution"),            0);

	// Build JSON body
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("lng"),              Lng);
	Body->SetNumberField(TEXT("lat"),              Lat);
	Body->SetStringField(TEXT("format"),           Format);
	Body->SetNumberField(TEXT("map_size"),         MapSize);
	Body->SetNumberField(TEXT("base_level"),       BaseLevel);
	Body->SetNumberField(TEXT("height_scale"),     HeightScale);
	Body->SetNumberField(TEXT("water_depth"),      WaterDepth);
	Body->SetNumberField(TEXT("gravity_center"),   GravityCenter);
	Body->SetNumberField(TEXT("level_correction"), LevelCorr);
	Body->SetNumberField(TEXT("blur_passes"),      BlurPasses);
	Body->SetNumberField(TEXT("blur_post_passes"), BlurPostPasses);
	Body->SetBoolField  (TEXT("sharpen"),          bSharpen);
	Body->SetBoolField  (TEXT("draw_streams"),     bDrawStreams);
	Body->SetNumberField(TEXT("stream_depth"),     StreamDepth);
	Body->SetNumberField(TEXT("plains_height"),    PlainsHeight);
	if (Resolution > 0)
	{
		Body->SetNumberField(TEXT("resolution"), Resolution);
	}

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body, Writer);

	const FString Url = GetTerrainBaseUrl() + TEXT("/api/terrain/heightmap");
	const FTerrainHttpResult HttpResult = TerrainHttpPost(Url, ApiKey, BodyStr);

	if (!HttpResult.bSuccess)
		return BuildErrorJson(TEXT("HTTP_ERROR"), HttpResult.ErrorMessage);

	if (HttpResult.ResponseCode != 200)
	{
		FString ErrMsg;
		if (HttpResult.Content.Num() > 0)
		{
			TArray<uint8> ContentCopy = HttpResult.Content;
			ContentCopy.Add(0);
			ErrMsg = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ContentCopy.GetData())));
		}
		return BuildErrorJson(
			FString::Printf(TEXT("HTTP_%d"), HttpResult.ResponseCode),
			ErrMsg.IsEmpty() ? FString::Printf(TEXT("Server returned %d"), HttpResult.ResponseCode) : ErrMsg
		);
	}

	// Determine file extension
	FString Ext = Format;
	const FString DefaultFilename = FString::Printf(TEXT("heightmap_%.4f_%.4f.%s"), Lat, Lng, *Ext);
	const FString FilePath = ResolveSavePath(SavePath, DefaultFilename);

	if (!FFileHelper::SaveArrayToFile(HttpResult.Content, *FilePath))
		return BuildErrorJson(TEXT("SAVE_ERROR"), FString::Printf(TEXT("Failed to save to: %s"), *FilePath));

	// Build success response
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField  (TEXT("success"),    true);
	Out->SetStringField(TEXT("file"),       FilePath);
	Out->SetStringField(TEXT("format"),     Format);
	Out->SetNumberField(TEXT("size_bytes"), HttpResult.Content.Num());

	const FString* MinH = HttpResult.Headers.Find(TEXT("X-Heightmap-Min-Height"));
	const FString* MaxH = HttpResult.Headers.Find(TEXT("X-Heightmap-Max-Height"));
	const FString* Size = HttpResult.Headers.Find(TEXT("X-Heightmap-Size"));
	if (MinH && !MinH->IsEmpty()) Out->SetNumberField(TEXT("min_height_m"), FCString::Atod(**MinH));
	if (MaxH && !MaxH->IsEmpty()) Out->SetNumberField(TEXT("max_height_m"), FCString::Atod(**MaxH));
	if (Size && !Size->IsEmpty()) Out->SetStringField(TEXT("dimensions"),   *Size);

	Out->SetStringField(TEXT("message"), FString::Printf(
		TEXT("Heightmap saved to %s. Import via Edit > Import Heightmap in the Landscape editor."), *FilePath));

	return BuildSuccessJson(Out);
}

// ---------------------------------------------------------------------------
// Action: preview_elevation
// ---------------------------------------------------------------------------
static FString ActionPreviewElevation(const TMap<FString, FString>& Params)
{
	const FString ApiKey = GetVibeUEApiKey();
	if (ApiKey.IsEmpty())
		return BuildErrorJson(TEXT("NO_API_KEY"), TEXT("No VibeUE API key configured."));

	const double Lng = ExtractTerrainDouble(Params, TEXT("lng"), 0.0);
	const double Lat = ExtractTerrainDouble(Params, TEXT("lat"), 0.0);
	if (ExtractTerrainParam(Params, TEXT("lat")).IsEmpty())
		return BuildErrorJson(TEXT("MISSING_PARAMS"), TEXT("lat and lng are required."));

	const double MapSize = ExtractTerrainDouble(Params, TEXT("map_size"), 17.28);

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("lng"),      Lng);
	Body->SetNumberField(TEXT("lat"),      Lat);
	Body->SetNumberField(TEXT("map_size"), MapSize);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body, Writer);

	const FString Url = GetTerrainBaseUrl() + TEXT("/api/terrain/preview");
	const FTerrainHttpResult HttpResult = TerrainHttpPost(Url, ApiKey, BodyStr);

	if (!HttpResult.bSuccess)
		return BuildErrorJson(TEXT("HTTP_ERROR"), HttpResult.ErrorMessage);

	if (HttpResult.ResponseCode != 200)
		return BuildErrorJson(
			FString::Printf(TEXT("HTTP_%d"), HttpResult.ResponseCode),
			FString::Printf(TEXT("Server returned %d"), HttpResult.ResponseCode));

	// Pass through the JSON response from the server
	TArray<uint8> ContentCopy = HttpResult.Content;
	ContentCopy.Add(0);
	return FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ContentCopy.GetData())));
}

// ---------------------------------------------------------------------------
// Action: get_map_image
// ---------------------------------------------------------------------------
static FString ActionGetMapImage(const TMap<FString, FString>& Params)
{
	const FString ApiKey = GetVibeUEApiKey();
	if (ApiKey.IsEmpty())
		return BuildErrorJson(TEXT("NO_API_KEY"), TEXT("No VibeUE API key configured."));

	const double Lng     = ExtractTerrainDouble(Params, TEXT("lng"),      0.0);
	const double Lat     = ExtractTerrainDouble(Params, TEXT("lat"),      0.0);
	const double MapSize = ExtractTerrainDouble(Params, TEXT("map_size"), 17.28);
	const FString Style  = ExtractTerrainParam (Params, TEXT("style"),    TEXT("satellite-v9"));
	const int32  Width   = ExtractTerrainInt   (Params, TEXT("width"),    1280);
	const int32  Height  = ExtractTerrainInt   (Params, TEXT("height"),   1280);
	const FString SavePath = ExtractTerrainParam(Params, TEXT("save_path"));

	if (ExtractTerrainParam(Params, TEXT("lat")).IsEmpty())
		return BuildErrorJson(TEXT("MISSING_PARAMS"), TEXT("lat and lng are required."));

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("lng"),      Lng);
	Body->SetNumberField(TEXT("lat"),      Lat);
	Body->SetNumberField(TEXT("map_size"), MapSize);
	Body->SetStringField(TEXT("style"),    Style);
	Body->SetNumberField(TEXT("width"),    Width);
	Body->SetNumberField(TEXT("height"),   Height);

	FString BodyStr;
	TSharedRef<TJsonWriter<>> BodyWriter = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body, BodyWriter);

	const FString Url = GetTerrainBaseUrl() + TEXT("/api/terrain/map-image");
	const FTerrainHttpResult HttpResult = TerrainHttpPost(Url, ApiKey, BodyStr);

	if (!HttpResult.bSuccess)
		return BuildErrorJson(TEXT("HTTP_ERROR"), HttpResult.ErrorMessage);

	if (HttpResult.ResponseCode != 200)
		return BuildErrorJson(
			FString::Printf(TEXT("HTTP_%d"), HttpResult.ResponseCode),
			FString::Printf(TEXT("Server returned %d"), HttpResult.ResponseCode));

	const FString StyleTag = Style.Replace(TEXT("-"), TEXT("_")).Replace(TEXT("."), TEXT("_"));
	const FString DefaultFilename = FString::Printf(TEXT("map_%s_%.4f_%.4f.png"), *StyleTag, Lat, Lng);
	const FString FilePath = ResolveSavePath(SavePath, DefaultFilename);

	if (!FFileHelper::SaveArrayToFile(HttpResult.Content, *FilePath))
		return BuildErrorJson(TEXT("SAVE_ERROR"), FString::Printf(TEXT("Failed to save to: %s"), *FilePath));

	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField  (TEXT("success"),    true);
	Out->SetStringField(TEXT("file"),       FilePath);
	Out->SetStringField(TEXT("style"),      Style);
	Out->SetNumberField(TEXT("size_bytes"), HttpResult.Content.Num());
	Out->SetStringField(TEXT("message"),    FString::Printf(TEXT("Map image saved to %s"), *FilePath));

	return BuildSuccessJson(Out);
}

// ---------------------------------------------------------------------------
// Action: list_styles
// ---------------------------------------------------------------------------
static FString ActionListStyles()
{
	const FString Url = GetTerrainBaseUrl() + TEXT("/api/terrain/styles");
	const FTerrainHttpResult HttpResult = TerrainHttpGet(Url, FString());

	if (!HttpResult.bSuccess || HttpResult.ResponseCode != 200)
		return BuildErrorJson(TEXT("HTTP_ERROR"), TEXT("Failed to fetch styles list."));

	TArray<uint8> ContentCopy = HttpResult.Content;
	ContentCopy.Add(0);
	return FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ContentCopy.GetData())));
}

// ---------------------------------------------------------------------------
// Tool registration
// ---------------------------------------------------------------------------
REGISTER_VIBEUE_TOOL(terrain_data,
	"Generate heightmaps and map images from real-world terrain data for "
	"Unreal Engine landscape import. Requires an active VibeUE API key. "
	"Actions: generate_heightmap, preview_elevation, get_map_image, list_styles. "
	"IMPORTANT: Use the 'resolution' parameter to match your landscape resolution. "
	"Workflow: 1) Decide landscape config (e.g. 8x8 components, 63 quads, 1 section = 505x505). "
	"2) Call preview_elevation for suggested settings. "
	"3) Call generate_heightmap with resolution=505 to match your landscape. "
	"4) Import via ULandscapeService.import_heightmap(). "
	"If resolution is omitted, defaults to 1081 (Cities: Skylines standard). "
	"You can also use ULandscapeService.resize_heightmap() to resize after generation.",
	"Terrain",
	TOOL_PARAMS(
		TOOL_PARAM("action",          "Action: generate_heightmap | preview_elevation | get_map_image | list_styles", "string", true),
		TOOL_PARAM("lng",             "Longitude of center point (e.g. -122.4194 for San Francisco)", "number", false),
		TOOL_PARAM("lat",             "Latitude of center point (e.g. 37.7749 for San Francisco)", "number", false),
		TOOL_PARAM("format",          "Output format for generate_heightmap: png (default), raw, zip", "string", false),
		TOOL_PARAM("resolution",      "Output resolution NxN pixels for generate_heightmap. MUST match landscape resolution. "
		                               "Use ULandscapeService.calculate_landscape_resolution() to compute. "
		                               "Common: 505 (8x8,63,1), 1009 (8x8,63,2 or 16x16,63,1), 1017 (8x8,127,1). Default: 1081", "number", false),
		TOOL_PARAM("map_size",        "Map size in km (default 17.28 for Cities: Skylines)", "number", false),
		TOOL_PARAM("base_level",      "Base elevation offset in meters (default 0; use preview_elevation for good value)", "number", false),
		TOOL_PARAM("height_scale",    "Height scale percentage 1-250 (default 100; use preview_elevation for good value)", "number", false),
		TOOL_PARAM("water_depth",     "Water depth in Cities: Skylines units (default 40)", "number", false),
		TOOL_PARAM("gravity_center",  "Water flow direction 0-13: 0=disabled, 1=center, 2=N, 3=NE, 4=E, 5=SE, 6=S, 7=SW, 8=W, 9=NW, 10=north side, 11=east side, 12=south side, 13=west side", "number", false),
		TOOL_PARAM("level_correction","Elevation curve style 0-9: 0=none, 2=coastline, 3=aggressive coastline (default 0)", "number", false),
		TOOL_PARAM("blur_passes",     "Smoothing passes for plains (default 10)", "number", false),
		TOOL_PARAM("blur_post_passes","Post-sharpening passes (default 2)", "number", false),
		TOOL_PARAM("sharpen",         "Apply sharpening kernel (default true)", "boolean", false),
		TOOL_PARAM("draw_streams",    "Re-etch waterways after smoothing (default true)", "boolean", false),
		TOOL_PARAM("stream_depth",    "Stream depth in meters (default 7)", "number", false),
		TOOL_PARAM("plains_height",   "Height threshold for plains smoothing in meters (default 140)", "number", false),
		TOOL_PARAM("style",           "Map image style for get_map_image: satellite-v9, outdoors-v11, streets-v11, light-v10, dark-v10", "string", false),
		TOOL_PARAM("save_path",       "File path to save output. Default: <ProjectDir>/Saved/Terrain/", "string", false)
	),
	{
		const FString Action = ExtractTerrainParam(Params, TEXT("action")).ToLower().TrimStartAndEnd();

		if (Action.IsEmpty())
			return BuildErrorJson(TEXT("MISSING_ACTION"), TEXT("'action' is required. Options: generate_heightmap, preview_elevation, get_map_image, list_styles"));

		if (Action == TEXT("generate_heightmap"))  return ActionGenerateHeightmap(Params);
		if (Action == TEXT("preview_elevation"))   return ActionPreviewElevation(Params);
		if (Action == TEXT("get_map_image"))       return ActionGetMapImage(Params);
		if (Action == TEXT("list_styles"))         return ActionListStyles();

		return BuildErrorJson(TEXT("UNKNOWN_ACTION"),
			FString::Printf(TEXT("Unknown action: '%s'. Valid: generate_heightmap, preview_elevation, get_map_image, list_styles"), *Action));
	}
);
