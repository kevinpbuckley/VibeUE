// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "PythonAPI/ULandscapeService.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeEdit.h"
#include "LandscapeEditLayer.h"
#include "LandscapeInfo.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplinesComponent.h"
#include "Misc/ScopeExit.h"

namespace VibeLandscapeSplineTests
{
	static ALandscape* FindLandscape(const FString& Label)
	{
		for (TActorIterator<ALandscape> It(GEditor->GetEditorWorldContext().World()); It; ++It)
		{
			if (It->GetActorLabel() == Label)
			{
				return *It;
			}
		}
		return nullptr;
	}

	static TArray<uint16> ReadHeights(ALandscape* Landscape, const FGuid& Layer)
	{
		FScopedSetLandscapeEditingLayer Scope(Landscape, Layer);
		FHeightmapAccessor<false> Accessor(Landscape->GetLandscapeInfo());
		TArray<uint16> Heights;
		Heights.SetNumZeroed(64 * 64);
		Accessor.GetDataFast(0, 0, 63, 63, Heights.GetData());
		return Heights;
	}
}

// Check actual component-space geometry, not only a service create/read round trip:
// the old getter used the same wrong transform and concealed misplaced points (#640).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeLandscapeSplineCoordinatesTest,
	"VibeUE.LandscapeService.Splines.ComponentCoordinates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeLandscapeSplineCoordinatesTest::RunTest(const FString&)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("editor world"), World)) return false;
	ALandscape* Landscape = World->SpawnActor<ALandscape>();
	if (!TestNotNull(TEXT("test landscape"), Landscape)) return false;
	ON_SCOPE_EXIT { World->DestroyActor(Landscape); };
	Landscape->SetActorLabel(FString::Printf(TEXT("SplineCoordinates_%s"), *FGuid::NewGuid().ToString()));
	const FString Label = Landscape->GetActorLabel();
	Landscape->SetActorLocation(FVector(-151200, -151200, -870));
	Landscape->SetActorScale3D(FVector(100));
	const FVector Requested(19860.77, 8802.70, -2.04);
	const FSplineCreateResult Created = ULandscapeService::CreateSplinePoint(Label, Requested, 1160);
	if (!TestTrue(TEXT("create point"), Created.bSuccess)) return false;
	ULandscapeSplinesComponent* Splines = Landscape->GetSplinesComponent();
	ULandscapeSplineControlPoint* Point = Splines->GetControlPoints()[Created.PointIndex];
	TestTrue(TEXT("reported repro has correct component-local offset"),
		Point->Location.Equals(FVector(171060.77, 160002.70, 867.96), 0.01));
	TestTrue(TEXT("actual point transforms to requested world position"),
		Splines->GetComponentTransform().TransformPosition(Point->Location).Equals(Requested, 0.01));
	TestEqual(TEXT("width stays in component units"), Point->Width, 1160.0f);

	// A relative translation/rotation/scale catches substitutions that merely divide
	// by actor scale rather than using the full component transform.
	Splines->SetRelativeTransform(FTransform(FRotator(0, 25, 0), FVector(17, -31, 5), FVector(0.02, 0.03, 0.01)));
	const FVector Moved(21860, 9300, 400);
	TestTrue(TEXT("modify point"), ULandscapeService::ModifySplinePoint(Label, Created.PointIndex, Moved));
	TestFalse(TEXT("isolated point retains a finite rotation"), Point->Rotation.ContainsNaN());
	TestTrue(TEXT("modified geometry is at requested world position"),
		Splines->GetComponentTransform().TransformPosition(Point->Location).Equals(Moved, 0.01));
	// Seed a location independently of the service to test the getter in isolation.
	Point->Location = FVector(500, 600, 700);
	const FLandscapeSplineInfo Info = ULandscapeService::GetSplineInfo(Label);
	if (!TestEqual(TEXT("one control point"), Info.ControlPoints.Num(), 1)) return false;
	TestTrue(TEXT("getter reports actual component geometry"), Info.ControlPoints[0].Location.Equals(
		Splines->GetComponentTransform().TransformPosition(Point->Location), 0.01));
	return true;
}

// Requires a full editor with rendering (not -NullRHI): verifies raw per-layer
// heightmaps and the merged result, including stale deformation after move/delete.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeLandscapeSplineLayersTest,
	"VibeUE.LandscapeService.Splines.PreserveSculptLayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeLandscapeSplineLayersTest::RunTest(const FString&)
{
	using namespace VibeLandscapeSplineTests;
	if (!TestNotNull(TEXT("editor"), GEditor)) return false;
	const FString Label = FString::Printf(TEXT("SplineLayers_%s"), *FGuid::NewGuid().ToString());
	const FLandscapeCreateResult Created = ULandscapeService::CreateLandscape(
		FVector(-3000, -3000, 0), FRotator::ZeroRotator, FVector(100), 1, 63, 1, 1, Label);
	if (!TestTrue(TEXT("create test landscape"), Created.bSuccess)) return false;
	ON_SCOPE_EXIT { ULandscapeService::DeleteLandscape(Label); };
	ALandscape* Landscape = FindLandscape(Label);
	if (!TestNotNull(TEXT("find test landscape"), Landscape)) return false;
	Landscape->ForceLayersFullUpdate();
	const ULandscapeEditLayerBase* Base = Landscape->GetEditLayerConst(0);
	if (!TestNotNull(TEXT("base edit layer"), Base)) return false;
	const FGuid BaseGuid = Base->GetGuid();
	const TArray<uint16> BaseBefore = ReadHeights(Landscape, BaseGuid);
	const TArray<uint16> MergedBefore = ReadHeights(Landscape, FGuid());

	const TArray<FVector> Locations = { FVector(-1800, 0, 600), FVector(1800, 0, 600) };
	TestTrue(TEXT("create terrain spline"), ULandscapeService::CreateSplineFromPoints(
		Label, Locations, 400, 200, 200).bSuccess);
	Landscape->SetEditingLayer(FGuid());
	AddExpectedError(TEXT("No writable spline target"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("no implicit first-layer bake"), ULandscapeService::ApplySplinesToLandscape(Label));
	TestTrue(TEXT("rejected apply preserves base bytes"), ReadHeights(Landscape, BaseGuid) == BaseBefore);

	const int32 SplinesIndex = Landscape->CreateLayer(TEXT("Road deformation"), ULandscapeEditLayerSplines::StaticClass());
	ULandscapeEditLayerBase* SplinesLayer = Landscape->GetEditLayer(SplinesIndex);
	if (!TestNotNull(TEXT("reserved spline layer (type, not name)"), SplinesLayer)) return false;
	TestTrue(TEXT("apply without a current editing layer"), ULandscapeService::ApplySplinesToLandscape(Label));
	TestFalse(TEXT("editing layer remains unset"), Landscape->GetEditingLayer().IsValid());
	TestTrue(TEXT("base heightmap byte-for-byte unchanged"), ReadHeights(Landscape, BaseGuid) == BaseBefore);
	const TArray<uint16> Applied = ReadHeights(Landscape, FGuid());
	TestTrue(TEXT("terrain actually deformed at the road"), Applied[30 * 64 + 30] > MergedBefore[30 * 64 + 30]);

	// Even an explicitly scoped base layer must lose priority to the reserved layer.
	{
		FScopedSetLandscapeEditingLayer Scope(Landscape, BaseGuid);
		TestTrue(TEXT("repeat apply with base active"), ULandscapeService::ApplySplinesToLandscape(Label));
		TestEqual(TEXT("restore caller's editing layer"), Landscape->GetEditingLayer(), BaseGuid);
	}
	TestTrue(TEXT("repeat apply is idempotent"), ReadHeights(Landscape, FGuid()) == Applied);
	TestTrue(TEXT("active base remains unchanged"), ReadHeights(Landscape, BaseGuid) == BaseBefore);
	SplinesLayer->SetVisible(false, false);
	Landscape->ForceLayersFullUpdate();
	TestTrue(TEXT("hiding Splines restores original merged heights"), ReadHeights(Landscape, FGuid()) == MergedBefore);
	SplinesLayer->SetVisible(true, false);

	TestTrue(TEXT("move start"), ULandscapeService::ModifySplinePoint(Label, 0, FVector(-1800, 1800, 600)));
	TestTrue(TEXT("move end"), ULandscapeService::ModifySplinePoint(Label, 1, FVector(1800, 1800, 600)));
	TestTrue(TEXT("rebuild moved spline"), ULandscapeService::ApplySplinesToLandscape(Label));
	const TArray<uint16> Moved = ReadHeights(Landscape, FGuid());
	TestEqual(TEXT("old road deformation removed"), Moved[30 * 64 + 30], MergedBefore[30 * 64 + 30]);
	TestTrue(TEXT("new road position deformed"), Moved[48 * 64 + 30] > MergedBefore[48 * 64 + 30]);
	TestTrue(TEXT("delete splines"), ULandscapeService::DeleteAllSplines(Label));
	TestTrue(TEXT("rebuild empty spline layer"), ULandscapeService::ApplySplinesToLandscape(Label));
	TestTrue(TEXT("deleting splines restores original heights"), ReadHeights(Landscape, FGuid()) == MergedBefore);
	TestTrue(TEXT("base stays unchanged after move/delete"), ReadHeights(Landscape, BaseGuid) == BaseBefore);

	// Legacy explicit baking is still available, but a locked layer is rejected.
	Landscape->DeleteLayer(SplinesIndex);
	ULandscapeEditLayerBase* MutableBase = Landscape->GetEditLayer(BaseGuid);
	FScopedSetLandscapeEditingLayer Scope(Landscape, BaseGuid);
	MutableBase->SetLocked(true, false);
	AddExpectedError(TEXT("No writable spline target"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("reject locked explicit target"), ULandscapeService::ApplySplinesToLandscape(Label));
	MutableBase->SetLocked(false, false);
	TestTrue(TEXT("recreate spline for explicit baking"), ULandscapeService::CreateSplineFromPoints(
		Label, Locations, 400, 200, 200).bSuccess);
	TestTrue(TEXT("explicit unlocked target can bake"), ULandscapeService::ApplySplinesToLandscape(Label));
	TestTrue(TEXT("explicit bake changes the selected layer"), ReadHeights(Landscape, BaseGuid) != BaseBefore);
	return true;
}

#endif
