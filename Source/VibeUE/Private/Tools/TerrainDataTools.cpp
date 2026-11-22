// Copyright Buckley Builds LLC 2026 All Rights Reserved.
//
// TerrainDataTools.cpp
// MCP tool: terrain_data — fetch and align real-world terrain assets.
// Calls vibeue.com terrain API endpoints authenticated with the user's VibeUE API key.

#include "Tools/TerrainDataInternal.h"
#include "Core/ToolRegistry.h"
#include "HAL/PlatformTime.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/ConfigCacheIni.h"
#include "Templates/UnrealTemplate.h"
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
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Core/VibeUEToolCancel.h"
#include <atomic>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace Terrain = VibeUE::Terrain;

FString Terrain::Param(const TMap<FString, FString>& Params, const FString& FieldName, const FString& Default)
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
			const TArray<TSharedPtr<FJsonValue>>* List;
			if (JsonObj->TryGetArrayField(FieldName, List))
			{
				FString ArrayText;
				FJsonSerializer::Serialize(*List, TJsonWriterFactory<>::Create(&ArrayText));
				return ArrayText;
			}
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

double Terrain::Number(const TMap<FString, FString>& Params, const FString& Name, double Default)
{
	FString V = Terrain::Param(Params, Name);
	return V.IsEmpty() ? Default : FCString::Atod(*V);
}

int32 Terrain::Integer(const TMap<FString, FString>& Params, const FString& Name, int32 Default)
{
	FString V = Terrain::Param(Params, Name);
	return V.IsEmpty() ? Default : FCString::Atoi(*V);
}

bool Terrain::Boolean(const TMap<FString, FString>& Params, const FString& Name, bool Default)
{
	FString V = Terrain::Param(Params, Name);
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

FString Terrain::Error(const FString& Code, const FString& Message)
{
	const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("success"), false);
	Data->SetStringField(TEXT("error"), Code);
	Data->SetStringField(TEXT("message"), Message);
	return Serialize(Data);
}

FString Terrain::Serialize(const TSharedRef<FJsonObject>& Data)
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
bool FTerrainHttpResult::HasContentType(const FString& Expected) const
{
	FString Mime = ContentType;
	int32 Separator;
	if (Mime.FindChar(TEXT(';'), Separator)) Mime.LeftInline(Separator);
	return Mime.TrimStartAndEnd().Equals(Expected, ESearchCase::IgnoreCase);
}

// What one request needs, built on any thread. The request itself is made on the game thread, which also reads the
// API key (a settings CDO field, written there).
struct FTerrainHttpRequestSpec
{
	FString Url;
	FString Verb = TEXT("GET");
	FString JsonBody;           // POST only
	bool bSendApiKey = false;   // when none is configured, the request is not made and the result says bNoApiKey
	bool bKeepTerrainHeaders = false;
	float TimeoutSeconds = 30.0f;
	float TransportGraceSeconds = 2.0f;
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

// Existing requests give the wait helper a small head start on the transport timeout.
// Contract requests use zero transport grace so HTTP itself enforces the 60-second deadline.
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
	Request->SetTimeout(Spec.TimeoutSeconds + Spec.TransportGraceSeconds);
	// The engine also fails a request that receives nothing for [HTTP] HttpActivityTimeout seconds (30 by default) with
	// "Connection failed", whatever its total timeout: a cold orthophoto tile, a vista or a slow vector query sends its
	// first byte later than that. The silence allowed is the request's whole time.
	Request->SetActivityTimeout(Spec.TimeoutSeconds + Spec.TransportGraceSeconds);

	const TWeakPtr<FTerrainHttpState, ESPMode::ThreadSafe> WeakState = State;
	const bool bKeepTerrainHeaders = Spec.bKeepTerrainHeaders;
	Request->OnProcessRequestComplete().BindLambda(
		[WeakState, bKeepTerrainHeaders](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bConnected)
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
				if (bKeepTerrainHeaders)
				{
					for (const TCHAR* Name : {TEXT("X-Heightmap-Min-Height"), TEXT("X-Heightmap-Max-Height"),
						TEXT("X-Heightmap-Size"), TEXT("X-Heightmap-Alignment"), TEXT("X-Vista-Alignment"),
						TEXT("X-Mask-Resolution"), TEXT("X-Terrain-Source"), TEXT("X-Ortho-Source"), TEXT("X-Terrain-Attribution")})
					{
						Result.Headers.Add(Name, Resp->GetHeader(Name));
					}
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
		return MoveTemp(State->Result);
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
		return MoveTemp(State->Result);
	}
	AsyncTask(ENamedThreads::GameThread, [State]() { AbandonTerrainRequest(State); });
	FTerrainHttpResult Failed;
	Failed.ErrorMessage = State->bCancelled ? TEXT("Request cancelled") : TEXT("Request timed out");
	return Failed;
}

FTerrainHttpResult Terrain::TerrainHttpPost(const FString& Path, const FString& JsonBody, float TimeoutSeconds, float TransportGraceSeconds)
{
	FTerrainHttpRequestSpec Spec;
	Spec.Url = GetTerrainBaseUrl() + Path; // GConfig reads take the config lock, safe on a worker
	Spec.Verb = TEXT("POST");
	Spec.JsonBody = JsonBody;
	Spec.bSendApiKey = true;
	Spec.bKeepTerrainHeaders = true;
	Spec.TransportGraceSeconds = TransportGraceSeconds;
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
// Save path resolution
// ---------------------------------------------------------------------------
FString Terrain::SavePath(const FString& RequestedPath, const FString& Filename)
{
	if (!RequestedPath.IsEmpty()) return RequestedPath;
	FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Terrain"));
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (!PF.DirectoryExists(*Dir)) PF.CreateDirectory(*Dir);
	// A full path: ProjectSavedDir is relative to the engine's Binaries folder, which means nothing to an MCP client
	// that opens the file with its own tools from outside the editor.
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(Dir, Filename));
}

// Off the game thread two terrain_data calls run at once, and two for the same place share a default file name: one
// save at a time, so neither fails on the other's open file (the last one wins, as when the calls ran in turn).
static FCriticalSection GTerrainDataSaveLock;

bool Terrain::SaveBytes(TArrayView64<const uint8> Bytes, const FString& FilePath)
{
	FScopeLock SaveGuard(&GTerrainDataSaveLock);
	return FFileHelper::SaveArrayToFile(Bytes, *FilePath);
}

bool Terrain::SaveText(const FString& Text, const FString& FilePath)
{
	FScopeLock SaveGuard(&GTerrainDataSaveLock);
	return FFileHelper::SaveStringToFile(Text, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

// ---------------------------------------------------------------------------
// The coordinates get_water_features has always written, and still writes unless landscape_space=true: an
// equirectangular approximation with +Y NORTH, which is mirrored north-south on a landscape imported north-first (row
// zero at minimum Y). Kept bit for bit, float rounding included, so a saved script gets the file it always got.
// ---------------------------------------------------------------------------
static TSharedRef<FJsonObject> TerrainLegacyWaterPoint(const TSharedPtr<FJsonObject>& Pt, double CenterLng, double CenterLat)
{
	double PtLng = 0.0, PtLat = 0.0;
	Pt->TryGetNumberField(TEXT("lng"), PtLng);
	Pt->TryGetNumberField(TEXT("lat"), PtLat);
	const double MetersPerDegree = 111320.0;
	const double CosLat = FMath::Cos(FMath::DegreesToRadians(CenterLat));
	const FVector V(
		(float)((PtLng - CenterLng) * MetersPerDegree * CosLat * 100.0),  // +X = East
		(float)((PtLat - CenterLat) * MetersPerDegree * 100.0),           // +Y = North
		0.0f);
	TSharedRef<FJsonObject> UE5Pt = MakeShared<FJsonObject>();
	UE5Pt->SetNumberField(TEXT("x"), V.X);
	UE5Pt->SetNumberField(TEXT("y"), V.Y);
	UE5Pt->SetNumberField(TEXT("z"), V.Z);
	return UE5Pt;
}

static TArray<TSharedPtr<FJsonValue>> TerrainLegacyWaterLine(const TArray<TSharedPtr<FJsonValue>>& Points, double CenterLng, double CenterLat)
{
	TArray<TSharedPtr<FJsonValue>> Line;
	Line.Reserve(Points.Num());
	for (const TSharedPtr<FJsonValue>& PtVal : Points)
	{
		const TSharedPtr<FJsonObject> Pt = PtVal->AsObject();
		if (Pt.IsValid()) Line.Add(MakeShared<FJsonValueObject>(TerrainLegacyWaterPoint(Pt, CenterLng, CenterLat)));
	}
	return Line;
}

static void AddLegacyWaterCoordinates(FJsonObject& ApiJson, double CenterLng, double CenterLat)
{
	const TArray<TSharedPtr<FJsonValue>>* Waterways;
	if (ApiJson.TryGetArrayField(TEXT("waterways"), Waterways))
	{
		for (const TSharedPtr<FJsonValue>& WwVal : *Waterways)
		{
			const TSharedPtr<FJsonObject> Ww = WwVal->AsObject();
			const TArray<TSharedPtr<FJsonValue>>* Points;
			if (!Ww.IsValid() || !Ww->TryGetArrayField(TEXT("points"), Points)) continue;
			Ww->SetArrayField(TEXT("ue5_points"), TerrainLegacyWaterLine(*Points, CenterLng, CenterLat));
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* Bodies;
	if (ApiJson.TryGetArrayField(TEXT("water_bodies"), Bodies))
	{
		for (const TSharedPtr<FJsonValue>& BdVal : *Bodies)
		{
			const TSharedPtr<FJsonObject> Bd = BdVal->AsObject();
			const TArray<TSharedPtr<FJsonValue>>* Rings;
			if (!Bd.IsValid() || !Bd->TryGetArrayField(TEXT("rings"), Rings)) continue;
			TArray<TSharedPtr<FJsonValue>> UE5Rings;
			UE5Rings.Reserve(Rings->Num());
			for (const TSharedPtr<FJsonValue>& RingVal : *Rings)
			{
				const TArray<TSharedPtr<FJsonValue>>* Points;
				if (!RingVal->TryGetArray(Points)) continue;
				UE5Rings.Add(MakeShared<FJsonValueArray>(TerrainLegacyWaterLine(*Points, CenterLng, CenterLat)));
			}
			Bd->SetArrayField(TEXT("ue5_rings"), UE5Rings);
		}
	}
}

// ---------------------------------------------------------------------------
// Action: get_water_features
// ---------------------------------------------------------------------------
static FString ActionGetWaterFeatures(const TMap<FString, FString>& Params)
{
	const double Lng = Terrain::Number(Params, TEXT("lng"), 0.0);
	const double Lat = Terrain::Number(Params, TEXT("lat"), 0.0);
	if (Terrain::Param(Params, TEXT("lat")).IsEmpty())
		return Terrain::Error(TEXT("MISSING_PARAMS"), TEXT("lat and lng are required."));

	const double MapSize = Terrain::Number(Params, TEXT("map_size"), 17.28);

	const FString BodyStr = Terrain::Serialize(Terrain::WaterBody(Params));

	// Use a longer timeout — PBF tiles may be fetched across multiple HTTP round-trips
	const FTerrainHttpResult HttpResult = Terrain::TerrainHttpPost(TEXT("/api/terrain/water-features"), BodyStr, 45.0f);

	if (HttpResult.bNoApiKey) // Read where the request starts
		return Terrain::Error(TEXT("NO_API_KEY"), NoApiKeyMessage());
	if (!HttpResult.bSuccess)
		return Terrain::Error(TEXT("HTTP_ERROR"), HttpResult.ErrorMessage);

	if (HttpResult.ResponseCode != 200)
	{
		TArray<uint8> ContentCopy = HttpResult.Content;
		ContentCopy.Add(0);
		const FString ErrBody = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ContentCopy.GetData())));
		return Terrain::Error(
			FString::Printf(TEXT("HTTP_%d"), HttpResult.ResponseCode),
			ErrBody.IsEmpty() ? FString::Printf(TEXT("Server returned %d"), HttpResult.ResponseCode) : ErrBody);
	}

	if (!HttpResult.HasContentType(TEXT("application/json")))
		return Terrain::Error(TEXT("INVALID_RESPONSE"), TEXT("Expected JSON water features."));

	// Parse the API response and inject UE5 world-space coordinates
	TArray<uint8> ContentCopy = HttpResult.Content;
	ContentCopy.Add(0);
	const FString JsonStr = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ContentCopy.GetData())));

	TSharedPtr<FJsonObject> ApiJson;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
	if (!FJsonSerializer::Deserialize(Reader, ApiJson) || !ApiJson.IsValid())
		return Terrain::Error(TEXT("PARSE_ERROR"), TEXT("Failed to parse server response."));

	// landscape_space=true: the landscape's own axes, the same conversion get_map_features uses, so water lies where an
	// exact heightmap, its masks and its orthophoto put it. Default (false): the file every older build wrote.
	const bool bLandscapeAxes = Terrain::Boolean(Params, TEXT("landscape_space"), false);
	if (bLandscapeAxes)
	{
		for (const TCHAR* Field : {TEXT("waterways"), TEXT("water_bodies")})
		{
			const TArray<TSharedPtr<FJsonValue>>* Features;
			if (!ApiJson->TryGetArrayField(Field, Features)) continue;
			for (const TSharedPtr<FJsonValue>& Value : *Features)
			{
				if (const TSharedPtr<FJsonObject> Feature = Value->AsObject())
					Terrain::AddCoordinates(*Feature, Lng, Lat, FMath::Clamp(MapSize, 1.0, 100.0));
			}
		}
	}
	else
	{
		AddLegacyWaterCoordinates(*ApiJson, Lng, Lat);
	}

	ApiJson->SetStringField(TEXT("ue5_coordinate_note"), bLandscapeAxes
		? TEXT("+X=East +Y=South (north is negative Y, as on a landscape imported from the heightmap), landscape center at world origin. "
		       "If landscape actor is offset, add its Location from "
		       "ULandscapeService.get_landscape_info() to each ue5_point/ue5_ring vertex.")
		: TEXT("+X=East +Y=North, landscape center at world origin. "
		       "If landscape actor is offset, add its Location from "
		       "ULandscapeService.get_landscape_info() to each ue5_point/ue5_ring vertex."));

	// Serialize the full JSON (with injected UE5 coordinates)
	FString FullJson;
	{
		TSharedRef<TJsonWriter<>> FullWriter = TJsonWriterFactory<>::Create(&FullJson);
		FJsonSerializer::Serialize(ApiJson.ToSharedRef(), FullWriter);
	}

	// Save full JSON to file (same pattern as heightmap/map_image)
	const FString SavePath = Terrain::Param(Params, TEXT("save_path"));
	const FString DefaultFilename = FString::Printf(TEXT("water_features_%.4f_%.4f_%gkm.json"), Lat, Lng, MapSize);
	const FString FilePath = Terrain::SavePath(SavePath, DefaultFilename);

	if (!Terrain::SaveText(FullJson, FilePath))
		return Terrain::Error(TEXT("SAVE_ERROR"), FString::Printf(TEXT("Failed to save water features to: %s"), *FilePath));

	// Build a compact summary to return (not the full JSON — that can be 200K+ tokens)
	int32 NumWaterways = 0;
	int32 TotalWaterwayPoints = 0;
	int32 NumBodies = 0;
	int32 TotalBodyPoints = 0;

	// Track class breakdown for waterways
	TMap<FString, int32> WaterwayClassCounts;

	const TArray<TSharedPtr<FJsonValue>>* WaterwaysArray;
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

	const TArray<TSharedPtr<FJsonValue>>* BodiesArray;
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

	Result->SetStringField(TEXT("ue5_coordinate_note"), bLandscapeAxes
		? TEXT("COORDINATE SYSTEM: +X=East +Y=South (north is negative Y), map center at world origin (0,0,0). "
			   "All ue5_points and ue5_rings are in this origin-centered space, the one get_map_features uses.")
		: TEXT("COORDINATE SYSTEM: +X=East +Y=North, map center at world origin (0,0,0). "
			   "All ue5_points and ue5_rings are in this origin-centered space. "
			   "On a landscape imported from the heightmap north is NEGATIVE Y: pass landscape_space=true for coordinates on the landscape's axes (+Y=South)."));

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
	const double Lng = Terrain::Number(Params, TEXT("lng"), 0.0);
	const double Lat = Terrain::Number(Params, TEXT("lat"), 0.0);
	if (Lat == 0.0 && Lng == 0.0 && Terrain::Param(Params, TEXT("lat")).IsEmpty())
		return Terrain::Error(TEXT("MISSING_PARAMS"), TEXT("lat and lng are required."));

	const FString Format = Terrain::Param(Params, TEXT("format"), TEXT("png"));
	const FString SavePath = Terrain::Param(Params, TEXT("save_path"));
	const FTerrainHttpResult HttpResult = Terrain::TerrainHttpPost(TEXT("/api/terrain/heightmap"),
		Terrain::Serialize(Terrain::HeightBody(Params, false)));

	if (HttpResult.bNoApiKey) // Read where the request starts
		return Terrain::Error(TEXT("NO_API_KEY"), NoApiKeyMessage());
	if (!HttpResult.bSuccess)
		return Terrain::Error(TEXT("HTTP_ERROR"), HttpResult.ErrorMessage);

	if (HttpResult.ResponseCode != 200)
	{
		FString ErrMsg;
		if (HttpResult.Content.Num() > 0)
		{
			TArray<uint8> ContentCopy = HttpResult.Content;
			ContentCopy.Add(0);
			ErrMsg = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ContentCopy.GetData())));
		}
		return Terrain::Error(
			FString::Printf(TEXT("HTTP_%d"), HttpResult.ResponseCode),
			ErrMsg.IsEmpty() ? FString::Printf(TEXT("Server returned %d"), HttpResult.ResponseCode) : ErrMsg
		);
	}

	const TCHAR* Expected = Format == TEXT("raw") ? TEXT("application/octet-stream") :
		(Format == TEXT("zip") ? TEXT("application/zip") : TEXT("image/png"));
	if (!HttpResult.HasContentType(Expected) || HttpResult.Content.IsEmpty())
		return Terrain::Error(TEXT("INVALID_RESPONSE"), TEXT("Unexpected heightmap response format."));

	// Determine file extension
	FString Ext = Format;
	const FString DefaultFilename = FString::Printf(TEXT("heightmap_%.4f_%.4f.%s"), Lat, Lng, *Ext);
	const FString FilePath = Terrain::SavePath(SavePath, DefaultFilename);

	if (!Terrain::SaveBytes(HttpResult.Content, FilePath))
		return Terrain::Error(TEXT("SAVE_ERROR"), FString::Printf(TEXT("Failed to save to: %s"), *FilePath));

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

	const FString Alignment = HttpResult.Headers.FindRef(TEXT("X-Heightmap-Alignment"));
	if (!Alignment.IsEmpty()) Out->SetStringField(TEXT("alignment"), Alignment);
	Out->SetStringField(TEXT("message"), FString::Printf(
		TEXT("Heightmap saved to %s. Import via Edit > Import Heightmap in the Landscape editor."), *FilePath));

	return Terrain::Serialize(Out);
}

// ---------------------------------------------------------------------------
// Action: preview_elevation
// ---------------------------------------------------------------------------
static FString ActionPreviewElevation(const TMap<FString, FString>& Params)
{
	if (Terrain::Param(Params, TEXT("lat")).IsEmpty())
		return Terrain::Error(TEXT("MISSING_PARAMS"), TEXT("lat and lng are required."));
	const FTerrainHttpResult HttpResult = Terrain::TerrainHttpPost(TEXT("/api/terrain/preview"), Terrain::Serialize(Terrain::PreviewBody(Params)));

	if (HttpResult.bNoApiKey) // Read where the request starts
		return Terrain::Error(TEXT("NO_API_KEY"), NoApiKeyMessage());
	if (!HttpResult.bSuccess)
		return Terrain::Error(TEXT("HTTP_ERROR"), HttpResult.ErrorMessage);

	if (HttpResult.ResponseCode != 200)
		return Terrain::Error(
			FString::Printf(TEXT("HTTP_%d"), HttpResult.ResponseCode),
			FString::Printf(TEXT("Server returned %d"), HttpResult.ResponseCode));

	if (!HttpResult.HasContentType(TEXT("application/json")))
		return Terrain::Error(TEXT("INVALID_RESPONSE"), TEXT("Expected JSON elevation preview."));

	// Preserve all response fields, including new fields added by the server.
	TArray<uint8> ContentCopy = HttpResult.Content;
	ContentCopy.Add(0);
	const FString Json(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ContentCopy.GetData())));
	const FString Alignment = HttpResult.Headers.FindRef(TEXT("X-Heightmap-Alignment"));
	if (Alignment.IsEmpty()) return Json;
	TSharedPtr<FJsonObject> Preview;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Preview) || !Preview)
		return Terrain::Error(TEXT("PARSE_ERROR"), TEXT("Invalid elevation preview JSON."));
	Preview->SetStringField(TEXT("alignment"), Alignment);
	return Terrain::Serialize(Preview.ToSharedRef());
}

// ---------------------------------------------------------------------------
// Action: get_map_image
// ---------------------------------------------------------------------------
static FString ActionGetMapImage(const TMap<FString, FString>& Params)
{
	const double Lng     = Terrain::Number(Params, TEXT("lng"),      0.0);
	const double Lat     = Terrain::Number(Params, TEXT("lat"),      0.0);
	const double MapSize = Terrain::Number(Params, TEXT("map_size"), 17.28);
	const FString Style  = Terrain::Param(Params, TEXT("style"),    TEXT("satellite-v9"));
	const int32  Width   = Terrain::Integer(Params, TEXT("width"),    1280);
	const int32  Height  = Terrain::Integer(Params, TEXT("height"),   1280);
	const FString SavePath = Terrain::Param(Params, TEXT("save_path"));

	if (Terrain::Param(Params, TEXT("lat")).IsEmpty())
		return Terrain::Error(TEXT("MISSING_PARAMS"), TEXT("lat and lng are required."));

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

	const FTerrainHttpResult HttpResult = Terrain::TerrainHttpPost(TEXT("/api/terrain/map-image"), BodyStr);

	if (HttpResult.bNoApiKey) // Read where the request starts
		return Terrain::Error(TEXT("NO_API_KEY"), NoApiKeyMessage());
	if (!HttpResult.bSuccess)
		return Terrain::Error(TEXT("HTTP_ERROR"), HttpResult.ErrorMessage);

	if (HttpResult.ResponseCode != 200)
		return Terrain::Error(
			FString::Printf(TEXT("HTTP_%d"), HttpResult.ResponseCode),
			FString::Printf(TEXT("Server returned %d"), HttpResult.ResponseCode));

	const FString StyleTag = Style.Replace(TEXT("-"), TEXT("_")).Replace(TEXT("."), TEXT("_"));
	const FString DefaultFilename = FString::Printf(TEXT("map_%s_%.4f_%.4f.png"), *StyleTag, Lat, Lng);
	const FString FilePath = Terrain::SavePath(SavePath, DefaultFilename);

	if (!Terrain::SaveBytes(HttpResult.Content, FilePath))
		return Terrain::Error(TEXT("SAVE_ERROR"), FString::Printf(TEXT("Failed to save to: %s"), *FilePath));

	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField  (TEXT("success"),    true);
	Out->SetStringField(TEXT("file"),       FilePath);
	Out->SetStringField(TEXT("style"),      Style);
	Out->SetNumberField(TEXT("size_bytes"), HttpResult.Content.Num());
	Out->SetStringField(TEXT("message"),    FString::Printf(TEXT("Map image saved to %s"), *FilePath));

	return Terrain::Serialize(Out);
}

// ---------------------------------------------------------------------------
// Action: list_styles
// ---------------------------------------------------------------------------
static FString ActionListStyles()
{
	const FTerrainHttpResult HttpResult = TerrainHttpGet(TEXT("/api/terrain/styles"));

	if (!HttpResult.bSuccess || HttpResult.ResponseCode != 200)
		return Terrain::Error(TEXT("HTTP_ERROR"), TEXT("Failed to fetch styles list."));

	TArray<uint8> ContentCopy = HttpResult.Content;
	ContentCopy.Add(0);
	return FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ContentCopy.GetData())));
}

// ---------------------------------------------------------------------------
// Tool registration
// ---------------------------------------------------------------------------
static FString DispatchTerrainAction(const TMap<FString, FString>& Params)
{
	const FString Action = Terrain::Param(Params, TEXT("action")).ToLower().TrimStartAndEnd();

	if (Action.IsEmpty())
		return Terrain::Error(TEXT("MISSING_ACTION"),
			TEXT("'action' is required. Options: generate_heightmap, preview_elevation, get_map_image, list_styles, get_water_features, get_map_features, get_feature_masks, get_ortho_image, generate_vista_heightmap"));

	if (Action == TEXT("generate_heightmap"))  return ActionGenerateHeightmap(Params);
	if (Action == TEXT("preview_elevation"))   return ActionPreviewElevation(Params);
	if (Action == TEXT("get_map_image"))       return ActionGetMapImage(Params);
	if (Action == TEXT("list_styles"))         return ActionListStyles();
	if (Action == TEXT("get_water_features"))  return ActionGetWaterFeatures(Params);

	if (Action == TEXT("get_map_features") || Action == TEXT("get_feature_masks") ||
		Action == TEXT("get_ortho_image") || Action == TEXT("generate_vista_heightmap"))
	{
		if (Terrain::Param(Params, TEXT("lng")).IsEmpty() || Terrain::Param(Params, TEXT("lat")).IsEmpty())
			return Terrain::Error(TEXT("MISSING_PARAMS"), TEXT("lat and lng are required."));
		if (Action == TEXT("get_map_features")) return Terrain::GetMapFeatures(Params);
		if (Action == TEXT("get_feature_masks")) return Terrain::GetFeatureMasks(Params);
		if (Action == TEXT("get_ortho_image")) return Terrain::GetOrthoImage(Params);
		return Terrain::GenerateVista(Params);
	}

	return Terrain::Error(TEXT("UNKNOWN_ACTION"),
		FString::Printf(TEXT("Unknown action: '%s'. Valid: generate_heightmap, preview_elevation, get_map_image, list_styles, get_water_features, get_map_features, get_feature_masks, get_ortho_image, generate_vista_heightmap"), *Action));
}

REGISTER_VIBEUE_TOOL(terrain_data,
	"Fetch real-world terrain using one VibeUE API key. Preview elevation, generate an exact-centred heightmap, "
	"download aligned orthophotos, canopy, masks and map vectors, fetch water geometry, or generate a vista backdrop. "
	"get_map_image and list_styles provide quick reference maps. Use matching lng, lat and map_size. "
	"Match resolution to the landscape. XY scale is 100000 * map_size / (resolution - 1) cm per quad, Z scale is 20000 / height_scale. "
	"Files default to Saved/Terrain/. Vector summaries link to full geometry in origin-centred cm: get_map_features uses the landscape's axes (+X east, +Y south); "
	"get_water_features does with landscape_space=true (its default keeps the older +Y north output). "
	"exact_center=false gives the off-centre heightmap crop older builds returned. "
	"Keep returned attribution and license with derived assets.",
	"Terrain",
	TOOL_PARAMS(
		TOOL_PARAM("action",          "Action: generate_heightmap | preview_elevation | get_map_image | list_styles | get_water_features | get_map_features | get_feature_masks | get_ortho_image | generate_vista_heightmap", "string", true),
		TOOL_PARAM("lng",             "Longitude of center point (e.g. -122.4194 for San Francisco)", "number", false),
		TOOL_PARAM("lat",             "Latitude of center point (e.g. 37.7749 for San Francisco)", "number", false),
		TOOL_PARAM("map_size",        "Map size in km (default 17.28). Use the same value for every action of one landscape (for a vista it is the inner, playable side).", "number", false),
		TOOL_PARAM("format",          "Heightmap or vista output format: png (default), raw, zip", "string", false),
		TOOL_PARAM("resolution",      "Samples per side for heightmap, vista or masks. Match the playable heightmap to its landscape. "
		                               "Use ULandscapeService.calculate_landscape_resolution() to compute. "
		                               "Common: 505 (8x8,63,1), 1009 (8x8,63,2 or 16x16,63,1), 1017 (8x8,127,1). Default: 1081 for heightmaps and vista, 1009 for masks", "number", false),
		TOOL_PARAM("base_level",      "Base elevation offset in meters (default 0; use preview_elevation for good value)", "number", false),
		TOOL_PARAM("height_scale",    "Height scale percentage 1-250 (default 100; use preview_elevation for good value)", "number", false),
		TOOL_PARAM("water_depth",     "Water depth in Cities: Skylines units (default 40)", "number", false),
		TOOL_PARAM("gravity_center",  "Water flow direction 0-13: 0=disabled, 1=center, 2=N, 3=NE, 4=E, 5=SE, 6=S, 7=SW, 8=W, 9=NW, 10=north side, 11=east side, 12=south side, 13=west side", "number", false),
		TOOL_PARAM("level_correction","Elevation curve style 0-9: 0=none, 2=coastline, 3=aggressive coastline (default 0)", "number", false),
		TOOL_PARAM("blur_passes",     "Smoothing passes for plains (default 10)", "number", false),
		TOOL_PARAM("blur_post_passes","Post-sharpening passes (default 2)", "number", false),
		TOOL_PARAM("sharpen",         "Apply sharpening kernel (default true)", "boolean", false),
		TOOL_PARAM("draw_streams",    "Request stream processing (default true, reserved by the current encoder)", "boolean", false),
		TOOL_PARAM("stream_depth",    "Stream depth in meters (default 7)", "number", false),
		TOOL_PARAM("plains_height",   "Height threshold for plains smoothing in meters (default 140)", "number", false),
		TOOL_PARAM("exact_center", "Exact geographic centre for preview and heightmap (default true; the reply says alignment exact). False selects the off-centre legacy crop older builds returned. Vista, masks, map features and orthophotos are always exact.", "boolean", false),
		TOOL_PARAM("landscape_space", "get_water_features only. Default false: ue5_points / ue5_rings exactly as older builds wrote them (+X east, +Y NORTH: mirrored north-south on a landscape imported from the heightmap). True: the landscape's own axes (+X east, +Y south, north is negative Y), the convention get_map_features, the masks and the orthophoto use. Pass true whenever the water has to line up with the landscape.", "boolean", false),
		TOOL_PARAM("min_spacing_m", "Minimum vertex spacing in metres for water and map features. Zero keeps full detail.", "number", false),
		TOOL_PARAM("detail", "Water feature detail: standard (default) or high.", "string", false),
		TOOL_PARAM("categories", "Comma list or string array: roads, tracks_paths, railways, buildings, parking, landcover, barriers, pois, water. Omit for server defaults.", "string_or_array", false),
		TOOL_PARAM("max_features", "Map features per category (server default 5000).", "integer", false),
		TOOL_PARAM("prefer", "Vector/mask source preference: auto (default), openstreetmap, tiles.", "string", false),
		TOOL_PARAM("layers", "Mask layers as a comma list: surface, landcover, parking, road_distance (default all).", "string", false),
		TOOL_PARAM("size", "Orthophoto pixels per side: 1024, 2048 (default), 4096, 8192.", "integer", false),
		TOOL_PARAM("source", "Orthophoto source: auto (default), naip, mapbox.", "string", false),
		TOOL_PARAM("band", "Orthophoto bands: rgb (default), canopy, both. Canopy requires NAIP.", "string", false),
		TOOL_PARAM("vista_factor", "Outer/inner side ratio 2-16 (default 8), outer side capped at 100 km.", "number", false),
		TOOL_PARAM("style",           "Map image style for get_map_image: satellite-v9, outdoors-v11, streets-v11, light-v10, dark-v10", "string", false),
		TOOL_PARAM("save_path",       "Single-output file path for heightmap, map image, vectors or vista. Default: <ProjectDir>/Saved/Terrain/", "string", false)
	),
	{ return DispatchTerrainAction(Params); }
);
