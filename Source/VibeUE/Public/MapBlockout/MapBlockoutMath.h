// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Image-processing primitives the MapBlockout pipeline needs. These mirror the
 * numpy + scipy.ndimage calls the host-Python reference implementation uses, but
 * stay header-light and stdlib-only — no third-party deps.
 *
 * All functions operate on row-major TArrays. By convention row 0 is the top
 * (north) of the rendered image, which matches the contributor's PNG output.
 * The landcover grid USTRUCT uses row 0 = south; conversion happens once at
 * the boundary (see UMapBlockoutService::ExportLandcoverGrid).
 */
namespace MapBlockoutMath
{
	// -------------------------------------------------------------------------
	// Float fields (weight maps, normalized 0..1)
	// -------------------------------------------------------------------------

	/** Bilinear resample of a float field. */
	VIBEUE_API void ResampleBilinear(
		const TArray<float>& Src, int32 SrcW, int32 SrcH,
		TArray<float>& OutDst, int32 DstW, int32 DstH);

	/** Separable Gaussian blur with the given sigma (radius derived as ceil(3*sigma)). */
	VIBEUE_API void GaussianBlur(
		TArray<float>& InOut, int32 W, int32 H, float Sigma);

	/** Vertical flip (north <-> south). Used to translate landcover grid row order. */
	VIBEUE_API void FlipVertical(TArray<float>& InOut, int32 W, int32 H);
	VIBEUE_API void FlipVerticalU8(TArray<uint8>& InOut, int32 W, int32 H);

	// -------------------------------------------------------------------------
	// Binary masks (0/1 uint8)
	// -------------------------------------------------------------------------

	/** Morphological dilation with a (2*Radius+1)-square kernel. */
	VIBEUE_API void Dilate(
		TArray<uint8>& InOut, int32 W, int32 H, int32 Radius);

	/** Morphological erosion. */
	VIBEUE_API void Erode(
		TArray<uint8>& InOut, int32 W, int32 H, int32 Radius);

	/** Erode then dilate (removes small noise specks). */
	VIBEUE_API void BinaryOpening(
		TArray<uint8>& InOut, int32 W, int32 H, int32 Radius);

	/** Dilate then erode (closes small gaps inside shapes). */
	VIBEUE_API void BinaryClosing(
		TArray<uint8>& InOut, int32 W, int32 H, int32 Radius);

	/**
	 * Compute the size (pixel count) of every connected component.
	 * Caller passes the label grid from LabelConnectedComponents.
	 * OutSizes[K-1] is the size of label K (labels are 1..NumComponents).
	 */
	VIBEUE_API void ComponentSizes(
		const TArray<int32>& Labels, int32 NumComponents,
		TArray<int32>& OutSizes);

	/**
	 * Generate a deterministic noise field at full resolution: samples a small
	 * (W/BlockSize) x (H/BlockSize) grid of uniform-random values seeded by Seed,
	 * then bilinearly upsamples to W x H. Equivalent to numpy's
	 * `Image.fromarray(rng.rand(H//div, W//div)*255).resize((W,H), BICUBIC)/255`
	 * used by the host-Python reference's `_noise()` helper.
	 */
	VIBEUE_API void GenerateNoiseField(
		TArray<float>& Out, int32 W, int32 H, int32 BlockSize, uint32 Seed);

	/**
	 * Euclidean distance transform: for every cell, the distance to the nearest
	 * non-zero cell (in cell units). Felzenszwalb-Huttenlocher 1D-pass algorithm.
	 */
	VIBEUE_API void DistanceTransformEDT(
		const TArray<uint8>& Mask, int32 W, int32 H,
		TArray<float>& OutDistance);

	/**
	 * Connected-components label (4-connectivity). Returns the component count.
	 * OutLabels is the same shape as Mask; cell value 0 = background, 1..K = label.
	 */
	VIBEUE_API int32 LabelConnectedComponents(
		const TArray<uint8>& Mask, int32 W, int32 H,
		TArray<int32>& OutLabels);

	/**
	 * Zhang-Suen 2D skeletonization. Used to extract river / road centerlines
	 * from a thick binary mask.
	 */
	VIBEUE_API void Skeletonize(
		TArray<uint8>& InOut, int32 W, int32 H);

	// -------------------------------------------------------------------------
	// Rasterization
	// -------------------------------------------------------------------------

	/** Bresenham line into a uint8 mask. Sets cells to Value (default 1). */
	VIBEUE_API void RasterizeLine(
		TArray<uint8>& Mask, int32 W, int32 H,
		int32 X0, int32 Y0, int32 X1, int32 Y1,
		int32 ThicknessRadius = 0,
		uint8 Value = 1);

	/** Filled disk. */
	VIBEUE_API void RasterizeDisk(
		TArray<uint8>& Mask, int32 W, int32 H,
		int32 Cx, int32 Cy, int32 Radius,
		uint8 Value = 1);

	/** Polyline (chain of Bresenham segments) with constant thickness. */
	VIBEUE_API void RasterizePolyline(
		TArray<uint8>& Mask, int32 W, int32 H,
		const TArray<FIntPoint>& Points,
		int32 ThicknessRadius = 0,
		uint8 Value = 1);

	/** Filled convex polygon (scanline fill). */
	VIBEUE_API void RasterizePolygonFilled(
		TArray<uint8>& Mask, int32 W, int32 H,
		const TArray<FIntPoint>& Polygon,
		uint8 Value = 1);

	// -------------------------------------------------------------------------
	// Stats
	// -------------------------------------------------------------------------

	/** Count cells where Mask[i] != 0. */
	VIBEUE_API int32 CountNonZero(const TArray<uint8>& Mask);

	/** Fraction in [0,1]. */
	VIBEUE_API float Coverage(const TArray<uint8>& Mask);

	/** Returns true if any cell of A AND any cell of B is co-located. */
	VIBEUE_API bool MasksOverlap(
		const TArray<uint8>& A, const TArray<uint8>& B);

	/** OutResult = A AND B  (cell-wise). */
	VIBEUE_API void MaskAnd(
		const TArray<uint8>& A, const TArray<uint8>& B, TArray<uint8>& OutResult);

	/** OutResult = A AND NOT B  (cell-wise). */
	VIBEUE_API void MaskAndNot(
		const TArray<uint8>& A, const TArray<uint8>& B, TArray<uint8>& OutResult);

	/** OutResult = A OR B  (cell-wise). */
	VIBEUE_API void MaskOr(
		const TArray<uint8>& A, const TArray<uint8>& B, TArray<uint8>& OutResult);
}
