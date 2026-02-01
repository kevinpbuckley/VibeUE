// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UAnimSequenceService.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimSequenceHelpers.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Factories/AnimSequenceFactory.h"
#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "EditorFramework/AssetImportData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "JsonObjectConverter.h"

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

UAnimSequence* UAnimSequenceService::LoadAnimSequence(const FString& AnimPath)
{
	if (AnimPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::LoadAnimSequence: Path is empty"));
		return nullptr;
	}

	// First, try direct load
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(AnimPath);
	if (LoadedObject)
	{
		UAnimSequence* AnimSeq = Cast<UAnimSequence>(LoadedObject);
		if (AnimSeq)
		{
			return AnimSeq;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::LoadAnimSequence: Not an AnimSequence: %s (got %s)"), 
				*AnimPath, *LoadedObject->GetClass()->GetName());
			return nullptr;
		}
	}

	// If direct load fails, the path might be a folder path (package_path) instead of asset path (package_name)
	// Try to find AnimSequence assets in this folder using the Asset Registry
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	
	FARFilter Filter;
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*AnimPath));
	Filter.bRecursivePaths = false;

	TArray<FAssetData> FoundAssets;
	AssetRegistry.GetAssets(Filter, FoundAssets);

	if (FoundAssets.Num() > 0)
	{
		// Found assets in the folder - provide helpful error message
		FString AssetNames;
		for (int32 i = 0; i < FMath::Min(5, FoundAssets.Num()); ++i)
		{
			if (i > 0) AssetNames += TEXT(", ");
			AssetNames += FoundAssets[i].PackageName.ToString();
		}
		if (FoundAssets.Num() > 5)
		{
			AssetNames += FString::Printf(TEXT("... and %d more"), FoundAssets.Num() - 5);
		}

		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::LoadAnimSequence: '%s' appears to be a folder, not an asset path. "
			"Use the full asset path (package_name from AssetData, not package_path). "
			"Found %d AnimSequences in this folder: [%s]"), 
			*AnimPath, FoundAssets.Num(), *AssetNames);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::LoadAnimSequence: Failed to load: %s. "
			"Make sure to use the full asset path (e.g., '/Game/Folder/AssetName' not just '/Game/Folder')"), 
			*AnimPath);
	}

	return nullptr;
}

FString UAnimSequenceService::AdditiveTypeToString(int32 Type)
{
	switch (static_cast<EAdditiveAnimationType>(Type))
	{
		case EAdditiveAnimationType::AAT_None:
			return TEXT("None");
		case EAdditiveAnimationType::AAT_LocalSpaceBase:
			return TEXT("LocalSpace");
		case EAdditiveAnimationType::AAT_RotationOffsetMeshSpace:
			return TEXT("MeshSpace");
		default:
			return TEXT("None");
	}
}

int32 UAnimSequenceService::StringToAdditiveType(const FString& TypeString)
{
	if (TypeString.Equals(TEXT("LocalSpace"), ESearchCase::IgnoreCase))
	{
		return static_cast<int32>(EAdditiveAnimationType::AAT_LocalSpaceBase);
	}
	else if (TypeString.Equals(TEXT("MeshSpace"), ESearchCase::IgnoreCase))
	{
		return static_cast<int32>(EAdditiveAnimationType::AAT_RotationOffsetMeshSpace);
	}
	return static_cast<int32>(EAdditiveAnimationType::AAT_None);
}

FString UAnimSequenceService::RootLockToString(int32 LockType)
{
	switch (static_cast<ERootMotionRootLock::Type>(LockType))
	{
		case ERootMotionRootLock::RefPose:
			return TEXT("RefPose");
		case ERootMotionRootLock::AnimFirstFrame:
			return TEXT("AnimFirstFrame");
		case ERootMotionRootLock::Zero:
			return TEXT("Zero");
		default:
			return TEXT("RefPose");
	}
}

int32 UAnimSequenceService::StringToRootLock(const FString& LockString)
{
	if (LockString.Equals(TEXT("AnimFirstFrame"), ESearchCase::IgnoreCase))
	{
		return static_cast<int32>(ERootMotionRootLock::AnimFirstFrame);
	}
	else if (LockString.Equals(TEXT("Zero"), ESearchCase::IgnoreCase))
	{
		return static_cast<int32>(ERootMotionRootLock::Zero);
	}
	return static_cast<int32>(ERootMotionRootLock::RefPose);
}

FString UAnimSequenceService::InterpModeToString(int32 Mode)
{
	switch (static_cast<ERichCurveInterpMode>(Mode))
	{
		case ERichCurveInterpMode::RCIM_Linear:
			return TEXT("Linear");
		case ERichCurveInterpMode::RCIM_Constant:
			return TEXT("Constant");
		case ERichCurveInterpMode::RCIM_Cubic:
			return TEXT("Cubic");
		case ERichCurveInterpMode::RCIM_None:
			return TEXT("None");
		default:
			return TEXT("Linear");
	}
}

FString UAnimSequenceService::TangentModeToString(int32 Mode)
{
	switch (static_cast<ERichCurveTangentMode>(Mode))
	{
		case ERichCurveTangentMode::RCTM_Auto:
			return TEXT("Auto");
		case ERichCurveTangentMode::RCTM_User:
			return TEXT("User");
		case ERichCurveTangentMode::RCTM_Break:
			return TEXT("Break");
		case ERichCurveTangentMode::RCTM_None:
			return TEXT("None");
		default:
			return TEXT("Auto");
	}
}

void UAnimSequenceService::FillAnimSequenceInfo(UAnimSequence* AnimSeq, FAnimSequenceInfo& OutInfo)
{
	if (!AnimSeq)
	{
		return;
	}

	OutInfo.AnimPath = AnimSeq->GetPathName();
	OutInfo.AnimName = AnimSeq->GetName();

	if (USkeleton* Skeleton = AnimSeq->GetSkeleton())
	{
		OutInfo.SkeletonPath = Skeleton->GetPathName();
	}

	OutInfo.Duration = AnimSeq->GetPlayLength();
	OutInfo.FrameRate = AnimSeq->GetSamplingFrameRate().AsDecimal();
	OutInfo.FrameCount = AnimSeq->GetNumberOfSampledKeys();

	// Get bone track count from data model
	const IAnimationDataModel* DataModel = AnimSeq->GetDataModel();
	if (DataModel)
	{
		OutInfo.BoneTrackCount = DataModel->GetNumBoneTracks();
	}

	// Get curve count
	OutInfo.CurveCount = AnimSeq->GetCurveData().FloatCurves.Num();

	// Get notify count
	OutInfo.NotifyCount = AnimSeq->Notifies.Num();

	// Root motion
	OutInfo.bEnableRootMotion = AnimSeq->bEnableRootMotion;

	// Additive type
	OutInfo.AdditiveAnimType = AdditiveTypeToString(static_cast<int32>(AnimSeq->AdditiveAnimType));

	// Rate scale
	OutInfo.RateScale = AnimSeq->RateScale;

	// Compression info
	OutInfo.CompressedSize = AnimSeq->GetApproxCompressedSize();
	OutInfo.RawSize = AnimSeq->GetApproxRawSize();
}

// ============================================================================
// ANIMATION DISCOVERY
// ============================================================================

TArray<FAnimSequenceInfo> UAnimSequenceService::ListAnimSequences(
	const FString& SearchPath,
	const FString& SkeletonFilter)
{
	TArray<FAnimSequenceInfo> Results;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(*SearchPath);
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	// Limit results to prevent memory issues and crashes
	const int32 MaxResults = 100;
	int32 LoadedCount = 0;

	for (const FAssetData& Asset : AssetList)
	{
		if (LoadedCount >= MaxResults)
		{
			UE_LOG(LogTemp, Warning, TEXT("ListAnimSequences: Limiting results to %d animations (found %d total)"), MaxResults, AssetList.Num());
			break;
		}

		// Skip loading - just get basic info from AssetData
		FAnimSequenceInfo Info;
		Info.AnimPath = Asset.GetObjectPathString();
		Info.AnimName = Asset.AssetName.ToString();
		
		// Try to get skeleton path from asset tag (without loading the full asset)
		FAssetTagValueRef SkeletonTag = Asset.TagsAndValues.FindTag(FName(TEXT("Skeleton")));
		if (SkeletonTag.IsSet())
		{
			Info.SkeletonPath = SkeletonTag.AsString();
			
			// Apply skeleton filter if specified
			if (!SkeletonFilter.IsEmpty() && !Info.SkeletonPath.Contains(SkeletonFilter))
			{
				continue;
			}
		}
		else if (!SkeletonFilter.IsEmpty())
		{
			// If we need skeleton filter but don't have tag data, load the asset
			FSoftObjectPath AssetPath(Asset.GetSoftObjectPath());
			UAnimSequence* AnimSeq = Cast<UAnimSequence>(AssetPath.TryLoad());
			if (!AnimSeq)
			{
				continue;
			}
			USkeleton* Skeleton = AnimSeq->GetSkeleton();
			if (!Skeleton || !Skeleton->GetPathName().Contains(SkeletonFilter))
			{
				continue;
			}
			// Fill full info since we loaded it anyway
			FillAnimSequenceInfo(AnimSeq, Info);
			Results.Add(Info);
			LoadedCount++;
			continue;
		}

		Results.Add(Info);
		LoadedCount++;
	}

	return Results;
}

bool UAnimSequenceService::GetAnimSequenceInfo(const FString& AnimPath, FAnimSequenceInfo& OutInfo)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	FillAnimSequenceInfo(AnimSeq, OutInfo);
	return true;
}

TArray<FAnimSequenceInfo> UAnimSequenceService::FindAnimationsForSkeleton(const FString& SkeletonPath)
{
	TArray<FAnimSequenceInfo> Results;

	if (SkeletonPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::FindAnimationsForSkeleton: Skeleton path is empty"));
		return Results;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	for (const FAssetData& Asset : AssetList)
	{
		// Use soft load to avoid crashes on corrupted/invalid assets
		FSoftObjectPath AssetPath(Asset.GetSoftObjectPath());
		UAnimSequence* AnimSeq = Cast<UAnimSequence>(AssetPath.TryLoad());
		if (!AnimSeq)
		{
			continue;
		}

		USkeleton* Skeleton = AnimSeq->GetSkeleton();
		if (Skeleton && Skeleton->GetPathName().Equals(SkeletonPath))
		{
			FAnimSequenceInfo Info;
			FillAnimSequenceInfo(AnimSeq, Info);
			Results.Add(Info);
		}
	}

	return Results;
}

TArray<FAnimSequenceInfo> UAnimSequenceService::SearchAnimations(
	const FString& NamePattern,
	const FString& SearchPath)
{
	TArray<FAnimSequenceInfo> Results;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(*SearchPath);
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	for (const FAssetData& Asset : AssetList)
	{
		// Match name pattern
		FString AssetName = Asset.AssetName.ToString();
		if (AssetName.MatchesWildcard(NamePattern))
		{
			// Use soft load to avoid crashes on corrupted/invalid assets
			FSoftObjectPath AssetPath(Asset.GetSoftObjectPath());
			UAnimSequence* AnimSeq = Cast<UAnimSequence>(AssetPath.TryLoad());
			if (AnimSeq)
			{
				FAnimSequenceInfo Info;
				FillAnimSequenceInfo(AnimSeq, Info);
				Results.Add(Info);
			}
		}
	}

	return Results;
}

// ============================================================================
// ANIMATION CREATION
// ============================================================================

FString UAnimSequenceService::CreateFromPose(
	const FString& SkeletonPath,
	const FString& AnimName,
	const FString& SavePath,
	float Duration)
{
	// Validate inputs
	if (SkeletonPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimSequenceService::CreateFromPose: Skeleton path is empty"));
		return FString();
	}

	if (AnimName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimSequenceService::CreateFromPose: Animation name is empty"));
		return FString();
	}

	// Load skeleton
	USkeleton* Skeleton = Cast<USkeleton>(UEditorAssetLibrary::LoadAsset(SkeletonPath));
	if (!Skeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimSequenceService::CreateFromPose: Failed to load skeleton: %s"), *SkeletonPath);
		return FString();
	}

	// Construct full asset path
	FString FullPath = SavePath;
	if (!FullPath.EndsWith(TEXT("/")))
	{
		FullPath += TEXT("/");
	}
	FullPath += AnimName;

	// Check if asset already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::CreateFromPose: Asset already exists: %s"), *FullPath);
		return FString();
	}

	// Create factory and set skeleton
	UAnimSequenceFactory* Factory = NewObject<UAnimSequenceFactory>();
	Factory->TargetSkeleton = Skeleton;

	// Create the animation sequence using AssetTools with factory
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UAnimSequence* NewAnimSeq = Cast<UAnimSequence>(
		AssetTools.CreateAsset(AnimName, SavePath, UAnimSequence::StaticClass(), Factory)
	);

	if (!NewAnimSeq)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimSequenceService::CreateFromPose: Failed to create animation sequence"));
		return FString();
	}

	// Set frame rate and duration
	FFrameRate FrameRate(30, 1); // 30 FPS
	
	// Get number of frames based on duration
	int32 NumFrames = FMath::Max(1, FMath::RoundToInt(Duration * FrameRate.AsDecimal()));

	// Get controller for animation data
	IAnimationDataController& Controller = NewAnimSeq->GetController();

	// Open bracket for all modifications
	Controller.OpenBracket(NSLOCTEXT("AnimSequenceService", "CreateFromPose", "Create Animation from Pose"));

	// Set frame rate and number of frames (SetPlayLength is deprecated)
	Controller.SetFrameRate(FrameRate);
	Controller.SetNumberOfFrames(FFrameNumber(NumFrames));

	// Close bracket to finalize all changes
	Controller.CloseBracket();

	// Mark as modified and save
	NewAnimSeq->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(FullPath);

	UE_LOG(LogTemp, Log, TEXT("UAnimSequenceService::CreateFromPose: Created animation: %s"), *FullPath);
	return FullPath;
}

FString UAnimSequenceService::CreateAnimSequence(
	const FString& SkeletonPath,
	const FString& AnimName,
	const FString& SavePath,
	float Duration,
	float FrameRate,
	const TArray<FBoneTrackData>& BoneTracks)
{
	// Validate inputs
	if (SkeletonPath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimSequenceService::CreateAnimSequence: Skeleton path is empty"));
		return FString();
	}

	if (AnimName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimSequenceService::CreateAnimSequence: Animation name is empty"));
		return FString();
	}

	if (Duration <= 0.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimSequenceService::CreateAnimSequence: Duration must be positive"));
		return FString();
	}

	if (FrameRate <= 0.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimSequenceService::CreateAnimSequence: Frame rate must be positive"));
		return FString();
	}

	// Load skeleton
	USkeleton* Skeleton = Cast<USkeleton>(UEditorAssetLibrary::LoadAsset(SkeletonPath));
	if (!Skeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimSequenceService::CreateAnimSequence: Failed to load skeleton: %s"), *SkeletonPath);
		return FString();
	}

	// Construct full asset path
	FString FullPath = SavePath;
	if (!FullPath.EndsWith(TEXT("/")))
	{
		FullPath += TEXT("/");
	}
	FullPath += AnimName;

	// Check if asset already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::CreateAnimSequence: Asset already exists: %s"), *FullPath);
		return FString();
	}

	// Create factory and set skeleton
	UAnimSequenceFactory* Factory = NewObject<UAnimSequenceFactory>();
	Factory->TargetSkeleton = Skeleton;

	// Create the animation sequence using AssetTools with factory
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UAnimSequence* NewAnimSeq = Cast<UAnimSequence>(
		AssetTools.CreateAsset(AnimName, SavePath, UAnimSequence::StaticClass(), Factory)
	);

	if (!NewAnimSeq)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimSequenceService::CreateAnimSequence: Failed to create animation sequence"));
		return FString();
	}

	// Set frame rate and duration
	FFrameRate AnimFrameRate(FMath::RoundToInt(FrameRate), 1);
	
	// Get number of frames based on duration
	int32 NumFrames = FMath::Max(1, FMath::RoundToInt(Duration * FrameRate));

	// Get controller for animation data
	IAnimationDataController& Controller = NewAnimSeq->GetController();

	// Open bracket for all modifications
	Controller.OpenBracket(NSLOCTEXT("AnimSequenceService", "CreateAnimSequence", "Create Animation Sequence"));

	// Set frame rate and number of frames (SetPlayLength is deprecated)
	Controller.SetFrameRate(AnimFrameRate);
	Controller.SetNumberOfFrames(FFrameNumber(NumFrames));

	// Get reference skeleton for bone validation
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	// Process bone tracks
	int32 TracksAdded = 0;
	for (const FBoneTrackData& TrackData : BoneTracks)
	{
		if (TrackData.BoneName.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::CreateAnimSequence: Skipping track with empty bone name"));
			continue;
		}

		// Validate bone exists in skeleton
		FName BoneFName(*TrackData.BoneName);
		int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneFName);
		if (BoneIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::CreateAnimSequence: Bone '%s' not found in skeleton, skipping"), *TrackData.BoneName);
			continue;
		}

		if (TrackData.Keyframes.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::CreateAnimSequence: No keyframes for bone '%s', skipping"), *TrackData.BoneName);
			continue;
		}

		// Check if bone track already exists (factory may have created it from skeleton reference pose)
		const IAnimationDataModel* DataModel = NewAnimSeq->GetDataModel();
		bool bTrackExists = false;
		if (DataModel)
		{
			for (const FBoneAnimationTrack& ExistingTrack : DataModel->GetBoneAnimationTracks())
			{
				if (ExistingTrack.Name == BoneFName)
				{
					bTrackExists = true;
					break;
				}
			}
		}

		// Add bone track only if it doesn't exist
		if (!bTrackExists)
		{
			bool bAddedTrack = Controller.AddBoneCurve(BoneFName, false);
			if (!bAddedTrack)
			{
				UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::CreateAnimSequence: Failed to add bone track for '%s'"), *TrackData.BoneName);
				continue;
			}
		}

		// Build key arrays from keyframe data
		// Keys need to be provided for every frame in the animation
		TArray<FVector3f> PositionalKeys;
		TArray<FQuat4f> RotationalKeys;
		TArray<FVector3f> ScalingKeys;

		// For proper animation, we need keys for every frame
		// Interpolate between the provided keyframes
		PositionalKeys.SetNum(NumFrames);
		RotationalKeys.SetNum(NumFrames);
		ScalingKeys.SetNum(NumFrames);

		// Sort keyframes by time
		TArray<FAnimKeyframe> SortedKeyframes = TrackData.Keyframes;
		SortedKeyframes.Sort([](const FAnimKeyframe& A, const FAnimKeyframe& B) { return A.Time < B.Time; });

		// Interpolate keyframes to fill all frames
		for (int32 FrameIdx = 0; FrameIdx < NumFrames; ++FrameIdx)
		{
			float FrameTime = (float)FrameIdx / FrameRate;

			// Find surrounding keyframes for interpolation
			int32 KeyBefore = 0;
			int32 KeyAfter = 0;

			for (int32 KeyIdx = 0; KeyIdx < SortedKeyframes.Num(); ++KeyIdx)
			{
				if (SortedKeyframes[KeyIdx].Time <= FrameTime)
				{
					KeyBefore = KeyIdx;
				}
				if (SortedKeyframes[KeyIdx].Time >= FrameTime)
				{
					KeyAfter = KeyIdx;
					break;
				}
				KeyAfter = KeyIdx;
			}

			// Interpolate between keyframes
			const FAnimKeyframe& KfBefore = SortedKeyframes[KeyBefore];
			const FAnimKeyframe& KfAfter = SortedKeyframes[KeyAfter];

			float Alpha = 0.0f;
			if (KeyBefore != KeyAfter && (KfAfter.Time - KfBefore.Time) > KINDA_SMALL_NUMBER)
			{
				Alpha = (FrameTime - KfBefore.Time) / (KfAfter.Time - KfBefore.Time);
				Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
			}

			// Interpolate position
			FVector InterpPosition = FMath::Lerp(KfBefore.Position, KfAfter.Position, Alpha);
			PositionalKeys[FrameIdx] = FVector3f(InterpPosition);

			// Interpolate rotation (spherical)
			FQuat InterpRotation = FQuat::Slerp(KfBefore.Rotation, KfAfter.Rotation, Alpha);
			RotationalKeys[FrameIdx] = FQuat4f(InterpRotation);

			// Interpolate scale
			FVector InterpScale = FMath::Lerp(KfBefore.Scale, KfAfter.Scale, Alpha);
			ScalingKeys[FrameIdx] = FVector3f(InterpScale);
		}

		// Set the bone track keys
		bool bSuccess = Controller.SetBoneTrackKeys(BoneFName, PositionalKeys, RotationalKeys, ScalingKeys, false);
		if (bSuccess)
		{
			TracksAdded++;
			UE_LOG(LogTemp, Log, TEXT("UAnimSequenceService::CreateAnimSequence: Added bone track '%s' with %d keyframes"), 
				*TrackData.BoneName, TrackData.Keyframes.Num());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::CreateAnimSequence: Failed to set keys for bone '%s'"), *TrackData.BoneName);
		}
	}

	// Close bracket to finalize all changes
	Controller.CloseBracket();

	// Mark as modified and save
	NewAnimSeq->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(FullPath);

	UE_LOG(LogTemp, Log, TEXT("UAnimSequenceService::CreateAnimSequence: Created animation: %s with %d bone tracks"), *FullPath, TracksAdded);
	return FullPath;
}

FAnimKeyframe UAnimSequenceService::GetReferencePoseKeyframe(
	const FString& SkeletonPath,
	const FString& BoneName,
	float Time)
{
	FAnimKeyframe Keyframe;
	Keyframe.Time = Time;
	Keyframe.Position = FVector::ZeroVector;
	Keyframe.Rotation = FQuat::Identity;
	Keyframe.Scale = FVector::OneVector;

	if (SkeletonPath.IsEmpty() || BoneName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::GetReferencePoseKeyframe: Empty skeleton path or bone name"));
		return Keyframe;
	}

	// Load skeleton
	USkeleton* Skeleton = Cast<USkeleton>(UEditorAssetLibrary::LoadAsset(SkeletonPath));
	if (!Skeleton)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::GetReferencePoseKeyframe: Failed to load skeleton: %s"), *SkeletonPath);
		return Keyframe;
	}

	// Find bone
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	int32 BoneIndex = RefSkeleton.FindBoneIndex(FName(*BoneName));
	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::GetReferencePoseKeyframe: Bone '%s' not found in skeleton"), *BoneName);
		return Keyframe;
	}

	// Get reference pose transform (local space)
	const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();
	if (BoneIndex < RefPose.Num())
	{
		const FTransform& BoneTransform = RefPose[BoneIndex];
		Keyframe.Position = BoneTransform.GetLocation();
		Keyframe.Rotation = BoneTransform.GetRotation();
		Keyframe.Scale = BoneTransform.GetScale3D();
	}

	return Keyframe;
}

void UAnimSequenceService::EulerToQuat(float Roll, float Pitch, float Yaw, FQuat& OutQuat)
{
	// FRotator expects Pitch, Yaw, Roll order
	FRotator Rotator(Pitch, Yaw, Roll);
	OutQuat = Rotator.Quaternion();
}

void UAnimSequenceService::MultiplyQuats(const FQuat& A, const FQuat& B, FQuat& OutResult)
{
	OutResult = A * B;
}

// ============================================================================
// ANIMATION PROPERTIES
// ============================================================================

float UAnimSequenceService::GetAnimationLength(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return -1.0f;
	}
	return AnimSeq->GetPlayLength();
}

float UAnimSequenceService::GetAnimationFrameRate(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return -1.0f;
	}
	return AnimSeq->GetSamplingFrameRate().AsDecimal();
}

int32 UAnimSequenceService::GetAnimationFrameCount(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return -1;
	}
	return AnimSeq->GetNumberOfSampledKeys();
}

bool UAnimSequenceService::SetAnimationFrameRate(const FString& AnimPath, float NewFrameRate)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (NewFrameRate <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetAnimationFrameRate: Invalid frame rate: %f"), NewFrameRate);
		return false;
	}

	// Note: Changing frame rate typically requires reimport
	// This sets the target frame rate for the data model
	IAnimationDataController& Controller = AnimSeq->GetController();
	Controller.SetFrameRate(FFrameRate(FMath::RoundToInt32(NewFrameRate), 1));

	AnimSeq->MarkPackageDirty();
	return true;
}

FString UAnimSequenceService::GetAnimationSkeleton(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return FString();
	}

	USkeleton* Skeleton = AnimSeq->GetSkeleton();
	if (!Skeleton)
	{
		return FString();
	}

	return Skeleton->GetPathName();
}

float UAnimSequenceService::GetRateScale(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return -1.0f;
	}
	return AnimSeq->RateScale;
}

bool UAnimSequenceService::SetRateScale(const FString& AnimPath, float RateScale)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->RateScale = RateScale;
	AnimSeq->MarkPackageDirty();
	return true;
}

// ============================================================================
// BONE TRACK DATA
// ============================================================================

TArray<FString> UAnimSequenceService::GetAnimatedBones(const FString& AnimPath)
{
	TArray<FString> Results;

	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return Results;
	}

	const IAnimationDataModel* DataModel = AnimSeq->GetDataModel();
	if (!DataModel)
	{
		return Results;
	}

	// Use the new UE5 API: GetBoneTrackNames instead of deprecated GetBoneAnimationTracks
	TArray<FName> BoneTrackNames;
	DataModel->GetBoneTrackNames(BoneTrackNames);

	for (const FName& BoneName : BoneTrackNames)
	{
		Results.Add(BoneName.ToString());
	}

	return Results;
}

bool UAnimSequenceService::GetBoneTransformAtTime(
	const FString& AnimPath,
	const FString& BoneName,
	float Time,
	bool bGlobalSpace,
	FTransform& OutTransform)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	USkeleton* Skeleton = AnimSeq->GetSkeleton();
	if (!Skeleton)
	{
		return false;
	}

	int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(FName(*BoneName));
	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::GetBoneTransformAtTime: Bone not found: %s"), *BoneName);
		return false;
	}

	// Clamp time to animation bounds
	Time = FMath::Clamp(Time, 0.0f, AnimSeq->GetPlayLength());

	// Get bone transform at time using FSkeletonPoseBoneIndex
	FSkeletonPoseBoneIndex SkeletonBoneIndex(BoneIndex);
	AnimSeq->GetBoneTransform(OutTransform, SkeletonBoneIndex, static_cast<double>(Time), true);

	if (bGlobalSpace)
	{
		// Build chain to root for global transform
		const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
		TArray<FTransform> ChainTransforms;
		int32 CurrentIndex = BoneIndex;

		while (CurrentIndex != INDEX_NONE)
		{
			FTransform BoneTransform;
			FSkeletonPoseBoneIndex CurrentSkeletonIndex(CurrentIndex);
			AnimSeq->GetBoneTransform(BoneTransform, CurrentSkeletonIndex, static_cast<double>(Time), true);
			ChainTransforms.Insert(BoneTransform, 0);
			CurrentIndex = RefSkeleton.GetParentIndex(CurrentIndex);
		}

		// Accumulate transforms
		FTransform GlobalTransform = FTransform::Identity;
		for (const FTransform& Transform : ChainTransforms)
		{
			GlobalTransform = Transform * GlobalTransform;
		}
		OutTransform = GlobalTransform;
	}

	return true;
}

bool UAnimSequenceService::GetBoneTransformAtFrame(
	const FString& AnimPath,
	const FString& BoneName,
	int32 Frame,
	bool bGlobalSpace,
	FTransform& OutTransform)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	float FrameRate = AnimSeq->GetSamplingFrameRate().AsDecimal();
	if (FrameRate <= 0.0f)
	{
		return false;
	}

	float Time = static_cast<float>(Frame) / FrameRate;
	return GetBoneTransformAtTime(AnimPath, BoneName, Time, bGlobalSpace, OutTransform);
}

// ============================================================================
// POSE EXTRACTION
// ============================================================================

TArray<FBonePose> UAnimSequenceService::GetPoseAtTime(
	const FString& AnimPath,
	float Time,
	bool bGlobalSpace)
{
	TArray<FBonePose> Results;

	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return Results;
	}

	USkeleton* Skeleton = AnimSeq->GetSkeleton();
	if (!Skeleton)
	{
		return Results;
	}

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	int32 NumBones = RefSkeleton.GetNum();

	// Clamp time
	Time = FMath::Clamp(Time, 0.0f, AnimSeq->GetPlayLength());

	// First pass: get all local transforms
	TArray<FTransform> LocalTransforms;
	LocalTransforms.SetNum(NumBones);

	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		FSkeletonPoseBoneIndex SkeletonBoneIndex(BoneIndex);
		AnimSeq->GetBoneTransform(LocalTransforms[BoneIndex], SkeletonBoneIndex, static_cast<double>(Time), true);
	}

	// Second pass: compute global transforms if needed and fill results
	TArray<FTransform> GlobalTransforms;
	if (bGlobalSpace)
	{
		GlobalTransforms.SetNum(NumBones);
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
			if (ParentIndex == INDEX_NONE)
			{
				GlobalTransforms[BoneIndex] = LocalTransforms[BoneIndex];
			}
			else
			{
				GlobalTransforms[BoneIndex] = LocalTransforms[BoneIndex] * GlobalTransforms[ParentIndex];
			}
		}
	}

	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		FBonePose Pose;
		Pose.BoneName = RefSkeleton.GetBoneName(BoneIndex).ToString();
		Pose.BoneIndex = BoneIndex;
		Pose.Transform = bGlobalSpace ? GlobalTransforms[BoneIndex] : LocalTransforms[BoneIndex];
		Results.Add(Pose);
	}

	return Results;
}

TArray<FBonePose> UAnimSequenceService::GetPoseAtFrame(
	const FString& AnimPath,
	int32 Frame,
	bool bGlobalSpace)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return TArray<FBonePose>();
	}

	float FrameRate = AnimSeq->GetSamplingFrameRate().AsDecimal();
	if (FrameRate <= 0.0f)
	{
		return TArray<FBonePose>();
	}

	float Time = static_cast<float>(Frame) / FrameRate;
	return GetPoseAtTime(AnimPath, Time, bGlobalSpace);
}

bool UAnimSequenceService::GetRootMotionAtTime(
	const FString& AnimPath,
	float Time,
	FTransform& OutTransform)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	Time = FMath::Clamp(Time, 0.0f, AnimSeq->GetPlayLength());

	// ExtractRootMotion(StartTime, DeltaTime, bAllowLooping)
	OutTransform = AnimSeq->ExtractRootMotion(0.0f, Time, false);
	return true;
}

bool UAnimSequenceService::GetTotalRootMotion(
	const FString& AnimPath,
	FTransform& OutTransform)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	// ExtractRootMotion(StartTime, DeltaTime, bAllowLooping)
	OutTransform = AnimSeq->ExtractRootMotion(0.0f, AnimSeq->GetPlayLength(), false);
	return true;
}

// ============================================================================
// CURVE DATA
// ============================================================================

TArray<FAnimCurveInfo> UAnimSequenceService::ListCurves(const FString& AnimPath)
{
	TArray<FAnimCurveInfo> Results;

	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return Results;
	}

	const FRawCurveTracks& CurveData = AnimSeq->GetCurveData();

	for (const FFloatCurve& Curve : CurveData.FloatCurves)
	{
		FAnimCurveInfo Info;
		Info.CurveName = Curve.GetName().ToString();
		Info.CurveType = TEXT("Float");
		Info.KeyCount = Curve.FloatCurve.GetNumKeys();
		Info.DefaultValue = Curve.FloatCurve.GetDefaultValue();
		Info.bMorphTarget = false; // Deprecated flag check removed
		Info.bMaterial = false; // Deprecated flag check removed
		Results.Add(Info);
	}

	return Results;
}

bool UAnimSequenceService::GetCurveInfo(
	const FString& AnimPath,
	const FString& CurveName,
	FAnimCurveInfo& OutInfo)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	const FRawCurveTracks& CurveData = AnimSeq->GetCurveData();

	for (const FFloatCurve& Curve : CurveData.FloatCurves)
	{
		if (Curve.GetName().ToString().Equals(CurveName, ESearchCase::IgnoreCase))
		{
			OutInfo.CurveName = Curve.GetName().ToString();
			OutInfo.CurveType = TEXT("Float");
			OutInfo.KeyCount = Curve.FloatCurve.GetNumKeys();
			OutInfo.DefaultValue = Curve.FloatCurve.GetDefaultValue();
			OutInfo.bMorphTarget = false; // Deprecated flag check removed
			OutInfo.bMaterial = false; // Deprecated flag check removed
			return true;
		}
	}

	return false;
}

bool UAnimSequenceService::GetCurveValueAtTime(
	const FString& AnimPath,
	const FString& CurveName,
	float Time,
	float& OutValue)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	const FRawCurveTracks& CurveData = AnimSeq->GetCurveData();

	for (const FFloatCurve& Curve : CurveData.FloatCurves)
	{
		if (Curve.GetName().ToString().Equals(CurveName, ESearchCase::IgnoreCase))
		{
			OutValue = Curve.FloatCurve.Eval(Time);
			return true;
		}
	}

	return false;
}

TArray<FCurveKeyframe> UAnimSequenceService::GetCurveKeyframes(
	const FString& AnimPath,
	const FString& CurveName)
{
	TArray<FCurveKeyframe> Results;

	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return Results;
	}

	const FRawCurveTracks& CurveData = AnimSeq->GetCurveData();

	for (const FFloatCurve& Curve : CurveData.FloatCurves)
	{
		if (Curve.GetName().ToString().Equals(CurveName, ESearchCase::IgnoreCase))
		{
			// Iterate through keys using the curve's iterator
			for (auto It = Curve.FloatCurve.GetKeyIterator(); It; ++It)
			{
				const FRichCurveKey& Key = *It;
				FCurveKeyframe Keyframe;
				Keyframe.Time = Key.Time;
				Keyframe.Value = Key.Value;
				Keyframe.InterpMode = InterpModeToString(static_cast<int32>(Key.InterpMode));
				Keyframe.TangentMode = TangentModeToString(static_cast<int32>(Key.TangentMode));
				Keyframe.ArriveTangent = Key.ArriveTangent;
				Keyframe.LeaveTangent = Key.LeaveTangent;
				Results.Add(Keyframe);
			}
			break;
		}
	}

	return Results;
}

bool UAnimSequenceService::AddCurve(
	const FString& AnimPath,
	const FString& CurveName,
	bool bIsMorphTarget)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	FName CurveNameFN(*CurveName);

	// Add the curve through the data controller
	IAnimationDataController& Controller = AnimSeq->GetController();

	FAnimationCurveIdentifier CurveId(CurveNameFN, ERawCurveTrackTypes::RCT_Float);
	if (!Controller.AddCurve(CurveId))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::AddCurve: Failed to add curve: %s"), *CurveName);
		return false;
	}

	AnimSeq->MarkPackageDirty();
	return true;
}

bool UAnimSequenceService::RemoveCurve(
	const FString& AnimPath,
	const FString& CurveName)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	IAnimationDataController& Controller = AnimSeq->GetController();

	FAnimationCurveIdentifier CurveId(FName(*CurveName), ERawCurveTrackTypes::RCT_Float);
	if (!Controller.RemoveCurve(CurveId))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::RemoveCurve: Failed to remove curve: %s"), *CurveName);
		return false;
	}

	AnimSeq->MarkPackageDirty();
	return true;
}

bool UAnimSequenceService::SetCurveKeys(
	const FString& AnimPath,
	const FString& CurveName,
	const TArray<FCurveKeyframe>& Keys)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	IAnimationDataController& Controller = AnimSeq->GetController();
	FAnimationCurveIdentifier CurveId(FName(*CurveName), ERawCurveTrackTypes::RCT_Float);

	// Build rich curve keys
	TArray<FRichCurveKey> RichKeys;
	for (const FCurveKeyframe& Key : Keys)
	{
		FRichCurveKey RichKey;
		RichKey.Time = Key.Time;
		RichKey.Value = Key.Value;
		RichKey.ArriveTangent = Key.ArriveTangent;
		RichKey.LeaveTangent = Key.LeaveTangent;
		RichKey.InterpMode = ERichCurveInterpMode::RCIM_Cubic;
		RichKey.TangentMode = ERichCurveTangentMode::RCTM_Auto;
		RichKeys.Add(RichKey);
	}

	if (!Controller.SetCurveKeys(CurveId, RichKeys))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetCurveKeys: Failed to set keys for curve: %s"), *CurveName);
		return false;
	}

	AnimSeq->MarkPackageDirty();
	return true;
}

bool UAnimSequenceService::AddCurveKey(
	const FString& AnimPath,
	const FString& CurveName,
	float Time,
	float Value)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	// Get existing keys and add the new one
	const FRawCurveTracks& CurveData = AnimSeq->GetCurveData();
	TArray<FRichCurveKey> ExistingKeys;

	for (const FFloatCurve& Curve : CurveData.FloatCurves)
	{
		if (Curve.GetName().ToString().Equals(CurveName, ESearchCase::IgnoreCase))
		{
			for (auto It = Curve.FloatCurve.GetKeyIterator(); It; ++It)
			{
				ExistingKeys.Add(*It);
			}
			break;
		}
	}

	// Add the new key
	FRichCurveKey NewKey;
	NewKey.Time = Time;
	NewKey.Value = Value;
	NewKey.InterpMode = ERichCurveInterpMode::RCIM_Cubic;
	NewKey.TangentMode = ERichCurveTangentMode::RCTM_Auto;
	ExistingKeys.Add(NewKey);

	// Sort by time
	ExistingKeys.Sort([](const FRichCurveKey& A, const FRichCurveKey& B) { return A.Time < B.Time; });

	// Set all keys through the controller
	IAnimationDataController& Controller = AnimSeq->GetController();
	FAnimationCurveIdentifier CurveId(FName(*CurveName), ERawCurveTrackTypes::RCT_Float);

	if (!Controller.SetCurveKeys(CurveId, ExistingKeys))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::AddCurveKey: Failed to add key to curve: %s"), *CurveName);
		return false;
	}

	AnimSeq->MarkPackageDirty();
	return true;
}

// ============================================================================
// ANIM NOTIFIES
// ============================================================================

TArray<FAnimNotifyInfo> UAnimSequenceService::ListNotifies(const FString& AnimPath)
{
	TArray<FAnimNotifyInfo> Results;

	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return Results;
	}

	for (int32 i = 0; i < AnimSeq->Notifies.Num(); ++i)
	{
		const FAnimNotifyEvent& NotifyEvent = AnimSeq->Notifies[i];

		FAnimNotifyInfo Info;
		Info.NotifyIndex = i;
		Info.NotifyName = NotifyEvent.NotifyName.ToString();

		if (NotifyEvent.Notify)
		{
			Info.NotifyClass = NotifyEvent.Notify->GetClass()->GetName();
			Info.bIsState = false;
		}
		else if (NotifyEvent.NotifyStateClass)
		{
			Info.NotifyClass = NotifyEvent.NotifyStateClass->GetClass()->GetName();
			Info.bIsState = true;
		}

		Info.TriggerTime = NotifyEvent.GetTriggerTime();
		Info.Duration = NotifyEvent.GetDuration();
		Info.TrackIndex = NotifyEvent.TrackIndex;
		Info.NotifyColor = NotifyEvent.NotifyColor;

		// Additional properties
		Info.TriggerChance = NotifyEvent.NotifyTriggerChance;
		Info.bTriggerOnServer = NotifyEvent.bTriggerOnDedicatedServer;
		Info.bTriggerOnFollower = NotifyEvent.bTriggerOnFollower;
		Info.TriggerWeightThreshold = NotifyEvent.TriggerWeightThreshold;
		Info.NotifyFilterLOD = NotifyEvent.NotifyFilterLOD;

		// Convert filter type enum to string
		switch (NotifyEvent.NotifyFilterType)
		{
		case ENotifyFilterType::NoFiltering:
			Info.NotifyFilterType = TEXT("NoFiltering");
			break;
		case ENotifyFilterType::LOD:
			Info.NotifyFilterType = TEXT("LOD");
			break;
		default:
			Info.NotifyFilterType = TEXT("NoFiltering");
			break;
		}

		Results.Add(Info);
	}

	return Results;
}

bool UAnimSequenceService::GetNotifyInfo(
	const FString& AnimPath,
	int32 NotifyIndex,
	FAnimNotifyInfo& OutInfo)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->Notifies.IsValidIndex(NotifyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::GetNotifyInfo: Invalid index: %d"), NotifyIndex);
		return false;
	}

	const FAnimNotifyEvent& NotifyEvent = AnimSeq->Notifies[NotifyIndex];

	OutInfo.NotifyIndex = NotifyIndex;
	OutInfo.NotifyName = NotifyEvent.NotifyName.ToString();

	if (NotifyEvent.Notify)
	{
		OutInfo.NotifyClass = NotifyEvent.Notify->GetClass()->GetName();
		OutInfo.bIsState = false;
	}
	else if (NotifyEvent.NotifyStateClass)
	{
		OutInfo.NotifyClass = NotifyEvent.NotifyStateClass->GetClass()->GetName();
		OutInfo.bIsState = true;
	}

	OutInfo.TriggerTime = NotifyEvent.GetTriggerTime();
	OutInfo.Duration = NotifyEvent.GetDuration();
	OutInfo.TrackIndex = NotifyEvent.TrackIndex;
	OutInfo.NotifyColor = NotifyEvent.NotifyColor;

	// Additional properties
	OutInfo.TriggerChance = NotifyEvent.NotifyTriggerChance;
	OutInfo.bTriggerOnServer = NotifyEvent.bTriggerOnDedicatedServer;
	OutInfo.bTriggerOnFollower = NotifyEvent.bTriggerOnFollower;
	OutInfo.TriggerWeightThreshold = NotifyEvent.TriggerWeightThreshold;
	OutInfo.NotifyFilterLOD = NotifyEvent.NotifyFilterLOD;

	// Convert filter type enum to string
	switch (NotifyEvent.NotifyFilterType)
	{
	case ENotifyFilterType::NoFiltering:
		OutInfo.NotifyFilterType = TEXT("NoFiltering");
		break;
	case ENotifyFilterType::LOD:
		OutInfo.NotifyFilterType = TEXT("LOD");
		break;
	default:
		OutInfo.NotifyFilterType = TEXT("NoFiltering");
		break;
	}

	return true;
}

int32 UAnimSequenceService::AddNotify(
	const FString& AnimPath,
	const FString& NotifyClass,
	float TriggerTime,
	const FString& NotifyName)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return -1;
	}

	// Find the notify class
	UClass* NotifyUClass = FindObject<UClass>(nullptr, *NotifyClass);
	if (!NotifyUClass)
	{
		// Try with full path
		NotifyUClass = LoadClass<UAnimNotify>(nullptr, *NotifyClass);
	}

	// Determine if we're using the base AnimNotify class with a custom name
	// In this case, we should create a "skeleton notify" (name only, no class object)
	// because UAnimNotify::GetNotifyName() returns the class display name by default
	// In UE5.7+, the base UAnimNotify is abstract and cannot be instantiated anyway
	const bool bIsBaseAnimNotify = NotifyUClass && NotifyUClass == UAnimNotify::StaticClass();
	const bool bHasCustomName = !NotifyName.IsEmpty();
	const bool bIsAbstract = NotifyUClass && NotifyUClass->HasAnyClassFlags(CLASS_Abstract);
	const bool bCreateSkeletonNotify = bIsBaseAnimNotify || bIsAbstract;

	if (!NotifyUClass && !bHasCustomName)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::AddNotify: Could not find notify class: %s"), *NotifyClass);
		return -1;
	}

	// If abstract class and no custom name, warn the user
	if (bIsAbstract && !bHasCustomName)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::AddNotify: Class '%s' is abstract. Creating skeleton notify with default name 'Notify'. Provide a custom name for better editor display."), *NotifyClass);
	}

	AnimSeq->Modify();

	// Create new notify event
	FAnimNotifyEvent& NewNotify = AnimSeq->Notifies.AddDefaulted_GetRef();
	
	// Determine the notify name - use provided name or fall back to class name
	FName FinalNotifyName;
	if (!NotifyName.IsEmpty())
	{
		FinalNotifyName = FName(*NotifyName);
	}
	else if (NotifyUClass)
	{
		// Use the class display name (e.g., "AnimNotify" from "UAnimNotify")
		FinalNotifyName = FName(*NotifyUClass->GetDisplayNameText().ToString());
	}
	else
	{
		FinalNotifyName = FName(TEXT("Notify"));
	}
	NewNotify.NotifyName = FinalNotifyName;
	
	NewNotify.Link(AnimSeq, TriggerTime);
	NewNotify.TriggerTimeOffset = GetTriggerTimeOffsetForType(AnimSeq->CalculateOffsetForNotify(TriggerTime));
	NewNotify.TrackIndex = 0;

	// Only create a notify object if:
	// 1. We have a non-base notify class (custom behavior), OR
	// 2. We're using base class without a custom name
	// For base AnimNotify with custom names, we create a "skeleton notify" (name only)
	// which displays the NotifyName in the editor instead of "AnimNotify"
	if (NotifyUClass && !bCreateSkeletonNotify)
	{
		NewNotify.Notify = NewObject<UAnimNotify>(AnimSeq, NotifyUClass, FinalNotifyName, RF_Transactional);
		
		// Warn if using a non-base class with custom name - the name won't display in editor
		if (bHasCustomName && !bIsBaseAnimNotify)
		{
			UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::AddNotify: Custom name '%s' provided with non-base notify class '%s'. The editor will display the class name instead. Use /Script/Engine.AnimNotify for custom-named notifies."), *NotifyName, *NotifyClass);
		}
	}
	// else: skeleton notify - Notify stays null, displays NotifyName in editor

	AnimSeq->MarkPackageDirty();
	AnimSeq->RefreshCacheData();

	return AnimSeq->Notifies.Num() - 1;
}

int32 UAnimSequenceService::AddNotifyState(
	const FString& AnimPath,
	const FString& NotifyStateClass,
	float StartTime,
	float Duration,
	const FString& NotifyName)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return -1;
	}

	// Find the notify state class
	UClass* NotifyStateUClass = FindObject<UClass>(nullptr, *NotifyStateClass);
	if (!NotifyStateUClass)
	{
		NotifyStateUClass = LoadClass<UAnimNotifyState>(nullptr, *NotifyStateClass);
	}

	// Note: Unlike instant notifies, state notifies REQUIRE a NotifyStateClass object
	// to function correctly (for duration to work). We cannot create "skeleton state notifies".
	// The custom name is set on both NotifyName property and the object name, but the
	// editor will display the class name from GetNotifyName() for state notifies.

	if (!NotifyStateUClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::AddNotifyState: Could not find notify state class: %s"), *NotifyStateClass);
		return -1;
	}

	// Check if the class is abstract - can't instantiate abstract classes
	const bool bIsAbstract = NotifyStateUClass->HasAnyClassFlags(CLASS_Abstract);
	if (bIsAbstract)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::AddNotifyState: Cannot instantiate abstract class: %s. Use a concrete notify state class like AnimNotify_PlaySound or a custom subclass."), *NotifyStateClass);
		return -1;
	}

	AnimSeq->Modify();

	// Create new notify event
	FAnimNotifyEvent& NewNotify = AnimSeq->Notifies.AddDefaulted_GetRef();
	
	// Determine the notify name - use provided name or fall back to class name
	FName FinalNotifyName;
	if (!NotifyName.IsEmpty())
	{
		FinalNotifyName = FName(*NotifyName);
	}
	else if (NotifyStateUClass)
	{
		// Use the class display name (e.g., "AnimNotifyState Trail" from "UAnimNotifyState_Trail")
		FinalNotifyName = FName(*NotifyStateUClass->GetDisplayNameText().ToString());
	}
	else
	{
		FinalNotifyName = FName(TEXT("NotifyState"));
	}
	NewNotify.NotifyName = FinalNotifyName;
	
	NewNotify.Link(AnimSeq, StartTime);
	NewNotify.TriggerTimeOffset = GetTriggerTimeOffsetForType(AnimSeq->CalculateOffsetForNotify(StartTime));
	NewNotify.TrackIndex = 0;
	NewNotify.SetDuration(Duration);

	// Always create a notify state object - state notifies require it for duration to work
	// Unlike instant notifies, skeleton state notifies don't function correctly
	NewNotify.NotifyStateClass = NewObject<UAnimNotifyState>(AnimSeq, NotifyStateUClass, FinalNotifyName, RF_Transactional);

	AnimSeq->MarkPackageDirty();
	AnimSeq->RefreshCacheData();

	return AnimSeq->Notifies.Num() - 1;
}

bool UAnimSequenceService::RemoveNotify(const FString& AnimPath, int32 NotifyIndex)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->Notifies.IsValidIndex(NotifyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::RemoveNotify: Invalid index: %d"), NotifyIndex);
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->Notifies.RemoveAt(NotifyIndex);
	AnimSeq->MarkPackageDirty();
	AnimSeq->RefreshCacheData();

	return true;
}

bool UAnimSequenceService::SetNotifyTriggerTime(
	const FString& AnimPath,
	int32 NotifyIndex,
	float NewTime)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->Notifies.IsValidIndex(NotifyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetNotifyTriggerTime: Invalid index: %d"), NotifyIndex);
		return false;
	}

	AnimSeq->Modify();
	FAnimNotifyEvent& Notify = AnimSeq->Notifies[NotifyIndex];
	Notify.Link(AnimSeq, NewTime);
	Notify.TriggerTimeOffset = GetTriggerTimeOffsetForType(AnimSeq->CalculateOffsetForNotify(NewTime));
	AnimSeq->MarkPackageDirty();
	AnimSeq->RefreshCacheData();

	return true;
}

bool UAnimSequenceService::SetNotifyDuration(
	const FString& AnimPath,
	int32 NotifyIndex,
	float NewDuration)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->Notifies.IsValidIndex(NotifyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetNotifyDuration: Invalid index: %d"), NotifyIndex);
		return false;
	}

	FAnimNotifyEvent& Notify = AnimSeq->Notifies[NotifyIndex];
	if (!Notify.NotifyStateClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetNotifyDuration: Not a state notify at index: %d"), NotifyIndex);
		return false;
	}

	AnimSeq->Modify();
	Notify.SetDuration(NewDuration);
	AnimSeq->MarkPackageDirty();
	AnimSeq->RefreshCacheData();

	return true;
}

bool UAnimSequenceService::SetNotifyTrack(
	const FString& AnimPath,
	int32 NotifyIndex,
	int32 TrackIndex)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->Notifies.IsValidIndex(NotifyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetNotifyTrack: Invalid index: %d"), NotifyIndex);
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->Notifies[NotifyIndex].TrackIndex = TrackIndex;
	AnimSeq->MarkPackageDirty();

	return true;
}

bool UAnimSequenceService::SetNotifyName(
	const FString& AnimPath,
	int32 NotifyIndex,
	const FString& NewName)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->Notifies.IsValidIndex(NotifyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetNotifyName: Invalid index: %d"), NotifyIndex);
		return false;
	}

	AnimSeq->Modify();
	
	FAnimNotifyEvent& NotifyEvent = AnimSeq->Notifies[NotifyIndex];
	NotifyEvent.NotifyName = FName(*NewName);
	
	// If this notify has a UAnimNotify object and it's the base AnimNotify class,
	// we need to convert it to a "skeleton notify" (no Notify object) so the editor
	// displays our custom NotifyName instead of calling GetNotifyName() on the object
	// which returns the class display name.
	if (NotifyEvent.Notify && NotifyEvent.Notify->GetClass() == UAnimNotify::StaticClass())
	{
		// Clear the notify object to convert to skeleton notify
		// This makes the editor display NotifyName instead of class name
		NotifyEvent.Notify = nullptr;
		UE_LOG(LogTemp, Log, TEXT("UAnimSequenceService::SetNotifyName: Converted base AnimNotify to skeleton notify for custom name display"));
	}
	
	AnimSeq->MarkPackageDirty();
	AnimSeq->RefreshCacheData();

	return true;
}

bool UAnimSequenceService::SetNotifyColor(
	const FString& AnimPath,
	int32 NotifyIndex,
	FLinearColor NewColor)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->Notifies.IsValidIndex(NotifyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetNotifyColor: Invalid index: %d"), NotifyIndex);
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->Notifies[NotifyIndex].NotifyColor = NewColor.ToFColor(true);
	AnimSeq->MarkPackageDirty();

	return true;
}

bool UAnimSequenceService::SetNotifyTriggerChance(
	const FString& AnimPath,
	int32 NotifyIndex,
	float TriggerChance)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->Notifies.IsValidIndex(NotifyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetNotifyTriggerChance: Invalid index: %d"), NotifyIndex);
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->Notifies[NotifyIndex].NotifyTriggerChance = FMath::Clamp(TriggerChance, 0.0f, 1.0f);
	AnimSeq->MarkPackageDirty();

	return true;
}

bool UAnimSequenceService::SetNotifyTriggerOnServer(
	const FString& AnimPath,
	int32 NotifyIndex,
	bool bTriggerOnServer)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->Notifies.IsValidIndex(NotifyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetNotifyTriggerOnServer: Invalid index: %d"), NotifyIndex);
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->Notifies[NotifyIndex].bTriggerOnDedicatedServer = bTriggerOnServer;
	AnimSeq->MarkPackageDirty();

	return true;
}

bool UAnimSequenceService::SetNotifyTriggerOnFollower(
	const FString& AnimPath,
	int32 NotifyIndex,
	bool bTriggerOnFollower)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->Notifies.IsValidIndex(NotifyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetNotifyTriggerOnFollower: Invalid index: %d"), NotifyIndex);
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->Notifies[NotifyIndex].bTriggerOnFollower = bTriggerOnFollower;
	AnimSeq->MarkPackageDirty();

	return true;
}

bool UAnimSequenceService::SetNotifyTriggerWeightThreshold(
	const FString& AnimPath,
	int32 NotifyIndex,
	float WeightThreshold)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->Notifies.IsValidIndex(NotifyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetNotifyTriggerWeightThreshold: Invalid index: %d"), NotifyIndex);
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->Notifies[NotifyIndex].TriggerWeightThreshold = FMath::Clamp(WeightThreshold, 0.0f, 1.0f);
	AnimSeq->MarkPackageDirty();

	return true;
}

bool UAnimSequenceService::SetNotifyLODFilter(
	const FString& AnimPath,
	int32 NotifyIndex,
	const FString& FilterType,
	int32 FilterLOD)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->Notifies.IsValidIndex(NotifyIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetNotifyLODFilter: Invalid index: %d"), NotifyIndex);
		return false;
	}

	// Parse filter type string
	ENotifyFilterType::Type ParsedFilterType = ENotifyFilterType::NoFiltering;
	if (FilterType.Equals(TEXT("LOD"), ESearchCase::IgnoreCase))
	{
		ParsedFilterType = ENotifyFilterType::LOD;
	}
	// Note: ENotifyFilterType only has NoFiltering and LOD in UE5

	AnimSeq->Modify();
	AnimSeq->Notifies[NotifyIndex].NotifyFilterType = ParsedFilterType;
	AnimSeq->Notifies[NotifyIndex].NotifyFilterLOD = FMath::Max(0, FilterLOD);
	AnimSeq->MarkPackageDirty();

	return true;
}

// ============================================================================
// NOTIFY TRACKS
// ============================================================================

TArray<FString> UAnimSequenceService::ListNotifyTracks(const FString& AnimPath)
{
	TArray<FString> Results;

	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return Results;
	}

	// Find the maximum track index used by notifies
	int32 MaxTrackIndex = -1;
	for (const FAnimNotifyEvent& Notify : AnimSeq->Notifies)
	{
		MaxTrackIndex = FMath::Max(MaxTrackIndex, Notify.TrackIndex);
	}

	// Generate track names (UE uses implicit track naming like "1", "2", etc.)
	for (int32 i = 0; i <= MaxTrackIndex; ++i)
	{
		Results.Add(FString::Printf(TEXT("Track %d"), i + 1));
	}

	return Results;
}

int32 UAnimSequenceService::GetNotifyTrackCount(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return -1;
	}

	// Track count is the max track index + 1 (or 1 if no notifies exist, as there's always at least one implicit track)
	int32 MaxTrackIndex = 0;
	for (const FAnimNotifyEvent& Notify : AnimSeq->Notifies)
	{
		MaxTrackIndex = FMath::Max(MaxTrackIndex, Notify.TrackIndex);
	}

	return MaxTrackIndex + 1;
}

int32 UAnimSequenceService::AddNotifyTrack(
	const FString& AnimPath,
	const FString& TrackName)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return -1;
	}

	// In UE 5.7, notify tracks are implicit - they're created when you place a notify on a higher track index.
	// Find the current max track index and return the next one.
	// The track will become "active" when a notify is placed on it.
	int32 MaxTrackIndex = -1;
	for (const FAnimNotifyEvent& Notify : AnimSeq->Notifies)
	{
		MaxTrackIndex = FMath::Max(MaxTrackIndex, Notify.TrackIndex);
	}

	int32 NewTrackIndex = MaxTrackIndex + 1;
	
	UE_LOG(LogTemp, Log, TEXT("UAnimSequenceService::AddNotifyTrack: New track index %d available (name: '%s' is informational only - UE uses implicit tracks)"), 
		NewTrackIndex, *TrackName);

	// Note: The track doesn't truly exist until a notify is placed on it.
	// Return the next available track index.
	return NewTrackIndex;
}

bool UAnimSequenceService::RenameNotifyTrack(
	const FString& AnimPath,
	int32 TrackIndex,
	const FString& NewName)
{
	// In UE 5.7, notify tracks don't have editable names - they're just indexed.
	// This operation is not supported in the current engine version.
	UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::RenameNotifyTrack: Notify tracks in UE 5.7 are implicitly named by index. Custom names are not supported."));
	return false;
}

bool UAnimSequenceService::RemoveNotifyTrack(
	const FString& AnimPath,
	int32 TrackIndex)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (TrackIndex < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::RemoveNotifyTrack: Invalid track index: %d"), TrackIndex);
		return false;
	}

	AnimSeq->Modify();

	bool bFoundNotifiesOnTrack = false;

	// Move any notifies on this track to track 0 (the first track)
	for (FAnimNotifyEvent& Notify : AnimSeq->Notifies)
	{
		if (Notify.TrackIndex == TrackIndex)
		{
			Notify.TrackIndex = 0;
			bFoundNotifiesOnTrack = true;
		}
		else if (Notify.TrackIndex > TrackIndex)
		{
			// Decrement track indices for notifies on higher tracks
			Notify.TrackIndex--;
		}
	}

	AnimSeq->MarkPackageDirty();

	UE_LOG(LogTemp, Log, TEXT("UAnimSequenceService::RemoveNotifyTrack: Removed track %d, moved %s notifies to track 0"), 
		TrackIndex, bFoundNotifiesOnTrack ? TEXT("some") : TEXT("no"));

	return true;
}

// ============================================================================
// SYNC MARKERS
// ============================================================================

TArray<FSyncMarkerInfo> UAnimSequenceService::ListSyncMarkers(const FString& AnimPath)
{
	TArray<FSyncMarkerInfo> Results;

	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return Results;
	}

	for (const FAnimSyncMarker& Marker : AnimSeq->AuthoredSyncMarkers)
	{
		FSyncMarkerInfo Info;
		Info.MarkerName = Marker.MarkerName.ToString();
		Info.Time = Marker.Time;
		Info.TrackIndex = Marker.TrackIndex;
		Results.Add(Info);
	}

	return Results;
}

bool UAnimSequenceService::AddSyncMarker(
	const FString& AnimPath,
	const FString& MarkerName,
	float Time)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	AnimSeq->Modify();

	FAnimSyncMarker NewMarker;
	NewMarker.MarkerName = FName(*MarkerName);
	NewMarker.Time = Time;
	NewMarker.TrackIndex = 0;

	AnimSeq->AuthoredSyncMarkers.Add(NewMarker);

	// Sort markers by time
	AnimSeq->AuthoredSyncMarkers.Sort([](const FAnimSyncMarker& A, const FAnimSyncMarker& B)
	{
		return A.Time < B.Time;
	});

	AnimSeq->MarkPackageDirty();
	return true;
}

bool UAnimSequenceService::RemoveSyncMarker(
	const FString& AnimPath,
	const FString& MarkerName,
	float Time)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	AnimSeq->Modify();

	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < AnimSeq->AuthoredSyncMarkers.Num(); ++i)
	{
		const FAnimSyncMarker& Marker = AnimSeq->AuthoredSyncMarkers[i];
		if (Marker.MarkerName.ToString().Equals(MarkerName) && FMath::IsNearlyEqual(Marker.Time, Time, 0.001f))
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::RemoveSyncMarker: Marker not found: %s at %f"), *MarkerName, Time);
		return false;
	}

	AnimSeq->AuthoredSyncMarkers.RemoveAt(FoundIndex);
	AnimSeq->MarkPackageDirty();

	return true;
}

bool UAnimSequenceService::SetSyncMarkerTime(
	const FString& AnimPath,
	int32 MarkerIndex,
	float NewTime)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (!AnimSeq->AuthoredSyncMarkers.IsValidIndex(MarkerIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetSyncMarkerTime: Invalid index: %d"), MarkerIndex);
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->AuthoredSyncMarkers[MarkerIndex].Time = NewTime;

	// Re-sort markers
	AnimSeq->AuthoredSyncMarkers.Sort([](const FAnimSyncMarker& A, const FAnimSyncMarker& B)
	{
		return A.Time < B.Time;
	});

	AnimSeq->MarkPackageDirty();
	return true;
}

bool UAnimSequenceService::SetSyncMarkerTimeByName(
	const FString& AnimPath,
	const FString& MarkerName,
	float CurrentTime,
	float NewTime)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	// Find the marker by name and current time
	int32 FoundIndex = INDEX_NONE;
	for (int32 i = 0; i < AnimSeq->AuthoredSyncMarkers.Num(); ++i)
	{
		const FAnimSyncMarker& Marker = AnimSeq->AuthoredSyncMarkers[i];
		if (Marker.MarkerName.ToString().Equals(MarkerName) && FMath::IsNearlyEqual(Marker.Time, CurrentTime, 0.001f))
		{
			FoundIndex = i;
			break;
		}
	}

	if (FoundIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetSyncMarkerTimeByName: Marker not found: %s at %f"), *MarkerName, CurrentTime);
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->AuthoredSyncMarkers[FoundIndex].Time = NewTime;

	// Re-sort markers
	AnimSeq->AuthoredSyncMarkers.Sort([](const FAnimSyncMarker& A, const FAnimSyncMarker& B)
	{
		return A.Time < B.Time;
	});

	AnimSeq->MarkPackageDirty();
	return true;
}

// ============================================================================
// ADDITIVE ANIMATION
// ============================================================================

FString UAnimSequenceService::GetAdditiveAnimType(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return TEXT("");
	}

	return AdditiveTypeToString(static_cast<int32>(AnimSeq->AdditiveAnimType));
}

bool UAnimSequenceService::SetAdditiveAnimType(const FString& AnimPath, const FString& Type)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->AdditiveAnimType = static_cast<EAdditiveAnimationType>(StringToAdditiveType(Type));
	AnimSeq->MarkPackageDirty();

	return true;
}

FString UAnimSequenceService::GetAdditiveBasePose(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return FString();
	}

	if (AnimSeq->RefPoseSeq)
	{
		return AnimSeq->RefPoseSeq->GetPathName();
	}

	return FString();
}

bool UAnimSequenceService::SetAdditiveBasePose(
	const FString& AnimPath,
	const FString& BasePoseAnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	UAnimSequence* BasePoseAnim = LoadAnimSequence(BasePoseAnimPath);
	if (!BasePoseAnim && !BasePoseAnimPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetAdditiveBasePose: Could not load base pose: %s"), *BasePoseAnimPath);
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->RefPoseSeq = BasePoseAnim;
	AnimSeq->RefPoseType = BasePoseAnim ? EAdditiveBasePoseType::ABPT_AnimScaled : EAdditiveBasePoseType::ABPT_RefPose;
	AnimSeq->MarkPackageDirty();

	return true;
}

// ============================================================================
// ROOT MOTION
// ============================================================================

bool UAnimSequenceService::GetEnableRootMotion(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	return AnimSeq->bEnableRootMotion;
}

bool UAnimSequenceService::SetEnableRootMotion(const FString& AnimPath, bool bEnable)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->bEnableRootMotion = bEnable;
	AnimSeq->MarkPackageDirty();

	return true;
}

FString UAnimSequenceService::GetRootMotionRootLock(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return TEXT("");
	}

	return RootLockToString(static_cast<int32>(AnimSeq->RootMotionRootLock));
}

bool UAnimSequenceService::SetRootMotionRootLock(const FString& AnimPath, const FString& LockType)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->RootMotionRootLock = static_cast<ERootMotionRootLock::Type>(StringToRootLock(LockType));
	AnimSeq->MarkPackageDirty();

	return true;
}

bool UAnimSequenceService::GetForceRootLock(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	return AnimSeq->bForceRootLock;
}

bool UAnimSequenceService::SetForceRootLock(const FString& AnimPath, bool bForce)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->bForceRootLock = bForce;
	AnimSeq->MarkPackageDirty();

	return true;
}

// ============================================================================
// COMPRESSION
// ============================================================================

bool UAnimSequenceService::GetCompressionInfo(
	const FString& AnimPath,
	FAnimCompressionInfo& OutInfo)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	OutInfo.RawSize = AnimSeq->GetApproxRawSize();
	OutInfo.CompressedSize = AnimSeq->GetApproxCompressedSize();

	if (OutInfo.RawSize > 0)
	{
		OutInfo.CompressionRatio = static_cast<float>(OutInfo.CompressedSize) / static_cast<float>(OutInfo.RawSize);
	}

	// Get compression scheme name from settings if available
	if (AnimSeq->BoneCompressionSettings)
	{
		OutInfo.CompressionScheme = AnimSeq->BoneCompressionSettings->GetName();
	}

	return true;
}

bool UAnimSequenceService::SetCompressionScheme(
	const FString& AnimPath,
	const FString& CompressionSchemePath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	// Load compression settings
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(CompressionSchemePath);
	UAnimBoneCompressionSettings* CompressionSettings = Cast<UAnimBoneCompressionSettings>(LoadedObject);

	if (!CompressionSettings)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetCompressionScheme: Could not load compression settings: %s"), *CompressionSchemePath);
		return false;
	}

	AnimSeq->Modify();
	AnimSeq->BoneCompressionSettings = CompressionSettings;
	AnimSeq->MarkPackageDirty();

	return true;
}

bool UAnimSequenceService::CompressAnimation(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	AnimSeq->Modify();
	// Request recompression through PostEditChange which triggers compression
	AnimSeq->PostEditChange();
	AnimSeq->MarkPackageDirty();

	return true;
}

// ============================================================================
// IMPORT/EXPORT
// ============================================================================

FString UAnimSequenceService::ExportAnimationToJson(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return TEXT("{}");
	}

	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject());

	// Basic info
	RootObject->SetStringField(TEXT("name"), AnimSeq->GetName());
	RootObject->SetStringField(TEXT("path"), AnimSeq->GetPathName());
	RootObject->SetNumberField(TEXT("duration"), AnimSeq->GetPlayLength());
	RootObject->SetNumberField(TEXT("frameRate"), AnimSeq->GetSamplingFrameRate().AsDecimal());
	RootObject->SetNumberField(TEXT("frameCount"), AnimSeq->GetNumberOfSampledKeys());

	if (USkeleton* Skeleton = AnimSeq->GetSkeleton())
	{
		RootObject->SetStringField(TEXT("skeleton"), Skeleton->GetPathName());
	}

	// Animated bones list
	TArray<TSharedPtr<FJsonValue>> BoneNamesArray;
	TArray<FString> AnimatedBones = GetAnimatedBones(AnimPath);
	for (const FString& BoneName : AnimatedBones)
	{
		BoneNamesArray.Add(MakeShareable(new FJsonValueString(BoneName)));
	}
	RootObject->SetArrayField(TEXT("animatedBones"), BoneNamesArray);

	// Curves info
	TArray<TSharedPtr<FJsonValue>> CurvesArray;
	TArray<FAnimCurveInfo> Curves = ListCurves(AnimPath);
	for (const FAnimCurveInfo& Curve : Curves)
	{
		TSharedPtr<FJsonObject> CurveObj = MakeShareable(new FJsonObject());
		CurveObj->SetStringField(TEXT("name"), Curve.CurveName);
		CurveObj->SetStringField(TEXT("type"), Curve.CurveType);
		CurveObj->SetNumberField(TEXT("keyCount"), Curve.KeyCount);
		CurvesArray.Add(MakeShareable(new FJsonValueObject(CurveObj)));
	}
	RootObject->SetArrayField(TEXT("curves"), CurvesArray);

	// Notifies info
	TArray<TSharedPtr<FJsonValue>> NotifiesArray;
	TArray<FAnimNotifyInfo> Notifies = ListNotifies(AnimPath);
	for (const FAnimNotifyInfo& Notify : Notifies)
	{
		TSharedPtr<FJsonObject> NotifyObj = MakeShareable(new FJsonObject());
		NotifyObj->SetStringField(TEXT("name"), Notify.NotifyName);
		NotifyObj->SetStringField(TEXT("class"), Notify.NotifyClass);
		NotifyObj->SetNumberField(TEXT("time"), Notify.TriggerTime);
		NotifyObj->SetNumberField(TEXT("duration"), Notify.Duration);
		NotifyObj->SetBoolField(TEXT("isState"), Notify.bIsState);
		NotifiesArray.Add(MakeShareable(new FJsonValueObject(NotifyObj)));
	}
	RootObject->SetArrayField(TEXT("notifies"), NotifiesArray);

	// Convert to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	return OutputString;
}

TArray<FString> UAnimSequenceService::GetSourceFiles(const FString& AnimPath)
{
	TArray<FString> Results;

	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return Results;
	}

	// Get import data
	if (UAssetImportData* ImportData = AnimSeq->AssetImportData)
	{
		ImportData->ExtractFilenames(Results);
	}

	return Results;
}

// ============================================================================
// EDITOR NAVIGATION
// ============================================================================

bool UAnimSequenceService::OpenAnimationEditor(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (GEditor)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AnimSeq);
		return true;
	}

	return false;
}

bool UAnimSequenceService::SetPreviewTime(const FString& AnimPath, float Time)
{
	// Note: This would require access to the animation editor's viewport
	// Currently just validates the path and time
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	if (Time < 0.0f || Time > AnimSeq->GetPlayLength())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimSequenceService::SetPreviewTime: Time out of range: %f"), Time);
		return false;
	}

	// TODO: Implement through IPersonaToolkit if editor is open
	UE_LOG(LogTemp, Log, TEXT("UAnimSequenceService::SetPreviewTime: Would set preview time to %f for %s"), Time, *AnimPath);
	return true;
}

bool UAnimSequenceService::PlayPreview(const FString& AnimPath, bool bLoop)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	// TODO: Implement through IPersonaToolkit if editor is open
	UE_LOG(LogTemp, Log, TEXT("UAnimSequenceService::PlayPreview: Would play preview for %s (loop: %d)"), *AnimPath, bLoop);
	return true;
}

bool UAnimSequenceService::StopPreview(const FString& AnimPath)
{
	UAnimSequence* AnimSeq = LoadAnimSequence(AnimPath);
	if (!AnimSeq)
	{
		return false;
	}

	// TODO: Implement through IPersonaToolkit if editor is open
	UE_LOG(LogTemp, Log, TEXT("UAnimSequenceService::StopPreview: Would stop preview for %s"), *AnimPath);
	return true;
}
