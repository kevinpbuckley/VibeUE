// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "USkeletonService.generated.h"

/**
 * Information about a Skeleton asset
 */
USTRUCT(BlueprintType)
struct FSkeletonAssetInfo
{
	GENERATED_BODY()

	/** Asset path of the skeleton */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString SkeletonPath;

	/** Display name of the skeleton */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString SkeletonName;

	/** Number of bones in the hierarchy */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 BoneCount = 0;

	/** Number of compatible skeletons configured */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 CompatibleSkeletonCount = 0;

	/** Number of curve metadata entries */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 CurveMetaDataCount = 0;

	/** Number of blend profiles */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 BlendProfileCount = 0;

	/** Preview forward axis setting */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString PreviewForwardAxis;

	/** List of blend profile names */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	TArray<FString> BlendProfileNames;
};

/**
 * Information about a bone in a skeleton hierarchy
 */
USTRUCT(BlueprintType)
struct FBoneNodeInfo
{
	GENERATED_BODY()

	/** Name of the bone */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString BoneName;

	/** Index in the bone hierarchy */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 BoneIndex = -1;

	/** Parent bone name (empty for root) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString ParentBoneName;

	/** Parent bone index (-1 for root) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 ParentBoneIndex = -1;

	/** Number of direct children */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 ChildCount = 0;

	/** Depth in hierarchy (0 = root) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 Depth = 0;

	/** Local transform relative to parent */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FTransform LocalTransform;

	/** Global/component space transform */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FTransform GlobalTransform;

	/** Translation retargeting mode */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString RetargetingMode;

	/** Names of direct child bones */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	TArray<FString> Children;
};

/**
 * Information about a socket attached to a skeletal mesh
 */
USTRUCT(BlueprintType)
struct FMeshSocketInfo
{
	GENERATED_BODY()

	/** Name of the socket */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString SocketName;

	/** Bone this socket is attached to */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString BoneName;

	/** Relative location in bone space */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FVector RelativeLocation = FVector::ZeroVector;

	/** Relative rotation in bone space */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	/** Relative scale */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FVector RelativeScale = FVector::OneVector;

	/** Whether socket forces bone to always animate */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	bool bForceAlwaysAnimated = false;
};

/**
 * Information about curve metadata in a skeleton
 */
USTRUCT(BlueprintType)
struct FCurveMetaInfo
{
	GENERATED_BODY()

	/** Name of the curve */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString CurveName;

	/** Whether this curve drives a morph target */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	bool bIsMorphTarget = false;

	/** Whether this curve drives a material parameter */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	bool bIsMaterial = false;
};

/**
 * Information about a blend profile in a skeleton
 */
USTRUCT(BlueprintType)
struct FBlendProfileData
{
	GENERATED_BODY()

	/** Name of the blend profile */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString ProfileName;

	/** Bones with custom blend scales in this profile */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	TArray<FString> BoneNames;

	/** Blend scales corresponding to bone names */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	TArray<float> BlendScales;
};

/**
 * Joint constraint limits for a bone
 */
USTRUCT(BlueprintType)
struct FBoneConstraint
{
	GENERATED_BODY()

	/** Name of the bone */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString BoneName;

	/** Minimum rotation (Euler angles in degrees) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FRotator MinRotation = FRotator(-180.0f, -180.0f, -180.0f);

	/** Maximum rotation (Euler angles in degrees) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FRotator MaxRotation = FRotator(180.0f, 180.0f, 180.0f);

	/** Whether this bone is constrained as a hinge (single axis rotation) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	bool bIsHinge = false;

	/** Hinge axis if bIsHinge is true (0=X/Roll, 1=Y/Pitch, 2=Z/Yaw) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 HingeAxis = 1;

	/** Preferred rotation order for Euler angles */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString RotationOrder = TEXT("YXZ");
};

/**
 * Learned rotation range for a bone from existing animations
 */
USTRUCT(BlueprintType)
struct FLearnedBoneRange
{
	GENERATED_BODY()

	/** Name of the bone */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString BoneName;

	/** Minimum observed rotation (Euler angles in degrees) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FRotator MinRotation = FRotator::ZeroRotator;

	/** Maximum observed rotation (Euler angles in degrees) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FRotator MaxRotation = FRotator::ZeroRotator;

	/** 5th percentile rotation (safer minimum) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FRotator Percentile5 = FRotator::ZeroRotator;

	/** 95th percentile rotation (safer maximum) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FRotator Percentile95 = FRotator::ZeroRotator;

	/** Number of samples analyzed */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 SampleCount = 0;
};

/**
 * Learned constraints from analyzing existing animations
 */
USTRUCT(BlueprintType)
struct FLearnedConstraintsInfo
{
	GENERATED_BODY()

	/** Path to the skeleton */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString SkeletonPath;

	/** Number of animations analyzed */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 AnimationCount = 0;

	/** Total frames sampled across all animations */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 TotalSamples = 0;

	/** Per-bone learned rotation ranges */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	TArray<FLearnedBoneRange> BoneRanges;
};

/**
 * Comprehensive skeleton profile with constraints and learned data
 */
USTRUCT(BlueprintType)
struct FSkeletonProfile
{
	GENERATED_BODY()

	/** Path to the skeleton */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString SkeletonPath;

	/** Display name of the skeleton */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString SkeletonName;

	/** Number of bones */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 BoneCount = 0;

	/** Whether this profile has been built */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	bool bIsValid = false;

	/** Whether learned constraints are available */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	bool bHasLearnedConstraints = false;

	/** Bone hierarchy (indexed by bone index) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	TArray<FBoneNodeInfo> BoneHierarchy;

	/** User-defined or default constraints per bone */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	TArray<FBoneConstraint> Constraints;

	/** Learned rotation ranges from animations (if available) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	TArray<FLearnedBoneRange> LearnedRanges;

	/** Retarget profile names configured for this skeleton */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	TArray<FString> RetargetProfiles;
};

/**
 * Result of validating a bone rotation against constraints
 */
USTRUCT(BlueprintType)
struct FBoneValidationResult
{
	GENERATED_BODY()

	/** Name of the bone */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString BoneName;

	/** Whether the rotation is valid (within constraints) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	bool bIsValid = true;

	/** Type of violation if invalid: "None", "MinExceeded", "MaxExceeded", "HingeViolation" */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString ViolationType = TEXT("None");

	/** The original rotation that was tested */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FRotator OriginalRotation = FRotator::ZeroRotator;

	/** The clamped rotation (valid within constraints) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FRotator ClampedRotation = FRotator::ZeroRotator;

	/** Descriptive message about the violation */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString Message;
};

/**
 * Information about a skeletal mesh asset
 */
USTRUCT(BlueprintType)
struct FSkeletalMeshData
{
	GENERATED_BODY()

	/** Asset path */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString MeshPath;

	/** Display name */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString MeshName;

	/** Path to associated skeleton */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString SkeletonPath;

	/** Number of bones */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 BoneCount = 0;

	/** Number of LOD levels */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 LODCount = 0;

	/** Number of sockets */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 SocketCount = 0;

	/** Number of morph targets */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 MorphTargetCount = 0;

	/** Number of materials */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	int32 MaterialCount = 0;

	/** Physics asset path (if assigned) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString PhysicsAssetPath;

	/** Post-process anim blueprint path (if assigned) */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString PostProcessAnimBPPath;

	/** Bounding box min */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FVector BoundsMin = FVector::ZeroVector;

	/** Bounding box max */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FVector BoundsMax = FVector::ZeroVector;
};

/**
 * Parameters for adding a new bone
 */
USTRUCT(BlueprintType)
struct FAddBoneParams
{
	GENERATED_BODY()

	/** Name for the new bone */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString BoneName;

	/** Name of the parent bone */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString ParentBoneName;

	/** Local transform relative to parent */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FTransform LocalTransform;
};

/**
 * Parameters for adding a socket
 */
USTRUCT(BlueprintType)
struct FAddSocketParams
{
	GENERATED_BODY()

	/** Name for the new socket */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString SocketName;

	/** Bone to attach socket to */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FString BoneName;

	/** Relative location in bone space */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FVector RelativeLocation = FVector::ZeroVector;

	/** Relative rotation in bone space */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FRotator RelativeRotation = FRotator::ZeroRotator;

	/** Relative scale */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	FVector RelativeScale = FVector::OneVector;

	/** Whether to add socket to skeleton (shared) vs mesh-specific */
	UPROPERTY(BlueprintReadWrite, Category = "Skeleton")
	bool bAddToSkeleton = false;
};

/**
 * Skeleton and Skeletal Mesh service exposed directly to Python.
 *
 * This service provides comprehensive CRUD operations for Skeleton and SkeletalMesh assets
 * including bone hierarchy management, socket operations, retargeting configuration,
 * curve metadata, and blend profile management.
 *
 * Python Usage:
 *   import unreal
 *
 *   # List all bones in a skeletal mesh
 *   bones = unreal.SkeletonService.list_bones("/Game/Characters/SKM_Mannequin")
 *   for bone in bones:
 *       print(f"{bone.bone_name} -> {bone.parent_bone_name}")
 *
 *   # Add a socket
 *   unreal.SkeletonService.add_socket(
 *       "/Game/Characters/SKM_Mannequin",
 *       "Weapon_R",
 *       "hand_r",
 *       unreal.Vector(10, 0, 0),
 *       unreal.Rotator(0, 0, 90)
 *   )
 *
 *   # Get skeleton info
 *   info = unreal.SkeletonService.get_skeleton_info("/Game/Characters/SK_Mannequin")
 *   print(f"Bones: {info.bone_count}, Blend Profiles: {info.blend_profile_count}")
 *
 * @note All methods are static and thread-safe
 * @note C++ out parameters become Python return values
 *
 * **C++ Source:**
 *
 * - **Plugin**: VibeUE
 * - **Module**: VibeUE
 * - **File**: USkeletonService.h
 */
UCLASS(BlueprintType)
class VIBEUE_API USkeletonService : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================================
	// SKELETON DISCOVERY
	// ============================================================================

	/**
	 * List all Skeleton assets in a path.
	 *
	 * @param SearchPath - Path to search for skeletons (default: "/Game")
	 * @param bRecursive - Whether to search subdirectories
	 * @return Array of skeleton asset paths
	 *
	 * Example:
	 *   skeletons = unreal.SkeletonService.list_skeletons("/Game/Characters")
	 *   for path in skeletons:
	 *       print(path)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Discovery")
	static TArray<FString> ListSkeletons(
		const FString& SearchPath = TEXT("/Game"),
		bool bRecursive = true);

	/**
	 * List all Skeletal Mesh assets in a path.
	 *
	 * @param SearchPath - Path to search for meshes (default: "/Game")
	 * @param bRecursive - Whether to search subdirectories
	 * @return Array of skeletal mesh asset paths
	 *
	 * Example:
	 *   meshes = unreal.SkeletonService.list_skeletal_meshes("/Game/Characters")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Discovery")
	static TArray<FString> ListSkeletalMeshes(
		const FString& SearchPath = TEXT("/Game"),
		bool bRecursive = true);

	/**
	 * Get detailed information about a Skeleton asset.
	 *
	 * @param SkeletonPath - Full path to the Skeleton asset
	 * @param OutInfo - Output skeleton info
	 * @return True if skeleton was found
	 *
	 * Example:
	 *   info = unreal.SkeletonService.get_skeleton_info("/Game/SK_Mannequin")
	 *   print(f"Bones: {info.bone_count}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Discovery")
	static bool GetSkeletonInfo(
		const FString& SkeletonPath,
		FSkeletonAssetInfo& OutInfo);

	/**
	 * Get detailed information about a Skeletal Mesh asset.
	 *
	 * @param SkeletalMeshPath - Full path to the Skeletal Mesh asset
	 * @param OutInfo - Output mesh info
	 * @return True if mesh was found
	 *
	 * Example:
	 *   info = unreal.SkeletonService.get_skeletal_mesh_info("/Game/SKM_Character")
	 *   print(f"LODs: {info.lod_count}, Sockets: {info.socket_count}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Discovery")
	static bool GetSkeletalMeshInfo(
		const FString& SkeletalMeshPath,
		FSkeletalMeshData& OutInfo);

	/**
	 * Get the Skeleton asset path used by a Skeletal Mesh.
	 *
	 * @param SkeletalMeshPath - Full path to the Skeletal Mesh
	 * @return Skeleton asset path, or empty if not found
	 *
	 * Example:
	 *   skeleton = unreal.SkeletonService.get_skeleton_for_mesh("/Game/SKM_Character")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Discovery")
	static FString GetSkeletonForMesh(const FString& SkeletalMeshPath);

	// ============================================================================
	// BONE HIERARCHY
	// ============================================================================

	/**
	 * List all bones in a Skeleton or Skeletal Mesh.
	 *
	 * @param AssetPath - Path to Skeleton or Skeletal Mesh
	 * @return Array of bone information
	 *
	 * Example:
	 *   bones = unreal.SkeletonService.list_bones("/Game/SKM_Mannequin")
	 *   for bone in bones:
	 *       indent = "  " * bone.depth
	 *       print(f"{indent}{bone.bone_name}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Bones")
	static TArray<FBoneNodeInfo> ListBones(const FString& AssetPath);

	/**
	 * Get detailed information about a specific bone.
	 *
	 * @param AssetPath - Path to Skeleton or Skeletal Mesh
	 * @param BoneName - Name of the bone
	 * @param OutInfo - Output bone info
	 * @return True if bone was found
	 *
	 * Example:
	 *   info = unreal.SkeletonService.get_bone_info("/Game/SKM_Mannequin", "hand_r")
	 *   print(f"Parent: {info.parent_bone_name}, Children: {len(info.children)}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Bones")
	static bool GetBoneInfo(
		const FString& AssetPath,
		const FString& BoneName,
		FBoneNodeInfo& OutInfo);

	/**
	 * Get the parent bone name for a given bone.
	 *
	 * @param AssetPath - Path to Skeleton or Skeletal Mesh
	 * @param BoneName - Name of the bone
	 * @return Parent bone name, or empty string if root bone
	 *
	 * Example:
	 *   parent = unreal.SkeletonService.get_bone_parent("/Game/SKM_Mannequin", "hand_r")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Bones")
	static FString GetBoneParent(
		const FString& AssetPath,
		const FString& BoneName);

	/**
	 * Get child bone names for a given bone.
	 *
	 * @param AssetPath - Path to Skeleton or Skeletal Mesh
	 * @param BoneName - Name of the bone
	 * @param bRecursive - Whether to include all descendants
	 * @return Array of child bone names
	 *
	 * Example:
	 *   children = unreal.SkeletonService.get_bone_children("/Game/SKM_Mannequin", "spine_01", True)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Bones")
	static TArray<FString> GetBoneChildren(
		const FString& AssetPath,
		const FString& BoneName,
		bool bRecursive = false);

	/**
	 * Get the transform of a bone in local or component space.
	 *
	 * @param AssetPath - Path to Skeleton or Skeletal Mesh
	 * @param BoneName - Name of the bone
	 * @param bComponentSpace - If true, returns global transform; if false, local
	 * @return Bone transform
	 *
	 * Example:
	 *   transform = unreal.SkeletonService.get_bone_transform("/Game/SKM_Mannequin", "hand_r", True)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Bones")
	static FTransform GetBoneTransform(
		const FString& AssetPath,
		const FString& BoneName,
		bool bComponentSpace = false);

	/**
	 * Find the root bone of a skeleton.
	 *
	 * @param AssetPath - Path to Skeleton or Skeletal Mesh
	 * @return Root bone name
	 *
	 * Example:
	 *   root = unreal.SkeletonService.get_root_bone("/Game/SKM_Mannequin")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Bones")
	static FString GetRootBone(const FString& AssetPath);

	/**
	 * Find a bone by partial name match.
	 *
	 * @param AssetPath - Path to Skeleton or Skeletal Mesh
	 * @param SearchPattern - Partial bone name to search for
	 * @return Array of matching bone names
	 *
	 * Example:
	 *   hands = unreal.SkeletonService.find_bones("/Game/SKM_Mannequin", "hand")
	 *   # Returns: ["hand_l", "hand_r"]
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Bones")
	static TArray<FString> FindBones(
		const FString& AssetPath,
		const FString& SearchPattern);

	// ============================================================================
	// BONE MODIFICATION (via SkeletonModifier)
	// ============================================================================

	/**
	 * Add a new bone to a skeletal mesh.
	 * Note: Changes must be committed with CommitBoneChanges.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param BoneName - Name for the new bone
	 * @param ParentBoneName - Name of the parent bone
	 * @param LocalTransform - Transform relative to parent
	 * @return True if bone was added successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.add_bone(
	 *       "/Game/SKM_Mannequin",
	 *       "twist_upperarm_l",
	 *       "upperarm_l",
	 *       unreal.Transform(location=[15, 0, 0])
	 *   )
	 *   unreal.SkeletonService.commit_bone_changes("/Game/SKM_Mannequin")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Modification")
	static bool AddBone(
		const FString& SkeletalMeshPath,
		const FString& BoneName,
		const FString& ParentBoneName,
		const FTransform& LocalTransform);

	/**
	 * Remove a bone from a skeletal mesh.
	 * Note: Changes must be committed with CommitBoneChanges.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param BoneName - Name of the bone to remove
	 * @param bRemoveChildren - Whether to also remove child bones
	 * @return True if bone was removed successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.remove_bone("/Game/SKM_Mannequin", "extra_bone", True)
	 *   unreal.SkeletonService.commit_bone_changes("/Game/SKM_Mannequin")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Modification")
	static bool RemoveBone(
		const FString& SkeletalMeshPath,
		const FString& BoneName,
		bool bRemoveChildren = false);

	/**
	 * Rename a bone in a skeletal mesh.
	 * Note: Changes must be committed with CommitBoneChanges.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param OldBoneName - Current name of the bone
	 * @param NewBoneName - New name for the bone
	 * @return True if bone was renamed successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.rename_bone("/Game/SKM_Mannequin", "old_name", "new_name")
	 *   unreal.SkeletonService.commit_bone_changes("/Game/SKM_Mannequin")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Modification")
	static bool RenameBone(
		const FString& SkeletalMeshPath,
		const FString& OldBoneName,
		const FString& NewBoneName);

	/**
	 * Change the parent of a bone.
	 * Note: Changes must be committed with CommitBoneChanges.
	 *
	 * IMPORTANT: Reparenting changes the skeleton hierarchy, which may require
	 * user confirmation if the skeleton is shared by multiple assets. To avoid
	 * the confirmation dialog, first call DuplicateSkeleton to create a unique
	 * skeleton for this mesh.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param BoneName - Name of the bone to reparent
	 * @param NewParentName - Name of the new parent bone
	 * @return True if bone was reparented successfully
	 *
	 * Example:
	 *   # For shared skeletons, duplicate first:
	 *   unreal.SkeletonService.duplicate_skeleton("/Game/SKM_Mannequin", "/Game/SK_MyMannequin")
	 *   
	 *   # Then reparent:
	 *   unreal.SkeletonService.reparent_bone("/Game/SKM_Mannequin", "weapon_bone", "hand_r")
	 *   unreal.SkeletonService.commit_bone_changes("/Game/SKM_Mannequin")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Modification")
	static bool ReparentBone(
		const FString& SkeletalMeshPath,
		const FString& BoneName,
		const FString& NewParentName);

	/**
	 * Duplicate the skeleton of a skeletal mesh to a new path.
	 * This creates a unique skeleton copy that can be modified independently.
	 *
	 * NOTE: This only duplicates the skeleton asset. To use the new skeleton with
	 * the mesh, you must assign it manually in the Skeletal Mesh Editor or use
	 * the mesh's set_skeleton() method in Python.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh (to get its skeleton)
	 * @param NewSkeletonPath - Path where the new skeleton should be saved
	 * @return True if skeleton was duplicated successfully
	 *
	 * Example:
	 *   # Duplicate the skeleton
	 *   unreal.SkeletonService.duplicate_skeleton("/Game/SKM_Mannequin", "/Game/SK_MyMannequin")
	 *   # Then manually assign the new skeleton to your mesh in the editor
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Modification")
	static bool DuplicateSkeleton(
		const FString& SkeletalMeshPath,
		const FString& NewSkeletonPath);

	/**
	 * Set the transform of a bone.
	 * Note: Changes must be committed with CommitBoneChanges.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param BoneName - Name of the bone
	 * @param NewTransform - New local transform
	 * @param bMoveChildren - Whether to move children with the bone
	 * @return True if transform was set successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.set_bone_transform(
	 *       "/Game/SKM_Mannequin",
	 *       "spine_01",
	 *       unreal.Transform(scale=[1.1, 1.1, 1.1]),
	 *       False
	 *   )
	 *   unreal.SkeletonService.commit_bone_changes("/Game/SKM_Mannequin")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Modification")
	static bool SetBoneTransform(
		const FString& SkeletalMeshPath,
		const FString& BoneName,
		const FTransform& NewTransform,
		bool bMoveChildren = false);

	/**
	 * Commit pending bone changes to the skeletal mesh.
	 * This must be called after add/remove/rename/reparent operations.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param bForce - If true, bypasses the confirmation dialog for hierarchy changes
	 *                 and automatically uses FullMerge mode. Use for silent/automated operations.
	 * @return True if changes were committed successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.add_bone(...)
	 *   unreal.SkeletonService.add_bone(...)
	 *   unreal.SkeletonService.commit_bone_changes("/Game/SKM_Mannequin")
	 *   # For automated/silent operations:
	 *   unreal.SkeletonService.commit_bone_changes("/Game/SKM_Mannequin", True)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Modification")
	static bool CommitBoneChanges(const FString& SkeletalMeshPath, bool bForce = false);

	/**
	 * Check if a skeleton is shared by multiple skeletal meshes or other assets.
	 * 
	 * This is useful to determine if a reparent_bone operation will trigger a 
	 * confirmation dialog. Hierarchy-breaking changes (like reparent) on shared 
	 * skeletons require user confirmation because they affect all assets using 
	 * that skeleton.
	 *
	 * @param SkeletalMeshPath - Path to a Skeletal Mesh
	 * @return True if the skeleton is used by other assets besides this mesh
	 *
	 * Example:
	 *   is_shared = unreal.SkeletonService.is_skeleton_shared("/Game/SKM_Mannequin")
	 *   if is_shared:
	 *       print("Warning: Reparent operations will require user confirmation")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Modification")
	static bool IsSkeletonShared(const FString& SkeletalMeshPath);

	// ============================================================================
	// SOCKET MANAGEMENT
	// ============================================================================

	/**
	 * List all sockets on a Skeletal Mesh.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @return Array of socket information
	 *
	 * Example:
	 *   sockets = unreal.SkeletonService.list_sockets("/Game/SKM_Mannequin")
	 *   for s in sockets:
	 *       print(f"{s.socket_name} -> {s.bone_name}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Sockets")
	static TArray<FMeshSocketInfo> ListSockets(const FString& SkeletalMeshPath);

	/**
	 * Get detailed information about a specific socket.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param SocketName - Name of the socket
	 * @param OutInfo - Output socket info
	 * @return True if socket was found
	 *
	 * Example:
	 *   info = unreal.SkeletonService.get_socket_info("/Game/SKM_Mannequin", "Weapon_R")
	 *   print(f"Bone: {info.bone_name}, Location: {info.relative_location}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Sockets")
	static bool GetSocketInfo(
		const FString& SkeletalMeshPath,
		const FString& SocketName,
		FMeshSocketInfo& OutInfo);

	/**
	 * Add a new socket to a Skeletal Mesh.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param SocketName - Name for the new socket
	 * @param BoneName - Bone to attach socket to
	 * @param RelativeLocation - Position relative to bone
	 * @param RelativeRotation - Rotation relative to bone
	 * @param RelativeScale - Scale of socket
	 * @param bAddToSkeleton - If true, socket is shared across meshes using this skeleton
	 * @return True if socket was added successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.add_socket(
	 *       "/Game/SKM_Mannequin",
	 *       "Weapon_R",
	 *       "hand_r",
	 *       unreal.Vector(10, 0, 0),
	 *       unreal.Rotator(0, 0, 90),
	 *       unreal.Vector(1, 1, 1),
	 *       True  # Add to skeleton for sharing
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Sockets")
	static bool AddSocket(
		const FString& SkeletalMeshPath,
		const FString& SocketName,
		const FString& BoneName,
		FVector RelativeLocation,
		FRotator RelativeRotation,
		FVector RelativeScale,
		bool bAddToSkeleton = false);

	/**
	 * Remove a socket from a Skeletal Mesh.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param SocketName - Name of the socket to remove
	 * @return True if socket was removed successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.remove_socket("/Game/SKM_Mannequin", "OldSocket")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Sockets")
	static bool RemoveSocket(
		const FString& SkeletalMeshPath,
		const FString& SocketName);

	/**
	 * Rename a socket.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param OldName - Current socket name
	 * @param NewName - New socket name
	 * @return True if socket was renamed successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.rename_socket("/Game/SKM_Mannequin", "Socket1", "Weapon_R")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Sockets")
	static bool RenameSocket(
		const FString& SkeletalMeshPath,
		const FString& OldName,
		const FString& NewName);

	/**
	 * Update socket transform.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param SocketName - Name of the socket
	 * @param RelativeLocation - New position relative to bone
	 * @param RelativeRotation - New rotation relative to bone
	 * @param RelativeScale - New scale
	 * @return True if socket was updated successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.set_socket_transform(
	 *       "/Game/SKM_Mannequin",
	 *       "Weapon_R",
	 *       unreal.Vector(12, 0, 0),
	 *       unreal.Rotator(0, 0, 90),
	 *       unreal.Vector(1, 1, 1)
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Sockets")
	static bool SetSocketTransform(
		const FString& SkeletalMeshPath,
		const FString& SocketName,
		const FVector& RelativeLocation,
		const FRotator& RelativeRotation,
		const FVector& RelativeScale);

	/**
	 * Change which bone a socket is attached to.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param SocketName - Name of the socket
	 * @param NewBoneName - New parent bone
	 * @return True if socket was reparented successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.set_socket_bone("/Game/SKM_Mannequin", "Weapon_R", "lowerarm_r")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Sockets")
	static bool SetSocketBone(
		const FString& SkeletalMeshPath,
		const FString& SocketName,
		const FString& NewBoneName);

	// ============================================================================
	// RETARGETING
	// ============================================================================

	/**
	 * Get the list of compatible skeletons for animation sharing.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @return Array of compatible skeleton paths
	 *
	 * Example:
	 *   compatible = unreal.SkeletonService.get_compatible_skeletons("/Game/SK_Mannequin")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Retargeting")
	static TArray<FString> GetCompatibleSkeletons(const FString& SkeletonPath);

	/**
	 * Add a compatible skeleton for animation sharing.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param CompatibleSkeletonPath - Path to the skeleton to add as compatible
	 * @return True if skeleton was added successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.add_compatible_skeleton(
	 *       "/Game/SK_Mannequin",
	 *       "/Game/MetaHumans/SK_MetaHuman"
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Retargeting")
	static bool AddCompatibleSkeleton(
		const FString& SkeletonPath,
		const FString& CompatibleSkeletonPath);

	/**
	 * Get the retargeting mode for a bone.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param BoneName - Name of the bone
	 * @return Retargeting mode string: "Animation", "Skeleton", "AnimatedZeroLength", "OrientAndScale"
	 *
	 * Example:
	 *   mode = unreal.SkeletonService.get_bone_retargeting_mode("/Game/SK_Mannequin", "pelvis")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Retargeting")
	static FString GetBoneRetargetingMode(
		const FString& SkeletonPath,
		const FString& BoneName);

	/**
	 * Set the retargeting mode for a bone.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param BoneName - Name of the bone
	 * @param Mode - Retargeting mode: "Animation", "Skeleton", "AnimatedZeroLength", "OrientAndScale"
	 * @return True if mode was set successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.set_bone_retargeting_mode("/Game/SK_Mannequin", "pelvis", "Skeleton")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Retargeting")
	static bool SetBoneRetargetingMode(
		const FString& SkeletonPath,
		const FString& BoneName,
		const FString& Mode);

	// ============================================================================
	// CURVE METADATA
	// ============================================================================

	/**
	 * List all curve metadata in a skeleton.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @return Array of curve metadata information
	 *
	 * Example:
	 *   curves = unreal.SkeletonService.list_curve_metadata("/Game/SK_Mannequin")
	 *   for c in curves:
	 *       print(f"{c.curve_name} - Morph: {c.is_morph_target}, Material: {c.is_material}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Curves")
	static TArray<FCurveMetaInfo> ListCurveMetaData(const FString& SkeletonPath);

	/**
	 * Add a curve metadata entry to a skeleton.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param CurveName - Name for the new curve
	 * @return True if curve was added successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.add_curve_metadata("/Game/SK_Mannequin", "BrowsUp")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Curves")
	static bool AddCurveMetaData(
		const FString& SkeletonPath,
		const FString& CurveName);

	/**
	 * Remove a curve metadata entry from a skeleton.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param CurveName - Name of the curve to remove
	 * @return True if curve was removed successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.remove_curve_metadata("/Game/SK_Mannequin", "OldCurve")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Curves")
	static bool RemoveCurveMetaData(
		const FString& SkeletonPath,
		const FString& CurveName);

	/**
	 * Rename a curve metadata entry.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param OldName - Current curve name
	 * @param NewName - New curve name
	 * @return True if curve was renamed successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.rename_curve_metadata("/Game/SK_Mannequin", "Old", "New")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Curves")
	static bool RenameCurveMetaData(
		const FString& SkeletonPath,
		const FString& OldName,
		const FString& NewName);

	/**
	 * Set whether a curve is used for morph targets.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param CurveName - Name of the curve
	 * @param bIsMorphTarget - Whether curve drives morph targets
	 * @return True if property was set successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.set_curve_morph_target("/Game/SK_Mannequin", "BrowsUp", True)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Curves")
	static bool SetCurveMorphTarget(
		const FString& SkeletonPath,
		const FString& CurveName,
		bool bIsMorphTarget);

	/**
	 * Set whether a curve is used for material parameters.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param CurveName - Name of the curve
	 * @param bIsMaterial - Whether curve drives material parameters
	 * @return True if property was set successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.set_curve_material("/Game/SK_Mannequin", "EyeGlow", True)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Curves")
	static bool SetCurveMaterial(
		const FString& SkeletonPath,
		const FString& CurveName,
		bool bIsMaterial);

	// ============================================================================
	// BLEND PROFILES
	// ============================================================================

	/**
	 * List all blend profiles in a skeleton.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @return Array of blend profile names
	 *
	 * Example:
	 *   profiles = unreal.SkeletonService.list_blend_profiles("/Game/SK_Mannequin")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|BlendProfiles")
	static TArray<FString> ListBlendProfiles(const FString& SkeletonPath);

	/**
	 * Get detailed information about a blend profile.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param ProfileName - Name of the blend profile
	 * @param OutInfo - Output blend profile data
	 * @return True if profile was found
	 *
	 * Example:
	 *   profile = unreal.SkeletonService.get_blend_profile("/Game/SK_Mannequin", "UpperBody")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|BlendProfiles")
	static bool GetBlendProfile(
		const FString& SkeletonPath,
		const FString& ProfileName,
		FBlendProfileData& OutInfo);

	/**
	 * Create a new blend profile.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param ProfileName - Name for the new profile
	 * @return True if profile was created successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.create_blend_profile("/Game/SK_Mannequin", "UpperBody")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|BlendProfiles")
	static bool CreateBlendProfile(
		const FString& SkeletonPath,
		const FString& ProfileName);

	/**
	 * Set the blend scale for a bone in a blend profile.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param ProfileName - Name of the blend profile
	 * @param BoneName - Name of the bone
	 * @param BlendScale - Blend weight scale (0.0 to 1.0)
	 * @return True if scale was set successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.set_blend_profile_bone("/Game/SK_Mannequin", "UpperBody", "spine_01", 1.0)
	 *   unreal.SkeletonService.set_blend_profile_bone("/Game/SK_Mannequin", "UpperBody", "pelvis", 0.0)
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|BlendProfiles")
	static bool SetBlendProfileBone(
		const FString& SkeletonPath,
		const FString& ProfileName,
		const FString& BoneName,
		float BlendScale);

	// ============================================================================
	// SKELETAL MESH PROPERTIES
	// ============================================================================

	/**
	 * Set the Physics Asset for a Skeletal Mesh.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param PhysicsAssetPath - Path to the Physics Asset (empty to clear)
	 * @return True if physics asset was set successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.set_physics_asset(
	 *       "/Game/SKM_Character",
	 *       "/Game/Physics/PA_Character"
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Properties")
	static bool SetPhysicsAsset(
		const FString& SkeletalMeshPath,
		const FString& PhysicsAssetPath);

	/**
	 * Set the Post Process Animation Blueprint for a Skeletal Mesh.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @param AnimBlueprintPath - Path to the AnimBP (empty to clear)
	 * @return True if AnimBP was set successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.set_post_process_anim_blueprint(
	 *       "/Game/SKM_Character",
	 *       "/Game/ABP_PostProcess"
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Properties")
	static bool SetPostProcessAnimBlueprint(
		const FString& SkeletalMeshPath,
		const FString& AnimBlueprintPath);

	/**
	 * List morph targets on a Skeletal Mesh.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh
	 * @return Array of morph target names
	 *
	 * Example:
	 *   morphs = unreal.SkeletonService.list_morph_targets("/Game/SKM_Character")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Properties")
	static TArray<FString> ListMorphTargets(const FString& SkeletalMeshPath);

	// ============================================================================
	// EDITOR NAVIGATION
	// ============================================================================

	/**
	 * Open a Skeleton in the Skeleton Editor.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @return True if editor was opened successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.open_skeleton_editor("/Game/SK_Mannequin")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Editor")
	static bool OpenSkeletonEditor(const FString& SkeletonPath);

	/**
	 * Open a Skeletal Mesh in the Skeletal Mesh Editor.
	 *
	 * @param SkeletalMeshPath - Path to the Skeletal Mesh asset
	 * @return True if editor was opened successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.open_skeletal_mesh_editor("/Game/SKM_Mannequin")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Editor")
	static bool OpenSkeletalMeshEditor(const FString& SkeletalMeshPath);

	/**
	 * Save changes to a Skeleton or Skeletal Mesh.
	 *
	 * @param AssetPath - Path to the Skeleton or Skeletal Mesh
	 * @return True if asset was saved successfully
	 *
	 * Example:
	 *   unreal.SkeletonService.save_asset("/Game/SK_Mannequin")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Editor")
	static bool SaveAsset(const FString& AssetPath);

	// ============================================================================
	// SKELETON PROFILES & CONSTRAINTS
	// ============================================================================

	/**
	 * Create or refresh a skeleton profile with hierarchy, constraints, and metadata.
	 * Builds a comprehensive profile that can be cached for repeated use.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param OutProfile - Output skeleton profile
	 * @return True if profile was created successfully
	 *
	 * Example:
	 *   profile = unreal.SkeletonService.create_skeleton_profile("/Game/SK_Mannequin")
	 *   print(f"Bones: {profile.bone_count}, Constraints: {len(profile.constraints)}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Profiles")
	static bool CreateSkeletonProfile(
		const FString& SkeletonPath,
		FSkeletonProfile& OutProfile);

	/**
	 * Get an existing skeleton profile from cache.
	 * Returns an empty profile if not cached - use CreateSkeletonProfile first.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param OutProfile - Output skeleton profile
	 * @return True if profile was found in cache
	 *
	 * Example:
	 *   profile = unreal.SkeletonService.get_skeleton_profile("/Game/SK_Mannequin")
	 *   if not profile.is_valid:
	 *       profile = unreal.SkeletonService.create_skeleton_profile("/Game/SK_Mannequin")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Profiles")
	static bool GetSkeletonProfile(
		const FString& SkeletonPath,
		FSkeletonProfile& OutProfile);

	/**
	 * Learn bone rotation constraints from existing animations for this skeleton.
	 * Analyzes all compatible animations to derive rotation ranges per bone.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param MaxAnimations - Maximum animations to analyze (0 = all)
	 * @param SamplesPerAnimation - Frames to sample per animation (0 = all frames)
	 * @param OutConstraints - Output learned constraints info
	 * @return True if learning was successful
	 *
	 * Example:
	 *   constraints = unreal.SkeletonService.learn_from_animations("/Game/SK_Mannequin", 50, 10)
	 *   print(f"Analyzed {constraints.animation_count} animations")
	 *   for bone_range in constraints.bone_ranges:
	 *       print(f"{bone_range.bone_name}: {bone_range.min_rotation} to {bone_range.max_rotation}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Profiles")
	static bool LearnFromAnimations(
		const FString& SkeletonPath,
		int32 MaxAnimations,
		int32 SamplesPerAnimation,
		FLearnedConstraintsInfo& OutConstraints);

	/**
	 * Get previously learned constraints for a skeleton.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param OutConstraints - Output learned constraints info
	 * @return True if learned constraints exist
	 *
	 * Example:
	 *   constraints = unreal.SkeletonService.get_learned_constraints("/Game/SK_Mannequin")
	 *   if constraints:
	 *       for br in constraints.bone_ranges:
	 *           print(f"{br.bone_name}: samples={br.sample_count}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Profiles")
	static bool GetLearnedConstraints(
		const FString& SkeletonPath,
		FLearnedConstraintsInfo& OutConstraints);

	/**
	 * Set custom constraints for a bone.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param BoneName - Name of the bone
	 * @param MinRotation - Minimum rotation limits (Euler degrees)
	 * @param MaxRotation - Maximum rotation limits (Euler degrees)
	 * @param bIsHinge - Whether this is a hinge joint (single axis)
	 * @param HingeAxis - Axis for hinge (0=X, 1=Y, 2=Z) if bIsHinge
	 * @return True if constraints were set successfully
	 *
	 * Example:
	 *   # Set elbow as hinge joint (only bends on Y axis)
	 *   unreal.SkeletonService.set_bone_constraints(
	 *       "/Game/SK_Mannequin",
	 *       "lowerarm_r",
	 *       unreal.Rotator(0, 0, 0),      # Min
	 *       unreal.Rotator(0, 145, 0),    # Max (145 degree bend)
	 *       True,                          # Is hinge
	 *       1                              # Y axis (pitch)
	 *   )
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Profiles")
	static bool SetBoneConstraints(
		const FString& SkeletonPath,
		const FString& BoneName,
		const FRotator& MinRotation,
		const FRotator& MaxRotation,
		bool bIsHinge = false,
		int32 HingeAxis = 1);

	/**
	 * Validate a bone rotation against constraints.
	 *
	 * @param SkeletonPath - Path to the Skeleton asset
	 * @param BoneName - Name of the bone
	 * @param Rotation - Rotation to validate (local space Euler degrees)
	 * @param bUseLearnedConstraints - Whether to use learned ranges instead of manual constraints
	 * @param OutResult - Validation result with clamped values if invalid
	 * @return True if validation completed (check OutResult.bIsValid for pass/fail)
	 *
	 * Example:
	 *   result = unreal.SkeletonService.validate_bone_rotation(
	 *       "/Game/SK_Mannequin",
	 *       "lowerarm_r",
	 *       unreal.Rotator(0, 160, 0),  # Exceeds typical elbow range
	 *       True
	 *   )
	 *   if not result.is_valid:
	 *       print(f"Violation: {result.violation_type} - {result.message}")
	 *       print(f"Use clamped value: {result.clamped_rotation}")
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Skeleton|Profiles")
	static bool ValidateBoneRotation(
		const FString& SkeletonPath,
		const FString& BoneName,
		const FRotator& Rotation,
		bool bUseLearnedConstraints,
		FBoneValidationResult& OutResult);

private:
	// ============================================================================
	// PRIVATE HELPERS
	// ============================================================================

	/** Load a Skeleton asset from path */
	static class USkeleton* LoadSkeleton(const FString& SkeletonPath);

	/** Load a Skeletal Mesh asset from path */
	static class USkeletalMesh* LoadSkeletalMesh(const FString& SkeletalMeshPath);

	/** Get skeleton from either a Skeleton or SkeletalMesh path */
	static class USkeleton* GetSkeletonFromAsset(const FString& AssetPath);

	/** Get reference skeleton for bone operations */
	static const struct FReferenceSkeleton* GetReferenceSkeleton(const FString& AssetPath);

	/** Convert retargeting mode enum to string */
	static FString RetargetingModeToString(EBoneTranslationRetargetingMode::Type Mode);

	/** Convert string to retargeting mode enum */
	static EBoneTranslationRetargetingMode::Type StringToRetargetingMode(const FString& ModeString);

	/** Get or create skeleton modifier for a mesh */
	static class USkeletonModifier* GetSkeletonModifier(const FString& SkeletalMeshPath);

	/** Clear cached skeleton modifier */
	static void ClearSkeletonModifier(const FString& SkeletalMeshPath);

	/** Map of active skeleton modifiers by mesh path - using TStrongObjectPtr for GC safety */
	static TMap<FString, TStrongObjectPtr<class USkeletonModifier>> ActiveModifiers;
};
