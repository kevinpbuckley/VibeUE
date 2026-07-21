// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "FabCatalogClient.h"
#include "FabEndpoints.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HttpManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

namespace
{
	struct FHttpState
	{
		bool bDone = false;
		bool bOk = false;
		int32 Code = 0;
		FString Body;
	};

	bool SyncGetOnce(const FString& Url, double TimeoutSeconds, int32& OutCode, FString& OutBody)
	{
		TSharedPtr<FHttpState, ESPMode::ThreadSafe> State = MakeShared<FHttpState, ESPMode::ThreadSafe>();
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetVerb(TEXT("GET"));
		Request->SetURL(Url);
		Request->SetHeader(TEXT("accept"), TEXT("application/json"));
		// Matches the user-agent application used by Epic's UE Fab browser and avoids the public site's
		// browser-only Cloudflare challenge.
		Request->SetHeader(TEXT("User-Agent"), TEXT("Fab"));
		Request->OnProcessRequestComplete().BindLambda(
			[State](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
			{
				State->bDone = true;
				State->bOk = bOk;
				if (bOk && Response.IsValid())
				{
					State->Code = Response->GetResponseCode();
					State->Body = Response->GetContentAsString();
				}
			});
		Request->ProcessRequest();

		const double Start = FPlatformTime::Seconds();
		while (!State->bDone && (FPlatformTime::Seconds() - Start) < TimeoutSeconds)
		{
			FHttpModule::Get().GetHttpManager().Tick(0.f);
			FPlatformProcess::Sleep(0.01f);
		}
		if (!State->bDone)
		{
			Request->CancelRequest();
			OutCode = 0;
			OutBody.Reset();
			return false;
		}
		OutCode = State->Code;
		OutBody = State->Body;
		return State->bOk;
	}

	bool SyncGet(const FString& Url, double TimeoutSeconds, int32& OutCode, FString& OutBody)
	{
		const bool bFirstOk = SyncGetOnce(Url, TimeoutSeconds, OutCode, OutBody);
		// Fab intermittently challenges the first headless request even with its UE user-agent, then
		// accepts the identical follow-up. Retry exactly once; never loop on a persistent challenge.
		if (bFirstOk && OutCode == 403)
		{
			return SyncGetOnce(Url, TimeoutSeconds, OutCode, OutBody);
		}
		return bFirstOk;
	}

	bool ParseObject(const FString& Body, TSharedPtr<FJsonObject>& OutObject)
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	double EffectivePrice(const TSharedPtr<FJsonObject>& Price)
	{
		if (!Price.IsValid())
		{
			return -1.0;
		}
		double Value = 0.0;
		if (Price->HasTypedField<EJson::Number>(TEXT("discountedPrice"))
			&& Price->TryGetNumberField(TEXT("discountedPrice"), Value))
		{
			return Value;
		}
		return Price->TryGetNumberField(TEXT("price"), Value) ? Value : -1.0;
	}

	void ParseFormats(const TSharedPtr<FJsonObject>& Listing, TArray<FString>& OutFormats)
	{
		const TArray<TSharedPtr<FJsonValue>>* Formats = nullptr;
		if (!Listing->TryGetArrayField(TEXT("assetFormats"), Formats))
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Formats)
		{
			const TSharedPtr<FJsonObject>* Format = nullptr;
			const TSharedPtr<FJsonObject>* Type = nullptr;
			FString Code;
			if (Value.IsValid() && Value->TryGetObject(Format) && Format->IsValid()
				&& (*Format)->TryGetObjectField(TEXT("assetFormatType"), Type) && Type->IsValid()
				&& (*Type)->TryGetStringField(TEXT("code"), Code))
			{
				OutFormats.AddUnique(Code);
			}
		}
	}

	bool ParseSearchListing(const TSharedPtr<FJsonObject>& Object, FFabCatalogListing& Out)
	{
		Object->TryGetStringField(TEXT("uid"), Out.ListingId);
		Object->TryGetStringField(TEXT("title"), Out.Title);
		Object->TryGetStringField(TEXT("listingType"), Out.ListingType);
		Object->TryGetBoolField(TEXT("isMature"), Out.bMature);

		const TSharedPtr<FJsonObject>* User = nullptr;
		if (Object->TryGetObjectField(TEXT("user"), User) && User->IsValid())
		{
			(*User)->TryGetStringField(TEXT("sellerName"), Out.Seller);
		}
		const TSharedPtr<FJsonObject>* StartingPrice = nullptr;
		if (Object->TryGetObjectField(TEXT("startingPrice"), StartingPrice) && StartingPrice->IsValid())
		{
			Out.EffectiveStartingPrice = EffectivePrice(*StartingPrice);
		}
		const TArray<TSharedPtr<FJsonValue>>* Thumbnails = nullptr;
		if (Object->TryGetArrayField(TEXT("thumbnails"), Thumbnails) && Thumbnails->Num() > 0)
		{
			const TSharedPtr<FJsonObject>* Thumbnail = nullptr;
			if ((*Thumbnails)[0]->TryGetObject(Thumbnail) && Thumbnail->IsValid())
			{
				(*Thumbnail)->TryGetStringField(TEXT("mediaUrl"), Out.ThumbnailUrl);
			}
		}
		ParseFormats(Object, Out.Formats);

		// Search's `is_free=1` means the cheapest tier is free; for many Quixel listings that is only
		// "UEFN - Reference only" while downloadable Personal source files are paid. The compact search
		// response encodes the price in priceTierId: <id>_<currency>_<minor-units>_<timestamp>.
		const TArray<TSharedPtr<FJsonValue>>* Licenses = nullptr;
		if (Object->TryGetArrayField(TEXT("licenses"), Licenses))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Licenses)
			{
				const TSharedPtr<FJsonObject>* License = nullptr;
				FString Name;
				FString PriceTier;
				if (!Value.IsValid() || !Value->TryGetObject(License) || !License->IsValid()
					|| !(*License)->TryGetStringField(TEXT("name"), Name)
					|| !Name.Equals(TEXT("Personal"), ESearchCase::IgnoreCase)
					|| !(*License)->TryGetStringField(TEXT("priceTier"), PriceTier))
				{
					continue;
				}
				TArray<FString> Tokens;
				PriceTier.ParseIntoArray(Tokens, TEXT("_"), true);
				if (Tokens.Num() >= 3 && Tokens[Tokens.Num() - 2].IsNumeric())
				{
					const int64 MinorUnits = FCString::Atoi64(*Tokens[Tokens.Num() - 2]);
					Out.bPersonalLicenseFree = MinorUnits == 0;
				}
				break;
			}
		}
		return !Out.ListingId.IsEmpty() && !Out.Title.IsEmpty();
	}

	FString QualityToken(const FString& Quality)
	{
		const FString Lower = Quality.ToLower();
		if (Lower == TEXT("raw")) return TEXT("_raw");
		if (Lower == TEXT("high")) return TEXT("_high");
		if (Lower == TEXT("medium") || Lower == TEXT("mid")) return TEXT("_mid");
		if (Lower == TEXT("low")) return TEXT("_low");
		return FString();
	}

	FString NormalizedQuality(const FString& Quality)
	{
		const FString Lower = Quality.ToLower();
		if (Lower == TEXT("raw")) return TEXT("Raw");
		if (Lower == TEXT("high")) return TEXT("High");
		if (Lower == TEXT("medium") || Lower == TEXT("mid")) return TEXT("Medium");
		if (Lower == TEXT("low")) return TEXT("Low");
		return FString();
	}
}

bool FVibeFabCatalog::SearchFree(const FString& BaseUrl, const FString& Query, const FString& SellerFilter,
	                              const FString& FormatFilter, int32 Limit, const FString& Cursor,
	                              double TimeoutSeconds, TArray<FFabCatalogListing>& OutListings,
	                              FString& OutNextCursor, int32& OutHttpCode, FString& OutError)
{
	OutListings.Reset();
	OutNextCursor.Reset();
	const int32 SafeLimit = FMath::Clamp(Limit <= 0 ? 20 : Limit, 1, 100);
	const FString FormatLower = FormatFilter.ToLower();
	FString PageCursor = Cursor;
	TSet<FString> SeenCursors;
	for (int32 Page = 0; Page < 5 && OutListings.Num() < SafeLimit; ++Page)
	{
		if (!PageCursor.IsEmpty() && SeenCursors.Contains(PageCursor))
		{
			break;
		}
		SeenCursors.Add(PageCursor);
		const FString Url = VibeUE::Fab::CatalogSearchUrl(BaseUrl, Query, SellerFilter, PageCursor);
		FString Body;
		if (!SyncGet(Url, TimeoutSeconds, OutHttpCode, Body) || OutHttpCode < 200 || OutHttpCode >= 300)
		{
			OutError = FString::Printf(TEXT("Fab catalog search failed (HTTP %d)."), OutHttpCode);
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		if (!ParseObject(Body, Root))
		{
			OutError = TEXT("Fab catalog search response was not valid JSON.");
			return false;
		}
		FString NextCursor;
		const TSharedPtr<FJsonObject>* Cursors = nullptr;
		if (Root->TryGetObjectField(TEXT("cursors"), Cursors) && Cursors->IsValid())
		{
			(*Cursors)->TryGetStringField(TEXT("next"), NextCursor);
		}
		OutNextCursor = NextCursor;

		const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
		if (Root->TryGetArrayField(TEXT("results"), Results))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Results)
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				FFabCatalogListing Listing;
				if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object->IsValid()
					|| !ParseSearchListing(*Object, Listing))
				{
					continue;
				}
				// `is_free` alone includes UEFN-reference-only offers. Require the Personal tier itself
				// to encode zero minor currency units, and still revalidate the detail before import.
				if (!FMath::IsNearlyZero(Listing.EffectiveStartingPrice) || !Listing.bPersonalLicenseFree)
				{
					continue;
				}
				if (!FormatLower.IsEmpty() && !Listing.Formats.ContainsByPredicate(
					[&](const FString& Candidate) { return Candidate.ToLower().Contains(FormatLower); }))
				{
					continue;
				}
				OutListings.Add(MoveTemp(Listing));
				if (OutListings.Num() >= SafeLimit)
				{
					break;
				}
			}
		}
		if (NextCursor.IsEmpty())
		{
			break;
		}
		PageCursor = NextCursor;
	}
	return true;
}

bool FVibeFabCatalog::ResolveFreeDownload(const FString& BaseUrl, const FString& ListingId,
	                                       const FString& LicenseSlug, const FString& Quality,
	                                       const FString& Format, const FString& Platform,
	                                       double TimeoutSeconds, FFabFreeDownload& OutDownload,
	                                       int32& OutHttpCode, FString& OutError)
{
	const FString RequestedQuality = NormalizedQuality(Quality.IsEmpty() ? TEXT("High") : Quality);
	if (RequestedQuality.IsEmpty())
	{
		OutError = TEXT("Quality must be Raw, High, Medium, or Low.");
		return false;
	}
	const FString RequestedFormat = Format.IsEmpty() ? TEXT("gltf") : Format.ToLower();
	const FString RequestedLicense = LicenseSlug.IsEmpty() ? TEXT("personal") : LicenseSlug.ToLower();

	FString Body;
	if (!SyncGet(VibeUE::Fab::CatalogListingUrl(BaseUrl, ListingId), TimeoutSeconds, OutHttpCode, Body)
		|| OutHttpCode < 200 || OutHttpCode >= 300)
	{
		OutError = FString::Printf(TEXT("Fab listing request failed (HTTP %d)."), OutHttpCode);
		return false;
	}
	TSharedPtr<FJsonObject> Listing;
	if (!ParseObject(Body, Listing))
	{
		OutError = TEXT("Fab listing response was not valid JSON.");
		return false;
	}
	Listing->TryGetStringField(TEXT("uid"), OutDownload.ListingId);
	Listing->TryGetStringField(TEXT("title"), OutDownload.Title);
	Listing->TryGetStringField(TEXT("listingType"), OutDownload.ListingType);
	const TSharedPtr<FJsonObject>* User = nullptr;
	if (Listing->TryGetObjectField(TEXT("user"), User) && User->IsValid())
	{
		(*User)->TryGetStringField(TEXT("sellerName"), OutDownload.Seller);
	}

	bool bZeroPriceLicense = false;
	const TArray<TSharedPtr<FJsonValue>>* Licenses = nullptr;
	if (Listing->TryGetArrayField(TEXT("licenses"), Licenses))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Licenses)
		{
			const TSharedPtr<FJsonObject>* License = nullptr;
			FString Slug;
			const TSharedPtr<FJsonObject>* PriceTier = nullptr;
			if (Value.IsValid() && Value->TryGetObject(License) && License->IsValid()
				&& (*License)->TryGetStringField(TEXT("slug"), Slug)
				&& Slug.Equals(RequestedLicense, ESearchCase::IgnoreCase)
				&& (*License)->TryGetObjectField(TEXT("priceTier"), PriceTier) && PriceTier->IsValid()
				&& FMath::IsNearlyZero(EffectivePrice(*PriceTier)))
			{
				bZeroPriceLicense = true;
				break;
			}
		}
	}
	if (!bZeroPriceLicense)
	{
		OutError = FString::Printf(TEXT("Listing '%s' does not currently offer a zero-price '%s' license."),
			*OutDownload.Title, *RequestedLicense);
		return false;
	}

	if (!SyncGet(VibeUE::Fab::CatalogFormatsUrl(BaseUrl, ListingId), TimeoutSeconds, OutHttpCode, Body)
		|| OutHttpCode < 200 || OutHttpCode >= 300)
	{
		OutError = FString::Printf(TEXT("Fab asset-format request failed (HTTP %d)."), OutHttpCode);
		return false;
	}
	TArray<TSharedPtr<FJsonValue>> FormatArray;
	TSharedRef<TJsonReader<>> FormatsReader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(FormatsReader, FormatArray))
	{
		OutError = TEXT("Fab asset-format response was not valid JSON.");
		return false;
	}

	const FString Token = QualityToken(RequestedQuality);
	TArray<FString> AvailableFiles;
	for (const TSharedPtr<FJsonValue>& FormatValue : FormatArray)
	{
		const TSharedPtr<FJsonObject>* FormatObject = nullptr;
		const TSharedPtr<FJsonObject>* TypeObject = nullptr;
		FString Code;
		if (!FormatValue.IsValid() || !FormatValue->TryGetObject(FormatObject) || !FormatObject->IsValid()
			|| !(*FormatObject)->TryGetObjectField(TEXT("assetFormatType"), TypeObject) || !TypeObject->IsValid()
			|| !(*TypeObject)->TryGetStringField(TEXT("code"), Code)
			|| !Code.Equals(RequestedFormat, ESearchCase::IgnoreCase))
		{
			continue;
		}
		const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
		if (!(*FormatObject)->TryGetArrayField(TEXT("files"), Files))
		{
			continue;
		}
		for (const TSharedPtr<FJsonValue>& FileValue : *Files)
		{
			const TSharedPtr<FJsonObject>* File = nullptr;
			FString Name;
			FString Uid;
			if (!FileValue.IsValid() || !FileValue->TryGetObject(File) || !File->IsValid()
				|| !(*File)->TryGetStringField(TEXT("name"), Name))
			{
				continue;
			}
			AvailableFiles.Add(Name);
			if (OutDownload.FileUid.IsEmpty() && Name.ToLower().Contains(Token)
				&& (*File)->TryGetStringField(TEXT("uid"), Uid))
			{
				OutDownload.FileUid = Uid;
				OutDownload.FileName = Name;
				double Size = 0.0;
				if ((*File)->TryGetNumberField(TEXT("fileSize"), Size))
				{
					OutDownload.FileSize = static_cast<uint64>(FMath::Max(0.0, Size));
				}
			}
		}
	}
	if (OutDownload.FileUid.IsEmpty())
	{
		OutError = FString::Printf(TEXT("No %s/%s source file is available. Available files: %s"),
			*RequestedFormat, *RequestedQuality, *FString::Join(AvailableFiles, TEXT(", ")));
		return false;
	}

	OutDownload.Format = RequestedFormat;
	OutDownload.Quality = RequestedQuality;
	const FString DownloadInfoUrl = VibeUE::Fab::CatalogDownloadInfoUrl(
		BaseUrl, ListingId, RequestedFormat, OutDownload.FileUid,
		Platform.IsEmpty() ? TEXT("Windows") : Platform);
	if (!SyncGet(DownloadInfoUrl, TimeoutSeconds, OutHttpCode, Body)
		|| OutHttpCode < 200 || OutHttpCode >= 300)
	{
		OutError = FString::Printf(TEXT("Fab free download-info request failed (HTTP %d)."), OutHttpCode);
		return false;
	}
	TSharedPtr<FJsonObject> DownloadRoot;
	if (!ParseObject(Body, DownloadRoot))
	{
		OutError = TEXT("Fab download-info response was not valid JSON.");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* DownloadInfo = nullptr;
	if (!DownloadRoot->TryGetArrayField(TEXT("downloadInfo"), DownloadInfo) || DownloadInfo->Num() == 0)
	{
		OutError = TEXT("Fab download-info response contained no downloads.");
		return false;
	}
	const TSharedPtr<FJsonObject>* Info = nullptr;
	FString Type;
	if (!(*DownloadInfo)[0]->TryGetObject(Info) || !Info->IsValid()
		|| !(*Info)->TryGetStringField(TEXT("type"), Type) || Type != TEXT("binary")
		|| !(*Info)->TryGetStringField(TEXT("downloadUrl"), OutDownload.DownloadUrl)
		|| OutDownload.DownloadUrl.IsEmpty())
	{
		OutError = TEXT("Fab returned an unsupported or malformed free download.");
		return false;
	}
	return true;
}
