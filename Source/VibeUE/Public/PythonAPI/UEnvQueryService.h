// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UEnvQueryService.generated.h"

/** One item a query run produced, with the score it was ranked by. */
USTRUCT(BlueprintType)
struct FEQSResultItem
{
	GENERATED_BODY()

	/**
	 * The item's world location. Populated for every item type derived from
	 * UEnvQueryItemType_VectorBase — which includes the actor types, since
	 * UEnvQueryItemType_ActorBase derives from it. Zero for an item type that is neither.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FVector Location = FVector::ZeroVector;

	/**
	 * The item's actor, by UObject name, or EMPTY when the query generated locations rather than
	 * actors. Empty is therefore not a failure — it is what a point-generator query looks like.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString ActorName;

	/**
	 * The item's final score. Whether it is normalized to 0..1 is decided by the generator's
	 * UEnvQueryGenerator::GetNormalizationOption AND by the RunMode this call was asked for — which
	 * makes it partly this service's argument, not purely the generator's choice.
	 *
	 * FEnvQueryInstance::FinalizeQuery resolves the generator's Default option differently per branch:
	 * under "AllMatching" Default falls through to Normalized (EnvQueryInstance.cpp:865-868), under
	 * every other mode it falls through to Unaltered (:820-822, :837-839). So the same query on the
	 * same generator normalizes at AllMatching and does not at SingleResult — a live PIE run measured
	 * 0.9517..1.0000 at AllMatching and a best score of 1.0508 at SingleResult on one query. Compare
	 * scores within one run of one mode, never across modes and never across queries.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	float Score = 0.0f;

	/**
	 * Whether the item passed every test — FEnvQueryItem::IsValid().
	 *
	 * True for every item under "AllMatching", and that is a fact about the engine rather than about
	 * this service: FEnvQueryInstance::FinalizeQuery truncates Items to NumValidItems there
	 * (EnvQueryInstance.cpp:855). It is NOT true under the three single-pick modes, which never
	 * truncate in an editor build — failed items come back alongside the passing ones. So this is read
	 * from each item rather than assumed, and a caller filtering results must actually test it. See
	 * FEQSRunResult::Items for the mechanism.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	bool bPassed = false;
};

/** The outcome of one RunQuery execution. */
USTRUCT(BlueprintType)
struct FEQSRunResult
{
	GENERATED_BODY()

	/**
	 * True only when the query RAN and finished with EEnvQueryStatus::Success.
	 *
	 * A query that executed correctly and matched nothing is false, with Status "Failed" — that is
	 * the engine's own word for it (FinalizeQuery marks a run with no valid items as failed), and
	 * folding it into success would report an empty result set as a good one.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	bool bSuccess = false;

	/** Populated whenever bSuccess is false. Empty otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Error;

	/**
	 * The engine's EEnvQueryStatus name: "Success", "Failed", "Aborted", "OwnerLost",
	 * "MissingParam", "Processing".
	 *
	 * EMPTY when the query never ran — a rejected RunMode, no play session, an unresolved asset or
	 * querier. Status and Error together therefore say which side of the call failed: a refusal has
	 * an Error and no Status, an executed-but-unsuccessful query has both.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Status;

	/** Items.Num(), reported so a caller can check the count without walking the array. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	int32 ItemCount = 0;

	/**
	 * The items. Empty for a refusal, and also for a query that ran and matched nothing.
	 *
	 * Only "AllMatching" gives the tidy contract — best first, failed items dropped, scores normalized
	 * unless the generator opted out (EnvQueryInstance.cpp:848-870). The three single-pick modes return
	 * neither one item nor a reliably ranked list, and that surprises everyone exactly once:
	 *
	 *   - FEnvQueryInstance::PickSingleItem keeps the WHOLE item array whenever bStoreDebugInfo is set
	 *     (:773-795). It swaps the pick to index 0 and returns everything else untruncated, failed
	 *     items included. That flag defaults to true in every non-shipping build
	 *     (FEnvQueryInstance::bDebuggingInfoEnabled, :107; USE_EQS_DEBUGGER, EnvQueryTypes.h:33) —
	 *     which is every build this editor-only service can run in.
	 *   - "SingleResult" has a second exit on top of that. When the last test was a pure condition
	 *     (bFoundSingleResult || bPassOnSingleResult) FinalizeQuery skips its whole SingleResult body
	 *     (:811-830): no SortScores, no normalization and no pick at all, so the items come back in
	 *     GENERATION order carrying raw scores. A live PIE run measured exactly that — 27 items with a
	 *     best score of 1.0508, against the same query's 27 items sorted 1.0000..0.9517 at AllMatching.
	 *
	 * So: treat index 0 as the engine's answer and the array as a ranking only under AllMatching.
	 * Under the other three, index 0 is the pick when a pick happened at all, the remaining entries are
	 * the debugger's view of the run rather than runners-up, and bPassed is per item for a reason.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	TArray<FEQSResultItem> Items;
};

/**
 * Environment Query Service - the two EQS capabilities the engine does not provide.
 *
 * Cut to the delta on the 5-9 branch. UE 5.9's `AIModuleToolset.EnvironmentQueryToolset`
 * (14 tools) owns EQS authoring and inspection; use it for everything else:
 *  - create the asset: `CreateEnvironmentQuery`
 *  - options and tests: `AddOption` (it takes the generator class, so there is no separate
 *    set-generator step), `RemoveOption`, `MoveOption`, `AddTest`, `RemoveTest`, `MoveTest`,
 *    `SetTestEnabled`
 *  - read the graph: `GetRootGraphNode`, `GetOptionGraphNodes`, `GetTestGraphNodes`,
 *    `GetOptionInstance`, `GetTestInstance`, `GetGenerator`
 *  - node and test properties of any kind: `EditorToolset.ObjectToolset`
 *    (`GetProperties` / `SetProperties` / `ListProperties`) on the instance those getters return
 *  - available generator/test/context classes: `ObjectToolset.SearchSubclasses`, or
 *    `search_symbols` / `get_symbol`
 *  - find, save and validate assets: `EditorToolset.AssetToolset` (`FindAssets`, `SaveAsset`,
 *    `ValidateAssets`)
 *
 * What is left is what no engine tool does: running a query, and repairing a sparse graph.
 *
 * Requires the EnvironmentQueryEditor plugin. Without it both methods report the feature as
 * unavailable rather than failing to build.
 */
UCLASS(BlueprintType)
class VIBEUE_API UEnvQueryService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Rebuild the option and test nodes of a sparse graph from UEnvQuery::Options, then lay out,
	 * commit and save. Empty on success, else the reason. This is the cure for the "sparse graph"
	 * shape: options live in UEnvQuery::Options with no matching nodes in the editor graph.
	 *
	 * The reconstruction is UEnvironmentQueryGraph::SpawnMissingNodes(), reached through
	 * Initialize() — it REUSES the live UEnvQueryOption / UEnvQueryTest objects rather than
	 * recreating them (EnvironmentQueryGraph.cpp:426-446), so a repaired query keeps every property
	 * ever authored on it.
	 *
	 * Refuses when there is nothing to repair — no options, or the graph already has option nodes
	 * below the root — so a second run cannot duplicate anything, and so it is never a way to
	 * "just recompile" a healthy query. It also refuses on a graph with no ROOT node, which
	 * is not a repairable shape but a destructive one: there would be nothing to link the rebuilt
	 * options to, and the commit would empty the option list it was asked to recover.
	 *
	 * It cannot repair an option whose node exists in the graph but is unlinked from the root (the
	 * "orphaned option" shape): SpawnMissingNodes skips any option that already has a node, so it
	 * is never relinked. That one is a hand fix in the Environment Query editor.
	 *
	 * Honest caveat, and the one thing worth knowing before reaching for this: on EQS the
	 * reconstruction is NOT exclusive to this call, and the sparse shape therefore self-heals.
	 * UEnvironmentQueryGraph::Initialize() IS LockUpdates + SpawnMissingNodes + CalculateAllWeights +
	 * UnlockUpdates (EnvironmentQueryGraph.cpp:168-176), the EQS asset editor calls Initialize() on
	 * every open (EnvironmentQueryEditor.cpp:224), and this service's own EnsureGraph calls it on
	 * every write. So opening a sparse query in the editor, or running any mutator against it,
	 * repairs it too.
	 *
	 * This is a genuine divergence from BehaviorTree, and NOT the one usually quoted: both editors
	 * call Initialize() unconditionally, outside their null-EdGraph branch (BehaviorTreeEditor.cpp
	 * :365, EnvironmentQueryEditor.cpp:224). The difference is the body —
	 * UBehaviorTreeGraph::Initialize() is Super::Initialize() + UpdateInjectedNodes()
	 * (BehaviorTreeGraph.cpp:202-206) and rebuilds nothing from the asset.
	 *
	 * What this call adds over opening or re-saving the asset is therefore the refusals, the
	 * before/after node counts in the log, and a name for the operation — not a capability those
	 * lack.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString RepairGraphFromOptions(const FString& AssetPath);

	/**
	 * Run the query against the live Play In Editor world, from QuerierActorNameOrLabel's position,
	 * and return the scored items. This is the call that turns authoring into a feedback loop:
	 * everything else on this service describes what a query SAYS, this one says what it DOES.
	 *
	 * REQUIRES a play session, and that is the inverse of every other method here. The rest are
	 * refused while PIE runs, because committing a graph renames the live UEnvQueryOption /
	 * UEnvQueryTest instances a running query may be executing. This one writes nothing — it loads
	 * with LoadObject and never opens the editor graph — so the write guard does not apply to it,
	 * and it needs the very world the others refuse to be near.
	 *
	 * The session it requires is specifically an IN-PROCESS one, and that has two blind spots worth
	 * knowing before they cost a debugging cycle. A session launched as a separate process —
	 * "Standalone Game", "Launch", anything with EPlaySessionDestinationType::NewProcess or Launcher
	 * — leaves GEditor->PlayWorld null, so this refuses while a game is visibly running on screen;
	 * use Play In Editor (selected viewport or new editor window) instead. And under net-PIE with
	 * several worlds in the one process, GEditor->PlayWorld names only ONE of them: the query runs
	 * against that world, which is not necessarily the window being watched.
	 *
	 * The querier is matched against each actor's object name AND its editor label, name first (a
	 * name is unique in a world, a label is not). It becomes the request's Owner, which is what
	 * UEnvQueryContext_Querier resolves to — so it is not merely a position, it is the actor the
	 * query is asked ON BEHALF OF, and every "distance to querier" test scores relative to it.
	 *
	 * RunMode is one of "SingleResult", "RandomBest5Pct", "RandomBest25Pct", "AllMatching",
	 * matched case-insensitively. An unrecognised value is REFUSED naming the valid set rather than
	 * defaulting: the modes differ in how many items come back and in whether the pick is random, so
	 * a silent fallback would answer a different question than the one asked and look like a result.
	 * The mode also decides whether the results are sorted, truncated to the passing items, and
	 * normalized — see FEQSRunResult::Items, which is where that stops being obvious.
	 *
	 * Execution is synchronous — UEnvQueryManager::RunInstantQuery steps the instance to completion
	 * before returning (EnvQueryManager.cpp:305-316) — so there is no delegate, no tick and no handle.
	 * The cost is paid on the calling thread: a heavy generator run at AllMatching stalls the editor
	 * for as long as the query takes.
	 *
	 * The query is read from DISK STATE, not from the editor graph: whatever the last commit wrote
	 * into UEnvQuery::Options is what runs. A query edited through this service is already committed
	 * by the time any setter returns, but one edited by hand in an open Environment Query editor tab
	 * and not saved will run as its saved version.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FEQSRunResult RunQuery(const FString& AssetPath, const FString& QuerierActorNameOrLabel,
		const FString& RunMode = TEXT("AllMatching"));
};
