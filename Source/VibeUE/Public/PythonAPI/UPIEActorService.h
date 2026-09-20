// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UPIEActorService.generated.h"

/**
 * Spawn and destroy transient actors inside a running Play-In-Editor (PIE) world.
 *
 * Native Python (unreal.World / unreal.GameplayStatics) exposes NO actor spawn — the
 * deferred-spawn entry points are BlueprintInternalUseOnly — and every editor-side spawn path
 * (EditorActorSubsystem.spawn_actor_from_class, the engine SceneTools toolset) either targets the
 * editor world or refuses outright while PIE is active. This service resolves the LIVE PIE world by
 * role and spawns into it, so a gate/harness can drop a blocker, a test dummy, or a trigger volume
 * into the actual game session without a Blueprint.
 *
 * Every method returns an FString JSON document. On success it carries success:true plus the
 * documented fields; on failure it carries success:false, an UPPER_SNAKE error_code, and a
 * human-readable message. Nothing here ever touches the editor world, opens a transaction, saves a
 * package, or writes config.
 *
 * Python method names (snake_case — the real bound API):
 * - resolve_world: Resolve a PIE world by selector and report its net role and actor count
 * - spawn_actor: Spawn a transient actor into a resolved PIE world; returns a handle
 * - destroy_actor: Destroy one actor this service spawned, by handle (idempotent)
 * - destroy_all: Destroy every actor this service spawned this session (idempotent)
 * - list_spawned: List the actors this service spawned this session
 *
 * Selector grammar (strict — anything else is INVALID_SELECTOR):
 *   "server"      -> PIE instance 0: the standalone world, the listen host, or the dedicated server.
 *   "client"      -> the single client world (instance >= 1). AMBIGUOUS_WORLD if more than one
 *                    client exists; WORLD_NOT_FOUND if there is none.
 *   "client:N"    -> client instance N, where N >= 1.
 *   "instance:N"  -> the exact PIE instance N, where N >= 0.
 * Worlds are resolved from GEditor->GetWorldContexts() (EWorldType::PIE only); the editor world,
 * GEditor->PlayWorld, and stale /Memory/UEDPIE_* shells are never used. No PIE -> PIE_NOT_RUNNING.
 *
 * Authority: a spawn into a client world (GetNetMode() == NM_Client) is refused with
 * CLIENT_REQUIRES_OPT_IN unless allow_client_local is passed — a client-local actor is cosmetic and
 * never replicates. Spawn on "server" for anything that must exist for all players.
 *
 * Handles are "<session_serial>:<guid>". They live only for the PIE session that produced them:
 * FEditorDelegates::EndPIE bumps the session serial and clears the registry, so a handle from an
 * earlier session reports STALE_HANDLE. Only actors THIS service registered are ever destroyed —
 * never by name, tag, or class search.
 *
 * Python usage:
 *   import unreal, json
 *   info = json.loads(unreal.PIEActorService.resolve_world("server"))
 *   res  = json.loads(unreal.PIEActorService.spawn_actor(
 *              "server", "/Script/Engine.StaticMeshActor", unreal.Transform()))
 *   handle = res["handle"]
 *   # ... run a check against the spawned actor ...
 *   unreal.PIEActorService.destroy_all()
 */
UCLASS(BlueprintType)
class VIBEUE_API UPIEActorService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Resolve a running PIE world by selector and report its net role.
	 *
	 * @param WorldSelector - "server", "client", "client:N" (N>=1), or "instance:N" (N>=0)
	 * @return JSON: {success, world_path, net_mode, pie_instance, is_authority, actor_count} or
	 *         {success:false, error_code, message}. error_code: INVALID_SELECTOR, PIE_NOT_RUNNING,
	 *         WORLD_NOT_FOUND, AMBIGUOUS_WORLD.
	 *
	 * Example:
	 *   unreal.PIEActorService.resolve_world("server")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|PIE|Actors")
	static FString ResolveWorld(const FString& WorldSelector);

	/**
	 * Spawn a transient actor into a resolved PIE world.
	 *
	 * Validation happens before any mutation, in order: selector -> class -> transform -> collision.
	 * Class paths accepted: /Game/Foo/BP_X, /Game/Foo/BP_X.BP_X, /Game/Foo/BP_X.BP_X_C, and native
	 * /Script/Engine.StaticMeshActor. A UBlueprint whose generated class is missing or dirty is
	 * compiled first. No package is saved.
	 *
	 * @param WorldSelector - see resolve_world
	 * @param ClassPath - actor class or Blueprint path
	 * @param Transform - spawn transform (every component must be finite)
	 * @param CollisionHandling - AlwaysSpawn (default), AdjustIfPossibleButAlwaysSpawn,
	 *        AdjustIfPossibleButDontSpawnIfColliding, or DontSpawnIfColliding
	 * @param bAllowClientLocal - allow a cosmetic, non-replicated spawn into a client world
	 * @param Label - optional editor label (SetActorLabel) applied only WITH_EDITOR
	 * @return JSON: {success, handle, actor_path, actor_name, class_path, world_path, net_mode,
	 *         pie_instance, is_authority, location:{x,y,z}, rotation:{pitch,yaw,roll}} or
	 *         {success:false, error_code, message}. error_code: INVALID_SELECTOR, PIE_NOT_RUNNING,
	 *         WORLD_NOT_FOUND, AMBIGUOUS_WORLD, CLASS_NOT_FOUND, NOT_AN_ACTOR_CLASS, CLASS_ABSTRACT,
	 *         CLASS_DEPRECATED, CLASS_REINSTANCED, INVALID_TRANSFORM, INVALID_COLLISION_HANDLING,
	 *         CLIENT_REQUIRES_OPT_IN, SPAWN_REJECTED.
	 *
	 * Example:
	 *   unreal.PIEActorService.spawn_actor("server", "/Script/Engine.StaticMeshActor", unreal.Transform())
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|PIE|Actors")
	static FString SpawnActor(
		const FString& WorldSelector,
		const FString& ClassPath,
		const FTransform& Transform,
		const FString& CollisionHandling = TEXT("AlwaysSpawn"),
		bool bAllowClientLocal = false,
		const FString& Label = TEXT(""));

	/**
	 * Destroy one actor this service spawned, by handle. Idempotent per handle: the record is kept
	 * after the actor is destroyed, so a repeat call on the same handle succeeds with
	 * already_gone:true (never UNKNOWN_HANDLE). A consumed handle stays in list_spawned as
	 * alive:false until PIE ends (EndPIE clears the whole registry).
	 *
	 * @param Handle - the handle string returned by spawn_actor ("<serial>:<guid>")
	 * @return JSON: {success, handle, already_gone} or {success:false, error_code, message}.
	 *         error_code: UNKNOWN_HANDLE (never issued this session), STALE_HANDLE (from an earlier
	 *         session).
	 *
	 * Example:
	 *   unreal.PIEActorService.destroy_actor(handle)
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|PIE|Actors")
	static FString DestroyActor(const FString& Handle);

	/**
	 * Destroy every actor this service spawned this session. Harmless when nothing is registered and
	 * idempotent: records are kept, so an actor already consumed this session counts under
	 * already_gone (not destroyed) and stays in list_spawned as alive:false until PIE ends.
	 *
	 * @return JSON: {success, destroyed, already_gone}
	 *
	 * Example:
	 *   unreal.PIEActorService.destroy_all()
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|PIE|Actors")
	static FString DestroyAll();

	/**
	 * List the actors this service spawned this session (only the current session's records). A
	 * handle that has been destroyed remains listed with alive:false until PIE ends, so this is a
	 * full spawn ledger for the session, not just the live actors.
	 *
	 * @return JSON: {success, count, spawned:[{handle, actor_path, class_path, world_path, alive}, ...]}
	 *
	 * Example:
	 *   unreal.PIEActorService.list_spawned()
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|PIE|Actors")
	static FString ListSpawned();

	/**
	 * Unhook the FEditorDelegates::EndPIE callback and drop the spawn registry. Called from the
	 * module's ShutdownModule. The hook is a raw static function pointer INTO THIS DLL, so leaving
	 * it registered past an unload (plugin reload / Live Coding) leaves FEditorDelegates holding a
	 * dangling callback — the same class of teardown bug the module unhooks its other delegates for.
	 * Safe no-op when nothing was ever spawned. GAME-THREAD ONLY.
	 */
	static void ShutdownEndPIEHook();

private:
	// Test-only surface (no UFUNCTION): let the automation tests exercise the pure validators and
	// read the session serial without a running PIE session. Reached through the friend struct.
	static FString ValidateSelectorCode(const FString& WorldSelector);
	static FString ValidateCollisionCode(const FString& CollisionHandling);
	static FString ValidateClassCode(const FString& ClassPath);
	static int32 GetSessionSerialForTest();

	friend struct FPIEActorServiceTestAccess;
};
