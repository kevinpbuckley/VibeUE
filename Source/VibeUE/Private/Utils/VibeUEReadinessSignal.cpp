// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Utils/VibeUEReadinessSignal.h"
#include "Utils/VibeUEPaths.h"
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogVibeUESignal, Log, All);

FString FVibeUEReadinessSignal::GetSignalPathForPid(uint32 ProcessId)
{
	// Deliberately not FVibeUEPaths::GetSignalsDir() — that creates the directory, and callers such as
	// Remove() must be able to name the file without touching the filesystem.
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("VibeUE"),
		TEXT("Signals"),
		FString::Printf(TEXT("editor-%u-true.json"), ProcessId));
}

FString FVibeUEReadinessSignal::GetSignalPath()
{
	return GetSignalPathForPid(FPlatformProcess::GetCurrentProcessId());
}

FDateTime FVibeUEReadinessSignal::GetSessionStartUtc()
{
	// GStartTime is FPlatformTime::Seconds() sampled at engine start, so the delta from "now" gives
	// this process's age. Wall-clock start time is what lets an agent reject a stale signal left by a
	// crashed editor whose PID was reused.
	const double SecondsSinceStart = FMath::Max(0.0, FPlatformTime::Seconds() - GStartTime);
	return FDateTime::UtcNow() - FTimespan::FromSeconds(SecondsSinceStart);
}

FString FVibeUEReadinessSignal::BuildSignalJson(uint32 ProcessId, const FDateTime& SessionStartUtc, const FDateTime& CreatedUtc)
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("signal"), TEXT("toolsets-registered"));
	// JSON has no integer type distinct from double; a PID fits exactly in a double, and writing it as
	// a number keeps `json.load(...)["pid"] == pid` working for agents.
	Root->SetNumberField(TEXT("pid"), static_cast<double>(ProcessId));
	Root->SetStringField(TEXT("createdUtc"), CreatedUtc.ToIso8601());
	Root->SetStringField(TEXT("sessionStartUtc"), SessionStartUtc.ToIso8601());
	Root->SetStringField(TEXT("pluginVersion"), FVibeUEPaths::GetPluginVersionName());

	FString Payload;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Payload);
	FJsonSerializer::Serialize(Root, Writer);
	return Payload;
}

bool FVibeUEReadinessSignal::Publish()
{
	const FString SignalsDir = FVibeUEPaths::GetSignalsDir();
	if (SignalsDir.IsEmpty())
	{
		UE_LOG(LogVibeUESignal, Error, TEXT("VibeUE: failed to create signal directory under %s"), *FPaths::ProjectSavedDir());
		return false;
	}

	const uint32 ProcessId = FPlatformProcess::GetCurrentProcessId();
	const FString SignalPath = GetSignalPathForPid(ProcessId);
	const FString TempPath = SignalPath + TEXT(".tmp");
	const FString Payload = BuildSignalJson(ProcessId, GetSessionStartUtc(), FDateTime::UtcNow());

	// Write-then-move: an agent watching for the create event must never read a partial file.
	if (!FFileHelper::SaveStringToFile(Payload, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogVibeUESignal, Error, TEXT("VibeUE: failed to stage readiness signal: %s"), *TempPath);
		return false;
	}

	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.Move(*SignalPath, *TempPath, /*Replace=*/true, /*EvenIfReadOnly=*/true))
	{
		UE_LOG(LogVibeUESignal, Error, TEXT("VibeUE: failed to publish readiness signal: %s"), *SignalPath);
		FileManager.Delete(*TempPath, /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true);
		return false;
	}

	UE_LOG(LogVibeUESignal, Display, TEXT("VibeUE: readiness signal published: %s"), *SignalPath);
	return true;
}

void FVibeUEReadinessSignal::Remove()
{
	IFileManager& FileManager = IFileManager::Get();
	const FString SignalPath = GetSignalPath();
	FileManager.Delete(*SignalPath, /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true);
	FileManager.Delete(*(SignalPath + TEXT(".tmp")), /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true);
}
