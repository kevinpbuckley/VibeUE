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

	UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, true); // leftovers from an earlier run

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
	const FUnattendedDeleteResult CleanupResult = UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, false);
	TestTrue(TEXT("unattended delete removes the test clip"), CleanupResult.bSuccess);
	TestFalse(TEXT("asset is gone"), UEditorAssetLibrary::DoesAssetExist(AssetPath));
	return true;
}

namespace
{
	/** Find the full global-space pose of one bone from a GetPoseAtFrame result. */
	bool FindBonePose(const TArray<FBonePose>& Pose, const FString& BoneName, FTransform& Out)
	{
		for (const FBonePose& P : Pose)
		{
			if (P.BoneName == BoneName)
			{
				Out = P.Transform;
				return true;
			}
		}
		return false;
	}

	/** A skeleton owning a 3-bone descending chain with non-trivial segment lengths, or empty. */
	FString FindSkeletonWithChain(FString& OutRoot, FString& OutMid, FString& OutEnd)
	{
		FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> Skeletons;
		Registry.Get().GetAssetsByClass(USkeleton::StaticClass()->GetClassPathName(), Skeletons, true);
		for (const FAssetData& Data : Skeletons)
		{
			const FString Path = Data.GetObjectPathString();
			if (!Path.StartsWith(TEXT("/Game/")) && !Path.StartsWith(TEXT("/Engine/")))
			{
				continue;
			}
			USkeleton* Skeleton = Cast<USkeleton>(Data.GetAsset());
			if (!Skeleton)
			{
				continue;
			}
			const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
			const TArray<FTransform>& RefPose = Ref.GetRefBonePose();
			for (int32 End = 0; End < Ref.GetNum(); ++End)
			{
				const int32 Mid = Ref.GetParentIndex(End);
				if (Mid == INDEX_NONE)
				{
					continue;
				}
				const int32 Root = Ref.GetParentIndex(Mid);
				if (Root == INDEX_NONE)
				{
					continue;
				}
				// Segment lengths come from the child bones' local translations
				if (RefPose[End].GetTranslation().Size() > 2.0 && RefPose[Mid].GetTranslation().Size() > 2.0)
				{
					OutRoot = Ref.GetBoneName(Root).ToString();
					OutMid = Ref.GetBoneName(Mid).ToString();
					OutEnd = Ref.GetBoneName(End).ToString();
					return Path;
				}
			}
		}
		return FString();
	}
}

// #672: a failed capture must hand the caller the reason, not a bare false/None.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeAnimCaptureErrorReturnsStructTest, "VibeUE.Animation.Capture.ErrorReturnsStruct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeAnimCaptureErrorReturnsStructTest::RunTest(const FString&)
{
	UFunction* Function = UAnimSequenceService::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UAnimSequenceService, CaptureAnimationPose));
	TestNotNull(TEXT("CaptureAnimationPose is reflected"), Function);
	if (Function)
	{
		const FProperty* Return = Function->GetReturnProperty();
		TestNotNull(TEXT("CaptureAnimationPose has a return property"), Return);
		TestTrue(TEXT("CaptureAnimationPose returns the result struct"), Return && Return->IsA<FStructProperty>());
	}

	const FAnimationPoseCaptureResult Result = UAnimSequenceService::CaptureAnimationPose(
		TEXT("/Game/VibeUETests/AS_DoesNotExist"), 0.0f, FString(), FString(), 64, 64);
	TestFalse(TEXT("missing animation fails"), Result.bSuccess);
	TestTrue(TEXT("failure carries the reason"), Result.ErrorMessage.Contains(TEXT("Failed to load animation")));
	return true;
}

// solve_limb_ik: the end bone's origin must land on the requested component-space target.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeAnimSolveLimbIKTest, "VibeUE.Animation.PoseAuthoring.SolveLimbIK",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeAnimSolveLimbIKTest::RunTest(const FString&)
{
	FString RootBone, MidBone, EndBone;
	const FString SkeletonPath = FindSkeletonWithChain(RootBone, MidBone, EndBone);
	if (SkeletonPath.IsEmpty())
	{
		AddInfo(TEXT("No skeleton with a 3-bone chain available; skipping"));
		return true;
	}

	const FString AssetName = TEXT("AS_VibeUETest_SolveLimbIK");
	const FString SavePath = TEXT("/Game/VibeUETests");
	const FString AssetPath = SavePath / AssetName;
	UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, true);

	const FString Created = UAnimSequenceService::CreateAnimSequence(
		SkeletonPath, AssetName, SavePath, 0.5f, 30.f, {});
	TestFalse(TEXT("clip created"), Created.IsEmpty());
	if (Created.IsEmpty())
	{
		return false;
	}

	// Current chain geometry at frame 5
	TArray<FBonePose> Before = UAnimSequenceService::GetPoseAtFrame(Created, 5, true);
	FTransform RootT, MidT, EndT;
	if (!FindBonePose(Before, RootBone, RootT) || !FindBonePose(Before, MidBone, MidT) || !FindBonePose(Before, EndBone, EndT))
	{
		AddError(TEXT("chain bones missing from pose"));
		return false;
	}
	const FVector S = RootT.GetLocation();
	const double LenA = FVector::Dist(S, MidT.GetLocation());
	const double LenB = FVector::Dist(MidT.GetLocation(), EndT.GetLocation());

	// A reachable target: 90% of full reach, pushed slightly off the current line
	FVector Dir = (EndT.GetLocation() - S).GetSafeNormal();
	FVector Perp = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
	if (Perp.IsNearlyZero())
	{
		Perp = FVector::CrossProduct(Dir, FVector::RightVector).GetSafeNormal();
	}
	const FVector Target = S + (Dir + Perp * 0.2).GetSafeNormal() * (0.9 * (LenA + LenB));
	if (0.9 * (LenA + LenB) < FMath::Abs(LenA - LenB) + 0.5)
	{
		AddInfo(TEXT("chain too asymmetric for the standard target; skipping"));
		UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, false);
		return true;
	}

	const FAnimationEditResult Result = UAnimSequenceService::SolveLimbIK(
		Created, 5, RootBone, MidBone, EndBone, Target, FVector::ZeroVector,
		FVector::ZeroVector, FVector::ZeroVector);
	TestTrue(FString::Printf(TEXT("solve_limb_ik succeeds (%s)"), *Result.ErrorMessage), Result.bSuccess);
	TestEqual(TEXT("three bones written"), Result.ModifiedBones.Num(), 3);

	TArray<FBonePose> After = UAnimSequenceService::GetPoseAtFrame(Created, 5, true);
	FTransform EndAfter;
	TestTrue(TEXT("end bone still present"), FindBonePose(After, EndBone, EndAfter));
	TestTrue(FString::Printf(TEXT("end bone reaches the target (err %.3f cm)"),
		FVector::Dist(EndAfter.GetLocation(), Target)),
		FVector::Dist(EndAfter.GetLocation(), Target) < 0.5);

	// Other frames keep their pose
	TArray<FBonePose> Frame0 = UAnimSequenceService::GetPoseAtFrame(Created, 0, true);
	FTransform EndFrame0;
	TestTrue(TEXT("frame 0 end bone present"), FindBonePose(Frame0, EndBone, EndFrame0));
	TestTrue(TEXT("frame 0 untouched"),
		FVector::Dist(EndFrame0.GetLocation(), EndT.GetLocation()) < 0.1);

	// Error paths
	const FAnimationEditResult BadBone = UAnimSequenceService::SolveLimbIK(
		Created, 5, TEXT("no_such_bone"), MidBone, EndBone, Target, FVector::ZeroVector,
		FVector::ZeroVector, FVector::ZeroVector);
	TestFalse(TEXT("unknown bone fails"), BadBone.bSuccess);
	TestTrue(TEXT("unknown bone is named"), BadBone.ErrorMessage.Contains(TEXT("no_such_bone")));
	const FAnimationEditResult BadChain = UAnimSequenceService::SolveLimbIK(
		Created, 5, EndBone, MidBone, RootBone, Target, FVector::ZeroVector,
		FVector::ZeroVector, FVector::ZeroVector);
	TestFalse(TEXT("inverted chain fails"), BadChain.bSuccess);

	UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, false);
	return true;
}

// interpolate_bone_frames: interior frames follow the chosen easing between the two keys.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeAnimInterpolateBoneFramesTest, "VibeUE.Animation.PoseAuthoring.InterpolateBoneFrames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeAnimInterpolateBoneFramesTest::RunTest(const FString&)
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

	const FString AssetName = TEXT("AS_VibeUETest_Interpolate");
	const FString SavePath = TEXT("/Game/VibeUETests");
	const FString AssetPath = SavePath / AssetName;
	UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, true);

	// 0.5 s at 30 fps: keys 0..15; a 90 degree turn at the end
	FBoneTrackData Track;
	Track.BoneName = BoneName;
	FAnimKeyframe Start;
	Start.Time = 0.f;
	FAnimKeyframe End;
	End.Time = 0.5f;
	End.Frame = 15;
	End.Rotation = FQuat(FVector::YAxisVector, FMath::DegreesToRadians(90.f));
	Track.Keyframes = { Start, End };
	const FString Created = UAnimSequenceService::CreateAnimSequence(
		SkeletonPath, AssetName, SavePath, 0.5f, 30.f, { Track });
	TestFalse(TEXT("clip created"), Created.IsEmpty());
	if (Created.IsEmpty())
	{
		return false;
	}

	const FAnimationEditResult Result = UAnimSequenceService::InterpolateBoneFrames(
		Created, { BoneName }, 0, 15, TEXT("smooth"));
	TestTrue(FString::Printf(TEXT("interpolate succeeds (%s)"), *Result.ErrorMessage), Result.bSuccess);

	// Frame 4: t = 4/15 -> smoothstep = 3t^2-2t^3 = 0.1896 -> 17.1 deg of the 90
	FTransform Frame4;
	TestTrue(TEXT("frame 4 readable"),
		UAnimSequenceService::GetBoneTransformAtFrame(Created, BoneName, 4, false, Frame4));
	const double T = 4.0 / 15.0;
	const double Expected = (T * T * (3.0 - 2.0 * T)) * 90.0;
	const double Actual = FMath::RadiansToDegrees(
		Frame4.GetRotation().AngularDistance(FQuat::Identity));
	TestTrue(FString::Printf(TEXT("frame 4 follows smoothstep (expected %.1f, got %.1f)"), Expected, Actual),
		FMath::Abs(Actual - Expected) < 3.0);

	// The key frames themselves stay put
	FTransform Frame15;
	UAnimSequenceService::GetBoneTransformAtFrame(Created, BoneName, 15, false, Frame15);
	TestTrue(TEXT("end key still the quarter turn"),
		FMath::Abs(FMath::RadiansToDegrees(Frame15.GetRotation().AngularDistance(FQuat::Identity)) - 90.0) < 1.0);

	// Error paths
	const FAnimationEditResult BadEase = UAnimSequenceService::InterpolateBoneFrames(
		Created, { BoneName }, 0, 15, TEXT("bouncy"));
	TestFalse(TEXT("unknown easing fails"), BadEase.bSuccess);
	TestTrue(TEXT("unknown easing is named"), BadEase.ErrorMessage.Contains(TEXT("bouncy")));
	const FAnimationEditResult BadSpan = UAnimSequenceService::InterpolateBoneFrames(
		Created, { BoneName }, 15, 15, TEXT("smooth"));
	TestFalse(TEXT("empty span fails"), BadSpan.bSuccess);

	UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, false);
	return true;
}

// apply_hand_pose: clean errors without a hand; finger writes are covered by the
// headless MetaHuman probe (a test-project skeleton has no finger bones).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeAnimApplyHandPoseErrorsTest, "VibeUE.Animation.PoseAuthoring.ApplyHandPoseErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibeAnimApplyHandPoseErrorsTest::RunTest(const FString&)
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

	const FString AssetName = TEXT("AS_VibeUETest_HandPose");
	const FString SavePath = TEXT("/Game/VibeUETests");
	const FString AssetPath = SavePath / AssetName;
	UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, true);
	const FString Created = UAnimSequenceService::CreateAnimSequence(
		SkeletonPath, AssetName, SavePath, 0.5f, 30.f, {});
	TestFalse(TEXT("clip created"), Created.IsEmpty());
	if (Created.IsEmpty())
	{
		return false;
	}

	const FAnimationEditResult BadPose = UAnimSequenceService::ApplyHandPose(
		Created, 0, TEXT("hand_r"), TEXT("jazz_hands"), 1.0f);
	TestFalse(TEXT("unknown pose fails"), BadPose.bSuccess);
	TestTrue(TEXT("unknown pose is named"), BadPose.ErrorMessage.Contains(TEXT("jazz_hands")));

	const FAnimationEditResult BadBone = UAnimSequenceService::ApplyHandPose(
		Created, 0, TEXT("no_such_hand_r"), TEXT("fist"), 1.0f);
	TestFalse(TEXT("missing hand bone fails"), BadBone.bSuccess);

	const FAnimationEditResult NoSuffix = UAnimSequenceService::ApplyHandPose(
		Created, 0, BoneName, TEXT("fist"), 1.0f);
	TestFalse(TEXT("bone without _l/_r suffix or fingers fails"), NoSuffix.bSuccess);
	TestFalse(TEXT("failure explains itself"), NoSuffix.ErrorMessage.IsEmpty());

	UAssetDiscoveryService::DeleteAssetUnattended(AssetPath, false);
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

	const FUnattendedDeleteResult MissingResult = UAssetDiscoveryService::DeleteAssetUnattended(TEXT("/Game/VibeUETests/AS_DoesNotExist"), false);
	TestFalse(TEXT("missing asset is refused"), MissingResult.bSuccess);
	TestTrue(TEXT("missing asset reports not found"), MissingResult.ErrorMessage.Contains(TEXT("not found")));
	const FUnattendedDeleteResult EmptyResult = UAssetDiscoveryService::DeleteAssetUnattended(TEXT(""), false);
	TestFalse(TEXT("empty path is refused"), EmptyResult.bSuccess);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
