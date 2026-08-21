// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Utils/VibeUEHealthSignal.h"
#include "Utils/VibeUEPaths.h"
#include "Utils/VibeUEReadinessSignal.h"
#include "Containers/Ticker.h"
#include "HAL/Event.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/Thread.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <atomic>

DEFINE_LOG_CATEGORY_STATIC(LogVibeUEHealth, Log, All);

namespace
{
	constexpr double WriteIntervalSeconds = 5.0;

	std::atomic<double> GLastGameThreadSeconds{ 0.0 };
	std::atomic<bool> GStopRequested{ false };
	FTSTicker::FDelegateHandle GTickerHandle;
	TUniquePtr<FThread> GWriterThread;
	FEvent* GStopEvent = nullptr;

	void WriteHealthFile()
	{
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

		FString Payload;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
		FJsonSerializer::Serialize(Root, Writer);

		// Write-then-move, same as the readiness signal — watchers never see a partial file.
		if (FFileHelper::SaveStringToFile(Payload, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			IFileManager::Get().Move(*HealthPath, *TempPath, /*Replace=*/true, /*EvenIfReadOnly=*/true);
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

	GTickerHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("VibeUEHealthSignal"), 0.0f,
		[](float) -> bool
		{
			GLastGameThreadSeconds.store(FPlatformTime::Seconds(), std::memory_order_relaxed);
			return true;
		});

	GStopEvent = FPlatformProcess::GetSynchEventFromPool(/*bIsManualReset=*/true);
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
	if (!GWriterThread.IsValid())
	{
		return;
	}

	GStopRequested.store(true, std::memory_order_relaxed);
	GStopEvent->Trigger();
	GWriterThread->Join();
	GWriterThread.Reset();
	FPlatformProcess::ReturnSynchEventToPool(GStopEvent);
	GStopEvent = nullptr;

	FTSTicker::GetCoreTicker().RemoveTicker(GTickerHandle);
	GTickerHandle.Reset();

	IFileManager& FileManager = IFileManager::Get();
	const FString HealthPath = GetHealthPathForPid(FPlatformProcess::GetCurrentProcessId());
	FileManager.Delete(*HealthPath, /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true);
	FileManager.Delete(*(HealthPath + TEXT(".tmp")), /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true);
}
