// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UPerformanceService.h"
#include "PythonAPI/PerformanceVerdict.h"
#include "Json.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
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
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/Frames.h"
#include "RenderTimer.h"       // GGameThreadTime / GRenderThreadTime / GRHIThreadTime (RenderCore)
#include "RHIGlobals.h"        // RHIGetGPUFrameCycles (RHI)

DEFINE_LOG_CATEGORY_STATIC(LogPerformance, Log, All);

// ---------------------------------------------------------------------------
// Session state — persists across MCP requests within an editor session
// ---------------------------------------------------------------------------

static FString GLastTraceFilePath;
static FString GLastLogFilePath;
static bool           GStandaloneRunning = false;
static FProcHandle    GStandaloneProcess;
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

// Read StoreDir from the Unreal Trace Server settings file (used when -tracehost is set).
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

// Find the most recently modified .utrace file in the UTS store.
static FString FindLatestUTSTrace()
{
	FString StoreDir = GetUTSStoreDir();
	if (StoreDir.IsEmpty()) return FString();

	FString Latest;
	FDateTime LatestTime = FDateTime::MinValue();

	IFileManager::Get().IterateDirectory(*StoreDir, [&](const TCHAR* Path, bool bDir) -> bool
	{
		if (!bDir && FPaths::GetExtension(Path).Equals(TEXT("utrace"), ESearchCase::IgnoreCase))
		{
			FDateTime T = IFileManager::Get().GetTimeStamp(Path);
			if (T > LatestTime)
			{
				LatestTime = T;
				Latest = Path;
			}
		}
		return true;
	});

	return Latest;
}

// ---------------------------------------------------------------------------
// Trace analysis
// ---------------------------------------------------------------------------

static FString AnalyseTrace(const FString& TraceFile)
{
	if (!FPaths::FileExists(TraceFile))
	{
		return ErrJson(TEXT("FILE_NOT_FOUND"), FString::Printf(TEXT("Trace file not found: %s"), *TraceFile));
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
					int32 P95Idx = FMath::Clamp((int32)(AllMs.Num() * 0.95), 0, AllMs.Num() - 1);
					Root->SetNumberField(TEXT("p95_frame_ms"), FMath::RoundToFloat(AllMs[P95Idx] * 100.0f) / 100.0f);
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

static FString AnalyseLogs(const FString& LogFile)
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
	int32 PSOHitches = 0;
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

		if (Line.Contains(TEXT("PSO creation hitch"))) ++PSOHitches;

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
	Root->SetNumberField(TEXT("pso_hitches"), PSOHitches);
	Root->SetArrayField(TEXT("notable_lines"), Notable);

	return OkJson(Root);
}

static FString AnalyseBoth(const FString& TraceFile, const FString& LogFile)
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

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("success"), true);
	Root->SetStringField(TEXT("source"), TEXT("both"));
	Root->SetObjectField(TEXT("trace"), ParseOrError(TraceResult, TEXT("trace")));
	Root->SetObjectField(TEXT("logs"),  ParseOrError(LogResult,   TEXT("logs")));

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

FString UPerformanceService::FrameTiming(float TargetFPS)
{
	const double GameMs   = FPlatformTime::ToMilliseconds(GGameThreadTime);
	const double RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	const double RHIMs    = FPlatformTime::ToMilliseconds(GRHIThreadTime);
	const double GpuMs    = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());

	auto Round2 = [](double V) -> double { return FMath::RoundToDouble(V * 100.0) / 100.0; };

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetNumberField(TEXT("game_thread_ms"),   Round2(GameMs));
	R->SetNumberField(TEXT("render_thread_ms"), Round2(RenderMs));
	R->SetNumberField(TEXT("rhi_thread_ms"),    Round2(RHIMs));
	R->SetNumberField(TEXT("gpu_ms"),           Round2(GpuMs));

	const bool bRhiAvailable = RHIMs > 0.0;
	const bool bGpuAvailable = GpuMs > 0.0;
	// Every ranked thread runs in parallel, so the frame is bounded by the slowest of them. RHIMs/GpuMs are
	// 0 when unavailable, so Max is safe — an absent thread can never be the max.
	const double FrameMs = FMath::Max(FMath::Max3(GameMs, RenderMs, GpuMs), RHIMs);
	R->SetNumberField(TEXT("frame_ms"), Round2(FrameMs));
	R->SetNumberField(TEXT("fps"), FrameMs > 0.0 ? Round2(1000.0 / FrameMs) : 0.0);

	// --- Robust verdict -------------------------------------------------------
	// Rank the candidate threads (GPU only counts when its timing is available) and judge how decisive
	// the winner is. Pure logic lives in PerformanceVerdict::Classify so it can be unit-tested headless.
	const PerformanceVerdict::FVerdict Verdict = PerformanceVerdict::Classify(GameMs, RenderMs, RHIMs, GpuMs, bRhiAvailable, bGpuAvailable);
	const FString Bound      = Verdict.Bound;
	const double  TopMs      = Verdict.TopMs;
	const double  RunnerMs   = Verdict.RunnerMs;
	const FString RunnerName = Verdict.RunnerName;
	const double  MarginMs   = Verdict.MarginMs;
	const bool    bContested = Verdict.bContested;

	R->SetStringField(TEXT("bound"), Bound);
	R->SetStringField(TEXT("bound_confidence"), Verdict.Confidence);
	R->SetNumberField(TEXT("margin_ms"), Round2(MarginMs));
	R->SetBoolField(TEXT("contested"), bContested);

	FString Hint = BoundHint(Bound);
	if (bContested && !RunnerName.IsEmpty())
	{
		R->SetStringField(TEXT("contested_with"), RunnerName);
		Hint = FString::Printf(
			TEXT("CONTESTED: %s (%.2f ms) and %s (%.2f ms) are within %.2f ms — frame-to-frame noise can flip which one 'wins', so treat both as bottlenecks. Take several readings, or trace both. %s"),
			*Bound, TopMs, *RunnerName, RunnerMs, MarginMs, *BoundHint(Bound));
	}
	else if (!bGpuAvailable && Bound != TEXT("GPU"))
	{
		Hint = FString::Printf(
			TEXT("%s NOTE: GPU timing is unavailable this frame, so this CPU verdict is unconfirmed — a hidden GPU cost could still be the real bottleneck. Confirm with the 'r.ScreenPercentage 50' test."),
			*Hint);
	}
	R->SetStringField(TEXT("hint"), Hint);

	// --- Frame-time budget gate ----------------------------------------------
	// FPS is 1000/frame_ms, and the threads run in parallel, so EACH thread must individually finish
	// inside the per-frame budget. Report every thread's headroom against the target and gate on the
	// bottleneck — this is what turns a one-shot reading into a pass/fail guard.
	const PerformanceVerdict::FBudget Gate = PerformanceVerdict::ComputeBudget(FrameMs, TargetFPS);
	const double BudgetMs = Gate.BudgetMs;

	TSharedPtr<FJsonObject> Budget = MakeShared<FJsonObject>();
	Budget->SetNumberField(TEXT("target_fps"), Round2(Gate.TargetFps));
	Budget->SetNumberField(TEXT("budget_ms"), Round2(BudgetMs));

	auto ThreadBudget = [&](double Ms, bool bAvailable) -> TSharedPtr<FJsonValue>
	{
		TSharedPtr<FJsonObject> T = MakeShared<FJsonObject>();
		T->SetNumberField(TEXT("ms"), Round2(Ms));
		if (bAvailable)
		{
			T->SetNumberField(TEXT("headroom_ms"), Round2(BudgetMs - Ms));
			T->SetBoolField(TEXT("over_budget"), PerformanceVerdict::IsOverBudget(Ms, BudgetMs));
		}
		else
		{
			T->SetBoolField(TEXT("available"), false);
		}
		return MakeShared<FJsonValueObject>(T);
	};

	TSharedPtr<FJsonObject> PerThread = MakeShared<FJsonObject>();
	PerThread->SetField(TEXT("game_thread"),   ThreadBudget(GameMs, true));
	PerThread->SetField(TEXT("render_thread"), ThreadBudget(RenderMs, true));
	PerThread->SetField(TEXT("gpu"),           ThreadBudget(GpuMs, bGpuAvailable));
	Budget->SetObjectField(TEXT("threads"), PerThread);

	Budget->SetNumberField(TEXT("frame_headroom_ms"), Round2(Gate.FrameHeadroomMs));
	const bool bMeetsTarget = Gate.bMeetsTarget;
	Budget->SetBoolField(TEXT("meets_target"), bMeetsTarget);
	Budget->SetStringField(TEXT("verdict"), bMeetsTarget ? TEXT("PASS") : TEXT("FAIL"));
	if (!bMeetsTarget && FrameMs > 0.0)
	{
		Budget->SetStringField(TEXT("budget_note"), FString::Printf(
			TEXT("%s is %.2f ms over the %.2f ms budget for %g FPS — that thread alone caps you at ~%.0f FPS. Fix the bottleneck thread, not whichever is cheapest."),
			*Bound, TopMs - BudgetMs, BudgetMs, Gate.TargetFps, FrameMs > 0.0 ? 1000.0 / FrameMs : 0.0));
	}
	R->SetObjectField(TEXT("budget"), Budget);

	const bool bPIE = IsPIERunning();
	R->SetBoolField(TEXT("pie_running"), bPIE);
	R->SetStringField(TEXT("note"),
		bPIE
		? TEXT("Values are for the most recently rendered PIE frame. Park in a representative/worst spot for a clean read.")
		: TEXT("PIE is NOT running — these values reflect the EDITOR viewport, not your game. Start PIE (Epic's EditorAppToolset.StartPIE, or VibeUE's StartPIE) for a real game-bound reading."));
	if (GpuMs <= 0.0)
	{
		R->SetStringField(TEXT("gpu_note"), TEXT("gpu_ms is 0 (GPU timing unavailable this frame) — rely on the game vs render comparison, and confirm GPU-bound with the r.ScreenPercentage 50 test."));
	}
	return OkJson(R);
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

FString UPerformanceService::Report(const FString& Title, const FString& Source, const FString& File)
{
	auto Deserialize = [](const FString& Json) -> TSharedPtr<FJsonObject>
	{
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Obj);
		return Obj;
	};
	auto Num = [](const TSharedPtr<FJsonObject>& O, const TCHAR* K, double D = 0.0) -> double
	{ double V = D; if (O.IsValid()) { O->TryGetNumberField(K, V); } return V; };
	auto Str = [](const TSharedPtr<FJsonObject>& O, const TCHAR* K) -> FString
	{ FString V; if (O.IsValid()) { O->TryGetStringField(K, V); } return V; };

	// --- Gather: live verdict + budget, and the capture summary (best-effort) --------------------------
	const TSharedPtr<FJsonObject> FT = Deserialize(FrameTiming(60.0f));
	if (!FT.IsValid())
	{
		return ErrJson(TEXT("REPORT_NO_TIMING"), TEXT("FrameTiming returned no parseable data to report on."));
	}
	const TSharedPtr<FJsonObject>* BudgetPtr = nullptr;
	FT->TryGetObjectField(TEXT("budget"), BudgetPtr);
	const TSharedPtr<FJsonObject> Budget = BudgetPtr ? *BudgetPtr : nullptr;

	const TSharedPtr<FJsonObject> AN = Deserialize(Analyse(Source, File));
	const TSharedPtr<FJsonObject>* TracePtr = nullptr;
	const TSharedPtr<FJsonObject>* LogsPtr  = nullptr;
	if (AN.IsValid()) { AN->TryGetObjectField(TEXT("trace"), TracePtr); AN->TryGetObjectField(TEXT("logs"), LogsPtr); }
	const TSharedPtr<FJsonObject> Trace = TracePtr ? *TracePtr : nullptr;
	const TSharedPtr<FJsonObject> Logs  = LogsPtr  ? *LogsPtr  : nullptr;

	const double GameMs   = Num(FT, TEXT("game_thread_ms"));
	const double RenderMs = Num(FT, TEXT("render_thread_ms"));
	const double RhiMs    = Num(FT, TEXT("rhi_thread_ms"));
	const double GpuMs    = Num(FT, TEXT("gpu_ms"));
	const double FrameMs  = Num(FT, TEXT("frame_ms"));
	const double Fps      = Num(FT, TEXT("fps"));
	const FString Bound   = Str(FT, TEXT("bound"));
	const FString Conf    = Str(FT, TEXT("bound_confidence"));
	const double BudgetMs = Num(Budget, TEXT("budget_ms"), 16.67);
	const double TargetFps= Num(Budget, TEXT("target_fps"), 60.0);
	const bool bMeets     = Budget.IsValid() && Budget->GetBoolField(TEXT("meets_target"));
	const bool bPIE       = FT->GetBoolField(TEXT("pie_running"));

	const bool bHasTrace = Trace.IsValid() && Num(Trace, TEXT("frame_count")) > 0.0;
	const double MaxFrameMs = Num(Trace, TEXT("max_frame_ms"));
	const double P95Ms      = Num(Trace, TEXT("p95_frame_ms"));
	const int32  FrameCount = (int32)Num(Trace, TEXT("frame_count"));
	const double AvgFps     = Num(Trace, TEXT("avg_fps"));
	const double DurationS  = Num(Trace, TEXT("duration_seconds"));
	const bool bHasLogs  = Logs.IsValid();
	const int32 PsoHitches = (int32)Num(Logs, TEXT("pso_hitches"));
	const int32 LogErrors  = (int32)Num(Logs, TEXT("errors"));
	const int32 LogWarnings= (int32)Num(Logs, TEXT("warnings"));

	// --- Build the "fix in this order" list from the data ---------------------------------------------
	TArray<FPerfFix> Fixes;
	if (bHasTrace && MaxFrameMs > 250.0)
	{
		Fixes.Add({
			FString::Printf(TEXT("Startup / load hitch — %s ms peak frame"), *FString::FormatAsNumber((int64)MaxFrameMs)),
			TEXT("A representative (cold-cache) run pays load, PSO and shader-compilation costs the editor's warm caches hide. This is almost always the single biggest stall a player feels."),
			TEXT("# Ship a bundled PSO cache + keep precaching on (default):\nr.PSOPrecache.GlobalShaders 1\n# Then async-stream the opening level so the first frame isn't a hard stall."),
			PerformanceDocs::PsoPrecache, TEXT("PSO Precaching — UE 5.8 docs") });
	}
	if (Bound == TEXT("GameThread"))
	{
		Fixes.Add({ TEXT("Game-thread bound — CPU logic cost"),
			TEXT("Tick / Blueprint / AI / animation dominate the frame. Dropping resolution or GPU cost will not help a game-thread-bound frame."),
			TEXT("stat dumpframe -ms=0.5 -root=gamethread\n# or capture with Unreal Insights, then cut the worst scopes:\n# throttle Tick, reduce ticking actors & AI, trim Blueprint tick logic."),
			PerformanceDocs::StatCommands, TEXT("Stat Commands — UE 5.8 docs") });
	}
	else if (Bound == TEXT("RenderThread"))
	{
		Fixes.Add({ TEXT("Render-thread bound — scene setup cost"),
			TEXT("Too many draw calls / primitives, or many dynamic shadow-casting lights, saturate render-thread scene setup."),
			TEXT("stat scenerendering\n# Levers: merge/instance meshes, enable Nanite, cut dynamic lights & per-light shadows."),
			PerformanceDocs::Profiling, TEXT("Performance Profiling — UE 5.8 docs") });
	}
	else if (Bound == TEXT("RHIThread"))
	{
		Fixes.Add({ TEXT("RHI-thread bound — GPU command submission"),
			TEXT("The thread submitting GPU commands is the bottleneck, distinct from render-thread scene setup — usually too many draw calls / state changes."),
			TEXT("stat rhi\nstat scenerendering\n# Levers: cut draw calls via merging / instancing / HISM, enable Nanite, reduce material & state permutations."),
			PerformanceDocs::Profiling, TEXT("Performance Profiling — UE 5.8 docs") });
	}
	else if (Bound == TEXT("GPU"))
	{
		Fixes.Add({ TEXT("GPU bound — render cost"),
			TEXT("The GPU is the bottleneck. Confirm with the resolution test: r.ScreenPercentage 50 should noticeably raise FPS if truly GPU-bound."),
			TEXT("ProfileGPU\n# Levers: Virtual Shadow Maps, Lumen GI/reflections, translucency, post-process, ScreenPercentage."),
			PerformanceDocs::Insights, TEXT("Unreal Insights — UE 5.8 docs") });
	}
	if (bHasLogs && PsoHitches > 0)
	{
		Fixes.Add({ FString::Printf(TEXT("%d PSO hitch%s — pipeline-state cache misses"), PsoHitches, PsoHitches == 1 ? TEXT("") : TEXT("es")),
			TEXT("A PSO stalls the first time a shader/state combo is drawn. Gathering a bundled cache from a representative playthrough removes them."),
			TEXT("# Enable PSO logging in a build, play a representative pass, then bundle the cache:\nr.ShaderPipelineCache.Enabled 1\nr.PSOPrecache.Components 1"),
			PerformanceDocs::PsoCaches, TEXT("Optimizing Rendering with PSO Caches — UE 5.8 docs") });
	}

	// --- Compose the HTML -----------------------------------------------------------------------------
	auto ConfClass = [](const FString& C) -> const TCHAR*
	{ return C == TEXT("clear") ? TEXT("good") : (C == TEXT("none") ? TEXT("crit") : TEXT("warn")); };

	FString H;
	H.Reserve(16 * 1024);
	H += FString::Printf(TEXT("<title>%s</title>"), *PerfHtmlEscape(Title));
	H += PerfReportCss;
	H += TEXT("<div class=\"wrap\"><div class=\"sheet\">");
	H += FString::Printf(TEXT("<p class=\"eyebrow\">VibeUE Performance &middot; %s &middot; %s</p>"),
		*PerfHtmlEscape(FApp::GetProjectName()), bPIE ? TEXT("PIE frame") : TEXT("Editor frame"));
	H += FString::Printf(TEXT("<h1>%s</h1>"), *PerfHtmlEscape(Title));
	H += FString::Printf(TEXT("<p class=\"lede\">Frame verdict and, where a capture is available, the representative frame stats — with a data-driven fix order. Generated %s.</p>"),
		*PerfHtmlEscape(FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M"))));

	// Headline tiles: with a representative capture, frame them as the in-process-vs-representative contrast
	// (warm reading vs the cold worst frame + p95); without one, fall back to the single-run KPIs.
	H += TEXT("<div class=\"tiles\">");
	if (bHasTrace)
	{
		const double Ratio = FrameMs > 0.0 ? (P95Ms / FrameMs) : 0.0;
		H += FString::Printf(TEXT("<div class=\"tile warn\"><p class=\"k\">In-process (warm)</p><p class=\"v\">%.1f<small>ms</small></p><p class=\"s\">%.0f fps &middot; %s &middot; budget %s</p></div>"),
			FrameMs, Fps, *PerfHtmlEscape(Bound.IsEmpty() ? TEXT("—") : Bound), bMeets ? TEXT("PASS") : TEXT("FAIL"));
		H += FString::Printf(TEXT("<div class=\"tile crit\"><p class=\"k\">Worst frame (cold)</p><p class=\"v\">%s<small>ms</small></p><p class=\"s\">The frame-0 stall the warm reading hid.</p></div>"),
			*FString::FormatAsNumber((int64)MaxFrameMs));
		H += FString::Printf(TEXT("<div class=\"tile %s\"><p class=\"k\">Steady-state (p95)</p><p class=\"v\">%.0f<small>ms</small></p><p class=\"s\">~%.0f&times; the %.1f ms in-process frame.</p></div>"),
			P95Ms > BudgetMs ? TEXT("crit") : TEXT("good"), P95Ms, Ratio, FrameMs);
	}
	else
	{
		H += FString::Printf(TEXT("<div class=\"tile %s\"><p class=\"k\">Frame</p><p class=\"v\">%.1f<small>ms</small></p><p class=\"s\">%.0f fps live</p></div>"),
			FrameMs > BudgetMs ? TEXT("crit") : TEXT("good"), FrameMs, Fps);
		H += FString::Printf(TEXT("<div class=\"tile %s\"><p class=\"k\">Bound</p><p class=\"v\" style=\"font-size:22px\">%s</p><p class=\"s\">%s confidence</p></div>"),
			ConfClass(Conf), *PerfHtmlEscape(Bound.IsEmpty() ? TEXT("—") : Bound), *PerfHtmlEscape(Conf));
		H += FString::Printf(TEXT("<div class=\"tile %s\"><p class=\"k\">Budget @ %.0f fps</p><p class=\"v\" style=\"font-size:22px\">%s</p><p class=\"s\">%.2f ms/frame budget</p></div>"),
			bMeets ? TEXT("good") : TEXT("crit"), TargetFps, bMeets ? TEXT("PASS") : TEXT("FAIL"), BudgetMs);
	}
	H += TEXT("</div>");

	// The full in-process vs representative matrix — only when a capture supplies the cold column. Numbers
	// are live; the descriptive cells are the fixed teaching layer (the nature of warm-vs-cold doesn't change
	// run to run).
	if (bHasTrace)
	{
		const TCHAR* SteadyRec =
			  Bound == TEXT("GameThread")   ? TEXT("Frame is <b>game-thread bound</b> &rarr; drill <code>stat dumpframe -root=gamethread</code> (Tick / BP / AI / anim)")
			: Bound == TEXT("RenderThread") ? TEXT("Frame is <b>render-thread bound</b> &rarr; <code>stat scenerendering</code>; merge/instance meshes, cut dynamic lights")
			: Bound == TEXT("RHIThread")    ? TEXT("Frame is <b>RHI-thread bound</b> &rarr; <code>stat rhi</code>; cut draw calls via instancing / HISM / Nanite")
			: Bound == TEXT("GPU")          ? TEXT("Frame is <b>GPU bound</b> &rarr; <code>ProfileGPU</code>; shadows, Lumen, translucency, ScreenPercentage")
			:                                 TEXT("Profile the bottleneck thread with Unreal Insights");

		H += TEXT("<h2>In-process vs Representative</h2><div class=\"card table-scroll\"><table style=\"min-width:720px\">");
		H += TEXT("<thead><tr><th>Dimension</th><th>In-process — warm</th><th>Representative — cold</th><th class=\"rec\">Where to improve</th></tr></thead><tbody>");
		H += TEXT("<tr><td class=\"dim\">Process &amp; caches<small>environment</small></td><td class=\"warm\">Editor/PIE process, <b>warm</b> shader/PSO caches, on-demand cooked data</td><td class=\"cold\">Separate process, <b>cold</b> caches — like a shipping build</td><td class=\"rec-cell\">Trust the representative numbers for anything absolute; in-process is for logic checks</td></tr>");
		H += FString::Printf(TEXT("<tr><td class=\"dim\">Startup hitch<small>the fixture</small></td><td class=\"warm\"><span class=\"tag hidden\">invisible</span></td><td class=\"cold\"><b>%s ms</b> @ frame 0 <span class=\"tag caught\">caught</span></td><td class=\"rec-cell\"><b>Biggest lever.</b> Load / PSO / shader-comp bound at boot — ship a PSO precache, warm the shader pipeline, async-stream the first level</td></tr>"),
			*FString::FormatAsNumber((int64)MaxFrameMs));
		H += FString::Printf(TEXT("<tr><td class=\"dim\">PSO hitches<small>pipeline stalls</small></td><td class=\"warm\">Not surfaced</td><td class=\"cold\"><b>%d</b> from <code>-logPSO</code>%s</td><td class=\"rec-cell\">Bundle a gathered PSO cache with the build — folds into the startup fix</td></tr>"),
			PsoHitches, PsoHitches > 0 ? TEXT(" <span class=\"tag warn\">hitch</span>") : TEXT(""));
		H += FString::Printf(TEXT("<tr class=\"bound\"><td class=\"dim\">Steady-state frame<small>hot path</small></td><td class=\"warm\"><b>%.1f ms</b> &middot; %s &middot; %.0f fps</td><td class=\"cold\"><b>p95 %.0f ms</b> &middot; avg <b>%.1f fps</b> &middot; %d frames</td><td class=\"rec-cell\">%s</td></tr>"),
			FrameMs, *PerfHtmlEscape(Bound.IsEmpty() ? TEXT("—") : Bound), Fps, P95Ms, AvgFps, FrameCount, SteadyRec);
		H += FString::Printf(TEXT("<tr><td class=\"dim\">Render thread<small>attribution</small></td><td class=\"warm\">Reads <b>%.1f ms</b> in-process</td><td class=\"cold\">Captured per-frame in the trace</td><td class=\"rec-cell\">Use a representative trace for render / RHI attribution — in-process render time is not shipping-accurate</td></tr>"),
			RenderMs);
		H += TEXT("<tr><td class=\"dim\">Bound verdict<small>CPU vs GPU</small></td><td class=\"warm\">Live — <code>FrameTiming</code> / <code>ForceHitch</code> self-measure</td><td class=\"cold\">Read via trace + <code>Analyse</code></td><td class=\"rec-cell\">In-process for the fast verdict + hitch self-validation; representative for <i>where</i> the ms goes</td></tr>");
		H += TEXT("<tr><td class=\"dim\">Best use<small>role</small></td><td class=\"warm\">Quick sanity + verdict/hitch <b>logic</b> checks</td><td class=\"cold\"><b>Real stall identification</b> &amp; regression capture</td><td class=\"rec-cell\">Keep both — they answer different questions</td></tr>");
		H += TEXT("</tbody></table></div>");
	}

	// Per-thread table
	H += TEXT("<h2>Thread breakdown &middot; in-process frame</h2><div class=\"card table-scroll\"><table><thead><tr><th>Thread</th><th>ms</th><th>vs budget</th></tr></thead><tbody>");
	struct FRow { const TCHAR* Name; double Ms; bool bShow; };
	const FRow Rows[] = {
		{ TEXT("Game"),   GameMs,   true },
		{ TEXT("Render"), RenderMs, true },
		{ TEXT("RHI"),    RhiMs,    RhiMs > 0.0 },
		{ TEXT("GPU"),    GpuMs,    GpuMs > 0.0 },
	};
	for (const FRow& Rw : Rows)
	{
		if (!Rw.bShow) continue;
		const bool bOver = Rw.Ms > BudgetMs;
		const bool bIsBound = Bound.StartsWith(Rw.Name);
		H += FString::Printf(TEXT("<tr%s><td class=\"th-name\">%s%s</td><td class=\"num\">%.2f</td><td>%s</td></tr>"),
			bIsBound ? TEXT(" class=\"bound\"") : TEXT(""),
			Rw.Name, bIsBound ? TEXT(" ◀") : TEXT(""),
			Rw.Ms,
			bOver ? TEXT("<span class=\"pill crit\">over</span>") : TEXT("<span class=\"pill good\">ok</span>"));
	}
	H += TEXT("</tbody></table></div>");

	// Capture summary (trace worst frames + log health), when present
	if (bHasTrace)
	{
		H += FString::Printf(TEXT("<h2>Representative capture &middot; %.0fs &middot; avg %.1f fps</h2>"), DurationS, AvgFps);
		const TArray<TSharedPtr<FJsonValue>>* Worst = nullptr;
		if (Trace->TryGetArrayField(TEXT("worst_frames"), Worst) && Worst)
		{
			H += TEXT("<div class=\"card\"><ul class=\"worst\">");
			int32 Shown = 0;
			for (const TSharedPtr<FJsonValue>& V : *Worst)
			{
				const TSharedPtr<FJsonObject> WO = V->AsObject();
				if (!WO.IsValid()) continue;
				H += FString::Printf(TEXT("<li><span class=\"fm\">frame %d</span><span>%s ms</span></li>"),
					(int32)WO->GetNumberField(TEXT("frame")), *FString::FormatAsNumber((int64)WO->GetNumberField(TEXT("ms"))));
				if (++Shown >= 6) break;
			}
			H += TEXT("</ul></div>");
		}
		if (bHasLogs)
		{
			H += FString::Printf(TEXT("<p class=\"lede\" style=\"margin-top:14px\">Log: %s errors, %s warnings, <b style=\"color:var(--ink)\">%d PSO hitch%s</b>.</p>"),
				*FString::FormatAsNumber(LogErrors), *FString::FormatAsNumber(LogWarnings), PsoHitches, PsoHitches == 1 ? TEXT("") : TEXT("es"));
		}
	}
	else
	{
		H += TEXT("<p class=\"lede\" style=\"margin-top:8px\">No capture analysed — run <b>StartStandalone</b> (or StartTrace → workload → StopTrace), then Report again for frame-level worst-case detail.</p>");
	}

	// Fix list
	if (Fixes.Num() > 0)
	{
		H += TEXT("<h2>Fix in this order</h2><div class=\"fixes\">");
		for (const FPerfFix& Fx : Fixes)
		{
			H += TEXT("<div class=\"fix\"><div>");
			H += FString::Printf(TEXT("<p class=\"ft\">%s</p><p class=\"fw\">%s</p>"), *PerfHtmlEscape(Fx.Title), *PerfHtmlEscape(Fx.Why));
			H += FString::Printf(TEXT("<p class=\"how-l\">How to fix</p><pre>%s</pre>"), *PerfHtmlEscape(Fx.How));
			H += FString::Printf(TEXT("<a class=\"doc\" href=\"%s\">%s &rarr;</a>"), *Fx.DocUrl, *PerfHtmlEscape(Fx.DocLabel));
			H += TEXT("</div></div>");
		}
		H += TEXT("</div>");
		H += TEXT("<div class=\"guide\"><b>Want a walkthrough?</b> Ask your assistant to guide you through any fix above — it can run the commands, read the results back, and validate the change with ForceHitch.</div>");
	}

	H += FString::Printf(TEXT("<footer><span><b>Project:</b> %s</span>"), *PerfHtmlEscape(FApp::GetProjectName()));
	if (bHasTrace) { H += FString::Printf(TEXT("<span><b>Capture:</b> %d frames / %.0fs</span>"), FrameCount, DurationS); }
	H += FString::Printf(TEXT("<span><b>Generated:</b> %s</span><span><b>Tool:</b> VibeUE Performance — FrameTiming + Analyse</span></footer>"),
		*PerfHtmlEscape(FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"))));
	H += TEXT("</div></div>");

	// --- Write the self-contained file ----------------------------------------------------------------
	const FString Dir = ProjectSavedDirAbs() / TEXT("VibeUE") / TEXT("Performance");
	IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*Dir);
	const FString Path = Dir / FString::Printf(TEXT("report_%s.html"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	if (!FFileHelper::SaveStringToFile(H, *Path, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		return ErrJson(TEXT("REPORT_WRITE_FAILED"), FString::Printf(TEXT("Could not write report to %s"), *Path));
	}

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("report_file"), Path);
	R->SetNumberField(TEXT("fix_count"), Fixes.Num());
	R->SetBoolField(TEXT("included_capture"), bHasTrace);
	R->SetStringField(TEXT("bound"), Bound);
	R->SetStringField(TEXT("hint"), FString::Printf(
		TEXT("Self-contained HTML written. Open it in any browser or screenshot it to share: %s"), *Path));
	return OkJson(R);
}

FString UPerformanceService::StartTrace(const FString& Name, const FString& Channels)
{
	const FString TraceName = Name.IsEmpty() ? TEXT("mcp_capture") : Name;
	const FString ChannelSet = Channels.IsEmpty() ? FString(DefaultTraceChannels()) : Channels;
	FString TracePath = BuildTraceFilePath(TraceName);
	GLastTraceFilePath = TracePath + TEXT(".utrace");
	GLastLogFilePath   = ProjectSavedDirAbs() / TEXT("Logs") / (FApp::GetProjectName() + FString(TEXT(".log")));

	if (FTraceAuxiliary::IsConnected())
	{
		return ErrJson(TEXT("ALREADY_TRACING"), TEXT("A trace is already active. Call StopTrace first."));
	}

	bool bOk = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, *TracePath, *ChannelSet);
	if (!bOk)
	{
		return ErrJson(TEXT("TRACE_FAILED"), FString::Printf(TEXT("Failed to start trace. Path: %s"), *TracePath));
	}

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("status"),     TEXT("tracing"));
	R->SetStringField(TEXT("trace_file"), GLastTraceFilePath);
	R->SetStringField(TEXT("channels"),   ChannelSet);
	R->SetStringField(TEXT("hint"),       TEXT("Call StopTrace when done, then Analyse to read results."));
	return OkJson(R);
}

FString UPerformanceService::StopTrace()
{
	if (!FTraceAuxiliary::IsConnected())
	{
		return ErrJson(TEXT("NOT_TRACING"), TEXT("No trace is active."));
	}
	FTraceAuxiliary::Stop();

	FString UTSTrace = FindLatestUTSTrace();
	if (!UTSTrace.IsEmpty())
	{
		FDateTime StoredTime = GLastTraceFilePath.IsEmpty() ? FDateTime::MinValue()
			: IFileManager::Get().GetTimeStamp(*GLastTraceFilePath);
		FDateTime UTSTime = IFileManager::Get().GetTimeStamp(*UTSTrace);
		if (UTSTime > StoredTime) GLastTraceFilePath = UTSTrace;
	}

	int64 FileSizeBytes = GLastTraceFilePath.IsEmpty() ? 0
		: IFileManager::Get().FileSize(*GLastTraceFilePath);

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("status"),       TEXT("stopped"));
	R->SetStringField(TEXT("trace_file"),   GLastTraceFilePath);
	R->SetNumberField(TEXT("file_size_mb"), FMath::RoundToFloat((float)FileSizeBytes / (1024.f * 1024.f) * 10.f) / 10.f);
	R->SetStringField(TEXT("hint"),         TEXT("Call Analyse to read the results."));
	return OkJson(R);
}

FString UPerformanceService::GetTraceStatus()
{
	bool bConnected = FTraceAuxiliary::IsConnected();
	FString Dest    = FTraceAuxiliary::GetTraceDestinationString();
	TStringBuilder<512> ChannelSB;
	FTraceAuxiliary::GetActiveChannelsString(ChannelSB);

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("tracing"),          bConnected);
	R->SetStringField(TEXT("destination"),    Dest);
	R->SetStringField(TEXT("active_channels"),ChannelSB.ToString());
	R->SetStringField(TEXT("last_trace_file"),GLastTraceFilePath);
	return OkJson(R);
}

FString UPerformanceService::Bookmark(const FString& Name)
{
	if (Name.IsEmpty()) return ErrJson(TEXT("MISSING_NAME"), TEXT("'Name' parameter required."));
	TRACE_BOOKMARK(TEXT("%s"), *Name);
	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("bookmark"), Name);
	R->SetStringField(TEXT("status"), TEXT("ok"));
	return OkJson(R);
}

FString UPerformanceService::RegionStart(const FString& Name)
{
	if (Name.IsEmpty()) return ErrJson(TEXT("MISSING_NAME"), TEXT("'Name' parameter required."));
	TRACE_BEGIN_REGION(*Name);
	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("region"), Name);
	R->SetStringField(TEXT("status"), TEXT("started"));
	return OkJson(R);
}

FString UPerformanceService::RegionEnd(const FString& Name)
{
	if (Name.IsEmpty()) return ErrJson(TEXT("MISSING_NAME"), TEXT("'Name' parameter required."));
	TRACE_END_REGION(*Name);
	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("region"), Name);
	R->SetStringField(TEXT("status"), TEXT("ended"));
	return OkJson(R);
}

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
		return AnalyseTrace(TraceFile);
	}
	if (Src == TEXT("logs"))
	{
		return AnalyseLogs(LogFile);
	}

	// both
	if (TraceFile.IsEmpty()) return AnalyseLogs(LogFile);
	return AnalyseBoth(TraceFile, LogFile);
}

FString UPerformanceService::StartStandalone(const FString& Name, const FString& Channels)
{
	if (!GEditor) return ErrJson(TEXT("NO_EDITOR"), TEXT("GEditor not available."));
	if (GStandaloneRunning) return ErrJson(TEXT("ALREADY_RUNNING"), TEXT("Standalone is already running. Call StopStandalone first."));

	const FString TraceName = Name.IsEmpty() ? TEXT("standalone_capture") : Name;
	const FString ChannelSet = Channels.IsEmpty() ? FString(DefaultTraceChannels()) : Channels;
	FString TracePath = BuildTraceFilePath(TraceName);
	GLastTraceFilePath = TracePath + TEXT(".utrace");

	// Give the standalone process its own timestamped log via -abslog, rather than sharing the
	// project's default <Project>.log. That file is held open by the running editor, so LoadFileToString
	// fails on it (surfacing as a misleading LOG_NOT_FOUND) and a "newest log in the folder" guess picks
	// the editor's live log instead of the standalone's. A dedicated, unique path is unlocked and exact.
	FString LogDir = ProjectSavedDirAbs() / TEXT("Logs");
	IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*LogDir);
	FString StandaloneLogPath = LogDir / FString::Printf(TEXT("%s_%s.log"),
		*TraceName, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
	GLastLogFilePath = StandaloneLogPath;

	FString ExtraArgs = FString::Printf(
		TEXT("-tracehost=127.0.0.1 -trace=%s -tracefile=\"%s\" -abslog=\"%s\""),
		*ChannelSet, *TracePath, *StandaloneLogPath);

	FRequestPlaySessionParams P;
	P.SessionDestination = EPlaySessionDestinationType::NewProcess;
	P.WorldType = EPlaySessionWorldType::PlayInEditor;
	P.AdditionalStandaloneCommandLineParameters = ExtraArgs;
	GEditor->RequestPlaySession(P);
	GStandaloneRunning = true;

	GStandalonePlayDelegateHandle = FEditorDelegates::BeginStandaloneLocalPlay.AddLambda([](uint32 PID)
	{
		GStandaloneProcess = FPlatformProcess::OpenProcess(PID);
		FEditorDelegates::BeginStandaloneLocalPlay.Remove(GStandalonePlayDelegateHandle);
		GStandalonePlayDelegateHandle.Reset();
	});

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("status"), TEXT("standalone start requested"));
	R->SetStringField(TEXT("trace_file"), GLastTraceFilePath);
	R->SetStringField(TEXT("log_file"),   GLastLogFilePath);
	R->SetStringField(TEXT("channels"),   ChannelSet);
	R->SetStringField(TEXT("hint"), TEXT("Call StopStandalone when done, then Analyse to read results."));
	return OkJson(R);
}

FString UPerformanceService::StopStandalone()
{
	if (!GStandaloneRunning) return ErrJson(TEXT("NOT_RUNNING"), TEXT("No standalone session tracked. Did you call StartStandalone?"));
	if (GEditor) GEditor->RequestEndPlayMap();

	if (GStandalonePlayDelegateHandle.IsValid())
	{
		FEditorDelegates::BeginStandaloneLocalPlay.Remove(GStandalonePlayDelegateHandle);
		GStandalonePlayDelegateHandle.Reset();
	}

	if (GStandaloneProcess.IsValid())
	{
		double StartTime = FPlatformTime::Seconds();
		while (FPlatformProcess::IsProcRunning(GStandaloneProcess)
			   && (FPlatformTime::Seconds() - StartTime) < 5.0)
		{
			FPlatformProcess::Sleep(0.2f);
		}
		if (FPlatformProcess::IsProcRunning(GStandaloneProcess))
		{
			FPlatformProcess::TerminateProc(GStandaloneProcess);
		}
		FPlatformProcess::CloseProc(GStandaloneProcess);
	}

	GStandaloneRunning = false;

	FString UTSTrace = FindLatestUTSTrace();
	if (!UTSTrace.IsEmpty()) GLastTraceFilePath = UTSTrace;

	// GLastLogFilePath was set to a dedicated timestamped file in StartStandalone (-abslog), so it is
	// already the standalone's own log — no need to guess the newest log in the folder (which would
	// pick the editor's live, locked log).

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("status"),     TEXT("standalone stop requested"));
	R->SetStringField(TEXT("trace_file"), GLastTraceFilePath);
	R->SetStringField(TEXT("log_file"),   GLastLogFilePath);
	R->SetStringField(TEXT("uts_store"),  GetUTSStoreDir());
	R->SetStringField(TEXT("hint"),       TEXT("Allow a few seconds for the trace file to finalise, then call Analyse."));
	return OkJson(R);
}

FString UPerformanceService::GetStandaloneStatus()
{
	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetBoolField(TEXT("running"), GStandaloneRunning);
	R->SetStringField(TEXT("last_trace_file"), GLastTraceFilePath);
	R->SetStringField(TEXT("last_log_file"),   GLastLogFilePath);
	return OkJson(R);
}

FString UPerformanceService::StartPIE()
{
	if (!GEditor) return ErrJson(TEXT("NO_EDITOR"), TEXT("GEditor not available."));
	if (IsPIERunning()) return ErrJson(TEXT("ALREADY_RUNNING"), TEXT("PIE is already running. Call StopPIE first."));

	FRequestPlaySessionParams P;
	P.SessionDestination = EPlaySessionDestinationType::InProcess;   // in-process, unlike StartStandalone's NewProcess
	P.WorldType          = EPlaySessionWorldType::PlayInEditor;
	GEditor->RequestPlaySession(P);

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("status"), TEXT("PIE start requested"));
	R->SetStringField(TEXT("note"),   TEXT("PIE starts on the next editor tick — call FrameTiming on a FOLLOWING frame (pie_running flips true). Because PIE is IN-PROCESS, ForceHitch game/render stalls now land and FrameTiming reads the live game world."));
	R->SetStringField(TEXT("caveat"), TEXT("PIE is NOT representative for real stall identification — it reuses the editor's warm shader/PSO caches and on-demand cooked data, hiding costs a standalone/shipping build pays. For trustworthy stall numbers use StartStandalone."));
	R->SetStringField(TEXT("hint"),   TEXT("StartTrace before your workload if you want a capture, then StopPIE and Analyse."));
	return OkJson(R);
}

FString UPerformanceService::StopPIE()
{
	if (!GEditor) return ErrJson(TEXT("NO_EDITOR"), TEXT("GEditor not available."));
	if (!IsPIERunning()) return ErrJson(TEXT("NOT_RUNNING"), TEXT("PIE is not running."));
	GEditor->RequestEndPlayMap();

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetStringField(TEXT("status"), TEXT("PIE end requested"));
	R->SetStringField(TEXT("hint"),   TEXT("PIE tears down on the next tick. If you were tracing, StopTrace then Analyse."));
	return OkJson(R);
}
