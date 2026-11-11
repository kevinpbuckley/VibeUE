// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UPerformanceService.h"
#include "PythonAPI/PerformanceAnalysis.h"
#include "PythonAPI/PerformanceVerdict.h"
#include "Json.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "Internationalization/Regex.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "RenderingThread.h"   // ENQUEUE_RENDER_COMMAND
#include "RHICommandList.h"    // FRHICommandListImmediate
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "PlayInEditorDataTypes.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/EditorPerformanceSettings.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/Frames.h"
#include "RenderTimer.h"       // GGameThreadTime / GRenderThreadTime / GRHIThreadTime (RenderCore)
#include "RHIGlobals.h"        // RHIGetGPUFrameCycles (RHI)

#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformMisc.h"
#include "Windows/WindowsHWrapper.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPerformance, Log, All);

// ---------------------------------------------------------------------------
// Session state — persists across MCP requests within an editor session
// ---------------------------------------------------------------------------

static FString GLastTraceFilePath;
static FString GLastLogFilePath;
static bool            GStandaloneRunning = false;
static bool            GStandaloneStartVerified = false;
static bool            GStandaloneFinalized = false;
static bool            GStandaloneForcedTermination = false;
static uint32          GStandalonePID = 0;
static FString         GStandaloneSessionId;
static FString         GStandaloneMap;
static FString         GStandaloneTraceFilePath;
static FString         GStandaloneLogFilePath;
static FDateTime       GStandaloneStartedUtc;
static FProcHandle     GStandaloneProcess;
static FDelegateHandle GStandalonePlayDelegateHandle;

// ---------------------------------------------------------------------------
// JSON response helpers
// ---------------------------------------------------------------------------

static FString OkJson(TSharedPtr<FJsonObject> Obj)
{
	Obj->SetBoolField(TEXT("success"), true);
	FString Out;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), W);
	return Out;
}

static FString ErrJson(const FString& Code, const FString& Msg)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("success"), false);
	Obj->SetStringField(TEXT("error_code"), Code);
	Obj->SetStringField(TEXT("error"), Msg);
	FString Out;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), W);
	return Out;
}

static bool IsPIERunning()
{
	return GEditor && GEditor->PlayWorld != nullptr;
}

static const TCHAR* DefaultTraceChannels()
{
	return TEXT("frame,cpu,gpu,log,loadtime,object,stats,bookmark,region");
}

static FString ProjectSavedDirAbs()
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
}

static FString BuildTraceFilePath(const FString& Name)
{
	FString Dir = ProjectSavedDirAbs() / TEXT("Profiling");
	IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*Dir);
	return Dir / Name;
}

static bool RequestStandaloneExitGracefully(uint32 ProcessId)
{
#if PLATFORM_WINDOWS
	struct FCloseWindowsData
	{
		uint32 PID = 0;
		bool bSent = false;
	} Data { ProcessId, false };
	::EnumWindows([](HWND Window, LPARAM Param) -> BOOL
	{
		FCloseWindowsData& CloseData = *reinterpret_cast<FCloseWindowsData*>(Param);
		DWORD WindowPID = 0;
		::GetWindowThreadProcessId(Window, &WindowPID);
		if (WindowPID == CloseData.PID && ::GetWindow(Window, GW_OWNER) == nullptr)
		{
			CloseData.bSent |= ::PostMessageW(Window, WM_CLOSE, 0, 0) != 0;
		}
		return 1;
	}, reinterpret_cast<LPARAM>(&Data));
	return Data.bSent;
#else
	return false;
#endif
}

FString VibeUEPerformanceAnalysis::MakeStandaloneSessionStem(const FString& RequestedName)
{
	static uint64 SessionCounter = 0;
	FString SafeName = FPaths::MakeValidFileName(RequestedName.IsEmpty() ? TEXT("standalone_capture") : RequestedName);
	if (SafeName.IsEmpty()) SafeName = TEXT("standalone_capture");
	const FDateTime Now = FDateTime::UtcNow();
	return FString::Printf(TEXT("%s_%s_%lld_%llu"), *SafeName,
		*Now.ToString(TEXT("%Y%m%dT%H%M%SZ")), Now.GetTicks(), ++SessionCounter);
}

// Read StoreDir only for diagnostics. Standalone sessions deliberately never select a trace from it.
static FString GetUTSStoreDir()
{
	FString SettingsPath = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"))
		/ TEXT("UnrealEngine/Common/UnrealTrace/Settings.ini");

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *SettingsPath))
	{
		return FString();
	}

	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("StoreDir=")))
		{
			FString Dir = Line.Mid(9).TrimStartAndEnd();
			FPaths::NormalizeFilename(Dir);
			return Dir;
		}
	}
	return FString();
}

// ---------------------------------------------------------------------------
// Trace analysis
// ---------------------------------------------------------------------------

FString VibeUEPerformanceAnalysis::AnalyseTrace(const FString& TraceFile)
{
	if (TraceFile.IsEmpty())
	{
		return ErrJson(TEXT("NO_TRACE"), TEXT("No trace file was supplied or recorded for this session."));
	}
	if (!FPaths::FileExists(TraceFile))
	{
		return ErrJson(TEXT("FILE_NOT_FOUND"), FString::Printf(TEXT("Trace file not found: %s"), *TraceFile));
	}
	const int64 TraceSize = IFileManager::Get().FileSize(*TraceFile);
	if (TraceSize <= 0)
	{
		return ErrJson(TEXT("EMPTY_TRACE"), FString::Printf(TEXT("Trace file is empty or not finalized: %s"), *TraceFile));
	}

	ITraceServicesModule* TraceModule = FModuleManager::LoadModulePtr<ITraceServicesModule>("TraceServices");
	if (!TraceModule)
	{
		return ErrJson(TEXT("MODULE_NOT_FOUND"), TEXT("TraceServices module not available."));
	}

	TSharedPtr<TraceServices::IAnalysisService> AnalysisService = TraceModule->GetAnalysisService();
	if (!AnalysisService)
	{
		return ErrJson(TEXT("SERVICE_NOT_FOUND"), TEXT("Could not get TraceServices analysis service."));
	}

	UE_LOG(LogPerformance, Log, TEXT("Analysing trace: %s"), *TraceFile);
	TSharedPtr<const TraceServices::IAnalysisSession> Session = AnalysisService->Analyze(*TraceFile);
	if (!Session)
	{
		return ErrJson(TEXT("ANALYSIS_FAILED"), TEXT("Failed to analyse trace file."));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("trace_file"), TraceFile);

	{
		TraceServices::FAnalysisSessionReadScope Scope(*Session);
		Root->SetNumberField(TEXT("duration_seconds"), Session->GetDurationSeconds());
		const TraceServices::IFrameProvider* Frames = Session->ReadProvider<TraceServices::IFrameProvider>(TraceServices::GetFrameProviderName());

		if (Frames)
		{
			uint64 FrameCount = Frames->GetFrameCount(ETraceFrameType::TraceFrameType_Game);
			Root->SetNumberField(TEXT("frame_count"), (double)FrameCount);

			if (FrameCount > 0)
			{
				double TotalMs = 0.0;
				double MaxMs = 0.0;
				uint64 MaxFrame = 0;
				double MaxFrameTime = 0.0;
				TArray<double> AllMs;
				AllMs.Reserve((int32)FMath::Min(FrameCount, (uint64)10000));

				Frames->EnumerateFrames(ETraceFrameType::TraceFrameType_Game, 0, FrameCount,
					[&](const TraceServices::FFrame& F)
					{
						if (F.EndTime <= F.StartTime) return;
						double DurationMs = (F.EndTime - F.StartTime) * 1000.0;
						if (!FMath::IsFinite(DurationMs) || DurationMs > 120000.0) return;
						AllMs.Add(DurationMs);
						TotalMs += DurationMs;
						if (DurationMs > MaxMs)
						{
							MaxMs = DurationMs;
							MaxFrame = F.Index;
							MaxFrameTime = F.StartTime;
						}
					});

				if (AllMs.IsEmpty())
				{
					Root->SetStringField(TEXT("warning"), TEXT("All frames were filtered (invalid timestamps) — no frame stats available."));
				}
				else
				{
					double AvgMs = TotalMs / (double)AllMs.Num();
					Root->SetNumberField(TEXT("avg_frame_ms"), FMath::RoundToFloat(AvgMs * 100.0f) / 100.0f);
					Root->SetNumberField(TEXT("avg_fps"), FMath::RoundToFloat(1000.0f / (float)AvgMs * 10.0f) / 10.0f);
					Root->SetNumberField(TEXT("max_frame_ms"), FMath::RoundToFloat(MaxMs * 100.0f) / 100.0f);
					Root->SetNumberField(TEXT("max_frame_index"), (double)MaxFrame);
					Root->SetNumberField(TEXT("max_frame_timestamp"), MaxFrameTime);

					AllMs.Sort();
					int32 FramesOver33Ms = 0;
					int32 FramesOver50Ms = 0;
					int32 FramesOver100Ms = 0;
					for (const double Ms : AllMs)
					{
						if (Ms > 33.0) ++FramesOver33Ms;
						if (Ms > 50.0) ++FramesOver50Ms;
						if (Ms > 100.0) ++FramesOver100Ms;
					}
					int32 MedianIdx = FMath::Clamp(AllMs.Num() / 2, 0, AllMs.Num() - 1);
					int32 P95Idx = FMath::Clamp((int32)(AllMs.Num() * 0.95), 0, AllMs.Num() - 1);
					int32 P99Idx = FMath::Clamp((int32)(AllMs.Num() * 0.99), 0, AllMs.Num() - 1);
					Root->SetNumberField(TEXT("median_frame_ms"), FMath::RoundToFloat(AllMs[MedianIdx] * 100.0f) / 100.0f);
					Root->SetNumberField(TEXT("p95_frame_ms"), FMath::RoundToFloat(AllMs[P95Idx] * 100.0f) / 100.0f);
					Root->SetNumberField(TEXT("p99_frame_ms"), FMath::RoundToFloat(AllMs[P99Idx] * 100.0f) / 100.0f);
					Root->SetNumberField(TEXT("frames_over_33ms"), FramesOver33Ms);
					Root->SetNumberField(TEXT("frames_over_50ms"), FramesOver50Ms);
					Root->SetNumberField(TEXT("frames_over_100ms"), FramesOver100Ms);
				}

				// Worst 10 frames
				TArray<TSharedPtr<FJsonValue>> WorstArr;
				struct FFrameEntry { double Ms; uint64 Index; double StartTime; };
				TArray<FFrameEntry> Entries;
				Entries.Reserve((int32)FMath::Min(FrameCount, (uint64)10000));
				Frames->EnumerateFrames(ETraceFrameType::TraceFrameType_Game, 0, FrameCount,
					[&](const TraceServices::FFrame& F)
					{
						if (F.EndTime <= F.StartTime) return;
						double Ms = (F.EndTime - F.StartTime) * 1000.0;
						if (!FMath::IsFinite(Ms) || Ms > 120000.0) return;
						Entries.Add({ Ms, F.Index, F.StartTime });
					});
				Entries.Sort([](const FFrameEntry& A, const FFrameEntry& B) { return A.Ms > B.Ms; });

				int32 WorstCount = FMath::Min(10, Entries.Num());
				for (int32 i = 0; i < WorstCount; ++i)
				{
					TSharedPtr<FJsonObject> FObj = MakeShared<FJsonObject>();
					FObj->SetNumberField(TEXT("frame"), (double)Entries[i].Index);
					FObj->SetNumberField(TEXT("ms"), FMath::RoundToFloat(Entries[i].Ms * 100.0f) / 100.0f);
					FObj->SetNumberField(TEXT("timestamp"), Entries[i].StartTime);
					WorstArr.Add(MakeShared<FJsonValueObject>(FObj));
				}
				Root->SetArrayField(TEXT("worst_frames"), WorstArr);
			}
		}
	}

	return OkJson(Root);
}

// ---------------------------------------------------------------------------
// Log analysis
// ---------------------------------------------------------------------------

FString VibeUEPerformanceAnalysis::AnalyseLogs(const FString& LogFile)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *LogFile))
	{
		// LoadFileToString fails both when the file is absent AND when it exists but is locked open by
		// another process (e.g. the editor's own live log). Distinguish the two so the caller isn't
		// misled into thinking a present-but-locked log is missing.
		if (FPaths::FileExists(LogFile))
		{
			return ErrJson(TEXT("LOG_LOCKED"), FString::Printf(TEXT("Log file exists but could not be read (likely held open by another process): %s"), *LogFile));
		}
		return ErrJson(TEXT("LOG_NOT_FOUND"), FString::Printf(TEXT("Log file not found: %s"), *LogFile));
	}

	TArray<FString> Lines;
	Content.ParseIntoArrayLines(Lines);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("log_file"), LogFile);
	Root->SetNumberField(TEXT("total_lines"), Lines.Num());

	TArray<TSharedPtr<FJsonValue>> Notable;
	int32 PSOHitchEventLines = 0;
	int32 PSOHitchSummaryLines = 0;
	int32 PSOHitchesReported = 0;
	int32 ErrorCount = 0;
	int32 WarningCount = 0;

	static const TArray<FString> NotablePatterns = {
		TEXT("hitch"), TEXT("Hitch"),
		TEXT("PSO"), TEXT("RTPSO"),
		TEXT("blocking load"), TEXT("Blocking Load"),
		TEXT("took "), TEXT(" ms."),
		TEXT("LogShaderCompilers"), TEXT("NiagaraSystem"),
		TEXT("async load"), TEXT("Async Load"),
		TEXT("streamable"), TEXT("Streamable"),
		TEXT("OutOfMemory"), TEXT("out of memory"),
	};

	for (const FString& Line : Lines)
	{
		bool bError   = Line.Contains(TEXT("] Error:"))   || Line.Contains(TEXT(":Error:"));
		bool bWarning = Line.Contains(TEXT("] Warning:")) || Line.Contains(TEXT(":Warning:"));
		if (bError)   ++ErrorCount;
		if (bWarning) ++WarningCount;

		int32 SummaryCount = INDEX_NONE;
		const FRegexPattern CountBeforePattern(TEXT("([0-9]+)\\s+PSO creation hitches"));
		FRegexMatcher CountBefore(CountBeforePattern, Line);
		if (CountBefore.FindNext())
		{
			SummaryCount = FCString::Atoi(*CountBefore.GetCaptureGroup(1));
		}
		else
		{
			const FRegexPattern CountAfterPattern(TEXT("PSO creation hitches[^0-9]*([0-9]+)"));
			FRegexMatcher CountAfter(CountAfterPattern, Line);
			if (CountAfter.FindNext()) SummaryCount = FCString::Atoi(*CountAfter.GetCaptureGroup(1));
		}
		if (SummaryCount != INDEX_NONE)
		{
			++PSOHitchSummaryLines;
			PSOHitchesReported = FMath::Max(PSOHitchesReported, SummaryCount);
		}
		else if (Line.Contains(TEXT("PSO creation hitch")))
		{
			++PSOHitchEventLines;
		}

		bool bNotable = bError;
		if (!bNotable)
		{
			for (const FString& Pat : NotablePatterns)
			{
				if (Line.Contains(Pat)) { bNotable = true; break; }
			}
		}
		if (bNotable && Notable.Num() < 40)
		{
			Notable.Add(MakeShared<FJsonValueString>(Line.TrimStartAndEnd()));
		}
	}

	Root->SetNumberField(TEXT("errors"), ErrorCount);
	Root->SetNumberField(TEXT("warnings"), WarningCount);
	Root->SetNumberField(TEXT("pso_hitch_event_lines"), PSOHitchEventLines);
	Root->SetNumberField(TEXT("pso_hitch_summary_lines"), PSOHitchSummaryLines);
	Root->SetNumberField(TEXT("pso_hitches_reported"), PSOHitchesReported);
	Root->SetNumberField(TEXT("pso_hitches"), FMath::Max(PSOHitchEventLines, PSOHitchesReported));
	Root->SetArrayField(TEXT("notable_lines"), Notable);

	return OkJson(Root);
}

FString VibeUEPerformanceAnalysis::AnalyseBoth(const FString& TraceFile, const FString& LogFile)
{
	FString TraceResult = AnalyseTrace(TraceFile);
	FString LogResult   = AnalyseLogs(LogFile);

	auto ParseOrError = [](const FString& Json, const FString& Label) -> TSharedPtr<FJsonObject>
	{
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(R, Obj) || !Obj.IsValid())
		{
			Obj = MakeShared<FJsonObject>();
			Obj->SetBoolField(TEXT("success"), false);
			Obj->SetStringField(TEXT("error"), FString::Printf(TEXT("%s result was not valid JSON"), *Label));
			Obj->SetStringField(TEXT("raw"), Json.Left(200));
		}
		return Obj;
	};

	TSharedPtr<FJsonObject> TraceObj = ParseOrError(TraceResult, TEXT("trace"));
	TSharedPtr<FJsonObject> ParsedLogObj = ParseOrError(LogResult, TEXT("logs"));
	bool bTraceSuccess = false;
	bool bLogSuccess = false;
	TraceObj->TryGetBoolField(TEXT("success"), bTraceSuccess);
	ParsedLogObj->TryGetBoolField(TEXT("success"), bLogSuccess);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("success"), bTraceSuccess && bLogSuccess);
	Root->SetBoolField(TEXT("partial"), bTraceSuccess != bLogSuccess);
	Root->SetStringField(TEXT("status"), bTraceSuccess && bLogSuccess ? TEXT("complete")
		: (bTraceSuccess || bLogSuccess ? TEXT("partial") : TEXT("failed")));
	Root->SetStringField(TEXT("source"), TEXT("both"));
	Root->SetObjectField(TEXT("trace"), TraceObj);
	Root->SetObjectField(TEXT("logs"), ParsedLogObj);
	TArray<TSharedPtr<FJsonValue>> Available;
	TArray<TSharedPtr<FJsonValue>> Failed;
	(bTraceSuccess ? Available : Failed).Add(MakeShared<FJsonValueString>(TEXT("trace")));
	(bLogSuccess ? Available : Failed).Add(MakeShared<FJsonValueString>(TEXT("logs")));
	Root->SetArrayField(TEXT("available_sources"), Available);
	Root->SetArrayField(TEXT("failed_sources"), Failed);

	FString Out;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root.ToSharedRef(), W);
	return Out;
}

// ===========================================================================
// AICallable methods
// ===========================================================================

// Per-thread advice keyed by the winning bottleneck. Kept next to FrameTiming so the verdict and its
// remediation hint stay in lockstep.
static FString BoundHint(const FString& Bound)
{
	if (Bound == TEXT("GPU"))
	{
		return TEXT("GPU-bound. NOW it is worth profiling the GPU: StartTrace (channels frame,gpu,cpu), then 'r.ProfileGPU.ShowUI 0' + 'ProfileGPU' and read the pass breakdown from the log. Levers: shadows (Virtual Shadow Maps), Lumen GI/reflections, translucency, post-process, ScreenPercentage. Confirm with the resolution test: 'r.ScreenPercentage 50' should noticeably raise FPS if truly GPU-bound.");
	}
	if (Bound == TEXT("RenderThread"))
	{
		return TEXT("CPU render-thread bound. Usual cause: too many draw calls / primitives, or many dynamic shadow-casting lights. Check 'stat scenerendering'. Levers: merge/instance meshes, enable Nanite, cut dynamic lights and per-light shadows. NOTE: dropping r.ScreenPercentage will NOT help a render-thread-bound frame.");
	}
	if (Bound == TEXT("RHIThread"))
	{
		return TEXT("CPU RHI-thread bound — the thread submitting GPU commands is the bottleneck, distinct from render-thread scene setup. Usual cause: too many draw calls / state changes saturating command submission. Check 'stat rhi' and 'stat scenerendering' (draw-call count). Levers: cut draw calls via merging/instancing/HISM, enable Nanite, reduce material/state permutations. NOTE: dropping r.ScreenPercentage will NOT help an RHI-thread-bound frame.");
	}
	return TEXT("CPU game-thread bound. Usual cause: Tick / Blueprint / AI / animation cost. Run 'stat dumpframe -ms=0.5 -root=gamethread' on the PIE world then read the result. Levers: throttle/disable unnecessary Tick, reduce ticking actors & AI, cut expensive Blueprint tick logic. NOTE: dropping r.ScreenPercentage will NOT help a game-thread-bound frame.");
}


FString UPerformanceService::ForceHitch(const FString& Thread, float Milliseconds, int32 Frames)
{
	const FString Mode = Thread.IsEmpty() ? TEXT("game") : Thread.ToLower();
	const bool bGame   = (Mode == TEXT("game")   || Mode == TEXT("both"));
	const bool bRender = (Mode == TEXT("render") || Mode == TEXT("both"));
	const bool bRhi    = (Mode == TEXT("rhi"));
	const bool bGpu    = (Mode == TEXT("gpu"));

	if (!bGame && !bRender && !bRhi && !bGpu)
	{
		return ErrJson(TEXT("BAD_THREAD"), FString::Printf(
			TEXT("Unknown Thread '%s'. Use 'game', 'render', 'both', 'rhi', or 'gpu'."), *Thread));
	}

	// Clamp so a fat-fingered value can't freeze the editor for a minute or spin forever.
	const float StallMs      = FMath::Clamp(Milliseconds, 1.0f, 5000.0f);
	const int32 HitchFrames  = FMath::Clamp(Frames, 1, 600);
	const float StallSeconds = StallMs / 1000.0f;

	// Tell the caller exactly what to expect from the verdict, so validation is a direct compare.
	const TCHAR* Expect =
		  (Mode == TEXT("both"))   ? TEXT("contested=true (GameThread and RenderThread within margin)")
		: (Mode == TEXT("render"))? TEXT("bound=RenderThread")
		: (Mode == TEXT("rhi"))   ? TEXT("bound=RHIThread WHEN RHI-threading is on; otherwise the stall folds into the render thread (no distinct RHIThread verdict)")
		: (Mode == TEXT("gpu"))   ? TEXT("bound=GPU IF the scene's GPU cost now exceeds the CPU threads (scene-dependent)")
		:                           TEXT("bound=GameThread");

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("forcing"), Mode);
	R->SetStringField(TEXT("expect"), Expect);

	// GPU forcing stays async: supersample by driving r.ScreenPercentage up, restored when the countdown
	// ends over HitchFrames frames. It's the only scene-agnostic GPU lever we have — the actual cost still
	// depends on what's on screen, so a trivial PIE scene may not tip the frame GPU-bound (best-effort). GPU
	// cost persists across the frames the reader sees, so unlike the CPU paths a follow-up FrameTiming reads
	// it back cleanly — which is why gpu keeps the read-a-following-frame flow instead of self-measuring.
	if (bGpu)
	{
		constexpr float GpuScreenPercentage = 400.0f;
		float SavedScreenPercentage = 100.0f;
		IConsoleVariable* ScreenPctCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
		if (ScreenPctCVar)
		{
			SavedScreenPercentage = ScreenPctCVar->GetFloat();
			ScreenPctCVar->Set(GpuScreenPercentage, ECVF_SetByConsole);
		}

		TSharedRef<int32> Remaining = MakeShared<int32>(HitchFrames);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[Remaining, ScreenPctCVar, SavedScreenPercentage](float) -> bool
			{
				if (--(*Remaining) > 0)
				{
					return true; // keep the supersample up for the next frame
				}
				if (ScreenPctCVar)
				{
					ScreenPctCVar->Set(SavedScreenPercentage, ECVF_SetByConsole); // restore + unregister
				}
				return false;
			}));

		R->SetNumberField(TEXT("frames"), HitchFrames);
		if (ScreenPctCVar)
		{
			R->SetNumberField(TEXT("screen_percentage"), GpuScreenPercentage);
			R->SetNumberField(TEXT("restore_screen_percentage"), SavedScreenPercentage);
		}
		else
		{
			R->SetStringField(TEXT("gpu_warning"), TEXT("r.ScreenPercentage cvar not found — GPU load NOT applied."));
		}
		if (!IsPIERunning())
		{
			R->SetStringField(TEXT("note"), TEXT("PIE is not running — the GPU load lands on the editor viewport frame. Start PIE for a game-representative validation."));
		}
		R->SetStringField(TEXT("hint"), TEXT("GPU cost is scene-dependent and not self-measured — call FrameTiming on a following frame and confirm bound=GPU. Unlike the CPU paths this reads back cleanly (the GPU cost persists across frames)."));
		return OkJson(R);
	}

	// CPU paths (game/render/both) — SELF-MEASURING, so validation is race-free. The naive approach schedules
	// the stall on a future frame and relies on a follow-up FrameTiming to catch it, but that read runs on the
	// game thread the stall blocks and reliably lands on a clean frame. Here we apply the stall synchronously
	// inside this call and time it directly, then return the induced peak per-thread ms and a
	// verdict_matched_expect flag — the caller validates from THIS single response, no second call.
	// Cap total synchronous blocking so a fat-fingered frames*ms can't freeze the editor for minutes.
	constexpr double TotalCapMs = 10000.0;
	const int32 EffectiveFrames = FMath::Min(HitchFrames, FMath::Max(1, (int32)(TotalCapMs / StallMs)));

	// "rhi" self-measures only when a separate RHI thread exists to stall. With RHI-threading off the RHI
	// work runs inline on the render thread, so there's no distinct RHIThread to bind the frame — we skip
	// the stall and flag it, mirroring how the verdict drops RHI from the ranking in that case.
	const bool bRhiThreaded = bRhi && IsRunningRHIInSeparateThread();

	double PeakGameMs = 0.0;
	double PeakRenderMs = 0.0;
	double PeakRhiMs = 0.0;
	for (int32 i = 0; i < EffectiveFrames; ++i)
	{
		if (bGame)
		{
			const double T0 = FPlatformTime::Seconds();
			FPlatformProcess::Sleep(StallSeconds);
			PeakGameMs = FMath::Max(PeakGameMs, (FPlatformTime::Seconds() - T0) * 1000.0);
		}
		if (bRender)
		{
			// Stall the render thread and have IT time its own sleep into a shared slot; FlushRenderingCommands
			// blocks the game thread until that command completes, so the measurement is done before we read it.
			TSharedRef<double, ESPMode::ThreadSafe> RtMs = MakeShared<double, ESPMode::ThreadSafe>(0.0);
			const float RtSeconds = StallSeconds;
			ENQUEUE_RENDER_COMMAND(VibePerfForceHitch)(
				[RtSeconds, RtMs](FRHICommandListImmediate&)
				{
					const double T0 = FPlatformTime::Seconds();
					FPlatformProcess::Sleep(RtSeconds);
					*RtMs = (FPlatformTime::Seconds() - T0) * 1000.0;
				});
			FlushRenderingCommands();
			PeakRenderMs = FMath::Max(PeakRenderMs, *RtMs);
		}
		if (bRhiThreaded)
		{
			// Stall the RHI thread specifically: from the render thread, enqueue a lambda onto the command
			// list (it executes on the RHI thread when threading is on) that times its own sleep, then flush
			// the RHI thread so the game thread blocks until that lambda has run and the slot is filled.
			TSharedRef<double, ESPMode::ThreadSafe> RhMs = MakeShared<double, ESPMode::ThreadSafe>(0.0);
			const float RhSeconds = StallSeconds;
			ENQUEUE_RENDER_COMMAND(VibePerfForceHitchRHI)(
				[RhSeconds, RhMs](FRHICommandListImmediate& RHICmdList)
				{
					RHICmdList.EnqueueLambda([RhSeconds, RhMs](FRHICommandListBase&)
					{
						const double T0 = FPlatformTime::Seconds();
						FPlatformProcess::Sleep(RhSeconds);
						*RhMs = (FPlatformTime::Seconds() - T0) * 1000.0;
					});
					RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
				});
			FlushRenderingCommands();
			PeakRhiMs = FMath::Max(PeakRhiMs, *RhMs);
		}
	}

	// A thread counts as "hit" if it induced at least half the requested stall (tolerates scheduler slop).
	const double MinExpectedMs = 0.5 * StallMs;
	const bool bMatched = PerformanceVerdict::HitchMatchesExpect(Mode, PeakGameMs, PeakRenderMs, MinExpectedMs, PeakRhiMs);

	R->SetNumberField(TEXT("frames"), EffectiveFrames);
	if (EffectiveFrames < HitchFrames)
	{
		R->SetNumberField(TEXT("frames_requested"), HitchFrames);
		R->SetStringField(TEXT("frames_note"), FString::Printf(
			TEXT("Capped to %d frame(s) (~%.0f ms total) so the synchronous stall can't freeze the editor."),
			EffectiveFrames, TotalCapMs));
	}
	R->SetNumberField(TEXT("stall_ms_per_frame"), StallMs);
	if (bGame)   { R->SetNumberField(TEXT("observed_peak_game_ms"), PeakGameMs); }
	if (bRender) { R->SetNumberField(TEXT("observed_peak_render_ms"), PeakRenderMs); }
	if (bRhi)
	{
		R->SetBoolField(TEXT("rhi_threading"), bRhiThreaded);
		if (bRhiThreaded)
		{
			R->SetNumberField(TEXT("observed_peak_rhi_ms"), PeakRhiMs);
		}
		else
		{
			R->SetStringField(TEXT("rhi_warning"), TEXT("RHI-threading is OFF — no separate RHI thread to stall, so no RHIThread verdict was induced. Re-run with RHI-threading on (r.RHIThread.Enable 1, or a platform where it's default) to validate the RHIThread bound."));
		}
	}
	R->SetBoolField(TEXT("verdict_matched_expect"), bMatched);
	if (!IsPIERunning())
	{
		R->SetStringField(TEXT("note"), TEXT("PIE is not running — the stall was still induced and measured directly on the editor frame, so verdict_matched_expect is valid. Start PIE for a game-representative FrameTiming reading."));
	}
	R->SetStringField(TEXT("hint"), TEXT("Validation is self-contained: 'verdict_matched_expect' compares the stall this call actually induced ('observed_peak_*_ms') against 'expect' — no follow-up FrameTiming needed (that read races the stall on the game thread and misses it)."));
	return OkJson(R);
}

// ---------------------------------------------------------------------------
// Report — render a self-contained HTML "printout" from the verdict + capture
// ---------------------------------------------------------------------------

// Verified UE 5.8 documentation URLs, linked per fix so the report tells you where to go next.
namespace PerformanceDocs
{
	static const TCHAR* PsoPrecache = TEXT("https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine");
	static const TCHAR* PsoCaches   = TEXT("https://dev.epicgames.com/documentation/en-us/unreal-engine/optimizing-rendering-with-pso-caches-in-unreal-engine");
	static const TCHAR* Profiling   = TEXT("https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-performance-profiling-and-configuration-in-unreal-engine");
	static const TCHAR* StatCommands= TEXT("https://dev.epicgames.com/documentation/en-us/unreal-engine/stat-commands-in-unreal-engine");
	static const TCHAR* Insights    = TEXT("https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-in-unreal-engine");
}

// Escape text destined for HTML body/attributes (paths, log lines, verdict strings).
static FString PerfHtmlEscape(const FString& In)
{
	FString Out = In;
	Out.ReplaceInline(TEXT("&"), TEXT("&amp;"));
	Out.ReplaceInline(TEXT("<"), TEXT("&lt;"));
	Out.ReplaceInline(TEXT(">"), TEXT("&gt;"));
	Out.ReplaceInline(TEXT("\""), TEXT("&quot;"));
	return Out;
}

// One recommended fix, rendered as a numbered card: what/why + the exact commands + a doc link.
struct FPerfFix
{
	FString Title;
	FString Why;
	FString How;      // concrete stat/console commands — shown in a mono block
	FString DocUrl;
	FString DocLabel;
};

// The report's stylesheet — inlined so the file is fully self-contained and opens anywhere. Mono headings,
// teal "trace" accent, semantic good/warn/crit, and both light + dark themes driven by tokens.
static const TCHAR* PerfReportCss = TEXT(R"CSS(<style>
:root{--bg:#eef2f1;--surface:#fff;--surface-2:#f6f9f8;--ink:#13201e;--muted:#586b68;--border:#dde5e3;--border-strong:#c7d2d0;--accent:#0c9a8b;--accent-ink:#0a7c70;--good:#1c8f52;--good-bg:#1c8f5216;--warn:#b1780a;--warn-bg:#b1780a18;--crit:#cc453b;--crit-bg:#cc453b16;--mono:"SFMono-Regular","Cascadia Code","JetBrains Mono","Fira Code",Menlo,Consolas,monospace;--sans:system-ui,-apple-system,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
@media(prefers-color-scheme:dark){:root{--bg:#0c1312;--surface:#111b1a;--surface-2:#0f1817;--ink:#e7f0ee;--muted:#93a6a3;--border:#22302e;--border-strong:#2d3d3a;--accent:#3ad6c6;--accent-ink:#5ee0d2;--good:#45c07e;--good-bg:#45c07e1f;--warn:#e0a53c;--warn-bg:#e0a53c22;--crit:#ec6f63;--crit-bg:#ec6f6320}}
*{box-sizing:border-box}
body{margin:0}
.wrap{background:var(--bg);color:var(--ink);font-family:var(--sans);line-height:1.5;padding:clamp(20px,4vw,52px);-webkit-font-smoothing:antialiased}
.sheet{max-width:960px;margin:0 auto}
.eyebrow{font-family:var(--mono);font-size:12px;letter-spacing:.18em;text-transform:uppercase;color:var(--accent-ink);display:flex;align-items:center;gap:10px;margin:0 0 14px}
.eyebrow::before{content:"";width:26px;height:2px;background:var(--accent);display:inline-block}
h1{font-family:var(--mono);font-weight:650;font-size:clamp(24px,4vw,36px);letter-spacing:-.01em;line-height:1.1;margin:0 0 10px}
.lede{font-size:15px;color:var(--muted);max-width:64ch;margin:0 0 30px}
.tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:14px;margin:0 0 30px}
.tile{background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:16px 18px;position:relative;overflow:hidden}
.tile::before{content:"";position:absolute;left:0;top:0;bottom:0;width:3px;background:var(--tc,var(--accent))}
.tile.good{--tc:var(--good)}.tile.warn{--tc:var(--warn)}.tile.crit{--tc:var(--crit)}
.tile .k{font-family:var(--mono);font-size:11px;letter-spacing:.1em;text-transform:uppercase;color:var(--muted);margin:0 0 8px}
.tile .v{font-family:var(--mono);font-size:clamp(22px,3vw,29px);font-weight:650;line-height:1;font-variant-numeric:tabular-nums}
.tile .v small{font-size:14px;font-weight:500;color:var(--muted);margin-left:3px}
.tile .s{font-size:12.5px;color:var(--muted);margin:8px 0 0}
thead th.rec{color:var(--accent-ink)}
tbody td{vertical-align:top}
.dim{font-weight:600;color:var(--ink);white-space:nowrap}
.dim small{display:block;font-weight:400;font-family:var(--mono);font-size:10.5px;letter-spacing:.04em;color:var(--muted);margin-top:3px;text-transform:uppercase}
td.warm,td.cold{color:var(--muted)}
td.warm b,td.cold b{color:var(--ink);font-family:var(--mono);font-variant-numeric:tabular-nums;font-weight:600}
td.rec-cell{color:var(--ink);border-left:1px solid var(--border)}
td.rec-cell code,td.warm code,td.cold code{font-family:var(--mono);font-size:12px;background:var(--surface-2);border:1px solid var(--border);padding:1px 5px;border-radius:4px}
.tag{display:inline-block;font-family:var(--mono);font-size:11px;font-weight:600;letter-spacing:.03em;padding:2px 7px;border-radius:5px;white-space:nowrap}
.tag.hidden{color:var(--crit);background:var(--crit-bg)}
.tag.caught{color:var(--good);background:var(--good-bg)}
.tag.warn{color:var(--warn);background:var(--warn-bg)}
h2{font-family:var(--mono);font-size:13px;letter-spacing:.1em;text-transform:uppercase;color:var(--accent-ink);margin:34px 0 14px}
.card{background:var(--surface);border:1px solid var(--border);border-radius:12px;overflow:hidden}
.table-scroll{overflow-x:auto}
table{border-collapse:collapse;width:100%;min-width:520px;font-size:14px}
thead th{text-align:left;font-family:var(--mono);font-size:11px;letter-spacing:.08em;text-transform:uppercase;color:var(--muted);font-weight:600;padding:12px 16px;background:var(--surface-2);border-bottom:1px solid var(--border-strong);white-space:nowrap}
tbody td{padding:11px 16px;border-bottom:1px solid var(--border);font-variant-numeric:tabular-nums}
tbody tr:last-child td{border-bottom:none}
tbody tr.bound td{background:var(--accent-wash,transparent)}
.th-name{font-family:var(--mono);font-weight:600}
.num{font-family:var(--mono)}
.pill{display:inline-block;font-family:var(--mono);font-size:11px;font-weight:600;padding:2px 8px;border-radius:20px;white-space:nowrap}
.pill.good{color:var(--good);background:var(--good-bg)}.pill.warn{color:var(--warn);background:var(--warn-bg)}.pill.crit{color:var(--crit);background:var(--crit-bg)}
.fixes{display:flex;flex-direction:column;gap:14px;counter-reset:fx}
.fix{counter-increment:fx;background:var(--surface);border:1px solid var(--border);border-radius:12px;padding:18px 20px;display:grid;grid-template-columns:auto 1fr;gap:16px}
.fix::before{content:counter(fx);font-family:var(--mono);font-weight:650;font-size:15px;color:var(--accent);width:30px;height:30px;display:grid;place-items:center;border:1px solid var(--border-strong);border-radius:8px;align-self:start}
.fix .ft{font-weight:600;margin:0 0 4px}
.fix .fw{color:var(--muted);font-size:13.5px;margin:0 0 12px}
.fix .how-l{font-family:var(--mono);font-size:10.5px;letter-spacing:.1em;text-transform:uppercase;color:var(--accent-ink);margin:0 0 6px}
.fix pre{font-family:var(--mono);font-size:12.5px;background:var(--surface-2);border:1px solid var(--border);border-radius:7px;padding:10px 12px;margin:0 0 12px;overflow-x:auto;color:var(--ink);white-space:pre-wrap}
.fix a.doc{font-size:13px;color:var(--accent-ink);text-decoration:none;font-weight:600;border-bottom:1px solid var(--accent)}
.fix a.doc:hover{opacity:.8}
.guide{margin:20px 0 0;padding:16px 20px;border:1px dashed var(--border-strong);border-radius:12px;font-size:13.5px;color:var(--muted)}
.guide b{color:var(--ink)}
.worst{list-style:none;margin:0;padding:0;font-family:var(--mono);font-size:13px}
.worst li{display:flex;justify-content:space-between;gap:14px;padding:8px 16px;border-bottom:1px solid var(--border);font-variant-numeric:tabular-nums}
.worst li:last-child{border-bottom:none}
.worst .fm{color:var(--muted)}
footer{margin:28px 2px 0;font-family:var(--mono);font-size:11px;letter-spacing:.03em;color:var(--muted);display:flex;flex-wrap:wrap;gap:6px 18px}
footer b{color:var(--ink);font-weight:600}
a:focus-visible,.doc:focus-visible{outline:2px solid var(--accent);outline-offset:2px}
</style>)CSS");








FString UPerformanceService::Analyse(const FString& Source, const FString& File)
{
	const FString Src = Source.IsEmpty() ? TEXT("both") : Source.ToLower();

	FString TraceFile = File.IsEmpty() ? GLastTraceFilePath : File;
	FString LogFile   = GLastLogFilePath;
	if (LogFile.IsEmpty())
	{
		LogFile = ProjectSavedDirAbs() / TEXT("Logs") / (FApp::GetProjectName() + FString(TEXT(".log")));
	}

	if (Src == TEXT("trace"))
	{
		if (TraceFile.IsEmpty()) return ErrJson(TEXT("NO_TRACE"), TEXT("No trace file known. Run StartTrace first, or pass File=<path>."));
		return VibeUEPerformanceAnalysis::AnalyseTrace(TraceFile);
	}
	if (Src == TEXT("logs"))
	{
		return VibeUEPerformanceAnalysis::AnalyseLogs(File.IsEmpty() ? LogFile : File);
	}

	// both
	return VibeUEPerformanceAnalysis::AnalyseBoth(TraceFile, LogFile);
}

FString UPerformanceService::StartStandalone(const FString& Name, const FString& Channels)
{
	if (!GEditor) return ErrJson(TEXT("NO_EDITOR"), TEXT("GEditor not available."));
	if (GStandaloneRunning) return ErrJson(TEXT("ALREADY_RUNNING"), TEXT("Standalone is already running. Call StopStandalone first."));

	const FString TraceName = VibeUEPerformanceAnalysis::MakeStandaloneSessionStem(Name);
	const FString ChannelSet = Channels.IsEmpty() ? FString(DefaultTraceChannels()) : Channels;
	FString TracePath = BuildTraceFilePath(TraceName);
	GStandaloneTraceFilePath = TracePath + TEXT(".utrace");
	GLastTraceFilePath = GStandaloneTraceFilePath;
	GStandaloneSessionId = TraceName;
	GStandaloneStartedUtc = FDateTime::UtcNow();
	GStandaloneStartVerified = false;
	GStandaloneFinalized = false;
	GStandaloneForcedTermination = false;
	GStandalonePID = 0;
	GStandaloneProcess.Reset();
	GStandaloneMap.Reset();
	if (const UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
	{
		GStandaloneMap = EditorWorld->GetOutermost()->GetName();
	}

	// Give the standalone process its own timestamped log via -abslog, rather than sharing the
	// project's default <Project>.log. That file is held open by the running editor, so LoadFileToString
	// fails on it (surfacing as a misleading LOG_NOT_FOUND) and a "newest log in the folder" guess picks
	// the editor's live log instead of the standalone's. A dedicated, unique path is unlocked and exact.
	FString LogDir = ProjectSavedDirAbs() / TEXT("Logs");
	IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*LogDir);
	GStandaloneLogFilePath = LogDir / (TraceName + TEXT(".log"));
	GLastLogFilePath = GStandaloneLogFilePath;

	FString ExtraArgs = FString::Printf(
		TEXT("-trace=%s -tracefile=\"%s\" -abslog=\"%s\""),
		*ChannelSet, *GStandaloneTraceFilePath, *GStandaloneLogFilePath);

	GStandalonePlayDelegateHandle = FEditorDelegates::BeginStandaloneLocalPlay.AddLambda([](uint32 PID)
	{
		GStandalonePID = PID;
		GStandaloneProcess = FPlatformProcess::OpenProcess(PID);
		GStandaloneStartVerified = GStandaloneProcess.IsValid()
			&& FPlatformProcess::IsProcRunning(GStandaloneProcess);
		FEditorDelegates::BeginStandaloneLocalPlay.Remove(GStandalonePlayDelegateHandle);
		GStandalonePlayDelegateHandle.Reset();
	});

	FRequestPlaySessionParams P;
	P.SessionDestination = EPlaySessionDestinationType::NewProcess;
	P.WorldType = EPlaySessionWorldType::PlayInEditor;
	P.AdditionalStandaloneCommandLineParameters = ExtraArgs;
	GEditor->RequestPlaySession(P);
	GStandaloneRunning = true;

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), false);
	R->SetBoolField(TEXT("pending"), true);
	R->SetBoolField(TEXT("request_accepted"), true);
	R->SetStringField(TEXT("status"), TEXT("start_pending"));
	R->SetStringField(TEXT("session_id"), GStandaloneSessionId);
	R->SetStringField(TEXT("map"), GStandaloneMap);
	R->SetStringField(TEXT("trace_file"), GLastTraceFilePath);
	R->SetStringField(TEXT("log_file"),   GLastLogFilePath);
	R->SetStringField(TEXT("channels"),   ChannelSet);
	R->SetStringField(TEXT("trace_transport"), TEXT("direct_file"));
	R->SetStringField(TEXT("hint"), TEXT("Poll GetStandaloneStatus until capture_verified=true before driving the workload."));
	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(R.ToSharedRef(), Writer);
	return Out;
}

FString UPerformanceService::StopStandalone()
{
	if (!GStandaloneRunning) return ErrJson(TEXT("NOT_RUNNING"), TEXT("No standalone session tracked. Did you call StartStandalone?"));
	// Separate-process PIE has no PlayWorld in the editor, so RequestEndPlayMap is a no-op and
	// EndPlayMap asserts. Ask the tracked game window to close so Unreal can flush its trace/log.
	bool bGracefulExitRequested = GStandalonePID != 0
		&& RequestStandaloneExitGracefully(GStandalonePID);

	if (GStandalonePlayDelegateHandle.IsValid())
	{
		FEditorDelegates::BeginStandaloneLocalPlay.Remove(GStandalonePlayDelegateHandle);
		GStandalonePlayDelegateHandle.Reset();
	}

	bool bForced = false;
	bool bProcessExited = false;
	if (GStandaloneProcess.IsValid())
	{
		double StartTime = FPlatformTime::Seconds();
		while (FPlatformProcess::IsProcRunning(GStandaloneProcess)
			   && (FPlatformTime::Seconds() - StartTime) < 10.0)
		{
			// Startup may replace its splash window with the game window after Stop was requested.
			// Re-send WM_CLOSE to all current top-level windows for this exact PID during the bound.
			bGracefulExitRequested |= RequestStandaloneExitGracefully(GStandalonePID);
			FPlatformProcess::Sleep(0.2f);
		}
		if (FPlatformProcess::IsProcRunning(GStandaloneProcess))
		{
			FPlatformProcess::TerminateProc(GStandaloneProcess);
			bForced = true;
			FPlatformProcess::WaitForProc(GStandaloneProcess);
		}
		bProcessExited = !FPlatformProcess::IsProcRunning(GStandaloneProcess);
		FPlatformProcess::CloseProc(GStandaloneProcess);
		GStandaloneProcess.Reset();
	}

	GStandaloneRunning = false;
	GStandaloneForcedTermination = bForced;

	// Finalize and verify only the exact direct-file destination assigned to this session.
	int64 TraceSize = IFileManager::Get().FileSize(*GStandaloneTraceFilePath);
	int64 PreviousSize = INDEX_NONE;
	int32 StableChecks = 0;
	const double FinalizeStart = FPlatformTime::Seconds();
	while ((FPlatformTime::Seconds() - FinalizeStart) < 5.0 && StableChecks < 2)
	{
		FPlatformProcess::Sleep(0.2f);
		TraceSize = IFileManager::Get().FileSize(*GStandaloneTraceFilePath);
		if (TraceSize > 0 && TraceSize == PreviousSize) ++StableChecks;
		else StableChecks = 0;
		PreviousSize = TraceSize;
	}
	GStandaloneFinalized = GStandaloneStartVerified && bProcessExited && !bForced
		&& TraceSize > 0 && StableChecks >= 2;

	// GLastLogFilePath was set to a dedicated timestamped file in StartStandalone (-abslog), so it is
	// already the standalone's own log — no need to guess the newest log in the folder (which would
	// pick the editor's live, locked log).

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), GStandaloneFinalized);
	R->SetBoolField(TEXT("partial"), !GStandaloneFinalized && (TraceSize > 0 || FPaths::FileExists(GStandaloneLogFilePath)));
	R->SetStringField(TEXT("status"), GStandaloneFinalized ? TEXT("finalized") : TEXT("finalization_failed"));
	R->SetStringField(TEXT("session_id"), GStandaloneSessionId);
	R->SetNumberField(TEXT("pid"), GStandalonePID);
	R->SetBoolField(TEXT("start_verified"), GStandaloneStartVerified);
	R->SetBoolField(TEXT("graceful_exit_requested"), bGracefulExitRequested);
	R->SetBoolField(TEXT("process_exited"), bProcessExited);
	R->SetBoolField(TEXT("forced_termination"), bForced);
	R->SetStringField(TEXT("trace_file"), GLastTraceFilePath);
	R->SetStringField(TEXT("log_file"),   GLastLogFilePath);
	R->SetNumberField(TEXT("trace_size_bytes"), TraceSize > 0 ? TraceSize : 0);
	const FString UTSStore = GetUTSStoreDir();
	R->SetStringField(TEXT("trace_transport"), TEXT("direct_file"));
	R->SetStringField(TEXT("uts_store"), UTSStore);
	R->SetBoolField(TEXT("uts_store_available"), !UTSStore.IsEmpty());
	R->SetStringField(TEXT("uts_store_note"), UTSStore.IsEmpty()
		? TEXT("Unreal Trace Server store unavailable; this direct-file capture does not depend on it.")
		: TEXT("Reported for diagnostics only; this session never selects traces from the global store."));
	R->SetStringField(TEXT("hint"), GStandaloneFinalized
		? TEXT("The exact trace destination is finalized; call Analyse to read it.")
		: TEXT("Capture could not be fully verified. Inspect start_verified, forced_termination, and the exact trace/log paths; Analyse(both) will report any log-only partial result."));
	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(R.ToSharedRef(), Writer);
	return Out;
}

FString UPerformanceService::GetStandaloneStatus()
{
	const bool bProcessRunning = GStandaloneProcess.IsValid()
		&& FPlatformProcess::IsProcRunning(GStandaloneProcess);
	const int64 TraceSize = GStandaloneTraceFilePath.IsEmpty() ? 0
		: IFileManager::Get().FileSize(*GStandaloneTraceFilePath);
	const bool bCaptureVerified = GStandaloneStartVerified && bProcessRunning && TraceSize > 0;
	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("success"), bCaptureVerified || GStandaloneFinalized);
	R->SetBoolField(TEXT("running"), GStandaloneRunning);
	R->SetBoolField(TEXT("process_running"), bProcessRunning);
	R->SetBoolField(TEXT("start_verified"), GStandaloneStartVerified);
	R->SetBoolField(TEXT("capture_verified"), bCaptureVerified);
	R->SetBoolField(TEXT("finalized"), GStandaloneFinalized);
	R->SetBoolField(TEXT("forced_termination"), GStandaloneForcedTermination);
	R->SetStringField(TEXT("status"), GStandaloneFinalized ? TEXT("finalized")
		: (bCaptureVerified ? TEXT("capturing") : (GStandaloneRunning ? TEXT("start_pending") : TEXT("idle"))));
	R->SetStringField(TEXT("session_id"), GStandaloneSessionId);
	R->SetStringField(TEXT("map"), GStandaloneMap);
	R->SetNumberField(TEXT("pid"), GStandalonePID);
	R->SetStringField(TEXT("started_utc"), GStandaloneStartedUtc == FDateTime() ? TEXT("") : GStandaloneStartedUtc.ToIso8601());
	R->SetNumberField(TEXT("trace_size_bytes"), TraceSize > 0 ? TraceSize : 0);
	R->SetStringField(TEXT("trace_transport"), TEXT("direct_file"));
	R->SetStringField(TEXT("last_trace_file"), GLastTraceFilePath);
	R->SetStringField(TEXT("last_log_file"),   GLastLogFilePath);
	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(R.ToSharedRef(), Writer);
	return Out;
}



// True while an agent has asked for full-rate rendering regardless of focus (issue #549).
static bool GDisableBackgroundThrottling = false;
// The ShouldDisableCPUThrottlingDelegates entry. Present ONLY while GDisableBackgroundThrottling is set,
// and it is a lambda compiled into this DLL, so ShutdownThrottlingHook removes it (FModule::OnPreExit and
// FModule::ShutdownModule).
static FDelegateHandle GBackgroundThrottlingHook;
// bThrottleCPUWhenNotForeground as it was before this service turned it off. ShutdownThrottlingHook puts it
// back, so an override nobody re-enabled does not outlive the module that made it.
static TOptional<bool> GThrottleSettingBeforeOverride;

// Removes this service's entry by its own handle, so another system's entry (InputService's hold scope among
// them) is never touched. With GEditor already gone (late module shutdown) the array went with the editor
// engine; only the handle is reset.
static void RemoveBackgroundThrottlingHook()
{
	if (!GBackgroundThrottlingHook.IsValid())
	{
		return;
	}
	if (GEditor)
	{
		const FDelegateHandle Handle = GBackgroundThrottlingHook;
		GEditor->ShouldDisableCPUThrottlingDelegates.RemoveAll(
			[Handle](const UEditorEngine::FShouldDisableCPUThrottling& D) { return D.GetHandle() == Handle; });
	}
	GBackgroundThrottlingHook.Reset();
}

FString UPerformanceService::SetBackgroundThrottling(bool bEnabled)
{
	if (!GEditor) return ErrJson(TEXT("NO_EDITOR"), TEXT("GEditor not available."));

	UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
	if (!Settings) return ErrJson(TEXT("NO_SETTINGS"), TEXT("EditorPerformanceSettings not available."));

	const bool bPreviousThrottle = Settings->bThrottleCPUWhenNotForeground != 0;
	// Remember the value from before the FIRST disable (a repeated disable keeps it); re-enabling ends the override.
	if (!bEnabled && !GDisableBackgroundThrottling)
	{
		GThrottleSettingBeforeOverride = bPreviousThrottle;
	}
	else if (bEnabled)
	{
		GThrottleSettingBeforeOverride.Reset();
	}
	Settings->bThrottleCPUWhenNotForeground = bEnabled;
	// Session-only on purpose: no SaveConfig(). PostEditChange so the unfocused-viewport render
	// disable (which reads this setting) re-evaluates.
	Settings->PostEditChange();

	// The settings flag doesn't cover a minimized editor (AreAllWindowsHidden) — the engine checks
	// ShouldDisableCPUThrottlingDelegates first, so one entry is registered exactly while throttling is off.
	// The engine skips throttling as soon as ANY entry returns true, so entries compose as an OR: this one and
	// InputService's hold scope each return true only while their own reason holds, and each removes only its
	// own entry, by handle.
	GDisableBackgroundThrottling = !bEnabled;
	if (GDisableBackgroundThrottling && !GBackgroundThrottlingHook.IsValid())
	{
		UEditorEngine::FShouldDisableCPUThrottling Delegate =
			UEditorEngine::FShouldDisableCPUThrottling::CreateLambda([]() { return GDisableBackgroundThrottling; });
		GBackgroundThrottlingHook = Delegate.GetHandle();
		GEditor->ShouldDisableCPUThrottlingDelegates.Add(MoveTemp(Delegate));
	}
	else if (!GDisableBackgroundThrottling)
	{
		RemoveBackgroundThrottlingHook();
	}

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("throttling_enabled"), bEnabled);
	R->SetBoolField(TEXT("previous"), bPreviousThrottle);
	R->SetStringField(TEXT("note"), bEnabled
		? TEXT("Editor background throttling restored to normal.")
		: TEXT("Editor runs at full rate while unfocused/minimized for this session. No more AppActivate hacks; re-enable when done."));
	return OkJson(R);
}

FString UPerformanceService::GetBackgroundThrottling()
{
	const UEditorPerformanceSettings* Settings = GetDefault<UEditorPerformanceSettings>();
	if (!Settings) return ErrJson(TEXT("NO_SETTINGS"), TEXT("EditorPerformanceSettings not available."));

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("throttling_enabled"), Settings->bThrottleCPUWhenNotForeground != 0);
	R->SetBoolField(TEXT("delegate_override_active"), GDisableBackgroundThrottling);
	return OkJson(R);
}

void UPerformanceService::ShutdownThrottlingHook()
{
	RemoveBackgroundThrottlingHook();
	// An override still on at teardown goes back to the value from before it. Assigned without PostEditChange:
	// this runs during editor exit / module unload, and the engine reads the flag directly every tick anyway
	// (UEditorEngine::ShouldThrottleCPUUsage and the unfocused-viewport check).
	if (GDisableBackgroundThrottling && GThrottleSettingBeforeOverride.IsSet() && UObjectInitialized())
	{
		if (UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>())
		{
			Settings->bThrottleCPUWhenNotForeground = GThrottleSettingBeforeOverride.GetValue();
		}
	}
	GDisableBackgroundThrottling = false;
	GThrottleSettingBeforeOverride.Reset();
}
