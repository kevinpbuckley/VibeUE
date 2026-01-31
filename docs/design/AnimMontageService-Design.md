# AnimMontageService Design Document

## Overview

**AnimMontageService** provides comprehensive CRUD operations for Animation Montage assets in Unreal Engine via VibeUE's Python API. This is a **gameplay-critical service** (75 assets) enabling AI-assisted montage manipulation including section management, slot tracks, blend settings, and branching logic essential for combat, interactions, and cinematic sequences.

**Target UE Version**: 5.7+
**Dependencies**: None (core animation module)

---

## Asset Types Covered

| Asset Type | UE Class | Description |
|------------|----------|-------------|
| Animation Montage | `UAnimMontage` | Composite animation with sections, slots, and notifies |
| Montage Section | `FCompositeSection` | Named time segments for branching |
| Slot Animation Track | `FSlotAnimationTrack` | Animation slot for layered playback |
| Anim Segment | `FAnimSegment` | Individual animation clip within a slot |
| Montage Notify | `FAnimNotifyEvent` | Events specific to montage playback |

---

## Architecture

### Service Class

```cpp
UCLASS()
class VIBEUE_API UAnimMontageService : public UObject
{
    GENERATED_BODY()

public:
    // === MONTAGE DISCOVERY ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static TArray<FMontageInfo> ListMontages(const FString& SearchPath = TEXT("/Game"), const FString& SkeletonFilter = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static FMontageInfo GetMontageInfo(const FString& MontagePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static TArray<FMontageInfo> FindMontagesForSkeleton(const FString& SkeletonPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static TArray<FMontageInfo> FindMontagesUsingAnimation(const FString& AnimSequencePath);

    // === MONTAGE PROPERTIES ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static float GetMontageLength(const FString& MontagePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static FString GetMontageSkeleton(const FString& MontagePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetBlendIn(const FString& MontagePath, float BlendTime, EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetBlendOut(const FString& MontagePath, float BlendTime, EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static FMontageBlendSettings GetBlendSettings(const FString& MontagePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetBlendOutTriggerTime(const FString& MontagePath, float TriggerTime);

    // === SECTION MANAGEMENT ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static TArray<FMontageSectionInfo> ListSections(const FString& MontagePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static FMontageSectionInfo GetSectionInfo(const FString& MontagePath, const FString& SectionName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static int32 GetSectionIndexAtTime(const FString& MontagePath, float Time);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static FString GetSectionNameAtTime(const FString& MontagePath, float Time);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool AddSection(const FString& MontagePath, const FString& SectionName, float StartTime);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool RemoveSection(const FString& MontagePath, const FString& SectionName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool RenameSection(const FString& MontagePath, const FString& OldName, const FString& NewName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetSectionStartTime(const FString& MontagePath, const FString& SectionName, float NewStartTime);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static float GetSectionLength(const FString& MontagePath, const FString& SectionName);

    // === SECTION LINKING (BRANCHING) ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static FString GetNextSection(const FString& MontagePath, const FString& SectionName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetNextSection(const FString& MontagePath, const FString& SectionName, const FString& NextSectionName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetSectionLoop(const FString& MontagePath, const FString& SectionName, bool bLoop);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static TArray<FSectionLink> GetAllSectionLinks(const FString& MontagePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool ClearSectionLink(const FString& MontagePath, const FString& SectionName);

    // === SLOT TRACK MANAGEMENT ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static TArray<FSlotTrackInfo> ListSlotTracks(const FString& MontagePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static FSlotTrackInfo GetSlotTrackInfo(const FString& MontagePath, int32 TrackIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static int32 AddSlotTrack(const FString& MontagePath, const FString& SlotName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool RemoveSlotTrack(const FString& MontagePath, int32 TrackIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetSlotName(const FString& MontagePath, int32 TrackIndex, const FString& NewSlotName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static TArray<FString> GetAllUsedSlotNames(const FString& MontagePath);

    // === ANIMATION SEGMENTS ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static TArray<FAnimSegmentInfo> ListAnimSegments(const FString& MontagePath, int32 TrackIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static FAnimSegmentInfo GetAnimSegmentInfo(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static int32 AddAnimSegment(const FString& MontagePath, int32 TrackIndex, const FString& AnimSequencePath, float StartTime, float PlayRate = 1.0f);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool RemoveAnimSegment(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetSegmentStartTime(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex, float NewStartTime);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetSegmentPlayRate(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex, float PlayRate);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetSegmentStartPosition(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex, float AnimStartPos);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetSegmentEndPosition(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex, float AnimEndPos);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetSegmentLoopCount(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex, int32 LoopCount);

    // === MONTAGE NOTIFIES ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static TArray<FAnimNotifyInfo> ListNotifies(const FString& MontagePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static int32 AddNotify(const FString& MontagePath, const FString& NotifyClass, float TriggerTime, const FString& NotifyName = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static int32 AddNotifyState(const FString& MontagePath, const FString& NotifyStateClass, float StartTime, float Duration, const FString& NotifyName = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool RemoveNotify(const FString& MontagePath, int32 NotifyIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetNotifyTriggerTime(const FString& MontagePath, int32 NotifyIndex, float NewTime);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetNotifyLinkToSection(const FString& MontagePath, int32 NotifyIndex, const FString& SectionName);

    // === BRANCHING POINTS ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static TArray<FBranchingPointInfo> ListBranchingPoints(const FString& MontagePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static int32 AddBranchingPoint(const FString& MontagePath, const FString& NotifyName, float TriggerTime);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool RemoveBranchingPoint(const FString& MontagePath, int32 Index);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool IsBranchingPointAtTime(const FString& MontagePath, float Time);

    // === ROOT MOTION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool GetEnableRootMotionTranslation(const FString& MontagePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetEnableRootMotionTranslation(const FString& MontagePath, bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool GetEnableRootMotionRotation(const FString& MontagePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetEnableRootMotionRotation(const FString& MontagePath, bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static FTransform GetRootMotionAtTime(const FString& MontagePath, float Time);

    // === MONTAGE CREATION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static FString CreateMontageFromAnimation(const FString& AnimSequencePath, const FString& DestPath, const FString& MontageName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static FString CreateEmptyMontage(const FString& SkeletonPath, const FString& DestPath, const FString& MontageName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool DuplicateMontage(const FString& SourcePath, const FString& DestPath, const FString& NewName);

    // === EDITOR NAVIGATION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool OpenMontageEditor(const FString& MontagePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool JumpToSection(const FString& MontagePath, const FString& SectionName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool SetPreviewTime(const FString& MontagePath, float Time);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Montage")
    static bool PlayPreview(const FString& MontagePath, const FString& StartSection = TEXT(""));
};
```

---

## Data Transfer Objects (DTOs)

### FMontageInfo

```cpp
USTRUCT(BlueprintType)
struct FMontageInfo
{
    GENERATED_BODY()

    /** Asset path of the montage */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString MontagePath;

    /** Display name */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString MontageName;

    /** Associated skeleton path */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString SkeletonPath;

    /** Total duration in seconds */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float Duration = 0.0f;

    /** Number of sections */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    int32 SectionCount = 0;

    /** Number of slot tracks */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    int32 SlotTrackCount = 0;

    /** Number of notifies */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    int32 NotifyCount = 0;

    /** Number of branching points */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    int32 BranchingPointCount = 0;

    /** Blend in time */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float BlendInTime = 0.0f;

    /** Blend out time */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float BlendOutTime = 0.0f;

    /** Blend out trigger time (-1 = auto) */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float BlendOutTriggerTime = -1.0f;

    /** Whether root motion translation is enabled */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    bool bEnableRootMotionTranslation = true;

    /** Whether root motion rotation is enabled */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    bool bEnableRootMotionRotation = true;

    /** List of slot names used */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    TArray<FString> SlotNames;
};
```

### FMontageBlendSettings

```cpp
USTRUCT(BlueprintType)
struct FMontageBlendSettings
{
    GENERATED_BODY()

    /** Blend in time */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float BlendInTime = 0.25f;

    /** Blend in curve type */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString BlendInOption;

    /** Blend out time */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float BlendOutTime = 0.25f;

    /** Blend out curve type */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString BlendOutOption;

    /** When to trigger blend out (-1 = auto at end minus blend time) */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float BlendOutTriggerTime = -1.0f;

    /** Custom blend in curve (if using custom option) */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString BlendInCurvePath;

    /** Custom blend out curve (if using custom option) */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString BlendOutCurvePath;
};
```

### FMontageSectionInfo

```cpp
USTRUCT(BlueprintType)
struct FMontageSectionInfo
{
    GENERATED_BODY()

    /** Section name */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString SectionName;

    /** Section index */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    int32 SectionIndex = 0;

    /** Start time in seconds */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float StartTime = 0.0f;

    /** End time in seconds */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float EndTime = 0.0f;

    /** Section duration */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float Duration = 0.0f;

    /** Next section name (empty if none linked) */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString NextSectionName;

    /** Whether section loops to itself */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    bool bLoops = false;

    /** Segment count in this section */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    int32 SegmentCount = 0;
};
```

### FSectionLink

```cpp
USTRUCT(BlueprintType)
struct FSectionLink
{
    GENERATED_BODY()

    /** Source section name */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString FromSection;

    /** Target section name */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString ToSection;

    /** Whether this is a self-loop */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    bool bIsLoop = false;
};
```

### FSlotTrackInfo

```cpp
USTRUCT(BlueprintType)
struct FSlotTrackInfo
{
    GENERATED_BODY()

    /** Track index */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    int32 TrackIndex = 0;

    /** Slot name for this track */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString SlotName;

    /** Number of animation segments */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    int32 SegmentCount = 0;

    /** Total duration of segments */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float TotalDuration = 0.0f;
};
```

### FAnimSegmentInfo

```cpp
USTRUCT(BlueprintType)
struct FAnimSegmentInfo
{
    GENERATED_BODY()

    /** Segment index within the track */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    int32 SegmentIndex = 0;

    /** Path to the source animation sequence */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString AnimSequencePath;

    /** Animation name */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString AnimName;

    /** Start time in the montage timeline */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float StartTime = 0.0f;

    /** Duration in the montage */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float Duration = 0.0f;

    /** Playback rate multiplier */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float PlayRate = 1.0f;

    /** Start position within the source animation */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float AnimStartPos = 0.0f;

    /** End position within the source animation */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float AnimEndPos = 0.0f;

    /** Number of loops (0 = use full length) */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    int32 LoopCount = 0;

    /** Whether this segment loops within its duration */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    bool bLoops = false;
};
```

### FBranchingPointInfo

```cpp
USTRUCT(BlueprintType)
struct FBranchingPointInfo
{
    GENERATED_BODY()

    /** Index of the branching point */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    int32 Index = 0;

    /** Notify name for this branching point */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString NotifyName;

    /** Trigger time in seconds */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    float TriggerTime = 0.0f;

    /** Section this branching point is in */
    UPROPERTY(BlueprintReadWrite, Category = "Montage")
    FString SectionName;
};
```

---

## Implementation Details

### Key Unreal APIs Used

#### Montage Data Access
```cpp
// Get montage
UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);

// Basic properties
float Duration = Montage->GetPlayLength();
USkeleton* Skeleton = Montage->GetSkeleton();
float BlendIn = Montage->BlendIn.GetBlendTime();
float BlendOut = Montage->BlendOut.GetBlendTime();
```

#### Section Management
```cpp
// List sections
const TArray<FCompositeSection>& Sections = Montage->CompositeSections;

// Add section
int32 Index = Montage->AddAnimCompositeSection(SectionName, StartTime);

// Get section at time
int32 SectionIdx = Montage->GetSectionIndexFromPosition(Time);
FName SectionName = Montage->GetSectionName(SectionIdx);

// Section linking
Montage->SetNextSectionName(SectionName, NextSectionName);
FName NextSection = Montage->GetNextSectionName(SectionName);

// Get section times
float StartTime, EndTime;
Montage->GetSectionStartAndEndTime(SectionIdx, StartTime, EndTime);
```

#### Slot Track Management
```cpp
// List slot tracks
const TArray<FSlotAnimationTrack>& SlotTracks = Montage->SlotAnimTracks;

// Add slot track
FSlotAnimationTrack& NewTrack = Montage->SlotAnimTracks.AddDefaulted_GetRef();
NewTrack.SlotName = SlotName;

// Get segments in track
const TArray<FAnimSegment>& Segments = SlotTracks[TrackIndex].AnimTrack.AnimSegments;
```

#### Animation Segments
```cpp
// Add segment
FAnimSegment NewSegment;
NewSegment.AnimReference = AnimSequence;
NewSegment.StartPos = StartTime;
NewSegment.AnimStartTime = AnimStartPos;
NewSegment.AnimEndTime = AnimEndPos;
NewSegment.AnimPlayRate = PlayRate;
NewSegment.LoopingCount = LoopCount;
SlotTracks[TrackIndex].AnimTrack.AnimSegments.Add(NewSegment);

// Modify segment
FAnimSegment& Segment = SlotTracks[TrackIndex].AnimTrack.AnimSegments[SegmentIndex];
Segment.AnimPlayRate = NewPlayRate;
Segment.StartPos = NewStartTime;
```

#### Blend Settings
```cpp
// Get blend settings
FAlphaBlend& BlendIn = Montage->BlendIn;
FAlphaBlend& BlendOut = Montage->BlendOut;

// Set blend in
Montage->BlendIn.SetBlendTime(BlendTime);
Montage->BlendIn.SetBlendOption(BlendOption);

// Set blend out trigger
Montage->BlendOutTriggerTime = TriggerTime; // -1 for auto
```

#### Branching Points
```cpp
// List branching point notifies
for (const FAnimNotifyEvent& Notify : Montage->Notifies)
{
    if (Notify.IsBranchingPoint())
    {
        // This is a branching point
    }
}

// Add branching point
FAnimNotifyEvent& BranchPoint = Montage->Notifies.AddDefaulted_GetRef();
BranchPoint.NotifyName = NotifyName;
BranchPoint.Link(Montage, TriggerTime);
BranchPoint.MontageTickType = EMontageNotifyTickType::BranchingPoint;
```

---

## Method Count Summary

| Category | Methods | Description |
|----------|---------|-------------|
| Montage Discovery | 4 | List, get info, find by skeleton, find by animation |
| Montage Properties | 6 | Length, skeleton, blend in/out, blend settings |
| Section Management | 9 | List, info, add, remove, rename, time, length |
| Section Linking | 5 | Get/set next, loop, all links, clear |
| Slot Track Management | 6 | List, info, add, remove, set name, get all names |
| Animation Segments | 9 | List, info, add, remove, time, rate, positions, loop |
| Montage Notifies | 6 | List, add notify/state, remove, time, link to section |
| Branching Points | 4 | List, add, remove, check time |
| Root Motion | 5 | Get/set translation, get/set rotation, get at time |
| Montage Creation | 3 | From animation, empty, duplicate |
| Editor Navigation | 4 | Open editor, jump to section, preview time, play |
| **TOTAL** | **61** | |

---

## Python API Examples

### Create a Combat Montage

```python
import unreal

# Create montage from base attack animation
montage_path = unreal.AnimMontageService.create_montage_from_animation(
    "/Game/Animations/Attack_Slash",
    "/Game/Montages",
    "AM_CombatAttack"
)

# Add sections for combo system
unreal.AnimMontageService.add_section(montage_path, "WindUp", 0.0)
unreal.AnimMontageService.add_section(montage_path, "Attack", 0.3)
unreal.AnimMontageService.add_section(montage_path, "Recovery", 0.8)

# Link sections for combo continuation
unreal.AnimMontageService.set_next_section(montage_path, "WindUp", "Attack")
# Recovery section ends the montage (no next section)

print(f"Created combat montage: {montage_path}")
```

### Configure Montage Blending

```python
import unreal

montage_path = "/Game/Montages/AM_Dodge"

# Set quick blend in, slower blend out
unreal.AnimMontageService.set_blend_in(montage_path, 0.1, unreal.AlphaBlendOption.CUBIC)
unreal.AnimMontageService.set_blend_out(montage_path, 0.3, unreal.AlphaBlendOption.LINEAR)

# Trigger blend out 0.2s before end
unreal.AnimMontageService.set_blend_out_trigger_time(montage_path, -0.2)
```

### Build Combo System Sections

```python
import unreal

montage_path = "/Game/Montages/AM_Combo"

# Create looping combo structure
sections = [
    ("Start", 0.0),
    ("Attack1", 0.3),
    ("Attack2", 0.9),
    ("Attack3", 1.5),
    ("Finisher", 2.1)
]

for name, time in sections:
    unreal.AnimMontageService.add_section(montage_path, name, time)

# Link sections for combo flow
unreal.AnimMontageService.set_next_section(montage_path, "Start", "Attack1")
unreal.AnimMontageService.set_next_section(montage_path, "Attack1", "Attack2")
unreal.AnimMontageService.set_next_section(montage_path, "Attack2", "Attack3")
unreal.AnimMontageService.set_next_section(montage_path, "Attack3", "Finisher")
# Finisher has no next = montage ends

# Add branching points for combo input windows
unreal.AnimMontageService.add_branching_point(montage_path, "ComboWindow", 0.6)
unreal.AnimMontageService.add_branching_point(montage_path, "ComboWindow", 1.2)
unreal.AnimMontageService.add_branching_point(montage_path, "ComboWindow", 1.8)
```

### Add Segments from Multiple Animations

```python
import unreal

montage_path = "/Game/Montages/AM_LocomotionMontage"

# Get the default slot track (index 0)
tracks = unreal.AnimMontageService.list_slot_tracks(montage_path)

# Add animation segments
unreal.AnimMontageService.add_anim_segment(
    montage_path, 0,
    "/Game/Animations/Idle",
    0.0,  # Start at beginning
    1.0   # Normal play rate
)

unreal.AnimMontageService.add_anim_segment(
    montage_path, 0,
    "/Game/Animations/Walk_Start",
    2.0,  # Start at 2 seconds
    1.0
)

unreal.AnimMontageService.add_anim_segment(
    montage_path, 0,
    "/Game/Animations/Walk_Loop",
    3.0,
    1.0
)

# Make the walk loop segment loop 3 times
unreal.AnimMontageService.set_segment_loop_count(montage_path, 0, 2, 3)
```

### Analyze Montage Structure

```python
import unreal

montage_path = "/Game/Montages/AM_AttackCombo"

info = unreal.AnimMontageService.get_montage_info(montage_path)
print(f"Montage: {info.montage_name}")
print(f"Duration: {info.duration:.2f}s")
print(f"Sections: {info.section_count}")
print(f"Blend In: {info.blend_in_time:.2f}s")
print(f"Blend Out: {info.blend_out_time:.2f}s")

# List sections with their links
sections = unreal.AnimMontageService.list_sections(montage_path)
print("\nSection Flow:")
for section in sections:
    next_sec = section.next_section_name if section.next_section_name else "(END)"
    loop = " [LOOP]" if section.loops else ""
    print(f"  {section.section_name} ({section.start_time:.2f}s) -> {next_sec}{loop}")

# List branching points
branches = unreal.AnimMontageService.list_branching_points(montage_path)
print(f"\nBranching Points: {len(branches)}")
for bp in branches:
    print(f"  {bp.notify_name} at {bp.trigger_time:.2f}s in {bp.section_name}")
```

---

## Skill Documentation

Create `Content/Skills/montage/skill.md`:

```markdown
---
name: montage
display_name: Animation Montages
description: Create and manipulate animation montages for gameplay, combos, and cinematics
vibeue_classes:
  - AnimMontageService
unreal_classes:
  - AnimMontage
  - AnimNotifyEvent
  - SlotAnimationTrack
keywords:
  - montage
  - section
  - slot
  - combo
  - branching point
  - blend
---

# Animation Montage Service Skill

## Critical Rules

### Sections Are Time-Based

Sections divide the montage timeline. When you add a section, it starts at that time and ends where the next section begins:

```
|--- WindUp ---|--- Attack ---|--- Recovery ---|
0.0s          0.3s           0.8s            1.2s
```

### Section Linking vs Looping

| Operation | Result |
|-----------|--------|
| `set_next_section("A", "B")` | A flows to B |
| `set_next_section("A", "A")` | A loops forever |
| `set_section_loop("A", True)` | Same as above |
| `clear_section_link("A")` | A ends montage |

### Slot Names Must Match AnimGraph

Montage slots (e.g., "DefaultSlot", "UpperBody") must match slot nodes in your Animation Blueprint!

```python
# Common slot names
"DefaultSlot"      # Full body animations
"UpperBody"        # Upper body only (layered)
"FullBody"         # Alternative naming
"Additive"         # Additive animations
```

### Branching Points vs Notifies

| Type | Use Case |
|------|----------|
| **Branching Point** | Combo input windows, decision points |
| **Notify** | Sound effects, VFX, gameplay events |

Branching points guarantee frame-accurate firing for gameplay decisions.

## Workflows

### Combat Combo Montage Pattern

```python
import unreal

def create_combo_montage(base_anim, num_attacks=3):
    """Create a standard combo montage"""
    montage_path = unreal.AnimMontageService.create_montage_from_animation(
        base_anim,
        "/Game/Montages",
        "AM_Combo"
    )

    # Create sections for each attack
    current_time = 0.0
    attack_duration = 0.5

    for i in range(num_attacks):
        section_name = f"Attack{i+1}"
        unreal.AnimMontageService.add_section(montage_path, section_name, current_time)

        # Add combo window branching point
        window_time = current_time + attack_duration - 0.15
        unreal.AnimMontageService.add_branching_point(
            montage_path, "ComboWindow", window_time
        )

        current_time += attack_duration

    # Link attacks in sequence
    for i in range(num_attacks - 1):
        unreal.AnimMontageService.set_next_section(
            montage_path,
            f"Attack{i+1}",
            f"Attack{i+2}"
        )

    return montage_path
```

### Upper Body Overlay Montage

```python
import unreal

montage_path = "/Game/Montages/AM_Reload"

# Use UpperBody slot for layered playback
tracks = unreal.AnimMontageService.list_slot_tracks(montage_path)
unreal.AnimMontageService.set_slot_name(montage_path, 0, "UpperBody")

# Fast blend for responsiveness
unreal.AnimMontageService.set_blend_in(montage_path, 0.1)
unreal.AnimMontageService.set_blend_out(montage_path, 0.2)
```

## Data Structures

### FMontageInfo
| Property | Type | Description |
|----------|------|-------------|
| montage_path | string | Asset path |
| duration | float | Total length |
| section_count | int | Number of sections |
| slot_names | array[string] | Slots used |
| blend_in_time | float | Blend in duration |
| blend_out_time | float | Blend out duration |

### FMontageSectionInfo
| Property | Type | Description |
|----------|------|-------------|
| section_name | string | Section identifier |
| start_time | float | Begin time |
| end_time | float | End time |
| next_section_name | string | Linked section |
| loops | bool | Self-referencing |
```

---

## Test Prompts

### 01_list_montages.md
```markdown
List all animation montages in /Game/Montages that use SK_Mannequin skeleton.
```

### 02_create_montage.md
```markdown
Create a new montage from the Attack_Slash animation with sections for WindUp, Attack, and Recovery.
```

### 03_setup_combo.md
```markdown
Configure the AM_Combo montage with 3 attack sections that link together, with branching points for combo input windows.
```

### 04_blend_settings.md
```markdown
Set the blend in to 0.15 seconds with cubic easing and blend out to 0.25 seconds for AM_Dodge.
```

### 05_analyze_montage.md
```markdown
Show the complete section structure and flow of AM_AttackCombo including all branching points.
```

---

## Dependencies

- **AnimationCore**: Core animation module

Add to VibeUE.Build.cs:
```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "AnimationCore",
});
```

---

## Implementation Priority

1. **Phase 1 - Core Read Operations** (Critical - Gameplay)
   - `ListMontages`, `GetMontageInfo`, `FindMontagesForSkeleton`
   - `ListSections`, `GetSectionInfo`, `GetSectionNameAtTime`
   - `ListSlotTracks`, `ListAnimSegments`

2. **Phase 2 - Section Management** (High Priority)
   - `AddSection`, `RemoveSection`, `RenameSection`
   - `SetNextSection`, `SetSectionLoop`, `ClearSectionLink`

3. **Phase 3 - Segment & Slot Management** (High Priority)
   - `AddAnimSegment`, `RemoveAnimSegment`
   - `SetSegmentPlayRate`, `SetSegmentLoopCount`
   - `AddSlotTrack`, `SetSlotName`

4. **Phase 4 - Notifies & Branching** (Medium Priority)
   - `AddNotify`, `AddNotifyState`, `RemoveNotify`
   - `AddBranchingPoint`, `RemoveBranchingPoint`

5. **Phase 5 - Creation & Advanced** (Lower Priority)
   - `CreateMontageFromAnimation`, `CreateEmptyMontage`
   - Blend settings
   - Root motion configuration

---

## Error Handling

All methods should return appropriate errors for:
- Asset not found
- Invalid section name
- Section time out of bounds
- Invalid track/segment index
- Skeleton mismatch for animations
- Slot name not found in skeleton
