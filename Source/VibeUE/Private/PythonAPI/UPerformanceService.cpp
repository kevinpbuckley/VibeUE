// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UPerformanceService.h"
#include "Json.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
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

FString UPerformanceService::FrameTiming()
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

	const double FrameMs = FMath::Max3(GameMs, RenderMs, GpuMs);
	R->SetNumberField(TEXT("frame_ms"), Round2(FrameMs));
	R->SetNumberField(TEXT("fps"), FrameMs > 0.0 ? Round2(1000.0 / FrameMs) : 0.0);

	FString Bound;
	FString Hint;
	if (GpuMs >= GameMs && GpuMs >= RenderMs && GpuMs > 0.0)
	{
		Bound = TEXT("GPU");
		Hint  = TEXT("GPU-bound. NOW it is worth profiling the GPU: StartTrace (channels frame,gpu,cpu), then 'r.ProfileGPU.ShowUI 0' + 'ProfileGPU' and read the pass breakdown from the log. Levers: shadows (Virtual Shadow Maps), Lumen GI/reflections, translucency, post-process, ScreenPercentage. Confirm with the resolution test: 'r.ScreenPercentage 50' should noticeably raise FPS if truly GPU-bound.");
	}
	else if (RenderMs >= GameMs)
	{
		Bound = TEXT("RenderThread");
		Hint  = TEXT("CPU render-thread bound. Usual cause: too many draw calls / primitives, or many dynamic shadow-casting lights. Check 'stat scenerendering'. Levers: merge/instance meshes, enable Nanite, cut dynamic lights and per-light shadows. NOTE: dropping r.ScreenPercentage will NOT help a render-thread-bound frame.");
	}
	else
	{
		Bound = TEXT("GameThread");
		Hint  = TEXT("CPU game-thread bound. Usual cause: Tick / Blueprint / AI / animation cost. Run 'stat dumpframe -ms=0.5 -root=gamethread' on the PIE world then read the result. Levers: throttle/disable unnecessary Tick, reduce ticking actors & AI, cut expensive Blueprint tick logic. NOTE: dropping r.ScreenPercentage will NOT help a game-thread-bound frame.");
	}
	R->SetStringField(TEXT("bound"), Bound);
	R->SetStringField(TEXT("hint"), Hint);

	const bool bPIE = IsPIERunning();
	R->SetBoolField(TEXT("pie_running"), bPIE);
	R->SetStringField(TEXT("note"),
		bPIE
		? TEXT("Values are for the most recently rendered PIE frame. Park in a representative/worst spot for a clean read.")
		: TEXT("PIE is NOT running — these values reflect the EDITOR viewport, not your game. Start PIE (Epic's EditorAppToolset.StartPIE) for a real game-bound reading."));
	if (GpuMs <= 0.0)
	{
		R->SetStringField(TEXT("gpu_note"), TEXT("gpu_ms is 0 (GPU timing unavailable this frame) — rely on the game vs render comparison, and confirm GPU-bound with the r.ScreenPercentage 50 test."));
	}
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
	GLastLogFilePath   = ProjectSavedDirAbs() / TEXT("Logs") / (FApp::GetProjectName() + FString(TEXT(".log")));

	FString ExtraArgs = FString::Printf(
		TEXT("-tracehost=127.0.0.1 -trace=%s -tracefile=\"%s\""),
		*ChannelSet, *TracePath);

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
