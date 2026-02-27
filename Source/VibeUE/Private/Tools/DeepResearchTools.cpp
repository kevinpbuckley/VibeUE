// Copyright Buckley Builds LLC 2026 All Rights Reserved.
//
// DeepResearchTools.cpp
// MCP tool: deep_research — web research and GPS geocoding with no API key required.
//
// Actions:
//   search          — DuckDuckGo Instant Answer API (free, no key)
//   fetch_page      — Jina AI Reader: converts any URL to clean markdown (free, no key)
//   geocode         — OpenStreetMap Nominatim: place name → lat/lng (free, no key)
//   reverse_geocode — OpenStreetMap Nominatim: lat/lng → place name (free, no key)

#include "Core/ToolRegistry.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "HttpModule.h"
#include "HttpManager.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HAL/PlatformProcess.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static FString ExtractResearchParam(const TMap<FString, FString>& Params, const FString& FieldName, const FString& Default = FString())
{
	const FString* Direct = Params.Find(FieldName);
	if (Direct) return *Direct;

	// MCP server sometimes capitalizes first letter
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
			// Numbers (lat, lng, etc.) arrive as JSON number values
			double NumValue;
			if (JsonObj->TryGetNumberField(FieldName, NumValue))
				return FString::Printf(TEXT("%.10g"), NumValue);
			// Booleans arrive as JSON bool values
			bool BoolValue;
			if (JsonObj->TryGetBoolField(FieldName, BoolValue))
				return BoolValue ? TEXT("true") : TEXT("false");
		}
	}

	return Default;
}

static double ExtractResearchDouble(const TMap<FString, FString>& Params, const FString& Name, double Default)
{
	FString V = ExtractResearchParam(Params, Name);
	return V.IsEmpty() ? Default : FCString::Atod(*V);
}

static FString BuildResearchError(const FString& Code, const FString& Message)
{
	// Escape the message to avoid breaking JSON
	FString SafeMsg = Message.Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n"));
	return FString::Printf(TEXT("{\"success\":false,\"error\":\"%s\",\"message\":\"%s\"}"), *Code, *SafeMsg);
}

// Minimal URL encoder — handles spaces and the few special chars common in queries/URLs
static FString UrlEncodeSimple(const FString& Input)
{
	FString Out;
	Out.Reserve(Input.Len() * 2);
	for (TCHAR Ch : Input)
	{
		if (FChar::IsAlpha(Ch) || FChar::IsDigit(Ch) ||
			Ch == TEXT('-') || Ch == TEXT('_') || Ch == TEXT('.') || Ch == TEXT('~'))
		{
			Out.AppendChar(Ch);
		}
		else if (Ch == TEXT(' '))
		{
			Out.AppendChar(TEXT('+'));
		}
		else
		{
			// Percent-encode as UTF-8 (ASCII range sufficient for typical queries)
			Out.Appendf(TEXT("%%%02X"), static_cast<uint8>(Ch));
		}
	}
	return Out;
}

// ---------------------------------------------------------------------------
// HTTP helper — blocking GET, returns response body as string
// ---------------------------------------------------------------------------
struct FResearchHttpResult
{
	bool    bSuccess      = false;
	int32   ResponseCode  = 0;
	FString Body;
	FString ErrorMessage;
};

static FResearchHttpResult ResearchHttpGet(
	const FString& Url,
	const FString& UserAgent = TEXT("VibeUE/1.0 (Unreal Engine plugin)"),
	const TArray<TPair<FString, FString>>& ExtraHeaders = {},
	float TimeoutSeconds = 30.0f)
{
	FResearchHttpResult Result;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	if (!UserAgent.IsEmpty())
		Request->SetHeader(TEXT("User-Agent"), UserAgent);
	for (const auto& KV : ExtraHeaders)
		Request->SetHeader(KV.Key, KV.Value);

	bool bComplete = false;
	Request->OnProcessRequestComplete().BindLambda(
		[&Result, &bComplete](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				Result.ErrorMessage = TEXT("Connection failed");
			}
			else
			{
				Result.bSuccess     = true;
				Result.ResponseCode = Resp->GetResponseCode();
				Result.Body         = Resp->GetContentAsString();
			}
			bComplete = true;
		}
	);

	Request->ProcessRequest();

	const double Start = FPlatformTime::Seconds();
	while (!bComplete)
	{
		FHttpModule::Get().GetHttpManager().Tick(0.0f);
		FPlatformProcess::Sleep(0.01f);
		if (FPlatformTime::Seconds() - Start > TimeoutSeconds)
		{
			Request->CancelRequest();
			Result.ErrorMessage = TEXT("Request timed out");
			return Result;
		}
	}

	return Result;
}

// ---------------------------------------------------------------------------
// Action: search  (DuckDuckGo Instant Answer API)
// ---------------------------------------------------------------------------
static FString ActionSearch(const TMap<FString, FString>& Params)
{
	const FString Query = ExtractResearchParam(Params, TEXT("query"));
	if (Query.IsEmpty())
		return BuildResearchError(TEXT("MISSING_PARAMS"), TEXT("'query' is required for the search action."));

	const FString Encoded = UrlEncodeSimple(Query);
	const FString Url = FString::Printf(
		TEXT("https://api.duckduckgo.com/?q=%s&format=json&no_html=1&skip_disambig=1&no_redirect=1"),
		*Encoded);

	const FResearchHttpResult Http = ResearchHttpGet(Url, TEXT("VibeUE/1.0 (Unreal Engine plugin)"), {}, 15.0f);

	if (!Http.bSuccess)
		return BuildResearchError(TEXT("HTTP_ERROR"), Http.ErrorMessage);

	if (Http.ResponseCode != 200)
		return BuildResearchError(
			FString::Printf(TEXT("HTTP_%d"), Http.ResponseCode),
			FString::Printf(TEXT("DuckDuckGo returned %d"), Http.ResponseCode));

	TSharedPtr<FJsonObject> DDG;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Http.Body);
	if (!FJsonSerializer::Deserialize(Reader, DDG) || !DDG.IsValid())
		return BuildResearchError(TEXT("PARSE_ERROR"), TEXT("Failed to parse DuckDuckGo response."));

	// Build a clean output object
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("query"), Query);

	FString Heading, AbstractText, AbstractUrl, Answer;
	DDG->TryGetStringField(TEXT("Heading"),      Heading);
	DDG->TryGetStringField(TEXT("AbstractText"),  AbstractText);
	DDG->TryGetStringField(TEXT("AbstractURL"),   AbstractUrl);
	DDG->TryGetStringField(TEXT("Answer"),        Answer);

	if (!Heading.IsEmpty())      Out->SetStringField(TEXT("heading"),      Heading);
	if (!AbstractText.IsEmpty()) Out->SetStringField(TEXT("abstract"),     AbstractText);
	if (!AbstractUrl.IsEmpty())  Out->SetStringField(TEXT("abstract_url"), AbstractUrl);
	if (!Answer.IsEmpty())       Out->SetStringField(TEXT("answer"),       Answer);

	// Collect related topics (text + url pairs)
	const TArray<TSharedPtr<FJsonValue>>* RelatedTopicsRaw;
	TArray<TSharedPtr<FJsonValue>> RelatedTopics;

	if (DDG->TryGetArrayField(TEXT("RelatedTopics"), RelatedTopicsRaw))
	{
		for (const TSharedPtr<FJsonValue>& Val : *RelatedTopicsRaw)
		{
			const TSharedPtr<FJsonObject>* TopicObj;
			if (!Val->TryGetObject(TopicObj)) continue;

			FString Text, FirstUrl;
			(*TopicObj)->TryGetStringField(TEXT("Text"),     Text);
			(*TopicObj)->TryGetStringField(TEXT("FirstURL"), FirstUrl);
			if (Text.IsEmpty() && FirstUrl.IsEmpty()) continue;

			TSharedRef<FJsonObject> Topic = MakeShared<FJsonObject>();
			if (!Text.IsEmpty())     Topic->SetStringField(TEXT("text"), Text);
			if (!FirstUrl.IsEmpty()) Topic->SetStringField(TEXT("url"),  FirstUrl);
			RelatedTopics.Add(MakeShared<FJsonValueObject>(Topic));

			if (RelatedTopics.Num() >= 10) break; // cap at 10
		}
	}

	if (RelatedTopics.Num() > 0)
		Out->SetArrayField(TEXT("related_topics"), RelatedTopics);

	// Hint for follow-up
	Out->SetStringField(TEXT("tip"), TEXT("Use fetch_page action with any URL from related_topics or abstract_url to read the full page content."));

	FString OutStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
	FJsonSerializer::Serialize(Out, Writer);
	return OutStr;
}

// ---------------------------------------------------------------------------
// Action: fetch_page  (Jina AI Reader — converts URL → clean markdown)
// ---------------------------------------------------------------------------
static FString ActionFetchPage(const TMap<FString, FString>& Params)
{
	const FString PageUrl = ExtractResearchParam(Params, TEXT("url"));
	if (PageUrl.IsEmpty())
		return BuildResearchError(TEXT("MISSING_PARAMS"), TEXT("'url' is required for the fetch_page action."));

	// Jina Reader: prefix any URL with https://r.jina.ai/
	const FString JinaUrl = FString::Printf(TEXT("https://r.jina.ai/%s"), *PageUrl);

	TArray<TPair<FString, FString>> Headers;
	Headers.Add(TPair<FString, FString>(TEXT("Accept"), TEXT("text/markdown")));
	Headers.Add(TPair<FString, FString>(TEXT("X-Return-Format"), TEXT("markdown")));

	const FResearchHttpResult Http = ResearchHttpGet(
		JinaUrl,
		TEXT("VibeUE/1.0 (Unreal Engine plugin)"),
		Headers,
		45.0f);

	if (!Http.bSuccess)
		return BuildResearchError(TEXT("HTTP_ERROR"), Http.ErrorMessage);

	if (Http.ResponseCode != 200)
		return BuildResearchError(
			FString::Printf(TEXT("HTTP_%d"), Http.ResponseCode),
			FString::Printf(TEXT("Jina Reader returned %d for URL: %s"), Http.ResponseCode, *PageUrl));

	// Return the markdown content inside a JSON envelope
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("url"),     PageUrl);
	Out->SetStringField(TEXT("content"), Http.Body);

	FString OutStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
	FJsonSerializer::Serialize(Out, Writer);
	return OutStr;
}

// ---------------------------------------------------------------------------
// Action: geocode  (Nominatim — place name → lat/lng)
// ---------------------------------------------------------------------------
static FString ActionGeocode(const TMap<FString, FString>& Params)
{
	const FString Query = ExtractResearchParam(Params, TEXT("query"));
	if (Query.IsEmpty())
		return BuildResearchError(TEXT("MISSING_PARAMS"), TEXT("'query' is required for the geocode action (e.g. 'Mount Fuji' or 'San Francisco, CA')."));

	const FString Encoded = UrlEncodeSimple(Query);
	const FString Url = FString::Printf(
		TEXT("https://nominatim.openstreetmap.org/search?q=%s&format=json&limit=5&addressdetails=1"),
		*Encoded);

	const FResearchHttpResult Http = ResearchHttpGet(Url, TEXT("VibeUE/1.0 (Unreal Engine plugin)"), {}, 15.0f);

	if (!Http.bSuccess)
		return BuildResearchError(TEXT("HTTP_ERROR"), Http.ErrorMessage);

	if (Http.ResponseCode != 200)
		return BuildResearchError(
			FString::Printf(TEXT("HTTP_%d"), Http.ResponseCode),
			FString::Printf(TEXT("Nominatim returned %d"), Http.ResponseCode));

	TArray<TSharedPtr<FJsonValue>> NominatimResults;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Http.Body);
	if (!FJsonSerializer::Deserialize(Reader, NominatimResults))
		return BuildResearchError(TEXT("PARSE_ERROR"), TEXT("Failed to parse Nominatim response."));

	if (NominatimResults.Num() == 0)
		return BuildResearchError(TEXT("NOT_FOUND"),
			FString::Printf(TEXT("No results found for: %s"), *Query));

	// Build clean results array
	TArray<TSharedPtr<FJsonValue>> Results;
	for (int32 i = 0; i < FMath::Min(NominatimResults.Num(), 5); ++i)
	{
		const TSharedPtr<FJsonObject>* Obj;
		if (!NominatimResults[i]->TryGetObject(Obj)) continue;

		FString LatStr, LonStr, DisplayName, Type, Class;
		(*Obj)->TryGetStringField(TEXT("lat"),          LatStr);
		(*Obj)->TryGetStringField(TEXT("lon"),          LonStr);
		(*Obj)->TryGetStringField(TEXT("display_name"), DisplayName);
		(*Obj)->TryGetStringField(TEXT("type"),         Type);
		(*Obj)->TryGetStringField(TEXT("class"),        Class);

		TSharedRef<FJsonObject> R = MakeShared<FJsonObject>();
		R->SetNumberField(TEXT("lat"),          FCString::Atod(*LatStr));
		R->SetNumberField(TEXT("lng"),          FCString::Atod(*LonStr));
		R->SetStringField(TEXT("display_name"), DisplayName);
		if (!Type.IsEmpty())  R->SetStringField(TEXT("type"),  Type);
		if (!Class.IsEmpty()) R->SetStringField(TEXT("class"), Class);
		Results.Add(MakeShared<FJsonValueObject>(R));
	}

	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("query"), Query);
	Out->SetArrayField(TEXT("results"), Results);

	// Promote top result's lat/lng to the root for easy access
	const TSharedPtr<FJsonObject>* First;
	if (Results.Num() > 0 && Results[0]->TryGetObject(First))
	{
		double Lat = 0.0, Lng = 0.0;
		(*First)->TryGetNumberField(TEXT("lat"), Lat);
		(*First)->TryGetNumberField(TEXT("lng"), Lng);
		FString DN;
		(*First)->TryGetStringField(TEXT("display_name"), DN);
		Out->SetNumberField(TEXT("lat"),          Lat);
		Out->SetNumberField(TEXT("lng"),          Lng);
		Out->SetStringField(TEXT("display_name"), DN);
	}

	Out->SetStringField(TEXT("tip"), TEXT("Pass lat and lng to the terrain_data tool for heightmap generation."));

	FString OutStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
	FJsonSerializer::Serialize(Out, Writer);
	return OutStr;
}

// ---------------------------------------------------------------------------
// Action: reverse_geocode  (Nominatim — lat/lng → place name)
// ---------------------------------------------------------------------------
static FString ActionReverseGeocode(const TMap<FString, FString>& Params)
{
	const FString LatStr = ExtractResearchParam(Params, TEXT("lat"));
	const FString LngStr = ExtractResearchParam(Params, TEXT("lng"));
	if (LatStr.IsEmpty() || LngStr.IsEmpty())
		return BuildResearchError(TEXT("MISSING_PARAMS"), TEXT("'lat' and 'lng' are required for the reverse_geocode action."));

	const FString Url = FString::Printf(
		TEXT("https://nominatim.openstreetmap.org/reverse?lat=%s&lon=%s&format=json&addressdetails=1"),
		*LatStr, *LngStr);

	const FResearchHttpResult Http = ResearchHttpGet(Url, TEXT("VibeUE/1.0 (Unreal Engine plugin)"), {}, 15.0f);

	if (!Http.bSuccess)
		return BuildResearchError(TEXT("HTTP_ERROR"), Http.ErrorMessage);

	if (Http.ResponseCode != 200)
		return BuildResearchError(
			FString::Printf(TEXT("HTTP_%d"), Http.ResponseCode),
			FString::Printf(TEXT("Nominatim returned %d"), Http.ResponseCode));

	// Pass through Nominatim response with success flag added
	TSharedPtr<FJsonObject> NominatimResponse;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Http.Body);
	if (!FJsonSerializer::Deserialize(Reader, NominatimResponse) || !NominatimResponse.IsValid())
		return BuildResearchError(TEXT("PARSE_ERROR"), TEXT("Failed to parse reverse geocoding response."));

	NominatimResponse->SetBoolField(TEXT("success"), true);

	// Normalize lon → lng for consistency
	FString LonValue;
	if (NominatimResponse->TryGetStringField(TEXT("lon"), LonValue))
		NominatimResponse->SetStringField(TEXT("lng"), LonValue);

	FString OutStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutStr);
	FJsonSerializer::Serialize(NominatimResponse.ToSharedRef(), Writer);
	return OutStr;
}

// ---------------------------------------------------------------------------
// Tool registration
// ---------------------------------------------------------------------------
REGISTER_VIBEUE_TOOL(deep_research,
	"Web research and GPS geocoding — no API key required. "
	"Use 'search' to look up any topic via DuckDuckGo and get an abstract plus relevant URLs. "
	"Use 'fetch_page' to read the full content of any URL as clean markdown (great for Unreal Engine "
	"documentation, Dev Community posts, API references). "
	"Use 'geocode' to convert any place name or address into GPS coordinates (lat/lng) for use with "
	"the terrain_data tool. "
	"Use 'reverse_geocode' to convert GPS coordinates back into a human-readable place name. "
	"Typical deep research workflow: search → fetch_page on the best URL → synthesize. "
	"Typical terrain workflow: geocode 'Mount Fuji' → pass lat/lng to terrain_data.",
	"Research",
	TOOL_PARAMS(
		TOOL_PARAM("action", "Action: search | fetch_page | geocode | reverse_geocode", "string", true),
		TOOL_PARAM("query",  "For search: topic or question. For geocode: place name or address (e.g. 'Mount Fuji', 'Grand Canyon South Rim').", "string", false),
		TOOL_PARAM("url",    "For fetch_page: the full URL to fetch and convert to markdown (e.g. https://dev.epicgames.com/documentation/...).", "string", false),
		TOOL_PARAM("lat",    "Latitude for reverse_geocode action.", "number", false),
		TOOL_PARAM("lng",    "Longitude for reverse_geocode action.", "number", false)
	),
	{
		const FString Action = ExtractResearchParam(Params, TEXT("action")).ToLower().TrimStartAndEnd();

		if (Action.IsEmpty())
			return BuildResearchError(TEXT("MISSING_ACTION"),
				TEXT("'action' is required. Options: search, fetch_page, geocode, reverse_geocode"));

		if (Action == TEXT("search"))          return ActionSearch(Params);
		if (Action == TEXT("fetch_page"))      return ActionFetchPage(Params);
		if (Action == TEXT("geocode"))         return ActionGeocode(Params);
		if (Action == TEXT("reverse_geocode")) return ActionReverseGeocode(Params);

		return BuildResearchError(TEXT("UNKNOWN_ACTION"),
			FString::Printf(TEXT("Unknown action: '%s'. Valid: search, fetch_page, geocode, reverse_geocode"), *Action));
	}
);
