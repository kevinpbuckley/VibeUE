// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UViewportService.generated.h"

/**
 * Current state of the level editor viewport.
 *
 * Python access:
 *   info = unreal.ViewportService.get_viewport_info()
 *
 * Properties:
 * - viewport_type (str): "perspective", "top", "bottom", "left", "right", "front", "back", "ortho_freelook"
 * - location (Vector): Camera world location
 * - rotation (Rotator): Camera world rotation
 * - fov (float): Horizontal field of view in degrees (perspective only)
 * - near_clip_plane (float): Near clipping plane distance (-1 = engine default)
 * - far_clip_plane (float): Far clipping plane distance (0 = infinity)
 * - is_realtime (bool): Whether the viewport renders in realtime
 * - is_game_view (bool): Whether Game View mode is active (hides editor icons)
 * - allow_cinematic_control (bool): Whether cinematic sequences can control this viewport
 * - exposure_settings_fixed (bool): Whether exposure is fixed (true) or auto (false)
 * - exposure_ev100 (float): Fixed EV100 exposure value (when fixed)
 * - camera_speed_setting (int): Camera movement speed index (1-8)
 * - layout (str): Viewport layout name ("OnePane", "FourPanes2x2", etc.)
 * - viewport_index (int): Index of the active viewport (0-based)
 */
USTRUCT(BlueprintType)
struct FViewportInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	FString ViewportType;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	float FOV = 90.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	float NearClipPlane = -1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	float FarClipPlane = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	bool bIsRealtime = true;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	bool bIsGameView = false;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	bool bAllowCinematicControl = true;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	bool bExposureFixed = false;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	float ExposureEV100 = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	int32 CameraSpeedSetting = 4;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	FString Layout;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	int32 ViewportIndex = 0;
};

/**
 * Viewport Service - Python API for controlling the Unreal Editor level viewport.
 *
 * Provides access to all viewport camera options from the perspective dropdown menu:
 * - View type switching (Perspective, Top, Bottom, Left, Right, Front, Back)
 * - Field of View, Near/Far Clip Plane configuration
 * - Exposure settings (Game Settings / fixed EV100)
 * - Game View toggle, Cinematic Control, Camera Shakes
 * - Viewport layout switching (single pane, quad view 2x2, etc.)
 * - Camera location/rotation control
 * - Realtime rendering toggle
 *
 * Python Usage:
 *   import unreal
 *
 *   # Get current viewport state
 *   info = unreal.ViewportService.get_viewport_info()
 *   print(f"Type: {info.viewport_type}, FOV: {info.fov}")
 *
 *   # Switch to orthographic views
 *   unreal.ViewportService.set_viewport_type("top")
 *   unreal.ViewportService.set_viewport_type("perspective")
 *
 *   # Adjust camera properties
 *   unreal.ViewportService.set_fov(75.0)
 *   unreal.ViewportService.set_near_clip_plane(10.0)
 *   unreal.ViewportService.set_far_clip_plane(50000.0)
 *
 *   # Exposure control
 *   unreal.ViewportService.set_exposure(True, 1.0)
 *   unreal.ViewportService.set_exposure_game_settings()
 *
 *   # Game View and cinematic
 *   unreal.ViewportService.set_game_view(True)
 *   unreal.ViewportService.set_allow_cinematic_control(True)
 *
 *   # Viewport layout (single vs quad)
 *   unreal.ViewportService.set_viewport_layout("FourPanes2x2")
 *   unreal.ViewportService.set_viewport_layout("OnePane")
 *
 *   # Camera position/rotation
 *   unreal.ViewportService.set_camera_location(unreal.Vector(0, 0, 500))
 *   unreal.ViewportService.set_camera_rotation(unreal.Rotator(-45, 0, 0))
 *
 * @note All 19 viewport operations available via Python
 */
UCLASS(BlueprintType)
class VIBEUE_API UViewportService : public UObject
{
	GENERATED_BODY()

public:
	// =================================================================
	// Viewport Information
	// =================================================================

	/**
	 * Get comprehensive information about the active level viewport.
	 *
	 * @return Viewport state including type, camera transform, FOV, exposure, layout
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static FViewportInfo GetViewportInfo();

	// =================================================================
	// View Type (Perspective / Orthographic)
	// =================================================================

	/**
	 * Set the viewport view type.
	 *
	 * @param ViewType - One of: "perspective", "top", "bottom", "left", "right", "front", "back"
	 * @return True if the view type was changed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetViewportType(const FString& ViewType);

	/**
	 * Get the current viewport view type as a string.
	 *
	 * @return One of: "perspective", "top", "bottom", "left", "right", "front", "back", "ortho_freelook"
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static FString GetViewportType();

	// =================================================================
	// View Mode (Lit, Wireframe, Unlit, etc.)
	// =================================================================

	/**
	 * Set the viewport rendering mode (Lit, Wireframe, Unlit, etc.).
	 * Corresponds to the "Lit" dropdown in the viewport toolbar.
	 *
	 * @param ViewMode - One of: "lit", "unlit", "wireframe", "detaillighting", "lightingonly",
	 *                   "lightcomplexity", "shadercomplexity", "pathtracing", "clay"
	 * @return True if the view mode was changed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetViewMode(const FString& ViewMode);

	/**
	 * Get the current viewport rendering mode.
	 *
	 * @return Current view mode as string (e.g., "lit", "wireframe", "unlit")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static FString GetViewMode();

	// =================================================================
	// Field of View
	// =================================================================

	/**
	 * Set the viewport horizontal field of view (perspective mode only).
	 *
	 * @param FOVDegrees - Field of view in degrees (typically 60-120, default 90)
	 * @return True if FOV was set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetFOV(float FOVDegrees);

	/**
	 * Get the current viewport horizontal field of view.
	 *
	 * @return FOV in degrees
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static float GetFOV();

	// =================================================================
	// Clipping Planes
	// =================================================================

	/**
	 * Set the near clipping plane distance.
	 *
	 * @param Distance - Near clip distance in Unreal units. Use -1 to reset to engine default (GNearClippingPlane).
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetNearClipPlane(float Distance);

	/**
	 * Set the far clipping plane distance.
	 *
	 * @param Distance - Far clip distance in Unreal units. Use 0 for infinity (default).
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetFarClipPlane(float Distance);

	// =================================================================
	// Exposure Settings
	// =================================================================

	/**
	 * Set fixed exposure mode with a specific EV100 value.
	 *
	 * @param bFixed - True for fixed exposure, false for auto eye adaptation
	 * @param EV100 - EV100 exposure compensation value (default 1.0)
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetExposure(bool bFixed, float EV100 = 1.0f);

	/**
	 * Set exposure to use game settings (auto eye adaptation).
	 * This is the "Game Settings" checkbox in the viewport menu.
	 *
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetExposureGameSettings();

	// =================================================================
	// Game View & Cinematic
	// =================================================================

	/**
	 * Toggle Game View mode on/off.
	 * Game View hides all editor visualizations (wireframes, icons, etc.)
	 * showing only what the player would see in-game.
	 *
	 * @param bEnable - True to enable Game View, false to disable
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetGameView(bool bEnable);

	/**
	 * Set whether the viewport allows cinematic control.
	 * When enabled, Sequencer cinematics can take over the viewport camera.
	 *
	 * @param bAllow - True to allow cinematic control
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetAllowCinematicControl(bool bAllow);

	// =================================================================
	// Realtime Rendering
	// =================================================================

	/**
	 * Set whether the viewport renders in realtime.
	 *
	 * @param bRealtime - True for realtime rendering, false for on-demand
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetRealtime(bool bRealtime);

	// =================================================================
	// Camera Position & Speed
	// =================================================================

	/**
	 * Set the viewport camera world location.
	 *
	 * @param NewLocation - World space location
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetCameraLocation(FVector NewLocation);

	/**
	 * Set the viewport camera world rotation.
	 *
	 * @param NewRotation - World space rotation
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetCameraRotation(FRotator NewRotation);

	/**
	 * Set the camera movement speed index.
	 *
	 * @param SpeedSetting - Speed index from 1 (slowest) to 8 (fastest)
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetCameraSpeed(int32 SpeedSetting);

	// =================================================================
	// Viewport Layout
	// =================================================================

	/**
	 * Set the viewport layout configuration (single pane, quad view, etc.).
	 *
	 * Valid layout names:
	 * - "OnePane"           - Single viewport (default)
	 * - "TwoPanesHoriz"     - Two viewports side by side
	 * - "TwoPanesVert"      - Two viewports stacked
	 * - "ThreePanesLeft"    - Three panes, large left
	 * - "ThreePanesRight"   - Three panes, large right
	 * - "ThreePanesTop"     - Three panes, large top
	 * - "ThreePanesBottom"  - Three panes, large bottom
	 * - "FourPanesLeft"     - Four panes, large left
	 * - "FourPanesRight"    - Four panes, large right
	 * - "FourPanesTop"      - Four panes, large top
	 * - "FourPanesBottom"   - Four panes, large bottom
	 * - "FourPanes2x2"      - Quad view (2x2 grid)
	 *
	 * @param LayoutName - Layout configuration name
	 * @return True if layout was changed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static bool SetViewportLayout(const FString& LayoutName);

	/**
	 * Get the current viewport layout name.
	 *
	 * @return Layout name (e.g., "OnePane", "FourPanes2x2")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Viewport")
	static FString GetViewportLayout();

private:
	/** Helper: get the active FLevelEditorViewportClient, or nullptr */
	static class FLevelEditorViewportClient* GetActiveViewportClient();

	/** Helper: get the active SLevelViewport widget, or nullptr */
	static TSharedPtr<class SLevelViewport> GetActiveLevelViewport();

	/** Convert ELevelViewportType to string */
	static FString ViewportTypeToString(int32 ViewportType);

	/** Convert string to ELevelViewportType, returns -1 on invalid input */
	static int32 StringToViewportType(const FString& TypeStr);

	/** Force the viewport to visually redraw (wakes Slate widget in non-realtime mode) */
	static void ForceViewportRedraw();
};
