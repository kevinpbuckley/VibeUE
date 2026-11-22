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

#if WITH_AUTOMATION_TESTS
static thread_local bool GTerrainTestEmptyKey = false;
#endif

static FString GetVibeUEApiKey()
{
	#if WITH_AUTOMATION_TESTS
	if (GTerrainTestEmptyKey) return FString();
#endif
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
	return FPaths::Combine(Dir, Filename);
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

	ApiJson->SetStringField(TEXT("ue5_coordinate_note"),
		TEXT("+X=East +Y=South, landscape center at world origin. "
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

	Result->SetStringField(TEXT("ue5_coordinate_note"),
		TEXT("COORDINATE SYSTEM: +X=East +Y=South, map center at world origin (0,0,0). "
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
	"Match resolution to the landscape. XY scale is 100000 * map_size / (resolution - 1) cm per quad. "
	"Files default to Saved/Terrain/. Vector summaries link to full geometry in origin-centred cm (+X east, +Y south). "
	"Keep returned attribution and license with derived assets.",
	"Terrain",
	TOOL_PARAMS(
		TOOL_PARAM("action",          "Action: generate_heightmap | preview_elevation | get_map_image | list_styles | get_water_features | get_map_features | get_feature_masks | get_ortho_image | generate_vista_heightmap", "string", true),
		TOOL_PARAM("lng",             "Longitude of center point (e.g. -122.4194 for San Francisco)", "number", false),
		TOOL_PARAM("lat",             "Latitude of center point (e.g. 37.7749 for San Francisco)", "number", false),
		TOOL_PARAM("map_size",        "Map size in km (default 17.28). Use the same value for heightmap and get_water_features.", "number", false),
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
		TOOL_PARAM("exact_center", "Exact geographic centre for preview and heightmap (default true). False selects the legacy crop. Vista is always exact.", "boolean", false),
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

#if WITH_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Async/TaskGraphInterfaces.h"

FString Terrain::RunWithoutKey(const FParams& Params)
{
	check(IsInGameThread());
	TGuardValue<bool> EmptyKey(GTerrainTestEmptyKey, true);
	return DispatchTerrainAction(Params);
}

// The same late-completion hazard as deep_research (VibeUE.Research.TimeoutIsSafe): a timed-out request is cancelled,
// its completion runs at a later HTTP tick, and it must write nothing the caller can see. Off the game thread, as the
// MCP bridge runs terrain_data, a cancel arriving on the game thread must wake the waiting worker at once. Test path
// prefix VibeUE.TerrainData.*

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeUETerrainDataTimeoutIsSafeTest, "VibeUE.TerrainData.TimeoutIsSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeUETerrainDataTimeoutIsSafeTest::RunTest(const FString& Parameters)
{
	// TEST-NET-1 (RFC 5737, documentation only, never a real host): the connection hangs, so the request times out.
	// The request's own HTTP timeout is longer than the helper's (TransportGraceSeconds), so the helper's wins.
	FTerrainHttpRequestSpec Spec;
	Spec.Url = TEXT("http://192.0.2.1/");
	Spec.TimeoutSeconds = 1.0f;
	const double Start = FPlatformTime::Seconds();
	FTerrainHttpResult Result = TerrainHttpRun(Spec);
	const double Elapsed = FPlatformTime::Seconds() - Start;
	const bool bSuccessAtReturn = Result.bSuccess;
	const FString MessageAtReturn = Result.ErrorMessage;
	TestFalse(TEXT("the request did not succeed"), bSuccessAtReturn);
	TestTrue(FString::Printf(TEXT("it gave up within its time (%.2f s)"), Elapsed), Elapsed < 3.0);
	TestEqual(TEXT("it says why"), MessageAtReturn, FString(TEXT("Request timed out")));

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
	Spec.Url = TEXT("http://192.0.2.1/"); // TEST-NET-1: hangs, so only the cancel ends the wait early
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
