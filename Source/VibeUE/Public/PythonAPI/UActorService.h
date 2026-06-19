// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UActorService.generated.h"

/**
 * View direction for camera positioning relative to an actor.
 * Used with GetActorViewCamera to calculate camera position that frames an actor.
 */
UENUM(BlueprintType)
enum class EViewDirection : uint8
{
	/** Camera looks down from above (+Z looking -Z) */
	Top		UMETA(DisplayName = "Top"),
	/** Camera looks up from below (-Z looking +Z) */
	Bottom	UMETA(DisplayName = "Bottom"),
	/** Camera looks from the left (-Y looking +Y) */
	Left	UMETA(DisplayName = "Left"),
	/** Camera looks from the right (+Y looking -Y) */
	Right	UMETA(DisplayName = "Right"),
	/** Camera looks from the front (+X looking -X) */
	Front	UMETA(DisplayName = "Front"),
	/** Camera looks from the back (-X looking +X) */
	Back	UMETA(DisplayName = "Back")
};

/**
 * Camera view information for positioning the viewport to frame an actor.
 */
USTRUCT(BlueprintType)
struct FCameraViewInfo
{
	GENERATED_BODY()

	/** Whether the view calculation succeeded */
	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	bool bSuccess = false;

	/** Calculated camera location */
	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	FVector CameraLocation = FVector::ZeroVector;

	/** Calculated camera rotation (looking at the actor) */
	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	FRotator CameraRotation = FRotator::ZeroRotator;

	/** The view direction used */
	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	EViewDirection ViewDirection = EViewDirection::Front;

	/** Actor bounds center that was framed */
	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	FVector ActorCenter = FVector::ZeroVector;

	/** Actor bounds extent */
	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	FVector ActorExtent = FVector::ZeroVector;

	/** Distance from camera to actor center */
	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	float ViewDistance = 0.0f;
};

/**
 * Property information for an actor
 */
USTRUCT(BlueprintType)
struct FActorPropertyData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Property")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "Property")
	FString Value;

	UPROPERTY(BlueprintReadWrite, Category = "Property")
	FString Type;

	UPROPERTY(BlueprintReadWrite, Category = "Property")
	FString Category;

	UPROPERTY(BlueprintReadWrite, Category = "Property")
	bool bIsEditable = true;
};

/**
 * Actor service exposed directly to Python.
 *
 * Trimmed to the operations Epic's native toolsets (ActorTools, SceneTools,
 * EditorAppToolset) do NOT provide: transform lock/constraints, absolute-transform
 * flags, preserve-scale-ratio, viewport camera framing, and full property dumps.
 *
 * Python Usage:
 *   import unreal
 *   actor_service = unreal.ActorService
 *
 *   # Transform Locking
 *   actor_service.set_actor_lock_location("MyActor", True)   # Lock location in viewport
 *   actor_service.set_preserve_scale_ratio(True)   # Lock uniform scale (padlock icon)
 *   actor_service.set_absolute_transform("MyActor", False, True, False)  # Absolute rotation
 *
 *   # Camera framing
 *   view = actor_service.get_actor_view_camera("MyActor", unreal.EViewDirection.TOP)
 *
 *   # Properties
 *   props = actor_service.get_all_properties("MyActor")
 */
UCLASS(BlueprintType)
class VIBEUE_API UActorService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════
	// Transform Lock / Constraint Operations
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Lock or unlock an actor's location in the viewport.
	 * When locked, the actor cannot be moved via viewport gizmos (only via code).
	 * This wraps the built-in AActor::bLockLocation property.
	 *
	 * NOTE: UE5 only supports locking LOCATION natively. There is no built-in
	 * per-actor lock for rotation or scale. Use absolute transform flags or
	 * set_property("lock_location", "True") as an alternative.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param bLocked - True to lock, False to unlock
	 * @return True if lock was set successfully
	 *
	 * Example:
	 *   unreal.ActorService.set_actor_lock_location("MyCube", True)   # Lock position
	 *   unreal.ActorService.set_actor_lock_location("MyCube", False)  # Unlock position
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static bool SetActorLockLocation(const FString& ActorNameOrLabel, bool bLocked);

	/**
	 * Check whether an actor's location is locked.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param OutLocked - Whether location is locked
	 * @return True if actor was found
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static bool GetActorLockLocation(const FString& ActorNameOrLabel, bool& OutLocked);

	/**
	 * Set absolute transform flags on an actor's root component.
	 * When absolute is set, that transform channel is world-space rather than
	 * relative to the parent. This is useful when attaching actors but needing
	 * independent positioning.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param bAbsoluteLocation - True = location is world-space, not relative to parent
	 * @param bAbsoluteRotation - True = rotation is world-space, not relative to parent
	 * @param bAbsoluteScale - True = scale is world-space, not relative to parent
	 * @return True if flags were set
	 *
	 * Example:
	 *   # Make rotation absolute (world-space) while keeping location relative
	 *   unreal.ActorService.set_absolute_transform("MyCube", False, True, False)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static bool SetAbsoluteTransform(
		const FString& ActorNameOrLabel,
		bool bAbsoluteLocation,
		bool bAbsoluteRotation,
		bool bAbsoluteScale);

	/**
	 * Get absolute transform flags from an actor's root component.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param OutAbsoluteLocation - Whether location is absolute
	 * @param OutAbsoluteRotation - Whether rotation is absolute
	 * @param OutAbsoluteScale - Whether scale is absolute
	 * @return True if actor was found
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static bool GetAbsoluteTransform(
		const FString& ActorNameOrLabel,
		bool& OutAbsoluteLocation,
		bool& OutAbsoluteRotation,
		bool& OutAbsoluteScale);

	/**
	 * Set the Preserve Scale Ratio (uniform scale padlock) in the editor.
	 * When enabled, scaling any axis in the Details panel scales all axes
	 * proportionally, maintaining the object's proportions.
	 * This is the padlock icon next to Scale in the Transform section.
	 *
	 * NOTE: This is a global editor preference, not per-actor.
	 * It affects ALL actors when scaling via the Details panel.
	 *
	 * @param bPreserve - True to lock (uniform scale), False to unlock (independent axes)
	 * @return True if setting was applied
	 *
	 * Example:
	 *   unreal.ActorService.set_preserve_scale_ratio(True)   # Lock scale axes together
	 *   unreal.ActorService.set_preserve_scale_ratio(False)  # Allow independent axis scaling
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static bool SetPreserveScaleRatio(bool bPreserve);

	/**
	 * Get the current Preserve Scale Ratio (uniform scale padlock) state.
	 *
	 * @return True if scale axes are locked together (uniform scaling)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static bool GetPreserveScaleRatio();

	// ═══════════════════════════════════════════════════════════════════
	// Camera Framing Operations
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Set the viewport camera to a specific location and rotation.
	 * Directly positions the editor viewport camera.
	 *
	 * @param Location - World location for the camera
	 * @param Rotation - World rotation for the camera (Pitch, Yaw, Roll)
	 * @return True if camera was positioned successfully
	 *
	 * Example:
	 *   # Position camera at (1000, 2000, 500) looking down at 45 degrees
	 *   unreal.ActorService.set_viewport_camera(unreal.Vector(1000, 2000, 500), unreal.Rotator(-45, 0, 0))
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static bool SetViewportCamera(FVector Location, FRotator Rotation);

	/**
	 * Calculate and apply a camera view that frames an actor from a specific direction.
	 * Computes the camera position based on the actor's bounding box so the entire
	 * actor fits in the viewport.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor to frame
	 * @param Direction - View direction (Top, Bottom, Left, Right, Front, Back)
	 * @param PaddingMultiplier - Extra distance multiplier (1.0 = tight fit, 1.5 = 50% padding). Default 1.2
	 * @return Camera view info with the calculated and applied camera position
	 *
	 * Example:
	 *   # Get a top-down view of "MyLandscape"
	 *   view = unreal.ActorService.get_actor_view_camera("MyLandscape", unreal.EViewDirection.TOP)
	 *   # Get a front view with extra padding
	 *   view = unreal.ActorService.get_actor_view_camera("MyBuilding", unreal.EViewDirection.FRONT, 1.5)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static FCameraViewInfo GetActorViewCamera(
		const FString& ActorNameOrLabel,
		EViewDirection Direction = EViewDirection::Front,
		float PaddingMultiplier = 1.2f);

	/**
	 * Calculate camera view info for an actor WITHOUT moving the viewport camera.
	 * Use this to get the camera position/rotation for a specific view direction,
	 * then apply it yourself with SetViewportCamera when ready.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor to frame
	 * @param Direction - View direction (Top, Bottom, Left, Right, Front, Back)
	 * @param PaddingMultiplier - Extra distance multiplier (1.0 = tight fit, 1.5 = 50% padding). Default 1.2
	 * @return Camera view info with the calculated position (camera NOT moved)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static FCameraViewInfo CalculateActorView(
		const FString& ActorNameOrLabel,
		EViewDirection Direction = EViewDirection::Front,
		float PaddingMultiplier = 1.2f);

	// ═══════════════════════════════════════════════════════════════════
	// Property Operations
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Get all properties from an actor.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param ComponentName - Optional: target a specific component
	 * @param CategoryFilter - Optional: filter by property category
	 * @return Array of property information
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static TArray<FActorPropertyData> GetAllProperties(
		const FString& ActorNameOrLabel,
		const FString& ComponentName = TEXT(""),
		const FString& CategoryFilter = TEXT(""));

	/** Find an actor in the current level by name or label (case-insensitive, falls back to contains-match). */
	static AActor* FindActorByIdentifier(const FString& NameOrLabel);

private:
	/** Helper: Get the current editor world */
	static UWorld* GetEditorWorld();

	/** Helper: Get property value as string */
	static FString GetPropertyValueAsString(UObject* Object, FProperty* Property);

	/** Helper: Begin undo transaction */
	static void BeginTransaction(const FText& Description);

	/** Helper: End undo transaction */
	static void EndTransaction();

	/** Helper: Force refresh of all level editing viewports (used after moving the camera) */
	static bool RefreshViewport();

	/** Helper: Calculate camera position and rotation to frame an actor from a direction */
	static FCameraViewInfo CalculateViewForActor(AActor* Actor, EViewDirection Direction, float PaddingMultiplier);

	/** Helper: Get the active perspective viewport client */
	static FLevelEditorViewportClient* GetPerspectiveViewportClient();
};
