// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
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
 * Information about an actor in the level
 */
USTRUCT(BlueprintType)
struct FLevelActorInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	FString ActorName;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	FString ActorLabel;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	FString ActorClass;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	FVector Scale = FVector::OneVector;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	FString FolderPath;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	bool bIsHidden = false;

	UPROPERTY(BlueprintReadWrite, Category = "Actor")
	bool bIsSelected = false;
};

/**
 * Transform information for an actor
 */
USTRUCT(BlueprintType)
struct FActorTransformData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Transform")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Transform")
	FRotator WorldRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Transform")
	FVector WorldScale = FVector::OneVector;

	UPROPERTY(BlueprintReadWrite, Category = "Transform")
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Transform")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, Category = "Transform")
	FVector RelativeScale = FVector::OneVector;

	UPROPERTY(BlueprintReadWrite, Category = "Transform")
	FVector Forward = FVector::ForwardVector;

	UPROPERTY(BlueprintReadWrite, Category = "Transform")
	FVector Right = FVector::RightVector;

	UPROPERTY(BlueprintReadWrite, Category = "Transform")
	FVector Up = FVector::UpVector;

	UPROPERTY(BlueprintReadWrite, Category = "Transform")
	FVector BoundsOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Transform")
	FVector BoundsExtent = FVector::ZeroVector;
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
 * Actor service exposed directly to Python for level actor management.
 *
 * Python Usage:
 *   import unreal
 *   actor_service = unreal.ActorService
 *
 *   # Discovery
 *   actors = actor_service.list_level_actors()
 *   lights = actor_service.find_actors_by_class("PointLight")
 *
 *   # Lifecycle
 *   actor_service.add_actor("PointLight", (0, 0, 100))
 *   actor_service.remove_actor("MyLight")
 *
 *   # Transform
 *   actor_service.set_location("MyActor", (100, 200, 300))
 *   actor_service.set_rotation("MyActor", (0, 90, 0))
 *   transform = actor_service.get_transform("MyActor")
 *
 *   # Properties
 *   value = actor_service.get_property("MyLight", "LightComponent.Intensity")
 *   actor_service.set_property("MyLight", "LightComponent.Intensity", "5000")
 *
 *   # Organization
 *   actor_service.set_folder("MyActor", "Level/Props")
 *   actor_service.rename_actor("OldName", "NewName")
 *
 *   # Viewport
 *   actor_service.focus_actor("MyActor")
 *   actor_service.refresh_viewport()
 *
 * @note All 21 level actor operations available via Python
 */
UCLASS(BlueprintType)
class VIBEUE_API UActorService : public UObject
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════
	// Discovery Operations
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * List all actors in the current level.
	 *
	 * @param ActorClassFilter - Optional filter by actor class name (supports wildcards with *)
	 * @param bIncludeHidden - Whether to include hidden actors
	 * @param MaxResults - Maximum number of results to return (default 100)
	 * @return Array of actor information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static TArray<FLevelActorInfo> ListLevelActors(
		const FString& ActorClassFilter = TEXT(""),
		bool bIncludeHidden = false,
		int32 MaxResults = 100);

	/**
	 * Find actors by class name.
	 *
	 * @param ClassName - Actor class name to search for
	 * @return Array of matching actors
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static TArray<FLevelActorInfo> FindActorsByClass(const FString& ClassName);

	/**
	 * Get detailed information about an actor by name or label.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param OutInfo - Structure containing actor details
	 * @return True if actor found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool GetActorInfo(const FString& ActorNameOrLabel, FLevelActorInfo& OutInfo);

	// ═══════════════════════════════════════════════════════════════════
	// Lifecycle Operations
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Add a new actor to the level.
	 *
	 * @param ActorClass - Class name to spawn (e.g., "PointLight", "StaticMeshActor", "BP_Player")
	 * @param Location - World location to spawn at (if zero, spawns in front of viewport)
	 * @param Rotation - World rotation
	 * @param Scale - Actor scale (default 1,1,1)
	 * @param ActorLabel - Optional display label for the actor
	 * @return Info of spawned actor, or empty struct if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static FLevelActorInfo AddActor(
		const FString& ActorClass,
		FVector Location,
		FRotator Rotation,
		FVector Scale,
		const FString& ActorLabel = TEXT(""));

	/**
	 * Remove an actor from the level.
	 *
	 * @param ActorNameOrLabel - Name or label of actor to remove
	 * @return True if actor was removed
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool RemoveActor(const FString& ActorNameOrLabel);

	// ═══════════════════════════════════════════════════════════════════
	// Transform Operations
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Get comprehensive transform information for an actor.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param OutTransform - Transform data including world/relative transforms and bounds
	 * @return True if actor found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool GetTransform(const FString& ActorNameOrLabel, FActorTransformData& OutTransform);

	/**
	 * Set complete transform (location, rotation, scale).
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param Location - New world location
	 * @param Rotation - New world rotation
	 * @param Scale - New scale
	 * @return True if transform was set
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool SetTransform(
		const FString& ActorNameOrLabel,
		FVector Location,
		FRotator Rotation,
		FVector Scale);

	/**
	 * Set actor location only.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param Location - New world location
	 * @param bSweep - Whether to check for collision during move
	 * @return True if location was set
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool SetLocation(
		const FString& ActorNameOrLabel,
		FVector Location,
		bool bSweep = false);

	/**
	 * Set actor rotation only.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param Rotation - New world rotation
	 * @return True if rotation was set
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool SetRotation(const FString& ActorNameOrLabel, FRotator Rotation);

	/**
	 * Set actor scale only.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param Scale - New scale
	 * @return True if scale was set
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool SetScale(const FString& ActorNameOrLabel, FVector Scale);

	// ═══════════════════════════════════════════════════════════════════
	// Viewport Operations
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Focus the viewport camera on an actor.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor to focus on
	 * @param bInstant - If true, jump instantly; if false, smooth transition
	 * @return True if focus succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool FocusActor(const FString& ActorNameOrLabel, bool bInstant = false);

	/**
	 * Move an actor to the center of the current viewport.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor to move
	 * @return True if move succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool MoveActorToView(const FString& ActorNameOrLabel);

	/**
	 * Force refresh of all level editing viewports.
	 * Call after making visual changes to ensure they're displayed.
	 *
	 * @return True if refresh succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool RefreshViewport();

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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
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
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static FCameraViewInfo CalculateActorView(
		const FString& ActorNameOrLabel,
		EViewDirection Direction = EViewDirection::Front,
		float PaddingMultiplier = 1.2f);

	// ═══════════════════════════════════════════════════════════════════
	// Property Operations
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Get a property value from an actor or its component.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param PropertyPath - Property path (e.g., "Intensity" or "LightComponent.Intensity")
	 * @param OutValue - The property value as string
	 * @return True if property found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool GetProperty(
		const FString& ActorNameOrLabel,
		const FString& PropertyPath,
		FString& OutValue);

	/**
	 * Set a property value on an actor or its component.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param PropertyPath - Property path (e.g., "Intensity" or "LightComponent.Intensity")
	 * @param Value - The value to set (as string, e.g., "5000" or "(R=255,G=0,B=0,A=255)")
	 * @return True if property was set
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool SetProperty(
		const FString& ActorNameOrLabel,
		const FString& PropertyPath,
		const FString& Value);

	/**
	 * Get all properties from an actor.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param ComponentName - Optional: target a specific component
	 * @param CategoryFilter - Optional: filter by property category
	 * @return Array of property information
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static TArray<FActorPropertyData> GetAllProperties(
		const FString& ActorNameOrLabel,
		const FString& ComponentName = TEXT(""),
		const FString& CategoryFilter = TEXT(""));

	// ═══════════════════════════════════════════════════════════════════
	// Organization Operations
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Set the folder path for an actor in the World Outliner.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor
	 * @param FolderPath - Folder path (e.g., "Level/Props" or "Characters/NPCs")
	 * @return True if folder was set
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool SetFolder(const FString& ActorNameOrLabel, const FString& FolderPath);

	/**
	 * Rename an actor's display label.
	 *
	 * @param ActorNameOrLabel - Current name or label of the actor
	 * @param NewLabel - New display label
	 * @return True if rename succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool RenameActor(const FString& ActorNameOrLabel, const FString& NewLabel);

	// ═══════════════════════════════════════════════════════════════════
	// Hierarchy Operations
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Attach one actor to another.
	 *
	 * @param ChildNameOrLabel - Name or label of the child actor to attach
	 * @param ParentNameOrLabel - Name or label of the parent actor
	 * @param SocketName - Optional socket name to attach to
	 * @return True if attach succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool AttachActor(
		const FString& ChildNameOrLabel,
		const FString& ParentNameOrLabel,
		const FString& SocketName = TEXT(""));

	/**
	 * Detach an actor from its parent.
	 *
	 * @param ActorNameOrLabel - Name or label of the actor to detach
	 * @return True if detach succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool DetachActor(const FString& ActorNameOrLabel);

	// ═══════════════════════════════════════════════════════════════════
	// Selection Operations
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Select actor(s) in the editor.
	 *
	 * @param ActorNameOrLabel - Name or label of actor to select (or comma-separated list)
	 * @param bAddToSelection - If true, add to current selection; if false, replace selection
	 * @return True if select succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool SelectActor(const FString& ActorNameOrLabel, bool bAddToSelection = false);

	/**
	 * Deselect all actors in the editor.
	 *
	 * @return True if deselect succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors")
	static bool DeselectAll();

	// ═══════════════════════════════════════════════════════════════════
	// Existence Checks
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Check if an actor with the given label exists in the current level.
	 *
	 * @param ActorLabel - Display label of the actor
	 * @return True if actor exists
	 *
	 * Example:
	 *   if not unreal.ActorService.actor_exists("TargetDummy_01"):
	 *       unreal.ActorService.add_actor("StaticMeshActor", (100, 200, 0), (0, 0, 0), (1, 1, 1), "TargetDummy_01")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors|Exists")
	static bool ActorExists(const FString& ActorLabel);

	/**
	 * Check if any actor with the given tag exists in the current level.
	 *
	 * @param Tag - Actor tag to search for
	 * @return True if any actor with this tag exists
	 *
	 * Example:
	 *   if not unreal.ActorService.actor_exists_by_tag("Enemy"):
	 *       # Spawn enemies
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Actors|Exists")
	static bool ActorExistsByTag(const FString& Tag);

private:
	/** Helper: Find actor by name or label */
	static AActor* FindActorByIdentifier(const FString& NameOrLabel);

	/** Helper: Get the current editor world */
	static UWorld* GetEditorWorld();

	/** Helper: Populate FLevelActorInfo from an actor */
	static void PopulateActorInfo(AActor* Actor, FLevelActorInfo& OutInfo);

	/** Helper: Find actor class by name */
	static UClass* FindActorClass(const FString& ClassName);

	/** Helper: Get property value as string */
	static FString GetPropertyValueAsString(UObject* Object, FProperty* Property);

	/** Helper: Begin undo transaction */
	static void BeginTransaction(const FText& Description);

	/** Helper: End undo transaction */
	static void EndTransaction();

	/** Helper: Calculate camera position and rotation to frame an actor from a direction */
	static FCameraViewInfo CalculateViewForActor(AActor* Actor, EViewDirection Direction, float PaddingMultiplier);

	/** Helper: Get the active perspective viewport client */
	static FLevelEditorViewportClient* GetPerspectiveViewportClient();
};
