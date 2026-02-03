// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UAnimMontageService.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "AssetToolsModule.h"
#include "Factories/AnimMontageFactory.h"
#include "ObjectTools.h"

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

UAnimMontage* UAnimMontageService::LoadMontage(const FString& MontagePath)
{
	if (MontagePath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimMontageService::LoadMontage: Path is empty"));
		return nullptr;
	}

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(MontagePath);
	if (!LoadedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimMontageService::LoadMontage: Failed to load: %s"), *MontagePath);
		return nullptr;
	}

	UAnimMontage* Montage = Cast<UAnimMontage>(LoadedObject);
	if (!Montage)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimMontageService::LoadMontage: Not an AnimMontage: %s (got %s)"),
			*MontagePath, *LoadedObject->GetClass()->GetName());
		return nullptr;
	}

	return Montage;
}

void UAnimMontageService::MarkMontageModified(UAnimMontage* Montage)
{
	if (!Montage) return;

	// Enable undo/redo support
	Montage->Modify();

	// Recalculate the sequence length based on segments
	// This is needed because adding/removing segments doesn't automatically update length
	float CalculatedLength = Montage->CalculateSequenceLength();
	
	// Update the sequence length using the controller
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Montage->GetController().SetPlayLength(CalculatedLength, false);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Mark package as needing save
	Montage->MarkPackageDirty();

	// Notify listeners that the montage has changed
	Montage->PostEditChange();
	
#if WITH_EDITOR
	// Also broadcast property change notification for editor refresh
	FPropertyChangedEvent EmptyPropertyChangedEvent(nullptr);
	Montage->PostEditChangeProperty(EmptyPropertyChangedEvent);
#endif
}

bool UAnimMontageService::ValidateSection(UAnimMontage* Montage, const FString& SectionName)
{
	if (!Montage) return false;

	const FName SectionFName(*SectionName);
	const int32 Index = Montage->GetSectionIndex(SectionFName);
	return Index != INDEX_NONE;
}

bool UAnimMontageService::ValidateTrackIndex(UAnimMontage* Montage, int32 TrackIndex)
{
	if (!Montage) return false;
	return TrackIndex >= 0 && TrackIndex < Montage->SlotAnimTracks.Num();
}

bool UAnimMontageService::ValidateSegmentIndex(UAnimMontage* Montage, int32 TrackIndex, int32 SegmentIndex)
{
	if (!ValidateTrackIndex(Montage, TrackIndex)) return false;

	const TArray<FAnimSegment>& Segments = Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments;
	return SegmentIndex >= 0 && SegmentIndex < Segments.Num();
}

void UAnimMontageService::FillMontageInfo(UAnimMontage* Montage, FMontageInfo& OutInfo)
{
	if (!Montage) return;

	OutInfo.MontagePath = Montage->GetPathName();
	OutInfo.MontageName = Montage->GetName();

	if (USkeleton* Skeleton = Montage->GetSkeleton())
	{
		OutInfo.SkeletonPath = Skeleton->GetPathName();
	}

	OutInfo.Duration = Montage->GetPlayLength();
	OutInfo.SectionCount = Montage->CompositeSections.Num();
	OutInfo.SlotTrackCount = Montage->SlotAnimTracks.Num();
	OutInfo.NotifyCount = Montage->Notifies.Num();

	// Count branching points
	OutInfo.BranchingPointCount = 0;
	for (const FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		if (Notify.MontageTickType == EMontageNotifyTickType::BranchingPoint)
		{
			OutInfo.BranchingPointCount++;
		}
	}

	OutInfo.BlendInTime = Montage->BlendIn.GetBlendTime();
	OutInfo.BlendOutTime = Montage->BlendOut.GetBlendTime();
	OutInfo.BlendOutTriggerTime = Montage->BlendOutTriggerTime;
	OutInfo.bEnableRootMotionTranslation = Montage->bEnableRootMotionTranslation;
	OutInfo.bEnableRootMotionRotation = Montage->bEnableRootMotionRotation;

	// Collect slot names
	for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		OutInfo.SlotNames.AddUnique(Track.SlotName.ToString());
	}
}

FString UAnimMontageService::BlendOptionToString(EAlphaBlendOption Option)
{
	switch (Option)
	{
		case EAlphaBlendOption::Linear: return TEXT("Linear");
		case EAlphaBlendOption::Cubic: return TEXT("Cubic");
		case EAlphaBlendOption::HermiteCubic: return TEXT("HermiteCubic");
		case EAlphaBlendOption::Sinusoidal: return TEXT("Sinusoidal");
		case EAlphaBlendOption::QuadraticInOut: return TEXT("QuadraticInOut");
		case EAlphaBlendOption::CubicInOut: return TEXT("CubicInOut");
		case EAlphaBlendOption::QuarticInOut: return TEXT("QuarticInOut");
		case EAlphaBlendOption::QuinticInOut: return TEXT("QuinticInOut");
		case EAlphaBlendOption::CircularIn: return TEXT("CircularIn");
		case EAlphaBlendOption::CircularOut: return TEXT("CircularOut");
		case EAlphaBlendOption::CircularInOut: return TEXT("CircularInOut");
		case EAlphaBlendOption::ExpIn: return TEXT("ExpIn");
		case EAlphaBlendOption::ExpOut: return TEXT("ExpOut");
		case EAlphaBlendOption::ExpInOut: return TEXT("ExpInOut");
		case EAlphaBlendOption::Custom: return TEXT("Custom");
		default: return TEXT("Linear");
	}
}

EAlphaBlendOption UAnimMontageService::StringToBlendOption(const FString& OptionString)
{
	if (OptionString.Equals(TEXT("Linear"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::Linear;
	if (OptionString.Equals(TEXT("Cubic"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::Cubic;
	if (OptionString.Equals(TEXT("HermiteCubic"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::HermiteCubic;
	if (OptionString.Equals(TEXT("Sinusoidal"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::Sinusoidal;
	if (OptionString.Equals(TEXT("QuadraticInOut"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::QuadraticInOut;
	if (OptionString.Equals(TEXT("CubicInOut"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::CubicInOut;
	if (OptionString.Equals(TEXT("QuarticInOut"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::QuarticInOut;
	if (OptionString.Equals(TEXT("QuinticInOut"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::QuinticInOut;
	if (OptionString.Equals(TEXT("CircularIn"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::CircularIn;
	if (OptionString.Equals(TEXT("CircularOut"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::CircularOut;
	if (OptionString.Equals(TEXT("CircularInOut"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::CircularInOut;
	if (OptionString.Equals(TEXT("ExpIn"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::ExpIn;
	if (OptionString.Equals(TEXT("ExpOut"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::ExpOut;
	if (OptionString.Equals(TEXT("ExpInOut"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::ExpInOut;
	if (OptionString.Equals(TEXT("Custom"), ESearchCase::IgnoreCase)) return EAlphaBlendOption::Custom;
	return EAlphaBlendOption::Linear;
}

// ============================================================================
// MONTAGE DISCOVERY
// ============================================================================

TArray<FMontageInfo> UAnimMontageService::ListMontages(
	const FString& SearchPath,
	const FString& SkeletonFilter)
{
	TArray<FMontageInfo> Results;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UAnimMontage::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(*SearchPath);
	Filter.bRecursivePaths = true;

	TArray<FAssetData> AssetList;
	AssetRegistry.GetAssets(Filter, AssetList);

	const int32 MaxResults = 100;
	int32 LoadedCount = 0;

	for (const FAssetData& Asset : AssetList)
	{
		if (LoadedCount >= MaxResults)
		{
			UE_LOG(LogTemp, Warning, TEXT("ListMontages: Limiting results to %d montages (found %d total)"),
				MaxResults, AssetList.Num());
			break;
		}

		FSoftObjectPath AssetPath(Asset.GetSoftObjectPath());
		UAnimMontage* Montage = Cast<UAnimMontage>(AssetPath.TryLoad());
		if (!Montage) continue;

		// Apply skeleton filter if specified
		if (!SkeletonFilter.IsEmpty())
		{
			USkeleton* Skeleton = Montage->GetSkeleton();
			if (!Skeleton || !Skeleton->GetPathName().Contains(SkeletonFilter))
			{
				continue;
			}
		}

		FMontageInfo Info;
		FillMontageInfo(Montage, Info);
		Results.Add(Info);
		LoadedCount++;
	}

	return Results;
}

bool UAnimMontageService::GetMontageInfo(const FString& MontagePath, FMontageInfo& OutInfo)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage)
	{
		return false;
	}

	FillMontageInfo(Montage, OutInfo);
	return true;
}

TArray<FMontageInfo> UAnimMontageService::FindMontagesForSkeleton(const FString& SkeletonPath)
{
	return ListMontages(TEXT("/Game"), SkeletonPath);
}

TArray<FMontageInfo> UAnimMontageService::FindMontagesUsingAnimation(const FString& AnimSequencePath)
{
	TArray<FMontageInfo> Results;

	UAnimSequence* AnimSeq = Cast<UAnimSequence>(UEditorAssetLibrary::LoadAsset(AnimSequencePath));
	if (!AnimSeq) return Results;

	TArray<FMontageInfo> AllMontages = ListMontages();
	for (const FMontageInfo& Info : AllMontages)
	{
		UAnimMontage* Montage = LoadMontage(Info.MontagePath);
		if (!Montage) continue;

		bool bFound = false;
		for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
		{
			for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
			{
				if (Segment.GetAnimReference() == AnimSeq)
				{
					Results.Add(Info);
					bFound = true;
					break;
				}
			}
			if (bFound) break;
		}
	}

	return Results;
}

// ============================================================================
// MONTAGE PROPERTIES
// ============================================================================

float UAnimMontageService::GetMontageLength(const FString& MontagePath)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return -1.0f;

	return Montage->GetPlayLength();
}

FString UAnimMontageService::GetMontageSkeleton(const FString& MontagePath)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return FString();

	USkeleton* Skeleton = Montage->GetSkeleton();
	return Skeleton ? Skeleton->GetPathName() : FString();
}

bool UAnimMontageService::SetBlendIn(
	const FString& MontagePath,
	float BlendTime,
	const FString& BlendOption)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	Montage->Modify();
	Montage->BlendIn.SetBlendTime(FMath::Max(0.0f, BlendTime));
	Montage->BlendIn.SetBlendOption(StringToBlendOption(BlendOption));
	MarkMontageModified(Montage);

	return true;
}

bool UAnimMontageService::SetBlendOut(
	const FString& MontagePath,
	float BlendTime,
	const FString& BlendOption)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	Montage->Modify();
	Montage->BlendOut.SetBlendTime(FMath::Max(0.0f, BlendTime));
	Montage->BlendOut.SetBlendOption(StringToBlendOption(BlendOption));
	MarkMontageModified(Montage);

	return true;
}

bool UAnimMontageService::GetBlendSettings(const FString& MontagePath, FVibeMontageBlendSettings& OutSettings)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	OutSettings.BlendInTime = Montage->BlendIn.GetBlendTime();
	OutSettings.BlendInOption = BlendOptionToString(Montage->BlendIn.GetBlendOption());
	OutSettings.BlendOutTime = Montage->BlendOut.GetBlendTime();
	OutSettings.BlendOutOption = BlendOptionToString(Montage->BlendOut.GetBlendOption());
	OutSettings.BlendOutTriggerTime = Montage->BlendOutTriggerTime;

	// Note: CustomCurve is private in FAlphaBlend, cannot access directly
	// Custom curves must be set via SetCustomCurve() and retrieved via other means if needed

	return true;
}

bool UAnimMontageService::SetBlendOutTriggerTime(const FString& MontagePath, float TriggerTime)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	Montage->Modify();
	Montage->BlendOutTriggerTime = TriggerTime;
	MarkMontageModified(Montage);

	return true;
}

// ============================================================================
// SECTION MANAGEMENT
// ============================================================================

TArray<FMontageSectionInfo> UAnimMontageService::ListSections(const FString& MontagePath)
{
	TArray<FMontageSectionInfo> Results;

	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return Results;

	for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
	{
		const FCompositeSection& Section = Montage->CompositeSections[i];

		FMontageSectionInfo Info;
		Info.SectionName = Section.SectionName.ToString();
		Info.SectionIndex = i;
		Info.StartTime = Section.GetTime();

		// Calculate end time (start of next section or end of montage)
		float NextTime = Montage->GetPlayLength();
		for (const FCompositeSection& OtherSection : Montage->CompositeSections)
		{
			if (OtherSection.GetTime() > Section.GetTime() && OtherSection.GetTime() < NextTime)
			{
				NextTime = OtherSection.GetTime();
			}
		}
		Info.EndTime = NextTime;
		Info.Duration = Info.EndTime - Info.StartTime;

		// Get linked section from CompositeSections
		Info.NextSectionName = Section.NextSectionName.ToString();
		Info.bLoops = (Section.NextSectionName == Section.SectionName);

		// Count segments in this section's time range
		Info.SegmentCount = 0;
		if (Montage->SlotAnimTracks.Num() > 0)
		{
			for (const FAnimSegment& Seg : Montage->SlotAnimTracks[0].AnimTrack.AnimSegments)
			{
				if (Seg.StartPos >= Info.StartTime && Seg.StartPos < Info.EndTime)
				{
					Info.SegmentCount++;
				}
			}
		}

		Results.Add(Info);
	}

	// Sort by start time
	Results.Sort([](const FMontageSectionInfo& A, const FMontageSectionInfo& B) {
		return A.StartTime < B.StartTime;
	});

	return Results;
}

bool UAnimMontageService::GetSectionInfo(
	const FString& MontagePath,
	const FString& SectionName,
	FMontageSectionInfo& OutInfo)
{
	TArray<FMontageSectionInfo> Sections = ListSections(MontagePath);
	for (const FMontageSectionInfo& Section : Sections)
	{
		if (Section.SectionName.Equals(SectionName, ESearchCase::IgnoreCase))
		{
			OutInfo = Section;
			return true;
		}
	}
	return false;
}

int32 UAnimMontageService::GetSectionIndexAtTime(const FString& MontagePath, float Time)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return -1;

	return Montage->GetSectionIndexFromPosition(Time);
}

FString UAnimMontageService::GetSectionNameAtTime(const FString& MontagePath, float Time)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return FString();

	int32 Index = Montage->GetSectionIndexFromPosition(Time);
	if (Index != INDEX_NONE)
	{
		return Montage->GetSectionName(Index).ToString();
	}
	return FString();
}

bool UAnimMontageService::AddSection(
	const FString& MontagePath,
	const FString& SectionName,
	float StartTime)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::AddSection: Montage not found '%s'"), *MontagePath);
		return false;
	}

	// Validate time is within montage
	if (StartTime < 0.0f || StartTime > Montage->GetPlayLength())
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::AddSection: StartTime %.2f is out of range [0, %.2f]"),
			StartTime, Montage->GetPlayLength());
		return false;
	}

	// Check for duplicate section name
	if (Montage->GetSectionIndex(FName(*SectionName)) != INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::AddSection: Section '%s' already exists"), *SectionName);
		return false;
	}

	Montage->Modify();
	int32 NewIndex = Montage->AddAnimCompositeSection(FName(*SectionName), StartTime);

	if (NewIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::AddSection: Failed to add section '%s'"), *SectionName);
		return false;
	}

	MarkMontageModified(Montage);
	return true;
}

bool UAnimMontageService::RemoveSection(const FString& MontagePath, const FString& SectionName)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	// Cannot remove the only section
	if (Montage->CompositeSections.Num() <= 1)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::RemoveSection: Cannot remove the only section"));
		return false;
	}

	Montage->Modify();

	const FName SectionFName(*SectionName);
	for (int32 i = Montage->CompositeSections.Num() - 1; i >= 0; --i)
	{
		if (Montage->CompositeSections[i].SectionName == SectionFName)
		{
			Montage->CompositeSections.RemoveAt(i);
			MarkMontageModified(Montage);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UAnimMontageService::RemoveSection: Section '%s' not found"), *SectionName);
	return false;
}

bool UAnimMontageService::RenameSection(
	const FString& MontagePath,
	const FString& OldName,
	const FString& NewName)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	// Check new name doesn't already exist
	if (Montage->GetSectionIndex(FName(*NewName)) != INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::RenameSection: Section '%s' already exists"), *NewName);
		return false;
	}

	Montage->Modify();

	const FName OldFName(*OldName);
	const FName NewFName(*NewName);

	for (FCompositeSection& Section : Montage->CompositeSections)
	{
		if (Section.SectionName == OldFName)
		{
			Section.SectionName = NewFName;
			MarkMontageModified(Montage);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UAnimMontageService::RenameSection: Section '%s' not found"), *OldName);
	return false;
}

bool UAnimMontageService::SetSectionStartTime(
	const FString& MontagePath,
	const FString& SectionName,
	float NewStartTime)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (NewStartTime < 0.0f || NewStartTime > Montage->GetPlayLength())
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::SetSectionStartTime: Time %.2f is out of range"), NewStartTime);
		return false;
	}

	Montage->Modify();

	const FName SectionFName(*SectionName);
	for (FCompositeSection& Section : Montage->CompositeSections)
	{
		if (Section.SectionName == SectionFName)
		{
			Section.SetTime(NewStartTime);
			MarkMontageModified(Montage);
			return true;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UAnimMontageService::SetSectionStartTime: Section '%s' not found"), *SectionName);
	return false;
}

float UAnimMontageService::GetSectionLength(const FString& MontagePath, const FString& SectionName)
{
	FMontageSectionInfo Info;
	if (GetSectionInfo(MontagePath, SectionName, Info))
	{
		return Info.Duration;
	}
	return -1.0f;
}

// ============================================================================
// SECTION LINKING
// ============================================================================

FString UAnimMontageService::GetNextSection(const FString& MontagePath, const FString& SectionName)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return FString();

	const FName SectionFName(*SectionName);
	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		if (Section.SectionName == SectionFName)
		{
			return Section.NextSectionName.ToString();
		}
	}
	return FString();
}

bool UAnimMontageService::SetNextSection(
	const FString& MontagePath,
	const FString& SectionName,
	const FString& NextSectionName)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	// Validate source section exists
	if (!ValidateSection(Montage, SectionName))
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::SetNextSection: Section '%s' not found"), *SectionName);
		return false;
	}

	// Validate target section exists (if not empty)
	if (!NextSectionName.IsEmpty() && !ValidateSection(Montage, NextSectionName))
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::SetNextSection: Target section '%s' not found"), *NextSectionName);
		return false;
	}

	Montage->Modify();

	// Find section and set NextSectionName directly
	const FName SectionFName(*SectionName);
	const FName NextFName(*NextSectionName);
	for (FCompositeSection& Section : Montage->CompositeSections)
	{
		if (Section.SectionName == SectionFName)
		{
			Section.NextSectionName = NextFName;
			MarkMontageModified(Montage);
			return true;
		}
	}

	return false;
}

bool UAnimMontageService::SetSectionLoop(
	const FString& MontagePath,
	const FString& SectionName,
	bool bLoop)
{
	if (bLoop)
	{
		return SetNextSection(MontagePath, SectionName, SectionName);
	}
	else
	{
		return ClearSectionLink(MontagePath, SectionName);
	}
}

TArray<FSectionLink> UAnimMontageService::GetAllSectionLinks(const FString& MontagePath)
{
	TArray<FSectionLink> Results;

	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return Results;

	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		if (!Section.NextSectionName.IsNone())
		{
			FSectionLink Link;
			Link.FromSection = Section.SectionName.ToString();
			Link.ToSection = Section.NextSectionName.ToString();
			Link.bIsLoop = (Section.NextSectionName == Section.SectionName);
			Results.Add(Link);
		}
	}

	return Results;
}

bool UAnimMontageService::ClearSectionLink(const FString& MontagePath, const FString& SectionName)
{
	return SetNextSection(MontagePath, SectionName, TEXT(""));
}

// ============================================================================
// SLOT TRACK MANAGEMENT
// ============================================================================

TArray<FSlotTrackInfo> UAnimMontageService::ListSlotTracks(const FString& MontagePath)
{
	TArray<FSlotTrackInfo> Results;

	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return Results;

	for (int32 i = 0; i < Montage->SlotAnimTracks.Num(); ++i)
	{
		const FSlotAnimationTrack& Track = Montage->SlotAnimTracks[i];

		FSlotTrackInfo Info;
		Info.TrackIndex = i;
		Info.SlotName = Track.SlotName.ToString();
		Info.SegmentCount = Track.AnimTrack.AnimSegments.Num();

		// Calculate total duration
		Info.TotalDuration = 0.0f;
		for (const FAnimSegment& Seg : Track.AnimTrack.AnimSegments)
		{
			float SegEnd = Seg.StartPos + Seg.GetLength();
			Info.TotalDuration = FMath::Max(Info.TotalDuration, SegEnd);
		}

		Results.Add(Info);
	}

	return Results;
}

bool UAnimMontageService::GetSlotTrackInfo(
	const FString& MontagePath,
	int32 TrackIndex,
	FSlotTrackInfo& OutInfo)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (!ValidateTrackIndex(Montage, TrackIndex)) return false;

	const FSlotAnimationTrack& Track = Montage->SlotAnimTracks[TrackIndex];

	OutInfo.TrackIndex = TrackIndex;
	OutInfo.SlotName = Track.SlotName.ToString();
	OutInfo.SegmentCount = Track.AnimTrack.AnimSegments.Num();

	OutInfo.TotalDuration = 0.0f;
	for (const FAnimSegment& Seg : Track.AnimTrack.AnimSegments)
	{
		float SegEnd = Seg.StartPos + Seg.GetLength();
		OutInfo.TotalDuration = FMath::Max(OutInfo.TotalDuration, SegEnd);
	}

	return true;
}

int32 UAnimMontageService::AddSlotTrack(const FString& MontagePath, const FString& SlotName)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return -1;

	Montage->Modify();

	FSlotAnimationTrack& NewTrack = Montage->SlotAnimTracks.AddDefaulted_GetRef();
	NewTrack.SlotName = FName(*SlotName);

	MarkMontageModified(Montage);

	return Montage->SlotAnimTracks.Num() - 1;
}

bool UAnimMontageService::RemoveSlotTrack(const FString& MontagePath, int32 TrackIndex)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (Montage->SlotAnimTracks.Num() <= 1)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::RemoveSlotTrack: Cannot remove the only slot track"));
		return false;
	}

	if (!ValidateTrackIndex(Montage, TrackIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::RemoveSlotTrack: Invalid track index %d"), TrackIndex);
		return false;
	}

	Montage->Modify();
	Montage->SlotAnimTracks.RemoveAt(TrackIndex);
	MarkMontageModified(Montage);

	return true;
}

bool UAnimMontageService::SetSlotName(
	const FString& MontagePath,
	int32 TrackIndex,
	const FString& NewSlotName)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (!ValidateTrackIndex(Montage, TrackIndex)) return false;

	Montage->Modify();
	Montage->SlotAnimTracks[TrackIndex].SlotName = FName(*NewSlotName);
	MarkMontageModified(Montage);

	return true;
}

TArray<FString> UAnimMontageService::GetAllUsedSlotNames(const FString& MontagePath)
{
	TArray<FString> Results;

	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return Results;

	for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		Results.AddUnique(Track.SlotName.ToString());
	}

	return Results;
}

// ============================================================================
// ANIMATION SEGMENTS
// ============================================================================

TArray<FAnimSegmentInfo> UAnimMontageService::ListAnimSegments(const FString& MontagePath, int32 TrackIndex)
{
	TArray<FAnimSegmentInfo> Results;

	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return Results;

	if (!ValidateTrackIndex(Montage, TrackIndex)) return Results;

	const TArray<FAnimSegment>& Segments = Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments;

	for (int32 i = 0; i < Segments.Num(); ++i)
	{
		const FAnimSegment& Seg = Segments[i];

		FAnimSegmentInfo Info;
		Info.SegmentIndex = i;

		if (UAnimSequenceBase* AnimSeqBase = Seg.GetAnimReference())
		{
			Info.AnimSequencePath = AnimSeqBase->GetPathName();
			Info.AnimName = AnimSeqBase->GetName();
		}

		Info.StartTime = Seg.StartPos;
		Info.Duration = Seg.GetLength();
		Info.PlayRate = Seg.AnimPlayRate;
		Info.AnimStartPos = Seg.AnimStartTime;
		Info.AnimEndPos = Seg.AnimEndTime;
		Info.LoopCount = Seg.LoopingCount;
		Info.bLoops = Seg.LoopingCount > 0;

		Results.Add(Info);
	}

	return Results;
}

bool UAnimMontageService::GetAnimSegmentInfo(
	const FString& MontagePath,
	int32 TrackIndex,
	int32 SegmentIndex,
	FAnimSegmentInfo& OutInfo)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (!ValidateSegmentIndex(Montage, TrackIndex, SegmentIndex)) return false;

	const FAnimSegment& Seg = Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments[SegmentIndex];

	OutInfo.SegmentIndex = SegmentIndex;

	if (UAnimSequenceBase* AnimSeqBase = Seg.GetAnimReference())
	{
		OutInfo.AnimSequencePath = AnimSeqBase->GetPathName();
		OutInfo.AnimName = AnimSeqBase->GetName();
	}

	OutInfo.StartTime = Seg.StartPos;
	OutInfo.Duration = Seg.GetLength();
	OutInfo.PlayRate = Seg.AnimPlayRate;
	OutInfo.AnimStartPos = Seg.AnimStartTime;
	OutInfo.AnimEndPos = Seg.AnimEndTime;
	OutInfo.LoopCount = Seg.LoopingCount;
	OutInfo.bLoops = Seg.LoopingCount > 0;

	return true;
}

int32 UAnimMontageService::AddAnimSegment(
	const FString& MontagePath,
	int32 TrackIndex,
	const FString& AnimSequencePath,
	float StartTime,
	float PlayRate)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return -1;

	if (!ValidateTrackIndex(Montage, TrackIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::AddAnimSegment: Invalid track index %d"), TrackIndex);
		return -1;
	}

	UAnimSequence* AnimSeq = Cast<UAnimSequence>(UEditorAssetLibrary::LoadAsset(AnimSequencePath));
	if (!AnimSeq)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::AddAnimSegment: Failed to load animation '%s'"), *AnimSequencePath);
		return -1;
	}

	// Verify skeleton compatibility
	if (Montage->GetSkeleton() != AnimSeq->GetSkeleton())
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::AddAnimSegment: Animation skeleton mismatch"));
		return -1;
	}

	Montage->Modify();

	FAnimSegment NewSegment;
	NewSegment.SetAnimReference(AnimSeq);
	NewSegment.StartPos = StartTime;
	NewSegment.AnimStartTime = 0.0f;
	NewSegment.AnimEndTime = AnimSeq->GetPlayLength();
	NewSegment.AnimPlayRate = FMath::Max(0.01f, PlayRate);
	NewSegment.LoopingCount = 1;  // Must be at least 1 to play the animation once

	int32 Index = Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments.Add(NewSegment);

	// The montage will recalculate its length on PostEditChange via MarkMontageModified
	// SetCompositeLength requires DataModelInterface which may be null
	
#if WITH_EDITOR
	// Update linkable elements (sections, notifies) after the new segment
	Montage->UpdateLinkableElements(TrackIndex, Index);
#endif

	MarkMontageModified(Montage);

	return Index;
}

bool UAnimMontageService::RemoveAnimSegment(
	const FString& MontagePath,
	int32 TrackIndex,
	int32 SegmentIndex)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (!ValidateSegmentIndex(Montage, TrackIndex, SegmentIndex))
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::RemoveAnimSegment: Invalid indices track=%d segment=%d"),
			TrackIndex, SegmentIndex);
		return false;
	}

	Montage->Modify();
	Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments.RemoveAt(SegmentIndex);
	
#if WITH_EDITOR
	// Update linkable elements
	Montage->UpdateLinkableElements();
#endif

	MarkMontageModified(Montage);

	return true;
}

bool UAnimMontageService::SetSegmentStartTime(
	const FString& MontagePath,
	int32 TrackIndex,
	int32 SegmentIndex,
	float NewStartTime)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (!ValidateSegmentIndex(Montage, TrackIndex, SegmentIndex)) return false;

	Montage->Modify();
	Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments[SegmentIndex].StartPos = NewStartTime;
	MarkMontageModified(Montage);

	return true;
}

bool UAnimMontageService::SetSegmentPlayRate(
	const FString& MontagePath,
	int32 TrackIndex,
	int32 SegmentIndex,
	float PlayRate)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (!ValidateSegmentIndex(Montage, TrackIndex, SegmentIndex)) return false;

	Montage->Modify();
	Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments[SegmentIndex].AnimPlayRate = FMath::Max(0.01f, PlayRate);
	MarkMontageModified(Montage);

	return true;
}

bool UAnimMontageService::SetSegmentStartPosition(
	const FString& MontagePath,
	int32 TrackIndex,
	int32 SegmentIndex,
	float AnimStartPos)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (!ValidateSegmentIndex(Montage, TrackIndex, SegmentIndex)) return false;

	Montage->Modify();
	Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments[SegmentIndex].AnimStartTime = AnimStartPos;
	MarkMontageModified(Montage);

	return true;
}

bool UAnimMontageService::SetSegmentEndPosition(
	const FString& MontagePath,
	int32 TrackIndex,
	int32 SegmentIndex,
	float AnimEndPos)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (!ValidateSegmentIndex(Montage, TrackIndex, SegmentIndex)) return false;

	Montage->Modify();
	Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments[SegmentIndex].AnimEndTime = AnimEndPos;
	MarkMontageModified(Montage);

	return true;
}

bool UAnimMontageService::SetSegmentLoopCount(
	const FString& MontagePath,
	int32 TrackIndex,
	int32 SegmentIndex,
	int32 LoopCount)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (!ValidateSegmentIndex(Montage, TrackIndex, SegmentIndex)) return false;

	Montage->Modify();
	Montage->SlotAnimTracks[TrackIndex].AnimTrack.AnimSegments[SegmentIndex].LoopingCount = FMath::Max(0, LoopCount);
	MarkMontageModified(Montage);

	return true;
}

// ============================================================================
// MONTAGE NOTIFIES
// ============================================================================

TArray<FMontageNotifyInfo> UAnimMontageService::ListNotifies(const FString& MontagePath)
{
	TArray<FMontageNotifyInfo> Results;

	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return Results;

	for (int32 i = 0; i < Montage->Notifies.Num(); ++i)
	{
		const FAnimNotifyEvent& Notify = Montage->Notifies[i];

		FMontageNotifyInfo Info;
		Info.NotifyIndex = i;
		Info.NotifyName = Notify.NotifyName.ToString();
		Info.TriggerTime = Notify.GetTriggerTime();
		Info.Duration = Notify.GetDuration();
		Info.bIsState = Notify.NotifyStateClass != nullptr;
		Info.bIsBranchingPoint = (Notify.MontageTickType == EMontageNotifyTickType::BranchingPoint);
		Info.TrackIndex = Notify.TrackIndex;

		if (Notify.Notify)
		{
			Info.NotifyClass = Notify.Notify->GetClass()->GetPathName();
		}
		else if (Notify.NotifyStateClass)
		{
			Info.NotifyClass = Notify.NotifyStateClass->GetClass()->GetPathName();
		}

		// Get linked section
		int32 SectionIdx = Montage->GetSectionIndexFromPosition(Info.TriggerTime);
		if (SectionIdx != INDEX_NONE)
		{
			Info.LinkedSectionName = Montage->GetSectionName(SectionIdx).ToString();
		}

		Results.Add(Info);
	}

	return Results;
}

int32 UAnimMontageService::AddNotify(
	const FString& MontagePath,
	const FString& NotifyClass,
	float TriggerTime,
	const FString& NotifyName)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return -1;

	if (TriggerTime < 0.0f || TriggerTime > Montage->GetPlayLength())
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::AddNotify: TriggerTime %.2f is out of range"), TriggerTime);
		return -1;
	}

	// Load notify class
	UClass* NotifyUClass = FindObject<UClass>(nullptr, *NotifyClass);
	if (!NotifyUClass)
	{
		NotifyUClass = LoadObject<UClass>(nullptr, *NotifyClass);
	}

	if (!NotifyUClass || !NotifyUClass->IsChildOf(UAnimNotify::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::AddNotify: Invalid notify class '%s'"), *NotifyClass);
		return -1;
	}

	Montage->Modify();

	// Create new notify
	FAnimNotifyEvent& NewNotify = Montage->Notifies.AddDefaulted_GetRef();
	NewNotify.NotifyName = NotifyName.IsEmpty() ? FName(*NotifyUClass->GetName()) : FName(*NotifyName);

	// Create the notify object
	UAnimNotify* NotifyObj = NewObject<UAnimNotify>(Montage, NotifyUClass, NAME_None, RF_Transactional);
	NewNotify.Notify = NotifyObj;

	// Set time
	NewNotify.Link(Montage, TriggerTime);
	NewNotify.TriggerTimeOffset = 0.0f;
	NewNotify.TrackIndex = 0;

	MarkMontageModified(Montage);

	return Montage->Notifies.Num() - 1;
}

int32 UAnimMontageService::AddNotifyState(
	const FString& MontagePath,
	const FString& NotifyStateClass,
	float StartTime,
	float Duration,
	const FString& NotifyName)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return -1;

	if (StartTime < 0.0f || StartTime > Montage->GetPlayLength())
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::AddNotifyState: StartTime %.2f is out of range"), StartTime);
		return -1;
	}

	// Load notify state class
	UClass* NotifyUClass = FindObject<UClass>(nullptr, *NotifyStateClass);
	if (!NotifyUClass)
	{
		NotifyUClass = LoadObject<UClass>(nullptr, *NotifyStateClass);
	}

	if (!NotifyUClass || !NotifyUClass->IsChildOf(UAnimNotifyState::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::AddNotifyState: Invalid notify state class '%s'"), *NotifyStateClass);
		return -1;
	}

	Montage->Modify();

	// Create new notify
	FAnimNotifyEvent& NewNotify = Montage->Notifies.AddDefaulted_GetRef();
	NewNotify.NotifyName = NotifyName.IsEmpty() ? FName(*NotifyUClass->GetName()) : FName(*NotifyName);

	// Create the notify state object
	UAnimNotifyState* NotifyStateObj = NewObject<UAnimNotifyState>(Montage, NotifyUClass, NAME_None, RF_Transactional);
	NewNotify.NotifyStateClass = NotifyStateObj;

	// Set time and duration
	NewNotify.Link(Montage, StartTime);
	NewNotify.SetDuration(Duration);
	NewNotify.TriggerTimeOffset = 0.0f;
	NewNotify.TrackIndex = 0;

	MarkMontageModified(Montage);

	return Montage->Notifies.Num() - 1;
}

bool UAnimMontageService::RemoveNotify(const FString& MontagePath, int32 NotifyIndex)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (NotifyIndex < 0 || NotifyIndex >= Montage->Notifies.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::RemoveNotify: Invalid notify index %d"), NotifyIndex);
		return false;
	}

	Montage->Modify();
	Montage->Notifies.RemoveAt(NotifyIndex);
	MarkMontageModified(Montage);

	return true;
}

bool UAnimMontageService::SetNotifyTriggerTime(
	const FString& MontagePath,
	int32 NotifyIndex,
	float NewTime)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (NotifyIndex < 0 || NotifyIndex >= Montage->Notifies.Num()) return false;

	if (NewTime < 0.0f || NewTime > Montage->GetPlayLength()) return false;

	Montage->Modify();
	Montage->Notifies[NotifyIndex].Link(Montage, NewTime);
	MarkMontageModified(Montage);

	return true;
}

bool UAnimMontageService::SetNotifyLinkToSection(
	const FString& MontagePath,
	int32 NotifyIndex,
	const FString& SectionName)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (NotifyIndex < 0 || NotifyIndex >= Montage->Notifies.Num()) return false;

	int32 SectionIndex = Montage->GetSectionIndex(FName(*SectionName));
	if (SectionIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::SetNotifyLinkToSection: Section '%s' not found"), *SectionName);
		return false;
	}

	Montage->Modify();
	// Move notify to section start time
	float SectionTime = 0.0f;
	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		if (Section.SectionName == FName(*SectionName))
		{
			SectionTime = Section.GetTime();
			break;
		}
	}
	Montage->Notifies[NotifyIndex].Link(Montage, SectionTime);
	MarkMontageModified(Montage);

	return true;
}

// ============================================================================
// BRANCHING POINTS
// ============================================================================

TArray<FBranchingPointInfo> UAnimMontageService::ListBranchingPoints(const FString& MontagePath)
{
	TArray<FBranchingPointInfo> Results;

	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return Results;

	int32 BPIndex = 0;
	for (int32 i = 0; i < Montage->Notifies.Num(); ++i)
	{
		const FAnimNotifyEvent& Notify = Montage->Notifies[i];

		if (Notify.MontageTickType == EMontageNotifyTickType::BranchingPoint)
		{
			FBranchingPointInfo Info;
			Info.Index = BPIndex++;
			Info.NotifyName = Notify.NotifyName.ToString();
			Info.TriggerTime = Notify.GetTriggerTime();

			// Find which section this branching point is in
			int32 SectionIdx = Montage->GetSectionIndexFromPosition(Info.TriggerTime);
			if (SectionIdx != INDEX_NONE)
			{
				Info.SectionName = Montage->GetSectionName(SectionIdx).ToString();
			}

			Results.Add(Info);
		}
	}

	return Results;
}

int32 UAnimMontageService::AddBranchingPoint(
	const FString& MontagePath,
	const FString& NotifyName,
	float TriggerTime)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return -1;

	if (TriggerTime < 0.0f || TriggerTime > Montage->GetPlayLength())
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::AddBranchingPoint: TriggerTime %.2f is out of range"), TriggerTime);
		return -1;
	}

	Montage->Modify();

	FAnimNotifyEvent& NewNotify = Montage->Notifies.AddDefaulted_GetRef();
	NewNotify.NotifyName = FName(*NotifyName);
	NewNotify.Link(Montage, TriggerTime);
	NewNotify.TriggerTimeOffset = 0.0f;
	NewNotify.MontageTickType = EMontageNotifyTickType::BranchingPoint;
	NewNotify.TrackIndex = 0;

	MarkMontageModified(Montage);

	// Return index among branching points only
	int32 BPCount = 0;
	for (const FAnimNotifyEvent& N : Montage->Notifies)
	{
		if (N.MontageTickType == EMontageNotifyTickType::BranchingPoint)
		{
			BPCount++;
		}
	}
	return BPCount - 1;
}

bool UAnimMontageService::RemoveBranchingPoint(const FString& MontagePath, int32 Index)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	// Find the branching point at this index
	int32 BPCount = 0;
	for (int32 i = 0; i < Montage->Notifies.Num(); ++i)
	{
		if (Montage->Notifies[i].MontageTickType == EMontageNotifyTickType::BranchingPoint)
		{
			if (BPCount == Index)
			{
				Montage->Modify();
				Montage->Notifies.RemoveAt(i);
				MarkMontageModified(Montage);
				return true;
			}
			BPCount++;
		}
	}

	UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::RemoveBranchingPoint: Branching point index %d not found"), Index);
	return false;
}

bool UAnimMontageService::IsBranchingPointAtTime(const FString& MontagePath, float Time)
{
	TArray<FBranchingPointInfo> BPs = ListBranchingPoints(MontagePath);
	for (const FBranchingPointInfo& BP : BPs)
	{
		if (FMath::IsNearlyEqual(BP.TriggerTime, Time, 0.01f))
		{
			return true;
		}
	}
	return false;
}

// ============================================================================
// ROOT MOTION
// ============================================================================

bool UAnimMontageService::GetEnableRootMotionTranslation(const FString& MontagePath)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	return Montage->bEnableRootMotionTranslation;
}

bool UAnimMontageService::SetEnableRootMotionTranslation(const FString& MontagePath, bool bEnable)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	Montage->Modify();
	Montage->bEnableRootMotionTranslation = bEnable;
	MarkMontageModified(Montage);

	return true;
}

bool UAnimMontageService::GetEnableRootMotionRotation(const FString& MontagePath)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	return Montage->bEnableRootMotionRotation;
}

bool UAnimMontageService::SetEnableRootMotionRotation(const FString& MontagePath, bool bEnable)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	Montage->Modify();
	Montage->bEnableRootMotionRotation = bEnable;
	MarkMontageModified(Montage);

	return true;
}

bool UAnimMontageService::GetRootMotionAtTime(
	const FString& MontagePath,
	float Time,
	FTransform& OutTransform)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	// Get root motion from the montage's underlying animation segments
	OutTransform = FTransform::Identity;

	if (Montage->SlotAnimTracks.Num() > 0 && Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() > 0)
	{
		for (const FAnimSegment& Seg : Montage->SlotAnimTracks[0].AnimTrack.AnimSegments)
		{
			if (Time >= Seg.StartPos && Time < Seg.StartPos + Seg.GetLength())
			{
				if (UAnimSequence* AnimSeq = Cast<UAnimSequence>(Seg.GetAnimReference()))
				{
					double LocalTime = static_cast<double>((Time - Seg.StartPos) * Seg.AnimPlayRate + Seg.AnimStartTime);
					// Use FAnimExtractContext with double to avoid deprecation warning
					FAnimExtractContext ExtractionContext(LocalTime, true);
					OutTransform = AnimSeq->ExtractRootMotion(ExtractionContext);
				}
				break;
			}
		}
	}

	return true;
}

// ============================================================================
// MONTAGE CREATION
// ============================================================================

FString UAnimMontageService::CreateMontageFromAnimation(
	const FString& AnimSequencePath,
	const FString& DestPath,
	const FString& MontageName)
{
	// Load animation sequence
	UAnimSequence* AnimSeq = Cast<UAnimSequence>(UEditorAssetLibrary::LoadAsset(AnimSequencePath));
	if (!AnimSeq)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::CreateMontageFromAnimation: Failed to load animation '%s'"),
			*AnimSequencePath);
		return FString();
	}

	// Get skeleton
	USkeleton* Skeleton = AnimSeq->GetSkeleton();
	if (!Skeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::CreateMontageFromAnimation: Animation has no skeleton"));
		return FString();
	}

	// Construct full asset path
	FString FullPath = DestPath;
	if (!FullPath.EndsWith(TEXT("/")))
	{
		FullPath += TEXT("/");
	}
	FullPath += MontageName;

	// Check if asset already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimMontageService::CreateMontageFromAnimation: Asset already exists: %s"), *FullPath);
		return FString();
	}

	// Create factory
	UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
	Factory->TargetSkeleton = Skeleton;
	Factory->SourceAnimation = AnimSeq;

	// Create the montage
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UAnimMontage* NewMontage = Cast<UAnimMontage>(
		AssetTools.CreateAsset(MontageName, DestPath, UAnimMontage::StaticClass(), Factory)
	);

	if (!NewMontage)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::CreateMontageFromAnimation: Failed to create montage"));
		return FString();
	}

	// Set default blend settings
	NewMontage->BlendIn.SetBlendTime(0.25f);
	NewMontage->BlendOut.SetBlendTime(0.25f);
	NewMontage->BlendOutTriggerTime = -1.0f;

	NewMontage->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(FullPath);

	UE_LOG(LogTemp, Log, TEXT("UAnimMontageService::CreateMontageFromAnimation: Created montage: %s"), *FullPath);
	return FullPath;
}

FString UAnimMontageService::CreateEmptyMontage(
	const FString& SkeletonPath,
	const FString& DestPath,
	const FString& MontageName)
{
	// Load skeleton
	USkeleton* Skeleton = Cast<USkeleton>(UEditorAssetLibrary::LoadAsset(SkeletonPath));
	if (!Skeleton)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::CreateEmptyMontage: Failed to load skeleton '%s'"),
			*SkeletonPath);
		return FString();
	}

	// Construct full asset path
	FString FullPath = DestPath;
	if (!FullPath.EndsWith(TEXT("/")))
	{
		FullPath += TEXT("/");
	}
	FullPath += MontageName;

	// Check if asset already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimMontageService::CreateEmptyMontage: Asset already exists: %s"), *FullPath);
		return FString();
	}

	// Create factory
	UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
	Factory->TargetSkeleton = Skeleton;

	// Create the montage
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	UAnimMontage* NewMontage = Cast<UAnimMontage>(
		AssetTools.CreateAsset(MontageName, DestPath, UAnimMontage::StaticClass(), Factory)
	);

	if (!NewMontage)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::CreateEmptyMontage: Failed to create montage"));
		return FString();
	}

	// Ensure we have at least one slot track
	if (NewMontage->SlotAnimTracks.Num() == 0)
	{
		FSlotAnimationTrack& Track = NewMontage->SlotAnimTracks.AddDefaulted_GetRef();
		Track.SlotName = FName("DefaultSlot");
	}

	// Ensure we have at least one section
	if (NewMontage->CompositeSections.Num() == 0)
	{
		NewMontage->AddAnimCompositeSection(FName("Default"), 0.0f);
	}

	NewMontage->BlendIn.SetBlendTime(0.25f);
	NewMontage->BlendOut.SetBlendTime(0.25f);

	NewMontage->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(FullPath);

	UE_LOG(LogTemp, Log, TEXT("UAnimMontageService::CreateEmptyMontage: Created montage: %s"), *FullPath);
	return FullPath;
}

FString UAnimMontageService::DuplicateMontage(
	const FString& SourcePath,
	const FString& DestPath,
	const FString& NewName)
{
	// Load source montage
	UAnimMontage* SourceMontage = LoadMontage(SourcePath);
	if (!SourceMontage)
	{
		return FString();
	}

	// Construct full destination path
	FString FullPath = DestPath;
	if (!FullPath.EndsWith(TEXT("/")))
	{
		FullPath += TEXT("/");
	}
	FullPath += NewName;

	// Check if asset already exists
	if (UEditorAssetLibrary::DoesAssetExist(FullPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimMontageService::DuplicateMontage: Asset already exists: %s"), *FullPath);
		return FString();
	}

	// Duplicate the asset
	if (UEditorAssetLibrary::DuplicateAsset(SourcePath, FullPath))
	{
		UE_LOG(LogTemp, Log, TEXT("UAnimMontageService::DuplicateMontage: Created duplicate: %s"), *FullPath);
		return FullPath;
	}

	UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::DuplicateMontage: Failed to duplicate montage"));
	return FString();
}

// ============================================================================
// EDITOR NAVIGATION
// ============================================================================

bool UAnimMontageService::OpenMontageEditor(const FString& MontagePath)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (GEditor)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Montage);
		return true;
	}

	return false;
}

bool UAnimMontageService::RefreshMontageEditor(const FString& MontagePath)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (!GEditor) return false;

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem) return false;

	// Close the editor if it's open
	AssetEditorSubsystem->CloseAllEditorsForAsset(Montage);

	// Reopen it to force a full refresh
	AssetEditorSubsystem->OpenEditorForAsset(Montage);

	return true;
}

bool UAnimMontageService::JumpToSection(const FString& MontagePath, const FString& SectionName)
{
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	int32 SectionIndex = Montage->GetSectionIndex(FName(*SectionName));
	if (SectionIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Error, TEXT("UAnimMontageService::JumpToSection: Section '%s' not found"), *SectionName);
		return false;
	}

	// Open editor and try to set time to section start
	OpenMontageEditor(MontagePath);

	// Get section start time
	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		if (Section.SectionName == FName(*SectionName))
		{
			return SetPreviewTime(MontagePath, Section.GetTime());
		}
	}

	return false;
}

bool UAnimMontageService::SetPreviewTime(const FString& MontagePath, float Time)
{
	// Note: This requires the animation editor to be open
	// The actual preview time setting depends on editor state
	// For now, we just validate the time is in range
	UAnimMontage* Montage = LoadMontage(MontagePath);
	if (!Montage) return false;

	if (Time < 0.0f || Time > Montage->GetPlayLength())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimMontageService::SetPreviewTime: Time %.2f is out of range"), Time);
		return false;
	}

	// The actual preview time would need to be set via the animation editor
	// This is a placeholder that validates and opens the editor
	OpenMontageEditor(MontagePath);
	return true;
}

bool UAnimMontageService::PlayPreview(const FString& MontagePath, const FString& StartSection)
{
	// Note: Controlling animation playback requires editor-specific APIs
	// This opens the editor at the section start
	if (!StartSection.IsEmpty())
	{
		return JumpToSection(MontagePath, StartSection);
	}

	return OpenMontageEditor(MontagePath);
}
