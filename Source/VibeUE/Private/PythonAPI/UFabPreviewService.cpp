// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UFabPreviewService.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Ticker.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageCore.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/ObjectThumbnail.inl"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "UObject/StrongObjectPtr.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UFabPreviewService)

DEFINE_LOG_CATEGORY_STATIC(LogVibeUEFabPreview, Log, All);

// File-local helpers carry a FabPreview prefix: the module builds in unity mode.
namespace FabPreview
{
	constexpr int32 MinCell = 64;
	constexpr int32 MaxCell = 512;
	constexpr int32 MaxSheetWidth = 2048;
	constexpr int32 Gap = 4;
	constexpr int32 MaxListingImages = 48;
	constexpr int32 MaxPackCells = 64;
	constexpr int32 MaxRefreshAssets = 256;
	constexpr int64 MaxImageBytes = 16ll * 1024 * 1024;
	constexpr double DownloadTimeoutSeconds = 30.0;
	constexpr int32 LowDetailEdge = 256; // long side under this = too small to judge an asset by

	const FColor SheetBackground(18, 18, 18, 255);
	const FColor CellBackground(42, 42, 42, 255);
	const FColor EmptyCell(70, 24, 24, 255);
	const FColor BadgeBackground(0, 0, 0, 255);
	const FColor BadgeLowDetail(150, 90, 0, 255);
	const FColor BadgeText(255, 255, 255, 255);

	// 3x5 digits, one row per byte (bit 2 = left column).
	const uint8 DigitRows[10][5] = {
		{7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
		{7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
	};

	int32 ClampCell(int32 CellSize)
	{
		return FMath::Clamp(CellSize <= 0 ? 256 : CellSize, MinCell, MaxCell);
	}

	int32 PickColumns(int32 Count, int32 CellSize, int32 Requested)
	{
		const int32 MaxColumns = FMath::Max(1, (MaxSheetWidth - Gap) / (CellSize + Gap));
		if (Requested > 0)
		{
			return FMath::Clamp(Requested, 1, FMath::Max(1, FMath::Min(Count, MaxColumns)));
		}
		return FMath::Clamp(FMath::CeilToInt(FMath::Sqrt(static_cast<double>(FMath::Max(1, Count)))), 1, MaxColumns);
	}

	void FillRect(FImage& Sheet, int32 X0, int32 Y0, int32 W, int32 H, const FColor& Color)
	{
		TArrayView64<FColor> Pixels = Sheet.AsBGRA8();
		const int32 X1 = FMath::Min(Sheet.SizeX, X0 + W);
		const int32 Y1 = FMath::Min(Sheet.SizeY, Y0 + H);
		for (int32 Y = FMath::Max(0, Y0); Y < Y1; ++Y)
		{
			for (int32 X = FMath::Max(0, X0); X < X1; ++X)
			{
				Pixels[static_cast<int64>(Y) * Sheet.SizeX + X] = Color;
			}
		}
	}

	/** Draws the cell number in a box at the cell's top-left corner. */
	void DrawBadge(FImage& Sheet, int32 CellX, int32 CellY, int32 CellSize, int32 Number, bool bLowDetail)
	{
		const FString Digits = FString::FromInt(Number);
		const int32 Scale = FMath::Max(2, CellSize / 80);
		const int32 Pad = Scale * 2;
		const int32 DigitW = 3 * Scale;
		const int32 Spacing = Scale;
		const int32 BoxW = Pad * 2 + Digits.Len() * DigitW + (Digits.Len() - 1) * Spacing;
		const int32 BoxH = Pad * 2 + 5 * Scale;
		FillRect(Sheet, CellX, CellY, BoxW, BoxH, bLowDetail ? BadgeLowDetail : BadgeBackground);
		for (int32 D = 0; D < Digits.Len(); ++D)
		{
			const int32 Digit = Digits[D] - TEXT('0');
			if (Digit < 0 || Digit > 9)
			{
				continue;
			}
			const int32 DX = CellX + Pad + D * (DigitW + Spacing);
			for (int32 Row = 0; Row < 5; ++Row)
			{
				for (int32 Col = 0; Col < 3; ++Col)
				{
					if (DigitRows[Digit][Row] & (4 >> Col))
					{
						FillRect(Sheet, DX + Col * Scale, CellY + Pad + Row * Scale, Scale, Scale, BadgeText);
					}
				}
			}
		}
	}

	/** Fits Tile (any format) into the cell, centred, keeping its aspect ratio (small pictures are scaled up). */
	void BlitFitted(FImage& Sheet, const FImageView& Tile, int32 CellX, int32 CellY, int32 CellSize)
	{
		if (Tile.SizeX <= 0 || Tile.SizeY <= 0)
		{
			return;
		}
		const double Scale = FMath::Min(static_cast<double>(CellSize) / Tile.SizeX, static_cast<double>(CellSize) / Tile.SizeY);
		const int32 W = FMath::Clamp(FMath::RoundToInt(Tile.SizeX * Scale), 1, CellSize);
		const int32 H = FMath::Clamp(FMath::RoundToInt(Tile.SizeY * Scale), 1, CellSize);
		FImage Fitted(W, H, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		FImageCore::ResizeImage(Tile, Fitted);
		const int32 OX = CellX + (CellSize - W) / 2;
		const int32 OY = CellY + (CellSize - H) / 2;
		TArrayView64<FColor> Dst = Sheet.AsBGRA8();
		TArrayView64<FColor> Src = Fitted.AsBGRA8();
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				FColor C = Src[static_cast<int64>(Y) * W + X];
				if (C.A < 255)
				{
					// Thumbnails can be translucent: composite over the cell background
					const float A = C.A / 255.f;
					C.R = static_cast<uint8>(C.R * A + CellBackground.R * (1.f - A));
					C.G = static_cast<uint8>(C.G * A + CellBackground.G * (1.f - A));
					C.B = static_cast<uint8>(C.B * A + CellBackground.B * (1.f - A));
					C.A = 255;
				}
				Dst[static_cast<int64>(OY + Y) * Sheet.SizeX + OX + X] = C;
			}
		}
	}

	/** Lays the tiles (null = empty cell) out on a grid, numbers the cells, encodes JPEG and optionally saves. */
	bool ComposeSheet(const TArray<const FImage*>& Tiles, const TArray<bool>& LowDetail, int32 CellSize, int32 Columns,
		const FString& SaveImagePath, FVibeUEPreviewSheet& InOut, FString& OutError)
	{
		const int32 Count = Tiles.Num();
		const int32 Rows = FMath::Max(1, FMath::DivideAndRoundUp(Count, Columns));
		const int32 SheetW = Columns * CellSize + (Columns + 1) * Gap;
		const int32 SheetH = Rows * CellSize + (Rows + 1) * Gap;
		FImage Sheet(SheetW, SheetH, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		FillRect(Sheet, 0, 0, SheetW, SheetH, SheetBackground);

		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 CellX = Gap + (Index % Columns) * (CellSize + Gap);
			const int32 CellY = Gap + (Index / Columns) * (CellSize + Gap);
			if (Tiles[Index])
			{
				FillRect(Sheet, CellX, CellY, CellSize, CellSize, CellBackground);
				BlitFitted(Sheet, *Tiles[Index], CellX, CellY, CellSize);
			}
			else
			{
				FillRect(Sheet, CellX, CellY, CellSize, CellSize, EmptyCell);
			}
			DrawBadge(Sheet, CellX, CellY, CellSize, Index + 1, LowDetail.IsValidIndex(Index) && LowDetail[Index]);
		}

		IImageWrapperModule& ImageWrapper = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TArray64<uint8> Jpeg;
		if (!ImageWrapper.CompressImage(Jpeg, EImageFormat::JPEG, Sheet, 85) || Jpeg.Num() == 0)
		{
			OutError = TEXT("Could not encode the sheet as JPEG.");
			return false;
		}
		InOut.Sheet.MimeType = TEXT("image/jpeg");
		InOut.Sheet.Data = FBase64::Encode(Jpeg.GetData(), static_cast<uint32>(Jpeg.Num()));
		InOut.Sheet.Label = TOptional<FString>(TEXT("contact sheet"));
		InOut.Columns = Columns;
		InOut.CellSize = CellSize;

		if (!SaveImagePath.IsEmpty())
		{
			const FString Full = FPaths::ConvertRelativePathToFull(SaveImagePath);
			TArray64<uint8> FileBytes;
			if (FPaths::GetExtension(Full).Equals(TEXT("png"), ESearchCase::IgnoreCase))
			{
				ImageWrapper.CompressImage(FileBytes, EImageFormat::PNG, Sheet);
			}
			else
			{
				FileBytes = Jpeg;
			}
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Full), true);
			if (FileBytes.Num() > 0 && FFileHelper::SaveArrayToFile(FileBytes, *Full))
			{
				InOut.SavedPath = Full;
			}
			else
			{
				OutError = FString::Printf(TEXT("The sheet was made but could not be written to %s."), *Full);
				// Not fatal: the image is still in the result
			}
		}
		return true;
	}

	/** Shared state of one ListingImageSheet call: the downloads land here; the last one (or the timeout) composes. */
	struct FListingFetch : public TSharedFromThis<FListingFetch>
	{
		TStrongObjectPtr<UVibeUEPreviewSheetResult> Result;
		TArray<FString> Urls;
		TArray<FString> Labels;
		TArray<TOptional<FImage>> Images;
		TArray<FVibeUEPreviewCell> Cells;
		TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> Requests;
		int32 Pending = 0;
		int32 CellSize = 256;
		int32 Columns = 0;
		FString SaveImagePath;
		bool bFinished = false;
		FTSTicker::FDelegateHandle TimeoutHandle;

		void OnDownloaded(int32 Index, FHttpResponsePtr Response, bool bConnected)
		{
			if (bFinished)
			{
				return;
			}
			const TSharedRef<FListingFetch> KeepAlive = AsShared(); // the caller's delegate may hold the last reference
			FVibeUEPreviewCell& Cell = Cells[Index];
			if (!bConnected || !Response.IsValid())
			{
				Cell.Error = TEXT("Connection failed.");
			}
			else if (Response->GetResponseCode() != 200)
			{
				Cell.Error = FString::Printf(TEXT("HTTP %d."), Response->GetResponseCode());
			}
			else
			{
				const TArray<uint8>& Bytes = Response->GetContent();
				Cell.SourceBytes = Bytes.Num();
				if (Bytes.Num() > MaxImageBytes)
				{
					Cell.Error = TEXT("Image over 16 MB, skipped.");
				}
				else
				{
					IImageWrapperModule& ImageWrapper = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
					FImage Decoded;
					if (Bytes.Num() > 0 && ImageWrapper.DecompressImage(Bytes.GetData(), Bytes.Num(), Decoded))
					{
						Cell.ImageOrigin = TEXT("fab");
						Cell.SourceWidth = Decoded.SizeX;
						Cell.SourceHeight = Decoded.SizeY;
						Cell.bLowDetail = FMath::Max(Decoded.SizeX, Decoded.SizeY) < LowDetailEdge;
						Images[Index] = MoveTemp(Decoded);
					}
					else
					{
						Cell.Error = FString::Printf(TEXT("Not a decodable image (%s, %d bytes)."),
							*Response->GetContentType(), Bytes.Num());
					}
				}
			}
			if (--Pending <= 0)
			{
				Finish(false);
			}
		}

		void Finish(bool bTimedOut)
		{
			if (bFinished)
			{
				return;
			}
			bFinished = true;
			const TSharedRef<FListingFetch> KeepAlive = AsShared();
			if (TimeoutHandle.IsValid())
			{
				FTSTicker::RemoveTicker(TimeoutHandle);
				TimeoutHandle.Reset();
			}
			if (bTimedOut)
			{
				// Only here, from the ticker: never unbind a delegate from inside its own call (the normal path)
				for (TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request : Requests)
				{
					if (Request.IsValid() && Request->GetStatus() == EHttpRequestStatus::Processing)
					{
						Request->OnProcessRequestComplete().Unbind();
						Request->CancelRequest();
					}
				}
			}
			Requests.Empty(); // breaks the fetch -> request -> delegate -> fetch cycle

			FVibeUEPreviewSheet Sheet;
			TArray<const FImage*> Tiles;
			TArray<bool> LowDetail;
			int32 Shown = 0, Low = 0;
			for (int32 Index = 0; Index < Cells.Num(); ++Index)
			{
				if (bTimedOut && Cells[Index].ImageOrigin.IsEmpty() && Cells[Index].Error.IsEmpty())
				{
					Cells[Index].Error = TEXT("Timed out.");
				}
				if (Cells[Index].ImageOrigin.IsEmpty())
				{
					Cells[Index].ImageOrigin = TEXT("none");
				}
				Tiles.Add(Images[Index].IsSet() ? &Images[Index].GetValue() : nullptr);
				LowDetail.Add(Cells[Index].bLowDetail);
				Shown += Images[Index].IsSet() ? 1 : 0;
				Low += Cells[Index].bLowDetail ? 1 : 0;
			}
			FString Error;
			const bool bComposed = ComposeSheet(Tiles, LowDetail, CellSize, Columns, SaveImagePath, Sheet, Error);
			Sheet.Cells = Cells;
			Sheet.bSuccess = bComposed && Shown > 0;
			Sheet.Summary = FString::Printf(TEXT("%d of %d pictures shown, %d columns, cells numbered in request order."),
				Shown, Cells.Num(), Sheet.Columns);
			if (Low > 0)
			{
				Sheet.Summary += FString::Printf(TEXT(" %d %s low-detail (under %d px, orange badge): too small to judge the asset by. "
					"Fab serves a blurred ~143x78 placeholder for a listing that is no longer live; an owned one still installs - "
					"install it and use PackSheet on its folder to see the real assets."), Low, Low == 1 ? TEXT("is") : TEXT("are"), LowDetailEdge);
			}
			if (!Error.IsEmpty())
			{
				Sheet.Summary += TEXT(" ") + Error;
			}
			UE_LOG(LogVibeUEFabPreview, Display, TEXT("ListingImageSheet: %s"), *Sheet.Summary);
			if (bComposed)
			{
				Result->SetValue(Sheet);
			}
			else
			{
				Result->SetError(Error);
			}
		}
	};

	FTopLevelAssetPath ResolveClassPath(const FString& Name)
	{
		const FString Trimmed = Name.TrimStartAndEnd();
		if (Trimmed.StartsWith(TEXT("/")))
		{
			return FTopLevelAssetPath(Trimmed);
		}
		if (const UClass* Class = FindFirstObject<UClass>(*Trimmed, EFindFirstObjectOptions::NativeFirst))
		{
			return Class->GetClassPathName();
		}
		return FTopLevelAssetPath();
	}

	/**
	 * True when a thumbnail holds no picture of the asset: no opaque pixel, a flat fill, or the static-mesh
	 * thumbnail scene with nothing in it (its dark checker floor). The editor saves that empty scene when it renders
	 * a thumbnail at save time while the mesh is still building: measured 2026-11-17 on 13 re-saved wreck meshes,
	 * 22 exact colours, 3 at 5 bits per channel, luminance spread 4.5 of 255. A real thumbnail, even of a thin
	 * object that covers a few percent of the cell on that same checker, had 320-5700 exact colours and 38-200 at
	 * 5 bits (spread 10-26). Counting colours, not the spread alone, is what tells a small object from no object.
	 */
	bool IsBlankPicture(const FImage& Image)
	{
		if (Image.SizeX <= 1 || Image.SizeY <= 1)
		{
			return true;
		}
		constexpr int32 MaxExactColours = 48;  // at or under: a fill or the empty checker, even PNG-stored
		constexpr int32 MaxCoarseColours = 8;  // with a small spread: the empty checker after JPEG noise
		FImage Bgra;
		Image.CopyTo(Bgra, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		const TArrayView64<FColor> Pixels = Bgra.AsBGRA8();
		int64 Opaque = 0;
		double Sum = 0.0, SumSq = 0.0;
		TSet<uint32> Exact;
		TSet<uint32> Coarse;
		for (const FColor& C : Pixels)
		{
			if (C.A < 16)
			{
				continue;
			}
			++Opaque;
			const double L = 0.299 * C.R + 0.587 * C.G + 0.114 * C.B;
			Sum += L;
			SumSq += L * L;
			if (Exact.Num() <= MaxExactColours)
			{
				Exact.Add((uint32(C.R) << 16) | (uint32(C.G) << 8) | uint32(C.B));
			}
			if (Coarse.Num() <= MaxCoarseColours)
			{
				Coarse.Add((uint32(C.R >> 3) << 10) | (uint32(C.G >> 3) << 5) | uint32(C.B >> 3)); // 5 bits per channel
			}
		}
		if (Opaque == 0)
		{
			return true;
		}
		const double Mean = Sum / Opaque;
		const double StdDev = FMath::Sqrt(FMath::Max(0.0, SumSq / Opaque - Mean * Mean));
		return Exact.Num() <= MaxExactColours || StdDev < 1.0 || (StdDev < 6.0 && Coarse.Num() <= MaxCoarseColours);
	}

	/** Renders a fresh thumbnail after finishing the shader and asset compiles the object's load queued. */
	bool RenderFreshThumbnail(UObject* Object, int32 Size, FObjectThumbnail& OutThumbnail)
	{
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
		FlushRenderingCommands();
		ThumbnailTools::RenderThumbnail(Object, Size, Size, ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush, nullptr, &OutThumbnail);
		return OutThumbnail.HasValidImageData() && OutThumbnail.GetImageWidth() > 1 && OutThumbnail.GetImageHeight() > 1;
	}

	/** The assets under Path (recursive) and those of ClassNames ("" or "*" = all), sorted by package path. */
	bool CollectAssets(const FString& ContentPath, const FString& ClassNames, FString& OutPath, TArray<FAssetData>& OutAll,
		TArray<FAssetData>& OutMatched, FString& OutError)
	{
		OutPath = ContentPath.TrimStartAndEnd();
		while (OutPath.Len() > 1 && OutPath.EndsWith(TEXT("/")))
		{
			OutPath.LeftChopInline(1);
		}
		if (!OutPath.StartsWith(TEXT("/")))
		{
			OutError = FString::Printf(TEXT("ContentPath must be a content folder such as /Game/MyPack; got '%s'."), *ContentPath);
			return false;
		}
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		Registry.GetAssetsByPath(FName(*OutPath), OutAll, /*bRecursive*/ true);
		if (OutAll.Num() == 0)
		{
			OutError = FString::Printf(TEXT("No assets under %s (wrong folder, or the install has not finished scanning)."), *OutPath);
			return false;
		}

		TSet<FTopLevelAssetPath> Wanted;
		bool bAllClasses = false;
		TArray<FString> Names;
		ClassNames.ParseIntoArray(Names, TEXT(","), true);
		for (const FString& Name : Names)
		{
			if (Name.TrimStartAndEnd() == TEXT("*"))
			{
				bAllClasses = true;
				continue;
			}
			const FTopLevelAssetPath ClassPath = ResolveClassPath(Name);
			if (ClassPath.IsNull())
			{
				OutError = FString::Printf(TEXT("Unknown asset class '%s' in ClassNames."), *Name.TrimStartAndEnd());
				return false;
			}
			Wanted.Add(ClassPath);
		}
		if (Names.Num() == 0)
		{
			bAllClasses = true;
		}
		for (const FAssetData& Asset : OutAll)
		{
			if (bAllClasses || Wanted.Contains(Asset.AssetClassPath))
			{
				OutMatched.Add(Asset);
			}
		}
		OutMatched.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
		return true;
	}
}

UVibeUEPreviewSheetResult* UFabPreviewService::ListingImageSheet(
	const TArray<FString>& ImageUrls,
	const TArray<FString>& Labels,
	int32 CellSize,
	int32 Columns,
	const FString& SaveImagePath)
{
	UVibeUEPreviewSheetResult* Result = NewObject<UVibeUEPreviewSheetResult>();
	if (ImageUrls.Num() == 0)
	{
		Result->SetError(TEXT("ImageUrls is empty: pass the thumbnailUrl of each listing to show."));
		return Result;
	}
	if (ImageUrls.Num() > FabPreview::MaxListingImages)
	{
		Result->SetError(FString::Printf(TEXT("%d URLs given; at most %d per sheet. Split the request."),
			ImageUrls.Num(), FabPreview::MaxListingImages));
		return Result;
	}
	for (const FString& Url : ImageUrls)
	{
		if (!Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
		{
			Result->SetError(FString::Printf(TEXT("Only https image URLs are fetched; got '%s'."), *Url));
			return Result;
		}
	}

	TSharedRef<FabPreview::FListingFetch> Fetch = MakeShared<FabPreview::FListingFetch>();
	Fetch->Result.Reset(Result);
	Fetch->Urls = ImageUrls;
	Fetch->Labels = Labels;
	Fetch->CellSize = FabPreview::ClampCell(CellSize);
	Fetch->Columns = FabPreview::PickColumns(ImageUrls.Num(), Fetch->CellSize, Columns);
	Fetch->SaveImagePath = SaveImagePath;
	Fetch->Images.SetNum(ImageUrls.Num());
	Fetch->Cells.SetNum(ImageUrls.Num());
	Fetch->Pending = ImageUrls.Num();
	FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper")); // load on the game thread

	for (int32 Index = 0; Index < ImageUrls.Num(); ++Index)
	{
		FVibeUEPreviewCell& Cell = Fetch->Cells[Index];
		Cell.Index = Index + 1;
		Cell.Source = ImageUrls[Index];
		Cell.Label = Labels.IsValidIndex(Index) ? Labels[Index] : FString();

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(ImageUrls[Index]);
		Request->SetVerb(TEXT("GET"));
		Request->SetHeader(TEXT("User-Agent"), TEXT("VibeUE/1.0 (Unreal Engine plugin)"));
		Request->SetHeader(TEXT("Accept"), TEXT("image/*"));
		Request->SetTimeout(FabPreview::DownloadTimeoutSeconds);
		Request->OnProcessRequestComplete().BindLambda(
			[Fetch, Index](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
			{
				Fetch->OnDownloaded(Index, Response, bConnected);
			});
		Fetch->Requests.Add(Request);
	}

	// A backstop in case a request never completes; it holds the fetch alive until it fires or is removed.
	Fetch->TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([Fetch](float) { Fetch->Finish(true); return false; }),
		static_cast<float>(FabPreview::DownloadTimeoutSeconds + 5.0));

	// Copy: a request that fails at once may complete inside ProcessRequest and empty the array via Finish
	const TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>> ToStart = Fetch->Requests;
	for (const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request : ToStart)
	{
		Request->ProcessRequest();
	}
	return Result;
}

FVibeUEPreviewSheet UFabPreviewService::PackSheet(
	const FString& ContentPath,
	const FString& ClassNames,
	int32 MaxAssets,
	int32 CellSize,
	int32 Columns,
	bool bRenderMissing,
	const FString& SaveImagePath)
{
	FVibeUEPreviewSheet Out;
	FString Path;
	TArray<FAssetData> All;
	TArray<FAssetData> Matched;
	FString CollectError;
	const bool bCollected = FabPreview::CollectAssets(ContentPath, ClassNames, Path, All, Matched, CollectError);
	Out.TotalAssets = All.Num();
	if (!bCollected)
	{
		Out.Summary = CollectError;
		return Out;
	}

	TMap<FString, int32> Counts;
	for (const FAssetData& Asset : All)
	{
		Counts.FindOrAdd(Asset.AssetClassPath.GetAssetName().ToString())++;
	}
	Counts.ValueSort([](int32 A, int32 B) { return A > B; });
	for (const TPair<FString, int32>& Pair : Counts)
	{
		FVibeUEPreviewClassCount& Count = Out.ClassCounts.AddDefaulted_GetRef();
		Count.AssetClass = Pair.Key;
		Count.Count = Pair.Value;
	}

	FString Folder;
	if (FPackageName::TryConvertLongPackageNameToFilename(Path, Folder))
	{
		int64 Bytes = 0;
		IFileManager::Get().IterateDirectoryStatRecursively(*Folder,
			[&Bytes](const TCHAR*, const FFileStatData& Stat)
			{
				if (!Stat.bIsDirectory)
				{
					Bytes += Stat.FileSize;
				}
				return true;
			});
		Out.BytesOnDisk = Bytes;
	}

	Out.MatchedAssets = Matched.Num();
	if (Matched.Num() == 0)
	{
		Out.bSuccess = true;
		Out.Summary = FString::Printf(TEXT("%d assets under %s, none of class %s."), All.Num(), *Path, *ClassNames);
		return Out;
	}

	const int32 Limit = FMath::Clamp(MaxAssets <= 0 ? 30 : MaxAssets, 1, FabPreview::MaxPackCells);
	if (Matched.Num() > Limit)
	{
		Matched.SetNum(Limit);
	}
	const int32 Cell = FabPreview::ClampCell(CellSize);
	const int32 Cols = FabPreview::PickColumns(Matched.Num(), Cell, Columns);

	TArray<TOptional<FImage>> Images;
	Images.SetNum(Matched.Num());
	int32 Cached = 0, Rendered = 0, Missing = 0, BlankSaved = 0;
	for (int32 Index = 0; Index < Matched.Num(); ++Index)
	{
		const FAssetData& Asset = Matched[Index];
		FVibeUEPreviewCell& CellInfo = Out.Cells.AddDefaulted_GetRef();
		CellInfo.Index = Index + 1;
		CellInfo.Label = Asset.AssetName.ToString();
		CellInfo.Source = Asset.GetObjectPathString();
		CellInfo.AssetClass = Asset.AssetClassPath.GetAssetName().ToString();

		// A saved thumbnail is used only when it holds a picture: the editor saves a blank (empty-scene checker)
		// thumbnail when it renders one at save time while the asset is still building.
		bool bSavedWasBlank = false;
		FObjectThumbnail Thumbnail;
		if (ThumbnailTools::LoadThumbnailFromPackage(Asset, Thumbnail) && Thumbnail.HasValidImageData() &&
			Thumbnail.GetImageWidth() > 1 && Thumbnail.GetImageHeight() > 1)
		{
			FImage Copy;
			Thumbnail.GetImage().CopyTo(Copy, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
			if (!FabPreview::IsBlankPicture(Copy))
			{
				CellInfo.ImageOrigin = TEXT("cached-thumbnail");
				CellInfo.SourceWidth = Copy.SizeX;
				CellInfo.SourceHeight = Copy.SizeY;
				Images[Index] = MoveTemp(Copy);
				++Cached;
				continue;
			}
			bSavedWasBlank = true;
			++BlankSaved;
		}
		if (!bRenderMissing)
		{
			CellInfo.ImageOrigin = TEXT("none");
			CellInfo.Error = bSavedWasBlank
				? TEXT("The saved thumbnail holds no picture (blank or near-uniform) and bRenderMissing is false. RefreshThumbnails re-renders and saves it.")
				: TEXT("No saved thumbnail (bRenderMissing is false).");
			++Missing;
			continue;
		}
		UObject* Object = Asset.GetAsset();
		if (!Object)
		{
			CellInfo.ImageOrigin = TEXT("none");
			CellInfo.Error = TEXT("Failed to load.");
			++Missing;
			continue;
		}
		// Render with real materials, not the default one: finish the compiles the load queued
		FObjectThumbnail Fresh;
		if (FabPreview::RenderFreshThumbnail(Object, Cell, Fresh))
		{
			FImage Copy;
			Fresh.GetImage().CopyTo(Copy, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
			CellInfo.ImageOrigin = TEXT("rendered");
			CellInfo.SourceWidth = Copy.SizeX;
			CellInfo.SourceHeight = Copy.SizeY;
			if (FabPreview::IsBlankPicture(Copy))
			{
				CellInfo.Error = TEXT("The rendered picture is blank or near-uniform: the asset may draw nothing in a thumbnail scene.");
			}
			else if (bSavedWasBlank)
			{
				CellInfo.Error = TEXT("The saved thumbnail was blank; this cell was rendered now. RefreshThumbnails re-renders and saves it.");
			}
			Images[Index] = MoveTemp(Copy);
			++Rendered;
		}
		else
		{
			CellInfo.ImageOrigin = TEXT("none");
			CellInfo.Error = TEXT("This asset type renders no thumbnail.");
			++Missing;
		}
	}

	TArray<const FImage*> Tiles;
	TArray<bool> LowDetail;
	for (TOptional<FImage>& Image : Images)
	{
		Tiles.Add(Image.IsSet() ? &Image.GetValue() : nullptr);
		LowDetail.Add(false);
	}
	FString Error;
	Out.bSuccess = FabPreview::ComposeSheet(Tiles, LowDetail, Cell, Cols, SaveImagePath, Out, Error);
	Out.Summary = FString::Printf(
		TEXT("%s: %d assets, %.1f MB on disk; %d match %s, %d shown (%d saved thumbnails, %d rendered, %d without a picture), cells numbered in path order."),
		*Path, All.Num(), Out.BytesOnDisk / (1024.0 * 1024.0), Out.MatchedAssets, *ClassNames, Matched.Num(), Cached, Rendered, Missing);
	if (BlankSaved > 0)
	{
		Out.Summary += FString::Printf(TEXT(" %d saved thumbnail%s held no picture (blank) and %s not used: run RefreshThumbnails on the folder to re-render and save %s."),
			BlankSaved, BlankSaved == 1 ? TEXT("") : TEXT("s"), BlankSaved == 1 ? TEXT("was") : TEXT("were"), BlankSaved == 1 ? TEXT("it") : TEXT("them"));
	}
	if (Out.MatchedAssets > Matched.Num())
	{
		Out.Summary += FString::Printf(TEXT(" Only the first %d shown: raise MaxAssets (max %d) or narrow ContentPath."), Matched.Num(), FabPreview::MaxPackCells);
	}
	if (!Error.IsEmpty())
	{
		Out.Summary += TEXT(" ") + Error;
	}
	UE_LOG(LogVibeUEFabPreview, Display, TEXT("PackSheet: %s"), *Out.Summary);
	return Out;
}

FVibeUEThumbnailRefresh UFabPreviewService::RefreshThumbnails(
	const FString& ContentPath,
	const FString& ClassNames,
	bool bOnlyBlank,
	bool bSave)
{
	FVibeUEThumbnailRefresh Out;
	FString Path;
	TArray<FAssetData> All;
	TArray<FAssetData> Matched;
	FString CollectError;
	if (!FabPreview::CollectAssets(ContentPath, ClassNames, Path, All, Matched, CollectError))
	{
		Out.Summary = CollectError;
		return Out;
	}
	if (Matched.Num() > FabPreview::MaxRefreshAssets)
	{
		Out.Summary = FString::Printf(TEXT("%d assets of %s under %s; at most %d per call. Narrow ContentPath or ClassNames."),
			Matched.Num(), *ClassNames, *Path, FabPreview::MaxRefreshAssets);
		return Out;
	}

	TArray<UPackage*> ToSave;
	for (const FAssetData& Asset : Matched)
	{
		++Out.Checked;
		const FString AssetPath = Asset.GetObjectPathString();
		if (bOnlyBlank)
		{
			FObjectThumbnail Saved;
			if (ThumbnailTools::LoadThumbnailFromPackage(Asset, Saved) && Saved.HasValidImageData() &&
				Saved.GetImageWidth() > 1 && Saved.GetImageHeight() > 1)
			{
				FImage Copy;
				Saved.GetImage().CopyTo(Copy, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
				if (!FabPreview::IsBlankPicture(Copy))
				{
					continue; // holds a picture already
				}
			}
		}
		UObject* Object = Asset.GetAsset();
		if (!Object)
		{
			Out.Failed.Add(AssetPath + TEXT(": failed to load"));
			continue;
		}
		FObjectThumbnail Fresh;
		if (!FabPreview::RenderFreshThumbnail(Object, ThumbnailTools::DefaultThumbnailSize, Fresh))
		{
			Out.Failed.Add(AssetPath + TEXT(": this asset type renders no thumbnail"));
			continue;
		}
		FImage Check;
		Fresh.GetImage().CopyTo(Check, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		if (FabPreview::IsBlankPicture(Check))
		{
			Out.Failed.Add(AssetPath + TEXT(": the render is blank too (the asset draws nothing in a thumbnail scene); left as it was"));
			continue;
		}
		UPackage* Package = Object->GetOutermost();
		// Replaces the package's in-memory thumbnail: the save keeps it (it is neither empty nor dirty) instead of
		// rendering another one at save time, which is how the blank ones were made.
		ThumbnailTools::CacheThumbnail(Object->GetFullName(), &Fresh, Package);
		Package->MarkPackageDirty();
		ToSave.AddUnique(Package);
		Out.Refreshed.Add(AssetPath);
	}

	bool bSaved = true;
	if (bSave && ToSave.Num() > 0)
	{
		bSaved = UEditorLoadingAndSavingUtils::SavePackages(ToSave, /*bOnlyDirty*/ false);
		if (bSaved)
		{
			for (const UPackage* Package : ToSave)
			{
				Out.SavedPackages.Add(Package->GetName());
			}
		}
	}
	Out.bSuccess = bSaved && Out.Failed.Num() == 0;
	Out.Summary = FString::Printf(TEXT("%s: %d asset%s of %s checked, %d thumbnail%s re-rendered%s, %d failed."),
		*Path, Out.Checked, Out.Checked == 1 ? TEXT("") : TEXT("s"), *ClassNames, Out.Refreshed.Num(),
		Out.Refreshed.Num() == 1 ? TEXT("") : TEXT("s"),
		Out.Refreshed.Num() == 0 ? TEXT("")
			: (!bSave ? TEXT(" (packages left dirty, not saved)") : (bSaved ? TEXT(" and saved") : TEXT(" but the save FAILED (see the log)"))),
		Out.Failed.Num());
	if (bOnlyBlank)
	{
		Out.Summary += TEXT(" Only blank saved thumbnails were replaced.");
	}
	UE_LOG(LogVibeUEFabPreview, Display, TEXT("RefreshThumbnails: %s"), *Out.Summary);
	return Out;
}

int32 UFabPreviewService::ClassifyImageFile(const FString& FilePath)
{
	TArray64<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *FilePath) || Bytes.Num() == 0)
	{
		return -1;
	}
	IImageWrapperModule& ImageWrapper = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	FImage Decoded;
	if (!ImageWrapper.DecompressImage(Bytes.GetData(), Bytes.Num(), Decoded))
	{
		return -1;
	}
	return FabPreview::IsBlankPicture(Decoded) ? 1 : 0;
}
