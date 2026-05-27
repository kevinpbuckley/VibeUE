// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "MapBlockout/MapBlockoutImage.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

// Stubs awaiting port. PNG encode/decode and color constants are real (we know
// the chart and IImageWrapper is already wired up). Drawing primitives are empty
// — the contributor fills them when porting the matplotlib equivalents.

namespace MapBlockoutImage
{
	namespace Colors
	{
		const FColor MainRoad    = FColor(245, 245, 245);
		const FColor DirtRoad    = FColor(150, 108,  66);
		const FColor Treeline    = FColor(182, 138, 228);
		const FColor Forest      = FColor( 86,  58, 132);
		const FColor Building    = FColor(206, 210, 216);
		const FColor Bridge      = FColor(247, 140,  28);
		const FColor Railway     = FColor( 66,  68,  74);
		const FColor POIBoundary = FColor( 46, 226,  86);
		const FColor Field       = FColor(226, 206,  64);
		const FColor Scrub       = FColor(122, 184, 230);
		const FColor River       = FColor( 48,  92, 138);
		const FColor Background  = FColor( 24,  26,  30);
		const FColor Panel       = FColor( 11,  12,  15);
		const FColor Grid        = FColor( 54,  58,  66);
		const FColor Ink         = FColor(232, 234, 238);
	}

	void NewBitmap(TArray<FColor>& OutBitmap, int32 Width, int32 Height, FColor Fill)
	{
		OutBitmap.SetNumUninitialized(Width * Height);
		for (FColor& C : OutBitmap) { C = Fill; }
	}

	void DrawLine(TArray<FColor>&, int32, int32, int32, int32, int32, int32, FColor, int32) {}
	void DrawPolyline(TArray<FColor>&, int32, int32, const TArray<FIntPoint>&, FColor, int32) {}
	void DrawDisk(TArray<FColor>&, int32, int32, int32, int32, int32, FColor) {}
	void DrawCircleOutline(TArray<FColor>&, int32, int32, int32, int32, int32, FColor, int32) {}
	void DrawRect(TArray<FColor>&, int32, int32, int32, int32, int32, int32, FColor) {}
	void OverlayMask(TArray<FColor>&, int32, int32, const TArray<uint8>&, FColor) {}
	void RenderHeatmap(const TArray<float>&, int32, int32, TArray<FColor>&) {}
	void DrawColorKey(TArray<FColor>&, int32, int32, int32, int32, int32, int32) {}

	bool WritePNG(const TArray<FColor>& Bitmap, int32 W, int32 H, const FString& OutputPath)
	{
		if (Bitmap.Num() != W * H || W <= 0 || H <= 0) { return false; }

		IImageWrapperModule& ImageModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		const TSharedPtr<IImageWrapper> Wrapper = ImageModule.CreateImageWrapper(EImageFormat::PNG);
		if (!Wrapper.IsValid()) { return false; }

		if (!Wrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FColor), W, H, ERGBFormat::BGRA, 8))
		{
			return false;
		}
		const TArray64<uint8>& Compressed = Wrapper->GetCompressed(100);

		const FString Dir = FPaths::GetPath(OutputPath);
		if (!Dir.IsEmpty())
		{
			FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Dir);
		}
		return FFileHelper::SaveArrayToFile(Compressed, *OutputPath);
	}

	bool LoadGrayscalePNG(const FString& FilePath, TArray<uint8>& OutPixels, int32& OutW, int32& OutH)
	{
		OutW = OutH = 0;
		OutPixels.Reset();

		TArray<uint8> Raw;
		if (!FFileHelper::LoadFileToArray(Raw, *FilePath)) { return false; }

		IImageWrapperModule& ImageModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		const TSharedPtr<IImageWrapper> Wrapper = ImageModule.CreateImageWrapper(EImageFormat::PNG);
		if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Raw.GetData(), Raw.Num())) { return false; }

		TArray<uint8> Decoded;
		if (!Wrapper->GetRaw(ERGBFormat::Gray, 8, Decoded)) { return false; }
		OutW = Wrapper->GetWidth();
		OutH = Wrapper->GetHeight();
		OutPixels = MoveTemp(Decoded);
		return true;
	}
}
