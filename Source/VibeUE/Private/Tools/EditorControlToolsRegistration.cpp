// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Core/ToolRegistry.h"
#include "Json.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "PlayInEditorDataTypes.h"
#include "Editor.h"
#include "HAL/PlatformProcess.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/Frames.h"
#include "RenderTimer.h"       // GGameThreadTime / GRenderThreadTime / GRHIThreadTime (RenderCore)
#include "RHIGlobals.h"        // GGPUFrameTime (RHI)
#include "HAL/PlatformTime.h"  // FPlatformTime::ToMilliseconds
#include "PythonAPI/UScreenshotService.h" // synchronous editor/active-window capture for the screenshot action

DEFINE_LOG_CATEGORY_STATIC(LogEditorControl, Log, All);

// ---------------------------------------------------------------------------
// Session state — persists across MCP requests within an editor session
// ---------------------------------------------------------------------------

static FString GLastTraceFilePath;
static FString GLastLogFilePath;
static bool           GStandaloneRunning = false;
static FProcHandle    GStandaloneProcess;
static FDelegateHandle GStandalonePlayDelegateHandle;

// ---------------------------------------------------------------------------
// Param helpers
// ---------------------------------------------------------------------------

static FString GetParam(const TMap<FString, FString>& Params, const FString& Key, const FString& Default = TEXT(""))
{
	// Direct key
	if (const FString* V = Params.Find(Key)) return *V;
	// Capitalized variant (MCPServer capitalizes "action" → "Action")
	FString Cap = Key;
	if (Cap.Len() > 0) Cap[0] = FChar::ToUpper(Cap[0]);
	if (const FString* V = Params.Find(Cap)) return *V;
	// Remaining params are packed into ParamsJson by MCPServer when an action key is present
	const FString* JsonStr = Params.Find(TEXT("ParamsJson"));
	if (JsonStr)
	{
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(*JsonStr);
		if (FJsonSerializer::Deserialize(R, Obj) && Obj.IsValid())
		{
			FString Val;
			if (Obj->TryGetStringField(Key, Val)) return Val;
		}
	}
	return Default;
}

static bool GetBoolParam(const TMap<FString, FString>& Params, const FString& Key, bool Default = false)
{
	FString V = GetParam(Params, Key);
	if (V.IsEmpty()) return Default;
	return V.Equals(TEXT("true"), ESearchCase::IgnoreCase) || V == TEXT("1");
}

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

// ---------------------------------------------------------------------------
// PIE helpers
// ---------------------------------------------------------------------------

static bool IsPIERunning()
{
	return GEditor && GEditor->PlayWorld != nullptr;
}

// ---------------------------------------------------------------------------
// Default channel set
// ---------------------------------------------------------------------------

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

// Read StoreDir from the Unreal Trace Server settings file.
// UTS saves traces there when -tracehost is used instead of -tracefile.
static FString GetUTSStoreDir()
{
	FString SettingsPath = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"))
		/ TEXT("UnrealEngine/Common/UnrealTrace/Settings.ini");

	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *SettingsPath))
	{
		return FString();
	}

	for (const FString& Line : TArray<FString>([&]{ TArray<FString> L; Content.ParseIntoArrayLines(L); return L; }()))
	{
		if (Line.StartsWith(TEXT("StoreDir=")))
		{
			FString Dir = Line.Mid(9).TrimStartAndEnd();
			// UTS uses backslashes — normalise
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

	UE_LOG(LogEditorControl, Log, TEXT("Analysing trace: %s"), *TraceFile);
	TSharedPtr<const TraceServices::IAnalysisSession> Session = AnalysisService->Analyze(*TraceFile);
	if (!Session)
	{
		return ErrJson(TEXT("ANALYSIS_FAILED"), TEXT("Failed to analyse trace file."));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("trace_file"), TraceFile);

	// --- Frame analysis (all session reads must be inside the ReadScope) ---
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
						// Skip frames with no valid end time (trace cut mid-frame)
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

				// p95
				AllMs.Sort();
				int32 P95Idx = FMath::Clamp((int32)(AllMs.Num() * 0.95), 0, AllMs.Num() - 1);
				Root->SetNumberField(TEXT("p95_frame_ms"), FMath::RoundToFloat(AllMs[P95Idx] * 100.0f) / 100.0f);
				} // end AllMs non-empty

				// Worst 10 frames
				TArray<TSharedPtr<FJsonValue>> WorstArr;
				// Re-enumerate to find top 10 — build sorted list of (ms, index, time)
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

// ---------------------------------------------------------------------------
// Combined analyse — merges trace + log into one report
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Frame timing / bound check
// ---------------------------------------------------------------------------
// Reports the Game / Render / GPU / RHI thread times for the most recently
// rendered frame — the same data the on-screen "stat unit" overlay shows, read
// from engine globals. The point of this action is to answer the FIRST question
// of any frame-rate investigation: are we CPU-bound or GPU-bound? Optimising GPU
// passes (shadows, Lumen, etc.) does nothing if the frame is actually game- or
// render-thread bound. Call this BEFORE running ProfileGPU.
//
// Values reflect the PIE game viewport while PIE runs, otherwise the editor
// viewport. GPU time is 0 when GPU timing is unavailable for the frame.
static FString FrameTimingReport()
{
	const double GameMs   = FPlatformTime::ToMilliseconds(GGameThreadTime);
	const double RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
	const double RHIMs    = FPlatformTime::ToMilliseconds(GRHIThreadTime);
	// GGPUFrameTime was deprecated in UE 5.6; RHIGetGPUFrameCycles() returns the
	// same per-frame GPU cycle count (0 when GPU timing is unavailable).
	const double GpuMs    = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());

	auto Round2 = [](double V) -> double { return FMath::RoundToDouble(V * 100.0) / 100.0; };

	TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
	R->SetNumberField(TEXT("game_thread_ms"),   Round2(GameMs));
	R->SetNumberField(TEXT("render_thread_ms"), Round2(RenderMs));
	R->SetNumberField(TEXT("rhi_thread_ms"),    Round2(RHIMs));
	R->SetNumberField(TEXT("gpu_ms"),           Round2(GpuMs));

	// The bottleneck is whichever pipeline stage is longest. The RHI thread is
	// normally subsumed by the render thread, so compare Game vs Render vs GPU.
	const double FrameMs = FMath::Max3(GameMs, RenderMs, GpuMs);
	R->SetNumberField(TEXT("frame_ms"), Round2(FrameMs));
	R->SetNumberField(TEXT("fps"), FrameMs > 0.0 ? Round2(1000.0 / FrameMs) : 0.0);

	FString Bound;
	FString Hint;
	if (GpuMs >= GameMs && GpuMs >= RenderMs && GpuMs > 0.0)
	{
		Bound = TEXT("GPU");
		Hint  = TEXT("GPU-bound. NOW it is worth profiling the GPU: profiler_start (channels frame,gpu,cpu), then 'r.ProfileGPU.ShowUI 0' + 'ProfileGPU' and read the pass breakdown from the log. Levers: shadows (Virtual Shadow Maps), Lumen GI/reflections, translucency, post-process, ScreenPercentage. Confirm with the resolution test: 'r.ScreenPercentage 50' should noticeably raise FPS if truly GPU-bound.");
	}
	else if (RenderMs >= GameMs)
	{
		Bound = TEXT("RenderThread");
		Hint  = TEXT("CPU render-thread bound. Usual cause: too many draw calls / primitives, or many dynamic shadow-casting lights. Check 'stat scenerendering' (Mesh draw calls, visible primitives). Levers: merge/instance meshes, enable Nanite, cut dynamic lights and per-light shadows, reduce visible primitive count. NOTE: dropping r.ScreenPercentage will NOT help a render-thread-bound frame.");
	}
	else
	{
		Bound = TEXT("GameThread");
		Hint  = TEXT("CPU game-thread bound. Usual cause: Tick / Blueprint / AI / animation cost. Run 'stat dumpframe -ms=0.5 -root=gamethread' (execute_console_command on the PIE world) then read_logs the result to see the hottest tick functions. Levers: disable or interval-throttle unnecessary Tick, reduce ticking actors & AI, cut expensive Blueprint tick logic, check animation update cost. NOTE: dropping r.ScreenPercentage will NOT help a game-thread-bound frame.");
	}
	R->SetStringField(TEXT("bound"), Bound);
	R->SetStringField(TEXT("hint"), Hint);

	const bool bPIE = IsPIERunning();
	R->SetBoolField(TEXT("pie_running"), bPIE);
	R->SetStringField(TEXT("note"),
		bPIE
		? TEXT("Values are for the most recently rendered PIE frame. Park in a representative/worst spot for a clean read.")
		: TEXT("PIE is NOT running — these values reflect the EDITOR viewport, not your game. Start PIE for a real game-bound reading."));
	if (GpuMs <= 0.0)
	{
		R->SetStringField(TEXT("gpu_note"), TEXT("gpu_ms is 0 (GPU timing unavailable this frame) — rely on the game vs render comparison, and confirm GPU-bound with the r.ScreenPercentage 50 test."));
	}
	return OkJson(R);
}

// ---------------------------------------------------------------------------
// Tool registration
// ---------------------------------------------------------------------------

REGISTER_VIBEUE_TOOL(editor_control,
	"Control the Unreal Editor, capture viewport screenshots, and profile. Actions: start_pie, stop_pie, pie_status, "
	"screenshot (synchronous PNG of the editor/active window so you can SEE what was built and self-review — params: mode=editor_window|active_window, name), "
	"start_standalone (launches game as separate process with trace attached), stop_standalone, standalone_status, "
	"profiler_start (begin Unreal Insights trace to file), profiler_stop, profiler_status, "
	"profiler_bookmark (drop named point in trace), profiler_region_start, profiler_region_end, "
	"frame_timing (report Game/Render/GPU thread split + CPU-vs-GPU bound verdict — RUN THIS FIRST), "
	"analyse (read trace + logs and return perf summary — source: trace|logs|both), help.",
	"Editor",
	TOOL_PARAMS(
		TOOL_PARAM("action",   "Action to perform — see tool description for full list", "string", true),
		TOOL_PARAM("name",     "[profiler_start|bookmark|region_*|screenshot] Trace file name (profiler_start), label (bookmark/region), or output PNG name (screenshot)", "string", false),
		TOOL_PARAM("channels", "[profiler_start|start_standalone] Comma-separated trace channels. Default: frame,cpu,gpu,log,loadtime,object,stats", "string", false),
		TOOL_PARAM("source",   "[analyse] What to read: trace, logs, or both (default: both)", "string", false),
		TOOL_PARAM("file",     "[analyse] Override trace or log file path", "string", false),
		TOOL_PARAM("mode",     "[screenshot] editor_window (default, whole editor incl. focused tab) or active_window (foreground window, e.g. a separate PIE window)", "string", false)
	),
	{
		FString Action = GetParam(Params, TEXT("action")).ToLower();

		// -----------------------------------------------------------------------
		// PIE
		// -----------------------------------------------------------------------
		if (Action == TEXT("start_pie"))
		{
			if (!GEditor) return ErrJson(TEXT("NO_EDITOR"), TEXT("GEditor not available."));
			if (IsPIERunning()) return ErrJson(TEXT("ALREADY_RUNNING"), TEXT("PIE is already running. Call stop_pie first."));

			FRequestPlaySessionParams P;
			P.SessionDestination = EPlaySessionDestinationType::InProcess;
			P.WorldType = EPlaySessionWorldType::PlayInEditor;
			GEditor->RequestPlaySession(P);

			// Record the log file path for this session
			GLastLogFilePath = ProjectSavedDirAbs() / TEXT("Logs") / (FApp::GetProjectName() + FString(TEXT(".log")));

			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetStringField(TEXT("status"), TEXT("PIE start requested"));
			R->SetStringField(TEXT("log_file"), GLastLogFilePath);
			return OkJson(R);
		}

		if (Action == TEXT("stop_pie"))
		{
			if (!GEditor) return ErrJson(TEXT("NO_EDITOR"), TEXT("GEditor not available."));
			if (!IsPIERunning()) return ErrJson(TEXT("NOT_RUNNING"), TEXT("PIE is not running."));
			GEditor->RequestEndPlayMap();
			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetStringField(TEXT("status"), TEXT("PIE stop requested"));
			return OkJson(R);
		}

		if (Action == TEXT("pie_status"))
		{
			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetBoolField(TEXT("running"), IsPIERunning());
			R->SetStringField(TEXT("status"), IsPIERunning() ? TEXT("running") : TEXT("stopped"));
			return OkJson(R);
		}

		// -----------------------------------------------------------------------
		// Standalone
		// -----------------------------------------------------------------------
		if (Action == TEXT("start_standalone"))
		{
			if (!GEditor) return ErrJson(TEXT("NO_EDITOR"), TEXT("GEditor not available."));
			if (GStandaloneRunning) return ErrJson(TEXT("ALREADY_RUNNING"), TEXT("Standalone is already running. Call stop_standalone first."));

			FString TraceName = GetParam(Params, TEXT("name"), TEXT("standalone_capture"));
			FString Channels  = GetParam(Params, TEXT("channels"), DefaultTraceChannels());
			FString TracePath = BuildTraceFilePath(TraceName);
			GLastTraceFilePath = TracePath + TEXT(".utrace");
			GLastLogFilePath   = ProjectSavedDirAbs() / TEXT("Logs") / (FApp::GetProjectName() + FString(TEXT(".log")));

			// Standalone receives trace channels via -trace= and connects back to the
			// editor's Unreal Trace Server on localhost:1985
			FString ExtraArgs = FString::Printf(
				TEXT("-tracehost=127.0.0.1 -trace=%s -tracefile=\"%s\""),
				*Channels, *TracePath);

			FRequestPlaySessionParams P;
			P.SessionDestination = EPlaySessionDestinationType::NewProcess;
			P.WorldType = EPlaySessionWorldType::PlayInEditor;
			P.AdditionalStandaloneCommandLineParameters = ExtraArgs;
			GEditor->RequestPlaySession(P);
			GStandaloneRunning = true;

			// Capture the child PID when the process starts so stop_standalone can
			// terminate it without enumerating all system processes.
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
			R->SetStringField(TEXT("channels"),   Channels);
			R->SetStringField(TEXT("hint"), TEXT("Call stop_standalone when done, then analyse to read results."));
			return OkJson(R);
		}

		if (Action == TEXT("stop_standalone"))
		{
			if (!GStandaloneRunning) return ErrJson(TEXT("NOT_RUNNING"), TEXT("No standalone session tracked. Did you call start_standalone?"));
			if (GEditor) GEditor->RequestEndPlayMap();

			// Remove the PID-capture delegate if the process never finished launching.
			if (GStandalonePlayDelegateHandle.IsValid())
			{
				FEditorDelegates::BeginStandaloneLocalPlay.Remove(GStandalonePlayDelegateHandle);
				GStandalonePlayDelegateHandle.Reset();
			}

			// RequestEndPlayMap does not terminate a standalone (NewProcess) game.
			// Use the handle captured from BeginStandaloneLocalPlay to wait for a clean
			// exit (up to 5 s) then force-terminate if needed.
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

			// Prefer the UTS store — standalone uses -tracehost so the file lands there
			FString UTSTrace = FindLatestUTSTrace();
			if (!UTSTrace.IsEmpty()) GLastTraceFilePath = UTSTrace;

			// Standalone writes its own log (e.g. PROTEUS_2.log) — find the newest one
			{
				FString LogDir = ProjectSavedDirAbs() / TEXT("Logs");
				FString NewestLog;
				FDateTime NewestTime = FDateTime::MinValue();
				IFileManager::Get().IterateDirectory(*LogDir, [&](const TCHAR* Path, bool bDir) -> bool
				{
					if (!bDir && FPaths::GetExtension(Path).Equals(TEXT("log"), ESearchCase::IgnoreCase))
					{
						FString Fname = FPaths::GetCleanFilename(Path);
						if (!Fname.Contains(TEXT("backup")) && !Fname.Contains(TEXT("cef")))
						{
							FDateTime T = IFileManager::Get().GetTimeStamp(Path);
							if (T > NewestTime) { NewestTime = T; NewestLog = Path; }
						}
					}
					return true;
				});
				if (!NewestLog.IsEmpty()) GLastLogFilePath = NewestLog;
			}

			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetStringField(TEXT("status"),     TEXT("standalone stop requested"));
			R->SetStringField(TEXT("trace_file"), GLastTraceFilePath);
			R->SetStringField(TEXT("log_file"),   GLastLogFilePath);
			R->SetStringField(TEXT("uts_store"),  GetUTSStoreDir());
			R->SetStringField(TEXT("hint"),       TEXT("Allow a few seconds for the trace file to finalise, then call analyse."));
			return OkJson(R);
		}

		if (Action == TEXT("standalone_status"))
		{
			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetBoolField(TEXT("running"), GStandaloneRunning);
			R->SetStringField(TEXT("last_trace_file"), GLastTraceFilePath);
			R->SetStringField(TEXT("last_log_file"),   GLastLogFilePath);
			return OkJson(R);
		}

		// -----------------------------------------------------------------------
		// Profiler
		// -----------------------------------------------------------------------
		if (Action == TEXT("profiler_start"))
		{
			FString TraceName = GetParam(Params, TEXT("name"), TEXT("mcp_capture"));
			FString Channels  = GetParam(Params, TEXT("channels"), DefaultTraceChannels());
			FString TracePath = BuildTraceFilePath(TraceName);
			GLastTraceFilePath = TracePath + TEXT(".utrace");
			GLastLogFilePath   = ProjectSavedDirAbs() / TEXT("Logs") / (FApp::GetProjectName() + FString(TEXT(".log")));

			if (FTraceAuxiliary::IsConnected())
			{
				return ErrJson(TEXT("ALREADY_TRACING"), TEXT("A trace is already active. Call profiler_stop first."));
			}

			bool bOk = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, *TracePath, *Channels);
			if (!bOk)
			{
				return ErrJson(TEXT("TRACE_FAILED"), FString::Printf(TEXT("Failed to start trace. Path: %s"), *TracePath));
			}

			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetStringField(TEXT("status"),     TEXT("tracing"));
			R->SetStringField(TEXT("trace_file"), GLastTraceFilePath);
			R->SetStringField(TEXT("channels"),   Channels);
			R->SetStringField(TEXT("hint"),       TEXT("Call profiler_stop when done, then analyse to read results."));
			return OkJson(R);
		}

		if (Action == TEXT("profiler_stop"))
		{
			if (!FTraceAuxiliary::IsConnected())
			{
				return ErrJson(TEXT("NOT_TRACING"), TEXT("No trace is active."));
			}
			FTraceAuxiliary::Stop();

			// If UTS has a newer file than our stored path, prefer it
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
			R->SetStringField(TEXT("hint"),         TEXT("Call analyse to read the results."));
			return OkJson(R);
		}

		if (Action == TEXT("profiler_status"))
		{
			bool bConnected = FTraceAuxiliary::IsConnected();
			FString Dest    = FTraceAuxiliary::GetTraceDestinationString();
			FStringBuilderBase ChannelSB;
			FTraceAuxiliary::GetActiveChannelsString(ChannelSB);

			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetBoolField(TEXT("tracing"),          bConnected);
			R->SetStringField(TEXT("destination"),    Dest);
			R->SetStringField(TEXT("active_channels"),ChannelSB.ToString());
			R->SetStringField(TEXT("last_trace_file"),GLastTraceFilePath);
			return OkJson(R);
		}

		// -----------------------------------------------------------------------
		// Annotations
		// -----------------------------------------------------------------------
		if (Action == TEXT("profiler_bookmark"))
		{
			FString Name = GetParam(Params, TEXT("name"));
			if (Name.IsEmpty()) return ErrJson(TEXT("MISSING_NAME"), TEXT("'name' parameter required."));
			TRACE_BOOKMARK(TEXT("%s"), *Name);
			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetStringField(TEXT("bookmark"), Name);
			R->SetStringField(TEXT("status"), TEXT("ok"));
			return OkJson(R);
		}

		if (Action == TEXT("profiler_region_start"))
		{
			FString Name = GetParam(Params, TEXT("name"));
			if (Name.IsEmpty()) return ErrJson(TEXT("MISSING_NAME"), TEXT("'name' parameter required."));
			TRACE_BEGIN_REGION(*Name);
			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetStringField(TEXT("region"), Name);
			R->SetStringField(TEXT("status"), TEXT("started"));
			return OkJson(R);
		}

		if (Action == TEXT("profiler_region_end"))
		{
			FString Name = GetParam(Params, TEXT("name"));
			if (Name.IsEmpty()) return ErrJson(TEXT("MISSING_NAME"), TEXT("'name' parameter required."));
			TRACE_END_REGION(*Name);
			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetStringField(TEXT("region"), Name);
			R->SetStringField(TEXT("status"), TEXT("ended"));
			return OkJson(R);
		}

		// -----------------------------------------------------------------------
		// Analyse
		// -----------------------------------------------------------------------
		if (Action == TEXT("analyse") || Action == TEXT("analyze"))
		{
			FString Source    = GetParam(Params, TEXT("source"), TEXT("both")).ToLower();
			FString FileParam = GetParam(Params, TEXT("file"));

			FString TraceFile = FileParam.IsEmpty() ? GLastTraceFilePath : FileParam;
			FString LogFile   = GLastLogFilePath;

			if (LogFile.IsEmpty())
			{
				LogFile = ProjectSavedDirAbs() / TEXT("Logs") / (FApp::GetProjectName() + FString(TEXT(".log")));
			}

			if (Source == TEXT("trace"))
			{
				if (TraceFile.IsEmpty()) return ErrJson(TEXT("NO_TRACE"), TEXT("No trace file known. Run profiler_start first, or pass file=<path>."));
				return AnalyseTrace(TraceFile);
			}
			if (Source == TEXT("logs"))
			{
				return AnalyseLogs(LogFile);
			}

			// both
			if (TraceFile.IsEmpty()) return AnalyseLogs(LogFile);
			return AnalyseBoth(TraceFile, LogFile);
		}

		// -----------------------------------------------------------------------
		// Frame timing / CPU-vs-GPU bound check (run this FIRST)
		// -----------------------------------------------------------------------
		if (Action == TEXT("frame_timing") || Action == TEXT("bound"))
		{
			return FrameTimingReport();
		}

		// -----------------------------------------------------------------------
		// Help
		// -----------------------------------------------------------------------
		if (Action == TEXT("screenshot") || Action == TEXT("capture"))
			{
				// Synchronous capture for the "build -> look -> fix" self-review loop.
				// This action is for EXTERNAL MCP clients (Claude Code, Cursor, ...), which
				// cannot call the internal-only attach_image tool. The VibeUE in-editor chat
				// must instead capture via execute_python_code -> ScreenshotService +
				// attach_image, so a returned file path actually enters its vision. Redirect
				// internal-chat callers (CurrentSession is non-null only for the in-editor chat).
				if (FToolRegistry::Get().GetCurrentSession() != nullptr)
				{
					return ErrJson(TEXT("USE_PYTHON_API"),
						TEXT("The editor_control 'screenshot' action is for external MCP clients only. From the VibeUE chat, capture with execute_python_code -> unreal.ScreenshotService.capture_editor_window(\"name\"), then call the attach_image tool with the returned file_path so the image enters your vision. Load the 'screenshots' skill for the full workflow."));
				}

				const FString Mode = GetParam(Params, TEXT("mode"), TEXT("editor_window")).ToLower();
				const FString Name = GetParam(Params, TEXT("name"), TEXT("editor_control_capture"));

				const FScreenshotResult Shot = (Mode == TEXT("active_window"))
					? UScreenshotService::CaptureActiveWindow(Name)
					: UScreenshotService::CaptureEditorWindow(Name);

				if (!Shot.bSuccess)
				{
					return ErrJson(TEXT("CAPTURE_FAILED"), Shot.Message);
				}

				TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
				R->SetStringField(TEXT("file_path"),       Shot.FilePath);
				R->SetNumberField(TEXT("width"),           Shot.Width);
				R->SetNumberField(TEXT("height"),          Shot.Height);
				R->SetStringField(TEXT("captured_window"), Shot.CapturedWindowTitle);
				R->SetStringField(TEXT("mode"),            Mode);
				R->SetStringField(TEXT("hint"),
					TEXT("Synchronous capture saved to disk - read/open the PNG at file_path to review what was built, then fix and re-capture. ")
					TEXT("To see the running game, start_pie first; if PIE opens its own window use mode=active_window."));
				return OkJson(R);
			}

			// -----------------------------------------------------------------------
			// Help
			// -----------------------------------------------------------------------
			if (Action == TEXT("help"))
		{
			TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
			R->SetStringField(TEXT("tool"), TEXT("editor_control"));

			TArray<TSharedPtr<FJsonValue>> Actions;
			auto A = [&](const FString& Name, const FString& Desc)
			{
				TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
				O->SetStringField(TEXT("action"), Name);
				O->SetStringField(TEXT("description"), Desc);
				Actions.Add(MakeShared<FJsonValueObject>(O));
			};

			A(TEXT("start_pie"),           TEXT("Start Play In Editor. Params: none"));
			A(TEXT("stop_pie"),            TEXT("Stop Play In Editor."));
			A(TEXT("pie_status"),          TEXT("Is PIE running?"));
			A(TEXT("start_standalone"),    TEXT("Launch game as standalone process with trace attached. Params: name (trace file name), channels"));
			A(TEXT("stop_standalone"),     TEXT("Stop standalone process."));
			A(TEXT("standalone_status"),   TEXT("Is standalone running? What trace file is it writing to?"));
			A(TEXT("profiler_start"),      TEXT("Start an Unreal Insights trace to file. Params: name (file name), channels"));
			A(TEXT("profiler_stop"),       TEXT("Stop the active trace. Returns file path and size."));
			A(TEXT("profiler_status"),     TEXT("Is a trace active? What channels are enabled?"));
			A(TEXT("profiler_bookmark"),   TEXT("Drop a named bookmark in the active trace. Params: name (required)"));
			A(TEXT("profiler_region_start"), TEXT("Begin a named region in the active trace. Params: name (required)"));
			A(TEXT("profiler_region_end"),   TEXT("End a named region in the active trace. Params: name (required)"));
			A(TEXT("analyse"),             TEXT("Read and summarise trace + logs. Params: source (trace|logs|both, default both), file (override path)"));
			A(TEXT("frame_timing"),        TEXT("Report Game/Render/GPU/RHI thread ms + CPU-vs-GPU bound verdict for the last rendered frame (same data as 'stat unit'). RUN THIS FIRST in any frame-rate investigation. Alias: bound."));
				A(TEXT("screenshot"),          TEXT("Synchronous PNG capture of the editor (or active window) for visual self-review — build, then SEE it. Params: mode (editor_window|active_window), name. Alias: capture. External MCP clients only; the in-editor VibeUE chat captures via ScreenshotService + attach_image instead."));

			R->SetArrayField(TEXT("actions"), Actions);

			TSharedPtr<FJsonObject> Workflow = MakeShared<FJsonObject>();
			Workflow->SetStringField(TEXT("diagnose_first"), TEXT("start_pie → [park in worst spot] → frame_timing → if GPU-bound: profiler_start→ProfileGPU; if CPU-bound: stat dumpframe -root=gamethread/renderthread"));
			Workflow->SetStringField(TEXT("in_editor"),  TEXT("profiler_start → [hit PIE] → profiler_stop → analyse"));
			Workflow->SetStringField(TEXT("standalone"), TEXT("start_standalone → [game loads] → stop_standalone → analyse"));
			Workflow->SetStringField(TEXT("pie_only"),   TEXT("start_pie → profiler_start → [play] → profiler_stop → stop_pie → analyse"));
			Workflow->SetStringField(TEXT("self_review"), TEXT("start_pie -> screenshot (mode=active_window) -> read the PNG -> fix -> screenshot again. For editor/asset views use screenshot (mode=editor_window)."));
				R->SetObjectField(TEXT("common_workflows"), Workflow);

			return OkJson(R);
		}

		return ErrJson(TEXT("UNKNOWN_ACTION"),
			FString::Printf(TEXT("Unknown action: '%s'. Call action=help for documentation."), *Action));
	}
);
