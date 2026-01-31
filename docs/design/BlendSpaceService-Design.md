# BlendSpaceService Design Document

## Overview

**BlendSpaceService** provides comprehensive CRUD operations for Blend Space assets in Unreal Engine via VibeUE's Python API. While a **lower volume service** (7 assets), blend spaces are critical for smooth locomotion and aim offset systems, enabling AI-assisted configuration of sample points, axis settings, and blend parameters.

**Target UE Version**: 5.7+
**Dependencies**: None (core animation module)

---

## Asset Types Covered

| Asset Type | UE Class | Description |
|------------|----------|-------------|
| Blend Space | `UBlendSpace` | 2D blend between animations based on two parameters |
| Blend Space 1D | `UBlendSpace1D` | 1D blend along a single axis |
| Aim Offset | `UAimOffsetBlendSpace` | Specialized blend space for aim offsets |
| Aim Offset 1D | `UAimOffsetBlendSpace1D` | 1D aim offset blend |
| Blend Sample | `FBlendSample` | Individual animation sample point in the blend space |

---

## Architecture

### Service Class

```cpp
UCLASS()
class VIBEUE_API UBlendSpaceService : public UObject
{
    GENERATED_BODY()

public:
    // === BLEND SPACE DISCOVERY ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static TArray<FBlendSpaceInfo> ListBlendSpaces(const FString& SearchPath = TEXT("/Game"), const FString& SkeletonFilter = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static FBlendSpaceInfo GetBlendSpaceInfo(const FString& BlendSpacePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static TArray<FBlendSpaceInfo> FindBlendSpacesForSkeleton(const FString& SkeletonPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static TArray<FBlendSpaceInfo> ListAimOffsets(const FString& SearchPath = TEXT("/Game"));

    // === BLEND SPACE PROPERTIES ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static FString GetBlendSpaceSkeleton(const FString& BlendSpacePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool Is1DBlendSpace(const FString& BlendSpacePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool IsAimOffset(const FString& BlendSpacePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static FString GetBlendSpaceType(const FString& BlendSpacePath);

    // === AXIS CONFIGURATION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static FBlendAxisInfo GetHorizontalAxisInfo(const FString& BlendSpacePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static FBlendAxisInfo GetVerticalAxisInfo(const FString& BlendSpacePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetHorizontalAxis(const FString& BlendSpacePath, const FString& AxisName, float MinValue, float MaxValue, int32 GridDivisions = 4);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetVerticalAxis(const FString& BlendSpacePath, const FString& AxisName, float MinValue, float MaxValue, int32 GridDivisions = 4);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetAxisRange(const FString& BlendSpacePath, EBlendSpaceAxis Axis, float MinValue, float MaxValue);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetAxisName(const FString& BlendSpacePath, EBlendSpaceAxis Axis, const FString& NewName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetGridDivisions(const FString& BlendSpacePath, EBlendSpaceAxis Axis, int32 Divisions);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetAxisInterpolationType(const FString& BlendSpacePath, EBlendSpaceAxis Axis, EFilterInterpolationType InterpType);

    // === SAMPLE MANAGEMENT ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static TArray<FBlendSampleInfo> ListSamples(const FString& BlendSpacePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static FBlendSampleInfo GetSampleInfo(const FString& BlendSpacePath, int32 SampleIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static FBlendSampleInfo GetSampleAtPosition(const FString& BlendSpacePath, float X, float Y = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static int32 AddSample(const FString& BlendSpacePath, const FString& AnimSequencePath, float X, float Y = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool RemoveSample(const FString& BlendSpacePath, int32 SampleIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool RemoveSampleAtPosition(const FString& BlendSpacePath, float X, float Y = 0.0f, float Tolerance = 0.01f);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetSamplePosition(const FString& BlendSpacePath, int32 SampleIndex, float X, float Y = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetSampleAnimation(const FString& BlendSpacePath, int32 SampleIndex, const FString& AnimSequencePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetSamplePlayRate(const FString& BlendSpacePath, int32 SampleIndex, float PlayRate);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SnapSampleToGrid(const FString& BlendSpacePath, int32 SampleIndex);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SnapAllSamplesToGrid(const FString& BlendSpacePath);

    // === BLEND PARAMETERS ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static FBlendParameters GetBlendParameters(const FString& BlendSpacePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetTargetWeightInterpolationSpeedPerSec(const FString& BlendSpacePath, float Speed);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetInterpolationPerAxis(const FString& BlendSpacePath, bool bPerAxis);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetSmoothingTime(const FString& BlendSpacePath, float SmoothingTime);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetNotifyTriggerMode(const FString& BlendSpacePath, ENotifyTriggerMode Mode);

    // === PREVIEW/EVALUATION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static TArray<FBlendSampleWeight> EvaluateBlendSpace(const FString& BlendSpacePath, float X, float Y = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static FVector2D GetBlendSpacePosition(const FString& BlendSpacePath, const TArray<FBlendSampleWeight>& Weights);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static TArray<FTriangleInfo> GetTriangulation(const FString& BlendSpacePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool ValidateBlendSpace(const FString& BlendSpacePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static TArray<FString> GetValidationErrors(const FString& BlendSpacePath);

    // === BLEND SPACE CREATION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static FString CreateBlendSpace(const FString& SkeletonPath, const FString& DestPath, const FString& Name, bool b1D = false);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static FString CreateAimOffset(const FString& SkeletonPath, const FString& DestPath, const FString& Name, bool b1D = false);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool DuplicateBlendSpace(const FString& SourcePath, const FString& DestPath, const FString& NewName);

    // === LOCOMOTION PRESETS ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetupLocomotionBlendSpace(const FString& BlendSpacePath, float MaxSpeed = 600.0f);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetupDirectionalBlendSpace(const FString& BlendSpacePath, float MaxSpeed = 600.0f);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetupAimOffsetAxes(const FString& BlendSpacePath, float YawRange = 90.0f, float PitchRange = 90.0f);

    // === SYNC MARKERS ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool GetRequireSyncMarkers(const FString& BlendSpacePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetRequireSyncMarkers(const FString& BlendSpacePath, bool bRequire);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static TArray<FString> GetCommonSyncMarkers(const FString& BlendSpacePath);

    // === EDITOR NAVIGATION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool OpenBlendSpaceEditor(const FString& BlendSpacePath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool SetPreviewPosition(const FString& BlendSpacePath, float X, float Y = 0.0f);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|BlendSpace")
    static bool FocusOnSample(const FString& BlendSpacePath, int32 SampleIndex);
};
```

---

## Data Transfer Objects (DTOs)

### FBlendSpaceInfo

```cpp
USTRUCT(BlueprintType)
struct FBlendSpaceInfo
{
    GENERATED_BODY()

    /** Asset path of the blend space */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FString BlendSpacePath;

    /** Display name */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FString BlendSpaceName;

    /** Associated skeleton path */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FString SkeletonPath;

    /** Blend space type (BlendSpace, BlendSpace1D, AimOffset, AimOffset1D) */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FString BlendSpaceType;

    /** Whether this is a 1D blend space */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    bool bIs1D = false;

    /** Whether this is an aim offset */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    bool bIsAimOffset = false;

    /** Number of sample points */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    int32 SampleCount = 0;

    /** Horizontal axis name */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FString HorizontalAxisName;

    /** Horizontal axis range */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FVector2D HorizontalAxisRange;

    /** Vertical axis name (empty for 1D) */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FString VerticalAxisName;

    /** Vertical axis range (0,0 for 1D) */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FVector2D VerticalAxisRange;

    /** Target weight interpolation speed */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    float InterpolationSpeed = 0.0f;

    /** Whether sync markers are required */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    bool bRequireSyncMarkers = false;
};
```

### FBlendAxisInfo

```cpp
USTRUCT(BlueprintType)
struct FBlendAxisInfo
{
    GENERATED_BODY()

    /** Axis name (e.g., "Speed", "Direction", "Yaw") */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FString AxisName;

    /** Minimum value */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    float MinValue = 0.0f;

    /** Maximum value */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    float MaxValue = 100.0f;

    /** Number of grid divisions */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    int32 GridDivisions = 4;

    /** Interpolation type */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FString InterpolationType;

    /** Whether to snap to grid */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    bool bSnapToGrid = false;
};
```

### FBlendSampleInfo

```cpp
USTRUCT(BlueprintType)
struct FBlendSampleInfo
{
    GENERATED_BODY()

    /** Sample index */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    int32 SampleIndex = 0;

    /** Animation sequence path */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FString AnimSequencePath;

    /** Animation name */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FString AnimName;

    /** X position (horizontal axis value) */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    float X = 0.0f;

    /** Y position (vertical axis value, 0 for 1D) */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    float Y = 0.0f;

    /** Playback rate multiplier */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    float PlayRate = 1.0f;

    /** Whether sample is snapped to grid */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    bool bSnappedToGrid = false;

    /** Animation duration */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    float AnimDuration = 0.0f;

    /** Whether animation is valid */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    bool bIsValid = true;
};
```

### FBlendSampleWeight

```cpp
USTRUCT(BlueprintType)
struct FBlendSampleWeight
{
    GENERATED_BODY()

    /** Sample index */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    int32 SampleIndex = 0;

    /** Weight contribution (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    float Weight = 0.0f;

    /** Animation path */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FString AnimSequencePath;
};
```

### FBlendParameters

```cpp
USTRUCT(BlueprintType)
struct FBlendParameters
{
    GENERATED_BODY()

    /** Target weight interpolation speed per second */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    float TargetWeightInterpolationSpeedPerSec = 0.0f;

    /** Whether to interpolate per axis */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    bool bInterpolatePerAxis = false;

    /** Smoothing time for input */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    float SmoothingTime = 0.0f;

    /** How notifies are triggered */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    FString NotifyTriggerMode;

    /** Whether to allow mesh space blending */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    bool bAllowMeshSpaceBlending = false;
};
```

### FTriangleInfo

```cpp
USTRUCT(BlueprintType)
struct FTriangleInfo
{
    GENERATED_BODY()

    /** Triangle index */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    int32 TriangleIndex = 0;

    /** First vertex sample index */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    int32 SampleIndex0 = 0;

    /** Second vertex sample index */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    int32 SampleIndex1 = 0;

    /** Third vertex sample index */
    UPROPERTY(BlueprintReadWrite, Category = "BlendSpace")
    int32 SampleIndex2 = 0;
};
```

---

## Implementation Details

### Key Unreal APIs Used

#### Blend Space Data Access
```cpp
// Get blend space
UBlendSpace* BlendSpace = LoadObject<UBlendSpace>(nullptr, *BlendSpacePath);
// or for 1D
UBlendSpace1D* BlendSpace1D = LoadObject<UBlendSpace1D>(nullptr, *BlendSpacePath);

// Check type
bool bIs1D = BlendSpace->IsA<UBlendSpace1D>();
bool bIsAimOffset = BlendSpace->IsA<UAimOffsetBlendSpace>();

// Get skeleton
USkeleton* Skeleton = BlendSpace->GetSkeleton();
```

#### Axis Configuration
```cpp
// Get axis info (for 2D blend space)
const FBlendParameter& HorizontalAxis = BlendSpace->GetBlendParameter(0);
const FBlendParameter& VerticalAxis = BlendSpace->GetBlendParameter(1);

// Set axis properties
FBlendParameter& Param = BlendSpace->GetBlendParameter(AxisIndex);
Param.DisplayName = AxisName;
Param.Min = MinValue;
Param.Max = MaxValue;
Param.GridNum = GridDivisions;

// Set interpolation type
Param.InterpolationType = EFilterInterpolationType::BSIT_Average;
```

#### Sample Management
```cpp
// Get samples
const TArray<FBlendSample>& Samples = BlendSpace->GetBlendSamples();

// Add sample
FBlendSample NewSample;
NewSample.Animation = AnimSequence;
NewSample.SampleValue = FVector(X, Y, 0.0f);
NewSample.RateScale = PlayRate;
BlendSpace->AddSample(NewSample);

// Modify sample
BlendSpace->EditSampleValue(SampleIndex, FVector(NewX, NewY, 0.0f));

// Remove sample
BlendSpace->DeleteSample(SampleIndex);

// Snap to grid
BlendSpace->SnapSamplesToGrid();
```

#### Blend Evaluation
```cpp
// Evaluate at position
TArray<FBlendSampleData> SampleData;
BlendSpace->GetSamplesFromBlendInput(FVector(X, Y, 0.0f), SampleData);

// Each FBlendSampleData contains:
// - SampleDataIndex (sample index)
// - TotalWeight (contribution weight)
// - Animation (UAnimSequence*)
```

#### Triangulation
```cpp
// Get grid/triangulation data
const TArray<FEditorElement>& GridElements = BlendSpace->GetGridSamples();
const TArray<FBlendSpaceTriangle>& Triangles = BlendSpace->GetTriangulationData();
```

#### Blend Parameters
```cpp
// Interpolation settings
BlendSpace->InterpolationParam[0].InterpolationTime = SmoothingTime;
BlendSpace->TargetWeightInterpolationSpeedPerSec = Speed;
BlendSpace->bRotationBlendInMeshSpace = bMeshSpace;

// Notify trigger mode
BlendSpace->NotifyTriggerMode = ENotifyTriggerMode::AllAnimations;
```

---

## Method Count Summary

| Category | Methods | Description |
|----------|---------|-------------|
| Blend Space Discovery | 4 | List, get info, find by skeleton, list aim offsets |
| Blend Space Properties | 4 | Skeleton, is 1D, is aim offset, get type |
| Axis Configuration | 8 | Get/set horizontal, get/set vertical, range, name, divisions, interp |
| Sample Management | 11 | List, get info, at position, add, remove, set position/anim/rate, snap |
| Blend Parameters | 5 | Get params, interp speed, per axis, smoothing, notify mode |
| Preview/Evaluation | 5 | Evaluate, get position, triangulation, validate, errors |
| Blend Space Creation | 3 | Create blend space, create aim offset, duplicate |
| Locomotion Presets | 3 | Setup locomotion, directional, aim offset axes |
| Sync Markers | 3 | Get/set require markers, get common markers |
| Editor Navigation | 3 | Open editor, set preview position, focus sample |
| **TOTAL** | **49** | |

---

## Python API Examples

### Create Locomotion Blend Space

```python
import unreal

skeleton_path = "/Game/Characters/SK_Mannequin"

# Create a new 2D blend space for locomotion
bs_path = unreal.BlendSpaceService.create_blend_space(
    skeleton_path,
    "/Game/BlendSpaces",
    "BS_Locomotion",
    False  # 2D blend space
)

# Setup standard locomotion axes
unreal.BlendSpaceService.set_horizontal_axis(
    bs_path,
    "Direction",  # Axis name
    -180.0,       # Min (backward)
    180.0,        # Max (backward, wrapped)
    8             # Grid divisions
)

unreal.BlendSpaceService.set_vertical_axis(
    bs_path,
    "Speed",
    0.0,          # Standing
    600.0,        # Max run speed
    4
)

print(f"Created locomotion blend space: {bs_path}")
```

### Populate Blend Space with Samples

```python
import unreal

bs_path = "/Game/BlendSpaces/BS_Locomotion"

# Define sample positions (Direction, Speed)
samples = [
    # Standing (center bottom)
    ("/Game/Anims/Idle", 0.0, 0.0),

    # Walking ring
    ("/Game/Anims/Walk_Fwd", 0.0, 150.0),
    ("/Game/Anims/Walk_Bwd", 180.0, 150.0),
    ("/Game/Anims/Walk_Left", -90.0, 150.0),
    ("/Game/Anims/Walk_Right", 90.0, 150.0),

    # Running ring
    ("/Game/Anims/Run_Fwd", 0.0, 600.0),
    ("/Game/Anims/Run_Bwd", 180.0, 600.0),
    ("/Game/Anims/Run_Left", -90.0, 600.0),
    ("/Game/Anims/Run_Right", 90.0, 600.0),
]

for anim_path, direction, speed in samples:
    idx = unreal.BlendSpaceService.add_sample(bs_path, anim_path, direction, speed)
    print(f"Added sample {idx}: {anim_path} at ({direction}, {speed})")

# Snap all to grid
unreal.BlendSpaceService.snap_all_samples_to_grid(bs_path)
```

### Create 1D Speed Blend Space

```python
import unreal

skeleton_path = "/Game/Characters/SK_Mannequin"

# Create 1D blend space
bs_path = unreal.BlendSpaceService.create_blend_space(
    skeleton_path,
    "/Game/BlendSpaces",
    "BS_WalkRun",
    True  # 1D blend space
)

# Configure speed axis
unreal.BlendSpaceService.set_horizontal_axis(
    bs_path,
    "Speed",
    0.0,      # Standing
    600.0,    # Max speed
    3         # 3 divisions (idle, walk, run)
)

# Add samples along the axis
unreal.BlendSpaceService.add_sample(bs_path, "/Game/Anims/Idle", 0.0)
unreal.BlendSpaceService.add_sample(bs_path, "/Game/Anims/Walk", 150.0)
unreal.BlendSpaceService.add_sample(bs_path, "/Game/Anims/Jog", 300.0)
unreal.BlendSpaceService.add_sample(bs_path, "/Game/Anims/Run", 600.0)
```

### Create Aim Offset

```python
import unreal

skeleton_path = "/Game/Characters/SK_Mannequin"

# Create aim offset
ao_path = unreal.BlendSpaceService.create_aim_offset(
    skeleton_path,
    "/Game/AimOffsets",
    "AO_Rifle",
    False  # 2D aim offset
)

# Setup standard aim offset axes
unreal.BlendSpaceService.setup_aim_offset_axes(ao_path, 90.0, 90.0)

# Add aim poses
aim_poses = [
    # Center
    ("/Game/Anims/Aim_Center", 0.0, 0.0),
    # Yaw extremes
    ("/Game/Anims/Aim_Left", -90.0, 0.0),
    ("/Game/Anims/Aim_Right", 90.0, 0.0),
    # Pitch extremes
    ("/Game/Anims/Aim_Up", 0.0, 90.0),
    ("/Game/Anims/Aim_Down", 0.0, -90.0),
    # Corners
    ("/Game/Anims/Aim_LeftUp", -90.0, 90.0),
    ("/Game/Anims/Aim_RightUp", 90.0, 90.0),
    ("/Game/Anims/Aim_LeftDown", -90.0, -90.0),
    ("/Game/Anims/Aim_RightDown", 90.0, -90.0),
]

for anim_path, yaw, pitch in aim_poses:
    unreal.BlendSpaceService.add_sample(ao_path, anim_path, yaw, pitch)
```

### Evaluate Blend Space

```python
import unreal

bs_path = "/Game/BlendSpaces/BS_Locomotion"

# Evaluate at specific position
direction = 45.0   # Moving forward-right
speed = 400.0      # Jogging speed

weights = unreal.BlendSpaceService.evaluate_blend_space(bs_path, direction, speed)

print(f"Blend at Direction={direction}, Speed={speed}:")
for w in weights:
    if w.weight > 0.01:  # Only show significant weights
        print(f"  {w.anim_sequence_path}: {w.weight:.2%}")
```

### Validate Blend Space

```python
import unreal

bs_path = "/Game/BlendSpaces/BS_Locomotion"

# Check for issues
is_valid = unreal.BlendSpaceService.validate_blend_space(bs_path)

if not is_valid:
    errors = unreal.BlendSpaceService.get_validation_errors(bs_path)
    print("Validation errors:")
    for error in errors:
        print(f"  - {error}")
else:
    print("Blend space is valid!")

# Check triangulation coverage
triangles = unreal.BlendSpaceService.get_triangulation(bs_path)
print(f"Triangulation: {len(triangles)} triangles")
```

---

## Skill Documentation

Create `Content/Skills/blendspace/skill.md`:

```markdown
---
name: blendspace
display_name: Blend Spaces
description: Create and configure blend spaces for locomotion and aim offsets
vibeue_classes:
  - BlendSpaceService
unreal_classes:
  - BlendSpace
  - BlendSpace1D
  - AimOffsetBlendSpace
keywords:
  - blend space
  - locomotion
  - aim offset
  - blend
  - 1d
  - 2d
---

# Blend Space Service Skill

## Critical Rules

### 1D vs 2D Blend Spaces

| Type | Axes | Use Case |
|------|------|----------|
| **1D** | Single axis (X only) | Speed-based blends (walk/run) |
| **2D** | Two axes (X, Y) | Directional locomotion, aim offsets |

### Blend Space vs Aim Offset

| Type | Purpose | Base Pose |
|------|---------|-----------|
| **Blend Space** | Full body animations | N/A |
| **Aim Offset** | Additive upper body | Uses additive poses |

⚠️ Aim offset animations must be additive!

### Sample Placement Best Practices

1. **Cover the corners**: Always place samples at axis extremes
2. **Consistent spacing**: Use grid snap for clean triangulation
3. **Sync markers**: Enable for locomotion to prevent foot sliding
4. **Same skeleton**: All sample animations must use the same skeleton

## Workflows

### Standard Locomotion Setup

```python
import unreal

def create_locomotion_blend_space(skeleton_path, anims):
    """
    anims: dict mapping positions to animation paths
    e.g., {(0, 0): "/Game/Idle", (0, 300): "/Game/Walk_Fwd"}
    """
    bs = unreal.BlendSpaceService.create_blend_space(
        skeleton_path, "/Game/BlendSpaces", "BS_Locomotion"
    )

    # Standard locomotion axes
    unreal.BlendSpaceService.set_horizontal_axis(bs, "Direction", -180, 180, 8)
    unreal.BlendSpaceService.set_vertical_axis(bs, "Speed", 0, 600, 4)

    # Add samples
    for (direction, speed), anim_path in anims.items():
        unreal.BlendSpaceService.add_sample(bs, anim_path, direction, speed)

    # Enable sync markers for foot sync
    unreal.BlendSpaceService.set_require_sync_markers(bs, True)

    return bs
```

### Quick Aim Offset

```python
import unreal

def create_quick_aim_offset(skeleton_path, center_anim):
    """Create aim offset with just center pose (for testing)"""
    ao = unreal.BlendSpaceService.create_aim_offset(
        skeleton_path, "/Game/AimOffsets", "AO_Test"
    )
    unreal.BlendSpaceService.setup_aim_offset_axes(ao, 90.0, 90.0)
    unreal.BlendSpaceService.add_sample(ao, center_anim, 0.0, 0.0)
    return ao
```

## Data Structures

### FBlendSpaceInfo
| Property | Type | Description |
|----------|------|-------------|
| blend_space_path | string | Asset path |
| blend_space_type | string | Type identifier |
| sample_count | int | Number of samples |
| horizontal_axis_name | string | X-axis name |
| vertical_axis_name | string | Y-axis name |

### FBlendSampleInfo
| Property | Type | Description |
|----------|------|-------------|
| sample_index | int | Index in array |
| anim_sequence_path | string | Animation asset |
| x | float | Horizontal position |
| y | float | Vertical position |
| play_rate | float | Speed multiplier |
```

---

## Test Prompts

### 01_list_blend_spaces.md
```markdown
List all blend spaces in /Game/BlendSpaces showing their type and sample count.
```

### 02_create_locomotion.md
```markdown
Create a 2D locomotion blend space with Direction (-180 to 180) and Speed (0 to 600) axes.
```

### 03_add_samples.md
```markdown
Add walk forward, walk backward, walk left, and walk right animations to BS_Locomotion at appropriate positions.
```

### 04_create_aim_offset.md
```markdown
Create an aim offset with Yaw (-90 to 90) and Pitch (-90 to 90) and add the center aim pose.
```

### 05_evaluate_blend.md
```markdown
Evaluate BS_Locomotion at Direction=45 and Speed=300 to see which animations are blended.
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

1. **Phase 1 - Core Read Operations** (High Priority)
   - `ListBlendSpaces`, `GetBlendSpaceInfo`, `FindBlendSpacesForSkeleton`
   - `ListSamples`, `GetSampleInfo`
   - `GetHorizontalAxisInfo`, `GetVerticalAxisInfo`

2. **Phase 2 - Sample Management** (High Priority)
   - `AddSample`, `RemoveSample`, `SetSamplePosition`
   - `SetSampleAnimation`, `SnapAllSamplesToGrid`

3. **Phase 3 - Axis Configuration** (Medium Priority)
   - `SetHorizontalAxis`, `SetVerticalAxis`
   - `SetAxisRange`, `SetAxisName`, `SetGridDivisions`

4. **Phase 4 - Creation & Presets** (Medium Priority)
   - `CreateBlendSpace`, `CreateAimOffset`
   - `SetupLocomotionBlendSpace`, `SetupAimOffsetAxes`

5. **Phase 5 - Evaluation & Validation** (Lower Priority)
   - `EvaluateBlendSpace`, `GetTriangulation`
   - `ValidateBlendSpace`, `GetValidationErrors`

---

## Error Handling

All methods should return appropriate errors for:
- Asset not found
- Invalid sample index
- Animation skeleton mismatch
- Invalid axis values (min >= max)
- Position out of axis range
- Duplicate sample position
- Non-additive animation in aim offset
