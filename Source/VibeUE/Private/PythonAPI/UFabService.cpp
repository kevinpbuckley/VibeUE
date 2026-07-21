// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UFabService.h"
#include "Fab/FabAuthBridge.h"
#include "Fab/FabLibraryClient.h"
#include "Fab/FabManifestClient.h"
#include "Fab/FabImportDriver.h"
#include "Fab/FabCatalogClient.h"
#include "Fab/FabEndpoints.h"

#include "Json.h"
#include "Misc/EngineVersion.h"

DEFINE_LOG_CATEGORY_STATIC(LogFabService, Log, All);

// ---------------------------------------------------------------------------
// Session state — the owned library is fetched once and cached in-process.
// ---------------------------------------------------------------------------

static TArray<FFabLibraryAsset> GLibraryCache;
static bool GLibraryCached = false;

// ---------------------------------------------------------------------------
// JSON response helpers (same contract as the other VibeUE services)
// ---------------------------------------------------------------------------

static FString OkJson(TSharedPtr<FJsonObject> Obj)
{
	Obj->SetBoolField(TEXT("success"), true);
	FString Out;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), W);
	return Out;
}

static FString ErrJson(const FString& Code, const FString& Msg)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), false);
	Obj->SetStringField(TEXT("error_code"), Code);
	Obj->SetStringField(TEXT("error"), Msg);
	FString Out;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), W);
	return Out;
}

// "5.8" for the running engine.
static FString CurrentEngineVersion()
{
	const FEngineVersion& V = FEngineVersion::Current();
	return FString::Printf(TEXT("%u.%u"), (uint32)V.GetMajor(), (uint32)V.GetMinor());
}

static FString BaseUrl()
{
	// Prod is the engine Fab plugin's default environment. (Environment overrides live in the Fab
	// plugin's private settings; if we ever need them, expose GetUrlFromEnvironment via a small patch.)
	return VibeUE::Fab::ProdBaseUrl();
}

static FString SafeAssetFolder(const FString& Title, const FString& ListingId)
{
	FString Safe = Title;
	for (int32 Index = 0; Index < Safe.Len(); ++Index)
	{
		if (!FChar::IsAlnum(Safe[Index]) && Safe[Index] != TEXT('_'))
		{
			Safe[Index] = TEXT('_');
		}
	}
	while (Safe.Contains(TEXT("__")))
	{
		Safe.ReplaceInline(TEXT("__"), TEXT("_"));
	}
	Safe = Safe.Left(64);
	while (Safe.RemoveFromStart(TEXT("_"))) {}
	while (Safe.RemoveFromEnd(TEXT("_"))) {}
	return Safe.IsEmpty() ? TEXT("FabAsset_") + ListingId.Left(8) : Safe;
}

// Ensure logged in, returning a ready-to-return NOT_AUTHENTICATED error JSON in OutErr if not.
static bool RequireAuth(double TimeoutSeconds, FString& OutErr)
{
	if (!FVibeFabAuth::IsSupported())
	{
		OutErr = ErrJson(TEXT("UNSUPPORTED"), TEXT("The EOS SDK is not available in this editor build, so Fab auth is unavailable."));
		return false;
	}
	FString Err;
	if (!FVibeFabAuth::EnsureLoggedIn(TimeoutSeconds, Err))
	{
		const FString State = FVibeFabAuth::GetStateString();
		const FString Code = State == TEXT("pending") ? TEXT("AUTH_PENDING") : TEXT("NOT_AUTHENTICATED");
		OutErr = ErrJson(Code, Err.IsEmpty()
			? TEXT("Not signed into Fab. Open the Fab window (or launch the editor from the Epic Games Launcher) to sign in, then retry.")
			: Err);
		return false;
	}
	return true;
}

// Fill the in-process cache if needed.
static bool EnsureLibrary(bool bRefresh, FString& OutErr)
{
	if (GLibraryCached && !bRefresh)
	{
		return true;
	}
	const FString Token = FVibeFabAuth::GetAccessToken();
	const FString Account = FVibeFabAuth::GetEpicAccountId();
	int32 HttpCode = 0;
	FString Err;
	TArray<FFabLibraryAsset> Fetched;
	if (!FVibeFabLibrary::Fetch(BaseUrl(), Account, Token, /*PageSize*/ 1000, /*Timeout*/ 45.0, Fetched, HttpCode, Err))
	{
		OutErr = ErrJson(HttpCode == 401 || HttpCode == 403 ? TEXT("AUTH_REJECTED") : TEXT("LIBRARY_FETCH_FAILED"), Err);
		return false;
	}
	GLibraryCache = MoveTemp(Fetched);
	GLibraryCached = true;
	return true;
}

static const FFabLibraryAsset* FindAsset(const FString& AssetId)
{
	return GLibraryCache.FindByPredicate([&](const FFabLibraryAsset& A) { return A.AssetId == AssetId; });
}

// Compact projection for ListLibrary.
static TSharedPtr<FJsonObject> CompactAsset(const FFabLibraryAsset& A, const FString& EngineVersion)
{
	TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("id"), A.AssetId);
	O->SetStringField(TEXT("title"), A.Title);
	O->SetStringField(TEXT("type"), A.DeriveAssetType());
	O->SetStringField(TEXT("source"), A.Source);
	O->SetStringField(TEXT("distribution_method"), A.DistributionMethod);
	O->SetStringField(TEXT("listing_type"), A.ListingType);
	O->SetBoolField(TEXT("compatible"), A.SupportsEngine(EngineVersion));
	if (A.Images.Num() > 0)
	{
		O->SetStringField(TEXT("thumbnail_url"), A.Images[0].Url);
	}
	return O;
}

// ---------------------------------------------------------------------------
// AuthStatus
// ---------------------------------------------------------------------------

FString UFabService::AuthStatus(float TimeoutSeconds)
{
	if (!FVibeFabAuth::IsSupported())
	{
		return ErrJson(TEXT("UNSUPPORTED"), TEXT("The EOS SDK is not available in this editor build, so Fab auth is unavailable."));
	}

	FString Err;
	const bool bLoggedIn = FVibeFabAuth::EnsureLoggedIn(FMath::Max(1.0f, TimeoutSeconds), Err);
	const FString State = FVibeFabAuth::GetStateString();

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("authenticated"), bLoggedIn);
	Obj->SetStringField(TEXT("state"), State);
	Obj->SetStringField(TEXT("engine_version"), CurrentEngineVersion());
	if (bLoggedIn)
	{
		Obj->SetStringField(TEXT("epic_account_id"), FVibeFabAuth::GetEpicAccountId());
		Obj->SetStringField(TEXT("message"), TEXT("Signed into Fab. The owned library is reachable."));
	}
	else if (State == TEXT("pending"))
	{
		Obj->SetStringField(TEXT("message"), TEXT("Login in progress — call auth_status again in a moment to let it resolve."));
	}
	else
	{
		Obj->SetStringField(TEXT("message"), Err.IsEmpty()
			? TEXT("Not signed into Fab. Open the Fab window (Window > Fab) or launch the editor from the Epic Games Launcher, then retry.")
			: Err);
	}
	return OkJson(Obj);
}

// ---------------------------------------------------------------------------
// ListLibrary
// ---------------------------------------------------------------------------

FString UFabService::ListLibrary(const FString& NameFilter, const FString& TypeFilter, const FString& EngineVersion,
                                 int32 Limit, int32 Offset, bool Refresh)
{
	FString AuthErr;
	if (!RequireAuth(15.0, AuthErr))
	{
		return AuthErr;
	}
	FString LibErr;
	if (!EnsureLibrary(Refresh, LibErr))
	{
		return LibErr;
	}

	const FString EngineVer = EngineVersion.IsEmpty() ? CurrentEngineVersion() : EngineVersion;
	const FString NameLower = NameFilter.ToLower();
	const FString TypeLower = TypeFilter.ToLower();

	// Filter.
	TArray<const FFabLibraryAsset*> Filtered;
	for (const FFabLibraryAsset& A : GLibraryCache)
	{
		if (!NameLower.IsEmpty() && !A.Title.ToLower().Contains(NameLower))
		{
			continue;
		}
		if (!TypeLower.IsEmpty())
		{
			const bool bTypeMatch = A.DeriveAssetType().ToLower().Contains(TypeLower)
				|| A.DistributionMethod.ToLower().Contains(TypeLower)
				|| A.Source.ToLower().Contains(TypeLower);
			if (!bTypeMatch)
			{
				continue;
			}
		}
		Filtered.Add(&A);
	}

	// Page.
	const int32 SafeOffset = FMath::Clamp(Offset, 0, Filtered.Num());
	const int32 SafeLimit = Limit <= 0 ? Filtered.Num() : Limit;
	const int32 End = FMath::Min(SafeOffset + SafeLimit, Filtered.Num());

	TArray<TSharedPtr<FJsonValue>> Results;
	int32 CompatibleCount = 0;
	for (int32 i = SafeOffset; i < End; ++i)
	{
		Results.Add(MakeShared<FJsonValueObject>(CompactAsset(*Filtered[i], EngineVer)));
	}
	for (const FFabLibraryAsset* A : Filtered)
	{
		if (A->SupportsEngine(EngineVer))
		{
			++CompatibleCount;
		}
	}

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetArrayField(TEXT("results"), Results);
	Obj->SetNumberField(TEXT("returned"), Results.Num());
	Obj->SetNumberField(TEXT("total_matched"), Filtered.Num());
	Obj->SetNumberField(TEXT("total_owned"), GLibraryCache.Num());
	Obj->SetNumberField(TEXT("total_compatible"), CompatibleCount);
	Obj->SetNumberField(TEXT("offset"), SafeOffset);
	Obj->SetStringField(TEXT("engine_version"), EngineVer);
	return OkJson(Obj);
}

// ---------------------------------------------------------------------------
// GetAsset
// ---------------------------------------------------------------------------

FString UFabService::GetAsset(const FString& AssetId)
{
	if (AssetId.IsEmpty())
	{
		return ErrJson(TEXT("BAD_ARGUMENT"), TEXT("AssetId is required."));
	}
	FString AuthErr;
	if (!RequireAuth(15.0, AuthErr))
	{
		return AuthErr;
	}
	FString LibErr;
	if (!EnsureLibrary(false, LibErr))
	{
		return LibErr;
	}

	const FFabLibraryAsset* A = FindAsset(AssetId);
	if (!A)
	{
		return ErrJson(TEXT("ASSET_NOT_OWNED"), FString::Printf(TEXT("No owned asset with id '%s' in the library. Call list_library (Refresh=true) if it was purchased recently."), *AssetId));
	}

	const FString EngineVer = CurrentEngineVersion();

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("id"), A->AssetId);
	Obj->SetStringField(TEXT("asset_namespace"), A->AssetNamespace);
	Obj->SetStringField(TEXT("title"), A->Title);
	Obj->SetStringField(TEXT("description"), A->Description);
	Obj->SetStringField(TEXT("listing_type"), A->ListingType);
	Obj->SetStringField(TEXT("seller"), A->Seller);
	Obj->SetStringField(TEXT("source"), A->Source);
	Obj->SetStringField(TEXT("distribution_method"), A->DistributionMethod);
	Obj->SetStringField(TEXT("type"), A->DeriveAssetType());
	Obj->SetStringField(TEXT("url"), A->Url);
	Obj->SetBoolField(TEXT("compatible"), A->SupportsEngine(EngineVer));
	Obj->SetStringField(TEXT("engine_version"), EngineVer);

	TArray<TSharedPtr<FJsonValue>> Versions;
	for (const FFabProjectVersion& PV : A->ProjectVersions)
	{
		TSharedPtr<FJsonObject> V = MakeShared<FJsonObject>();
		V->SetStringField(TEXT("artifact_id"), PV.ArtifactId);
		TArray<TSharedPtr<FJsonValue>> EVs;
		for (const FString& EV : PV.EngineVersions) { EVs.Add(MakeShared<FJsonValueString>(EV)); }
		V->SetArrayField(TEXT("engine_versions"), EVs);
		TArray<TSharedPtr<FJsonValue>> Plats;
		for (const FString& P : PV.TargetPlatforms) { Plats.Add(MakeShared<FJsonValueString>(P)); }
		V->SetArrayField(TEXT("target_platforms"), Plats);
		Versions.Add(MakeShared<FJsonValueObject>(V));
	}
	Obj->SetArrayField(TEXT("project_versions"), Versions);

	TArray<TSharedPtr<FJsonValue>> AllEng;
	for (const FString& EV : A->AllEngineVersions()) { AllEng.Add(MakeShared<FJsonValueString>(EV)); }
	Obj->SetArrayField(TEXT("engine_versions"), AllEng);

	TArray<TSharedPtr<FJsonValue>> Imgs;
	for (const FFabLibraryAsset::FImage& Img : A->Images)
	{
		TSharedPtr<FJsonObject> IO = MakeShared<FJsonObject>();
		IO->SetStringField(TEXT("url"), Img.Url);
		IO->SetStringField(TEXT("type"), Img.Type);
		IO->SetNumberField(TEXT("width"), Img.Width);
		IO->SetNumberField(TEXT("height"), Img.Height);
		Imgs.Add(MakeShared<FJsonValueObject>(IO));
	}
	Obj->SetArrayField(TEXT("images"), Imgs);

	return OkJson(Obj);
}

// ---------------------------------------------------------------------------
// ImportAsset / ImportStatus — Phase 2 (download + import). The download half of
// the engine Fab plugin is FAB_API-exported and reusable; the built-in IMPORT
// workflows are Private and the signed-manifest negotiation is not in engine C++,
// so this path is finalized after Phase 1 (discovery) is validated live and the
// import-reuse approach is chosen. Until then these report their status honestly
// rather than pretending to import. See docs/design/fab-service-spec.md §2/§4.1.
// ---------------------------------------------------------------------------

FString UFabService::SearchFreeCatalog(const FString& Query, const FString& SellerFilter,
	                                    const FString& FormatFilter, int32 Limit, const FString& Cursor)
{
	TArray<FFabCatalogListing> Listings;
	FString NextCursor;
	FString Error;
	int32 HttpCode = 0;
	if (!FVibeFabCatalog::SearchFree(BaseUrl(), Query, SellerFilter, FormatFilter, Limit, Cursor,
	                                 30.0, Listings, NextCursor, HttpCode, Error))
	{
		return ErrJson(TEXT("CATALOG_FETCH_FAILED"), Error);
	}

	TArray<TSharedPtr<FJsonValue>> Results;
	for (const FFabCatalogListing& Listing : Listings)
	{
		TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("id"), Listing.ListingId);
		Item->SetStringField(TEXT("title"), Listing.Title);
		Item->SetStringField(TEXT("seller"), Listing.Seller);
		Item->SetStringField(TEXT("listing_type"), Listing.ListingType);
		Item->SetNumberField(TEXT("price"), Listing.EffectiveStartingPrice);
		Item->SetStringField(TEXT("license_slug"), TEXT("personal"));
		Item->SetBoolField(TEXT("mature"), Listing.bMature);
		Item->SetStringField(TEXT("listing_url"), BaseUrl() + TEXT("/listings/") + Listing.ListingId);
		if (!Listing.ThumbnailUrl.IsEmpty())
		{
			Item->SetStringField(TEXT("thumbnail_url"), Listing.ThumbnailUrl);
		}
		TArray<TSharedPtr<FJsonValue>> Formats;
		for (const FString& SourceFormat : Listing.Formats)
		{
			Formats.Add(MakeShared<FJsonValueString>(SourceFormat));
		}
		Item->SetArrayField(TEXT("formats"), Formats);
		Results.Add(MakeShared<FJsonValueObject>(Item));
	}

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetArrayField(TEXT("results"), Results);
	Obj->SetNumberField(TEXT("returned"), Results.Num());
	Obj->SetStringField(TEXT("next_cursor"), NextCursor);
	Obj->SetBoolField(TEXT("eula_acceptance_required_for_import"), true);
	Obj->SetBoolField(TEXT("library_changed"), false);
	return OkJson(Obj);
}

FString UFabService::ImportAsset(const FString& AssetId, const FString& EngineVersion, const FString& Quality, const FString& Format)
{
	if (AssetId.IsEmpty())
	{
		return ErrJson(TEXT("BAD_ARGUMENT"), TEXT("AssetId is required."));
	}
	FString AuthErr;
	if (!RequireAuth(15.0, AuthErr))
	{
		return AuthErr;
	}
	FString LibErr;
	if (!EnsureLibrary(false, LibErr))
	{
		return LibErr;
	}
	const FFabLibraryAsset* A = FindAsset(AssetId);
	if (!A)
	{
		return ErrJson(TEXT("ASSET_NOT_OWNED"), FString::Printf(TEXT("No owned asset with id '%s'."), *AssetId));
	}
	const FString EngineVer = EngineVersion.IsEmpty() ? CurrentEngineVersion() : EngineVersion;
	if (!A->SupportsEngine(EngineVer))
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetBoolField(TEXT("success"), false);
		Obj->SetStringField(TEXT("error_code"), TEXT("ENGINE_VERSION_MISMATCH"));
		Obj->SetStringField(TEXT("error"), FString::Printf(TEXT("'%s' has no version for engine %s."), *A->Title, *EngineVer));
		TArray<TSharedPtr<FJsonValue>> AllEng;
		for (const FString& EV : A->AllEngineVersions()) { AllEng.Add(MakeShared<FJsonValueString>(EV)); }
		Obj->SetArrayField(TEXT("available_engine_versions"), AllEng);
		FString Out; TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out); FJsonSerializer::Serialize(Obj.ToSharedRef(), W); return Out;
	}

	// Idempotent: if this asset already imported this session, report it rather than re-downloading.
	if (TSharedPtr<FFabImportProgress> Prev = FVibeFabImport::Get(AssetId))
	{
		if (Prev->Phase == FFabImportProgress::EPhase::Done)
		{
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetStringField(TEXT("asset_id"), AssetId);
			Obj->SetStringField(TEXT("status"), TEXT("already_imported"));
			Obj->SetStringField(TEXT("install_root"), Prev->InstallRoot);
			return OkJson(Obj);
		}
	}

	// Resolve the artifact version, then the signed BuildPatch download for it.
	const FString ArtifactId = A->ArtifactIdForEngine(EngineVer);
	if (ArtifactId.IsEmpty())
	{
		return ErrJson(TEXT("NO_ARTIFACT"), TEXT("No downloadable artifact was found for this asset/engine."));
	}
	const FString Token = FVibeFabAuth::GetAccessToken();
	FFabDownloadInfo Info;
	int32 HttpCode = 0;
	FString MErr;
	if (!FVibeFabManifest::Fetch(BaseUrl(), ArtifactId, A->AssetNamespace, A->AssetId, TEXT("Windows"),
	                             Token, 30.0, Info, HttpCode, MErr))
	{
		return ErrJson(TEXT("DOWNLOAD_INFO_FAILED"), MErr);
	}

	const bool bIsPlugin = A->DistributionMethod.ToLower().Contains(TEXT("plugin"));
	FString SErr;
	if (!FVibeFabImport::Start(AssetId, A->Title, bIsPlugin, Info, SErr))
	{
		return ErrJson(TEXT("IMPORT_START_FAILED"), SErr);
	}

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("asset_id"), AssetId);
	Obj->SetStringField(TEXT("title"), A->Title);
	Obj->SetStringField(TEXT("status"), TEXT("downloading"));
	Obj->SetStringField(TEXT("download_type"), bIsPlugin ? TEXT("buildpatch-plugin") : TEXT("buildpatch-pack"));
	Obj->SetStringField(TEXT("build_version"), Info.BuildVersion);
	Obj->SetStringField(TEXT("message"), TEXT("Download started — poll import_status(asset_id) until it reports imported or failed."));
	return OkJson(Obj);
}

FString UFabService::ImportFreeAsset(const FString& ListingId, const FString& Quality, const FString& Format,
	                                  const FString& LicenseSlug, bool AcceptEula)
{
	if (ListingId.IsEmpty())
	{
		return ErrJson(TEXT("BAD_ARGUMENT"), TEXT("ListingId is required."));
	}
	if (!AcceptEula)
	{
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetBoolField(TEXT("success"), false);
		Obj->SetStringField(TEXT("error_code"), TEXT("EULA_ACCEPTANCE_REQUIRED"));
		Obj->SetStringField(TEXT("error"), TEXT("Set AcceptEula=true only after reviewing and accepting the Fab EULA for this import."));
		Obj->SetStringField(TEXT("eula_url"), TEXT("https://www.fab.com/eula"));
		Obj->SetBoolField(TEXT("library_changed"), false);
		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
		return Out;
	}

	if (TSharedPtr<FFabImportProgress> Previous = FVibeFabImport::Get(ListingId))
	{
		if (Previous->Phase != FFabImportProgress::EPhase::Failed)
		{
			return ImportStatus(ListingId);
		}
	}

	FFabFreeDownload Download;
	FString Error;
	int32 HttpCode = 0;
	if (!FVibeFabCatalog::ResolveFreeDownload(BaseUrl(), ListingId, LicenseSlug, Quality, Format,
	                                           TEXT("Windows"), 30.0, Download, HttpCode, Error))
	{
		return ErrJson(TEXT("FREE_ASSET_RESOLUTION_FAILED"), Error);
	}
	const FString Destination = TEXT("/Game/Fab/Free/") + SafeAssetFolder(Download.Title, ListingId);
	if (!FVibeFabImport::StartDirect(ListingId, Download.Title, Download.DownloadUrl, Destination, Error))
	{
		return ErrJson(TEXT("IMPORT_START_FAILED"), Error);
	}

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("asset_id"), ListingId);
	Obj->SetStringField(TEXT("title"), Download.Title);
	Obj->SetStringField(TEXT("seller"), Download.Seller);
	Obj->SetStringField(TEXT("status"), TEXT("downloading"));
	Obj->SetStringField(TEXT("format"), Download.Format);
	Obj->SetStringField(TEXT("quality"), Download.Quality);
	Obj->SetStringField(TEXT("file_name"), Download.FileName);
	Obj->SetNumberField(TEXT("file_size"), static_cast<double>(Download.FileSize));
	Obj->SetStringField(TEXT("install_root"), Destination);
	Obj->SetBoolField(TEXT("direct_free_download"), true);
	Obj->SetBoolField(TEXT("library_changed"), false);
	Obj->SetStringField(TEXT("message"), TEXT("Download started - poll import_status(listing_id) until it reports imported or failed."));
	return OkJson(Obj);
}

FString UFabService::ImportStatus(const FString& AssetId)
{
	TSharedPtr<FFabImportProgress> P = FVibeFabImport::Get(AssetId);
	if (!P)
	{
		return ErrJson(TEXT("NO_IMPORT"), TEXT("No import has been started for this asset this session. Call import_asset first."));
	}

	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("asset_id"), AssetId);
	Obj->SetStringField(TEXT("title"), P->AssetName);
	Obj->SetNumberField(TEXT("percent"), P->Percent);
	Obj->SetNumberField(TEXT("completed_bytes"), (double)P->CompletedBytes);
	Obj->SetNumberField(TEXT("total_bytes"), (double)P->TotalBytes);
	Obj->SetBoolField(TEXT("cached"), P->bCached);

	switch (P->Phase)
	{
	case FFabImportProgress::EPhase::Done:
	{
		Obj->SetStringField(TEXT("status"), TEXT("imported"));
		Obj->SetStringField(TEXT("install_root"), P->InstallRoot);
		TArray<TSharedPtr<FJsonValue>> Paths;
		for (const FString& Pkg : P->AssetPaths) { Paths.Add(MakeShared<FJsonValueString>(Pkg)); }
		Obj->SetArrayField(TEXT("asset_paths"), Paths);
		Obj->SetNumberField(TEXT("asset_count"), P->AssetPaths.Num());
		break;
	}
	case FFabImportProgress::EPhase::Failed:
		Obj->SetStringField(TEXT("status"), TEXT("failed"));
		Obj->SetStringField(TEXT("error"), P->Error);
		break;
	case FFabImportProgress::EPhase::Importing:
		Obj->SetStringField(TEXT("status"), TEXT("importing"));
		break;
	default:
		Obj->SetStringField(TEXT("status"), TEXT("downloading"));
		break;
	}
	return OkJson(Obj);
}
