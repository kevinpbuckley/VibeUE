// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UMapBlockoutService.h"
#include "PythonAPI/ULandscapeService.h"
#include "MapBlockout/MapBlockoutMath.h"
#include "MapBlockout/MapBlockoutImage.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

// Stage 0 — landcover grid export, JSON writer, river centerline extraction.
// Stages 1-5 + renderers + orchestrators in UMapBlockoutService_Stages.cpp.
// Materializers in UMapBlockoutService_Materialize.cpp.

// =========================================================================
// Helpers
// =========================================================================

namespace
{
	/** Resolve a unique temp PNG path for a one-shot layer/height export. */
	FString MakeTempPNGPath(const FString& Tag)
	{
		const FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MapBlockout"), TEXT("_Temp"));
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Dir);
		const FString Name = FString::Printf(TEXT("%s_%s.png"), *Tag, *FGuid::NewGuid().ToString(EGuidFormats::Short));
		return FPaths::Combine(Dir, Name);
	}

	/** Decode a grayscale PNG and downsample to GridN x GridN normalized [0,1]. */
	bool DecodeAndDownsampleToGrid(const FString& PngPath, int32 GridN, TArray<float>& Out)
	{
		TArray<uint8> SrcU8;
		int32 SrcW = 0, SrcH = 0;
		if (!MapBlockoutImage::LoadGrayscalePNG(PngPath, SrcU8, SrcW, SrcH)) { return false; }
		if (SrcW <= 0 || SrcH <= 0) { return false; }

		TArray<float> SrcFloat; SrcFloat.SetNumUninitialized(SrcW * SrcH);
		const float Inv = 1.0f / 255.0f;
		for (int32 i = 0; i < SrcW * SrcH; ++i) { SrcFloat[i] = SrcU8[i] * Inv; }

		MapBlockoutMath::ResampleBilinear(SrcFloat, SrcW, SrcH, Out, GridN, GridN);
		return true;
	}
}


// =========================================================================
// Stage 0 — Landcover grid export
// =========================================================================

FMapBlockoutLandcoverGrid UMapBlockoutService::ExportLandcoverGrid(
	const FString& LandscapeLabel, int32 GridN)
{
	FMapBlockoutLandcoverGrid Result;
	Result.LandscapeLabel = LandscapeLabel;
	Result.GridN = FMath::Max(8, GridN);

	if (LandscapeLabel.IsEmpty())
	{
		Result.ErrorMessage = TEXT("ExportLandcoverGrid: empty LandscapeLabel.");
		return Result;
	}

	// Get landscape info (bounds + layer list).
	FLandscapeInfo_Custom Info;
	if (!ULandscapeService::GetLandscapeInfo(LandscapeLabel, Info))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Landscape '%s' not found."), *LandscapeLabel);
		return Result;
	}

	// World bounds: symmetric, origin-centred square half-span, matching the
	// host-Python export_terrain_data.py contract. Span derives from the
	// landscape's resolution * scale (cm/quad).
	const float HalfSpanX = (Info.ResolutionX > 0 ? (Info.ResolutionX - 1) : 0) * Info.Scale.X * 0.5f;
	const float HalfSpanY = (Info.ResolutionY > 0 ? (Info.ResolutionY - 1) : 0) * Info.Scale.Y * 0.5f;
	const float HalfSpan = FMath::Max(HalfSpanX, HalfSpanY);
	Result.WorldLo = -HalfSpan;
	Result.WorldHi = +HalfSpan;

	// Heightmap: export → decode → downsample.
	{
		const FString HmTemp = MakeTempPNGPath(TEXT("height"));
		if (ULandscapeService::ExportHeightmap(LandscapeLabel, HmTemp))
		{
			DecodeAndDownsampleToGrid(HmTemp, Result.GridN, Result.HeightNormalized);
			IFileManager::Get().Delete(*HmTemp, false, true);
		}
	}

	// Weight layers: one per paint layer reported in Info.
	for (const FLandscapeLayerInfo_Custom& Layer : Info.Layers)
	{
		FMapBlockoutLayerMap LayerMap;
		LayerMap.LayerName = Layer.LayerName;
		LayerMap.GridN = Result.GridN;

		const FString TempPath = MakeTempPNGPath(FString::Printf(TEXT("layer_%s"), *Layer.LayerName));
		const FWeightMapExportResult Exp = ULandscapeService::ExportWeightMap(LandscapeLabel, Layer.LayerName, TempPath);
		bool bOk = false;
		if (Exp.bSuccess && FPaths::FileExists(TempPath))
		{
			bOk = DecodeAndDownsampleToGrid(TempPath, Result.GridN, LayerMap.Weights);
		}
		IFileManager::Get().Delete(*TempPath, false, true);

		if (!bOk) { LayerMap.Weights.SetNumZeroed(Result.GridN * Result.GridN); }
		Result.Layers.Add(MoveTemp(LayerMap));
	}

	Result.bSuccess = (Result.GridN > 0 && Result.WorldHi > Result.WorldLo);
	if (!Result.bSuccess)
	{
		Result.ErrorMessage = TEXT("ExportLandcoverGrid: invalid bounds from landscape info.");
	}
	return Result;
}


// =========================================================================
// JSON I/O (compatible with landcover_grid.json from the host-Python reference)
// =========================================================================

FString UMapBlockoutService::WriteLandcoverGridJson(
	const FMapBlockoutLandcoverGrid& Grid, const FString& OutputFilePath)
{
	if (OutputFilePath.IsEmpty()) { return FString(); }

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("N"), Grid.GridN);
	Root->SetNumberField(TEXT("lo"), Grid.WorldLo);
	Root->SetNumberField(TEXT("hi"), Grid.WorldHi);
	Root->SetStringField(TEXT("landscape"), Grid.LandscapeLabel);

	// Reference format: each layer is a list of lists [N][N], row 0 = south.
	// Our in-memory storage is row 0 = south (set by the export helper that
	// reads PNG row 0 = top-of-image and lets caller flip if desired). For the
	// JSON round-trip we mirror the host-Python contract exactly.
	const int32 N = Grid.GridN;
	for (const FMapBlockoutLayerMap& Layer : Grid.Layers)
	{
		if (Layer.Weights.Num() < N * N) { continue; }
		TArray<TSharedPtr<FJsonValue>> Rows;
		Rows.Reserve(N);
		for (int32 Y = 0; Y < N; ++Y)
		{
			TArray<TSharedPtr<FJsonValue>> Cols;
			Cols.Reserve(N);
			for (int32 X = 0; X < N; ++X)
			{
				const float W = Layer.Weights[Y * N + X];
				Cols.Add(MakeShared<FJsonValueNumber>(FMath::RoundToFloat(W * 1000.0f) / 1000.0f));
			}
			Rows.Add(MakeShared<FJsonValueArray>(Cols));
		}
		Root->SetArrayField(Layer.LayerName, Rows);
	}

	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (!FJsonSerializer::Serialize(Root, Writer)) { return FString(); }

	const FString Dir = FPaths::GetPath(OutputFilePath);
	if (!Dir.IsEmpty())
	{
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Dir);
	}
	if (!FFileHelper::SaveStringToFile(Text, *OutputFilePath)) { return FString(); }
	return FPaths::ConvertRelativePathToFull(OutputFilePath);
}


// =========================================================================
// Read landcover_grid.json (for tests + alternative entry points)
// =========================================================================

FMapBlockoutLandcoverGrid UMapBlockoutService::LoadLandcoverGridJson(const FString& FilePath)
{
	FMapBlockoutLandcoverGrid Out;

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FilePath))
	{
		Out.ErrorMessage = FString::Printf(TEXT("LoadLandcoverGridJson: cannot read %s"), *FilePath);
		return Out;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Out.ErrorMessage = TEXT("LoadLandcoverGridJson: malformed JSON");
		return Out;
	}

	Out.GridN = static_cast<int32>(Root->GetNumberField(TEXT("N")));
	Out.WorldLo = static_cast<float>(Root->GetNumberField(TEXT("lo")));
	Out.WorldHi = static_cast<float>(Root->GetNumberField(TEXT("hi")));
	Root->TryGetStringField(TEXT("landscape"), Out.LandscapeLabel);

	if (Out.GridN <= 0 || Out.WorldHi <= Out.WorldLo)
	{
		Out.ErrorMessage = TEXT("LoadLandcoverGridJson: invalid N or bounds");
		return Out;
	}

	const int32 N = Out.GridN;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Root->Values)
	{
		if (Pair.Key == TEXT("N") || Pair.Key == TEXT("lo") || Pair.Key == TEXT("hi") || Pair.Key == TEXT("landscape")) { continue; }
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (!Pair.Value->TryGetArray(Rows) || Rows->Num() != N) { continue; }
		FMapBlockoutLayerMap LM;
		LM.LayerName = Pair.Key;
		LM.GridN = N;
		LM.Weights.SetNumZeroed(N * N);
		for (int32 Y = 0; Y < N; ++Y)
		{
			const TArray<TSharedPtr<FJsonValue>>* Cols = nullptr;
			if (!(*Rows)[Y]->TryGetArray(Cols) || Cols->Num() != N) { continue; }
			for (int32 X = 0; X < N; ++X)
			{
				LM.Weights[Y * N + X] = static_cast<float>((*Cols)[X]->AsNumber());
			}
		}
		Out.Layers.Add(MoveTemp(LM));
	}
	Out.bSuccess = true;
	return Out;
}


// =========================================================================
// River centerline extraction (skeletonize binary water -> simplified polylines)
// =========================================================================

namespace
{
	struct FCellPt { int32 X, Y; };

	void TracePolylinesFromSkeleton(
		const TArray<uint8>& Skel, int32 W, int32 H,
		TArray<TArray<FCellPt>>& OutLines)
	{
		TArray<uint8> Visited; Visited.SetNumZeroed(W * H);

		auto At = [&](int32 X, int32 Y) -> uint8 {
			if (X < 0 || Y < 0 || X >= W || Y >= H) { return 0; }
			return Skel[Y * W + X];
		};
		auto NeighborCount = [&](int32 X, int32 Y) -> int32 {
			int32 N = 0;
			for (int32 Dy = -1; Dy <= 1; ++Dy)
				for (int32 Dx = -1; Dx <= 1; ++Dx)
					if (!(Dx == 0 && Dy == 0) && At(X + Dx, Y + Dy)) { ++N; }
			return N;
		};

		// Endpoints (1 neighbor) first; remaining loops handled by general walk.
		auto Walk = [&](int32 X0, int32 Y0)
		{
			TArray<FCellPt> Line;
			int32 X = X0, Y = Y0;
			while (At(X, Y) && !Visited[Y * W + X])
			{
				Visited[Y * W + X] = 1;
				Line.Add({X, Y});
				bool bAdvanced = false;
				for (int32 Dy = -1; Dy <= 1 && !bAdvanced; ++Dy)
				for (int32 Dx = -1; Dx <= 1 && !bAdvanced; ++Dx)
				{
					if (Dx == 0 && Dy == 0) continue;
					const int32 Nx = X + Dx, Ny = Y + Dy;
					if (At(Nx, Ny) && !Visited[Ny * W + Nx]) { X = Nx; Y = Ny; bAdvanced = true; }
				}
				if (!bAdvanced) { break; }
			}
			if (Line.Num() >= 2) { OutLines.Add(MoveTemp(Line)); }
		};

		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				if (At(X, Y) && !Visited[Y * W + X] && NeighborCount(X, Y) == 1) { Walk(X, Y); }
			}
		}
		// Then handle interior cycles / unwalked components.
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				if (At(X, Y) && !Visited[Y * W + X]) { Walk(X, Y); }
			}
		}
	}

	/** Ramer-Douglas-Peucker simplification in cell coordinates. */
	void SimplifyRDP(const TArray<FCellPt>& In, float EpsCells, TArray<FCellPt>& Out)
	{
		if (In.Num() < 3) { Out = In; return; }

		TArray<bool> Keep; Keep.Init(false, In.Num());
		Keep[0] = true; Keep[In.Num() - 1] = true;

		TArray<TPair<int32, int32>> Stack;
		Stack.Add({0, In.Num() - 1});
		while (Stack.Num())
		{
			const TPair<int32, int32> P = Stack.Pop();
			const int32 A = P.Key, B = P.Value;
			float DMax = 0.0f; int32 IdxMax = -1;
			const float Ax = In[A].X, Ay = In[A].Y, Bx = In[B].X, By = In[B].Y;
			const float Lx = Bx - Ax, Ly = By - Ay;
			const float Len2 = Lx * Lx + Ly * Ly;
			for (int32 K = A + 1; K < B; ++K)
			{
				const float Px = In[K].X - Ax, Py = In[K].Y - Ay;
				const float Cross = Px * Ly - Py * Lx;
				const float Dist = Len2 > 0 ? FMath::Abs(Cross) / FMath::Sqrt(Len2) : 0.0f;
				if (Dist > DMax) { DMax = Dist; IdxMax = K; }
			}
			if (IdxMax >= 0 && DMax > EpsCells)
			{
				Keep[IdxMax] = true;
				Stack.Add({A, IdxMax});
				Stack.Add({IdxMax, B});
			}
		}
		Out.Reset();
		for (int32 i = 0; i < In.Num(); ++i) { if (Keep[i]) { Out.Add(In[i]); } }
	}
}

TArray<FMapBlockoutRiver> UMapBlockoutService::ExtractRiverCenterlines(
	const FMapBlockoutMask& WaterMask, float WorldLo, float WorldHi, float MinLengthCm)
{
	TArray<FMapBlockoutRiver> Out;
	if (WaterMask.Width <= 0 || WaterMask.Height <= 0 || WaterMask.Cells.Num() < WaterMask.Width * WaterMask.Height) { return Out; }
	if (WorldHi <= WorldLo) { return Out; }

	const int32 W = WaterMask.Width, H = WaterMask.Height;
	TArray<uint8> Mask = WaterMask.Cells;

	// Skeletonize a buffered mask. Closing first to seal small painted gaps.
	MapBlockoutMath::BinaryClosing(Mask, W, H, 1);
	MapBlockoutMath::Skeletonize(Mask, W, H);

	TArray<TArray<FCellPt>> Lines;
	TracePolylinesFromSkeleton(Mask, W, H, Lines);

	const float Span = WorldHi - WorldLo;
	const float CellSpanCm = Span / FMath::Max(1, W - 1);
	const float MinLengthCells = MinLengthCm / FMath::Max(1.0f, CellSpanCm);

	auto CellLen = [](const TArray<FCellPt>& L) -> float {
		float D = 0.0f;
		for (int32 i = 0; i + 1 < L.Num(); ++i)
		{
			const float Dx = L[i + 1].X - L[i].X, Dy = L[i + 1].Y - L[i].Y;
			D += FMath::Sqrt(Dx * Dx + Dy * Dy);
		}
		return D;
	};

	int32 Index = 0;
	for (const TArray<FCellPt>& L : Lines)
	{
		if (CellLen(L) < MinLengthCells) { continue; }
		TArray<FCellPt> Simplified;
		SimplifyRDP(L, /*EpsCells=*/1.5f, Simplified);
		if (Simplified.Num() < 2) { continue; }

		FMapBlockoutRiver River;
		River.Name = FString::Printf(TEXT("River_%d"), ++Index);
		River.Points.Reserve(Simplified.Num());
		for (const FCellPt& P : Simplified)
		{
			FMapBlockoutRiverPoint RP;
			// Cell (col, row) -> world (X east, Y north). Row 0 = north.
			const float Wx = WorldLo + (Span * P.X) / FMath::Max(1, W - 1);
			const float Wy = WorldHi - (Span * P.Y) / FMath::Max(1, H - 1);
			RP.World = FVector2D(Wx, Wy);
			RP.WidthM = 30.0f;
			River.Points.Add(RP);
		}
		Out.Add(River);
	}
	return Out;
}
