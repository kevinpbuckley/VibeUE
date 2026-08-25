// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UWorkflowService.generated.h"

/** High-level, evidence-producing workflows built from VibeUE's lower-level editor primitives. */
UCLASS(BlueprintType)
class VIBEUE_API UWorkflowService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** Authoritative project/engine/toolchain/build context as JSON. Read-only. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow")
	static FString GetEnvironment();

	/** Start an opt-in durable task journal under Saved/VibeUE/Runs. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|Journal")
	static FString StartRun(const FString& Name, const FString& MetadataJson = TEXT("{}"));

	/** Add a note to an active or historical run. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|Journal")
	static FString AddRunNote(const FString& RunId, const FString& Text);

	/** Attach a file/capture/scenario/build artifact to a run. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|Journal")
	static FString AttachRunArtifact(const FString& RunId, const FString& PathOrId, const FString& Kind = TEXT("file"));

	/** Finish a run and write final JSON plus readable Markdown atomically. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|Journal")
	static FString FinishRun(const FString& RunId, const FString& Outcome, const FString& Summary);

	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|Journal")
	static FString GetRun(const FString& RunId);

	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|Journal")
	static FString ListRuns(int32 Limit = 50);

	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|Journal")
	static FString DeleteRun(const FString& RunId);

	/**
	 * Queue a deterministic PIE scenario. Returns a scenario id immediately; poll GetScenario until
	 * status is passed/failed/cancelled. Teardown is guaranteed on every terminal path.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|PIE")
	static FString RunScenario(const FString& ScenarioJson);

	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|PIE")
	static FString GetScenario(const FString& ScenarioId);

	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|PIE")
	static FString CancelScenario(const FString& ScenarioId);

	/**
	 * Execute or dry-run an explicit, bounded maintenance plan. Supported operations:
	 * interface_add, interface_remove, variable_metadata, asset_move, cleanup_review.
	 * Plan JSON owns targets/options; dry-run is the default and cleanup never deletes.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|Bulk")
	static FString RunBulkMaintenance(const FString& PlanJson, bool bApply = false, int32 BatchSize = 20,
		bool bStopOnError = false);

	/** Module lifecycle hooks for mutation observation and interrupted-run recovery. */
	static void InitializeJournal();
	static void ShutdownJournal();

	/** Internal workflow composition helpers. They intentionally are not exposed as agent tools. */
	static FString GetActiveRunId();
	static void AttachActiveRunArtifact(const FString& PathOrId, const FString& Kind);
	static FString GetOptionalGameIQImpact(const FString& TopicOrId);
};
