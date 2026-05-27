// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "MapBlockout/MapBlockoutImage.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

namespace MapBlockoutImage
{
	// ---------------------------------------------------------------------
	// Colors (authoritative MapDesigner chart)
	// ---------------------------------------------------------------------

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

	// ---------------------------------------------------------------------
	// Bitmap helpers
	// ---------------------------------------------------------------------

	void NewBitmap(TArray<FColor>& OutBitmap, int32 Width, int32 Height, FColor Fill)
	{
		OutBitmap.SetNumUninitialized(Width * Height);
		for (FColor& C : OutBitmap) { C = Fill; }
	}

	static FORCEINLINE void SetPixel(TArray<FColor>& B, int32 W, int32 H,
		int32 X, int32 Y, FColor C)
	{
		if (X >= 0 && Y >= 0 && X < W && Y < H) { B[Y * W + X] = C; }
	}

	static FORCEINLINE void PlotPixelThick(TArray<FColor>& B, int32 W, int32 H,
		int32 X, int32 Y, int32 R, FColor C)
	{
		if (R <= 0) { SetPixel(B, W, H, X, Y, C); return; }
		const int32 X0 = FMath::Max(0, X - R);
		const int32 X1 = FMath::Min(W - 1, X + R);
		const int32 Y0 = FMath::Max(0, Y - R);
		const int32 Y1 = FMath::Min(H - 1, Y + R);
		const int32 R2 = R * R;
		for (int32 Yi = Y0; Yi <= Y1; ++Yi)
		{
			for (int32 Xi = X0; Xi <= X1; ++Xi)
			{
				const int32 Dx = Xi - X, Dy = Yi - Y;
				if (Dx * Dx + Dy * Dy <= R2) { B[Yi * W + Xi] = C; }
			}
		}
	}

	// ---------------------------------------------------------------------
	// Primitives
	// ---------------------------------------------------------------------

	void DrawLine(TArray<FColor>& Bitmap, int32 W, int32 H,
		int32 X0, int32 Y0, int32 X1, int32 Y1, FColor Color, int32 ThicknessRadius)
	{
		if (W <= 0 || H <= 0 || Bitmap.Num() < W * H) { return; }
		int32 Dx = FMath::Abs(X1 - X0), Sx = (X0 < X1) ? 1 : -1;
		int32 Dy = -FMath::Abs(Y1 - Y0), Sy = (Y0 < Y1) ? 1 : -1;
		int32 Err = Dx + Dy;
		int32 X = X0, Y = Y0;
		while (true)
		{
			PlotPixelThick(Bitmap, W, H, X, Y, ThicknessRadius, Color);
			if (X == X1 && Y == Y1) { break; }
			const int32 E2 = 2 * Err;
			if (E2 >= Dy) { Err += Dy; X += Sx; }
			if (E2 <= Dx) { Err += Dx; Y += Sy; }
		}
	}

	void DrawPolyline(TArray<FColor>& Bitmap, int32 W, int32 H,
		const TArray<FIntPoint>& Points, FColor Color, int32 ThicknessRadius)
	{
		for (int32 I = 0; I + 1 < Points.Num(); ++I)
		{
			DrawLine(Bitmap, W, H, Points[I].X, Points[I].Y,
				Points[I + 1].X, Points[I + 1].Y, Color, ThicknessRadius);
		}
	}

	void DrawDisk(TArray<FColor>& Bitmap, int32 W, int32 H,
		int32 Cx, int32 Cy, int32 Radius, FColor Color)
	{
		PlotPixelThick(Bitmap, W, H, Cx, Cy, Radius, Color);
	}

	void DrawCircleOutline(TArray<FColor>& Bitmap, int32 W, int32 H,
		int32 Cx, int32 Cy, int32 Radius, FColor Color, int32 Thickness)
	{
		if (Radius <= 0 || W <= 0 || H <= 0 || Bitmap.Num() < W * H) { return; }
		const int32 ROuter2 = (Radius + Thickness / 2) * (Radius + Thickness / 2);
		const int32 RInner2 = FMath::Max(0, Radius - Thickness / 2) * FMath::Max(0, Radius - Thickness / 2);
		const int32 X0 = FMath::Max(0, Cx - Radius - Thickness);
		const int32 X1 = FMath::Min(W - 1, Cx + Radius + Thickness);
		const int32 Y0 = FMath::Max(0, Cy - Radius - Thickness);
		const int32 Y1 = FMath::Min(H - 1, Cy + Radius + Thickness);
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			for (int32 X = X0; X <= X1; ++X)
			{
				const int32 D2 = (X - Cx) * (X - Cx) + (Y - Cy) * (Y - Cy);
				if (D2 <= ROuter2 && D2 >= RInner2) { Bitmap[Y * W + X] = Color; }
			}
		}
	}

	void DrawRect(TArray<FColor>& Bitmap, int32 W, int32 H,
		int32 X0, int32 Y0, int32 X1, int32 Y1, FColor Color)
	{
		if (W <= 0 || H <= 0 || Bitmap.Num() < W * H) { return; }
		const int32 Mx0 = FMath::Max(0, FMath::Min(X0, X1));
		const int32 Mx1 = FMath::Min(W - 1, FMath::Max(X0, X1));
		const int32 My0 = FMath::Max(0, FMath::Min(Y0, Y1));
		const int32 My1 = FMath::Min(H - 1, FMath::Max(Y0, Y1));
		for (int32 Y = My0; Y <= My1; ++Y)
		{
			for (int32 X = Mx0; X <= Mx1; ++X) { Bitmap[Y * W + X] = Color; }
		}
	}

	void OverlayMask(TArray<FColor>& Bitmap, int32 W, int32 H,
		const TArray<uint8>& Mask, FColor Color)
	{
		if (W <= 0 || H <= 0) { return; }
		const int32 N = FMath::Min(Bitmap.Num(), Mask.Num());
		for (int32 i = 0; i < N; ++i)
		{
			if (Mask[i]) { Bitmap[i] = Color; }
		}
	}

	// ---------------------------------------------------------------------
	// Heatmap
	// ---------------------------------------------------------------------

	void RenderHeatmap(const TArray<float>& Density, int32 W, int32 H, TArray<FColor>& OutBitmap)
	{
		NewBitmap(OutBitmap, W, H, Colors::Background);
		if (Density.Num() < W * H || W <= 0 || H <= 0) { return; }

		float MaxV = 0.0f;
		for (float V : Density) { MaxV = FMath::Max(MaxV, V); }
		if (MaxV <= 0.0f) { return; }

		for (int32 i = 0; i < W * H; ++i)
		{
			const float T = FMath::Clamp(Density[i] / MaxV, 0.0f, 1.0f);
			const uint8 G = static_cast<uint8>(FMath::Clamp(T * 255.0f, 0.0f, 255.0f));
			OutBitmap[i] = FColor(G, G, G);
		}
	}

	// ---------------------------------------------------------------------
	// 5x7 ASCII bitmap font
	//
	// Each glyph is 7 rows (top to bottom), each row's low 5 bits encode 5 pixels
	// (MSB-of-the-5 = leftmost). Indexed by `char - ' '`. Missing chars render
	// as an empty block.
	// ---------------------------------------------------------------------

	static const uint8 Font5x7[][7] = {
		{0,0,0,0,0,0,0},                          // ' '  (space)
		{0x04,0x04,0x04,0x04,0x00,0x00,0x04},     // !
		{0x0A,0x0A,0x0A,0x00,0x00,0x00,0x00},     // "
		{0x0A,0x1F,0x0A,0x1F,0x0A,0x00,0x00},     // #
		{0x04,0x1E,0x05,0x0E,0x14,0x0F,0x04},     // $
		{0x19,0x19,0x02,0x04,0x08,0x13,0x13},     // %
		{0x06,0x09,0x09,0x06,0x15,0x09,0x16},     // &
		{0x04,0x04,0x08,0x00,0x00,0x00,0x00},     // '
		{0x02,0x04,0x08,0x08,0x08,0x04,0x02},     // (
		{0x08,0x04,0x02,0x02,0x02,0x04,0x08},     // )
		{0x00,0x0A,0x04,0x1F,0x04,0x0A,0x00},     // *
		{0x00,0x04,0x04,0x1F,0x04,0x04,0x00},     // +
		{0x00,0x00,0x00,0x00,0x00,0x04,0x08},     // ,
		{0x00,0x00,0x00,0x1F,0x00,0x00,0x00},     // -
		{0x00,0x00,0x00,0x00,0x00,0x00,0x04},     // .
		{0x01,0x01,0x02,0x04,0x08,0x10,0x10},     // /
		{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},     // 0
		{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},     // 1
		{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},     // 2
		{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},     // 3
		{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},     // 4
		{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},     // 5
		{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},     // 6
		{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},     // 7
		{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},     // 8
		{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},     // 9
		{0x00,0x04,0x00,0x00,0x00,0x04,0x00},     // :
		{0x00,0x04,0x00,0x00,0x00,0x04,0x08},     // ;
		{0x02,0x04,0x08,0x10,0x08,0x04,0x02},     // <
		{0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},     // =
		{0x08,0x04,0x02,0x01,0x02,0x04,0x08},     // >
		{0x0E,0x11,0x01,0x02,0x04,0x00,0x04},     // ?
		{0x0E,0x11,0x17,0x15,0x17,0x10,0x0E},     // @
		{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},     // A
		{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},     // B
		{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},     // C
		{0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},     // D
		{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},     // E
		{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},     // F
		{0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},     // G
		{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},     // H
		{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},     // I
		{0x07,0x02,0x02,0x02,0x02,0x12,0x0C},     // J
		{0x11,0x12,0x14,0x18,0x14,0x12,0x11},     // K
		{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},     // L
		{0x11,0x1B,0x15,0x15,0x11,0x11,0x11},     // M
		{0x11,0x11,0x19,0x15,0x13,0x11,0x11},     // N
		{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},     // O
		{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},     // P
		{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},     // Q
		{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},     // R
		{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},     // S
		{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},     // T
		{0x11,0x11,0x11,0x11,0x11,0x11,0x0E},     // U
		{0x11,0x11,0x11,0x11,0x11,0x0A,0x04},     // V
		{0x11,0x11,0x11,0x15,0x15,0x15,0x0A},     // W
		{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},     // X
		{0x11,0x11,0x11,0x0A,0x04,0x04,0x04},     // Y
		{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},     // Z
		{0x0E,0x08,0x08,0x08,0x08,0x08,0x0E},     // [
		{0x10,0x10,0x08,0x04,0x02,0x01,0x01},     // backslash
		{0x0E,0x02,0x02,0x02,0x02,0x02,0x0E},     // ]
		{0x04,0x0A,0x11,0x00,0x00,0x00,0x00},     // ^
		{0x00,0x00,0x00,0x00,0x00,0x00,0x1F},     // _
		{0x08,0x04,0x02,0x00,0x00,0x00,0x00},     // `
		{0x00,0x00,0x0E,0x01,0x0F,0x11,0x0F},     // a
		{0x10,0x10,0x16,0x19,0x11,0x19,0x16},     // b
		{0x00,0x00,0x0E,0x11,0x10,0x11,0x0E},     // c
		{0x01,0x01,0x0D,0x13,0x11,0x13,0x0D},     // d
		{0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E},     // e
		{0x06,0x09,0x08,0x1C,0x08,0x08,0x08},     // f
		{0x00,0x00,0x0F,0x11,0x0F,0x01,0x0E},     // g
		{0x10,0x10,0x16,0x19,0x11,0x11,0x11},     // h
		{0x04,0x00,0x0C,0x04,0x04,0x04,0x0E},     // i
		{0x02,0x00,0x06,0x02,0x02,0x12,0x0C},     // j
		{0x10,0x10,0x12,0x14,0x18,0x14,0x12},     // k
		{0x0C,0x04,0x04,0x04,0x04,0x04,0x0E},     // l
		{0x00,0x00,0x1A,0x15,0x15,0x11,0x11},     // m
		{0x00,0x00,0x16,0x19,0x11,0x11,0x11},     // n
		{0x00,0x00,0x0E,0x11,0x11,0x11,0x0E},     // o
		{0x00,0x00,0x16,0x19,0x16,0x10,0x10},     // p
		{0x00,0x00,0x0D,0x13,0x0D,0x01,0x01},     // q
		{0x00,0x00,0x16,0x19,0x10,0x10,0x10},     // r
		{0x00,0x00,0x0F,0x10,0x0E,0x01,0x1E},     // s
		{0x08,0x08,0x1C,0x08,0x08,0x09,0x06},     // t
		{0x00,0x00,0x11,0x11,0x11,0x13,0x0D},     // u
		{0x00,0x00,0x11,0x11,0x11,0x0A,0x04},     // v
		{0x00,0x00,0x11,0x11,0x15,0x15,0x0A},     // w
		{0x00,0x00,0x11,0x0A,0x04,0x0A,0x11},     // x
		{0x00,0x00,0x11,0x11,0x0F,0x01,0x0E},     // y
		{0x00,0x00,0x1F,0x02,0x04,0x08,0x1F},     // z
		{0x02,0x04,0x04,0x08,0x04,0x04,0x02},     // {
		{0x04,0x04,0x04,0x04,0x04,0x04,0x04},     // |
		{0x08,0x04,0x04,0x02,0x04,0x04,0x08},     // }
		{0x00,0x00,0x08,0x15,0x02,0x00,0x00},     // ~
	};
	static constexpr int32 Font5x7Count = sizeof(Font5x7) / sizeof(Font5x7[0]);

	void DrawText5x7(TArray<FColor>& Bitmap, int32 W, int32 H,
		int32 X, int32 Y, const FString& Text, FColor Color, int32 Scale)
	{
		Scale = FMath::Max(1, Scale);
		const int32 GlyphW = 5 * Scale;
		const int32 GlyphH = 7 * Scale;
		const int32 Advance = (5 + 1) * Scale;

		int32 PenX = X;
		for (int32 I = 0; I < Text.Len(); ++I)
		{
			const TCHAR Ch = Text[I];
			const int32 Idx = static_cast<int32>(Ch) - 32;
			if (Idx < 0 || Idx >= Font5x7Count) { PenX += Advance; continue; }
			const uint8* Glyph = Font5x7[Idx];
			for (int32 Gy = 0; Gy < 7; ++Gy)
			{
				const uint8 Row = Glyph[Gy];
				for (int32 Gx = 0; Gx < 5; ++Gx)
				{
					if (Row & (1u << (4 - Gx)))
					{
						const int32 Bx = PenX + Gx * Scale;
						const int32 By = Y + Gy * Scale;
						for (int32 Sy = 0; Sy < Scale; ++Sy)
						{
							for (int32 Sx = 0; Sx < Scale; ++Sx)
							{
								SetPixel(Bitmap, W, H, Bx + Sx, By + Sy, Color);
							}
						}
					}
				}
			}
			PenX += Advance;
		}
	}

	// ---------------------------------------------------------------------
	// Color Key
	// ---------------------------------------------------------------------

	int32 DrawColorKey(TArray<FColor>& Bitmap, int32 W, int32 H,
		int32 PanelLeft, int32 PanelTop, int32 PanelWidth,
		const FString& Title, const TArray<FKeyEntry>& Entries)
	{
		const int32 RowH = 34;
		const int32 PanelH = 50 + Entries.Num() * RowH + 10;
		const int32 PanelRight = PanelLeft + PanelWidth;
		const int32 PanelBottom = PanelTop + PanelH;

		// Background + outline
		DrawRect(Bitmap, W, H, PanelLeft, PanelTop, PanelRight, PanelBottom, Colors::Panel);
		// Outline (manual — 4 lines)
		DrawLine(Bitmap, W, H, PanelLeft,  PanelTop,    PanelRight, PanelTop,    FColor(120, 126, 136), 1);
		DrawLine(Bitmap, W, H, PanelLeft,  PanelBottom, PanelRight, PanelBottom, FColor(120, 126, 136), 1);
		DrawLine(Bitmap, W, H, PanelLeft,  PanelTop,    PanelLeft,  PanelBottom, FColor(120, 126, 136), 1);
		DrawLine(Bitmap, W, H, PanelRight, PanelTop,    PanelRight, PanelBottom, FColor(120, 126, 136), 1);

		// Title
		DrawText5x7(Bitmap, W, H, PanelLeft + 14, PanelTop + 12, Title, Colors::Ink, 2);

		// Entries
		int32 Y = PanelTop + 50;
		for (const FKeyEntry& E : Entries)
		{
			if (E.Style == TEXT("line"))
			{
				DrawLine(Bitmap, W, H, PanelLeft + 14, Y + 12, PanelLeft + 48, Y + 12, E.Color, 2);
			}
			else if (E.Style == TEXT("ring"))
			{
				DrawCircleOutline(Bitmap, W, H, PanelLeft + 30, Y + 12, 12, E.Color, 3);
			}
			else
			{
				DrawRect(Bitmap, W, H, PanelLeft + 14, Y, PanelLeft + 46, Y + 24, E.Color);
			}
			DrawText5x7(Bitmap, W, H, PanelLeft + 60, Y + 5, E.Label, FColor(228, 230, 234), 2);
			Y += RowH;
		}
		return PanelLeft;
	}

	// ---------------------------------------------------------------------
	// PNG I/O
	// ---------------------------------------------------------------------

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
