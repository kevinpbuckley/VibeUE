// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Utils/VibeUEHealthSignal.h"
#include "Utils/VibeUEPaths.h"
#include "Utils/VibeUEReadinessSignal.h"
#include "Utils/VibeUEMcpStatus.h"
#include "Containers/Ticker.h"
#include "HAL/CriticalSection.h"
#include "HAL/Event.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/Thread.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <atomic>

DEFINE_LOG_CATEGORY_STATIC(LogVibeUEHealth, Log, All);

namespace
{
	constexpr double WriteIntervalSeconds = 5.0;

	std::atomic<double> GLastGameThreadSeconds{ 0.0 };
	// MCP endpoint snapshot (issue B6), refreshed on the game-thread ticker and read by the background
	// writer. The module read must stay on the game thread; the writer only touches these atomics.
	std::atomic<uint32> GMcpPort{ 0 };
	std::atomic<bool> GMcpListening{ false };
	std::atomic<bool> GStopRequested{ false };
	FTSTicker::FDelegateHandle GTickerHandle;
	TUniquePtr<FThread> GWriterThread;
	FEvent* GStopEvent = nullptr;
	// Held by the writer for one write-then-move, and by RequestStopFromAnyThread while it sets the stop flag and
	// deletes the file, so a heartbeat in flight can never re-create a file that a forced exit just deleted. Also
	// guards GStopEvent's lifetime against that off-thread Trigger. Recursive (FCriticalSection is): a writer that
	// crashes mid-write reaches RequestStopFromAnyThread on itself and must not deadlock.
	FCriticalSection GHealthWriteLock;

	void DeleteHealthFiles()
	{
		IFileManager& FileManager = IFileManager::Get();
		const FString HealthPath = FVibeUEHealthSignal::GetHealthPathForPid(FPlatformProcess::GetCurrentProcessId());
		FileManager.Delete(*HealthPath, /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true);
		FileManager.Delete(*(HealthPath + TEXT(".tmp")), /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true);
	}

	void WriteHealthFile()
	{
		FScopeLock Lock(&GHealthWriteLock);
		if (GStopRequested.load())
		{
			return;
		}

		const uint32 ProcessId = FPlatformProcess::GetCurrentProcessId();
		const FString HealthPath = FVibeUEHealthSignal::GetHealthPathForPid(ProcessId);
		const FString TempPath = HealthPath + TEXT(".tmp");

		const double Now = FPlatformTime::Seconds();
		const double LastTick = GLastGameThreadSeconds.load(std::memory_order_relaxed);
		const double StallSeconds = FMath::Max(0.0, Now - LastTick);

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("signal"), TEXT("health"));
		Root->SetNumberField(TEXT("pid"), static_cast<double>(ProcessId));
		Root->SetStringField(TEXT("updatedUtc"), FDateTime::UtcNow().ToIso8601());
		Root->SetStringField(TEXT("sessionStartUtc"), FVibeUEReadinessSignal::GetSessionStartUtc().ToIso8601());
		Root->SetNumberField(TEXT("gameThreadStallSeconds"), FMath::RoundToDouble(StallSeconds * 100.0) / 100.0);
		// MCP endpoint status (issue B6) — snapshot taken on the game-thread ticker below.
		Root->SetNumberField(TEXT("mcpPort"), static_cast<double>(GMcpPort.load(std::memory_order_relaxed)));
		Root->SetBoolField(TEXT("mcpListening"), GMcpListening.load(std::memory_order_relaxed));

		FString Payload;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
		FJsonSerializer::Serialize(Root, Writer);

		// Write-then-move, same as the readiness signal — watchers never see a partial file.
		if (FFileHelper::SaveStringToFile(Payload, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			IFileManager::Get().Move(*HealthPath, *TempPath, /*Replace=*/true, /*EvenIfReadOnly=*/true);
		}
		// A forced exit whose lock wait timed out may have set the flag and deleted the files while this write was
		// stalled past the check above (its .tmp delete fails while the file is held open here): clean up after it.
		if (GStopRequested.load())
		{
			DeleteHealthFiles();
		}
	}
}

FString FVibeUEHealthSignal::GetHealthPathForPid(uint32 ProcessId)
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("VibeUE"),
		TEXT("Signals"),
		FString::Printf(TEXT("editor-%u-health.json"), ProcessId));
}

void FVibeUEHealthSignal::Start()
{
	if (GWriterThread.IsValid())
	{
		return;
	}
	if (FVibeUEPaths::GetSignalsDir().IsEmpty())
	{
		UE_LOG(LogVibeUEHealth, Warning, TEXT("VibeUE: health signal disabled — no signals directory."));
		return;
	}

	GLastGameThreadSeconds.store(FPlatformTime::Seconds(), std::memory_order_relaxed);
	GStopRequested.store(false, std::memory_order_relaxed);

	// Seed the MCP snapshot on the game thread so the very first heartbeat (written before the ticker
	// first fires) reports a real port rather than 0. Start() is called from RegisterToolsets.
	{
		uint32 McpPort = 0;
		bool bMcpListening = false;
		FVibeUEMcpStatus::Query(McpPort, bMcpListening);
		GMcpPort.store(McpPort, std::memory_order_relaxed);
		GMcpListening.store(bMcpListening, std::memory_order_relaxed);
	}

	GTickerHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("VibeUEHealthSignal"), 0.0f,
		[](float) -> bool
		{
			const double NowSeconds = FPlatformTime::Seconds();
			GLastGameThreadSeconds.store(NowSeconds, std::memory_order_relaxed);

			// Refresh the MCP snapshot on the game thread (the module read is not thread-safe), at most
			// every ~2s so the per-frame ticker stays cheap. The background writer reads the atomics.
			static double LastMcpRefreshSeconds = 0.0;
			if (NowSeconds - LastMcpRefreshSeconds > 2.0)
			{
				LastMcpRefreshSeconds = NowSeconds;
				uint32 McpPort = 0;
				bool bMcpListening = false;
				FVibeUEMcpStatus::Query(McpPort, bMcpListening);
				GMcpPort.store(McpPort, std::memory_order_relaxed);
				GMcpListening.store(bMcpListening, std::memory_order_relaxed);
			}
			return true;
		});

	{
		FScopeLock Lock(&GHealthWriteLock);
		GStopEvent = FPlatformProcess::GetSynchEventFromPool(/*bIsManualReset=*/true);
	}
	GWriterThread = MakeUnique<FThread>(TEXT("VibeUEHealthSignal"), []()
	{
		while (!GStopRequested.load(std::memory_order_relaxed))
		{
			WriteHealthFile();
			GStopEvent->Wait(FTimespan::FromSeconds(WriteIntervalSeconds));
		}
	});

	UE_LOG(LogVibeUEHealth, Display, TEXT("VibeUE: health heartbeat started: %s"),
		*GetHealthPathForPid(FPlatformProcess::GetCurrentProcessId()));
}

void FVibeUEHealthSignal::Stop()
{
	// Joining belongs to the game thread, which owns the writer; from any other thread (possibly the writer
	// itself) a Join is a self-join or a race on GWriterThread. Fall back to the non-joining stop.
	if (!IsInGameThread())
	{
		RequestStopFromAnyThread();
		return;
	}
	if (!GWriterThread.IsValid())
	{
		return;
	}

	// After an early RequestStopFromAnyThread (a Ctrl-C, then OnPreExit) the flag is already set and the writer
	// is already leaving: this still joins it (an FThread must be joined before destruction) and frees the event.
	GStopRequested.store(true);
	{
		FScopeLock Lock(&GHealthWriteLock);
		GStopEvent->Trigger();
	}
	GWriterThread->Join();
	GWriterThread.Reset();
	{
		FScopeLock Lock(&GHealthWriteLock);
		FPlatformProcess::ReturnSynchEventToPool(GStopEvent);
		GStopEvent = nullptr;
	}

	FTSTicker::GetCoreTicker().RemoveTicker(GTickerHandle);
	GTickerHandle.Reset();

	DeleteHealthFiles();
}

void FVibeUEHealthSignal::RequestStopFromAnyThread()
{
	// Bounded wait for the lock: the writer holds it only for one small file write, but this runs on the way to
	// TerminateProcess and must never hang the exit. Without the lock, the flag and the delete still happen; only
	// the wake-up is skipped (GStopEvent's lifetime is guarded by that lock), and the writer then leaves at its
	// next wait timeout.
	// A wall-clock deadline, not a count of 1 ms sleeps: a minimized or hidden process on Windows 11 does not get
	// 1 ms timer resolution, so each Sleep(1) can last ~15.6 ms.
	bool bLocked = GHealthWriteLock.TryLock();
	const double Deadline = FPlatformTime::Seconds() + 0.2;
	while (!bLocked && FPlatformTime::Seconds() < Deadline)
	{
		FPlatformProcess::SleepNoStats(0.001f);
		bLocked = GHealthWriteLock.TryLock();
	}

	GStopRequested.store(true);
	if (bLocked && GStopEvent)
	{
		GStopEvent->Trigger();
	}
	DeleteHealthFiles();

	if (bLocked)
	{
		GHealthWriteLock.Unlock();
	}
}

bool FVibeUEHealthSignal::IsWriterThreadOwned()
{
	return GWriterThread.IsValid();
}
