// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UPIEActorService.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// =============================================================================
// Internal state and helpers (file-local)
// =============================================================================

namespace
{
	/** A single actor this service spawned, plus the session it belongs to. */
	struct FSpawnedRecord
	{
		TWeakObjectPtr<AActor> Actor;
		FString ClassPath;
		FString WorldPath;
		FDateTime SpawnTime = FDateTime::UtcNow();
		int32 Serial = 0;
	};

	/** Handle ("<serial>:<guid>") -> record, for the current and (until EndPIE) prior sessions. */
	TMap<FString, FSpawnedRecord> GSpawnRegistry;

	/** Bumped on every EndPIE so handles from an earlier session are detectable as stale. */
	int32 GSessionSerial = 0;

	/** Lazily-registered EndPIE hook (services are static classes with no module hook of their own). */
	bool GEndPIEHookRegistered = false;
	FDelegateHandle GEndPIEHandle;

	void HandleEndPIE(const bool /*bIsSimulating*/)
	{
		++GSessionSerial;
		GSpawnRegistry.Empty();
	}

	void EnsureEndPIEHook()
	{
		if (GEndPIEHookRegistered)
		{
			return;
		}
		GEndPIEHookRegistered = true;
		GEndPIEHandle = FEditorDelegates::EndPIE.AddStatic(&HandleEndPIE);
	}

	/** A parsed selector: an explicit PIE instance, or "the single client" (bClientAny). */
	struct FSelectorRequest
	{
		int32 Instance = INDEX_NONE;
		bool bClientAny = false;
	};

	// ---- JSON helpers --------------------------------------------------------

	FString SerializeJson(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	FString ErrorJson(const FString& Code, const FString& Message)
	{
		const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetBoolField(TEXT("success"), false);
		Obj->SetStringField(TEXT("error_code"), Code);
		Obj->SetStringField(TEXT("message"), Message);
		return SerializeJson(Obj);
	}

	FString OkJson(const TSharedRef<FJsonObject>& Obj)
	{
		Obj->SetBoolField(TEXT("success"), true);
		return SerializeJson(Obj);
	}

	FString NetModeToString(ENetMode Mode)
	{
		switch (Mode)
		{
		case NM_Standalone:      return TEXT("Standalone");
		case NM_ListenServer:    return TEXT("ListenServer");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_Client:          return TEXT("Client");
		default:                 return TEXT("Unknown");
		}
	}

	// ---- Selector parsing (grammar only; no world lookup) --------------------

	/** Returns "" when the selector is well-formed (Out filled); otherwise "INVALID_SELECTOR". */
	FString ParseSelector(const FString& Selector, FSelectorRequest& Out)
	{
		const FString S = Selector.TrimStartAndEnd();
		if (S.IsEmpty())
		{
			return TEXT("INVALID_SELECTOR");
		}

		if (S.Equals(TEXT("server"), ESearchCase::IgnoreCase))
		{
			Out.Instance = 0;
			Out.bClientAny = false;
			return FString();
		}
		if (S.Equals(TEXT("client"), ESearchCase::IgnoreCase))
		{
			Out.bClientAny = true;
			return FString();
		}

		FString Prefix;
		FString NumberPart;
		if (S.Split(TEXT(":"), &Prefix, &NumberPart))
		{
			Prefix = Prefix.TrimStartAndEnd();
			NumberPart = NumberPart.TrimStartAndEnd();

			// Strict: the index must be a plain run of digits — no sign, decimal point, or exponent.
			// (This rejects "instance:-1", "client:1.5", etc.)
			bool bAllDigits = !NumberPart.IsEmpty();
			for (const TCHAR Ch : NumberPart)
			{
				if (!FChar::IsDigit(Ch))
				{
					bAllDigits = false;
					break;
				}
			}
			if (!bAllDigits)
			{
				return TEXT("INVALID_SELECTOR");
			}
			const int32 N = FCString::Atoi(*NumberPart);

			if (Prefix.Equals(TEXT("client"), ESearchCase::IgnoreCase))
			{
				if (N < 1)
				{
					return TEXT("INVALID_SELECTOR");
				}
				Out.Instance = N;
				Out.bClientAny = false;
				return FString();
			}
			if (Prefix.Equals(TEXT("instance"), ESearchCase::IgnoreCase))
			{
				// N >= 0 is guaranteed by the all-digits check above.
				Out.Instance = N;
				Out.bClientAny = false;
				return FString();
			}
		}

		return TEXT("INVALID_SELECTOR");
	}

	// ---- Collision policy ----------------------------------------------------

	/** Returns "" when the name is a known policy (Out filled); otherwise "INVALID_COLLISION_HANDLING". */
	FString ValidateCollision(const FString& In, ESpawnActorCollisionHandlingMethod& Out)
	{
		if (In.Equals(TEXT("AlwaysSpawn"), ESearchCase::IgnoreCase))
		{
			Out = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return FString();
		}
		if (In.Equals(TEXT("AdjustIfPossibleButAlwaysSpawn"), ESearchCase::IgnoreCase))
		{
			Out = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			return FString();
		}
		if (In.Equals(TEXT("AdjustIfPossibleButDontSpawnIfColliding"), ESearchCase::IgnoreCase))
		{
			Out = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
			return FString();
		}
		if (In.Equals(TEXT("DontSpawnIfColliding"), ESearchCase::IgnoreCase))
		{
			Out = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
			return FString();
		}
		return TEXT("INVALID_COLLISION_HANDLING");
	}

	// ---- Transform -----------------------------------------------------------

	bool IsTransformFinite(const FTransform& T)
	{
		const FVector Loc = T.GetLocation();
		const FQuat Rot = T.GetRotation();
		const FVector Scale = T.GetScale3D();
		return FMath::IsFinite(Loc.X) && FMath::IsFinite(Loc.Y) && FMath::IsFinite(Loc.Z)
			&& FMath::IsFinite(Rot.X) && FMath::IsFinite(Rot.Y) && FMath::IsFinite(Rot.Z) && FMath::IsFinite(Rot.W)
			&& FMath::IsFinite(Scale.X) && FMath::IsFinite(Scale.Y) && FMath::IsFinite(Scale.Z);
	}

	// ---- Class resolution ----------------------------------------------------

	/**
	 * Resolve and validate an actor class. Uses LoadObject/LoadClass (NOT EditorAssetLibrary, which
	 * returns null during PIE). Compiles a UBlueprint when its generated class is missing or dirty,
	 * then re-reads GeneratedClass. Returns null with OutError set to a distinct code on rejection.
	 */
	UClass* ResolveActorClass(const FString& ClassPath, FString& OutResolvedPath, FString& OutError)
	{
		OutError.Reset();
		OutResolvedPath.Reset();

		const FString Trimmed = ClassPath.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			OutError = TEXT("CLASS_NOT_FOUND");
			return nullptr;
		}

		// Package-only paths (/Game/Foo/BP_X, /Script/Engine.StaticMeshActor already carries a dot)
		// get the primary-object suffix appended, mirroring WidgetService::SpawnWidgetInPIE.
		FString ObjectPath = Trimmed;
		if (!ObjectPath.Contains(TEXT(".")))
		{
			const FString ShortName = FPackageName::GetShortName(Trimmed);
			ObjectPath = FString::Printf(TEXT("%s.%s"), *Trimmed, *ShortName);
		}

		UClass* Candidate = nullptr;

		// LOAD_NoWarn | LOAD_Quiet: a bad class path is a normal, reported outcome here
		// (CLASS_NOT_FOUND) — it must not spam the log or trip the automation "no Error line" rule.
		const uint32 LoadFlags = LOAD_NoWarn | LOAD_Quiet;

		// Native class path or an explicit generated-class ("..._C") path resolves straight to a UClass.
		if (UClass* AsClass = LoadObject<UClass>(nullptr, *ObjectPath, nullptr, LoadFlags))
		{
			Candidate = AsClass;
		}
		else if (UObject* Obj = LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LoadFlags))
		{
			if (UClass* ObjClass = Cast<UClass>(Obj))
			{
				Candidate = ObjClass;
			}
			else if (UBlueprint* Blueprint = Cast<UBlueprint>(Obj))
			{
				if (!Blueprint->GeneratedClass || Blueprint->Status == BS_Dirty)
				{
					FKismetEditorUtilities::CompileBlueprint(Blueprint);
				}
				// Re-read after compile — never cache the pre-compile pointer.
				Candidate = Blueprint->GeneratedClass;
			}
		}

		if (!Candidate)
		{
			OutError = TEXT("CLASS_NOT_FOUND");
			return nullptr;
		}

		if (!Candidate->IsChildOf(AActor::StaticClass()))
		{
			OutError = TEXT("NOT_AN_ACTOR_CLASS");
			return nullptr;
		}

		const FString Name = Candidate->GetName();
		if (Candidate->HasAnyClassFlags(CLASS_NewerVersionExists)
			|| Name.StartsWith(TEXT("REINST_")) || Name.StartsWith(TEXT("SKEL_")) || Name.StartsWith(TEXT("TRASHCLASS_")))
		{
			OutError = TEXT("CLASS_REINSTANCED");
			return nullptr;
		}
		if (Candidate->HasAnyClassFlags(CLASS_Abstract))
		{
			OutError = TEXT("CLASS_ABSTRACT");
			return nullptr;
		}
		if (Candidate->HasAnyClassFlags(CLASS_Deprecated))
		{
			OutError = TEXT("CLASS_DEPRECATED");
			return nullptr;
		}

		OutResolvedPath = Candidate->GetPathName();
		return Candidate;
	}

	// ---- World resolution ----------------------------------------------------

	/**
	 * Resolve the live PIE world for a parsed selector. Only EWorldType::PIE contexts with a valid
	 * World() are considered; the editor world, GEditor->PlayWorld, and /Memory/UEDPIE_* shells are
	 * never used. Returns null with OutError on failure.
	 */
	UWorld* ResolveWorldFromRequest(const FSelectorRequest& Req, int32& OutInstance, FString& OutError)
	{
		OutError.Reset();
		OutInstance = INDEX_NONE;

		if (!GEditor)
		{
			OutError = TEXT("PIE_NOT_RUNNING");
			return nullptr;
		}

		TArray<TPair<int32, UWorld*>> PIEWorlds; // (PIEInstance, World)
		for (const FWorldContext& Context : GEditor->GetWorldContexts())
		{
			if (Context.WorldType != EWorldType::PIE)
			{
				continue;
			}
			UWorld* World = Context.World();
			if (!World)
			{
				continue;
			}
			PIEWorlds.Emplace(Context.PIEInstance, World);
		}

		if (PIEWorlds.Num() == 0)
		{
			OutError = TEXT("PIE_NOT_RUNNING");
			return nullptr;
		}

		if (Req.bClientAny)
		{
			TArray<TPair<int32, UWorld*>> Clients;
			for (const TPair<int32, UWorld*>& Pair : PIEWorlds)
			{
				if (Pair.Key >= 1)
				{
					Clients.Add(Pair);
				}
			}
			if (Clients.Num() == 0)
			{
				OutError = TEXT("WORLD_NOT_FOUND");
				return nullptr;
			}
			if (Clients.Num() > 1)
			{
				OutError = TEXT("AMBIGUOUS_WORLD");
				return nullptr;
			}
			OutInstance = Clients[0].Key;
			return Clients[0].Value;
		}

		for (const TPair<int32, UWorld*>& Pair : PIEWorlds)
		{
			if (Pair.Key == Req.Instance)
			{
				OutInstance = Pair.Key;
				return Pair.Value;
			}
		}

		OutError = TEXT("WORLD_NOT_FOUND");
		return nullptr;
	}

	int32 CountActors(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			++Count;
		}
		return Count;
	}
}

// =============================================================================
// Public API
// =============================================================================

FString UPIEActorService::ResolveWorld(const FString& WorldSelector)
{
	FSelectorRequest Req;
	const FString SelectorError = ParseSelector(WorldSelector, Req);
	if (!SelectorError.IsEmpty())
	{
		return ErrorJson(SelectorError, FString::Printf(
			TEXT("Selector '%s' is not valid — use 'server', 'client', 'client:N' (N>=1), or 'instance:N' (N>=0)."),
			*WorldSelector));
	}

	int32 Instance = INDEX_NONE;
	FString WorldError;
	UWorld* World = ResolveWorldFromRequest(Req, Instance, WorldError);
	if (!World)
	{
		FString Message;
		if (WorldError == TEXT("PIE_NOT_RUNNING"))
		{
			Message = TEXT("PIE is not running — start it first.");
		}
		else if (WorldError == TEXT("AMBIGUOUS_WORLD"))
		{
			Message = TEXT("More than one client world exists — address one with 'client:N'.");
		}
		else
		{
			Message = FString::Printf(TEXT("No PIE world matched selector '%s'."), *WorldSelector);
		}
		return ErrorJson(WorldError, Message);
	}

	const ENetMode NetMode = World->GetNetMode();

	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("world_path"), World->GetPathName());
	Obj->SetStringField(TEXT("net_mode"), NetModeToString(NetMode));
	Obj->SetNumberField(TEXT("pie_instance"), Instance);
	Obj->SetBoolField(TEXT("is_authority"), NetMode != NM_Client);
	Obj->SetNumberField(TEXT("actor_count"), CountActors(World));
	return OkJson(Obj);
}

FString UPIEActorService::SpawnActor(
	const FString& WorldSelector,
	const FString& ClassPath,
	const FTransform& Transform,
	const FString& CollisionHandling,
	bool bAllowClientLocal,
	const FString& Label)
{
	// 1) Selector (grammar + world resolution).
	FSelectorRequest Req;
	const FString SelectorError = ParseSelector(WorldSelector, Req);
	if (!SelectorError.IsEmpty())
	{
		return ErrorJson(SelectorError, FString::Printf(
			TEXT("Selector '%s' is not valid — use 'server', 'client', 'client:N' (N>=1), or 'instance:N' (N>=0)."),
			*WorldSelector));
	}

	int32 Instance = INDEX_NONE;
	FString WorldError;
	UWorld* World = ResolveWorldFromRequest(Req, Instance, WorldError);
	if (!World)
	{
		FString Message;
		if (WorldError == TEXT("PIE_NOT_RUNNING"))
		{
			Message = TEXT("PIE is not running — start it first.");
		}
		else if (WorldError == TEXT("AMBIGUOUS_WORLD"))
		{
			Message = TEXT("More than one client world exists — address one with 'client:N'.");
		}
		else
		{
			Message = FString::Printf(TEXT("No PIE world matched selector '%s'."), *WorldSelector);
		}
		return ErrorJson(WorldError, Message);
	}

	// 2) Class.
	FString ResolvedClassPath;
	FString ClassError;
	UClass* SpawnClass = ResolveActorClass(ClassPath, ResolvedClassPath, ClassError);
	if (!SpawnClass)
	{
		return ErrorJson(ClassError, FString::Printf(TEXT("Class '%s' rejected (%s)."), *ClassPath, *ClassError));
	}

	// 3) Transform.
	if (!IsTransformFinite(Transform))
	{
		return ErrorJson(TEXT("INVALID_TRANSFORM"), TEXT("Transform has a non-finite component (NaN/Inf)."));
	}

	// 4) Collision policy.
	ESpawnActorCollisionHandlingMethod CollisionMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FString CollisionError = ValidateCollision(CollisionHandling, CollisionMethod);
	if (!CollisionError.IsEmpty())
	{
		return ErrorJson(CollisionError, FString::Printf(
			TEXT("Unknown collision handling '%s' — use AlwaysSpawn, AdjustIfPossibleButAlwaysSpawn, AdjustIfPossibleButDontSpawnIfColliding, or DontSpawnIfColliding."),
			*CollisionHandling));
	}

	// 5) Authority — a client-local spawn is cosmetic and never replicates.
	const ENetMode NetMode = World->GetNetMode();
	if (NetMode == NM_Client && !bAllowClientLocal)
	{
		return ErrorJson(TEXT("CLIENT_REQUIRES_OPT_IN"),
			TEXT("The selected world is a client. A client-local spawn is cosmetic and never replicates — pass allow_client_local=true to spawn anyway, or target 'server'."));
	}

	// Spawn.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = CollisionMethod;
	Params.ObjectFlags |= RF_Transient;
	Params.bNoFail = false;

	AActor* Spawned = World->SpawnActor<AActor>(SpawnClass, Transform, Params);
	if (!Spawned)
	{
		return ErrorJson(TEXT("SPAWN_REJECTED"), FString::Printf(
			TEXT("Spawn was rejected under collision policy '%s' (the placement collided)."), *CollisionHandling));
	}

#if WITH_EDITOR
	if (!Label.IsEmpty())
	{
		// bMarkDirty=false: this is a transient PIE actor, never saved — don't dirty any package.
		Spawned->SetActorLabel(Label, /*bMarkDirty=*/false);
	}
#endif

	// Register.
	EnsureEndPIEHook();
	const FString Handle = FString::Printf(TEXT("%d:%s"),
		GSessionSerial, *FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));

	FSpawnedRecord Record;
	Record.Actor = Spawned;
	Record.ClassPath = ResolvedClassPath;
	Record.WorldPath = World->GetPathName();
	Record.SpawnTime = FDateTime::UtcNow();
	Record.Serial = GSessionSerial;
	GSpawnRegistry.Add(Handle, Record);

	const FVector Location = Spawned->GetActorLocation();
	const FRotator Rotation = Spawned->GetActorRotation();

	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("handle"), Handle);
	Obj->SetStringField(TEXT("actor_path"), Spawned->GetPathName());
	Obj->SetStringField(TEXT("actor_name"), Spawned->GetName());
	Obj->SetStringField(TEXT("class_path"), ResolvedClassPath);
	Obj->SetStringField(TEXT("world_path"), World->GetPathName());
	Obj->SetStringField(TEXT("net_mode"), NetModeToString(NetMode));
	Obj->SetNumberField(TEXT("pie_instance"), Instance);
	Obj->SetBoolField(TEXT("is_authority"), NetMode != NM_Client);

	const TSharedRef<FJsonObject> Loc = MakeShared<FJsonObject>();
	Loc->SetNumberField(TEXT("x"), Location.X);
	Loc->SetNumberField(TEXT("y"), Location.Y);
	Loc->SetNumberField(TEXT("z"), Location.Z);
	Obj->SetObjectField(TEXT("location"), Loc);

	const TSharedRef<FJsonObject> Rot = MakeShared<FJsonObject>();
	Rot->SetNumberField(TEXT("pitch"), Rotation.Pitch);
	Rot->SetNumberField(TEXT("yaw"), Rotation.Yaw);
	Rot->SetNumberField(TEXT("roll"), Rotation.Roll);
	Obj->SetObjectField(TEXT("rotation"), Rot);

	return OkJson(Obj);
}

FString UPIEActorService::DestroyActor(const FString& Handle)
{
	// Parse "<serial>:<guid>".
	FString SerialPart;
	FString GuidPart;
	if (!Handle.Split(TEXT(":"), &SerialPart, &GuidPart) || SerialPart.IsEmpty() || !SerialPart.IsNumeric())
	{
		return ErrorJson(TEXT("UNKNOWN_HANDLE"), FString::Printf(TEXT("Handle '%s' is not a handle this service issued."), *Handle));
	}
	const int32 HandleSerial = FCString::Atoi(*SerialPart);

	FSpawnedRecord* Record = GSpawnRegistry.Find(Handle);
	if (!Record)
	{
		if (HandleSerial != GSessionSerial)
		{
			return ErrorJson(TEXT("STALE_HANDLE"),
				TEXT("Handle is from an earlier PIE session — its actor and registry entry are gone."));
		}
		return ErrorJson(TEXT("UNKNOWN_HANDLE"), FString::Printf(TEXT("No spawned actor is registered under handle '%s'."), *Handle));
	}

	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("handle"), Handle);

	if (Record->Actor.IsValid())
	{
		AActor* Actor = Record->Actor.Get();
		Actor->Destroy();
		Obj->SetBoolField(TEXT("already_gone"), false);
	}
	else
	{
		Obj->SetBoolField(TEXT("already_gone"), true);
	}

	// Deliberately KEEP the record. Destroy() marks the actor as garbage, so the TWeakObjectPtr goes
	// invalid and a repeat destroy_actor(handle) takes the already_gone branch above — idempotent per
	// handle. The record (now alive=false) lingers until EndPIE clears the whole registry.
	return OkJson(Obj);
}

FString UPIEActorService::DestroyAll()
{
	int32 Destroyed = 0;
	int32 AlreadyGone = 0;

	// Records are KEPT (not removed), mirroring destroy_actor: an actor we already consumed this
	// session counts under already_gone, and every record lingers as alive=false until EndPIE clears
	// the whole registry. This keeps destroy_all idempotent and harmless to call repeatedly.
	for (TPair<FString, FSpawnedRecord>& Pair : GSpawnRegistry)
	{
		if (Pair.Value.Serial != GSessionSerial)
		{
			// Belt-and-suspenders: EndPIE already clears the registry, so this normally never trips.
			continue;
		}
		if (Pair.Value.Actor.IsValid())
		{
			Pair.Value.Actor->Destroy();
			++Destroyed;
		}
		else
		{
			++AlreadyGone;
		}
	}

	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("destroyed"), Destroyed);
	Obj->SetNumberField(TEXT("already_gone"), AlreadyGone);
	return OkJson(Obj);
}

FString UPIEActorService::ListSpawned()
{
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const TPair<FString, FSpawnedRecord>& Pair : GSpawnRegistry)
	{
		if (Pair.Value.Serial != GSessionSerial)
		{
			continue;
		}
		const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("handle"), Pair.Key);
		Entry->SetStringField(TEXT("actor_path"), Pair.Value.Actor.IsValid() ? Pair.Value.Actor->GetPathName() : FString());
		Entry->SetStringField(TEXT("class_path"), Pair.Value.ClassPath);
		Entry->SetStringField(TEXT("world_path"), Pair.Value.WorldPath);
		Entry->SetBoolField(TEXT("alive"), Pair.Value.Actor.IsValid());
		Items.Add(MakeShared<FJsonValueObject>(Entry));
	}

	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("count"), Items.Num());
	Obj->SetArrayField(TEXT("spawned"), Items);
	return OkJson(Obj);
}

// =============================================================================
// Test-only surface (no UFUNCTION) — reached through FPIEActorServiceTestAccess
// =============================================================================

FString UPIEActorService::ValidateSelectorCode(const FString& WorldSelector)
{
	FSelectorRequest Req;
	return ParseSelector(WorldSelector, Req);
}

FString UPIEActorService::ValidateCollisionCode(const FString& CollisionHandling)
{
	ESpawnActorCollisionHandlingMethod Method = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return ValidateCollision(CollisionHandling, Method);
}

FString UPIEActorService::ValidateClassCode(const FString& ClassPath)
{
	FString ResolvedPath;
	FString Error;
	ResolveActorClass(ClassPath, ResolvedPath, Error);
	return Error;
}

int32 UPIEActorService::GetSessionSerialForTest()
{
	return GSessionSerial;
}
