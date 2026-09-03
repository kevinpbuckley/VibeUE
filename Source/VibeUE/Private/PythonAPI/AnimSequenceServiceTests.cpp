// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UAnimSequenceService.h"
#include "PythonAPI/UAssetDiscoveryService.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EditorAssetLibrary.h"

namespace
{
	/** Any skeleton the project has: the test authors a throwaway clip against it. */
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

// create_anim_sequence used to hand the data model NumFrames keys where it wants NumFrames + 1
// (one per frame boundary), so every track was rejected and the saved asset had no animation.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeAnimSequenceCreateWritesKeysTest, "VibeUE.Animation.CreateAnimSequence.WritesKeys",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeAnimSequenceCreateWritesKeysTest::RunTest(const FString&)
{
	const FString SkeletonPath = FindAnySkeletonPath();
	if (SkeletonPath.IsEmpty())
	{
		AddInfo(TEXT("No skeleton asset available; skipping"));
		return true;
	}
	USkeleton* Skeleton = Cast<USkeleton>(UEditorAssetLibrary::LoadAsset(SkeletonPath));
	if (!Skeleton || Skeleton->GetReferenceSkeleton().GetNum() == 0)
	{
		AddInfo(TEXT("Skeleton has no bones; skipping"));
		return true;
	}
	const FString BoneName = Skeleton->GetReferenceSkeleton().GetBoneName(0).ToString();
	const FString AssetName = TEXT("AS_VibeUETest_CreateWritesKeys");
	const FString SavePath = TEXT("/Game/VibeUETests");
	const FString AssetPath = SavePath / AssetName;

	TArray<FString> Referencers;
	FString Error;
	UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, true, Referencers, Error); // leftovers from an earlier run

	// Two keys: identity at 0 s, a quarter turn about Y at the end
	const float Duration = 0.5f;
	const float FrameRate = 30.f;
	FBoneTrackData Track;
	Track.BoneName = BoneName;
	FAnimKeyframe Start;
	Start.Time = 0.f;
	FAnimKeyframe End;
	End.Time = Duration;
	End.Frame = FMath::RoundToInt(Duration * FrameRate);
	End.Rotation = FQuat(FVector::YAxisVector, FMath::DegreesToRadians(90.f));
	Track.Keyframes = { Start, End };

	const FString Created = UAnimSequenceService::CreateAnimSequence(SkeletonPath, AssetName, SavePath, Duration, FrameRate, { Track });
	TestFalse(TEXT("create_anim_sequence returns a path"), Created.IsEmpty());
	if (Created.IsEmpty())
	{
		return false;
	}

	UAnimSequence* Sequence = Cast<UAnimSequence>(UEditorAssetLibrary::LoadAsset(Created));
	TestNotNull(TEXT("created asset loads as an AnimSequence"), Sequence);
	if (Sequence)
	{
		const IAnimationDataModel* Model = Sequence->GetDataModel();
		TestNotNull(TEXT("data model exists"), Model);
		if (Model)
		{
			const int32 ExpectedFrames = FMath::RoundToInt(Duration * FrameRate);
			TestEqual(TEXT("frame count matches duration x rate"), Model->GetNumberOfFrames(), ExpectedFrames);
			TestEqual(TEXT("one key per frame boundary"), Model->GetNumberOfKeys(), ExpectedFrames + 1);
			TArray<FName> TrackNames;
			Model->GetBoneTrackNames(TrackNames);
			TestTrue(TEXT("the requested bone has a track"), TrackNames.Contains(FName(*BoneName)));
			// The last key must carry the authored rotation, not the reference pose
			const FTransform LastKey = Model->GetBoneTrackTransform(FName(*BoneName), FFrameNumber(ExpectedFrames));
			TestTrue(TEXT("last key is the authored quarter turn"), LastKey.GetRotation().Equals(End.Rotation, 0.01f));
		}
	}

	// Cleanup through the unattended delete (also exercises its unreferenced path)
	TestTrue(TEXT("unattended delete removes the test clip"), UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, false, Referencers, Error));
	TestFalse(TEXT("asset is gone"), UEditorAssetLibrary::DoesAssetExist(AssetPath));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeDeleteAssetUnattendedTest, "VibeUE.Assets.DeleteAssetUnattended",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeDeleteAssetUnattendedTest::RunTest(const FString&)
{
	UFunction* Function = UAssetDiscoveryService::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UAssetDiscoveryService, DeleteAssetUnattended));
	TestNotNull(TEXT("DeleteAssetUnattended is reflected"), Function);
	if (Function)
	{
		TestTrue(TEXT("DeleteAssetUnattended is AICallable"), Function->HasMetaData(TEXT("AICallable")));
	}

	TArray<FString> Referencers;
	FString Error;
	TestFalse(TEXT("missing asset is refused"), UAssetDiscoveryService::DeleteAssetUnattended(TEXT("/Game/VibeUETests/AS_DoesNotExist"), false, Referencers, Error));
	TestTrue(TEXT("missing asset reports not found"), Error.Contains(TEXT("not found")));
	TestFalse(TEXT("empty path is refused"), UAssetDiscoveryService::DeleteAssetUnattended(TEXT(""), false, Referencers, Error));
	return true;
}

#endif // WITH_AUTOMATION_TESTS
