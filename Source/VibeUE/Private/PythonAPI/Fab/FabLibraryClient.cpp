// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "FabLibraryClient.h"
#include "FabEndpoints.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

// ---------------------------------------------------------------------------
// FFabLibraryAsset helpers
// ---------------------------------------------------------------------------

bool FFabLibraryAsset::SupportsEngine(const FString& EngineVersion) const
{
	if (EngineVersion.IsEmpty())
	{
		return true;
	}
	for (const FFabProjectVersion& PV : ProjectVersions)
	{
		for (const FString& EV : PV.EngineVersions)
		{
			// Match "5.8" against "5.8" or "UE_5.8" style tokens, lenient on prefix.
			if (EV == EngineVersion || EV.EndsWith(EngineVersion))
			{
				return true;
			}
		}
	}
	// No projectVersions at all → unknown; treat as compatible so we don't hide items on sparse data.
	return ProjectVersions.Num() == 0;
}

TArray<FString> FFabLibraryAsset::AllEngineVersions() const
{
	TArray<FString> Out;
	for (const FFabProjectVersion& PV : ProjectVersions)
	{
		for (const FString& EV : PV.EngineVersions)
		{
			Out.AddUnique(EV);
		}
	}
	return Out;
}

FString FFabLibraryAsset::DeriveAssetType() const
{
	const FString SourceLower = Source.ToLower();
	const FString DistLower = DistributionMethod.ToLower();
	if (SourceLower.Contains(TEXT("quixel")) || SourceLower.Contains(TEXT("megascan")))
	{
		return TEXT("quixel");
	}
	if (DistLower.Contains(TEXT("plugin")))
	{
		return TEXT("plugin");
	}
	if (DistLower.Contains(TEXT("complete_project")) || DistLower.Contains(TEXT("complete project")))
	{
		return TEXT("complete-project");
	}
	if (DistLower.Contains(TEXT("asset_pack")) || DistLower.Contains(TEXT("asset pack")) || DistLower.Contains(TEXT("engine")))
	{
		return TEXT("unreal-engine");
	}
	// glTF/FBX 3D models come through as generic downloads.
	return TEXT("model");
}

FString FFabLibraryAsset::ArtifactIdForEngine(const FString& EngineVersion) const
{
	if (!EngineVersion.IsEmpty())
	{
		for (const FFabProjectVersion& PV : ProjectVersions)
		{
			for (const FString& EV : PV.EngineVersions)
			{
				if ((EV == EngineVersion || EV.EndsWith(EngineVersion)) && !PV.ArtifactId.IsEmpty())
				{
					return PV.ArtifactId;
				}
			}
		}
	}
	return ProjectVersions.Num() > 0 ? ProjectVersions[0].ArtifactId : FString();
}

// ---------------------------------------------------------------------------
// Synchronous, bounded HTTP GET (uses a shared state so a timed-out request's
// completion lambda never touches freed stack).
// ---------------------------------------------------------------------------

namespace
{
	struct FHttpState
	{
		bool bDone = false;
		bool bOk = false;
		int32 Code = 0;
		FString Body;
	};

	bool SyncGet(const FString& Url, const FString& Bearer, double TimeoutSeconds, int32& OutCode, FString& OutBody)
	{
		TSharedPtr<FHttpState, ESPMode::ThreadSafe> St = MakeShared<FHttpState, ESPMode::ThreadSafe>();

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
		Req->SetVerb(TEXT("GET"));
		Req->SetURL(Url);
		Req->SetHeader(TEXT("accept"), TEXT("application/json"));
		Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Bearer));
		Req->OnProcessRequestComplete().BindLambda(
			[St](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
			{
				St->bDone = true;
				St->bOk = bOk;
				if (bOk && Resp.IsValid())
				{
					St->Code = Resp->GetResponseCode();
					St->Body = Resp->GetContentAsString();
				}
			});
		Req->ProcessRequest();

		const double Start = FPlatformTime::Seconds();
		while (!St->bDone && (FPlatformTime::Seconds() - Start) < TimeoutSeconds)
		{
			FHttpModule::Get().GetHttpManager().Tick(0.f);
			FPlatformProcess::Sleep(0.01f);
		}

		if (!St->bDone)
		{
			Req->CancelRequest();
			OutCode = 0;
			OutBody.Reset();
			return false;
		}
		OutCode = St->Code;
		OutBody = St->Body;
		return St->bOk;
	}

	void ParseStringArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, TArray<FString>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj->TryGetArrayField(Field, Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S))
				{
					Out.Add(S);
				}
			}
		}
	}

	void ParseProjectVersions(const TSharedPtr<FJsonObject>& Item, TArray<FFabProjectVersion>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Item->TryGetArrayField(TEXT("projectVersions"), Arr))
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject>* PVObj = nullptr;
			if (!V.IsValid() || !V->TryGetObject(PVObj) || !PVObj->IsValid())
			{
				continue;
			}
			FFabProjectVersion PV;
			// artifactId may be a string or an object depending on the feed — accept either.
			if (!(*PVObj)->TryGetStringField(TEXT("artifactId"), PV.ArtifactId))
			{
				const TSharedPtr<FJsonObject>* ArtObj = nullptr;
				if ((*PVObj)->TryGetObjectField(TEXT("artifactId"), ArtObj) && ArtObj->IsValid())
				{
					(*ArtObj)->TryGetStringField(TEXT("id"), PV.ArtifactId);
				}
			}
			ParseStringArray(*PVObj, TEXT("engineVersions"), PV.EngineVersions);
			ParseStringArray(*PVObj, TEXT("targetPlatforms"), PV.TargetPlatforms);
			ParseStringArray(*PVObj, TEXT("buildVersions"), PV.BuildVersions);
			Out.Add(MoveTemp(PV));
		}
	}

	void ParseImages(const TSharedPtr<FJsonObject>& Item, TArray<FFabLibraryAsset::FImage>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Item->TryGetArrayField(TEXT("images"), Arr))
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject>* ImgObj = nullptr;
			if (!V.IsValid() || !V->TryGetObject(ImgObj) || !ImgObj->IsValid())
			{
				continue;
			}
			FFabLibraryAsset::FImage Img;
			(*ImgObj)->TryGetStringField(TEXT("url"), Img.Url);
			(*ImgObj)->TryGetStringField(TEXT("type"), Img.Type);
			(*ImgObj)->TryGetNumberField(TEXT("width"), Img.Width);
			(*ImgObj)->TryGetNumberField(TEXT("height"), Img.Height);
			Out.Add(MoveTemp(Img));
		}
	}

	// Parse one page. Returns the next cursor (empty if none) via OutNextCursor.
	bool ParsePage(const FString& Body, TArray<FFabLibraryAsset>& OutAssets, FString& OutNextCursor, FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("Library response was not valid JSON.");
			return false;
		}

		const TSharedPtr<FJsonObject>* Cursors = nullptr;
		if (Root->TryGetObjectField(TEXT("cursors"), Cursors) && Cursors->IsValid())
		{
			(*Cursors)->TryGetStringField(TEXT("next"), OutNextCursor);
		}

		const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
		if (!Root->TryGetArrayField(TEXT("results"), Results))
		{
			// No results array is only an error if there's also no cursor — treat empty page as success.
			return true;
		}
		for (const TSharedPtr<FJsonValue>& V : *Results)
		{
			const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
			if (!V.IsValid() || !V->TryGetObject(ItemPtr) || !ItemPtr->IsValid())
			{
				continue;
			}
			const TSharedPtr<FJsonObject>& Item = *ItemPtr;
			FFabLibraryAsset A;
			Item->TryGetStringField(TEXT("assetId"), A.AssetId);
			Item->TryGetStringField(TEXT("assetNamespace"), A.AssetNamespace);
			Item->TryGetStringField(TEXT("title"), A.Title);
			Item->TryGetStringField(TEXT("description"), A.Description);
			Item->TryGetStringField(TEXT("listingType"), A.ListingType);
			Item->TryGetStringField(TEXT("seller"), A.Seller);
			Item->TryGetStringField(TEXT("source"), A.Source);
			Item->TryGetStringField(TEXT("distributionMethod"), A.DistributionMethod);
			Item->TryGetStringField(TEXT("url"), A.Url);
			ParseProjectVersions(Item, A.ProjectVersions);
			ParseImages(Item, A.Images);
			if (!A.AssetId.IsEmpty())
			{
				OutAssets.Add(MoveTemp(A));
			}
		}
		return true;
	}
}

bool FVibeFabLibrary::Fetch(const FString& BaseUrl, const FString& EpicAccountId, const FString& BearerToken,
                            int32 PageSize, double TimeoutSeconds,
                            TArray<FFabLibraryAsset>& OutAssets, int32& OutHttpCode, FString& OutError)
{
	OutAssets.Reset();
	OutHttpCode = 0;
	if (EpicAccountId.IsEmpty() || BearerToken.IsEmpty())
	{
		OutError = TEXT("Missing Epic account id or auth token.");
		return false;
	}

	const double Deadline = FPlatformTime::Seconds() + TimeoutSeconds;
	FString Cursor;
	const int32 MaxPages = 100;   // safety cap — 100 * 1000 items is far beyond any real library
	for (int32 Page = 0; Page < MaxPages; ++Page)
	{
		const double Remaining = Deadline - FPlatformTime::Seconds();
		if (Remaining <= 0.0)
		{
			OutError = TEXT("Timed out fetching the Fab library.");
			return false;
		}
		const FString Url = VibeUE::Fab::LibraryUrl(BaseUrl, EpicAccountId, PageSize, Cursor);
		FString Body;
		const bool bOk = SyncGet(Url, BearerToken, Remaining, OutHttpCode, Body);
		if (!bOk || OutHttpCode < 200 || OutHttpCode >= 300)
		{
			OutError = OutHttpCode == 401 || OutHttpCode == 403
				? FString::Printf(TEXT("Fab library request was rejected (HTTP %d) — the auth token may be expired or lack fab.com scope."), OutHttpCode)
				: FString::Printf(TEXT("Fab library request failed (HTTP %d)."), OutHttpCode);
			return false;
		}

		FString NextCursor;
		if (!ParsePage(Body, OutAssets, NextCursor, OutError))
		{
			return false;
		}
		if (NextCursor.IsEmpty())
		{
			break;
		}
		Cursor = NextCursor;
	}

	return true;
}
