// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UPIEActorService.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LandscapeProxy.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Components/ActorComponent.h"
#include "Misc/StringOutputDevice.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/Script.h"

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

		// A landscape spawned without a LandscapeGuid registers with ULandscapeSubsystem but is never
		// unregistered on destroy (UnregisterAllComponents is GUID-gated), so the subsystem's Tick later
		// dereferences a GC-nulled entry and crashes. Refuse every landscape proxy class.
		if (Candidate->IsChildOf(ALandscapeProxy::StaticClass()))
		{
			OutError = TEXT("UNSUPPORTED_CLASS");
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

	// ---- call_function / console_command helpers -----------------------------

	/** Human message for a ResolveWorldFromRequest error code (same wording as ResolveWorld/SpawnActor). */
	FString WorldErrorMessage(const FString& WorldError, const FString& WorldSelector)
	{
		if (WorldError == TEXT("PIE_NOT_RUNNING"))
		{
			return TEXT("PIE is not running — start it first.");
		}
		if (WorldError == TEXT("AMBIGUOUS_WORLD"))
		{
			return TEXT("More than one client world exists — address one with 'client:N'.");
		}
		return FString::Printf(TEXT("No PIE world matched selector '%s'."), *WorldSelector);
	}

	/** Selector grammar + world lookup in one step. Returns null with OutErrorReply (JSON) set on failure. */
	UWorld* ResolvePIEWorldOrError(const FString& WorldSelector, int32& OutInstance, FString& OutErrorReply)
	{
		FSelectorRequest Req;
		const FString SelectorError = ParseSelector(WorldSelector, Req);
		if (!SelectorError.IsEmpty())
		{
			OutErrorReply = ErrorJson(SelectorError, FString::Printf(
				TEXT("Selector '%s' is not valid — use 'server', 'client', 'client:N' (N>=1), or 'instance:N' (N>=0)."),
				*WorldSelector));
			return nullptr;
		}

		FString WorldError;
		UWorld* World = ResolveWorldFromRequest(Req, OutInstance, WorldError);
		if (!World)
		{
			OutErrorReply = ErrorJson(WorldError, WorldErrorMessage(WorldError, WorldSelector));
		}
		return World;
	}

	/** Find an actor in World by exact object name first, then by editor label (unique match only). */
	AActor* FindActorByNameOrLabel(UWorld* World, const FString& Name, bool& bOutAmbiguous)
	{
		bOutAmbiguous = false;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetName().Equals(Name, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
#if WITH_EDITOR
		AActor* LabelMatch = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetActorLabel(/*bCreateIfNone=*/false).Equals(Name, ESearchCase::IgnoreCase))
			{
				if (LabelMatch)
				{
					bOutAmbiguous = true;
					return nullptr;
				}
				LabelMatch = *It;
			}
		}
		return LabelMatch;
#else
		return nullptr;
#endif
	}

	/**
	 * Resolve an object INSIDE World only (never the editor world, never another PIE world).
	 * Accepts: a spawn_actor handle, a full object path ("/..."), an actor name or label, or
	 * "<actor>.<component>" / "<actor>:<subobject>". Returns null with OutError set to
	 * OBJECT_NOT_FOUND, AMBIGUOUS_OBJECT, or OBJECT_NOT_IN_WORLD.
	 */
	UObject* ResolveObjectInWorld(UWorld* World, const FString& InPath, FString& OutError)
	{
		OutError.Reset();
		const FString Path = InPath.TrimStartAndEnd();
		if (!World || Path.IsEmpty())
		{
			OutError = TEXT("OBJECT_NOT_FOUND");
			return nullptr;
		}

		// 1) A handle this service issued.
		if (const FSpawnedRecord* Record = GSpawnRegistry.Find(Path))
		{
			AActor* Actor = Record->Actor.Get();
			if (!Actor)
			{
				OutError = TEXT("OBJECT_NOT_FOUND");
				return nullptr;
			}
			if (Actor->GetWorld() != World)
			{
				OutError = TEXT("OBJECT_NOT_IN_WORLD");
				return nullptr;
			}
			return Actor;
		}

		// 2) A full object path: must belong to the selected world.
		if (Path.StartsWith(TEXT("/")))
		{
			UObject* Found = StaticFindObject(UObject::StaticClass(), nullptr, Path);
			if (!IsValid(Found))
			{
				OutError = TEXT("OBJECT_NOT_FOUND");
				return nullptr;
			}
			if (Found->GetWorld() != World)
			{
				OutError = TEXT("OBJECT_NOT_IN_WORLD");
				return nullptr;
			}
			return Found;
		}

		// 3) The whole string as an actor name/label (labels may contain '.' or ':').
		bool bAmbiguous = false;
		if (AActor* Actor = FindActorByNameOrLabel(World, Path, bAmbiguous))
		{
			return Actor;
		}
		if (bAmbiguous)
		{
			OutError = TEXT("AMBIGUOUS_OBJECT");
			return nullptr;
		}

		// 4) "<actor>.<component>" or "<actor>:<subobject>" — split at the FIRST delimiter.
		int32 DotIndex = INDEX_NONE;
		int32 ColonIndex = INDEX_NONE;
		Path.FindChar(TEXT('.'), DotIndex);
		Path.FindChar(TEXT(':'), ColonIndex);
		int32 SplitIndex = DotIndex;
		if (ColonIndex != INDEX_NONE && (SplitIndex == INDEX_NONE || ColonIndex < SplitIndex))
		{
			SplitIndex = ColonIndex;
		}
		if (SplitIndex == INDEX_NONE)
		{
			OutError = TEXT("OBJECT_NOT_FOUND");
			return nullptr;
		}

		const FString ActorPart = Path.Left(SplitIndex);
		const FString SubPart = Path.Mid(SplitIndex + 1);
		AActor* Owner = FindActorByNameOrLabel(World, ActorPart, bAmbiguous);
		if (!Owner || SubPart.IsEmpty())
		{
			OutError = bAmbiguous ? TEXT("AMBIGUOUS_OBJECT") : TEXT("OBJECT_NOT_FOUND");
			return nullptr;
		}

		if (SplitIndex == DotIndex)
		{
			for (UActorComponent* Component : Owner->GetComponents())
			{
				if (IsValid(Component) && Component->GetName().Equals(SubPart, ESearchCase::IgnoreCase))
				{
					return Component;
				}
			}
		}

		UObject* Sub = StaticFindObject(UObject::StaticClass(), Owner, SubPart);
		if (!IsValid(Sub))
		{
			OutError = TEXT("OBJECT_NOT_FOUND");
			return nullptr;
		}
		return Sub;
	}

	/**
	 * Resolve an object/class ARGUMENT. Order: the PIE world (name/label/handle/in-world path) when
	 * World is set, then a global object path (find, then a quiet load for assets). An object that lives
	 * in a different world than World (another PIE instance, or the editor world) is refused.
	 */
	UObject* ResolveArgumentObject(UWorld* World, const FString& Text, FString& OutProblem, FString& OutCode)
	{
		OutProblem.Reset();
		OutCode = TEXT("INVALID_ARG_VALUE");
		if (World)
		{
			FString InWorldError;
			if (UObject* InWorld = ResolveObjectInWorld(World, Text, InWorldError))
			{
				return InWorld;
			}
		}

		if (!Text.StartsWith(TEXT("/")))
		{
			OutProblem = FString::Printf(TEXT("'%s' did not resolve to an object (use a name/label in the PIE world, or a full object path)."), *Text);
			return nullptr;
		}

		UObject* Found = StaticFindObject(UObject::StaticClass(), nullptr, Text);
		if (!Found)
		{
			// Never load a map (or anything inside one) to satisfy an argument: loading a map package as
			// an ASSET leaves it resident, and the next load_level then fatals. Only non-map assets load.
			if (FEditorFileUtils::IsMapPackageAsset(Text))
			{
				OutCode = TEXT("MAP_PATH_REFUSED");
				OutProblem = FString::Printf(TEXT("'%s' is in a map package that is not loaded; map packages are never loaded as arguments (pass an object in the PIE world instead)."), *Text);
				return nullptr;
			}
			Found = StaticLoadObject(UObject::StaticClass(), nullptr, Text, FStringView(), LOAD_NoWarn | LOAD_Quiet);
		}
		if (!IsValid(Found))
		{
			OutProblem = FString::Printf(TEXT("No object found at '%s'."), *Text);
			return nullptr;
		}
		if (World)
		{
			const UWorld* ObjectWorld = Found->GetWorld();
			if (ObjectWorld && ObjectWorld != World)
			{
				OutProblem = FString::Printf(TEXT("'%s' belongs to a different world (%s), not the selected PIE world."),
					*Text, *ObjectWorld->GetPathName());
				return nullptr;
			}
		}
		return Found;
	}

	/** Print a JSON number as ImportText input (integral values without a fraction). */
	FString JsonNumberToText(double Value)
	{
		if (FMath::IsFinite(Value) && FMath::Frac(Value) == 0.0 && FMath::Abs(Value) < 9.0e15)
		{
			return FString::Printf(TEXT("%lld"), static_cast<long long>(Value));
		}
		return FString::SanitizeFloat(Value);
	}

	/**
	 * True when Value fits the concrete integer property type. OutRange describes the accepted range.
	 * uint64 is capped at 2^63-1 (the value is written through int64).
	 */
	bool IntegerInRange(const FNumericProperty* IntProp, double Value, FString& OutRange)
	{
		double Min = 0.0;
		double MaxExclusive = 0.0;
		if (IntProp->IsA<FInt8Property>())        { Min = -128.0;                  MaxExclusive = 128.0; }
		else if (IntProp->IsA<FInt16Property>())  { Min = -32768.0;                MaxExclusive = 32768.0; }
		else if (IntProp->IsA<FIntProperty>())    { Min = -2147483648.0;           MaxExclusive = 2147483648.0; }
		else if (IntProp->IsA<FInt64Property>())  { Min = -9223372036854775808.0;  MaxExclusive = 9223372036854775808.0; }
		else if (IntProp->IsA<FByteProperty>())   { Min = 0.0;                     MaxExclusive = 256.0; }
		else if (IntProp->IsA<FUInt16Property>()) { Min = 0.0;                     MaxExclusive = 65536.0; }
		else if (IntProp->IsA<FUInt32Property>()) { Min = 0.0;                     MaxExclusive = 4294967296.0; }
		else if (IntProp->IsA<FUInt64Property>()) { Min = 0.0;                     MaxExclusive = 9223372036854775808.0; }
		else
		{
			return true;
		}
		OutRange = FString::Printf(TEXT("[%.0f, %.0f]"), Min, MaxExclusive - 1.0);
		return Value >= Min && Value < MaxExclusive;
	}

	/** A value that names a real enumerator (never the generated _MAX). Bitflag enums are not checked. */
	bool IsUsableEnumValue(const UEnum* Enum, int64 Value)
	{
		if (!Enum || Enum->HasAnyEnumFlags(EEnumFlags::Flags))
		{
			return true;
		}
		const int32 Index = Enum->GetIndexByValue(Value);
		if (Index == INDEX_NONE)
		{
			return false;
		}
		return !(Enum->ContainsExistingMax() && Index == Enum->NumEnums() - 1);
	}

	/** Write an integral JSON value into an integer property (or an enum's underlying property). */
	FString WriteIntegerArgument(FNumericProperty* IntProp, const UEnum* Enum, void* ValuePtr, double Value,
		const FString& ParamName, FString& OutMessage)
	{
		if (!FMath::IsFinite(Value) || FMath::Frac(Value) != 0.0)
		{
			OutMessage = FString::Printf(TEXT("Parameter '%s' takes an integer — %s is not one."), *ParamName, *FString::SanitizeFloat(Value));
			return TEXT("INVALID_ARG_VALUE");
		}
		FString Range;
		if (!IntegerInRange(IntProp, Value, Range))
		{
			OutMessage = FString::Printf(TEXT("Parameter '%s' (%s): %s is out of range %s."),
				*ParamName, *IntProp->GetCPPType(), *JsonNumberToText(Value), *Range);
			return TEXT("INVALID_ARG_VALUE");
		}
		const int64 IntValue = static_cast<int64>(Value);
		if (Enum && !IsUsableEnumValue(Enum, IntValue))
		{
			OutMessage = FString::Printf(TEXT("Parameter '%s': %lld is not a valid %s value."),
				*ParamName, static_cast<long long>(IntValue), *Enum->GetName());
			return TEXT("INVALID_ARG_VALUE");
		}
		IntProp->SetIntPropertyValue(ValuePtr, IntValue);
		return FString();
	}

	/** Write a finite JSON value into a float/double property. */
	FString WriteFloatArgument(FNumericProperty* FloatProp, void* ValuePtr, double Value,
		const FString& ParamName, FString& OutMessage)
	{
		if (!FMath::IsFinite(Value))
		{
			OutMessage = FString::Printf(TEXT("Parameter '%s': value is not finite."), *ParamName);
			return TEXT("INVALID_ARG_VALUE");
		}
		FloatProp->SetFloatingPointPropertyValue(ValuePtr, Value);
		return FString();
	}

	/**
	 * Write one JSON value into a parameter. Returns "" on success, otherwise an error code
	 * (INVALID_ARG_VALUE / UNSUPPORTED_ARG_TYPE) with OutMessage set.
	 */
	FString ImportArgument(FProperty* Prop, void* ValuePtr, const TSharedPtr<FJsonValue>& Json, UObject* Owner,
		UWorld* World, FString& OutMessage)
	{
		const FString ParamName = Prop->GetName();

		if (Prop->IsA<FDelegateProperty>() || Prop->IsA<FMulticastDelegateProperty>())
		{
			OutMessage = FString::Printf(TEXT("Parameter '%s' is a delegate — delegate parameters are not supported."), *ParamName);
			return TEXT("UNSUPPORTED_ARG_TYPE");
		}
		if (!Json.IsValid() || Json->Type == EJson::Array || Json->Type == EJson::Object)
		{
			OutMessage = FString::Printf(
				TEXT("Parameter '%s': JSON arrays/objects are not accepted — pass a number, bool, or string (structs and arrays as their ImportText form, e.g. \"(X=1,Y=2,Z=3)\")."),
				*ParamName);
			return TEXT("UNSUPPORTED_ARG_TYPE");
		}

		// Hard object / class references: resolved here (PIE world first), never via ImportText.
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
		{
			FString Text;
			if (Json->Type == EJson::String)
			{
				Text = Json->AsString().TrimStartAndEnd();
			}
			else if (Json->Type != EJson::Null)
			{
				OutMessage = FString::Printf(TEXT("Parameter '%s' is an object reference — pass a name, label, or object path string."), *ParamName);
				return TEXT("INVALID_ARG_VALUE");
			}

			if (Text.IsEmpty() || Text.Equals(TEXT("None"), ESearchCase::IgnoreCase))
			{
				ObjProp->SetObjectPropertyValue(ValuePtr, nullptr);
				return FString();
			}

			FString Problem;
			FString ProblemCode;
			UObject* Resolved = ResolveArgumentObject(World, Text, Problem, ProblemCode);
			if (!Resolved)
			{
				OutMessage = FString::Printf(TEXT("Parameter '%s': %s"), *ParamName, *Problem);
				return ProblemCode;
			}

			if (FClassProperty* ClassProp = CastField<FClassProperty>(Prop))
			{
				UClass* AsClass = Cast<UClass>(Resolved);
				if (!AsClass)
				{
					if (const UBlueprint* Blueprint = Cast<UBlueprint>(Resolved))
					{
						AsClass = Blueprint->GeneratedClass;
					}
				}
				UClass* MetaClass = ClassProp->MetaClass;
				if (!AsClass || (MetaClass && !AsClass->IsChildOf(MetaClass)))
				{
					OutMessage = FString::Printf(TEXT("Parameter '%s': '%s' is not a class derived from %s."),
						*ParamName, *Text, MetaClass ? *MetaClass->GetName() : TEXT("UObject"));
					return TEXT("INVALID_ARG_VALUE");
				}
				ClassProp->SetObjectPropertyValue(ValuePtr, AsClass);
				return FString();
			}

			UClass* PropertyClass = ObjProp->PropertyClass;
			if (PropertyClass && !Resolved->IsA(PropertyClass))
			{
				OutMessage = FString::Printf(TEXT("Parameter '%s': '%s' is a %s, expected %s."),
					*ParamName, *Text, *Resolved->GetClass()->GetName(), *PropertyClass->GetName());
				return TEXT("INVALID_ARG_VALUE");
			}
			ObjProp->SetObjectPropertyValue(ValuePtr, Resolved);
			return FString();
		}

		if (Json->Type == EJson::Null)
		{
			OutMessage = FString::Printf(TEXT("Parameter '%s': null is only accepted for object references."), *ParamName);
			return TEXT("INVALID_ARG_VALUE");
		}

		// Direct typed writes for the common scalar cases.
		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
		{
			if (Json->Type == EJson::Boolean)
			{
				BoolProp->SetPropertyValue(ValuePtr, Json->AsBool());
				return FString();
			}
		}
		else if (FNumericProperty* NumProp = CastField<FNumericProperty>(Prop))
		{
			const UEnum* ByteEnum = NumProp->GetIntPropertyEnum();
			if (Json->Type == EJson::Number)
			{
				if (NumProp->IsInteger())
				{
					return WriteIntegerArgument(NumProp, ByteEnum, ValuePtr, Json->AsNumber(), ParamName, OutMessage);
				}
				return WriteFloatArgument(NumProp, ValuePtr, Json->AsNumber(), ParamName, OutMessage);
			}
			if (Json->Type == EJson::String && !ByteEnum)
			{
				// A number given as a string: the whole string must be numeric ("12abc" is refused).
				const FString Trimmed = Json->AsString().TrimStartAndEnd();
				if (Trimmed.IsEmpty() || !Trimmed.IsNumeric())
				{
					OutMessage = FString::Printf(TEXT("Parameter '%s' (%s): '%s' is not a number."), *ParamName, *Prop->GetCPPType(), *Trimmed);
					return TEXT("INVALID_ARG_VALUE");
				}
				const double Value = FCString::Atod(*Trimmed);
				if (NumProp->IsInteger())
				{
					return WriteIntegerArgument(NumProp, nullptr, ValuePtr, Value, ParamName, OutMessage);
				}
				return WriteFloatArgument(NumProp, ValuePtr, Value, ParamName, OutMessage);
			}
		}
		else if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
		{
			if (Json->Type == EJson::Number)
			{
				return WriteIntegerArgument(EnumProp->GetUnderlyingProperty(), EnumProp->GetEnum(), ValuePtr, Json->AsNumber(), ParamName, OutMessage);
			}
		}
		else if (Json->Type == EJson::String)
		{
			if (FStrProperty* StrProp = CastField<FStrProperty>(Prop))
			{
				StrProp->SetPropertyValue(ValuePtr, Json->AsString());
				return FString();
			}
			if (FNameProperty* NameProp = CastField<FNameProperty>(Prop))
			{
				NameProp->SetPropertyValue(ValuePtr, FName(*Json->AsString()));
				return FString();
			}
			if (FTextProperty* TextProp = CastField<FTextProperty>(Prop))
			{
				TextProp->SetPropertyValue(ValuePtr, FText::FromString(Json->AsString()));
				return FString();
			}
		}

		// Everything else (enums by name, bools as text, structs, containers, soft/weak refs): the
		// property's own ImportText form. Object references NESTED inside a struct/array/map are resolved
		// by the engine's global object lookup here, NOT confined to the selected PIE world.
		FString Text;
		switch (Json->Type)
		{
		case EJson::String:  Text = Json->AsString(); break;
		case EJson::Number:  Text = JsonNumberToText(Json->AsNumber()); break;
		case EJson::Boolean: Text = Json->AsBool() ? TEXT("true") : TEXT("false"); break;
		default: break;
		}

		FStringOutputDevice ImportErrors;
		const TCHAR* End = Prop->ImportText_Direct(*Text, ValuePtr, Owner, PPF_None, &ImportErrors);
		if (!End || !ImportErrors.IsEmpty())
		{
			OutMessage = FString::Printf(TEXT("Parameter '%s' (%s) could not parse '%s'%s%s"),
				*ParamName, *Prop->GetCPPType(), *Text,
				ImportErrors.IsEmpty() ? TEXT(".") : TEXT(": "), *ImportErrors.TrimStartAndEnd());
			return TEXT("INVALID_ARG_VALUE");
		}

		// Scalars parsed from text must consume the whole string, and enums must land on a real value.
		const bool bScalar = Prop->IsA<FNumericProperty>() || Prop->IsA<FEnumProperty>() || Prop->IsA<FBoolProperty>();
		if (bScalar)
		{
			while (FChar::IsWhitespace(*End))
			{
				++End;
			}
			if (*End != TEXT('\0'))
			{
				OutMessage = FString::Printf(TEXT("Parameter '%s' (%s): unexpected trailing text '%s' in '%s'."),
					*ParamName, *Prop->GetCPPType(), End, *Text);
				return TEXT("INVALID_ARG_VALUE");
			}
		}

		const UEnum* ImportedEnum = nullptr;
		const FNumericProperty* ImportedUnderlying = nullptr;
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
		{
			ImportedEnum = EnumProp->GetEnum();
			ImportedUnderlying = EnumProp->GetUnderlyingProperty();
		}
		else if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Prop))
		{
			ImportedEnum = NumProp->GetIntPropertyEnum();
			ImportedUnderlying = NumProp;
		}
		if (ImportedEnum && ImportedUnderlying
			&& !IsUsableEnumValue(ImportedEnum, ImportedUnderlying->GetSignedIntPropertyValue(ValuePtr)))
		{
			OutMessage = FString::Printf(TEXT("Parameter '%s': '%s' is not a valid %s value."),
				*ParamName, *Text, *ImportedEnum->GetName());
			return TEXT("INVALID_ARG_VALUE");
		}
		return FString();
	}

	/** Export a parameter value as a typed JSON value (scalars typed, everything else as ExportText). */
	TSharedPtr<FJsonValue> ExportParam(const FProperty* Prop, const void* ValuePtr)
	{
		if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
		{
			return MakeShared<FJsonValueBoolean>(BoolProp->GetPropertyValue(ValuePtr));
		}
		if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Prop))
		{
			if (!NumProp->IsEnum())
			{
				if (NumProp->IsInteger())
				{
					return MakeShared<FJsonValueNumber>(static_cast<double>(NumProp->GetSignedIntPropertyValue(ValuePtr)));
				}
				return MakeShared<FJsonValueNumber>(NumProp->GetFloatingPointPropertyValue(ValuePtr));
			}
		}
		if (const FStrProperty* StrProp = CastField<FStrProperty>(Prop))
		{
			return MakeShared<FJsonValueString>(StrProp->GetPropertyValue(ValuePtr));
		}
		if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
		{
			return MakeShared<FJsonValueString>(NameProp->GetPropertyValue(ValuePtr).ToString());
		}
		if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
		{
			return MakeShared<FJsonValueString>(TextProp->GetPropertyValue(ValuePtr).ToString());
		}
		if (const FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
		{
			const UObject* Value = ObjProp->GetObjectPropertyValue(ValuePtr);
			if (!Value)
			{
				return MakeShared<FJsonValueNull>();
			}
			return MakeShared<FJsonValueString>(Value->GetPathName());
		}

		FString Text;
		Prop->ExportText_Direct(Text, ValuePtr, nullptr, nullptr, PPF_None);
		return MakeShared<FJsonValueString>(Text);
	}

	FString CallspaceToString(int32 Callspace)
	{
		const bool bLocal = (Callspace & FunctionCallspace::Local) != 0;
		const bool bRemote = (Callspace & FunctionCallspace::Remote) != 0;
		if (bLocal && bRemote)
		{
			return TEXT("local+remote");
		}
		if (bRemote)
		{
			return TEXT("remote");
		}
		if (bLocal)
		{
			return TEXT("local");
		}
		return TEXT("absorbed");
	}

	/** A parameter the caller supplies: not the return value, and not an out-only (non-ref) param. */
	bool IsInputParam(const FProperty* Prop)
	{
		if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			return false;
		}
		return !Prop->HasAnyPropertyFlags(CPF_OutParm) || Prop->HasAnyPropertyFlags(CPF_ReferenceParm);
	}

	/** Find the JSON value supplied for a parameter name (case-insensitive), or null. */
	TSharedPtr<FJsonValue> FindArgument(const FJsonObject& Args, const FString& ParamName)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Args.Values)
		{
			if (Pair.Key.Equals(ParamName, ESearchCase::IgnoreCase))
			{
				return Pair.Value.IsValid() ? Pair.Value : MakeShared<FJsonValueNull>();
			}
		}
		return nullptr;
	}

	/**
	 * Marshal, call and export. Fills OutReply's call fields and returns true, or returns false with
	 * OutError/OutMessage set on a rejection made BEFORE anything was called.
	 */
	bool CallFunctionCore(UObject* Object, const FString& FunctionName, const FString& ArgsJson, UWorld* World,
		const TSharedRef<FJsonObject>& OutReply, FString& OutError, FString& OutMessage)
	{
		if (!IsValid(Object))
		{
			OutError = TEXT("OBJECT_NOT_FOUND");
			OutMessage = TEXT("The target object is null or pending destruction.");
			return false;
		}

		const FString TrimmedName = FunctionName.TrimStartAndEnd();
		UFunction* Func = TrimmedName.IsEmpty() ? nullptr : Object->FindFunction(FName(*TrimmedName));
		if (!Func)
		{
			OutError = TEXT("FUNCTION_NOT_FOUND");
			OutMessage = FString::Printf(TEXT("%s has no UFUNCTION named '%s'."), *Object->GetClass()->GetName(), *FunctionName);
			return false;
		}

		// Arguments: "" means none; otherwise it must be a JSON object.
		TSharedPtr<FJsonObject> Args;
		const FString ArgsText = ArgsJson.TrimStartAndEnd();
		if (ArgsText.IsEmpty())
		{
			Args = MakeShared<FJsonObject>();
		}
		else if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ArgsText), Args) || !Args.IsValid())
		{
			OutError = TEXT("INVALID_ARGS_JSON");
			OutMessage = TEXT("args_json must be a JSON object keyed by parameter name, e.g. '{\"A\": 1}'.");
			return false;
		}

		// Reject keys that name no parameter (a typo must not silently fall back to a default).
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Args->Values)
		{
			bool bKnown = false;
			for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				if (!It->HasAnyPropertyFlags(CPF_ReturnParm) && It->GetName().Equals(Pair.Key, ESearchCase::IgnoreCase))
				{
					bKnown = true;
					break;
				}
			}
			if (!bKnown)
			{
				OutError = TEXT("UNKNOWN_ARG");
				OutMessage = FString::Printf(TEXT("'%s' is not a parameter of %s."), *Pair.Key, *Func->GetName());
				return false;
			}
		}

		// Parameter buffer: FStructOnScope initializes every property and destroys them on scope exit.
		FStructOnScope Params(Func);
		uint8* ParamMemory = Params.GetStructMemory();

		for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* Prop = *It;
			if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}
			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ParamMemory);

			const TSharedPtr<FJsonValue> Supplied = FindArgument(*Args, Prop->GetName());
			if (Supplied.IsValid())
			{
				FString Message;
				const FString Error = ImportArgument(Prop, ValuePtr, Supplied, Object, World, Message);
				if (!Error.IsEmpty())
				{
					OutError = Error;
					OutMessage = Message;
					return false;
				}
				continue;
			}

			if (!IsInputParam(Prop))
			{
				continue; // out-only: starts zero-initialized
			}

			// Missing input: use the declared C++ default when there is one.
			bool bDefaulted = false;
#if WITH_METADATA
			const FString DefaultKey = FString::Printf(TEXT("CPP_Default_%s"), *Prop->GetName());
			if (const FString* DefaultText = Func->FindMetaData(*DefaultKey))
			{
				FStringOutputDevice ImportErrors;
				if (Prop->ImportText_Direct(**DefaultText, ValuePtr, Object, PPF_None, &ImportErrors) != nullptr && ImportErrors.IsEmpty())
				{
					bDefaulted = true;
				}
			}
#endif
			if (!bDefaulted)
			{
				OutError = TEXT("MISSING_ARG");
				OutMessage = FString::Printf(TEXT("%s requires parameter '%s' (%s)."), *Func->GetName(), *Prop->GetName(), *Prop->GetCPPType());
				return false;
			}
		}

		// The point of this call: lower the Python plugin's script-execution guard so
		// AActor::GetFunctionCallspace answers by real net role. Scoped to this call only; the previous
		// value is restored on scope exit.
		//
		// ProcessEvent routes BOTH kinds of function, so it must stay the only call here (adding a
		// CallRemoteFunction of our own would send a Blueprint RPC twice):
		// - native (C++) functions: UObject::ProcessEvent checks the callspace itself, calls
		//   CallRemoteFunction when it is Remote and skips the body unless it is Local;
		// - script (Blueprint) functions: UFunction::Bind points Func at UObject::ProcessInternal, and
		//   ProcessEvent calls Function->Invoke, so ProcessInternal does the same check (remote send,
		//   local body only when Local, nothing when Absorbed).
		// The reported Callspace is computed with the same guard state, just before the call.
		int32 Callspace = FunctionCallspace::Local;
		{
			TGuardValue<bool> RealRpcRouting(GAllowActorScriptExecutionInEditor, false);
			Callspace = Object->GetFunctionCallspace(Func, nullptr);
			Object->ProcessEvent(Func, ParamMemory);
		}

		const bool bExecutedLocally = (Callspace & FunctionCallspace::Local) != 0;
		OutReply->SetStringField(TEXT("function"), Func->GetName());
		OutReply->SetStringField(TEXT("object_path"), Object->GetPathName());
		OutReply->SetStringField(TEXT("callspace"), CallspaceToString(Callspace));
		OutReply->SetBoolField(TEXT("executed_locally"), bExecutedLocally);

		TSharedPtr<FJsonValue> ReturnValue = MakeShared<FJsonValueNull>();
		const TSharedRef<FJsonObject> OutParams = MakeShared<FJsonObject>();
		if (bExecutedLocally)
		{
			for (TFieldIterator<FProperty> It(Func); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				const FProperty* Prop = *It;
				const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ParamMemory);
				if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					ReturnValue = ExportParam(Prop, ValuePtr);
				}
				else if (Prop->HasAnyPropertyFlags(CPF_OutParm) && !Prop->HasAnyPropertyFlags(CPF_ConstParm))
				{
					OutParams->SetField(Prop->GetName(), ExportParam(Prop, ValuePtr));
				}
			}
		}
		OutReply->SetField(TEXT("return"), ReturnValue);
		OutReply->SetObjectField(TEXT("out_params"), OutParams);
		return true;
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
		if (ClassError == TEXT("UNSUPPORTED_CLASS"))
		{
			return ErrorJson(ClassError, FString::Printf(
				TEXT("Class '%s' is a landscape proxy; spawning and destroying one in PIE leaves a dangling ULandscapeSubsystem entry that crashes the editor."),
				*ClassPath));
		}
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

FString UPIEActorService::CallFunction(
	const FString& WorldSelector,
	const FString& ObjectPath,
	const FString& FunctionName,
	const FString& ArgsJson)
{
	int32 Instance = INDEX_NONE;
	FString WorldErrorReply;
	UWorld* World = ResolvePIEWorldOrError(WorldSelector, Instance, WorldErrorReply);
	if (!World)
	{
		return WorldErrorReply;
	}

	FString ObjectError;
	UObject* Object = ResolveObjectInWorld(World, ObjectPath, ObjectError);
	if (!Object)
	{
		FString Message;
		if (ObjectError == TEXT("AMBIGUOUS_OBJECT"))
		{
			Message = FString::Printf(TEXT("More than one actor in %s has the label '%s' — use its object name or full path."),
				*World->GetPathName(), *ObjectPath);
		}
		else if (ObjectError == TEXT("OBJECT_NOT_IN_WORLD"))
		{
			Message = FString::Printf(TEXT("'%s' exists but is not in the selected PIE world %s."), *ObjectPath, *World->GetPathName());
		}
		else
		{
			Message = FString::Printf(TEXT("No object '%s' in PIE world %s."), *ObjectPath, *World->GetPathName());
		}
		return ErrorJson(ObjectError, Message);
	}

	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	FString CallError;
	FString CallMessage;
	if (!CallFunctionCore(Object, FunctionName, ArgsJson, World, Obj, CallError, CallMessage))
	{
		return ErrorJson(CallError, CallMessage);
	}

	const ENetMode NetMode = World->GetNetMode();
	Obj->SetStringField(TEXT("world_path"), World->GetPathName());
	Obj->SetStringField(TEXT("net_mode"), NetModeToString(NetMode));
	Obj->SetNumberField(TEXT("pie_instance"), Instance);
	return OkJson(Obj);
}

FString UPIEActorService::ConsoleCommand(const FString& WorldSelector, const FString& Command)
{
	int32 Instance = INDEX_NONE;
	FString WorldErrorReply;
	UWorld* World = ResolvePIEWorldOrError(WorldSelector, Instance, WorldErrorReply);
	if (!World)
	{
		return WorldErrorReply;
	}

	const FString Trimmed = Command.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return ErrorJson(TEXT("EMPTY_COMMAND"), TEXT("Command is empty."));
	}
	if (!GEngine)
	{
		return ErrorJson(TEXT("PIE_NOT_RUNNING"), TEXT("GEngine is not available."));
	}

	FStringOutputDevice Output;
	Output.SetAutoEmitLineTerminator(true);
	bool bHandled = false;
	{
		// Same scoped fix as call_function: an RPC sent by the command's handler routes for real.
		TGuardValue<bool> RealRpcRouting(GAllowActorScriptExecutionInEditor, false);
		bHandled = GEngine->Exec(World, *Trimmed, Output);
	}

	const ENetMode NetMode = World->GetNetMode();
	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("command"), Trimmed);
	Obj->SetBoolField(TEXT("handled"), bHandled);
	Obj->SetStringField(TEXT("output"), Output);
	Obj->SetStringField(TEXT("world_path"), World->GetPathName());
	Obj->SetStringField(TEXT("net_mode"), NetModeToString(NetMode));
	Obj->SetNumberField(TEXT("pie_instance"), Instance);
	if (!bHandled)
	{
		Obj->SetBoolField(TEXT("success"), false);
		Obj->SetStringField(TEXT("error_code"), TEXT("COMMAND_NOT_HANDLED"));
		Obj->SetStringField(TEXT("message"), FString::Printf(
			TEXT("No engine/world handler claimed '%s' (PlayerController exec functions and cheats are not routed here — use call_function on the controller)."),
			*Trimmed));
		return SerializeJson(Obj);
	}
	return OkJson(Obj);
}

bool UPIEActorService::CallFunctionOnObject(
	UObject* Object,
	const FString& FunctionName,
	const FString& ArgsJson,
	FString& OutReplyJson,
	UWorld* ResolveWorld)
{
	const TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	FString CallError;
	FString CallMessage;
	if (!CallFunctionCore(Object, FunctionName, ArgsJson, ResolveWorld, Obj, CallError, CallMessage))
	{
		OutReplyJson = ErrorJson(CallError, CallMessage);
		return false;
	}
	OutReplyJson = OkJson(Obj);
	return true;
}

void UPIEActorService::ShutdownEndPIEHook()
{
	if (GEndPIEHookRegistered)
	{
		FEditorDelegates::EndPIE.Remove(GEndPIEHandle);
		GEndPIEHandle.Reset();
		GEndPIEHookRegistered = false;
	}
	// Any surviving records point at PIE actors that are already gone (or about to be); the service
	// never destroys on shutdown, it just stops tracking.
	GSpawnRegistry.Empty();
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
