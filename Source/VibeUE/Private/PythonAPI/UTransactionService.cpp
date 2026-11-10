// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UTransactionService.h"
#include "Editor.h"
#include "Editor/Transactor.h"

namespace
{
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
	return true;
}
