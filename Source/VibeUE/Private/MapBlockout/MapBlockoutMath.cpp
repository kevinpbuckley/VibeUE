// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "MapBlockout/MapBlockoutMath.h"
#include "Math/UnrealMathUtility.h"

// Pure-C++ port of the numpy + scipy.ndimage primitives the original
// host-Python reference implementation used.
// No UE engine deps beyond TArray + FMath.

namespace MapBlockoutMath
{
	// ---------------------------------------------------------------------
	// Float fields
	// ---------------------------------------------------------------------

	void ResampleBilinear(const TArray<float>& Src, int32 SrcW, int32 SrcH,
		TArray<float>& OutDst, int32 DstW, int32 DstH)
	{
		OutDst.SetNumZeroed(DstW * DstH);
		if (SrcW <= 0 || SrcH <= 0 || DstW <= 0 || DstH <= 0 || Src.Num() < SrcW * SrcH) { return; }
		if (SrcW == DstW && SrcH == DstH) { OutDst = Src; return; }

		const float ScaleX = static_cast<float>(SrcW - 1) / FMath::Max(1, DstW - 1);
		const float ScaleY = static_cast<float>(SrcH - 1) / FMath::Max(1, DstH - 1);
		for (int32 Y = 0; Y < DstH; ++Y)
		{
			const float Sy = Y * ScaleY;
			const int32 Y0 = FMath::Clamp(static_cast<int32>(Sy), 0, SrcH - 1);
			const int32 Y1 = FMath::Clamp(Y0 + 1, 0, SrcH - 1);
			const float Fy = Sy - Y0;
			for (int32 X = 0; X < DstW; ++X)
			{
				const float Sx = X * ScaleX;
				const int32 X0 = FMath::Clamp(static_cast<int32>(Sx), 0, SrcW - 1);
				const int32 X1 = FMath::Clamp(X0 + 1, 0, SrcW - 1);
				const float Fx = Sx - X0;
				const float V00 = Src[Y0 * SrcW + X0];
				const float V10 = Src[Y0 * SrcW + X1];
				const float V01 = Src[Y1 * SrcW + X0];
				const float V11 = Src[Y1 * SrcW + X1];
				const float A = V00 * (1.0f - Fx) + V10 * Fx;
				const float B = V01 * (1.0f - Fx) + V11 * Fx;
				OutDst[Y * DstW + X] = A * (1.0f - Fy) + B * Fy;
			}
		}
	}

	void GaussianBlur(TArray<float>& InOut, int32 W, int32 H, float Sigma)
	{
		if (W <= 0 || H <= 0 || Sigma <= 0.0f || InOut.Num() < W * H) { return; }

		const int32 Radius = FMath::Max(1, FMath::CeilToInt(3.0f * Sigma));
		TArray<float> Kernel; Kernel.SetNumZeroed(2 * Radius + 1);
		float Sum = 0.0f;
		const float TwoSigSq = 2.0f * Sigma * Sigma;
		for (int32 I = -Radius; I <= Radius; ++I)
		{
			const float V = FMath::Exp(-(I * I) / TwoSigSq);
			Kernel[I + Radius] = V;
			Sum += V;
		}
		for (float& K : Kernel) { K /= Sum; }

		TArray<float> Temp; Temp.SetNumZeroed(W * H);

		// Horizontal pass
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				float Acc = 0.0f;
				for (int32 K = -Radius; K <= Radius; ++K)
				{
					const int32 Sx = FMath::Clamp(X + K, 0, W - 1);
					Acc += InOut[Y * W + Sx] * Kernel[K + Radius];
				}
				Temp[Y * W + X] = Acc;
			}
		}
		// Vertical pass
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				float Acc = 0.0f;
				for (int32 K = -Radius; K <= Radius; ++K)
				{
					const int32 Sy = FMath::Clamp(Y + K, 0, H - 1);
					Acc += Temp[Sy * W + X] * Kernel[K + Radius];
				}
				InOut[Y * W + X] = Acc;
			}
		}
	}

	void FlipVertical(TArray<float>& InOut, int32 W, int32 H)
	{
		if (W <= 0 || H <= 0 || InOut.Num() < W * H) { return; }
		for (int32 Y = 0; Y < H / 2; ++Y)
		{
			const int32 YOther = H - 1 - Y;
			for (int32 X = 0; X < W; ++X) { Swap(InOut[Y * W + X], InOut[YOther * W + X]); }
		}
	}

	void FlipVerticalU8(TArray<uint8>& InOut, int32 W, int32 H)
	{
		if (W <= 0 || H <= 0 || InOut.Num() < W * H) { return; }
		for (int32 Y = 0; Y < H / 2; ++Y)
		{
			const int32 YOther = H - 1 - Y;
			for (int32 X = 0; X < W; ++X) { Swap(InOut[Y * W + X], InOut[YOther * W + X]); }
		}
	}

	// ---------------------------------------------------------------------
	// Binary masks: dilate / erode (square structuring element of radius R)
	// ---------------------------------------------------------------------

	static void MorphSquare(TArray<uint8>& InOut, int32 W, int32 H, int32 Radius, bool bDilate)
	{
		if (W <= 0 || H <= 0 || Radius <= 0 || InOut.Num() < W * H) { return; }
		// Separable: horizontal max/min then vertical
		TArray<uint8> Temp; Temp.SetNumZeroed(W * H);
		auto Op = [bDilate](uint8 A, uint8 B) -> uint8 {
			return bDilate ? (A | B) : (A & B);
		};
		const uint8 Identity = bDilate ? 0 : 1;

		// Horizontal
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				uint8 Acc = Identity;
				for (int32 K = -Radius; K <= Radius; ++K)
				{
					const int32 Sx = FMath::Clamp(X + K, 0, W - 1);
					Acc = Op(Acc, InOut[Y * W + Sx]);
				}
				Temp[Y * W + X] = Acc;
			}
		}
		// Vertical
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				uint8 Acc = Identity;
				for (int32 K = -Radius; K <= Radius; ++K)
				{
					const int32 Sy = FMath::Clamp(Y + K, 0, H - 1);
					Acc = Op(Acc, Temp[Sy * W + X]);
				}
				InOut[Y * W + X] = Acc ? 1 : 0;
			}
		}
	}

	void Dilate(TArray<uint8>& InOut, int32 W, int32 H, int32 Radius)
	{
		MorphSquare(InOut, W, H, Radius, /*bDilate=*/true);
	}

	void Erode(TArray<uint8>& InOut, int32 W, int32 H, int32 Radius)
	{
		MorphSquare(InOut, W, H, Radius, /*bDilate=*/false);
	}

	void BinaryOpening(TArray<uint8>& InOut, int32 W, int32 H, int32 Radius)
	{
		Erode(InOut, W, H, Radius);
		Dilate(InOut, W, H, Radius);
	}

	void BinaryClosing(TArray<uint8>& InOut, int32 W, int32 H, int32 Radius)
	{
		Dilate(InOut, W, H, Radius);
		Erode(InOut, W, H, Radius);
	}

	void ComponentSizes(const TArray<int32>& Labels, int32 NumComponents, TArray<int32>& OutSizes)
	{
		OutSizes.SetNumZeroed(FMath::Max(0, NumComponents));
		if (NumComponents <= 0) { return; }
		for (int32 L : Labels)
		{
			if (L >= 1 && L <= NumComponents) { ++OutSizes[L - 1]; }
		}
	}

	void GenerateNoiseField(TArray<float>& Out, int32 W, int32 H, int32 BlockSize, uint32 Seed)
	{
		Out.SetNumZeroed(W * H);
		if (W <= 0 || H <= 0) { return; }
		BlockSize = FMath::Max(1, BlockSize);
		const int32 NX = FMath::Max(2, W / BlockSize);
		const int32 NY = FMath::Max(2, H / BlockSize);

		// LCG, deterministic per Seed
		uint32 State = Seed ? Seed : 0xa3c59ac3u;
		auto NextU01 = [&State]() -> float {
			State = State * 1664525u + 1013904223u;
			return (State >> 8) * (1.0f / 16777216.0f);
		};

		TArray<float> Small; Small.SetNumZeroed(NX * NY);
		for (int32 i = 0; i < NX * NY; ++i) { Small[i] = NextU01(); }
		ResampleBilinear(Small, NX, NY, Out, W, H);
	}

	// ---------------------------------------------------------------------
	// Euclidean distance transform (Felzenszwalb-Huttenlocher, 1D-pass per row/column)
	// ---------------------------------------------------------------------

	static void EDT1D(const float* F, int32 N, float* OutD)
	{
		// F[i] = squared distance contribution; OutD[i] = lower-envelope sample at i.
		TArray<int32> V; V.SetNumZeroed(N);
		TArray<float> Z; Z.SetNumZeroed(N + 1);
		const float INF = TNumericLimits<float>::Max();
		V[0] = 0; Z[0] = -INF; Z[1] = +INF;
		int32 K = 0;
		for (int32 Q = 1; Q < N; ++Q)
		{
			float S;
			while (true)
			{
				const int32 Vk = V[K];
				S = ((F[Q] + Q * Q) - (F[Vk] + Vk * Vk)) / (2.0f * (Q - Vk));
				if (S > Z[K]) { break; }
				if (--K < 0) { K = 0; break; }
			}
			++K;
			V[K] = Q;
			Z[K] = S;
			Z[K + 1] = +INF;
		}
		K = 0;
		for (int32 Q = 0; Q < N; ++Q)
		{
			while (Z[K + 1] < Q) { ++K; }
			const int32 Vk = V[K];
			OutD[Q] = (Q - Vk) * (Q - Vk) + F[Vk];
		}
	}

	void DistanceTransformEDT(const TArray<uint8>& Mask, int32 W, int32 H, TArray<float>& OutDistance)
	{
		OutDistance.SetNumZeroed(W * H);
		if (W <= 0 || H <= 0 || Mask.Num() < W * H) { return; }
		const float INF = 1e20f;

		// Seed: 0 where mask is set (the "source" set we measure distance TO), INF elsewhere.
		TArray<float> F; F.SetNumUninitialized(W * H);
		for (int32 i = 0; i < W * H; ++i) { F[i] = Mask[i] ? 0.0f : INF; }

		TArray<float> Col; Col.SetNumZeroed(FMath::Max(W, H));
		TArray<float> Out; Out.SetNumZeroed(FMath::Max(W, H));

		// Column pass
		for (int32 X = 0; X < W; ++X)
		{
			for (int32 Y = 0; Y < H; ++Y) { Col[Y] = F[Y * W + X]; }
			EDT1D(Col.GetData(), H, Out.GetData());
			for (int32 Y = 0; Y < H; ++Y) { F[Y * W + X] = Out[Y]; }
		}
		// Row pass
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X) { Col[X] = F[Y * W + X]; }
			EDT1D(Col.GetData(), W, Out.GetData());
			for (int32 X = 0; X < W; ++X) { OutDistance[Y * W + X] = FMath::Sqrt(Out[X]); }
		}
	}

	// ---------------------------------------------------------------------
	// Connected components (4-connectivity, two-pass union-find)
	// ---------------------------------------------------------------------

	static int32 UFFind(TArray<int32>& Parent, int32 X)
	{
		while (Parent[X] != X) { Parent[X] = Parent[Parent[X]]; X = Parent[X]; }
		return X;
	}
	static void UFUnion(TArray<int32>& Parent, int32 A, int32 B)
	{
		const int32 Ra = UFFind(Parent, A);
		const int32 Rb = UFFind(Parent, B);
		if (Ra != Rb) { Parent[Ra] = Rb; }
	}

	int32 LabelConnectedComponents(const TArray<uint8>& Mask, int32 W, int32 H, TArray<int32>& OutLabels)
	{
		OutLabels.SetNumZeroed(W * H);
		if (W <= 0 || H <= 0 || Mask.Num() < W * H) { return 0; }

		TArray<int32> Parent;
		Parent.Reserve(W * H / 4 + 4);
		Parent.Add(0); // label 0 = background (sentinel)

		// First pass
		for (int32 Y = 0; Y < H; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				if (!Mask[Y * W + X]) { continue; }
				const int32 LL = (X > 0) ? OutLabels[Y * W + (X - 1)] : 0;
				const int32 LU = (Y > 0) ? OutLabels[(Y - 1) * W + X] : 0;
				if (!LL && !LU)
				{
					const int32 NewLabel = Parent.Num();
					Parent.Add(NewLabel);
					OutLabels[Y * W + X] = NewLabel;
				}
				else if (LL && !LU) { OutLabels[Y * W + X] = LL; }
				else if (!LL && LU) { OutLabels[Y * W + X] = LU; }
				else
				{
					OutLabels[Y * W + X] = FMath::Min(LL, LU);
					if (LL != LU) { UFUnion(Parent, LL, LU); }
				}
			}
		}
		// Second pass — flatten + compact labels
		TArray<int32> Remap; Remap.SetNumZeroed(Parent.Num());
		int32 Next = 1;
		for (int32 I = 1; I < Parent.Num(); ++I)
		{
			const int32 R = UFFind(Parent, I);
			if (Remap[R] == 0) { Remap[R] = Next++; }
			Remap[I] = Remap[R];
		}
		for (int32 I = 0; I < W * H; ++I)
		{
			if (OutLabels[I] > 0) { OutLabels[I] = Remap[OutLabels[I]]; }
		}
		return Next - 1;
	}

	// ---------------------------------------------------------------------
	// Skeletonize (Zhang-Suen 1984)
	// ---------------------------------------------------------------------

	void Skeletonize(TArray<uint8>& InOut, int32 W, int32 H)
	{
		if (W < 3 || H < 3 || InOut.Num() < W * H) { return; }
		auto IDX = [W](int32 X, int32 Y) { return Y * W + X; };

		TArray<uint8> ToErase; ToErase.SetNumZeroed(W * H);

		bool bChanged = true;
		while (bChanged)
		{
			bChanged = false;
			for (int32 SubIter = 0; SubIter < 2; ++SubIter)
			{
				FMemory::Memzero(ToErase.GetData(), ToErase.Num());

				for (int32 Y = 1; Y < H - 1; ++Y)
				{
					for (int32 X = 1; X < W - 1; ++X)
					{
						if (!InOut[IDX(X, Y)]) { continue; }

						const uint8 P2 = InOut[IDX(X,     Y - 1)];
						const uint8 P3 = InOut[IDX(X + 1, Y - 1)];
						const uint8 P4 = InOut[IDX(X + 1, Y    )];
						const uint8 P5 = InOut[IDX(X + 1, Y + 1)];
						const uint8 P6 = InOut[IDX(X,     Y + 1)];
						const uint8 P7 = InOut[IDX(X - 1, Y + 1)];
						const uint8 P8 = InOut[IDX(X - 1, Y    )];
						const uint8 P9 = InOut[IDX(X - 1, Y - 1)];

						const int32 B = P2 + P3 + P4 + P5 + P6 + P7 + P8 + P9;
						if (B < 2 || B > 6) { continue; }

						int32 A = 0;
						if (!P2 && P3) ++A; if (!P3 && P4) ++A; if (!P4 && P5) ++A;
						if (!P5 && P6) ++A; if (!P6 && P7) ++A; if (!P7 && P8) ++A;
						if (!P8 && P9) ++A; if (!P9 && P2) ++A;
						if (A != 1) { continue; }

						if (SubIter == 0)
						{
							if (P2 && P4 && P6) { continue; }
							if (P4 && P6 && P8) { continue; }
						}
						else
						{
							if (P2 && P4 && P8) { continue; }
							if (P2 && P6 && P8) { continue; }
						}
						ToErase[IDX(X, Y)] = 1;
					}
				}
				for (int32 i = 0; i < W * H; ++i)
				{
					if (ToErase[i] && InOut[i]) { InOut[i] = 0; bChanged = true; }
				}
			}
		}
	}

	// ---------------------------------------------------------------------
	// Rasterizers
	// ---------------------------------------------------------------------

	static FORCEINLINE void PlotPixelThickU8(TArray<uint8>& Mask, int32 W, int32 H,
		int32 X, int32 Y, int32 R, uint8 Value)
	{
		if (R <= 0)
		{
			if (X >= 0 && Y >= 0 && X < W && Y < H) { Mask[Y * W + X] = Value; }
			return;
		}
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
				if (Dx * Dx + Dy * Dy <= R2) { Mask[Yi * W + Xi] = Value; }
			}
		}
	}

	void RasterizeLine(TArray<uint8>& Mask, int32 W, int32 H,
		int32 X0, int32 Y0, int32 X1, int32 Y1, int32 ThicknessRadius, uint8 Value)
	{
		if (W <= 0 || H <= 0 || Mask.Num() < W * H) { return; }
		int32 Dx = FMath::Abs(X1 - X0), Sx = (X0 < X1) ? 1 : -1;
		int32 Dy = -FMath::Abs(Y1 - Y0), Sy = (Y0 < Y1) ? 1 : -1;
		int32 Err = Dx + Dy;
		int32 X = X0, Y = Y0;
		while (true)
		{
			PlotPixelThickU8(Mask, W, H, X, Y, ThicknessRadius, Value);
			if (X == X1 && Y == Y1) { break; }
			const int32 E2 = 2 * Err;
			if (E2 >= Dy) { Err += Dy; X += Sx; }
			if (E2 <= Dx) { Err += Dx; Y += Sy; }
		}
	}

	void RasterizeDisk(TArray<uint8>& Mask, int32 W, int32 H,
		int32 Cx, int32 Cy, int32 Radius, uint8 Value)
	{
		PlotPixelThickU8(Mask, W, H, Cx, Cy, Radius, Value);
	}

	void RasterizePolyline(TArray<uint8>& Mask, int32 W, int32 H,
		const TArray<FIntPoint>& Points, int32 ThicknessRadius, uint8 Value)
	{
		for (int32 I = 0; I + 1 < Points.Num(); ++I)
		{
			RasterizeLine(Mask, W, H, Points[I].X, Points[I].Y,
				Points[I + 1].X, Points[I + 1].Y, ThicknessRadius, Value);
		}
	}

	void RasterizePolygonFilled(TArray<uint8>& Mask, int32 W, int32 H,
		const TArray<FIntPoint>& Polygon, uint8 Value)
	{
		if (Polygon.Num() < 3 || W <= 0 || H <= 0 || Mask.Num() < W * H) { return; }

		int32 MinY = TNumericLimits<int32>::Max(), MaxY = TNumericLimits<int32>::Min();
		for (const FIntPoint& P : Polygon)
		{
			MinY = FMath::Min(MinY, P.Y); MaxY = FMath::Max(MaxY, P.Y);
		}
		MinY = FMath::Max(0, MinY);
		MaxY = FMath::Min(H - 1, MaxY);

		TArray<int32> Crossings;
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			Crossings.Reset();
			for (int32 I = 0; I < Polygon.Num(); ++I)
			{
				const FIntPoint& A = Polygon[I];
				const FIntPoint& B = Polygon[(I + 1) % Polygon.Num()];
				if (A.Y == B.Y) { continue; }
				const int32 Y0 = FMath::Min(A.Y, B.Y);
				const int32 Y1 = FMath::Max(A.Y, B.Y);
				if (Y < Y0 || Y >= Y1) { continue; }
				const double T = static_cast<double>(Y - A.Y) / static_cast<double>(B.Y - A.Y);
				const int32 X = static_cast<int32>(A.X + T * (B.X - A.X));
				Crossings.Add(X);
			}
			Crossings.Sort();
			for (int32 I = 0; I + 1 < Crossings.Num(); I += 2)
			{
				const int32 X0 = FMath::Max(0, Crossings[I]);
				const int32 X1 = FMath::Min(W - 1, Crossings[I + 1]);
				for (int32 X = X0; X <= X1; ++X) { Mask[Y * W + X] = Value; }
			}
		}
	}

	// ---------------------------------------------------------------------
	// Stats
	// ---------------------------------------------------------------------

	int32 CountNonZero(const TArray<uint8>& Mask)
	{
		int32 N = 0;
		for (uint8 V : Mask) { if (V) ++N; }
		return N;
	}

	float Coverage(const TArray<uint8>& Mask)
	{
		if (Mask.Num() == 0) { return 0.0f; }
		return static_cast<float>(CountNonZero(Mask)) / static_cast<float>(Mask.Num());
	}

	bool MasksOverlap(const TArray<uint8>& A, const TArray<uint8>& B)
	{
		const int32 N = FMath::Min(A.Num(), B.Num());
		for (int32 i = 0; i < N; ++i) { if (A[i] && B[i]) { return true; } }
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
