// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "PlayInEditorDataTypes.h"
#endif
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

	/** Read-only feature negotiation. Python: get_scenario_capabilities().
	 * Live exclusive PIE remains false until ownership and teardown are qualified.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|PIE")
	static FString GetScenarioCapabilities();

	/**
	 * Queue a PIE scenario. Poll GetScenario until terminal; cancellation stays
	 * running while owned startup/teardown is pending. Normal runs require an
	 * assertion; smoke:true permits assertion-free runs but never reports passed.
	 * Optional dependencies bind evidence to their file content and engine build.
	 * exclusive_pie reserves the runner before preflight and refuses foreign active/queued PIE.
	 * Only this scenario's tagged request/session and bound world may be stopped or receive input.
	 * Nonexclusive observation may borrow existing PIE, but never owns its teardown.
	 * inject_key targets the owned player controller, not global Slate/UI focus.
	 * Python assertions and console commands are trusted code, not a sandbox.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Workflow|PIE")
	static FString RunScenario(const FString& ScenarioJson);

	/** Recheck tracked evidence. Changed inputs return stale/passed=false with historicalPassed preserved.
	 * verifiedCurrent covers declared files, the VibeUE binary and engine version, not unsaved state.
	 */
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

#if WITH_DEV_AUTOMATION_TESTS
/** Internal, unreflected editor-boundary fixture. Never exposed to Python/tools. */
struct FWorkflowScenarioTestState
{
	bool bEditorAvailable = true;
	bool bExclusivePIEQualified = true;
	TOptional<FRequestPlaySessionParams> PlayRequest;
	TOptional<FPlayInEditorSessionInfo> PlaySession;
	TWeakObjectPtr<UWorld> PlayWorld;
	uint64 Frame = 0;
	int32 StartCalls = 0;
	int32 CancelCalls = 0;
	int32 EndCalls = 0;
	int32 MutationCalls = 0;
	int32 PreflightCalls = 0;
	int32 ReportWrites = 0;
	bool bPreflightSuccess = true;
	bool bCompleteEndPlay = true;
	TFunction<void(const FString&)> OnPreflight;
	TFunction<void()> OnRequestQueued;
	TFunction<void()> OnMutation;
};

/** Scoped binding refuses active scenarios, isolates reports, and restores terminal history. */
class FScopedWorkflowScenarioTest
{
public:
	explicit FScopedWorkflowScenarioTest(FWorkflowScenarioTestState& State);
	~FScopedWorkflowScenarioTest();
	bool IsBound() const { return bBound; }
	void Tick(bool bAdvanceFrame = true);
	void NotifyPIEBegin();
	void NotifyPIEStarted();
	FScopedWorkflowScenarioTest(const FScopedWorkflowScenarioTest&) = delete;
	FScopedWorkflowScenarioTest& operator=(const FScopedWorkflowScenarioTest&) = delete;
private:
	bool bBound = false;
};
#endif
