// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UAnimMontageService.generated.h"

// ============================================================================
// DATA TRANSFER OBJECTS (DTOs)
// ============================================================================

/**
 * Comprehensive information about an Animation Montage asset
 */
USTRUCT(BlueprintType)
struct FMontageInfo
{
	GENERATED_BODY()

	/** Asset path of the montage */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString MontagePath;

	/** Display name */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString MontageName;

	/** Associated skeleton path */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString SkeletonPath;

	/** Total duration in seconds */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float Duration = 0.0f;

	/** Number of sections */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 SectionCount = 0;

	/** Number of slot tracks */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 SlotTrackCount = 0;

	/** Number of notifies */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 NotifyCount = 0;

	/** Number of branching points */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 BranchingPointCount = 0;

	/** Blend in time */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float BlendInTime = 0.0f;

	/** Blend out time */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float BlendOutTime = 0.0f;

	/** Blend out trigger time (-1 = auto) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float BlendOutTriggerTime = -1.0f;

	/** Whether root motion translation is enabled */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bEnableRootMotionTranslation = true;

	/** Whether root motion rotation is enabled */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bEnableRootMotionRotation = true;

	/** List of slot names used */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	TArray<FString> SlotNames;
};

/**
 * Blend settings for a montage (VibeUE wrapper)
 */
USTRUCT(BlueprintType)
struct FVibeMontageBlendSettings
{
	GENERATED_BODY()

	/** Blend in time */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float BlendInTime = 0.25f;

	/** Blend in curve type */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString BlendInOption;

	/** Blend out time */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float BlendOutTime = 0.25f;

	/** Blend out curve type */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString BlendOutOption;

	/** When to trigger blend out (-1 = auto at end minus blend time) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float BlendOutTriggerTime = -1.0f;

	/** Custom blend in curve (if using custom option) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString BlendInCurvePath;

	/** Custom blend out curve (if using custom option) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString BlendOutCurvePath;
};

/**
 * Information about a montage section
 */
USTRUCT(BlueprintType)
struct FMontageSectionInfo
{
	GENERATED_BODY()

	/** Section name */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString SectionName;

	/** Section index */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 SectionIndex = 0;

	/** Start time in seconds */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float StartTime = 0.0f;

	/** End time in seconds */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float EndTime = 0.0f;

	/** Section duration */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float Duration = 0.0f;

	/** Next section name (empty if none linked) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString NextSectionName;

	/** Whether section loops to itself */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bLoops = false;

	/** Segment count in this section */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 SegmentCount = 0;
};

/**
 * Section linking information
 */
USTRUCT(BlueprintType)
struct FSectionLink
{
	GENERATED_BODY()

	/** Source section name */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString FromSection;

	/** Target section name */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString ToSection;

	/** Whether this is a self-loop */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bIsLoop = false;
};

/**
 * Information about a slot animation track
 */
USTRUCT(BlueprintType)
struct FSlotTrackInfo
{
	GENERATED_BODY()

	/** Track index */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 TrackIndex = 0;

	/** Slot name for this track */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString SlotName;

	/** Number of animation segments */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 SegmentCount = 0;

	/** Total duration of segments */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float TotalDuration = 0.0f;
};

/**
 * Information about an animation segment within a slot track
 */
USTRUCT(BlueprintType)
struct FAnimSegmentInfo
{
	GENERATED_BODY()

	/** Segment index within the track */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 SegmentIndex = 0;

	/** Path to the source animation sequence */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString AnimSequencePath;

	/** Animation name */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString AnimName;

	/** Start time in the montage timeline */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float StartTime = 0.0f;

	/** Duration in the montage */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float Duration = 0.0f;

	/** Playback rate multiplier */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float PlayRate = 1.0f;

	/** Start position within the source animation */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float AnimStartPos = 0.0f;

	/** End position within the source animation */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float AnimEndPos = 0.0f;

	/** Number of loops (0 = use full length) */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 LoopCount = 0;

	/** Whether this segment loops within its duration */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bLoops = false;
};

/**
 * Information about a branching point
 */
USTRUCT(BlueprintType)
struct FBranchingPointInfo
{
	GENERATED_BODY()

	/** Index of the branching point */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 Index = 0;

	/** Notify name for this branching point */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString NotifyName;

	/** Trigger time in seconds */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	float TriggerTime = 0.0f;

	/** Section this branching point is in */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString SectionName;
};

/**
 * Information about a montage notify event
 */
USTRUCT(BlueprintType)
struct FMontageNotifyInfo
{
	GENERATED_BODY()

	/** Index of the notify in the montage */
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

	/** Whether this is a branching point */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	bool bIsBranchingPoint = false;

	/** Track index in the notify panel */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	int32 TrackIndex = 0;

	/** Section name this notify is linked to */
	UPROPERTY(BlueprintReadWrite, Category = "Animation")
	FString LinkedSectionName;
};

// ============================================================================
// SERVICE CLASS
// ============================================================================

/**
 * Animation Montage service exposed directly to Python.
 *
 * This service provides comprehensive CRUD operations for Animation Montage assets
 * including section management, slot tracks, animation segments, branching points,
 * and blend settings. These operations require C++ because Python's set_editor_property()
 * returns read-only copies of internal TArrays like CompositeSections and SlotAnimTracks.
 *
 * IMPORTANT: All montage_path parameters require the FULL asset path (package_name from AssetData),
 * NOT the folder path (package_path). For example:
 *   - CORRECT: "/Game/Characters/Mannequin/Montages/AM_Attack"
 *   - WRONG:   "/Game/Characters/Mannequin/Montages" (this is a folder, not an asset)
 *
 * Python Usage:
 *   import unreal
 *
 *   # List all montages for a skeleton
 *   montages = unreal.AnimMontageService.find_montages_for_skeleton("/Game/SK_Mannequin")
 *   for m in montages:
 *       print(f"{m.montage_name}: {m.duration}s, {m.section_count} sections")
 *
 *   # Create a montage from an animation
 *   path = unreal.AnimMontageService.create_montage_from_animation(
 *       "/Game/Animations/Attack", "/Game/Montages", "AM_Attack")
 *
 *   # Add sections for combo system
 *   unreal.AnimMontageService.add_section(path, "WindUp", 0.0)
 *   unreal.AnimMontageService.add_section(path, "Attack", 0.3)
 *   unreal.AnimMontageService.add_section(path, "Recovery", 0.8)
 *
 *   # Link sections for combo flow
 *   unreal.AnimMontageService.set_next_section(path, "WindUp", "Attack")
 *   unreal.AnimMontageService.set_next_section(path, "Attack", "Recovery")
 *
 *   # Add branching point for combo input window
 *   unreal.AnimMontageService.add_branching_point(path, "ComboWindow", 0.6)
 *
 * @note All methods are static and thread-safe
 * @note For notifies on montages, you can use AnimSequenceService methods (inherited)
 *
 * **C++ Source:**
 *
 * - **Plugin**: VibeUE
 * - **Module**: VibeUE
 * - **File**: UAnimMontageService.h
 */
UCLASS(BlueprintType)
class VIBEUE_API UAnimMontageService : public UObject
{
	GENERATED_BODY()

public:
	// ============================================================================
	// MONTAGE DISCOVERY
	// ============================================================================

	/**
	 * List all Animation Montage assets in a path.
	 *
	 * @param SearchPath - Path to search for montages (default: "/Game")
	 * @param SkeletonFilter - Optional skeleton path to filter by
	 * @return Array of montage info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
	static TArray<FMontageInfo> ListMontages(
		const FString& SearchPath = TEXT("/Game"),
		const FString& SkeletonFilter = TEXT(""));

	/**
	 * Get detailed information about a montage.
	 *
	 * @param MontagePath - Full path to the montage asset
	 * @param OutInfo - Output montage info
	 * @return True if montage was found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
	static bool GetMontageInfo(
		const FString& MontagePath,
		FMontageInfo& OutInfo);

	/**
	 * Find all montages compatible with a specific skeleton.
	 *
	 * @param SkeletonPath - Path to the skeleton asset
	 * @return Array of montage info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
	static TArray<FMontageInfo> FindMontagesForSkeleton(const FString& SkeletonPath);

	/**
	 * Find all montages that use a specific animation sequence.
	 *
	 * @param AnimSequencePath - Path to the animation sequence
	 * @return Array of montage info structs containing that animation
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
	static TArray<FMontageInfo> FindMontagesUsingAnimation(const FString& AnimSequencePath);

	// ============================================================================
	// MONTAGE PROPERTIES
	// ============================================================================

	/**
	 * Get the total duration of a montage in seconds.
	 *
	 * @param MontagePath - Path to the montage asset
	 * @return Duration in seconds, or -1 if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
	static float GetMontageLength(const FString& MontagePath);

	/**
	 * Get the skeleton asset path for a montage.
	 *
	 * @param MontagePath - Path to the montage asset
	 * @return Skeleton asset path, or empty if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
	static FString GetMontageSkeleton(const FString& MontagePath);

	/**
	 * Set blend in settings.
	 *
	 * @param MontagePath - Path to montage
	 * @param BlendTime - Blend duration in seconds
	 * @param BlendOption - Blend curve type as string ("Linear", "Cubic", "HermiteCubic", "Sinusoidal", "QuadraticInOut", "CubicInOut", "QuarticInOut", "QuinticInOut", "CircularIn", "CircularOut", "CircularInOut", "ExpIn", "ExpOut", "ExpInOut", "Custom")
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
	static bool SetBlendIn(
		const FString& MontagePath,
		float BlendTime,
		const FString& BlendOption = TEXT("Linear"));

	/**
	 * Set blend out settings.
	 *
	 * @param MontagePath - Path to montage
	 * @param BlendTime - Blend duration in seconds
	 * @param BlendOption - Blend curve type as string
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
	static bool SetBlendOut(
		const FString& MontagePath,
		float BlendTime,
		const FString& BlendOption = TEXT("Linear"));

	/**
	 * Get current blend settings.
	 *
	 * @param MontagePath - Path to montage
	 * @param OutSettings - Output blend settings
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
	static bool GetBlendSettings(
		const FString& MontagePath,
		FVibeMontageBlendSettings& OutSettings);

	/**
	 * Set when blend out begins (-1 = auto, based on blend out time).
	 *
	 * @param MontagePath - Path to montage
	 * @param TriggerTime - Time before end to trigger blend out (-1 for auto)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
	static bool SetBlendOutTriggerTime(const FString& MontagePath, float TriggerTime);

	// ============================================================================
	// SECTION MANAGEMENT (Requires C++ - Python cannot modify TArrays)
	// ============================================================================

	/**
	 * List all sections in a montage.
	 *
	 * @param MontagePath - Path to montage
	 * @return Array of section info structs, ordered by start time
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static TArray<FMontageSectionInfo> ListSections(const FString& MontagePath);

	/**
	 * Get info for a specific section by name.
	 *
	 * @param MontagePath - Path to montage
	 * @param SectionName - Name of the section
	 * @param OutInfo - Output section info
	 * @return True if section was found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static bool GetSectionInfo(
		const FString& MontagePath,
		const FString& SectionName,
		FMontageSectionInfo& OutInfo);

	/**
	 * Get section index at a specific time.
	 *
	 * @param MontagePath - Path to montage
	 * @param Time - Time in seconds
	 * @return Section index, or -1 if time is out of range
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static int32 GetSectionIndexAtTime(const FString& MontagePath, float Time);

	/**
	 * Get section name at a specific time.
	 *
	 * @param MontagePath - Path to montage
	 * @param Time - Time in seconds
	 * @return Section name, or empty string if time is out of range
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static FString GetSectionNameAtTime(const FString& MontagePath, float Time);

	/**
	 * Add a new section to the montage.
	 *
	 * @param MontagePath - Path to montage
	 * @param SectionName - Name for the new section (must be unique)
	 * @param StartTime - Start time in seconds (must be within montage duration)
	 * @return True if section was added successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static bool AddSection(
		const FString& MontagePath,
		const FString& SectionName,
		float StartTime);

	/**
	 * Remove a section from the montage.
	 *
	 * @param MontagePath - Path to montage
	 * @param SectionName - Name of the section to remove
	 * @return True if successful
	 * @note Cannot remove the "Default" section if it's the only one
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static bool RemoveSection(const FString& MontagePath, const FString& SectionName);

	/**
	 * Rename an existing section.
	 *
	 * @param MontagePath - Path to montage
	 * @param OldName - Current section name
	 * @param NewName - New section name (must be unique)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static bool RenameSection(
		const FString& MontagePath,
		const FString& OldName,
		const FString& NewName);

	/**
	 * Move a section to a new start time.
	 *
	 * @param MontagePath - Path to montage
	 * @param SectionName - Name of the section
	 * @param NewStartTime - New start time in seconds
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static bool SetSectionStartTime(
		const FString& MontagePath,
		const FString& SectionName,
		float NewStartTime);

	/**
	 * Get the duration of a specific section.
	 *
	 * @param MontagePath - Path to montage
	 * @param SectionName - Name of the section
	 * @return Section duration, or -1 if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static float GetSectionLength(const FString& MontagePath, const FString& SectionName);

	// ============================================================================
	// SECTION LINKING (BRANCHING)
	// ============================================================================

	/**
	 * Get the next section that plays after the specified section.
	 *
	 * @param MontagePath - Path to montage
	 * @param SectionName - Name of the section
	 * @return Next section name, or empty string if section ends montage
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static FString GetNextSection(const FString& MontagePath, const FString& SectionName);

	/**
	 * Link a section to play another section when it completes.
	 *
	 * @param MontagePath - Path to montage
	 * @param SectionName - Source section
	 * @param NextSectionName - Section to play next (empty string = end montage)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static bool SetNextSection(
		const FString& MontagePath,
		const FString& SectionName,
		const FString& NextSectionName);

	/**
	 * Set a section to loop to itself.
	 *
	 * @param MontagePath - Path to montage
	 * @param SectionName - Name of the section
	 * @param bLoop - True to loop, false to clear loop (section ends montage)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static bool SetSectionLoop(
		const FString& MontagePath,
		const FString& SectionName,
		bool bLoop);

	/**
	 * Get all section links in the montage.
	 *
	 * @param MontagePath - Path to montage
	 * @return Array of section link info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static TArray<FSectionLink> GetAllSectionLinks(const FString& MontagePath);

	/**
	 * Clear the link from a section (section will end the montage).
	 *
	 * @param MontagePath - Path to montage
	 * @param SectionName - Name of the section
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
	static bool ClearSectionLink(const FString& MontagePath, const FString& SectionName);

	// ============================================================================
	// SLOT TRACK MANAGEMENT (Requires C++ - Python cannot modify TArrays)
	// ============================================================================

	/**
	 * List all slot tracks in a montage.
	 *
	 * @param MontagePath - Path to montage
	 * @return Array of slot track info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Slots")
	static TArray<FSlotTrackInfo> ListSlotTracks(const FString& MontagePath);

	/**
	 * Get info for a specific slot track.
	 *
	 * @param MontagePath - Path to montage
	 * @param TrackIndex - Index of the track
	 * @param OutInfo - Output track info
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Slots")
	static bool GetSlotTrackInfo(
		const FString& MontagePath,
		int32 TrackIndex,
		FSlotTrackInfo& OutInfo);

	/**
	 * Add a new slot track to the montage.
	 *
	 * @param MontagePath - Path to montage
	 * @param SlotName - Name of the animation slot (must exist in skeleton)
	 * @return Index of the new track, or -1 on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Slots")
	static int32 AddSlotTrack(const FString& MontagePath, const FString& SlotName);

	/**
	 * Remove a slot track from the montage.
	 *
	 * @param MontagePath - Path to montage
	 * @param TrackIndex - Index of the track to remove
	 * @return True if successful
	 * @note Cannot remove the last slot track
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Slots")
	static bool RemoveSlotTrack(const FString& MontagePath, int32 TrackIndex);

	/**
	 * Change the slot name for a track.
	 *
	 * @param MontagePath - Path to montage
	 * @param TrackIndex - Index of the track
	 * @param NewSlotName - New slot name
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Slots")
	static bool SetSlotName(
		const FString& MontagePath,
		int32 TrackIndex,
		const FString& NewSlotName);

	/**
	 * Get all unique slot names used in the montage.
	 *
	 * @param MontagePath - Path to montage
	 * @return Array of slot names
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Slots")
	static TArray<FString> GetAllUsedSlotNames(const FString& MontagePath);

	// ============================================================================
	// ANIMATION SEGMENTS (Multiple Animations per Montage)
	// ============================================================================

	/**
	 * List all animation segments in a slot track.
	 *
	 * @param MontagePath - Path to montage
	 * @param TrackIndex - Index of the slot track
	 * @return Array of segment info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
	static TArray<FAnimSegmentInfo> ListAnimSegments(const FString& MontagePath, int32 TrackIndex);

	/**
	 * Get info for a specific animation segment.
	 *
	 * @param MontagePath - Path to montage
	 * @param TrackIndex - Index of the slot track
	 * @param SegmentIndex - Index of the segment
	 * @param OutInfo - Output segment info
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
	static bool GetAnimSegmentInfo(
		const FString& MontagePath,
		int32 TrackIndex,
		int32 SegmentIndex,
		FAnimSegmentInfo& OutInfo);

	/**
	 * Add an animation segment to a slot track.
	 *
	 * @param MontagePath - Path to montage
	 * @param TrackIndex - Slot track to add to
	 * @param AnimSequencePath - Path to the animation sequence
	 * @param StartTime - Start position in montage timeline
	 * @param PlayRate - Playback rate multiplier (1.0 = normal)
	 * @return Index of the new segment, or -1 on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
	static int32 AddAnimSegment(
		const FString& MontagePath,
		int32 TrackIndex,
		const FString& AnimSequencePath,
		float StartTime,
		float PlayRate = 1.0f);

	/**
	 * Remove an animation segment from a slot track.
	 *
	 * @param MontagePath - Path to montage
	 * @param TrackIndex - Index of the slot track
	 * @param SegmentIndex - Index of the segment to remove
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
	static bool RemoveAnimSegment(
		const FString& MontagePath,
		int32 TrackIndex,
		int32 SegmentIndex);

	/**
	 * Set the start time of a segment in the montage timeline.
	 *
	 * @param MontagePath - Path to montage
	 * @param TrackIndex - Index of the slot track
	 * @param SegmentIndex - Index of the segment
	 * @param NewStartTime - New start time in seconds
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
	static bool SetSegmentStartTime(
		const FString& MontagePath,
		int32 TrackIndex,
		int32 SegmentIndex,
		float NewStartTime);

	/**
	 * Set the playback rate of a segment.
	 *
	 * @param MontagePath - Path to montage
	 * @param TrackIndex - Index of the slot track
	 * @param SegmentIndex - Index of the segment
	 * @param PlayRate - New playback rate
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
	static bool SetSegmentPlayRate(
		const FString& MontagePath,
		int32 TrackIndex,
		int32 SegmentIndex,
		float PlayRate);

	/**
	 * Set the start position within the source animation.
	 *
	 * @param MontagePath - Path to montage
	 * @param TrackIndex - Index of the slot track
	 * @param SegmentIndex - Index of the segment
	 * @param AnimStartPos - Time in source animation to start playing from
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
	static bool SetSegmentStartPosition(
		const FString& MontagePath,
		int32 TrackIndex,
		int32 SegmentIndex,
		float AnimStartPos);

	/**
	 * Set the end position within the source animation.
	 *
	 * @param MontagePath - Path to montage
	 * @param TrackIndex - Index of the slot track
	 * @param SegmentIndex - Index of the segment
	 * @param AnimEndPos - Time in source animation to stop playing
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
	static bool SetSegmentEndPosition(
		const FString& MontagePath,
		int32 TrackIndex,
		int32 SegmentIndex,
		float AnimEndPos);

	/**
	 * Set how many times a segment loops.
	 *
	 * @param MontagePath - Path to montage
	 * @param TrackIndex - Index of the slot track
	 * @param SegmentIndex - Index of the segment
	 * @param LoopCount - Number of loops (0 = play once, no loop)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
	static bool SetSegmentLoopCount(
		const FString& MontagePath,
		int32 TrackIndex,
		int32 SegmentIndex,
		int32 LoopCount);

	// ============================================================================
	// MONTAGE NOTIFIES
	// ============================================================================

	/**
	 * List all notifies in a montage.
	 *
	 * @param MontagePath - Path to montage
	 * @return Array of notify info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Notifies")
	static TArray<FMontageNotifyInfo> ListNotifies(const FString& MontagePath);

	/**
	 * Add an instant notify (point in time) to the montage.
	 *
	 * @param MontagePath - Path to montage
	 * @param NotifyClass - Full class path (e.g., "/Script/Engine.AnimNotify")
	 * @param TriggerTime - Time in seconds when notify triggers
	 * @param NotifyName - Optional name for the notify
	 * @return Index of the new notify, or -1 on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Notifies")
	static int32 AddNotify(
		const FString& MontagePath,
		const FString& NotifyClass,
		float TriggerTime,
		const FString& NotifyName = TEXT(""));

	/**
	 * Add a notify state (duration-based) to the montage.
	 *
	 * @param MontagePath - Path to montage
	 * @param NotifyStateClass - Full class path (e.g., "/Script/Engine.AnimNotifyState")
	 * @param StartTime - Start time in seconds
	 * @param Duration - Duration in seconds
	 * @param NotifyName - Optional name for the notify
	 * @return Index of the new notify, or -1 on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Notifies")
	static int32 AddNotifyState(
		const FString& MontagePath,
		const FString& NotifyStateClass,
		float StartTime,
		float Duration,
		const FString& NotifyName = TEXT(""));

	/**
	 * Remove a notify from the montage.
	 *
	 * @param MontagePath - Path to montage
	 * @param NotifyIndex - Index of the notify to remove
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Notifies")
	static bool RemoveNotify(const FString& MontagePath, int32 NotifyIndex);

	/**
	 * Set the trigger time for a notify.
	 *
	 * @param MontagePath - Path to montage
	 * @param NotifyIndex - Index of the notify
	 * @param NewTime - New trigger time in seconds
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Notifies")
	static bool SetNotifyTriggerTime(
		const FString& MontagePath,
		int32 NotifyIndex,
		float NewTime);

	/**
	 * Link a notify to a specific section.
	 *
	 * @param MontagePath - Path to montage
	 * @param NotifyIndex - Index of the notify
	 * @param SectionName - Name of the section to link to
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Notifies")
	static bool SetNotifyLinkToSection(
		const FString& MontagePath,
		int32 NotifyIndex,
		const FString& SectionName);

	// ============================================================================
	// BRANCHING POINTS (Frame-accurate gameplay events)
	// ============================================================================

	/**
	 * List all branching points in a montage.
	 *
	 * @param MontagePath - Path to montage
	 * @return Array of branching point info structs
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|BranchingPoints")
	static TArray<FBranchingPointInfo> ListBranchingPoints(const FString& MontagePath);

	/**
	 * Add a branching point to the montage.
	 * Branching points are frame-accurate notifies used for gameplay decisions.
	 *
	 * @param MontagePath - Path to montage
	 * @param NotifyName - Name for the branching point event
	 * @param TriggerTime - Time in seconds when the event fires
	 * @return Index of the new branching point, or -1 on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|BranchingPoints")
	static int32 AddBranchingPoint(
		const FString& MontagePath,
		const FString& NotifyName,
		float TriggerTime);

	/**
	 * Remove a branching point from the montage.
	 *
	 * @param MontagePath - Path to montage
	 * @param Index - Index of the branching point to remove
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|BranchingPoints")
	static bool RemoveBranchingPoint(const FString& MontagePath, int32 Index);

	/**
	 * Check if a branching point exists at a specific time.
	 *
	 * @param MontagePath - Path to montage
	 * @param Time - Time to check
	 * @return True if a branching point fires at this time
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|BranchingPoints")
	static bool IsBranchingPointAtTime(const FString& MontagePath, float Time);

	// ============================================================================
	// ROOT MOTION
	// ============================================================================

	/**
	 * Get whether root motion translation is enabled.
	 *
	 * @param MontagePath - Path to montage
	 * @return True if root motion translation is enabled
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|RootMotion")
	static bool GetEnableRootMotionTranslation(const FString& MontagePath);

	/**
	 * Enable or disable root motion translation.
	 *
	 * @param MontagePath - Path to montage
	 * @param bEnable - Whether to enable root motion translation
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|RootMotion")
	static bool SetEnableRootMotionTranslation(const FString& MontagePath, bool bEnable);

	/**
	 * Get whether root motion rotation is enabled.
	 *
	 * @param MontagePath - Path to montage
	 * @return True if root motion rotation is enabled
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|RootMotion")
	static bool GetEnableRootMotionRotation(const FString& MontagePath);

	/**
	 * Enable or disable root motion rotation.
	 *
	 * @param MontagePath - Path to montage
	 * @param bEnable - Whether to enable root motion rotation
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|RootMotion")
	static bool SetEnableRootMotionRotation(const FString& MontagePath, bool bEnable);

	/**
	 * Get root motion transform at a specific time.
	 *
	 * @param MontagePath - Path to montage
	 * @param Time - Time in seconds
	 * @param OutTransform - Output root motion transform
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|RootMotion")
	static bool GetRootMotionAtTime(
		const FString& MontagePath,
		float Time,
		FTransform& OutTransform);

	// ============================================================================
	// MONTAGE CREATION
	// ============================================================================

	/**
	 * Create a new montage from an existing animation sequence.
	 *
	 * @param AnimSequencePath - Source animation to base montage on
	 * @param DestPath - Folder to create montage in
	 * @param MontageName - Name for the new montage asset
	 * @return Path to created montage, or empty string on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Creation")
	static FString CreateMontageFromAnimation(
		const FString& AnimSequencePath,
		const FString& DestPath,
		const FString& MontageName);

	/**
	 * Create an empty montage for a skeleton.
	 *
	 * @param SkeletonPath - Skeleton the montage is for
	 * @param DestPath - Folder to create montage in
	 * @param MontageName - Name for the new montage asset
	 * @return Path to created montage, or empty string on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Creation")
	static FString CreateEmptyMontage(
		const FString& SkeletonPath,
		const FString& DestPath,
		const FString& MontageName);

	/**
	 * Duplicate an existing montage.
	 *
	 * @param SourcePath - Montage to duplicate
	 * @param DestPath - Folder for the copy
	 * @param NewName - Name for the duplicate
	 * @return Path to duplicated montage, or empty string on failure
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Creation")
	static FString DuplicateMontage(
		const FString& SourcePath,
		const FString& DestPath,
		const FString& NewName);

	// ============================================================================
	// EDITOR NAVIGATION
	// ============================================================================

	/**
	 * Open a montage in the Animation Editor.
	 *
	 * @param MontagePath - Path to montage
	 * @return True if editor opened successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Editor")
	static bool OpenMontageEditor(const FString& MontagePath);

	/**
	 * Refresh the montage editor by closing and reopening it.
	 * Use after programmatic modifications to ensure the UI shows current state.
	 *
	 * @param MontagePath - Path to montage
	 * @return True if editor refreshed successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Editor")
	static bool RefreshMontageEditor(const FString& MontagePath);

	/**
	 * Jump the editor preview to a specific section.
	 *
	 * @param MontagePath - Path to montage
	 * @param SectionName - Name of the section to jump to
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Editor")
	static bool JumpToSection(const FString& MontagePath, const FString& SectionName);

	/**
	 * Set the editor preview time.
	 *
	 * @param MontagePath - Path to montage
	 * @param Time - Time in seconds
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Editor")
	static bool SetPreviewTime(const FString& MontagePath, float Time);

	/**
	 * Play the montage in the editor preview.
	 *
	 * @param MontagePath - Path to montage
	 * @param StartSection - Optional section to start from (empty = beginning)
	 * @return True if successful
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Editor")
	static bool PlayPreview(const FString& MontagePath, const FString& StartSection = TEXT(""));

private:
	// ============================================================================
	// PRIVATE HELPERS
	// ============================================================================

	/** Load an animation montage from path */
	static class UAnimMontage* LoadMontage(const FString& MontagePath);

	/** Mark montage as modified for undo/redo and saving */
	static void MarkMontageModified(class UAnimMontage* Montage);

	/** Validate section name exists in montage */
	static bool ValidateSection(class UAnimMontage* Montage, const FString& SectionName);

	/** Validate track index is in range */
	static bool ValidateTrackIndex(class UAnimMontage* Montage, int32 TrackIndex);

	/** Validate segment index within track */
	static bool ValidateSegmentIndex(class UAnimMontage* Montage, int32 TrackIndex, int32 SegmentIndex);

	/** Fill montage info struct from montage */
	static void FillMontageInfo(class UAnimMontage* Montage, FMontageInfo& OutInfo);

	/** Convert blend option enum to string */
	static FString BlendOptionToString(EAlphaBlendOption Option);

	/** Convert string to blend option enum */
	static EAlphaBlendOption StringToBlendOption(const FString& OptionString);
};
