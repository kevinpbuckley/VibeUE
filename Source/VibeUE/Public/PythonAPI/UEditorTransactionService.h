// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UEditorTransactionService.generated.h"

/**
 * Information about a single transaction in the undo/redo history.
 */
USTRUCT(BlueprintType)
struct FTransactionInfo
{
	GENERATED_BODY()

	/** Index in the transaction buffer */
	UPROPERTY(BlueprintReadWrite, Category = "Transaction")
	int32 Index = -1;

	/** Human-readable title of the transaction */
	UPROPERTY(BlueprintReadWrite, Category = "Transaction")
	FString Title;

	/** Whether this is the current transaction (next to undo) */
	UPROPERTY(BlueprintReadWrite, Category = "Transaction")
	bool bIsCurrent = false;
};

/**
 * Result of an undo or redo operation.
 */
USTRUCT(BlueprintType)
struct FTransactionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Transaction")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadWrite, Category = "Transaction")
	FString Message;
};

/**
 * Editor Transaction Service - Python API for Unreal Engine undo/redo system.
 *
 * Provides full access to the editor's transaction (undo/redo) system:
 * - Undo/redo operations
 * - Transaction grouping (begin/end)
 * - Transaction history inspection
 * - Transaction buffer management
 *
 * Most VibeUE services (ActorService, BlueprintService, etc.) already wrap their
 * operations in transactions automatically. This service lets you:
 * 1. Undo/redo those operations from Python
 * 2. Group multiple operations into a single undoable transaction
 * 3. Inspect what's in the undo/redo history
 *
 * Python Usage:
 *   import unreal
 *
 *   # Undo the last operation
 *   result = unreal.EditorTransactionService.undo()
 *
 *   # Redo the last undone operation
 *   result = unreal.EditorTransactionService.redo()
 *
 *   # Group multiple operations into one undo step
 *   unreal.EditorTransactionService.begin_transaction("Build Castle")
 *   # ... multiple actor spawns, property changes, etc. ...
 *   unreal.EditorTransactionService.end_transaction()
 *   # Now a single undo() reverts ALL of them
 *
 *   # Check undo history
 *   can_undo = unreal.EditorTransactionService.can_undo()
 *   history = unreal.EditorTransactionService.get_undo_history(10)
 */
UCLASS(BlueprintType)
class VIBEUE_API UEditorTransactionService : public UObject
{
	GENERATED_BODY()

public:
	// ═══════════════════════════════════════════════════════════════════
	// Undo / Redo
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Undo the last editor transaction.
	 *
	 * @return Result with success status and description of what was undone
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static FTransactionResult Undo();

	/**
	 * Redo the last undone editor transaction.
	 *
	 * @return Result with success status and description of what was redone
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static FTransactionResult Redo();

	/**
	 * Undo multiple transactions at once.
	 *
	 * @param Count - Number of transactions to undo
	 * @return Result with success status and count of transactions undone
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static FTransactionResult UndoMultiple(int32 Count);

	/**
	 * Redo multiple transactions at once.
	 *
	 * @param Count - Number of transactions to redo
	 * @return Result with success status and count of transactions redone
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static FTransactionResult RedoMultiple(int32 Count);

	// ═══════════════════════════════════════════════════════════════════
	// Transaction Grouping
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Begin a named transaction. All subsequent editor operations will be grouped
	 * under this transaction until EndTransaction() is called.
	 * A single Undo() will revert everything in the group.
	 *
	 * @param Description - Human-readable name for the transaction (shown in Edit > Undo)
	 * @return True if transaction was started
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static bool BeginTransaction(const FString& Description);

	/**
	 * End the current transaction group started by BeginTransaction().
	 *
	 * @return True if a transaction was ended
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static bool EndTransaction();

	/**
	 * Cancel the current transaction group, reverting all operations since
	 * the matching BeginTransaction() call.
	 * Internally ends the transaction then immediately undoes it.
	 *
	 * @return Result with success status and description of what was reverted
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static FTransactionResult CancelTransaction();

	// ═══════════════════════════════════════════════════════════════════
	// History & Inspection
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Check whether there are any transactions available to undo.
	 *
	 * @return True if undo is available
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static bool CanUndo();

	/**
	 * Check whether there are any transactions available to redo.
	 *
	 * @return True if redo is available
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static bool CanRedo();

	/**
	 * Get the title/description of the next transaction that would be undone.
	 *
	 * @return Transaction title, or empty string if nothing to undo
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static FString GetUndoDescription();

	/**
	 * Get the title/description of the next transaction that would be redone.
	 *
	 * @return Transaction title, or empty string if nothing to redo
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static FString GetRedoDescription();

	/**
	 * Get the recent undo history (transactions that can be undone).
	 *
	 * @param MaxEntries - Maximum number of history entries to return (default 20)
	 * @return Array of transaction info, most recent first
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static TArray<FTransactionInfo> GetUndoHistory(int32 MaxEntries = 20);

	/**
	 * Get the redo history (transactions that were undone and can be redone).
	 *
	 * @param MaxEntries - Maximum number of history entries to return (default 20)
	 * @return Array of transaction info, most recent first
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static TArray<FTransactionInfo> GetRedoHistory(int32 MaxEntries = 20);

	/**
	 * Get the total number of transactions in the undo buffer.
	 *
	 * @return Number of undoable transactions
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static int32 GetUndoCount();

	/**
	 * Get the total number of transactions in the redo buffer.
	 *
	 * @return Number of redoable transactions
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static int32 GetRedoCount();

	// ═══════════════════════════════════════════════════════════════════
	// Buffer Management
	// ═══════════════════════════════════════════════════════════════════

	/**
	 * Reset the entire undo/redo history. This cannot be undone.
	 *
	 * @param Reason - Reason for clearing (logged for diagnostics)
	 * @return True if buffer was reset
	 */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transaction")
	static bool ResetHistory(const FString& Reason = TEXT("Manual reset"));
};
