// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/USkeletonService.h"

#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/BlendProfile.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "SkeletalMeshEditorSubsystem.h"
#include "SkeletonModifier.h"
#include "PhysicsEngine/PhysicsAsset.h"

// Static map for skeleton modifiers - using TStrongObjectPtr for GC safety
TMap<FString, TStrongObjectPtr<USkeletonModifier>> USkeletonService::ActiveModifiers;

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

USkeleton* USkeletonService::LoadSkeleton(const FString& SkeletonPath)
{
	if (SkeletonPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::LoadSkeleton: Path is empty"));
		return nullptr;
	}

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(SkeletonPath);
	if (!LoadedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::LoadSkeleton: Failed to load: %s"), *SkeletonPath);
		return nullptr;
	}

	USkeleton* Skeleton = Cast<USkeleton>(LoadedObject);
	if (!Skeleton)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::LoadSkeleton: Not a Skeleton: %s"), *SkeletonPath);
		return nullptr;
	}

	return Skeleton;
}

USkeletalMesh* USkeletonService::LoadSkeletalMesh(const FString& SkeletalMeshPath)
{
	if (SkeletalMeshPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::LoadSkeletalMesh: Path is empty"));
		return nullptr;
	}

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(SkeletalMeshPath);
	if (!LoadedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::LoadSkeletalMesh: Failed to load: %s"), *SkeletalMeshPath);
		return nullptr;
	}

	USkeletalMesh* Mesh = Cast<USkeletalMesh>(LoadedObject);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::LoadSkeletalMesh: Not a SkeletalMesh: %s"), *SkeletalMeshPath);
		return nullptr;
	}

	return Mesh;
}

USkeleton* USkeletonService::GetSkeletonFromAsset(const FString& AssetPath)
{
	// Try loading as Skeleton first
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!LoadedObject)
	{
		return nullptr;
	}

	// Check if it's a Skeleton
	USkeleton* Skeleton = Cast<USkeleton>(LoadedObject);
	if (Skeleton)
	{
		return Skeleton;
	}

	// Check if it's a SkeletalMesh and get its Skeleton
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(LoadedObject);
	if (Mesh)
	{
		return Mesh->GetSkeleton();
	}

	return nullptr;
}

const FReferenceSkeleton* USkeletonService::GetReferenceSkeleton(const FString& AssetPath)
{
	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!LoadedObject)
	{
		return nullptr;
	}

	// Check if it's a Skeleton
	USkeleton* Skeleton = Cast<USkeleton>(LoadedObject);
	if (Skeleton)
	{
		return &Skeleton->GetReferenceSkeleton();
	}

	// Check if it's a SkeletalMesh
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(LoadedObject);
	if (Mesh)
	{
		return &Mesh->GetRefSkeleton();
	}

	return nullptr;
}

FString USkeletonService::RetargetingModeToString(EBoneTranslationRetargetingMode::Type Mode)
{
	switch (Mode)
	{
		case EBoneTranslationRetargetingMode::Animation:
			return TEXT("Animation");
		case EBoneTranslationRetargetingMode::Skeleton:
			return TEXT("Skeleton");
		case EBoneTranslationRetargetingMode::AnimationScaled:
			return TEXT("AnimationScaled");
		case EBoneTranslationRetargetingMode::AnimationRelative:
			return TEXT("AnimationRelative");
		case EBoneTranslationRetargetingMode::OrientAndScale:
			return TEXT("OrientAndScale");
		default:
			return TEXT("Animation");
	}
}

EBoneTranslationRetargetingMode::Type USkeletonService::StringToRetargetingMode(const FString& ModeString)
{
	if (ModeString.Equals(TEXT("Skeleton"), ESearchCase::IgnoreCase))
	{
		return EBoneTranslationRetargetingMode::Skeleton;
	}
	else if (ModeString.Equals(TEXT("AnimationScaled"), ESearchCase::IgnoreCase))
	{
		return EBoneTranslationRetargetingMode::AnimationScaled;
	}
	else if (ModeString.Equals(TEXT("AnimationRelative"), ESearchCase::IgnoreCase))
	{
		return EBoneTranslationRetargetingMode::AnimationRelative;
	}
	else if (ModeString.Equals(TEXT("OrientAndScale"), ESearchCase::IgnoreCase))
	{
		return EBoneTranslationRetargetingMode::OrientAndScale;
	}

	return EBoneTranslationRetargetingMode::Animation;
}

USkeletonModifier* USkeletonService::GetSkeletonModifier(const FString& SkeletalMeshPath)
{
	// Check if we already have an active modifier
	if (TStrongObjectPtr<USkeletonModifier>* ExistingModifier = ActiveModifiers.Find(SkeletalMeshPath))
	{
		if (ExistingModifier->IsValid())
		{
			return ExistingModifier->Get();
		}
	}

	// Load the mesh and create a new modifier
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return nullptr;
	}

	USkeletonModifier* Modifier = NewObject<USkeletonModifier>();
	if (!Modifier->SetSkeletalMesh(Mesh))
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::GetSkeletonModifier: Failed to set skeletal mesh"));
		return nullptr;
	}

	ActiveModifiers.Add(SkeletalMeshPath, TStrongObjectPtr<USkeletonModifier>(Modifier));
	return Modifier;
}

void USkeletonService::ClearSkeletonModifier(const FString& SkeletalMeshPath)
{
	ActiveModifiers.Remove(SkeletalMeshPath);
}

// ============================================================================
// SKELETON DISCOVERY
// ============================================================================

TArray<FString> USkeletonService::ListSkeletons(const FString& SearchPath, bool bRecursive)
{
	TArray<FString> Results;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(USkeleton::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(*SearchPath);
	Filter.bRecursivePaths = bRecursive;

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	for (const FAssetData& Asset : AssetList)
	{
		Results.Add(Asset.GetSoftObjectPath().ToString());
	}

	return Results;
}

TArray<FString> USkeletonService::ListSkeletalMeshes(const FString& SearchPath, bool bRecursive)
{
	TArray<FString> Results;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(USkeletalMesh::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(*SearchPath);
	Filter.bRecursivePaths = bRecursive;

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	for (const FAssetData& Asset : AssetList)
	{
		Results.Add(Asset.GetSoftObjectPath().ToString());
	}

	return Results;
}

bool USkeletonService::GetSkeletonInfo(const FString& SkeletonPath, FSkeletonAssetInfo& OutInfo)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return false;
	}

	OutInfo.SkeletonPath = SkeletonPath;
	OutInfo.SkeletonName = Skeleton->GetName();
	OutInfo.BoneCount = Skeleton->GetReferenceSkeleton().GetNum();
	OutInfo.CompatibleSkeletonCount = Skeleton->GetCompatibleSkeletons().Num();

	// Get curve metadata count
	TArray<FName> CurveNames;
	Skeleton->GetCurveMetaDataNames(CurveNames);
	OutInfo.CurveMetaDataCount = CurveNames.Num();

	// Get blend profiles
	OutInfo.BlendProfileCount = Skeleton->BlendProfiles.Num();
	for (UBlendProfile* Profile : Skeleton->BlendProfiles)
	{
		if (Profile)
		{
			OutInfo.BlendProfileNames.Add(Profile->GetName());
		}
	}

	// Forward axis is not directly accessible in UE 5.7 - would need preview mesh settings
	OutInfo.PreviewForwardAxis = TEXT("X"); // Default

	return true;
}

bool USkeletonService::GetSkeletalMeshInfo(const FString& SkeletalMeshPath, FSkeletalMeshData& OutInfo)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return false;
	}

	OutInfo.MeshPath = SkeletalMeshPath;
	OutInfo.MeshName = Mesh->GetName();

	if (USkeleton* Skeleton = Mesh->GetSkeleton())
	{
		OutInfo.SkeletonPath = Skeleton->GetPathName();
	}

	OutInfo.BoneCount = Mesh->GetRefSkeleton().GetNum();
	OutInfo.LODCount = Mesh->GetLODNum();
	OutInfo.SocketCount = Mesh->NumSockets();
	OutInfo.MorphTargetCount = Mesh->GetMorphTargets().Num();
	OutInfo.MaterialCount = Mesh->GetMaterials().Num();

	if (UPhysicsAsset* PhysAsset = Mesh->GetPhysicsAsset())
	{
		OutInfo.PhysicsAssetPath = PhysAsset->GetPathName();
	}

	if (TSubclassOf<UAnimInstance> PostProcessBP = Mesh->GetPostProcessAnimBlueprint())
	{
		if (UBlueprint* BP = Cast<UBlueprint>(PostProcessBP->ClassGeneratedBy))
		{
			OutInfo.PostProcessAnimBPPath = BP->GetPathName();
		}
	}

	FBoxSphereBounds Bounds = Mesh->GetBounds();
	OutInfo.BoundsMin = Bounds.Origin - Bounds.BoxExtent;
	OutInfo.BoundsMax = Bounds.Origin + Bounds.BoxExtent;

	return true;
}

FString USkeletonService::GetSkeletonForMesh(const FString& SkeletalMeshPath)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return FString();
	}

	if (USkeleton* Skeleton = Mesh->GetSkeleton())
	{
		return Skeleton->GetPathName();
	}

	return FString();
}

// ============================================================================
// BONE HIERARCHY
// ============================================================================

TArray<FBoneNodeInfo> USkeletonService::ListBones(const FString& AssetPath)
{
	TArray<FBoneNodeInfo> Results;

	const FReferenceSkeleton* RefSkel = GetReferenceSkeleton(AssetPath);
	if (!RefSkel)
	{
		return Results;
	}

	const int32 NumBones = RefSkel->GetNum();
	Results.Reserve(NumBones);

	// First pass: create all bone entries
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		FBoneNodeInfo BoneInfo;
		BoneInfo.BoneName = RefSkel->GetBoneName(BoneIndex).ToString();
		BoneInfo.BoneIndex = BoneIndex;
		BoneInfo.ParentBoneIndex = RefSkel->GetParentIndex(BoneIndex);

		if (BoneInfo.ParentBoneIndex >= 0)
		{
			BoneInfo.ParentBoneName = RefSkel->GetBoneName(BoneInfo.ParentBoneIndex).ToString();
		}

		BoneInfo.LocalTransform = RefSkel->GetRefBonePose()[BoneIndex];

		// Calculate depth
		int32 Depth = 0;
		int32 ParentIdx = BoneInfo.ParentBoneIndex;
		while (ParentIdx >= 0)
		{
			Depth++;
			ParentIdx = RefSkel->GetParentIndex(ParentIdx);
		}
		BoneInfo.Depth = Depth;

		Results.Add(BoneInfo);
	}

	// Second pass: calculate children and global transforms
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		// Find children
		for (int32 ChildIndex = 0; ChildIndex < NumBones; ++ChildIndex)
		{
			if (RefSkel->GetParentIndex(ChildIndex) == BoneIndex)
			{
				Results[BoneIndex].Children.Add(RefSkel->GetBoneName(ChildIndex).ToString());
				Results[BoneIndex].ChildCount++;
			}
		}

		// Calculate global transform by walking up the hierarchy
		FTransform GlobalTransform = Results[BoneIndex].LocalTransform;
		int32 ParentIdx = Results[BoneIndex].ParentBoneIndex;
		while (ParentIdx >= 0)
		{
			GlobalTransform = GlobalTransform * RefSkel->GetRefBonePose()[ParentIdx];
			ParentIdx = RefSkel->GetParentIndex(ParentIdx);
		}
		Results[BoneIndex].GlobalTransform = GlobalTransform;
	}

	// Get retargeting modes from skeleton if available
	USkeleton* Skeleton = GetSkeletonFromAsset(AssetPath);
	if (Skeleton)
	{
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			EBoneTranslationRetargetingMode::Type Mode = Skeleton->GetBoneTranslationRetargetingMode(BoneIndex);
			Results[BoneIndex].RetargetingMode = RetargetingModeToString(Mode);
		}
	}

	return Results;
}

bool USkeletonService::GetBoneInfo(const FString& AssetPath, const FString& BoneName, FBoneNodeInfo& OutInfo)
{
	const FReferenceSkeleton* RefSkel = GetReferenceSkeleton(AssetPath);
	if (!RefSkel)
	{
		return false;
	}

	int32 BoneIndex = RefSkel->FindBoneIndex(FName(*BoneName));
	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::GetBoneInfo: Bone not found: %s"), *BoneName);
		return false;
	}

	OutInfo.BoneName = BoneName;
	OutInfo.BoneIndex = BoneIndex;
	OutInfo.ParentBoneIndex = RefSkel->GetParentIndex(BoneIndex);

	if (OutInfo.ParentBoneIndex >= 0)
	{
		OutInfo.ParentBoneName = RefSkel->GetBoneName(OutInfo.ParentBoneIndex).ToString();
	}

	OutInfo.LocalTransform = RefSkel->GetRefBonePose()[BoneIndex];

	// Calculate depth
	int32 Depth = 0;
	int32 ParentIdx = OutInfo.ParentBoneIndex;
	while (ParentIdx >= 0)
	{
		Depth++;
		ParentIdx = RefSkel->GetParentIndex(ParentIdx);
	}
	OutInfo.Depth = Depth;

	// Calculate global transform
	FTransform GlobalTransform = OutInfo.LocalTransform;
	ParentIdx = OutInfo.ParentBoneIndex;
	while (ParentIdx >= 0)
	{
		GlobalTransform = GlobalTransform * RefSkel->GetRefBonePose()[ParentIdx];
		ParentIdx = RefSkel->GetParentIndex(ParentIdx);
	}
	OutInfo.GlobalTransform = GlobalTransform;

	// Find children
	const int32 NumBones = RefSkel->GetNum();
	for (int32 ChildIndex = 0; ChildIndex < NumBones; ++ChildIndex)
	{
		if (RefSkel->GetParentIndex(ChildIndex) == BoneIndex)
		{
			OutInfo.Children.Add(RefSkel->GetBoneName(ChildIndex).ToString());
			OutInfo.ChildCount++;
		}
	}

	// Get retargeting mode
	USkeleton* Skeleton = GetSkeletonFromAsset(AssetPath);
	if (Skeleton)
	{
		EBoneTranslationRetargetingMode::Type Mode = Skeleton->GetBoneTranslationRetargetingMode(BoneIndex);
		OutInfo.RetargetingMode = RetargetingModeToString(Mode);
	}

	return true;
}

FString USkeletonService::GetBoneParent(const FString& AssetPath, const FString& BoneName)
{
	const FReferenceSkeleton* RefSkel = GetReferenceSkeleton(AssetPath);
	if (!RefSkel)
	{
		return FString();
	}

	int32 BoneIndex = RefSkel->FindBoneIndex(FName(*BoneName));
	if (BoneIndex == INDEX_NONE)
	{
		return FString();
	}

	int32 ParentIndex = RefSkel->GetParentIndex(BoneIndex);
	if (ParentIndex >= 0)
	{
		return RefSkel->GetBoneName(ParentIndex).ToString();
	}

	return FString();
}

TArray<FString> USkeletonService::GetBoneChildren(const FString& AssetPath, const FString& BoneName, bool bRecursive)
{
	TArray<FString> Results;

	const FReferenceSkeleton* RefSkel = GetReferenceSkeleton(AssetPath);
	if (!RefSkel)
	{
		return Results;
	}

	int32 BoneIndex = RefSkel->FindBoneIndex(FName(*BoneName));
	if (BoneIndex == INDEX_NONE)
	{
		return Results;
	}

	const int32 NumBones = RefSkel->GetNum();

	if (bRecursive)
	{
		// Use a queue to process all descendants
		TArray<int32> ToProcess;
		ToProcess.Add(BoneIndex);

		while (ToProcess.Num() > 0)
		{
			int32 CurrentIndex = ToProcess.Pop();

			for (int32 ChildIndex = 0; ChildIndex < NumBones; ++ChildIndex)
			{
				if (RefSkel->GetParentIndex(ChildIndex) == CurrentIndex)
				{
					Results.Add(RefSkel->GetBoneName(ChildIndex).ToString());
					ToProcess.Add(ChildIndex);
				}
			}
		}
	}
	else
	{
		// Just direct children
		for (int32 ChildIndex = 0; ChildIndex < NumBones; ++ChildIndex)
		{
			if (RefSkel->GetParentIndex(ChildIndex) == BoneIndex)
			{
				Results.Add(RefSkel->GetBoneName(ChildIndex).ToString());
			}
		}
	}

	return Results;
}

FTransform USkeletonService::GetBoneTransform(const FString& AssetPath, const FString& BoneName, bool bComponentSpace)
{
	const FReferenceSkeleton* RefSkel = GetReferenceSkeleton(AssetPath);
	if (!RefSkel)
	{
		return FTransform::Identity;
	}

	int32 BoneIndex = RefSkel->FindBoneIndex(FName(*BoneName));
	if (BoneIndex == INDEX_NONE)
	{
		return FTransform::Identity;
	}

	if (!bComponentSpace)
	{
		return RefSkel->GetRefBonePose()[BoneIndex];
	}

	// Calculate global transform
	FTransform GlobalTransform = RefSkel->GetRefBonePose()[BoneIndex];
	int32 ParentIdx = RefSkel->GetParentIndex(BoneIndex);
	while (ParentIdx >= 0)
	{
		GlobalTransform = GlobalTransform * RefSkel->GetRefBonePose()[ParentIdx];
		ParentIdx = RefSkel->GetParentIndex(ParentIdx);
	}

	return GlobalTransform;
}

FString USkeletonService::GetRootBone(const FString& AssetPath)
{
	const FReferenceSkeleton* RefSkel = GetReferenceSkeleton(AssetPath);
	if (!RefSkel || RefSkel->GetNum() == 0)
	{
		return FString();
	}

	return RefSkel->GetBoneName(0).ToString();
}

TArray<FString> USkeletonService::FindBones(const FString& AssetPath, const FString& SearchPattern)
{
	TArray<FString> Results;

	const FReferenceSkeleton* RefSkel = GetReferenceSkeleton(AssetPath);
	if (!RefSkel)
	{
		return Results;
	}

	const int32 NumBones = RefSkel->GetNum();
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		FString BoneName = RefSkel->GetBoneName(BoneIndex).ToString();
		if (BoneName.Contains(SearchPattern, ESearchCase::IgnoreCase))
		{
			Results.Add(BoneName);
		}
	}

	return Results;
}

// ============================================================================
// BONE MODIFICATION
// ============================================================================

bool USkeletonService::AddBone(const FString& SkeletalMeshPath, const FString& BoneName, const FString& ParentBoneName, const FTransform& LocalTransform)
{
	USkeletonModifier* Modifier = GetSkeletonModifier(SkeletalMeshPath);
	if (!Modifier)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::AddBone: Failed to get skeleton modifier"));
		return false;
	}

	return Modifier->AddBone(FName(*BoneName), FName(*ParentBoneName), LocalTransform);
}

bool USkeletonService::RemoveBone(const FString& SkeletalMeshPath, const FString& BoneName, bool bRemoveChildren)
{
	USkeletonModifier* Modifier = GetSkeletonModifier(SkeletalMeshPath);
	if (!Modifier)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::RemoveBone: Failed to get skeleton modifier"));
		return false;
	}

	return Modifier->RemoveBone(FName(*BoneName), bRemoveChildren);
}

bool USkeletonService::RenameBone(const FString& SkeletalMeshPath, const FString& OldBoneName, const FString& NewBoneName)
{
	USkeletonModifier* Modifier = GetSkeletonModifier(SkeletalMeshPath);
	if (!Modifier)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::RenameBone: Failed to get skeleton modifier"));
		return false;
	}

	return Modifier->RenameBone(FName(*OldBoneName), FName(*NewBoneName));
}

bool USkeletonService::ReparentBone(const FString& SkeletalMeshPath, const FString& BoneName, const FString& NewParentName)
{
	// NOTE: Using SkeletonModifier->ParentBone() causes hierarchy incompatibility
	// during commit, which triggers a modal dialog that blocks the game thread.
	// Instead, we implement reparenting as: remove bone+descendants, add bone with new parent, add descendants back.
	
	USkeletonModifier* Modifier = GetSkeletonModifier(SkeletalMeshPath);
	if (!Modifier)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::ReparentBone: Failed to get skeleton modifier"));
		return false;
	}

	FName BoneFName = FName(*BoneName);
	FName NewParentFName = FName(*NewParentName);
	
	const FReferenceSkeleton& RefSkeleton = Modifier->GetReferenceSkeleton();
	int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneFName);
	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::ReparentBone: Bone '%s' not found"), *BoneName);
		return false;
	}
	
	// Get the bone transform
	FTransform BoneTransform = Modifier->GetBoneTransform(BoneFName, false);
	
	// Collect ALL descendants recursively with their parent names and transforms
	// We need to preserve the subtree structure
	struct FBoneData
	{
		FName Name;
		FName ParentName;
		FTransform Transform;
	};
	
	TArray<FBoneData> Descendants;
	TFunction<void(int32, FName)> CollectDescendants = [&](int32 ParentIdx, FName ParentName)
	{
		for (int32 i = 0; i < RefSkeleton.GetRawBoneNum(); ++i)
		{
			if (RefSkeleton.GetParentIndex(i) == ParentIdx)
			{
				FName ChildName = RefSkeleton.GetBoneName(i);
				FBoneData Data;
				Data.Name = ChildName;
				Data.ParentName = ParentName;
				Data.Transform = Modifier->GetBoneTransform(ChildName, false);
				Descendants.Add(Data);
				
				// Recurse to collect grandchildren
				CollectDescendants(i, ChildName);
			}
		}
	};
	CollectDescendants(BoneIndex, BoneFName);
	
	// Remove the bone AND all its children
	if (!Modifier->RemoveBone(BoneFName, true))  // true = remove children
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::ReparentBone: Failed to remove bone '%s' with children"), *BoneName);
		return false;
	}
	
	// Add the bone back with new parent
	if (!Modifier->AddBone(BoneFName, NewParentFName, BoneTransform))
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::ReparentBone: Failed to add bone '%s' under new parent '%s'"), *BoneName, *NewParentName);
		return false;
	}
	
	// Re-add all descendants in order (they were collected in hierarchy order)
	for (const FBoneData& Desc : Descendants)
	{
		if (!Modifier->AddBone(Desc.Name, Desc.ParentName, Desc.Transform))
		{
			UE_LOG(LogTemp, Warning, TEXT("USkeletonService::ReparentBone: Failed to restore descendant '%s'"), *Desc.Name.ToString());
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("USkeletonService::ReparentBone: Successfully reparented '%s' to '%s' (with %d descendants)"), 
		*BoneName, *NewParentName, Descendants.Num());
	return true;
}

bool USkeletonService::DuplicateSkeleton(const FString& SkeletalMeshPath, const FString& NewSkeletonPath)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::DuplicateSkeleton: Failed to load mesh: %s"), *SkeletalMeshPath);
		return false;
	}

	USkeleton* OriginalSkeleton = Mesh->GetSkeleton();
	if (!OriginalSkeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::DuplicateSkeleton: Mesh has no skeleton: %s"), *SkeletalMeshPath);
		return false;
	}

	// Duplicate the skeleton to the new path
	UObject* DuplicatedAsset = UEditorAssetLibrary::DuplicateAsset(
		OriginalSkeleton->GetPathName(),
		NewSkeletonPath
	);
	
	if (!DuplicatedAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::DuplicateSkeleton: Failed to duplicate skeleton to: %s"), *NewSkeletonPath);
		return false;
	}

	USkeleton* NewSkeleton = Cast<USkeleton>(DuplicatedAsset);
	if (!NewSkeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::DuplicateSkeleton: Duplicated asset is not a skeleton: %s"), *NewSkeletonPath);
		return false;
	}

	// Save the new skeleton
	if (!UEditorAssetLibrary::SaveAsset(NewSkeletonPath, false))
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::DuplicateSkeleton: Failed to save new skeleton: %s"), *NewSkeletonPath);
	}

	UE_LOG(LogTemp, Log, TEXT("USkeletonService::DuplicateSkeleton: Successfully duplicated skeleton from '%s' to '%s'"), 
		*OriginalSkeleton->GetPathName(), *NewSkeletonPath);
	UE_LOG(LogTemp, Log, TEXT("USkeletonService::DuplicateSkeleton: NOTE: The mesh still uses the original skeleton. "
		"Use the Skeletal Mesh Editor to manually assign the new skeleton, or use SetSkeleton in Python."));
	return true;
}

bool USkeletonService::SetBoneTransform(const FString& SkeletalMeshPath, const FString& BoneName, const FTransform& NewTransform, bool bMoveChildren)
{
	USkeletonModifier* Modifier = GetSkeletonModifier(SkeletalMeshPath);
	if (!Modifier)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::SetBoneTransform: Failed to get skeleton modifier"));
		return false;
	}

	return Modifier->SetBoneTransform(FName(*BoneName), NewTransform, bMoveChildren);
}

bool USkeletonService::CommitBoneChanges(const FString& SkeletalMeshPath, bool bForce)
{
	USkeletonModifier* Modifier = GetSkeletonModifier(SkeletalMeshPath);
	if (!Modifier)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::CommitBoneChanges: No pending changes for: %s"), *SkeletalMeshPath);
		return false;
	}

	bool bSuccess = false;

	if (bForce)
	{
		// Force commit - use a workaround for hierarchy-breaking changes
		// The regular CommitSkeletonToSkeletalMesh shows a dialog when the skeleton
		// hierarchy becomes incompatible. For automated/silent operations, we
		// attempt the commit but if it fails or blocks, we log a warning.
		//
		// NOTE: Unreal's SkeletonModifier.PreCommitSkeleton() shows a modal dialog
		// when the parent chain check fails. This is by design to protect shared
		// skeletons. When bForce=true, we still call the normal commit but with
		// the understanding that for true hierarchy-breaking reparent operations,
		// user interaction may still be required.
		
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::CommitBoneChanges: Force mode requested. "
			"Note: Hierarchy-breaking changes may still require user confirmation due to "
			"Unreal Engine's skeleton compatibility checks."));
		
		bSuccess = Modifier->CommitSkeletonToSkeletalMesh();
	}
	else
	{
		// Normal commit - may show dialog for hierarchy conflicts
		bSuccess = Modifier->CommitSkeletonToSkeletalMesh();
	}

	if (bSuccess)
	{
		ClearSkeletonModifier(SkeletalMeshPath);
	}

	return bSuccess;
}

bool USkeletonService::IsSkeletonShared(const FString& SkeletalMeshPath)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return false;
	}

	USkeleton* Skeleton = Mesh->GetSkeleton();
	if (!Skeleton)
	{
		return false;
	}

	// Get the asset registry to find references
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	// Find all packages that reference this skeleton's package
	TArray<FName> Referencers;
	FName SkeletonPackageName = Skeleton->GetOutermost()->GetFName();
	AssetRegistry.GetReferencers(SkeletonPackageName, Referencers);

	// If there's more than one referencer (the skeletal mesh we're querying),
	// the skeleton is shared
	return Referencers.Num() > 1;
}

// ============================================================================
// SOCKET MANAGEMENT
// ============================================================================

TArray<FMeshSocketInfo> USkeletonService::ListSockets(const FString& SkeletalMeshPath)
{
	TArray<FMeshSocketInfo> Results;

	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return Results;
	}

	const int32 NumSockets = Mesh->NumSockets();
	Results.Reserve(NumSockets);

	for (int32 i = 0; i < NumSockets; ++i)
	{
		USkeletalMeshSocket* Socket = Mesh->GetSocketByIndex(i);
		if (Socket)
		{
			FMeshSocketInfo SocketInfo;
			SocketInfo.SocketName = Socket->SocketName.ToString();
			SocketInfo.BoneName = Socket->BoneName.ToString();
			SocketInfo.RelativeLocation = Socket->RelativeLocation;
			SocketInfo.RelativeRotation = Socket->RelativeRotation;
			SocketInfo.RelativeScale = Socket->RelativeScale;
			SocketInfo.bForceAlwaysAnimated = Socket->bForceAlwaysAnimated;
			Results.Add(SocketInfo);
		}
	}

	return Results;
}

bool USkeletonService::GetSocketInfo(const FString& SkeletalMeshPath, const FString& SocketName, FMeshSocketInfo& OutInfo)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return false;
	}

	int32 SocketIndex;
	USkeletalMeshSocket* Socket = Mesh->FindSocketAndIndex(FName(*SocketName), SocketIndex);
	if (!Socket)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::GetSocketInfo: Socket not found: %s"), *SocketName);
		return false;
	}

	OutInfo.SocketName = Socket->SocketName.ToString();
	OutInfo.BoneName = Socket->BoneName.ToString();
	OutInfo.RelativeLocation = Socket->RelativeLocation;
	OutInfo.RelativeRotation = Socket->RelativeRotation;
	OutInfo.RelativeScale = Socket->RelativeScale;
	OutInfo.bForceAlwaysAnimated = Socket->bForceAlwaysAnimated;

	return true;
}

bool USkeletonService::AddSocket(const FString& SkeletalMeshPath, const FString& SocketName, const FString& BoneName,
	FVector RelativeLocation, FRotator RelativeRotation, FVector RelativeScale, bool bAddToSkeleton)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return false;
	}

	// Verify the bone exists
	int32 BoneIndex = Mesh->GetRefSkeleton().FindBoneIndex(FName(*BoneName));
	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::AddSocket: Bone not found: %s"), *BoneName);
		return false;
	}

	// Check if socket already exists
	int32 ExistingIndex;
	if (Mesh->FindSocketAndIndex(FName(*SocketName), ExistingIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::AddSocket: Socket already exists: %s"), *SocketName);
		return false;
	}

	// Create the socket
	USkeletalMeshSocket* NewSocket = NewObject<USkeletalMeshSocket>(Mesh);
	NewSocket->SocketName = FName(*SocketName);
	NewSocket->BoneName = FName(*BoneName);
	NewSocket->RelativeLocation = RelativeLocation;
	NewSocket->RelativeRotation = RelativeRotation;
	NewSocket->RelativeScale = RelativeScale;

	// Add to mesh
	Mesh->AddSocket(NewSocket, bAddToSkeleton);
	Mesh->MarkPackageDirty();

	return true;
}

bool USkeletonService::RemoveSocket(const FString& SkeletalMeshPath, const FString& SocketName)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return false;
	}

	// Find and remove from mesh-only sockets
	TArray<TObjectPtr<USkeletalMeshSocket>>& MeshSockets = Mesh->GetMeshOnlySocketList();
	int32 SocketIndex = INDEX_NONE;
	for (int32 i = 0; i < MeshSockets.Num(); ++i)
	{
		if (MeshSockets[i] && MeshSockets[i]->SocketName == FName(*SocketName))
		{
			SocketIndex = i;
			break;
		}
	}

	if (SocketIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::RemoveSocket: Socket not found in mesh-only sockets: %s"), *SocketName);
		return false;
	}

	// Remove from mesh sockets
	MeshSockets.RemoveAt(SocketIndex);
	Mesh->MarkPackageDirty();

	return true;
}

bool USkeletonService::RenameSocket(const FString& SkeletalMeshPath, const FString& OldName, const FString& NewName)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return false;
	}

	int32 SocketIndex;
	USkeletalMeshSocket* Socket = Mesh->FindSocketAndIndex(FName(*OldName), SocketIndex);
	if (!Socket)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::RenameSocket: Socket not found: %s"), *OldName);
		return false;
	}

	Socket->SocketName = FName(*NewName);
	Mesh->MarkPackageDirty();

	return true;
}

bool USkeletonService::SetSocketTransform(const FString& SkeletalMeshPath, const FString& SocketName,
	const FVector& RelativeLocation, const FRotator& RelativeRotation, const FVector& RelativeScale)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return false;
	}

	int32 SocketIndex;
	USkeletalMeshSocket* Socket = Mesh->FindSocketAndIndex(FName(*SocketName), SocketIndex);
	if (!Socket)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::SetSocketTransform: Socket not found: %s"), *SocketName);
		return false;
	}

	Socket->RelativeLocation = RelativeLocation;
	Socket->RelativeRotation = RelativeRotation;
	Socket->RelativeScale = RelativeScale;
	Mesh->MarkPackageDirty();

	return true;
}

bool USkeletonService::SetSocketBone(const FString& SkeletalMeshPath, const FString& SocketName, const FString& NewBoneName)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return false;
	}

	// Verify the new bone exists
	int32 BoneIndex = Mesh->GetRefSkeleton().FindBoneIndex(FName(*NewBoneName));
	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("USkeletonService::SetSocketBone: Bone not found: %s"), *NewBoneName);
		return false;
	}

	int32 SocketIndex;
	USkeletalMeshSocket* Socket = Mesh->FindSocketAndIndex(FName(*SocketName), SocketIndex);
	if (!Socket)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::SetSocketBone: Socket not found: %s"), *SocketName);
		return false;
	}

	Socket->BoneName = FName(*NewBoneName);
	Mesh->MarkPackageDirty();

	return true;
}

// ============================================================================
// RETARGETING
// ============================================================================

TArray<FString> USkeletonService::GetCompatibleSkeletons(const FString& SkeletonPath)
{
	TArray<FString> Results;

	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return Results;
	}

	const TArray<TSoftObjectPtr<USkeleton>>& Compatible = Skeleton->GetCompatibleSkeletons();
	for (const TSoftObjectPtr<USkeleton>& CompatSkel : Compatible)
	{
		if (!CompatSkel.IsNull())
		{
			Results.Add(CompatSkel.ToSoftObjectPath().ToString());
		}
	}

	return Results;
}

bool USkeletonService::AddCompatibleSkeleton(const FString& SkeletonPath, const FString& CompatibleSkeletonPath)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return false;
	}

	USkeleton* CompatibleSkeleton = LoadSkeleton(CompatibleSkeletonPath);
	if (!CompatibleSkeleton)
	{
		return false;
	}

	Skeleton->AddCompatibleSkeleton(CompatibleSkeleton);
	Skeleton->MarkPackageDirty();

	return true;
}

FString USkeletonService::GetBoneRetargetingMode(const FString& SkeletonPath, const FString& BoneName)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return TEXT("Unknown");
	}

	int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(FName(*BoneName));
	if (BoneIndex == INDEX_NONE)
	{
		return TEXT("Unknown");
	}

	EBoneTranslationRetargetingMode::Type Mode = Skeleton->GetBoneTranslationRetargetingMode(BoneIndex);
	return RetargetingModeToString(Mode);
}

bool USkeletonService::SetBoneRetargetingMode(const FString& SkeletonPath, const FString& BoneName, const FString& Mode)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return false;
	}

	int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(FName(*BoneName));
	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::SetBoneRetargetingMode: Bone not found: %s"), *BoneName);
		return false;
	}

	EBoneTranslationRetargetingMode::Type RetargetMode = StringToRetargetingMode(Mode);
	Skeleton->SetBoneTranslationRetargetingMode(BoneIndex, RetargetMode);
	Skeleton->MarkPackageDirty();

	return true;
}

// ============================================================================
// CURVE METADATA
// ============================================================================

TArray<FCurveMetaInfo> USkeletonService::ListCurveMetaData(const FString& SkeletonPath)
{
	TArray<FCurveMetaInfo> Results;

	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return Results;
	}

	TArray<FName> CurveNames;
	Skeleton->GetCurveMetaDataNames(CurveNames);

	for (const FName& CurveName : CurveNames)
	{
		const FCurveMetaData* MetaData = Skeleton->GetCurveMetaData(CurveName);
		if (!MetaData)
		{
			continue; // Skip curves with no metadata
		}
		
		FCurveMetaInfo CurveInfo;
		CurveInfo.CurveName = CurveName.ToString();
		CurveInfo.bIsMorphTarget = MetaData->Type.bMorphtarget;
		CurveInfo.bIsMaterial = MetaData->Type.bMaterial;
		Results.Add(CurveInfo);
	}

	return Results;
}

bool USkeletonService::AddCurveMetaData(const FString& SkeletonPath, const FString& CurveName)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return false;
	}

	Skeleton->AddCurveMetaData(FName(*CurveName));
	Skeleton->MarkPackageDirty();

	return true;
}

bool USkeletonService::RemoveCurveMetaData(const FString& SkeletonPath, const FString& CurveName)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return false;
	}

	Skeleton->RemoveCurveMetaData(FName(*CurveName));
	Skeleton->MarkPackageDirty();

	return true;
}

bool USkeletonService::RenameCurveMetaData(const FString& SkeletonPath, const FString& OldName, const FString& NewName)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return false;
	}

	Skeleton->RenameCurveMetaData(FName(*OldName), FName(*NewName));
	Skeleton->MarkPackageDirty();

	return true;
}

bool USkeletonService::SetCurveMorphTarget(const FString& SkeletonPath, const FString& CurveName, bool bIsMorphTarget)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return false;
	}

	FCurveMetaData* MetaData = Skeleton->GetCurveMetaData(FName(*CurveName));
	if (!MetaData)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::SetCurveMorphTarget: Curve not found: %s"), *CurveName);
		return false;
	}

	MetaData->Type.bMorphtarget = bIsMorphTarget;
	Skeleton->MarkPackageDirty();

	return true;
}

bool USkeletonService::SetCurveMaterial(const FString& SkeletonPath, const FString& CurveName, bool bIsMaterial)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return false;
	}

	FCurveMetaData* MetaData = Skeleton->GetCurveMetaData(FName(*CurveName));
	if (!MetaData)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::SetCurveMaterial: Curve not found: %s"), *CurveName);
		return false;
	}

	MetaData->Type.bMaterial = bIsMaterial;
	Skeleton->MarkPackageDirty();

	return true;
}

// ============================================================================
// BLEND PROFILES
// ============================================================================

TArray<FString> USkeletonService::ListBlendProfiles(const FString& SkeletonPath)
{
	TArray<FString> Results;

	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return Results;
	}

	for (UBlendProfile* Profile : Skeleton->BlendProfiles)
	{
		if (Profile)
		{
			Results.Add(Profile->GetName());
		}
	}

	return Results;
}

bool USkeletonService::GetBlendProfile(const FString& SkeletonPath, const FString& ProfileName, FBlendProfileData& OutInfo)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return false;
	}

	UBlendProfile* Profile = Skeleton->GetBlendProfile(FName(*ProfileName));
	if (!Profile)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::GetBlendProfile: Profile not found: %s"), *ProfileName);
		return false;
	}

	OutInfo.ProfileName = ProfileName;

	const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
	const int32 NumBones = RefSkel.GetNum();

	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		float Scale = Profile->GetBoneBlendScale(BoneIndex);
		if (Scale != 1.0f) // Only include bones with non-default scales
		{
			OutInfo.BoneNames.Add(RefSkel.GetBoneName(BoneIndex).ToString());
			OutInfo.BlendScales.Add(Scale);
		}
	}

	return true;
}

bool USkeletonService::CreateBlendProfile(const FString& SkeletonPath, const FString& ProfileName)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return false;
	}

	// Check if profile already exists
	if (Skeleton->GetBlendProfile(FName(*ProfileName)))
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::CreateBlendProfile: Profile already exists: %s"), *ProfileName);
		return false;
	}

	// Create new blend profile
	UBlendProfile* NewProfile = NewObject<UBlendProfile>(Skeleton, FName(*ProfileName), RF_Public | RF_Transactional);
	NewProfile->OwningSkeleton = Skeleton;
	Skeleton->BlendProfiles.Add(NewProfile);
	Skeleton->MarkPackageDirty();

	return true;
}

bool USkeletonService::SetBlendProfileBone(const FString& SkeletonPath, const FString& ProfileName, const FString& BoneName, float BlendScale)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return false;
	}

	UBlendProfile* Profile = Skeleton->GetBlendProfile(FName(*ProfileName));
	if (!Profile)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::SetBlendProfileBone: Profile not found: %s"), *ProfileName);
		return false;
	}

	int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindBoneIndex(FName(*BoneName));
	if (BoneIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("USkeletonService::SetBlendProfileBone: Bone not found: %s"), *BoneName);
		return false;
	}

	Profile->SetBoneBlendScale(BoneIndex, BlendScale, false, true);
	Skeleton->MarkPackageDirty();

	return true;
}

// ============================================================================
// SKELETAL MESH PROPERTIES
// ============================================================================

bool USkeletonService::SetPhysicsAsset(const FString& SkeletalMeshPath, const FString& PhysicsAssetPath)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return false;
	}

	if (PhysicsAssetPath.IsEmpty())
	{
		Mesh->SetPhysicsAsset(nullptr);
	}
	else
	{
		UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(PhysicsAssetPath);
		UPhysicsAsset* PhysAsset = Cast<UPhysicsAsset>(LoadedAsset);
		if (!PhysAsset)
		{
			UE_LOG(LogTemp, Error, TEXT("USkeletonService::SetPhysicsAsset: Failed to load physics asset: %s"), *PhysicsAssetPath);
			return false;
		}
		Mesh->SetPhysicsAsset(PhysAsset);
	}

	Mesh->MarkPackageDirty();
	return true;
}

bool USkeletonService::SetPostProcessAnimBlueprint(const FString& SkeletalMeshPath, const FString& AnimBlueprintPath)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return false;
	}

	if (AnimBlueprintPath.IsEmpty())
	{
		Mesh->SetPostProcessAnimBlueprint(nullptr);
	}
	else
	{
		UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AnimBlueprintPath);
		UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(LoadedAsset);
		if (!AnimBP)
		{
			UE_LOG(LogTemp, Error, TEXT("USkeletonService::SetPostProcessAnimBlueprint: Failed to load AnimBP: %s"), *AnimBlueprintPath);
			return false;
		}

		if (AnimBP->GetBlueprintClass())
		{
			Mesh->SetPostProcessAnimBlueprint(AnimBP->GetBlueprintClass());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("USkeletonService::SetPostProcessAnimBlueprint: AnimBP has no generated class"));
			return false;
		}
	}

	Mesh->MarkPackageDirty();
	return true;
}

TArray<FString> USkeletonService::ListMorphTargets(const FString& SkeletalMeshPath)
{
	TArray<FString> Results;

	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return Results;
	}

	for (UMorphTarget* MorphTarget : Mesh->GetMorphTargets())
	{
		if (MorphTarget)
		{
			Results.Add(MorphTarget->GetName());
		}
	}

	return Results;
}

// ============================================================================
// EDITOR NAVIGATION
// ============================================================================

bool USkeletonService::OpenSkeletonEditor(const FString& SkeletonPath)
{
	USkeleton* Skeleton = LoadSkeleton(SkeletonPath);
	if (!Skeleton)
	{
		return false;
	}

	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Skeleton);
	return true;
}

bool USkeletonService::OpenSkeletalMeshEditor(const FString& SkeletalMeshPath)
{
	USkeletalMesh* Mesh = LoadSkeletalMesh(SkeletalMeshPath);
	if (!Mesh)
	{
		return false;
	}

	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Mesh);
	return true;
}

bool USkeletonService::SaveAsset(const FString& AssetPath)
{
	UObject* Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!Asset)
	{
		return false;
	}

	return UEditorAssetLibrary::SaveAsset(AssetPath);
}
