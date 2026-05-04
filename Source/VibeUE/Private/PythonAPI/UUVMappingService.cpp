// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UUVMappingService.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshDescription.h"
#include "StaticMeshOperations.h"
#include "UVMapSettings.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "UObject/Package.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogVibeUVMapping, Log, All);

// =================================================================
// Result helpers
// =================================================================

FUVMappingResult UUVMappingService::MakeFailure(const FString& MeshPath, const FString& Message)
{
	FUVMappingResult Result;
	Result.bSuccess = false;
	Result.MeshPath = MeshPath;
	Result.Message = Message;
	UE_LOG(LogVibeUVMapping, Warning, TEXT("UVMappingService failure on %s: %s"), *MeshPath, *Message);
	return Result;
}

FUVMappingResult UUVMappingService::MakeSuccess(const FString& MeshPath, const FString& Message)
{
	FUVMappingResult Result;
	Result.bSuccess = true;
	Result.MeshPath = MeshPath;
	Result.Message = Message;
	return Result;
}

// =================================================================
// Asset loading
// =================================================================

UStaticMesh* UUVMappingService::LoadStaticMeshAsset(const FString& MeshPath)
{
	UObject* Loaded = UEditorAssetLibrary::LoadAsset(MeshPath);
	return Cast<UStaticMesh>(Loaded);
}

USkeletalMesh* UUVMappingService::LoadSkeletalMeshAsset(const FString& MeshPath)
{
	UObject* Loaded = UEditorAssetLibrary::LoadAsset(MeshPath);
	return Cast<USkeletalMesh>(Loaded);
}

bool UUVMappingService::IsStaticMeshPath(const FString& MeshPath)
{
	UObject* Loaded = UEditorAssetLibrary::LoadAsset(MeshPath);
	return Loaded != nullptr && Loaded->IsA<UStaticMesh>();
}

FMeshDescription* UUVMappingService::GetStaticMeshDescription(UStaticMesh* Mesh, int32 LODIndex)
{
	if (!Mesh) return nullptr;
	if (LODIndex < 0 || LODIndex >= Mesh->GetNumSourceModels()) return nullptr;
	return Mesh->GetMeshDescription(LODIndex);
}

void UUVMappingService::CommitStaticMeshDescription(UStaticMesh* Mesh, int32 LODIndex)
{
	if (!Mesh) return;

	UStaticMesh::FCommitMeshDescriptionParams Params;
	Params.bMarkPackageDirty = true;
	Params.bUseHashAsGuid = false;
	Mesh->CommitMeshDescription(LODIndex, Params);

	Mesh->PostEditChange();
	Mesh->MarkPackageDirty();
}

int32 UUVMappingService::GetStaticMeshNumUVChannels(const UStaticMesh* Mesh, int32 LODIndex)
{
	if (!Mesh) return 0;
	if (LODIndex < 0 || LODIndex >= Mesh->GetNumSourceModels()) return 0;

	UStaticMesh* MutableMesh = const_cast<UStaticMesh*>(Mesh);
	const FMeshDescription* MeshDesc = MutableMesh->GetMeshDescription(LODIndex);
	if (!MeshDesc) return 0;

	FStaticMeshConstAttributes Attrs(*MeshDesc);
	return Attrs.GetVertexInstanceUVs().GetNumChannels();
}

int32 UUVMappingService::GetSkeletalMeshNumUVChannels(const USkeletalMesh* Mesh, int32 LODIndex)
{
	if (!Mesh) return 0;
	const FSkeletalMeshModel* Model = Mesh->GetImportedModel();
	if (!Model || !Model->LODModels.IsValidIndex(LODIndex)) return 0;
	return Model->LODModels[LODIndex].NumTexCoords;
}

// =================================================================
// Stat computation
// =================================================================

namespace
{
	struct FTriUVStats
	{
		FVector2f UV0, UV1, UV2;
		FBox2f Bounds;
		float UVArea = 0.0f;
		float WorldArea = 0.0f;
	};

	static FTriUVStats BuildTriStats(
		const FMeshDescription& MeshDesc,
		const FStaticMeshConstAttributes& Attrs,
		FTriangleID TriID,
		int32 ChannelIndex)
	{
		FTriUVStats Stats;

		TArrayView<const FVertexInstanceID> VIs = MeshDesc.GetTriangleVertexInstances(TriID);
		const auto& UVs = Attrs.GetVertexInstanceUVs();
		const auto& Positions = Attrs.GetVertexPositions();

		Stats.UV0 = UVs.Get(VIs[0], ChannelIndex);
		Stats.UV1 = UVs.Get(VIs[1], ChannelIndex);
		Stats.UV2 = UVs.Get(VIs[2], ChannelIndex);

		Stats.Bounds = FBox2f(ForceInit);
		Stats.Bounds += Stats.UV0;
		Stats.Bounds += Stats.UV1;
		Stats.Bounds += Stats.UV2;

		const FVector2f E1 = Stats.UV1 - Stats.UV0;
		const FVector2f E2 = Stats.UV2 - Stats.UV0;
		Stats.UVArea = 0.5f * FMath::Abs(E1.X * E2.Y - E1.Y * E2.X);

		const FVertexID V0 = MeshDesc.GetVertexInstanceVertex(VIs[0]);
		const FVertexID V1 = MeshDesc.GetVertexInstanceVertex(VIs[1]);
		const FVertexID V2 = MeshDesc.GetVertexInstanceVertex(VIs[2]);
		const FVector3f P0 = Positions[V0];
		const FVector3f P1 = Positions[V1];
		const FVector3f P2 = Positions[V2];
		Stats.WorldArea = 0.5f * FVector3f::CrossProduct(P1 - P0, P2 - P0).Size();

		return Stats;
	}

	static bool BoundsOverlap(const FBox2f& A, const FBox2f& B)
	{
		return !(A.Max.X < B.Min.X || B.Max.X < A.Min.X ||
		         A.Max.Y < B.Min.Y || B.Max.Y < A.Min.Y);
	}
}

void UUVMappingService::ComputeChannelStats(
	const FMeshDescription& MeshDesc,
	int32 ChannelIndex,
	FUVChannelInfo& OutInfo)
{
	FStaticMeshConstAttributes Attrs(MeshDesc);
	const auto& UVs = Attrs.GetVertexInstanceUVs();

	OutInfo.ChannelIndex = ChannelIndex;
	OutInfo.VertexInstanceCount = MeshDesc.VertexInstances().Num();
	OutInfo.TriangleCount = MeshDesc.Triangles().Num();

	if (ChannelIndex < 0 || ChannelIndex >= UVs.GetNumChannels())
	{
		return;
	}

	// UV bounds + in-unit-square fraction
	FBox2f Bounds(ForceInit);
	int32 InUnitSquare = 0;
	int32 TotalUVs = 0;
	for (FVertexInstanceID VI : MeshDesc.VertexInstances().GetElementIDs())
	{
		const FVector2f UV = UVs.Get(VI, ChannelIndex);
		Bounds += UV;
		++TotalUVs;
		if (UV.X >= 0.0f && UV.X <= 1.0f && UV.Y >= 0.0f && UV.Y <= 1.0f)
		{
			++InUnitSquare;
		}
	}

	if (TotalUVs > 0)
	{
		OutInfo.MinU = Bounds.Min.X;
		OutInfo.MinV = Bounds.Min.Y;
		OutInfo.MaxU = Bounds.Max.X;
		OutInfo.MaxV = Bounds.Max.Y;
		OutInfo.InUnitSquarePercent = 100.0f * static_cast<float>(InUnitSquare) / static_cast<float>(TotalUVs);
	}

	// Per-triangle stats (overlap heuristic + texel density)
	TArray<FTriUVStats> Tris;
	Tris.Reserve(OutInfo.TriangleCount);
	for (FTriangleID TriID : MeshDesc.Triangles().GetElementIDs())
	{
		Tris.Add(BuildTriStats(MeshDesc, Attrs, TriID, ChannelIndex));
	}

	// AABB-overlap heuristic. O(N^2) but capped at 5000 triangles to stay snappy;
	// beyond that we report -1 to mean "skipped (mesh too large)".
	const int32 OverlapCap = 5000;
	if (Tris.Num() > 0 && Tris.Num() <= OverlapCap)
	{
		TBitArray<> Overlapping(false, Tris.Num());
		for (int32 i = 0; i < Tris.Num(); ++i)
		{
			for (int32 j = i + 1; j < Tris.Num(); ++j)
			{
				if (BoundsOverlap(Tris[i].Bounds, Tris[j].Bounds))
				{
					Overlapping[i] = true;
					Overlapping[j] = true;
				}
			}
		}
		int32 OverlapCount = 0;
		for (TBitArray<>::FConstIterator It(Overlapping); It; ++It)
		{
			if (It.GetValue()) ++OverlapCount;
		}
		OutInfo.OverlapPercent = 100.0f * static_cast<float>(OverlapCount) / static_cast<float>(Tris.Num());
	}
	else if (Tris.Num() > OverlapCap)
	{
		OutInfo.OverlapPercent = -1.0f;
	}

	// Texel density at 1024px reference: density = sqrt((uv_area * 1024^2) / world_area)
	double TotalDensity = 0.0;
	int32 DensitySamples = 0;
	for (const FTriUVStats& T : Tris)
	{
		if (T.WorldArea > KINDA_SMALL_NUMBER && T.UVArea > KINDA_SMALL_NUMBER)
		{
			const double D = FMath::Sqrt((T.UVArea * 1024.0 * 1024.0) / T.WorldArea);
			TotalDensity += D;
			++DensitySamples;
		}
	}
	if (DensitySamples > 0)
	{
		OutInfo.TexelDensity1k = static_cast<float>(TotalDensity / DensitySamples);
	}
}

// =================================================================
// Inspect
// =================================================================

TArray<FUVChannelInfo> UUVMappingService::ListUVChannels(const FString& MeshPath)
{
	TArray<FUVChannelInfo> Result;

	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (StaticMesh)
	{
		const int32 NumLODs = StaticMesh->GetNumSourceModels();
		for (int32 LOD = 0; LOD < NumLODs; ++LOD)
		{
			FMeshDescription* MeshDesc = StaticMesh->GetMeshDescription(LOD);
			if (!MeshDesc) continue;

			FStaticMeshConstAttributes Attrs(*MeshDesc);
			const int32 NumChannels = Attrs.GetVertexInstanceUVs().GetNumChannels();
			for (int32 Ch = 0; Ch < NumChannels; ++Ch)
			{
				FUVChannelInfo Info;
				Info.LODIndex = LOD;
				ComputeChannelStats(*MeshDesc, Ch, Info);
				Result.Add(Info);
			}
		}
		return Result;
	}

	USkeletalMesh* SkelMesh = LoadSkeletalMeshAsset(MeshPath);
	if (SkelMesh)
	{
		const FSkeletalMeshModel* Model = SkelMesh->GetImportedModel();
		if (Model)
		{
			for (int32 LOD = 0; LOD < Model->LODModels.Num(); ++LOD)
			{
				const int32 NumChannels = Model->LODModels[LOD].NumTexCoords;
				for (int32 Ch = 0; Ch < NumChannels; ++Ch)
				{
					FUVChannelInfo Info;
					Info.LODIndex = LOD;
					Info.ChannelIndex = Ch;
					Info.VertexInstanceCount = 0;
					Info.TriangleCount = 0;
					Result.Add(Info);
				}
			}
		}
		return Result;
	}

	UE_LOG(LogVibeUVMapping, Warning, TEXT("ListUVChannels: %s is not a StaticMesh or SkeletalMesh"), *MeshPath);
	return Result;
}

bool UUVMappingService::GetUVChannelInfo(
	const FString& MeshPath,
	int32 LODIndex,
	int32 ChannelIndex,
	FUVChannelInfo& OutInfo)
{
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh)
	{
		UE_LOG(LogVibeUVMapping, Warning, TEXT("GetUVChannelInfo: only StaticMesh supports detailed stats. Path: %s"), *MeshPath);
		return false;
	}
	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return false;

	OutInfo.LODIndex = LODIndex;
	ComputeChannelStats(*MeshDesc, ChannelIndex, OutInfo);
	return true;
}

bool UUVMappingService::GetUVHealth(const FString& MeshPath, FUVHealthReport& OutReport)
{
	OutReport = FUVHealthReport();
	OutReport.MeshPath = MeshPath;

	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh)
	{
		OutReport.Warnings.Add(TEXT("Asset is not a StaticMesh; SkeletalMesh health report not implemented."));
		return false;
	}

	OutReport.LODCount = StaticMesh->GetNumSourceModels();
	OutReport.LightmapCoordinateIndex = StaticMesh->GetLightMapCoordinateIndex();
	OutReport.LightMapResolution = StaticMesh->GetLightMapResolution();
	const FStaticMeshSourceModel& SM0 = StaticMesh->GetSourceModel(0);
	OutReport.bGenerateLightmapUVs = SM0.BuildSettings.bGenerateLightmapUVs;

	for (int32 LOD = 0; LOD < OutReport.LODCount; ++LOD)
	{
		FMeshDescription* MeshDesc = StaticMesh->GetMeshDescription(LOD);
		if (!MeshDesc) continue;

		FStaticMeshConstAttributes Attrs(*MeshDesc);
		const int32 NumChannels = Attrs.GetVertexInstanceUVs().GetNumChannels();
		for (int32 Ch = 0; Ch < NumChannels; ++Ch)
		{
			FUVChannelInfo Info;
			Info.LODIndex = LOD;
			ComputeChannelStats(*MeshDesc, Ch, Info);
			OutReport.Channels.Add(Info);

			if (LOD == 0 && Ch == OutReport.LightmapCoordinateIndex && Info.OverlapPercent > 0.0f)
			{
				OutReport.bLightmapHasOverlaps = true;
			}
		}

		if (NumChannels <= OutReport.LightmapCoordinateIndex && LOD == 0)
		{
			OutReport.Warnings.Add(FString::Printf(
				TEXT("LightMapCoordinateIndex (%d) is out of range for LOD %d (only %d channels)."),
				OutReport.LightmapCoordinateIndex, LOD, NumChannels));
		}
	}

	if (OutReport.bLightmapHasOverlaps)
	{
		OutReport.Warnings.Add(TEXT("Lightmap UV channel has overlapping triangles; baked lighting will produce artifacts."));
	}

	return true;
}

// =================================================================
// Channel lifecycle
// =================================================================

FUVMappingResult UUVMappingService::AddUVChannel(const FString& MeshPath, int32 LODIndex)
{
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return MakeFailure(MeshPath, FString::Printf(TEXT("LOD %d has no mesh description"), LODIndex));

	StaticMesh->Modify();
	if (!FStaticMeshOperations::AddUVChannel(*MeshDesc))
	{
		return MakeFailure(MeshPath, TEXT("FStaticMeshOperations::AddUVChannel returned false (likely at MAX_MESH_TEXTURE_COORDS_MD)"));
	}

	const int32 NewChannelIndex = FStaticMeshConstAttributes(*MeshDesc).GetVertexInstanceUVs().GetNumChannels() - 1;
	CommitStaticMeshDescription(StaticMesh, LODIndex);

	return MakeSuccess(MeshPath, FString::Printf(TEXT("Added UV channel %d to LOD %d"), NewChannelIndex, LODIndex));
}

FUVMappingResult UUVMappingService::RemoveUVChannel(
	const FString& MeshPath,
	int32 LODIndex,
	int32 ChannelIndex)
{
	if (ChannelIndex == 0)
	{
		return MakeFailure(MeshPath, TEXT("UV channel 0 cannot be removed"));
	}

	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return MakeFailure(MeshPath, FString::Printf(TEXT("LOD %d has no mesh description"), LODIndex));

	StaticMesh->Modify();
	if (!FStaticMeshOperations::RemoveUVChannel(*MeshDesc, ChannelIndex))
	{
		return MakeFailure(MeshPath, FString::Printf(TEXT("RemoveUVChannel(%d) failed"), ChannelIndex));
	}

	// Adjust LightMapCoordinateIndex if it was pointing at or above the removed channel.
	const int32 OldLM = StaticMesh->GetLightMapCoordinateIndex();
	if (OldLM == ChannelIndex)
	{
		StaticMesh->SetLightMapCoordinateIndex(0);
	}
	else if (OldLM > ChannelIndex)
	{
		StaticMesh->SetLightMapCoordinateIndex(OldLM - 1);
	}

	CommitStaticMeshDescription(StaticMesh, LODIndex);
	return MakeSuccess(MeshPath, FString::Printf(TEXT("Removed UV channel %d from LOD %d"), ChannelIndex, LODIndex));
}

FUVMappingResult UUVMappingService::CopyUVChannel(
	const FString& MeshPath,
	int32 LODIndex,
	int32 SourceChannel,
	int32 DestChannel)
{
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return MakeFailure(MeshPath, FString::Printf(TEXT("LOD %d has no mesh description"), LODIndex));

	FStaticMeshAttributes Attrs(*MeshDesc);
	const int32 NumChannels = Attrs.GetVertexInstanceUVs().GetNumChannels();
	if (SourceChannel < 0 || SourceChannel >= NumChannels)
	{
		return MakeFailure(MeshPath, FString::Printf(TEXT("Source channel %d out of range (0..%d)"), SourceChannel, NumChannels - 1));
	}

	StaticMesh->Modify();

	// Auto-grow the channel array up to DestChannel.
	while (FStaticMeshConstAttributes(*MeshDesc).GetVertexInstanceUVs().GetNumChannels() <= DestChannel)
	{
		if (!FStaticMeshOperations::AddUVChannel(*MeshDesc))
		{
			return MakeFailure(MeshPath, TEXT("Could not grow UV channel array (hit MAX_MESH_TEXTURE_COORDS_MD)"));
		}
	}

	auto UVs = Attrs.GetVertexInstanceUVs();
	for (FVertexInstanceID VI : MeshDesc->VertexInstances().GetElementIDs())
	{
		const FVector2f Src = UVs.Get(VI, SourceChannel);
		UVs.Set(VI, DestChannel, Src);
	}

	CommitStaticMeshDescription(StaticMesh, LODIndex);
	return MakeSuccess(MeshPath, FString::Printf(
		TEXT("Copied UV channel %d -> %d on LOD %d"), SourceChannel, DestChannel, LODIndex));
}

FUVMappingResult UUVMappingService::SetUVChannelCount(
	const FString& MeshPath,
	int32 LODIndex,
	int32 Count)
{
	if (Count < 1) return MakeFailure(MeshPath, TEXT("Count must be >= 1"));
	if (Count > MAX_MESH_TEXTURE_COORDS_MD)
	{
		return MakeFailure(MeshPath, FString::Printf(
			TEXT("Count %d exceeds MAX_MESH_TEXTURE_COORDS_MD (%d)"), Count, MAX_MESH_TEXTURE_COORDS_MD));
	}

	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return MakeFailure(MeshPath, FString::Printf(TEXT("LOD %d has no mesh description"), LODIndex));

	StaticMesh->Modify();
	MeshDesc->SetNumUVChannels(Count);

	// Clamp lightmap index if we shrunk below it.
	if (StaticMesh->GetLightMapCoordinateIndex() >= Count)
	{
		StaticMesh->SetLightMapCoordinateIndex(FMath::Max(0, Count - 1));
	}

	CommitStaticMeshDescription(StaticMesh, LODIndex);
	return MakeSuccess(MeshPath, FString::Printf(TEXT("LOD %d now has %d UV channels"), LODIndex, Count));
}

// =================================================================
// Generation
// =================================================================

FUVMappingResult UUVMappingService::GenerateLightmapUVs(
	const FString& MeshPath,
	int32 SourceUVIndex,
	int32 DestUVIndex,
	float MinChartSpacingPercent)
{
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));
	if (StaticMesh->GetNumSourceModels() == 0)
	{
		return MakeFailure(MeshPath, TEXT("StaticMesh has no source models"));
	}

	StaticMesh->Modify();

	// Configure build settings on LOD 0 to drive the generator.
	FStaticMeshSourceModel& SM = StaticMesh->GetSourceModel(0);
	SM.BuildSettings.bGenerateLightmapUVs = true;
	SM.BuildSettings.SrcLightmapIndex = SourceUVIndex;
	SM.BuildSettings.DstLightmapIndex = DestUVIndex;
	SM.BuildSettings.MinLightmapResolution = FMath::Max(
		StaticMesh->GetLightMapResolution(),
		FMath::CeilToInt(100.0f / FMath::Max(MinChartSpacingPercent, 0.1f)));

	StaticMesh->SetLightMapCoordinateIndex(DestUVIndex);

	// Trigger a rebuild so the lightmap channel is regenerated immediately.
	StaticMesh->Build(false);
	StaticMesh->PostEditChange();
	StaticMesh->MarkPackageDirty();

	return MakeSuccess(MeshPath, FString::Printf(
		TEXT("Generated lightmap UVs into channel %d (source %d). LightMapCoordinateIndex set to %d."),
		DestUVIndex, SourceUVIndex, DestUVIndex));
}

FUVMappingResult UUVMappingService::AutoUnwrapUVs(
	const FString& MeshPath,
	int32 LODIndex,
	int32 ChannelIndex,
	const FString& ProjectionType,
	float HardAngleThreshold)
{
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return MakeFailure(MeshPath, FString::Printf(TEXT("LOD %d has no mesh description"), LODIndex));

	FStaticMeshAttributes Attrs(*MeshDesc);
	if (ChannelIndex < 0 || ChannelIndex >= Attrs.GetVertexInstanceUVs().GetNumChannels())
	{
		return MakeFailure(MeshPath, FString::Printf(
			TEXT("Channel %d out of range (have %d). Add channel first."),
			ChannelIndex, Attrs.GetVertexInstanceUVs().GetNumChannels()));
	}

	// Use mesh bounds to size the projection gizmo.
	const FBox MeshBounds = StaticMesh->GetBoundingBox();
	const FVector Size = MeshBounds.GetSize();
	const FVector Center = MeshBounds.GetCenter();

	FUVMapParameters Params;
	Params.Position = Center;
	Params.Rotation = FQuat::Identity;
	Params.Size = Size;
	Params.Scale = FVector(1.0, 1.0, 1.0);
	Params.UVTile = FVector2D(1.0, 1.0);

	TMap<FVertexInstanceID, FVector2D> NewUVs;

	const FString Lower = ProjectionType.ToLower();
	if (Lower == TEXT("planar"))
	{
		FStaticMeshOperations::GeneratePlanarUV(*MeshDesc, Params, NewUVs);
	}
	else if (Lower == TEXT("cylindrical"))
	{
		FStaticMeshOperations::GenerateCylindricalUV(*MeshDesc, Params, NewUVs);
	}
	else if (Lower == TEXT("box"))
	{
		FStaticMeshOperations::GenerateBoxUV(*MeshDesc, Params, NewUVs);
	}
	else
	{
		return MakeFailure(MeshPath, FString::Printf(
			TEXT("Unknown ProjectionType '%s'. Use 'Planar', 'Box', or 'Cylindrical'."), *ProjectionType));
	}

	// HardAngleThreshold is accepted for forward-compatibility (Box mode); the engine
	// generators above use mesh normals to seam internally. We keep the param for parity
	// with future PatchBuilder integration.
	(void)HardAngleThreshold;

	StaticMesh->Modify();
	auto UVs = Attrs.GetVertexInstanceUVs();
	for (const TPair<FVertexInstanceID, FVector2D>& Pair : NewUVs)
	{
		UVs.Set(Pair.Key, ChannelIndex, FVector2f(Pair.Value));
	}

	CommitStaticMeshDescription(StaticMesh, LODIndex);
	return MakeSuccess(MeshPath, FString::Printf(
		TEXT("Auto-unwrapped %d vertex instances into LOD %d channel %d using %s projection"),
		NewUVs.Num(), LODIndex, ChannelIndex, *ProjectionType));
}

FUVMappingResult UUVMappingService::PackUVs(
	const FString& MeshPath,
	int32 LODIndex,
	int32 ChannelIndex,
	float PaddingPercent)
{
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return MakeFailure(MeshPath, FString::Printf(TEXT("LOD %d has no mesh description"), LODIndex));

	FStaticMeshAttributes Attrs(*MeshDesc);
	if (ChannelIndex < 0 || ChannelIndex >= Attrs.GetVertexInstanceUVs().GetNumChannels())
	{
		return MakeFailure(MeshPath, FString::Printf(TEXT("Channel %d out of range"), ChannelIndex));
	}

	auto UVs = Attrs.GetVertexInstanceUVs();

	// Compute current UV bounds.
	FBox2f Bounds(ForceInit);
	int32 Count = 0;
	for (FVertexInstanceID VI : MeshDesc->VertexInstances().GetElementIDs())
	{
		Bounds += UVs.Get(VI, ChannelIndex);
		++Count;
	}
	if (Count == 0) return MakeFailure(MeshPath, TEXT("No vertex instances on this LOD"));

	const FVector2f Extent = Bounds.GetSize();
	if (Extent.X < KINDA_SMALL_NUMBER || Extent.Y < KINDA_SMALL_NUMBER)
	{
		return MakeFailure(MeshPath, TEXT("UV bounds are degenerate; nothing to pack"));
	}

	// Fit-to-square with padding. This is a "tight repack" without true island detection;
	// for full island shelf-packing, run auto_unwrap_uvs with Box projection first.
	const float Pad = FMath::Clamp(PaddingPercent, 0.0f, 25.0f) / 100.0f;
	const float TargetExtent = 1.0f - 2.0f * Pad;
	const float Scale = TargetExtent / FMath::Max(Extent.X, Extent.Y);

	StaticMesh->Modify();
	for (FVertexInstanceID VI : MeshDesc->VertexInstances().GetElementIDs())
	{
		const FVector2f UV = UVs.Get(VI, ChannelIndex);
		FVector2f Packed = (UV - Bounds.Min) * Scale + FVector2f(Pad, Pad);
		UVs.Set(VI, ChannelIndex, Packed);
	}

	CommitStaticMeshDescription(StaticMesh, LODIndex);
	return MakeSuccess(MeshPath, FString::Printf(
		TEXT("Packed channel %d on LOD %d into [%.3f, %.3f]^2 (scale %.4f)"),
		ChannelIndex, LODIndex, Pad, 1.0f - Pad, Scale));
}

// =================================================================
// Transform
// =================================================================

FUVMappingResult UUVMappingService::TransformUVs(
	const FString& MeshPath,
	int32 LODIndex,
	int32 ChannelIndex,
	float ScaleU, float ScaleV,
	float RotationDegrees,
	float OffsetU, float OffsetV)
{
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return MakeFailure(MeshPath, FString::Printf(TEXT("LOD %d has no mesh description"), LODIndex));

	FStaticMeshAttributes Attrs(*MeshDesc);
	if (ChannelIndex < 0 || ChannelIndex >= Attrs.GetVertexInstanceUVs().GetNumChannels())
	{
		return MakeFailure(MeshPath, FString::Printf(TEXT("Channel %d out of range"), ChannelIndex));
	}

	const float CosR = FMath::Cos(FMath::DegreesToRadians(RotationDegrees));
	const float SinR = FMath::Sin(FMath::DegreesToRadians(RotationDegrees));

	StaticMesh->Modify();
	auto UVs = Attrs.GetVertexInstanceUVs();
	for (FVertexInstanceID VI : MeshDesc->VertexInstances().GetElementIDs())
	{
		FVector2f UV = UVs.Get(VI, ChannelIndex);
		// Scale
		UV.X *= ScaleU;
		UV.Y *= ScaleV;
		// Rotate about origin
		const float Rx = UV.X * CosR - UV.Y * SinR;
		const float Ry = UV.X * SinR + UV.Y * CosR;
		UV.X = Rx + OffsetU;
		UV.Y = Ry + OffsetV;
		UVs.Set(VI, ChannelIndex, UV);
	}

	CommitStaticMeshDescription(StaticMesh, LODIndex);
	return MakeSuccess(MeshPath, FString::Printf(
		TEXT("Transformed channel %d on LOD %d (scale %.3f,%.3f rot %.1f offset %.3f,%.3f)"),
		ChannelIndex, LODIndex, ScaleU, ScaleV, RotationDegrees, OffsetU, OffsetV));
}

FUVMappingResult UUVMappingService::FlipUVs(
	const FString& MeshPath,
	int32 LODIndex,
	int32 ChannelIndex,
	bool bFlipU, bool bFlipV)
{
	if (!bFlipU && !bFlipV)
	{
		return MakeSuccess(MeshPath, TEXT("No flip requested"));
	}

	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return MakeFailure(MeshPath, FString::Printf(TEXT("LOD %d has no mesh description"), LODIndex));

	FStaticMeshAttributes Attrs(*MeshDesc);
	if (ChannelIndex < 0 || ChannelIndex >= Attrs.GetVertexInstanceUVs().GetNumChannels())
	{
		return MakeFailure(MeshPath, FString::Printf(TEXT("Channel %d out of range"), ChannelIndex));
	}

	StaticMesh->Modify();
	auto UVs = Attrs.GetVertexInstanceUVs();
	for (FVertexInstanceID VI : MeshDesc->VertexInstances().GetElementIDs())
	{
		FVector2f UV = UVs.Get(VI, ChannelIndex);
		if (bFlipU) UV.X = 1.0f - UV.X;
		if (bFlipV) UV.Y = 1.0f - UV.Y;
		UVs.Set(VI, ChannelIndex, UV);
	}

	CommitStaticMeshDescription(StaticMesh, LODIndex);
	return MakeSuccess(MeshPath, FString::Printf(
		TEXT("Flipped channel %d on LOD %d (U=%s V=%s)"),
		ChannelIndex, LODIndex,
		bFlipU ? TEXT("true") : TEXT("false"),
		bFlipV ? TEXT("true") : TEXT("false")));
}

// =================================================================
// Per-region selection & transform
// =================================================================

namespace
{
	static FVector3f NormalizedAxis(float X, float Y, float Z)
	{
		FVector3f Axis(X, Y, Z);
		const float Sq = Axis.SizeSquared();
		if (Sq < KINDA_SMALL_NUMBER) return FVector3f(0.0f, 0.0f, 1.0f);
		return Axis * FMath::InvSqrt(Sq);
	}

	static void ApplyAffine(FVector2f& UV, float ScaleU, float ScaleV, float CosR, float SinR, float OffsetU, float OffsetV)
	{
		UV.X *= ScaleU;
		UV.Y *= ScaleV;
		const float Rx = UV.X * CosR - UV.Y * SinR;
		const float Ry = UV.X * SinR + UV.Y * CosR;
		UV.X = Rx + OffsetU;
		UV.Y = Ry + OffsetV;
	}
}

int32 UUVMappingService::CountVerticesByNormal(
	const FString& MeshPath,
	int32 LODIndex,
	float AxisX, float AxisY, float AxisZ,
	float MinDot, float MaxDot)
{
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return 0;

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return 0;

	FStaticMeshConstAttributes Attrs(*MeshDesc);
	const auto& Normals = Attrs.GetVertexInstanceNormals();
	const FVector3f Axis = NormalizedAxis(AxisX, AxisY, AxisZ);

	int32 Count = 0;
	for (FVertexInstanceID VI : MeshDesc->VertexInstances().GetElementIDs())
	{
		const FVector3f N = Normals[VI].GetSafeNormal();
		const float Dot = FVector3f::DotProduct(N, Axis);
		if (Dot >= MinDot && Dot <= MaxDot)
		{
			++Count;
		}
	}
	return Count;
}

FUVMappingResult UUVMappingService::TransformUVsByNormal(
	const FString& MeshPath,
	int32 LODIndex,
	int32 ChannelIndex,
	float AxisX, float AxisY, float AxisZ,
	float MinDot, float MaxDot,
	float ScaleU, float ScaleV,
	float RotationDegrees,
	float OffsetU, float OffsetV)
{
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return MakeFailure(MeshPath, FString::Printf(TEXT("LOD %d has no mesh description"), LODIndex));

	FStaticMeshAttributes Attrs(*MeshDesc);
	if (ChannelIndex < 0 || ChannelIndex >= Attrs.GetVertexInstanceUVs().GetNumChannels())
	{
		return MakeFailure(MeshPath, FString::Printf(TEXT("Channel %d out of range"), ChannelIndex));
	}

	const FVector3f Axis = NormalizedAxis(AxisX, AxisY, AxisZ);
	const float CosR = FMath::Cos(FMath::DegreesToRadians(RotationDegrees));
	const float SinR = FMath::Sin(FMath::DegreesToRadians(RotationDegrees));

	StaticMesh->Modify();
	auto UVs = Attrs.GetVertexInstanceUVs();
	const auto& Normals = Attrs.GetVertexInstanceNormals();

	int32 Touched = 0;
	for (FVertexInstanceID VI : MeshDesc->VertexInstances().GetElementIDs())
	{
		const FVector3f N = Normals[VI].GetSafeNormal();
		const float Dot = FVector3f::DotProduct(N, Axis);
		if (Dot >= MinDot && Dot <= MaxDot)
		{
			FVector2f UV = UVs.Get(VI, ChannelIndex);
			ApplyAffine(UV, ScaleU, ScaleV, CosR, SinR, OffsetU, OffsetV);
			UVs.Set(VI, ChannelIndex, UV);
			++Touched;
		}
	}

	if (Touched == 0)
	{
		// Don't dirty / rebuild for nothing.
		return MakeFailure(MeshPath, FString::Printf(
			TEXT("No vertex instances matched normal filter axis=(%.2f,%.2f,%.2f) dot in [%.2f, %.2f]"),
			Axis.X, Axis.Y, Axis.Z, MinDot, MaxDot));
	}

	CommitStaticMeshDescription(StaticMesh, LODIndex);
	return MakeSuccess(MeshPath, FString::Printf(
		TEXT("Transformed %d vertex instances on LOD %d ch %d (axis=(%.2f,%.2f,%.2f) dot in [%.2f, %.2f] scale %.2f,%.2f rot %.1f offset %.2f,%.2f)"),
		Touched, LODIndex, ChannelIndex, Axis.X, Axis.Y, Axis.Z, MinDot, MaxDot,
		ScaleU, ScaleV, RotationDegrees, OffsetU, OffsetV));
}

TArray<FString> UUVMappingService::ListPolygonGroups(const FString& MeshPath, int32 LODIndex)
{
	TArray<FString> Result;
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return Result;

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return Result;

	FStaticMeshConstAttributes Attrs(*MeshDesc);
	const auto& SlotNames = Attrs.GetPolygonGroupMaterialSlotNames();
	for (FPolygonGroupID PGID : MeshDesc->PolygonGroups().GetElementIDs())
	{
		const FName SlotName = SlotNames[PGID];
		Result.Add(SlotName == NAME_None
			? FString::Printf(TEXT("PolygonGroup_%d"), PGID.GetValue())
			: SlotName.ToString());
	}
	return Result;
}

FUVMappingResult UUVMappingService::TransformUVsByPolygonGroup(
	const FString& MeshPath,
	int32 LODIndex,
	int32 ChannelIndex,
	const FString& PolygonGroupName,
	float ScaleU, float ScaleV,
	float RotationDegrees,
	float OffsetU, float OffsetV)
{
	if (PolygonGroupName.IsEmpty())
	{
		return MakeFailure(MeshPath, TEXT("PolygonGroupName must not be empty"));
	}

	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return MakeFailure(MeshPath, FString::Printf(TEXT("LOD %d has no mesh description"), LODIndex));

	FStaticMeshAttributes Attrs(*MeshDesc);
	if (ChannelIndex < 0 || ChannelIndex >= Attrs.GetVertexInstanceUVs().GetNumChannels())
	{
		return MakeFailure(MeshPath, FString::Printf(TEXT("Channel %d out of range"), ChannelIndex));
	}

	// Resolve target polygon group by name (or by "PolygonGroup_<n>" fallback).
	const auto& SlotNames = Attrs.GetPolygonGroupMaterialSlotNames();
	FPolygonGroupID TargetPGID = FPolygonGroupID(INDEX_NONE);
	for (FPolygonGroupID PGID : MeshDesc->PolygonGroups().GetElementIDs())
	{
		if (SlotNames[PGID].ToString().Equals(PolygonGroupName, ESearchCase::IgnoreCase))
		{
			TargetPGID = PGID;
			break;
		}
	}
	if (TargetPGID == FPolygonGroupID(INDEX_NONE) && PolygonGroupName.StartsWith(TEXT("PolygonGroup_")))
	{
		const int32 Idx = FCString::Atoi(*PolygonGroupName.RightChop(13));
		if (MeshDesc->PolygonGroups().IsValid(FPolygonGroupID(Idx)))
		{
			TargetPGID = FPolygonGroupID(Idx);
		}
	}
	if (TargetPGID == FPolygonGroupID(INDEX_NONE))
	{
		return MakeFailure(MeshPath, FString::Printf(
			TEXT("Polygon group '%s' not found. Use ListPolygonGroups to discover."), *PolygonGroupName));
	}

	const float CosR = FMath::Cos(FMath::DegreesToRadians(RotationDegrees));
	const float SinR = FMath::Sin(FMath::DegreesToRadians(RotationDegrees));

	StaticMesh->Modify();
	auto UVs = Attrs.GetVertexInstanceUVs();

	// Collect unique vertex instances belonging to triangles in the target group, then transform once each.
	TSet<FVertexInstanceID> Selected;
	for (FTriangleID TID : MeshDesc->Triangles().GetElementIDs())
	{
		if (MeshDesc->GetTrianglePolygonGroup(TID) != TargetPGID) continue;
		for (FVertexInstanceID VI : MeshDesc->GetTriangleVertexInstances(TID))
		{
			Selected.Add(VI);
		}
	}

	if (Selected.Num() == 0)
	{
		return MakeFailure(MeshPath, FString::Printf(TEXT("Polygon group '%s' has no triangles"), *PolygonGroupName));
	}

	for (FVertexInstanceID VI : Selected)
	{
		FVector2f UV = UVs.Get(VI, ChannelIndex);
		ApplyAffine(UV, ScaleU, ScaleV, CosR, SinR, OffsetU, OffsetV);
		UVs.Set(VI, ChannelIndex, UV);
	}

	CommitStaticMeshDescription(StaticMesh, LODIndex);
	return MakeSuccess(MeshPath, FString::Printf(
		TEXT("Transformed %d vertex instances in polygon group '%s' on LOD %d ch %d"),
		Selected.Num(), *PolygonGroupName, LODIndex, ChannelIndex));
}

// =================================================================
// UV Island detection (union-find)
// =================================================================

namespace
{
	// Union-find over a contiguous index range.
	struct FDSU
	{
		TArray<int32> Parent;
		TArray<int32> Rank;
		void Init(int32 N)
		{
			Parent.SetNumUninitialized(N);
			Rank.SetNumZeroed(N);
			for (int32 i = 0; i < N; ++i) Parent[i] = i;
		}
		int32 Find(int32 X)
		{
			while (Parent[X] != X)
			{
				Parent[X] = Parent[Parent[X]];
				X = Parent[X];
			}
			return X;
		}
		void Union(int32 A, int32 B)
		{
			int32 RA = Find(A), RB = Find(B);
			if (RA == RB) return;
			if (Rank[RA] < Rank[RB]) Swap(RA, RB);
			Parent[RB] = RA;
			if (Rank[RA] == Rank[RB]) ++Rank[RA];
		}
	};

	// Build a deterministic island assignment.
	//
	// Algorithm:
	//   1. Map every FVertexInstanceID to a contiguous local index 0..N-1.
	//   2. For each triangle, union its 3 vertex instances (they're trivially in the same UV region).
	//   3. For vertex instances sharing the same FVertexID with matching UVs, union them
	//      (they're stitched across the implicit edge — no UV seam between).
	//   4. Walk vertex instances in element order; assign island ids in the order each
	//      new root is first encountered. This makes ids stable across runs of the same mesh.
	struct FIslandBuild
	{
		TArray<FVertexInstanceID> OrderedVIs;             // contiguous-index -> VI
		TMap<FVertexInstanceID, int32> VIToLocal;          // VI -> contiguous index
		FDSU Dsu;                                          // union-find on contiguous indices
		TMap<int32, int32> RootToIslandId;                 // root index -> stable island id
		TArray<int32> IslandIdByLocal;                     // local index -> island id
	};

	static void BuildIslands(
		const FMeshDescription& MeshDesc,
		int32 ChannelIndex,
		FIslandBuild& Out)
	{
		FStaticMeshConstAttributes Attrs(MeshDesc);
		const auto& UVs = Attrs.GetVertexInstanceUVs();

		const int32 N = MeshDesc.VertexInstances().Num();
		Out.OrderedVIs.Reset();
		Out.OrderedVIs.Reserve(N);
		Out.VIToLocal.Reset();
		Out.VIToLocal.Reserve(N);
		for (FVertexInstanceID VI : MeshDesc.VertexInstances().GetElementIDs())
		{
			Out.VIToLocal.Add(VI, Out.OrderedVIs.Num());
			Out.OrderedVIs.Add(VI);
		}
		Out.Dsu.Init(N);

		// Step A: union vertex instances that share a triangle.
		for (FTriangleID TID : MeshDesc.Triangles().GetElementIDs())
		{
			TArrayView<const FVertexInstanceID> VIs = MeshDesc.GetTriangleVertexInstances(TID);
			const int32 A = Out.VIToLocal[VIs[0]];
			const int32 B = Out.VIToLocal[VIs[1]];
			const int32 C = Out.VIToLocal[VIs[2]];
			Out.Dsu.Union(A, B);
			Out.Dsu.Union(B, C);
		}

		// Step B: union vertex instances on the same vertex with matching UVs (stitched seams).
		// Group by underlying FVertexID, then within a group merge any pair with near-equal UVs.
		TMap<FVertexID, TArray<FVertexInstanceID>> ByVertex;
		ByVertex.Reserve(MeshDesc.Vertices().Num());
		for (FVertexInstanceID VI : MeshDesc.VertexInstances().GetElementIDs())
		{
			ByVertex.FindOrAdd(MeshDesc.GetVertexInstanceVertex(VI)).Add(VI);
		}
		const float UVEpsilonSq = 1e-8f;
		for (auto& Pair : ByVertex)
		{
			const TArray<FVertexInstanceID>& Group = Pair.Value;
			for (int32 i = 0; i < Group.Num(); ++i)
			{
				const FVector2f UVi = UVs.Get(Group[i], ChannelIndex);
				const int32 Li = Out.VIToLocal[Group[i]];
				for (int32 j = i + 1; j < Group.Num(); ++j)
				{
					const FVector2f UVj = UVs.Get(Group[j], ChannelIndex);
					if ((UVi - UVj).SizeSquared() < UVEpsilonSq)
					{
						Out.Dsu.Union(Li, Out.VIToLocal[Group[j]]);
					}
				}
			}
		}

		// Step C: assign deterministic island ids in element-iteration order.
		Out.IslandIdByLocal.SetNumUninitialized(N);
		Out.RootToIslandId.Reset();
		for (int32 Local = 0; Local < N; ++Local)
		{
			const int32 Root = Out.Dsu.Find(Local);
			int32* ExistingId = Out.RootToIslandId.Find(Root);
			Out.IslandIdByLocal[Local] = ExistingId ? *ExistingId : Out.RootToIslandId.Add(Root, Out.RootToIslandId.Num());
		}
	}
}

TArray<FUVIslandInfo> UUVMappingService::IdentifyUVIslands(
	const FString& MeshPath,
	int32 LODIndex,
	int32 ChannelIndex)
{
	TArray<FUVIslandInfo> Out;
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return Out;

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return Out;

	FStaticMeshConstAttributes Attrs(*MeshDesc);
	if (ChannelIndex < 0 || ChannelIndex >= Attrs.GetVertexInstanceUVs().GetNumChannels())
	{
		return Out;
	}

	FIslandBuild Build;
	BuildIslands(*MeshDesc, ChannelIndex, Build);

	const auto& UVs = Attrs.GetVertexInstanceUVs();
	const auto& Positions = Attrs.GetVertexPositions();
	const auto& Normals = Attrs.GetVertexInstanceNormals();

	const int32 IslandCount = Build.RootToIslandId.Num();
	Out.SetNum(IslandCount);
	TArray<int32> WorldSampleCount;
	WorldSampleCount.SetNumZeroed(IslandCount);
	TArray<FBox2f> Bounds;
	Bounds.Init(FBox2f(ForceInit), IslandCount);
	TArray<FVector> WorldCenters;
	WorldCenters.SetNumZeroed(IslandCount);
	TArray<FVector> NormalSums;
	NormalSums.SetNumZeroed(IslandCount);

	// Per-VI accumulation
	for (int32 Local = 0; Local < Build.OrderedVIs.Num(); ++Local)
	{
		const int32 IslandId = Build.IslandIdByLocal[Local];
		FUVIslandInfo& Info = Out[IslandId];
		Info.IslandId = IslandId;
		++Info.VertexInstanceCount;
		const FVertexInstanceID VI = Build.OrderedVIs[Local];
		const FVector2f UV = UVs.Get(VI, ChannelIndex);
		Bounds[IslandId] += UV;

		const FVertexID V = MeshDesc->GetVertexInstanceVertex(VI);
		WorldCenters[IslandId] += FVector(Positions[V]);
		NormalSums[IslandId] += FVector(Normals[VI]);
		++WorldSampleCount[IslandId];
	}

	// Per-triangle accumulation (count + UV area)
	TArray<int32> TriCount;
	TriCount.SetNumZeroed(IslandCount);
	TArray<float> AreaSum;
	AreaSum.SetNumZeroed(IslandCount);
	for (FTriangleID TID : MeshDesc->Triangles().GetElementIDs())
	{
		TArrayView<const FVertexInstanceID> VIs = MeshDesc->GetTriangleVertexInstances(TID);
		const int32 IslandId = Build.IslandIdByLocal[Build.VIToLocal[VIs[0]]];
		++TriCount[IslandId];
		const FVector2f UV0 = UVs.Get(VIs[0], ChannelIndex);
		const FVector2f UV1 = UVs.Get(VIs[1], ChannelIndex);
		const FVector2f UV2 = UVs.Get(VIs[2], ChannelIndex);
		const FVector2f E1 = UV1 - UV0;
		const FVector2f E2 = UV2 - UV0;
		AreaSum[IslandId] += 0.5f * FMath::Abs(E1.X * E2.Y - E1.Y * E2.X);
	}

	for (int32 i = 0; i < IslandCount; ++i)
	{
		FUVIslandInfo& Info = Out[i];
		Info.TriangleCount = TriCount[i];
		if (Bounds[i].bIsValid)
		{
			Info.MinU = Bounds[i].Min.X;
			Info.MinV = Bounds[i].Min.Y;
			Info.MaxU = Bounds[i].Max.X;
			Info.MaxV = Bounds[i].Max.Y;
		}
		Info.UVArea = AreaSum[i];
		const int32 NSamples = WorldSampleCount[i];
		if (NSamples > 0)
		{
			Info.WorldCenter = WorldCenters[i] / static_cast<double>(NSamples);
			Info.AverageNormal = (NormalSums[i] / static_cast<double>(NSamples)).GetSafeNormal();
		}
	}

	return Out;
}

FUVMappingResult UUVMappingService::TransformUVIsland(
	const FString& MeshPath,
	int32 LODIndex,
	int32 ChannelIndex,
	int32 IslandId,
	float ScaleU, float ScaleV,
	float RotationDegrees,
	float OffsetU, float OffsetV)
{
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return MakeFailure(MeshPath, FString::Printf(TEXT("LOD %d has no mesh description"), LODIndex));

	FStaticMeshAttributes Attrs(*MeshDesc);
	if (ChannelIndex < 0 || ChannelIndex >= Attrs.GetVertexInstanceUVs().GetNumChannels())
	{
		return MakeFailure(MeshPath, FString::Printf(TEXT("Channel %d out of range"), ChannelIndex));
	}

	FIslandBuild Build;
	BuildIslands(*MeshDesc, ChannelIndex, Build);

	const int32 IslandCount = Build.RootToIslandId.Num();
	if (IslandId < 0 || IslandId >= IslandCount)
	{
		return MakeFailure(MeshPath, FString::Printf(
			TEXT("IslandId %d out of range; mesh has %d islands"), IslandId, IslandCount));
	}

	const float CosR = FMath::Cos(FMath::DegreesToRadians(RotationDegrees));
	const float SinR = FMath::Sin(FMath::DegreesToRadians(RotationDegrees));

	StaticMesh->Modify();
	auto UVs = Attrs.GetVertexInstanceUVs();

	int32 Touched = 0;
	for (int32 Local = 0; Local < Build.OrderedVIs.Num(); ++Local)
	{
		if (Build.IslandIdByLocal[Local] != IslandId) continue;
		const FVertexInstanceID VI = Build.OrderedVIs[Local];
		FVector2f UV = UVs.Get(VI, ChannelIndex);
		ApplyAffine(UV, ScaleU, ScaleV, CosR, SinR, OffsetU, OffsetV);
		UVs.Set(VI, ChannelIndex, UV);
		++Touched;
	}

	if (Touched == 0)
	{
		return MakeFailure(MeshPath, FString::Printf(TEXT("Island %d has no vertex instances"), IslandId));
	}

	CommitStaticMeshDescription(StaticMesh, LODIndex);
	return MakeSuccess(MeshPath, FString::Printf(
		TEXT("Transformed %d vertex instances in island %d on LOD %d ch %d (scale %.2f,%.2f rot %.1f offset %.2f,%.2f)"),
		Touched, IslandId, LODIndex, ChannelIndex, ScaleU, ScaleV, RotationDegrees, OffsetU, OffsetV));
}

// =================================================================
// Lightmap settings
// =================================================================

bool UUVMappingService::GetLightmapSettings(const FString& MeshPath, FUVLightmapSettings& OutSettings)
{
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return false;
	if (StaticMesh->GetNumSourceModels() == 0) return false;

	const FStaticMeshSourceModel& SM = StaticMesh->GetSourceModel(0);
	OutSettings.bGenerateLightmapUVs = SM.BuildSettings.bGenerateLightmapUVs;
	OutSettings.SourceLightmapIndex = SM.BuildSettings.SrcLightmapIndex;
	OutSettings.DestinationLightmapIndex = SM.BuildSettings.DstLightmapIndex;
	OutSettings.MinLightmapResolution = SM.BuildSettings.MinLightmapResolution;
	OutSettings.LightmapCoordinateIndex = StaticMesh->GetLightMapCoordinateIndex();
	OutSettings.LightMapResolution = StaticMesh->GetLightMapResolution();
	return true;
}

FUVMappingResult UUVMappingService::SetLightmapSettings(
	const FString& MeshPath,
	int32 LightmapCoordinateIndex,
	int32 SourceLightmapIndex,
	int32 LightMapResolution,
	bool bGenerateLightmapUVs)
{
	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));
	if (StaticMesh->GetNumSourceModels() == 0)
	{
		return MakeFailure(MeshPath, TEXT("StaticMesh has no source models"));
	}

	StaticMesh->Modify();
	FStaticMeshSourceModel& SM = StaticMesh->GetSourceModel(0);
	SM.BuildSettings.bGenerateLightmapUVs = bGenerateLightmapUVs;
	SM.BuildSettings.SrcLightmapIndex = SourceLightmapIndex;
	SM.BuildSettings.DstLightmapIndex = LightmapCoordinateIndex;
	SM.BuildSettings.MinLightmapResolution = LightMapResolution;

	StaticMesh->SetLightMapCoordinateIndex(LightmapCoordinateIndex);
	StaticMesh->SetLightMapResolution(LightMapResolution);

	StaticMesh->Build(false);
	StaticMesh->PostEditChange();
	StaticMesh->MarkPackageDirty();

	return MakeSuccess(MeshPath, FString::Printf(
		TEXT("Lightmap configured: coord=%d source=%d res=%d generate=%s"),
		LightmapCoordinateIndex, SourceLightmapIndex, LightMapResolution,
		bGenerateLightmapUVs ? TEXT("true") : TEXT("false")));
}

// =================================================================
// Visualize
// =================================================================

namespace
{
	static void DrawPixel(TArray<FColor>& Pixels, int32 W, int32 H, int32 X, int32 Y, FColor Color)
	{
		if (X < 0 || X >= W || Y < 0 || Y >= H) return;
		Pixels[Y * W + X] = Color;
	}

	static void DrawLine(TArray<FColor>& Pixels, int32 W, int32 H, int32 X0, int32 Y0, int32 X1, int32 Y1, FColor Color)
	{
		// Bresenham
		int32 DX = FMath::Abs(X1 - X0);
		int32 DY = -FMath::Abs(Y1 - Y0);
		int32 SX = X0 < X1 ? 1 : -1;
		int32 SY = Y0 < Y1 ? 1 : -1;
		int32 Err = DX + DY;
		while (true)
		{
			DrawPixel(Pixels, W, H, X0, Y0, Color);
			if (X0 == X1 && Y0 == Y1) break;
			const int32 E2 = 2 * Err;
			if (E2 >= DY) { Err += DY; X0 += SX; }
			if (E2 <= DX) { Err += DX; Y0 += SY; }
		}
	}

	static FIntPoint UVToPixelFit(const FVector2f& UV, int32 ImageSize, const FBox2f& Bounds, float Padding)
	{
		// Auto-fit: map [Bounds.Min, Bounds.Max] into [Padding*ImageSize, (1-Padding)*ImageSize].
		// V is flipped so that V=Bounds.Min.Y is at the bottom of the image.
		const FVector2f Extent = Bounds.GetSize();
		const float Inv = (Extent.X > KINDA_SMALL_NUMBER && Extent.Y > KINDA_SMALL_NUMBER) ? 1.0f : 0.0f;
		const float SafeX = (Extent.X > KINDA_SMALL_NUMBER) ? Extent.X : 1.0f;
		const float SafeY = (Extent.Y > KINDA_SMALL_NUMBER) ? Extent.Y : 1.0f;
		const float NormX = (UV.X - Bounds.Min.X) / SafeX;
		const float NormY = (UV.Y - Bounds.Min.Y) / SafeY;
		const float UseX = Padding + NormX * (1.0f - 2.0f * Padding);
		const float UseY = Padding + (1.0f - NormY) * (1.0f - 2.0f * Padding);
		const int32 X = FMath::Clamp(FMath::RoundToInt(UseX * (ImageSize - 1)), 0, ImageSize - 1);
		const int32 Y = FMath::Clamp(FMath::RoundToInt(UseY * (ImageSize - 1)), 0, ImageSize - 1);
		return FIntPoint(X, Y);
	}
}

FUVMappingResult UUVMappingService::ExportUVLayoutImage(
	const FString& MeshPath,
	int32 LODIndex,
	int32 ChannelIndex,
	const FString& OutputPath,
	int32 ImageSize)
{
	if (ImageSize < 256) ImageSize = 256;
	if (ImageSize > 4096) ImageSize = 4096;

	UStaticMesh* StaticMesh = LoadStaticMeshAsset(MeshPath);
	if (!StaticMesh) return MakeFailure(MeshPath, TEXT("StaticMesh not found"));

	FMeshDescription* MeshDesc = GetStaticMeshDescription(StaticMesh, LODIndex);
	if (!MeshDesc) return MakeFailure(MeshPath, FString::Printf(TEXT("LOD %d has no mesh description"), LODIndex));

	FStaticMeshConstAttributes Attrs(*MeshDesc);
	if (ChannelIndex < 0 || ChannelIndex >= Attrs.GetVertexInstanceUVs().GetNumChannels())
	{
		return MakeFailure(MeshPath, FString::Printf(TEXT("Channel %d out of range"), ChannelIndex));
	}

	// Compute auto-fit bounds: include all UVs, plus expand to at least cover [0,1] so the
	// unit-square reference grid stays visible even when UVs sit inside it.
	const auto& UVs = Attrs.GetVertexInstanceUVs();
	FBox2f Bounds(ForceInit);
	for (FVertexInstanceID VI : MeshDesc->VertexInstances().GetElementIDs())
	{
		Bounds += UVs.Get(VI, ChannelIndex);
	}
	if (!Bounds.bIsValid)
	{
		Bounds = FBox2f(FVector2f::ZeroVector, FVector2f(1.0f, 1.0f));
	}
	Bounds += FVector2f(0.0f, 0.0f);
	Bounds += FVector2f(1.0f, 1.0f);

	const float Padding = 0.04f; // 4% margin

	// Allocate background buffer.
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(ImageSize * ImageSize);
	const FColor Bg(20, 20, 20, 255);
	for (FColor& P : Pixels) P = Bg;

	// Reference grid every 0.1 UV unit, only for grid lines that fall inside the bounds.
	const FColor GridColor(60, 60, 60, 255);
	const FColor UnitColor(120, 120, 120, 255); // emphasize the [0,1] frame
	const FVector2f Extent = Bounds.GetSize();
	const float Step = 0.1f;
	auto ToPixel = [&](float U, float V) { return UVToPixelFit(FVector2f(U, V), ImageSize, Bounds, Padding); };

	const int32 GridStartU = FMath::FloorToInt(Bounds.Min.X / Step);
	const int32 GridEndU   = FMath::CeilToInt (Bounds.Max.X / Step);
	const int32 GridStartV = FMath::FloorToInt(Bounds.Min.Y / Step);
	const int32 GridEndV   = FMath::CeilToInt (Bounds.Max.Y / Step);
	for (int32 g = GridStartU; g <= GridEndU; ++g)
	{
		const float U = g * Step;
		const FIntPoint A = ToPixel(U, Bounds.Min.Y);
		const FIntPoint B = ToPixel(U, Bounds.Max.Y);
		const bool bUnit = (g == 0 || g == 10);
		DrawLine(Pixels, ImageSize, ImageSize, A.X, A.Y, B.X, B.Y, bUnit ? UnitColor : GridColor);
	}
	for (int32 g = GridStartV; g <= GridEndV; ++g)
	{
		const float V = g * Step;
		const FIntPoint A = ToPixel(Bounds.Min.X, V);
		const FIntPoint B = ToPixel(Bounds.Max.X, V);
		const bool bUnit = (g == 0 || g == 10);
		DrawLine(Pixels, ImageSize, ImageSize, A.X, A.Y, B.X, B.Y, bUnit ? UnitColor : GridColor);
	}

	// Triangle outlines.
	const FColor TriColor(160, 220, 255, 255);
	for (FTriangleID TriID : MeshDesc->Triangles().GetElementIDs())
	{
		TArrayView<const FVertexInstanceID> VIs = MeshDesc->GetTriangleVertexInstances(TriID);
		const FIntPoint P0 = UVToPixelFit(UVs.Get(VIs[0], ChannelIndex), ImageSize, Bounds, Padding);
		const FIntPoint P1 = UVToPixelFit(UVs.Get(VIs[1], ChannelIndex), ImageSize, Bounds, Padding);
		const FIntPoint P2 = UVToPixelFit(UVs.Get(VIs[2], ChannelIndex), ImageSize, Bounds, Padding);
		DrawLine(Pixels, ImageSize, ImageSize, P0.X, P0.Y, P1.X, P1.Y, TriColor);
		DrawLine(Pixels, ImageSize, ImageSize, P1.X, P1.Y, P2.X, P2.Y, TriColor);
		DrawLine(Pixels, ImageSize, ImageSize, P2.X, P2.Y, P0.X, P0.Y, TriColor);
	}

	// Encode and save.
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!Wrapper.IsValid())
	{
		return MakeFailure(MeshPath, TEXT("Failed to create PNG ImageWrapper"));
	}

	if (!Wrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), ImageSize, ImageSize, ERGBFormat::BGRA, 8))
	{
		return MakeFailure(MeshPath, TEXT("ImageWrapper::SetRaw failed"));
	}

	const TArray64<uint8>& Compressed = Wrapper->GetCompressed(100);

	FString FinalPath = OutputPath;
	if (!FinalPath.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase))
	{
		FinalPath = FPaths::ChangeExtension(FinalPath, TEXT(".png"));
	}

	if (!FFileHelper::SaveArrayToFile(Compressed, *FinalPath))
	{
		return MakeFailure(MeshPath, FString::Printf(TEXT("Failed to write PNG to %s"), *FinalPath));
	}

	return MakeSuccess(MeshPath, FinalPath);
}

// =================================================================
// Existence
// =================================================================

bool UUVMappingService::MeshHasUVChannel(const FString& MeshPath, int32 LODIndex, int32 ChannelIndex)
{
	if (UStaticMesh* SM = LoadStaticMeshAsset(MeshPath))
	{
		return ChannelIndex >= 0 && ChannelIndex < GetStaticMeshNumUVChannels(SM, LODIndex);
	}
	if (USkeletalMesh* SkM = LoadSkeletalMeshAsset(MeshPath))
	{
		return ChannelIndex >= 0 && ChannelIndex < GetSkeletalMeshNumUVChannels(SkM, LODIndex);
	}
	return false;
}
