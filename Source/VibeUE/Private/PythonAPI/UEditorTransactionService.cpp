// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UEditorTransactionService.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "ScopedTransaction.h"

// ═══════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════

static UTransBuffer* GetTransBuffer()
{
	if (!GEditor) return nullptr;
	return CastChecked<UTransBuffer>(GEditor->Trans);
}

// ═══════════════════════════════════════════════════════════════════
// Undo / Redo
// ═══════════════════════════════════════════════════════════════════

FTransactionResult UEditorTransactionService::Undo()
{
	FTransactionResult Result;

	if (!GEditor || !GEditor->Trans)
	{
		Result.bSuccess = false;
		Result.Message = TEXT("No editor transaction system available");
		return Result;
	}

	if (!GEditor->Trans->CanUndo())
	{
		Result.bSuccess = false;
		Result.Message = TEXT("Nothing to undo");
		return Result;
	}

	FText UndoDescription = GEditor->Trans->GetUndoContext(false).Title;
	bool bSuccess = GEditor->UndoTransaction();
	Result.bSuccess = bSuccess;
	Result.Message = bSuccess
		? FString::Printf(TEXT("Undone: %s"), *UndoDescription.ToString())
		: TEXT("Undo failed");
	return Result;
}

FTransactionResult UEditorTransactionService::Redo()
{
	FTransactionResult Result;

	if (!GEditor || !GEditor->Trans)
	{
		Result.bSuccess = false;
		Result.Message = TEXT("No editor transaction system available");
		return Result;
	}

	if (!GEditor->Trans->CanRedo())
	{
		Result.bSuccess = false;
		Result.Message = TEXT("Nothing to redo");
		return Result;
	}

	FText RedoDescription = GEditor->Trans->GetRedoContext().Title;
	bool bSuccess = GEditor->RedoTransaction();
	Result.bSuccess = bSuccess;
	Result.Message = bSuccess
		? FString::Printf(TEXT("Redone: %s"), *RedoDescription.ToString())
		: TEXT("Redo failed");
	return Result;
}

FTransactionResult UEditorTransactionService::UndoMultiple(int32 Count)
{
	FTransactionResult Result;

	if (Count <= 0)
	{
		Result.bSuccess = false;
		Result.Message = TEXT("Count must be greater than 0");
		return Result;
	}

	int32 Undone = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		if (!GEditor || !GEditor->Trans || !GEditor->Trans->CanUndo())
		{
			break;
		}
		if (GEditor->UndoTransaction())
		{
			Undone++;
		}
		else
		{
			break;
		}
	}

	Result.bSuccess = Undone > 0;
	Result.Message = FString::Printf(TEXT("Undone %d of %d requested transactions"), Undone, Count);
	return Result;
}

FTransactionResult UEditorTransactionService::RedoMultiple(int32 Count)
{
	FTransactionResult Result;

	if (Count <= 0)
	{
		Result.bSuccess = false;
		Result.Message = TEXT("Count must be greater than 0");
		return Result;
	}

	int32 Redone = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		if (!GEditor || !GEditor->Trans || !GEditor->Trans->CanRedo())
		{
			break;
		}
		if (GEditor->RedoTransaction())
		{
			Redone++;
		}
		else
		{
			break;
		}
	}

	Result.bSuccess = Redone > 0;
	Result.Message = FString::Printf(TEXT("Redone %d of %d requested transactions"), Redone, Count);
	return Result;
}

// ═══════════════════════════════════════════════════════════════════
// Transaction Grouping
// ═══════════════════════════════════════════════════════════════════

bool UEditorTransactionService::BeginTransaction(const FString& Description)
{
	if (!GEditor)
	{
		return false;
	}

	GEditor->BeginTransaction(FText::FromString(Description));
	return true;
}

bool UEditorTransactionService::EndTransaction()
{
	if (!GEditor)
	{
		return false;
	}

	GEditor->EndTransaction();
	return true;
}

FTransactionResult UEditorTransactionService::CancelTransaction()
{
	FTransactionResult Result;

	if (!GEditor)
	{
		Result.bSuccess = false;
		Result.Message = TEXT("No editor available");
		return Result;
	}

	// End the transaction to finalize the record, then undo it.
	// Using GEditor->CancelTransaction() only discards the undo record
	// without reverting the actual changes, which leaves the world
	// in an inconsistent state and can cause TypedElement assertion crashes.
	GEditor->EndTransaction();

	if (GEditor->Trans && GEditor->Trans->CanUndo())
	{
		FText UndoDescription = GEditor->Trans->GetUndoContext(false).Title;
		bool bUndone = GEditor->UndoTransaction();
		Result.bSuccess = bUndone;
		Result.Message = bUndone
			? FString::Printf(TEXT("Cancelled and reverted: %s"), *UndoDescription.ToString())
			: TEXT("Transaction ended but undo failed");
	}
	else
	{
		Result.bSuccess = false;
		Result.Message = TEXT("Transaction ended but nothing to undo");
	}

	return Result;
}

// ═══════════════════════════════════════════════════════════════════
// History & Inspection
// ═══════════════════════════════════════════════════════════════════

bool UEditorTransactionService::CanUndo()
{
	return GEditor && GEditor->Trans && GEditor->Trans->CanUndo();
}

bool UEditorTransactionService::CanRedo()
{
	return GEditor && GEditor->Trans && GEditor->Trans->CanRedo();
}

FString UEditorTransactionService::GetUndoDescription()
{
	if (!GEditor || !GEditor->Trans || !GEditor->Trans->CanUndo())
	{
		return FString();
	}
	return GEditor->Trans->GetUndoContext(false).Title.ToString();
}

FString UEditorTransactionService::GetRedoDescription()
{
	if (!GEditor || !GEditor->Trans || !GEditor->Trans->CanRedo())
	{
		return FString();
	}
	return GEditor->Trans->GetRedoContext().Title.ToString();
}

TArray<FTransactionInfo> UEditorTransactionService::GetUndoHistory(int32 MaxEntries)
{
	TArray<FTransactionInfo> History;

	UTransBuffer* TransBuffer = GetTransBuffer();
	if (!TransBuffer)
	{
		return History;
	}

	// Walk the transaction buffer to get actual titles
	int32 QueueLength = TransBuffer->GetQueueLength();
	int32 UndoneCount = TransBuffer->GetUndoCount();
	// Undoable transactions are at indices [0, QueueLength - UndoneCount)
	int32 UndoableCount = QueueLength - UndoneCount;
	int32 Count = FMath::Min(MaxEntries, UndoableCount);

	for (int32 i = 0; i < Count; ++i)
	{
		// Walk from most recent undoable backwards
		int32 BufferIndex = UndoableCount - 1 - i;
		const FTransaction* Transaction = TransBuffer->GetTransaction(BufferIndex);

		FTransactionInfo Info;
		Info.Index = i;
		Info.Title = Transaction ? Transaction->GetContext().Title.ToString() : TEXT("[Transaction]");
		Info.bIsCurrent = (i == 0);
		History.Add(Info);
	}

	return History;
}

TArray<FTransactionInfo> UEditorTransactionService::GetRedoHistory(int32 MaxEntries)
{
	TArray<FTransactionInfo> History;

	UTransBuffer* TransBuffer = GetTransBuffer();
	if (!TransBuffer)
	{
		return History;
	}

	int32 QueueLength = TransBuffer->GetQueueLength();
	int32 UndoneCount = TransBuffer->GetUndoCount();
	// Redoable transactions are at indices [QueueLength - UndoneCount, QueueLength)
	int32 RedoStart = QueueLength - UndoneCount;
	int32 Count = FMath::Min(MaxEntries, UndoneCount);

	for (int32 i = 0; i < Count; ++i)
	{
		int32 BufferIndex = RedoStart + i;
		const FTransaction* Transaction = TransBuffer->GetTransaction(BufferIndex);

		FTransactionInfo Info;
		Info.Index = i;
		Info.Title = Transaction ? Transaction->GetContext().Title.ToString() : TEXT("[Transaction]");
		Info.bIsCurrent = (i == 0);
		History.Add(Info);
	}

	return History;
}

int32 UEditorTransactionService::GetUndoCount()
{
	UTransBuffer* TransBuffer = GetTransBuffer();
	if (!TransBuffer) return 0;
	// Undoable = total queue length minus the ones that have been undone
	return TransBuffer->GetQueueLength() - TransBuffer->GetUndoCount();
}

int32 UEditorTransactionService::GetRedoCount()
{
	UTransBuffer* TransBuffer = GetTransBuffer();
	// UndoCount in UE = number of undone transactions (available for redo)
	return TransBuffer ? TransBuffer->GetUndoCount() : 0;
}

// ═══════════════════════════════════════════════════════════════════
// Buffer Management
// ═══════════════════════════════════════════════════════════════════

bool UEditorTransactionService::ResetHistory(const FString& Reason)
{
	UTransBuffer* TransBuffer = GetTransBuffer();
	if (!TransBuffer)
	{
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("UEditorTransactionService::ResetHistory - Reason: %s"), *Reason);
	TransBuffer->Reset(FText::FromString(Reason));
	return true;
}
