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
 * - is_realtime (bool): Whether the viewport renders in realtime right now, INCLUDING temporary
 *     overrides (e.g. the "Background Process" override a backgrounded editor puts on every viewport)
 * - realtime_base (bool): The user's own realtime setting, without overrides (the value the editor
 *     saves to its ini). restore_viewport_info() compares and writes THIS one, never is_realtime.
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

	/** The base realtime flag without overrides (FEditorViewportClient::SaveRealtimeStateToConfig). */
	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	bool bRealtimeBase = true;
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
 * - view_applied (bool): capture_with_view only: True once the temporary view was applied to the
 *     level viewport (and therefore restored afterwards); False when it refused before touching it.
 *     Always False from capture_scene.
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

	UPROPERTY(BlueprintReadWrite, Category = "Viewport")
	bool bViewApplied = false;
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
 *   # WARNING: set_fov / set_exposure PERSIST. The editor saves the level viewport's FOV and
 *   # exposure to EditorPerProjectUserSettings.ini, so a script that changes them and exits
 *   # leaves the user's viewport that way on every later launch (a fixed EV100 on a dim map
 *   # renders the Lit viewport black). Snapshot first and restore afterwards:
 *   info = unreal.ViewportService.get_viewport_info()
 *   unreal.ViewportService.set_exposure(True, 12.0)
 *   unreal.ViewportService.restore_viewport_info(info)
 *
 *   # Or let one call do apply -> capture -> restore. It restores on every ordinary failure or
 *   # early return; only a crash caught by the Python runner's SEH guard (an access violation
 *   # inside the draw) skips the restore. The snapshot is logged at Display first, so the
 *   # values can be put back by hand:
 *   res = unreal.ViewportService.capture_with_view(loc, rot, 1280, 720, "shot.png", 60.0, True, 10.0)
 *
 *   # Main editor window (a minimized window freezes PIE and makes capture_image refuse):
 *   print(unreal.ViewportService.get_editor_window_state())
 *   unreal.ViewportService.set_editor_window_state("restore")
 *
 * @note ViewportService owns view type, view mode, FOV, clip planes, exposure, game view,
 *       cinematic control, realtime, camera speed, and layout. It does NOT set camera
 *       position/rotation (no set_camera_location / set_camera_rotation) — use the engine
 *       path above. (issue #471) The exceptions are restore_viewport_info(), which puts the
 *       camera pose back as part of restoring a get_viewport_info() snapshot, and
 *       capture_with_view(), which moves the camera only for the duration of its shot.
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

	/**
	 * Put the level viewport back to a snapshot taken by GetViewportInfo(), in one call.
	 *
	 * Restores: camera location and rotation, FOV, exposure (fixed + EV100, or game settings
	 * when the snapshot's exposure_fixed is false), the BASE realtime setting (realtime_base, not
	 * is_realtime, which includes temporary overrides such as a backgrounded editor's), and game
	 * view. Each field is compared first and written only when it differs, so restoring an
	 * unchanged snapshot writes nothing. View type, view mode, clip planes, cinematic control,
	 * layout and camera speed are NOT restored. When the snapshot's viewport_type differs from
	 * the current view type, the camera pose is skipped (with a Warning) rather than written into
	 * the other view type's transform; switch the type back first to restore the pose too.
	 *
	 * Why it exists: set_fov / set_exposure PERSIST. The editor saves the level viewport's FOV
	 * and exposure into EditorPerProjectUserSettings.ini, so a script that changes them and
	 * exits leaves the user's viewport that way on every later launch.
	 *
	 * Python:
	 *   info = unreal.ViewportService.get_viewport_info()
	 *   # ... change FOV / exposure / camera ...
	 *   unreal.ViewportService.restore_viewport_info(info)
	 *
	 * @param Info - A snapshot from GetViewportInfo().
	 * @return True if a level viewport client was found and the snapshot applied; false
	 *         (with a Warning log) when there is no level viewport.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static bool RestoreViewportInfo(const FViewportInfo& Info);

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
	 *   comes out black. Pass ManualEV100 != 0 to force a FIXED manual exposure decoupled from the
	 *   physical camera, where the value is the manual exposure TARGET in EV100 — exactly like a
	 *   camera: a HIGHER EV100 assumes a brighter scene and stops down, so the image gets DARKER;
	 *   a lower (negative) EV100 brightens. Measured on a daylit scene from a backgrounded editor:
	 *   mean luminance 0->16.4 (auto), +4->4.3, -4->43.8, -8->86.0. A daylit backgrounded scene
	 *   reads well around -6 to -8; try -4 first for bright scenes.
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
	 * @param ManualEV100 - 0 (default) = keep the engine's automatic exposure (correct for a
	 *        foreground/PIE window). Non-zero = force a FIXED manual exposure decoupled from the
	 *        physical camera, where this value is the exposure TARGET in EV100: HIGHER is DARKER,
	 *        lower/negative is brighter (like a camera's metered EV). Use it for a backgrounded
	 *        editor, which captures black on auto; a daylit scene reads well around -6 to -8, and
	 *        -4 is a good first try for bright scenes.
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
		float ManualEV100 = 0.0f);

	/**
	 * Capture the REAL level viewport from a given camera, then restore the viewport.
	 *
	 * Snapshots GetViewportInfo(), applies the camera pose, the optional FOV and the optional
	 * fixed exposure to the active level viewport, draws that viewport once (FViewport::Draw +
	 * ReadPixels, the same path capture_image source='editor' uses) and writes a PNG. Then it
	 * restores everything from the snapshot. The restore is scoped, so an ordinary failure or an
	 * early return restores too. Caveat: an access violation inside the draw is caught by the
	 * Python runner's SEH guard, and SEH unwinding runs no destructors, so then the shot's view
	 * stays in the session (and is saved at the next layout save or exit). The snapshot's pose,
	 * FOV and exposure are logged at Display ("CaptureWithView: snapshot ...") before anything is
	 * applied, so they can be restored by hand with restore_viewport_info / set_fov / set_exposure.
	 *
	 * Unlike CaptureScene (an off-screen SceneCapture2D), this renders the level viewport
	 * itself, so its view mode, show flags, game view and exposure apply. Refused (nothing
	 * applied) during PIE, in a non-perspective view, while the viewport pilots an actor or is
	 * locked to a cinematic camera (the camera would move that actor, and its FOV would win),
	 * while the main editor window is minimized (see SetEditorWindowState("restore")), and when
	 * the viewport has no drawable surface. A backgrounded editor has no converged auto
	 * exposure, so pass bFixedExposure=True for a dependable brightness.
	 *
	 * @param Location - Camera world location for the shot.
	 * @param Rotation - Camera world rotation for the shot (in Python use keyword args:
	 *        unreal.Rotator(roll=0, pitch=-20, yaw=90)).
	 * @param Width - Output width in pixels (1..8192). The viewport draws at its own size and the
	 *        pixels are resized to Width x Height; an aspect ratio unlike the viewport's stretches.
	 * @param Height - Output height in pixels (1..8192).
	 * @param OutputPngPath - Where to write the PNG. Absolute paths are used as-is; a relative
	 *        path lands under <Project>/Saved/VibeUE/Captures. A missing ".png" is appended.
	 * @param FOV - Horizontal FOV in degrees for the shot (clamped to 5..170); 0 (default) keeps
	 *        the viewport's current FOV.
	 * @param bFixedExposure - True = fixed exposure at EV100 for the shot; false (default) keeps
	 *        the viewport's current exposure setting.
	 * @param EV100 - Fixed EV100 for the shot when bFixedExposure is true (higher = darker).
	 * @return FSceneCaptureResult with bSuccess, OutputPath, Width/Height, FileSizeBytes, ErrorMessage.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static FSceneCaptureResult CaptureWithView(
		FVector Location,
		FRotator Rotation,
		int32 Width,
		int32 Height,
		const FString& OutputPngPath,
		float FOV = 0.0f,
		bool bFixedExposure = false,
		float EV100 = 0.0f);

	/**
	 * C++ only (not a UFUNCTION, not reachable from Python): the apply-and-restore core of CaptureWithView.
	 * Snapshots the active level viewport (logged at Display), applies the pose, the optional FOV (> 0) and
	 * the optional fixed exposure, runs Body, and restores the snapshot when the scope ends, Body's early
	 * returns included. It does NONE of CaptureWithView's refusal checks (PIE, view type, actor lock,
	 * minimized window, surface size). Tests drive it to exercise the restore path without an RHI.
	 *
	 * @return False, with Body not run and nothing touched, when there is no level viewport client.
	 */
	static bool RunWithTemporaryView(
		FVector Location,
		FRotator Rotation,
		float FOV,
		bool bFixedExposure,
		float EV100,
		TFunctionRef<void()> Body);

	// =================================================================
	// Main editor window state
	// =================================================================

	/**
	 * Read the main editor window's state.
	 *
	 * A minimized editor window freezes PIE (world time stays 0) and makes capture_image
	 * refuse with "window is minimized"; SetEditorWindowState("restore") fixes that.
	 *
	 * @return JSON {"success": true, "is_minimized": bool, "is_maximized": bool,
	 *         "is_foreground": bool, "is_visible": bool, "title": str}, or
	 *         {"success": false, "error_code": "NO_WINDOW", "error_message": str} when there is no
	 *         main editor window (headless / -nullrhi runs).
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static FString GetEditorWindowState();

	/**
	 * Change the main editor window's state.
	 *
	 * "restore" un-minimizes; a window that is not minimized is left alone (a no-op that
	 * succeeds), so a maximized window stays maximized. "front" un-minimizes if needed and then
	 * asks the OS for the foreground; Windows may refuse that for a background process, and then
	 * success is false and is_foreground says so. "minimize" and "maximize" do what they say.
	 *
	 * @param State - "restore" | "front" | "minimize" | "maximize" (case-insensitive).
	 * @return JSON {"success": bool, "state": str, "was_minimized": bool, "is_minimized": bool,
	 *         "is_maximized": bool, "is_foreground": bool, "is_visible": bool, "title": str};
	 *         success is true when the requested state holds afterwards. Failures add
	 *         "error_code" ("NO_WINDOW", "BAD_STATE" or "STATE_NOT_REACHED") and "error_message".
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Viewport")
	static FString SetEditorWindowState(const FString& State);

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
