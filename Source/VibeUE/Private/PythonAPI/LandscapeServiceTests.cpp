// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/ULandscapeService.h"
#include "PythonAPI/LandscapeServiceTestEditLayer.h"
#include "Async/Async.h"
#include "Components/SplineMeshComponent.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "LandscapeEdit.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeEditLayer.h"
#include "LandscapeSplineControlPoint.h"
#include "LandscapeSplineSegment.h"
#include "LandscapeSplinesComponent.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace
{
	struct FSplineFixture
	{
		ALandscape* Landscape;
		ULandscapeSplinesComponent* Component;
		FString Label;

		FSplineFixture(bool bCreateSpline = true)
		{
			Label = TEXT("VibeUESplineTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
			FActorSpawnParameters Params;
			Params.ObjectFlags = RF_Transient | RF_Transactional;
			Landscape = GEditor->GetEditorWorldContext().World()->SpawnActor<ALandscape>(Params);
			Landscape->SetActorLabel(Label);
			Landscape->SetLandscapeGuid(FGuid::NewGuid());
			Component = nullptr;
			if (bCreateSpline)
			{
				Landscape->CreateLandscapeInfo();
				Landscape->CreateSplineComponent();
				Component = Landscape->GetSplinesComponent();
			}
		}

		~FSplineFixture()
		{
			if (ULandscapeInfo* Info = Landscape->GetLandscapeInfo())
			{
				Info->UnregisterActor(Landscape);
			}
			Landscape->Destroy();
		}

		ULandscapeSplineSegment* AddSegment()
		{
			ULandscapeSplineControlPoint* Start = NewObject<ULandscapeSplineControlPoint>(Component, NAME_None, RF_Transactional);
			ULandscapeSplineControlPoint* End = NewObject<ULandscapeSplineControlPoint>(Component, NAME_None, RF_Transactional);
			Start->Location = FVector(0, 0, 500);
			End->Location = FVector(2000, 600, 700);
			Start->Rotation = FRotator(5, 20, 3);
			End->Rotation = FRotator(-4, -15, 9);
			Start->Width = 200;
			End->Width = 400;
			Start->SideFalloff = 300;
			End->SideFalloff = 700;
			Start->EndFalloff = End->EndFalloff = 0;
			Start->SegmentMeshOffset = 2;
			End->SegmentMeshOffset = 6;
			Component->GetControlPoints().Add(Start);
			Component->GetControlPoints().Add(End);
			ULandscapeSplineSegment* Segment = NewObject<ULandscapeSplineSegment>(Component, NAME_None, RF_Transactional);
			Segment->Connections[0].ControlPoint = Start;
			Segment->Connections[0].TangentLen = 1500;
			Segment->Connections[1].ControlPoint = End;
			Segment->Connections[1].TangentLen = -1800;
			Start->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 0));
			End->ConnectedSegments.Add(FLandscapeSplineConnection(Segment, 1));
			Segment->bRaiseTerrain = Segment->bLowerTerrain = false;
			Segment->bPlaceSplineMeshesInStreamingLevels = false;
			FLandscapeSplineMeshEntry Entry;
			Entry.Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			Segment->SplineMeshes.Add(Entry);
			Component->GetSegments().Add(Segment);
			Segment->UpdateSplinePoints();
			return Segment;
		}

		int32 MeshCount() const
		{
			TArray<USplineMeshComponent*> Meshes;
			Landscape->GetComponents(Meshes);
			return Meshes.Num();
		}
	};

	const EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeLandscapeOffThreadTest, "VibeUE.Landscape.ThreadGuard", Flags)
bool FVibeLandscapeOffThreadTest::RunTest(const FString&)
{
	if (!GEditor || GEditor->PlayWorld) { AddError(TEXT("Requires an idle Editor world")); return false; }
	FSplineFixture Fixture;
	Fixture.AddSegment();
	TestTrue(TEXT("game-thread rebuild works"), ULandscapeService::RebuildSplines(Fixture.Label));
	ULandscapeLayerInfoObject* Layer = NewObject<ULandscapeLayerInfoObject>(GetTransientPackage(), NAME_None, RF_Transient);
	Layer->SetLayerName(TEXT("ThreadGuardLayer"), false);
	const FString Label = Fixture.Label;
	const TArray<bool> Rejected = Async(EAsyncExecution::ThreadPool, [Label, Layer]()
	{
		return TArray<bool>{
			!ULandscapeService::RebuildSplines(Label),
			!ULandscapeService::SplitSplineSegment(Label, 0, 0.5f).bSuccess,
			ULandscapeService::EnsureSplineEditLayer(Label) == INDEX_NONE,
			!ULandscapeService::ReplaceLayerInfo(Label, Layer, Layer)};
	}).Get();
	TestTrue(TEXT("worker-thread rebuild rejected"), Rejected[0]);
	TestTrue(TEXT("worker-thread split rejected"), Rejected[1]);
	TestTrue(TEXT("worker-thread layer creation rejected"), Rejected[2]);
	TestTrue(TEXT("worker-thread replacement rejected"), Rejected[3]);
	TestEqual(TEXT("spline unchanged"), Fixture.Component->GetSegments().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeLandscapeHiddenTargetTest, "VibeUE.Landscape.Splines.HiddenTarget", Flags)
bool FVibeLandscapeHiddenTargetTest::RunTest(const FString&)
{
	if (!GEditor || GEditor->PlayWorld) { AddError(TEXT("Requires an idle Editor world")); return false; }
	FSplineFixture Fixture;
	Fixture.AddSegment();
	ULevel* Level = Fixture.Landscape->GetLevel();
	if (!TestNotNull(TEXT("fixture level"), Level)) { return false; }
	const bool bWasVisible = Level->bIsVisible;
	Level->bIsVisible = false;
	const bool bExists = ULandscapeService::LandscapeExists(Fixture.Label);
	const bool bRebuilt = ULandscapeService::RebuildSplines(Fixture.Label);
	const FHeightmapImportResult Import = ULandscapeService::ImportHeightmap(Fixture.Label, TEXT("unused.raw"));
	ULandscapeLayerInfoObject* Layer = NewObject<ULandscapeLayerInfoObject>(GetTransientPackage(), NAME_None, RF_Transient);
	Layer->SetLayerName(TEXT("HiddenTargetTest"), false);
	const FLandscapeLayerInfoReplacementResult Replacement = ULandscapeService::ReplaceLayerInfoWithResult(Fixture.Label, Layer, Layer);
	const FSplineCreateResult Split = ULandscapeService::SplitSplineSegment(Fixture.Label, 0, 0.5f);
	const FLandscapeSplineInfo SplineInfo = ULandscapeService::GetSplineInfo(Fixture.Label);
	const FWeightMapExportResult Export = ULandscapeService::ExportWeightMap(Fixture.Label, TEXT("Unused"), TEXT("unused.png"));
	const FLandscapeNoiseResult Noise = ULandscapeService::ApplyNoise(Fixture.Label, 0, 0, 100, 1);
	Level->bIsVisible = bWasVisible;
	TestTrue(TEXT("hidden target still exists"), bExists);
	TestFalse(TEXT("hidden target cannot be edited"), bRebuilt);
	TestFalse(TEXT("hidden target cannot auto-create on import"), Import.bSuccess);
	TestTrue(TEXT("hidden import explains unavailable target"), Import.ErrorMessage.Contains(TEXT("inactive")));
	TestTrue(TEXT("hidden replacement explains unavailable target"), !Replacement.bSuccess && Replacement.Message.Contains(TEXT("inactive")));
	TestTrue(TEXT("hidden split explains unavailable target"), !Split.bSuccess && Split.ErrorMessage.Contains(TEXT("inactive")));
	TestTrue(TEXT("hidden spline info explains unavailable target"), !SplineInfo.bSuccess && SplineInfo.ErrorMessage.Contains(TEXT("inactive")));
	TestTrue(TEXT("hidden export explains unavailable target"), !Export.bSuccess && Export.ErrorMessage.Contains(TEXT("inactive")));
	TestTrue(TEXT("hidden noise explains unavailable target"), !Noise.bSuccess && Noise.ErrorMessage.Contains(TEXT("inactive")));
	TestTrue(TEXT("visible target still accepts no-op replacement"), ULandscapeService::ReplaceLayerInfoWithResult(Fixture.Label, Layer, Layer).bSuccess);
	TestEqual(TEXT("hidden target spline preserved"), Fixture.Component->GetSegments().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeLandscapeAmbiguousTargetTest, "VibeUE.Landscape.Splines.AmbiguousTarget", Flags)
bool FVibeLandscapeAmbiguousTargetTest::RunTest(const FString&)
{
	if (!GEditor || GEditor->PlayWorld) { AddError(TEXT("Requires an idle Editor world")); return false; }
	FSplineFixture First;
	FSplineFixture Second;
	First.AddSegment();
	Second.AddSegment();
	Second.Landscape->SetActorLabel(First.Label, false);
	TestEqual(TEXT("fixture has duplicate labels"), Second.Landscape->GetActorLabel(), First.Label);
	TestTrue(TEXT("ambiguous label still exists"), ULandscapeService::LandscapeExists(First.Label));
	AddExpectedError(TEXT("already exists"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("ambiguous label cannot create another landscape"), ULandscapeService::CreateLandscape(
		FVector::ZeroVector, FRotator::ZeroRotator, FVector(100.0), 1, 7, 1, 1, First.Label).bSuccess);
	const FHeightmapImportResult Import = ULandscapeService::ImportHeightmap(First.Label, TEXT("unused.raw"));
	TestFalse(TEXT("ambiguous import rejected"), Import.bSuccess);
	TestTrue(TEXT("ambiguous import reports ambiguity"), Import.ErrorMessage.Contains(TEXT("ambiguous")));
	ULandscapeLayerInfoObject* Layer = NewObject<ULandscapeLayerInfoObject>(GetTransientPackage(), NAME_None, RF_Transient);
	Layer->SetLayerName(TEXT("AmbiguousTargetTest"), false);
	const FLandscapeLayerInfoReplacementResult Replacement = ULandscapeService::ReplaceLayerInfoWithResult(First.Label, Layer, Layer);
	const FSplineCreateResult Split = ULandscapeService::SplitSplineSegment(First.Label, 0, 0.5f);
	const FLandscapeSplineInfo SplineInfo = ULandscapeService::GetSplineInfo(First.Label);
	const FWeightMapImportResult WeightImport = ULandscapeService::ImportWeightMap(First.Label, TEXT("Unused"), TEXT("unused.png"));
	const FLandscapeNoiseResult Noise = ULandscapeService::ApplyNoise(First.Label, 0, 0, 100, 1);
	TestTrue(TEXT("ambiguous replacement explains ambiguity"), !Replacement.bSuccess && Replacement.Message.Contains(TEXT("ambiguous")));
	TestTrue(TEXT("ambiguous split explains ambiguity"), !Split.bSuccess && Split.ErrorMessage.Contains(TEXT("ambiguous")));
	TestTrue(TEXT("ambiguous spline info explains ambiguity"), !SplineInfo.bSuccess && SplineInfo.ErrorMessage.Contains(TEXT("ambiguous")));
	TestTrue(TEXT("ambiguous weight import explains ambiguity"), !WeightImport.bSuccess && WeightImport.ErrorMessage.Contains(TEXT("ambiguous")));
	TestTrue(TEXT("ambiguous noise explains ambiguity"), !Noise.bSuccess && Noise.ErrorMessage.Contains(TEXT("ambiguous")));
	TestFalse(TEXT("ambiguous target rejected"), ULandscapeService::DeleteAllSplines(First.Label));
	TestEqual(TEXT("first landscape unchanged"), First.Component->GetSegments().Num(), 1);
	TestEqual(TEXT("second landscape unchanged"), Second.Component->GetSegments().Num(), 1);
	Second.Landscape->SetActorLabel(Second.Label, false);
	TestTrue(TEXT("unique target still accepts no-op replacement"), ULandscapeService::ReplaceLayerInfoWithResult(First.Label, Layer, Layer).bSuccess);
	First.Landscape->SetActorLabel(Second.Landscape->GetName(), false);
	TestFalse(TEXT("name-label collision rejected"), ULandscapeService::DeleteAllSplines(Second.Landscape->GetName()));
	TestEqual(TEXT("name collision preserves first"), First.Component->GetSegments().Num(), 1);
	TestEqual(TEXT("name collision preserves second"), Second.Component->GetSegments().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeLandscapeSplineDeletionTest, "VibeUE.Landscape.Splines.Deletion", Flags)
bool FVibeLandscapeSplineDeletionTest::RunTest(const FString&)
{
	if (!GEditor || GEditor->PlayWorld)
	{
		AddError(TEXT("Requires an idle Editor world"));
		return false;
	}
	FSplineFixture Fixture;
	ULandscapeSplineSegment* Segment = Fixture.AddSegment();
	ULandscapeSplineControlPoint* Start = Segment->Connections[0].ControlPoint;
	ULandscapeSplineControlPoint* End = Segment->Connections[1].ControlPoint;
	TestTrue(TEXT("generated mesh fixture"), Fixture.MeshCount() > 0);
	Fixture.Component->SetFlags(RF_Transactional);
	Start->SetFlags(RF_Transactional);
	const float OriginalWidth = Start->Width;
	{
		FScopedTransaction PreviousEdit(NSLOCTEXT("VibeUELandscapeTests", "PreviousSplineEdit", "Previous Spline Edit"));
		Start->Modify();
		Start->Width = OriginalWidth + 10.0f;
	}
	Fixture.Component->GetControlPoints().Add(nullptr);
	ON_SCOPE_EXIT { Fixture.Component->GetControlPoints().Remove(nullptr); };
	if (!TestTrue(TEXT("delete empty spline point slot"), ULandscapeService::DeleteSplinePoint(Fixture.Label, 2))) { return false; }
	if (!TestEqual(TEXT("empty slot removed without changing other points"), Fixture.Component->GetControlPoints().Num(), 2)) { return false; }
	TestEqual(TEXT("empty slot deletion keeps segment"), Fixture.Component->GetSegments().Num(), 1);
	TestEqual(TEXT("start point identity preserved"), Fixture.Component->GetControlPoints()[0].Get(), Start);
	TestEqual(TEXT("end point identity preserved"), Fixture.Component->GetControlPoints()[1].Get(), End);
	TestTrue(TEXT("Undo reaches prior valid edit, not invalid slot"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo restores prior width"), Start->Width, OriginalWidth);
	TestEqual(TEXT("Undo does not reintroduce null slot"), Fixture.Component->GetControlPoints().Num(), 2);
	TestTrue(TEXT("Redo prior valid edit"), GEditor->RedoTransaction());
	TestEqual(TEXT("Redo restores edited width"), Start->Width, OriginalWidth + 10.0f);
	AddExpectedError(TEXT("ULandscapeService::DeleteSplinePoint: Invalid index -1"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("invalid delete rejected"), ULandscapeService::DeleteSplinePoint(Fixture.Label, -1));
	TestEqual(TEXT("invalid delete preserves points"), Fixture.Component->GetControlPoints().Num(), 2);
	TestTrue(TEXT("delete endpoint"), ULandscapeService::DeleteSplinePoint(Fixture.Label, 0));
	TestEqual(TEXT("one surviving control point"), Fixture.Component->GetControlPoints().Num(), 1);
	TestEqual(TEXT("no segments"), Fixture.Component->GetSegments().Num(), 0);
	TestEqual(TEXT("no orphan generated meshes"), Fixture.MeshCount(), 0);
	TestEqual(TEXT("survivor has no stale connection"), End->ConnectedSegments.Num(), 0);
	TestEqual(TEXT("deleted point detached"), Start->GetOuter(), static_cast<UObject*>(GetTransientPackage()));
	TestEqual(TEXT("deleted segment detached"), Segment->GetOuter(), static_cast<UObject*>(GetTransientPackage()));
	TestTrue(TEXT("Undo delete"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo restores points"), Fixture.Component->GetControlPoints().Num(), 2);
	TestTrue(TEXT("explicit recovery preserves Redo"), ULandscapeService::RebuildSplines(Fixture.Label));
	TestTrue(TEXT("Redo delete"), GEditor->RedoTransaction());
	TestTrue(TEXT("recover Redo"), ULandscapeService::RebuildSplines(Fixture.Label));
	TestEqual(TEXT("Redo leaves no orphan meshes"), Fixture.MeshCount(), 0);
	TestTrue(TEXT("Undo again"), GEditor->UndoTransaction());
	TestTrue(TEXT("recover before clear-all"), ULandscapeService::RebuildSplines(Fixture.Label));
	TestTrue(TEXT("clear all"), ULandscapeService::DeleteAllSplines(Fixture.Label));
	TestEqual(TEXT("all points removed"), Fixture.Component->GetControlPoints().Num(), 0);
	TestEqual(TEXT("all generated meshes removed"), Fixture.MeshCount(), 0);
	TestTrue(TEXT("Undo clear-all"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo clear-all restores points"), Fixture.Component->GetControlPoints().Num(), 2);
	TestTrue(TEXT("recover clear-all Undo"), ULandscapeService::RebuildSplines(Fixture.Label));
	TestTrue(TEXT("Redo clear-all"), GEditor->RedoTransaction());
	TestTrue(TEXT("recover empty layer after Redo"), ULandscapeService::RebuildSplines(Fixture.Label));
	TestEqual(TEXT("Redo clear-all no points"), Fixture.Component->GetControlPoints().Num(), 0);
	TestEqual(TEXT("Redo clear-all no generated meshes"), Fixture.MeshCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeLandscapeSplineSplitTest, "VibeUE.Landscape.Splines.Split", Flags)
bool FVibeLandscapeSplineSplitTest::RunTest(const FString&)
{
	if (!GEditor || GEditor->PlayWorld)
	{
		AddError(TEXT("Requires an idle Editor world"));
		return false;
	}
	FSplineFixture Fixture;
	ULandscapeSplineSegment* Segment = Fixture.AddSegment();
	ULandscapeSplineControlPoint* Start = Segment->Connections[0].ControlPoint;
	ULandscapeSplineControlPoint* End = Segment->Connections[1].ControlPoint;
	Segment->RandomSeed = 17;
	FSplineFixture Unrelated;
	ULandscapeSplineSegment* UnrelatedSegment = Unrelated.AddSegment();
	const FGuid UnrelatedKey = UnrelatedSegment->GetModificationKey();
	TestTrue(TEXT("target-only rebuild"), ULandscapeService::RebuildSplines(Fixture.Label));
	TestEqual(TEXT("other Landscape not rebuilt"), UnrelatedSegment->GetModificationKey(), UnrelatedKey);
	TestFalse(TEXT("missing rebuild target rejected"), ULandscapeService::RebuildSplines(TEXT("NoSuchLandscape_VibeUESplineTest")));
	Segment->LDMaxDrawDistance = 12345;
	Segment->VirtualTextureMainPassMaxDrawDistance = 9000;
	Segment->LayerName = TEXT("RoadTest");
	const FRotator StartRotation = Start->Rotation;
	const FRotator EndRotation = End->Rotation;
	TArray<FVector> Samples;
	for (int32 Index = 0; Index <= 20; ++Index)
	{
		Samples.Add(Segment->Evaluate(Index / 20.0f));
	}
	TestFalse(TEXT("missing target rejected"), ULandscapeService::SplitSplineSegment(TEXT("NoSuchLandscape_VibeUESplineTest"), 0, 0.5f).bSuccess);
	TestFalse(TEXT("missing segment rejected"), ULandscapeService::SplitSplineSegment(Fixture.Label, -1, 0.5f).bSuccess);
	for (float Fraction : { 0.0f, 1.0f, -1.0f, std::numeric_limits<float>::quiet_NaN() })
	{
		TestFalse(TEXT("invalid fraction rejected"), ULandscapeService::SplitSplineSegment(Fixture.Label, 0, Fraction).bSuccess);
	}
	Segment->Connections[1].SocketName = TEXT("UnsupportedSocket");
	TestFalse(TEXT("socket split rejected"), ULandscapeService::SplitSplineSegment(Fixture.Label, 0, 0.4f).bSuccess);
	Segment->Connections[1].SocketName = NAME_None;
	const FVector EndLocation = End->Location;
	const float StartLength = Segment->Connections[0].TangentLen;
	const float EndLength = Segment->Connections[1].TangentLen;
	End->Location = Start->Location;
	Segment->Connections[0].TangentLen = Segment->Connections[1].TangentLen = 0;
	TestFalse(TEXT("degenerate split rejected"), ULandscapeService::SplitSplineSegment(Fixture.Label, 0, 0.4f).bSuccess);
	End->Location = EndLocation;
	Segment->Connections[0].TangentLen = StartLength;
	Segment->Connections[1].TangentLen = EndLength;
	TestEqual(TEXT("rejections leave points unchanged"), Fixture.Component->GetControlPoints().Num(), 2);
	TestEqual(TEXT("rejections leave spans unchanged"), Fixture.Component->GetSegments().Num(), 1);
	const FSplineCreateResult Result = ULandscapeService::SplitSplineSegment(Fixture.Label, 0, 0.4f);
	if (!TestTrue(TEXT("split succeeds"), Result.bSuccess))
	{
		return false;
	}
	TestEqual(TEXT("new point appended"), Result.PointIndex, 2);
	ULandscapeSplineControlPoint* Point = Fixture.Component->GetControlPoints()[2];
	ULandscapeSplineSegment* Second = Fixture.Component->GetSegments()[1];
	TestEqual(TEXT("width interpolated"), Point->Width, 280.0f);
	TestEqual(TEXT("mesh offset interpolated"), Point->SegmentMeshOffset, 3.6f);
	TestEqual(TEXT("mesh seed retained"), Second->RandomSeed, 17);
	TestEqual(TEXT("draw distance retained"), Second->LDMaxDrawDistance, 12345.0f);
	TestEqual(TEXT("virtual texture distance retained"), Second->VirtualTextureMainPassMaxDrawDistance, 9000.0f);
	TestEqual(TEXT("layer retained"), Second->LayerName, FName(TEXT("RoadTest")));
	TestEqual(TEXT("source mesh retained"), Second->SplineMeshes[0].Mesh.Get(), Segment->SplineMeshes[0].Mesh.Get());
	TestEqual(TEXT("new point has two connections"), Point->ConnectedSegments.Num(), 2);
	TestEqual(TEXT("start rotation unchanged"), Start->Rotation, StartRotation);
	TestEqual(TEXT("end rotation unchanged"), End->Rotation, EndRotation);
	for (int32 Index = 0; Index <= 20; ++Index)
	{
		const float Fraction = Index / 20.0f;
		const FVector Actual = Fraction <= 0.4f ? Segment->Evaluate(Fraction / 0.4f) : Second->Evaluate((Fraction - 0.4f) / 0.6f);
		TestTrue(TEXT("curve preserved within 0.01cm"), Actual.Equals(Samples[Index], 0.01));
	}
	TestTrue(TEXT("Undo split"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo split restores spans"), Fixture.Component->GetSegments().Num(), 1);
	TestTrue(TEXT("recover Undo split"), ULandscapeService::RebuildSplines(Fixture.Label));
	TestTrue(TEXT("Redo split"), GEditor->RedoTransaction());
	TestEqual(TEXT("Redo split restores spans"), Fixture.Component->GetSegments().Num(), 2);
	TestTrue(TEXT("recover Redo split"), ULandscapeService::RebuildSplines(Fixture.Label));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeLandscapeSplineLayerTest, "VibeUE.Landscape.Splines.EditLayer", Flags)
bool FVibeLandscapeSplineLayerTest::RunTest(const FString&)
{
	if (!GEditor || GEditor->PlayWorld)
	{
		AddError(TEXT("Requires an idle Editor world"));
		return false;
	}
	FSplineFixture Fixture;
	FSplineFixture Other;
	const int32 OtherCount = Other.Landscape->GetEditLayers().Num();
	const int32 BaseIndex = Fixture.Landscape->CreateLayer(TEXT("Base"));
	const FGuid BaseGuid = Fixture.Landscape->GetEditLayer(BaseIndex)->GetGuid();
	const int32 InitialCount = Fixture.Landscape->GetEditLayers().Num();
	Fixture.Landscape->SetEditingLayer(BaseGuid);
	TestEqual(TEXT("missing target rejected"), ULandscapeService::EnsureSplineEditLayer(TEXT("NoSuchLandscape_VibeUESplineTest"), TEXT("Road")), INDEX_NONE);
	TestEqual(TEXT("name collision rejected"), ULandscapeService::EnsureSplineEditLayer(Fixture.Label, TEXT("Base")), INDEX_NONE);
	TestEqual(TEXT("rejection preserves layers"), Fixture.Landscape->GetEditLayers().Num(), InitialCount);
	const int32 Index = ULandscapeService::EnsureSplineEditLayer(Fixture.Label, TEXT("Road"));
	if (!TestTrue(TEXT("spline layer created"), Index != INDEX_NONE)) { return false; }
	TestTrue(TEXT("native specialized class"), Fixture.Landscape->GetEditLayer(Index)->IsA<ULandscapeEditLayerSplines>());
	TestEqual(TEXT("base layer preserved"), Fixture.Landscape->GetEditLayer(BaseIndex)->GetGuid(), BaseGuid);
	TestEqual(TEXT("active base remains active"), Fixture.Landscape->GetEditingLayer(), BaseGuid);
	const FGuid SplineGuid = Fixture.Landscape->GetEditLayer(Index)->GetGuid();
	TestEqual(TEXT("repeat returns existing layer"), ULandscapeService::EnsureSplineEditLayer(Fixture.Label, TEXT("Road")), Index);
	TestEqual(TEXT("different requested name does not duplicate or rename"), ULandscapeService::EnsureSplineEditLayer(Fixture.Label, TEXT("AnotherName")), Index);
	TestEqual(TEXT("only one specialized layer"), Fixture.Landscape->GetEditLayers().Num(), InitialCount + 1);
	TestEqual(TEXT("other Landscape untouched"), Other.Landscape->GetEditLayers().Num(), OtherCount);
	TestTrue(TEXT("Undo creation"), GEditor->UndoTransaction());
	TestNull(TEXT("Undo removes specialized layer"), Fixture.Landscape->FindEditLayerOfType(ULandscapeEditLayerSplines::StaticClass()));
	TestTrue(TEXT("Redo creation"), GEditor->RedoTransaction());
	TestEqual(TEXT("Redo restores same layer"), Fixture.Landscape->GetEditLayer(Index)->GetGuid(), SplineGuid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeLandscapeDetachedProxyTest, "VibeUE.Landscape.Paint.DetachedProxy", Flags)
bool FVibeLandscapeDetachedProxyTest::RunTest(const FString&)
{
	if (!GEditor || GEditor->PlayWorld) { AddError(TEXT("Requires an idle Editor world")); return false; }
	if (GEditor->GetEditorWorldContext().World()->IsPartitionedWorld()) { AddError(TEXT("Requires a non-World-Partition Editor world")); return false; }
	FSplineFixture Fixture(false);
	UWorld* World = Fixture.Landscape->GetWorld();
	ULandscapeLayerInfoObject* From = NewObject<ULandscapeLayerInfoObject>(GetTransientPackage(), NAME_None, RF_Transient);
	ULandscapeLayerInfoObject* To = NewObject<ULandscapeLayerInfoObject>(GetTransientPackage(), NAME_None, RF_Transient);
	From->SetLayerName(TEXT("RoadSoil"), false);
	To->SetLayerName(TEXT("RoadSoil"), false);
	TArray<uint16> Heights;
	Heights.Init(32768, 64 * 64);
	TMap<FGuid, TArray<uint16>> HeightLayers;
	HeightLayers.Add(FGuid(), Heights);
	FLandscapeImportLayerInfo Paint;
	Paint.LayerName = TEXT("RoadSoil");
	Paint.LayerInfo = From;
	Paint.LayerData.Init(113, 64 * 64);
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> PaintLayers;
	PaintLayers.Add(FGuid(), {Paint});
	const FGuid LandscapeGuid = FGuid::NewGuid();
	Fixture.Landscape->Import(LandscapeGuid, 0, 0, 63, 63, 1, 63, HeightLayers, nullptr, PaintLayers,
		ELandscapeImportAlphamapType::Additive, {});
	const FString ReimportPath(TEXT("VibeUEFixtures/RoadSoil.png"));
	Fixture.Landscape->AddTargetLayer(TEXT("RoadSoil"), FLandscapeTargetLayerSettings(From, ReimportPath), false);
	ULandscapeInfo* Info = Fixture.Landscape->GetLandscapeInfo();
	if (!TestNotNull(TEXT("landscape info"), Info) || !TestEqual(TEXT("parent component"), Fixture.Landscape->LandscapeComponents.Num(), 1)) { return false; }
	FActorSpawnParameters Params;
	Params.ObjectFlags = RF_Transient | RF_Transactional;
	Params.OverrideLevel = Fixture.Landscape->GetLevel();
	Params.bCreateActorPackage = false;
	TStrongObjectPtr<ALandscapeStreamingProxy> Proxy(World->SpawnActor<ALandscapeStreamingProxy>(Params));
	if (!TestNotNull(TEXT("streaming proxy"), Proxy.Get())) { return false; }
	ULevel* Level = Proxy->GetLevel();
	int32 ActorIndex = INDEX_NONE;
	bool bDetached = false;
	bool bInGCList = false;
	ON_SCOPE_EXIT
	{
		if (bDetached)
		{
			Level->Actors[ActorIndex] = Proxy.Get();
			if (bInGCList) { Level->ActorsForGC.AddUnique(Proxy.Get()); }
			World->AddNetworkActor(Proxy.Get());
		}
		if (ULandscapeInfo* ProxyInfo = Proxy->GetLandscapeInfo()) { ProxyInfo->UnregisterActor(Proxy.Get()); }
		Proxy->Destroy();
	};
	Proxy->SynchronizeSharedProperties(Fixture.Landscape);
	Proxy->SetLandscapeActor(Fixture.Landscape);
	Proxy->Import(LandscapeGuid, 63, 0, 126, 63, 1, 63, HeightLayers, nullptr, PaintLayers,
		ELandscapeImportAlphamapType::Additive, {});
	if (!TestEqual(TEXT("proxy component"), Proxy->LandscapeComponents.Num(), 1)) { return false; }
	const auto ExpectTarget = [&](const TCHAR* Stage, ULandscapeLayerInfoObject* Layer)
	{
		bool bMatches = true;
		for (ALandscapeProxy* Actor : {static_cast<ALandscapeProxy*>(Fixture.Landscape), static_cast<ALandscapeProxy*>(Proxy.Get())})
		{
			const FLandscapeTargetLayerSettings* Settings = Actor->GetTargetLayers().Find(TEXT("RoadSoil"));
			bMatches &= TestTrue(FString::Printf(TEXT("%s: %s target and reimport path"), Stage, *Actor->GetName()),
				Settings && Settings->LayerInfoObj == Layer && Settings->ReimportLayerFilePath == ReimportPath);
		}
		return bMatches;
	};
	if (!ExpectTarget(TEXT("fixture"), From)) { return false; }
	ULandscapeComponent* ProxyComponent = Proxy->LandscapeComponents[0];
	if (!TestNotNull(TEXT("proxy component valid"), ProxyComponent) ||
		!TestTrue(TEXT("proxy component registered"), ProxyComponent->IsRegistered()) ||
		!TestEqual(TEXT("proxy component mapped"), Info->XYtoComponentMap.FindRef(ProxyComponent->GetComponentKey()), ProxyComponent)) { return false; }
	if (!TestNotNull(TEXT("parent edit layer"), Fixture.Landscape->GetEditLayer(0))) { return false; }
	const FGuid LayerGuid = Fixture.Landscape->GetEditLayer(0)->GetGuid();
	ULandscapeComponent* ParentComponent = Fixture.Landscape->LandscapeComponents[0];
	if (!TestTrue(TEXT("parent has source paint"), ParentComponent->GetWeightmapLayerAllocations(LayerGuid).ContainsByPredicate([&](const FWeightmapLayerAllocationInfo& A) { return A.LayerInfo == From; })) ||
		!TestTrue(TEXT("proxy has source paint"), ProxyComponent->GetWeightmapLayerAllocations(LayerGuid).ContainsByPredicate([&](const FWeightmapLayerAllocationInfo& A) { return A.LayerInfo == From; }))) { return false; }
	ActorIndex = Level->Actors.Find(Proxy.Get());
	if (!TestTrue(TEXT("proxy in level"), ActorIndex != INDEX_NONE)) { return false; }
	bInGCList = Level->ActorsForGC.Contains(Proxy.Get());
	World->RemoveActor(Proxy.Get(), false);
	bDetached = true;
	bool bListed = false;
	Info->ForEachLandscapeProxy([&](ALandscapeProxy* P) { bListed |= P == Proxy.Get(); return true; });
	if (!TestTrue(TEXT("detached proxy remains in info"), bListed) ||
		!TestFalse(TEXT("detached proxy absent from level"), Level->Actors.Contains(Proxy.Get())) ||
		!TestTrue(TEXT("detached component remains registered"), ProxyComponent->IsRegistered()) ||
		!TestTrue(TEXT("all listed components remain registered"), Info->AreAllComponentsRegistered())) { return false; }
	const bool bReplaced = ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To);
	TestFalse(TEXT("detached proxy rejects replacement"), bReplaced);
	TestEqual(TEXT("parent target unchanged"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).LayerInfoObj.Get(), From);
	TestTrue(TEXT("parent source identity retained"), ParentComponent->GetWeightmapLayerAllocations(LayerGuid).ContainsByPredicate([&](const FWeightmapLayerAllocationInfo& A) { return A.LayerInfo == From; }));
	TestTrue(TEXT("proxy source identity retained"), ProxyComponent->GetWeightmapLayerAllocations(LayerGuid).ContainsByPredicate([&](const FWeightmapLayerAllocationInfo& A) { return A.LayerInfo == From; }));
	Level->Actors[ActorIndex] = Proxy.Get();
	if (bInGCList) { Level->ActorsForGC.AddUnique(Proxy.Get()); }
	World->AddNetworkActor(Proxy.Get());
	bDetached = false;
	if (!TestTrue(TEXT("reattached proxy allows replacement"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To))) { return false; }
	ExpectTarget(TEXT("replacement"), To);
	if (!TestTrue(TEXT("Undo proxy replacement"), GEditor->UndoTransaction())) { return false; }
	ExpectTarget(TEXT("Undo"), From);
	if (!TestTrue(TEXT("Redo proxy replacement"), GEditor->RedoTransaction())) { return false; }
	ExpectTarget(TEXT("Redo"), To);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeLandscapeLayerInfoReplacementTest, "VibeUE.Landscape.Paint.ReplaceLayerInfo", Flags)
bool FVibeLandscapeLayerInfoReplacementTest::RunTest(const FString&)
{
	if (!GEditor || GEditor->PlayWorld) { AddError(TEXT("Requires an idle Editor world")); return false; }
	if (GEditor->GetEditorWorldContext().World()->IsPartitionedWorld()) { AddError(TEXT("Requires a non-World-Partition Editor world")); return false; }
	// Import must create the first edit layer after its components exist.
	FSplineFixture Fixture(false);
	FSplineFixture Other(false);
	const auto MakeLayer = [](FName Name, ELandscapeTargetLayerBlendMethod Blend = ELandscapeTargetLayerBlendMethod::None)
	{
		ULandscapeLayerInfoObject* Layer = NewObject<ULandscapeLayerInfoObject>(GetTransientPackage(), NAME_None, RF_Transient);
		Layer->SetLayerName(Name, false);
		Layer->SetBlendMethod(Blend, false);
		return Layer;
	};
	ULandscapeLayerInfoObject* From = MakeLayer(TEXT("RoadSoil"));
	ULandscapeLayerInfoObject* To = MakeLayer(TEXT("RoadSoil"));
	ULandscapeLayerInfoObject* WrongName = MakeLayer(TEXT("OtherSoil"));
	ULandscapeLayerInfoObject* WrongBlend = MakeLayer(TEXT("RoadSoil"), ELandscapeTargetLayerBlendMethod::FinalWeightBlending);
	ULandscapeLayerInfoObject* Unrelated = MakeLayer(TEXT("UnrelatedSoil"));
	const auto ImportTerrain = [&](ALandscape* Land)
	{
		TArray<uint16> Heights;
		Heights.Init(32768, 64 * 64);
		TMap<FGuid, TArray<uint16>> HeightLayers;
		HeightLayers.Add(FGuid(), MoveTemp(Heights));
		FLandscapeImportLayerInfo Paint;
		Paint.LayerName = TEXT("RoadSoil");
		Paint.LayerInfo = From;
		Paint.LayerData.Init(113, 64 * 64);
		TMap<FGuid, TArray<FLandscapeImportLayerInfo>> PaintLayers;
		PaintLayers.Add(FGuid(), {Paint});
		Land->Import(FGuid::NewGuid(), 0, 0, 63, 63, 1, 63, HeightLayers, nullptr, PaintLayers,
			ELandscapeImportAlphamapType::Additive, {});
		if (Land->LandscapeComponents.Num() != 1 || !Land->GetEditLayer(0)) { return static_cast<ULandscapeComponent*>(nullptr); }
		ULandscapeComponent* Component = Land->LandscapeComponents[0];
		if (!Component || !Component->GetLayerData(Land->GetEditLayer(0)->GetGuid())) { return static_cast<ULandscapeComponent*>(nullptr); }
		Component->SetFlags(RF_Transactional);
		Component->MobileWeightmapLayerAllocations = {FWeightmapLayerAllocationInfo(From)};
		return Component;
	};
	ULandscapeComponent* Component = ImportTerrain(Fixture.Landscape);
	ULandscapeComponent* OtherComponent = ImportTerrain(Other.Landscape);
	if (!TestNotNull(TEXT("target import created a component and edit layer"), Component) ||
		!TestNotNull(TEXT("other import created a component and edit layer"), OtherComponent)) { return false; }
	if (!TestTrue(TEXT("target collision component exists"), Fixture.Landscape->CollisionComponents.Num() > 0 && Fixture.Landscape->CollisionComponents[0])) { return false; }
	const FGuid BaseGuid = Fixture.Landscape->GetEditLayer(0)->GetGuid();
	const FGuid OtherGuid = Other.Landscape->GetEditLayer(0)->GetGuid();
	if (!TestEqual(TEXT("imported paint allocation"), Component->GetWeightmapLayerAllocations(BaseGuid).Num(), 1)) { return false; }
	const int32 RenderedAllocationCount = Component->GetWeightmapLayerAllocations(false).Num();
	for (const FWeightmapLayerAllocationInfo& Allocation : Component->GetWeightmapLayerAllocations(false))
	{
		if (!TestTrue(TEXT("rendered allocation has backing texture"), !Allocation.IsAllocated() || Component->GetWeightmapTextures(false).IsValidIndex(Allocation.WeightmapTextureIndex))) { return false; }
	}
	ULandscapeHeightfieldCollisionComponent* Collision = Fixture.Landscape->CollisionComponents[0];
	Collision->SetFlags(RF_Transactional);
	Collision->ComponentLayerInfos = {From};
	Component->LayerAllowList = {From, Unrelated};
	// Undo must restore a consistent target list, or native fixup rebinds it again.
	const FString ReimportPath(TEXT("VibeUEFixtures/RoadSoil.png"));
	Fixture.Landscape->AddTargetLayer(TEXT("RoadSoil"), FLandscapeTargetLayerSettings(From, ReimportPath), false);
	const FGuid Editing = Fixture.Landscape->GetEditingLayer();
	TArray<uint16> BeforeHeights;
	BeforeHeights.SetNumZeroed(64 * 64);
	TArray<uint8> BeforeWeights;
	BeforeWeights.SetNumZeroed(64 * 64);
	{
		FLandscapeEditDataInterface Edit(Fixture.Landscape->GetLandscapeInfo(), BaseGuid);
		Edit.GetHeightDataFast(0, 0, 63, 63, BeforeHeights.GetData(), 64);
		Edit.GetWeightDataFast(From, 0, 0, 63, 63, BeforeWeights.GetData(), 64);
	}
	TestFalse(TEXT("missing target"), ULandscapeService::ReplaceLayerInfo(TEXT("NoSuchLandscape_VibeUESplineTest"), From, To));
	TestFalse(TEXT("null source"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, nullptr, To));
	TestFalse(TEXT("null destination"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, nullptr));
	TestFalse(TEXT("different name rejected"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, WrongName));
	TestFalse(TEXT("different blend rejected"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, WrongBlend));
	From->SetBlendMethod(ELandscapeTargetLayerBlendMethod::PremultipliedAlphaBlending, false);
	To->SetBlendMethod(ELandscapeTargetLayerBlendMethod::PremultipliedAlphaBlending, false);
	From->SetBlendGroup(TEXT("GroupA"), false);
	To->SetBlendGroup(TEXT("GroupB"), false);
	const bool bReplacedDifferentGroup = ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To);
	From->SetBlendMethod(ELandscapeTargetLayerBlendMethod::None, false);
	To->SetBlendMethod(ELandscapeTargetLayerBlendMethod::None, false);
	From->SetBlendGroup(NAME_None, false);
	To->SetBlendGroup(NAME_None, false);
	if (!TestFalse(TEXT("different blend group rejected before replacement"), bReplacedDifferentGroup)) { return false; }
	TestEqual(TEXT("blend group rejection preserves allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), From);
	UPhysicalMaterial* DifferentPhysics = NewObject<UPhysicalMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
	To->SetPhysicalMaterial(DifferentPhysics, false);
	const bool bReplacedDifferentPhysics = ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To);
	To->SetPhysicalMaterial(nullptr, false);
	if (!TestFalse(TEXT("different physical material rejected before replacement"), bReplacedDifferentPhysics)) { return false; }
	TestEqual(TEXT("physical material rejection preserves allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), From);
	To->SetMinimumCollisionRelevanceWeight(0.75f, false, EPropertyChangeType::ValueSet);
	const bool bReplacedDifferentThreshold = ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To);
	To->SetMinimumCollisionRelevanceWeight(From->GetMinimumCollisionRelevanceWeight(), false, EPropertyChangeType::ValueSet);
	if (!TestFalse(TEXT("different collision threshold rejected"), bReplacedDifferentThreshold)) { return false; }
	TestEqual(TEXT("collision threshold rejection preserves allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), From);
	Collision->ComponentLayerInfos.Add(To);
	const bool bReplacedDuplicateCollision = ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To);
	Collision->ComponentLayerInfos.Pop();
	if (!TestFalse(TEXT("duplicate collision identity rejected"), bReplacedDuplicateCollision)) { return false; }
	TestEqual(TEXT("collision rejection preserves allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), From);
	Component->UnregisterComponent();
	const bool bReplacedUnregistered = ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To);
	Component->RegisterComponent();
	if (!TestFalse(TEXT("unregistered component rejected before replacement"), bReplacedUnregistered)) { return false; }
	TestEqual(TEXT("unregistered rejection preserves allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), From);
	ULandscapeInfo* Info = Fixture.Landscape->GetLandscapeInfo();
	const FIntPoint Key = Component->GetComponentKey();
	if (!TestEqual(TEXT("fixture component mapped"), Info->XYtoComponentMap.FindRef(Key), Component)) { return false; }
	Info->XYtoComponentMap.Remove(Key);
	const bool bReplacedUnmapped = ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To);
	Info->XYtoComponentMap.Add(Key, Component);
	if (!TestFalse(TEXT("unmapped component rejected before replacement"), bReplacedUnmapped)) { return false; }
	TestEqual(TEXT("unmapped rejection preserves allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), From);
	Fixture.Landscape->LandscapeComponents.Remove(Component);
	const bool bReplacedUnlisted = ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To);
	Fixture.Landscape->LandscapeComponents.Add(Component);
	if (!TestFalse(TEXT("mapped but unlisted component rejected"), bReplacedUnlisted)) { return false; }
	TestEqual(TEXT("unlisted rejection preserves allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), From);
	Component->GetWeightmapLayerAllocations(BaseGuid).Add(FWeightmapLayerAllocationInfo(To));
	TestFalse(TEXT("paint merge rejected before mutation"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To));
	Component->GetWeightmapLayerAllocations(BaseGuid).Pop();
	TestEqual(TEXT("rejections preserve allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), From);
	{
		const int32 OriginalLayerCount = Fixture.Landscape->GetEditLayers().Num();
		int32 CustomIndex;
		{
			FScopedAllowAbstractClassAllocation AllowTestClass;
			CustomIndex = Fixture.Landscape->CreateLayer(TEXT("CustomGuard"), UVibeUELandscapeServiceTestEditLayer::StaticClass());
		}
		if (!TestTrue(TEXT("custom layer created"), CustomIndex != INDEX_NONE)) { return false; }
		ULandscapeEditLayerBase* CustomLayer = Fixture.Landscape->GetEditLayer(CustomIndex);
		if (!TestNotNull(TEXT("custom layer exists"), CustomLayer)) { return false; }
		const FGuid CustomGuid = CustomLayer->GetGuid();
		ON_SCOPE_EXIT
		{
			const int32 Index = Fixture.Landscape->GetLayerIndex(CustomGuid);
			if (Index != INDEX_NONE) { TestTrue(TEXT("custom layer cleanup"), Fixture.Landscape->DeleteLayer(Index)); }
		};
		if (!TestEqual(TEXT("custom layer class"), CustomLayer->GetClass(), UVibeUELandscapeServiceTestEditLayer::StaticClass())) { return false; }
		FMapProperty* BlendProperty = FindFProperty<FMapProperty>(ULandscapeEditLayerBase::StaticClass(), TEXT("WeightmapLayerAllocationBlend"));
		if (!TestNotNull(TEXT("custom test blend metadata property"), BlendProperty)) { return false; }
		FObjectPropertyBase* BlendKey = CastField<FObjectPropertyBase>(BlendProperty->KeyProp);
		FBoolProperty* BlendValue = CastField<FBoolProperty>(BlendProperty->ValueProp);
		if (!TestNotNull(TEXT("custom test blend key type"), BlendKey) || !TestNotNull(TEXT("custom test blend value type"), BlendValue)) { return false; }
		FScriptMapHelper BlendMap(BlendProperty, BlendProperty->ContainerPtrToValuePtr<void>(CustomLayer));
		const auto FindBlend = [&](ULandscapeLayerInfoObject* Layer) -> int32
		{
			for (int32 I = 0; I < BlendMap.GetMaxIndex(); ++I)
			{
				if (BlendMap.IsValidIndex(I) && BlendKey->GetObjectPropertyValue(BlendMap.GetKeyPtr(I)) == Layer) { return I; }
			}
			return INDEX_NONE;
		};
		if (!TestEqual(TEXT("custom test source blend initially absent"), FindBlend(From), INDEX_NONE) ||
			!TestEqual(TEXT("custom test destination blend initially absent"), FindBlend(To), INDEX_NONE)) { return false; }
		const int32 SourceBlend = BlendMap.AddDefaultValue_Invalid_NeedsRehash();
		BlendKey->SetObjectPropertyValue(BlendMap.GetKeyPtr(SourceBlend), From);
		BlendValue->SetPropertyValue(BlendMap.GetValuePtr(SourceBlend), true);
		BlendMap.Rehash();
		ON_SCOPE_EXIT
		{
			const int32 FromIndex = FindBlend(From);
			if (FromIndex != INDEX_NONE) { BlendMap.RemoveAt(FromIndex); }
			const int32 ToIndex = FindBlend(To);
			if (ToIndex != INDEX_NONE) { BlendMap.RemoveAt(ToIndex); }
		};
		TArray<uint16> CustomBeforeHeights;
		CustomBeforeHeights.SetNumZeroed(64 * 64);
		TArray<uint8> CustomBeforeWeights;
		CustomBeforeWeights.SetNumZeroed(64 * 64);
		{
			FLandscapeEditDataInterface Edit(Info, BaseGuid);
			Edit.GetHeightDataFast(0, 0, 63, 63, CustomBeforeHeights.GetData(), 64);
			Edit.GetWeightDataFast(From, 0, 0, 63, 63, CustomBeforeWeights.GetData(), 64);
		}
		AddExpectedMessage(TEXT("Custom edit layer"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
		if (!TestFalse(TEXT("custom edit layer rejects replacement"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To))) { return false; }
		TestEqual(TEXT("custom rejection preserves target"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).LayerInfoObj.Get(), From);
		TestEqual(TEXT("custom rejection preserves reimport path"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).ReimportLayerFilePath, ReimportPath);
		TestEqual(TEXT("custom rejection preserves paint allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), From);
		TestEqual(TEXT("custom rejection preserves mobile allocation"), Component->MobileWeightmapLayerAllocations[0].LayerInfo.Get(), From);
		TestEqual(TEXT("custom rejection preserves collision allocation"), Collision->ComponentLayerInfos[0].Get(), From);
		TestTrue(TEXT("custom rejection preserves allowlist"), Component->LayerAllowList.Contains(From) && Component->LayerAllowList.Contains(Unrelated));
		const int32 RemainingBlend = FindBlend(From);
		if (TestTrue(TEXT("custom rejection preserves source blend"), RemainingBlend != INDEX_NONE))
		{
			TestTrue(TEXT("custom rejection preserves blend value"), BlendValue->GetPropertyValue(BlendMap.GetValuePtr(RemainingBlend)));
		}
		TestEqual(TEXT("custom rejection leaves destination blend absent"), FindBlend(To), INDEX_NONE);
		TArray<uint16> CustomAfterHeights;
		CustomAfterHeights.SetNumZeroed(64 * 64);
		TArray<uint8> CustomAfterWeights;
		CustomAfterWeights.SetNumZeroed(64 * 64);
		{
			FLandscapeEditDataInterface Edit(Info, BaseGuid);
			Edit.GetHeightDataFast(0, 0, 63, 63, CustomAfterHeights.GetData(), 64);
			Edit.GetWeightDataFast(From, 0, 0, 63, 63, CustomAfterWeights.GetData(), 64);
		}
		TestTrue(TEXT("custom rejection preserves heights"), CustomBeforeHeights == CustomAfterHeights);
		TestTrue(TEXT("custom rejection preserves weights"), CustomBeforeWeights == CustomAfterWeights);
		const FLandscapeLayerInfoReplacementResult CustomRefused = ULandscapeService::ReplaceLayerInfoWithResult(Fixture.Label, From, To);
		TestFalse(TEXT("custom layer requires opt-in"), CustomRefused.bSuccess);
		TestTrue(TEXT("custom layer warning identifies layer"), CustomRefused.Message.Contains(CustomLayer->GetName().ToString()));
		TestEqual(TEXT("detailed custom refusal preserves target"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).LayerInfoObj.Get(), From);
		ULandscapeLayerInfoObject* Unused = MakeLayer(TEXT("RoadSoil"));
		const FLandscapeLayerInfoReplacementResult NoSource = ULandscapeService::ReplaceLayerInfoWithResult(Fixture.Label, To, Unused);
		TestFalse(TEXT("unallocated source is rejected before requesting approval"), NoSource.bSuccess);
		TestTrue(TEXT("unallocated source reason is actionable"), NoSource.Message.Contains(TEXT("source is not allocated")) && !NoSource.Message.Contains(TEXT("Obtain user approval")));
		const int32 ConflictingBlend = BlendMap.AddDefaultValue_Invalid_NeedsRehash();
		BlendKey->SetObjectPropertyValue(BlendMap.GetKeyPtr(ConflictingBlend), To);
		BlendValue->SetPropertyValue(BlendMap.GetValuePtr(ConflictingBlend), false);
		BlendMap.Rehash();
		const int32 QueueBeforeConflict = GEditor->Trans->GetQueueLength();
		const FLandscapeLayerInfoReplacementResult Conflict = ULandscapeService::ReplaceLayerInfoWithResult(Fixture.Label, From, To, true);
		TestFalse(TEXT("opt-in cannot bypass conflicting custom references"), Conflict.bSuccess);
		TestTrue(TEXT("conflict result explains hard rejection"), Conflict.Message.Contains(TEXT("Ambiguous")) && !Conflict.Message.Contains(TEXT("Obtain user approval")));
		TestEqual(TEXT("conflict opens no transaction"), GEditor->Trans->GetQueueLength(), QueueBeforeConflict);
		const int32 ToConflictIndex = FindBlend(To);
		if (!TestTrue(TEXT("conflicting blend preserved"), ToConflictIndex != INDEX_NONE)) { return false; }
		BlendMap.RemoveAt(ToConflictIndex);
		const FLandscapeLayerInfoReplacementResult CustomAllowed = ULandscapeService::ReplaceLayerInfoWithResult(Fixture.Label, From, To, true);
		if (!TestTrue(TEXT("custom layer permits explicit opt-in"), CustomAllowed.bSuccess)) { return false; }
		TestTrue(TEXT("custom success retains warning"), CustomAllowed.Message.Contains(CustomLayer->GetName().ToString()));
		TestEqual(TEXT("custom opt-in changes target"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).LayerInfoObj.Get(), To);
		TestEqual(TEXT("custom opt-in removes old blend key"), FindBlend(From), INDEX_NONE);
		if (TestTrue(TEXT("custom opt-in moves blend key"), FindBlend(To) != INDEX_NONE)) { TestTrue(TEXT("custom blend value preserved"), BlendValue->GetPropertyValue(BlendMap.GetValuePtr(FindBlend(To)))); }
		if (!TestTrue(TEXT("undo custom opt-in"), GEditor->UndoTransaction())) { return false; }
		TestEqual(TEXT("undo custom opt-in restores target"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).LayerInfoObj.Get(), From);
		TestTrue(TEXT("undo custom opt-in restores blend"), FindBlend(From) != INDEX_NONE && FindBlend(To) == INDEX_NONE);
		if (!TestTrue(TEXT("redo custom opt-in"), GEditor->RedoTransaction())) { return false; }
		const int32 RedoneBlend = FindBlend(To);
		if (!TestTrue(TEXT("redo custom opt-in moves blend"), FindBlend(From) == INDEX_NONE && RedoneBlend != INDEX_NONE)) { return false; }
		BlendMap.RemoveAt(RedoneBlend);
		const int32 StaleBlend = BlendMap.AddDefaultValue_Invalid_NeedsRehash();
		BlendKey->SetObjectPropertyValue(BlendMap.GetKeyPtr(StaleBlend), From);
		BlendValue->SetPropertyValue(BlendMap.GetValuePtr(StaleBlend), true);
		BlendMap.Rehash();
		const FLandscapeLayerInfoReplacementResult Repaired = ULandscapeService::ReplaceLayerInfoWithResult(Fixture.Label, From, To, true);
		TestTrue(TEXT("opt-in repairs stale custom blend"), Repaired.bSuccess);
		TestTrue(TEXT("stale custom blend moves to destination"), FindBlend(From) == INDEX_NONE && FindBlend(To) != INDEX_NONE);
		if (!TestTrue(TEXT("undo custom stale-blend repair"), GEditor->UndoTransaction())) { return false; }
		TestTrue(TEXT("undo repair restores stale custom blend"), FindBlend(From) != INDEX_NONE && FindBlend(To) == INDEX_NONE);
		if (!TestTrue(TEXT("redo custom stale-blend repair"), GEditor->RedoTransaction())) { return false; }
		TestTrue(TEXT("redo repair moves custom blend"), FindBlend(From) == INDEX_NONE && FindBlend(To) != INDEX_NONE);
		if (!TestTrue(TEXT("undo repair before original replacement"), GEditor->UndoTransaction())) { return false; }
		if (!TestTrue(TEXT("undo custom opt-in after redo"), GEditor->UndoTransaction())) { return false; }
		if (!TestTrue(TEXT("custom layer removed"), Fixture.Landscape->DeleteLayer(Fixture.Landscape->GetLayerIndex(CustomGuid)))) { return false; }
		TestEqual(TEXT("original layer count restored"), Fixture.Landscape->GetEditLayers().Num(), OriginalLayerCount);
		TestNull(TEXT("custom component data removed"), Component->GetLayerData(CustomGuid));
	}
	{
		FActorSpawnParameters BrushParams;
		BrushParams.ObjectFlags = RF_Transient | RF_Transactional;
		BrushParams.OverrideLevel = Fixture.Landscape->GetLevel();
		AVibeUELandscapeServiceTestBrush* Brush = GEditor->GetEditorWorldContext().World()->SpawnActor<AVibeUELandscapeServiceTestBrush>(BrushParams);
		if (!TestNotNull(TEXT("custom brush spawned"), Brush)) { return false; }
		ON_SCOPE_EXIT { Fixture.Landscape->RemoveBrush(Brush); Brush->Destroy(); };
		Brush->LayerInfo = From;
		Brush->SetActorTickEnabled(false);
		Fixture.Landscape->AddBrushToLayer(0, Brush);
		if (!TestEqual(TEXT("brush attached to standard edit layer"), Fixture.Landscape->GetBrushLayer(Brush), 0) ||
			!TestTrue(TEXT("brush listed on landscape"), Fixture.Landscape->GetBrushesForLayer(0).Contains(Brush)) ||
			!TestEqual(TEXT("brush owner landscape"), Brush->GetOwningLandscape(), Fixture.Landscape) ||
			!TestEqual(TEXT("standard edit layer class"), Fixture.Landscape->GetEditLayer(0)->GetClass(), ULandscapeEditLayer::StaticClass())) { return false; }
		if (!TestNotNull(TEXT("editor transactor"), GEditor->Trans.Get())) { return false; }
		const int32 QueueBeforeBrushRejection = GEditor->Trans->GetQueueLength();
		const int32 RedoBeforeBrushRejection = GEditor->Trans->GetUndoCount();
		const bool bDirtyBeforeBrushRejection = Fixture.Landscape->GetOutermost()->IsDirty();
		{
			FLandscapeLayer* LayerWithBrush = nullptr;
			Fixture.Landscape->ForEachLayerConst([&](const FLandscapeLayer& Layer) { if (Layer.Brushes.ContainsByPredicate([&](const FLandscapeLayerBrush& Entry) { return Entry.GetBrush() == Brush; })) { LayerWithBrush = const_cast<FLandscapeLayer*>(&Layer); return false; } return true; });
			if (!TestNotNull(TEXT("brush layer found"), LayerWithBrush)) { return false; }
			LayerWithBrush->Brushes.AddDefaulted();
			ON_SCOPE_EXIT { LayerWithBrush->Brushes.Pop(); };
			const FLandscapeLayerInfoReplacementResult Missing = ULandscapeService::ReplaceLayerInfoWithResult(Fixture.Label, From, To, true);
			TestFalse(TEXT("opt-in cannot bypass missing brush"), Missing.bSuccess);
			TestTrue(TEXT("missing brush reason returned"), Missing.Message.Contains(TEXT("unavailable")) && !Missing.Message.Contains(TEXT("Obtain user approval")));
			TestEqual(TEXT("missing brush opens no transaction"), GEditor->Trans->GetQueueLength(), QueueBeforeBrushRejection);
		}
		TestTrue(TEXT("identical layer is a no-op with an attached brush"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, From));
		AddExpectedMessage(TEXT("brush references may remain unchanged"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
		const bool bBrushReplaced = ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To);
		if (!TestFalse(TEXT("attached brush rejects replacement"), bBrushReplaced))
		{
			TestTrue(TEXT("undo unexpected brush replacement"), GEditor->UndoTransaction());
			return false;
		}
		TestEqual(TEXT("brush rejection preserves reference"), Brush->LayerInfo.Get(), From);
		TestEqual(TEXT("brush rejection preserves transaction count"), GEditor->Trans->GetQueueLength(), QueueBeforeBrushRejection);
		TestEqual(TEXT("brush rejection preserves redo count"), GEditor->Trans->GetUndoCount(), RedoBeforeBrushRejection);
		TestEqual(TEXT("brush rejection preserves package dirty state"), Fixture.Landscape->GetOutermost()->IsDirty(), bDirtyBeforeBrushRejection);
		TestEqual(TEXT("brush rejection preserves target"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).LayerInfoObj.Get(), From);
		TestEqual(TEXT("brush rejection preserves reimport path"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).ReimportLayerFilePath, ReimportPath);
		TestEqual(TEXT("brush rejection preserves active layer"), Fixture.Landscape->GetEditingLayer(), Editing);
		TestEqual(TEXT("brush rejection preserves ownership"), Brush->GetOwningLandscape(), Fixture.Landscape);
		TestTrue(TEXT("brush rejection preserves attachment"), Fixture.Landscape->GetBrushesForLayer(0).Contains(Brush));
		TestEqual(TEXT("brush rejection preserves paint allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), From);
		TestEqual(TEXT("brush rejection preserves rendered allocation count"), Component->GetWeightmapLayerAllocations(false).Num(), RenderedAllocationCount);
		for (const FWeightmapLayerAllocationInfo& Allocation : Component->GetWeightmapLayerAllocations(false))
		{
			TestNotEqual(TEXT("brush rejection leaves destination absent from rendered allocations"), Allocation.LayerInfo.Get(), To);
		}
		TestEqual(TEXT("brush rejection preserves mobile allocation"), Component->MobileWeightmapLayerAllocations[0].LayerInfo.Get(), From);
		TestEqual(TEXT("brush rejection preserves collision allocation"), Collision->ComponentLayerInfos[0].Get(), From);
		TestTrue(TEXT("brush rejection preserves allowlist"), Component->LayerAllowList.Contains(From) && Component->LayerAllowList.Contains(Unrelated));
		TArray<uint16> BrushHeights;
		BrushHeights.SetNumZeroed(64 * 64);
		TArray<uint8> BrushWeights;
		BrushWeights.SetNumZeroed(64 * 64);
		{
			FLandscapeEditDataInterface Edit(Info, BaseGuid);
			Edit.GetHeightDataFast(0, 0, 63, 63, BrushHeights.GetData(), 64);
			Edit.GetWeightDataFast(From, 0, 0, 63, 63, BrushWeights.GetData(), 64);
		}
		TestTrue(TEXT("brush rejection preserves heights"), BeforeHeights == BrushHeights);
		TestTrue(TEXT("brush rejection preserves weights"), BeforeWeights == BrushWeights);
		const FLandscapeLayerInfoReplacementResult Refused = ULandscapeService::ReplaceLayerInfoWithResult(Fixture.Label, From, To);
		TestFalse(TEXT("detailed result defaults to refusal"), Refused.bSuccess);
		TestTrue(TEXT("refusal explains brush risk"), Refused.Message.Contains(TEXT("brush")) && Refused.Message.Contains(TEXT("reference")));
		TestEqual(TEXT("detailed brush refusal preserves transaction count"), GEditor->Trans->GetQueueLength(), QueueBeforeBrushRejection);
		TestEqual(TEXT("detailed brush refusal preserves redo count"), GEditor->Trans->GetUndoCount(), RedoBeforeBrushRejection);
		TestEqual(TEXT("detailed brush refusal preserves target"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).LayerInfoObj.Get(), From);
		TestEqual(TEXT("detailed brush refusal preserves brush"), Brush->LayerInfo.Get(), From);
		TestFalse(TEXT("opt-in still rejects incompatible asset"), ULandscapeService::ReplaceLayerInfoWithResult(Fixture.Label, From, WrongName, true).bSuccess);
		const FLandscapeLayerInfoReplacementResult Allowed = ULandscapeService::ReplaceLayerInfoWithResult(Fixture.Label, From, To, true);
		if (!TestTrue(TEXT("explicit opt-in permits replacement"), Allowed.bSuccess)) { return false; }
		TestTrue(TEXT("success retains risk warning"), Allowed.Message.Contains(TEXT("brush")) && Allowed.Message.Contains(TEXT("reference")));
		TestEqual(TEXT("opt-in does not claim brush repair"), Brush->LayerInfo.Get(), From);
		TestEqual(TEXT("opt-in changes target"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).LayerInfoObj.Get(), To);
		if (!TestTrue(TEXT("undo opt-in replacement"), GEditor->UndoTransaction())) { return false; }
		TestEqual(TEXT("undo restores target after opt-in"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).LayerInfoObj.Get(), From);
	}
	TestTrue(TEXT("replace"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To));
	TestEqual(TEXT("replacement preserves reimport path"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).ReimportLayerFilePath, ReimportPath);
	TestEqual(TEXT("edit layer allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), To);
	TestEqual(TEXT("rendered allocation count preserved"), Component->GetWeightmapLayerAllocations(false).Num(), RenderedAllocationCount);
	for (const FWeightmapLayerAllocationInfo& Allocation : Component->GetWeightmapLayerAllocations(false))
	{
		TestNotEqual(TEXT("no stale rendered reference"), Allocation.LayerInfo.Get(), From);
	}
	TestEqual(TEXT("mobile allocation"), Component->MobileWeightmapLayerAllocations[0].LayerInfo.Get(), To);
	TestEqual(TEXT("collision allocation"), Collision->ComponentLayerInfos[0].Get(), To);
	TestFalse(TEXT("old allowlist reference removed"), Component->LayerAllowList.Contains(From));
	TestTrue(TEXT("replacement allowlist reference present"), Component->LayerAllowList.Contains(To));
	TestTrue(TEXT("unrelated allowlist reference preserved"), Component->LayerAllowList.Contains(Unrelated));
	TestEqual(TEXT("other landscape untouched"), OtherComponent->GetWeightmapLayerAllocations(OtherGuid)[0].LayerInfo.Get(), From);
	TestEqual(TEXT("editing layer preserved"), Fixture.Landscape->GetEditingLayer(), Editing);
	TArray<uint16> AfterHeights;
	AfterHeights.SetNumZeroed(64 * 64);
	TArray<uint8> AfterWeights;
	AfterWeights.SetNumZeroed(64 * 64);
	{
		FLandscapeEditDataInterface Edit(Fixture.Landscape->GetLandscapeInfo(), BaseGuid);
		Edit.GetHeightDataFast(0, 0, 63, 63, AfterHeights.GetData(), 64);
		Edit.GetWeightDataFast(To, 0, 0, 63, 63, AfterWeights.GetData(), 64);
	}
	TestTrue(TEXT("height samples preserved"), BeforeHeights == AfterHeights);
	TestTrue(TEXT("weight samples preserved"), BeforeWeights == AfterWeights);
	TestEqual(TEXT("nonzero source paint"), BeforeWeights[32 * 64 + 32], static_cast<uint8>(113));
	TestTrue(TEXT("repeat is idempotent"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To));
	TestTrue(TEXT("Undo"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo preserves reimport path"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).ReimportLayerFilePath, ReimportPath);
	TestEqual(TEXT("Undo edit allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), From);
	TestEqual(TEXT("Undo rendered count"), Component->GetWeightmapLayerAllocations(false).Num(), RenderedAllocationCount);
	TestEqual(TEXT("Undo mobile"), Component->MobileWeightmapLayerAllocations[0].LayerInfo.Get(), From);
	TestEqual(TEXT("Undo collision"), Collision->ComponentLayerInfos[0].Get(), From);
	TestTrue(TEXT("Undo allowlist restored"), Component->LayerAllowList.Contains(From));
	TestTrue(TEXT("Undo unrelated allowlist retained"), Component->LayerAllowList.Contains(Unrelated));
	TestTrue(TEXT("Redo"), GEditor->RedoTransaction());
	TestEqual(TEXT("Redo preserves reimport path"), Fixture.Landscape->GetTargetLayers().FindChecked(TEXT("RoadSoil")).ReimportLayerFilePath, ReimportPath);
	TestEqual(TEXT("Redo edit allocation"), Component->GetWeightmapLayerAllocations(BaseGuid)[0].LayerInfo.Get(), To);
	TestEqual(TEXT("Redo rendered count"), Component->GetWeightmapLayerAllocations(false).Num(), RenderedAllocationCount);
	for (const FWeightmapLayerAllocationInfo& Allocation : Component->GetWeightmapLayerAllocations(false))
	{
		TestNotEqual(TEXT("Redo no stale rendered reference"), Allocation.LayerInfo.Get(), From);
	}
	TestEqual(TEXT("Redo mobile"), Component->MobileWeightmapLayerAllocations[0].LayerInfo.Get(), To);
	TestEqual(TEXT("Redo collision"), Collision->ComponentLayerInfos[0].Get(), To);
	TestFalse(TEXT("Redo old allowlist reference removed"), Component->LayerAllowList.Contains(From));
	TestTrue(TEXT("Redo new allowlist reference present"), Component->LayerAllowList.Contains(To));
	TestTrue(TEXT("Redo unrelated allowlist retained"), Component->LayerAllowList.Contains(Unrelated));

	Collision->ComponentLayerInfos[0] = From;
	Component->LayerAllowList.Add(From);
	TestTrue(TEXT("repair stale collision-only reference"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To));
	TestEqual(TEXT("collision-only reference repaired"), Collision->ComponentLayerInfos[0].Get(), To);
	TestFalse(TEXT("stale allowlist reference repaired"), Component->LayerAllowList.Contains(From));
	TestTrue(TEXT("Undo collision-only repair"), GEditor->UndoTransaction());
	TestTrue(TEXT("Undo stale allowlist repair"), Component->LayerAllowList.Contains(From));
	TestEqual(TEXT("Undo restores stale collision reference"), Collision->ComponentLayerInfos[0].Get(), From);
	TestTrue(TEXT("Redo collision-only repair"), GEditor->RedoTransaction());
	TestFalse(TEXT("Redo stale allowlist repair"), Component->LayerAllowList.Contains(From));
	TestEqual(TEXT("Redo repairs collision reference"), Collision->ComponentLayerInfos[0].Get(), To);

	// Fixture-only reflection: UE 5.8 exposes this map's semantic accessors as internal.
	ULandscapeEditLayerBase* EditLayer = Fixture.Landscape->GetEditLayer(0);
	FMapProperty* BlendProperty = FindFProperty<FMapProperty>(ULandscapeEditLayerBase::StaticClass(), TEXT("WeightmapLayerAllocationBlend"));
	if (!TestNotNull(TEXT("blend metadata property"), BlendProperty)) { return false; }
	FObjectPropertyBase* BlendKey = CastField<FObjectPropertyBase>(BlendProperty->KeyProp);
	FBoolProperty* BlendValue = CastField<FBoolProperty>(BlendProperty->ValueProp);
	if (!TestNotNull(TEXT("blend metadata key type"), BlendKey) || !TestNotNull(TEXT("blend metadata value type"), BlendValue)) { return false; }
	FScriptMapHelper BlendMap(BlendProperty, BlendProperty->ContainerPtrToValuePtr<void>(EditLayer));
	const auto FindBlend = [&](ULandscapeLayerInfoObject* Layer) -> int32
	{
		for (int32 I = 0; I < BlendMap.GetMaxIndex(); ++I)
		{
			if (BlendMap.IsValidIndex(I) && BlendKey->GetObjectPropertyValue(BlendMap.GetKeyPtr(I)) == Layer) { return I; }
		}
		return INDEX_NONE;
	};
	if (!TestEqual(TEXT("no source blend metadata before fixture"), FindBlend(From), INDEX_NONE) ||
		!TestEqual(TEXT("no destination blend metadata before fixture"), FindBlend(To), INDEX_NONE) ||
		!TestEqual(TEXT("no unrelated blend metadata before fixture"), FindBlend(Unrelated), INDEX_NONE)) { return false; }
	const int32 BlendIndex = BlendMap.AddDefaultValue_Invalid_NeedsRehash();
	BlendKey->SetObjectPropertyValue(BlendMap.GetKeyPtr(BlendIndex), From);
	BlendValue->SetPropertyValue(BlendMap.GetValuePtr(BlendIndex), true);
	const int32 UnrelatedBlend = BlendMap.AddDefaultValue_Invalid_NeedsRehash();
	BlendKey->SetObjectPropertyValue(BlendMap.GetKeyPtr(UnrelatedBlend), Unrelated);
	BlendValue->SetPropertyValue(BlendMap.GetValuePtr(UnrelatedBlend), false);
	BlendMap.Rehash();
	TestTrue(TEXT("repair stale blend-only reference"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To));
	TestEqual(TEXT("stale blend source removed"), FindBlend(From), INDEX_NONE);
	const int32 ReboundBlend = FindBlend(To);
	if (!TestTrue(TEXT("blend destination present"), ReboundBlend != INDEX_NONE)) { return false; }
	TestTrue(TEXT("blend value preserved"), BlendValue->GetPropertyValue(BlendMap.GetValuePtr(ReboundBlend)));
	const int32 UnrelatedIndex = FindBlend(Unrelated);
	if (!TestTrue(TEXT("unrelated blend preserved"), UnrelatedIndex != INDEX_NONE)) { return false; }
	TestFalse(TEXT("unrelated blend value preserved"), BlendValue->GetPropertyValue(BlendMap.GetValuePtr(UnrelatedIndex)));
	TestTrue(TEXT("repeat blend repair is idempotent"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To));
	TestTrue(TEXT("Undo blend repair"), GEditor->UndoTransaction());
	const int32 UndoBlend = FindBlend(From);
	if (!TestTrue(TEXT("Undo restores source blend"), UndoBlend != INDEX_NONE)) { return false; }
	TestTrue(TEXT("Undo source blend value"), BlendValue->GetPropertyValue(BlendMap.GetValuePtr(UndoBlend)));
	TestEqual(TEXT("Undo removes destination blend"), FindBlend(To), INDEX_NONE);
	TestTrue(TEXT("Redo blend repair"), GEditor->RedoTransaction());
	TestEqual(TEXT("Redo removes source blend"), FindBlend(From), INDEX_NONE);
	if (!TestTrue(TEXT("Redo restores destination blend"), FindBlend(To) != INDEX_NONE)) { return false; }
	const int32 ConflictingBlend = BlendMap.AddDefaultValue_Invalid_NeedsRehash();
	BlendKey->SetObjectPropertyValue(BlendMap.GetKeyPtr(ConflictingBlend), From);
	BlendValue->SetPropertyValue(BlendMap.GetValuePtr(ConflictingBlend), false);
	BlendMap.Rehash();
	TestFalse(TEXT("conflicting blend references rejected"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To));
	const int32 ConflictingFrom = FindBlend(From);
	const int32 ConflictingTo = FindBlend(To);
	if (!TestTrue(TEXT("conflicting source retained"), ConflictingFrom != INDEX_NONE) ||
		!TestTrue(TEXT("conflicting destination retained"), ConflictingTo != INDEX_NONE)) { return false; }
	TestFalse(TEXT("conflicting source value unchanged"), BlendValue->GetPropertyValue(BlendMap.GetValuePtr(ConflictingFrom)));
	TestTrue(TEXT("conflicting destination value unchanged"), BlendValue->GetPropertyValue(BlendMap.GetValuePtr(ConflictingTo)));
	BlendMap.RemoveAt(ConflictingTo);
	TestTrue(TEXT("repair additive blend reference"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, From, To));
	const int32 AdditiveTo = FindBlend(To);
	if (!TestTrue(TEXT("additive destination present"), AdditiveTo != INDEX_NONE)) { return false; }
	TestFalse(TEXT("additive value preserved"), BlendValue->GetPropertyValue(BlendMap.GetValuePtr(AdditiveTo)));
	TestTrue(TEXT("Undo additive repair"), GEditor->UndoTransaction());
	const int32 AdditiveFrom = FindBlend(From);
	if (!TestTrue(TEXT("Undo additive source present"), AdditiveFrom != INDEX_NONE)) { return false; }
	TestFalse(TEXT("Undo additive value preserved"), BlendValue->GetPropertyValue(BlendMap.GetValuePtr(AdditiveFrom)));
	TestTrue(TEXT("Redo additive repair"), GEditor->RedoTransaction());
	TestTrue(TEXT("normal replacement moves blend metadata"), ULandscapeService::ReplaceLayerInfo(Fixture.Label, To, From));
	TestEqual(TEXT("normal replacement removed old blend"), FindBlend(To), INDEX_NONE);
	const int32 NormalFrom = FindBlend(From);
	if (!TestTrue(TEXT("normal replacement moved blend"), NormalFrom != INDEX_NONE)) { return false; }
	TestFalse(TEXT("normal replacement preserved blend value"), BlendValue->GetPropertyValue(BlendMap.GetValuePtr(NormalFrom)));
	TestTrue(TEXT("Undo normal replacement"), GEditor->UndoTransaction());
	TestEqual(TEXT("Undo normal replacement removed blend"), FindBlend(From), INDEX_NONE);
	const int32 NormalUndo = FindBlend(To);
	if (!TestTrue(TEXT("Undo normal replacement restored blend"), NormalUndo != INDEX_NONE)) { return false; }
	TestFalse(TEXT("Undo normal replacement preserved blend value"), BlendValue->GetPropertyValue(BlendMap.GetValuePtr(NormalUndo)));

	return true;
}

#endif
