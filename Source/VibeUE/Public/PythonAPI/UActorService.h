// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UActorService.generated.h"

/**
 * Actor Service - one capability the engine's toolsets and Python do not expose.
 *
 * Cut to the delta on the 5-9 branch. Use the engine for everything else:
 *  - transforms, labels, tags, components, bounds, look-at, reparenting, actor diff:
 *    `EditorToolset.ActorToolset` (GetActorTransform / SetActorTransform / AddComponent /
 *    RemoveComponent / GetComponents / SetParentComponent / GetActorBounds / LookAt / GetLabel /
 *    SetLabel / AddTag / RemoveTag / HasTag / GetTags / Diff / GetCanonicalActorState)
 *  - finding, spawning, destroying, selecting actors and loading levels:
 *    `EditorToolset.LevelToolset`
 *  - any other property, including the lock-location and absolute-transform flags VibeUE used to
 *    wrap: `EditorToolset.ObjectToolset` (GetProperties / SetProperties / ListProperties)
 *  - viewport camera and framing: `EditorToolset.EditorAppToolset`
 *    (GetCameraTransform / SetCameraTransform / FocusOnActors)
 *
 * What is left is the one thing none of those can do.
 */
UCLASS(BlueprintType)
class VIBEUE_API UActorService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Lock or read an actor's location lock (the padlock in the details panel).
	 *
	 * NOT reachable through EditorToolset.ObjectToolset: AActor::bLockLocation is a bare
	 * UPROPERTY() with neither CPF_Edit nor CPF_BlueprintVisible, and ObjectToolset routes every
	 * access through CanGetPropertyValue/CheckCanSetProperty, which refuse exactly that shape.
	 * UE5 has no per-actor rotation or scale lock -- location only.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static bool SetActorLockLocation(const FString& ActorNameOrLabel, bool bLocked);

	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static bool GetActorLockLocation(const FString& ActorNameOrLabel, bool& OutLocked);

	/**
	 * Re-run a placed Blueprint actor's construction script in the editor world. This is the
	 * capability Python and the engine toolsets otherwise lack — after changing a component
	 * template or an instance's exposed variable, the placed actor's construction script does not
	 * re-execute on its own, so the SCS-driven state goes stale. Editor world only; refuses while a
	 * PIE session is running.
	 *
	 * @param ActorLabelOrPath - Name or label of the actor
	 * @return True if the actor was found and RerunConstructionScripts was called
	 *
	 * Example:
	 *   unreal.ActorService.rerun_construction_scripts("BP_Refrigerator_2")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Actors")
	static bool RerunConstructionScripts(const FString& ActorLabelOrPath);

	/** Resolve an actor in the editor world by object name or display label. Not a tool; shared with
	 * other services (StateTreeService) that take an actor label. */
	static class AActor* FindActorByIdentifier(const FString& NameOrLabel);
};
