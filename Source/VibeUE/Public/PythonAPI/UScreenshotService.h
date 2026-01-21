// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UScreenshotService.generated.h"

/**
 * Result of a screenshot operation
 */
USTRUCT(BlueprintType)
struct FScreenshotResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Screenshot")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Screenshot")
	FString FilePath;

	UPROPERTY(BlueprintReadWrite, Category = "Screenshot")
	FString Message;

	UPROPERTY(BlueprintReadWrite, Category = "Screenshot")
	int32 Width = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Screenshot")
	int32 Height = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Screenshot")
	FString CapturedWindowTitle;
};

/**
 * Information about an open editor tab/window
 */
USTRUCT(BlueprintType)
struct FEditorTabInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Editor")
	FString TabLabel;

	UPROPERTY(BlueprintReadWrite, Category = "Editor")
	FString TabType;

	UPROPERTY(BlueprintReadWrite, Category = "Editor")
	FString AssetPath;

	UPROPERTY(BlueprintReadWrite, Category = "Editor")
	bool bIsForeground = false;
};

/**
 * Service for capturing screenshots of editor windows and viewports.
 * Provides Python-accessible methods for AI vision capabilities.
 * 
 * This service supports:
 * - Level viewport screenshots (using existing UE functionality)
 * - Full editor window screenshots (using Windows GDI with DWM)
 * - Active tab/window information retrieval
 * 
 * Usage from Python:
 *   import unreal
 *   service = unreal.ScreenshotService()
 *   result = service.capture_active_window("E:/Screenshots/capture.png")
 *   if result.success:
 *       print(f"Captured: {result.file_path}")
 */
UCLASS(BlueprintType)
class VIBEUE_API UScreenshotService : public UObject
{
	GENERATED_BODY()

public:
	UScreenshotService();

	/**
	 * Capture the level viewport to a file.
	 * This uses Unreal's built-in screenshot functionality and only works for level viewports.
	 * @param FilePath - Output file path (PNG format recommended)
	 * @param Width - Desired width (0 for viewport size)
	 * @param Height - Desired height (0 for viewport size)
	 * @return Screenshot result with success status and file info
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Screenshot")
	FScreenshotResult CaptureViewport(const FString& FilePath, int32 Width = 1920, int32 Height = 1080);

	/**
	 * Capture the entire Unreal Editor window to a file.
	 * Uses Windows DWM (Desktop Window Manager) for accurate capture of DirectX content.
	 * Works for all editor content including Blueprint graphs, material editors, etc.
	 * @param FilePath - Output file path (PNG format recommended)
	 * @return Screenshot result with success status and file info
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Screenshot")
	FScreenshotResult CaptureEditorWindow(const FString& FilePath);

	/**
	 * Capture the currently active/foreground window to a file.
	 * This captures whatever window is currently in focus.
	 * @param FilePath - Output file path (PNG format recommended)
	 * @return Screenshot result with success status and file info
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Screenshot")
	FScreenshotResult CaptureActiveWindow(const FString& FilePath);

	/**
	 * Get information about open editor tabs.
	 * Useful for understanding what the user is currently viewing.
	 * @return Array of editor tab information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Screenshot")
	TArray<FEditorTabInfo> GetOpenEditorTabs();

	/**
	 * Get the title of the currently focused window.
	 * @return Window title string
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Screenshot")
	FString GetActiveWindowTitle();

	/**
	 * Check if the Unreal Editor main window is in focus.
	 * @return True if the editor is the foreground window
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Screenshot")
	bool IsEditorWindowActive();

private:
	/**
	 * Find the Unreal Editor main window handle
	 * @return Window handle or nullptr if not found
	 */
	void* FindEditorWindowHandle();

	/**
	 * Capture a window by its handle to a file
	 * @param WindowHandle - Native window handle
	 * @param FilePath - Output file path
	 * @param OutResult - Result structure to populate
	 */
	void CaptureWindowToFile(void* WindowHandle, const FString& FilePath, FScreenshotResult& OutResult);

	/**
	 * Save raw bitmap data to PNG file
	 * @param BitmapData - Raw BGRA pixel data
	 * @param Width - Image width
	 * @param Height - Image height
	 * @param FilePath - Output file path
	 * @return True if save succeeded
	 */
	bool SaveBitmapAsPNG(const TArray<uint8>& BitmapData, int32 Width, int32 Height, const FString& FilePath);
};
