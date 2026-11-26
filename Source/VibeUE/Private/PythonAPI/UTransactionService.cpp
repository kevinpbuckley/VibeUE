// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UTransactionService.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Editor/TransBuffer.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Misc/ITransaction.h"

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

// =================================================================
// Who made each transaction (#701)
// =================================================================

namespace
{
	/** How each transaction began, keyed by its id. Game thread only. */
	struct FTransactionOriginRecord
	{
		bool bFromUserInput = false;
		double StartedSeconds = 0.0;
	};

	constexpr int32 MaxOriginRecords = 2000;
	TMap<FGuid, FTransactionOriginRecord> GOriginRecords;
	TArray<FGuid> GOriginOrder;
	FDelegateHandle GTransactionStateHandle;
	TWeakObjectPtr<UTransBuffer> GTrackedBuffer;

	void OnTransactionStateChanged(const FTransactionContext& Context, ETransactionStateEventType State)
	{
		if (State != ETransactionStateEventType::TransactionStarted || !Context.TransactionId.IsValid())
		{
			return;
		}
		// Slate counts the input event it is dispatching (a key, mouse or analog event: FSlateApplication::Process*Event).
		// A transaction that BEGINS inside one was started by a person: a gizmo drag, a details-panel commit, a menu
		// entry, a hotkey. Scripts, MCP tool calls and timers run from the core ticker, outside input processing.
		FTransactionOriginRecord Record;
		Record.bFromUserInput = FSlateApplication::IsInitialized() && FSlateApplication::Get().IsProcessingInput();
		Record.StartedSeconds = FPlatformTime::Seconds();
		if (!GOriginRecords.Contains(Context.TransactionId))
		{
			GOriginOrder.Add(Context.TransactionId);
		}
		GOriginRecords.Add(Context.TransactionId, Record);
		if (GOriginOrder.Num() > MaxOriginRecords)
		{
			const int32 Drop = GOriginOrder.Num() - MaxOriginRecords;
			for (int32 Index = 0; Index < Drop; ++Index)
			{
				GOriginRecords.Remove(GOriginOrder[Index]);
			}
			GOriginOrder.RemoveAt(0, Drop);
		}
	}
}

void UTransactionService::StartOriginTracking()
{
	if (GTransactionStateHandle.IsValid() || !GEditor)
	{
		return;
	}
	if (UTransBuffer* Buffer = Cast<UTransBuffer>(GEditor->Trans))
	{
		GTrackedBuffer = Buffer;
		GTransactionStateHandle = Buffer->OnTransactionStateChanged().AddStatic(&OnTransactionStateChanged);
	}
}

void UTransactionService::StopOriginTracking()
{
	if (UTransBuffer* Buffer = GTrackedBuffer.Get())
	{
		Buffer->OnTransactionStateChanged().Remove(GTransactionStateHandle);
	}
	GTransactionStateHandle.Reset();
	GTrackedBuffer.Reset();
	GOriginRecords.Reset();
	GOriginOrder.Reset();
}

bool UTransactionService::IsTrackingOrigins()
{
	return GTransactionStateHandle.IsValid();
}

FString UTransactionService::GetTransactionOrigin(const FString& TransactionId)
{
	FGuid Id;
	if (!FGuid::Parse(TransactionId.TrimStartAndEnd(), Id))
	{
		return TEXT("unknown");
	}
	if (const FTransactionOriginRecord* Record = GOriginRecords.Find(Id))
	{
		return Record->bFromUserInput ? TEXT("user") : TEXT("script");
	}
	return TEXT("unknown");
}
