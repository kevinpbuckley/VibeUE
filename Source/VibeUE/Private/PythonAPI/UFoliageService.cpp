// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UFoliageService.h"
#include "InstancedFoliageActor.h"
#include "FoliageType.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeEdit.h"
#include "LandscapeLayerInfoObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
// World Partition (per grid cell foliage actor) support
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "ActorPartition/PartitionActor.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"
#include "WorldPartition/ActorPartition/PartitionActorDesc.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"
#include "Misc/FeedbackContext.h" // GWarn as FSavePackageArgs::Error (complete type needed for FOutputDevice*)

// =================================================================
// World Partition helpers
// =================================================================
// On a partitioned (World Partition) world the engine keeps foliage in one InstancedFoliageActor per grid cell
// (UActorPartitionSubsystem, WorldSettings InstancedFoliageGridSize). There:
// - GetInstancedFoliageActorFor*Level must never run: ensure(IsLevelPartition) in InstancedFoliage.cpp, then ONE plain
//   non-partitioned actor (named *_UAID_*) that holds the whole map's foliage.
// - Foliage types must be assets: an IFA-outered type is forbidden (AddMesh check(!IsPartitionedWorld)), and
//   AddFoliageType would silently duplicate a non-asset type into every cell actor.
// - AInstancedFoliageActor::Get(World, true, Level, Location) returns nullptr when the cell's actor exists but is not
//   loaded, so placement checks the target cells first and fails the whole call instead of dropping instances. That
//   check runs before the call opens its transaction and before a new foliage type asset is written, so a refused call
//   changes nothing (no FScopedTransaction::Cancel: nested in a caller's transaction it would cancel the caller's).
// Non-partitioned worlds keep the original code paths.

// Read-only marker, so a tool or guard can tell a VibeUE build that places foliage per World Partition cell from an
// older one that must not run FoliageService on a World Partition level.
static TAutoConsoleVariable<int32> CVarVibeUEFoliageWorldPartitionCells(
	TEXT("vibeue.Foliage.WorldPartitionCells"),
	1,
	TEXT("Read-only marker: this VibeUE build's FoliageService places foliage per World Partition cell."),
	ECVF_ReadOnly);

namespace VibeUEFoliageWP
{
	using FCellKey = TTuple<int64, int64, int64>;

	/** A foliage actor of the world that has an actor desc but is not loaded */
	struct FUnloadedFoliageActor
	{
		FString Name;
		FBox Bounds = FBox(ForceInit); // XY footprint for overlap tests (cell box + editor bounds); invalid = unknown
		bool bIsGridCell = false;
		FCellKey Cell;
	};

	bool IsPartitioned(const UWorld* World)
	{
		return World && World->IsPartitionedWorld();
	}

	/** The grid size the partition subsystem uses for foliage (WorldSettings InstancedFoliageGridSize) */
	uint32 GetFoliageGridSize(UWorld* World)
	{
		const APartitionActor* Default = AInstancedFoliageActor::StaticClass()->GetDefaultObject<APartitionActor>();
		return FMath::Max<uint32>(1u, Default->GetDefaultGridSize(World));
	}

	/** Same cell math as UActorPartitionSubsystem (FCellCoord::GetCellCoord on the persistent level) */
	FCellKey GetCellKey(UWorld* World, const FVector& Location, uint32 GridSize)
	{
		const UActorPartitionSubsystem::FCellCoord Coord = UActorPartitionSubsystem::FCellCoord::GetCellCoord(Location, World->PersistentLevel, GridSize);
		return FCellKey(Coord.X, Coord.Y, Coord.Z);
	}

	/** Foliage mode's palette holder (AInstancedFoliageActor::GetDefault) is transient on partitioned worlds and holds no instances */
	bool IsTransientFoliageActor(const AInstancedFoliageActor* IFA)
	{
		return IFA->HasAnyFlags(RF_Transient);
	}

	TArray<FUnloadedFoliageActor> CollectUnloadedFoliageActors(UWorld* World)
	{
		TArray<FUnloadedFoliageActor> Out;
		UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
		if (!WorldPartition)
		{
			return Out;
		}

		const uint32 GridSize = GetFoliageGridSize(World);

		FWorldPartitionHelpers::ForEachActorDescInstance(WorldPartition, AInstancedFoliageActor::StaticClass(),
			[&Out, GridSize](const FWorldPartitionActorDescInstance* Desc)
			{
				// same "loaded" test as the partition subsystem's lookup
				if (Desc->GetActor())
				{
					return true;
				}

				FUnloadedFoliageActor& Entry = Out.AddDefaulted_GetRef();
				Entry.Name = Desc->GetActorLabelOrName().ToString();

				// foliage actors use FPartitionActorDesc (APartitionActor::CreateClassActorDesc), same cast as the engine
				const FPartitionActorDesc* PartitionDesc = static_cast<const FPartitionActorDesc*>(Desc->GetActorDesc());
				if (PartitionDesc && PartitionDesc->GridSize == GridSize && !PartitionDesc->GridGuid.IsValid())
				{
					Entry.bIsGridCell = true;
					Entry.Cell = FCellKey(PartitionDesc->GridIndexX, PartitionDesc->GridIndexY, PartitionDesc->GridIndexZ);
					Entry.Bounds = Desc->GetEditorBounds() + UActorPartitionSubsystem::FCellCoord::GetCellBounds(
						UActorPartitionSubsystem::FCellCoord(PartitionDesc->GridIndexX, PartitionDesc->GridIndexY, PartitionDesc->GridIndexZ, nullptr), GridSize);
				}
				// A non-grid actor (the *_UAID_* one) keeps Bounds invalid = unknown footprint.
				// Its desc bounds are FPartitionActorDesc's cell box for its own GridSize (1 cm at the origin for a plain
				// SpawnActor), not where its instances are, so it has to count as overlapping every area.

				return true;
			});

		return Out;
	}

	/** 2D test like the service's own radius tests; an unknown footprint counts as overlapping */
	bool OverlapsCircleXY(const FBox& Box, double CenterX, double CenterY, double Radius)
	{
		if (!Box.IsValid)
		{
			return true;
		}

		const double DX = FMath::Max3(Box.Min.X - CenterX, 0.0, CenterX - Box.Max.X);
		const double DY = FMath::Max3(Box.Min.Y - CenterY, 0.0, CenterY - Box.Max.Y);
		return DX * DX + DY * DY <= Radius * Radius;
	}

	TArray<FUnloadedFoliageActor> CollectUnloadedFoliageActorsInCircle(UWorld* World, double CenterX, double CenterY, double Radius)
	{
		TArray<FUnloadedFoliageActor> Unloaded = CollectUnloadedFoliageActors(World);
		Unloaded.RemoveAll([CenterX, CenterY, Radius](const FUnloadedFoliageActor& Entry)
		{
			return !OverlapsCircleXY(Entry.Bounds, CenterX, CenterY, Radius);
		});
		return Unloaded;
	}

	FString DescribeUnloaded(const TArray<FUnloadedFoliageActor>& Unloaded)
	{
		constexpr int32 MaxNames = 6;
		TArray<FString> Names;
		for (int32 Index = 0; Index < Unloaded.Num() && Index < MaxNames; ++Index)
		{
			const FUnloadedFoliageActor& Entry = Unloaded[Index];
			Names.Add(Entry.bIsGridCell
				? FString::Printf(TEXT("%s (cell %lld,%lld,%lld)"), *Entry.Name, Entry.Cell.Get<0>(), Entry.Cell.Get<1>(), Entry.Cell.Get<2>())
				: FString::Printf(TEXT("%s (not a grid cell actor)"), *Entry.Name));
		}

		FString Text = FString::Join(Names, TEXT(", "));
		if (Unloaded.Num() > MaxNames)
		{
			Text += FString::Printf(TEXT(" and %d more"), Unloaded.Num() - MaxNames);
		}
		return Text;
	}

	/** Loaded foliage actors that are not per-cell partition actors, e.g. the *_UAID_* one the level path creates */
	TArray<FString> FindNonGridFoliageActors(UWorld* World)
	{
		TArray<FString> Names;
		const uint32 GridSize = GetFoliageGridSize(World);
		for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
		{
			if (!IsTransientFoliageActor(*It) && It->GetGridSize() != GridSize)
			{
				Names.Add(It->GetActorNameOrLabel());
			}
		}
		return Names;
	}

	/** List / count calls return data, not errors: log what they could not see */
	void WarnAboutPartialView(UWorld* World, const TCHAR* Caller)
	{
		const TArray<FUnloadedFoliageActor> Unloaded = CollectUnloadedFoliageActors(World);
		if (Unloaded.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("UFoliageService::%s: %d foliage actor(s) of this World Partition level are not loaded (%s); the result covers loaded cells only. Load them for a complete result."),
				Caller, Unloaded.Num(), *DescribeUnloaded(Unloaded));
		}

		const TArray<FString> NonGrid = FindNonGridFoliageActors(World);
		if (NonGrid.Num() > 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("UFoliageService::%s: non-partitioned foliage actor(s) %s in a World Partition level (made through GetInstancedFoliageActorFor*Level); their instances are not split per grid cell."),
				Caller, *FString::Join(NonGrid, TEXT(", ")));
		}
	}

	bool HasInstanceInCircleXY(const FFoliageInfo& Info, float CenterX, float CenterY, float RadiusSq)
	{
		for (const FFoliageInstance& Instance : Info.Instances)
		{
			const float DX = Instance.Location.X - CenterX;
			const float DY = Instance.Location.Y - CenterY;
			if (DX * DX + DY * DY <= RadiusSq)
			{
				return true;
			}
		}
		return false;
	}

	int32 CountInstances(const AInstancedFoliageActor* IFA)
	{
		int32 Count = 0;
		for (const auto& Pair : IFA->GetFoliageInfos())
		{
			Count += Pair.Value.Get().Instances.Num();
		}
		return Count;
	}

	/**
	 * The foliage type a placement call on a partitioned
	 * world will use, resolved without side effects. Type is an existing asset; when it is null (or bSaveBeforePlacing),
	 * FinalizePlacementFoliageType creates / saves <Mesh>_FoliageType, and only once the call passed validation and has
	 * instances to place, so a failed or empty call leaves no new asset on disk.
	 */
	struct FPlacementFoliageType
	{
		UFoliageType* Type = nullptr;
		UStaticMesh* MeshToCreateFor = nullptr;
		FString PackageName;
		FString AssetName;
		bool bSaveBeforePlacing = false;
	};

	/** Saving while PIE runs is refused: saves during a play session have crashed the editor */
	bool RefuseSaveDuringPlay(const FString& What, FString& OutError)
	{
		if (GEditor && GEditor->IsPlaySessionInProgress())
		{
			OutError = FString::Printf(TEXT("World Partition levels need a saved foliage type asset, and %s would have to be created or saved, which is refused while PIE runs. Stop PIE, or pass an existing foliage type asset path."), *What);
			return true;
		}
		return false;
	}

	/**
	 * The foliage type to place on a partitioned world: always an instanced-static-mesh foliage type ASSET.
	 * A foliage type path must be an asset. A mesh path resolves the same way whatever cells are loaded:
	 * 1. <Mesh>_FoliageType next to the mesh (Foliage mode's default name; /Game/Foliage for meshes outside /Game);
	 * 2. else the single other ISM foliage type asset for that mesh (asset registry referencers of the mesh, plus the
	 *    loaded foliage actors); several = ambiguous, the call fails and asks for a foliage type path;
	 * 3. else <Mesh>_FoliageType is created with default settings by FinalizePlacementFoliageType.
	 */
	bool ResolvePlacementFoliageType(UWorld* World, const FString& MeshOrFoliageTypePath, FPlacementFoliageType& Out, FString& OutError)
	{
		Out = FPlacementFoliageType();
		UObject* LoadedAsset = StaticLoadObject(UObject::StaticClass(), nullptr, *MeshOrFoliageTypePath);

		UFoliageType* FoliageType = Cast<UFoliageType>(LoadedAsset);
		UStaticMesh* Mesh = Cast<UStaticMesh>(LoadedAsset);
		if (!FoliageType && !Mesh)
		{
			FoliageType = LoadObject<UFoliageType>(nullptr, *MeshOrFoliageTypePath);
		}
		if (!FoliageType && !Mesh)
		{
			Mesh = LoadObject<UStaticMesh>(nullptr, *MeshOrFoliageTypePath);
		}

		if (FoliageType)
		{
			if (!FoliageType->IsAsset())
			{
				OutError = FString::Printf(TEXT("'%s' is not a foliage type asset. World Partition levels need a saved UFoliageType asset (create one with create_foliage_type, or pass the static mesh path)."), *MeshOrFoliageTypePath);
				return false;
			}
			if (!FoliageType->IsA<UFoliageType_InstancedStaticMesh>())
			{
				OutError = FString::Printf(TEXT("'%s' is not an instanced static mesh foliage type; actor foliage is not supported on World Partition levels."), *MeshOrFoliageTypePath);
				return false;
			}
			Out.Type = FoliageType;
			return true;
		}

		if (!Mesh)
		{
			OutError = FString::Printf(TEXT("Could not load '%s' as a StaticMesh or FoliageType"), *MeshOrFoliageTypePath);
			return false;
		}

		// 1. <Mesh>_FoliageType next to the mesh, reused when it exists
		FString PackagePath = FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName());
		if (PackagePath != TEXT("/Game") && !PackagePath.StartsWith(TEXT("/Game/")))
		{
			PackagePath = TEXT("/Game/Foliage");
		}
		Out.AssetName = Mesh->GetName() + TEXT("_FoliageType");
		Out.PackageName = PackagePath / Out.AssetName;
		const FString ObjectPath = Out.PackageName + TEXT(".") + Out.AssetName;

		if (UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			UFoliageType_InstancedStaticMesh* ExistingType = Cast<UFoliageType_InstancedStaticMesh>(Existing);
			if (!ExistingType || ExistingType->GetStaticMesh() != Mesh)
			{
				OutError = FString::Printf(TEXT("'%s' exists but is not an instanced static mesh foliage type for '%s'. Pass a UFoliageType asset path instead."), *ObjectPath, *Mesh->GetPathName());
				return false;
			}
			Out.Type = ExistingType;
			// made by an earlier call whose save failed: still only in memory, saved before placing
			Out.bSaveBeforePlacing = !FPackageName::DoesPackageExist(Out.PackageName);
			return !Out.bSaveBeforePlacing || !RefuseSaveDuringPlay(ObjectPath, OutError);
		}

		// 2. the single other ISM foliage type asset for this mesh
		TArray<UFoliageType*> Candidates;
		auto AddCandidate = [&Candidates, Mesh](UFoliageType* Type)
		{
			const UFoliageType_InstancedStaticMesh* ISMType = Cast<UFoliageType_InstancedStaticMesh>(Type);
			if (ISMType && ISMType->IsAsset() && ISMType->GetStaticMesh() == Mesh)
			{
				Candidates.AddUnique(Type);
			}
		};

		IAssetRegistry& AssetRegistry = FAssetRegistryModule::GetRegistry();
		TArray<FName> Referencers;
		AssetRegistry.GetReferencers(Mesh->GetOutermost()->GetFName(), Referencers);
		for (const FName& Referencer : Referencers)
		{
			TArray<FAssetData> Assets;
			AssetRegistry.GetAssetsByPackageName(Referencer, Assets);
			for (const FAssetData& Asset : Assets)
			{
				if (Asset.IsInstanceOf(UFoliageType_InstancedStaticMesh::StaticClass()))
				{
					AddCandidate(Cast<UFoliageType>(Asset.GetAsset()));
				}
			}
		}
		for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
		{
			for (const auto& Pair : It->GetFoliageInfos())
			{
				AddCandidate(Pair.Key);
			}
		}

		if (Candidates.Num() == 1)
		{
			Out.Type = Candidates[0];
			return true;
		}
		if (Candidates.Num() > 1)
		{
			TArray<FString> Paths;
			for (const UFoliageType* Candidate : Candidates)
			{
				Paths.Add(Candidate->GetPathName());
			}
			Paths.Sort();
			OutError = FString::Printf(TEXT("'%s' is used by %d foliage type assets (%s) and there is no %s. Pass the foliage type asset path to use."),
				*Mesh->GetPathName(), Paths.Num(), *FString::Join(Paths, TEXT(", ")), *ObjectPath);
			return false;
		}

		// 3. created (and saved) by FinalizePlacementFoliageType once the call has instances to place
		Out.MeshToCreateFor = Mesh;
		Out.bSaveBeforePlacing = true;
		return !RefuseSaveDuringPlay(ObjectPath, OutError);
	}

	/**
	 * Creates <Mesh>_FoliageType when needed and saves it when it is not on disk yet (the way Foliage mode does on
	 * World Partition levels, without its save dialog). Called after validation, right before placing.
	 */
	UFoliageType* FinalizePlacementFoliageType(const FPlacementFoliageType& Plan, FString& OutError)
	{
		UFoliageType* FoliageType = Plan.Type;
		const FString ObjectPath = Plan.PackageName + TEXT(".") + Plan.AssetName;

		if (!FoliageType)
		{
			UPackage* NewPackage = CreatePackage(*Plan.PackageName);
			if (!NewPackage)
			{
				OutError = FString::Printf(TEXT("Failed to create package '%s' for the foliage type of '%s'. Nothing was placed."), *Plan.PackageName, *Plan.MeshToCreateFor->GetPathName());
				return nullptr;
			}

			UFoliageType_InstancedStaticMesh* NewType = NewObject<UFoliageType_InstancedStaticMesh>(
				NewPackage, *Plan.AssetName, RF_Public | RF_Standalone | RF_Transactional);
			NewType->SetStaticMesh(Plan.MeshToCreateFor);
			NewType->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(NewType);
			FoliageType = NewType;

			UE_LOG(LogTemp, Log, TEXT("UFoliageService: created foliage type asset '%s' for mesh '%s' (World Partition levels need asset foliage types)"),
				*NewType->GetPathName(), *Plan.MeshToCreateFor->GetPathName());
		}

		if (Plan.bSaveBeforePlacing)
		{
			UPackage* Package = FoliageType->GetOutermost();
			const FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			// The default SaveArgs.Error is GError, which turns any save error
			// (file move failed under OneDrive / antivirus, not allowed to save, ...) into a fatal appError; GWarn logs it
			// and SavePackage returns false, as the editor's own saves do (EditorServer.cpp).
			SaveArgs.Error = GWarn;
			if (!UPackage::SavePackage(Package, FoliageType, *PackageFileName, SaveArgs))
			{
				OutError = FString::Printf(TEXT("Failed to save the foliage type '%s' (World Partition levels need a saved foliage type asset; it stays in memory unsaved, see the log). Nothing was placed."), *ObjectPath);
				return nullptr;
			}
		}

		return FoliageType;
	}

	/** The unloaded grid-cell foliage actor whose XY footprint contains Point, if any */
	const FUnloadedFoliageActor* FindUnloadedCellInColumn(const TArray<FUnloadedFoliageActor>& Unloaded, const FVector2D& Point)
	{
		for (const FUnloadedFoliageActor& Entry : Unloaded)
		{
			if (Entry.bIsGridCell && Entry.Bounds.IsValid && Entry.Bounds.IsInsideOrOnXY(FVector(Point.X, Point.Y, 0.0)))
			{
				return &Entry;
			}
		}
		return nullptr;
	}

	/**
	 * Side-effect-free check,
	 * run before the call opens its transaction, so a refused call never has to cancel one (FScopedTransaction::Cancel on
	 * a transaction nested in the caller's cancels the caller's whole transaction: UTransBuffer::Cancel). Fails, naming
	 * the cells, when
	 * - an instance's grid cell has a foliage actor that exists but is not loaded, or
	 * - a candidate whose surface trace found nothing lies in the XY footprint of such a cell (that region's terrain is
	 *   almost certainly unloaded too, so the candidate was dropped for that reason, not for having no surface).
	 */
	bool ValidatePartitionedPlacement(UWorld* World, const TArray<FFoliageInstance>& NewInstances, const TArray<FVector2D>& TraceMisses, FString& OutError)
	{
		const TArray<FUnloadedFoliageActor> Unloaded = CollectUnloadedFoliageActors(World);
		TMap<FCellKey, FString> UnloadedCells;
		for (const FUnloadedFoliageActor& Entry : Unloaded)
		{
			if (Entry.bIsGridCell)
			{
				UnloadedCells.Add(Entry.Cell, Entry.Name);
			}
		}
		if (UnloadedCells.Num() == 0)
		{
			return true;
		}

		constexpr int32 MaxListed = 6;
		const uint32 GridSize = GetFoliageGridSize(World);
		TArray<FString> Problems;

		// 1. instances whose target cell actor exists but is not loaded
		TMap<FCellKey, int32> FirstBlockedInstanceOfCell;
		int32 Blocked = 0;
		for (int32 Index = 0; Index < NewInstances.Num(); ++Index)
		{
			const FCellKey Key = GetCellKey(World, NewInstances[Index].Location, GridSize);
			if (UnloadedCells.Contains(Key))
			{
				++Blocked;
				if (!FirstBlockedInstanceOfCell.Contains(Key))
				{
					FirstBlockedInstanceOfCell.Add(Key, Index);
				}
			}
		}
		if (Blocked > 0)
		{
			TArray<FString> Cells;
			for (const TPair<FCellKey, int32>& Pair : FirstBlockedInstanceOfCell)
			{
				if (Cells.Num() == MaxListed)
				{
					Cells.Add(FString::Printf(TEXT("and %d more"), FirstBlockedInstanceOfCell.Num() - MaxListed));
					break;
				}
				Cells.Add(FString::Printf(TEXT("%s (cell %lld,%lld,%lld, e.g. an instance at %s)"), *UnloadedCells.FindChecked(Pair.Key),
					Pair.Key.Get<0>(), Pair.Key.Get<1>(), Pair.Key.Get<2>(), *NewInstances[Pair.Value].Location.ToCompactString()));
			}
			Problems.Add(FString::Printf(TEXT("%d of %d instances fall in grid cells whose InstancedFoliageActor exists but is not loaded: %s"),
				Blocked, NewInstances.Num(), *FString::Join(Cells, TEXT("; "))));
		}

		// 2. candidates dropped because their region is not loaded (the trace found no surface there)
		TMap<const FUnloadedFoliageActor*, FVector2D> FirstMissInCell;
		int32 Missed = 0;
		for (const FVector2D& Point : TraceMisses)
		{
			if (const FUnloadedFoliageActor* Entry = FindUnloadedCellInColumn(Unloaded, Point))
			{
				++Missed;
				if (!FirstMissInCell.Contains(Entry))
				{
					FirstMissInCell.Add(Entry, Point);
				}
			}
		}
		if (Missed > 0)
		{
			TArray<FString> Cells;
			for (const TPair<const FUnloadedFoliageActor*, FVector2D>& Pair : FirstMissInCell)
			{
				if (Cells.Num() == MaxListed)
				{
					Cells.Add(FString::Printf(TEXT("and %d more"), FirstMissInCell.Num() - MaxListed));
					break;
				}
				Cells.Add(FString::Printf(TEXT("%s (cell %lld,%lld,%lld, e.g. at X=%.0f Y=%.0f)"), *Pair.Key->Name,
					Pair.Key->Cell.Get<0>(), Pair.Key->Cell.Get<1>(), Pair.Key->Cell.Get<2>(), Pair.Value.X, Pair.Value.Y));
			}
			Problems.Add(FString::Printf(TEXT("%d candidate positions found no surface inside grid cells whose InstancedFoliageActor exists but is not loaded (their terrain is most likely not loaded either): %s"),
				Missed, *FString::Join(Cells, TEXT("; "))));
		}

		if (Problems.Num() == 0)
		{
			return true;
		}

		OutError = FString::Printf(TEXT("World Partition: %s. Load those cells (World Partition editor: load the region) and retry. Nothing was placed."),
			*FString::Join(Problems, TEXT(". ")));
		return false;
	}

	/**
	 * Adds the instances to the foliage actor of each one's grid cell (missing cell actors are created). Runs after
	 * ValidatePartitionedPlacement passed, inside the transaction the caller opened after it.
	 */
	bool ApplyPartitionedPlacement(UWorld* World, UFoliageType* FoliageType, const TArray<FFoliageInstance>& NewInstances,
		int32& OutAdded, FString& OutError)
	{
		OutAdded = 0;
		const uint32 GridSize = GetFoliageGridSize(World);

		TMap<FCellKey, int32> FirstInstanceOfCell;
		for (int32 Index = 0; Index < NewInstances.Num(); ++Index)
		{
			const FCellKey Key = GetCellKey(World, NewInstances[Index].Location, GridSize);
			if (!FirstInstanceOfCell.Contains(Key))
			{
				FirstInstanceOfCell.Add(Key, Index);
			}
		}

		// 1. every target cell's foliage actor (missing ones are created) before anything is placed
		TMap<FCellKey, AInstancedFoliageActor*> CellActors;
		for (const TPair<FCellKey, int32>& Pair : FirstInstanceOfCell)
		{
			const FVector& Location = NewInstances[Pair.Value].Location;
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::Get(World, true, World->PersistentLevel, Location);
			if (!IFA)
			{
				OutError = FString::Printf(TEXT("World Partition: no foliage actor for grid cell %lld,%lld,%lld (instance at %s); its actor is probably not loaded in the current data layer / content bundle context. No instance was placed (empty cell actors created before this are part of this call's undo step)."),
					Pair.Key.Get<0>(), Pair.Key.Get<1>(), Pair.Key.Get<2>(), *Location.ToCompactString());
				return false;
			}
			CellActors.Add(Pair.Key, IFA);
		}

		TMap<AInstancedFoliageActor*, TArray<const FFoliageInstance*>> InstancesPerActor;
		for (const FFoliageInstance& Instance : NewInstances)
		{
			InstancesPerActor.FindOrAdd(CellActors.FindChecked(GetCellKey(World, Instance.Location, GridSize))).Add(&Instance);
		}

		// 3. place, one batch per cell (the asset type is used as-is by every cell actor)
		for (const TPair<AInstancedFoliageActor*, TArray<const FFoliageInstance*>>& Pair : InstancesPerActor)
		{
			AInstancedFoliageActor* IFA = Pair.Key;
			IFA->Modify();

			FFoliageInfo* Info = nullptr;
			UFoliageType* CellType = IFA->AddFoliageType(FoliageType, &Info);
			if (!CellType || !Info)
			{
				OutError = FString::Printf(TEXT("World Partition: could not register '%s' on %s; %d instances were placed in other cells before this (undo reverts them)."),
					*FoliageType->GetPathName(), *IFA->GetActorNameOrLabel(), OutAdded);
				return false;
			}

			Info->AddInstances(CellType, Pair.Value);
			Info->Refresh(/*Async*/ true, /*Force*/ false);
			OutAdded += Pair.Value.Num();
		}

		return true;
	}
}

// =================================================================
// Helper Methods
// =================================================================

UWorld* UFoliageService::GetEditorWorld()
{
	if (GEditor)
	{
		return GEditor->GetEditorWorldContext().World();
	}
	return nullptr;
}

AInstancedFoliageActor* UFoliageService::GetOrCreateFoliageActor(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	// Never reach GetInstancedFoliageActorForCurrentLevel on a partitioned world (ensure + one plain
	// non-partitioned actor for the whole map); partitioned callers go through VibeUEFoliageWP instead.
	if (VibeUEFoliageWP::IsPartitioned(World))
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::GetOrCreateFoliageActor: refused on a World Partition world (foliage goes to per-cell actors there)"));
		return nullptr;
	}

	// Find existing IFA (return the first one if any exist)
	if (TActorIterator<AInstancedFoliageActor> It(World); It)
	{
		return *It;
	}

	// Create one if none exists
	return AInstancedFoliageActor::GetInstancedFoliageActorForCurrentLevel(World, true);
}

UFoliageType* UFoliageService::FindFoliageTypeInIFA(
	const FString& MeshOrFoliageTypePath,
	AInstancedFoliageActor* IFA)
{
	if (!IFA)
	{
		return nullptr;
	}

	// First try to load as a UFoliageType directly
	UObject* LoadedAsset = StaticLoadObject(UObject::StaticClass(), nullptr, *MeshOrFoliageTypePath);
	if (UFoliageType* FT = Cast<UFoliageType>(LoadedAsset))
	{
		// Check if this foliage type is registered in the IFA
		const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();
		if (FoliageInfos.Contains(FT))
		{
			return FT;
		}
	}

	// Try to find by mesh path — iterate all foliage types in IFA
	UStaticMesh* Mesh = Cast<UStaticMesh>(LoadedAsset);
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, *MeshOrFoliageTypePath);
	}

	if (Mesh)
	{
		const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();
		for (const auto& Pair : FoliageInfos)
		{
			UFoliageType_InstancedStaticMesh* ISMT = Cast<UFoliageType_InstancedStaticMesh>(Pair.Key);
			if (ISMT && ISMT->GetStaticMesh() == Mesh)
			{
				return Pair.Key;
			}
		}
	}

	return nullptr;
}

UFoliageType* UFoliageService::FindOrCreateFoliageTypeForMesh(
	const FString& MeshOrFoliageTypePath,
	AInstancedFoliageActor* IFA)
{
	if (!IFA)
	{
		return nullptr;
	}

	// This path duplicates non-asset types into the actor (IFA-outered types are forbidden on
	// partitioned worlds); partitioned callers use VibeUEFoliageWP::ResolvePlacementFoliageType.
	if (VibeUEFoliageWP::IsPartitioned(IFA->GetWorld()))
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::FindOrCreateFoliageTypeForMesh: refused on a World Partition world (asset foliage types only)"));
		return nullptr;
	}

	// Check if already registered
	UFoliageType* Existing = FindFoliageTypeInIFA(MeshOrFoliageTypePath, IFA);
	if (Existing)
	{
		return Existing;
	}

	// Try loading as UFoliageType asset
	UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *MeshOrFoliageTypePath);
	if (FoliageType)
	{
		IFA->AddFoliageType(FoliageType);
		return FoliageType;
	}

	// Try loading as UStaticMesh — create a transient foliage type
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshOrFoliageTypePath);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService: Could not load asset '%s' as StaticMesh or FoliageType"), *MeshOrFoliageTypePath);
		return nullptr;
	}

	// Create a new UFoliageType_InstancedStaticMesh in the transient package
	UFoliageType_InstancedStaticMesh* NewFT = NewObject<UFoliageType_InstancedStaticMesh>(
		GetTransientPackage(), NAME_None, RF_Transactional);
	NewFT->SetStaticMesh(Mesh);

	// Register it with the IFA
	FoliageType = IFA->AddFoliageType(NewFT);
	return FoliageType;
}

bool UFoliageService::TraceToSurface(
	UWorld* World, float X, float Y,
	FVector& OutLocation, FVector& OutNormal)
{
	if (!World)
	{
		return false;
	}

	// Trace from high above down to find surface
	FVector Start(X, Y, 100000.0f);
	FVector End(X, Y, -100000.0f);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.bReturnPhysicalMaterial = false;

	if (World->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldStatic, QueryParams))
	{
		OutLocation = HitResult.ImpactPoint;
		OutNormal = HitResult.ImpactNormal;
		return true;
	}

	return false;
}

// =================================================================
// Internal Scatter Implementation
// =================================================================

FFoliageScatterResult UFoliageService::ScatterInternal(
	const FString& MeshOrFoliageTypePath,
	const TArray<FVector2D>& CandidatePositions,
	int32 Count,
	float MinScale, float MaxScale,
	bool bAlignToNormal, bool bRandomYaw,
	int32 Seed,
	const FString& LandscapeNameOrLabel,
	const FString& LayerName,
	float LayerWeightThreshold)
{
	FFoliageScatterResult Result;
	Result.InstancesRequested = Count;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	// A partitioned world has no single foliage actor; the type must be an asset and the instances go
	// to the actor of their grid cell (VibeUEFoliageWP::ValidatePartitionedPlacement / ApplyPartitionedPlacement below).
	const bool bPartitioned = VibeUEFoliageWP::IsPartitioned(World);
	AInstancedFoliageActor* IFA = nullptr;
	UFoliageType* FoliageType = nullptr;
	VibeUEFoliageWP::FPlacementFoliageType PartitionedType;

	if (bPartitioned)
	{
		if (!VibeUEFoliageWP::ResolvePlacementFoliageType(World, MeshOrFoliageTypePath, PartitionedType, Result.ErrorMessage))
		{
			UE_LOG(LogTemp, Error, TEXT("UFoliageService::ScatterInternal: %s"), *Result.ErrorMessage);
			return Result;
		}
	}
	else
	{
		IFA = GetOrCreateFoliageActor(World);
		if (!IFA)
		{
			Result.ErrorMessage = TEXT("Failed to get or create InstancedFoliageActor");
			return Result;
		}

		FoliageType = FindOrCreateFoliageTypeForMesh(MeshOrFoliageTypePath, IFA);
		if (!FoliageType)
		{
			Result.ErrorMessage = FString::Printf(TEXT("Could not load or create foliage type for '%s'"), *MeshOrFoliageTypePath);
			return Result;
		}
	}

	// Optionally find landscape for layer-aware placement
	ALandscape* Landscape = nullptr;
	ULandscapeInfo* LandscapeInfo = nullptr;
	if (!LandscapeNameOrLabel.IsEmpty())
	{
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			ALandscape* L = *It;
			if (L->GetActorLabel().Equals(LandscapeNameOrLabel, ESearchCase::IgnoreCase) ||
				L->GetName().Equals(LandscapeNameOrLabel, ESearchCase::IgnoreCase))
			{
				Landscape = L;
				LandscapeInfo = L->GetLandscapeInfo();
				break;
			}
		}
	}

	// Check if layer-aware placement is requested but landscape not found
	bool bLayerAware = !LayerName.IsEmpty() && LayerWeightThreshold > 0.0f;
	if (bLayerAware && (!Landscape || !LandscapeInfo))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Layer-aware placement requires a valid landscape. '%s' not found."), *LandscapeNameOrLabel);
		return Result;
	}

	// Find target layer info for layer-aware placement
	ULandscapeLayerInfoObject* TargetLayerInfo = nullptr;
	if (bLayerAware)
	{
		for (const FLandscapeInfoLayerSettings& LayerSettings : LandscapeInfo->Layers)
		{
			if (LayerSettings.LayerInfoObj &&
				LayerSettings.LayerInfoObj->GetLayerName().ToString().Equals(LayerName, ESearchCase::IgnoreCase))
			{
				TargetLayerInfo = LayerSettings.LayerInfoObj;
				break;
			}
		}
		if (!TargetLayerInfo)
		{
			Result.ErrorMessage = FString::Printf(TEXT("Layer '%s' not found on landscape '%s'"), *LayerName, *LandscapeNameOrLabel);
			return Result;
		}
	}

	// Partitioned worlds open their transaction only after validation (see below), never cancelling one
	FScopedTransaction Transaction(NSLOCTEXT("FoliageService", "ScatterFoliage", "Scatter Foliage"), !bPartitioned);
	if (IFA) // null on partitioned worlds, the cell actors are modified when instances are added
	{
		IFA->Modify();
	}

	FRandomStream RNG(Seed != 0 ? Seed : FMath::Rand());

	// Collect valid instances
	TArray<FFoliageInstance> NewInstances;
	NewInstances.Reserve(Count);
	TArray<FVector2D> TraceMisses; // checked against unloaded cells on partitioned worlds

	for (const FVector2D& Pos : CandidatePositions)
	{
		if (NewInstances.Num() >= Count)
		{
			break;
		}

		// Trace to surface
		FVector HitLocation;
		FVector HitNormal;
		if (!TraceToSurface(World, Pos.X, Pos.Y, HitLocation, HitNormal))
		{
			Result.InstancesRejected++;
			if (bPartitioned)
			{
				TraceMisses.Add(Pos);
			}
			continue;
		}

		// Layer weight check
		if (bLayerAware && Landscape && LandscapeInfo)
		{
			FVector LandscapeLocation = Landscape->GetActorLocation();
			FVector LandscapeScale = Landscape->GetActorScale3D();
			int32 LocalX = FMath::RoundToInt((Pos.X - LandscapeLocation.X) / LandscapeScale.X);
			int32 LocalY = FMath::RoundToInt((Pos.Y - LandscapeLocation.Y) / LandscapeScale.Y);

			FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
			TArray<uint8> WeightData;
			WeightData.SetNumZeroed(1);
			LandscapeEdit.GetWeightData(TargetLayerInfo, LocalX, LocalY, LocalX, LocalY, WeightData.GetData(), 0);

			float Weight = WeightData[0] / 255.0f;
			if (Weight < LayerWeightThreshold)
			{
				Result.InstancesRejected++;
				continue;
			}
		}

		// Build foliage instance
		FFoliageInstance Instance;
		Instance.Location = HitLocation;

		// Scale
		float Scale = RNG.FRandRange(MinScale, MaxScale);
		Instance.DrawScale3D = FVector3f(Scale, Scale, Scale);

		// Rotation
		FRotator Rot = FRotator::ZeroRotator;
		if (bRandomYaw)
		{
			Rot.Yaw = RNG.FRandRange(0.0f, 360.0f);
		}
		if (bAlignToNormal)
		{
			// Align Z axis to surface normal
			FQuat NormalQuat = FQuat::FindBetweenNormals(FVector::UpVector, HitNormal);
			FQuat YawQuat(FVector::UpVector, FMath::DegreesToRadians(Rot.Yaw));
			Rot = (NormalQuat * YawQuat).Rotator();
		}
		Instance.Rotation = Rot;

		Instance.Flags = 0;
		Instance.PreAlignRotation = Instance.Rotation;
		Instance.ZOffset = 0.0f;

		NewInstances.Add(Instance);
	}

	// Add all instances to the IFA in one batch
	// Partitioned worlds add per grid cell. Validation runs first (the whole call fails, nothing
	// placed and no asset written, when a target cell's foliage actor exists but is not loaded, or a candidate was
	// dropped inside such a cell); only then is the foliage type created / saved if needed and the transaction opened.
	if (bPartitioned)
	{
		if (!VibeUEFoliageWP::ValidatePartitionedPlacement(World, NewInstances, TraceMisses, Result.ErrorMessage))
		{
			UE_LOG(LogTemp, Error, TEXT("UFoliageService::ScatterInternal: %s"), *Result.ErrorMessage);
			return Result;
		}

		if (NewInstances.Num() > 0)
		{
			UFoliageType* PlacementType = VibeUEFoliageWP::FinalizePlacementFoliageType(PartitionedType, Result.ErrorMessage);
			if (!PlacementType)
			{
				UE_LOG(LogTemp, Error, TEXT("UFoliageService::ScatterInternal: %s"), *Result.ErrorMessage);
				return Result;
			}

			FScopedTransaction PartitionedTransaction(NSLOCTEXT("FoliageService", "ScatterFoliage", "Scatter Foliage"));
			if (!VibeUEFoliageWP::ApplyPartitionedPlacement(World, PlacementType, NewInstances, Result.InstancesAdded, Result.ErrorMessage))
			{
				UE_LOG(LogTemp, Error, TEXT("UFoliageService::ScatterInternal: %s"), *Result.ErrorMessage);
				return Result;
			}
		}
	}
	else if (NewInstances.Num() > 0)
	{
		FFoliageInfo* FoliageInfo = IFA->FindInfo(FoliageType);
		if (FoliageInfo)
		{
			TArray<const FFoliageInstance*> InstancePtrs;
			InstancePtrs.Reserve(NewInstances.Num());
			for (const FFoliageInstance& Inst : NewInstances)
			{
				InstancePtrs.Add(&Inst);
			}
			FoliageInfo->AddInstances(FoliageType, InstancePtrs);
			Result.InstancesAdded = NewInstances.Num();
		}
		else
		{
			Result.ErrorMessage = TEXT("Failed to find FoliageInfo after registering type");
			return Result;
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("UFoliageService::ScatterInternal: Placed %d/%d instances (%d rejected) for '%s'"),
		Result.InstancesAdded, Result.InstancesRequested, Result.InstancesRejected, *MeshOrFoliageTypePath);

	return Result;
}

// =================================================================
// Poisson Disk Sampling Helper
// =================================================================

static TArray<FVector2D> GeneratePoissonDiskSamples(
	float MinX, float MinY, float MaxX, float MaxY,
	int32 Count, FRandomStream& RNG,
	float MinDistance = 0.0f)
{
	TArray<FVector2D> Result;

	if (Count <= 0)
	{
		return Result;
	}

	float Width = MaxX - MinX;
	float Height = MaxY - MinY;

	if (Width <= 0.0f || Height <= 0.0f)
	{
		return Result;
	}

	// Calculate minimum distance between points based on area and count
	if (MinDistance <= 0.0f)
	{
		float Area = Width * Height;
		// Approximate: each point gets Area/Count space, min distance is ~0.7 of the side of that square
		MinDistance = FMath::Sqrt(Area / static_cast<float>(Count)) * 0.7f;
	}

	// Cell size for spatial grid
	float CellSize = MinDistance / FMath::Sqrt(2.0f);
	int32 GridW = FMath::CeilToInt(Width / CellSize);
	int32 GridH = FMath::CeilToInt(Height / CellSize);

	// Grid stores index into Result array, -1 = empty
	TArray<int32> Grid;
	Grid.SetNumUninitialized(GridW * GridH);
	for (int32 i = 0; i < Grid.Num(); i++)
	{
		Grid[i] = -1;
	}

	// Start with a random point
	FVector2D Initial(
		RNG.FRandRange(MinX, MaxX),
		RNG.FRandRange(MinY, MaxY));
	Result.Add(Initial);

	int32 GX = FMath::Clamp(FMath::FloorToInt((Initial.X - MinX) / CellSize), 0, GridW - 1);
	int32 GY = FMath::Clamp(FMath::FloorToInt((Initial.Y - MinY) / CellSize), 0, GridH - 1);
	Grid[GY * GridW + GX] = 0;

	TArray<int32> ActiveList;
	ActiveList.Add(0);

	int32 MaxAttempts = 30;

	while (ActiveList.Num() > 0 && Result.Num() < Count)
	{
		int32 ActiveIdx = RNG.RandRange(0, ActiveList.Num() - 1);
		int32 PointIdx = ActiveList[ActiveIdx];
		FVector2D Point = Result[PointIdx];

		bool bFound = false;
		for (int32 Attempt = 0; Attempt < MaxAttempts; Attempt++)
		{
			float Angle = RNG.FRandRange(0.0f, 2.0f * PI);
			float Dist = RNG.FRandRange(MinDistance, MinDistance * 2.0f);
			FVector2D Candidate(
				Point.X + Dist * FMath::Cos(Angle),
				Point.Y + Dist * FMath::Sin(Angle));

			// Bounds check
			if (Candidate.X < MinX || Candidate.X > MaxX ||
				Candidate.Y < MinY || Candidate.Y > MaxY)
			{
				continue;
			}

			int32 CandGX = FMath::Clamp(FMath::FloorToInt((Candidate.X - MinX) / CellSize), 0, GridW - 1);
			int32 CandGY = FMath::Clamp(FMath::FloorToInt((Candidate.Y - MinY) / CellSize), 0, GridH - 1);

			// Check neighbors in a 5x5 grid around the candidate
			bool bTooClose = false;
			for (int32 NY = FMath::Max(0, CandGY - 2); NY <= FMath::Min(GridH - 1, CandGY + 2) && !bTooClose; NY++)
			{
				for (int32 NX = FMath::Max(0, CandGX - 2); NX <= FMath::Min(GridW - 1, CandGX + 2) && !bTooClose; NX++)
				{
					int32 NeighborIdx = Grid[NY * GridW + NX];
					if (NeighborIdx >= 0)
					{
						float D = FVector2D::Distance(Candidate, Result[NeighborIdx]);
						if (D < MinDistance)
						{
							bTooClose = true;
						}
					}
				}
			}

			if (!bTooClose)
			{
				int32 NewIdx = Result.Num();
				Result.Add(Candidate);
				Grid[CandGY * GridW + CandGX] = NewIdx;
				ActiveList.Add(NewIdx);
				bFound = true;
				break;
			}
		}

		if (!bFound)
		{
			ActiveList.RemoveAtSwap(ActiveIdx);
		}
	}

	// If Poisson didn't generate enough points (can happen in tight spaces),
	// fill with random points
	int32 FallbackAttempts = 0;
	while (Result.Num() < Count && FallbackAttempts < Count * 10)
	{
		FallbackAttempts++;
		FVector2D Candidate(
			RNG.FRandRange(MinX, MaxX),
			RNG.FRandRange(MinY, MaxY));
		Result.Add(Candidate);
	}

	return Result;
}

// =================================================================
// Discovery
// =================================================================

TArray<FVibeUEFoliageTypeInfo> UFoliageService::ListFoliageTypes()
{
	TArray<FVibeUEFoliageTypeInfo> Result;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UFoliageService::ListFoliageTypes: No editor world available"));
		return Result;
	}

	// A partitioned world has one foliage actor per grid cell; list each foliage type once with its
	// instances summed over every loaded cell, and log the cells that are not loaded.
	const bool bPartitioned = VibeUEFoliageWP::IsPartitioned(World);
	TMap<UFoliageType*, int32> RowOfType;

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		if (bPartitioned && VibeUEFoliageWP::IsTransientFoliageActor(IFA)) // Foliage mode palette holder
		{
			continue;
		}
		const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();

		for (const auto& Pair : FoliageInfos)
		{
			UFoliageType* FT = Pair.Key;
			const FFoliageInfo& Info = Pair.Value.Get();

			// Same type in another cell, add to its row
			if (bPartitioned)
			{
				if (const int32* Row = RowOfType.Find(FT))
				{
					Result[*Row].InstanceCount += Info.Instances.Num();
					continue;
				}
			}

			FVibeUEFoliageTypeInfo TypeInfo;
			TypeInfo.FoliageTypeName = FT->GetName();
			TypeInfo.InstanceCount = Info.Instances.Num();

			if (UFoliageType_InstancedStaticMesh* ISMT = Cast<UFoliageType_InstancedStaticMesh>(FT))
			{
				if (ISMT->GetStaticMesh())
				{
					TypeInfo.MeshPath = ISMT->GetStaticMesh()->GetPathName();
				}
			}

			TypeInfo.FoliageTypePath = FT->GetPathName();
			const int32 NewRow = Result.Add(TypeInfo);
			if (bPartitioned)
			{
				RowOfType.Add(FT, NewRow);
			}
		}
	}

	if (bPartitioned)
	{
		VibeUEFoliageWP::WarnAboutPartialView(World, TEXT("ListFoliageTypes"));
	}

	return Result;
}

int32 UFoliageService::GetInstanceCount(const FString& MeshOrFoliageTypePath)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		return -1;
	}

	// On a partitioned world sum over every loaded cell actor (the first actor holding the type is
	// only one cell there) and log the cells that are not loaded. -1 still means "not found in any loaded actor".
	const bool bPartitioned = VibeUEFoliageWP::IsPartitioned(World);
	int32 PartitionedTotal = -1;

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		if (bPartitioned && VibeUEFoliageWP::IsTransientFoliageActor(IFA)) // Foliage mode palette holder
		{
			continue;
		}
		UFoliageType* FT = FindFoliageTypeInIFA(MeshOrFoliageTypePath, IFA);
		if (FT)
		{
			const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();
			const TUniqueObj<FFoliageInfo>* FoundInfo = FoliageInfos.Find(FT);
			if (FoundInfo)
			{
				if (bPartitioned)
				{
					PartitionedTotal = FMath::Max(PartitionedTotal, 0) + FoundInfo->Get().Instances.Num();
					continue;
				}
				return FoundInfo->Get().Instances.Num();
			}
		}
	}

	if (bPartitioned)
	{
		VibeUEFoliageWP::WarnAboutPartialView(World, TEXT("GetInstanceCount"));
		return PartitionedTotal;
	}

	return -1;
}

// =================================================================
// Foliage Type Management
// =================================================================

FFoliageTypeCreateResult UFoliageService::CreateFoliageType(
	const FString& MeshPath,
	const FString& SavePath,
	const FString& AssetName,
	float MinScale,
	float MaxScale,
	bool bAlignToNormal,
	float AlignToNormalMaxAngle,
	float GroundSlopeMaxAngle,
	float CullDistanceMax)
{
	FFoliageTypeCreateResult Result;

	// Load the static mesh
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Could not load static mesh '%s'"), *MeshPath);
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::CreateFoliageType: %s"), *Result.ErrorMessage);
		return Result;
	}

	// Create the package
	FString FullPath = SavePath / AssetName;
	FString PackageName = FullPath;
	if (!PackageName.StartsWith(TEXT("/")))
	{
		PackageName = TEXT("/") + PackageName;
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to create package at '%s'"), *PackageName);
		return Result;
	}

	// Create the foliage type asset
	UFoliageType_InstancedStaticMesh* FoliageType = NewObject<UFoliageType_InstancedStaticMesh>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);

	if (!FoliageType)
	{
		Result.ErrorMessage = TEXT("Failed to create UFoliageType_InstancedStaticMesh");
		return Result;
	}

	FoliageType->SetStaticMesh(Mesh);

	// Configure properties
	FoliageType->Scaling = EFoliageScaling::Uniform;
	FoliageType->ScaleX = FFloatInterval(MinScale, MaxScale);
	FoliageType->ScaleY = FFloatInterval(MinScale, MaxScale);
	FoliageType->ScaleZ = FFloatInterval(MinScale, MaxScale);
	FoliageType->AlignToNormal = bAlignToNormal;
	FoliageType->AlignMaxAngle = AlignToNormalMaxAngle;
	FoliageType->GroundSlopeAngle = FFloatInterval(0.0f, GroundSlopeMaxAngle);
	FoliageType->CullDistance = FInt32Interval(0, FMath::RoundToInt(CullDistanceMax));
	FoliageType->RandomYaw = true;

	// Mark dirty and save
	FoliageType->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(FoliageType);

	// Save the package
	FString PackageFileName = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	// The default Error = GError makes any
	// save error a fatal appError; with GWarn it is logged and the failure branch below runs.
	SaveArgs.Error = GWarn;
	if (!UPackage::SavePackage(Package, FoliageType, *PackageFileName, SaveArgs))
	{
		Result.ErrorMessage = FString::Printf(TEXT("Failed to save package '%s'"), *PackageName);
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::CreateFoliageType: %s"), *Result.ErrorMessage);
		return Result;
	}

	Result.bSuccess = true;
	Result.AssetPath = FoliageType->GetPathName();

	UE_LOG(LogTemp, Log, TEXT("UFoliageService::CreateFoliageType: Created '%s' with mesh '%s'"),
		*Result.AssetPath, *MeshPath);

	return Result;
}

namespace
{
	// Resolves a property path like "CullDistance" or "CullDistance.Max" to the
	// leaf property and its value address. Intermediate segments must be struct
	// properties; returns nullptr if any segment is missing or not a struct.
	FProperty* ResolveFoliageTypePropertyPath(UFoliageType* FoliageType, const FString& PropertyPath, void*& OutValueAddr)
	{
		TArray<FString> Segments;
		PropertyPath.ParseIntoArray(Segments, TEXT("."));

		UStruct* OwnerType = FoliageType->GetClass();
		void* Container = FoliageType;

		for (int32 i = 0; i < Segments.Num(); i++)
		{
			FProperty* Property = OwnerType->FindPropertyByName(FName(*Segments[i]));
			if (!Property)
			{
				return nullptr;
			}

			void* ValueAddr = Property->ContainerPtrToValuePtr<void>(Container);
			if (i == Segments.Num() - 1)
			{
				OutValueAddr = ValueAddr;
				return Property;
			}

			FStructProperty* StructProperty = CastField<FStructProperty>(Property);
			if (!StructProperty)
			{
				return nullptr;
			}
			OwnerType = StructProperty->Struct;
			Container = ValueAddr;
		}

		return nullptr;
	}
}

bool UFoliageService::SetFoliageTypeProperty(
	const FString& FoliageTypePath,
	const FString& PropertyName,
	const FString& Value)
{
	UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
	if (!FoliageType)
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::SetFoliageTypeProperty: Could not load '%s'"), *FoliageTypePath);
		return false;
	}

	void* PropertyAddr = nullptr;
	FProperty* Property = ResolveFoliageTypePropertyPath(FoliageType, PropertyName, PropertyAddr);
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::SetFoliageTypeProperty: Property path '%s' not found on UFoliageType (nested struct members use dots, e.g. 'CullDistance.Max')"), *PropertyName);
		return false;
	}

	if (!Property->ImportText_Direct(*Value, PropertyAddr, FoliageType, PPF_None))
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::SetFoliageTypeProperty: Failed to set '%s' to '%s'"), *PropertyName, *Value);
		return false;
	}

	FoliageType->MarkPackageDirty();
	return true;
}

FString UFoliageService::GetFoliageTypeProperty(
	const FString& FoliageTypePath,
	const FString& PropertyName)
{
	UFoliageType* FoliageType = LoadObject<UFoliageType>(nullptr, *FoliageTypePath);
	if (!FoliageType)
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::GetFoliageTypeProperty: Could not load '%s'"), *FoliageTypePath);
		return FString();
	}

	void* PropertyAddr = nullptr;
	FProperty* Property = ResolveFoliageTypePropertyPath(FoliageType, PropertyName, PropertyAddr);
	if (!Property)
	{
		UE_LOG(LogTemp, Error, TEXT("UFoliageService::GetFoliageTypeProperty: Property path '%s' not found (nested struct members use dots, e.g. 'CullDistance.Max')"), *PropertyName);
		return FString();
	}

	FString Result;
	Property->ExportTextItem_Direct(Result, PropertyAddr, nullptr, FoliageType, PPF_None);
	return Result;
}

// =================================================================
// Placement
// =================================================================

FFoliageScatterResult UFoliageService::ScatterFoliage(
	const FString& MeshOrFoliageTypePath,
	float WorldCenterX,
	float WorldCenterY,
	float Radius,
	int32 Count,
	float MinScale,
	float MaxScale,
	bool bAlignToNormal,
	bool bRandomYaw,
	int32 Seed,
	const FString& LandscapeNameOrLabel)
{
	if (Count <= 0)
	{
		FFoliageScatterResult Result;
		Result.bSuccess = true;
		Result.InstancesRequested = 0;
		return Result;
	}

	if (Radius <= 0.0f)
	{
		FFoliageScatterResult Result;
		Result.ErrorMessage = TEXT("Radius must be > 0");
		return Result;
	}

	FRandomStream RNG(Seed != 0 ? Seed : FMath::Rand());

	// Generate Poisson disk samples within the bounding box, then filter to circle
	TArray<FVector2D> AllSamples = GeneratePoissonDiskSamples(
		WorldCenterX - Radius, WorldCenterY - Radius,
		WorldCenterX + Radius, WorldCenterY + Radius,
		Count * 2, // Over-generate since we'll filter to circle
		RNG);

	// Filter to circular region
	TArray<FVector2D> CircleSamples;
	CircleSamples.Reserve(Count);
	float RadiusSq = Radius * Radius;
	for (const FVector2D& Sample : AllSamples)
	{
		float DX = Sample.X - WorldCenterX;
		float DY = Sample.Y - WorldCenterY;
		if (DX * DX + DY * DY <= RadiusSq)
		{
			CircleSamples.Add(Sample);
		}
	}

	return ScatterInternal(MeshOrFoliageTypePath, CircleSamples, Count,
		MinScale, MaxScale, bAlignToNormal, bRandomYaw, Seed, LandscapeNameOrLabel);
}

FFoliageScatterResult UFoliageService::ScatterFoliageRect(
	const FString& MeshOrFoliageTypePath,
	float WorldMinX,
	float WorldMinY,
	float WorldMaxX,
	float WorldMaxY,
	int32 Count,
	float MinScale,
	float MaxScale,
	bool bAlignToNormal,
	bool bRandomYaw,
	int32 Seed,
	const FString& LandscapeNameOrLabel)
{
	if (Count <= 0)
	{
		FFoliageScatterResult Result;
		Result.bSuccess = true;
		Result.InstancesRequested = 0;
		return Result;
	}

	FRandomStream RNG(Seed != 0 ? Seed : FMath::Rand());

	TArray<FVector2D> Samples = GeneratePoissonDiskSamples(
		WorldMinX, WorldMinY, WorldMaxX, WorldMaxY,
		Count, RNG);

	return ScatterInternal(MeshOrFoliageTypePath, Samples, Count,
		MinScale, MaxScale, bAlignToNormal, bRandomYaw, Seed, LandscapeNameOrLabel);
}

FFoliageScatterResult UFoliageService::AddFoliageInstances(
	const FString& MeshOrFoliageTypePath,
	const TArray<FVector>& Locations,
	float MinScale,
	float MaxScale,
	bool bAlignToNormal,
	bool bRandomYaw,
	bool bTraceToSurface)
{
	FFoliageScatterResult Result;
	Result.InstancesRequested = Locations.Num();

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	// A partitioned world has no single foliage actor; the type must be an asset and the instances go
	// to the actor of their grid cell (VibeUEFoliageWP::ValidatePartitionedPlacement / ApplyPartitionedPlacement below).
	const bool bPartitioned = VibeUEFoliageWP::IsPartitioned(World);
	AInstancedFoliageActor* IFA = nullptr;
	UFoliageType* FoliageType = nullptr;
	VibeUEFoliageWP::FPlacementFoliageType PartitionedType;

	if (bPartitioned)
	{
		if (!VibeUEFoliageWP::ResolvePlacementFoliageType(World, MeshOrFoliageTypePath, PartitionedType, Result.ErrorMessage))
		{
			UE_LOG(LogTemp, Error, TEXT("UFoliageService::AddFoliageInstances: %s"), *Result.ErrorMessage);
			return Result;
		}
	}
	else
	{
		IFA = GetOrCreateFoliageActor(World);
		if (!IFA)
		{
			Result.ErrorMessage = TEXT("Failed to get or create InstancedFoliageActor");
			return Result;
		}

		FoliageType = FindOrCreateFoliageTypeForMesh(MeshOrFoliageTypePath, IFA);
		if (!FoliageType)
		{
			Result.ErrorMessage = FString::Printf(TEXT("Could not load or create foliage type for '%s'"), *MeshOrFoliageTypePath);
			return Result;
		}
	}

	// Partitioned worlds open their transaction only after validation (see below), never cancelling one
	FScopedTransaction Transaction(NSLOCTEXT("FoliageService", "AddFoliageInstances", "Add Foliage Instances"), !bPartitioned);
	if (IFA) // null on partitioned worlds, the cell actors are modified when instances are added
	{
		IFA->Modify();
	}

	FRandomStream RNG(FMath::Rand());
	TArray<FFoliageInstance> NewInstances;
	NewInstances.Reserve(Locations.Num());
	TArray<FVector2D> TraceMisses; // checked against unloaded cells on partitioned worlds

	for (const FVector& Location : Locations)
	{
		FVector FinalLocation = Location;
		FVector SurfaceNormal = FVector::UpVector;

		if (bTraceToSurface)
		{
			FVector HitLocation, HitNormal;
			if (TraceToSurface(World, Location.X, Location.Y, HitLocation, HitNormal))
			{
				FinalLocation = HitLocation;
				SurfaceNormal = HitNormal;
			}
			else
			{
				Result.InstancesRejected++;
				if (bPartitioned)
				{
					TraceMisses.Add(FVector2D(Location.X, Location.Y));
				}
				continue;
			}
		}

		FFoliageInstance Instance;
		Instance.Location = FinalLocation;

		float Scale = RNG.FRandRange(MinScale, MaxScale);
		Instance.DrawScale3D = FVector3f(Scale, Scale, Scale);

		FRotator Rot = FRotator::ZeroRotator;
		if (bRandomYaw)
		{
			Rot.Yaw = RNG.FRandRange(0.0f, 360.0f);
		}
		if (bAlignToNormal)
		{
			FQuat NormalQuat = FQuat::FindBetweenNormals(FVector::UpVector, SurfaceNormal);
			FQuat YawQuat(FVector::UpVector, FMath::DegreesToRadians(Rot.Yaw));
			Rot = (NormalQuat * YawQuat).Rotator();
		}
		Instance.Rotation = Rot;
		Instance.PreAlignRotation = Instance.Rotation;
		Instance.Flags = 0;
		Instance.ZOffset = 0.0f;

		NewInstances.Add(Instance);
	}

	// Partitioned worlds add per grid cell. Validation runs first (the whole call fails, nothing
	// placed and no asset written, when a target cell's foliage actor exists but is not loaded, or a traced location
	// found no surface inside such a cell); only then is the foliage type created / saved if needed and the
	// transaction opened.
	if (bPartitioned)
	{
		if (!VibeUEFoliageWP::ValidatePartitionedPlacement(World, NewInstances, TraceMisses, Result.ErrorMessage))
		{
			UE_LOG(LogTemp, Error, TEXT("UFoliageService::AddFoliageInstances: %s"), *Result.ErrorMessage);
			return Result;
		}

		if (NewInstances.Num() > 0)
		{
			UFoliageType* PlacementType = VibeUEFoliageWP::FinalizePlacementFoliageType(PartitionedType, Result.ErrorMessage);
			if (!PlacementType)
			{
				UE_LOG(LogTemp, Error, TEXT("UFoliageService::AddFoliageInstances: %s"), *Result.ErrorMessage);
				return Result;
			}

			FScopedTransaction PartitionedTransaction(NSLOCTEXT("FoliageService", "AddFoliageInstances", "Add Foliage Instances"));
			if (!VibeUEFoliageWP::ApplyPartitionedPlacement(World, PlacementType, NewInstances, Result.InstancesAdded, Result.ErrorMessage))
			{
				UE_LOG(LogTemp, Error, TEXT("UFoliageService::AddFoliageInstances: %s"), *Result.ErrorMessage);
				return Result;
			}
		}
	}
	else if (NewInstances.Num() > 0)
	{
		FFoliageInfo* FoliageInfo = IFA->FindInfo(FoliageType);
		if (FoliageInfo)
		{
			TArray<const FFoliageInstance*> InstancePtrs;
			InstancePtrs.Reserve(NewInstances.Num());
			for (const FFoliageInstance& Inst : NewInstances)
			{
				InstancePtrs.Add(&Inst);
			}
			FoliageInfo->AddInstances(FoliageType, InstancePtrs);
			Result.InstancesAdded = NewInstances.Num();
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("UFoliageService::AddFoliageInstances: Placed %d/%d instances for '%s'"),
		Result.InstancesAdded, Result.InstancesRequested, *MeshOrFoliageTypePath);

	return Result;
}

// =================================================================
// Layer-Aware Placement
// =================================================================

FFoliageScatterResult UFoliageService::ScatterFoliageOnLayer(
	const FString& MeshOrFoliageTypePath,
	const FString& LandscapeNameOrLabel,
	const FString& LayerName,
	int32 Count,
	float MinScale,
	float MaxScale,
	float LayerWeightThreshold,
	bool bAlignToNormal,
	bool bRandomYaw,
	int32 Seed)
{
	if (Count <= 0)
	{
		FFoliageScatterResult Result;
		Result.bSuccess = true;
		Result.InstancesRequested = 0;
		return Result;
	}

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		FFoliageScatterResult Result;
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	// Find the landscape to determine scatter bounds
	ALandscape* Landscape = nullptr;
	for (TActorIterator<ALandscape> It(World); It; ++It)
	{
		ALandscape* L = *It;
		if (L->GetActorLabel().Equals(LandscapeNameOrLabel, ESearchCase::IgnoreCase) ||
			L->GetName().Equals(LandscapeNameOrLabel, ESearchCase::IgnoreCase))
		{
			Landscape = L;
			break;
		}
	}

	if (!Landscape)
	{
		FFoliageScatterResult Result;
		Result.ErrorMessage = FString::Printf(TEXT("Landscape '%s' not found"), *LandscapeNameOrLabel);
		return Result;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		FFoliageScatterResult Result;
		Result.ErrorMessage = TEXT("No landscape info available");
		return Result;
	}

	// Get landscape bounds in world space
	int32 MinLX, MinLY, MaxLX, MaxLY;
	if (!LandscapeInfo->GetLandscapeExtent(MinLX, MinLY, MaxLX, MaxLY))
	{
		FFoliageScatterResult Result;
		Result.ErrorMessage = TEXT("Could not get landscape extent");
		return Result;
	}

	FVector LandscapeLocation = Landscape->GetActorLocation();
	FVector LandscapeScale = Landscape->GetActorScale3D();

	float WorldMinX = LandscapeLocation.X + MinLX * LandscapeScale.X;
	float WorldMinY = LandscapeLocation.Y + MinLY * LandscapeScale.Y;
	float WorldMaxX = LandscapeLocation.X + MaxLX * LandscapeScale.X;
	float WorldMaxY = LandscapeLocation.Y + MaxLY * LandscapeScale.Y;

	FRandomStream RNG(Seed != 0 ? Seed : FMath::Rand());

	// Over-generate candidates since many will be rejected by layer weight check
	int32 OverGenerateCount = Count * 4;
	TArray<FVector2D> Samples = GeneratePoissonDiskSamples(
		WorldMinX, WorldMinY, WorldMaxX, WorldMaxY,
		OverGenerateCount, RNG);

	return ScatterInternal(MeshOrFoliageTypePath, Samples, Count,
		MinScale, MaxScale, bAlignToNormal, bRandomYaw, Seed, LandscapeNameOrLabel,
		LayerName, LayerWeightThreshold);
}

// =================================================================
// Removal
// =================================================================

FFoliageRemoveResult UFoliageService::RemoveFoliageInRadius(
	const FString& MeshOrFoliageTypePath,
	float WorldCenterX,
	float WorldCenterY,
	float Radius)
{
	FFoliageRemoveResult Result;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	float RadiusSq = Radius * Radius;

	// On a partitioned world refuse (nothing removed) when a foliage actor overlapping the circle is
	// not loaded, and make the removal over all cells one undo step.
	const bool bPartitioned = VibeUEFoliageWP::IsPartitioned(World);
	if (bPartitioned)
	{
		const TArray<VibeUEFoliageWP::FUnloadedFoliageActor> Unloaded =
			VibeUEFoliageWP::CollectUnloadedFoliageActorsInCircle(World, WorldCenterX, WorldCenterY, Radius);
		if (Unloaded.Num() > 0)
		{
			Result.ErrorMessage = FString::Printf(TEXT("World Partition: %d foliage actor(s) overlapping this circle are not loaded (%s). Load them and retry. Nothing was removed."),
				Unloaded.Num(), *VibeUEFoliageWP::DescribeUnloaded(Unloaded));
			UE_LOG(LogTemp, Error, TEXT("UFoliageService::RemoveFoliageInRadius: %s"), *Result.ErrorMessage);
			return Result;
		}
	}
	FScopedTransaction PartitionedTransaction(NSLOCTEXT("FoliageService", "RemoveFoliageInRadius", "Remove Foliage In Radius"), bPartitioned);

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		UFoliageType* FT = FindFoliageTypeInIFA(MeshOrFoliageTypePath, IFA);
		if (!FT)
		{
			continue;
		}

		FFoliageInfo* InfoPtr = IFA->FindInfo(FT);
		if (!InfoPtr)
		{
			continue;
		}

		// Leave cells with nothing to remove untouched (no Modify, so their actor packages stay clean)
		if (bPartitioned && !VibeUEFoliageWP::HasInstanceInCircleXY(*InfoPtr, WorldCenterX, WorldCenterY, RadiusSq))
		{
			continue;
		}

		FScopedTransaction Transaction(NSLOCTEXT("FoliageService", "RemoveFoliageInRadius", "Remove Foliage In Radius"));
		IFA->Modify();

		FFoliageInfo& Info = *InfoPtr;
		TArray<int32> IndicesToRemove;

		for (int32 i = 0; i < Info.Instances.Num(); i++)
		{
			const FFoliageInstance& Instance = Info.Instances[i];
			float DX = Instance.Location.X - WorldCenterX;
			float DY = Instance.Location.Y - WorldCenterY;
			if (DX * DX + DY * DY <= RadiusSq)
			{
				IndicesToRemove.Add(i);
			}
		}

		if (IndicesToRemove.Num() > 0)
		{
			Info.RemoveInstances(IndicesToRemove, true);
			Result.InstancesRemoved += IndicesToRemove.Num();
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("UFoliageService::RemoveFoliageInRadius: Removed %d instances of '%s' in radius %.0f at (%.0f, %.0f)"),
		Result.InstancesRemoved, *MeshOrFoliageTypePath, Radius, WorldCenterX, WorldCenterY);

	return Result;
}

FFoliageRemoveResult UFoliageService::RemoveAllFoliageOfType(
	const FString& MeshOrFoliageTypePath)
{
	FFoliageRemoveResult Result;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	// "all" cannot be honoured on a partitioned world while some foliage actors are not loaded:
	// refuse (nothing removed) and make the removal over all cells one undo step.
	const bool bPartitioned = VibeUEFoliageWP::IsPartitioned(World);
	if (bPartitioned)
	{
		const TArray<VibeUEFoliageWP::FUnloadedFoliageActor> Unloaded = VibeUEFoliageWP::CollectUnloadedFoliageActors(World);
		if (Unloaded.Num() > 0)
		{
			Result.ErrorMessage = FString::Printf(TEXT("World Partition: %d foliage actor(s) are not loaded (%s). Load them (e.g. the whole level) and retry. Nothing was removed."),
				Unloaded.Num(), *VibeUEFoliageWP::DescribeUnloaded(Unloaded));
			UE_LOG(LogTemp, Error, TEXT("UFoliageService::RemoveAllFoliageOfType: %s"), *Result.ErrorMessage);
			return Result;
		}
	}
	FScopedTransaction PartitionedTransaction(NSLOCTEXT("FoliageService", "RemoveAllOfType", "Remove All Foliage Of Type"), bPartitioned);

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		UFoliageType* FT = FindFoliageTypeInIFA(MeshOrFoliageTypePath, IFA);
		if (!FT)
		{
			continue;
		}

		FFoliageInfo* InfoPtr = IFA->FindInfo(FT);
		if (!InfoPtr)
		{
			continue;
		}

		// Leave cells with nothing to remove untouched (no Modify, so their actor packages stay clean)
		if (bPartitioned && InfoPtr->Instances.Num() == 0)
		{
			continue;
		}

		FScopedTransaction Transaction(NSLOCTEXT("FoliageService", "RemoveAllOfType", "Remove All Foliage Of Type"));
		IFA->Modify();

		FFoliageInfo& Info = *InfoPtr;
		int32 Count = Info.Instances.Num();

		TArray<int32> AllIndices;
		AllIndices.Reserve(Count);
		for (int32 i = 0; i < Count; i++)
		{
			AllIndices.Add(i);
		}

		if (AllIndices.Num() > 0)
		{
			Info.RemoveInstances(AllIndices, true);
			Result.InstancesRemoved += Count;
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("UFoliageService::RemoveAllFoliageOfType: Removed %d instances of '%s'"),
		Result.InstancesRemoved, *MeshOrFoliageTypePath);

	return Result;
}

FFoliageRemoveResult UFoliageService::ClearAllFoliage()
{
	FFoliageRemoveResult Result;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	// "all" cannot be honoured on a partitioned world while some foliage actors are not loaded:
	// refuse (nothing removed) and make the clear over all cells one undo step.
	const bool bPartitioned = VibeUEFoliageWP::IsPartitioned(World);
	if (bPartitioned)
	{
		const TArray<VibeUEFoliageWP::FUnloadedFoliageActor> Unloaded = VibeUEFoliageWP::CollectUnloadedFoliageActors(World);
		if (Unloaded.Num() > 0)
		{
			Result.ErrorMessage = FString::Printf(TEXT("World Partition: %d foliage actor(s) are not loaded (%s). Load them (e.g. the whole level) and retry. Nothing was removed."),
				Unloaded.Num(), *VibeUEFoliageWP::DescribeUnloaded(Unloaded));
			UE_LOG(LogTemp, Error, TEXT("UFoliageService::ClearAllFoliage: %s"), *Result.ErrorMessage);
			return Result;
		}
	}
	FScopedTransaction PartitionedTransaction(NSLOCTEXT("FoliageService", "ClearAllFoliage", "Clear All Foliage"), bPartitioned);

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;

		// Leave empty cells (and Foliage mode's palette holder) untouched (no Modify, no dirty package)
		if (bPartitioned && VibeUEFoliageWP::CountInstances(IFA) == 0)
		{
			continue;
		}

		FScopedTransaction Transaction(NSLOCTEXT("FoliageService", "ClearAllFoliage", "Clear All Foliage"));
		IFA->Modify();

		// Collect all types first (can't modify while iterating const map)
		TArray<UFoliageType*> TypesToClear;
		{
			const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();
			for (const auto& Pair : FoliageInfos)
			{
				TypesToClear.Add(Pair.Key);
			}
		}

		for (UFoliageType* FT : TypesToClear)
		{
			FFoliageInfo* InfoPtr = IFA->FindInfo(FT);
			if (InfoPtr)
			{
				int32 Count = InfoPtr->Instances.Num();

				TArray<int32> AllIndices;
				AllIndices.Reserve(Count);
				for (int32 i = 0; i < Count; i++)
				{
					AllIndices.Add(i);
				}

				if (AllIndices.Num() > 0)
				{
					InfoPtr->RemoveInstances(AllIndices, true);
					Result.InstancesRemoved += Count;
				}
			}
		}
	}

	Result.bSuccess = true;
	UE_LOG(LogTemp, Log, TEXT("UFoliageService::ClearAllFoliage: Removed %d total instances"),
		Result.InstancesRemoved);

	return Result;
}

// =================================================================
// Query
// =================================================================

FFoliageQueryResult UFoliageService::GetFoliageInRadius(
	const FString& MeshOrFoliageTypePath,
	float WorldCenterX,
	float WorldCenterY,
	float Radius,
	int32 MaxResults)
{
	FFoliageQueryResult Result;

	UWorld* World = GetEditorWorld();
	if (!World)
	{
		Result.ErrorMessage = TEXT("No editor world available");
		return Result;
	}

	float RadiusSq = Radius * Radius;

	// On a partitioned world TotalInstances sums over every loaded cell actor (VibeUE resets it per
	// actor, which only works with one actor), and foliage actors overlapping the circle that are not loaded are
	// reported (bSuccess = false, ErrorMessage) instead of silently left out.
	const bool bPartitioned = VibeUEFoliageWP::IsPartitioned(World);

	for (TActorIterator<AInstancedFoliageActor> It(World); It; ++It)
	{
		AInstancedFoliageActor* IFA = *It;
		UFoliageType* FT = FindFoliageTypeInIFA(MeshOrFoliageTypePath, IFA);
		if (!FT)
		{
			continue;
		}

		const TMap<UFoliageType*, TUniqueObj<FFoliageInfo>>& FoliageInfos = IFA->GetFoliageInfos();
		const TUniqueObj<FFoliageInfo>* FoundInfo = FoliageInfos.Find(FT);
		if (!FoundInfo)
		{
			continue;
		}

		const FFoliageInfo& Info = FoundInfo->Get();
		if (!bPartitioned) // VibeUE's per-actor reset, kept as-is for non-partitioned worlds
		{
			Result.TotalInstances = 0;
		}

		for (int32 i = 0; i < Info.Instances.Num(); i++)
		{
			const FFoliageInstance& Instance = Info.Instances[i];
			float DX = Instance.Location.X - WorldCenterX;
			float DY = Instance.Location.Y - WorldCenterY;
			if (DX * DX + DY * DY <= RadiusSq)
			{
				Result.TotalInstances++;
				if (Result.Instances.Num() < MaxResults)
				{
					FFoliageInstanceInfo InstInfo;
					InstInfo.Location = Instance.Location;
					InstInfo.Rotation = Instance.Rotation;
					InstInfo.Scale = FVector(Instance.DrawScale3D.X, Instance.DrawScale3D.Y, Instance.DrawScale3D.Z);
					InstInfo.InstanceIndex = i;
					Result.Instances.Add(InstInfo);
				}
			}
		}
	}

	// Partial view on a partitioned world is an explicit failure (the loaded part is still returned)
	if (bPartitioned)
	{
		const TArray<VibeUEFoliageWP::FUnloadedFoliageActor> Unloaded =
			VibeUEFoliageWP::CollectUnloadedFoliageActorsInCircle(World, WorldCenterX, WorldCenterY, Radius);
		if (Unloaded.Num() > 0)
		{
			Result.ErrorMessage = FString::Printf(TEXT("World Partition: partial result, %d foliage actor(s) overlapping this circle are not loaded (%s); TotalInstances and Instances cover the loaded cells only. Load them and query again."),
				Unloaded.Num(), *VibeUEFoliageWP::DescribeUnloaded(Unloaded));
			UE_LOG(LogTemp, Warning, TEXT("UFoliageService::GetFoliageInRadius: %s"), *Result.ErrorMessage);
			return Result;
		}
	}

	Result.bSuccess = true;
	return Result;
}

// =================================================================
// Existence Checks
// =================================================================

bool UFoliageService::FoliageTypeExists(const FString& AssetPath)
{
	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Asset)
	{
		return false;
	}
	return Cast<UFoliageType>(Asset) != nullptr || Cast<UStaticMesh>(Asset) != nullptr;
}

bool UFoliageService::HasFoliageInstances(const FString& MeshOrFoliageTypePath)
{
	int32 Count = GetInstanceCount(MeshOrFoliageTypePath);
	return Count > 0;
}
