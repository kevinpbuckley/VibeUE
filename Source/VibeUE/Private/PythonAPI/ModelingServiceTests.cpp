// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UModelingService.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "GameFramework/Actor.h"
#include "Subsystems/EditorActorSubsystem.h"

// Self-provisioning: every test builds its own geometry through the service, writes only under
// kModelingTestDir, and releases / deletes what it made. No project content is required.

static const EAutomationTestFlags kModelingTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
static const TCHAR* kModelingTestDir = TEXT("/Game/Developers/VibeUEModelingTests");

namespace
{
	struct FScopedModelingSession
	{
		~FScopedModelingSession() { UModelingService::ReleaseAllMeshes(); }
	};

	int32 MakeBox(float Size = 100.f)
	{
		const int32 Handle = UModelingService::CreateMesh().Handle;
		UModelingService::AppendBox(Handle, FTransform::Identity, Size, Size, Size);
		return Handle;
	}

	int32 MakeSphere(float Radius = 50.f)
	{
		const int32 Handle = UModelingService::CreateMesh().Handle;
		UModelingService::AppendSphere(Handle, FTransform::Identity, Radius, 12, 18);
		return Handle;
	}
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeModelingSessionTest, "VibeUE.Modeling.Session", kModelingTestFlags)
bool FVibeModelingSessionTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const FModelingResult Created = UModelingService::CreateMesh();
	TestTrue(TEXT("create succeeds"), Created.bSuccess);
	TestTrue(TEXT("handle is positive"), Created.Handle > 0);

	const FModelingResult Box = UModelingService::AppendBox(Created.Handle, FTransform::Identity, 100.f, 100.f, 100.f);
	TestEqual(TEXT("box has 12 triangles"), Box.TriangleCount, 12);

	const FModelingMeshInfo Info = UModelingService::GetMeshInfo(Created.Handle);
	TestTrue(TEXT("info succeeds"), Info.bSuccess);
	TestEqual(TEXT("info triangle count"), Info.TriangleCount, 12);
	TestEqual(TEXT("info vertex count"), Info.VertexCount, 8);
	TestTrue(TEXT("box is closed"), Info.bIsClosed);
	TestEqual(TEXT("box bounds min z is 0 (Base origin)"), Info.BoundsMin.Z, 0.0);
	TestTrue(TEXT("bounds max z ~100"), FMath::IsNearlyEqual(Info.BoundsMax.Z, 100.0, 0.01));

	const FModelingResult Copy = UModelingService::CopyMesh(Created.Handle);
	TestTrue(TEXT("copy succeeds"), Copy.bSuccess && Copy.Handle != Created.Handle);
	TestEqual(TEXT("copy has same triangles"), Copy.TriangleCount, 12);
	TestEqual(TEXT("two meshes listed"), UModelingService::ListMeshes().Num(), 2);
	TestTrue(TEXT("dynamic mesh accessible"), UModelingService::GetDynamicMesh(Created.Handle) != nullptr);
	TestTrue(TEXT("release copy"), UModelingService::ReleaseMesh(Copy.Handle));
	TestFalse(TEXT("release twice fails"), UModelingService::ReleaseMesh(Copy.Handle));
	TestEqual(TEXT("release all count"), UModelingService::ReleaseAllMeshes(), 1);
	TestEqual(TEXT("nothing listed"), UModelingService::ListMeshes().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeModelingPrimitivesTest, "VibeUE.Modeling.Primitives", kModelingTestFlags)
bool FVibeModelingPrimitivesTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const int32 H = UModelingService::CreateMesh().Handle;
	int32 Last = 0;
	auto Grows = [&](const FModelingResult& R, const TCHAR* What)
	{
		TestTrue(FString::Printf(TEXT("%s succeeds: %s"), What, *R.Message), R.bSuccess);
		TestTrue(FString::Printf(TEXT("%s adds triangles"), What), R.TriangleCount > Last);
		Last = R.TriangleCount;
	};
	Grows(UModelingService::AppendBox(H, FTransform::Identity), TEXT("box"));
	Grows(UModelingService::AppendSphere(H, FTransform(FVector(120, 0, 0))), TEXT("sphere"));
	Grows(UModelingService::AppendSphereBox(H, FTransform(FVector(240, 0, 0))), TEXT("sphere box"));
	Grows(UModelingService::AppendCylinder(H, FTransform(FVector(360, 0, 0))), TEXT("cylinder"));
	Grows(UModelingService::AppendCone(H, FTransform(FVector(480, 0, 0))), TEXT("cone"));
	Grows(UModelingService::AppendCapsule(H, FTransform(FVector(600, 0, 0))), TEXT("capsule"));
	Grows(UModelingService::AppendTorus(H, FTransform(FVector(720, 0, 0))), TEXT("torus"));
	Grows(UModelingService::AppendRectangle(H, FTransform(FVector(0, 120, 0))), TEXT("rectangle"));
	Grows(UModelingService::AppendDisc(H, FTransform(FVector(120, 120, 0)), 50.f, 16, 0, 0.f, 270.f, 10.f), TEXT("disc"));
	Grows(UModelingService::AppendStairs(H, FTransform(FVector(240, 120, 0))), TEXT("stairs"));
	Grows(UModelingService::AppendCurvedStairs(H, FTransform(FVector(0, 300, 0))), TEXT("curved stairs"));
	const TArray<FVector2D> L = { FVector2D(0, 0), FVector2D(40, 0), FVector2D(40, 20), FVector2D(0, 30) };
	Grows(UModelingService::AppendExtrudePolygon(H, FTransform(FVector(480, 120, 0)), L, 15.f), TEXT("extrude polygon"));
	const TArray<FVector2D> Profile = { FVector2D(0, 0), FVector2D(20, 0), FVector2D(25, 40), FVector2D(0, 60) };
	Grows(UModelingService::AppendRevolvePolygon(H, FTransform(FVector(0, 240, 0)), Profile, 0.f, 16, 360.f), TEXT("revolve"));
	const TArray<FVector2D> V = { FVector2D(-4, 0), FVector2D(0, 4), FVector2D(4, 0) };
	const TArray<FTransform> Path = { FTransform(FVector(0, 0, 0)), FTransform(FVector(0, 0, 50)), FTransform(FVector(30, 0, 100)) };
	Grows(UModelingService::AppendSweepPolyline(H, FTransform(FVector(120, 240, 0)), V, Path), TEXT("sweep"));

	const int32 Other = MakeBox(20.f);
	Grows(UModelingService::AppendMesh(H, Other, FTransform(FVector(0, 0, 300))), TEXT("append mesh"));
	TestFalse(TEXT("unknown origin rejected"), UModelingService::AppendBox(H, FTransform::Identity, 1, 1, 1, 0, 0, 0, TEXT("Sideways")).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeModelingBooleanTest, "VibeUE.Modeling.Booleans", kModelingTestFlags)
bool FVibeModelingBooleanTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const int32 A = MakeBox();
	const int32 B = UModelingService::CreateMesh().Handle;
	UModelingService::AppendSphere(B, FTransform(FVector(50, 50, 50)), 40.f, 10, 16);

	const FModelingResult Sub = UModelingService::Boolean(A, B, TEXT("Subtract"), FTransform::Identity);
	TestTrue(TEXT("subtract succeeds"), Sub.bSuccess);
	TestTrue(TEXT("subtract still closed"), UModelingService::GetMeshInfo(A).bIsClosed);
	TestTrue(TEXT("subtract changed topology"), Sub.TriangleCount > 12);

	const int32 C = MakeBox();
	const int32 D = UModelingService::CreateMesh().Handle;
	UModelingService::AppendBox(D, FTransform(FVector(50, 0, 0)), 100.f, 100.f, 100.f);
	TestTrue(TEXT("union"), UModelingService::Boolean(C, D, TEXT("Union"), FTransform::Identity).bSuccess);
	TestTrue(TEXT("intersection"), UModelingService::Boolean(MakeBox(), D, TEXT("Intersection"), FTransform::Identity).bSuccess);
	TestTrue(TEXT("self union"), UModelingService::SelfUnion(C).bSuccess);
	TestTrue(TEXT("plane cut"), UModelingService::PlaneCut(C, FTransform(FVector(0, 0, 80)), true).bSuccess);
	TestEqual(TEXT("plane cut keeps mesh closed"), UModelingService::GetMeshInfo(C).OpenBorderEdges, 0);
	const FModelingMeshInfo Before = UModelingService::GetMeshInfo(C);
	TestTrue(TEXT("mirror"), UModelingService::Mirror(C, FTransform(FRotator(0, 0, 90)), true, true).bSuccess);
	const FModelingMeshInfo After = UModelingService::GetMeshInfo(C);
	TestTrue(TEXT("mirror produced geometry"), After.TriangleCount > 0 && Before.TriangleCount > 0);
	TestFalse(TEXT("unknown op rejected"), UModelingService::Boolean(A, B, TEXT("Frobnicate"), FTransform::Identity).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeModelingSelectionEditTest, "VibeUE.Modeling.SelectionsPolyEdit", kModelingTestFlags)
bool FVibeModelingSelectionEditTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const int32 H = MakeBox();
	TestEqual(TEXT("select all"), UModelingService::SelectAll(H, TEXT("all")), 12);
	TestEqual(TEXT("select top by normal"), UModelingService::SelectByNormalAngle(H, TEXT("top"), FVector::UpVector, 5.f), 2);
	TestTrue(TEXT("select in box"), UModelingService::SelectInBox(H, TEXT("lower"), FVector(-60, -60, -1), FVector(60, 60, 30)) > 0);
	TestTrue(TEXT("select in sphere"), UModelingService::SelectInSphere(H, TEXT("corner"), FVector(50, 50, 100), 150.f) > 0);
	TestEqual(TEXT("selection count"), UModelingService::SelectionCount(H, TEXT("top")), 2);
	FVector Min, Max;
	TestTrue(TEXT("selection bounds"), UModelingService::SelectionBounds(H, TEXT("top"), Min, Max));
	TestTrue(TEXT("top bounds at z=100"), FMath::IsNearlyEqual(Max.Z, 100.0, 0.01) && FMath::IsNearlyEqual(Min.Z, 100.0, 0.01));
	TestTrue(TEXT("expand"), UModelingService::ExpandContractSelection(H, TEXT("top"), TEXT("grown"), 1, false) > 2);
	TestEqual(TEXT("invert"), UModelingService::InvertSelection(H, TEXT("top"), TEXT("not_top")), 10);
	TestTrue(TEXT("polygroups by angle"), UModelingService::ComputePolygroups(H, TEXT("Angle"), 15.f).bSuccess);
	TestTrue(TEXT("select by polygroup"), UModelingService::SelectByPolygroup(H, TEXT("g1"), 1) >= 0);

	TestTrue(TEXT("inset"), UModelingService::InsetFaces(H, TEXT("top"), 6.f).bSuccess);
	UModelingService::SelectByNormalAngle(H, TEXT("top"), FVector::UpVector, 5.f);
	const FModelingResult Extruded = UModelingService::ExtrudeFaces(H, TEXT("top"), -1.5f);
	TestTrue(TEXT("extrude"), Extruded.bSuccess);
	UModelingService::SelectByNormalAngle(H, TEXT("top"), FVector::UpVector, 5.f);
	TestTrue(TEXT("outset"), UModelingService::OutsetFaces(H, TEXT("top"), 2.f).bSuccess);
	TestTrue(TEXT("side select"), UModelingService::SelectByNormalAngle(H, TEXT("side"), FVector(1, 0, 0), 5.f) > 0);
	TestTrue(TEXT("offset faces"), UModelingService::OffsetFaces(H, TEXT("side"), 2.f).bSuccess);
	TestFalse(TEXT("stale side selection refused after offset"), UModelingService::TranslateSelection(H, TEXT("side"), FVector(1, 0, 0)).bSuccess);
	UModelingService::SelectByNormalAngle(H, TEXT("side"), FVector(1, 0, 0), 5.f);
	TestTrue(TEXT("translate selection"), UModelingService::TranslateSelection(H, TEXT("side"), FVector(1, 0, 0)).bSuccess);
	TestTrue(TEXT("set material id"), UModelingService::SetMaterialID(H, TEXT("side"), 3).bSuccess);
	TestTrue(TEXT("select by material id"), UModelingService::SelectByMaterialID(H, TEXT("mat3"), 3) > 0);
	TestTrue(TEXT("remap material id"), UModelingService::RemapMaterialID(H, 3, 1).bSuccess);
	TestEqual(TEXT("material 3 gone"), UModelingService::SelectByMaterialID(H, TEXT("mat3b"), 3), 0);
	TestTrue(TEXT("vertex color on selection"), UModelingService::SetVertexColor(H, TEXT("side"), FLinearColor::Green).bSuccess);
	TestTrue(TEXT("constant vertex color"), UModelingService::SetVertexColor(H, TEXT(""), FLinearColor::White).bSuccess);
	TestTrue(TEXT("bevel"), (UModelingService::ComputePolygroups(H, TEXT("Angle"), 15.f), UModelingService::BevelPolygroups(H, 0.5f).bSuccess));
	TestTrue(TEXT("bottom select"), UModelingService::SelectByNormalAngle(H, TEXT("bottom"), FVector::DownVector, 5.f) > 0);
	TestTrue(TEXT("delete faces"), UModelingService::DeleteFaces(H, TEXT("bottom")).bSuccess);
	TestTrue(TEXT("hole after delete"), UModelingService::GetMeshInfo(H).OpenBorderEdges > 0);
	TestTrue(TEXT("fill holes"), UModelingService::FillHoles(H).bSuccess);
	TestEqual(TEXT("closed after fill"), UModelingService::GetMeshInfo(H).OpenBorderEdges, 0);
	TestTrue(TEXT("clear selections"), UModelingService::ClearSelections(H));
	TestFalse(TEXT("missing selection rejected"), UModelingService::InsetFaces(H, TEXT("nope"), 1.f).bSuccess);
	UModelingService::SelectByNormalAngle(H, TEXT("stale"), FVector(1, 0, 0), 5.f);
	UModelingService::Subdivide(H, 1, TEXT("Uniform"));
	TestFalse(TEXT("stale selection rejected after topology change"), UModelingService::TranslateSelection(H, TEXT("stale"), FVector(1, 0, 0)).bSuccess);

	const int32 Flat = UModelingService::CreateMesh().Handle;
	UModelingService::AppendRectangle(Flat, FTransform::Identity, 50.f, 50.f);
	TestTrue(TEXT("shell"), UModelingService::ShellMesh(Flat, 2.f).bSuccess);
	TestTrue(TEXT("shell is closed"), UModelingService::GetMeshInfo(Flat).bIsClosed);
	TestTrue(TEXT("offset mesh"), UModelingService::OffsetMesh(Flat, 1.f).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeModelingMeshOpsTest, "VibeUE.Modeling.MeshOps", kModelingTestFlags)
bool FVibeModelingMeshOpsTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const int32 M = MakeSphere(40.f);
	TestTrue(TEXT("noise"), UModelingService::Noise(M, TEXT(""), 2.f, 0.1f, 3).bSuccess);
	const FModelingResult Remeshed = UModelingService::Remesh(M, 1500);
	TestTrue(TEXT("remesh"), Remeshed.bSuccess && Remeshed.TriangleCount > 1000);
	const FModelingResult Simp = UModelingService::SimplifyToTriangleCount(M, 600);
	TestTrue(TEXT("simplify to tris"), Simp.bSuccess && Simp.TriangleCount <= 620);
	TestTrue(TEXT("simplify to verts"), UModelingService::SimplifyToVertexCount(M, 250).bSuccess);
	TestTrue(TEXT("simplify to tolerance"), UModelingService::SimplifyToTolerance(M, 0.5f).bSuccess);
	for (const TCHAR* Method : { TEXT("PN"), TEXT("Uniform"), TEXT("Loop") })
	{
		const int32 Before = UModelingService::GetMeshInfo(M).TriangleCount;
		const FModelingResult R = UModelingService::Subdivide(M, 1, Method);
		TestTrue(FString::Printf(TEXT("subdivide %s"), Method), R.bSuccess && R.TriangleCount > Before);
		UModelingService::SimplifyToTriangleCount(M, 600);
	}
	const int32 Q = MakeBox(50.f);
	UModelingService::ComputePolygroups(Q, TEXT("Polygons"));
	const FModelingResult CC = UModelingService::Subdivide(Q, 1, TEXT("CatmullClark"));
	TestTrue(TEXT("subdivide CatmullClark"), CC.bSuccess && CC.TriangleCount > 12);
	TestTrue(TEXT("planar simplify"), UModelingService::SimplifyPlanar(MakeBox(), 0.01f).bSuccess);
	TestFalse(TEXT("unknown subdivide rejected"), UModelingService::Subdivide(M, 1, TEXT("Sierpinski")).bSuccess);
	TestTrue(TEXT("smooth"), UModelingService::Smooth(M, TEXT(""), 2, 0.2f).bSuccess);
	TestTrue(TEXT("weld"), UModelingService::WeldEdges(M, 0.001f).bSuccess);
	TestTrue(TEXT("repair"), UModelingService::Repair(M).bSuccess);
	TestTrue(TEXT("remove hidden"), UModelingService::RemoveHiddenTriangles(M).bSuccess);

	const int32 Two = MakeBox();
	UModelingService::AppendBox(Two, FTransform(FVector(500, 0, 0)));
	TestEqual(TEXT("two components"), UModelingService::GetMeshInfo(Two).ConnectedComponents, 2);
	const FModelingSplitResult Split = UModelingService::SplitByComponents(Two);
	TestTrue(TEXT("split succeeds"), Split.bSuccess);
	TestEqual(TEXT("split into two handles"), Split.Handles.Num(), 2);
	TestTrue(TEXT("polygroups UV islands"), UModelingService::ComputePolygroups(M, TEXT("UVIslands")).bSuccess);
	TestTrue(TEXT("polygroups components"), UModelingService::ComputePolygroups(M, TEXT("Components")).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeModelingDeformVoxelTest, "VibeUE.Modeling.DeformVoxel", kModelingTestFlags)
bool FVibeModelingDeformVoxelTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const int32 V = UModelingService::CreateMesh().Handle;
	UModelingService::AppendCylinder(V, FTransform::Identity, 20.f, 100.f, 12, 8);
	const FTransform Mid(FVector(0, 0, 50));
	TestTrue(TEXT("bend"), UModelingService::Bend(V, Mid, 30.f, 50.f).bSuccess);
	TestTrue(TEXT("twist"), UModelingService::Twist(V, Mid, 30.f, 50.f).bSuccess);
	TestTrue(TEXT("flare"), UModelingService::Flare(V, Mid, 20.f, 20.f, 50.f).bSuccess);
	const FModelingResult Solid = UModelingService::VoxelSolidify(V, 32);
	TestTrue(TEXT("voxel solidify"), Solid.bSuccess && Solid.TriangleCount > 0);
	TestTrue(TEXT("solidified is closed"), UModelingService::GetMeshInfo(V).bIsClosed);
	TestTrue(TEXT("voxel close"), UModelingService::VoxelMorphology(V, TEXT("Close"), 2.f, 32).bSuccess);
	TestFalse(TEXT("unknown morphology rejected"), UModelingService::VoxelMorphology(V, TEXT("Wobble"), 1.f, 32).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeModelingAttributesTest, "VibeUE.Modeling.AttributesTransforms", kModelingTestFlags)
bool FVibeModelingAttributesTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const int32 U = MakeBox(60.f);
	TestTrue(TEXT("set uv layers"), UModelingService::SetNumUVLayers(U, 2).bSuccess);
	TestEqual(TEXT("two uv layers"), UModelingService::GetMeshInfo(U).NumUVLayers, 2);
	TestTrue(TEXT("auto uv xatlas"), UModelingService::AutoUV(U, TEXT("XAtlas"), 0).bSuccess);
	TestTrue(TEXT("auto uv patch builder"), UModelingService::AutoUV(U, TEXT("PatchBuilder"), 1).bSuccess);
	TestFalse(TEXT("unknown uv method rejected"), UModelingService::AutoUV(U, TEXT("Magic"), 0).bSuccess);
	for (const TCHAR* Method : { TEXT("Planar"), TEXT("Box"), TEXT("Cylinder") })
	{
		TestTrue(FString::Printf(TEXT("project uv %s"), Method), UModelingService::ProjectUV(U, Method, FTransform::Identity, 0).bSuccess);
	}
	TestTrue(TEXT("repack"), UModelingService::RepackUV(U, 0, 512).bSuccess);
	TestTrue(TEXT("normals hard"), UModelingService::RecomputeNormals(U, 30.f).bSuccess);
	TestTrue(TEXT("normals smooth"), UModelingService::RecomputeNormals(U, -1.f).bSuccess);
	TestTrue(TEXT("flip normals"), UModelingService::FlipNormals(U).bSuccess && UModelingService::FlipNormals(U).bSuccess);

	const FModelingMeshInfo Before = UModelingService::GetMeshInfo(U);
	TestTrue(TEXT("transform"), UModelingService::TransformMesh(U, FTransform(FVector(10, 0, 0))).bSuccess);
	TestTrue(TEXT("translate"), UModelingService::TranslateMesh(U, FVector(-10, 0, 0)).bSuccess);
	TestTrue(TEXT("rotate"), UModelingService::RotateMesh(U, FRotator(0, 45, 0)).bSuccess);
	TestTrue(TEXT("scale"), UModelingService::ScaleMesh(U, FVector(1, 1, 2)).bSuccess);
	TestTrue(TEXT("scaled height doubled"), FMath::IsNearlyEqual(UModelingService::GetMeshInfo(U).BoundsMax.Z, Before.BoundsMax.Z * 2.0, 0.1));
	TestTrue(TEXT("recenter bounds"), UModelingService::RecenterMesh(U, TEXT("Bounds")).bSuccess);
	TestTrue(TEXT("centered"), FMath::IsNearlyZero(UModelingService::GetMeshInfo(U).BoundsMin.Z + UModelingService::GetMeshInfo(U).BoundsMax.Z, 0.1));
	TestTrue(TEXT("recenter base"), UModelingService::RecenterMesh(U, TEXT("Base")).bSuccess);
	TestTrue(TEXT("on the floor"), FMath::IsNearlyZero(UModelingService::GetMeshInfo(U).BoundsMin.Z, 0.01));
	TestFalse(TEXT("unknown recenter rejected"), UModelingService::RecenterMesh(U, TEXT("Sideways")).bSuccess);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeModelingQueriesTest, "VibeUE.Modeling.QueriesHulls", kModelingTestFlags)
bool FVibeModelingQueriesTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const int32 S = MakeSphere(50.f);
	const FModelingSurfacePoint Hit = UModelingService::RayCast(S, FVector(-200, 0, 0), FVector(1, 0, 0));
	TestTrue(TEXT("ray hits"), Hit.bFound);
	TestTrue(TEXT("ray hit near x=-50"), FMath::IsNearlyEqual(Hit.Position.X, -50.0, 2.0));
	TestFalse(TEXT("ray miss"), UModelingService::RayCast(S, FVector(-200, 500, 0), FVector(1, 0, 0)).bFound);
	const FModelingSurfacePoint Near = UModelingService::NearestPoint(S, FVector(0, 0, 120));
	TestTrue(TEXT("nearest found"), Near.bFound && FMath::IsNearlyEqual(Near.Position.Z, 50.0, 2.0));
	TestTrue(TEXT("origin inside"), UModelingService::IsPointInside(S, FVector::ZeroVector));
	TestFalse(TEXT("far point outside"), UModelingService::IsPointInside(S, FVector(0, 0, 500)));
	TestTrue(TEXT("samples"), UModelingService::SampleSurfacePoints(S, 15.f).Num() > 20);

	const FModelingResult Hull = UModelingService::ConvexHull(S);
	TestTrue(TEXT("convex hull"), Hull.bSuccess && Hull.TriangleCount > 0 && Hull.Handle != S);
	const FModelingDistanceReport Report = UModelingService::MeasureDistance(S, Hull.Handle);
	TestTrue(TEXT("distance measured"), Report.bSuccess);
	TestTrue(TEXT("hull hugs sphere"), Report.MaxDistance < 5.f);
	const int32 Boxes = MakeBox();
	UModelingService::AppendBox(Boxes, FTransform(FVector(150, 0, 0)), 50.f, 50.f, 50.f);
	const FModelingResult Decomp = UModelingService::ConvexDecomposition(Boxes, 2);
	TestTrue(TEXT("convex decomposition"), Decomp.bSuccess && Decomp.TriangleCount > 0);
	const FModelingResult Swept = UModelingService::SweptHull(S, FTransform::Identity);
	TestTrue(TEXT("swept hull"), Swept.bSuccess && Swept.TriangleCount > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeModelingAssetsTest, "VibeUE.Modeling.Assets", kModelingTestFlags)
bool FVibeModelingAssetsTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const FString AssetPath = FString(kModelingTestDir) / TEXT("SM_ModelingTest");
	if (UEditorAssetLibrary::DoesDirectoryExist(kModelingTestDir))
	{
		UEditorAssetLibrary::DeleteDirectory(kModelingTestDir);
	}

	const int32 H = MakeBox();
	UModelingService::AutoUV(H, TEXT("XAtlas"), 0);
	const FModelingResult Saved = UModelingService::SaveMeshToStaticMesh(H, AssetPath, true, true, false, false);
	TestTrue(FString::Printf(TEXT("save creates asset: %s"), *Saved.Message), Saved.bSuccess);
	TestTrue(TEXT("asset exists"), UEditorAssetLibrary::DoesAssetExist(AssetPath));
	TestFalse(TEXT("replace refused when disabled"), UModelingService::SaveMeshToStaticMesh(H, AssetPath, false, true, false, false).bSuccess);
	TestTrue(TEXT("replace LOD0"), UModelingService::SaveMeshToStaticMesh(H, AssetPath, true, true, false, false).bSuccess);

	const FModelingResult Loaded = UModelingService::LoadMeshFromStaticMesh(AssetPath, 0);
	TestTrue(TEXT("load back"), Loaded.bSuccess && Loaded.TriangleCount == 12);
	TestTrue(TEXT("collision"), UModelingService::GenerateCollision(AssetPath, TEXT("ConvexHulls"), 2, 25, false).bSuccess);
	TestFalse(TEXT("unknown collision method rejected"), UModelingService::GenerateCollision(AssetPath, TEXT("Blobs"), 1, 25, false).bSuccess);
	const FModelingResult LODs = UModelingService::SetLODs(AssetPath, { 1.f, 0.5f }, true, false);
	TestTrue(FString::Printf(TEXT("lods: %s"), *LODs.Message), LODs.bSuccess);

	const FModelingResult Spawned = UModelingService::SpawnStaticMeshActor(AssetPath, FTransform(FVector(0, 0, -5000)), TEXT("VibeUEModelingTestActor"));
	TestTrue(TEXT("spawn"), Spawned.bSuccess);
	const FModelingResult FromActor = UModelingService::LoadMeshFromActor(TEXT("VibeUEModelingTestActor"), true, 0);
	TestTrue(TEXT("load from actor"), FromActor.bSuccess && FromActor.TriangleCount == 12);
	TestTrue(TEXT("world space applied"), UModelingService::GetMeshInfo(FromActor.Handle).BoundsMin.Z < -4000.0);

	const int32 Source = MakeSphere(60.f);
	const FModelingBakeResult Bake = UModelingService::BakeTextures(H, Source, TEXT("TangentNormal,AmbientOcclusion"), 64, kModelingTestDir, TEXT("ModelingTest"));
	TestTrue(FString::Printf(TEXT("bake: %s"), *Bake.Message), Bake.bSuccess);
	TestEqual(TEXT("two textures"), Bake.TexturePaths.Num(), 2);
	TestFalse(TEXT("unknown bake type rejected"), UModelingService::BakeTextures(H, Source, TEXT("Sparkle"), 64, kModelingTestDir, TEXT("X")).bSuccess);

	// Cleanup: actor, then the whole test folder.
	if (UEditorActorSubsystem* Actors = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr)
	{
		for (AActor* Actor : Actors->GetAllLevelActors())
		{
			if (Actor && Actor->GetActorLabel() == TEXT("VibeUEModelingTestActor"))
			{
				Actors->DestroyActor(Actor);
			}
		}
	}
	UModelingService::ReleaseAllMeshes();
	TestTrue(TEXT("test folder removed"), UEditorAssetLibrary::DeleteDirectory(kModelingTestDir));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeModelingErrorsTest, "VibeUE.Modeling.Errors", kModelingTestFlags)
bool FVibeModelingErrorsTest::RunTest(const FString&)
{
	FScopedModelingSession Session;
	const FModelingResult NoHandle = UModelingService::AppendBox(999, FTransform::Identity);
	TestFalse(TEXT("unknown handle fails"), NoHandle.bSuccess);
	TestTrue(TEXT("unknown handle explains"), NoHandle.Message.Contains(TEXT("create_mesh")));
	const int32 H = MakeBox();
	const FModelingResult BadOp = UModelingService::Boolean(H, H, TEXT("Frobnicate"), FTransform::Identity);
	TestTrue(TEXT("bad enum lists values"), !BadOp.bSuccess && BadOp.Message.Contains(TEXT("Union")));
	TestFalse(TEXT("empty mesh cannot be saved"), UModelingService::SaveMeshToStaticMesh(UModelingService::CreateMesh().Handle, TEXT("/Game/Developers/Nope"), true, true, false, false).bSuccess);
	TestFalse(TEXT("relative asset path rejected"), UModelingService::SaveMeshToStaticMesh(H, TEXT("Nope"), true, true, false, false).bSuccess);
	TestFalse(TEXT("missing texture rejected"), UModelingService::DisplaceFromTexture(H, TEXT(""), TEXT("/Game/Developers/DoesNotExist"), 1.f).bSuccess);
	TestFalse(TEXT("missing skeleton rejected"), UModelingService::SmoothBoneWeights(H, TEXT("/Game/Developers/NoSkeleton")).bSuccess);
	TestFalse(TEXT("empty bone list rejected"), UModelingService::PruneBoneWeights(H, TEXT(" , ")).bSuccess);
	TestEqual(TEXT("selection on unknown handle"), UModelingService::SelectAll(4242, TEXT("x")), -1);
	TestFalse(TEXT("ray cast on unknown handle"), UModelingService::RayCast(4242, FVector::ZeroVector, FVector::UpVector).bFound);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
