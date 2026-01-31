# AnimSequenceService Design Document

## Overview

**AnimSequenceService** provides comprehensive CRUD operations for Animation Sequence assets in Unreal Engine via VibeUE's Python API. This is the **highest volume service** (1,511 assets) enabling AI-assisted animation manipulation including keyframe access, curve editing, notify management, sync markers, and animation data extraction.

**Target UE Version**: 5.7+
**Dependencies**: None (core animation module)

---

## Asset Types Covered

| Asset Type | UE Class | Description |
|------------|----------|-------------|
| Animation Sequence | `UAnimSequence` | Primary animation asset with keyframes, curves, notifies |
| Anim Notify | `UAnimNotify` | Point-in-time events (sounds, effects) |
| Anim Notify State | `UAnimNotifyState` | Duration-based events (trails, states) |
| Float Curve | `FFloatCurve` | Custom float curves for blend weights, parameters |
| Bone Compression Settings | `UAnimBoneCompressionSettings` | Per-bone compression configuration |

---

## Architecture

### Service Class

```cpp
UCLASS()
class VIBEUE_API UAnimSequenceService : public UObject
{
    GENERATED_BODY()

public:
    // === ANIMATION DISCOVERY ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FAnimSequenceInfo> ListAnimSequences(const FString& SearchPath = TEXT("/Game"), const FString& SkeletonFilter = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static FAnimSequenceInfo GetAnimSequenceInfo(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FAnimSequenceInfo> FindAnimationsForSkeleton(const FString& SkeletonPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FAnimSequenceInfo> SearchAnimations(const FString& NamePattern, const FString& SearchPath = TEXT("/Game"));

    // === ANIMATION PROPERTIES ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static float GetAnimationLength(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static float GetAnimationFrameRate(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static int32 GetAnimationFrameCount(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetAnimationFrameRate(const FString& AnimPath, float NewFrameRate);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static FString GetAnimationSkeleton(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetRateScale(const FString& AnimPath, float RateScale);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static float GetRateScale(const FString& AnimPath);

    // === BONE TRACK DATA ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FString> GetAnimatedBones(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FBoneAnimationTrack> GetBoneTracks(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static FBoneAnimationTrack GetBoneTrack(const FString& AnimPath, const FString& BoneName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static FTransform GetBoneTransformAtTime(const FString& AnimPath, const FString& BoneName, float Time, bool bGlobalSpace = false);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FTransform> GetBoneTransformAtFrame(const FString& AnimPath, const FString& BoneName, int32 Frame, bool bGlobalSpace = false);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FAnimKeyframe> GetBoneKeyframes(const FString& AnimPath, const FString& BoneName);

    // === POSE EXTRACTION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FBonePose> GetPoseAtTime(const FString& AnimPath, float Time, bool bGlobalSpace = false);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FBonePose> GetPoseAtFrame(const FString& AnimPath, int32 Frame, bool bGlobalSpace = false);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static FTransform GetRootMotionAtTime(const FString& AnimPath, float Time);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static FTransform GetTotalRootMotion(const FString& AnimPath);

    // === CURVE DATA ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FAnimCurveInfo> ListCurves(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static FAnimCurveInfo GetCurveInfo(const FString& AnimPath, const FString& CurveName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static float GetCurveValueAtTime(const FString& AnimPath, const FString& CurveName, float Time);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FCurveKeyframe> GetCurveKeyframes(const FString& AnimPath, const FString& CurveName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool AddCurve(const FString& AnimPath, const FString& CurveName, EAnimCurveType CurveType = EAnimCurveType::Float);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool RemoveCurve(const FString& AnimPath, const FString& CurveName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetCurveKeys(const FString& AnimPath, const FString& CurveName, const TArray<FCurveKeyframe>& Keys);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool AddCurveKey(const FString& AnimPath, const FString& CurveName, float Time, float Value);

    // === ANIM NOTIFIES ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FAnimNotifyInfo> ListNotifies(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static FAnimNotifyInfo GetNotifyInfo(const FString& AnimPath, int32 NotifyIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static int32 AddNotify(const FString& AnimPath, const FString& NotifyClass, float TriggerTime, const FString& NotifyName = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static int32 AddNotifyState(const FString& AnimPath, const FString& NotifyStateClass, float StartTime, float Duration, const FString& NotifyName = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool RemoveNotify(const FString& AnimPath, int32 NotifyIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetNotifyTriggerTime(const FString& AnimPath, int32 NotifyIndex, float NewTime);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetNotifyDuration(const FString& AnimPath, int32 NotifyIndex, float NewDuration);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetNotifyTrack(const FString& AnimPath, int32 NotifyIndex, int32 TrackIndex);

    // === SYNC MARKERS ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FSyncMarkerInfo> ListSyncMarkers(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool AddSyncMarker(const FString& AnimPath, const FString& MarkerName, float Time);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool RemoveSyncMarker(const FString& AnimPath, const FString& MarkerName, float Time);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetSyncMarkerTime(const FString& AnimPath, int32 MarkerIndex, float NewTime);

    // === ADDITIVE ANIMATION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static EAdditiveAnimationType GetAdditiveAnimType(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetAdditiveAnimType(const FString& AnimPath, EAdditiveAnimationType Type);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static FString GetAdditiveBasePose(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetAdditiveBasePose(const FString& AnimPath, const FString& BasePoseAnimPath);

    // === ROOT MOTION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool GetEnableRootMotion(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetEnableRootMotion(const FString& AnimPath, bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static ERootMotionRootLock GetRootMotionRootLock(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetRootMotionRootLock(const FString& AnimPath, ERootMotionRootLock LockType);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool GetForceRootLock(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetForceRootLock(const FString& AnimPath, bool bForce);

    // === COMPRESSION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static FAnimCompressionInfo GetCompressionInfo(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetCompressionScheme(const FString& AnimPath, const FString& CompressionSchemePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool CompressAnimation(const FString& AnimPath);

    // === IMPORT/EXPORT ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static FString ExportAnimationToJson(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static TArray<FString> GetSourceFiles(const FString& AnimPath);

    // === EDITOR NAVIGATION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool OpenAnimationEditor(const FString& AnimPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool SetPreviewTime(const FString& AnimPath, float Time);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool PlayPreview(const FString& AnimPath, bool bLoop = false);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|Animation")
    static bool StopPreview(const FString& AnimPath);
};
```

---

## Data Transfer Objects (DTOs)

### FAnimSequenceInfo

```cpp
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

    /** Additive animation type */
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
```

### FBoneAnimationTrack

```cpp
USTRUCT(BlueprintType)
struct FBoneAnimationTrack
{
    GENERATED_BODY()

    /** Bone name this track animates */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString BoneName;

    /** Number of position keys */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    int32 PositionKeyCount = 0;

    /** Number of rotation keys */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    int32 RotationKeyCount = 0;

    /** Number of scale keys */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    int32 ScaleKeyCount = 0;

    /** Whether track has any keys */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    bool bHasKeys = false;
};
```

### FAnimKeyframe

```cpp
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
    FVector Position;

    /** Rotation value */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FQuat Rotation;

    /** Scale value */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FVector Scale;
};
```

### FBonePose

```cpp
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
```

### FAnimCurveInfo

```cpp
USTRUCT(BlueprintType)
struct FAnimCurveInfo
{
    GENERATED_BODY()

    /** Name of the curve */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString CurveName;

    /** Curve type (float, vector, transform) */
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
```

### FCurveKeyframe

```cpp
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

    /** Interpolation mode */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString InterpMode;

    /** Tangent mode */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString TangentMode;

    /** Arrive tangent */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float ArriveTangent = 0.0f;

    /** Leave tangent */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float LeaveTangent = 0.0f;
};
```

### FAnimNotifyInfo

```cpp
USTRUCT(BlueprintType)
struct FAnimNotifyInfo
{
    GENERATED_BODY()

    /** Index of the notify */
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
    FLinearColor NotifyColor;
};
```

### FSyncMarkerInfo

```cpp
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
```

### FAnimCompressionInfo

```cpp
USTRUCT(BlueprintType)
struct FAnimCompressionInfo
{
    GENERATED_BODY()

    /** Compression scheme name */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    FString CompressionScheme;

    /** Raw data size */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    int64 RawSize = 0;

    /** Compressed size */
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
```

---

## Implementation Details

### Key Unreal APIs Used

#### Animation Data Access
```cpp
// Get animation sequence
UAnimSequence* AnimSeq = LoadObject<UAnimSequence>(nullptr, *AnimPath);

// Basic properties
float Duration = AnimSeq->GetPlayLength();
float FrameRate = AnimSeq->GetSamplingFrameRate().AsDecimal();
int32 NumFrames = AnimSeq->GetNumberOfSampledKeys();
USkeleton* Skeleton = AnimSeq->GetSkeleton();

// Get bone transform at time
FTransform BoneTransform;
AnimSeq->GetBoneTransform(BoneTransform, BoneIndex, Time, bUseRawData);

// Get all bone transforms (pose)
FCompactPose Pose;
FAnimExtractContext Context(Time);
AnimSeq->GetBonePose(Pose, Context, bLooping);
```

#### Curve Access
```cpp
// List curves
const TArray<FFloatCurve>& Curves = AnimSeq->GetCurveData().FloatCurves;

// Get curve by name
const FFloatCurve* Curve = AnimSeq->GetCurveData().GetCurveData(CurveName);

// Evaluate curve at time
float Value = Curve->FloatCurve.Eval(Time);

// Add curve
AnimSeq->Modify(true);
AnimSeq->RawCurveData.AddCurveData(CurveName, CurveFlags);

// Set curve keys
FRichCurve& RichCurve = Curve->FloatCurve;
RichCurve.Reset();
for (const FCurveKeyframe& Key : Keys)
{
    FKeyHandle Handle = RichCurve.AddKey(Key.Time, Key.Value);
    RichCurve.SetKeyInterpMode(Handle, InterpMode);
}
```

#### Notify Operations
```cpp
// List notifies
const TArray<FAnimNotifyEvent>& Notifies = AnimSeq->Notifies;

// Add notify
FAnimNotifyEvent& NewNotify = AnimSeq->Notifies.AddDefaulted_GetRef();
NewNotify.NotifyName = NotifyName;
NewNotify.Link(AnimSeq, TriggerTime);
NewNotify.Notify = NewObject<UAnimNotify>(AnimSeq, NotifyClass);

// Add notify state
FAnimNotifyEvent& NewState = AnimSeq->Notifies.AddDefaulted_GetRef();
NewState.NotifyStateClass = NewObject<UAnimNotifyState>(AnimSeq, StateClass);
NewState.SetDuration(Duration);
NewState.Link(AnimSeq, StartTime);

// Modify notify
AnimSeq->Notifies[Index].SetTime(NewTime);
AnimSeq->Notifies[Index].SetDuration(NewDuration);

// Remove notify
AnimSeq->Notifies.RemoveAt(Index);
```

#### Sync Markers
```cpp
// List markers
const TArray<FAnimSyncMarker>& Markers = AnimSeq->AuthoredSyncMarkers;

// Add marker
FAnimSyncMarker NewMarker;
NewMarker.MarkerName = MarkerName;
NewMarker.Time = Time;
AnimSeq->AuthoredSyncMarkers.Add(NewMarker);

// Sort after modification
AnimSeq->AuthoredSyncMarkers.Sort([](const FAnimSyncMarker& A, const FAnimSyncMarker& B) { return A.Time < B.Time; });
```

#### Root Motion
```cpp
// Check root motion
bool bRootMotion = AnimSeq->bEnableRootMotion;

// Get root motion transform
FTransform RootMotion = AnimSeq->ExtractRootMotion(StartTime, EndTime, bLoop);

// Configure root motion
AnimSeq->bEnableRootMotion = true;
AnimSeq->RootMotionRootLock = ERootMotionRootLock::AnimFirstFrame;
AnimSeq->bForceRootLock = true;
```

---

## Method Count Summary

| Category | Methods | Description |
|----------|---------|-------------|
| Animation Discovery | 4 | List, get info, find by skeleton, search |
| Animation Properties | 7 | Length, frame rate, frame count, skeleton, rate scale |
| Bone Track Data | 6 | Animated bones, tracks, transform at time/frame, keyframes |
| Pose Extraction | 4 | Pose at time/frame, root motion |
| Curve Data | 8 | List, info, value, keyframes, add, remove, set keys |
| Anim Notifies | 8 | List, info, add notify/state, remove, time, duration, track |
| Sync Markers | 4 | List, add, remove, set time |
| Additive Animation | 4 | Get/set type, get/set base pose |
| Root Motion | 6 | Enable, root lock, force lock |
| Compression | 3 | Info, set scheme, compress |
| Import/Export | 2 | Export JSON, source files |
| Editor Navigation | 4 | Open editor, preview time, play, stop |
| **TOTAL** | **60** | |

---

## Python API Examples

### List Animations for Skeleton

```python
import unreal

skeleton_path = "/Game/Characters/Mannequin/SK_Mannequin"

# Find all animations for this skeleton
anims = unreal.AnimSequenceService.find_animations_for_skeleton(skeleton_path)
print(f"Found {len(anims)} animations")

for anim in anims:
    print(f"  {anim.anim_name}: {anim.duration:.2f}s, {anim.frame_count} frames")
```

### Extract Pose Data

```python
import unreal

anim_path = "/Game/Animations/Run_Fwd"

# Get pose at specific time
pose = unreal.AnimSequenceService.get_pose_at_time(anim_path, 0.5, True)

for bone in pose:
    loc = bone.transform.location
    print(f"{bone.bone_name}: ({loc.x:.2f}, {loc.y:.2f}, {loc.z:.2f})")
```

### Add Animation Notify

```python
import unreal

anim_path = "/Game/Animations/Attack_Slash"

# Add footstep sound notify at 0.3 seconds
notify_idx = unreal.AnimSequenceService.add_notify(
    anim_path,
    "/Script/Engine.AnimNotify_PlaySound",
    0.3,
    "Footstep_L"
)

# Add particle trail state from 0.1s to 0.5s
state_idx = unreal.AnimSequenceService.add_notify_state(
    anim_path,
    "/Script/Engine.AnimNotifyState_Trail",
    0.1,
    0.4,
    "SwordTrail"
)

print(f"Added notify at index {notify_idx}, state at index {state_idx}")
```

### Work with Curves

```python
import unreal

anim_path = "/Game/Animations/Locomotion_Blend"

# Add a custom curve for blend weight
unreal.AnimSequenceService.add_curve(anim_path, "UpperBodyWeight")

# Set curve keyframes
keys = [
    {"time": 0.0, "value": 0.0},
    {"time": 0.5, "value": 1.0},
    {"time": 1.0, "value": 0.0}
]
unreal.AnimSequenceService.set_curve_keys(anim_path, "UpperBodyWeight", keys)

# Read curve value at time
value = unreal.AnimSequenceService.get_curve_value_at_time(anim_path, "UpperBodyWeight", 0.5)
print(f"Curve value at 0.5s: {value}")
```

### Configure Root Motion

```python
import unreal

anim_path = "/Game/Animations/Dash_Forward"

# Enable root motion
unreal.AnimSequenceService.set_enable_root_motion(anim_path, True)
unreal.AnimSequenceService.set_root_motion_root_lock(
    anim_path,
    unreal.RootMotionRootLock.ANIM_FIRST_FRAME
)

# Get total root motion displacement
total_motion = unreal.AnimSequenceService.get_total_root_motion(anim_path)
print(f"Root motion: {total_motion.location.x:.1f} forward")
```

### Add Sync Markers for Locomotion

```python
import unreal

anim_path = "/Game/Animations/Walk_Fwd"

# Add foot sync markers for sync groups
unreal.AnimSequenceService.add_sync_marker(anim_path, "LeftFoot", 0.0)
unreal.AnimSequenceService.add_sync_marker(anim_path, "RightFoot", 0.5)

# List all markers
markers = unreal.AnimSequenceService.list_sync_markers(anim_path)
for marker in markers:
    print(f"  {marker.marker_name} at {marker.time:.3f}s")
```

---

## Skill Documentation

Create `Content/Skills/animsequence/skill.md`:

```markdown
---
name: animsequence
display_name: Animation Sequences
description: Query and manipulate animation sequences, keyframes, curves, and notifies
vibeue_classes:
  - AnimSequenceService
unreal_classes:
  - AnimSequence
  - AnimNotify
  - AnimNotifyState
keywords:
  - animation
  - sequence
  - keyframe
  - curve
  - notify
  - sync marker
  - root motion
---

# Animation Sequence Service Skill

## Critical Rules

### Time vs Frame

Most APIs work with **time in seconds**. Convert as needed:
```python
frame_rate = unreal.AnimSequenceService.get_animation_frame_rate(anim_path)
frame = int(time * frame_rate)
time = frame / frame_rate
```

### Notify States vs Instant Notifies

| Type | Method | Duration |
|------|--------|----------|
| Instant Notify | `add_notify()` | 0 (point in time) |
| State Notify | `add_notify_state()` | > 0 (duration) |

### Root Motion Requirements

For root motion to work properly:
1. Enable root motion on the animation
2. Root bone must have movement in the animation
3. Character must support root motion (Character Movement Component)

## Workflows

### Batch Add Footstep Notifies

```python
import unreal

def add_footstep_notifies(anim_path):
    """Add footstep notifies at foot plant frames"""
    info = unreal.AnimSequenceService.get_anim_sequence_info(anim_path)

    # Get foot bone positions over time
    for frame in range(info.frame_count):
        time = frame / info.frame_rate

        left_pos = unreal.AnimSequenceService.get_bone_transform_at_time(
            anim_path, "foot_l", time).location

        # Detect foot plant (when foot is lowest)
        # ... detection logic ...

        unreal.AnimSequenceService.add_notify(
            anim_path,
            "AnimNotify_Footstep",
            time,
            "Footstep_L"
        )
```

### Export Animation Data for ML

```python
import unreal
import json

def export_for_training(anim_path, output_file):
    """Export animation as JSON for ML training"""
    info = unreal.AnimSequenceService.get_anim_sequence_info(anim_path)

    frames = []
    for frame in range(info.frame_count):
        pose = unreal.AnimSequenceService.get_pose_at_frame(anim_path, frame, True)
        frame_data = {
            "frame": frame,
            "bones": {
                bone.bone_name: {
                    "location": [bone.transform.location.x, bone.transform.location.y, bone.transform.location.z],
                    "rotation": [bone.transform.rotation.x, bone.transform.rotation.y, bone.transform.rotation.z, bone.transform.rotation.w]
                }
                for bone in pose
            }
        }
        frames.append(frame_data)

    with open(output_file, 'w') as f:
        json.dump({"anim": info.anim_name, "frames": frames}, f)
```

## Data Structures

### FAnimSequenceInfo
| Property | Type | Description |
|----------|------|-------------|
| anim_path | string | Asset path |
| anim_name | string | Display name |
| skeleton_path | string | Associated skeleton |
| duration | float | Length in seconds |
| frame_rate | float | Frames per second |
| frame_count | int | Total frames |
| bone_track_count | int | Animated bones |
| curve_count | int | Custom curves |
| notify_count | int | Notify events |
| enable_root_motion | bool | Root motion enabled |
```

---

## Test Prompts

### 01_list_animations.md
```markdown
List all animations in /Game/Animations that use SK_Mannequin skeleton.
```

### 02_get_pose.md
```markdown
Get the pose of the character at frame 30 of the Run_Fwd animation.
```

### 03_add_notify.md
```markdown
Add a footstep sound notify at 0.25 seconds in the Walk_Fwd animation.
```

### 04_read_curves.md
```markdown
List all curves in the Locomotion_BlendSpace animation and show their values at time 0.5.
```

### 05_root_motion.md
```markdown
Enable root motion on Attack_Dash and get the total forward movement distance.
```

---

## Dependencies

- **AnimationCore**: Core animation module
- **AnimationBlueprintLibrary**: For animation blueprint operations

Add to VibeUE.Build.cs:
```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "AnimationCore",
});
```

---

## Implementation Priority

1. **Phase 1 - Core Read Operations** (Critical - High Volume)
   - `ListAnimSequences`, `GetAnimSequenceInfo`, `FindAnimationsForSkeleton`
   - `GetAnimationLength`, `GetAnimationFrameRate`, `GetAnimationFrameCount`
   - `GetAnimatedBones`, `GetBoneTransformAtTime`, `GetPoseAtTime`

2. **Phase 2 - Notify Management** (High Priority)
   - `ListNotifies`, `AddNotify`, `AddNotifyState`, `RemoveNotify`
   - `SetNotifyTriggerTime`, `SetNotifyDuration`

3. **Phase 3 - Curve Operations** (Medium Priority)
   - `ListCurves`, `GetCurveValueAtTime`, `GetCurveKeyframes`
   - `AddCurve`, `RemoveCurve`, `SetCurveKeys`

4. **Phase 4 - Root Motion & Sync** (Medium Priority)
   - Root motion configuration
   - Sync marker management

5. **Phase 5 - Advanced Features** (Lower Priority)
   - Compression settings
   - Export functions
   - Editor preview controls

---

## Error Handling

All methods should return appropriate errors for:
- Asset not found
- Invalid time/frame values
- Skeleton mismatch
- Invalid notify class
- Curve not found
- Read-only asset (cooked)
