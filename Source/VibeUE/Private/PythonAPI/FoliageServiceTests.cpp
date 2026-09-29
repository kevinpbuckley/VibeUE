// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UFoliageService.h"
#include "AIServiceTestFixture.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "ActorPartition/PartitionActor.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "InstancedFoliage.h"
#include "InstancedFoliageActor.h"
#include "Misc/ScopeExit.h"

static const EAutomationTestFlags kFoliageTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

// The fixture reset (in-memory and on-disk) is shared with the AI suites; see AIServiceTestFixture.h.
using VibeAITest::FScopedFixtureReset;

// On a World Partition level the engine keeps foliage in one InstancedFoliageActor per grid cell. Placing, counting and
// removing through the service must use those cell actors: no actor may hold instances from more than one cell.
// The test opens a new, unsaved World Partition map and ends on a new plain map, as a fresh editor starts; no map file
// is written.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeFoliageWorldPartitionCellsTest,
	"VibeUE.Foliage.WorldPartitionCells", kFoliageTestFlags)
bool FVibeFoliageWorldPartitionCellsTest::RunTest(const FString&)
{
	if (!GEditor || GEditor->PlayWorld)
	{
		AddError(TEXT("Needs the editor with no PIE session running."));
		return false;
	}

	const FString Dir = TEXT("/Game/Developers/VibeUEFoliageTests");
	const FString TypePath = Dir / TEXT("FT_VibeWPTest");
	FScopedFixtureReset ResetType(TypePath);

	UWorld* World = GEditor->NewMap(/*bIsPartitionedWorld*/ true);
	ON_SCOPE_EXIT
	{
		GEditor->NewMap(/*bIsPartitionedWorld*/ false);
	};
	if (!TestTrue(TEXT("a new World Partition map is open"), World && World->IsPartitionedWorld()))
	{
		return false;
	}

	const FFoliageTypeCreateResult Created = UFoliageService::CreateFoliageType(TEXT("/Engine/BasicShapes/Cube"), Dir, TEXT("FT_VibeWPTest"));
	if (!TestTrue(FString::Printf(TEXT("foliage type asset created (%s)"), *Created.ErrorMessage), Created.bSuccess))
	{
		return false;
	}

	// Two instances in one grid cell, one in the next cell along X.
	const uint32 GridSize = FMath::Max<uint32>(1u, AInstancedFoliageActor::StaticClass()->GetDefaultObject<APartitionActor>()->GetDefaultGridSize(World));
	const double NextCellX = GridSize * 1.5;
	const TArray<FVector> Locations = { FVector(100.0, 100.0, 0.0), FVector(300.0, 100.0, 0.0), FVector(NextCellX, 100.0, 0.0) };
	const FFoliageScatterResult Placed = UFoliageService::AddFoliageInstances(TypePath, Locations,
		/*MinScale*/ 1.0f, /*MaxScale*/ 1.0f, /*bAlignToNormal*/ false, /*bRandomYaw*/ false, /*bTraceToSurface*/ false);
	TestTrue(FString::Printf(TEXT("add_foliage_instances succeeded (%s)"), *Placed.ErrorMessage), Placed.bSuccess);
	TestEqual(TEXT("three instances added"), Placed.InstancesAdded, 3);

	int32 ActorsWithInstances = 0;
	int32 InstancesInActors = 0;
	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* Actor = *It;
		if (Actor->HasAnyFlags(RF_Transient))
		{
			continue; // foliage mode's palette holder on a partitioned world; it holds no instances
		}
		TSet<FIntVector> Cells;
		int32 Count = 0;
		Actor->ForEachFoliageInfo([World, GridSize, &Cells, &Count](UFoliageType*, FFoliageInfo& Info)
		{
			for (const FFoliageInstance& Instance : Info.Instances)
			{
				const UActorPartitionSubsystem::FCellCoord Coord = UActorPartitionSubsystem::FCellCoord::GetCellCoord(Instance.Location, World->PersistentLevel, GridSize);
				Cells.Add(FIntVector(static_cast<int32>(Coord.X), static_cast<int32>(Coord.Y), static_cast<int32>(Coord.Z)));
				++Count;
			}
			return true;
		});
		if (Count > 0)
		{
			++ActorsWithInstances;
			InstancesInActors += Count;
			TestEqual(FString::Printf(TEXT("%s holds instances from one grid cell only"), *Actor->GetActorNameOrLabel()), Cells.Num(), 1);
		}
	}
	TestEqual(TEXT("one foliage actor per grid cell used"), ActorsWithInstances, 2);
	TestEqual(TEXT("the actors hold all three instances"), InstancesInActors, 3);

	TestEqual(TEXT("get_instance_count counts every cell"), UFoliageService::GetInstanceCount(TypePath), 3);
	const FFoliageRemoveResult Removed = UFoliageService::RemoveAllFoliageOfType(TypePath);
	TestTrue(FString::Printf(TEXT("remove_all_foliage_of_type succeeded (%s)"), *Removed.ErrorMessage), Removed.bSuccess);
	TestEqual(TEXT("remove_all_foliage_of_type removes every cell's instances"), Removed.InstancesRemoved, 3);
	TestEqual(TEXT("none left"), UFoliageService::GetInstanceCount(TypePath), 0);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
