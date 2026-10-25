// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UAnimMontageService.h"
#include "PythonAPI/UAssetDiscoveryService.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Editor.h"
#include "Engine/World.h"

namespace
{
	/** Any skeleton the project has: the test authors a throwaway montage against it. */
	FString FindAnySkeletonPath()
	{
		FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> Skeletons;
		Registry.Get().GetAssetsByClass(USkeleton::StaticClass()->GetClassPathName(), Skeletons, true);
		for (const FAssetData& Data : Skeletons)
		{
			const FString Path = Data.GetObjectPathString();
			if (Path.StartsWith(TEXT("/Game/")) || Path.StartsWith(TEXT("/Engine/")))
			{
				return Path;
			}
		}
		return FString();
	}
}

// Every montage read loaded the asset through UEditorAssetLibrary::LoadAsset, which refuses any load while a play
// session runs (EditorScriptingHelpers::CheckIfInEditorAndPIE: "The Editor is currently in a play mode"). So during
// PIE get_montage_length returned -1, get_montage_skeleton "", list_sections and list_slot_tracks empty lists, all
// indistinguishable from a montage that does not exist. A read must give the same answer in and out of PIE; an edit
// stays refused during PIE, since a montage a live actor plays must not change under it.
// Same FScopedPlayWorld pattern as VibeUE.Blackboard.Asset.WriteGuards: PlayWorld is pointed at the editor world for
// the duration of the reads.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeAnimMontageReadsDuringPIETest, "VibeUE.AnimMontage.ReadsDuringPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeAnimMontageReadsDuringPIETest::RunTest(const FString&)
{
	const FString SkeletonPath = FindAnySkeletonPath();
	if (SkeletonPath.IsEmpty())
	{
		AddInfo(TEXT("No skeleton asset available; skipping"));
		return true;
	}
	UWorld* const StandInWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("an editor world is available to stand in for a play world"), StandInWorld))
	{
		return false;
	}

	const FString AssetName = TEXT("AM_VibeUETest_ReadsDuringPIE");
	const FString SavePath = TEXT("/Game/VibeUETests");
	UAssetDiscoveryService::DeleteAssetUnattended(SavePath / AssetName, true); // leftovers from an earlier run

	// A package path (no ".AssetName"), as create_empty_montage returns it and as callers pass it.
	const FString MontagePath = UAnimMontageService::CreateEmptyMontage(SkeletonPath, SavePath, AssetName);
	if (!TestFalse(TEXT("create_empty_montage returns a path"), MontagePath.IsEmpty()))
	{
		return false;
	}

	// Outside a play session: the montage create_empty_montage made (one slot track, one section, 0.25 s blends).
	const float Length = UAnimMontageService::GetMontageLength(MontagePath);
	const FString Skeleton = UAnimMontageService::GetMontageSkeleton(MontagePath);
	const int32 Sections = UAnimMontageService::ListSections(MontagePath).Num();
	const int32 SlotTracks = UAnimMontageService::ListSlotTracks(MontagePath).Num();
	FVibeMontageBlendSettings Blend;
	TestTrue(TEXT("blend settings read outside PIE"), UAnimMontageService::GetBlendSettings(MontagePath, Blend));
	TestTrue(TEXT("length read outside PIE"), Length >= 0.f);
	TestFalse(TEXT("skeleton read outside PIE"), Skeleton.IsEmpty());
	TestEqual(TEXT("one section outside PIE"), Sections, 1);
	TestEqual(TEXT("one slot track outside PIE"), SlotTracks, 1);

	UWorld* const PlayWorldBefore = ToRawPtr(GEditor->PlayWorld);
	float LengthDuringPIE = -2.f;
	FString SkeletonDuringPIE;
	int32 SectionsDuringPIE = -1;
	int32 SlotTracksDuringPIE = -1;
	bool bBlendReadDuringPIE = false;
	FVibeMontageBlendSettings BlendDuringPIE;
	bool bBlendInSetDuringPIE = true;
	{
		struct FScopedPlayWorld
		{
			UWorld* Previous;
			explicit FScopedPlayWorld(UWorld* World) : Previous(ToRawPtr(GEditor->PlayWorld))
			{
				GEditor->PlayWorld = World;
			}
			~FScopedPlayWorld() { GEditor->PlayWorld = Previous; }
		} PretendPIE(StandInWorld);

		LengthDuringPIE = UAnimMontageService::GetMontageLength(MontagePath);
		SkeletonDuringPIE = UAnimMontageService::GetMontageSkeleton(MontagePath);
		SectionsDuringPIE = UAnimMontageService::ListSections(MontagePath).Num();
		SlotTracksDuringPIE = UAnimMontageService::ListSlotTracks(MontagePath).Num();
		bBlendReadDuringPIE = UAnimMontageService::GetBlendSettings(MontagePath, BlendDuringPIE);
		// The edit's refusal logs editor scripting's error once; a read logging it too would make it two.
		AddExpectedError(TEXT("The Editor is currently in a play mode"), EAutomationExpectedErrorFlags::Contains, 1);
		bBlendInSetDuringPIE = UAnimMontageService::SetBlendIn(MontagePath, Blend.BlendInTime + 0.5f);
	}
	TestEqual(TEXT("PlayWorld restored"), ToRawPtr(GEditor->PlayWorld), PlayWorldBefore);

	// The reads give the same answers during PIE.
	TestEqual(TEXT("get_montage_length during PIE"), LengthDuringPIE, Length);
	TestEqual(TEXT("get_montage_skeleton during PIE"), SkeletonDuringPIE, Skeleton);
	TestEqual(TEXT("list_sections during PIE"), SectionsDuringPIE, Sections);
	TestEqual(TEXT("list_slot_tracks during PIE"), SlotTracksDuringPIE, SlotTracks);
	TestTrue(TEXT("get_blend_settings during PIE"), bBlendReadDuringPIE);
	TestEqual(TEXT("blend in time read during PIE"), BlendDuringPIE.BlendInTime, Blend.BlendInTime);

	// An edit is still refused during PIE and changes nothing.
	TestFalse(TEXT("set_blend_in during PIE is refused"), bBlendInSetDuringPIE);
	FVibeMontageBlendSettings BlendAfter;
	TestTrue(TEXT("blend settings read after PIE"), UAnimMontageService::GetBlendSettings(MontagePath, BlendAfter));
	TestEqual(TEXT("the refused edit left the blend in time as it was"), BlendAfter.BlendInTime, Blend.BlendInTime);

	UAssetDiscoveryService::DeleteAssetUnattended(MontagePath, true);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
