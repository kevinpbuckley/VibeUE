// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UModelingService.generated.h"

class UDynamicMesh;

/**
 * Outcome of a modeling operation. Handle echoes the mesh the call acted on (or the one it
 * created), TriangleCount / VertexCount describe that mesh afterwards, AssetPath is set by the
 * asset-writing calls.
 */
USTRUCT(BlueprintType)
struct FModelingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FString Message;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	int32 Handle = -1;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	int32 TriangleCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	int32 VertexCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FString AssetPath;
};

/** Diagnostic snapshot of a session mesh — read it between stages instead of trusting a green result. */
USTRUCT(BlueprintType)
struct FModelingMeshInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FString Message;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	int32 Handle = -1;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	int32 TriangleCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	int32 VertexCount = 0;

	/** Watertight (no open border edges). Booleans and voxel ops want this true. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	bool bIsClosed = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	int32 OpenBorderEdges = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	int32 ConnectedComponents = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FVector BoundsMin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FVector BoundsMax = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	float SurfaceArea = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	float Volume = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	int32 NumUVLayers = 0;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	bool bHasVertexColors = false;

	/** Distinct material IDs in use (empty when the mesh has no material attribute). */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	TArray<int32> MaterialIDs;

	/** Named selections currently stored on this handle. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	TArray<FString> Selections;
};

USTRUCT(BlueprintType)
struct FModelingSplitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FString Message;

	/** One new session handle per connected component, largest first. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	TArray<int32> Handles;
};

/** A point found on a mesh surface by RayCast / NearestPoint. */
USTRUCT(BlueprintType)
struct FModelingSurfacePoint
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	bool bFound = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	int32 TriangleID = -1;

	/** Ray parameter (distance along the ray) for RayCast; distance from the query point for NearestPoint. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	float Distance = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FVector BaryCoords = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FString Message;
};

/** Surface-to-surface distances between two session meshes (MeasureDistance). */
USTRUCT(BlueprintType)
struct FModelingDistanceReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FString Message;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	float MaxDistance = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	float MinDistance = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	float AverageDistance = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	float RootMeanSquareDeviation = 0.f;
};

USTRUCT(BlueprintType)
struct FModelingBakeResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FString Message;

	/** Saved Texture2D asset paths, in the order of the requested bake types. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	TArray<FString> TexturePaths;
};

/** One bone for CreateBones. Transform is in mesh space (the space the vertices are in); an empty ParentName makes it the root. */
USTRUCT(BlueprintType)
struct FModelingBoneDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FString ParentName;

	/** Bone frame in mesh space. Point its X axis along a hinge line and the control surface deflects by rolling the bone. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FTransform Transform;
};

/** A bone on a session mesh: hierarchy, reference pose, and how many vertices it influences. */
USTRUCT(BlueprintType)
struct FModelingBoneInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	int32 Index = -1;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FString Name;

	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FString ParentName;

	/** Reference pose relative to the parent bone. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FTransform LocalTransform;

	/** Reference pose in mesh space. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	FTransform MeshTransform;

	/** Vertices carrying a non-zero weight on this bone. */
	UPROPERTY(BlueprintReadWrite, Category = "VibeUE|Modeling")
	int32 InfluencedVertices = 0;
};

/**
 * Programmatic mesh modeling — the operator half of Unreal's Modeling Mode, driven without a
 * viewport. Meshes live in an in-editor session as integer handles wrapping a UDynamicMesh:
 * create or load one, chain operations on it, save it as a StaticMesh / SkeletalMesh asset,
 * release it. Operations that act on part of a mesh take a named selection made on the same
 * handle (pass "" for the whole mesh). Everything is built on GeometryScript, so the long tail
 * is one call away: GetDynamicMesh(handle) hands the UDynamicMesh to any
 * unreal.GeometryScript_* function in the same Python script.
 *
 * Python: unreal.ModelingService.<snake_case>(...). See the `modeling` skill for the workflow.
 */
UCLASS(BlueprintType)
class VIBEUE_API UModelingService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// =====================================================================
	// Session
	// =====================================================================

	/** Create an empty session mesh. Result.Handle is the new handle. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Create Mesh"))
	static FModelingResult CreateMesh();

	/** Load a StaticMesh asset's LOD into a new session mesh (materials become material IDs by section). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Load Mesh From Static Mesh"))
	static FModelingResult LoadMeshFromStaticMesh(const FString& AssetPath, int32 LODIndex = 0);

	/** Load a SkeletalMesh asset's LOD (with bone weights) into a new session mesh. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Load Mesh From Skeletal Mesh"))
	static FModelingResult LoadMeshFromSkeletalMesh(const FString& AssetPath, int32 LODIndex = 0);

	/** Load the static mesh of a level actor (by label) into a new session mesh; bWorldSpace bakes the actor transform in. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Load Mesh From Actor"))
	static FModelingResult LoadMeshFromActor(const FString& ActorLabel, bool bWorldSpace = false, int32 LODIndex = 0);

	/** Duplicate a session mesh into a new handle. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Copy Mesh"))
	static FModelingResult CopyMesh(int32 Handle);

	/** Free one session mesh (and its selections). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Release Mesh"))
	static bool ReleaseMesh(int32 Handle);

	/** Free every session mesh. Returns how many were released. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Release All Meshes"))
	static int32 ReleaseAllMeshes();

	/** Handles currently alive in the session. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "List Meshes"))
	static TArray<int32> ListMeshes();

	/** Counts, bounds, closedness, components, UV layers, material IDs, stored selections. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Get Mesh Info"))
	static FModelingMeshInfo GetMeshInfo(int32 Handle);

	/** The UDynamicMesh behind a handle, for direct use with unreal.GeometryScript_* libraries (Python only). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (DisplayName = "Get Dynamic Mesh"))
	static UDynamicMesh* GetDynamicMesh(int32 Handle);

	// =====================================================================
	// Primitives (appended into an existing handle; Transform places them)
	// =====================================================================

	/** Origin: "Base" (sits on Z=0) or "Center". Steps subdivide the faces. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Box"))
	static FModelingResult AppendBox(int32 Handle, FTransform Transform, float DimensionX = 100.f, float DimensionY = 100.f, float DimensionZ = 100.f,
		int32 StepsX = 0, int32 StepsY = 0, int32 StepsZ = 0, const FString& Origin = TEXT("Base"), int32 MaterialID = 0);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Sphere"))
	static FModelingResult AppendSphere(int32 Handle, FTransform Transform, float Radius = 50.f, int32 StepsPhi = 10, int32 StepsTheta = 16,
		const FString& Origin = TEXT("Center"), int32 MaterialID = 0);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Cylinder"))
	static FModelingResult AppendCylinder(int32 Handle, FTransform Transform, float Radius = 50.f, float Height = 100.f, int32 RadialSteps = 12,
		int32 HeightSteps = 0, bool bCapped = true, const FString& Origin = TEXT("Base"), int32 MaterialID = 0);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Cone"))
	static FModelingResult AppendCone(int32 Handle, FTransform Transform, float BaseRadius = 50.f, float TopRadius = 5.f, float Height = 100.f,
		int32 RadialSteps = 12, int32 HeightSteps = 4, bool bCapped = true, const FString& Origin = TEXT("Base"), int32 MaterialID = 0);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Capsule"))
	static FModelingResult AppendCapsule(int32 Handle, FTransform Transform, float Radius = 30.f, float LineLength = 75.f, int32 HemisphereSteps = 5,
		int32 CircleSteps = 8, const FString& Origin = TEXT("Base"), int32 MaterialID = 0);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Torus"))
	static FModelingResult AppendTorus(int32 Handle, FTransform Transform, float MajorRadius = 50.f, float MinorRadius = 25.f, int32 MajorSteps = 16,
		int32 MinorSteps = 8, const FString& Origin = TEXT("Base"), int32 MaterialID = 0);

	/** Flat rectangle in the XY plane. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Rectangle"))
	static FModelingResult AppendRectangle(int32 Handle, FTransform Transform, float DimensionX = 100.f, float DimensionY = 100.f, int32 StepsWidth = 0,
		int32 StepsHeight = 0, int32 MaterialID = 0);

	/** Flat disc / ring / arc in the XY plane. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Disc"))
	static FModelingResult AppendDisc(int32 Handle, FTransform Transform, float Radius = 50.f, int32 AngleSteps = 16, int32 SpokeSteps = 0,
		float StartAngle = 0.f, float EndAngle = 360.f, float HoleRadius = 0.f, int32 MaterialID = 0);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Stairs"))
	static FModelingResult AppendStairs(int32 Handle, FTransform Transform, float StepWidth = 100.f, float StepHeight = 20.f, float StepDepth = 30.f,
		int32 NumSteps = 8, bool bFloating = false, int32 MaterialID = 0);

	/** Extrude a closed 2D polygon (XY, counter-clockwise) along +Z. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Extrude Polygon"))
	static FModelingResult AppendExtrudePolygon(int32 Handle, FTransform Transform, const TArray<FVector2D>& PolygonPoints, float Height = 100.f,
		int32 HeightSteps = 0, bool bCapped = true, const FString& Origin = TEXT("Base"), int32 MaterialID = 0);

	/** Revolve a 2D profile (X = radius offset, Y = height) around the Z axis: lathe shapes, rings, vases. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Revolve Polygon"))
	static FModelingResult AppendRevolvePolygon(int32 Handle, FTransform Transform, const TArray<FVector2D>& ProfilePoints, float Radius = 100.f,
		int32 Steps = 16, float RevolveDegrees = 360.f, int32 MaterialID = 0);

	/** Append another session mesh (transformed) into this one — no boolean, just merged geometry. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Mesh"))
	static FModelingResult AppendMesh(int32 Handle, int32 OtherHandle, FTransform Transform);

	// =====================================================================
	// Booleans
	// =====================================================================

	/** Operation: "Union", "Subtract", "Intersection". ToolTransform places the tool mesh relative to the target. Both should be closed. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Boolean"))
	static FModelingResult Boolean(int32 TargetHandle, int32 ToolHandle, const FString& Operation, FTransform ToolTransform,
		bool bFillHoles = true, bool bSimplifyOutput = true);

	/** Union a mesh with itself — merges overlapping kitbash pieces into one shell. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Self Union"))
	static FModelingResult SelfUnion(int32 Handle, bool bFillHoles = true, bool bTrimFlaps = true);

	/** Cut away everything on the +Z side of CutFrame (its Z axis is the plane normal). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Plane Cut"))
	static FModelingResult PlaneCut(int32 Handle, FTransform CutFrame, bool bFillHoles = true, bool bFlipCutSide = false);

	/** Mirror across MirrorFrame's XY plane (optionally cutting the source side first and welding the seam). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Mirror"))
	static FModelingResult Mirror(int32 Handle, FTransform MirrorFrame, bool bApplyPlaneCut = true, bool bWeldAlongPlane = true, bool bFlipCutSide = false);

	// =====================================================================
	// Selections (named, stored per handle; "" means the whole mesh)
	// =====================================================================

	/** SelectionType: "Triangles" (default), "Vertices", "Edges", "Polygroups". Returns the element count. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Select All"))
	static int32 SelectAll(int32 Handle, const FString& SelectionName, const FString& SelectionType = TEXT("Triangles"));

	/** Triangles whose normal is within MaxAngleDeg of Normal (e.g. (0,0,1) + 5 = the top faces). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Select By Normal Angle"))
	static int32 SelectByNormalAngle(int32 Handle, const FString& SelectionName, FVector Normal, float MaxAngleDeg = 1.f, bool bInvert = false);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Select In Box"))
	static int32 SelectInBox(int32 Handle, const FString& SelectionName, FVector BoxMin, FVector BoxMax, bool bInvert = false);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Select In Sphere"))
	static int32 SelectInSphere(int32 Handle, const FString& SelectionName, FVector Center, float Radius = 100.f, bool bInvert = false);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Select By Material ID"))
	static int32 SelectByMaterialID(int32 Handle, const FString& SelectionName, int32 MaterialID);

	/** Grow (or shrink with bContract) a selection by N rings of neighbours; stores it under NewSelectionName. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Expand Contract Selection"))
	static int32 ExpandContractSelection(int32 Handle, const FString& SelectionName, const FString& NewSelectionName, int32 Iterations = 1, bool bContract = false);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Invert Selection"))
	static int32 InvertSelection(int32 Handle, const FString& SelectionName, const FString& NewSelectionName);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Selection Count"))
	static int32 SelectionCount(int32 Handle, const FString& SelectionName);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Clear Selections"))
	static bool ClearSelections(int32 Handle);

	// =====================================================================
	// Poly-edit operations (Modeling Mode's PolyEdit / Offset / Bevel)
	// =====================================================================

	/** Extrude selected faces. Zero Direction = along each face's average normal; otherwise a fixed world direction. Negative distance recesses. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Extrude Faces"))
	static FModelingResult ExtrudeFaces(int32 Handle, const FString& SelectionName, float Distance = 10.f, FVector Direction = FVector::ZeroVector);

	/** Offset selected faces along their normals (parallel face offset). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Offset Faces"))
	static FModelingResult OffsetFaces(int32 Handle, const FString& SelectionName, float Distance = 10.f);

	/** Inset selected faces (panel lines, frames). Softness rounds the inset boundary. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Inset Faces"))
	static FModelingResult InsetFaces(int32 Handle, const FString& SelectionName, float Distance = 5.f, float Softness = 0.f);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Outset Faces"))
	static FModelingResult OutsetFaces(int32 Handle, const FString& SelectionName, float Distance = 5.f, float Softness = 0.f);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Delete Faces"))
	static FModelingResult DeleteFaces(int32 Handle, const FString& SelectionName);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Translate Selection"))
	static FModelingResult TranslateSelection(int32 Handle, const FString& SelectionName, FVector Delta);

	/** Bevel the edges between polygroups (run ComputePolygroups first on imported meshes). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Bevel Polygroups"))
	static FModelingResult BevelPolygroups(int32 Handle, float Distance = 1.f, int32 Subdivisions = 0, float RoundWeight = 1.f);

	/** Offset the whole surface along its normals (inflate / deflate). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Offset Mesh"))
	static FModelingResult OffsetMesh(int32 Handle, float Distance = 1.f);

	/** Turn an open surface into a solid shell of the given thickness. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Shell Mesh"))
	static FModelingResult ShellMesh(int32 Handle, float Thickness = 1.f);

	// =====================================================================
	// Mesh processing
	// =====================================================================

	/** Uniform remesh to a triangle count, or to an edge length when EdgeLength > 0. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Remesh"))
	static FModelingResult Remesh(int32 Handle, int32 TargetTriangleCount = 5000, float EdgeLength = 0.f, bool bReprojectToInput = true);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Simplify To Triangle Count"))
	static FModelingResult SimplifyToTriangleCount(int32 Handle, int32 TriangleCount, bool bAllowSeamCollapse = true);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Simplify To Tolerance"))
	static FModelingResult SimplifyToTolerance(int32 Handle, float Tolerance = 0.5f, bool bAllowSeamCollapse = true);

	/** Method: "PN" (curved, smooth normals), "Uniform" (flat split), "CatmullClark" (polygroup SubD), "Loop" (triangle SubD). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Subdivide"))
	static FModelingResult Subdivide(int32 Handle, int32 Level = 1, const FString& Method = TEXT("PN"));

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Smooth"))
	static FModelingResult Smooth(int32 Handle, const FString& SelectionName, int32 Iterations = 5, float Alpha = 0.2f);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Fill Holes"))
	static FModelingResult FillHoles(int32 Handle);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Weld Edges"))
	static FModelingResult WeldEdges(int32 Handle, float Tolerance = 0.001f);

	/** Repair degenerate triangles, drop tiny floating pieces, compact — the standard cleanup after imports and booleans. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Repair"))
	static FModelingResult Repair(int32 Handle, float MinComponentVolume = 0.0001f, int32 MinComponentTriangles = 1);

	/** Remove triangles that cannot be seen from outside (kitbash interiors). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Remove Hidden Triangles"))
	static FModelingResult RemoveHiddenTriangles(int32 Handle);

	/** Split into connected components, each becoming a new session handle. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Split By Components"))
	static FModelingSplitResult SplitByComponents(int32 Handle);

	// =====================================================================
	// Deformation
	// =====================================================================

	/** Bend around the frame's Y axis by Angle degrees within Extent of its origin. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Bend"))
	static FModelingResult Bend(int32 Handle, FTransform Orientation, float AngleDeg = 45.f, float Extent = 50.f, bool bBidirectional = true);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Twist"))
	static FModelingResult Twist(int32 Handle, FTransform Orientation, float AngleDeg = 45.f, float Extent = 50.f, bool bBidirectional = true);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Flare"))
	static FModelingResult Flare(int32 Handle, FTransform Orientation, float PercentX = 20.f, float PercentY = 20.f, float Extent = 50.f);

	/** Perlin displacement along normals (rock, damage, organic wobble). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Noise"))
	static FModelingResult Noise(int32 Handle, const FString& SelectionName, float Magnitude = 5.f, float Frequency = 0.25f, int32 RandomSeed = 0);

	/** Displace along normals by a texture sampled through a UV layer (height maps). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Displace From Texture"))
	static FModelingResult DisplaceFromTexture(int32 Handle, const FString& SelectionName, const FString& TexturePath, float Magnitude = 10.f, int32 UVLayer = 0);

	// =====================================================================
	// Voxel operations
	// =====================================================================

	/** Rebuild as a watertight voxel-derived surface (fixes any topology; loses UVs). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Voxel Solidify"))
	static FModelingResult VoxelSolidify(int32 Handle, int32 GridResolution = 64, float WindingThreshold = 0.5f);

	/** Operation: "Dilate", "Contract", "Open", "Close" by Distance (Close welds nearby pieces, Open removes thin bits). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Voxel Morphology"))
	static FModelingResult VoxelMorphology(int32 Handle, const FString& Operation, float Distance = 5.f, int32 GridResolution = 64);

	// =====================================================================
	// UVs, normals, groups, materials, colors
	// =====================================================================

	/** Method: "XAtlas" (general unwrap) or "PatchBuilder" (hard-surface, respects groups). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Auto UV"))
	static FModelingResult AutoUV(int32 Handle, const FString& Method = TEXT("XAtlas"), int32 UVLayer = 0);

	/** Method: "Planar", "Box", "Cylinder"; the transform positions/scales the projector. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Project UV"))
	static FModelingResult ProjectUV(int32 Handle, const FString& Method, FTransform ProjectorTransform, int32 UVLayer = 0, const FString& SelectionName = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Repack UV"))
	static FModelingResult RepackUV(int32 Handle, int32 UVLayer = 0, int32 TextureResolution = 1024);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Set Num UV Layers"))
	static FModelingResult SetNumUVLayers(int32 Handle, int32 NumLayers);

	/** HardAngleDeg < 0 = fully smooth; otherwise edges sharper than the angle get split normals. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Recompute Normals"))
	static FModelingResult RecomputeNormals(int32 Handle, float HardAngleDeg = 30.f);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Flip Normals"))
	static FModelingResult FlipNormals(int32 Handle);

	/** Assign polygroups. Method: "Angle" (crease angle), "UVIslands", "Components", "Polygons" (detect quads/planar polygons). Needed by BevelPolygroups / PatchBuilder / SelectByPolygroup on imported meshes. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Compute Polygroups"))
	static FModelingResult ComputePolygroups(int32 Handle, const FString& Method = TEXT("Angle"), float CreaseAngleDeg = 15.f, int32 MinGroupSize = 2);

	/** Move every triangle with FromMaterialID to ToMaterialID. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Remap Material ID"))
	static FModelingResult RemapMaterialID(int32 Handle, int32 FromMaterialID, int32 ToMaterialID);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Set Material ID"))
	static FModelingResult SetMaterialID(int32 Handle, const FString& SelectionName, int32 MaterialID);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Set Vertex Color"))
	static FModelingResult SetVertexColor(int32 Handle, const FString& SelectionName, FLinearColor Color);

	// =====================================================================
	// Transform
	// =====================================================================

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Transform Mesh"))
	static FModelingResult TransformMesh(int32 Handle, FTransform Transform);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Translate Mesh"))
	static FModelingResult TranslateMesh(int32 Handle, FVector Translation);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Rotate Mesh"))
	static FModelingResult RotateMesh(int32 Handle, FRotator Rotation, FVector Origin = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Scale Mesh"))
	static FModelingResult ScaleMesh(int32 Handle, FVector Scale, FVector Origin = FVector::ZeroVector);

	/** Mode: "Bounds" (bounds center to origin) or "Base" (XY center to origin, bottom to Z=0). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Recenter Mesh"))
	static FModelingResult RecenterMesh(int32 Handle, const FString& Mode = TEXT("Base"));

	// =====================================================================
	// Assets, collision, LODs, baking, placement
	// =====================================================================

	/** Write the mesh to a StaticMesh asset (created, or LOD0 replaced when it exists and bReplaceExisting). Saves the package. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Save Mesh To Static Mesh"))
	static FModelingResult SaveMeshToStaticMesh(int32 Handle, const FString& AssetPath, bool bReplaceExisting = true, bool bEnableCollision = true,
		bool bEnableNanite = false, bool bSaveAsset = true);

	/**
	 * Write the mesh (with its bone weights) to a SkeletalMesh asset. SkeletonPath "" creates a new Skeleton asset at
	 * <AssetPath>_Skeleton from the bones on the mesh (CreateBones); an existing skeleton is bound as-is. One material slot
	 * is made per material ID — assign materials afterwards with SetAssetMaterials.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Save Mesh To Skeletal Mesh"))
	static FModelingResult SaveMeshToSkeletalMesh(int32 Handle, const FString& AssetPath, const FString& SkeletonPath = TEXT(""), bool bReplaceExisting = true, bool bSaveAsset = true);

	/** Assign material slots on a StaticMesh or SkeletalMesh asset: comma-separated material asset paths, in slot order. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Set Asset Materials"))
	static FModelingResult SetAssetMaterials(const FString& AssetPath, const FString& MaterialPaths, bool bSaveAsset = true);

	/** Copy skin weights from a SkeletalMesh asset onto this mesh by closest surface point (re-skin a modified body). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Transfer Bone Weights"))
	static FModelingResult TransferBoneWeights(int32 Handle, const FString& SourceSkeletalMeshPath, int32 SourceLODIndex = 0);

	/** Method: "MinVolumeShapes", "ConvexHulls", "AlignedBoxes", "OrientedBoxes", "MinimalSpheres", "Capsules", "SweptHulls". Acts on the asset. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Generate Collision"))
	static FModelingResult GenerateCollision(const FString& AssetPath, const FString& Method = TEXT("MinVolumeShapes"), int32 MaxConvexHulls = 1,
		int32 ConvexHullTargetFaceCount = 25, bool bSaveAsset = true);

	/** Rebuild the asset's LOD chain: one entry per LOD as a fraction of LOD0 triangles (LOD0 first, e.g. [1.0, 0.5, 0.25]). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Set LODs", ScriptName = "set_lods;set_lo_ds"))
	static FModelingResult SetLODs(const FString& AssetPath, const TArray<float>& PercentTrianglesPerLOD, bool bAutoComputeScreenSize = true, bool bSaveAsset = true);

	/** Bake maps from SourceHandle onto TargetHandle's UVs. BakeTypes: comma list of "TangentNormal", "ObjectNormal", "AmbientOcclusion", "Curvature", "Position", "BentNormal". Resolution 16..8192 (power of two). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Bake Textures"))
	static FModelingBakeResult BakeTextures(int32 TargetHandle, int32 SourceHandle, const FString& BakeTypes, int32 Resolution, const FString& OutputFolder,
		const FString& BaseName, int32 TargetUVLayer = 0, int32 SamplesPerPixel = 1);

	/** Place a StaticMesh asset in the current level as a StaticMeshActor. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Spawn Static Mesh Actor"))
	static FModelingResult SpawnStaticMeshActor(const FString& AssetPath, FTransform Transform, const FString& ActorLabel = TEXT(""));

	// =====================================================================
	// More primitives
	// =====================================================================

	/** Spiral / curved staircase around the +Z axis. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Curved Stairs"))
	static FModelingResult AppendCurvedStairs(int32 Handle, FTransform Transform, float StepWidth = 100.f, float StepHeight = 20.f, float InnerRadius = 150.f,
		float CurveAngle = 90.f, int32 NumSteps = 8, bool bFloating = false, int32 MaterialID = 0);

	/** Sphere built from a subdivided cube (even quads — better for booleans and UVs than a lat-long sphere). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Sphere Box"))
	static FModelingResult AppendSphereBox(int32 Handle, FTransform Transform, float Radius = 50.f, int32 Steps = 6, const FString& Origin = TEXT("Center"), int32 MaterialID = 0);

	/** Sweep an OPEN 2D polyline along a path of transforms: rails, trim, ribbons, gutters. The profile lies in each frame's local YZ plane and is scaled by the frame's scale; bLoop closes the PATH (last frame back to the first), not the profile. For closed, capped solids use AppendLoft. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Sweep Polyline"))
	static FModelingResult AppendSweepPolyline(int32 Handle, FTransform Transform, const TArray<FVector2D>& ProfilePoints, const TArray<FTransform>& SweepPath,
		bool bLoop = false, float StartScale = 1.f, float EndScale = 1.f, int32 MaterialID = 0);

	// =====================================================================
	// More simplification
	// =====================================================================

	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Simplify To Vertex Count"))
	static FModelingResult SimplifyToVertexCount(int32 Handle, int32 VertexCount, bool bAllowSeamCollapse = true);

	/** Collapse coplanar triangles only — lossless cleanup for boolean / voxel output. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Simplify Planar"))
	static FModelingResult SimplifyPlanar(int32 Handle, float AngleThresholdDeg = 0.001f);

	// =====================================================================
	// Spatial queries and sampling
	// =====================================================================

	/** First hit of a ray against the mesh (MaxDistance 0 = unlimited). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Ray Cast"))
	static FModelingSurfacePoint RayCast(int32 Handle, FVector Origin, FVector Direction, float MaxDistance = 0.f);

	/** Closest point on the surface to a query point (MaxDistance 0 = unlimited). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Nearest Point"))
	static FModelingSurfacePoint NearestPoint(int32 Handle, FVector Point, float MaxDistance = 0.f);

	/** Winding-number containment test — the mesh should be closed. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Is Point Inside"))
	static bool IsPointInside(int32 Handle, FVector Point);

	/** Poisson-disc samples on the surface (position + surface-aligned rotation), for scattering props/foliage/decals. MaxSamples 0 = no cap. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Sample Surface Points"))
	static TArray<FTransform> SampleSurfacePoints(int32 Handle, float SamplingRadius = 10.f, int32 MaxSamples = 0);

	/** Bounding box of a selection. Returns false when the selection is empty. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Selection Bounds"))
	static bool SelectionBounds(int32 Handle, const FString& SelectionName, FVector& OutMin, FVector& OutMax);

	/** Select every triangle in one polygroup (see ComputePolygroups / GetMeshInfo). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Select By Polygroup"))
	static int32 SelectByPolygroup(int32 Handle, const FString& SelectionName, int32 PolygroupID);

	// =====================================================================
	// Hulls and comparison
	// =====================================================================

	/** Convex hull of the mesh as a NEW handle (SimplifyToFaceCount 0 = exact). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Convex Hull"))
	static FModelingResult ConvexHull(int32 Handle, int32 SimplifyToFaceCount = 0);

	/** Approximate the mesh with NumHulls convex pieces, returned merged as a NEW handle (collision proxies, blockout simplification). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Convex Decomposition"))
	static FModelingResult ConvexDecomposition(int32 Handle, int32 NumHulls = 2);

	/** Hull swept along ProjectionFrame's Z axis (2D outline extruded through the mesh) as a NEW handle. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Swept Hull"))
	static FModelingResult SweptHull(int32 Handle, FTransform ProjectionFrame);

	/** Surface distances between two meshes — verify a LOD / simplified copy against its source. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Measure Distance"))
	static FModelingDistanceReport MeasureDistance(int32 HandleA, int32 HandleB);

	// =====================================================================
	// More skeletal
	// =====================================================================

	/** Recompute smooth skin weights against a skeleton after transfer or edits. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Smooth Bone Weights"))
	static FModelingResult SmoothBoneWeights(int32 Handle, const FString& SkeletonPath, float Stiffness = 0.2f, int32 MaxInfluences = 5);

	/** Remove bones (comma-separated names) from the skin weights, renormalizing the rest. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Prune Bone Weights"))
	static FModelingResult PruneBoneWeights(int32 Handle, const FString& BoneNames);

	// =====================================================================
	// Lofts, orientation, connected pieces
	// =====================================================================

	/**
	 * Solid made by sweeping a closed 2D profile through a list of frames and capping both ends — wings, fins, hulls,
	 * tapered beams. The profile lies in each frame's local YZ plane (profile X along the frame's Y axis, profile Y along
	 * its Z axis) and is multiplied by the frame's scale, so chord and taper go in Frames[i].Scale. A wing running out
	 * along +Y with its chord toward -X: root and tip frames with rotation (roll 0, pitch 0, yaw 90). The result is always
	 * closed and outward-facing regardless of the winding the sweep produced.
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Append Loft"))
	static FModelingResult AppendLoft(int32 Handle, FTransform Transform, const TArray<FVector2D>& ProfilePoints, const TArray<FTransform>& Frames, int32 MaterialID = 0);

	/** Flip the mesh when its enclosed volume is negative (normals facing inward). Run on any closed part before booleans — an inside-out part is silently discarded by SelfUnion. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Ensure Outward"))
	static FModelingResult EnsureOutward(int32 Handle);

	/** Select the whole connected piece of geometry nearest to Point — a control surface isolated by slot cuts, a bolt on a plate. Returns the triangle count. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Select Connected"))
	static int32 SelectConnected(int32 Handle, const FString& SelectionName, FVector Point);

	// =====================================================================
	// Rigging: bones and skin weights authored on the mesh itself
	// =====================================================================

	/** Replace the mesh's bone hierarchy. The first bone is the root; every other bone names a parent listed before it. Every vertex starts bound to the root. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Create Bones"))
	static FModelingResult CreateBones(int32 Handle, const TArray<FModelingBoneDef>& Bones);

	/** Bind the vertices of a selection ("" = all) to a bone. Weight 1 replaces their weights; a lower weight blends with what is there. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "Bind Selection To Bone"))
	static FModelingResult BindSelectionToBone(int32 Handle, const FString& SelectionName, const FString& BoneName, float Weight = 1.f);

	/** Bones on the mesh with their reference poses and influenced-vertex counts — check a rig before saving it. */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Modeling", meta = (AICallable, DisplayName = "List Bones"))
	static TArray<FModelingBoneInfo> ListBones(int32 Handle);
};
