// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Compact public Fab catalog listing used by SearchFreeCatalog. */
struct FFabCatalogListing
{
	FString ListingId;
	FString Title;
	FString Seller;
	FString ListingType;
	FString ThumbnailUrl;
	TArray<FString> Formats;
	double EffectiveStartingPrice = -1.0;
	bool bMature = false;
	bool bPersonalLicenseFree = false;
};

/** Resolved public download for a listing that was revalidated as free. */
struct FFabFreeDownload
{
	FString ListingId;
	FString Title;
	FString Seller;
	FString ListingType;
	FString Format;
	FString Quality;
	FString FileUid;
	FString FileName;
	FString DownloadUrl;
	uint64 FileSize = 0;
};

/**
 * Public Fab catalog client. Fab officially permits anonymous downloads of free products; these
 * endpoints are the same /i/listings routes used by the UE 5.8 Fab web frontend. They are still an
 * unsupported web contract, so all endpoint strings remain isolated in FabEndpoints.h.
 */
class FVibeFabCatalog
{
public:
	/** Search the public catalog, restricted to listings whose effective starting price is exactly 0. */
	static bool SearchFree(const FString& BaseUrl, const FString& Query, const FString& SellerFilter,
	                       const FString& FormatFilter, int32 Limit, const FString& Cursor,
	                       double TimeoutSeconds, TArray<FFabCatalogListing>& OutListings,
	                       FString& OutNextCursor, int32& OutHttpCode, FString& OutError);

	/**
	 * Re-fetch a listing, require a zero-price license, select the requested source file, and obtain
	 * its short-lived signed URL. No entitlement/library mutation is performed.
	 */
	static bool ResolveFreeDownload(const FString& BaseUrl, const FString& ListingId,
	                                const FString& LicenseSlug, const FString& Quality,
	                                const FString& Format, const FString& Platform,
	                                double TimeoutSeconds, FFabFreeDownload& OutDownload,
	                                int32& OutHttpCode, FString& OutError);
};
