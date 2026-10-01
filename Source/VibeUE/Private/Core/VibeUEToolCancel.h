// Copyright Buckley Builds LLC 2026 All Rights Reserved.
//
// Cancellation for a VibeUE tool call that runs off the game thread.
//
// The MCP bridge runs deep_research on a worker thread (it only makes an HTTP request and parses text), so the
// editor keeps ticking while it waits. For each such call the bridge creates one FVibeUEToolCancel, makes it
// current on the worker thread while the tool runs, and calls Cancel() when the client sends
// notifications/cancelled (IModelContextProtocolTool::CancelAsync). A tool that waits registers what to do on
// cancel with SetOnCancel, typically waking its wait.
//
// Module-private (Private/, no VIBEUE_API): only this module's bridge makes a token current, for the tools it runs
// off the game thread, and CurrentSlot() is defined in VibeUEMCPToolBridge.cpp.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include <atomic>

class FVibeUEToolCancel : public TSharedFromThis<FVibeUEToolCancel, ESPMode::ThreadSafe>
{
public:
	bool IsCancelled() const { return bCancelled.load(); }

	/** Runs Handler at once if the call is already cancelled, otherwise when Cancel() runs. Replaces any earlier handler. */
	void SetOnCancel(TFunction<void()> Handler)
	{
		{
			FScopeLock Guard(&Lock);
			if (!bCancelled.load())
			{
				OnCancel = MoveTemp(Handler);
				return;
			}
		}
		if (Handler)
		{
			Handler();
		}
	}

	void ClearOnCancel()
	{
		FScopeLock Guard(&Lock);
		OnCancel = nullptr;
	}

	/** Safe from any thread; the handler runs once, on the calling thread. */
	void Cancel()
	{
		TFunction<void()> Handler;
		{
			FScopeLock Guard(&Lock);
			if (bCancelled.exchange(true))
			{
				return;
			}
			Handler = MoveTemp(OnCancel);
		}
		if (Handler)
		{
			Handler();
		}
	}

	/** The token of the tool call running on this thread, or null when the call is not cancellable. */
	static FVibeUEToolCancel* GetCurrent() { return CurrentSlot(); }

	/** Makes a token current on this thread for the scope's lifetime. */
	struct FScope
	{
		explicit FScope(FVibeUEToolCancel* Token) : Previous(CurrentSlot()) { CurrentSlot() = Token; }
		~FScope() { CurrentSlot() = Previous; }
		FVibeUEToolCancel* Previous;
	};

private:
	/** Defined in VibeUEMCPToolBridge.cpp: one slot per thread. */
	static FVibeUEToolCancel*& CurrentSlot();

	std::atomic<bool> bCancelled{false};
	FCriticalSection Lock;
	TFunction<void()> OnCancel;
};
