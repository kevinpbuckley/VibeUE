// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UTransactionService.generated.h"

/**
 * Transaction Service - driving the editor's undo/redo stepping from script (issue #469).
 *
 * Trimmed on the 5-9 branch to the half the engine does not expose:
 *  - Grouping edits into one named transaction is native: `with unreal.ScopedEditorTransaction("..."):`
 *    (PythonScriptPlugin), so begin_transaction / end_transaction / cancel_transaction were cut.
 *  - Reading history is native: UE 5.9's EditorToolset.UndoHistoryToolset serves
 *    GetRecentTransactions / GetTransactionDetails / GetTransactionRecencyIndex /
 *    SearchTransactions, so get_state / get_history / get_undo_description /
 *    get_redo_description were cut.
 *  - **Stepping** the buffer is not exposed anywhere: Python has no undo/redo binding and GEditor
 *    is not scriptable, so these stay. That is the whole remaining justification for this service.
 *
 * IMPORTANT: most VibeUE service edits already wrap themselves in their own transactions, so
 * undo() typically reverts the last whole operation. To group MULTIPLE edits into one undo step,
 * use unreal.ScopedEditorTransaction around them.
 *
 * Python Usage:
 *   import unreal
 *   svc = unreal.TransactionService
 *
 *   with unreal.ScopedEditorTransaction("Build wall"):
 *       pass  # ... spawn/modify actors ...
 *
 *   if svc.can_undo():
 *       svc.undo()             # revert it
 *   svc.redo()                 # re-apply it
 *   svc.undo_multiple(3)       # step back three transactions
 *
 *   svc.reset_buffer("Test cleanup")  # clears the undo buffer (does NOT touch the world)
 *
 * For "what is in the buffer?", call the engine's UndoHistoryToolset via call_tool.
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
	// Inspection
	// =================================================================

	/** True if an undo can be performed. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static bool CanUndo();

	/** True if a redo can be performed. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static bool CanRedo();

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
