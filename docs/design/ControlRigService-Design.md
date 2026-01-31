# ControlRigService Design Document

## Overview

**ControlRigService** provides comprehensive CRUD operations for Control Rig assets in Unreal Engine via VibeUE's Python API. This service (58 assets) enables AI-assisted procedural animation manipulation including rig hierarchy management, control configuration, constraint setup, and runtime control for IK, physics, and pose-space deformations.

**Target UE Version**: 5.7+
**Dependencies**: ControlRig plugin (enabled by default in 5.0+)

---

## Asset Types Covered

| Asset Type | UE Class | Description |
|------------|----------|-------------|
| Control Rig | `UControlRig` | Procedural animation rig with controls and constraints |
| Control Rig Blueprint | `UControlRigBlueprint` | Blueprint-based control rig definition |
| Rig Hierarchy | `URigHierarchy` | Hierarchy of bones, controls, nulls, and connectors |
| Rig Control | `FRigControl` | Animatable control element (transform, float, bool, etc.) |
| Rig Bone | `FRigBone` | Bone element mirroring skeletal hierarchy |
| Rig Null | `FRigNull` | Transform-only helper element |
| Rig Connector | `FRigConnector` | Connection points for modular rigs |
| Control Rig Sequence | `UControlRigSequence` | Sequencer track for control rig animation |

---

## Architecture

### Service Class

```cpp
UCLASS()
class VIBEUE_API UControlRigService : public UObject
{
    GENERATED_BODY()

public:
    // === CONTROL RIG DISCOVERY ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FControlRigInfo> ListControlRigs(const FString& SearchPath = TEXT("/Game"));

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FControlRigInfo GetControlRigInfo(const FString& ControlRigPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FControlRigInfo> FindControlRigsForSkeleton(const FString& SkeletonPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FControlRigInfo> FindControlRigsByType(const FString& BaseClass);

    // === HIERARCHY ELEMENTS ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FRigElementInfo> ListAllElements(const FString& ControlRigPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FRigElementInfo> ListElementsByType(const FString& ControlRigPath, ERigElementType ElementType);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FRigElementInfo GetElementInfo(const FString& ControlRigPath, const FString& ElementName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FString> GetElementChildren(const FString& ControlRigPath, const FString& ElementName, bool bRecursive = false);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FString GetElementParent(const FString& ControlRigPath, const FString& ElementName);

    // === CONTROL MANAGEMENT ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FRigControlInfo> ListControls(const FString& ControlRigPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FRigControlInfo GetControlInfo(const FString& ControlRigPath, const FString& ControlName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool AddControl(const FString& ControlRigPath, const FString& ControlName, ERigControlType ControlType, const FString& ParentName = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool RemoveControl(const FString& ControlRigPath, const FString& ControlName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool RenameControl(const FString& ControlRigPath, const FString& OldName, const FString& NewName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlParent(const FString& ControlRigPath, const FString& ControlName, const FString& NewParentName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlSettings(const FString& ControlRigPath, const FString& ControlName, const FRigControlSettings& Settings);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FRigControlSettings GetControlSettings(const FString& ControlRigPath, const FString& ControlName);

    // === CONTROL VALUES ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FTransform GetControlTransform(const FString& ControlRigPath, const FString& ControlName, bool bInitial = false);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlTransform(const FString& ControlRigPath, const FString& ControlName, const FTransform& Transform, bool bInitial = false);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static float GetControlFloat(const FString& ControlRigPath, const FString& ControlName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlFloat(const FString& ControlRigPath, const FString& ControlName, float Value);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool GetControlBool(const FString& ControlRigPath, const FString& ControlName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlBool(const FString& ControlRigPath, const FString& ControlName, bool Value);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FVector GetControlVector(const FString& ControlRigPath, const FString& ControlName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlVector(const FString& ControlRigPath, const FString& ControlName, const FVector& Value);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static int32 GetControlInteger(const FString& ControlRigPath, const FString& ControlName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlInteger(const FString& ControlRigPath, const FString& ControlName, int32 Value);

    // === CONTROL LIMITS ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FRigControlLimits GetControlLimits(const FString& ControlRigPath, const FString& ControlName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlLimits(const FString& ControlRigPath, const FString& ControlName, const FRigControlLimits& Limits);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlTranslationLimits(const FString& ControlRigPath, const FString& ControlName, bool bEnabled, const FVector& Min, const FVector& Max);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlRotationLimits(const FString& ControlRigPath, const FString& ControlName, bool bEnabled, const FRotator& Min, const FRotator& Max);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlScaleLimits(const FString& ControlRigPath, const FString& ControlName, bool bEnabled, const FVector& Min, const FVector& Max);

    // === BONE ELEMENTS ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FRigBoneInfo> ListBones(const FString& ControlRigPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FRigBoneInfo GetBoneInfo(const FString& ControlRigPath, const FString& BoneName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FTransform GetBoneTransform(const FString& ControlRigPath, const FString& BoneName, bool bInitial = false, bool bGlobal = true);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetBoneTransform(const FString& ControlRigPath, const FString& BoneName, const FTransform& Transform, bool bInitial = false, bool bGlobal = true);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool ImportBonesFromSkeleton(const FString& ControlRigPath, const FString& SkeletonPath);

    // === NULL ELEMENTS ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FRigNullInfo> ListNulls(const FString& ControlRigPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool AddNull(const FString& ControlRigPath, const FString& NullName, const FString& ParentName = TEXT(""), const FTransform& InitialTransform = FTransform::Identity);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool RemoveNull(const FString& ControlRigPath, const FString& NullName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FTransform GetNullTransform(const FString& ControlRigPath, const FString& NullName, bool bGlobal = true);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetNullTransform(const FString& ControlRigPath, const FString& NullName, const FTransform& Transform, bool bGlobal = true);

    // === CONNECTORS (MODULAR RIGS) ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FRigConnectorInfo> ListConnectors(const FString& ControlRigPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FRigConnectorInfo GetConnectorInfo(const FString& ControlRigPath, const FString& ConnectorName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool AddConnector(const FString& ControlRigPath, const FString& ConnectorName, bool bIsPrimary, const FString& ParentName = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool RemoveConnector(const FString& ControlRigPath, const FString& ConnectorName);

    // === RIG UNITS/NODES ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FRigUnitInfo> ListRigUnits(const FString& ControlRigPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FRigUnitInfo GetRigUnitInfo(const FString& ControlRigPath, const FString& UnitName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FString> GetAvailableRigUnits(const FString& CategoryFilter = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool AddRigUnit(const FString& ControlRigPath, const FString& UnitClass, FVector2D NodePosition = FVector2D::ZeroVector);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool RemoveRigUnit(const FString& ControlRigPath, const FString& UnitName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool ConnectRigUnits(const FString& ControlRigPath, const FString& SourceUnit, const FString& SourcePin, const FString& TargetUnit, const FString& TargetPin);

    // === SPACES & SPACE SWITCHING ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FString> GetAvailableSpaces(const FString& ControlRigPath, const FString& ControlName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FString GetCurrentSpace(const FString& ControlRigPath, const FString& ControlName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetCurrentSpace(const FString& ControlRigPath, const FString& ControlName, const FString& SpaceName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool AddSpace(const FString& ControlRigPath, const FString& ControlName, const FString& SpaceName, const FString& SpaceElement);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool RemoveSpace(const FString& ControlRigPath, const FString& ControlName, const FString& SpaceName);

    // === CONTROL RIG CREATION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FString CreateControlRig(const FString& SkeletonPath, const FString& DestPath, const FString& Name);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FString CreateControlRigFromTemplate(const FString& TemplatePath, const FString& DestPath, const FString& Name);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool DuplicateControlRig(const FString& SourcePath, const FString& DestPath, const FString& NewName);

    // === CONTROL SHAPES ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FString> GetAvailableControlShapes();

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FString GetControlShape(const FString& ControlRigPath, const FString& ControlName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlShape(const FString& ControlRigPath, const FString& ControlName, const FString& ShapeName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlShapeTransform(const FString& ControlRigPath, const FString& ControlName, const FTransform& ShapeTransform);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static FLinearColor GetControlColor(const FString& ControlRigPath, const FString& ControlName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SetControlColor(const FString& ControlRigPath, const FString& ControlName, const FLinearColor& Color);

    // === COMPILATION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool CompileControlRig(const FString& ControlRigPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static TArray<FString> GetCompilationErrors(const FString& ControlRigPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool IsControlRigValid(const FString& ControlRigPath);

    // === EDITOR NAVIGATION ===
    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool OpenControlRigEditor(const FString& ControlRigPath);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SelectControl(const FString& ControlRigPath, const FString& ControlName);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool SelectMultipleControls(const FString& ControlRigPath, const TArray<FString>& ControlNames);

    UFUNCTION(BlueprintCallable, Category = "VibeUE|ControlRig")
    static bool FocusOnElement(const FString& ControlRigPath, const FString& ElementName);
};
```

---

## Data Transfer Objects (DTOs)

### FControlRigInfo

```cpp
USTRUCT(BlueprintType)
struct FControlRigInfo
{
    GENERATED_BODY()

    /** Asset path of the control rig */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ControlRigPath;

    /** Display name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ControlRigName;

    /** Parent class name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ParentClass;

    /** Associated skeleton path (if any) */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString SkeletonPath;

    /** Number of controls */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    int32 ControlCount = 0;

    /** Number of bones */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    int32 BoneCount = 0;

    /** Number of nulls */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    int32 NullCount = 0;

    /** Number of connectors */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    int32 ConnectorCount = 0;

    /** Number of rig units/nodes */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    int32 RigUnitCount = 0;

    /** Whether rig is compiled and valid */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    bool bIsValid = false;

    /** Whether this is a modular rig */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    bool bIsModular = false;
};
```

### FRigElementInfo

```cpp
USTRUCT(BlueprintType)
struct FRigElementInfo
{
    GENERATED_BODY()

    /** Element name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ElementName;

    /** Element type (Control, Bone, Null, Connector) */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ElementType;

    /** Parent element name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ParentName;

    /** Number of children */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    int32 ChildCount = 0;

    /** Current global transform */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FTransform GlobalTransform;

    /** Initial/setup transform */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FTransform InitialTransform;
};
```

### FRigControlInfo

```cpp
USTRUCT(BlueprintType)
struct FRigControlInfo
{
    GENERATED_BODY()

    /** Control name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ControlName;

    /** Control type (Transform, Float, Bool, Integer, Vector2D, etc.) */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ControlType;

    /** Parent element name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ParentName;

    /** Display name for UI */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString DisplayName;

    /** Current transform value */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FTransform Transform;

    /** Whether control is animatable */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    bool bAnimatable = true;

    /** Whether control is visible in viewport */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    bool bGizmoVisible = true;

    /** Shape name used for visualization */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ShapeName;

    /** Control color */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FLinearColor Color;

    /** Driven elements (bones, other controls) */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    TArray<FString> DrivenElements;

    /** Available spaces for this control */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    TArray<FString> AvailableSpaces;

    /** Currently active space */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString CurrentSpace;
};
```

### FRigControlSettings

```cpp
USTRUCT(BlueprintType)
struct FRigControlSettings
{
    GENERATED_BODY()

    /** Control type */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ControlType;

    /** Display name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString DisplayName;

    /** Whether control is animatable */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    bool bAnimatable = true;

    /** Whether gizmo is visible */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    bool bGizmoVisible = true;

    /** Whether to draw limits */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    bool bDrawLimits = false;

    /** Primary axis */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString PrimaryAxis;

    /** Shape name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ShapeName;

    /** Shape transform offset */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FTransform ShapeTransform;

    /** Control color */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FLinearColor Color;

    /** Whether control is a proxy */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    bool bIsProxy = false;
};
```

### FRigControlLimits

```cpp
USTRUCT(BlueprintType)
struct FRigControlLimits
{
    GENERATED_BODY()

    /** Translation limits enabled */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    bool bTranslationLimitEnabled = false;

    /** Translation minimum */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FVector TranslationMin;

    /** Translation maximum */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FVector TranslationMax;

    /** Rotation limits enabled */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    bool bRotationLimitEnabled = false;

    /** Rotation minimum */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FRotator RotationMin;

    /** Rotation maximum */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FRotator RotationMax;

    /** Scale limits enabled */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    bool bScaleLimitEnabled = false;

    /** Scale minimum */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FVector ScaleMin;

    /** Scale maximum */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FVector ScaleMax;
};
```

### FRigBoneInfo

```cpp
USTRUCT(BlueprintType)
struct FRigBoneInfo
{
    GENERATED_BODY()

    /** Bone name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString BoneName;

    /** Bone index */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    int32 BoneIndex = -1;

    /** Parent bone name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ParentName;

    /** Initial transform */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FTransform InitialTransform;

    /** Current transform */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FTransform CurrentTransform;

    /** Type of bone (Imported, User) */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString BoneType;
};
```

### FRigNullInfo

```cpp
USTRUCT(BlueprintType)
struct FRigNullInfo
{
    GENERATED_BODY()

    /** Null name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString NullName;

    /** Parent element name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ParentName;

    /** Initial transform */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FTransform InitialTransform;

    /** Current transform */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FTransform CurrentTransform;
};
```

### FRigConnectorInfo

```cpp
USTRUCT(BlueprintType)
struct FRigConnectorInfo
{
    GENERATED_BODY()

    /** Connector name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ConnectorName;

    /** Whether this is a primary connector */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    bool bIsPrimary = false;

    /** Parent element */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ParentName;

    /** Connected module (if any) */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString ConnectedModule;

    /** Transform */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FTransform Transform;
};
```

### FRigUnitInfo

```cpp
USTRUCT(BlueprintType)
struct FRigUnitInfo
{
    GENERATED_BODY()

    /** Node name in the graph */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString UnitName;

    /** Unit/node class name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString UnitClass;

    /** Display name */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString DisplayName;

    /** Category */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FString Category;

    /** Input pin names */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    TArray<FString> InputPins;

    /** Output pin names */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    TArray<FString> OutputPins;

    /** Position in graph */
    UPROPERTY(BlueprintReadWrite, Category = "ControlRig")
    FVector2D NodePosition;
};
```

---

## Implementation Details

### Key Unreal APIs Used

#### Control Rig Blueprint Access
```cpp
// Load control rig blueprint
UControlRigBlueprint* Blueprint = LoadObject<UControlRigBlueprint>(nullptr, *ControlRigPath);

// Get hierarchy
URigHierarchy* Hierarchy = Blueprint->Hierarchy;

// Get model (graph)
URigVMController* Controller = Blueprint->GetController();
URigVMGraph* Graph = Blueprint->GetModel();
```

#### Hierarchy Element Operations
```cpp
// List all elements
TArray<FRigElementKey> Keys = Hierarchy->GetAllKeys();

// Get elements by type
TArray<FRigElementKey> Controls = Hierarchy->GetAllKeys(ERigElementType::Control);
TArray<FRigElementKey> Bones = Hierarchy->GetAllKeys(ERigElementType::Bone);
TArray<FRigElementKey> Nulls = Hierarchy->GetAllKeys(ERigElementType::Null);

// Get element info
FRigElementKey Key(ElementName, ERigElementType::Control);
FRigControlElement* Control = Hierarchy->Find<FRigControlElement>(Key);

// Get parent/children
FRigElementKey ParentKey = Hierarchy->GetFirstParent(Key);
TArray<FRigElementKey> Children = Hierarchy->GetChildren(Key, bRecursive);
```

#### Control Operations
```cpp
// Add control
FRigControlSettings Settings;
Settings.ControlType = ERigControlType::EulerTransform;
Settings.bAnimatable = true;
FRigElementKey NewKey = Hierarchy->GetController()->AddControl(
    ControlName,
    ParentKey,
    Settings,
    FRigControlValue(),
    FTransform::Identity,
    FTransform::Identity
);

// Get/set control value
FRigControlValue Value = Hierarchy->GetControlValue(ControlKey);
Hierarchy->SetControlValue(ControlKey, NewValue);

// Control transforms
FTransform GlobalTransform = Hierarchy->GetGlobalTransform(ControlKey);
Hierarchy->SetGlobalTransform(ControlKey, NewTransform);
```

#### Control Shapes
```cpp
// Get shape
FRigControlElement* Control = Hierarchy->Find<FRigControlElement>(Key);
FName ShapeName = Control->Settings.ShapeName;

// Set shape
Control->Settings.ShapeName = NewShapeName;
Control->Settings.ShapeTransform = ShapeTransform;
Control->Settings.ShapeColor = Color;

// Available shapes
TArray<FName> Shapes = UControlRigShapeLibrary::GetShapeNames();
```

#### Rig Units/Nodes
```cpp
// Get graph nodes
const TArray<URigVMNode*>& Nodes = Graph->GetNodes();

// Add unit
URigVMNode* NewNode = Controller->AddUnitNode(
    UnitClass,
    FString(),       // Method name
    NodePosition,
    FString(),       // Node name
    true             // Setup undo
);

// Connect nodes
Controller->AddLink(
    SourceNode->GetName(), SourcePinName,
    TargetNode->GetName(), TargetPinName,
    true
);
```

#### Compilation
```cpp
// Compile
Blueprint->RecompileVM();

// Check status
bool bValid = Blueprint->Status == BS_UpToDate;

// Get errors
TArray<FRigVMCompilerMessage> Messages = Blueprint->GetCompileMessages();
```

---

## Method Count Summary

| Category | Methods | Description |
|----------|---------|-------------|
| Control Rig Discovery | 4 | List, get info, find by skeleton, find by type |
| Hierarchy Elements | 5 | List all, by type, get info, children, parent |
| Control Management | 8 | List, info, add, remove, rename, parent, settings |
| Control Values | 10 | Get/set transform, float, bool, vector, integer |
| Control Limits | 5 | Get/set limits, translation, rotation, scale |
| Bone Elements | 5 | List, info, get/set transform, import from skeleton |
| Null Elements | 5 | List, add, remove, get/set transform |
| Connectors | 4 | List, info, add, remove |
| Rig Units/Nodes | 6 | List, info, available, add, remove, connect |
| Spaces | 5 | Get available, get/set current, add, remove |
| Control Rig Creation | 3 | Create, from template, duplicate |
| Control Shapes | 6 | Available shapes, get/set shape, transform, color |
| Compilation | 3 | Compile, errors, is valid |
| Editor Navigation | 4 | Open editor, select, select multiple, focus |
| **TOTAL** | **73** | |

---

## Python API Examples

### List Control Rig Controls

```python
import unreal

cr_path = "/Game/ControlRigs/CR_Mannequin"

# Get all controls
controls = unreal.ControlRigService.list_controls(cr_path)

print(f"Controls in {cr_path}:")
for ctrl in controls:
    space = f" [{ctrl.current_space}]" if ctrl.current_space else ""
    print(f"  {ctrl.control_name} ({ctrl.control_type}){space}")
    if ctrl.driven_elements:
        print(f"    Drives: {', '.join(ctrl.driven_elements)}")
```

### Create IK Control

```python
import unreal

cr_path = "/Game/ControlRigs/CR_Character"

# Add IK target control for foot
unreal.ControlRigService.add_control(
    cr_path,
    "IK_Foot_L",
    unreal.RigControlType.EULER_TRANSFORM,
    "root"  # Parent to root for world space IK
)

# Configure the control
settings = unreal.RigControlSettings()
settings.display_name = "Left Foot IK"
settings.animatable = True
settings.gizmo_visible = True
settings.shape_name = "Sphere"
settings.color = unreal.LinearColor(0.0, 0.5, 1.0, 1.0)

unreal.ControlRigService.set_control_settings(cr_path, "IK_Foot_L", settings)

# Set initial position at foot bone
foot_transform = unreal.ControlRigService.get_bone_transform(cr_path, "foot_l")
unreal.ControlRigService.set_control_transform(cr_path, "IK_Foot_L", foot_transform, True)

print("IK control created")
```

### Set Control Limits

```python
import unreal

cr_path = "/Game/ControlRigs/CR_Character"
control_name = "Spine_Twist"

# Set rotation limits for spine twist control
unreal.ControlRigService.set_control_rotation_limits(
    cr_path,
    control_name,
    True,  # Enable limits
    unreal.Rotator(-45, 0, -30),  # Min (pitch, yaw, roll)
    unreal.Rotator(45, 0, 30)     # Max
)

# Verify
limits = unreal.ControlRigService.get_control_limits(cr_path, control_name)
print(f"Rotation limits: {limits.rotation_min} to {limits.rotation_max}")
```

### Work with Spaces

```python
import unreal

cr_path = "/Game/ControlRigs/CR_Character"
control_name = "Hand_L"

# Check available spaces
spaces = unreal.ControlRigService.get_available_spaces(cr_path, control_name)
print(f"Available spaces: {spaces}")

# Add a new space
unreal.ControlRigService.add_space(
    cr_path,
    control_name,
    "Head",        # Space name
    "head"         # Element to use as space
)

# Switch to head space
unreal.ControlRigService.set_current_space(cr_path, control_name, "Head")
print(f"Switched {control_name} to Head space")
```

### Import Skeleton and Setup Basic Rig

```python
import unreal

skeleton_path = "/Game/Characters/SK_Character"

# Create new control rig
cr_path = unreal.ControlRigService.create_control_rig(
    skeleton_path,
    "/Game/ControlRigs",
    "CR_Character"
)

# Import bones from skeleton
unreal.ControlRigService.import_bones_from_skeleton(cr_path, skeleton_path)

# Add master control
unreal.ControlRigService.add_control(
    cr_path, "Master", unreal.RigControlType.EULER_TRANSFORM
)

# Add spine controls
spine_bones = ["spine_01", "spine_02", "spine_03"]
for bone in spine_bones:
    ctrl_name = f"FK_{bone}"
    unreal.ControlRigService.add_control(
        cr_path, ctrl_name, unreal.RigControlType.EULER_TRANSFORM, bone
    )
    # Match control to bone transform
    bone_transform = unreal.ControlRigService.get_bone_transform(cr_path, bone)
    unreal.ControlRigService.set_control_transform(cr_path, ctrl_name, bone_transform, True)

# Compile the rig
unreal.ControlRigService.compile_control_rig(cr_path)

if unreal.ControlRigService.is_control_rig_valid(cr_path):
    print(f"Control rig created and compiled: {cr_path}")
else:
    errors = unreal.ControlRigService.get_compilation_errors(cr_path)
    for error in errors:
        print(f"Error: {error}")
```

### Customize Control Shapes

```python
import unreal

cr_path = "/Game/ControlRigs/CR_Character"

# Get available shapes
shapes = unreal.ControlRigService.get_available_control_shapes()
print(f"Available shapes: {shapes}")

# Set shapes for different control types
control_shapes = {
    "Master": ("FourArrows", unreal.LinearColor(1, 1, 0, 1)),       # Yellow
    "IK_Foot_L": ("Box", unreal.LinearColor(0, 0.5, 1, 1)),         # Blue
    "IK_Foot_R": ("Box", unreal.LinearColor(1, 0, 0.5, 1)),         # Red
    "FK_spine_01": ("Circle", unreal.LinearColor(0, 1, 0.5, 1)),    # Cyan
}

for control_name, (shape, color) in control_shapes.items():
    unreal.ControlRigService.set_control_shape(cr_path, control_name, shape)
    unreal.ControlRigService.set_control_color(cr_path, control_name, color)
```

---

## Skill Documentation

Create `Content/Skills/controlrig/skill.md`:

```markdown
---
name: controlrig
display_name: Control Rigs
description: Create and manipulate control rigs for procedural animation
vibeue_classes:
  - ControlRigService
unreal_classes:
  - ControlRig
  - ControlRigBlueprint
  - RigHierarchy
keywords:
  - control rig
  - procedural
  - ik
  - fk
  - rigging
  - constraint
---

# Control Rig Service Skill

## Critical Rules

### Control Types

| Type | Use Case | Value Type |
|------|----------|------------|
| `EulerTransform` | Full transform control | Transform |
| `TransformNoScale` | Position + rotation only | Transform |
| `Position` | Position only | Vector |
| `Rotation` | Rotation only | Rotator |
| `Scale` | Scale only | Vector |
| `Float` | Blend weights, IK strength | Float |
| `Integer` | Enum selections | Int |
| `Bool` | Toggles (IK on/off) | Bool |

### Hierarchy Rules

1. **Controls** can parent to: Bones, Nulls, other Controls
2. **Bones** maintain skeletal hierarchy
3. **Nulls** are helper transforms (like locators)
4. **Connectors** are for modular rig assembly

### Compilation Required

⚠️ Changes to the control rig require recompilation:

```python
unreal.ControlRigService.compile_control_rig(cr_path)
if not unreal.ControlRigService.is_control_rig_valid(cr_path):
    print("Compilation failed!")
```

## Workflows

### FK Spine Chain

```python
import unreal

def create_fk_chain(cr_path, bones, parent="Master"):
    """Create FK controls for a bone chain"""
    prev_control = parent

    for bone in bones:
        ctrl_name = f"FK_{bone}"
        unreal.ControlRigService.add_control(
            cr_path, ctrl_name,
            unreal.RigControlType.EULER_TRANSFORM,
            prev_control
        )

        # Position at bone
        transform = unreal.ControlRigService.get_bone_transform(cr_path, bone)
        unreal.ControlRigService.set_control_transform(cr_path, ctrl_name, transform, True)

        # Style
        unreal.ControlRigService.set_control_shape(cr_path, ctrl_name, "Circle")

        prev_control = ctrl_name

    return prev_control  # Return last control
```

### IK Limb Setup

```python
import unreal

def setup_ik_limb(cr_path, ik_name, end_bone, pole_offset):
    """Setup IK control with pole vector"""

    # IK target at end effector
    unreal.ControlRigService.add_control(
        cr_path, f"IK_{ik_name}",
        unreal.RigControlType.EULER_TRANSFORM
    )

    # Pole vector control
    unreal.ControlRigService.add_control(
        cr_path, f"Pole_{ik_name}",
        unreal.RigControlType.POSITION
    )

    # Position controls
    end_transform = unreal.ControlRigService.get_bone_transform(cr_path, end_bone)
    unreal.ControlRigService.set_control_transform(
        cr_path, f"IK_{ik_name}", end_transform, True
    )

    pole_transform = unreal.Transform(location=pole_offset)
    unreal.ControlRigService.set_control_transform(
        cr_path, f"Pole_{ik_name}", pole_transform, True
    )
```

## Data Structures

### FRigControlInfo
| Property | Type | Description |
|----------|------|-------------|
| control_name | string | Unique identifier |
| control_type | string | EulerTransform, Float, etc. |
| parent_name | string | Parent element |
| shape_name | string | Visual shape |
| color | LinearColor | Display color |
| available_spaces | array[string] | Space options |
| current_space | string | Active space |
```

---

## Test Prompts

### 01_list_controls.md
```markdown
List all controls in CR_Mannequin showing their type and parent.
```

### 02_add_fk_control.md
```markdown
Add an FK control for spine_02 bone in CR_Character with a yellow circle shape.
```

### 03_setup_ik.md
```markdown
Create an IK target control for the left foot at the foot_l bone position.
```

### 04_control_limits.md
```markdown
Set rotation limits on the spine twist control: pitch ±45°, roll ±30°.
```

### 05_compile_rig.md
```markdown
Compile CR_Character and report any errors.
```

---

## Dependencies

- **ControlRig plugin**: Must be enabled
- **RigVM**: Virtual machine for rig evaluation

Add to VibeUE.Build.cs:
```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "ControlRig",
    "RigVM",
});
```

---

## Implementation Priority

1. **Phase 1 - Core Read Operations** (High Priority)
   - `ListControlRigs`, `GetControlRigInfo`
   - `ListControls`, `GetControlInfo`
   - `ListBones`, `GetBoneTransform`

2. **Phase 2 - Control Management** (High Priority)
   - `AddControl`, `RemoveControl`, `RenameControl`
   - `SetControlParent`, `SetControlSettings`
   - `GetControlTransform`, `SetControlTransform`

3. **Phase 3 - Control Configuration** (Medium Priority)
   - `SetControlLimits`, `SetControlShape`, `SetControlColor`
   - Control value accessors (float, bool, etc.)

4. **Phase 4 - Hierarchy Management** (Medium Priority)
   - Null operations
   - Bone import and management
   - Connector operations

5. **Phase 5 - Advanced Features** (Lower Priority)
   - Rig unit/node operations
   - Space switching
   - Compilation and validation

---

## Error Handling

All methods should return appropriate errors for:
- Asset not found
- Invalid element name
- Element type mismatch
- Circular parent reference
- Compilation errors
- Invalid control type for operation
- Skeleton mismatch
