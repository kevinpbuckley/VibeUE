// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "MapBlockout/MapBlockoutMath.h"

// All implementations are stubs awaiting the C++ port of the host-Python
// reference at Source/VibeUE/Tests/MapBlockout/reference/. Signatures are
// locked; contributor fills in the bodies. Empty bodies are safe — every
// caller in UMapBlockoutService currently returns "not implemented".

namespace MapBlockoutMath
{
	void ResampleBilinear(const TArray<float>& Src, int32 SrcW, int32 SrcH,
		TArray<float>& OutDst, int32 DstW, int32 DstH)
	{
		OutDst.SetNumZeroed(DstW * DstH);
	}

	void GaussianBlur(TArray<float>& InOut, int32 W, int32 H, float Sigma) {}

	void FlipVertical(TArray<float>& InOut, int32 W, int32 H)
	{
		if (W <= 0 || H <= 0 || InOut.Num() < W * H) return;
		for (int32 Y = 0; Y < H / 2; ++Y)
		{
			const int32 YOther = H - 1 - Y;
			for (int32 X = 0; X < W; ++X)
			{
				Swap(InOut[Y * W + X], InOut[YOther * W + X]);
			}
		}
	}

	void FlipVerticalU8(TArray<uint8>& InOut, int32 W, int32 H)
	{
		if (W <= 0 || H <= 0 || InOut.Num() < W * H) return;
		for (int32 Y = 0; Y < H / 2; ++Y)
		{
			const int32 YOther = H - 1 - Y;
			for (int32 X = 0; X < W; ++X)
			{
				Swap(InOut[Y * W + X], InOut[YOther * W + X]);
			}
		}
	}

	void Dilate(TArray<uint8>& InOut, int32 W, int32 H, int32 Radius) {}
	void Erode(TArray<uint8>& InOut, int32 W, int32 H, int32 Radius) {}

	void DistanceTransformEDT(const TArray<uint8>& Mask, int32 W, int32 H,
		TArray<float>& OutDistance)
	{
		OutDistance.SetNumZeroed(W * H);
	}

	int32 LabelConnectedComponents(const TArray<uint8>& Mask, int32 W, int32 H,
		TArray<int32>& OutLabels)
	{
		OutLabels.SetNumZeroed(W * H);
		return 0;
	}

	void Skeletonize(TArray<uint8>& InOut, int32 W, int32 H) {}

	void RasterizeLine(TArray<uint8>& Mask, int32 W, int32 H,
		int32 X0, int32 Y0, int32 X1, int32 Y1,
		int32 ThicknessRadius, uint8 Value)
	{
	}

	void RasterizeDisk(TArray<uint8>& Mask, int32 W, int32 H,
		int32 Cx, int32 Cy, int32 Radius, uint8 Value)
	{
	}

	void RasterizePolyline(TArray<uint8>& Mask, int32 W, int32 H,
		const TArray<FIntPoint>& Points, int32 ThicknessRadius, uint8 Value)
	{
	}

	void RasterizePolygonFilled(TArray<uint8>& Mask, int32 W, int32 H,
		const TArray<FIntPoint>& Polygon, uint8 Value)
	{
	}

	int32 CountNonZero(const TArray<uint8>& Mask)
	{
		int32 N = 0;
		for (uint8 V : Mask) { if (V) ++N; }
		return N;
	}

	float Coverage(const TArray<uint8>& Mask)
	{
		if (Mask.Num() == 0) return 0.0f;
		return static_cast<float>(CountNonZero(Mask)) / static_cast<float>(Mask.Num());
	}

	bool MasksOverlap(const TArray<uint8>& A, const TArray<uint8>& B)
	{
		const int32 N = FMath::Min(A.Num(), B.Num());
		for (int32 i = 0; i < N; ++i) { if (A[i] && B[i]) return true; }
		return false;
	}

	void MaskAnd(const TArray<uint8>& A, const TArray<uint8>& B, TArray<uint8>& OutResult)
	{
		const int32 N = FMath::Min(A.Num(), B.Num());
		OutResult.SetNumZeroed(N);
		for (int32 i = 0; i < N; ++i) { OutResult[i] = (A[i] && B[i]) ? 1 : 0; }
	}

	void MaskAndNot(const TArray<uint8>& A, const TArray<uint8>& B, TArray<uint8>& OutResult)
	{
		const int32 N = FMath::Min(A.Num(), B.Num());
		OutResult.SetNumZeroed(N);
		for (int32 i = 0; i < N; ++i) { OutResult[i] = (A[i] && !B[i]) ? 1 : 0; }
	}

	void MaskOr(const TArray<uint8>& A, const TArray<uint8>& B, TArray<uint8>& OutResult)
	{
		const int32 N = FMath::Max(A.Num(), B.Num());
		OutResult.SetNumZeroed(N);
		for (int32 i = 0; i < N; ++i)
		{
			const uint8 Av = (i < A.Num()) ? A[i] : 0;
			const uint8 Bv = (i < B.Num()) ? B[i] : 0;
			OutResult[i] = (Av || Bv) ? 1 : 0;
		}
	}
}
