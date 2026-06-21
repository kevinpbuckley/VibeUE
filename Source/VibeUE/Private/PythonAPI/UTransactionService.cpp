// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UTransactionService.h"
#include "Editor.h"
#include "Editor/Transactor.h"

namespace
{
	// Index returned by the most recent BeginTransaction, so CancelTransaction can roll it back.
	int32 GActiveTransactionIndex = INDEX_NONE;

	UTransactor* GetTransactor()
	{
		return GEditor ? GEditor->Trans : nullptr;
	}
}

bool UTransactionService::Undo()
{
	if (!GEditor)
	{
		return false;
	}
	return GEditor->UndoTransaction();
}

bool UTransactionService::Redo()
{
	if (!GEditor)
	{
		return false;
	}
	return GEditor->RedoTransaction();
}

int32 UTransactionService::UndoMultiple(int32 Count)
{
	int32 Done = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		if (!Undo())
		{
			break;
		}
		++Done;
	}
	return Done;
}

int32 UTransactionService::RedoMultiple(int32 Count)
{
	int32 Done = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		if (!Redo())
		{
			break;
		}
		++Done;
	}
	return Done;
}

int32 UTransactionService::BeginTransaction(const FString& Description)
{
	if (!GEditor)
	{
		return INDEX_NONE;
	}
	const FText Desc = Description.IsEmpty()
		? NSLOCTEXT("VibeUE", "TransactionService_Default", "VibeUE Transaction")
		: FText::FromString(Description);
	GActiveTransactionIndex = GEditor->BeginTransaction(Desc);
	return GActiveTransactionIndex;
}

int32 UTransactionService::EndTransaction()
{
	if (!GEditor)
	{
		return INDEX_NONE;
	}
	const int32 Index = GEditor->EndTransaction();
	GActiveTransactionIndex = INDEX_NONE;
	return Index;
}

bool UTransactionService::CancelTransaction()
{
	if (!GEditor)
	{
		return false;
	}
	// NOTE: GEditor->CancelTransaction only DISCARDS the in-progress transaction record;
	// it does not restore object state (interactive tools revert themselves on escape).
	// To deliver the expected "roll back everything done since begin_transaction" behavior,
	// finalize the open transaction and immediately undo it. The reverted transaction is
	// left as a redo candidate.
	GEditor->EndTransaction();
	GActiveTransactionIndex = INDEX_NONE;
	return GEditor->UndoTransaction();
}

FTransactionState UTransactionService::GetState()
{
	FTransactionState State;
	UTransactor* Trans = GetTransactor();
	if (!Trans)
	{
		return State;
	}

	const int32 QueueLength = Trans->GetQueueLength();
	const int32 RedoCount = Trans->GetUndoCount();         // entries currently undone
	State.RedoCount = RedoCount;
	State.UndoCount = FMath::Max(0, QueueLength - RedoCount);

	FText UndoText;
	State.bCanUndo = Trans->CanUndo(&UndoText);
	FText RedoText;
	State.bCanRedo = Trans->CanRedo(&RedoText);

	State.NextUndoTitle = Trans->GetUndoContext().Title.ToString();
	State.NextRedoTitle = Trans->GetRedoContext().Title.ToString();
	return State;
}

bool UTransactionService::CanUndo()
{
	UTransactor* Trans = GetTransactor();
	return Trans ? Trans->CanUndo() : false;
}

bool UTransactionService::CanRedo()
{
	UTransactor* Trans = GetTransactor();
	return Trans ? Trans->CanRedo() : false;
}

FString UTransactionService::GetUndoDescription()
{
	UTransactor* Trans = GetTransactor();
	return Trans ? Trans->GetUndoContext().Title.ToString() : FString();
}

FString UTransactionService::GetRedoDescription()
{
	UTransactor* Trans = GetTransactor();
	return Trans ? Trans->GetRedoContext().Title.ToString() : FString();
}

TArray<FTransactionHistoryEntry> UTransactionService::GetHistory(int32 MaxEntries)
{
	TArray<FTransactionHistoryEntry> History;
	UTransactor* Trans = GetTransactor();
	if (!Trans)
	{
		return History;
	}

	const int32 QueueLength = Trans->GetQueueLength();
	const int32 RedoCount = Trans->GetUndoCount();
	const int32 FirstUndoneIndex = QueueLength - RedoCount; // [FirstUndoneIndex, QueueLength) are undone

	const int32 Start = (MaxEntries > 0) ? FMath::Max(0, QueueLength - MaxEntries) : 0;
	for (int32 i = Start; i < QueueLength; ++i)
	{
		const FTransaction* Transaction = Trans->GetTransaction(i);
		if (!Transaction)
		{
			continue;
		}
		FTransactionHistoryEntry Entry;
		Entry.Title = Transaction->GetContext().Title.ToString();
		Entry.QueueIndex = i;
		Entry.bIsUndone = (i >= FirstUndoneIndex);
		History.Add(Entry);
	}
	return History;
}

bool UTransactionService::ResetBuffer(const FString& Reason)
{
	UTransactor* Trans = GetTransactor();
	if (!Trans)
	{
		return false;
	}
	const FText ResetReason = Reason.IsEmpty()
		? NSLOCTEXT("VibeUE", "TransactionService_Reset", "VibeUE reset transaction buffer")
		: FText::FromString(Reason);
	Trans->Reset(ResetReason);
	GActiveTransactionIndex = INDEX_NONE;
	return true;
}
