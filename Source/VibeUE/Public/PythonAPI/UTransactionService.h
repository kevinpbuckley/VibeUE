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

	// =================================================================
	// Who made a transaction (#701)
	// =================================================================

	/**
	 * Where a transaction in the undo history came from, by its id (as EditorToolset.UndoHistoryToolset returns it):
	 *  - "user": it began while Slate was dispatching an input event (a key, mouse or analog event): a gizmo drag,
	 *    a details-panel edit, a menu entry, a hotkey. Input an agent injects through Slate (SlateInspectorToolset
	 *    PressKey / Click) counts too: to the editor it is input.
	 *  - "script": it began anywhere else: an execute_python script, an MCP tool call, a timer.
	 *  - "unknown": it began before VibeUE started recording (editor start-up), or the id is not valid.
	 * vibeue.editor_state() splits the undo history with it ("user_edits").
	 *
	 * Python:
	 *   for t in vibeue.recent_transactions(10): print(t["label"], t["origin"])
	 *   unreal.TransactionService.get_transaction_origin("<transaction id>")
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|Transactions")
	static FString GetTransactionOrigin(const FString& TransactionId);

	/** True while transaction origins are being recorded (from the end of editor start-up). */
	UFUNCTION(BlueprintCallable, Category = "VibeUE|Transactions")
	static bool IsTrackingOrigins();

	/** Start recording transaction origins (module start-up, once GEditor->Trans exists). Idempotent. */
	static void StartOriginTracking();

	/** Stop recording and forget the records (module shutdown). */
	static void StopOriginTracking();
};
