// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Tools/PythonTools.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Core/ErrorCodes.h"
#include "HAL/PlatformProcess.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h" // TObjectIterator<UWorld> for the resident-map check
#include "Engine/World.h"
#include "Engine/Level.h"          // ULevel::OwningWorld, for the sublevel filter in GetResidentMaps
#include "Engine/LevelStreaming.h" // ULevelStreaming::GetLoadedLevel
#include "FileHelpers.h" // FEditorFileUtils + UEditorLoadingAndSavingUtils (headless SavePackages)

#include "WorldPartition/WorldPartition.h"

DEFINE_LOG_CATEGORY_STATIC(LogPythonTools, Log, All);

TArray<FString> UPythonTools::GetResidentMapWorlds()
{
	// A map opened as an ASSET (unreal.load_asset("/Game/Maps/Foo"), EditorAssetLibrary.load_asset,
	// find_object, ...) stays resident afterwards, and a resident map that is not the one currently
	// open makes the engine's own "old level package cleaned up?" check fail the NEXT time any level
	// is loaded. That check is a fatal, not a warning:
	//
	//   EditorServer.cpp:2544  World Memory Leaks: N leaks objects and packages
	//   LogEditorServer: Error: Old level package /Game/Maps/Foo not cleaned up by garbage collection
	//
	// The crash therefore lands minutes later, on whoever calls load_level next, with nothing in the
	// message pointing at the script that actually caused it. Listing the stragglers in the reply of
	// the run that created them turns that into an immediate, attributable warning.
	TArray<FString> Resident;

	if (!GEditor)
	{
		return Resident;
	}

	const UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

	for (TObjectIterator<UWorld> It; It; ++It)
	{
		UWorld* World = *It;
		if (!World || World == EditorWorld || !IsValid(World))
		{
			continue;
		}

		// Only worlds that came from a real map package can block a level load. This drops the
		// transient preview worlds the editor legitimately keeps alive in numbers (Blueprint,
		// material and thumbnail previews all live in the transient package), and PIE worlds, whose
		// lifetime EndPlayMap owns.
		const UPackage* Package = World->GetPackage();
		if (!Package || Package == GetTransientPackage())
		{
			continue;
		}
		if (World->WorldType != EWorldType::Editor && World->WorldType != EWorldType::Inactive)
		{
			continue;
		}
		if (!FPackageName::IsValidLongPackageName(Package->GetName()))
		{
			continue;
		}

		Resident.AddUnique(World->GetPathName());
	}

	return Resident;
}

TArray<FString> UPythonTools::GetResidentMaps()
{
	// Narrower than the resident_maps reply field (which keeps GetResidentMapWorlds' rule unchanged):
	// a legacy streaming sublevel of the open map is a separate map package that is resident by design,
	// and the level change unloads it with its persistent world. Reporting it here would make
	// vibeue.safe_load_level refuse forever on any map that has sublevels.
	TArray<FString> Resident = GetResidentMapWorlds();
	const UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!EditorWorld || Resident.Num() == 0)
	{
		return Resident;
	}

	TSet<FString> Sublevels;
	for (const ULevelStreaming* Streaming : EditorWorld->GetStreamingLevels())
	{
		const ULevel* Loaded = Streaming ? Streaming->GetLoadedLevel() : nullptr;
		const UWorld* SublevelWorld = Loaded ? Loaded->GetTypedOuter<UWorld>() : nullptr;
		if (SublevelWorld && SublevelWorld != EditorWorld)
		{
			Sublevels.Add(SublevelWorld->GetPathName());
		}
	}
	for (TObjectIterator<UWorld> It; It; ++It)
	{
		const UWorld* World = *It;
		if (World && World != EditorWorld && World->PersistentLevel && World->PersistentLevel->OwningWorld == EditorWorld)
		{
			Sublevels.Add(World->GetPathName());
		}
	}
	Resident.RemoveAll([&Sublevels](const FString& Path) { return Sublevels.Contains(Path); });
	return Resident;
}

TArray<FString> UPythonTools::GetDirtyPackages(bool bIncludeMaps)
{
	TArray<FString> Names;
	if (!GEditor)
	{
		return Names;
	}

	// The same enumeration as the pre-run auto-save sweep, so this is exactly what an auto_save=true
	// call would write.
	TArray<UPackage*> DirtyPackages;
	FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);
	if (bIncludeMaps)
	{
		FEditorFileUtils::GetDirtyWorldPackages(DirtyPackages);
	}

	// A TSet, not AddUnique: a World Partition map can have thousands of dirty external-actor packages,
	// and this runs twice per execute_python_code call.
	TSet<FString> Unique;
	Unique.Reserve(DirtyPackages.Num());
	for (const UPackage* DirtyPackage : DirtyPackages)
	{
		if (DirtyPackage)
		{
			Unique.Add(DirtyPackage->GetName());
		}
	}
	Names = Unique.Array();
	Names.Sort();
	return Names;
}

void UPythonTools::ComputeDirtyPackageDelta(const TArray<FString>& Before, const TArray<FString>& After,
	TArray<FString>& OutDirtied, TArray<FString>& OutCleaned)
{
	OutDirtied.Reset();
	OutCleaned.Reset();

	const TSet<FString> BeforeSet(Before);
	const TSet<FString> AfterSet(After);
	for (const FString& Name : AfterSet)
	{
		if (!BeforeSet.Contains(Name))
		{
			OutDirtied.Add(Name);
		}
	}
	for (const FString& Name : BeforeSet)
	{
		if (!AfterSet.Contains(Name))
		{
			OutCleaned.Add(Name);
		}
	}
	OutDirtied.Sort();
	OutCleaned.Sort();
}
