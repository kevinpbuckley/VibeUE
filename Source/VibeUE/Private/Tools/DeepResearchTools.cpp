// Copyright Buckley Builds LLC 2026 All Rights Reserved.
//
// DeepResearchTools.cpp
// MCP tool: deep_research — web research and GPS geocoding with no API key required.
//
// Actions:
//   search          — DuckDuckGo HTML search (real web results with titles, URLs, snippets)
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
// Shared request state, a wait off the game thread, cancellation
#include "Async/Async.h"
#include "HAL/Event.h"
#include "Core/VibeUEToolCancel.h"

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
	bool    bTooLarge     = false; // The response passed the request's MaxResponseBytes; ErrorMessage says so
};

// The request's own HTTP timeout is this much longer than the helper's deadline (TimeoutSeconds on the game thread,
// TimeoutSeconds + 1 for a worker's wait), so the helper gives up first and says "Request timed out" rather than
// HTTP's "Connection failed". The HTTP timeout is only a backstop for a request nobody abandons.
static constexpr float ResearchHttpTimeoutMargin = 2.0f;

static FString ResearchTooLargeMessage(uint64 Bytes, uint64 MaxResponseBytes)
{
	return FString::Printf(TEXT("The response is at least %llu bytes, over the %llu MB limit."), Bytes, MaxResponseBytes / (1024 * 1024));
}

// The request's completion writes into this shared state, which it holds only weakly, never into the helper's
// locals: a request that times out is cancelled, CancelRequest only schedules the abort, and the completion then runs
// at a later HTTP tick, after the helper has returned.
struct FResearchHttpState
{
	FResearchHttpResult Result;
	std::atomic<bool> bComplete{false};
	std::atomic<bool> bCancelled{false};
	std::atomic<bool> bTooLarge{false};    // Set by the size checks, which wake the wait; the waiter abandons
	std::atomic<uint64> TooLargeBytes{0};
	FEventRef Done{EEventMode::ManualReset};
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> Request; // touched on the game thread only
};
using FResearchHttpStateRef = TSharedRef<FResearchHttpState, ESPMode::ThreadSafe>;

// Builds and starts the request. Game thread only. MaxResponseBytes > 0 caps the response: a larger Content-Length
// or download flags bTooLarge and wakes the wait, and the waiter abandons the request (never these delegates).
static void StartResearchRequest(
	const FResearchHttpStateRef& State,
	const FString& Url,
	const FString& UserAgent,
	const TArray<TPair<FString, FString>>& ExtraHeaders,
	float TimeoutSeconds,
	uint64 MaxResponseBytes)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetTimeout(TimeoutSeconds + ResearchHttpTimeoutMargin);
	if (!UserAgent.IsEmpty())
		Request->SetHeader(TEXT("User-Agent"), UserAgent);
	for (const auto& KV : ExtraHeaders)
		Request->SetHeader(KV.Key, KV.Value);

	const TWeakPtr<FResearchHttpState, ESPMode::ThreadSafe> WeakState = State;
	if (MaxResponseBytes > 0)
	{
		auto FlagTooLarge = [WeakState, MaxResponseBytes](uint64 Bytes)
		{
			const TSharedPtr<FResearchHttpState, ESPMode::ThreadSafe> S = WeakState.Pin();
			if (S && Bytes > MaxResponseBytes && !S->bTooLarge)
			{
				S->TooLargeBytes = Bytes;
				S->bTooLarge = true;
				S->Done->Trigger();
			}
		};
		Request->OnHeaderReceived().BindLambda([FlagTooLarge](FHttpRequestPtr, const FString& HeaderName, const FString& HeaderValue)
		{
			if (HeaderName.Equals(TEXT("Content-Length"), ESearchCase::IgnoreCase))
			{
				FlagTooLarge(FCString::Strtoui64(*HeaderValue, nullptr, 10));
			}
		});
		Request->OnRequestProgress64().BindLambda([FlagTooLarge](FHttpRequestPtr, uint64 /*BytesSent*/, uint64 BytesReceived)
		{
			FlagTooLarge(BytesReceived);
		});
	}
	Request->OnProcessRequestComplete().BindLambda(
		[WeakState, MaxResponseBytes](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
		{
			const TSharedPtr<FResearchHttpState, ESPMode::ThreadSafe> S = WeakState.Pin();
			if (!S)
			{
				return; // the caller gave up and is gone
			}
			if (!bConnected || !Resp.IsValid())
			{
				S->Result.ErrorMessage = TEXT("Connection failed");
			}
			else if (MaxResponseBytes > 0 && static_cast<uint64>(Resp->GetContent().Num()) > MaxResponseBytes)
			{
				// It arrived whole before a progress tick saw it: refused all the same, and never decoded
				S->Result.ResponseCode = Resp->GetResponseCode();
				S->Result.bTooLarge    = true;
				S->Result.ErrorMessage = ResearchTooLargeMessage(Resp->GetContent().Num(), MaxResponseBytes);
			}
			else
			{
				S->Result.bSuccess     = true;
				S->Result.ResponseCode = Resp->GetResponseCode();
				S->Result.Body         = Resp->GetContentAsString();
			}
			S->bComplete = true;
			S->Done->Trigger();
		}
	);

	State->Request = Request;
	Request->ProcessRequest();
}

// Unbinds the completion, then cancels. Game thread only.
static void AbandonResearchRequest(const FResearchHttpStateRef& State)
{
	if (State->Request.IsValid())
	{
		State->Request->OnProcessRequestComplete().Unbind();
		State->Request->OnHeaderReceived().Unbind();
		State->Request->OnRequestProgress64().Unbind();
		State->Request->CancelRequest();
		State->Request.Reset();
	}
}

static FResearchHttpResult ResearchHttpGet(
	const FString& Url,
	const FString& UserAgent = TEXT("VibeUE/1.0 (Unreal Engine plugin)"),
	const TArray<TPair<FString, FString>>& ExtraHeaders = {},
	float TimeoutSeconds = 30.0f,
	uint64 MaxResponseBytes = 0) // 0: no cap
{
	const FResearchHttpStateRef State = MakeShared<FResearchHttpState, ESPMode::ThreadSafe>();
	auto TooLarge = [&State, MaxResponseBytes]()
	{
		FResearchHttpResult Refused;
		Refused.bTooLarge = true;
		Refused.ErrorMessage = ResearchTooLargeMessage(State->TooLargeBytes, MaxResponseBytes);
		return Refused;
	};

	if (IsInGameThread())
	{
		// On the game thread (a direct call, e.g. from Python): the original blocking wait, now safe.
		StartResearchRequest(State, Url, UserAgent, ExtraHeaders, TimeoutSeconds, MaxResponseBytes);
		const double Start = FPlatformTime::Seconds();
		while (!State->bComplete)
		{
			FHttpModule::Get().GetHttpManager().Tick(0.0f);
			FPlatformProcess::Sleep(0.01f);
			if (State->bTooLarge)
			{
				AbandonResearchRequest(State);
				return TooLarge();
			}
			if (FPlatformTime::Seconds() - Start > TimeoutSeconds)
			{
				AbandonResearchRequest(State);
				FResearchHttpResult TimedOut;
				TimedOut.ErrorMessage = TEXT("Request timed out");
				return TimedOut;
			}
		}
		return State->Result;
	}

	// On a worker thread (the MCP bridge runs deep_research there) every HTTP call stays on the
	// game thread and this thread only waits, so the editor keeps ticking. A cancel from the client wakes the wait.
	FVibeUEToolCancel* Cancel = FVibeUEToolCancel::GetCurrent();
	if (Cancel)
	{
		const TWeakPtr<FResearchHttpState, ESPMode::ThreadSafe> WeakState = State;
		Cancel->SetOnCancel([WeakState]()
		{
			if (const TSharedPtr<FResearchHttpState, ESPMode::ThreadSafe> S = WeakState.Pin())
			{
				S->bCancelled = true;
				if (IsInGameThread()) // The exit sweep runs here, before HTTP shuts down
				{
					AbandonResearchRequest(S.ToSharedRef());
				}
				S->Done->Trigger();
			}
		});
	}
	AsyncTask(ENamedThreads::GameThread, [State, Url, UserAgent, ExtraHeaders, TimeoutSeconds, MaxResponseBytes]()
	{
		if (!State->bCancelled)
		{
			StartResearchRequest(State, Url, UserAgent, ExtraHeaders, TimeoutSeconds, MaxResponseBytes);
		}
	});
	const bool bSignalled = State->Done->Wait(FTimespan::FromSeconds(TimeoutSeconds + 1.0));
	if (Cancel)
	{
		Cancel->ClearOnCancel();
	}
	if (bSignalled && State->bComplete && !State->bCancelled)
	{
		return State->Result;
	}
	AsyncTask(ENamedThreads::GameThread, [State]() { AbandonResearchRequest(State); });
	if (State->bTooLarge && !State->bCancelled)
	{
		return TooLarge();
	}
	FResearchHttpResult Failed;
	Failed.ErrorMessage = State->bCancelled ? TEXT("Request cancelled") : TEXT("Request timed out");
	return Failed;
}

// ---------------------------------------------------------------------------
// Helpers for parsing search results
// ---------------------------------------------------------------------------

// Decode percent-encoded URL components (e.g. %3A → :, %2F → /)
static FString UrlDecodeSimple(const FString& Input)
{
	FString Out;
	Out.Reserve(Input.Len());
	for (int32 i = 0; i < Input.Len(); ++i)
	{
		if (Input[i] == TEXT('%') && i + 2 < Input.Len())
		{
			const FString Hex = Input.Mid(i + 1, 2);
			const int32 Value = FParse::HexDigit(Hex[0]) * 16 + FParse::HexDigit(Hex[1]);
			if (Value >= 0)
			{
				Out.AppendChar(static_cast<TCHAR>(Value));
				i += 2;
				continue;
			}
		}
		else if (Input[i] == TEXT('+'))
		{
			Out.AppendChar(TEXT(' '));
			continue;
		}
		Out.AppendChar(Input[i]);
	}
	return Out;
}

// Extract the real URL from a DuckDuckGo redirect href (uddg= parameter)
static FString ExtractDDGUrl(const FString& Href)
{
	const FString UddgKey = TEXT("uddg=");
	int32 Start = Href.Find(UddgKey);
	if (Start != INDEX_NONE)
	{
		Start += UddgKey.Len();
		int32 End = Href.Find(TEXT("&"), ESearchCase::IgnoreCase, ESearchDir::FromStart, Start);
		FString Encoded = (End != INDEX_NONE) ? Href.Mid(Start, End - Start) : Href.Mid(Start);
		return UrlDecodeSimple(Encoded);
	}
	if (Href.StartsWith(TEXT("//")))
		return FString(TEXT("https:")) + Href;
	return Href;
}

// Parse markdown search results (from Jina-rendered DDG page) into structured data
struct FDDGResult
{
	FString Title;
	FString Url;
	FString Snippet;
};

static TArray<FDDGResult> ParseMarkdownSearchResults(const FString& Markdown, int32 MaxResults = 15)
{
	TArray<FDDGResult> Results;

	// Jina renders DDG Lite results as markdown like:
	//   1.[Title Text](https://duckduckgo.com/l/?uddg=REAL_URL&rut=...)
	//   Snippet text here
	//   domain.com/path
	//
	// or for DDG HTML:
	//   ## [Title Text](https://duckduckgo.com/l/?uddg=REAL_URL&rut=...)
	//   Snippet text
	//   domain.com/path

	TArray<FString> Lines;
	Markdown.ParseIntoArrayLines(Lines);

	for (int32 i = 0; i < Lines.Num() && Results.Num() < MaxResults; ++i)
	{
		const FString& Line = Lines[i];

		// Look for markdown links: [Title](URL)
		// These appear on lines starting with a number+dot or ## 
		int32 BracketStart = Line.Find(TEXT("["));
		if (BracketStart == INDEX_NONE) continue;

		// Check format: must have ](url) pattern
		int32 BracketEnd = Line.Find(TEXT("]("), ESearchCase::IgnoreCase, ESearchDir::FromStart, BracketStart);
		if (BracketEnd == INDEX_NONE) continue;

		int32 UrlEnd = Line.Find(TEXT(")"), ESearchCase::IgnoreCase, ESearchDir::FromStart, BracketEnd + 2);
		if (UrlEnd == INDEX_NONE) continue;

		FString Title = Line.Mid(BracketStart + 1, BracketEnd - BracketStart - 1).TrimStartAndEnd();
		FString LinkUrl = Line.Mid(BracketEnd + 2, UrlEnd - BracketEnd - 2).TrimStartAndEnd();

		// Skip image links, navigation links, DuckDuckGo internal stuff
		if (Title.IsEmpty()) continue;
		if (Title.StartsWith(TEXT("Image"))) continue;
		if (LinkUrl.Contains(TEXT("duckduckgo.com/t/"))) continue; // tracking pixel

		// Extract real URL from DDG redirect
		FString RealUrl = ExtractDDGUrl(LinkUrl);

		// Skip DDG-internal URLs (category pages, etc.)
		if (RealUrl.Contains(TEXT("duckduckgo.com")) && !RealUrl.Contains(TEXT("uddg="))) continue;

		// Collect snippet from subsequent lines (up to 3 lines of text, stop at next numbered result or markdown heading)
		FString Snippet;
		for (int32 j = i + 1; j < FMath::Min(i + 4, Lines.Num()); ++j)
		{
			const FString& NextLine = Lines[j].TrimStartAndEnd();
			if (NextLine.IsEmpty()) continue;
			// Stop if this is another result (starts with number+bracket or ##)
			if (NextLine.Len() > 2 && FChar::IsDigit(NextLine[0]) && NextLine.Contains(TEXT("[")) && NextLine.Contains(TEXT("]("))) break;
			if (NextLine.StartsWith(TEXT("##"))) break;
			if (NextLine.StartsWith(TEXT("!["))) continue; // skip image references
			// Skip bare domain lines (like "dev.epicgames.com/...")  
			if (!NextLine.Contains(TEXT(" ")) && NextLine.Contains(TEXT("."))) continue;
			// This is snippet text
			if (!Snippet.IsEmpty()) Snippet += TEXT(" ");
			// Remove markdown bold markers
			FString Clean = NextLine.Replace(TEXT("**"), TEXT(""));
			// Remove &hellip; entities
			Clean = Clean.Replace(TEXT("&hellip;"), TEXT("..."));
			Snippet += Clean;
		}

		FDDGResult R;
		R.Title   = Title.Replace(TEXT("**"), TEXT("")); // strip markdown bold
		R.Url     = RealUrl;
		R.Snippet = Snippet.TrimStartAndEnd();
		Results.Add(R);
	}

	return Results;
}

// ---------------------------------------------------------------------------
// A direct fallback when Jina Reader refuses the request. Jina answers 401 to anonymous requests from networks it
// rates badly ("You have been blocked from performing anonymous queries due to bad network reputation"), which breaks
// search and fetch_page there. Both then fetch the source directly (DuckDuckGo Lite answers the same query) and read
// its HTML here.
// ---------------------------------------------------------------------------

// An optional Jina API key, from the JINA_API_KEY environment variable (read per call: it is thread-safe, and the
// key never lands in a project file). With it Jina also serves the networks it refuses anonymously.
static void AddJinaAuth(TArray<TPair<FString, FString>>& Headers)
{
	const FString Key = FPlatformMisc::GetEnvironmentVariable(TEXT("JINA_API_KEY")).TrimStartAndEnd();
	if (!Key.IsEmpty())
	{
		Headers.Add(TPair<FString, FString>(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Key)));
	}
}

static bool IsReaderRefusal(const FResearchHttpResult& Http)
{
	return Http.bSuccess && (Http.ResponseCode == 401 || Http.ResponseCode == 402 || Http.ResponseCode == 403
		|| Http.ResponseCode == 429 || Http.ResponseCode == 451);
}

// fetch_page fetches a page itself only over the web's own schemes: libcurl here also speaks file, ftp, smb, gopher,
// dict, telnet and tftp.
static bool IsDirectlyFetchableUrl(const FString& Url)
{
	return Url.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) || Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
}

// The most of a page fetch_page downloads itself (its text is cut to 200,000 characters anyway)
static constexpr uint64 ResearchDirectFetchMaxBytes = 5ull * 1024 * 1024;

static FString DecodeHtmlEntities(const FString& In)
{
	FString Out;
	Out.Reserve(In.Len());
	for (int32 i = 0; i < In.Len(); ++i)
	{
		if (In[i] == TEXT('&'))
		{
			const int32 Semi = In.Find(TEXT(";"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i);
			if (Semi != INDEX_NONE && Semi - i <= 10)
			{
				const FString Entity = In.Mid(i + 1, Semi - i - 1);
				TCHAR Decoded = 0;
				uint32 CodePoint = 0; // A numeric entity's
				if (Entity == TEXT("amp")) Decoded = TEXT('&');
				else if (Entity == TEXT("lt")) Decoded = TEXT('<');
				else if (Entity == TEXT("gt")) Decoded = TEXT('>');
				else if (Entity == TEXT("quot")) Decoded = TEXT('"');
				else if (Entity == TEXT("apos")) Decoded = TEXT('\'');
				else if (Entity == TEXT("nbsp")) Decoded = TEXT(' ');
				else if (Entity.StartsWith(TEXT("#x")) || Entity.StartsWith(TEXT("#X"))) CodePoint = FParse::HexNumber(*Entity.Mid(2));
				else if (Entity.StartsWith(TEXT("#"))) CodePoint = static_cast<uint32>(FCString::Atoi(*Entity.Mid(1)));
				// FString is UTF-16: a code point past U+FFFF (emoji) is a surrogate pair. Zero, a lone surrogate and
				// anything past U+10FFFF stay as written.
				if (CodePoint != 0 && StringConv::IsValidCodepoint(CodePoint)
					&& !StringConv::IsHighSurrogate(CodePoint) && !StringConv::IsLowSurrogate(CodePoint))
				{
					if (CodePoint > 0xFFFF)
					{
						uint16 High = 0;
						uint16 Low = 0;
						StringConv::DecodeSurrogate(CodePoint, High, Low);
						Out.AppendChar(static_cast<TCHAR>(High));
						Out.AppendChar(static_cast<TCHAR>(Low));
					}
					else
					{
						Out.AppendChar(static_cast<TCHAR>(CodePoint));
					}
					i = Semi;
					continue;
				}
				if (Decoded != 0)
				{
					Out.AppendChar(Decoded);
					i = Semi;
					continue;
				}
			}
		}
		Out.AppendChar(In[i]);
	}
	return Out;
}

// Tags out, entities decoded, runs of whitespace collapsed to one space.
static FString HtmlFragmentToText(const FString& Html)
{
	FString NoTags;
	NoTags.Reserve(Html.Len());
	bool bInTag = false;
	for (const TCHAR Ch : Html)
	{
		if (Ch == TEXT('<')) { bInTag = true; continue; }
		if (Ch == TEXT('>')) { bInTag = false; continue; }
		if (!bInTag) NoTags.AppendChar(Ch);
	}
	const FString Decoded = DecodeHtmlEntities(NoTags);
	FString Out;
	Out.Reserve(Decoded.Len());
	bool bSpace = false;
	for (const TCHAR Ch : Decoded)
	{
		if (FChar::IsWhitespace(Ch))
		{
			bSpace = true;
			continue;
		}
		if (bSpace && !Out.IsEmpty()) Out.AppendChar(TEXT(' '));
		bSpace = false;
		Out.AppendChar(Ch);
	}
	return Out;
}

// DuckDuckGo Lite's HTML: each result is <a ... href="//duckduckgo.com/l/?uddg=URL&amp;rut=..." class='result-link'>Title</a>,
// followed by <td class='result-snippet'>Snippet</td>.
static TArray<FDDGResult> ParseDDGLiteHtml(const FString& Html, int32 MaxResults = 15)
{
	TArray<FDDGResult> Results;
	int32 Cursor = 0;
	while (Results.Num() < MaxResults)
	{
		const int32 Marker = Html.Find(TEXT("class='result-link'"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
		if (Marker == INDEX_NONE) break;
		const int32 AnchorStart = Html.Find(TEXT("<a "), ESearchCase::CaseSensitive, ESearchDir::FromEnd, Marker);
		const int32 TagEnd = Html.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Marker);
		const int32 AnchorEnd = TagEnd == INDEX_NONE ? INDEX_NONE : Html.Find(TEXT("</a>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, TagEnd);
		if (AnchorStart == INDEX_NONE || AnchorEnd == INDEX_NONE) break;
		Cursor = AnchorEnd;

		const FString Tag = Html.Mid(AnchorStart, TagEnd - AnchorStart);
		const int32 HrefStart = Tag.Find(TEXT("href=\""));
		if (HrefStart == INDEX_NONE) continue;
		const int32 HrefEnd = Tag.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, HrefStart + 6);
		if (HrefEnd == INDEX_NONE) continue;
		const FString Href = DecodeHtmlEntities(Tag.Mid(HrefStart + 6, HrefEnd - HrefStart - 6));

		FDDGResult R;
		R.Title = HtmlFragmentToText(Html.Mid(TagEnd + 1, AnchorEnd - TagEnd - 1));
		R.Url = ExtractDDGUrl(Href);

		// The snippet belongs to this result only if it comes before the next result link.
		const int32 NextLink = Html.Find(TEXT("class='result-link'"), ESearchCase::CaseSensitive, ESearchDir::FromStart, AnchorEnd);
		const int32 SnippetMarker = Html.Find(TEXT("class='result-snippet'>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, AnchorEnd);
		if (SnippetMarker != INDEX_NONE && (NextLink == INDEX_NONE || SnippetMarker < NextLink))
		{
			const int32 SnippetStart = SnippetMarker + FCString::Strlen(TEXT("class='result-snippet'>"));
			const int32 SnippetEnd = Html.Find(TEXT("</td>"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SnippetStart);
			if (SnippetEnd != INDEX_NONE)
			{
				R.Snippet = HtmlFragmentToText(Html.Mid(SnippetStart, SnippetEnd - SnippetStart));
			}
		}
		if (!R.Title.IsEmpty() && !R.Url.IsEmpty())
		{
			Results.Add(R);
		}
	}
	return Results;
}

// A readable text version of a whole page: no scripts, styles, comments or markup; block ends become line breaks.
// Tags come out of the whole document before it is split into lines, so an attribute value that spans lines (pages
// put JSON in them) never leaks into the text.
static FString HtmlPageToText(const FString& Html, int32 MaxChars = 200000)
{
	FString Work = Html;
	// One forward pass per pair, copying what is kept once: removing in place and searching again from the start was
	// quadratic on a page full of scripts.
	auto DropBetween = [&Work](const FString& Open, const FString& Close)
	{
		FString Kept;
		Kept.Reserve(Work.Len());
		int32 Cursor = 0;
		while (Cursor < Work.Len())
		{
			const int32 Start = Work.Find(Open, ESearchCase::IgnoreCase, ESearchDir::FromStart, Cursor);
			if (Start == INDEX_NONE)
			{
				Kept.Append(*Work + Cursor, Work.Len() - Cursor);
				break;
			}
			Kept.Append(*Work + Cursor, Start - Cursor);
			const int32 End = Work.Find(Close, ESearchCase::IgnoreCase, ESearchDir::FromStart, Start + Open.Len());
			Cursor = End == INDEX_NONE ? Work.Len() : End + Close.Len(); // Unclosed: dropped to the end
		}
		Work = MoveTemp(Kept);
	};
	DropBetween(TEXT("<!--"), TEXT("-->"));
	for (const TCHAR* Drop : { TEXT("script"), TEXT("style"), TEXT("noscript"), TEXT("svg"), TEXT("template") })
	{
		DropBetween(FString::Printf(TEXT("<%s"), Drop), FString::Printf(TEXT("</%s>"), Drop));
	}
	const TCHAR* LineBreak = TEXT("\x01");
	for (const TCHAR* Block : { TEXT("</p>"), TEXT("<br>"), TEXT("<br/>"), TEXT("<br />"), TEXT("</div>"), TEXT("</li>"),
		TEXT("</h1>"), TEXT("</h2>"), TEXT("</h3>"), TEXT("</h4>"), TEXT("</tr>"), TEXT("</pre>"), TEXT("</section>") })
	{
		Work.ReplaceInline(Block, LineBreak, ESearchCase::IgnoreCase);
	}
	FString NoTags;
	NoTags.Reserve(Work.Len());
	bool bInTag = false;
	for (const TCHAR Ch : Work)
	{
		if (Ch == TEXT('<')) { bInTag = true; continue; }
		if (Ch == TEXT('>')) { bInTag = false; continue; }
		if (!bInTag) NoTags.AppendChar(Ch);
	}
	TArray<FString> Lines;
	NoTags.ParseIntoArray(Lines, LineBreak, /*InCullEmpty=*/false);
	FString Out;
	for (const FString& Line : Lines)
	{
		const FString Text = HtmlFragmentToText(Line);
		if (!Text.IsEmpty())
		{
			Out += Text;
			Out += TEXT("\n");
		}
		if (Out.Len() >= MaxChars)
		{
			Out.LeftInline(MaxChars);
			Out += TEXT("\n[truncated]");
			break;
		}
	}
	return Out;
}

// ---------------------------------------------------------------------------
// Action: search  (DuckDuckGo via Jina Reader — real web results)
// ---------------------------------------------------------------------------
static FString ActionSearch(const TMap<FString, FString>& Params)
{
	const FString Query = ExtractResearchParam(Params, TEXT("query"));
	if (Query.IsEmpty())
		return BuildResearchError(TEXT("MISSING_PARAMS"), TEXT("'query' is required for the search action."));

	// Build DuckDuckGo Lite URL and route through Jina Reader for clean markdown
	const FString Encoded = UrlEncodeSimple(Query);
	const FString DDGUrl = FString::Printf(TEXT("https://lite.duckduckgo.com/lite/?q=%s"), *Encoded);
	const FString JinaUrl = FString::Printf(TEXT("https://r.jina.ai/%s"), *DDGUrl);

	TArray<TPair<FString, FString>> Headers;
	Headers.Add(TPair<FString, FString>(TEXT("Accept"), TEXT("text/markdown")));
	Headers.Add(TPair<FString, FString>(TEXT("X-Return-Format"), TEXT("markdown")));

	AddJinaAuth(Headers);

	FResearchHttpResult Http = ResearchHttpGet(
		JinaUrl,
		TEXT("VibeUE/1.0 (Unreal Engine plugin)"),
		Headers,
		20.0f);

	// Jina refused (e.g. 401 for a network it rates badly) -> ask DuckDuckGo Lite directly and read its HTML.
	// DuckDuckGo may refuse a direct request too (a 202 bot check or a 403, depending on the client); then the
	// error says how to make Jina work.
	FString Source = TEXT("jina-reader");
	int32 ReaderRefusalCode = 0;
	if (IsReaderRefusal(Http))
	{
		ReaderRefusalCode = Http.ResponseCode;
		Http = ResearchHttpGet(DDGUrl, TEXT("VibeUE/1.0 (Unreal Engine plugin)"), {}, 20.0f);
		Source = TEXT("duckduckgo-lite-direct");
	}
	const FString RefusedBoth = FString::Printf(
		TEXT("Jina Reader refused the search (HTTP %d; it refuses anonymous requests from networks it rates badly) and DuckDuckGo refused the direct request (HTTP %d, a bot check). Set a Jina API key (free at jina.ai) in the JINA_API_KEY environment variable and restart the editor."),
		ReaderRefusalCode, Http.ResponseCode);

	if (!Http.bSuccess)
		return BuildResearchError(TEXT("HTTP_ERROR"), Http.ErrorMessage);

	if (Http.ResponseCode != 200)
		return ReaderRefusalCode
			? BuildResearchError(TEXT("SEARCH_REFUSED"), RefusedBoth)
			: BuildResearchError(
				FString::Printf(TEXT("HTTP_%d"), Http.ResponseCode),
				FString::Printf(TEXT("Search request returned %d"), Http.ResponseCode));

	// Parse the markdown results (Jina) or the HTML (direct)
	TArray<FDDGResult> ParsedResults = Source == TEXT("jina-reader")
		? ParseMarkdownSearchResults(Http.Body, 15)
		: ParseDDGLiteHtml(Http.Body, 15);

	// DuckDuckGo answered 200 but nothing in it read as a result: no results, or a page that is not a result list
	if (ParsedResults.Num() == 0 && ReaderRefusalCode)
		return BuildResearchError(TEXT("SEARCH_REFUSED"), FString::Printf(
			TEXT("Jina Reader refused the search (HTTP %d; it refuses anonymous requests from networks it rates badly), and DuckDuckGo's direct answer (HTTP 200) held no results this tool could read: there may be none for this query, or the page was not a result list (a bot check, for one). Set a Jina API key (free at jina.ai) in the JINA_API_KEY environment variable and restart the editor."),
			ReaderRefusalCode));

	if (ParsedResults.Num() == 0)
		return BuildResearchError(TEXT("NO_RESULTS"),
			FString::Printf(TEXT("No search results found for: %s"), *Query));

	// Build structured JSON output
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("query"), Query);
	Out->SetStringField(TEXT("source"), Source);
	Out->SetNumberField(TEXT("result_count"), ParsedResults.Num());

	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	for (const FDDGResult& R : ParsedResults)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		if (!R.Title.IsEmpty())   Item->SetStringField(TEXT("title"),   R.Title);
		if (!R.Url.IsEmpty())     Item->SetStringField(TEXT("url"),     R.Url);
		if (!R.Snippet.IsEmpty()) Item->SetStringField(TEXT("snippet"), R.Snippet);
		ResultsArray.Add(MakeShared<FJsonValueObject>(Item));
	}

	Out->SetArrayField(TEXT("results"), ResultsArray);

	// Hint for follow-up
	Out->SetStringField(TEXT("tip"), TEXT("Use fetch_page action with any result URL to read the full page content as clean markdown."));

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

	AddJinaAuth(Headers);

	FResearchHttpResult Http = ResearchHttpGet(
		JinaUrl,
		TEXT("VibeUE/1.0 (Unreal Engine plugin)"),
		Headers,
		45.0f);

	// Jina refused -> fetch the page itself and reduce its HTML to text here
	bool bDirect = false;
	if (IsReaderRefusal(Http))
	{
		if (!IsDirectlyFetchableUrl(PageUrl))
			return BuildResearchError(TEXT("UNSUPPORTED_URL_SCHEME"), FString::Printf(
				TEXT("Jina Reader refused the request (HTTP %d), and the direct fallback fetches only http:// and https:// URLs. Pass the page's full http(s) URL, or set a Jina API key (free at jina.ai) in the JINA_API_KEY environment variable and restart the editor."),
				Http.ResponseCode));

		TArray<TPair<FString, FString>> DirectHeaders;
		DirectHeaders.Add(TPair<FString, FString>(TEXT("Accept"), TEXT("text/html,text/plain;q=0.9,*/*;q=0.5")));
		Http = ResearchHttpGet(PageUrl, TEXT("VibeUE/1.0 (Unreal Engine plugin)"), DirectHeaders, 45.0f, ResearchDirectFetchMaxBytes);
		bDirect = true;
	}

	if (Http.bTooLarge) // Only the direct fetch is capped
		return BuildResearchError(TEXT("PAGE_TOO_LARGE"), FString::Printf(
			TEXT("Jina Reader refused the request, and the page is too large for the direct fallback to read. %s"), *Http.ErrorMessage));

	if (!Http.bSuccess)
		return BuildResearchError(TEXT("HTTP_ERROR"), Http.ErrorMessage);

	if (Http.ResponseCode != 200)
		return BuildResearchError(
			FString::Printf(TEXT("HTTP_%d"), Http.ResponseCode),
			FString::Printf(TEXT("%s returned %d for URL: %s"), bDirect ? TEXT("The page") : TEXT("Jina Reader"), Http.ResponseCode, *PageUrl));

	// Return the markdown content inside a JSON envelope
	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("url"),     PageUrl);
	Out->SetStringField(TEXT("source"),  bDirect ? TEXT("direct-html-to-text") : TEXT("jina-reader"));
	Out->SetStringField(TEXT("content"), bDirect ? HtmlPageToText(Http.Body) : Http.Body);

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
	"Use 'search' to look up any topic via DuckDuckGo and get real web results with titles, URLs, and snippets. "
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

#if WITH_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

// A request that times out is cancelled, and CancelRequest only schedules the abort: the completion still runs at a
// later HTTP tick. It must write nothing the caller can see, because the helper has returned by then. The helper's
// result object is returned by value (NRVO builds it in the caller's variable), so a late write shows up there.
// Test path prefix VibeUE.Research.*

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeUEResearchTimeoutIsSafeTest, "VibeUE.Research.TimeoutIsSafe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeUEResearchTimeoutIsSafeTest::RunTest(const FString& Parameters)
{
	// TEST-NET-1 (RFC 5737, documentation only, never a real host): the connection hangs, so the request times out.
	const double Start = FPlatformTime::Seconds();
	FResearchHttpResult Result = ResearchHttpGet(TEXT("http://192.0.2.1/"), TEXT("VibeUE test"), {}, 1.0f);
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

// The direct fallback reads DuckDuckGo Lite's HTML and turns pages into text. Test path prefix VibeUE.Research.*

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeUEResearchDirectFallbackParsingTest, "VibeUE.Research.DirectFallbackParsing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeUEResearchDirectFallbackParsingTest::RunTest(const FString& Parameters)
{
	// Two results in DuckDuckGo Lite's markup, as it answered on 2026-09-26; the second has no snippet.
	const FString Html = TEXT(
		"<td><a rel=\"nofollow\" href=\"//duckduckgo.com/l/?uddg=https%3A%2F%2Fdev.epicgames.com%2Fdocumentation%2Funreal%2Dengine%2Fenhanced%2Dinput%2Din%2Dunreal%2Dengine%3Flang%3Den%2DUS&amp;rut=abc\" class='result-link'>Enhanced Input in Unreal Engine | Unreal Engine 5.8 Documentation ...</a></td></tr>"
		"<tr><td>&nbsp;&nbsp;&nbsp;</td><td class='result-snippet'>\n  For <b>Unreal</b> <b>Engine</b> 5 (UE5) projects\n</td></tr>"
		"<tr><td><a rel=\"nofollow\" href=\"//duckduckgo.com/l/?uddg=https%3A%2F%2Fexample.com%2Fb&amp;rut=x\" class='result-link'>Second &amp; last</a></td></tr>");
	const TArray<FDDGResult> Results = ParseDDGLiteHtml(Html);
	if (!TestEqual(TEXT("two results"), Results.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("the uddg URL is decoded"), Results[0].Url, FString(TEXT("https://dev.epicgames.com/documentation/unreal-engine/enhanced-input-in-unreal-engine?lang=en-US")));
	TestEqual(TEXT("the snippet loses its tags and extra spaces"), Results[0].Snippet, FString(TEXT("For Unreal Engine 5 (UE5) projects")));
	TestEqual(TEXT("entities in a title are decoded"), Results[1].Title, FString(TEXT("Second & last")));
	TestTrue(TEXT("a result without a snippet does not take the next one's"), Results[1].Snippet.IsEmpty());

	const FString Page = HtmlPageToText(TEXT("<html><head><style>x{}</style><script>var a = 1;</script></head><body><h1>Title</h1><p>One &amp; two</p><div>Three</div></body></html>"));
	TestEqual(TEXT("a page becomes its text, one block per line"), Page, FString(TEXT("Title\nOne & two\nThree\n")));

	// dev.epicgames.com puts JSON in a multi-line attribute of a custom element; none of it may leak.
	const FString Attr = HtmlPageToText(TEXT("<nav links=\"[\n  {\n    &quot;id&quot;: &quot;notifications&quot;\n  }\n]\"></nav><!-- a > comment --><p>Body text</p>"));
	TestEqual(TEXT("a multi-line attribute and a comment leave no text"), Attr, FString(TEXT("Body text\n")));

	// Several scripts, in any case, and one never closed: each goes, the text between them stays.
	const FString Scripts = HtmlPageToText(TEXT("<p>A</p><script>1</script><p>B</p><SCRIPT type=\"x\">2</SCRIPT><p>C</p><script>never closed"));
	TestEqual(TEXT("every script goes, the text between them stays"), Scripts, FString(TEXT("A\nB\nC\n")));

	// U+1F600 is the surrogate pair D83D DE00 in an FString; a lone surrogate is not a character and stays as written.
	const TCHAR Grin[] = { static_cast<TCHAR>(0xD83D), static_cast<TCHAR>(0xDE00), 0 };
	TestEqual(TEXT("numeric entities past U+FFFF become surrogate pairs"),
		DecodeHtmlEntities(TEXT("&#x1F600;|&#128512;|&#65;|&#xD800;")), FString::Printf(TEXT("%s|%s|A|&#xD800;"), Grin, Grin));

	// The direct fetch only takes the web's own schemes.
	TestTrue(TEXT("https is fetched directly"), IsDirectlyFetchableUrl(TEXT("https://example.com/")));
	TestTrue(TEXT("http is fetched directly, in any case"), IsDirectlyFetchableUrl(TEXT("HTTP://EXAMPLE.COM/")));
	TestFalse(TEXT("file is not"), IsDirectlyFetchableUrl(TEXT("file:///C:/Windows/win.ini")));
	TestFalse(TEXT("ftp is not"), IsDirectlyFetchableUrl(TEXT("ftp://example.com/a.txt")));
	TestFalse(TEXT("gopher is not"), IsDirectlyFetchableUrl(TEXT("gopher://example.com/")));
	TestFalse(TEXT("a URL without a scheme is not"), IsDirectlyFetchableUrl(TEXT("example.com/page")));
	return true;
}
// On a worker thread the helper starts and cancels its request on the game thread and only waits; a cancel from the
// client wakes the wait. Test path prefix VibeUE.Research.*
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeUEResearchWorkerWaitIsCancellableTest, "VibeUE.Research.WorkerWaitIsCancellable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeUEResearchWorkerWaitIsCancellableTest::RunTest(const FString& Parameters)
{
	struct FState
	{
		TSharedRef<FVibeUEToolCancel, ESPMode::ThreadSafe> Cancel = MakeShared<FVibeUEToolCancel, ESPMode::ThreadSafe>();
		std::atomic<bool> bDone{false};
		FResearchHttpResult Result;
		double Started = 0.0;
		double Finished = 0.0;
		uint64 FrameAtStart = 0;
		uint64 FrameAtCancel = 0;
	};
	const TSharedRef<FState, ESPMode::ThreadSafe> S = MakeShared<FState, ESPMode::ThreadSafe>();

	// TEST-NET-1 (RFC 5737, never a real host): the connection hangs, so only the cancel can end the wait early.
	S->Started = FPlatformTime::Seconds();
	S->FrameAtStart = GFrameCounter;
	Async(EAsyncExecution::ThreadPool, [S]()
	{
		FVibeUEToolCancel::FScope CurrentCall(&S->Cancel.Get());
		S->Result = ResearchHttpGet(TEXT("http://192.0.2.1/"), TEXT("VibeUE test"), {}, 10.0f);
		S->Finished = FPlatformTime::Seconds();
		S->bDone = true;
	});

	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, S]()
	{
		TestFalse(TEXT("the worker is still waiting after 1 s"), S->bDone.load());
		S->FrameAtCancel = GFrameCounter;
		TestTrue(FString::Printf(TEXT("the game thread kept ticking while the worker waited (%llu frames)"), S->FrameAtCancel - S->FrameAtStart),
			S->FrameAtCancel - S->FrameAtStart >= 5);
		S->Cancel->Cancel();
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, S]()
	{
		if (!S->bDone.load())
		{
			if (FPlatformTime::Seconds() - S->Started > 15.0)
			{
				AddError(TEXT("The worker had not returned 15 s after the start."));
				return true;
			}
			return false;
		}
		const double Elapsed = S->Finished - S->Started;
		AddInfo(FString::Printf(TEXT("The cancelled wait returned %.2f s after the start (timeout 10 s)."), Elapsed));
		TestTrue(FString::Printf(TEXT("the cancel woke the wait well before the 10 s timeout (%.2f s)"), Elapsed), Elapsed < 4.0);
		TestFalse(TEXT("a cancelled request does not succeed"), S->Result.bSuccess);
		TestEqual(TEXT("it says it was cancelled"), S->Result.ErrorMessage, FString(TEXT("Request cancelled")));
		return true;
	}));
	return true;
}
#endif // WITH_AUTOMATION_TESTS
