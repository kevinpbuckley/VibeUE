// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
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
 * - near_clip_plane (float): Near clipping plane distance. Set -1 to restore the engine
 *     default; note the read-back then reports the *resolved* engine value (e.g. 10.0), not -1.
 * - far_clip_plane (float): Far clipping plane distance (0 = infinity)
 * - is_realtime (bool): Whether the viewport renders in realtime
 * - is_game_view (bool): Whether Game View mode is active (hides editor icons)
 * - allow_cinematic_control (bool): Whether cinematic sequences can control this viewport
 * - exposure_fixed (bool): Whether exposure is fixed (true) or auto/game-settings (false)
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
 * Result of ViewportService.capture_scene() — a scene-capture screenshot written to a PNG.
 *
 * Python access:
 *   res = unreal.ViewportService.capture_scene(location, rotation, 1024, 1024, "C:/tmp/shot.png")
 *   if res.b_success: print(res.output_path, res.file_size_bytes)
 *
 * Properties:
 * - b_success (bool): True if the PNG was written.
 * - output_path (str): Absolute path of the PNG that was written (empty on failure).
 * - width / height (int): Pixel dimensions of the captured image.
 * - file_size_bytes (int): Size of the written PNG on disk (0 on failure).
 * - error_message (str): Reason on failure (empty on success).
 */
USTRUCT(BlueprintType)
struct FSceneCaptureResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	FString OutputPath;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	int32 Width = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	int32 Height = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	int64 FileSizeBytes = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	FString ErrorMessage;
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
 *   # Camera position/rotation is engine-side (this service is read-only for pose):
 *   #   read:  ViewportService.get_viewport_info().location / .rotation
 *   #   write: EditorAppToolset.SetCameraTransform via call_tool, or
 *   #          unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
 *   #              .set_level_viewport_camera_info(location, rotation)
 *
 * @note ViewportService owns view type, view mode, FOV, clip planes, exposure, game view,
 *       cinematic control, realtime, camera speed, and layout. It does NOT set camera
 *       position/rotation (no set_camera_location / set_camera_rotation) — use the engine
 *       path above. (issue #471)
 */
UCLASS(BlueprintType)
class VIBEUE_API UViewportService : public UToolsetDefinition
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static bool SetViewportType(const FString& ViewType);

	/**
	 * Get the current viewport view type as a string.
	 *
	 * @return One of: "perspective", "top", "bottom", "left", "right", "front", "back", "ortho_freelook"
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static bool SetViewMode(const FString& ViewMode);

	/**
	 * Get the current viewport rendering mode.
	 *
	 * @return Current view mode as string (e.g., "lit", "wireframe", "unlit")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static bool SetFOV(float FOVDegrees);

	/**
	 * Get the current viewport horizontal field of view.
	 *
	 * @return FOV in degrees
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static bool SetNearClipPlane(float Distance);

	/**
	 * Set the far clipping plane distance.
	 *
	 * @param Distance - Far clip distance in Unreal units. Use 0 for infinity (default).
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static bool SetExposure(bool bFixed, float EV100 = 1.0f);

	/**
	 * Set exposure to use game settings (auto eye adaptation).
	 * This is the "Game Settings" checkbox in the viewport menu.
	 *
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static bool SetGameView(bool bEnable);

	/**
	 * Set whether the viewport allows cinematic control.
	 * When enabled, Sequencer cinematics can take over the viewport camera.
	 *
	 * @param bAllow - True to allow cinematic control
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static bool SetRealtime(bool bRealtime);

	// =================================================================
	// Camera Speed
	// =================================================================

	/**
	 * Set the camera movement speed index.
	 *
	 * @param SpeedSetting - Speed index from 1 (slowest) to 8 (fastest)
	 * @return True if set successfully
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
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
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static bool SetViewportLayout(const FString& LayoutName);

	/**
	 * Get the current viewport layout name.
	 *
	 * @return Layout name (e.g., "OnePane", "FourPanes2x2")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static FString GetViewportLayout();

	// =================================================================
	// Scene Capture (works while the editor is backgrounded)
	// =================================================================

	/**
	 * Capture the editor world from an arbitrary camera to a PNG, synchronously.
	 *
	 * Unlike CaptureViewport / CaptureEditorImage, this does NOT depend on the level
	 * viewport pumping frames: it spawns a transient ASceneCapture2D, renders one frame
	 * with CaptureScene(), reads the pixels back and writes the PNG in-call. It therefore
	 * works when the editor is minimised or backgrounded (the case CaptureViewport returns
	 * a stale frame for), and is the reliable path for scripted minimaps and top-down maps.
	 *
	 * Facts baked in (measured for this workflow — see the viewport skill):
	 * - CaptureSource is SCS_FinalColorLDR, which yields alpha 255. SCS_BaseColor writes
	 *   alpha 0, producing PNGs that render as a blank white page in most viewers.
	 * - The render target is RTF_RGBA8. The default float format (RTF_RGBA16f) writes
	 *   non-PNG bytes. The exported pixels are forced opaque (A=255) regardless.
	 * - A backgrounded editor has no converged auto-exposure, so an auto-exposed capture
	 *   comes out black. Pass ExposureBias != 0 to force a FIXED manual exposure decoupled from
	 *   the physical camera; the final exposure scale is then pow(2, ExposureBias), so positive
	 *   brightens (+1 EV = 2x) and negative darkens. Small values are usually right — start near
	 *   0 and raise a stop at a time. (Leaving the physical camera coupled, the engine default for
	 *   Manual, uses a ~9.9-stop white point that crushes the shot and inverts the bias response.)
	 *
	 * @param Location - World location of the capture camera.
	 * @param Rotation - World rotation of the capture camera. For a north-up top-down
	 *        minimap, use an orthographic capture (OrthoWidth > 0) with pitch=-90, yaw=-90,
	 *        roll=0.
	 * @param Width - Output width in pixels (1..8192).
	 * @param Height - Output height in pixels (1..8192).
	 * @param OutputPngPath - Where to write the PNG. Absolute paths are used as-is; a
	 *        relative path lands under <Project>/Saved/VibeUE/Captures. A missing ".png"
	 *        extension is appended.
	 * @param OrthoWidth - 0 (default) = perspective projection using FOV. > 0 = orthographic
	 *        projection with this world-space width (for a minimap, pass the map size in uu).
	 * @param FOV - Horizontal field of view in degrees for perspective mode (ignored when
	 *        OrthoWidth > 0). Default 90.
	 * @param ExposureBias - 0 (default) = keep the engine's automatic exposure (correct for a
	 *        foreground/PIE window). Non-zero = force a FIXED manual exposure decoupled from the
	 *        physical camera, where this value is a clean EV offset (exposure scale = pow(2,bias)):
	 *        positive brightens, negative darkens. Use it for a backgrounded editor, which captures
	 *        black on auto; small values are usually right (start near 0 and raise a stop at a time).
	 * @return FSceneCaptureResult with bSuccess, OutputPath, Width/Height, FileSizeBytes, ErrorMessage.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static FSceneCaptureResult CaptureScene(
		FVector Location,
		FRotator Rotation,
		int32 Width,
		int32 Height,
		const FString& OutputPngPath,
		float OrthoWidth = 0.0f,
		float FOV = 90.0f,
		float ExposureBias = 0.0f);

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
