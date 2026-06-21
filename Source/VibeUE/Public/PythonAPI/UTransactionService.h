// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UTransactionService.generated.h"

/**
 * A single entry in the editor undo/redo history.
 */
USTRUCT(BlueprintType)
struct FTransactionHistoryEntry
{
	GENERATED_BODY()

	/** Human-readable title shown in the Edit menu (e.g. "Move Actor"). */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FString Title;

	/** True if this entry is currently undone (i.e. it is a redo candidate). */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	bool bIsUndone = false;

	/** 0-based index within the editor transaction queue. */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	int32 QueueIndex = 0;
};

/**
 * Snapshot of the editor's undo/redo state.
 */
USTRUCT(BlueprintType)
struct FTransactionState
{
	GENERATED_BODY()

	/** Whether an undo can be performed right now. */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	bool bCanUndo = false;

	/** Whether a redo can be performed right now. */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	bool bCanRedo = false;

	/** Number of transactions that can still be undone. */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	int32 UndoCount = 0;

	/** Number of transactions that can be redone. */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	int32 RedoCount = 0;

	/** Title of the next undo ("" if none). */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FString NextUndoTitle;

	/** Title of the next redo ("" if none). */
	UPROPERTY(BlueprintReadOnly, Category = "Transaction")
	FString NextRedoTitle;
};

/**
 * Transaction Service - Python API for the editor undo/redo system (issue #469).
 *
 * Wraps GEditor's transaction buffer (GEditor->Trans) so agents can drive undo/redo,
 * group edits into a single named transaction, inspect history, and reset the buffer —
 * the analogue of pressing Ctrl+Z / Ctrl+Y or using the Edit menu.
 *
 * IMPORTANT: Most VibeUE service edits already wrap themselves in their own transactions,
 * so undo() typically reverts the last whole operation. To group MULTIPLE edits into one
 * undo step, call begin_transaction("My change"), perform the edits, then end_transaction().
 *
 * Python Usage:
 *   import unreal
 *   svc = unreal.TransactionService
 *
 *   # Group several edits into one undo step
 *   svc.begin_transaction("Build wall")
 *   # ... spawn/modify actors ...
 *   svc.end_transaction()
 *
 *   svc.undo()                 # revert it
 *   svc.redo()                 # re-apply it
 *
 *   state = svc.get_state()    # can_undo/can_redo, undo/redo counts + next titles
 *   print(state.undo_count, state.next_undo_title)
 *   for h in svc.get_history(20):
 *       print(h.title, "(undone)" if h.is_undone else "")
 *
 *   svc.reset("Test cleanup")  # clears the undo buffer (does NOT touch the world)
 */
UCLASS(BlueprintType)
class VIBEUE_API UTransactionService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// =================================================================
	// Undo / Redo
	// =================================================================

	/** Undo the most recent transaction. Returns true if an undo was performed. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static bool Undo();

	/** Redo the most recently undone transaction. Returns true if a redo was performed. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static bool Redo();

	/** Undo the last N transactions. Returns the number actually undone. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static int32 UndoMultiple(int32 Count);

	/** Redo the last N undone transactions. Returns the number actually redone. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static int32 RedoMultiple(int32 Count);

	// =================================================================
	// Grouping
	// =================================================================

	/**
	 * Begin a named transaction so subsequent edits collapse into a single undo step.
	 * Pair with end_transaction (or cancel_transaction). Returns the transaction index
	 * (>=0), or -1 on failure.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static int32 BeginTransaction(const FString& Description);

	/** End the active transaction opened with begin_transaction. Returns the index closed. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static int32 EndTransaction();

	/**
	 * Cancel the active transaction, rolling back everything recorded since begin_transaction.
	 * Returns true if a transaction was cancelled.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static bool CancelTransaction();

	// =================================================================
	// Inspection
	// =================================================================

	/** Snapshot of the undo/redo state (counts, can_undo/can_redo, next titles). */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static FTransactionState GetState();

	/** True if an undo can be performed. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static bool CanUndo();

	/** True if a redo can be performed. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static bool CanRedo();

	/** Title of the next undo (the description shown next to Edit > Undo). "" if none. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static FString GetUndoDescription();

	/** Title of the next redo. "" if none. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static FString GetRedoDescription();

	/**
	 * List the undo history (most recent last), up to MaxEntries (<=0 = all).
	 * Each entry's is_undone flag marks redo candidates.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static TArray<FTransactionHistoryEntry> GetHistory(int32 MaxEntries = 20);

	// =================================================================
	// Buffer
	// =================================================================

	/**
	 * Reset (clear) the entire undo/redo buffer. This clears HISTORY only — it does NOT
	 * change anything in the level/world. Returns true on success.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static bool ResetBuffer(const FString& Reason);
};
