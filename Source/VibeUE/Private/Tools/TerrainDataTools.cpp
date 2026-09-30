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
#include "Settings/VibeUEEditorSettings.h"
// Shared request state, a wait off the game thread, cancellation
#include "Async/Async.h"
#include "HAL/Event.h"
#include "Core/VibeUEToolCancel.h"

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
	// Primary: Editor Preferences > Plugins > VibeUE (UVibeUEEditorSettings::ApiKey).
	if (const UVibeUEEditorSettings* Settings = GetDefault<UVibeUEEditorSettings>())
	{
		if (!Settings->ApiKey.IsEmpty())
			return Settings->ApiKey;
	}

	// Backward-compat: the legacy [VibeUE] VibeUEApiKey key written by older builds.
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

static const TCHAR* NoApiKeyMessage()
{
	return TEXT("No VibeUE API key configured. Set it in Editor Preferences > Plugins > VibeUE (get a free key at https://www.vibeue.com/login).");
}

// ---------------------------------------------------------------------------
// HTTP helper — blocking POST with JSON body, returns response bytes
// ---------------------------------------------------------------------------
struct FTerrainHttpResult
{
	bool bSuccess = false;
	bool bNoApiKey = false; // The key is read where the request starts, on the game thread
	int32 ResponseCode = 0;
	TArray<uint8> Content;
	TMap<FString, FString> Headers;
	FString ContentType;
	FString ErrorMessage;
};

// What one request needs, built on any thread. The request itself is made on the game thread, which also reads the
// API key (a settings CDO field, written there).
struct FTerrainHttpRequestSpec
{
	FString Url;
	FString Verb = TEXT("GET");
	FString JsonBody;           // POST only
	bool bSendApiKey = false;   // when none is configured, the request is not made and the result says bNoApiKey
	bool bKeepHeightmapHeaders = false;
	float TimeoutSeconds = 30.0f;
};

// The request's completion writes into this shared state, which it holds only weakly, never into the helper's
// locals: a request that times out is cancelled, CancelRequest only schedules the abort, and the completion then runs
// at a later HTTP tick, after the helper has returned (the same fix as deep_research's ResearchHttpGet).
struct FTerrainHttpState
{
	FTerrainHttpResult Result;
	std::atomic<bool> bComplete{false};
	std::atomic<bool> bCancelled{false};
	FEventRef Done{EEventMode::ManualReset};
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request; // touched on the game thread only
};
using FTerrainHttpStateRef = TSharedRef<FTerrainHttpState, ESPMode::ThreadSafe>;

// Builds and starts the request. Game thread only.
static void StartTerrainRequest(const FTerrainHttpStateRef& State, const FTerrainHttpRequestSpec& Spec)
{
	FString ApiKey;
	if (Spec.bSendApiKey)
	{
		ApiKey = GetVibeUEApiKey();
		if (ApiKey.IsEmpty())
		{
			State->Result.bNoApiKey = true;
			State->Result.ErrorMessage = NoApiKeyMessage();
			State->bComplete = true;
			State->Done->Trigger();
			return;
		}
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Spec.Url);
	Request->SetVerb(Spec.Verb);
	if (Spec.Verb == TEXT("POST"))
	{
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Request->SetContentAsString(Spec.JsonBody);
	}
	if (!ApiKey.IsEmpty())
		Request->SetHeader(TEXT("X-API-Key"), ApiKey);
	Request->SetTimeout(Spec.TimeoutSeconds);

	const TWeakPtr<FTerrainHttpState, ESPMode::ThreadSafe> WeakState = State;
	const bool bKeepHeightmapHeaders = Spec.bKeepHeightmapHeaders;
	Request->OnProcessRequestComplete().BindLambda(
		[WeakState, bKeepHeightmapHeaders](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
		{
			const TSharedPtr<FTerrainHttpState, ESPMode::ThreadSafe> S = WeakState.Pin();
			if (!S)
			{
				return; // the caller gave up and is gone
			}
			FTerrainHttpResult& Result = S->Result;
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
				if (bKeepHeightmapHeaders)
				{
					Result.Headers.Add(TEXT("X-Heightmap-Min-Height"), Resp->GetHeader(TEXT("X-Heightmap-Min-Height")));
					Result.Headers.Add(TEXT("X-Heightmap-Max-Height"), Resp->GetHeader(TEXT("X-Heightmap-Max-Height")));
					Result.Headers.Add(TEXT("X-Heightmap-Size"),       Resp->GetHeader(TEXT("X-Heightmap-Size")));
				}
			}
			S->bComplete = true;
			S->Done->Trigger();
		}
	);

	State->Request = Request;
	Request->ProcessRequest();
}

// Unbinds the completion, then cancels. Game thread only.
static void AbandonTerrainRequest(const FTerrainHttpStateRef& State)
{
	if (State->Request.IsValid())
	{
		State->Request->OnProcessRequestComplete().Unbind();
		State->Request->CancelRequest();
		State->Request.Reset();
	}
}

static FTerrainHttpResult TerrainHttpRun(const FTerrainHttpRequestSpec& Spec)
{
	const FTerrainHttpStateRef State = MakeShared<FTerrainHttpState, ESPMode::ThreadSafe>();

	if (IsInGameThread())
	{
		// On the game thread (a direct call, e.g. from Python or the VibeUE console): the original blocking wait.
		StartTerrainRequest(State, Spec);
		const double StartTime = FPlatformTime::Seconds();
		while (!State->bComplete)
		{
			// Tick the HTTP manager so it can dispatch the callback on the game thread.
			FHttpModule::Get().GetHttpManager().Tick(0.0f);
			FPlatformProcess::Sleep(0.01f);
			if (FPlatformTime::Seconds() - StartTime > Spec.TimeoutSeconds)
			{
				AbandonTerrainRequest(State);
				FTerrainHttpResult TimedOut;
				TimedOut.ErrorMessage = TEXT("Request timed out");
				return TimedOut;
			}
		}
		return State->Result;
	}

	// On a worker thread (the MCP bridge runs terrain_data there) the request is made, started and
	// abandoned on the game thread and this thread only waits, so the editor keeps ticking. A cancel from the client,
	// or the exit sweep (VibeUEMCPToolBridge::CancelAllRunning), wakes the wait; on the game thread it also abandons
	// the request at once, so nothing is left for HTTP's shutdown to find.
	FVibeUEToolCancel* Cancel = FVibeUEToolCancel::GetCurrent();
	if (Cancel)
	{
		const TWeakPtr<FTerrainHttpState, ESPMode::ThreadSafe> WeakState = State;
		Cancel->SetOnCancel([WeakState]()
		{
			if (const TSharedPtr<FTerrainHttpState, ESPMode::ThreadSafe> S = WeakState.Pin())
			{
				S->bCancelled = true;
				if (IsInGameThread())
				{
					AbandonTerrainRequest(S.ToSharedRef());
				}
				S->Done->Trigger();
			}
		});
	}
	AsyncTask(ENamedThreads::GameThread, [State, Spec]()
	{
		if (!State->bCancelled)
		{
			StartTerrainRequest(State, Spec);
		}
	});
	const bool bSignalled = State->Done->Wait(FTimespan::FromSeconds(Spec.TimeoutSeconds + 1.0));
	if (Cancel)
	{
		Cancel->ClearOnCancel();
	}
	if (bSignalled && State->bComplete && !State->bCancelled)
	{
		return State->Result;
	}
	AsyncTask(ENamedThreads::GameThread, [State]() { AbandonTerrainRequest(State); });
	FTerrainHttpResult Failed;
	Failed.ErrorMessage = State->bCancelled ? TEXT("Request cancelled") : TEXT("Request timed out");
	return Failed;
}

static FTerrainHttpResult TerrainHttpPost(const FString& Path, const FString& JsonBody, float TimeoutSeconds = 30.0f)
{
	FTerrainHttpRequestSpec Spec;
	Spec.Url = GetTerrainBaseUrl() + Path; // GConfig reads take the config lock, safe on a worker
	Spec.Verb = TEXT("POST");
	Spec.JsonBody = JsonBody;
	Spec.bSendApiKey = true;
	Spec.bKeepHeightmapHeaders = true;
	Spec.TimeoutSeconds = TimeoutSeconds;
	return TerrainHttpRun(Spec);
}

static FTerrainHttpResult TerrainHttpGet(const FString& Path, float TimeoutSeconds = 15.0f)
{
	FTerrainHttpRequestSpec Spec;
	Spec.Url = GetTerrainBaseUrl() + Path;
	Spec.TimeoutSeconds = TimeoutSeconds;
	return TerrainHttpRun(Spec);
}

// ---------------------------------------------------------------------------
// Coordinate conversion: lng/lat → UE5 world space
//
// Assumptions (standard VibeUE landscape import workflow):
//   • Landscape actor is centered at world origin (0, 0, Z).
//   • +X = East, +Y = North, 1 m = 100 UE units.
// If the landscape actor is not at origin, add its Location from
// ULandscapeService.get_landscape_info() to every ue5_point.
// ---------------------------------------------------------------------------
static FVector TerrainLngLatToUE5(double PointLng, double PointLat, double CenterLng, double CenterLat)
{
	const double METERS_PER_DEG = 111320.0;
	const double CosLat         = FMath::Cos(FMath::DegreesToRadians(CenterLat));
	return FVector(
		(float)((PointLng - CenterLng) * METERS_PER_DEG * CosLat * 100.0),  // +X = East
		(float)((PointLat - CenterLat) * METERS_PER_DEG             * 100.0),  // +Y = North
		0.0f);
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
// Action: get_water_features
// ---------------------------------------------------------------------------
static FString ActionGetWaterFeatures(const TMap<FString, FString>& Params)
{
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

	// Use a longer timeout — PBF tiles may be fetched across multiple HTTP round-trips
	const FTerrainHttpResult HttpResult = TerrainHttpPost(TEXT("/api/terrain/water-features"), BodyStr, 45.0f);

	if (HttpResult.bNoApiKey) // Read where the request starts
		return BuildErrorJson(TEXT("NO_API_KEY"), NoApiKeyMessage());
	if (!HttpResult.bSuccess)
		return BuildErrorJson(TEXT("HTTP_ERROR"), HttpResult.ErrorMessage);

	if (HttpResult.ResponseCode != 200)
	{
		TArray<uint8> ContentCopy = HttpResult.Content;
		ContentCopy.Add(0);
		const FString ErrBody = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ContentCopy.GetData())));
		return BuildErrorJson(
			FString::Printf(TEXT("HTTP_%d"), HttpResult.ResponseCode),
			ErrBody.IsEmpty() ? FString::Printf(TEXT("Server returned %d"), HttpResult.ResponseCode) : ErrBody);
	}

	// Parse the API response and inject UE5 world-space coordinates
	TArray<uint8> ContentCopy = HttpResult.Content;
	ContentCopy.Add(0);
	const FString JsonStr = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ContentCopy.GetData())));

	TSharedPtr<FJsonObject> ApiJson;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, ApiJson) || !ApiJson.IsValid())
		return BuildErrorJson(TEXT("PARSE_ERROR"), TEXT("Failed to parse server response."));

	// Inject ue5_points into each waterway
	const TArray<TSharedPtr<FJsonValue>>* WaterwaysArray;
	if (ApiJson->TryGetArrayField(TEXT("waterways"), WaterwaysArray))
	{
		for (const TSharedPtr<FJsonValue>& WwVal : *WaterwaysArray)
		{
			TSharedPtr<FJsonObject> Ww = WwVal->AsObject();
			if (!Ww.IsValid()) continue;
			const TArray<TSharedPtr<FJsonValue>>* PtsArray;
			if (!Ww->TryGetArrayField(TEXT("points"), PtsArray)) continue;

			TArray<TSharedPtr<FJsonValue>> UE5Pts;
			UE5Pts.Reserve(PtsArray->Num());
			for (const TSharedPtr<FJsonValue>& PtVal : *PtsArray)
			{
				TSharedPtr<FJsonObject> Pt = PtVal->AsObject();
				if (!Pt.IsValid()) continue;
				double PtLng = 0.0, PtLat = 0.0;
				Pt->TryGetNumberField(TEXT("lng"), PtLng);
				Pt->TryGetNumberField(TEXT("lat"), PtLat);
				FVector V = TerrainLngLatToUE5(PtLng, PtLat, Lng, Lat);
				TSharedRef<FJsonObject> UE5Pt = MakeShared<FJsonObject>();
				UE5Pt->SetNumberField(TEXT("x"), V.X);
				UE5Pt->SetNumberField(TEXT("y"), V.Y);
				UE5Pt->SetNumberField(TEXT("z"), V.Z);
				UE5Pts.Add(MakeShared<FJsonValueObject>(UE5Pt));
			}
			Ww->SetArrayField(TEXT("ue5_points"), UE5Pts);
		}
	}

	// Inject ue5_rings into each water body
	const TArray<TSharedPtr<FJsonValue>>* BodiesArray;
	if (ApiJson->TryGetArrayField(TEXT("water_bodies"), BodiesArray))
	{
		for (const TSharedPtr<FJsonValue>& BdVal : *BodiesArray)
		{
			TSharedPtr<FJsonObject> Bd = BdVal->AsObject();
			if (!Bd.IsValid()) continue;
			const TArray<TSharedPtr<FJsonValue>>* RingsArray;
			if (!Bd->TryGetArrayField(TEXT("rings"), RingsArray)) continue;

			TArray<TSharedPtr<FJsonValue>> UE5Rings;
			UE5Rings.Reserve(RingsArray->Num());
			for (const TSharedPtr<FJsonValue>& RingVal : *RingsArray)
			{
				const TArray<TSharedPtr<FJsonValue>>* PtsArray;
				if (!RingVal->TryGetArray(PtsArray)) continue;
				TArray<TSharedPtr<FJsonValue>> UE5Ring;
				UE5Ring.Reserve(PtsArray->Num());
				for (const TSharedPtr<FJsonValue>& PtVal : *PtsArray)
				{
					TSharedPtr<FJsonObject> Pt = PtVal->AsObject();
					if (!Pt.IsValid()) continue;
					double PtLng = 0.0, PtLat = 0.0;
					Pt->TryGetNumberField(TEXT("lng"), PtLng);
					Pt->TryGetNumberField(TEXT("lat"), PtLat);
					FVector V = TerrainLngLatToUE5(PtLng, PtLat, Lng, Lat);
					TSharedRef<FJsonObject> UE5Pt = MakeShared<FJsonObject>();
					UE5Pt->SetNumberField(TEXT("x"), V.X);
					UE5Pt->SetNumberField(TEXT("y"), V.Y);
					UE5Pt->SetNumberField(TEXT("z"), V.Z);
					UE5Ring.Add(MakeShared<FJsonValueObject>(UE5Pt));
				}
				UE5Rings.Add(MakeShared<FJsonValueArray>(UE5Ring));
			}
			Bd->SetArrayField(TEXT("ue5_rings"), UE5Rings);
		}
	}

	ApiJson->SetStringField(TEXT("ue5_coordinate_note"),
		TEXT("+X=East +Y=North, landscape center at world origin. "
		     "If landscape actor is offset, add its Location from "
		     "ULandscapeService.get_landscape_info() to each ue5_point/ue5_ring vertex."));

	// Serialize the full JSON (with injected UE5 coordinates)
	FString FullJson;
	{
		TSharedRef<TJsonWriter<>> FullWriter = TJsonWriterFactory<>::Create(&FullJson);
		FJsonSerializer::Serialize(ApiJson.ToSharedRef(), FullWriter);
	}

	// Save full JSON to file (same pattern as heightmap/map_image)
	const FString SavePath = ExtractTerrainParam(Params, TEXT("save_path"));
	const FString DefaultFilename = FString::Printf(TEXT("water_features_%.4f_%.4f_%gkm.json"), Lat, Lng, MapSize);
	const FString FilePath = ResolveSavePath(SavePath, DefaultFilename);

	if (!FFileHelper::SaveStringToFile(FullJson, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		return BuildErrorJson(TEXT("SAVE_ERROR"), FString::Printf(TEXT("Failed to save water features to: %s"), *FilePath));

	// Build a compact summary to return (not the full JSON — that can be 200K+ tokens)
	int32 NumWaterways = 0;
	int32 TotalWaterwayPoints = 0;
	int32 NumBodies = 0;
	int32 TotalBodyPoints = 0;

	// Track class breakdown for waterways
	TMap<FString, int32> WaterwayClassCounts;

	TArray<TSharedPtr<FJsonValue>> WaterwaySummaries;
	if (ApiJson->TryGetArrayField(TEXT("waterways"), WaterwaysArray))
	{
		NumWaterways = WaterwaysArray->Num();
		for (const TSharedPtr<FJsonValue>& WwVal : *WaterwaysArray)
		{
			TSharedPtr<FJsonObject> Ww = WwVal->AsObject();
			if (!Ww.IsValid()) continue;
			const TArray<TSharedPtr<FJsonValue>>* Pts;
			int32 PtCount = 0;
			if (Ww->TryGetArrayField(TEXT("ue5_points"), Pts)) PtCount = Pts->Num();
			TotalWaterwayPoints += PtCount;

			TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
			FString Name; Ww->TryGetStringField(TEXT("name"), Name);
			FString Class; Ww->TryGetStringField(TEXT("class"), Class);
			double Width = 0; Ww->TryGetNumberField(TEXT("estimated_width_m"), Width);
			Summary->SetStringField(TEXT("name"), Name);
			Summary->SetStringField(TEXT("class"), Class);
			Summary->SetNumberField(TEXT("estimated_width_m"), Width);
			Summary->SetNumberField(TEXT("num_points"), PtCount);
			WaterwaySummaries.Add(MakeShared<FJsonValueObject>(Summary));

			// Track class counts
			FString ClassKey = Class.IsEmpty() ? TEXT("unknown") : Class;
			WaterwayClassCounts.FindOrAdd(ClassKey) += 1;
		}
	}

	TArray<TSharedPtr<FJsonValue>> BodySummaries;
	if (ApiJson->TryGetArrayField(TEXT("water_bodies"), BodiesArray))
	{
		NumBodies = BodiesArray->Num();
		for (const TSharedPtr<FJsonValue>& BdVal : *BodiesArray)
		{
			TSharedPtr<FJsonObject> Bd = BdVal->AsObject();
			if (!Bd.IsValid()) continue;
			const TArray<TSharedPtr<FJsonValue>>* Rings;
			int32 PtCount = 0;
			if (Bd->TryGetArrayField(TEXT("ue5_rings"), Rings))
			{
				for (const TSharedPtr<FJsonValue>& R : *Rings)
				{
					const TArray<TSharedPtr<FJsonValue>>* RingPts;
					if (R->TryGetArray(RingPts)) PtCount += RingPts->Num();
				}
			}
			TotalBodyPoints += PtCount;

			TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
			FString Name; Bd->TryGetStringField(TEXT("name"), Name);
			FString Class; Bd->TryGetStringField(TEXT("class"), Class);
			double Area = 0; Bd->TryGetNumberField(TEXT("area_sq_m"), Area);
			Summary->SetStringField(TEXT("name"), Name);
			Summary->SetStringField(TEXT("class"), Class);
			if (Area > 0) Summary->SetNumberField(TEXT("area_sq_m"), Area);
			Summary->SetNumberField(TEXT("num_ring_points"), PtCount);
			BodySummaries.Add(MakeShared<FJsonValueObject>(Summary));
		}
	}

	// Build the compact result
	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("file"), FilePath);
	Result->SetNumberField(TEXT("file_size_bytes"), FullJson.Len());
	Result->SetNumberField(TEXT("num_waterways"), NumWaterways);
	Result->SetNumberField(TEXT("total_waterway_points"), TotalWaterwayPoints);
	Result->SetNumberField(TEXT("num_water_bodies"), NumBodies);
	Result->SetNumberField(TEXT("total_water_body_points"), TotalBodyPoints);
	Result->SetArrayField(TEXT("waterways"), WaterwaySummaries);
	Result->SetArrayField(TEXT("water_bodies"), BodySummaries);

	// Add waterway class breakdown so the AI knows what types of waterways exist
	TSharedRef<FJsonObject> ClassBreakdown = MakeShared<FJsonObject>();
	for (const auto& Pair : WaterwayClassCounts)
	{
		ClassBreakdown->SetNumberField(Pair.Key, Pair.Value);
	}
	Result->SetObjectField(TEXT("waterway_class_breakdown"), ClassBreakdown);

	Result->SetStringField(TEXT("message"), FString::Printf(
		TEXT("Water features saved to %s. Found %d waterways (%d points) and %d water bodies (%d points). "
			 "Water bodies use 'ue5_rings', waterways use 'ue5_points'. Both are origin-centered."),
		*FilePath, NumWaterways, TotalWaterwayPoints, NumBodies, TotalBodyPoints));

	Result->SetStringField(TEXT("ue5_coordinate_note"),
		TEXT("COORDINATE SYSTEM: +X=East +Y=North, map center at world origin (0,0,0). "
			 "All ue5_points and ue5_rings are in this origin-centered space."));

	FString Out;
	TSharedRef<TJsonWriter<>> OutWriter = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Result, OutWriter);
	return Out;
}

// ---------------------------------------------------------------------------
// Action: generate_heightmap
// ---------------------------------------------------------------------------
static FString ActionGenerateHeightmap(const TMap<FString, FString>& Params)
{
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

	const FTerrainHttpResult HttpResult = TerrainHttpPost(TEXT("/api/terrain/heightmap"), BodyStr);

	if (HttpResult.bNoApiKey) // Read where the request starts
		return BuildErrorJson(TEXT("NO_API_KEY"), NoApiKeyMessage());
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

	const FTerrainHttpResult HttpResult = TerrainHttpPost(TEXT("/api/terrain/preview"), BodyStr);

	if (HttpResult.bNoApiKey) // Read where the request starts
		return BuildErrorJson(TEXT("NO_API_KEY"), NoApiKeyMessage());
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

	const FTerrainHttpResult HttpResult = TerrainHttpPost(TEXT("/api/terrain/map-image"), BodyStr);

	if (HttpResult.bNoApiKey) // Read where the request starts
		return BuildErrorJson(TEXT("NO_API_KEY"), NoApiKeyMessage());
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
	const FTerrainHttpResult HttpResult = TerrainHttpGet(TEXT("/api/terrain/styles"));

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
	"Generate heightmaps, map images, and water feature data from real-world terrain. "
	"Requires an active VibeUE API key. "
	"Actions: generate_heightmap, preview_elevation, get_map_image, list_styles, get_water_features. "
	"IMPORTANT: Use the 'resolution' parameter to match your landscape resolution. "
	"Heightmap workflow: 1) preview_elevation for suggested settings (returns suggestedZScale, suggestedXYScales by resolution). "
	"2) generate_heightmap with resolution matching your landscape. "
	"3) Import via ULandscapeService.import_heightmap() (auto-creates a matching landscape if missing, but explicit create_landscape is recommended for real-world XY/Z scale control). "
	"CRITICAL: Use suggestedXYScales[resolution] as the landscape X and Y scale — do NOT use the default 100. "
	"Water workflow (after heightmap is imported): "
	"1) get_water_features with the same lng/lat/map_size — saves JSON to Saved/Terrain/ and returns a summary with the file path. "
	"2) Read the saved JSON file: json_str = open(file_path).read() "
	"3) Water bodies use 'ue5_rings', waterways use 'ue5_points'. Both are origin-centered.",
	"Terrain",
	TOOL_PARAMS(
		TOOL_PARAM("action",          "Action: generate_heightmap | preview_elevation | get_map_image | list_styles | get_water_features", "string", true),
		TOOL_PARAM("lng",             "Longitude of center point (e.g. -122.4194 for San Francisco)", "number", false),
		TOOL_PARAM("lat",             "Latitude of center point (e.g. 37.7749 for San Francisco)", "number", false),
		TOOL_PARAM("map_size",        "Map size in km (default 17.28). Use the same value for heightmap and get_water_features.", "number", false),
		TOOL_PARAM("format",          "Output format for generate_heightmap: png (default), raw, zip", "string", false),
		TOOL_PARAM("resolution",      "Output resolution NxN pixels for generate_heightmap. MUST match landscape resolution. "
		                               "Use ULandscapeService.calculate_landscape_resolution() to compute. "
		                               "Common: 505 (8x8,63,1), 1009 (8x8,63,2 or 16x16,63,1), 1017 (8x8,127,1). Default: 1081", "number", false),
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
			return BuildErrorJson(TEXT("MISSING_ACTION"),
				TEXT("'action' is required. Options: generate_heightmap, preview_elevation, get_map_image, list_styles, get_water_features"));

		if (Action == TEXT("generate_heightmap"))  return ActionGenerateHeightmap(Params);
		if (Action == TEXT("preview_elevation"))   return ActionPreviewElevation(Params);
		if (Action == TEXT("get_map_image"))       return ActionGetMapImage(Params);
		if (Action == TEXT("list_styles"))         return ActionListStyles();
		if (Action == TEXT("get_water_features"))  return ActionGetWaterFeatures(Params);

		return BuildErrorJson(TEXT("UNKNOWN_ACTION"),
			FString::Printf(TEXT("Unknown action: '%s'. Valid: generate_heightmap, preview_elevation, get_map_image, list_styles, get_water_features"), *Action));
	}
);

#if WITH_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Async/TaskGraphInterfaces.h"

// The same late-completion hazard as deep_research (VibeUE.Research.TimeoutIsSafe): a timed-out request is cancelled,
// its completion runs at a later HTTP tick, and it must write nothing the caller can see. Off the game thread, as the
// MCP bridge runs terrain_data, a cancel arriving on the game thread must wake the waiting worker at once. Test path
// prefix VibeUE.TerrainData.*

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeUETerrainDataTimeoutIsSafeTest, "VibeUE.TerrainData.TimeoutIsSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeUETerrainDataTimeoutIsSafeTest::RunTest(const FString& Parameters)
{
	// A non-routable address: the connection hangs, so the request times out (a network that rejects the address
	// outright fails it as "Connection failed" instead).
	FTerrainHttpRequestSpec Spec;
	Spec.Url = TEXT("http://10.255.255.1/");
	Spec.TimeoutSeconds = 1.0f;
	const double Start = FPlatformTime::Seconds();
	FTerrainHttpResult Result = TerrainHttpRun(Spec);
	const double Elapsed = FPlatformTime::Seconds() - Start;
	const bool bSuccessAtReturn = Result.bSuccess;
	const FString MessageAtReturn = Result.ErrorMessage;
	TestFalse(TEXT("the request did not succeed"), bSuccessAtReturn);
	TestTrue(FString::Printf(TEXT("it gave up within its time (%.2f s)"), Elapsed), Elapsed < 3.0);
	TestTrue(FString::Printf(TEXT("it says why: '%s'"), *MessageAtReturn),
		MessageAtReturn == TEXT("Request timed out") || MessageAtReturn == TEXT("Connection failed"));

	// Let the cancelled request's late completion run, then check it changed nothing after the return.
	const double PumpUntil = FPlatformTime::Seconds() + 2.0;
	while (FPlatformTime::Seconds() < PumpUntil)
	{
		FHttpModule::Get().GetHttpManager().Tick(0.0f);
		FPlatformProcess::Sleep(0.01f);
	}
	TestEqual(TEXT("the late completion did not write into the returned result (message)"), Result.ErrorMessage, MessageAtReturn);
	TestEqual(TEXT("the late completion did not write into the returned result (success)"), Result.bSuccess, bSuccessAtReturn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeUETerrainDataCancelWakesWorkerTest, "VibeUE.TerrainData.CancelWakesWorker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeUETerrainDataCancelWakesWorkerTest::RunTest(const FString& Parameters)
{
	if (FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread))
	{
		AddWarning(TEXT("Skipped: this runs inside a game-thread task, where pumping the game thread's queue would assert."));
		return true;
	}

	const TSharedRef<FVibeUEToolCancel, ESPMode::ThreadSafe> Cancel = MakeShared<FVibeUEToolCancel, ESPMode::ThreadSafe>();
	FTerrainHttpRequestSpec Spec;
	Spec.Url = TEXT("http://10.255.255.1/");
	Spec.TimeoutSeconds = 10.0f;
	TFuture<FTerrainHttpResult> Future = Async(EAsyncExecution::ThreadPool, [Cancel, Spec]()
	{
		FVibeUEToolCancel::FScope CurrentCall(&Cancel.Get());
		return TerrainHttpRun(Spec);
	});

	// Pump the game thread so the worker's request starts, then cancel from here after half a second, as the client's
	// notifications/cancelled and the exit sweep arrive.
	const double Start = FPlatformTime::Seconds();
	bool bCancelSent = false;
	while (!Future.IsReady() && FPlatformTime::Seconds() - Start < 5.0)
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FHttpModule::Get().GetHttpManager().Tick(0.0f);
		if (!bCancelSent && FPlatformTime::Seconds() - Start > 0.5)
		{
			Cancel->Cancel();
			bCancelSent = true;
		}
		FPlatformProcess::Sleep(0.01f);
	}
	const double Elapsed = FPlatformTime::Seconds() - Start;
	if (!TestTrue(FString::Printf(TEXT("the worker woke after the cancel (%.2f s)"), Elapsed), Future.IsReady()))
	{
		return false;
	}
	const FTerrainHttpResult Result = Future.Get();
	TestEqual(TEXT("it says it was cancelled"), Result.ErrorMessage, FString(TEXT("Request cancelled")));
	TestTrue(FString::Printf(TEXT("well before the request's own 10 s timeout (%.2f s)"), Elapsed), Elapsed < 3.0);

	// Let the worker's queued abandon and any late completion run while this frame is still alive.
	const double PumpUntil = FPlatformTime::Seconds() + 1.0;
	while (FPlatformTime::Seconds() < PumpUntil)
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FHttpModule::Get().GetHttpManager().Tick(0.0f);
		FPlatformProcess::Sleep(0.01f);
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
