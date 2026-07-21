// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "FabManifestClient.h"
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

FString FFabDownloadInfo::ToDownloadUrl() const
{
	// FFabDownloadRequest (BuildPatch) expects "manifestURL,baseURL[,baseURL2,...]" split on the FIRST comma.
	FString Url = ManifestOrFileUrl;
	for (const FString& Base : BaseUrls)
	{
		Url += TEXT(",");
		Url += Base;
	}
	return Url;
}

namespace
{
	bool SyncPost(const FString& Url, const FString& Bearer, const FString& Body,
	              double TimeoutSeconds, int32& OutCode, FString& OutBody)
	{
		struct FState { bool bDone = false; bool bOk = false; int32 Code = 0; FString Body; };
		TSharedPtr<FState, ESPMode::ThreadSafe> St = MakeShared<FState, ESPMode::ThreadSafe>();

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
		Req->SetVerb(TEXT("POST"));
		Req->SetURL(Url);
		Req->SetHeader(TEXT("accept"), TEXT("application/json"));
		Req->SetHeader(TEXT("content-type"), TEXT("application/json"));
		Req->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Bearer));
		Req->SetContentAsString(Body);
		Req->OnProcessRequestComplete().BindLambda(
			[St](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
			{
				St->bDone = true; St->bOk = bOk;
				if (bOk && Resp.IsValid()) { St->Code = Resp->GetResponseCode(); St->Body = Resp->GetContentAsString(); }
			});
		Req->ProcessRequest();

		const double Start = FPlatformTime::Seconds();
		while (!St->bDone && (FPlatformTime::Seconds() - Start) < TimeoutSeconds)
		{
			FHttpModule::Get().GetHttpManager().Tick(0.f);
			FPlatformProcess::Sleep(0.01f);
		}
		if (!St->bDone) { Req->CancelRequest(); OutCode = 0; return false; }
		OutCode = St->Code; OutBody = St->Body; return St->bOk;
	}
}

bool FVibeFabManifest::Fetch(const FString& BaseUrl, const FString& ArtifactId, const FString& AssetNamespace,
                             const FString& AssetId, const FString& Platform, const FString& BearerToken,
                             double TimeoutSeconds, FFabDownloadInfo& OutInfo, int32& OutHttpCode, FString& OutError)
{
	if (ArtifactId.IsEmpty() || AssetNamespace.IsEmpty() || AssetId.IsEmpty())
	{
		OutError = TEXT("Missing artifactId / namespace / itemId for the manifest request.");
		return false;
	}

	const FString Url = VibeUE::Fab::ArtifactManifestUrl(BaseUrl, ArtifactId);
	const FString Body = FString::Printf(
		TEXT("{\"namespace\":\"%s\",\"itemId\":\"%s\",\"platform\":\"%s\"}"),
		*AssetNamespace, *AssetId, *(Platform.IsEmpty() ? FString(TEXT("Windows")) : Platform));

	FString Resp;
	if (!SyncPost(Url, BearerToken, Body, TimeoutSeconds, OutHttpCode, Resp) || OutHttpCode < 200 || OutHttpCode >= 300)
	{
		OutError = FString::Printf(TEXT("Manifest request failed (HTTP %d): %s"), OutHttpCode, *Resp.Left(300));
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Resp);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Manifest response was not valid JSON.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* DownloadInfo = nullptr;
	if (!Root->TryGetArrayField(TEXT("downloadInfo"), DownloadInfo) || DownloadInfo->Num() == 0)
	{
		OutError = TEXT("Manifest response had no downloadInfo entries.");
		return false;
	}

	const TSharedPtr<FJsonObject>* InfoObj = nullptr;
	if (!(*DownloadInfo)[0]->TryGetObject(InfoObj) || !InfoObj->IsValid())
	{
		OutError = TEXT("Malformed downloadInfo entry.");
		return false;
	}
	const TSharedPtr<FJsonObject>& Info = *InfoObj;

	Info->TryGetStringField(TEXT("type"), OutInfo.RawType);
	Info->TryGetStringField(TEXT("assetFormat"), OutInfo.AssetFormat);
	Info->TryGetStringField(TEXT("buildVersion"), OutInfo.BuildVersion);

	if (OutInfo.RawType != TEXT("manifest"))
	{
		OutError = FString::Printf(TEXT("Unsupported download type '%s' (only BuildPatch 'manifest' assets are supported so far)."), *OutInfo.RawType);
		return false;
	}
	OutInfo.bIsBuildPatch = true;

	// Signed manifest URL — take the first distribution point (CDN order is fine; they are equivalent).
	const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
	if (Info->TryGetArrayField(TEXT("distributionPoints"), Points) && Points->Num() > 0)
	{
		const TSharedPtr<FJsonObject>* P0 = nullptr;
		if ((*Points)[0]->TryGetObject(P0) && P0->IsValid())
		{
			(*P0)->TryGetStringField(TEXT("manifestUrl"), OutInfo.ManifestOrFileUrl);
		}
	}
	if (OutInfo.ManifestOrFileUrl.IsEmpty())
	{
		OutError = TEXT("Manifest response had no signed manifestUrl.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Bases = nullptr;
	if (Info->TryGetArrayField(TEXT("distributionPointBaseUrls"), Bases))
	{
		for (const TSharedPtr<FJsonValue>& V : *Bases)
		{
			FString B;
			if (V.IsValid() && V->TryGetString(B) && !B.IsEmpty())
			{
				OutInfo.BaseUrls.Add(B);
			}
		}
	}
	if (OutInfo.BaseUrls.Num() == 0)
	{
		OutError = TEXT("Manifest response had no distributionPointBaseUrls.");
		return false;
	}

	return true;
}
