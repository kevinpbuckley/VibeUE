// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Utils/VibeUEPythonResultLog.h"
#include "Tools/PythonTypes.h"
#include "Algo/Reverse.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <atomic>

DEFINE_LOG_CATEGORY_STATIC(LogVibeUEPythonResult, Log, All);

namespace
{
	// Each payload field is clamped so a script printing megabytes cannot produce a giant file.
	constexpr int32 MaxFieldChars = 200000;
	// JSONL history bounds: keep the tail small enough to read cheaply on recovery.
	constexpr int32 MaxRunLines = 200;
	constexpr int64 MaxRunsChars = 2 * 1024 * 1024; // ~2 MB proxy (char count, close enough)

	FString ClampField(const FString& In)
	{
		if (In.Len() <= MaxFieldChars)
		{
			return In;
		}
		return In.Left(MaxFieldChars) + FString::Printf(TEXT("...[truncated %d chars]"), In.Len() - MaxFieldChars);
	}

	FString BuildLabel(int64 RunId)
	{
		// Do not persist source text. Python snippets often contain credentials passed to SDKs;
		// recording even a short prefix would turn timeout recovery into a plaintext secret log.
		return FString::Printf(TEXT("#%lld execute_python_code"), (long long)RunId);
	}

	// Serialize the record as one condensed (single-line) JSON object.
	FString BuildRecordJson(const VibeUE::FPythonExecutionResult& Result,
		const FDateTime& StartedUtc, const FDateTime& FinishedUtc, uint32 ProcessId)
	{
		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("runId"), static_cast<double>(Result.RunId));
		Root->SetNumberField(TEXT("pid"), static_cast<double>(ProcessId));
		Root->SetBoolField(TEXT("success"), Result.bSuccess);
		Root->SetStringField(TEXT("label"), BuildLabel(Result.RunId));
		Root->SetStringField(TEXT("output"), ClampField(Result.Output));
		Root->SetStringField(TEXT("error"), ClampField(Result.ErrorMessage));
		Root->SetStringField(TEXT("result"), ClampField(Result.Result));
		Root->SetNumberField(TEXT("execution_time_ms"), Result.ExecutionTimeMs);
		Root->SetStringField(TEXT("startedUtc"), StartedUtc.ToIso8601());
		Root->SetStringField(TEXT("finishedUtc"), FinishedUtc.ToIso8601());

		FString Payload;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
		FJsonSerializer::Serialize(Root, Writer);
		return Payload;
	}

	// Atomic write via a .tmp sibling + move, matching the readiness/health signal writers.
	bool AtomicWrite(const FString& Path, const FString& Contents)
	{
		const FString TempPath = Path + TEXT(".tmp");
		if (!FFileHelper::SaveStringToFile(Contents, *TempPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			return false;
		}
		if (!IFileManager::Get().Move(*Path, *TempPath, /*Replace=*/true, /*EvenIfReadOnly=*/true))
		{
			IFileManager::Get().Delete(*TempPath, /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true);
			return false;
		}
		return true;
	}

	// Append the new line to the JSONL history, then rewrite it trimmed to the last N lines / ~2 MB.
	// Reads and rewrites the (bounded) file each run — simple and always correct; the task noted one
	// file write per run is acceptable.
	void AppendTrimmed(const FString& RunsPath, const FString& Line)
	{
		TArray<FString> Lines;
		FString Existing;
		if (FFileHelper::LoadFileToString(Existing, *RunsPath))
		{
			Existing.ParseIntoArrayLines(Lines, /*InCullEmpty=*/true);
		}
		Lines.Add(Line);

		// Keep the newest lines that fit the line- and byte-budgets. The newest line is always kept.
		TArray<FString> Kept;
		int64 Budget = MaxRunsChars;
		for (int32 i = Lines.Num() - 1; i >= 0 && Kept.Num() < MaxRunLines; --i)
		{
			const int64 Cost = static_cast<int64>(Lines[i].Len()) + 1; // + newline
			if (!Kept.IsEmpty() && Budget - Cost < 0)
			{
				break;
			}
			Budget -= Cost;
			Kept.Add(Lines[i]);
		}
		Algo::Reverse(Kept);

		FString Content;
		for (const FString& L : Kept)
		{
			Content += L;
			Content += TEXT("\n");
		}

		if (!AtomicWrite(RunsPath, Content))
		{
			UE_LOG(LogVibeUEPythonResult, Warning, TEXT("VibeUE: failed to write Python run history: %s"), *RunsPath);
		}
	}
}

int64 FVibeUEPythonResultLog::NextRunId()
{
	static std::atomic<int64> Counter{ 0 };
	return ++Counter;
}

FString FVibeUEPythonResultLog::GetLastResultPathForPid(uint32 ProcessId)
{
	// Deliberately not FVibeUEPaths::GetSignalsDir() — that creates the directory; keep this a pure
	// path helper. Record() ensures the directory exists before writing.
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("VibeUE"),
		TEXT("Signals"),
		FString::Printf(TEXT("python-%u-last.json"), ProcessId));
}

FString FVibeUEPythonResultLog::GetRunsPathForPid(uint32 ProcessId)
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("VibeUE"),
		TEXT("Signals"),
		FString::Printf(TEXT("python-%u-runs.jsonl"), ProcessId));
}

void FVibeUEPythonResultLog::Record(
	const VibeUE::FPythonExecutionResult& Result,
	const FDateTime& StartedUtc,
	const FDateTime& FinishedUtc)
{
	const uint32 ProcessId = FPlatformProcess::GetCurrentProcessId();

	// Ensure the Signals directory exists (the launch scripts and other signals also live here).
	const FString SignalsDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VibeUE"), TEXT("Signals"));
	IFileManager::Get().MakeDirectory(*SignalsDir, /*Tree=*/true);

	const FString RecordJson = BuildRecordJson(Result, StartedUtc, FinishedUtc, ProcessId);

	if (!AtomicWrite(GetLastResultPathForPid(ProcessId), RecordJson))
	{
		UE_LOG(LogVibeUEPythonResult, Warning, TEXT("VibeUE: failed to write last Python result: %s"),
			*GetLastResultPathForPid(ProcessId));
	}

	AppendTrimmed(GetRunsPathForPid(ProcessId), RecordJson);
}
