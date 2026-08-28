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
			if (M.MessageType == EGeometryScriptDebugMessageType::ErrorMessage) { return true; }
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
	USkeleton* Skeleton = LoadAssetAs<USkeleton>(SkeletonPath);
	if (!Skeleton) { return Fail(Handle, FString::Printf(TEXT("Skeleton not found: %s"), *SkeletonPath)); }

	UGeometryScriptDebug* Debug = NewDebug();
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	USkeletalMesh* SkeletalMesh = nullptr;
	FString What;
	if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
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
		SkeletalMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewSkeletalMeshAssetFromMesh(Mesh, Skeleton, AssetPath, Options, Outcome, Debug);
		What = FString::Printf(TEXT("Created %s"), *AssetPath);
	}
	if (Outcome != EGeometryScriptOutcomePins::Success || !SkeletalMesh)
	{
		return Fail(Handle, FString::Printf(TEXT("Saving to %s failed: %s"), *AssetPath, *DebugText(Debug)));
	}
	SaveIf(bSaveAsset, SkeletalMesh->GetPathName());
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
		else { Result.Message = FString::Printf(TEXT("Unknown bake type '%s' (use TangentNormal, ObjectNormal, FaceNormal, BentNormal, AmbientOcclusion, Curvature, Position)"), *Name); return Result; }
	}
	if (Types.Num() == 0) { Result.Message = TEXT("No bake types given"); return Result; }

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
		UEditorAssetLibrary::SaveAsset(Saved->GetPathName(), false);
		Result.TexturePaths.Add(Saved->GetPathName());
	}
	Result.bSuccess = true;
	Result.Message = FString::Printf(TEXT("Baked %d texture(s) at %d"), Result.TexturePaths.Num(), Resolution);
	return Result;
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
