// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UAnimSequenceService.generated.h"

/**
 * Comprehensive information about an Animation Sequence asset
 */
USTRUCT(BlueprintType)
struct FAnimSequenceInfo
{
	GENERATED_BODY()

	/** Asset path of the animation */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString AnimPath;

	/** Display name */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString AnimName;

	/** Associated skeleton path */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString SkeletonPath;

	/** Duration in seconds */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float Duration = 0.0f;

	/** Frame rate */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float FrameRate = 30.0f;

	/** Total number of frames */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 FrameCount = 0;

	/** Number of bone tracks */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 BoneTrackCount = 0;

	/** Number of curves */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 CurveCount = 0;

	/** Number of notifies */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 NotifyCount = 0;

	/** Whether root motion is enabled */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bEnableRootMotion = false;

	/** Additive animation type as string */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString AdditiveAnimType;

	/** Rate scale multiplier */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float RateScale = 1.0f;

	/** Compressed size in bytes */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int64 CompressedSize = 0;

	/** Raw size in bytes */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int64 RawSize = 0;
};

/**
 * A single animation keyframe with transform data
 */
USTRUCT(BlueprintType)
struct FAnimKeyframe
{
	GENERATED_BODY()

	/** Frame number */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 Frame = 0;

	/** Time in seconds */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float Time = 0.0f;

	/** Position value */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FVector Position = FVector::ZeroVector;

	/** Rotation value as quaternion */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FQuat Rotation = FQuat::Identity;

	/** Scale value */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FVector Scale = FVector::OneVector;
};

/**
 * Bone pose data at a specific time/frame
 */
USTRUCT(BlueprintType)
struct FBonePose
{
	GENERATED_BODY()

	/** Bone name */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString BoneName;

	/** Bone index */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 BoneIndex = -1;

	/** Transform at this pose */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FTransform Transform;
};

/**
 * Information about an animation curve
 */
USTRUCT(BlueprintType)
struct FAnimCurveInfo
{
	GENERATED_BODY()

	/** Name of the curve */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString CurveName;

	/** Curve type (Float, Vector, Transform) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString CurveType;

	/** Number of keys */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 KeyCount = 0;

	/** Default value */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float DefaultValue = 0.0f;

	/** Whether it drives a morph target */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bMorphTarget = false;

	/** Whether it drives a material parameter */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bMaterial = false;
};

/**
 * A single keyframe in a curve
 */
USTRUCT(BlueprintType)
struct FCurveKeyframe
{
	GENERATED_BODY()

	/** Time in seconds */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float Time = 0.0f;

	/** Value at this key */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float Value = 0.0f;

	/** Interpolation mode (Constant, Linear, Cubic) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString InterpMode;

	/** Tangent mode (Auto, User, Break) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString TangentMode;

	/** Arrive tangent */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float ArriveTangent = 0.0f;

	/** Leave tangent */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float LeaveTangent = 0.0f;
};

/**
 * Information about an animation notify event
 */
USTRUCT(BlueprintType)
struct FAnimNotifyInfo
{
	GENERATED_BODY()

	/** Index of the notify in the sequence */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 NotifyIndex = -1;

	/** Notify name */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString NotifyName;

	/** Class name of the notify */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString NotifyClass;

	/** Trigger time in seconds */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float TriggerTime = 0.0f;

	/** Duration (0 for instant notifies) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float Duration = 0.0f;

	/** Whether this is a state notify */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bIsState = false;

	/** Track index in the notify panel */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 TrackIndex = 0;

	/** Notify color in editor */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FLinearColor NotifyColor = FLinearColor::White;

	/** Trigger chance (0.0-1.0, where 1.0 = always triggers) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float TriggerChance = 1.0f;

	/** Whether notify triggers on dedicated servers */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bTriggerOnServer = true;

	/** Whether notify triggers when animation is a follower in sync group */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bTriggerOnFollower = false;

	/** Minimum blend weight threshold to trigger notify */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float TriggerWeightThreshold = 0.0f;

	/** LOD filter type: "NoFiltering", "LOD", or "BelowLOD" */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString NotifyFilterType;

	/** LOD level to start filtering from */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 NotifyFilterLOD = 0;
};

/**
 * Information about an animation sync marker
 */
USTRUCT(BlueprintType)
struct FSyncMarkerInfo
{
	GENERATED_BODY()

	/** Marker name */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString MarkerName;

	/** Time in seconds */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float Time = 0.0f;

	/** Track index */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 TrackIndex = 0;
};

/**
 * Data for creating a bone track in an animation sequence
 */
USTRUCT(BlueprintType)
struct FBoneTrackData
{
	GENERATED_BODY()

	/** Name of the bone */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString BoneName;

	/** Array of keyframes for this bone */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	TArray<FAnimKeyframe> Keyframes;
};

/**
 * Information about animation compression settings
 */
USTRUCT(BlueprintType)
struct FAnimCompressionInfo
{
	GENERATED_BODY()

	/** Compression scheme name */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString CompressionScheme;

	/** Raw data size in bytes */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int64 RawSize = 0;

	/** Compressed size in bytes */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int64 CompressedSize = 0;

	/** Compression ratio */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float CompressionRatio = 1.0f;

	/** Translation error threshold */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float TranslationErrorThreshold = 0.0f;

	/** Rotation error threshold */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float RotationErrorThreshold = 0.0f;

	/** Scale error threshold */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float ScaleErrorThreshold = 0.0f;
};

/**
 * A single bone delta for preview editing
 */
USTRUCT(BlueprintType)
struct FBoneDelta
{
	GENERATED_BODY()

	/** Name of the bone to modify */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString BoneName;

	/** Rotation delta to apply (Euler degrees) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FRotator RotationDelta = FRotator::ZeroRotator;

	/** Translation delta to apply (optional) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FVector TranslationDelta = FVector::ZeroVector;

	/** Scale delta to apply (multiplicative, default 1,1,1 = no change) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FVector ScaleDelta = FVector::OneVector;
};

/**
 * Result of previewing or applying an animation edit
 */
USTRUCT(BlueprintType)
struct FAnimationEditResult
{
	GENERATED_BODY()

	/** Whether the edit was successful */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bSuccess = false;

	/** List of bone names that were modified */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	TArray<FString> ModifiedBones;

	/** Frame range that was affected (start frame) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 StartFrame = 0;

	/** Frame range that was affected (end frame) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 EndFrame = 0;

	/** Whether any rotations were clamped due to constraints */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bWasClamped = false;

	/** Warnings or informational messages */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	TArray<FString> Messages;

	/** Error message if bSuccess is false */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString ErrorMessage;
};

/**
 * Result of capturing an animation pose to an image file
 */
USTRUCT(BlueprintType)
struct FAnimationPoseCaptureResult
{
	GENERATED_BODY()

	/** Whether the capture was successful */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bSuccess = false;

	/** Full path to the output image file */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString ImagePath;

	/** Animation path that was captured */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString AnimPath;

	/** Time in seconds that was captured */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float CapturedTime = 0.0f;

	/** Frame number that was captured */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 CapturedFrame = 0;

	/** Width of the captured image */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 ImageWidth = 0;

	/** Height of the captured image */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 ImageHeight = 0;

	/** Camera angle used (front, side, back, three_quarter, top) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString CameraAngle;

	/** Error message if bSuccess is false */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString ErrorMessage;
};

/**
 * Result of validating a pose against constraints
 */
USTRUCT(BlueprintType)
struct FPoseValidationResult
{
	GENERATED_BODY()

	/** Whether all bones passed validation */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bIsValid = true;

	/** Number of bones that passed validation */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 PassedCount = 0;

	/** Number of bones that failed validation */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 FailedCount = 0;

	/** List of bone names with constraint violations */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	TArray<FString> ViolatingBones;

	/** Detailed violation messages per bone */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	TArray<FString> ViolationMessages;

	/** Suggested corrections for invalid rotations */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	TArray<FString> Suggestions;
};

/**
 * State of an active animation preview session
 */
USTRUCT(BlueprintType)
struct FAnimationPreviewState
{
	GENERATED_BODY()

	/** Path to the animation being previewed */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString AnimPath;

	/** Whether a preview is currently active */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bIsActive = false;

	/** Number of pending bone edits */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 PendingEditCount = 0;

	/** Bones with pending edits */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	TArray<FString> PendingBones;

	/** Frame being previewed */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 PreviewFrame = 0;
};

/**
 * Animation Sequence service exposed directly to Python.
 *
 * This service provides comprehensive CRUD operations for Animation Sequence assets
 * including keyframe access, curve editing, notify management, sync markers,
 * root motion configuration, and animation data extraction.
 *
 * IMPORTANT: All anim_path parameters require the FULL asset path (package_name from AssetData),
 * NOT the folder path (package_path). For example:
 *   - CORRECT: "/Game/Characters/Mannequin/Animations/Run/AS_Run_Forward"
 *   - WRONG:   "/Game/Characters/Mannequin/Animations/Run" (this is a folder, not an asset)
 *
 * Python Usage:
 *   import unreal
 *
 *   # Search for an animation and get the FULL asset path
 *   results = unreal.AssetDiscoveryService.search_assets("Run", "AnimSequence")
 *   anim_path = str(results[0].package_name)  # Use package_name, NOT package_path!
 *
 *   # List all animations for a skeleton
 *   anims = unreal.AnimSequenceService.find_animations_for_skeleton("/Game/SK_Mannequin")
 *   for anim in anims:
 *       print(f"{anim.anim_name}: {anim.duration}s")
 *
 *   # Get bone pose at time
 *   pose = unreal.AnimSequenceService.get_pose_at_time(anim_path, 0.5, True)
 *   for bone in pose:
 *       print(f"{bone.bone_name}: {bone.transform.location}")
 *
 *   # Add animation notify (use full class path)
 *   unreal.AnimSequenceService.add_notify(
 *       anim_path,
 *       "/Script/Engine.AnimNotify",
 *       0.25,
 *       "Footstep"
 *   )
 *
 * @note All methods are static and thread-safe
 * @note C++ out parameters become Python return values
 *
 * **C++ Source:**
 *
 * - **Plugin**: VibeUE
 * - **Module**: VibeUE
 * - **File**: UAnimSequenceService.h
 */
UCLASS(BlueprintType)
class VIBEUE_API UAnimSequenceService : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================================
	// ANIMATION DISCOVERY
	// ============================================================================

	/**
	 * List all Animation Sequence assets in a path.
	 *
	 * @param SearchPath - Path to search for animations (default: "/Game")
	 * @param SkeletonFilter - Optional skeleton path to filter by
	 * @return Array of animation sequence info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Discovery")
	static TArray<FAnimSequenceInfo> ListAnimSequences(
		const FString& SearchPath = TEXT("/Game"),
		const FString& SkeletonFilter = TEXT(""));

	/**
	 * Get detailed information about an Animation Sequence asset.
	 *
	 * @param AnimPath - Full path to the animation asset
	 * @param OutInfo - Output animation info
	 * @return True if animation was found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Discovery")
	static bool GetAnimSequenceInfo(
		const FString& AnimPath,
		FAnimSequenceInfo& OutInfo);

	/**
	 * Find all animations compatible with a specific skeleton.
	 *
	 * @param SkeletonPath - Path to the skeleton asset
	 * @return Array of animation sequence info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Discovery")
	static TArray<FAnimSequenceInfo> FindAnimationsForSkeleton(const FString& SkeletonPath);

	/**
	 * Search animations by name pattern.
	 *
	 * @param NamePattern - Pattern to match (supports wildcards)
	 * @param SearchPath - Path to search in (default: "/Game")
	 * @return Array of matching animation info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Discovery")
	static TArray<FAnimSequenceInfo> SearchAnimations(
		const FString& NamePattern,
		const FString& SearchPath = TEXT("/Game"));

	// ============================================================================
	// ANIMATION CREATION
	// ============================================================================

	/**
	 * Create an animation sequence from a skeletal mesh's current pose.
	 * Captures the pose at the current time and creates a static animation.
	 *
	 * @param SkeletonPath - Path to the skeleton asset
	 * @param AnimName - Name for the new animation
	 * @param SavePath - Directory path to save the animation (default: "/Game")
	 * @param Duration - Duration of the animation in seconds (default: 1.0)
	 * @return Path to the created animation, or empty string on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString CreateFromPose(
		const FString& SkeletonPath,
		const FString& AnimName,
		const FString& SavePath = TEXT("/Game"),
		float Duration = 1.0f);

	/**
	 * Create an animation sequence with bone track data.
	 * Creates a new animation from scratch with custom keyframes.
	 *
	 * @param SkeletonPath - Path to the skeleton asset
	 * @param AnimName - Name for the new animation
	 * @param SavePath - Directory path to save the animation (default: "/Game")
	 * @param Duration - Duration of the animation in seconds
	 * @param FrameRate - Frame rate for the animation (default: 30.0)
	 * @param BoneTracks - Array of bone track data with keyframes
	 * @return Path to the created animation, or empty string on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FString CreateAnimSequence(
		const FString& SkeletonPath,
		const FString& AnimName,
		const FString& SavePath,
		float Duration,
		float FrameRate,
		const TArray<FBoneTrackData>& BoneTracks);

	/**
	 * Get reference pose keyframe for a bone.
	 * Returns a keyframe initialized with the bone's reference pose transform.
	 * Useful for creating animations that start from the reference pose.
	 *
	 * @param SkeletonPath - Path to the skeleton asset
	 * @param BoneName - Name of the bone to get reference pose for
	 * @param Time - Time value to set on the keyframe (default: 0.0)
	 * @return Keyframe with reference pose transform, or default keyframe if bone not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Creation")
	static FAnimKeyframe GetReferencePoseKeyframe(
		const FString& SkeletonPath,
		const FString& BoneName,
		float Time = 0.0f);

	/**
	 * Convert Euler angles (in degrees) to a Quaternion.
	 * Helper method for creating rotation keyframes.
	 * Modifies the OutQuat parameter with the result.
	 *
	 * @param Roll - Rotation around X axis in degrees
	 * @param Pitch - Rotation around Y axis in degrees
	 * @param Yaw - Rotation around Z axis in degrees
	 * @param OutQuat - Output quaternion representing the rotation
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Helpers")
	static void EulerToQuat(float Roll, float Pitch, float Yaw, FQuat& OutQuat);

	/**
	 * Multiply two quaternions together.
	 * Useful for combining reference pose rotation with a delta rotation.
	 * Modifies the OutResult parameter with the result (A * B).
	 *
	 * @param A - First quaternion
	 * @param B - Second quaternion
	 * @param OutResult - Output combined rotation
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Helpers")
	static void MultiplyQuats(const FQuat& A, const FQuat& B, FQuat& OutResult);

	// ============================================================================
	// ANIMATION PROPERTIES
	// ============================================================================

	/**
	 * Get animation duration in seconds.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Duration in seconds, or -1 if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Properties")
	static float GetAnimationLength(const FString& AnimPath);

	/**
	 * Get animation frame rate.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Frame rate, or -1 if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Properties")
	static float GetAnimationFrameRate(const FString& AnimPath);

	/**
	 * Get total frame count.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Frame count, or -1 if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Properties")
	static int32 GetAnimationFrameCount(const FString& AnimPath);

	/**
	 * Set animation frame rate (requires reimport for actual change).
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NewFrameRate - New frame rate to set
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Properties")
	static bool SetAnimationFrameRate(const FString& AnimPath, float NewFrameRate);

	/**
	 * Get the skeleton asset path used by this animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Skeleton asset path, or empty if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Properties")
	static FString GetAnimationSkeleton(const FString& AnimPath);

	/**
	 * Get animation rate scale (playback speed multiplier).
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Rate scale, or -1 if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Properties")
	static float GetRateScale(const FString& AnimPath);

	/**
	 * Set animation rate scale (playback speed multiplier).
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param RateScale - New rate scale value
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Properties")
	static bool SetRateScale(const FString& AnimPath, float RateScale);

	// ============================================================================
	// BONE TRACK DATA
	// ============================================================================

	/**
	 * Get list of all bones that have animation data.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Array of bone names
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|BoneTracks")
	static TArray<FString> GetAnimatedBones(const FString& AnimPath);

	/**
	 * Get bone transform at a specific time.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param BoneName - Name of the bone
	 * @param Time - Time in seconds
	 * @param bGlobalSpace - If true, returns global space transform
	 * @param OutTransform - Output transform
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|BoneTracks")
	static bool GetBoneTransformAtTime(
		const FString& AnimPath,
		const FString& BoneName,
		float Time,
		bool bGlobalSpace,
		FTransform& OutTransform);

	/**
	 * Get bone transform at a specific frame.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param BoneName - Name of the bone
	 * @param Frame - Frame number
	 * @param bGlobalSpace - If true, returns global space transform
	 * @param OutTransform - Output transform
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|BoneTracks")
	static bool GetBoneTransformAtFrame(
		const FString& AnimPath,
		const FString& BoneName,
		int32 Frame,
		bool bGlobalSpace,
		FTransform& OutTransform);

	// ============================================================================
	// POSE EXTRACTION
	// ============================================================================

	/**
	 * Get full skeleton pose at a specific time.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param Time - Time in seconds
	 * @param bGlobalSpace - If true, returns global space transforms
	 * @return Array of bone poses
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Pose")
	static TArray<FBonePose> GetPoseAtTime(
		const FString& AnimPath,
		float Time,
		bool bGlobalSpace = false);

	/**
	 * Get full skeleton pose at a specific frame.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param Frame - Frame number
	 * @param bGlobalSpace - If true, returns global space transforms
	 * @return Array of bone poses
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Pose")
	static TArray<FBonePose> GetPoseAtFrame(
		const FString& AnimPath,
		int32 Frame,
		bool bGlobalSpace = false);

	/**
	 * Get root motion transform at a specific time.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param Time - Time in seconds
	 * @param OutTransform - Output root motion transform
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Pose")
	static bool GetRootMotionAtTime(
		const FString& AnimPath,
		float Time,
		FTransform& OutTransform);

	/**
	 * Get total root motion transform for the entire animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param OutTransform - Output total root motion transform
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Pose")
	static bool GetTotalRootMotion(
		const FString& AnimPath,
		FTransform& OutTransform);

	// ============================================================================
	// CURVE DATA
	// ============================================================================

	/**
	 * List all curves in an animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Array of curve info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Curves")
	static TArray<FAnimCurveInfo> ListCurves(const FString& AnimPath);

	/**
	 * Get information about a specific curve.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param CurveName - Name of the curve
	 * @param OutInfo - Output curve info
	 * @return True if curve was found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Curves")
	static bool GetCurveInfo(
		const FString& AnimPath,
		const FString& CurveName,
		FAnimCurveInfo& OutInfo);

	/**
	 * Get curve value at a specific time.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param CurveName - Name of the curve
	 * @param Time - Time in seconds
	 * @param OutValue - Output value
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Curves")
	static bool GetCurveValueAtTime(
		const FString& AnimPath,
		const FString& CurveName,
		float Time,
		float& OutValue);

	/**
	 * Get all keyframes for a curve.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param CurveName - Name of the curve
	 * @return Array of curve keyframes
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Curves")
	static TArray<FCurveKeyframe> GetCurveKeyframes(
		const FString& AnimPath,
		const FString& CurveName);

	/**
	 * Add a new curve to the animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param CurveName - Name for the new curve
	 * @param bIsMorphTarget - Whether this curve drives a morph target
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Curves")
	static bool AddCurve(
		const FString& AnimPath,
		const FString& CurveName,
		bool bIsMorphTarget = false);

	/**
	 * Remove a curve from the animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param CurveName - Name of the curve to remove
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Curves")
	static bool RemoveCurve(
		const FString& AnimPath,
		const FString& CurveName);

	/**
	 * Set all keys for a curve.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param CurveName - Name of the curve
	 * @param Keys - Array of keyframes to set
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Curves")
	static bool SetCurveKeys(
		const FString& AnimPath,
		const FString& CurveName,
		const TArray<FCurveKeyframe>& Keys);

	/**
	 * Add a single key to a curve.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param CurveName - Name of the curve
	 * @param Time - Time in seconds
	 * @param Value - Value at this key
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Curves")
	static bool AddCurveKey(
		const FString& AnimPath,
		const FString& CurveName,
		float Time,
		float Value);

	// ============================================================================
	// ANIM NOTIFIES
	// ============================================================================

	/**
	 * List all notifies in an animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Array of notify info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static TArray<FAnimNotifyInfo> ListNotifies(const FString& AnimPath);

	/**
	 * Get information about a specific notify.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NotifyIndex - Index of the notify
	 * @param OutInfo - Output notify info
	 * @return True if notify was found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static bool GetNotifyInfo(
		const FString& AnimPath,
		int32 NotifyIndex,
		FAnimNotifyInfo& OutInfo);

	/**
	 * Add an instant notify (point in time).
	 *
	 * @param AnimPath - Full path to the animation asset (use package_name from AssetData, not package_path)
	 * @param NotifyClass - Full class path (e.g., "/Script/Engine.AnimNotify" or "/Script/Engine.AnimNotify_PlaySound")
	 * @param TriggerTime - Time in seconds when notify triggers
	 * @param NotifyName - Optional name for the notify
	 * @return Index of the new notify, or -1 on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static int32 AddNotify(
		const FString& AnimPath,
		const FString& NotifyClass,
		float TriggerTime,
		const FString& NotifyName = TEXT(""));

	/**
	 * Add a notify state (duration-based).
	 *
	 * @param AnimPath - Full path to the animation asset (use package_name from AssetData, not package_path)
	 * @param NotifyStateClass - Full class path (e.g., "/Script/Engine.AnimNotifyState")
	 * @param StartTime - Start time in seconds
	 * @param Duration - Duration in seconds
	 * @param NotifyName - Optional name for the notify
	 * @return Index of the new notify, or -1 on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static int32 AddNotifyState(
		const FString& AnimPath,
		const FString& NotifyStateClass,
		float StartTime,
		float Duration,
		const FString& NotifyName = TEXT(""));

	/**
	 * Remove a notify from the animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NotifyIndex - Index of the notify to remove
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static bool RemoveNotify(const FString& AnimPath, int32 NotifyIndex);

	/**
	 * Set the trigger time for a notify.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NotifyIndex - Index of the notify
	 * @param NewTime - New trigger time in seconds
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static bool SetNotifyTriggerTime(
		const FString& AnimPath,
		int32 NotifyIndex,
		float NewTime);

	/**
	 * Set the duration for a notify state.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NotifyIndex - Index of the notify
	 * @param NewDuration - New duration in seconds
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static bool SetNotifyDuration(
		const FString& AnimPath,
		int32 NotifyIndex,
		float NewDuration);

	/**
	 * Set the track index for a notify.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NotifyIndex - Index of the notify
	 * @param TrackIndex - New track index
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static bool SetNotifyTrack(
		const FString& AnimPath,
		int32 NotifyIndex,
		int32 TrackIndex);

	/**
	 * Set the name for a notify.
	 * For skeleton notifies (base AnimNotify class), this changes the display name.
	 * For class-based notifies, this changes the stored NotifyName but display uses class name.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NotifyIndex - Index of the notify
	 * @param NewName - New notify name
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static bool SetNotifyName(
		const FString& AnimPath,
		int32 NotifyIndex,
		const FString& NewName);

	/**
	 * Set the color for a notify in the editor.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NotifyIndex - Index of the notify
	 * @param NewColor - New color (RGBA, 0-1 range)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static bool SetNotifyColor(
		const FString& AnimPath,
		int32 NotifyIndex,
		FLinearColor NewColor);

	/**
	 * Set the trigger chance for a notify (0-1, where 1 = always triggers).
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NotifyIndex - Index of the notify
	 * @param TriggerChance - Chance to trigger (0.0 = never, 1.0 = always)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static bool SetNotifyTriggerChance(
		const FString& AnimPath,
		int32 NotifyIndex,
		float TriggerChance);

	/**
	 * Set whether the notify triggers on dedicated servers.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NotifyIndex - Index of the notify
	 * @param bTriggerOnServer - True to trigger on dedicated servers
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static bool SetNotifyTriggerOnServer(
		const FString& AnimPath,
		int32 NotifyIndex,
		bool bTriggerOnServer);

	/**
	 * Set whether the notify triggers when the animation is a follower in a sync group.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NotifyIndex - Index of the notify
	 * @param bTriggerOnFollower - True to trigger on followers
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static bool SetNotifyTriggerOnFollower(
		const FString& AnimPath,
		int32 NotifyIndex,
		bool bTriggerOnFollower);

	/**
	 * Set the weight threshold for notify triggering.
	 * Notify only fires if blend weight is above this threshold.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NotifyIndex - Index of the notify
	 * @param WeightThreshold - Minimum blend weight to trigger (0.0-1.0)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static bool SetNotifyTriggerWeightThreshold(
		const FString& AnimPath,
		int32 NotifyIndex,
		float WeightThreshold);

	/**
	 * Set the LOD filtering for a notify.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param NotifyIndex - Index of the notify
	 * @param FilterType - Filter type: "NoFiltering", "LOD", or "BelowLOD"
	 * @param FilterLOD - LOD level to filter from (0 = highest detail)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Notifies")
	static bool SetNotifyLODFilter(
		const FString& AnimPath,
		int32 NotifyIndex,
		const FString& FilterType,
		int32 FilterLOD);

	// ============================================================================
	// NOTIFY TRACKS
	// ============================================================================

	/**
	 * List all notify tracks in an animation.
	 * In UE 5.7, notify tracks are implicit - they're created based on notify TrackIndex values.
	 * Returns generated track names like "Track 1", "Track 2", etc.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Array of track names (index in array corresponds to track index)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|NotifyTracks")
	static TArray<FString> ListNotifyTracks(const FString& AnimPath);

	/**
	 * Get the number of notify tracks in an animation.
	 * Track count is determined by the highest TrackIndex among all notifies + 1.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Number of notify tracks (minimum 1), or -1 on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|NotifyTracks")
	static int32 GetNotifyTrackCount(const FString& AnimPath);

	/**
	 * Get the next available notify track index.
	 * In UE 5.7, tracks are implicit - place a notify on any track index to "create" it.
	 * The TrackName parameter is informational only (UE uses indexed tracks).
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param TrackName - Informational name (not stored, UE uses track indices)
	 * @return Next available track index, or -1 on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|NotifyTracks")
	static int32 AddNotifyTrack(
		const FString& AnimPath,
		const FString& TrackName);

	/**
	 * Rename an existing notify track.
	 * NOTE: Not supported in UE 5.7 - notify tracks are implicitly named by index.
	 * Always returns false.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param TrackIndex - Index of the track to rename
	 * @param NewName - New name for the track (ignored)
	 * @return Always false - operation not supported
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|NotifyTracks")
	static bool RenameNotifyTrack(
		const FString& AnimPath,
		int32 TrackIndex,
		const FString& NewName);

	/**
	 * Remove a notify track from an animation.
	 * Moves all notifies on this track to track 0, then decrements
	 * track indices for notifies on higher tracks.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param TrackIndex - Index of the track to remove
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|NotifyTracks")
	static bool RemoveNotifyTrack(
		const FString& AnimPath,
		int32 TrackIndex);

	// ============================================================================
	// SYNC MARKERS
	// ============================================================================

	/**
	 * List all sync markers in an animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Array of sync marker info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|SyncMarkers")
	static TArray<FSyncMarkerInfo> ListSyncMarkers(const FString& AnimPath);

	/**
	 * Add a sync marker to the animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param MarkerName - Name for the marker
	 * @param Time - Time in seconds
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|SyncMarkers")
	static bool AddSyncMarker(
		const FString& AnimPath,
		const FString& MarkerName,
		float Time);

	/**
	 * Remove a sync marker from the animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param MarkerName - Name of the marker
	 * @param Time - Time of the marker to remove
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|SyncMarkers")
	static bool RemoveSyncMarker(
		const FString& AnimPath,
		const FString& MarkerName,
		float Time);

	/**
	 * Set the time for a sync marker by index.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param MarkerIndex - Index of the marker
	 * @param NewTime - New time in seconds
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|SyncMarkers")
	static bool SetSyncMarkerTime(
		const FString& AnimPath,
		int32 MarkerIndex,
		float NewTime);

	/**
	 * Set the time for a sync marker by name and current time.
	 * This is a convenience method that finds the marker by name and current time,
	 * then updates it to the new time.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param MarkerName - Name of the marker to find
	 * @param CurrentTime - Current time of the marker (used to identify which marker)
	 * @param NewTime - New time in seconds
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|SyncMarkers")
	static bool SetSyncMarkerTimeByName(
		const FString& AnimPath,
		const FString& MarkerName,
		float CurrentTime,
		float NewTime);

	// ============================================================================
	// ADDITIVE ANIMATION
	// ============================================================================

	/**
	 * Get the additive animation type.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Additive type as string ("None", "LocalSpace", "MeshSpace")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Additive")
	static FString GetAdditiveAnimType(const FString& AnimPath);

	/**
	 * Set the additive animation type.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param Type - Type as string ("None", "LocalSpace", "MeshSpace")
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Additive")
	static bool SetAdditiveAnimType(const FString& AnimPath, const FString& Type);

	/**
	 * Get the base pose animation for additive.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Path to the base pose animation, or empty if none
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Additive")
	static FString GetAdditiveBasePose(const FString& AnimPath);

	/**
	 * Set the base pose animation for additive.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param BasePoseAnimPath - Path to the base pose animation
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Additive")
	static bool SetAdditiveBasePose(
		const FString& AnimPath,
		const FString& BasePoseAnimPath);

	// ============================================================================
	// ROOT MOTION
	// ============================================================================

	/**
	 * Check if root motion is enabled.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return True if root motion is enabled
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|RootMotion")
	static bool GetEnableRootMotion(const FString& AnimPath);

	/**
	 * Enable or disable root motion.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param bEnable - Whether to enable root motion
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|RootMotion")
	static bool SetEnableRootMotion(const FString& AnimPath, bool bEnable);

	/**
	 * Get root motion root lock type.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Root lock type as string
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|RootMotion")
	static FString GetRootMotionRootLock(const FString& AnimPath);

	/**
	 * Set root motion root lock type.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param LockType - Lock type as string ("RefPose", "AnimFirstFrame", "Zero")
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|RootMotion")
	static bool SetRootMotionRootLock(const FString& AnimPath, const FString& LockType);

	/**
	 * Check if force root lock is enabled.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return True if force root lock is enabled
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|RootMotion")
	static bool GetForceRootLock(const FString& AnimPath);

	/**
	 * Enable or disable force root lock.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param bForce - Whether to force root lock
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|RootMotion")
	static bool SetForceRootLock(const FString& AnimPath, bool bForce);

	// ============================================================================
	// COMPRESSION
	// ============================================================================

	/**
	 * Get compression information for an animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param OutInfo - Output compression info
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Compression")
	static bool GetCompressionInfo(
		const FString& AnimPath,
		FAnimCompressionInfo& OutInfo);

	/**
	 * Set compression scheme for an animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param CompressionSchemePath - Path to the compression settings asset
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Compression")
	static bool SetCompressionScheme(
		const FString& AnimPath,
		const FString& CompressionSchemePath);

	/**
	 * Compress/recompress an animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Compression")
	static bool CompressAnimation(const FString& AnimPath);

	// ============================================================================
	// IMPORT/EXPORT
	// ============================================================================

	/**
	 * Export animation data to JSON string.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return JSON string with animation data
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Export")
	static FString ExportAnimationToJson(const FString& AnimPath);

	/**
	 * Get source files associated with the animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return Array of source file paths
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Export")
	static TArray<FString> GetSourceFiles(const FString& AnimPath);

	// ============================================================================
	// EDITOR NAVIGATION
	// ============================================================================

	/**
	 * Open the animation in the Animation Editor.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Editor")
	static bool OpenAnimationEditor(const FString& AnimPath);

	/**
	 * Set preview playback time in the Animation Editor.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param Time - Time in seconds
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Editor")
	static bool SetPreviewTime(const FString& AnimPath, float Time);

	/**
	 * Start preview playback in the Animation Editor.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param bLoop - Whether to loop playback
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Editor")
	static bool PlayPreview(const FString& AnimPath, bool bLoop = false);

	/**
	 * Stop preview playback in the Animation Editor.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Editor")
	static bool StopPreview(const FString& AnimPath);

	// ============================================================================
	// PREVIEW EDITING (Inspect → Preview → Validate → Bake workflow)
	// ============================================================================

	/**
	 * Preview a bone rotation delta before baking to keyframes.
	 * The delta is applied to the bone's current rotation in the specified space.
	 * Multiple previews can be stacked before baking.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param BoneName - Name of the bone to rotate
	 * @param RotationDelta - Rotation delta (Euler degrees)
	 * @param Space - Coordinate space: "local", "component", or "world"
	 * @param PreviewFrame - Frame to preview at (default: current frame)
	 * @param OutResult - Result with applied changes and any clamping
	 * @return True if preview was applied successfully
	 *
	 * Example:
	 *   # Preview rotating the upper arm 30 degrees
	 *   result = unreal.AnimSequenceService.preview_bone_rotation(
	 *       "/Game/Anims/AS_Idle",
	 *       "upperarm_r",
	 *       unreal.Rotator(0, 30, 0),  # 30 degree pitch
	 *       "local",
	 *       0
	 *   )
	 *   if result.was_clamped:
	 *       print("Rotation was clamped to constraints")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Preview")
	static bool PreviewBoneRotation(
		const FString& AnimPath,
		const FString& BoneName,
		const FRotator& RotationDelta,
		const FString& Space,
		int32 PreviewFrame,
		FAnimationEditResult& OutResult);

	/**
	 * Preview rotation deltas for multiple bones at once.
	 * All deltas are applied atomically - if one fails validation, none are applied.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param BoneDeltas - Array of bone deltas to apply
	 * @param Space - Coordinate space: "local", "component", or "world"
	 * @param PreviewFrame - Frame to preview at
	 * @param OutResult - Result with applied changes
	 * @return True if all previews were applied successfully
	 *
	 * Example:
	 *   deltas = [
	 *       unreal.BoneDelta(bone_name="upperarm_r", rotation_delta=unreal.Rotator(0, 45, 0)),
	 *       unreal.BoneDelta(bone_name="lowerarm_r", rotation_delta=unreal.Rotator(0, 30, 0)),
	 *       unreal.BoneDelta(bone_name="hand_r", rotation_delta=unreal.Rotator(10, 0, 15))
	 *   ]
	 *   result = unreal.AnimSequenceService.preview_pose_delta("/Game/Anims/AS_Idle", deltas, "local", 0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Preview")
	static bool PreviewPoseDelta(
		const FString& AnimPath,
		const TArray<FBoneDelta>& BoneDeltas,
		const FString& Space,
		int32 PreviewFrame,
		FAnimationEditResult& OutResult);

	/**
	 * Cancel all pending preview edits without baking to keyframes.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @return True if preview was cancelled
	 *
	 * Example:
	 *   # Discard previewed changes
	 *   unreal.AnimSequenceService.cancel_preview("/Game/Anims/AS_Idle")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Preview")
	static bool CancelPreview(const FString& AnimPath);

	/**
	 * Get the current state of an animation preview session.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param OutState - Current preview state
	 * @return True if state was retrieved
	 *
	 * Example:
	 *   state = unreal.AnimSequenceService.get_preview_state("/Game/Anims/AS_Idle")
	 *   if state.is_active:
	 *       print(f"Previewing {state.pending_edit_count} edits")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Preview")
	static bool GetPreviewState(
		const FString& AnimPath,
		FAnimationPreviewState& OutState);

	/**
	 * Validate the current preview pose against bone constraints.
	 * Uses the skeleton's constraint profile (manual or learned).
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param bUseLearnedConstraints - Use learned constraints instead of manual
	 * @param OutResult - Validation result with violations and suggestions
	 * @return True if validation completed (check OutResult.bIsValid for pass/fail)
	 *
	 * Example:
	 *   result = unreal.AnimSequenceService.validate_pose("/Game/Anims/AS_Idle", True)
	 *   if not result.is_valid:
	 *       for violation in result.violation_messages:
	 *           print(f"Violation: {violation}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Preview")
	static bool ValidatePose(
		const FString& AnimPath,
		bool bUseLearnedConstraints,
		FPoseValidationResult& OutResult);

	/**
	 * Bake all pending preview edits to keyframes in the animation.
	 * This commits the previewed changes to the actual animation data.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param StartFrame - Start frame of range to bake (0 = start of animation)
	 * @param EndFrame - End frame of range to bake (-1 = end of animation)
	 * @param InterpMode - Interpolation mode: "linear", "cubic", "auto"
	 * @param OutResult - Result with baked frame range and any issues
	 * @return True if bake was successful
	 *
	 * Example:
	 *   # Bake preview to all frames with cubic interpolation
	 *   result = unreal.AnimSequenceService.bake_preview_to_keyframes(
	 *       "/Game/Anims/AS_Idle",
	 *       0, -1,  # All frames
	 *       "cubic"
	 *   )
	 *   if result.success:
	 *       print(f"Baked frames {result.start_frame} to {result.end_frame}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Preview")
	static bool BakePreviewToKeyframes(
		const FString& AnimPath,
		int32 StartFrame,
		int32 EndFrame,
		const FString& InterpMode,
		FAnimationEditResult& OutResult);

	/**
	 * Apply a bone rotation directly to keyframes without preview.
	 * For quick edits when preview validation is not needed.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param BoneName - Name of the bone to rotate
	 * @param Rotation - Rotation to apply (absolute or delta based on bIsDelta)
	 * @param Space - Coordinate space: "local", "component", or "world"
	 * @param StartFrame - Start frame of range
	 * @param EndFrame - End frame of range (-1 = end of animation)
	 * @param bIsDelta - If true, rotation is added to existing; if false, it replaces
	 * @param OutResult - Result with applied changes
	 * @return True if rotation was applied successfully
	 *
	 * Example:
	 *   # Add 15 degree rotation to frames 0-30
	 *   result = unreal.AnimSequenceService.apply_bone_rotation(
	 *       "/Game/Anims/AS_Idle",
	 *       "spine_01",
	 *       unreal.Rotator(0, 0, 15),  # 15 degree yaw
	 *       "local",
	 *       0, 30,
	 *       True  # Delta mode
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Preview")
	static bool ApplyBoneRotation(
		const FString& AnimPath,
		const FString& BoneName,
		const FRotator& Rotation,
		const FString& Space,
		int32 StartFrame,
		int32 EndFrame,
		bool bIsDelta,
		FAnimationEditResult& OutResult);

	// ============================================================================
	// POSE UTILITIES
	// ============================================================================

	/**
	 * Copy a pose from one frame/animation to another.
	 *
	 * @param SrcAnimPath - Source animation path
	 * @param SrcFrame - Source frame number
	 * @param DstAnimPath - Destination animation path
	 * @param DstFrame - Destination frame number
	 * @param BoneFilter - Optional list of bone names to copy (empty = all bones)
	 * @param OutResult - Result with copied bones
	 * @return True if pose was copied successfully
	 *
	 * Example:
	 *   # Copy frame 0 from idle to frame 15 of walk
	 *   result = unreal.AnimSequenceService.copy_pose(
	 *       "/Game/Anims/AS_Idle", 0,
	 *       "/Game/Anims/AS_Walk", 15,
	 *       []  # All bones
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Pose")
	static bool CopyPose(
		const FString& SrcAnimPath,
		int32 SrcFrame,
		const FString& DstAnimPath,
		int32 DstFrame,
		const TArray<FString>& BoneFilter,
		FAnimationEditResult& OutResult);

	/**
	 * Mirror a pose across the character's symmetry axis.
	 * Swaps left/right bone transforms (e.g., hand_l ↔ hand_r).
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param Frame - Frame to mirror
	 * @param MirrorAxis - Axis to mirror across: "X", "Y", or "Z" (default: "X")
	 * @param OutResult - Result with mirrored bones
	 * @return True if pose was mirrored successfully
	 *
	 * Example:
	 *   result = unreal.AnimSequenceService.mirror_pose("/Game/Anims/AS_Wave", 15, "X")
	 *   print(f"Mirrored {len(result.modified_bones)} bones")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Pose")
	static bool MirrorPose(
		const FString& AnimPath,
		int32 Frame,
		const FString& MirrorAxis,
		FAnimationEditResult& OutResult);

	/**
	 * Get the reference pose (T-pose/bind pose) for a skeleton.
	 *
	 * @param SkeletonPath - Path to the skeleton asset
	 * @return Array of bone poses in reference pose
	 *
	 * Example:
	 *   ref_pose = unreal.AnimSequenceService.get_reference_pose("/Game/SK_Mannequin")
	 *   for bone in ref_pose:
	 *       print(f"{bone.bone_name}: {bone.transform.rotation}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Pose")
	static TArray<FBonePose> GetReferencePose(const FString& SkeletonPath);

	/**
	 * Convert a quaternion to Euler angles (degrees).
	 *
	 * @param Quat - Quaternion to convert
	 * @param OutRotator - Output Euler rotation
	 *
	 * Example:
	 *   euler = unreal.AnimSequenceService.quat_to_euler(some_quat)
	 *   print(f"Roll={euler.roll}, Pitch={euler.pitch}, Yaw={euler.yaw}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Helpers")
	static void QuatToEuler(const FQuat& Quat, FRotator& OutRotator);

	// ============================================================================
	// RETARGETING
	// ============================================================================

	/**
	 * Preview an animation on a different skeleton (retarget preview).
	 * Does not modify the original animation.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param TargetSkeletonPath - Path to the target skeleton
	 * @param OutResult - Result with any retargeting issues
	 * @return True if preview was set up successfully
	 *
	 * Example:
	 *   result = unreal.AnimSequenceService.retarget_preview(
	 *       "/Game/Anims/AS_Run",
	 *       "/Game/MetaHumans/SK_MetaHuman"
	 *   )
	 *   if result.success:
	 *       print("Preview active - check animation editor")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Retarget")
	static bool RetargetPreview(
		const FString& AnimPath,
		const FString& TargetSkeletonPath,
		FAnimationEditResult& OutResult);

	// ============================================================================
	// ANIMATION POSE CAPTURE (Visual Feedback)
	// ============================================================================

	/**
	 * Capture an animation pose at a specific time to an image file.
	 * This renders the skeletal mesh at a specific animation frame without using
	 * screenshots, providing visual feedback for AI-generated animations.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param Time - Time in seconds to capture (0.0 = first frame)
	 * @param OutputPath - Full path to output image file (PNG format)
	 * @param CameraAngle - Camera angle: "front", "side", "back", "three_quarter", "top" (default: "three_quarter")
	 * @param ImageWidth - Width of output image in pixels (default: 512)
	 * @param ImageHeight - Height of output image in pixels (default: 512)
	 * @param OutResult - Capture result with file info
	 * @return True if capture was successful
	 *
	 * Example:
	 *   result = unreal.AnimSequenceService.capture_animation_pose(
	 *       "/Game/Animations/AS_Run",
	 *       0.5,  # Capture at 0.5 seconds
	 *       "C:/Temp/run_frame.png",
	 *       "three_quarter",
	 *       512, 512
	 *   )
	 *   if result.success:
	 *       print(f"Captured to: {result.image_path}")
	 *
	 * Note: This spawns a temporary actor, sets the pose, renders to texture,
	 * and exports to PNG - all without affecting the current viewport or taking screenshots.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Capture")
	static bool CaptureAnimationPose(
		const FString& AnimPath,
		float Time,
		const FString& OutputPath,
		const FString& CameraAngle,
		int32 ImageWidth,
		int32 ImageHeight,
		FAnimationPoseCaptureResult& OutResult);

	/**
	 * Capture multiple frames of an animation as a sequence of images.
	 * Useful for creating thumbnails or comparing poses across time.
	 *
	 * @param AnimPath - Path to the animation asset
	 * @param OutputDirectory - Directory to save images (files named frame_001.png, etc.)
	 * @param FrameCount - Number of frames to capture (evenly distributed across animation)
	 * @param CameraAngle - Camera angle: "front", "side", "back", "three_quarter", "top" (default: "three_quarter")
	 * @param ImageWidth - Width of output images (default: 256)
	 * @param ImageHeight - Height of output images (default: 256)
	 * @return Array of capture results for each frame
	 *
	 * Example:
	 *   results = unreal.AnimSequenceService.capture_animation_sequence(
	 *       "/Game/Animations/AS_Run",
	 *       "C:/Temp/run_frames/",
	 *       8,  # Capture 8 frames
	 *       "front",
	 *       256, 256
	 *   )
	 *   for r in results:
	 *       print(f"Frame {r.captured_frame}: {r.image_path}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Capture")
	static TArray<FAnimationPoseCaptureResult> CaptureAnimationSequence(
		const FString& AnimPath,
		const FString& OutputDirectory,
		int32 FrameCount,
		const FString& CameraAngle,
		int32 ImageWidth,
		int32 ImageHeight);

private:
	// ============================================================================
	// PRIVATE HELPERS
	// ============================================================================

	/** Load an animation sequence from path */
	static class UAnimSequence* LoadAnimSequence(const FString& AnimPath);

	/** Convert additive type enum to string */
	static FString AdditiveTypeToString(int32 Type);

	/** Convert string to additive type enum */
	static int32 StringToAdditiveType(const FString& TypeString);

	/** Convert root lock enum to string */
	static FString RootLockToString(int32 LockType);

	/** Convert string to root lock enum */
	static int32 StringToRootLock(const FString& LockString);

	/** Convert interpolation mode to string */
	static FString InterpModeToString(int32 Mode);

	/** Convert tangent mode to string */
	static FString TangentModeToString(int32 Mode);

	/** Fill animation info struct from animation sequence */
	static void FillAnimSequenceInfo(class UAnimSequence* AnimSeq, FAnimSequenceInfo& OutInfo);
};
