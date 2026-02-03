# AnimMontageService Design Document

## Overview

**AnimMontageService** provides comprehensive CRUD operations for Animation Montage assets in Unreal Engine via VibeUE's Python API. This is a **gameplay-critical service** (75 assets) enabling AI-assisted montage manipulation including section management, slot tracks, blend settings, and branching logic essential for combat, interactions, and cinematic sequences.

**Target UE Version**: 5.7+
**Dependencies**: None (core animation module)

---

## Current Limitations & Required Updates (v2.0)

### What Works (Property-Level Access) ✅

The following properties CAN be read/written via standard `set_editor_property()` / `get_editor_property()`:

| Property | Type | Access | Notes |
|----------|------|--------|-------|
| `blend_in` | FAlphaBlend | Read/Write | Blend in settings |
| `blend_out` | FAlphaBlend | Read/Write | Blend out settings |
| `blend_out_trigger_time` | float | Read/Write | When to trigger blend out |
| `enable_root_motion_translation` | bool | Read/Write | Root motion X/Y |
| `enable_root_motion_rotation` | bool | Read/Write | Root motion rotation |
| `rate_scale` | float | Read/Write | Playback rate |
| `sync_group` | FName | Read/Write | Sync group name |
| `sync_slot_index` | int | Read/Write | Which slot to sync |

### What Does NOT Work (Internal Array Access) ❌

The following CANNOT be modified via Python `set_editor_property()`:

| Property | Type | Why It Fails |
|----------|------|--------------|
| `slot_anim_tracks` | TArray<FSlotAnimationTrack> | Internal array, read-only exposure |
| `composite_sections` | TArray<FCompositeSection> | Internal array, read-only exposure |
| `notifies` | TArray<FAnimNotifyEvent> | Inherited from UAnimSequenceBase |
| `anim_notify_tracks` | TArray<FAnimNotifyTrack> | Inherited from UAnimSequenceBase |

### Root Cause Analysis

1. **TArray properties in Python are exposed as read-only copies** - modifying them doesn't affect the source
2. **FSlotAnimationTrack, FCompositeSection, FAnimSegment** are not exposed to Python as full classes
3. **Section/Segment editing requires C++ helper functions** to manipulate internal UAnimMontage methods
4. **Notifies on montages use the same system as AnimSequence** - handled by `AnimSequenceService`

### Required Solution

Implement **C++ service methods exposed to Python** that wrap internal UAnimMontage APIs:

```cpp
// These internal methods exist but need C++ wrappers:
UAnimMontage::AddAnimCompositeSection()      // Add section
UAnimMontage::SetNextSectionName()           // Link sections
UAnimMontage::GetSectionIndexFromPosition() // Query sections
// SlotAnimTracks require direct array manipulation with proper marking
```

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

## Relationship to Existing Services

### AnimSequenceService Already Provides

The following functionality is **already implemented** in `AnimSequenceService` and applies to montages (since UAnimMontage inherits from UAnimSequenceBase):

| Feature | AnimSequenceService Methods | Reuse for Montages? |
|---------|----------------------------|---------------------|
| Notify management | `add_notify`, `add_notify_state`, `remove_notify`, `list_notifies` | ✅ Yes - same API |
| Notify tracks | `add_notify_track`, `remove_notify_track`, `list_notify_tracks` | ✅ Yes - inherited |
| Sync markers | `add_sync_marker`, `remove_sync_marker`, `list_sync_markers` | ✅ Yes - inherited |
| Curves | `add_curve`, `set_curve_keys`, `list_curves` | ✅ Yes - inherited |
| Root motion info | `get_root_motion_at_time`, `get_total_root_motion` | ✅ Yes - inherited |
| Editor preview | `open_animation_editor`, `set_preview_time`, `play_preview` | ⚠️ Partial - montage needs own editor |

### AnimMontageService Provides (Unique)

These are **montage-specific** and NOT available in AnimSequenceService:

| Feature | Why Montage-Specific |
|---------|----------------------|
| Sections | CompositeSections are montage-only |
| Section linking/branching | Montage-specific flow control |
| Slot tracks | Multiple animation layers |
| Animation segments | Multiple anims per slot |
| Branching points | Frame-accurate gameplay events |
| Slot name assignment | Which AnimGraph slot to target |

---

## Architecture

### Implementation Strategy

Due to Python's read-only access to UAnimMontage's internal TArrays, we **must implement C++ service methods** that wrap internal Unreal APIs and expose them to Python via UFUNCTION macros.

```
┌─────────────────────────────────────────────────────────────────┐
│                      Python API Layer                            │
│  unreal.AnimMontageService.add_section(path, name, time)        │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│               UAnimMontageService (C++ UCLASS)                  │
│  UFUNCTION(BlueprintCallable) static bool AddSection(...)       │
│  - Loads UAnimMontage from path                                 │
│  - Calls Montage->AddAnimCompositeSection()                     │
│  - Marks package dirty, returns success/failure                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                  UAnimMontage (Engine Class)                    │
│  - AddAnimCompositeSection() - INTERNAL API                     │
│  - SetNextSectionName() - INTERNAL API                          │
│  - Direct SlotAnimTracks array manipulation                     │
└─────────────────────────────────────────────────────────────────┘
```

### Critical Implementation Notes

1. **Mark Package Dirty**: After ANY modification, call `Montage->MarkPackageDirty()` to enable saving
2. **Post Edit**: Call `Montage->PostEditChange()` for editor refresh
3. **Notify Broadcast**: Use `Montage->Modify()` before changes for undo/redo support
4. **SlotAnimTracks**: Direct array access + `Montage->InvalidateCacheData()` after changes
5. **Sections**: Use `AddAnimCompositeSection()`, NOT direct array manipulation

### Service Class

```cpp
UCLASS()
class VIBEUE_API UAnimMontageService : public UObject
{
    GENERATED_BODY()

public:
    // === HELPER (INTERNAL) ===
    static UAnimMontage* LoadMontage(const FString& MontagePath);
    static void MarkMontageModified(UAnimMontage* Montage);

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

    // === SECTION MANAGEMENT (Requires C++ - Cannot do in Python) ===
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

    // === SLOT TRACK MANAGEMENT (Requires C++ - Cannot do in Python) ===
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

    // === ANIMATION SEGMENTS (Requires C++ - Multiple Anims per Montage) ===
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

### Why C++ Is Required (Not Python)

**Python's `set_editor_property()` CANNOT modify internal TArrays** in UAnimMontage. Here's why:

```python
# THIS DOES NOT WORK - Python gets a COPY of the array
montage = unreal.load_asset("/Game/Montages/AM_Test")
tracks = montage.get_editor_property("slot_anim_tracks")  # Returns a copy
tracks.append(new_track)  # Modifies the COPY, not the original
# The montage is unchanged!
```

**C++ has direct access** to the internal arrays:

```cpp
// THIS WORKS - C++ modifies the actual array
UAnimMontage* Montage = LoadObject<UAnimMontage>(...);
Montage->Modify();  // Enable undo support
FSlotAnimationTrack& NewTrack = Montage->SlotAnimTracks.AddDefaulted_GetRef();
NewTrack.SlotName = FName("DefaultSlot");
Montage->MarkPackageDirty();  // Enable saving
```

### Key Unreal APIs Used

#### Helper Function Pattern (All Methods Should Follow This)
```cpp
UAnimMontage* UAnimMontageService::LoadMontage(const FString& MontagePath)
{
    if (MontagePath.IsEmpty()) return nullptr;
    
    UObject* Obj = StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *MontagePath);
    return Cast<UAnimMontage>(Obj);
}

void UAnimMontageService::MarkMontageModified(UAnimMontage* Montage)
{
    if (Montage)
    {
        Montage->Modify();  // Undo/redo support
        Montage->MarkPackageDirty();  // Enable save
        Montage->PostEditChange();  // Refresh editors
    }
}
```

#### Montage Data Access
```cpp
// Get montage
UAnimMontage* Montage = LoadMontage(MontagePath);
if (!Montage) return false;

// Basic properties (these CAN be accessed via Python too)
float Duration = Montage->GetPlayLength();
USkeleton* Skeleton = Montage->GetSkeleton();
float BlendIn = Montage->BlendIn.GetBlendTime();
float BlendOut = Montage->BlendOut.GetBlendTime();
```

#### Section Management (C++ ONLY)
```cpp
// List sections (read-only in Python, full access in C++)
const TArray<FCompositeSection>& Sections = Montage->CompositeSections;

// Add section - REQUIRES C++
Montage->Modify();
int32 Index = Montage->AddAnimCompositeSection(FName(*SectionName), StartTime);
MarkMontageModified(Montage);

// Get section at time
int32 SectionIdx = Montage->GetSectionIndexFromPosition(Time);
FName SectionName = Montage->GetSectionName(SectionIdx);

// Section linking - REQUIRES C++
Montage->Modify();
Montage->SetNextSectionName(FName(*SectionName), FName(*NextSectionName));
MarkMontageModified(Montage);

// Get section times
float StartTime, EndTime;
Montage->GetSectionStartAndEndTime(SectionIdx, StartTime, EndTime);

// Remove section - REQUIRES C++ (no built-in method, manual array manipulation)
Montage->Modify();
for (int32 i = Montage->CompositeSections.Num() - 1; i >= 0; --i)
{
    if (Montage->CompositeSections[i].SectionName == FName(*SectionName))
    {
        Montage->CompositeSections.RemoveAt(i);
        break;
    }
}
MarkMontageModified(Montage);
```

#### Slot Track Management (C++ ONLY)
```cpp
// List slot tracks (read-only in Python)
const TArray<FSlotAnimationTrack>& SlotTracks = Montage->SlotAnimTracks;

// Add slot track - REQUIRES C++
Montage->Modify();
FSlotAnimationTrack& NewTrack = Montage->SlotAnimTracks.AddDefaulted_GetRef();
NewTrack.SlotName = FName(*SlotName);
MarkMontageModified(Montage);

// Get segments in track
const TArray<FAnimSegment>& Segments = SlotTracks[TrackIndex].AnimTrack.AnimSegments;

// Remove slot track - REQUIRES C++
Montage->Modify();
if (TrackIndex >= 0 && TrackIndex < Montage->SlotAnimTracks.Num())
{
    Montage->SlotAnimTracks.RemoveAt(TrackIndex);
}
MarkMontageModified(Montage);
```

#### Animation Segments (C++ ONLY - Multiple Animations Per Montage)
```cpp
// Add segment to slot track - REQUIRES C++
Montage->Modify();
UAnimSequence* AnimSeq = LoadObject<UAnimSequence>(nullptr, *AnimSequencePath);
if (AnimSeq && TrackIndex < Montage->SlotAnimTracks.Num())
{
    FAnimSegment NewSegment;
    NewSegment.AnimReference = AnimSeq;
    NewSegment.StartPos = StartTime;
    NewSegment.AnimStartTime = 0.0f;  // Start of source anim
    NewSegment.AnimEndTime = AnimSeq->GetPlayLength();  // End of source anim
    NewSegment.AnimPlayRate = PlayRate;
    NewSegment.LoopingCount = 0;  // 0 = play once
    
    Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments.Add(NewSegment);
}
MarkMontageModified(Montage);

// Modify segment properties - REQUIRES C++
Montage->Modify();
FAnimSegment& Segment = Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments[SegmentIndex];
Segment.AnimPlayRate = NewPlayRate;
Segment.StartPos = NewStartTime;
MarkMontageModified(Montage);

// Remove segment - REQUIRES C++
Montage->Modify();
Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments.RemoveAt(SegmentIndex);
MarkMontageModified(Montage);
```

#### Blend Settings (Works in Both Python and C++)
```cpp
// Get blend settings
FAlphaBlend& BlendIn = Montage->BlendIn;
FAlphaBlend& BlendOut = Montage->BlendOut;

// Set blend in
Montage->Modify();
Montage->BlendIn.SetBlendTime(BlendTime);
Montage->BlendIn.SetBlendOption(BlendOption);
MarkMontageModified(Montage);

// Set blend out trigger
Montage->BlendOutTriggerTime = TriggerTime; // -1 for auto
```

#### Branching Points (C++ ONLY for Adding)
```cpp
// List branching point notifies (read-only in Python)
for (const FAnimNotifyEvent& Notify : Montage->Notifies)
{
    if (Notify.IsBranchingPoint())
    {
        // This is a branching point
    }
}

// Add branching point - REQUIRES C++
Montage->Modify();
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

Test prompts are located in `test_prompts/montage/` directory.

### Test Workflow Summary

| Test File | Category | What It Validates |
|-----------|----------|-------------------|
| `01_discovery.md` | Read | List montages, get info, find by skeleton |
| `02_properties.md` | Read/Write | Blend settings, root motion (Python-accessible) |
| `03_sections.md` | Write | Add/remove sections, section timing |
| `04_section_linking.md` | Write | Section flow, loops, branching |
| `05_slots_tracks.md` | Write | Slot management, track names |
| `06_multi_animation.md` | Write | Multiple animations per montage, timing |
| `07_segments.md` | Write | Segment timing, play rate, loops |
| `08_combo_workflow.md` | E2E | Full combo system creation |
| `09_layered_montage.md` | E2E | Upper body overlay montage |

### Current Capability Status

| Feature | Status | Notes |
|---------|--------|-------|
| Read montage properties | ✅ Works | Python `get_editor_property()` |
| Set blend in/out | ✅ Works | Python `set_editor_property()` |
| Read sections (list) | ✅ Works | Python read-only access |
| Add/remove sections | ❌ Needs C++ | Python can't modify TArray |
| Read slot tracks | ✅ Works | Python read-only access |
| Add/modify slot tracks | ❌ Needs C++ | Python can't modify TArray |
| Read segments | ✅ Works | Python read-only access |
| Add/modify segments | ❌ Needs C++ | Python can't modify TArray |
| Add notifies | ⚠️ Use AnimSequenceService | Inherited from base class |

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

### Phase 1 - Core Read Operations (C++ Service - Week 1)

**Critical for discovery and analysis:**

```cpp
// These read operations wrap existing UAnimMontage methods
static TArray<FMontageInfo> ListMontages(SearchPath, SkeletonFilter);
static FMontageInfo GetMontageInfo(MontagePath);
static TArray<FMontageInfo> FindMontagesForSkeleton(SkeletonPath);
static TArray<FMontageSectionInfo> ListSections(MontagePath);
static TArray<FSlotTrackInfo> ListSlotTracks(MontagePath);
static TArray<FAnimSegmentInfo> ListAnimSegments(MontagePath, TrackIndex);
```

### Phase 2 - Section Management (C++ Service - Week 2)

**Required for combo systems and gameplay:**

```cpp
// Section CRUD - must be C++ (Python can't modify CompositeSections array)
static bool AddSection(MontagePath, SectionName, StartTime);
static bool RemoveSection(MontagePath, SectionName);
static bool RenameSection(MontagePath, OldName, NewName);
static bool SetNextSection(MontagePath, SectionName, NextSectionName);
static bool SetSectionLoop(MontagePath, SectionName, bLoop);
static bool ClearSectionLink(MontagePath, SectionName);
```

### Phase 3 - Slot & Segment Management (C++ Service - Week 3)

**Required for multi-animation montages:**

```cpp
// Slot track management - must be C++ (Python can't modify SlotAnimTracks array)
static int32 AddSlotTrack(MontagePath, SlotName);
static bool RemoveSlotTrack(MontagePath, TrackIndex);
static bool SetSlotName(MontagePath, TrackIndex, NewSlotName);

// Segment management - must be C++ (Python can't modify AnimSegments array)
static int32 AddAnimSegment(MontagePath, TrackIndex, AnimPath, StartTime, PlayRate);
static bool RemoveAnimSegment(MontagePath, TrackIndex, SegmentIndex);
static bool SetSegmentTiming(MontagePath, TrackIndex, SegmentIndex, StartTime, Duration);
static bool SetSegmentPlayRate(MontagePath, TrackIndex, SegmentIndex, PlayRate);
```

### Phase 4 - Branching Points (C++ Service - Week 4)

**Required for gameplay-critical events:**

```cpp
// Branching points - must be C++ (Python can't add to Notifies array)
static int32 AddBranchingPoint(MontagePath, NotifyName, TriggerTime);
static bool RemoveBranchingPoint(MontagePath, Index);
static TArray<FBranchingPointInfo> ListBranchingPoints(MontagePath);
```

### Phase 5 - Creation Helpers (C++ Service - Week 5)

**Convenience methods:**

```cpp
static FString CreateMontageFromAnimation(AnimPath, DestPath, MontageName);
static FString CreateEmptyMontage(SkeletonPath, DestPath, MontageName);
static bool DuplicateMontage(SourcePath, DestPath, NewName);
```

---

## Relationship to AnimSequenceService (Avoid Duplication)

**DO NOT re-implement in AnimMontageService:**

| Feature | Use This | Why |
|---------|----------|-----|
| Add/remove notifies | `AnimSequenceService.add_notify()` | UAnimMontage inherits from UAnimSequenceBase |
| Add/remove notify tracks | `AnimSequenceService.add_notify_track()` | Same inheritance |
| Sync markers | `AnimSequenceService` | Same inheritance |
| Curves | `AnimSequenceService` | Same inheritance |
| Root motion queries | `AnimSequenceService.get_root_motion_at_time()` | Same inheritance |

**AnimMontageService provides ONLY montage-specific features:**
- Sections (CompositeSections)
- Section linking/branching
- Slot tracks (SlotAnimTracks)
- Animation segments (FAnimSegment)
- Branching points (special notify type)

---

## Error Handling

All methods should return appropriate errors for:
- Asset not found
- Invalid section name
- Section time out of bounds
- Invalid track/segment index
- Skeleton mismatch for animations
- Slot name not found in skeleton

---

## File Organization

```
Plugins/VibeUE/
├── Source/VibeUE/
│   ├── Public/PythonAPI/
│   │   └── UAnimMontageService.h       # Service header with DTOs
│   ├── Private/PythonAPI/
│   │   └── UAnimMontageService.cpp     # Service implementation
│   └── VibeUE.Build.cs                 # No changes needed (deps exist)
├── Content/Skills/montage/
│   └── skill.md                        # Skill documentation
└── docs/
    ├── design/
    │   └── AnimMontageService-Design.md  # This document
    └── test_prompts/montage/
        └── *.md                          # Test prompt files
```

---

## Header File Template

Create `Source/VibeUE/Public/PythonAPI/UAnimMontageService.h`:

```cpp
// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "UAnimMontageService.generated.h"

// ============================================================================
// DATA TRANSFER OBJECTS (DTOs)
// ============================================================================

/**
 * Information about an animation montage asset
 */
USTRUCT(BlueprintType)
struct VIBEUE_API FMontageInfo
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
 * Blend settings for a montage
 */
USTRUCT(BlueprintType)
struct VIBEUE_API FMontageBlendSettings
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float BlendInTime = 0.25f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString BlendInOption;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float BlendOutTime = 0.25f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString BlendOutOption;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float BlendOutTriggerTime = -1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString BlendInCurvePath;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString BlendOutCurvePath;
};

/**
 * Information about a montage section
 */
USTRUCT(BlueprintType)
struct VIBEUE_API FMontageSectionInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString SectionName;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    int32 SectionIndex = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float StartTime = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float EndTime = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float Duration = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString NextSectionName;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    bool bLoops = false;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    int32 SegmentCount = 0;
};

/**
 * Section linking information
 */
USTRUCT(BlueprintType)
struct VIBEUE_API FSectionLink
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString FromSection;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString ToSection;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    bool bIsLoop = false;
};

/**
 * Information about a slot animation track
 */
USTRUCT(BlueprintType)
struct VIBEUE_API FSlotTrackInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    int32 TrackIndex = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString SlotName;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    int32 SegmentCount = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float TotalDuration = 0.0f;
};

/**
 * Information about an animation segment within a slot track
 */
USTRUCT(BlueprintType)
struct VIBEUE_API FAnimSegmentInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    int32 SegmentIndex = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString AnimSequencePath;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString AnimName;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float StartTime = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float Duration = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float PlayRate = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float AnimStartPos = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float AnimEndPos = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    int32 LoopCount = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    bool bLoops = false;
};

/**
 * Information about a branching point
 */
USTRUCT(BlueprintType)
struct VIBEUE_API FBranchingPointInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    int32 Index = 0;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString NotifyName;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float TriggerTime = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString SectionName;
};

// ============================================================================
// SERVICE CLASS
// ============================================================================

/**
 * Animation Montage service exposed directly to Python.
 *
 * This service provides comprehensive CRUD operations for Animation Montage assets.
 * Unlike properties accessible via set_editor_property(), this service exposes
 * internal montage structures (sections, slots, segments) that require C++ access.
 *
 * Python Usage:
 *   import unreal
 *
 *   # List all montages for a skeleton
 *   montages = unreal.AnimMontageService.find_montages_for_skeleton("/Game/Mannequin/Skeleton")
 *
 *   # Create a montage from an animation
 *   path = unreal.AnimMontageService.create_montage_from_animation(
 *       "/Game/Animations/Attack", "/Game/Montages", "AM_Attack")
 *
 *   # Add sections for combo system
 *   unreal.AnimMontageService.add_section(path, "WindUp", 0.0)
 *   unreal.AnimMontageService.add_section(path, "Attack", 0.3)
 *   unreal.AnimMontageService.set_next_section(path, "WindUp", "Attack")
 *
 * @note All methods are static. Montage modifications require C++ because
 *       Python's set_editor_property() returns copies of internal TArrays.
 *
 * **C++ Source:**
 * - **Plugin**: VibeUE
 * - **Module**: VibeUE
 * - **File**: UAnimMontageService.h
 */
UCLASS(BlueprintType)
class VIBEUE_API UAnimMontageService : public UObject
{
    GENERATED_BODY()

public:
    // ========================================================================
    // INTERNAL HELPERS (Not exposed to Blueprint/Python)
    // ========================================================================

    /** Load montage from asset path, returns nullptr if not found */
    static UAnimMontage* LoadMontage(const FString& MontagePath);

    /** Mark montage as modified for undo/redo and saving */
    static void MarkMontageModified(UAnimMontage* Montage);

    /** Validate section name exists in montage */
    static bool ValidateSection(UAnimMontage* Montage, const FString& SectionName);

    /** Validate track index is in range */
    static bool ValidateTrackIndex(UAnimMontage* Montage, int32 TrackIndex);

    /** Validate segment index within track */
    static bool ValidateSegmentIndex(UAnimMontage* Montage, int32 TrackIndex, int32 SegmentIndex);

    // ========================================================================
    // MONTAGE DISCOVERY
    // ========================================================================

    /**
     * List all montage assets in the specified path
     *
     * @param SearchPath - Content path to search (default: "/Game")
     * @param SkeletonFilter - Optional skeleton path to filter by
     * @return Array of montage info structs
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
    static TArray<FMontageInfo> ListMontages(const FString& SearchPath = TEXT("/Game"), const FString& SkeletonFilter = TEXT(""));

    /**
     * Get detailed information about a montage
     *
     * @param MontagePath - Asset path to the montage
     * @return Montage info struct (empty path if not found)
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
    static FMontageInfo GetMontageInfo(const FString& MontagePath);

    /**
     * Find all montages compatible with a skeleton
     *
     * @param SkeletonPath - Path to the skeleton asset
     * @return Array of compatible montage info structs
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
    static TArray<FMontageInfo> FindMontagesForSkeleton(const FString& SkeletonPath);

    /**
     * Find all montages that use a specific animation sequence
     *
     * @param AnimSequencePath - Path to the animation sequence
     * @return Array of montage info structs containing that animation
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
    static TArray<FMontageInfo> FindMontagesUsingAnimation(const FString& AnimSequencePath);

    // ========================================================================
    // MONTAGE PROPERTIES
    // ========================================================================

    /**
     * Get the total duration of a montage in seconds
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
    static float GetMontageLength(const FString& MontagePath);

    /**
     * Get the skeleton asset path for a montage
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
    static FString GetMontageSkeleton(const FString& MontagePath);

    /**
     * Set blend in settings
     *
     * @param MontagePath - Path to montage
     * @param BlendTime - Blend duration in seconds
     * @param BlendOption - Blend curve type (Linear, Cubic, etc.)
     * @return True if successful
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
    static bool SetBlendIn(const FString& MontagePath, float BlendTime, EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear);

    /**
     * Set blend out settings
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
    static bool SetBlendOut(const FString& MontagePath, float BlendTime, EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear);

    /**
     * Get current blend settings
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
    static FMontageBlendSettings GetBlendSettings(const FString& MontagePath);

    /**
     * Set when blend out begins (-1 = auto, based on blend out time)
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage")
    static bool SetBlendOutTriggerTime(const FString& MontagePath, float TriggerTime);

    // ========================================================================
    // SECTION MANAGEMENT (Requires C++ - Python cannot modify TArrays)
    // ========================================================================

    /**
     * List all sections in a montage
     *
     * @param MontagePath - Path to montage
     * @return Array of section info structs, ordered by start time
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static TArray<FMontageSectionInfo> ListSections(const FString& MontagePath);

    /**
     * Get info for a specific section by name
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static FMontageSectionInfo GetSectionInfo(const FString& MontagePath, const FString& SectionName);

    /**
     * Get section index at a specific time
     *
     * @return Section index, or -1 if time is out of range
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static int32 GetSectionIndexAtTime(const FString& MontagePath, float Time);

    /**
     * Get section name at a specific time
     *
     * @return Section name, or empty string if time is out of range
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static FString GetSectionNameAtTime(const FString& MontagePath, float Time);

    /**
     * Add a new section to the montage
     *
     * @param MontagePath - Path to montage
     * @param SectionName - Name for the new section (must be unique)
     * @param StartTime - Start time in seconds (must be within montage duration)
     * @return True if section was added successfully
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static bool AddSection(const FString& MontagePath, const FString& SectionName, float StartTime);

    /**
     * Remove a section from the montage
     *
     * @note Cannot remove the "Default" section if it's the only one
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static bool RemoveSection(const FString& MontagePath, const FString& SectionName);

    /**
     * Rename an existing section
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static bool RenameSection(const FString& MontagePath, const FString& OldName, const FString& NewName);

    /**
     * Move a section to a new start time
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static bool SetSectionStartTime(const FString& MontagePath, const FString& SectionName, float NewStartTime);

    /**
     * Get the duration of a specific section
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static float GetSectionLength(const FString& MontagePath, const FString& SectionName);

    // ========================================================================
    // SECTION LINKING (BRANCHING)
    // ========================================================================

    /**
     * Get the next section that plays after the specified section
     *
     * @return Next section name, or empty string if section ends montage
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static FString GetNextSection(const FString& MontagePath, const FString& SectionName);

    /**
     * Link a section to play another section when it completes
     *
     * @param SectionName - Source section
     * @param NextSectionName - Section to play next (empty string = end montage)
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static bool SetNextSection(const FString& MontagePath, const FString& SectionName, const FString& NextSectionName);

    /**
     * Set a section to loop to itself
     *
     * @param bLoop - True to loop, false to clear loop (section ends montage)
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static bool SetSectionLoop(const FString& MontagePath, const FString& SectionName, bool bLoop);

    /**
     * Get all section links in the montage
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static TArray<FSectionLink> GetAllSectionLinks(const FString& MontagePath);

    /**
     * Clear the link from a section (section will end the montage)
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Sections")
    static bool ClearSectionLink(const FString& MontagePath, const FString& SectionName);

    // ========================================================================
    // SLOT TRACK MANAGEMENT (Requires C++ - Python cannot modify TArrays)
    // ========================================================================

    /**
     * List all slot tracks in a montage
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Slots")
    static TArray<FSlotTrackInfo> ListSlotTracks(const FString& MontagePath);

    /**
     * Get info for a specific slot track
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Slots")
    static FSlotTrackInfo GetSlotTrackInfo(const FString& MontagePath, int32 TrackIndex);

    /**
     * Add a new slot track to the montage
     *
     * @param SlotName - Name of the animation slot (must exist in skeleton)
     * @return Index of the new track, or -1 on failure
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Slots")
    static int32 AddSlotTrack(const FString& MontagePath, const FString& SlotName);

    /**
     * Remove a slot track from the montage
     *
     * @note Cannot remove the last slot track
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Slots")
    static bool RemoveSlotTrack(const FString& MontagePath, int32 TrackIndex);

    /**
     * Change the slot name for a track
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Slots")
    static bool SetSlotName(const FString& MontagePath, int32 TrackIndex, const FString& NewSlotName);

    /**
     * Get all unique slot names used in the montage
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Slots")
    static TArray<FString> GetAllUsedSlotNames(const FString& MontagePath);

    // ========================================================================
    // ANIMATION SEGMENTS (Multiple Animations per Montage)
    // ========================================================================

    /**
     * List all animation segments in a slot track
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
    static TArray<FAnimSegmentInfo> ListAnimSegments(const FString& MontagePath, int32 TrackIndex);

    /**
     * Get info for a specific animation segment
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
    static FAnimSegmentInfo GetAnimSegmentInfo(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex);

    /**
     * Add an animation segment to a slot track
     *
     * @param TrackIndex - Slot track to add to
     * @param AnimSequencePath - Path to the animation sequence
     * @param StartTime - Start position in montage timeline
     * @param PlayRate - Playback rate multiplier (1.0 = normal)
     * @return Index of the new segment, or -1 on failure
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
    static int32 AddAnimSegment(const FString& MontagePath, int32 TrackIndex, const FString& AnimSequencePath, float StartTime, float PlayRate = 1.0f);

    /**
     * Remove an animation segment from a slot track
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
    static bool RemoveAnimSegment(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex);

    /**
     * Set the start time of a segment in the montage timeline
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
    static bool SetSegmentStartTime(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex, float NewStartTime);

    /**
     * Set the playback rate of a segment
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
    static bool SetSegmentPlayRate(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex, float PlayRate);

    /**
     * Set the start position within the source animation
     *
     * @param AnimStartPos - Time in source animation to start playing from
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
    static bool SetSegmentStartPosition(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex, float AnimStartPos);

    /**
     * Set the end position within the source animation
     *
     * @param AnimEndPos - Time in source animation to stop playing
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
    static bool SetSegmentEndPosition(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex, float AnimEndPos);

    /**
     * Set how many times a segment loops
     *
     * @param LoopCount - Number of loops (0 = play once, no loop)
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Segments")
    static bool SetSegmentLoopCount(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex, int32 LoopCount);

    // ========================================================================
    // BRANCHING POINTS (Frame-accurate gameplay events)
    // ========================================================================

    /**
     * List all branching points in a montage
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|BranchingPoints")
    static TArray<FBranchingPointInfo> ListBranchingPoints(const FString& MontagePath);

    /**
     * Add a branching point to the montage
     *
     * @param NotifyName - Name for the branching point event
     * @param TriggerTime - Time in seconds when the event fires
     * @return Index of the new branching point, or -1 on failure
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|BranchingPoints")
    static int32 AddBranchingPoint(const FString& MontagePath, const FString& NotifyName, float TriggerTime);

    /**
     * Remove a branching point from the montage
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|BranchingPoints")
    static bool RemoveBranchingPoint(const FString& MontagePath, int32 Index);

    /**
     * Check if a branching point exists at a specific time
     *
     * @param Time - Time to check
     * @return True if a branching point fires at this time
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|BranchingPoints")
    static bool IsBranchingPointAtTime(const FString& MontagePath, float Time);

    // ========================================================================
    // ROOT MOTION
    // ========================================================================

    /**
     * Get whether root motion translation is enabled
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|RootMotion")
    static bool GetEnableRootMotionTranslation(const FString& MontagePath);

    /**
     * Enable or disable root motion translation
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|RootMotion")
    static bool SetEnableRootMotionTranslation(const FString& MontagePath, bool bEnable);

    /**
     * Get whether root motion rotation is enabled
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|RootMotion")
    static bool GetEnableRootMotionRotation(const FString& MontagePath);

    /**
     * Enable or disable root motion rotation
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|RootMotion")
    static bool SetEnableRootMotionRotation(const FString& MontagePath, bool bEnable);

    /**
     * Get root motion transform at a specific time
     *
     * @param Time - Time in seconds
     * @return Root motion transform (position, rotation, scale)
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|RootMotion")
    static FTransform GetRootMotionAtTime(const FString& MontagePath, float Time);

    // ========================================================================
    // MONTAGE CREATION
    // ========================================================================

    /**
     * Create a new montage from an existing animation sequence
     *
     * @param AnimSequencePath - Source animation to base montage on
     * @param DestPath - Folder to create montage in
     * @param MontageName - Name for the new montage asset
     * @return Path to created montage, or empty string on failure
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Creation")
    static FString CreateMontageFromAnimation(const FString& AnimSequencePath, const FString& DestPath, const FString& MontageName);

    /**
     * Create an empty montage for a skeleton
     *
     * @param SkeletonPath - Skeleton the montage is for
     * @param DestPath - Folder to create montage in
     * @param MontageName - Name for the new montage asset
     * @return Path to created montage, or empty string on failure
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Creation")
    static FString CreateEmptyMontage(const FString& SkeletonPath, const FString& DestPath, const FString& MontageName);

    /**
     * Duplicate an existing montage
     *
     * @param SourcePath - Montage to duplicate
     * @param DestPath - Folder for the copy
     * @param NewName - Name for the duplicate
     * @return True if successful
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Creation")
    static bool DuplicateMontage(const FString& SourcePath, const FString& DestPath, const FString& NewName);

    // ========================================================================
    // EDITOR NAVIGATION
    // ========================================================================

    /**
     * Open a montage in the Animation Editor
     *
     * @return True if editor opened successfully
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Editor")
    static bool OpenMontageEditor(const FString& MontagePath);

    /**
     * Jump the editor preview to a specific section
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Editor")
    static bool JumpToSection(const FString& MontagePath, const FString& SectionName);

    /**
     * Set the editor preview time
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Editor")
    static bool SetPreviewTime(const FString& MontagePath, float Time);

    /**
     * Play the montage in the editor preview
     *
     * @param StartSection - Optional section to start from (empty = beginning)
     */
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation|Montage|Editor")
    static bool PlayPreview(const FString& MontagePath, const FString& StartSection = TEXT(""));
};
```

---

## Source File Template

Create `Source/VibeUE/Private/PythonAPI/UAnimMontageService.cpp`:

```cpp
// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UAnimMontageService.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/SavePackage.h"

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

UAnimMontage* UAnimMontageService::LoadMontage(const FString& MontagePath)
{
    if (MontagePath.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("AnimMontageService: Empty montage path provided"));
        return nullptr;
    }

    UObject* LoadedObj = StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *MontagePath);
    if (!LoadedObj)
    {
        UE_LOG(LogTemp, Warning, TEXT("AnimMontageService: Failed to load montage at '%s'"), *MontagePath);
        return nullptr;
    }

    return Cast<UAnimMontage>(LoadedObj);
}

void UAnimMontageService::MarkMontageModified(UAnimMontage* Montage)
{
    if (!Montage) return;

    // Enable undo/redo support
    Montage->Modify();

    // Mark package as needing save
    Montage->MarkPackageDirty();

    // Refresh any open editors
    Montage->PostEditChange();

    // Invalidate cached data (important for sections/segments)
    Montage->InvalidateRecursiveAsset();
}

bool UAnimMontageService::ValidateSection(UAnimMontage* Montage, const FString& SectionName)
{
    if (!Montage) return false;

    const FName SectionFName(*SectionName);
    const int32 Index = Montage->GetSectionIndex(SectionFName);
    return Index != INDEX_NONE;
}

bool UAnimMontageService::ValidateTrackIndex(UAnimMontage* Montage, int32 TrackIndex)
{
    if (!Montage) return false;
    return TrackIndex >= 0 && TrackIndex < Montage->SlotAnimTracks.Num();
}

bool UAnimMontageService::ValidateSegmentIndex(UAnimMontage* Montage, int32 TrackIndex, int32 SegmentIndex)
{
    if (!ValidateTrackIndex(Montage, TrackIndex)) return false;

    const TArray<FAnimSegment>& Segments = Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments;
    return SegmentIndex >= 0 && SegmentIndex < Segments.Num();
}

// ============================================================================
// MONTAGE DISCOVERY
// ============================================================================

TArray<FMontageInfo> UAnimMontageService::ListMontages(const FString& SearchPath, const FString& SkeletonFilter)
{
    TArray<FMontageInfo> Results;

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    FARFilter Filter;
    Filter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(FName(*SearchPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetList;
    AssetRegistry.GetAssets(Filter, AssetList);

    USkeleton* FilterSkeleton = nullptr;
    if (!SkeletonFilter.IsEmpty())
    {
        FilterSkeleton = Cast<USkeleton>(StaticLoadObject(USkeleton::StaticClass(), nullptr, *SkeletonFilter));
    }

    for (const FAssetData& AssetData : AssetList)
    {
        UAnimMontage* Montage = Cast<UAnimMontage>(AssetData.GetAsset());
        if (!Montage) continue;

        // Filter by skeleton if specified
        if (FilterSkeleton && Montage->GetSkeleton() != FilterSkeleton) continue;

        Results.Add(GetMontageInfo(AssetData.GetObjectPathString()));
    }

    return Results;
}

FMontageInfo UAnimMontageService::GetMontageInfo(const FString& MontagePath)
{
    FMontageInfo Info;

    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return Info;

    Info.MontagePath = MontagePath;
    Info.MontageName = Montage->GetName();

    if (USkeleton* Skeleton = Montage->GetSkeleton())
    {
        Info.SkeletonPath = Skeleton->GetPathName();
    }

    Info.Duration = Montage->GetPlayLength();
    Info.SectionCount = Montage->CompositeSections.Num();
    Info.SlotTrackCount = Montage->SlotAnimTracks.Num();
    Info.NotifyCount = Montage->Notifies.Num();

    // Count branching points
    Info.BranchingPointCount = 0;
    for (const FAnimNotifyEvent& Notify : Montage->Notifies)
    {
        if (Notify.MontageTickType == EMontageNotifyTickType::BranchingPoint)
        {
            Info.BranchingPointCount++;
        }
    }

    Info.BlendInTime = Montage->BlendIn.GetBlendTime();
    Info.BlendOutTime = Montage->BlendOut.GetBlendTime();
    Info.BlendOutTriggerTime = Montage->BlendOutTriggerTime;
    Info.bEnableRootMotionTranslation = Montage->bEnableRootMotionTranslation;
    Info.bEnableRootMotionRotation = Montage->bEnableRootMotionRotation;

    // Collect slot names
    for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
    {
        Info.SlotNames.AddUnique(Track.SlotName.ToString());
    }

    return Info;
}

TArray<FMontageInfo> UAnimMontageService::FindMontagesForSkeleton(const FString& SkeletonPath)
{
    return ListMontages(TEXT("/Game"), SkeletonPath);
}

TArray<FMontageInfo> UAnimMontageService::FindMontagesUsingAnimation(const FString& AnimSequencePath)
{
    TArray<FMontageInfo> Results;

    UAnimSequence* AnimSeq = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *AnimSequencePath));
    if (!AnimSeq) return Results;

    TArray<FMontageInfo> AllMontages = ListMontages();
    for (const FMontageInfo& Info : AllMontages)
    {
        UAnimMontage* Montage = LoadMontage(Info.MontagePath);
        if (!Montage) continue;

        // Check all segments in all tracks
        for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
        {
            for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
            {
                if (Segment.GetAnimReference() == AnimSeq)
                {
                    Results.Add(Info);
                    break; // Found, move to next montage
                }
            }
            if (Results.Num() > 0 && Results.Last().MontagePath == Info.MontagePath)
            {
                break; // Already added this montage
            }
        }
    }

    return Results;
}

// ============================================================================
// SECTION MANAGEMENT
// ============================================================================

TArray<FMontageSectionInfo> UAnimMontageService::ListSections(const FString& MontagePath)
{
    TArray<FMontageSectionInfo> Results;

    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return Results;

    for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
    {
        const FCompositeSection& Section = Montage->CompositeSections[i];

        FMontageSectionInfo Info;
        Info.SectionName = Section.SectionName.ToString();
        Info.SectionIndex = i;
        Info.StartTime = Section.GetTime();

        // Calculate end time (start of next section or end of montage)
        float NextTime = Montage->GetPlayLength();
        for (const FCompositeSection& OtherSection : Montage->CompositeSections)
        {
            if (OtherSection.GetTime() > Section.GetTime() && OtherSection.GetTime() < NextTime)
            {
                NextTime = OtherSection.GetTime();
            }
        }
        Info.EndTime = NextTime;
        Info.Duration = Info.EndTime - Info.StartTime;

        // Get linked section
        const FName NextSectionName = Montage->GetNextSectionName(Section.SectionName);
        Info.NextSectionName = NextSectionName.ToString();
        Info.bLoops = (NextSectionName == Section.SectionName);

        // Count segments in this section's time range
        Info.SegmentCount = 0;
        if (Montage->SlotAnimTracks.Num() > 0)
        {
            for (const FAnimSegment& Seg : Montage->SlotAnimTracks[0].AnimTrack.AnimSegments)
            {
                if (Seg.StartPos >= Info.StartTime && Seg.StartPos < Info.EndTime)
                {
                    Info.SegmentCount++;
                }
            }
        }

        Results.Add(Info);
    }

    // Sort by start time
    Results.Sort([](const FMontageSectionInfo& A, const FMontageSectionInfo& B) {
        return A.StartTime < B.StartTime;
    });

    return Results;
}

bool UAnimMontageService::AddSection(const FString& MontagePath, const FString& SectionName, float StartTime)
{
    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage)
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::AddSection: Montage not found '%s'"), *MontagePath);
        return false;
    }

    // Validate time is within montage
    if (StartTime < 0.0f || StartTime > Montage->GetPlayLength())
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::AddSection: StartTime %.2f is out of range [0, %.2f]"),
            StartTime, Montage->GetPlayLength());
        return false;
    }

    // Check for duplicate section name
    if (Montage->GetSectionIndex(FName(*SectionName)) != INDEX_NONE)
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::AddSection: Section '%s' already exists"), *SectionName);
        return false;
    }

    // Add the section using internal API
    Montage->Modify();
    int32 NewIndex = Montage->AddAnimCompositeSection(FName(*SectionName), StartTime);

    if (NewIndex == INDEX_NONE)
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::AddSection: Failed to add section '%s'"), *SectionName);
        return false;
    }

    MarkMontageModified(Montage);
    return true;
}

bool UAnimMontageService::RemoveSection(const FString& MontagePath, const FString& SectionName)
{
    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return false;

    // Cannot remove the only section
    if (Montage->CompositeSections.Num() <= 1)
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::RemoveSection: Cannot remove the only section"));
        return false;
    }

    Montage->Modify();

    const FName SectionFName(*SectionName);
    for (int32 i = Montage->CompositeSections.Num() - 1; i >= 0; --i)
    {
        if (Montage->CompositeSections[i].SectionName == SectionFName)
        {
            Montage->CompositeSections.RemoveAt(i);
            MarkMontageModified(Montage);
            return true;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("AnimMontageService::RemoveSection: Section '%s' not found"), *SectionName);
    return false;
}

bool UAnimMontageService::SetNextSection(const FString& MontagePath, const FString& SectionName, const FString& NextSectionName)
{
    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return false;

    // Validate source section exists
    if (!ValidateSection(Montage, SectionName))
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::SetNextSection: Section '%s' not found"), *SectionName);
        return false;
    }

    // Validate target section exists (if not empty)
    if (!NextSectionName.IsEmpty() && !ValidateSection(Montage, NextSectionName))
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::SetNextSection: Target section '%s' not found"), *NextSectionName);
        return false;
    }

    Montage->Modify();
    Montage->SetNextSectionName(FName(*SectionName), FName(*NextSectionName));
    MarkMontageModified(Montage);

    return true;
}

// ============================================================================
// SLOT TRACK MANAGEMENT
// ============================================================================

TArray<FSlotTrackInfo> UAnimMontageService::ListSlotTracks(const FString& MontagePath)
{
    TArray<FSlotTrackInfo> Results;

    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return Results;

    for (int32 i = 0; i < Montage->SlotAnimTracks.Num(); ++i)
    {
        const FSlotAnimationTrack& Track = Montage->SlotAnimTracks[i];

        FSlotTrackInfo Info;
        Info.TrackIndex = i;
        Info.SlotName = Track.SlotName.ToString();
        Info.SegmentCount = Track.AnimTrack.AnimSegments.Num();

        // Calculate total duration
        Info.TotalDuration = 0.0f;
        for (const FAnimSegment& Seg : Track.AnimTrack.AnimSegments)
        {
            float SegEnd = Seg.StartPos + Seg.GetLength();
            Info.TotalDuration = FMath::Max(Info.TotalDuration, SegEnd);
        }

        Results.Add(Info);
    }

    return Results;
}

int32 UAnimMontageService::AddSlotTrack(const FString& MontagePath, const FString& SlotName)
{
    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return -1;

    Montage->Modify();

    FSlotAnimationTrack& NewTrack = Montage->SlotAnimTracks.AddDefaulted_GetRef();
    NewTrack.SlotName = FName(*SlotName);

    MarkMontageModified(Montage);

    return Montage->SlotAnimTracks.Num() - 1;
}

bool UAnimMontageService::RemoveSlotTrack(const FString& MontagePath, int32 TrackIndex)
{
    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return false;

    // Cannot remove the only track
    if (Montage->SlotAnimTracks.Num() <= 1)
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::RemoveSlotTrack: Cannot remove the only slot track"));
        return false;
    }

    if (!ValidateTrackIndex(Montage, TrackIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::RemoveSlotTrack: Invalid track index %d"), TrackIndex);
        return false;
    }

    Montage->Modify();
    Montage->SlotAnimTracks.RemoveAt(TrackIndex);
    MarkMontageModified(Montage);

    return true;
}

// ============================================================================
// ANIMATION SEGMENTS
// ============================================================================

TArray<FAnimSegmentInfo> UAnimMontageService::ListAnimSegments(const FString& MontagePath, int32 TrackIndex)
{
    TArray<FAnimSegmentInfo> Results;

    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return Results;

    if (!ValidateTrackIndex(Montage, TrackIndex)) return Results;

    const TArray<FAnimSegment>& Segments = Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments;

    for (int32 i = 0; i < Segments.Num(); ++i)
    {
        const FAnimSegment& Seg = Segments[i];

        FAnimSegmentInfo Info;
        Info.SegmentIndex = i;

        if (UAnimSequence* AnimSeq = Cast<UAnimSequence>(Seg.GetAnimReference()))
        {
            Info.AnimSequencePath = AnimSeq->GetPathName();
            Info.AnimName = AnimSeq->GetName();
        }

        Info.StartTime = Seg.StartPos;
        Info.Duration = Seg.GetLength();
        Info.PlayRate = Seg.AnimPlayRate;
        Info.AnimStartPos = Seg.AnimStartTime;
        Info.AnimEndPos = Seg.AnimEndTime;
        Info.LoopCount = Seg.LoopingCount;
        Info.bLoops = Seg.LoopingCount > 0;

        Results.Add(Info);
    }

    return Results;
}

int32 UAnimMontageService::AddAnimSegment(const FString& MontagePath, int32 TrackIndex, const FString& AnimSequencePath, float StartTime, float PlayRate)
{
    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return -1;

    if (!ValidateTrackIndex(Montage, TrackIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::AddAnimSegment: Invalid track index %d"), TrackIndex);
        return -1;
    }

    UAnimSequence* AnimSeq = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *AnimSequencePath));
    if (!AnimSeq)
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::AddAnimSegment: Failed to load animation '%s'"), *AnimSequencePath);
        return -1;
    }

    // Verify skeleton compatibility
    if (Montage->GetSkeleton() != AnimSeq->GetSkeleton())
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::AddAnimSegment: Animation skeleton mismatch"));
        return -1;
    }

    Montage->Modify();

    FAnimSegment NewSegment;
    NewSegment.SetAnimReference(AnimSeq);
    NewSegment.StartPos = StartTime;
    NewSegment.AnimStartTime = 0.0f;
    NewSegment.AnimEndTime = AnimSeq->GetPlayLength();
    NewSegment.AnimPlayRate = PlayRate;
    NewSegment.LoopingCount = 0;

    int32 Index = Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments.Add(NewSegment);

    MarkMontageModified(Montage);

    return Index;
}

bool UAnimMontageService::RemoveAnimSegment(const FString& MontagePath, int32 TrackIndex, int32 SegmentIndex)
{
    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return false;

    if (!ValidateSegmentIndex(Montage, TrackIndex, SegmentIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::RemoveAnimSegment: Invalid indices track=%d segment=%d"),
            TrackIndex, SegmentIndex);
        return false;
    }

    Montage->Modify();
    Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments.RemoveAt(SegmentIndex);
    MarkMontageModified(Montage);

    return true;
}

// ============================================================================
// BRANCHING POINTS
// ============================================================================

TArray<FBranchingPointInfo> UAnimMontageService::ListBranchingPoints(const FString& MontagePath)
{
    TArray<FBranchingPointInfo> Results;

    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return Results;

    int32 BPIndex = 0;
    for (int32 i = 0; i < Montage->Notifies.Num(); ++i)
    {
        const FAnimNotifyEvent& Notify = Montage->Notifies[i];

        if (Notify.MontageTickType == EMontageNotifyTickType::BranchingPoint)
        {
            FBranchingPointInfo Info;
            Info.Index = BPIndex++;
            Info.NotifyName = Notify.NotifyName.ToString();
            Info.TriggerTime = Notify.GetTriggerTime();

            // Find which section this branching point is in
            int32 SectionIdx = Montage->GetSectionIndexFromPosition(Info.TriggerTime);
            if (SectionIdx != INDEX_NONE)
            {
                Info.SectionName = Montage->GetSectionName(SectionIdx).ToString();
            }

            Results.Add(Info);
        }
    }

    return Results;
}

int32 UAnimMontageService::AddBranchingPoint(const FString& MontagePath, const FString& NotifyName, float TriggerTime)
{
    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return -1;

    if (TriggerTime < 0.0f || TriggerTime > Montage->GetPlayLength())
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::AddBranchingPoint: TriggerTime %.2f is out of range"), TriggerTime);
        return -1;
    }

    Montage->Modify();

    FAnimNotifyEvent& NewNotify = Montage->Notifies.AddDefaulted_GetRef();
    NewNotify.NotifyName = FName(*NotifyName);
    NewNotify.Link(Montage, TriggerTime);
    NewNotify.TriggerTimeOffset = 0.0f;
    NewNotify.MontageTickType = EMontageNotifyTickType::BranchingPoint;

    MarkMontageModified(Montage);

    // Return index among branching points only
    int32 BPCount = 0;
    for (const FAnimNotifyEvent& N : Montage->Notifies)
    {
        if (N.MontageTickType == EMontageNotifyTickType::BranchingPoint)
        {
            BPCount++;
        }
    }
    return BPCount - 1;
}

// ============================================================================
// MONTAGE CREATION
// ============================================================================

FString UAnimMontageService::CreateMontageFromAnimation(const FString& AnimSequencePath, const FString& DestPath, const FString& MontageName)
{
    UAnimSequence* AnimSeq = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *AnimSequencePath));
    if (!AnimSeq)
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::CreateMontageFromAnimation: Failed to load animation '%s'"), *AnimSequencePath);
        return TEXT("");
    }

    // Create package
    FString PackagePath = FString::Printf(TEXT("%s/%s"), *DestPath, *MontageName);
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::CreateMontageFromAnimation: Failed to create package '%s'"), *PackagePath);
        return TEXT("");
    }

    // Create montage
    UAnimMontage* NewMontage = NewObject<UAnimMontage>(Package, FName(*MontageName), RF_Public | RF_Standalone);
    if (!NewMontage)
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::CreateMontageFromAnimation: Failed to create montage object"));
        return TEXT("");
    }

    // Set skeleton
    NewMontage->SetSkeleton(AnimSeq->GetSkeleton());

    // Add default slot track with the animation
    FSlotAnimationTrack& Track = NewMontage->SlotAnimTracks.AddDefaulted_GetRef();
    Track.SlotName = FName("DefaultSlot");

    FAnimSegment& Segment = Track.AnimTrack.AnimSegments.AddDefaulted_GetRef();
    Segment.SetAnimReference(AnimSeq);
    Segment.StartPos = 0.0f;
    Segment.AnimStartTime = 0.0f;
    Segment.AnimEndTime = AnimSeq->GetPlayLength();
    Segment.AnimPlayRate = 1.0f;
    Segment.LoopingCount = 0;

    // Add default section
    NewMontage->AddAnimCompositeSection(FName("Default"), 0.0f);

    // Set default blend settings
    NewMontage->BlendIn.SetBlendTime(0.25f);
    NewMontage->BlendOut.SetBlendTime(0.25f);
    NewMontage->BlendOutTriggerTime = -1.0f;

    // Mark for save
    NewMontage->MarkPackageDirty();

    // Notify asset registry
    FAssetRegistryModule::AssetCreated(NewMontage);

    return NewMontage->GetPathName();
}

FString UAnimMontageService::CreateEmptyMontage(const FString& SkeletonPath, const FString& DestPath, const FString& MontageName)
{
    USkeleton* Skeleton = Cast<USkeleton>(StaticLoadObject(USkeleton::StaticClass(), nullptr, *SkeletonPath));
    if (!Skeleton)
    {
        UE_LOG(LogTemp, Error, TEXT("AnimMontageService::CreateEmptyMontage: Failed to load skeleton '%s'"), *SkeletonPath);
        return TEXT("");
    }

    FString PackagePath = FString::Printf(TEXT("%s/%s"), *DestPath, *MontageName);
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package) return TEXT("");

    UAnimMontage* NewMontage = NewObject<UAnimMontage>(Package, FName(*MontageName), RF_Public | RF_Standalone);
    if (!NewMontage) return TEXT("");

    NewMontage->SetSkeleton(Skeleton);

    // Add empty default slot track
    FSlotAnimationTrack& Track = NewMontage->SlotAnimTracks.AddDefaulted_GetRef();
    Track.SlotName = FName("DefaultSlot");

    // Add default section
    NewMontage->AddAnimCompositeSection(FName("Default"), 0.0f);

    NewMontage->BlendIn.SetBlendTime(0.25f);
    NewMontage->BlendOut.SetBlendTime(0.25f);

    NewMontage->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewMontage);

    return NewMontage->GetPathName();
}

// ============================================================================
// EDITOR NAVIGATION
// ============================================================================

bool UAnimMontageService::OpenMontageEditor(const FString& MontagePath)
{
    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return false;

    if (GEditor)
    {
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Montage);
        return true;
    }

    return false;
}

// ... Additional method implementations follow the same patterns ...
```

---

## Validation Rules

### Parameter Validation

| Parameter | Validation Rule | Error Handling |
|-----------|----------------|----------------|
| `MontagePath` | Non-empty, valid UAnimMontage | Return nullptr/false, log warning |
| `SectionName` | Non-empty, exists in montage | Return false, log error |
| `TrackIndex` | >= 0 and < SlotAnimTracks.Num() | Return false, log error |
| `SegmentIndex` | >= 0 and < AnimSegments.Num() | Return false, log error |
| `StartTime` | >= 0 and <= montage duration | Return false, log error |
| `PlayRate` | > 0 (non-zero, positive) | Clamp to minimum 0.01 |
| `BlendTime` | >= 0 | Clamp to minimum 0.0 |
| `AnimSequencePath` | Valid UAnimSequence, matching skeleton | Return -1, log error |

### Edge Cases

| Scenario | Behavior |
|----------|----------|
| Add section with duplicate name | Fail with error |
| Remove the only section | Fail with error |
| Remove the only slot track | Fail with error |
| Add segment with mismatched skeleton | Fail with error |
| Set next section to non-existent section | Fail with error |
| Section start time at exact montage end | Allow (creates zero-length section) |
| Overlapping segments in same track | Allow (Unreal handles layering) |
| Empty NextSectionName in SetNextSection | Valid - clears link, section ends montage |

---

## Thread Safety & Performance

### Thread Safety

```cpp
// All service methods should be called from the GAME THREAD only
// Asset loading and modification are not thread-safe in Unreal

// Incorrect - will crash:
AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, []() {
    UAnimMontageService::AddSection(...); // WRONG!
});

// Correct - use game thread:
AsyncTask(ENamedThreads::GameThread, []() {
    UAnimMontageService::AddSection(...); // OK
});
```

### Performance Considerations

| Operation | Cost | Mitigation |
|-----------|------|------------|
| `ListMontages()` with large `/Game` path | High (asset registry scan) | Use specific search paths |
| `LoadMontage()` for unloaded asset | Medium (disk I/O) | Cache in calling code if repeated |
| `MarkMontageModified()` | Low | Called once per modification |
| Batch section additions | Medium | Use single `Modify()` call for batch |

### Batch Operation Pattern

```cpp
// For multiple operations, minimize overhead:
UAnimMontage* Montage = LoadMontage(MontagePath);
if (!Montage) return;

Montage->Modify();  // Single undo checkpoint

// Perform multiple operations...
for (const auto& Section : SectionsToAdd)
{
    Montage->AddAnimCompositeSection(Section.Name, Section.Time);
}

MarkMontageModified(Montage);  // Single refresh
```

---

## Undo/Redo Support

### How It Works

```cpp
// Montage->Modify() creates an undo transaction checkpoint
// Call BEFORE any modifications:

bool UAnimMontageService::AddSection(...)
{
    UAnimMontage* Montage = LoadMontage(MontagePath);
    if (!Montage) return false;

    // Create undo checkpoint BEFORE modifying
    Montage->Modify();

    // Now make changes
    Montage->AddAnimCompositeSection(Name, Time);

    // Mark dirty for save and refresh editors
    MarkMontageModified(Montage);

    return true;
}
```

### Transaction Scope

- Each method creates its own undo transaction
- Multiple method calls = multiple undo steps
- For atomic multi-operation undo, use batch methods (not yet implemented)

---

## Debugging & Troubleshooting

### Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Changes not visible in editor | `PostEditChange()` not called | Ensure `MarkMontageModified()` is called |
| Asset won't save | `MarkPackageDirty()` not called | Ensure `MarkMontageModified()` is called |
| Undo doesn't work | `Modify()` not called before changes | Call `Montage->Modify()` first |
| Python sees stale data | Cached references | Re-load asset after modifications |
| Section not found | Case-sensitive FName | Verify exact section name |
| Animation skeleton mismatch | Different skeletons | Use `FindMontagesForSkeleton()` to find compatible |

### Debug Logging

```cpp
// Enable verbose logging for debugging:
UE_LOG(LogTemp, Verbose, TEXT("AnimMontageService: Loading '%s'"), *MontagePath);
UE_LOG(LogTemp, Warning, TEXT("AnimMontageService: Section '%s' not found"), *SectionName);
UE_LOG(LogTemp, Error, TEXT("AnimMontageService: Critical failure in AddSection"));

// Log current state for debugging:
UE_LOG(LogTemp, Log, TEXT("Montage '%s' has %d sections, %d tracks"),
    *Montage->GetName(),
    Montage->CompositeSections.Num(),
    Montage->SlotAnimTracks.Num());
```

### Python Debugging

```python
import unreal

# Check if montage exists
info = unreal.AnimMontageService.get_montage_info("/Game/Montages/AM_Test")
if not info.montage_path:
    print("Montage not found!")

# List sections to verify
sections = unreal.AnimMontageService.list_sections("/Game/Montages/AM_Test")
for s in sections:
    print(f"Section: {s.section_name} at {s.start_time}s -> {s.next_section_name}")

# Verify skeleton compatibility
skeleton = unreal.AnimMontageService.get_montage_skeleton("/Game/Montages/AM_Test")
print(f"Uses skeleton: {skeleton}")
```

---

## Future Considerations

### Potential Enhancements

1. **Batch Operations API**
   ```cpp
   static bool AddSectionsBatch(const FString& MontagePath, const TArray<FSectionDefinition>& Sections);
   static bool ApplyMontageTemplate(const FString& MontagePath, const FMontageTemplate& Template);
   ```

2. **Validation Mode**
   ```cpp
   // Dry-run mode that reports what would change without modifying
   static FMontageValidationResult ValidateChanges(const FString& MontagePath, const FMontageChangeset& Changes);
   ```

3. **Event Hooks**
   ```cpp
   // Delegate for montage modification events
   DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMontageModified, const FString&, EMontageMod);
   ```

4. **Import/Export**
   ```cpp
   static bool ExportMontageToJson(const FString& MontagePath, const FString& JsonPath);
   static FString ImportMontageFromJson(const FString& JsonPath, const FString& DestPath);
   ```

### Integration Points

- **AnimSequenceService**: Share notify management code via inheritance or helper class
- **SkeletonService**: Validate slot names against skeleton slot definitions
- **AssetService**: Common asset creation/duplication utilities

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | TBD | Initial implementation - Core CRUD operations |
| | | - ListMontages, GetMontageInfo |
| | | - Section management (Add/Remove/Rename) |
| | | - Section linking |
| | | - Slot track management |
| | | - Animation segments |
| | | - Branching points |
| | | - Montage creation |
| | | - Editor navigation |

---

## Checklist for Implementation

- [ ] Create `UAnimMontageService.h` with all DTOs and method declarations
- [ ] Create `UAnimMontageService.cpp` with implementations
- [ ] Add unit tests for each method category
- [ ] Create skill.md documentation
- [ ] Create test prompt files
- [ ] Verify Python exposure works correctly
- [ ] Test undo/redo functionality
- [ ] Test with various montage configurations
- [ ] Performance test with large asset counts
- [ ] Document any discovered limitations
