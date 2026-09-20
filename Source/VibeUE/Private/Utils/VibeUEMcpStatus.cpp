// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Utils/VibeUEMcpStatus.h"
#include "Utils/VibeUEReadinessSignal.h"

#include "IModelContextProtocolModule.h"
#include "ModelContextProtocolServer.h"
#include "ModelContextProtocolSettings.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogVibeUEMcp, Log, All);

namespace
{
	// Startup-grace ticker state (see FVibeUEMcpStatus header). Single ticker at a time; all touched on
	// the game thread only, so plain file-statics are safe (matches VibeUEHealthSignal's style).
	FTSTicker::FDelegateHandle GStartupGraceTickerHandle;
	double GStartupGraceStartSeconds = 0.0;

	// One terminal conclusion per process: once we have either seen the listener come up or logged the
	// grace-expiry / contradiction error, later republishes (map-open) must not re-log or reschedule.
	bool GStartupResolved = false;

	// The per-tick body. Returns true to keep ticking, false to stop (FTSTicker then removes it).
	bool TickStartupGrace()
	{
		uint32 Port = 0;
		bool bListening = false;
		FVibeUEMcpStatus::Query(Port, bListening);

		const double Elapsed = FMath::Max(0.0, FPlatformTime::Seconds() - GStartupGraceStartSeconds);
		const bool bAutoStart = UE::ModelContextProtocol::ShouldAutoStartServer();
		const EVibeUEMcpPortProbe Probe = FVibeUEMcpStatus::ProbeLoopbackPort(Port);
		const EVibeUEMcpStatusAction Action =
			FVibeUEMcpStatus::DecideAction(bAutoStart, bListening, Probe, Elapsed, FVibeUEMcpStatus::StartupGraceSeconds);

		if (Action == EVibeUEMcpStatusAction::Wait)
		{
			return true; // listener not up yet, still within grace — keep waiting
		}

		// Terminal fire. Drop our handle and latch the conclusion BEFORE any republish, so the re-entrant
		// OnReadinessPublished (via Publish below) is a no-op and cannot restack a ticker.
		GStartupGraceTickerHandle.Reset();
		GStartupResolved = true;

		if (Action == EVibeUEMcpStatusAction::Silent)
		{
			// The listener bound within grace: republish so the readiness JSON flips mcpListening=true
			// immediately, instead of waiting for the next map-open event.
			FVibeUEReadinessSignal::Publish();
		}
		else
		{
			FVibeUEMcpStatus::LogActionResult(Action, Port);
		}
		return false; // stop ticking
	}
}

void FVibeUEMcpStatus::Query(uint32& OutPort, bool& OutListening)
{
	// Configured port (honours -ModelContextProtocolPort=N; falls back to the settings CDO, default 8000).
	OutPort = UE::ModelContextProtocol::GetServerPortNumber();
	OutListening = false;

	if (IModelContextProtocolModule* Module = IModelContextProtocolModule::Get())
	{
		if (FModelContextProtocolServer* Server = Module->GetServer())
		{
			OutListening = Server->IsServerRunning();
			if (OutListening)
			{
				const uint32 ActivePort = Server->GetServerPort();
				if (ActivePort != 0)
				{
					OutPort = ActivePort;
				}
			}
		}
	}
}

EVibeUEMcpPortProbe FVibeUEMcpStatus::ProbeLoopbackPort(uint32 Port)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get();
	if (!SocketSub)
	{
		return EVibeUEMcpPortProbe::ProbeError;
	}

	const TSharedRef<FInternetAddr> Addr = SocketSub->CreateInternetAddr();
	bool bIpValid = false;
	Addr->SetIp(TEXT("127.0.0.1"), bIpValid);
	Addr->SetPort(static_cast<int32>(Port));
	if (!bIpValid)
	{
		return EVibeUEMcpPortProbe::ProbeError;
	}

	FSocket* Socket = SocketSub->CreateSocket(NAME_Stream, TEXT("VibeUEMcpPortProbe"), Addr->GetProtocolType());
	if (!Socket)
	{
		return EVibeUEMcpPortProbe::ProbeError;
	}

	// Reuse-addr OFF so an existing listener makes our bind fail with address-in-use.
	Socket->SetReuseAddr(false);
	const bool bBound = Socket->Bind(*Addr);
	// Read the last error immediately after Bind, before Close/Destroy can clobber it.
	const ESocketErrors BindError = bBound ? SE_NO_ERROR : SocketSub->GetLastErrorCode();

	Socket->Close();
	SocketSub->DestroySocket(Socket);

	if (bBound)
	{
		return EVibeUEMcpPortProbe::Free;
	}

	// Address-in-use is the only failure that proves a live listener; anything else (permissions, an
	// invalid protocol, a transient subsystem error) is a probe that could not classify the port.
	return (BindError == SE_EADDRINUSE) ? EVibeUEMcpPortProbe::InUse : EVibeUEMcpPortProbe::ProbeError;
}

EVibeUEMcpStatusAction FVibeUEMcpStatus::DecideAction(bool bAutoStart, bool bModuleListening,
	EVibeUEMcpPortProbe Probe, double ElapsedSeconds, double GraceSeconds)
{
	if (bModuleListening)
	{
		// Genuine contradiction (the only immediate-error case kept): nobody asked this process to
		// auto-start, yet the module claims a live server while the port probes FREE — the listener
		// never actually bound. When auto-start IS pending we do not raise this; the listener may simply
		// not have finished binding, which the grace window covers.
		if (!bAutoStart && Probe == EVibeUEMcpPortProbe::Free)
		{
			return EVibeUEMcpStatusAction::LogContradiction;
		}
		return EVibeUEMcpStatusAction::Silent;
	}

	// Module not listening.
	if (!bAutoStart)
	{
		// This process was never meant to serve MCP — another editor owning the port is not our problem.
		return EVibeUEMcpStatusAction::Silent;
	}

	// Auto-start requested but the listener has not reported running yet.
	if (ElapsedSeconds < GraceSeconds)
	{
		return EVibeUEMcpStatusAction::Wait;
	}

	// Grace expired with no local server — classify by what holds the port.
	switch (Probe)
	{
	case EVibeUEMcpPortProbe::Free:  return EVibeUEMcpStatusAction::LogFreeError;
	case EVibeUEMcpPortProbe::InUse: return EVibeUEMcpStatusAction::LogOccupiedError;
	default:                         return EVibeUEMcpStatusAction::LogProbeError;
	}
}

void FVibeUEMcpStatus::LogActionResult(EVibeUEMcpStatusAction Action, uint32 Port)
{
	switch (Action)
	{
	case EVibeUEMcpStatusAction::LogFreeError:
		UE_LOG(LogVibeUEMcp, Error,
			TEXT("VibeUE: MCP auto-start was requested but no HTTP listener bound to 127.0.0.1:%u within %.0f s ")
			TEXT("and the port is FREE — this editor's MCP server failed to start (or never started), so every MCP ")
			TEXT("call to this editor will fail to connect. Check the log above for a ModelContextProtocol / ")
			TEXT("LogHttpListener startup error."),
			Port, FVibeUEMcpStatus::StartupGraceSeconds);
		break;

	case EVibeUEMcpStatusAction::LogOccupiedError:
		UE_LOG(LogVibeUEMcp, Error,
			TEXT("VibeUE: MCP auto-start was requested but this editor never bound 127.0.0.1:%u within %.0f s and the ")
			TEXT("port is held by ANOTHER process — this editor lost the port-%u fight and owns no MCP listener, so ")
			TEXT("every MCP call to it will fail to connect. If a headless UnrealEditor-Cmd and the GUI editor started ")
			TEXT("together, one lost the bind (see LogHttpListener 'unable to bind'); restart the loser and never run a ")
			TEXT("headless editor while the GUI editor is starting."),
			Port, FVibeUEMcpStatus::StartupGraceSeconds, Port);
		break;

	case EVibeUEMcpStatusAction::LogProbeError:
		UE_LOG(LogVibeUEMcp, Error,
			TEXT("VibeUE: MCP auto-start was requested but no listener reported running on 127.0.0.1:%u within %.0f s ")
			TEXT("and the socket bind probe could not run — unable to confirm a live MCP listener on this process."),
			Port, FVibeUEMcpStatus::StartupGraceSeconds);
		break;

	case EVibeUEMcpStatusAction::LogContradiction:
		UE_LOG(LogVibeUEMcp, Error,
			TEXT("VibeUE: this editor's MCP module reports a running server on 127.0.0.1:%u but a socket bind probe found ")
			TEXT("the port FREE — no HTTP listener is actually bound, so every MCP call to this editor will fail to connect."),
			Port);
		break;

	default:
		// Wait / Silent — nothing to log.
		break;
	}
}

void FVibeUEMcpStatus::OnReadinessPublished(uint32 Port, bool bModuleListening)
{
	if (GStartupResolved)
	{
		// A terminal conclusion was already reached this process. Later republishes still refresh the
		// JSON's mcpPort/mcpListening (built from Query before this call), so there is nothing to do here.
		return;
	}

	const bool bAutoStart = UE::ModelContextProtocol::ShouldAutoStartServer();
	const EVibeUEMcpPortProbe Probe = ProbeLoopbackPort(Port);
	const EVibeUEMcpStatusAction Action = DecideAction(bAutoStart, bModuleListening, Probe, /*ElapsedSeconds=*/0.0, StartupGraceSeconds);

	switch (Action)
	{
	case EVibeUEMcpStatusAction::Wait:
		// The publish beat the listener to the port (the startup race). Wait for it on a bounded ticker
		// instead of logging a spurious error. Stays unresolved; the ticker reaches the conclusion.
		ScheduleStartupGraceTicker(Port);
		break;

	case EVibeUEMcpStatusAction::Silent:
		GStartupResolved = true;
		break;

	default:
		LogActionResult(Action, Port);
		GStartupResolved = true;
		break;
	}
}

void FVibeUEMcpStatus::ScheduleStartupGraceTicker(uint32 Port)
{
	if (GStartupGraceTickerHandle.IsValid())
	{
		return; // already waiting — do not restart the clock or stack a second ticker
	}

	GStartupGraceStartSeconds = FPlatformTime::Seconds();
	GStartupGraceTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		TEXT("VibeUEMcpStartupGrace"), StartupProbeIntervalSeconds,
		[](float) -> bool { return TickStartupGrace(); });
	(void)Port; // the port is re-read from the module on every tick, so nothing is captured here
}

void FVibeUEMcpStatus::CancelStartupGraceTicker()
{
	if (GStartupGraceTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GStartupGraceTickerHandle);
		GStartupGraceTickerHandle.Reset();
	}
}

bool FVibeUEMcpStatus::IsStartupGraceTickerPending()
{
	return GStartupGraceTickerHandle.IsValid();
}
