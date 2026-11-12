// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UTransactionService.generated.h"

/**
 * Transaction Service - stepping and clearing the editor's undo buffer (issue #469).
 *
 * Cut to the delta on the 5-9 branch. Use the engine for everything else:
 *  - grouping several edits into one named undo step is native Python:
 *    `with unreal.ScopedEditorTransaction("..."):` (PythonScriptPlugin)
 *  - asking what is IN the buffer -- the recent transactions, what each one touched, how recent a
 *    given one is: `EditorToolset.UndoHistoryToolset` (GetRecentTransactions /
 *    GetTransactionDetails / GetTransactionRecencyIndex / SearchTransactions), via call_tool
 *
 * What is left is what none of those does with a result. UndoHistoryToolset only READS the buffer,
 * and Python has no undo/redo binding. `ConsoleToolset.RunConsoleCommand("TRANSACTION UNDO")` /
 * `("TRANSACTION REDO")` does step the buffer (GEditor->Exec -> Exec_Transaction), but it reports no
 * success, cannot step N at once, and has no can-undo/can-redo query and no reset -- so this service
 * keeps undo/redo with a result, the multi-step and query forms, and clearing the buffer.
 *
 * Most VibeUE edits already open their own transaction, so undo() typically reverts the last whole
 * operation; wrap a batch in unreal.ScopedEditorTransaction to make it one step.
 *
 * Python:
 *   svc = unreal.TransactionService
 *   with unreal.ScopedEditorTransaction("Build wall"):
 *       pass  # ... spawn/modify actors ...
 *   if svc.can_undo():
 *       svc.undo()
 *   svc.undo_multiple(3)
 *   svc.reset_buffer("Test cleanup")  # clears HISTORY only; does NOT touch the world
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
