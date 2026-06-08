// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Image I/O + bitmap drawing helpers for the MapBlockout snapshot renderer.
 * RGBA8 buffers stored row-major (row 0 = top). PNG encode/decode goes through
 * IImageWrapperModule (already a plugin dependency).
 */
namespace MapBlockoutImage
{
	/** Authoritative MapDesigner color chart (FColor uses BGRA in memory but APIs accept RGBA). */
	namespace Colors
	{
		extern VIBEUE_API const FColor MainRoad;        // White
		extern VIBEUE_API const FColor DirtRoad;        // Brown
		extern VIBEUE_API const FColor Treeline;        // Light Purple
		extern VIBEUE_API const FColor Forest;          // Dark Purple
		extern VIBEUE_API const FColor Building;        // Light Grey
		extern VIBEUE_API const FColor Bridge;          // Orange
		extern VIBEUE_API const FColor Railway;         // Dark Grey
		extern VIBEUE_API const FColor POIBoundary;     // Bright Green
		extern VIBEUE_API const FColor Field;           // Yellow
		extern VIBEUE_API const FColor Scrub;           // Light Blue
		extern VIBEUE_API const FColor River;           // Blue (context only)
		extern VIBEUE_API const FColor Background;      // Dark
		extern VIBEUE_API const FColor Panel;           // Color-key panel
		extern VIBEUE_API const FColor Grid;            // Faint reference grid
		extern VIBEUE_API const FColor Ink;             // Text
	}

	/** Allocate an RGBA8 bitmap filled with a single color. */
	VIBEUE_API void NewBitmap(
		TArray<FColor>& OutBitmap, int32 Width, int32 Height, FColor Fill);

	/** Bresenham line into an RGBA8 bitmap. */
	VIBEUE_API void DrawLine(
		TArray<FColor>& Bitmap, int32 W, int32 H,
		int32 X0, int32 Y0, int32 X1, int32 Y1,
		FColor Color, int32 ThicknessRadius = 0);

	VIBEUE_API void DrawPolyline(
		TArray<FColor>& Bitmap, int32 W, int32 H,
		const TArray<FIntPoint>& Points,
		FColor Color, int32 ThicknessRadius = 0);

	/** Filled disk. */
	VIBEUE_API void DrawDisk(
		TArray<FColor>& Bitmap, int32 W, int32 H,
		int32 Cx, int32 Cy, int32 Radius, FColor Color);

	/** Outlined circle (used for POI boundary). */
	VIBEUE_API void DrawCircleOutline(
		TArray<FColor>& Bitmap, int32 W, int32 H,
		int32 Cx, int32 Cy, int32 Radius, FColor Color, int32 Thickness = 2);

	/** Filled axis-aligned rectangle. */
	VIBEUE_API void DrawRect(
		TArray<FColor>& Bitmap, int32 W, int32 H,
		int32 X0, int32 Y0, int32 X1, int32 Y1, FColor Color);

	/**
	 * Paint a binary mask into a bitmap as a single color. Used to lay down field
	 * fills, forest fills, scrub fills, etc.
	 */
	VIBEUE_API void OverlayMask(
		TArray<FColor>& Bitmap, int32 W, int32 H,
		const TArray<uint8>& Mask, FColor Color);

	/**
	 * Heatmap rendering — accumulate every primitive (mask/polyline/disk) into a
	 * float density buffer and tonemap to grayscale. Used for the FoliageHeatMap.png
	 * and MapHeatMap.png deliverables.
	 */
	VIBEUE_API void RenderHeatmap(
		const TArray<float>& Density, int32 W, int32 H,
		TArray<FColor>& OutBitmap);

	/** One entry in the color key (a color swatch + text label). */
	struct FKeyEntry
	{
		FColor Color;
		FString Label;
		/** "swatch" = filled rect; "line" = horizontal line; "ring" = outlined circle. */
		FString Style;

		FKeyEntry(FColor InColor, const FString& InLabel, const FString& InStyle = TEXT("swatch"))
			: Color(InColor), Label(InLabel), Style(InStyle) {}
	};

	/**
	 * Draws the top-right color key panel with the MapDesigner color chart.
	 * Panel must not cover the map area; caller passes in the allowed panel rect.
	 * Returns the panel's left X (used by snapshot renderer's "Color Key off map" check).
	 */
	VIBEUE_API int32 DrawColorKey(
		TArray<FColor>& Bitmap, int32 W, int32 H,
		int32 PanelLeft, int32 PanelTop, int32 PanelWidth,
		const FString& Title, const TArray<FKeyEntry>& Entries);

	/** Filled solid text using a built-in 5x7 ASCII bitmap font. Scale=1 → 5x7 chars. */
	VIBEUE_API void DrawText5x7(
		TArray<FColor>& Bitmap, int32 W, int32 H,
		int32 X, int32 Y, const FString& Text, FColor Color, int32 Scale = 2);

	/**
	 * Encode a bitmap to a 24-bit PNG file at OutputPath (creates parent dirs).
	 *
	 * @return true on success
	 */
	VIBEUE_API bool WritePNG(
		const TArray<FColor>& Bitmap, int32 W, int32 H,
		const FString& OutputPath);

	/**
	 * Load a grayscale PNG into a uint8 mask. Used to round-trip mask outputs for
	 * test fixtures.
	 */
	VIBEUE_API bool LoadGrayscalePNG(
		const FString& FilePath,
		TArray<uint8>& OutPixels, int32& OutW, int32& OutH);
}
