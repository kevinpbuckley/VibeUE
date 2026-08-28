// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UModelingService.h"

#include "UDynamicMesh.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshSelectionFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshRemeshFunctions.h"
#include "GeometryScript/MeshSimplifyFunctions.h"
#include "GeometryScript/MeshSubdivideFunctions.h"
#include "GeometryScript/MeshDeformFunctions.h"
#include "GeometryScript/MeshVoxelFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"
#include "GeometryScript/MeshDecompositionFunctions.h"
#include "GeometryScript/MeshPolygroupFunctions.h"
#include "GeometryScript/MeshMaterialFunctions.h"
#include "GeometryScript/MeshVertexColorFunctions.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBakeFunctions.h"
#include "GeometryScript/MeshBoneWeightFunctions.h"
#include "GeometryScript/MeshPoolFunctions.h"
#include "GeometryScript/CollisionFunctions.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/OpenSubdivUtilityFunctions.h"
#include "GeometryScript/MeshSpatialFunctions.h"
#include "GeometryScript/MeshSamplingFunctions.h"
#include "GeometryScript/ContainmentFunctions.h"
#include "GeometryScript/MeshComparisonFunctions.h"
#include "GeometryScript/MeshSelectionQueryFunctions.h"
#include "GeometryScript/SceneUtilityFunctions.h"
#include "Components/MeshComponent.h"

#include "Animation/Skeleton.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Misc/CoreDelegates.h"
#include "StaticMeshEditorSubsystem.h"
#include "StaticMeshEditorSubsystemHelpers.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Math/RandomStream.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Selections/MeshConnectedComponents.h"
#include "DynamicMesh/DynamicBoneAttribute.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Materials/Material.h"
#include "SkeletalMeshAttributes.h"

DEFINE_LOG_CATEGORY_STATIC(LogVibeModeling, Log, All);

using namespace UE::Geometry;

// =========================================================================
// Session: integer handles -> UDynamicMesh (kept alive for the editor session)
// =========================================================================

namespace
{
	/** A named selection plus the mesh counts it was made against, so a stale one can be refused. */
	struct FStoredSelection
	{
		FGeometryScriptMeshSelection Selection;
		int32 TriangleCount = 0;
		int32 VertexCount = 0;
	};

	struct FModelingSession
	{
		TMap<int32, TStrongObjectPtr<UDynamicMesh>> Meshes;
		TMap<int32, TMap<FString, FStoredSelection>> Selections;
		int32 NextHandle = 1;
		bool bExitHooked = false;

		void Reset()
		{
			Selections.Reset();
			Meshes.Reset();
		}
	};

	FModelingSession& Session()
	{
		static FModelingSession S;
		if (!S.bExitHooked)
		{
			// Release the strong references before the UObject system tears down.
			S.bExitHooked = true;
			FCoreDelegates::OnPreExit.AddLambda([]() { Session().Reset(); });
		}
		return S;
	}

	UDynamicMesh* FindMesh(int32 Handle)
	{
		const TStrongObjectPtr<UDynamicMesh>* Found = Session().Meshes.Find(Handle);
		return Found ? Found->Get() : nullptr;
	}

	int32 AddMesh(UDynamicMesh* Mesh)
	{
		const int32 Handle = Session().NextHandle++;
		Session().Meshes.Add(Handle, TStrongObjectPtr<UDynamicMesh>(Mesh));
		return Handle;
	}

	UDynamicMesh* NewSessionMesh(int32& OutHandle)
	{
		UDynamicMesh* Mesh = NewObject<UDynamicMesh>(GetTransientPackage(), NAME_None, RF_Transient);
		OutHandle = AddMesh(Mesh);
		return Mesh;
	}

	UDynamicMesh* NewScratchMesh()
	{
		return NewObject<UDynamicMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	}

	int32 TriCount(UDynamicMesh* Mesh)
	{
		return Mesh ? Mesh->GetTriangleCount() : 0;
	}

	int32 VertCount(UDynamicMesh* Mesh)
	{
		int32 Count = 0;
		if (Mesh)
		{
			Mesh->ProcessMesh([&Count](const FDynamicMesh3& M) { Count = M.VertexCount(); });
		}
		return Count;
	}

	// ---- results -----------------------------------------------------------

	FModelingResult Fail(int32 Handle, const FString& Message)
	{
		FModelingResult R;
		R.bSuccess = false;
		R.Handle = Handle;
		R.Message = Message;
		UE_LOG(LogVibeModeling, Warning, TEXT("ModelingService failure (handle %d): %s"), Handle, *Message);
		return R;
	}

	FModelingResult Ok(int32 Handle, UDynamicMesh* Mesh, const FString& Message)
	{
		FModelingResult R;
		R.bSuccess = true;
		R.Handle = Handle;
		R.Message = Message;
		R.TriangleCount = TriCount(Mesh);
		R.VertexCount = VertCount(Mesh);
		return R;
	}

	FModelingResult NoMesh(int32 Handle)
	{
		return Fail(Handle, FString::Printf(TEXT("No session mesh with handle %d (create_mesh / load_mesh_* first, or see list_meshes)"), Handle));
	}

	// ---- GeometryScript debug capture --------------------------------------

	UGeometryScriptDebug* NewDebug()
	{
		return NewObject<UGeometryScriptDebug>(GetTransientPackage(), NAME_None, RF_Transient);
	}

	bool DebugHasErrors(const UGeometryScriptDebug* Debug)
	{
		if (!Debug) { return false; }
		for (const FGeometryScriptDebugMessage& M : Debug->Messages)
		{
			if (M.MessageType == EGeometryScriptDebugMessageType::ErrorMessage || M.Message.ToString().Contains(TEXT("cannot be run"))) { return true; }
		}
		return false;
	}

	FString DebugText(const UGeometryScriptDebug* Debug)
	{
		TArray<FString> Lines;
		if (Debug)
		{
			for (const FGeometryScriptDebugMessage& M : Debug->Messages)
			{
				Lines.Add(M.Message.ToString());
			}
		}
		return FString::Join(Lines, TEXT("; "));
	}

	/** Standard end of a mutating call: errors reported by GeometryScript become a failure. */
	FModelingResult Finish(int32 Handle, UDynamicMesh* Mesh, UGeometryScriptDebug* Debug, const FString& What)
	{
		if (DebugHasErrors(Debug))
		{
			return Fail(Handle, What + TEXT(": ") + DebugText(Debug));
		}
		const FString Extra = DebugText(Debug);
		return Ok(Handle, Mesh, Extra.IsEmpty() ? What : What + TEXT(" (") + Extra + TEXT(")"));
	}

	// ---- enums by name -------------------------------------------------------

	template <typename TEnum>
	FString EnumNames()
	{
		TArray<FString> Names;
		const UEnum* E = StaticEnum<TEnum>();
		for (int32 i = 0; i < E->NumEnums() - 1; ++i)
		{
			Names.Add(E->GetNameStringByIndex(i));
		}
		return FString::Join(Names, TEXT(", "));
	}

	template <typename TEnum>
	bool ParseEnum(const FString& Name, TEnum& Out)
	{
		const UEnum* E = StaticEnum<TEnum>();
		const int64 Value = E->GetValueByNameString(Name.TrimStartAndEnd());
		if (Value == INDEX_NONE)
		{
			return false;
		}
		Out = static_cast<TEnum>(Value);
		return true;
	}

	// ---- selections ----------------------------------------------------------

	void StoreSelection(int32 Handle, const FString& Name, const FGeometryScriptMeshSelection& Selection)
	{
		FStoredSelection Stored;
		Stored.Selection = Selection;
		Stored.TriangleCount = TriCount(FindMesh(Handle));
		Stored.VertexCount = VertCount(FindMesh(Handle));
		Session().Selections.FindOrAdd(Handle).Add(Name, Stored);
	}

	/** Every element id the selection references must still exist — GeometryScript asserts on stale ids instead of failing. */
	bool SelectionIsValidForMesh(UDynamicMesh* Mesh, const FGeometryScriptMeshSelection& Selection)
	{
		FGeometryScriptIndexList IndexList;
		EGeometryScriptIndexType ResultType = EGeometryScriptIndexType::Any;
		UGeometryScriptLibrary_MeshSelectionFunctions::ConvertMeshSelectionToIndexList(Mesh, Selection, IndexList, ResultType, EGeometryScriptIndexType::Any);
		if (!IndexList.List.IsValid() || ResultType == EGeometryScriptIndexType::PolygroupID || ResultType == EGeometryScriptIndexType::Any)
		{
			return true;
		}
		bool bValid = true;
		Mesh->ProcessMesh([&](const FDynamicMesh3& M)
		{
			for (const int32 Id : *IndexList.List)
			{
				const bool bExists =
					ResultType == EGeometryScriptIndexType::Triangle ? M.IsTriangle(Id) :
					ResultType == EGeometryScriptIndexType::Vertex ? M.IsVertex(Id) :
					ResultType == EGeometryScriptIndexType::Edge ? M.IsEdge(Id) : true;
				if (!bExists) { bValid = false; return; }
			}
		});
		return bValid;
	}

	/** "" resolves to the whole mesh as a triangle selection; a named selection must predate no topology change. */
	bool ResolveSelection(int32 Handle, const FString& Name, UDynamicMesh* Mesh, FGeometryScriptMeshSelection& Out, FString& Error)
	{
		if (Name.IsEmpty())
		{
			UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(Mesh, Out, EGeometryScriptMeshSelectionType::Triangles);
			return true;
		}
		const TMap<FString, FStoredSelection>* PerMesh = Session().Selections.Find(Handle);
		const FStoredSelection* Found = PerMesh ? PerMesh->Find(Name) : nullptr;
		if (!Found)
		{
			Error = FString::Printf(TEXT("No selection named '%s' on handle %d (make one with select_all / select_by_normal_angle / select_in_box / ...)"), *Name, Handle);
			return false;
		}
		const int32 Tris = TriCount(Mesh);
		const int32 Verts = VertCount(Mesh);
		if (Tris != Found->TriangleCount || Verts != Found->VertexCount || !SelectionIsValidForMesh(Mesh, Found->Selection))
		{
			Error = FString::Printf(TEXT("Selection '%s' is stale: handle %d changed since it was made (%d -> %d triangles). Re-run the select_* call after any op that changes topology."),
				*Name, Handle, Found->TriangleCount, Tris);
			return false;
		}
		Out = Found->Selection;
		return true;
	}

	FGeometryScriptPrimitiveOptions PrimitiveOptions(int32 MaterialID)
	{
		FGeometryScriptPrimitiveOptions Options;
		Options.MaterialID = MaterialID;
		return Options;
	}

	bool ParseOrigin(const FString& Origin, EGeometryScriptPrimitiveOriginMode& Out, FString& Error)
	{
		if (ParseEnum(Origin, Out)) { return true; }
		Error = FString::Printf(TEXT("Unknown origin '%s' (use %s)"), *Origin, *EnumNames<EGeometryScriptPrimitiveOriginMode>());
		return false;
	}

	// ---- assets ----------------------------------------------------------------

	template <typename T>
	T* LoadAssetAs(const FString& AssetPath)
	{
		// DoesAssetExist first: LoadAsset logs an error for a missing path, which agents (and automation) read as a failure of their own.
		if (AssetPath.IsEmpty() || !UEditorAssetLibrary::DoesAssetExist(AssetPath))
		{
			return nullptr;
		}
		return Cast<T>(UEditorAssetLibrary::LoadAsset(AssetPath));
	}

	bool CopyFromStaticMesh(UStaticMesh* StaticMesh, UDynamicMesh* Target, int32 LODIndex, UGeometryScriptDebug* Debug)
	{
		FGeometryScriptCopyMeshFromAssetOptions AssetOptions;
		FGeometryScriptMeshReadLOD ReadLOD;
		ReadLOD.LODIndex = LODIndex;
		EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(StaticMesh, Target, AssetOptions, ReadLOD, Outcome, Debug);
		return Outcome == EGeometryScriptOutcomePins::Success;
	}

	bool CopyFromSkeletalMesh(USkeletalMesh* SkeletalMesh, UDynamicMesh* Target, int32 LODIndex, UGeometryScriptDebug* Debug)
	{
		FGeometryScriptCopyMeshFromAssetOptions AssetOptions;
		FGeometryScriptMeshReadLOD ReadLOD;
		ReadLOD.LODIndex = LODIndex;
		EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(SkeletalMesh, Target, AssetOptions, ReadLOD, Outcome, Debug);
		return Outcome == EGeometryScriptOutcomePins::Success;
	}

	void SaveIf(bool bSave, const FString& AssetPath)
	{
		if (bSave)
		{
			UEditorAssetLibrary::SaveAsset(AssetPath, false);
		}
	}

	AActor* FindActorByLabel(const FString& Label)
	{
		UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
		if (!ActorSubsystem) { return nullptr; }
		for (AActor* Actor : ActorSubsystem->GetAllLevelActors())
		{
			if (Actor && Actor->GetActorLabel().Equals(Label, ESearchCase::IgnoreCase))
			{
				return Actor;
			}
		}
		return nullptr;
	}
}

// =========================================================================
// Session
// =========================================================================

FModelingResult UModelingService::CreateMesh()
{
	int32 Handle = -1;
	UDynamicMesh* Mesh = NewSessionMesh(Handle);
	return Ok(Handle, Mesh, TEXT("Created empty mesh"));
}

FModelingResult UModelingService::LoadMeshFromStaticMesh(const FString& AssetPath, int32 LODIndex)
{
	UStaticMesh* StaticMesh = LoadAssetAs<UStaticMesh>(AssetPath);
	if (!StaticMesh)
	{
		return Fail(-1, FString::Printf(TEXT("StaticMesh not found: %s"), *AssetPath));
	}
	int32 Handle = -1;
	UDynamicMesh* Mesh = NewSessionMesh(Handle);
	UGeometryScriptDebug* Debug = NewDebug();
	if (!CopyFromStaticMesh(StaticMesh, Mesh, LODIndex, Debug))
	{
		ReleaseMesh(Handle);
		return Fail(-1, FString::Printf(TEXT("Could not read LOD %d of %s: %s"), LODIndex, *AssetPath, *DebugText(Debug)));
	}
	FModelingResult R = Ok(Handle, Mesh, FString::Printf(TEXT("Loaded %s LOD %d"), *AssetPath, LODIndex));
	R.AssetPath = AssetPath;
	return R;
}

FModelingResult UModelingService::LoadMeshFromSkeletalMesh(const FString& AssetPath, int32 LODIndex)
{
	USkeletalMesh* SkeletalMesh = LoadAssetAs<USkeletalMesh>(AssetPath);
	if (!SkeletalMesh)
	{
		return Fail(-1, FString::Printf(TEXT("SkeletalMesh not found: %s"), *AssetPath));
	}
	int32 Handle = -1;
	UDynamicMesh* Mesh = NewSessionMesh(Handle);
	UGeometryScriptDebug* Debug = NewDebug();
	if (!CopyFromSkeletalMesh(SkeletalMesh, Mesh, LODIndex, Debug))
	{
		ReleaseMesh(Handle);
		return Fail(-1, FString::Printf(TEXT("Could not read LOD %d of %s: %s"), LODIndex, *AssetPath, *DebugText(Debug)));
	}
	FModelingResult R = Ok(Handle, Mesh, FString::Printf(TEXT("Loaded %s LOD %d (with bone weights)"), *AssetPath, LODIndex));
	R.AssetPath = AssetPath;
	return R;
}

FModelingResult UModelingService::LoadMeshFromActor(const FString& ActorLabel, bool bWorldSpace, int32 LODIndex)
{
	AActor* Actor = FindActorByLabel(ActorLabel);
	if (!Actor)
	{
		return Fail(-1, FString::Printf(TEXT("No level actor labeled '%s'"), *ActorLabel));
	}
	UStaticMeshComponent* StaticComponent = Actor->FindComponentByClass<UStaticMeshComponent>();
	UStaticMesh* StaticMesh = StaticComponent ? StaticComponent->GetStaticMesh() : nullptr;
	int32 Handle = -1;
	UDynamicMesh* Mesh = NewSessionMesh(Handle);
	UGeometryScriptDebug* Debug = NewDebug();
	if (StaticMesh)
	{
		if (!CopyFromStaticMesh(StaticMesh, Mesh, LODIndex, Debug))
		{
			ReleaseMesh(Handle);
			return Fail(-1, FString::Printf(TEXT("Could not read %s: %s"), *StaticMesh->GetPathName(), *DebugText(Debug)));
		}
		if (bWorldSpace)
		{
			UGeometryScriptLibrary_MeshTransformFunctions::TransformMesh(Mesh, StaticComponent->GetComponentTransform(), true, Debug);
		}
		FModelingResult R = Ok(Handle, Mesh, FString::Printf(TEXT("Loaded %s from actor '%s'%s"), *StaticMesh->GetPathName(), *ActorLabel,
			bWorldSpace ? TEXT(" in world space") : TEXT(" in asset space")));
		R.AssetPath = StaticMesh->GetPathName();
		return R;
	}

	// Anything else that renders a mesh (skeletal mesh, dynamic mesh, instanced components): copy its render geometry.
	UMeshComponent* MeshComponent = Actor->FindComponentByClass<UMeshComponent>();
	if (!MeshComponent)
	{
		ReleaseMesh(Handle);
		return Fail(-1, FString::Printf(TEXT("Actor '%s' has no mesh component"), *ActorLabel));
	}
	FGeometryScriptCopyMeshFromComponentOptions Options;
	Options.RequestedLOD.LODIndex = LODIndex;
	FTransform LocalToWorld;
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	UGeometryScriptLibrary_SceneUtilityFunctions::CopyMeshFromComponent(MeshComponent, Mesh, Options, bWorldSpace, LocalToWorld, Outcome, Debug);
	if (Outcome != EGeometryScriptOutcomePins::Success)
	{
		ReleaseMesh(Handle);
		return Fail(-1, FString::Printf(TEXT("Could not copy geometry from '%s' (%s): %s"), *ActorLabel, *MeshComponent->GetClass()->GetName(), *DebugText(Debug)));
	}
	return Ok(Handle, Mesh, FString::Printf(TEXT("Loaded %s geometry from actor '%s'%s"), *MeshComponent->GetClass()->GetName(), *ActorLabel,
		bWorldSpace ? TEXT(" in world space") : TEXT(" in component space")));
}

FModelingResult UModelingService::CopyMesh(int32 Handle)
{
	UDynamicMesh* Source = FindMesh(Handle);
	if (!Source) { return NoMesh(Handle); }
	int32 NewHandle = -1;
	UDynamicMesh* Target = NewSessionMesh(NewHandle);
	UDynamicMesh* Out = nullptr;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(Source, Target, Out, Debug);
	return Finish(NewHandle, Target, Debug, FString::Printf(TEXT("Copied handle %d"), Handle));
}

bool UModelingService::ReleaseMesh(int32 Handle)
{
	Session().Selections.Remove(Handle);
	return Session().Meshes.Remove(Handle) > 0;
}

int32 UModelingService::ReleaseAllMeshes()
{
	const int32 Count = Session().Meshes.Num();
	Session().Reset();
	return Count;
}

TArray<int32> UModelingService::ListMeshes()
{
	TArray<int32> Handles;
	Session().Meshes.GetKeys(Handles);
	Handles.Sort();
	return Handles;
}

FModelingMeshInfo UModelingService::GetMeshInfo(int32 Handle)
{
	FModelingMeshInfo Info;
	Info.Handle = Handle;
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh)
	{
		Info.Message = NoMesh(Handle).Message;
		return Info;
	}
	Info.bSuccess = true;
	Info.TriangleCount = TriCount(Mesh);
	Info.bIsClosed = UGeometryScriptLibrary_MeshQueryFunctions::GetIsClosedMesh(Mesh);
	Info.OpenBorderEdges = UGeometryScriptLibrary_MeshQueryFunctions::GetNumOpenBorderEdges(Mesh);
	Info.ConnectedComponents = UGeometryScriptLibrary_MeshQueryFunctions::GetNumConnectedComponents(Mesh);
	const FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
	Info.BoundsMin = Bounds.Min;
	Info.BoundsMax = Bounds.Max;
	UGeometryScriptLibrary_MeshQueryFunctions::GetMeshVolumeArea(Mesh, Info.SurfaceArea, Info.Volume);
	Mesh->ProcessMesh([&Info](const FDynamicMesh3& M)
	{
		Info.VertexCount = M.VertexCount();
		if (M.HasAttributes())
		{
			Info.NumUVLayers = M.Attributes()->NumUVLayers();
			Info.bHasVertexColors = M.Attributes()->HasPrimaryColors();
			if (M.Attributes()->HasMaterialID())
			{
				TSet<int32> Ids;
				const FDynamicMeshMaterialAttribute* Materials = M.Attributes()->GetMaterialID();
				for (const int32 Tid : M.TriangleIndicesItr())
				{
					Ids.Add(Materials->GetValue(Tid));
				}
				Info.MaterialIDs = Ids.Array();
				Info.MaterialIDs.Sort();
			}
		}
	});
	if (const TMap<FString, FStoredSelection>* PerMesh = Session().Selections.Find(Handle))
	{
		PerMesh->GetKeys(Info.Selections);
	}
	Info.Message = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshInfoString(Mesh);
	return Info;
}

UDynamicMesh* UModelingService::GetDynamicMesh(int32 Handle)
{
	return FindMesh(Handle);
}

// =========================================================================
// Primitives
// =========================================================================

FModelingResult UModelingService::AppendBox(int32 Handle, FTransform Transform, float DimensionX, float DimensionY, float DimensionZ,
	int32 StepsX, int32 StepsY, int32 StepsZ, const FString& Origin, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	EGeometryScriptPrimitiveOriginMode OriginMode; FString Error;
	if (!ParseOrigin(Origin, OriginMode, Error)) { return Fail(Handle, Error); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(Mesh, PrimitiveOptions(MaterialID), Transform, DimensionX, DimensionY, DimensionZ,
		StepsX, StepsY, StepsZ, OriginMode, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended box"));
}

FModelingResult UModelingService::AppendSphere(int32 Handle, FTransform Transform, float Radius, int32 StepsPhi, int32 StepsTheta,
	const FString& Origin, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	EGeometryScriptPrimitiveOriginMode OriginMode; FString Error;
	if (!ParseOrigin(Origin, OriginMode, Error)) { return Fail(Handle, Error); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereLatLong(Mesh, PrimitiveOptions(MaterialID), Transform, Radius, StepsPhi, StepsTheta, OriginMode, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended sphere"));
}

FModelingResult UModelingService::AppendCylinder(int32 Handle, FTransform Transform, float Radius, float Height, int32 RadialSteps,
	int32 HeightSteps, bool bCapped, const FString& Origin, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	EGeometryScriptPrimitiveOriginMode OriginMode; FString Error;
	if (!ParseOrigin(Origin, OriginMode, Error)) { return Fail(Handle, Error); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(Mesh, PrimitiveOptions(MaterialID), Transform, Radius, Height, RadialSteps, HeightSteps,
		bCapped, OriginMode, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended cylinder"));
}

FModelingResult UModelingService::AppendCone(int32 Handle, FTransform Transform, float BaseRadius, float TopRadius, float Height,
	int32 RadialSteps, int32 HeightSteps, bool bCapped, const FString& Origin, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	EGeometryScriptPrimitiveOriginMode OriginMode; FString Error;
	if (!ParseOrigin(Origin, OriginMode, Error)) { return Fail(Handle, Error); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCone(Mesh, PrimitiveOptions(MaterialID), Transform, BaseRadius, TopRadius, Height, RadialSteps,
		HeightSteps, bCapped, OriginMode, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended cone"));
}

FModelingResult UModelingService::AppendCapsule(int32 Handle, FTransform Transform, float Radius, float LineLength, int32 HemisphereSteps,
	int32 CircleSteps, const FString& Origin, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	EGeometryScriptPrimitiveOriginMode OriginMode; FString Error;
	if (!ParseOrigin(Origin, OriginMode, Error)) { return Fail(Handle, Error); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCapsule(Mesh, PrimitiveOptions(MaterialID), Transform, Radius, LineLength, HemisphereSteps,
		CircleSteps, 0, OriginMode, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended capsule"));
}

FModelingResult UModelingService::AppendTorus(int32 Handle, FTransform Transform, float MajorRadius, float MinorRadius, int32 MajorSteps,
	int32 MinorSteps, const FString& Origin, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	EGeometryScriptPrimitiveOriginMode OriginMode; FString Error;
	if (!ParseOrigin(Origin, OriginMode, Error)) { return Fail(Handle, Error); }
	UGeometryScriptDebug* Debug = NewDebug();
	FGeometryScriptRevolveOptions RevolveOptions;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendTorus(Mesh, PrimitiveOptions(MaterialID), Transform, RevolveOptions, MajorRadius, MinorRadius,
		MajorSteps, MinorSteps, OriginMode, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended torus"));
}

FModelingResult UModelingService::AppendRectangle(int32 Handle, FTransform Transform, float DimensionX, float DimensionY, int32 StepsWidth,
	int32 StepsHeight, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRectangleXY(Mesh, PrimitiveOptions(MaterialID), Transform, DimensionX, DimensionY, StepsWidth, StepsHeight, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended rectangle"));
}

FModelingResult UModelingService::AppendDisc(int32 Handle, FTransform Transform, float Radius, int32 AngleSteps, int32 SpokeSteps,
	float StartAngle, float EndAngle, float HoleRadius, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendDisc(Mesh, PrimitiveOptions(MaterialID), Transform, Radius, AngleSteps, SpokeSteps, StartAngle, EndAngle, HoleRadius, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended disc"));
}

FModelingResult UModelingService::AppendStairs(int32 Handle, FTransform Transform, float StepWidth, float StepHeight, float StepDepth,
	int32 NumSteps, bool bFloating, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendLinearStairs(Mesh, PrimitiveOptions(MaterialID), Transform, StepWidth, StepHeight, StepDepth, NumSteps, bFloating, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended stairs"));
}

FModelingResult UModelingService::AppendExtrudePolygon(int32 Handle, FTransform Transform, const TArray<FVector2D>& PolygonPoints, float Height,
	int32 HeightSteps, bool bCapped, const FString& Origin, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (PolygonPoints.Num() < 3) { return Fail(Handle, TEXT("PolygonPoints needs at least 3 points")); }
	EGeometryScriptPrimitiveOriginMode OriginMode; FString Error;
	if (!ParseOrigin(Origin, OriginMode, Error)) { return Fail(Handle, Error); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(Mesh, PrimitiveOptions(MaterialID), Transform, PolygonPoints, Height, HeightSteps,
		bCapped, OriginMode, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended extruded polygon"));
}

FModelingResult UModelingService::AppendRevolvePolygon(int32 Handle, FTransform Transform, const TArray<FVector2D>& ProfilePoints, float Radius,
	int32 Steps, float RevolveDegrees, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (ProfilePoints.Num() < 3) { return Fail(Handle, TEXT("ProfilePoints needs at least 3 points")); }
	UGeometryScriptDebug* Debug = NewDebug();
	FGeometryScriptRevolveOptions RevolveOptions;
	RevolveOptions.RevolveDegrees = RevolveDegrees;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendRevolvePolygon(Mesh, PrimitiveOptions(MaterialID), Transform, ProfilePoints, RevolveOptions, Radius, Steps, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended revolved profile"));
}

FModelingResult UModelingService::AppendMesh(int32 Handle, int32 OtherHandle, FTransform Transform)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UDynamicMesh* Other = FindMesh(OtherHandle);
	if (!Other) { return NoMesh(OtherHandle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(Mesh, Other, Transform, false, FGeometryScriptAppendMeshOptions(), Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Appended handle %d"), OtherHandle));
}

// =========================================================================
// Booleans
// =========================================================================

FModelingResult UModelingService::Boolean(int32 TargetHandle, int32 ToolHandle, const FString& Operation, FTransform ToolTransform,
	bool bFillHoles, bool bSimplifyOutput)
{
	UDynamicMesh* Target = FindMesh(TargetHandle);
	if (!Target) { return NoMesh(TargetHandle); }
	UDynamicMesh* Tool = FindMesh(ToolHandle);
	if (!Tool) { return NoMesh(ToolHandle); }
	EGeometryScriptBooleanOperation Op;
	if (!ParseEnum(Operation, Op))
	{
		return Fail(TargetHandle, FString::Printf(TEXT("Unknown boolean operation '%s' (use %s)"), *Operation, *EnumNames<EGeometryScriptBooleanOperation>()));
	}
	FGeometryScriptMeshBooleanOptions Options;
	Options.bFillHoles = bFillHoles;
	Options.bSimplifyOutput = bSimplifyOutput;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(Target, FTransform::Identity, Tool, ToolTransform, Op, Options, Debug);
	return Finish(TargetHandle, Target, Debug, FString::Printf(TEXT("Boolean %s with handle %d"), *Operation, ToolHandle));
}

FModelingResult UModelingService::SelfUnion(int32 Handle, bool bFillHoles, bool bTrimFlaps)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelfUnionOptions Options;
	Options.bFillHoles = bFillHoles;
	Options.bTrimFlaps = bTrimFlaps;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshSelfUnion(Mesh, Options, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Self union"));
}

FModelingResult UModelingService::PlaneCut(int32 Handle, FTransform CutFrame, bool bFillHoles, bool bFlipCutSide)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshPlaneCutOptions Options;
	Options.bFillHoles = bFillHoles;
	Options.bFlipCutSide = bFlipCutSide;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshPlaneCut(Mesh, CutFrame, Options, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Plane cut"));
}

FModelingResult UModelingService::Mirror(int32 Handle, FTransform MirrorFrame, bool bApplyPlaneCut, bool bWeldAlongPlane, bool bFlipCutSide)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshMirrorOptions Options;
	Options.bApplyPlaneCut = bApplyPlaneCut;
	Options.bWeldAlongPlane = bWeldAlongPlane;
	Options.bFlipCutSide = bFlipCutSide;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshMirror(Mesh, MirrorFrame, Options, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Mirror"));
}

// =========================================================================
// Selections
// =========================================================================

int32 UModelingService::SelectAll(int32 Handle, const FString& SelectionName, const FString& SelectionType)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh || SelectionName.IsEmpty()) { return -1; }
	EGeometryScriptMeshSelectionType Type = EGeometryScriptMeshSelectionType::Triangles;
	if (!SelectionType.IsEmpty() && !ParseEnum(SelectionType, Type))
	{
		UE_LOG(LogVibeModeling, Warning, TEXT("Unknown selection type '%s' (use %s)"), *SelectionType, *EnumNames<EGeometryScriptMeshSelectionType>());
		return -1;
	}
	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(Mesh, Selection, Type);
	StoreSelection(Handle, SelectionName, Selection);
	return Selection.GetNumSelected();
}

int32 UModelingService::SelectByNormalAngle(int32 Handle, const FString& SelectionName, FVector Normal, float MaxAngleDeg, bool bInvert)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh || SelectionName.IsEmpty()) { return -1; }
	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByNormalAngle(Mesh, Selection, Normal.GetSafeNormal(), MaxAngleDeg,
		EGeometryScriptMeshSelectionType::Triangles, bInvert, 3);
	StoreSelection(Handle, SelectionName, Selection);
	return Selection.GetNumSelected();
}

int32 UModelingService::SelectInBox(int32 Handle, const FString& SelectionName, FVector BoxMin, FVector BoxMax, bool bInvert)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh || SelectionName.IsEmpty()) { return -1; }
	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsInBox(Mesh, Selection, FBox(BoxMin, BoxMax), EGeometryScriptMeshSelectionType::Triangles, bInvert, 3);
	StoreSelection(Handle, SelectionName, Selection);
	return Selection.GetNumSelected();
}

int32 UModelingService::SelectInSphere(int32 Handle, const FString& SelectionName, FVector Center, float Radius, bool bInvert)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh || SelectionName.IsEmpty()) { return -1; }
	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsInSphere(Mesh, Selection, Center, Radius, EGeometryScriptMeshSelectionType::Triangles, bInvert, 3);
	StoreSelection(Handle, SelectionName, Selection);
	return Selection.GetNumSelected();
}

int32 UModelingService::SelectByMaterialID(int32 Handle, const FString& SelectionName, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh || SelectionName.IsEmpty()) { return -1; }
	TArray<int32> TriangleIDs;
	Mesh->ProcessMesh([&TriangleIDs, MaterialID](const FDynamicMesh3& M)
	{
		if (M.HasAttributes() && M.Attributes()->HasMaterialID())
		{
			const FDynamicMeshMaterialAttribute* Materials = M.Attributes()->GetMaterialID();
			for (const int32 Tid : M.TriangleIndicesItr())
			{
				if (Materials->GetValue(Tid) == MaterialID)
				{
					TriangleIDs.Add(Tid);
				}
			}
		}
	});
	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshSelectionFunctions::ConvertIndexArrayToMeshSelection(Mesh, TriangleIDs, EGeometryScriptMeshSelectionType::Triangles, Selection);
	StoreSelection(Handle, SelectionName, Selection);
	return Selection.GetNumSelected();
}

int32 UModelingService::ExpandContractSelection(int32 Handle, const FString& SelectionName, const FString& NewSelectionName, int32 Iterations, bool bContract)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh || NewSelectionName.IsEmpty()) { return -1; }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { UE_LOG(LogVibeModeling, Warning, TEXT("%s"), *Error); return -1; }
	FGeometryScriptMeshSelection NewSelection;
	UGeometryScriptLibrary_MeshSelectionFunctions::ExpandContractMeshSelection(Mesh, Selection, NewSelection, Iterations, bContract, false);
	StoreSelection(Handle, NewSelectionName, NewSelection);
	return NewSelection.GetNumSelected();
}

int32 UModelingService::InvertSelection(int32 Handle, const FString& SelectionName, const FString& NewSelectionName)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh || NewSelectionName.IsEmpty()) { return -1; }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { UE_LOG(LogVibeModeling, Warning, TEXT("%s"), *Error); return -1; }
	FGeometryScriptMeshSelection NewSelection;
	UGeometryScriptLibrary_MeshSelectionFunctions::InvertMeshSelection(Mesh, Selection, NewSelection, false);
	StoreSelection(Handle, NewSelectionName, NewSelection);
	return NewSelection.GetNumSelected();
}

int32 UModelingService::SelectionCount(int32 Handle, const FString& SelectionName)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return -1; }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return -1; }
	return Selection.GetNumSelected();
}

bool UModelingService::ClearSelections(int32 Handle)
{
	return Session().Selections.Remove(Handle) > 0;
}

// =========================================================================
// Poly-edit operations
// =========================================================================

FModelingResult UModelingService::ExtrudeFaces(int32 Handle, const FString& SelectionName, float Distance, FVector Direction)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	FGeometryScriptMeshLinearExtrudeOptions Options;
	Options.Distance = Distance;
	if (Direction.IsNearlyZero())
	{
		Options.DirectionMode = EGeometryScriptLinearExtrudeDirection::AverageFaceNormal;
	}
	else
	{
		Options.DirectionMode = EGeometryScriptLinearExtrudeDirection::FixedDirection;
		Options.Direction = Direction.GetSafeNormal();
	}
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshLinearExtrudeFaces(Mesh, Options, Selection, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Extruded %d faces by %.2f"), Selection.GetNumSelected(), Distance));
}

FModelingResult UModelingService::OffsetFaces(int32 Handle, const FString& SelectionName, float Distance)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	FGeometryScriptMeshOffsetFacesOptions Options;
	Options.Distance = Distance;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffsetFaces(Mesh, Options, Selection, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Offset %d faces by %.2f"), Selection.GetNumSelected(), Distance));
}

FModelingResult UModelingService::InsetFaces(int32 Handle, const FString& SelectionName, float Distance, float Softness)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	FGeometryScriptMeshInsetOutsetFacesOptions Options;
	Options.Distance = Distance;
	Options.Softness = Softness;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshInsetOutsetFaces(Mesh, Options, Selection, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Inset %d faces by %.2f"), Selection.GetNumSelected(), Distance));
}

FModelingResult UModelingService::OutsetFaces(int32 Handle, const FString& SelectionName, float Distance, float Softness)
{
	// GeometryScript's inset/outset op uses the sign of the distance: negative = outset.
	FModelingResult R = InsetFaces(Handle, SelectionName, -FMath::Abs(Distance), Softness);
	if (R.bSuccess) { R.Message = R.Message.Replace(TEXT("Inset"), TEXT("Outset")); }
	return R;
}

FModelingResult UModelingService::DeleteFaces(int32 Handle, const FString& SelectionName)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	int32 NumDeleted = 0;
	UGeometryScriptLibrary_MeshBasicEditFunctions::DeleteSelectedTrianglesFromMesh(Mesh, Selection, NumDeleted, false);
	return Ok(Handle, Mesh, FString::Printf(TEXT("Deleted %d triangles"), NumDeleted));
}

FModelingResult UModelingService::TranslateSelection(int32 Handle, const FString& SelectionName, FVector Delta)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshTransformFunctions::TranslateMeshSelection(Mesh, Selection, Delta, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Translated selection"));
}

FModelingResult UModelingService::BevelPolygroups(int32 Handle, float Distance, int32 Subdivisions, float RoundWeight)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshBevelOptions Options;
	Options.BevelDistance = Distance;
	Options.Subdivisions = Subdivisions;
	Options.RoundWeight = RoundWeight;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshPolygroupBevel(Mesh, Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Beveled polygroup edges by %.2f"), Distance));
}

FModelingResult UModelingService::OffsetMesh(int32 Handle, float Distance)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshOffsetOptions Options;
	Options.OffsetDistance = Distance;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffset(Mesh, Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Offset surface by %.2f"), Distance));
}

FModelingResult UModelingService::ShellMesh(int32 Handle, float Thickness)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshOffsetOptions Options;
	Options.OffsetDistance = Thickness;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshShell(Mesh, Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Shelled with thickness %.2f"), Thickness));
}

// =========================================================================
// Mesh processing
// =========================================================================

FModelingResult UModelingService::Remesh(int32 Handle, int32 TargetTriangleCount, float EdgeLength, bool bReprojectToInput)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptRemeshOptions RemeshOptions;
	RemeshOptions.bReprojectToInputMesh = bReprojectToInput;
	FGeometryScriptUniformRemeshOptions UniformOptions;
	if (EdgeLength > 0.f)
	{
		UniformOptions.TargetType = EGeometryScriptUniformRemeshTargetType::TargetEdgeLength;
		UniformOptions.TargetEdgeLength = EdgeLength;
	}
	else
	{
		UniformOptions.TargetType = EGeometryScriptUniformRemeshTargetType::TriangleCount;
		UniformOptions.TargetTriangleCount = FMath::Max(4, TargetTriangleCount);
	}
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_RemeshingFunctions::ApplyUniformRemesh(Mesh, RemeshOptions, UniformOptions, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Remeshed"));
}

FModelingResult UModelingService::SimplifyToTriangleCount(int32 Handle, int32 TriangleCount, bool bAllowSeamCollapse)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptSimplifyMeshOptions Options;
	Options.bAllowSeamCollapse = bAllowSeamCollapse;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(Mesh, FMath::Max(4, TriangleCount), Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Simplified toward %d triangles"), TriangleCount));
}

FModelingResult UModelingService::SimplifyToTolerance(int32 Handle, float Tolerance, bool bAllowSeamCollapse)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptSimplifyMeshOptions Options;
	Options.bAllowSeamCollapse = bAllowSeamCollapse;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTolerance(Mesh, Tolerance, Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Simplified to tolerance %.3f"), Tolerance));
}

FModelingResult UModelingService::Subdivide(int32 Handle, int32 Level, const FString& Method)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	Level = FMath::Clamp(Level, 1, 6);
	UGeometryScriptDebug* Debug = NewDebug();
	const FString M = Method.TrimStartAndEnd();
	UDynamicMesh* Result = Mesh;
	if (M.Equals(TEXT("PN"), ESearchCase::IgnoreCase))
	{
		FGeometryScriptPNTessellateOptions Options;
		Result = UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyPNTessellation(Mesh, Options, Level, Debug);
	}
	else if (M.Equals(TEXT("Uniform"), ESearchCase::IgnoreCase))
	{
		Result = UGeometryScriptLibrary_MeshSubdivideFunctions::ApplyUniformTessellation(Mesh, Level, Debug);
	}
	else if (M.Equals(TEXT("CatmullClark"), ESearchCase::IgnoreCase))
	{
		Result = UGeometryScriptLibrary_OpenSubdivFunctions::ApplyPolygroupCatmullClarkSubD(Mesh, Level, FGeometryScriptGroupLayer(), Debug);
	}
	else if (M.Equals(TEXT("Loop"), ESearchCase::IgnoreCase))
	{
		Result = UGeometryScriptLibrary_OpenSubdivFunctions::ApplyTriangleLoopSubD(Mesh, Level, Debug);
	}
	else
	{
		return Fail(Handle, FString::Printf(TEXT("Unknown subdivide method '%s' (use PN, Uniform, CatmullClark, Loop)"), *Method));
	}
	if (Result && Result != Mesh)
	{
		UDynamicMesh* Out = nullptr;
		UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(Result, Mesh, Out, Debug);
	}
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Subdivided (%s, level %d)"), *M, Level));
}

FModelingResult UModelingService::Smooth(int32 Handle, const FString& SelectionName, int32 Iterations, float Alpha)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	FGeometryScriptIterativeMeshSmoothingOptions Options;
	Options.NumIterations = FMath::Max(1, Iterations);
	Options.Alpha = Alpha;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshDeformFunctions::ApplyIterativeSmoothingToMesh(Mesh, Selection, Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Smoothed (%d iterations)"), Iterations));
}

FModelingResult UModelingService::FillHoles(int32 Handle)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptFillHolesOptions Options;
	int32 NumFilled = 0, NumFailed = 0;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(Mesh, Options, NumFilled, NumFailed, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Filled %d holes (%d failed)"), NumFilled, NumFailed));
}

FModelingResult UModelingService::WeldEdges(int32 Handle, float Tolerance)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptWeldEdgesOptions Options;
	Options.Tolerance = Tolerance;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshRepairFunctions::WeldMeshEdges(Mesh, Options, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Welded edges"));
}

FModelingResult UModelingService::Repair(int32 Handle, float MinComponentVolume, int32 MinComponentTriangles)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	const int32 Before = TriCount(Mesh);
	FGeometryScriptDegenerateTriangleOptions DegenerateOptions;
	UGeometryScriptLibrary_MeshRepairFunctions::RepairMeshDegenerateGeometry(Mesh, DegenerateOptions, Debug);
	FGeometryScriptRemoveSmallComponentOptions SmallOptions;
	SmallOptions.MinVolume = MinComponentVolume;
	SmallOptions.MinTriangleCount = MinComponentTriangles;
	UGeometryScriptLibrary_MeshRepairFunctions::RemoveSmallComponents(Mesh, SmallOptions, Debug);
	UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Mesh, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Repaired (%d -> %d triangles)"), Before, TriCount(Mesh)));
}

FModelingResult UModelingService::RemoveHiddenTriangles(int32 Handle)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptRemoveHiddenTrianglesOptions Options;
	UGeometryScriptDebug* Debug = NewDebug();
	const int32 Before = TriCount(Mesh);
	UGeometryScriptLibrary_MeshRepairFunctions::RemoveHiddenTriangles(Mesh, Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Removed %d hidden triangles"), Before - TriCount(Mesh)));
}

FModelingSplitResult UModelingService::SplitByComponents(int32 Handle)
{
	FModelingSplitResult Result;
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh)
	{
		Result.Message = NoMesh(Handle).Message;
		return Result;
	}
	UDynamicMeshPool* Pool = UGeometryScriptLibrary_MeshPoolFunctions::GetGlobalMeshPool();
	TArray<UDynamicMesh*> Parts;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshDecompositionFunctions::SplitMeshByComponents(Mesh, Parts, Pool, Debug);
	if (DebugHasErrors(Debug))
	{
		Result.Message = DebugText(Debug);
		return Result;
	}
	TArray<TPair<int32, int32>> HandlesByTriangles; // (triangle count, handle)
	for (UDynamicMesh* Part : Parts)
	{
		if (!Part) { continue; }
		int32 NewHandle = -1;
		UDynamicMesh* Target = NewSessionMesh(NewHandle);
		UDynamicMesh* Out = nullptr;
		UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(Part, Target, Out, Debug);
		HandlesByTriangles.Emplace(TriCount(Target), NewHandle);
		if (Pool) { Pool->ReturnMesh(Part); }
	}
	HandlesByTriangles.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B) { return A.Key > B.Key; });
	for (const TPair<int32, int32>& Entry : HandlesByTriangles)
	{
		Result.Handles.Add(Entry.Value);
	}
	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("Split into %d components (largest first); source handle %d unchanged"), Result.Handles.Num(), Handle);
	return Result;
}

// =========================================================================
// Deformation
// =========================================================================

FModelingResult UModelingService::Bend(int32 Handle, FTransform Orientation, float AngleDeg, float Extent, bool bBidirectional)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptBendWarpOptions Options;
	Options.bBidirectional = bBidirectional;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshDeformFunctions::ApplyBendWarpToMesh(Mesh, Options, Orientation, AngleDeg, Extent, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Bent %.1f degrees"), AngleDeg));
}

FModelingResult UModelingService::Twist(int32 Handle, FTransform Orientation, float AngleDeg, float Extent, bool bBidirectional)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptTwistWarpOptions Options;
	Options.bBidirectional = bBidirectional;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshDeformFunctions::ApplyTwistWarpToMesh(Mesh, Options, Orientation, AngleDeg, Extent, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Twisted %.1f degrees"), AngleDeg));
}

FModelingResult UModelingService::Flare(int32 Handle, FTransform Orientation, float PercentX, float PercentY, float Extent)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptFlareWarpOptions Options;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshDeformFunctions::ApplyFlareWarpToMesh(Mesh, Options, Orientation, PercentX, PercentY, Extent, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Flared"));
}

FModelingResult UModelingService::Noise(int32 Handle, const FString& SelectionName, float Magnitude, float Frequency, int32 RandomSeed)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	FGeometryScriptPerlinNoiseOptions Options;
	Options.BaseLayer.Magnitude = Magnitude;
	Options.BaseLayer.Frequency = Frequency;
	Options.BaseLayer.RandomSeed = RandomSeed;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshDeformFunctions::ApplyPerlinNoiseToMesh2(Mesh, Selection, Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Applied noise (magnitude %.2f, frequency %.3f)"), Magnitude, Frequency));
}

FModelingResult UModelingService::DisplaceFromTexture(int32 Handle, const FString& SelectionName, const FString& TexturePath, float Magnitude, int32 UVLayer)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UTexture2D* Texture = LoadAssetAs<UTexture2D>(TexturePath);
	if (!Texture) { return Fail(Handle, FString::Printf(TEXT("Texture2D not found: %s"), *TexturePath)); }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	FGeometryScriptDisplaceFromTextureOptions Options;
	Options.Magnitude = Magnitude;
	FGeometryScriptAdaptiveTessellationOptions TessellationOptions;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshDeformFunctions::ApplyDisplaceFromTextureMap(Mesh, Texture, Selection, Options, TessellationOptions, UVLayer, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Displaced from %s"), *TexturePath));
}

// =========================================================================
// Voxel operations
// =========================================================================

FModelingResult UModelingService::VoxelSolidify(int32 Handle, int32 GridResolution, float WindingThreshold)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptSolidifyOptions Options;
	Options.GridParameters.SizeMethod = EGeometryScriptGridSizingMethod::GridResolution;
	Options.GridParameters.GridResolution = FMath::Clamp(GridResolution, 8, 512);
	Options.WindingThreshold = WindingThreshold;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshVoxelFunctions::ApplyMeshSolidify(Mesh, Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Solidified at grid %d"), GridResolution));
}

FModelingResult UModelingService::VoxelMorphology(int32 Handle, const FString& Operation, float Distance, int32 GridResolution)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMorphologyOptions Options;
	if (!ParseEnum(Operation, Options.Operation))
	{
		return Fail(Handle, FString::Printf(TEXT("Unknown morphology operation '%s' (use %s)"), *Operation, *EnumNames<EGeometryScriptMorphologicalOpType>()));
	}
	Options.Distance = Distance;
	Options.SDFGridParameters.SizeMethod = EGeometryScriptGridSizingMethod::GridResolution;
	Options.SDFGridParameters.GridResolution = FMath::Clamp(GridResolution, 8, 512);
	Options.MeshGridParameters = Options.SDFGridParameters;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshVoxelFunctions::ApplyMeshMorphology(Mesh, Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Morphology %s by %.2f"), *Operation, Distance));
}

// =========================================================================
// UVs, normals, groups, materials, colors
// =========================================================================

FModelingResult UModelingService::AutoUV(int32 Handle, const FString& Method, int32 UVLayer)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	// Booleans, deletes, and cuts leave id gaps; XAtlas refuses a non-compact mesh, so compact first.
	UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Mesh, Debug);
	if (GetMeshInfo(Handle).NumUVLayers <= UVLayer)
	{
		UGeometryScriptLibrary_MeshUVFunctions::SetNumUVSets(Mesh, UVLayer + 1, Debug);
	}
	if (Method.Equals(TEXT("XAtlas"), ESearchCase::IgnoreCase))
	{
		FGeometryScriptXAtlasOptions Options;
		UGeometryScriptLibrary_MeshUVFunctions::AutoGenerateXAtlasMeshUVs(Mesh, UVLayer, Options, Debug);
	}
	else if (Method.Equals(TEXT("PatchBuilder"), ESearchCase::IgnoreCase))
	{
		FGeometryScriptPatchBuilderOptions Options;
		UGeometryScriptLibrary_MeshUVFunctions::AutoGeneratePatchBuilderMeshUVs(Mesh, UVLayer, Options, Debug);
	}
	else
	{
		return Fail(Handle, FString::Printf(TEXT("Unknown UV method '%s' (use XAtlas or PatchBuilder)"), *Method));
	}
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Generated UVs (%s) in layer %d"), *Method, UVLayer));
}

FModelingResult UModelingService::ProjectUV(int32 Handle, const FString& Method, FTransform ProjectorTransform, int32 UVLayer, const FString& SelectionName)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	UGeometryScriptDebug* Debug = NewDebug();
	if (GetMeshInfo(Handle).NumUVLayers <= UVLayer)
	{
		UGeometryScriptLibrary_MeshUVFunctions::SetNumUVSets(Mesh, UVLayer + 1, Debug);
	}
	if (Method.Equals(TEXT("Planar"), ESearchCase::IgnoreCase))
	{
		UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromPlanarProjection(Mesh, UVLayer, ProjectorTransform, Selection, Debug);
	}
	else if (Method.Equals(TEXT("Box"), ESearchCase::IgnoreCase))
	{
		UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(Mesh, UVLayer, ProjectorTransform, Selection, 2, Debug);
	}
	else if (Method.Equals(TEXT("Cylinder"), ESearchCase::IgnoreCase))
	{
		UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromCylinderProjection(Mesh, UVLayer, ProjectorTransform, Selection, 45.f, Debug);
	}
	else
	{
		return Fail(Handle, FString::Printf(TEXT("Unknown projection '%s' (use Planar, Box, Cylinder)"), *Method));
	}
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Projected %s UVs into layer %d"), *Method, UVLayer));
}

FModelingResult UModelingService::RepackUV(int32 Handle, int32 UVLayer, int32 TextureResolution)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptRepackUVsOptions Options;
	Options.TargetImageWidth = FMath::Max(64, TextureResolution);
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshUVFunctions::RepackMeshUVs(Mesh, UVLayer, Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Repacked UV layer %d"), UVLayer));
}

FModelingResult UModelingService::SetNumUVLayers(int32 Handle, int32 NumLayers)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshUVFunctions::SetNumUVSets(Mesh, FMath::Clamp(NumLayers, 1, 8), Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Set %d UV layers"), NumLayers));
}

FModelingResult UModelingService::RecomputeNormals(int32 Handle, float HardAngleDeg)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	FGeometryScriptCalculateNormalsOptions CalculateOptions;
	if (HardAngleDeg < 0.f)
	{
		UGeometryScriptLibrary_MeshNormalsFunctions::SetPerVertexNormals(Mesh, Debug);
		UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, CalculateOptions, false, Debug);
		return Finish(Handle, Mesh, Debug, TEXT("Recomputed smooth normals"));
	}
	FGeometryScriptSplitNormalsOptions SplitOptions;
	SplitOptions.bSplitByOpeningAngle = true;
	SplitOptions.OpeningAngleDeg = HardAngleDeg;
	UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(Mesh, SplitOptions, CalculateOptions, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Recomputed normals (hard edges over %.1f degrees)"), HardAngleDeg));
}

FModelingResult UModelingService::FlipNormals(int32 Handle)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(Mesh, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Flipped normals"));
}

FModelingResult UModelingService::ComputePolygroups(int32 Handle, const FString& Method, float CreaseAngleDeg, int32 MinGroupSize)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	const FGeometryScriptGroupLayer Layer;
	if (Method.Equals(TEXT("Angle"), ESearchCase::IgnoreCase))
	{
		UGeometryScriptLibrary_MeshPolygroupFunctions::ComputePolygroupsFromAngleThreshold(Mesh, Layer, CreaseAngleDeg, MinGroupSize, Debug);
	}
	else if (Method.Equals(TEXT("UVIslands"), ESearchCase::IgnoreCase))
	{
		UGeometryScriptLibrary_MeshPolygroupFunctions::ConvertUVIslandsToPolygroups(Mesh, Layer, 0, Debug);
	}
	else if (Method.Equals(TEXT("Components"), ESearchCase::IgnoreCase))
	{
		UGeometryScriptLibrary_MeshPolygroupFunctions::ConvertComponentsToPolygroups(Mesh, Layer, Debug);
	}
	else if (Method.Equals(TEXT("Polygons"), ESearchCase::IgnoreCase))
	{
		UGeometryScriptLibrary_MeshPolygroupFunctions::ComputePolygroupsFromPolygonDetection(Mesh, Layer, true, false, 1.0, 1.0, 1, Debug);
	}
	else
	{
		return Fail(Handle, FString::Printf(TEXT("Unknown polygroup method '%s' (use Angle, UVIslands, Components, Polygons)"), *Method));
	}
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Computed polygroups (%s)"), *Method));
}

FModelingResult UModelingService::RemapMaterialID(int32 Handle, int32 FromMaterialID, int32 ToMaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshMaterialFunctions::EnableMaterialIDs(Mesh, Debug);
	UGeometryScriptLibrary_MeshMaterialFunctions::RemapMaterialIDs(Mesh, FromMaterialID, ToMaterialID, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Remapped material %d -> %d"), FromMaterialID, ToMaterialID));
}

FModelingResult UModelingService::SetMaterialID(int32 Handle, const FString& SelectionName, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshMaterialFunctions::EnableMaterialIDs(Mesh, Debug);
	UGeometryScriptLibrary_MeshMaterialFunctions::SetMaterialIDForMeshSelection(Mesh, Selection, MaterialID, false, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Set material ID %d on %d triangles"), MaterialID, Selection.GetNumSelected()));
}

FModelingResult UModelingService::SetVertexColor(int32 Handle, const FString& SelectionName, FLinearColor Color)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	FGeometryScriptColorFlags Flags;
	if (SelectionName.IsEmpty())
	{
		UGeometryScriptLibrary_MeshVertexColorFunctions::SetMeshConstantVertexColor(Mesh, Color, Flags, false, Debug);
		return Finish(Handle, Mesh, Debug, TEXT("Set constant vertex color"));
	}
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	if (!GetMeshInfo(Handle).bHasVertexColors)
	{
		UGeometryScriptLibrary_MeshVertexColorFunctions::SetMeshConstantVertexColor(Mesh, FLinearColor::White, Flags, false, Debug);
	}
	UGeometryScriptLibrary_MeshVertexColorFunctions::SetMeshSelectionVertexColor(Mesh, Selection, Color, Flags, false, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Set vertex color on selection '%s'"), *SelectionName));
}

// =========================================================================
// Transform
// =========================================================================

FModelingResult UModelingService::TransformMesh(int32 Handle, FTransform Transform)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshTransformFunctions::TransformMesh(Mesh, Transform, true, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Transformed"));
}

FModelingResult UModelingService::TranslateMesh(int32 Handle, FVector Translation)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(Mesh, Translation, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Translated"));
}

FModelingResult UModelingService::RotateMesh(int32 Handle, FRotator Rotation, FVector Origin)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshTransformFunctions::RotateMesh(Mesh, Rotation, Origin, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Rotated"));
}

FModelingResult UModelingService::ScaleMesh(int32 Handle, FVector Scale, FVector Origin)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(Mesh, Scale, Origin, true, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Scaled"));
}

FModelingResult UModelingService::RecenterMesh(int32 Handle, const FString& Mode)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	const FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
	FVector Shift;
	if (Mode.Equals(TEXT("Bounds"), ESearchCase::IgnoreCase))
	{
		Shift = -Bounds.GetCenter();
	}
	else if (Mode.Equals(TEXT("Base"), ESearchCase::IgnoreCase))
	{
		const FVector Center = Bounds.GetCenter();
		Shift = FVector(-Center.X, -Center.Y, -Bounds.Min.Z);
	}
	else
	{
		return Fail(Handle, FString::Printf(TEXT("Unknown recenter mode '%s' (use Bounds or Base)"), *Mode));
	}
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(Mesh, Shift, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Recentered (%s), shifted by %s"), *Mode, *Shift.ToCompactString()));
}

// =========================================================================
// Assets, collision, LODs, baking, placement
// =========================================================================

FModelingResult UModelingService::SaveMeshToStaticMesh(int32 Handle, const FString& AssetPath, bool bReplaceExisting, bool bEnableCollision,
	bool bEnableNanite, bool bSaveAsset)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (TriCount(Mesh) == 0) { return Fail(Handle, TEXT("Mesh is empty; nothing to save")); }
	if (!AssetPath.StartsWith(TEXT("/"))) { return Fail(Handle, TEXT("AssetPath must be a content path like /Game/Props/SM_Crate")); }

	UGeometryScriptDebug* Debug = NewDebug();
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	UStaticMesh* StaticMesh = nullptr;
	FString What;
	if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
	{
		StaticMesh = LoadAssetAs<UStaticMesh>(AssetPath);
		if (!StaticMesh) { return Fail(Handle, FString::Printf(TEXT("%s exists but is not a StaticMesh"), *AssetPath)); }
		if (!bReplaceExisting) { return Fail(Handle, FString::Printf(TEXT("%s already exists (pass replace_existing=True to overwrite LOD0)"), *AssetPath)); }
		FGeometryScriptCopyMeshToAssetOptions Options;
		FGeometryScriptMeshWriteLOD WriteLOD;
		WriteLOD.LODIndex = 0;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToStaticMesh(Mesh, StaticMesh, Options, WriteLOD, Outcome, true, Debug);
		What = FString::Printf(TEXT("Replaced LOD0 of %s"), *AssetPath);
	}
	else
	{
		FGeometryScriptCreateNewStaticMeshAssetOptions Options;
		Options.bEnableCollision = bEnableCollision;
		Options.bEnableNanite = bEnableNanite;
		StaticMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(Mesh, AssetPath, Options, Outcome, Debug);
		What = FString::Printf(TEXT("Created %s"), *AssetPath);
	}
	if (Outcome != EGeometryScriptOutcomePins::Success || !StaticMesh)
	{
		return Fail(Handle, FString::Printf(TEXT("Saving to %s failed: %s"), *AssetPath, *DebugText(Debug)));
	}
	SaveIf(bSaveAsset, StaticMesh->GetPathName());
	FModelingResult R = Ok(Handle, Mesh, What);
	R.AssetPath = StaticMesh->GetPathName();
	return R;
}

FModelingResult UModelingService::SaveMeshToSkeletalMesh(int32 Handle, const FString& AssetPath, const FString& SkeletonPath, bool bReplaceExisting, bool bSaveAsset)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (TriCount(Mesh) == 0) { return Fail(Handle, TEXT("Mesh is empty; nothing to save")); }
	if (!AssetPath.StartsWith(TEXT("/"))) { return Fail(Handle, FString::Printf(TEXT("AssetPath must be a full package path like /Game/Folder/SK_Name (got '%s')"), *AssetPath)); }

	bool bMeshHasBones = false;
	bool bMeshHasWeights = false;
	int32 MaxMaterialID = 0;
	Mesh->ProcessMesh([&](const FDynamicMesh3& M)
	{
		if (!M.HasAttributes()) { return; }
		bMeshHasBones = M.Attributes()->HasBones();
		bMeshHasWeights = M.Attributes()->GetSkinWeightsAttributes().Num() > 0;
		if (M.Attributes()->HasMaterialID())
		{
			const FDynamicMeshMaterialAttribute* MaterialIDs = M.Attributes()->GetMaterialID();
			for (const int32 Tid : M.TriangleIndicesItr()) { MaxMaterialID = FMath::Max(MaxMaterialID, MaterialIDs->GetValue(Tid)); }
		}
	});
	if (!bMeshHasWeights)
	{
		return Fail(Handle, TEXT("Mesh has no skin weights: run create_bones + bind_selection_to_bone (or transfer_bone_weights from a skeletal mesh) first"));
	}

	const bool bExists = UEditorAssetLibrary::DoesAssetExist(AssetPath);
	USkeleton* Skeleton = nullptr;
	FString SkeletonNote;
	if (!SkeletonPath.IsEmpty())
	{
		Skeleton = LoadAssetAs<USkeleton>(SkeletonPath);
		if (!Skeleton) { return Fail(Handle, FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath)); }
	}
	else if (!bExists)
	{
		if (!bMeshHasBones)
		{
			return Fail(Handle, TEXT("No SkeletonPath and the mesh has no bones: pass an existing skeleton, or define bones with create_bones so one can be created"));
		}
		// Create an empty Skeleton next to the mesh; CreateSkeletalMeshAsset merges the mesh's bones into it.
		const FString NewSkeletonPath = AssetPath + TEXT("_Skeleton");
		if (UEditorAssetLibrary::DoesAssetExist(NewSkeletonPath))
		{
			Skeleton = LoadAssetAs<USkeleton>(NewSkeletonPath);
			if (!Skeleton) { return Fail(Handle, FString::Printf(TEXT("%s exists but is not a Skeleton"), *NewSkeletonPath)); }
		}
		else
		{
			UPackage* Package = CreatePackage(*NewSkeletonPath);
			Skeleton = NewObject<USkeleton>(Package, *FPackageName::GetLongPackageAssetName(NewSkeletonPath), RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Skeleton);
			Package->MarkPackageDirty();
		}
		SkeletonNote = FString::Printf(TEXT(" with skeleton %s"), *NewSkeletonPath);
	}

	UGeometryScriptDebug* Debug = NewDebug();
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	USkeletalMesh* SkeletalMesh = nullptr;
	FString What;
	if (bExists)
	{
		SkeletalMesh = LoadAssetAs<USkeletalMesh>(AssetPath);
		if (!SkeletalMesh) { return Fail(Handle, FString::Printf(TEXT("%s exists but is not a SkeletalMesh"), *AssetPath)); }
		if (!bReplaceExisting) { return Fail(Handle, FString::Printf(TEXT("%s already exists (pass replace_existing=True to overwrite LOD0)"), *AssetPath)); }
		FGeometryScriptCopyMeshToAssetOptions Options;
		FGeometryScriptMeshWriteLOD WriteLOD;
		WriteLOD.LODIndex = 0;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshToSkeletalMesh(Mesh, SkeletalMesh, Options, WriteLOD, Outcome, Debug);
		What = FString::Printf(TEXT("Replaced LOD0 of %s"), *AssetPath);
	}
	else
	{
		FGeometryScriptCreateNewSkeletalMeshAssetOptions Options;
		// Bones authored on the mesh (create_bones, or copied from a source skeletal mesh) define the reference skeleton.
		Options.bUseMeshBoneProportions = bMeshHasBones;
		// One slot per material ID so multi-material meshes keep their sections; set_asset_materials fills them in.
		for (int32 i = 0; i <= MaxMaterialID; ++i)
		{
			Options.Materials.Add(*FString::Printf(TEXT("Slot%d"), i), UMaterial::GetDefaultMaterial(MD_Surface));
		}
		SkeletalMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewSkeletalMeshAssetFromMesh(Mesh, Skeleton, AssetPath, Options, Outcome, Debug);
		if (SkeletalMesh && Skeleton && !Skeleton->GetPreviewMesh())
		{
			Skeleton->SetPreviewMesh(SkeletalMesh);
		}
		What = FString::Printf(TEXT("Created %s%s (%d material slot(s))"), *AssetPath, *SkeletonNote, MaxMaterialID + 1);
	}
	if (Outcome != EGeometryScriptOutcomePins::Success || !SkeletalMesh)
	{
		return Fail(Handle, FString::Printf(TEXT("Saving to %s failed: %s"), *AssetPath, *DebugText(Debug)));
	}
	SaveIf(bSaveAsset, SkeletalMesh->GetPathName());
	if (Skeleton) { SaveIf(bSaveAsset, Skeleton->GetPathName()); }
	FModelingResult R = Ok(Handle, Mesh, What);
	R.AssetPath = SkeletalMesh->GetPathName();
	return R;
}

FModelingResult UModelingService::TransferBoneWeights(int32 Handle, const FString& SourceSkeletalMeshPath, int32 SourceLODIndex)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	USkeletalMesh* SkeletalMesh = LoadAssetAs<USkeletalMesh>(SourceSkeletalMeshPath);
	if (!SkeletalMesh) { return Fail(Handle, FString::Printf(TEXT("SkeletalMesh not found: %s"), *SourceSkeletalMeshPath)); }
	UGeometryScriptDebug* Debug = NewDebug();
	UDynamicMesh* Source = NewScratchMesh();
	if (!CopyFromSkeletalMesh(SkeletalMesh, Source, SourceLODIndex, Debug))
	{
		return Fail(Handle, FString::Printf(TEXT("Could not read %s: %s"), *SourceSkeletalMeshPath, *DebugText(Debug)));
	}
	UGeometryScriptLibrary_MeshBoneWeightFunctions::TransferBoneWeightsFromMesh(Source, Mesh, FGeometryScriptTransferBoneWeightsOptions(), FGeometryScriptMeshSelection(), Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Transferred bone weights from %s"), *SourceSkeletalMeshPath));
}

FModelingResult UModelingService::GenerateCollision(const FString& AssetPath, const FString& Method, int32 MaxConvexHulls, int32 ConvexHullTargetFaceCount, bool bSaveAsset)
{
	UStaticMesh* StaticMesh = LoadAssetAs<UStaticMesh>(AssetPath);
	if (!StaticMesh) { return Fail(-1, FString::Printf(TEXT("StaticMesh not found: %s"), *AssetPath)); }
	FGeometryScriptCollisionFromMeshOptions Options;
	if (!ParseEnum(Method, Options.Method))
	{
		return Fail(-1, FString::Printf(TEXT("Unknown collision method '%s' (use %s)"), *Method, *EnumNames<EGeometryScriptCollisionGenerationMethod>()));
	}
	Options.MaxConvexHullsPerMesh = FMath::Max(1, MaxConvexHulls);
	Options.ConvexHullTargetFaceCount = FMath::Max(4, ConvexHullTargetFaceCount);
	UGeometryScriptDebug* Debug = NewDebug();
	UDynamicMesh* Scratch = NewScratchMesh();
	if (!CopyFromStaticMesh(StaticMesh, Scratch, 0, Debug))
	{
		return Fail(-1, FString::Printf(TEXT("Could not read %s: %s"), *AssetPath, *DebugText(Debug)));
	}
	UGeometryScriptLibrary_CollisionFunctions::SetStaticMeshCollisionFromMesh(Scratch, StaticMesh, Options, FGeometryScriptSetStaticMeshCollisionOptions(), Debug);
	if (DebugHasErrors(Debug))
	{
		return Fail(-1, FString::Printf(TEXT("Collision generation failed: %s"), *DebugText(Debug)));
	}
	SaveIf(bSaveAsset, AssetPath);
	FModelingResult R = Ok(-1, Scratch, FString::Printf(TEXT("Generated %s collision on %s"), *Method, *AssetPath));
	R.AssetPath = AssetPath;
	return R;
}

FModelingResult UModelingService::SetLODs(const FString& AssetPath, const TArray<float>& PercentTrianglesPerLOD, bool bAutoComputeScreenSize, bool bSaveAsset)
{
	UStaticMesh* StaticMesh = LoadAssetAs<UStaticMesh>(AssetPath);
	if (!StaticMesh) { return Fail(-1, FString::Printf(TEXT("StaticMesh not found: %s"), *AssetPath)); }
	if (PercentTrianglesPerLOD.Num() == 0) { return Fail(-1, TEXT("Give one PercentTriangles entry per LOD, e.g. [1.0, 0.5, 0.25]")); }
	UStaticMeshEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>() : nullptr;
	if (!Subsystem) { return Fail(-1, TEXT("StaticMeshEditorSubsystem unavailable")); }
	FStaticMeshReductionOptions Options;
	Options.bAutoComputeLODScreenSize = bAutoComputeScreenSize;
	for (int32 i = 0; i < PercentTrianglesPerLOD.Num(); ++i)
	{
		FStaticMeshReductionSettings Settings;
		Settings.PercentTriangles = FMath::Clamp(PercentTrianglesPerLOD[i], 0.01f, 1.f);
		Settings.ScreenSize = 1.f / FMath::Pow(2.f, static_cast<float>(i));
		Options.ReductionSettings.Add(Settings);
	}
	const int32 NumLODs = Subsystem->SetLods(StaticMesh, Options);
	if (NumLODs < 0)
	{
		return Fail(-1, FString::Printf(TEXT("SetLods failed on %s"), *AssetPath));
	}
	SaveIf(bSaveAsset, AssetPath);
	FModelingResult R;
	R.bSuccess = true;
	R.AssetPath = AssetPath;
	R.Message = FString::Printf(TEXT("%s now has %d LODs"), *AssetPath, NumLODs);
	return R;
}

namespace
{
	/** Shared tail of every bake: run the baker, then save one Texture2D asset per requested type (T_<BaseName>_<Suffix>). */
	FModelingBakeResult RunBake(UDynamicMesh* Target, UDynamicMesh* Source, const TArray<FGeometryScriptBakeTypeOptions>& Types, const TArray<FString>& Suffixes,
		int32 Resolution, const FString& OutputFolder, const FString& BaseName, int32 TargetUVLayer, int32 SamplesPerPixel)
	{
		FModelingBakeResult Result;
		FGeometryScriptBakeTargetMeshOptions TargetOptions;
		TargetOptions.TargetUVLayer = TargetUVLayer;
		FGeometryScriptBakeSourceMeshOptions SourceOptions;
		FGeometryScriptBakeTextureOptions BakeOptions;
		if (!ParseEnum(FString::Printf(TEXT("Resolution%d"), Resolution), BakeOptions.Resolution))
		{
			Result.Message = FString::Printf(TEXT("Unsupported resolution %d (use a power of two from 16 to 8192)"), Resolution);
			return Result;
		}
		if (!ParseEnum(FString::Printf(TEXT("Sample%d"), SamplesPerPixel), BakeOptions.SamplesPerPixel))
		{
			BakeOptions.SamplesPerPixel = EGeometryScriptBakeSamplesPerPixel::Sample1;
		}
		UGeometryScriptDebug* Debug = NewDebug();
		// The baker samples through the target's tangent frame; session meshes rarely carry tangents yet.
		UGeometryScriptLibrary_MeshNormalsFunctions::ComputeTangents(Target, FGeometryScriptTangentsOptions(), Debug);
		const TArray<UTexture2D*> Textures = UGeometryScriptLibrary_MeshBakeFunctions::BakeTexture(Target, FTransform::Identity, TargetOptions,
			Source, FTransform::Identity, SourceOptions, Types, BakeOptions, Debug);
		if (DebugHasErrors(Debug) || Textures.Num() != Types.Num())
		{
			Result.Message = FString::Printf(TEXT("Bake failed: %s"), *DebugText(Debug));
			return Result;
		}
		for (int32 i = 0; i < Textures.Num(); ++i)
		{
			if (!Textures[i]) { continue; }
			FGeometryScriptCreateNewTexture2DAssetOptions TextureOptions;
			TextureOptions.bOverwriteIfExists = true;
			EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
			const FString AssetPath = OutputFolder / FString::Printf(TEXT("T_%s_%s"), *BaseName, *Suffixes[i]);
			UTexture2D* Saved = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewTexture2DAsset(Textures[i], AssetPath, TextureOptions, Outcome, Debug);
			if (Outcome != EGeometryScriptOutcomePins::Success || !Saved)
			{
				Result.Message = FString::Printf(TEXT("Could not save %s: %s"), *AssetPath, *DebugText(Debug));
				return Result;
			}
			Saved->BlockOnAnyAsyncBuild();   // the texture compiles asynchronously; finish it so a save or delete right after cannot race it
			UEditorAssetLibrary::SaveAsset(Saved->GetPathName(), false);
			Result.TexturePaths.Add(Saved->GetPathName());
		}
		Result.bSuccess = true;
		Result.Message = FString::Printf(TEXT("Baked %d texture(s) at %d"), Result.TexturePaths.Num(), Resolution);
		return Result;
	}
}

FModelingBakeResult UModelingService::BakeTextures(int32 TargetHandle, int32 SourceHandle, const FString& BakeTypes, int32 Resolution, const FString& OutputFolder,
	const FString& BaseName, int32 TargetUVLayer, int32 SamplesPerPixel)
{
	FModelingBakeResult Result;
	UDynamicMesh* Target = FindMesh(TargetHandle);
	if (!Target) { Result.Message = NoMesh(TargetHandle).Message; return Result; }
	UDynamicMesh* Source = FindMesh(SourceHandle);
	if (!Source) { Result.Message = NoMesh(SourceHandle).Message; return Result; }
	if (!OutputFolder.StartsWith(TEXT("/")) || BaseName.IsEmpty()) { Result.Message = TEXT("OutputFolder must be a content path (e.g. /Game/Textures) and BaseName non-empty"); return Result; }

	TArray<FString> TypeNames;
	BakeTypes.ParseIntoArray(TypeNames, TEXT(","), true);
	TArray<FGeometryScriptBakeTypeOptions> Types;
	TArray<FString> Suffixes;
	for (FString Name : TypeNames)
	{
		Name.TrimStartAndEndInline();
		if (Name.Equals(TEXT("TangentNormal"), ESearchCase::IgnoreCase)) { Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeTangentNormal()); Suffixes.Add(TEXT("N")); }
		else if (Name.Equals(TEXT("ObjectNormal"), ESearchCase::IgnoreCase)) { Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeObjectNormal()); Suffixes.Add(TEXT("ON")); }
		else if (Name.Equals(TEXT("FaceNormal"), ESearchCase::IgnoreCase)) { Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeFaceNormal()); Suffixes.Add(TEXT("FN")); }
		else if (Name.Equals(TEXT("BentNormal"), ESearchCase::IgnoreCase)) { Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeBentNormal()); Suffixes.Add(TEXT("BN")); }
		else if (Name.Equals(TEXT("AmbientOcclusion"), ESearchCase::IgnoreCase)) { Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeAmbientOcclusion()); Suffixes.Add(TEXT("AO")); }
		else if (Name.Equals(TEXT("Curvature"), ESearchCase::IgnoreCase)) { Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeCurvature()); Suffixes.Add(TEXT("Curv")); }
		else if (Name.Equals(TEXT("Position"), ESearchCase::IgnoreCase)) { Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypePosition()); Suffixes.Add(TEXT("Pos")); }
		else if (Name.Equals(TEXT("VertexColor"), ESearchCase::IgnoreCase)) { Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeVertexColor()); Suffixes.Add(TEXT("VC")); }
		else if (Name.Equals(TEXT("MaterialID"), ESearchCase::IgnoreCase)) { Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeMaterialID()); Suffixes.Add(TEXT("ID")); }
		else if (Name.Equals(TEXT("UVShell"), ESearchCase::IgnoreCase)) { Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeUVShell(TargetUVLayer, 1.f, FLinearColor::White, FLinearColor(0.25f, 0.25f, 0.25f), FLinearColor::Black)); Suffixes.Add(TEXT("UV")); }
		else if (Name.Equals(TEXT("Height"), ESearchCase::IgnoreCase)) { Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeHeight()); Suffixes.Add(TEXT("H")); }
		else
		{
			Result.Message = FString::Printf(TEXT("Unknown bake type '%s' (use TangentNormal, ObjectNormal, FaceNormal, BentNormal, AmbientOcclusion, Curvature, Position, VertexColor, MaterialID, UVShell, Height; textures via bake_texture_transfer)"), *Name);
			return Result;
		}
	}
	if (Types.Num() == 0) { Result.Message = TEXT("No bake types given"); return Result; }
	return RunBake(Target, Source, Types, Suffixes, Resolution, OutputFolder, BaseName, TargetUVLayer, SamplesPerPixel);
}

FModelingBakeResult UModelingService::BakeTextureTransfer(int32 TargetHandle, int32 SourceHandle, const FString& SourceTexturePaths, int32 Resolution, const FString& OutputFolder,
	const FString& BaseName, int32 TargetUVLayer, int32 SourceUVLayer, int32 SamplesPerPixel)
{
	FModelingBakeResult Result;
	UDynamicMesh* Target = FindMesh(TargetHandle);
	if (!Target) { Result.Message = NoMesh(TargetHandle).Message; return Result; }
	UDynamicMesh* Source = FindMesh(SourceHandle);
	if (!Source) { Result.Message = NoMesh(SourceHandle).Message; return Result; }
	if (!OutputFolder.StartsWith(TEXT("/")) || BaseName.IsEmpty()) { Result.Message = TEXT("OutputFolder must be a content path (e.g. /Game/Textures) and BaseName non-empty"); return Result; }
	TArray<FString> Paths;
	SourceTexturePaths.ParseIntoArray(Paths, TEXT(","), true);
	TArray<UTexture2D*> Textures;
	for (FString& Path : Paths)
	{
		Path.TrimStartAndEndInline();
		if (Path.IsEmpty()) { continue; }
		UTexture2D* Texture = LoadAssetAs<UTexture2D>(Path);
		if (!Texture) { Result.Message = FString::Printf(TEXT("Texture2D not found: %s"), *Path); return Result; }
		Textures.Add(Texture);
	}
	if (Textures.Num() == 0) { Result.Message = TEXT("SourceTexturePaths must list at least one Texture2D asset (comma-separated, in source material-ID order for several)"); return Result; }
	TArray<FGeometryScriptBakeTypeOptions> Types;
	TArray<FString> Suffixes;
	if (Textures.Num() == 1)
	{
		Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeTexture(Textures[0], SourceUVLayer));
		Suffixes.Add(TEXT("Tex"));
	}
	else
	{
		Types.Add(UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeMultiTexture(Textures, SourceUVLayer));
		Suffixes.Add(TEXT("MultiTex"));
	}
	return RunBake(Target, Source, Types, Suffixes, Resolution, OutputFolder, BaseName, TargetUVLayer, SamplesPerPixel);
}

FModelingResult UModelingService::SpawnStaticMeshActor(const FString& AssetPath, FTransform Transform, const FString& ActorLabel)
{
	UStaticMesh* StaticMesh = LoadAssetAs<UStaticMesh>(AssetPath);
	if (!StaticMesh) { return Fail(-1, FString::Printf(TEXT("StaticMesh not found: %s"), *AssetPath)); }
	UEditorActorSubsystem* ActorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
	if (!ActorSubsystem) { return Fail(-1, TEXT("EditorActorSubsystem unavailable")); }
	AActor* Actor = ActorSubsystem->SpawnActorFromObject(StaticMesh, Transform.GetLocation(), Transform.Rotator(), false);
	if (!Actor) { return Fail(-1, FString::Printf(TEXT("Could not spawn an actor for %s"), *AssetPath)); }
	Actor->SetActorScale3D(Transform.GetScale3D());
	if (!ActorLabel.IsEmpty())
	{
		Actor->SetActorLabel(ActorLabel);
	}
	FModelingResult R;
	R.bSuccess = true;
	R.AssetPath = AssetPath;
	R.Message = FString::Printf(TEXT("Spawned '%s' (%s) at %s"), *Actor->GetActorLabel(), *Actor->GetPathName(), *Transform.GetLocation().ToCompactString());
	return R;
}

// =========================================================================
// More primitives
// =========================================================================

FModelingResult UModelingService::AppendCurvedStairs(int32 Handle, FTransform Transform, float StepWidth, float StepHeight, float InnerRadius,
	float CurveAngle, int32 NumSteps, bool bFloating, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCurvedStairs(Mesh, PrimitiveOptions(MaterialID), Transform, StepWidth, StepHeight, InnerRadius,
		CurveAngle, NumSteps, bFloating, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended curved stairs"));
}

FModelingResult UModelingService::AppendSphereBox(int32 Handle, FTransform Transform, float Radius, int32 Steps, const FString& Origin, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	EGeometryScriptPrimitiveOriginMode OriginMode; FString Error;
	if (!ParseOrigin(Origin, OriginMode, Error)) { return Fail(Handle, Error); }
	UGeometryScriptDebug* Debug = NewDebug();
	const int32 S = FMath::Clamp(Steps, 1, 64);
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereBox(Mesh, PrimitiveOptions(MaterialID), Transform, Radius, S, S, S, OriginMode, Debug);
	return Finish(Handle, Mesh, Debug, TEXT("Appended sphere box"));
}

FModelingResult UModelingService::AppendSweepPolyline(int32 Handle, FTransform Transform, const TArray<FVector2D>& ProfilePoints, const TArray<FTransform>& SweepPath,
	bool bLoop, float StartScale, float EndScale, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (ProfilePoints.Num() < 2) { return Fail(Handle, TEXT("ProfilePoints needs at least 2 points")); }
	if (SweepPath.Num() < 2) { return Fail(Handle, TEXT("SweepPath needs at least 2 transforms")); }
	UGeometryScriptDebug* Debug = NewDebug();
	const TArray<float> NoTexParams;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolyline(Mesh, PrimitiveOptions(MaterialID), Transform, ProfilePoints, SweepPath,
		NoTexParams, NoTexParams, bLoop, StartScale, EndScale, 0.f, 1.f, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Appended sweep (%d profile points along %d path frames)"), ProfilePoints.Num(), SweepPath.Num()));
}

// =========================================================================
// More simplification
// =========================================================================

FModelingResult UModelingService::SimplifyToVertexCount(int32 Handle, int32 VertexCount, bool bAllowSeamCollapse)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptSimplifyMeshOptions Options;
	Options.bAllowSeamCollapse = bAllowSeamCollapse;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToVertexCount(Mesh, FMath::Max(4, VertexCount), Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Simplified toward %d vertices"), VertexCount));
}

FModelingResult UModelingService::SimplifyPlanar(int32 Handle, float AngleThresholdDeg)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptPlanarSimplifyOptions Options;
	Options.AngleThreshold = AngleThresholdDeg;
	UGeometryScriptDebug* Debug = NewDebug();
	const int32 Before = TriCount(Mesh);
	UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToPlanar(Mesh, Options, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Planar simplify (%d -> %d triangles)"), Before, TriCount(Mesh)));
}

// =========================================================================
// Spatial queries and sampling
// =========================================================================

FModelingSurfacePoint UModelingService::RayCast(int32 Handle, FVector Origin, FVector Direction, float MaxDistance)
{
	FModelingSurfacePoint Result;
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { Result.Message = NoMesh(Handle).Message; return Result; }
	if (Direction.IsNearlyZero()) { Result.Message = TEXT("Direction must be non-zero"); return Result; }
	UGeometryScriptDebug* Debug = NewDebug();
	FGeometryScriptDynamicMeshBVH BVH;
	UGeometryScriptLibrary_MeshSpatial::BuildBVHForMesh(Mesh, BVH, Debug);
	FGeometryScriptSpatialQueryOptions Options;
	Options.MaxDistance = MaxDistance;
	FGeometryScriptRayHitResult Hit;
	EGeometryScriptSearchOutcomePins Outcome = EGeometryScriptSearchOutcomePins::NotFound;
	UGeometryScriptLibrary_MeshSpatial::FindNearestRayIntersectionWithMesh(Mesh, BVH, Origin, Direction.GetSafeNormal(), Options, Hit, Outcome, Debug);
	Result.bFound = Outcome == EGeometryScriptSearchOutcomePins::Found && Hit.bHit;
	Result.Position = Hit.HitPosition;
	Result.TriangleID = Hit.HitTriangleID;
	Result.Distance = Hit.RayParameter;
	Result.BaryCoords = Hit.HitBaryCoords;
	Result.Message = Result.bFound ? TEXT("Hit") : (DebugText(Debug).IsEmpty() ? TEXT("No hit") : DebugText(Debug));
	return Result;
}

FModelingSurfacePoint UModelingService::NearestPoint(int32 Handle, FVector Point, float MaxDistance)
{
	FModelingSurfacePoint Result;
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { Result.Message = NoMesh(Handle).Message; return Result; }
	UGeometryScriptDebug* Debug = NewDebug();
	FGeometryScriptDynamicMeshBVH BVH;
	UGeometryScriptLibrary_MeshSpatial::BuildBVHForMesh(Mesh, BVH, Debug);
	FGeometryScriptSpatialQueryOptions Options;
	Options.MaxDistance = MaxDistance;
	FGeometryScriptTrianglePoint Nearest;
	EGeometryScriptSearchOutcomePins Outcome = EGeometryScriptSearchOutcomePins::NotFound;
	UGeometryScriptLibrary_MeshSpatial::FindNearestPointOnMesh(Mesh, BVH, Point, Options, Nearest, Outcome, Debug);
	Result.bFound = Outcome == EGeometryScriptSearchOutcomePins::Found && Nearest.bValid;
	Result.Position = Nearest.Position;
	Result.TriangleID = Nearest.TriangleID;
	Result.Distance = Result.bFound ? static_cast<float>(FVector::Distance(Point, Nearest.Position)) : 0.f;
	Result.BaryCoords = Nearest.BaryCoords;
	Result.Message = Result.bFound ? TEXT("Found") : (DebugText(Debug).IsEmpty() ? TEXT("Nothing within MaxDistance") : DebugText(Debug));
	return Result;
}

bool UModelingService::IsPointInside(int32 Handle, FVector Point)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return false; }
	UGeometryScriptDebug* Debug = NewDebug();
	FGeometryScriptDynamicMeshBVH BVH;
	UGeometryScriptLibrary_MeshSpatial::BuildBVHForMesh(Mesh, BVH, Debug);
	FGeometryScriptSpatialQueryOptions Options;
	bool bInside = false;
	EGeometryScriptContainmentOutcomePins Outcome = EGeometryScriptContainmentOutcomePins::Outside;
	UGeometryScriptLibrary_MeshSpatial::IsPointInsideMesh(Mesh, BVH, Point, Options, bInside, Outcome, Debug);
	return bInside;
}

TArray<FTransform> UModelingService::SampleSurfacePoints(int32 Handle, float SamplingRadius, int32 MaxSamples)
{
	TArray<FTransform> Samples;
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return Samples; }
	FGeometryScriptMeshPointSamplingOptions Options;
	Options.SamplingRadius = FMath::Max(0.01f, SamplingRadius);
	Options.MaxNumSamples = FMath::Max(0, MaxSamples);
	FGeometryScriptIndexList TriangleIDs;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshSamplingFunctions::ComputePointSampling(Mesh, Options, Samples, TriangleIDs, Debug);
	return Samples;
}

bool UModelingService::SelectionBounds(int32 Handle, const FString& SelectionName, FVector& OutMin, FVector& OutMax)
{
	OutMin = OutMax = FVector::ZeroVector;
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return false; }
	FGeometryScriptMeshSelection Selection; FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return false; }
	FBox Bounds(ForceInit);
	bool bEmpty = true;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshSelectionQueryFunctions::GetMeshSelectionBoundingBox(Mesh, Selection, Bounds, bEmpty, Debug);
	if (bEmpty) { return false; }
	OutMin = Bounds.Min;
	OutMax = Bounds.Max;
	return true;
}

int32 UModelingService::SelectByPolygroup(int32 Handle, const FString& SelectionName, int32 PolygroupID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh || SelectionName.IsEmpty()) { return -1; }
	FGeometryScriptMeshSelection Selection;
	UGeometryScriptLibrary_MeshSelectionFunctions::SelectMeshElementsByPolygroup(Mesh, FGeometryScriptGroupLayer(), PolygroupID, Selection, EGeometryScriptMeshSelectionType::Triangles);
	StoreSelection(Handle, SelectionName, Selection);
	return Selection.GetNumSelected();
}

// =========================================================================
// Hulls and comparison
// =========================================================================

namespace
{
	/**
	 * Booleans, deletes, and cuts leave gaps in the vertex/triangle id space. The containment
	 * algorithms index by id and assert on a non-compact mesh, so hull operations run on a
	 * compacted scratch copy of the source (the session mesh itself is left untouched).
	 */
	UDynamicMesh* CompactedCopy(UDynamicMesh* Source, UGeometryScriptDebug* Debug)
	{
		UDynamicMesh* Scratch = NewScratchMesh();
		UDynamicMesh* Out = nullptr;
		UGeometryScriptLibrary_MeshDecompositionFunctions::CopyMeshToMesh(Source, Scratch, Out, Debug);
		UGeometryScriptLibrary_MeshRepairFunctions::CompactMesh(Scratch, Debug);
		return Scratch;
	}
}

FModelingResult UModelingService::ConvexHull(int32 Handle, int32 SimplifyToFaceCount)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UDynamicMesh* Source = CompactedCopy(Mesh, Debug);
	int32 NewHandle = -1;
	UDynamicMesh* Hull = NewSessionMesh(NewHandle);
	UDynamicMesh* Out = nullptr;
	FGeometryScriptMeshSelection All;
	UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(Source, All, EGeometryScriptMeshSelectionType::Triangles);
	FGeometryScriptConvexHullOptions Options;
	Options.SimplifyToFaceCount = FMath::Max(0, SimplifyToFaceCount);
	UGeometryScriptLibrary_ContainmentFunctions::ComputeMeshConvexHull(Source, Hull, Out, All, Options, Debug);
	return Finish(NewHandle, Hull, Debug, FString::Printf(TEXT("Convex hull of handle %d"), Handle));
}

FModelingResult UModelingService::ConvexDecomposition(int32 Handle, int32 NumHulls)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UDynamicMesh* Source = CompactedCopy(Mesh, Debug);
	int32 NewHandle = -1;
	UDynamicMesh* Hulls = NewSessionMesh(NewHandle);
	UDynamicMesh* Out = nullptr;
	FGeometryScriptConvexDecompositionOptions Options;
	Options.NumHulls = FMath::Clamp(NumHulls, 1, 64);
	UGeometryScriptLibrary_ContainmentFunctions::ComputeMeshConvexDecomposition(Source, Hulls, Out, Options, Debug);
	return Finish(NewHandle, Hulls, Debug, FString::Printf(TEXT("Convex decomposition of handle %d into %d hulls"), Handle, NumHulls));
}

FModelingResult UModelingService::SweptHull(int32 Handle, FTransform ProjectionFrame)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UGeometryScriptDebug* Debug = NewDebug();
	UDynamicMesh* Source = CompactedCopy(Mesh, Debug);
	int32 NewHandle = -1;
	UDynamicMesh* Hull = NewSessionMesh(NewHandle);
	UDynamicMesh* Out = nullptr;
	FGeometryScriptSweptHullOptions Options;
	UGeometryScriptLibrary_ContainmentFunctions::ComputeMeshSweptHull(Source, Hull, Out, ProjectionFrame, Options, Debug);
	return Finish(NewHandle, Hull, Debug, FString::Printf(TEXT("Swept hull of handle %d"), Handle));
}

FModelingDistanceReport UModelingService::MeasureDistance(int32 HandleA, int32 HandleB)
{
	FModelingDistanceReport Report;
	UDynamicMesh* A = FindMesh(HandleA);
	if (!A) { Report.Message = NoMesh(HandleA).Message; return Report; }
	UDynamicMesh* B = FindMesh(HandleB);
	if (!B) { Report.Message = NoMesh(HandleB).Message; return Report; }
	FGeometryScriptMeasureMeshDistanceOptions Options;
	double MaxD = 0, MinD = 0, AvgD = 0, Rms = 0;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshComparisonFunctions::MeasureDistancesBetweenMeshes(A, B, Options, MaxD, MinD, AvgD, Rms, Debug);
	if (DebugHasErrors(Debug)) { Report.Message = DebugText(Debug); return Report; }
	Report.bSuccess = true;
	Report.MaxDistance = static_cast<float>(MaxD);
	Report.MinDistance = static_cast<float>(MinD);
	Report.AverageDistance = static_cast<float>(AvgD);
	Report.RootMeanSquareDeviation = static_cast<float>(Rms);
	Report.Message = FString::Printf(TEXT("max %.3f, mean %.3f, rms %.3f"), MaxD, AvgD, Rms);
	return Report;
}

// =========================================================================
// More skeletal
// =========================================================================

FModelingResult UModelingService::SmoothBoneWeights(int32 Handle, const FString& SkeletonPath, float Stiffness, int32 MaxInfluences)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	USkeleton* Skeleton = LoadAssetAs<USkeleton>(SkeletonPath);
	if (!Skeleton) { return Fail(Handle, FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath)); }
	FGeometryScriptSmoothBoneWeightsOptions Options;
	Options.Stiffness = Stiffness;
	Options.MaxInfluences = FMath::Clamp(MaxInfluences, 1, 12);
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshBoneWeightFunctions::ComputeSmoothBoneWeights(Mesh, Skeleton, Options, FGeometryScriptBoneWeightProfile(), Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Smoothed bone weights against %s"), *SkeletonPath));
}

FModelingResult UModelingService::PruneBoneWeights(int32 Handle, const FString& BoneNames)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	TArray<FString> Names;
	BoneNames.ParseIntoArray(Names, TEXT(","), true);
	TArray<FName> Bones;
	for (FString& Name : Names)
	{
		Name.TrimStartAndEndInline();
		if (!Name.IsEmpty()) { Bones.Add(FName(*Name)); }
	}
	if (Bones.Num() == 0) { return Fail(Handle, TEXT("BoneNames must list at least one bone (comma-separated)")); }
	FGeometryScriptPruneBoneWeightsOptions Options;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshBoneWeightFunctions::PruneBoneWeights(Mesh, Bones, Options, FGeometryScriptBoneWeightProfile(), Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Pruned %d bone(s) from skin weights"), Bones.Num()));
}

// =========================================================================
// Lofts, orientation, connected pieces
// =========================================================================

FModelingResult UModelingService::EnsureOutward(int32 Handle)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	float Area = 0.f, Volume = 0.f;
	UGeometryScriptLibrary_MeshQueryFunctions::GetMeshVolumeArea(Mesh, Area, Volume);
	if (Volume >= 0.f)
	{
		return Ok(Handle, Mesh, FString::Printf(TEXT("Already outward-facing (volume %.1f)"), Volume));
	}
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(Mesh, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Flipped an inward-facing mesh (volume was %.1f)"), Volume));
}

FModelingResult UModelingService::AppendLoft(int32 Handle, FTransform Transform, const TArray<FVector2D>& ProfilePoints, const TArray<FTransform>& Frames, int32 MaterialID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (ProfilePoints.Num() < 3) { return Fail(Handle, TEXT("ProfilePoints needs at least 3 points (a closed profile)")); }
	if (Frames.Num() < 2) { return Fail(Handle, TEXT("Frames needs at least 2 transforms")); }
	UGeometryScriptDebug* Debug = NewDebug();

	// Closed-polygon sweep with caps, built in scratch so the orientation fix can only touch the new part.
	// The generator maps profile X to each frame's Y axis and profile Y to its Z axis, scaled by the frame's Y/Z scale.
	UDynamicMesh* Part = NewScratchMesh();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(Part, PrimitiveOptions(MaterialID), Transform, ProfilePoints, Frames,
		false, true, 1.f, 1.f, 0.f, 1.f, Debug);
	float Area = 0.f, Volume = 0.f;
	UGeometryScriptLibrary_MeshQueryFunctions::GetMeshVolumeArea(Part, Area, Volume);
	if (Volume < 0.f)
	{
		UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(Part, Debug);
	}
	UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(Mesh, Part, FTransform::Identity, false, FGeometryScriptAppendMeshOptions(), Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Appended loft (%d-point profile through %d frames%s)"),
		ProfilePoints.Num(), Frames.Num(), Volume < 0.f ? TEXT(", flipped outward") : TEXT("")));
}

int32 UModelingService::SelectConnected(int32 Handle, const FString& SelectionName, FVector Point)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh || SelectionName.IsEmpty()) { return -1; }
	UGeometryScriptDebug* Debug = NewDebug();
	FGeometryScriptDynamicMeshBVH BVH;
	UGeometryScriptLibrary_MeshSpatial::BuildBVHForMesh(Mesh, BVH, Debug);
	FGeometryScriptTrianglePoint Nearest;
	EGeometryScriptSearchOutcomePins Outcome = EGeometryScriptSearchOutcomePins::NotFound;
	UGeometryScriptLibrary_MeshSpatial::FindNearestPointOnMesh(Mesh, BVH, Point, FGeometryScriptSpatialQueryOptions(), Nearest, Outcome, Debug);
	FGeometryScriptMeshSelection Selection;
	if (Outcome == EGeometryScriptSearchOutcomePins::Found && Nearest.bValid)
	{
		FGeometryScriptMeshSelection Seed;
		UGeometryScriptLibrary_MeshSelectionFunctions::ConvertIndexArrayToMeshSelection(Mesh, TArray<int32>{ Nearest.TriangleID }, EGeometryScriptMeshSelectionType::Triangles, Seed);
		UGeometryScriptLibrary_MeshSelectionFunctions::ExpandMeshSelectionToConnected(Mesh, Seed, Selection, EGeometryScriptTopologyConnectionType::Geometric);
	}
	StoreSelection(Handle, SelectionName, Selection);
	return Selection.GetNumSelected();
}

// =========================================================================
// Rigging: bones and skin weights authored on the mesh
// =========================================================================

namespace
{
	using FSkinWeights = FDynamicMeshVertexSkinWeightsAttribute;
	using UE::AnimationCore::FBoneWeight;
	using UE::AnimationCore::FBoneWeights;

	/** Mesh-space reference pose of every bone (poses are stored relative to the parent, parents precede children). */
	TArray<FTransform> BoneMeshTransforms(const FDynamicMeshAttributeSet* Attributes)
	{
		TArray<FTransform> Out;
		const int32 Num = Attributes->GetNumBones();
		Out.SetNum(Num);
		for (int32 i = 0; i < Num; ++i)
		{
			const int32 Parent = Attributes->GetBoneParentIndices()->GetValue(i);
			const FTransform& Local = Attributes->GetBonePoses()->GetValue(i);
			Out[i] = (Parent >= 0 && Parent < i) ? Local * Out[Parent] : Local;
		}
		return Out;
	}

	int32 FindBoneIndexByName(const FDynamicMeshAttributeSet* Attributes, const FName Name)
	{
		for (int32 i = 0; i < Attributes->GetNumBones(); ++i)
		{
			if (Attributes->GetBoneNames()->GetValue(i) == Name) { return i; }
		}
		return INDEX_NONE;
	}

	FSkinWeights* DefaultSkinWeights(FDynamicMesh3& M)
	{
		return M.HasAttributes() ? M.Attributes()->GetSkinWeightsAttribute(FSkeletalMeshAttributes::DefaultSkinWeightProfileName) : nullptr;
	}
}

FModelingResult UModelingService::CreateBones(int32 Handle, const TArray<FModelingBoneDef>& Bones)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (Bones.Num() == 0) { return Fail(Handle, TEXT("Bones is empty: pass at least a root bone")); }

	TMap<FName, int32> IndexByName;
	TArray<FName> Names;
	TArray<int32> ParentIndices;
	for (int32 i = 0; i < Bones.Num(); ++i)
	{
		const FName Name(*Bones[i].Name.TrimStartAndEnd());
		if (Name.IsNone()) { return Fail(Handle, FString::Printf(TEXT("Bone %d has no name"), i)); }
		if (IndexByName.Contains(Name)) { return Fail(Handle, FString::Printf(TEXT("Duplicate bone name '%s'"), *Name.ToString())); }
		const FString ParentName = Bones[i].ParentName.TrimStartAndEnd();
		int32 Parent = INDEX_NONE;
		if (i == 0)
		{
			if (!ParentName.IsEmpty() && ParentName != TEXT("None"))
			{
				return Fail(Handle, FString::Printf(TEXT("The first bone ('%s') is the root and cannot have a parent"), *Name.ToString()));
			}
		}
		else
		{
			const int32* Found = ParentName.IsEmpty() ? nullptr : IndexByName.Find(FName(*ParentName));
			if (!Found)
			{
				return Fail(Handle, FString::Printf(TEXT("Bone '%s' needs a parent listed before it (got '%s'); only the first bone is a root"), *Name.ToString(), *ParentName));
			}
			Parent = *Found;
		}
		IndexByName.Add(Name, i);
		Names.Add(Name);
		ParentIndices.Add(Parent);
	}

	Mesh->EditMesh([&](FDynamicMesh3& M)
	{
		M.EnableAttributes();
		FDynamicMeshAttributeSet* Attributes = M.Attributes();
		Attributes->DisableBones();
		Attributes->EnableBones(Bones.Num());
		TArray<FTransform> MeshSpace;
		for (int32 i = 0; i < Bones.Num(); ++i)
		{
			const FTransform& World = Bones[i].Transform;
			MeshSpace.Add(World);
			const int32 Parent = ParentIndices[i];
			Attributes->GetBoneNames()->SetValue(i, Names[i]);
			Attributes->GetBoneParentIndices()->SetValue(i, Parent);
			Attributes->GetBonePoses()->SetValue(i, Parent >= 0 ? World.GetRelativeTransform(MeshSpace[Parent]) : World);
		}
	});

	// A fresh default profile with everything on the root, so the mesh is a valid skeletal mesh immediately.
	bool bProfileExisted = false;
	UGeometryScriptLibrary_MeshBoneWeightFunctions::MeshCreateBoneWeights(Mesh, bProfileExisted, true, FGeometryScriptBoneWeightProfile());
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshBoneWeightFunctions::SetAllVertexBoneWeights(Mesh, TArray<FGeometryScriptBoneWeight>{ FGeometryScriptBoneWeight(0, 1.f) }, FGeometryScriptBoneWeightProfile(), Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Created %d bone(s) rooted at '%s'; every vertex bound to the root (bind_selection_to_bone to assign pieces)"), Bones.Num(), *Names[0].ToString()));
}

FModelingResult UModelingService::BindSelectionToBone(int32 Handle, const FString& SelectionName, const FString& BoneName, float Weight)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection;
	FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }

	int32 NumBones = 0;
	int32 BoneIndex = INDEX_NONE;
	const FName Bone(*BoneName.TrimStartAndEnd());
	Mesh->ProcessMesh([&](const FDynamicMesh3& M)
	{
		if (M.HasAttributes() && M.Attributes()->HasBones())
		{
			NumBones = M.Attributes()->GetNumBones();
			BoneIndex = FindBoneIndexByName(M.Attributes(), Bone);
		}
	});
	if (NumBones == 0) { return Fail(Handle, TEXT("Mesh has no bones: run create_bones first (or load a skeletal mesh)")); }
	if (BoneIndex == INDEX_NONE) { return Fail(Handle, FString::Printf(TEXT("No bone named '%s' (see list_bones)"), *BoneName)); }

	bool bProfileExisted = false;
	UGeometryScriptLibrary_MeshBoneWeightFunctions::MeshCreateBoneWeights(Mesh, bProfileExisted, false, FGeometryScriptBoneWeightProfile());

	FGeometryScriptIndexList Vertices;
	EGeometryScriptIndexType ResultType = EGeometryScriptIndexType::Any;
	UGeometryScriptLibrary_MeshSelectionFunctions::ConvertMeshSelectionToIndexList(Mesh, Selection, Vertices, ResultType, EGeometryScriptIndexType::Vertex);
	if (!Vertices.List.IsValid() || ResultType != EGeometryScriptIndexType::Vertex)
	{
		return Fail(Handle, FString::Printf(TEXT("Selection '%s' could not be converted to vertices"), *SelectionName));
	}

	const float W = FMath::Clamp(Weight, 0.f, 1.f);
	int32 Count = 0;
	Mesh->EditMesh([&](FDynamicMesh3& M)
	{
		FSkinWeights* Skin = DefaultSkinWeights(M);
		if (!Skin) { return; }
		for (const int32 Vid : *Vertices.List)
		{
			if (!M.IsVertex(Vid)) { continue; }
			TArray<FBoneWeight> NewWeights;
			if (W < 1.f)
			{
				FBoneWeights Existing;
				Skin->GetValue(Vid, Existing);
				for (int32 k = 0; k < Existing.Num(); ++k)
				{
					const FBoneWeight& Old = Existing[k];
					if (Old.GetBoneIndex() != BoneIndex && Old.GetWeight() > 0.f)
					{
						NewWeights.Add(FBoneWeight(Old.GetBoneIndex(), Old.GetWeight() * (1.f - W)));
					}
				}
			}
			NewWeights.Add(FBoneWeight(static_cast<FBoneIndexType>(BoneIndex), W));
			Skin->SetValue(Vid, FBoneWeights::Create(NewWeights));
			++Count;
		}
	});
	return Ok(Handle, Mesh, FString::Printf(TEXT("Bound %d vertices to '%s' (weight %.2f)"), Count, *Bone.ToString(), W));
}

TArray<FModelingBoneInfo> UModelingService::ListBones(int32 Handle)
{
	TArray<FModelingBoneInfo> Out;
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return Out; }
	Mesh->ProcessMesh([&Out](const FDynamicMesh3& M)
	{
		if (!M.HasAttributes() || !M.Attributes()->HasBones()) { return; }
		const FDynamicMeshAttributeSet* Attributes = M.Attributes();
		const TArray<FTransform> MeshSpace = BoneMeshTransforms(Attributes);
		Out.SetNum(Attributes->GetNumBones());
		for (int32 i = 0; i < Out.Num(); ++i)
		{
			const int32 Parent = Attributes->GetBoneParentIndices()->GetValue(i);
			Out[i].Index = i;
			Out[i].Name = Attributes->GetBoneNames()->GetValue(i).ToString();
			Out[i].ParentName = (Parent >= 0 && Parent < Out.Num()) ? Attributes->GetBoneNames()->GetValue(Parent).ToString() : FString();
			Out[i].LocalTransform = Attributes->GetBonePoses()->GetValue(i);
			Out[i].MeshTransform = MeshSpace[i];
		}
		if (const FSkinWeights* Skin = Attributes->GetSkinWeightsAttribute(FSkeletalMeshAttributes::DefaultSkinWeightProfileName))
		{
			FBoneWeights Weights;
			for (const int32 Vid : M.VertexIndicesItr())
			{
				Skin->GetValue(Vid, Weights);
				for (int32 k = 0; k < Weights.Num(); ++k)
				{
					const int32 BoneIndex = Weights[k].GetBoneIndex();
					if (Weights[k].GetWeight() > 0.f && BoneIndex < Out.Num()) { ++Out[BoneIndex].InfluencedVertices; }
				}
			}
		}
	});
	return Out;
}

FModelingResult UModelingService::SetAssetMaterials(const FString& AssetPath, const FString& MaterialPaths, bool bSaveAsset)
{
	TArray<FString> Paths;
	MaterialPaths.ParseIntoArray(Paths, TEXT(","), true);
	TArray<UMaterialInterface*> Materials;
	for (FString& Path : Paths)
	{
		Path.TrimStartAndEndInline();
		if (Path.IsEmpty()) { continue; }
		UMaterialInterface* Material = LoadAssetAs<UMaterialInterface>(Path);
		if (!Material) { return Fail(-1, FString::Printf(TEXT("Material not found: %s"), *Path)); }
		Materials.Add(Material);
	}
	if (Materials.Num() == 0) { return Fail(-1, TEXT("MaterialPaths must list at least one material asset path (comma-separated, in slot order)")); }

	UObject* Asset = UEditorAssetLibrary::DoesAssetExist(AssetPath) ? UEditorAssetLibrary::LoadAsset(AssetPath) : nullptr;
	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset))
	{
		TArray<FStaticMaterial> Slots = StaticMesh->GetStaticMaterials();
		Slots.SetNum(FMath::Max(Slots.Num(), Materials.Num()));
		for (int32 i = 0; i < Materials.Num(); ++i)
		{
			Slots[i].MaterialInterface = Materials[i];
			if (Slots[i].MaterialSlotName.IsNone()) { Slots[i].MaterialSlotName = *FString::Printf(TEXT("Slot%d"), i); }
		}
		StaticMesh->SetStaticMaterials(Slots);
		StaticMesh->MarkPackageDirty();
		StaticMesh->PostEditChange();
	}
	else if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Asset))
	{
		TArray<FSkeletalMaterial> Slots = SkeletalMesh->GetMaterials();
		Slots.SetNum(FMath::Max(Slots.Num(), Materials.Num()));
		for (int32 i = 0; i < Materials.Num(); ++i)
		{
			Slots[i].MaterialInterface = Materials[i];
			if (Slots[i].MaterialSlotName.IsNone()) { Slots[i].MaterialSlotName = *FString::Printf(TEXT("Slot%d"), i); }
		}
		SkeletalMesh->SetMaterials(Slots);
		SkeletalMesh->MarkPackageDirty();
		SkeletalMesh->PostEditChange();
	}
	else
	{
		return Fail(-1, FString::Printf(TEXT("%s is not a StaticMesh or SkeletalMesh asset"), *AssetPath));
	}
	SaveIf(bSaveAsset, AssetPath);
	FModelingResult R;
	R.bSuccess = true;
	R.AssetPath = AssetPath;
	R.Message = FString::Printf(TEXT("Assigned %d material slot(s) on %s"), Materials.Num(), *AssetPath);
	return R;
}

// =========================================================================
// UV control
// =========================================================================

namespace
{
	FGeometryScriptGroupLayer DefaultGroupLayer()
	{
		FGeometryScriptGroupLayer Layer;
		Layer.bDefaultLayer = true;
		return Layer;
	}

	/** Case-insensitive enum lookup so "Polygroups" finds PolyGroups. */
	template <typename TEnum>
	bool ParseEnumLoose(const FString& Name, TEnum& Out)
	{
		const UEnum* E = StaticEnum<TEnum>();
		const FString Wanted = Name.TrimStartAndEnd();
		for (int32 i = 0; i < E->NumEnums() - 1; ++i)
		{
			if (E->GetNameStringByIndex(i).Equals(Wanted, ESearchCase::IgnoreCase))
			{
				Out = static_cast<TEnum>(E->GetValueByIndex(i));
				return true;
			}
		}
		return false;
	}

	int32 NumUVLayers(UDynamicMesh* Mesh)
	{
		int32 Num = 0;
		Mesh->ProcessMesh([&Num](const FDynamicMesh3& M) { Num = M.HasAttributes() ? M.Attributes()->NumUVLayers() : 0; });
		return Num;
	}

	FGeometryScriptMeshSelection SelectionForMaterial(UDynamicMesh* Mesh, int32 MaterialID)
	{
		TArray<int32> TriangleIDs;
		Mesh->ProcessMesh([&](const FDynamicMesh3& M)
		{
			if (!M.HasAttributes() || !M.Attributes()->HasMaterialID()) { return; }
			const FDynamicMeshMaterialAttribute* Materials = M.Attributes()->GetMaterialID();
			for (const int32 Tid : M.TriangleIndicesItr())
			{
				if (Materials->GetValue(Tid) == MaterialID) { TriangleIDs.Add(Tid); }
			}
		});
		FGeometryScriptMeshSelection Selection;
		UGeometryScriptLibrary_MeshSelectionFunctions::ConvertIndexArrayToMeshSelection(Mesh, TriangleIDs, EGeometryScriptMeshSelectionType::Triangles, Selection);
		return Selection;
	}
}

FModelingResult UModelingService::SetPolygroup(int32 Handle, const FString& SelectionName, int32 GroupID)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection;
	FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	Mesh->EditMesh([](FDynamicMesh3& M)
	{
		if (!M.HasTriangleGroups()) { M.EnableTriangleGroups(0); }
	});
	UGeometryScriptDebug* Debug = NewDebug();
	int32 SetID = 0;
	UGeometryScriptLibrary_MeshPolygroupFunctions::SetPolygroupForMeshSelection(Mesh, DefaultGroupLayer(), Selection, SetID, FMath::Max(0, GroupID), GroupID < 0, false);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Selection '%s' is now polygroup %d"), *SelectionName, SetID));
}

FModelingResult UModelingService::RecomputeUVs(int32 Handle, int32 UVLayer, const FString& IslandSource, const FString& Method, const FString& SelectionName, bool bAlignIslandsWithAxes)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection;
	FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	FGeometryScriptRecomputeUVsOptions Options;
	if (!ParseEnumLoose(IslandSource, Options.IslandSource))
	{
		return Fail(Handle, FString::Printf(TEXT("Unknown island source '%s' (use %s)"), *IslandSource, *EnumNames<EGeometryScriptUVIslandSource>()));
	}
	if (!ParseEnumLoose(Method, Options.Method))
	{
		return Fail(Handle, FString::Printf(TEXT("Unknown unwrap method '%s' (use %s)"), *Method, *EnumNames<EGeometryScriptUVFlattenMethod>()));
	}
	Options.GroupLayer = DefaultGroupLayer();
	Options.bAutoAlignIslandsWithAxes = bAlignIslandsWithAxes;
	if (UVLayer >= NumUVLayers(Mesh)) { SetNumUVLayers(Handle, UVLayer + 1); }
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshUVFunctions::RecomputeMeshUVs(Mesh, UVLayer, Options, Selection, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Recomputed UV layer %d from %s (%s) - islands are not packed yet, run layout_uv"), UVLayer, *IslandSource, *Method));
}

FModelingResult UModelingService::TransformUV(int32 Handle, int32 UVLayer, const FString& SelectionName, FVector2D Translation, FVector2D Scale, float RotationDeg, FVector2D Origin)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection;
	FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	if (UVLayer < 0 || UVLayer >= NumUVLayers(Mesh)) { return Fail(Handle, FString::Printf(TEXT("UV layer %d does not exist (mesh has %d)"), UVLayer, NumUVLayers(Mesh))); }
	UGeometryScriptDebug* Debug = NewDebug();
	if (!Scale.Equals(FVector2D(1, 1)))
	{
		UGeometryScriptLibrary_MeshUVFunctions::ScaleMeshUVs(Mesh, UVLayer, Scale, Origin, Selection, Debug);
	}
	if (!FMath::IsNearlyZero(RotationDeg))
	{
		UGeometryScriptLibrary_MeshUVFunctions::RotateMeshUVs(Mesh, UVLayer, RotationDeg, Origin, Selection, Debug);
	}
	if (!Translation.IsNearlyZero())
	{
		UGeometryScriptLibrary_MeshUVFunctions::TranslateMeshUVs(Mesh, UVLayer, Translation, Selection, Debug);
	}
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Transformed UVs of '%s' in layer %d"), *SelectionName, UVLayer));
}

FModelingResult UModelingService::LayoutUV(int32 Handle, int32 UVLayer, const FString& LayoutType, const FString& SelectionName, int32 TextureResolution, float Scale, FVector2D Translation)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	FGeometryScriptMeshSelection Selection;
	FString Error;
	if (!ResolveSelection(Handle, SelectionName, Mesh, Selection, Error)) { return Fail(Handle, Error); }
	if (UVLayer < 0 || UVLayer >= NumUVLayers(Mesh)) { return Fail(Handle, FString::Printf(TEXT("UV layer %d does not exist (mesh has %d)"), UVLayer, NumUVLayers(Mesh))); }
	FGeometryScriptLayoutUVsOptions Options;
	if (!ParseEnumLoose(LayoutType, Options.LayoutType))
	{
		return Fail(Handle, FString::Printf(TEXT("Unknown layout type '%s' (use %s)"), *LayoutType, *EnumNames<EGeometryScriptUVLayoutType>()));
	}
	Options.TextureResolution = FMath::Clamp(TextureResolution, 16, 16384);
	Options.Scale = Scale;
	Options.Translation = Translation;
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshUVFunctions::LayoutMeshUVs(Mesh, UVLayer, Options, Selection, Debug);
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("%s layout of UV layer %d"), *LayoutType, UVLayer));
}

FModelingResult UModelingService::PackUVPerMaterial(int32 Handle, int32 UVLayer, int32 TextureResolution)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (UVLayer < 0 || UVLayer >= NumUVLayers(Mesh)) { return Fail(Handle, FString::Printf(TEXT("UV layer %d does not exist (mesh has %d)"), UVLayer, NumUVLayers(Mesh))); }
	TSet<int32> MaterialIDs;
	Mesh->ProcessMesh([&MaterialIDs](const FDynamicMesh3& M)
	{
		if (!M.HasAttributes() || !M.Attributes()->HasMaterialID()) { return; }
		for (const int32 Tid : M.TriangleIndicesItr()) { MaterialIDs.Add(M.Attributes()->GetMaterialID()->GetValue(Tid)); }
	});
	if (MaterialIDs.Num() == 0) { return Fail(Handle, TEXT("Mesh has no material IDs (set_material_id first) - use layout_uv for a single-material pack")); }
	FGeometryScriptLayoutUVsOptions Options;
	Options.LayoutType = EGeometryScriptUVLayoutType::Repack;
	Options.TextureResolution = FMath::Clamp(TextureResolution, 16, 16384);
	UGeometryScriptDebug* Debug = NewDebug();
	for (const int32 MaterialID : MaterialIDs)
	{
		UGeometryScriptLibrary_MeshUVFunctions::LayoutMeshUVs(Mesh, UVLayer, Options, SelectionForMaterial(Mesh, MaterialID), Debug);
	}
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Packed UV layer %d separately for %d material ID(s) - each slot now uses the full 0-1 range"), UVLayer, MaterialIDs.Num()));
}

FModelingUVStats UModelingService::GetUVStats(int32 Handle, int32 UVLayer, int32 GridResolution)
{
	FModelingUVStats Stats;
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { Stats.Message = NoMesh(Handle).Message; return Stats; }
	if (UVLayer < 0 || UVLayer >= NumUVLayers(Mesh)) { Stats.Message = FString::Printf(TEXT("UV layer %d does not exist (mesh has %d)"), UVLayer, NumUVLayers(Mesh)); return Stats; }

	FGeometryScriptMeshSelection All;
	UGeometryScriptLibrary_MeshSelectionFunctions::CreateSelectAllMeshSelection(Mesh, All, EGeometryScriptMeshSelectionType::Triangles);
	double MeshArea = 0.0, UVArea = 0.0;
	FBox MeshBounds(ForceInit);
	FBox2D UVBounds(ForceInit);
	bool bValid = false, bUnset = false;
	UGeometryScriptLibrary_MeshUVFunctions::GetMeshUVSizeInfo(Mesh, UVLayer, All, MeshArea, UVArea, MeshBounds, UVBounds, bValid, bUnset, true, nullptr);
	Stats.MeshArea = static_cast<float>(MeshArea);
	Stats.UVArea = static_cast<float>(UVArea);
	Stats.UVBoundsMin = UVBounds.Min;
	Stats.UVBoundsMax = UVBounds.Max;
	Stats.TexelsPerCmAt1K = MeshArea > 0.0 ? static_cast<float>(FMath::Sqrt(UVArea / MeshArea) * 1024.0) : 0.f;

	const int32 Res = FMath::Clamp(GridResolution, 16, 4096);
	TArray<uint8> Hits;
	Hits.SetNumZeroed(Res * Res);
	Mesh->ProcessMesh([&](const FDynamicMesh3& M)
	{
		const FDynamicMeshUVOverlay* UV = M.Attributes()->GetUVLayer(UVLayer);
		if (!UV) { return; }
		FMeshConnectedComponents Components(&M);
		Components.FindConnectedTriangles([UV](int32 T0, int32 T1) { return UV->AreTrianglesConnected(T0, T1); });
		Stats.NumIslands = Components.Num();
		for (const int32 Tid : M.TriangleIndicesItr())
		{
			if (!UV->IsSetTriangle(Tid)) { ++Stats.NumUnsetTriangles; continue; }
			FVector2f A, B, C;
			UV->GetTriElements(Tid, A, B, C);
			const FVector2f P0 = A * Res, P1 = B * Res, P2 = C * Res;
			const int32 X0 = FMath::Clamp(FMath::FloorToInt(FMath::Min3(P0.X, P1.X, P2.X)), 0, Res - 1), X1 = FMath::Clamp(FMath::CeilToInt(FMath::Max3(P0.X, P1.X, P2.X)), 0, Res - 1);
			const int32 Y0 = FMath::Clamp(FMath::FloorToInt(FMath::Min3(P0.Y, P1.Y, P2.Y)), 0, Res - 1), Y1 = FMath::Clamp(FMath::CeilToInt(FMath::Max3(P0.Y, P1.Y, P2.Y)), 0, Res - 1);
			const float Area = (P1.X - P0.X) * (P2.Y - P0.Y) - (P2.X - P0.X) * (P1.Y - P0.Y);
			if (FMath::Abs(Area) < 1e-6f) { continue; }
			for (int32 Y = Y0; Y <= Y1; ++Y)
			{
				for (int32 X = X0; X <= X1; ++X)
				{
					const FVector2f P(X + 0.5f, Y + 0.5f);
					const float W0 = ((P1.X - P.X) * (P2.Y - P.Y) - (P2.X - P.X) * (P1.Y - P.Y)) / Area;
					const float W1 = ((P2.X - P.X) * (P0.Y - P.Y) - (P0.X - P.X) * (P2.Y - P.Y)) / Area;
					const float W2 = 1.f - W0 - W1;
					if (W0 >= 0.f && W1 >= 0.f && W2 >= 0.f)
					{
						uint8& Cell = Hits[Y * Res + X];
						if (Cell < 255) { ++Cell; }
					}
				}
			}
		}
	});
	int32 Covered = 0, Overlapped = 0;
	for (const uint8 Cell : Hits)
	{
		if (Cell > 0) { ++Covered; }
		if (Cell > 1) { ++Overlapped; }
	}
	Stats.Coverage = static_cast<float>(Covered) / static_cast<float>(Res * Res);
	Stats.OverlapFraction = Covered > 0 ? static_cast<float>(Overlapped) / static_cast<float>(Covered) : 0.f;
	Stats.bSuccess = true;
	Stats.Message = FString::Printf(TEXT("%d island(s), %.0f%% of 0-1 covered, %.1f%% overlapping, %d unset triangle(s), %.2f texels/cm at 1K"),
		Stats.NumIslands, Stats.Coverage * 100.f, Stats.OverlapFraction * 100.f, Stats.NumUnsetTriangles, Stats.TexelsPerCmAt1K);
	return Stats;
}

bool UModelingService::WorldToUV(int32 Handle, FVector Point, int32 UVLayer, FVector2D& OutUV)
{
	OutUV = FVector2D::ZeroVector;
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh || UVLayer < 0 || UVLayer >= NumUVLayers(Mesh)) { return false; }
	UGeometryScriptDebug* Debug = NewDebug();
	FGeometryScriptDynamicMeshBVH BVH;
	UGeometryScriptLibrary_MeshSpatial::BuildBVHForMesh(Mesh, BVH, Debug);
	FGeometryScriptTrianglePoint Nearest;
	EGeometryScriptSearchOutcomePins Outcome = EGeometryScriptSearchOutcomePins::NotFound;
	UGeometryScriptLibrary_MeshSpatial::FindNearestPointOnMesh(Mesh, BVH, Point, FGeometryScriptSpatialQueryOptions(), Nearest, Outcome, Debug);
	if (Outcome != EGeometryScriptSearchOutcomePins::Found || !Nearest.bValid) { return false; }
	bool bFound = false;
	Mesh->ProcessMesh([&](const FDynamicMesh3& M)
	{
		const FDynamicMeshUVOverlay* UV = M.Attributes()->GetUVLayer(UVLayer);
		if (!UV || !UV->IsSetTriangle(Nearest.TriangleID)) { return; }
		FVector2f A, B, C;
		UV->GetTriElements(Nearest.TriangleID, A, B, C);
		const FVector2f Result = A * static_cast<float>(Nearest.BaryCoords.X) + B * static_cast<float>(Nearest.BaryCoords.Y) + C * static_cast<float>(Nearest.BaryCoords.Z);
		OutUV = FVector2D(Result.X, Result.Y);
		bFound = true;
	});
	return bFound;
}

// =========================================================================
// Image tools
// =========================================================================

namespace
{
	bool ParseCompression(const FString& Name, TextureCompressionSettings& Out)
	{
		const FString N = Name.TrimStartAndEnd();
		if (N.Equals(TEXT("Default"), ESearchCase::IgnoreCase)) { Out = TC_Default; return true; }
		if (N.Equals(TEXT("Normalmap"), ESearchCase::IgnoreCase)) { Out = TC_Normalmap; return true; }
		if (N.Equals(TEXT("Masks"), ESearchCase::IgnoreCase)) { Out = TC_Masks; return true; }
		if (N.Equals(TEXT("Grayscale"), ESearchCase::IgnoreCase)) { Out = TC_Grayscale; return true; }
		if (N.Equals(TEXT("HDR"), ESearchCase::IgnoreCase)) { Out = TC_HDR; return true; }
		return false;
	}

	/** Perlin fBm that wraps at the image borders (blend of the four torus-shifted lookups). */
	float TileableNoise(float U, float V, float Cells, const FVector2D& Offset)
	{
		const FVector2D P(U * Cells, V * Cells);
		const float N00 = FMath::PerlinNoise2D(P + Offset);
		const float N10 = FMath::PerlinNoise2D(P - FVector2D(Cells, 0) + Offset);
		const float N01 = FMath::PerlinNoise2D(P - FVector2D(0, Cells) + Offset);
		const float N11 = FMath::PerlinNoise2D(P - FVector2D(Cells, Cells) + Offset);
		return FMath::Lerp(FMath::Lerp(N00, N10, U), FMath::Lerp(N01, N11, U), V);
	}

	void FinishTextureEdit(UTexture2D* Texture, bool bSaveAsset)
	{
		Texture->UpdateResource();
		Texture->PostEditChange();
		Texture->MarkPackageDirty();
		Texture->BlockOnAnyAsyncBuild();
		SaveIf(bSaveAsset, Texture->GetPathName());
	}
}

FModelingResult UModelingService::ImportTexture(const FString& FilePath, const FString& AssetPath, bool bSRGB, const FString& Compression, bool bSaveAsset)
{
	if (!FPaths::FileExists(FilePath)) { return Fail(-1, FString::Printf(TEXT("File not found: %s"), *FilePath)); }
	if (!AssetPath.StartsWith(TEXT("/"))) { return Fail(-1, FString::Printf(TEXT("AssetPath must be a content path like /Game/Textures/T_Name (got '%s')"), *AssetPath)); }
	TextureCompressionSettings Settings = TC_Default;
	if (!ParseCompression(Compression, Settings)) { return Fail(-1, FString::Printf(TEXT("Unknown compression '%s' (use Default, Normalmap, Masks, Grayscale, HDR)"), *Compression)); }

	UAssetImportTask* Task = NewObject<UAssetImportTask>();
	Task->Filename = FilePath;
	Task->DestinationPath = FPackageName::GetLongPackagePath(AssetPath);
	Task->DestinationName = FPackageName::GetLongPackageAssetName(AssetPath);
	Task->bAutomated = true;
	Task->bReplaceExisting = true;
	Task->bSave = false;
	FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().ImportAssetTasks({ Task });
	UTexture2D* Texture = Task->GetObjects().Num() > 0 ? Cast<UTexture2D>(Task->GetObjects()[0]) : nullptr;
	if (!Texture) { return Fail(-1, FString::Printf(TEXT("Import of %s produced no Texture2D (unsupported format?)"), *FilePath)); }
	Texture->SRGB = bSRGB;
	Texture->CompressionSettings = Settings;
	if (Settings == TC_Normalmap)
	{
		Texture->SRGB = false;
		Texture->LODGroup = TEXTUREGROUP_WorldNormalMap;
	}
	FinishTextureEdit(Texture, bSaveAsset);
	FModelingResult R;
	R.bSuccess = true;
	R.AssetPath = Texture->GetPathName();
	R.Message = FString::Printf(TEXT("Imported %s as %s (%dx%d, %s, sRGB %s)"), *FPaths::GetCleanFilename(FilePath), *R.AssetPath, Texture->GetSizeX(), Texture->GetSizeY(), *Compression, bSRGB ? TEXT("on") : TEXT("off"));
	return R;
}

FModelingResult UModelingService::CreateNoiseTexture(const FString& AssetPath, int32 Width, int32 Height, float Scale, int32 Octaves, float Persistence, int32 Seed, bool bSaveAsset)
{
	if (!AssetPath.StartsWith(TEXT("/"))) { return Fail(-1, FString::Printf(TEXT("AssetPath must be a content path like /Game/Textures/T_Name (got '%s')"), *AssetPath)); }
	const int32 W = FMath::Clamp(Width, 4, 8192), H = FMath::Clamp(Height, 4, 8192);
	const int32 NumOctaves = FMath::Clamp(Octaves, 1, 10);
	const float Cells = FMath::Max(1.f, Scale);
	FRandomStream Random(Seed);
	const FVector2D Offset(Random.FRandRange(0.f, 1000.f), Random.FRandRange(0.f, 1000.f));

	TArray<uint8> Pixels;
	Pixels.SetNumUninitialized(W * H * 4);
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			float Value = 0.f, Amplitude = 1.f, Total = 0.f, Frequency = Cells;
			for (int32 O = 0; O < NumOctaves; ++O)
			{
				Value += Amplitude * TileableNoise(static_cast<float>(X) / W, static_cast<float>(Y) / H, Frequency, Offset * (O + 1));
				Total += Amplitude;
				Amplitude *= Persistence;
				Frequency *= 2.f;
			}
			const uint8 Gray = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt((0.5f + 0.5f * Value / Total) * 255.f), 0, 255));
			uint8* P = &Pixels[(Y * W + X) * 4];
			P[0] = Gray; P[1] = Gray; P[2] = Gray; P[3] = 255;
		}
	}

	UTexture2D* Texture = UEditorAssetLibrary::DoesAssetExist(AssetPath) ? LoadAssetAs<UTexture2D>(AssetPath) : nullptr;
	if (!Texture)
	{
		if (UEditorAssetLibrary::DoesAssetExist(AssetPath)) { return Fail(-1, FString::Printf(TEXT("%s exists and is not a Texture2D"), *AssetPath)); }
		UPackage* Package = CreatePackage(*AssetPath);
		Texture = NewObject<UTexture2D>(Package, *FPackageName::GetLongPackageAssetName(AssetPath), RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Texture);
	}
	Texture->Source.Init(W, H, 1, 1, TSF_BGRA8, Pixels.GetData());
	Texture->SRGB = false;
	Texture->CompressionSettings = TC_Grayscale;
	FinishTextureEdit(Texture, bSaveAsset);
	FModelingResult R;
	R.bSuccess = true;
	R.AssetPath = Texture->GetPathName();
	R.Message = FString::Printf(TEXT("Noise texture %dx%d (%d octaves, %.0f cells, seed %d) at %s"), W, H, NumOctaves, Cells, Seed, *R.AssetPath);
	return R;
}

FModelingResult UModelingService::DrawOnTexture(const FString& AssetPath, const FString& Shape, const TArray<FVector2D>& Points, FLinearColor Color, float ThicknessPx, float SpacingPx, bool bSaveAsset)
{
	UTexture2D* Texture = LoadAssetAs<UTexture2D>(AssetPath);
	if (!Texture) { return Fail(-1, FString::Printf(TEXT("Texture2D not found: %s"), *AssetPath)); }
	FTextureSource& Source = Texture->Source;
	if (!Source.IsValid()) { return Fail(-1, FString::Printf(TEXT("%s has no editable source data"), *AssetPath)); }
	const ETextureSourceFormat Format = Source.GetFormat();
	if (Format != TSF_BGRA8 && Format != TSF_G8)
	{
		return Fail(-1, FString::Printf(TEXT("%s is not an 8-bit texture (BGRA8 or G8); draw into an imported PNG or a create_noise_texture result"), *AssetPath));
	}
	const FString Kind = Shape.TrimStartAndEnd();
	const bool bLine = Kind.Equals(TEXT("Line"), ESearchCase::IgnoreCase), bDots = Kind.Equals(TEXT("Dots"), ESearchCase::IgnoreCase),
		bRect = Kind.Equals(TEXT("Rect"), ESearchCase::IgnoreCase), bFill = Kind.Equals(TEXT("Fill"), ESearchCase::IgnoreCase);
	if (!bLine && !bDots && !bRect && !bFill) { return Fail(-1, FString::Printf(TEXT("Unknown shape '%s' (use Line, Dots, Rect, Fill)"), *Shape)); }
	if ((bLine || bDots) && Points.Num() < 2) { return Fail(-1, TEXT("Line and Dots need at least 2 points")); }
	if (bRect && Points.Num() < 2) { return Fail(-1, TEXT("Rect needs 2 points (opposite corners)")); }

	const int32 W = static_cast<int32>(Source.GetSizeX()), H = static_cast<int32>(Source.GetSizeY());
	const FColor Pixel = Color.ToFColor(Texture->SRGB);
	const uint8 Gray = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(0.299f * Pixel.R + 0.587f * Pixel.G + 0.114f * Pixel.B), 0, 255));
	uint8* Data = Source.LockMip(0, 0, 0);
	if (!Data) { return Fail(-1, FString::Printf(TEXT("Could not lock %s for writing"), *AssetPath)); }
	int64 Painted = 0;
	auto Plot = [&](int32 X, int32 Y)
	{
		if (X < 0 || Y < 0 || X >= W || Y >= H) { return; }
		if (Format == TSF_BGRA8)
		{
			uint8* P = Data + (static_cast<int64>(Y) * W + X) * 4;
			P[0] = Pixel.B; P[1] = Pixel.G; P[2] = Pixel.R; P[3] = Pixel.A;
		}
		else
		{
			Data[static_cast<int64>(Y) * W + X] = Gray;
		}
		++Painted;
	};
	auto Disc = [&](const FVector2D& Center, float Radius)
	{
		const float R = FMath::Max(0.5f, Radius);
		for (int32 Y = FMath::FloorToInt(Center.Y - R); Y <= FMath::CeilToInt(Center.Y + R); ++Y)
		{
			for (int32 X = FMath::FloorToInt(Center.X - R); X <= FMath::CeilToInt(Center.X + R); ++X)
			{
				if (FVector2D::DistSquared(FVector2D(X + 0.5f, Y + 0.5f), Center) <= R * R) { Plot(X, Y); }
			}
		}
	};
	auto ToPixels = [&](const FVector2D& UV) { return FVector2D(UV.X * W, UV.Y * H); };

	if (bFill)
	{
		for (int32 Y = 0; Y < H; ++Y) { for (int32 X = 0; X < W; ++X) { Plot(X, Y); } }
	}
	else if (bRect)
	{
		const FVector2D A = ToPixels(Points[0]), B = ToPixels(Points[1]);
		for (int32 Y = FMath::FloorToInt(FMath::Min(A.Y, B.Y)); Y < FMath::CeilToInt(FMath::Max(A.Y, B.Y)); ++Y)
		{
			for (int32 X = FMath::FloorToInt(FMath::Min(A.X, B.X)); X < FMath::CeilToInt(FMath::Max(A.X, B.X)); ++X) { Plot(X, Y); }
		}
	}
	else
	{
		const float Radius = FMath::Max(0.5f, ThicknessPx * 0.5f);
		const float Step = bDots ? FMath::Max(1.f, SpacingPx) : 0.5f;
		float Carry = 0.f;   // distance already travelled since the last stamp, carried across segments
		bool bFirst = true;
		for (int32 i = 0; i + 1 < Points.Num(); ++i)
		{
			const FVector2D A = ToPixels(Points[i]), B = ToPixels(Points[i + 1]);
			const float Length = FVector2D::Distance(A, B);
			if (Length < KINDA_SMALL_NUMBER) { continue; }
			const FVector2D Dir = (B - A) / Length;
			float T = bFirst ? 0.f : Step - Carry;
			bFirst = false;
			for (; T <= Length; T += Step) { Disc(A + Dir * T, Radius); }
			Carry = Length - (T - Step);
		}
		if (bLine) { Disc(ToPixels(Points.Last()), Radius); }
	}
	Source.UnlockMip(0, 0, 0);
	FinishTextureEdit(Texture, bSaveAsset);
	FModelingResult R;
	R.bSuccess = true;
	R.AssetPath = Texture->GetPathName();
	R.Message = FString::Printf(TEXT("Drew %s on %s (%lld pixel writes)"), *Kind, *AssetPath, Painted);
	return R;
}

// =========================================================================
// Surface detail helpers
// =========================================================================

FModelingResult UModelingService::AppendMeshAtTransforms(int32 Handle, int32 OtherHandle, const TArray<FTransform>& Transforms)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	UDynamicMesh* Other = FindMesh(OtherHandle);
	if (!Other) { return NoMesh(OtherHandle); }
	if (Handle == OtherHandle) { return Fail(Handle, TEXT("A mesh cannot be stamped into itself - copy_mesh it first")); }
	if (Transforms.Num() == 0) { return Fail(Handle, TEXT("Transforms is empty")); }
	UGeometryScriptDebug* Debug = NewDebug();
	for (const FTransform& T : Transforms)
	{
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(Mesh, Other, T, true, FGeometryScriptAppendMeshOptions(), Debug);
	}
	return Finish(Handle, Mesh, Debug, FString::Printf(TEXT("Stamped handle %d at %d transform(s)"), OtherHandle, Transforms.Num()));
}

FModelingResult UModelingService::AppendMeshAlongPolyline(int32 Handle, int32 OtherHandle, const TArray<FVector>& Points, float Spacing, FVector UpVector, float Scale)
{
	if (Points.Num() < 2) { return Fail(Handle, TEXT("Points needs at least 2 positions")); }
	if (Spacing <= 0.f) { return Fail(Handle, TEXT("Spacing must be positive")); }
	const FVector Up = UpVector.IsNearlyZero() ? FVector::UpVector : UpVector.GetSafeNormal();
	TArray<FTransform> Transforms;
	float Carry = 0.f;
	bool bFirst = true;
	for (int32 i = 0; i + 1 < Points.Num(); ++i)
	{
		const float Length = static_cast<float>(FVector::Distance(Points[i], Points[i + 1]));
		if (Length < KINDA_SMALL_NUMBER) { continue; }
		const FVector Dir = (Points[i + 1] - Points[i]) / Length;
		const FQuat Rotation = FRotationMatrix::MakeFromXZ(Dir, Up).ToQuat();
		float T = bFirst ? 0.f : Spacing - Carry;
		bFirst = false;
		for (; T <= Length + KINDA_SMALL_NUMBER; T += Spacing)
		{
			Transforms.Add(FTransform(Rotation, Points[i] + Dir * T, FVector(Scale)));
		}
		Carry = Length - (T - Spacing);
	}
	if (Transforms.Num() == 0) { return Fail(Handle, TEXT("Polyline is shorter than one Spacing")); }
	FModelingResult R = AppendMeshAtTransforms(Handle, OtherHandle, Transforms);
	if (R.bSuccess) { R.Message = FString::Printf(TEXT("Stamped handle %d %d time(s) along a %d-point polyline every %.1f"), OtherHandle, Transforms.Num(), Points.Num(), Spacing); }
	return R;
}

FModelingResult UModelingService::CutGrooveAlongPolyline(int32 Handle, const TArray<FVector>& Points, float Width, float Depth, FVector UpVector)
{
	UDynamicMesh* Mesh = FindMesh(Handle);
	if (!Mesh) { return NoMesh(Handle); }
	if (Points.Num() < 2) { return Fail(Handle, TEXT("Points needs at least 2 positions")); }
	if (Width <= 0.f || Depth <= 0.f) { return Fail(Handle, TEXT("Width and Depth must be positive")); }
	const FVector Up = UpVector.IsNearlyZero() ? FVector::UpVector : UpVector.GetSafeNormal();

	// One frame per point, X along the path (averaged at interior corners), Z along Up; the profile lives in the frame's YZ plane.
	TArray<FTransform> Frames;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		FVector Dir = FVector::ZeroVector;
		if (i > 0) { Dir += (Points[i] - Points[i - 1]).GetSafeNormal(); }
		if (i + 1 < Points.Num()) { Dir += (Points[i + 1] - Points[i]).GetSafeNormal(); }
		if (Dir.IsNearlyZero()) { continue; }
		Frames.Add(FTransform(FRotationMatrix::MakeFromXZ(Dir.GetSafeNormal(), Up).ToQuat(), Points[i]));
	}
	if (Frames.Num() < 2) { return Fail(Handle, TEXT("Polyline has fewer than 2 distinct points")); }
	const TArray<FVector2D> Profile = { FVector2D(-Width * 0.5, -Depth), FVector2D(Width * 0.5, -Depth), FVector2D(Width * 0.5, Depth), FVector2D(-Width * 0.5, Depth) };

	int32 ToolHandle = -1;
	UDynamicMesh* Tool = NewSessionMesh(ToolHandle);
	UGeometryScriptDebug* Debug = NewDebug();
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSweepPolygon(Tool, PrimitiveOptions(0), FTransform::Identity, Profile, Frames, false, true, 1.f, 1.f, 0.f, 1.f, Debug);
	float Area = 0.f, Volume = 0.f;
	UGeometryScriptLibrary_MeshQueryFunctions::GetMeshVolumeArea(Tool, Area, Volume);
	if (Volume < 0.f) { UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(Tool, Debug); }
	FModelingResult R = Boolean(Handle, ToolHandle, TEXT("Subtract"), FTransform::Identity, true, true);
	ReleaseMesh(ToolHandle);
	if (R.bSuccess) { R.Message = FString::Printf(TEXT("Cut a %.1f x %.1f groove along a %d-point polyline"), Width, Depth, Points.Num()); }
	return R;
}
