// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Utils/VibeUEMcpStatus.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Containers/Ticker.h"

#if WITH_AUTOMATION_TESTS

// Covers the MCP startup-race fix (branch fix/mcp-startup-probe-grace). The bug: the readiness signal
// was published ~10 ms before Epic's MCP module bound its HTTP listener, so a bind probe at publish
// time always found the port FREE and logged a spurious Error while MCP was perfectly healthy. The fix
// replaces the immediate probe with a pure decision state machine + a bounded startup-grace ticker.
// These tests exercise the parts that are unit-testable without a live MCP server in a wrong state:
// the pure decision function (exhaustively), the port probe (against a real occupied loopback socket),
// and the ticker schedule/cancel lifecycle. Test path prefix VibeUE.McpStatus.*

namespace
{
	const TCHAR* ActionName(EVibeUEMcpStatusAction Action)
	{
		switch (Action)
		{
		case EVibeUEMcpStatusAction::Wait:             return TEXT("Wait");
		case EVibeUEMcpStatusAction::Silent:           return TEXT("Silent");
		case EVibeUEMcpStatusAction::LogFreeError:     return TEXT("LogFreeError");
		case EVibeUEMcpStatusAction::LogOccupiedError: return TEXT("LogOccupiedError");
		case EVibeUEMcpStatusAction::LogProbeError:    return TEXT("LogProbeError");
		case EVibeUEMcpStatusAction::LogContradiction: return TEXT("LogContradiction");
		default:                                       return TEXT("<unknown>");
		}
	}

	const TCHAR* ProbeName(EVibeUEMcpPortProbe Probe)
	{
		switch (Probe)
		{
		case EVibeUEMcpPortProbe::Free:       return TEXT("Free");
		case EVibeUEMcpPortProbe::InUse:      return TEXT("InUse");
		case EVibeUEMcpPortProbe::ProbeError: return TEXT("ProbeError");
		default:                              return TEXT("<unknown>");
		}
	}
}

// The whole state machine, table-driven. Grace is fixed at 10 s here; "before" = 1 s elapsed,
// "after" = 20 s elapsed. Every case names the scenario so a failure points straight at the row.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeMcpStatusDecideActionTest, "VibeUE.McpStatus.DecideAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeMcpStatusDecideActionTest::RunTest(const FString&)
{
	constexpr double Grace = 10.0;
	constexpr double Before = 1.0;   // inside the grace window
	constexpr double After = 20.0;   // grace expired

	struct FCase
	{
		const TCHAR* Name;
		bool bAutoStart;
		bool bListening;
		EVibeUEMcpPortProbe Probe;
		double Elapsed;
		EVibeUEMcpStatusAction Expected;
	};

	const FCase Cases[] =
	{
		// Clean auto-start with a listener that has not come up yet: NO premature error, just wait —
		// this is the exact startup race the fix targets.
		{ TEXT("auto-start, not listening, port free, before grace"),  true,  false, EVibeUEMcpPortProbe::Free,       Before, EVibeUEMcpStatusAction::Wait },
		{ TEXT("auto-start, not listening, port in use, before grace"), true,  false, EVibeUEMcpPortProbe::InUse,      Before, EVibeUEMcpStatusAction::Wait },
		{ TEXT("auto-start, not listening, probe error, before grace"), true,  false, EVibeUEMcpPortProbe::ProbeError, Before, EVibeUEMcpStatusAction::Wait },

		// Grace expired with no local server: one classified error per port state.
		{ TEXT("grace expired, port free"),        true,  false, EVibeUEMcpPortProbe::Free,       After, EVibeUEMcpStatusAction::LogFreeError },
		{ TEXT("grace expired, port occupied"),    true,  false, EVibeUEMcpPortProbe::InUse,      After, EVibeUEMcpStatusAction::LogOccupiedError },
		{ TEXT("grace expired, probe could not run"), true, false, EVibeUEMcpPortProbe::ProbeError, After, EVibeUEMcpStatusAction::LogProbeError },

		// Auto-start disabled and not listening: stay silent regardless of probe / elapsed.
		{ TEXT("no auto-start, not listening, free"),   false, false, EVibeUEMcpPortProbe::Free,       After, EVibeUEMcpStatusAction::Silent },
		{ TEXT("no auto-start, not listening, in use"),  false, false, EVibeUEMcpPortProbe::InUse,      After, EVibeUEMcpStatusAction::Silent },

		// Module reports listening: healthy — silent — even if the probe says in use (we hold the socket).
		{ TEXT("auto-start, listening, in use"),   true,  true,  EVibeUEMcpPortProbe::InUse,      After, EVibeUEMcpStatusAction::Silent },
		{ TEXT("auto-start, listening, free"),     true,  true,  EVibeUEMcpPortProbe::Free,       After, EVibeUEMcpStatusAction::Silent },
		{ TEXT("no auto-start, listening, in use"), false, true,  EVibeUEMcpPortProbe::InUse,      After, EVibeUEMcpStatusAction::Silent },

		// The one preserved immediate contradiction: not auto-starting, module claims listening, port free.
		{ TEXT("no auto-start, listening, free -> contradiction"), false, true, EVibeUEMcpPortProbe::Free, Before, EVibeUEMcpStatusAction::LogContradiction },
		// A probe error must NOT be mistaken for a contradiction (cannot prove the port is free).
		{ TEXT("no auto-start, listening, probe error -> silent"), false, true, EVibeUEMcpPortProbe::ProbeError, Before, EVibeUEMcpStatusAction::Silent },
	};

	for (const FCase& C : Cases)
	{
		const EVibeUEMcpStatusAction Got =
			FVibeUEMcpStatus::DecideAction(C.bAutoStart, C.bListening, C.Probe, C.Elapsed, Grace);
		if (Got != C.Expected)
		{
			AddError(FString::Printf(TEXT("DecideAction[%s]: expected %s, got %s (autoStart=%d listening=%d probe=%s elapsed=%.0f/%.0f)"),
				C.Name, ActionName(C.Expected), ActionName(Got),
				C.bAutoStart ? 1 : 0, C.bListening ? 1 : 0, ProbeName(C.Probe), C.Elapsed, Grace));
		}
	}

	// Boundary: exactly at grace is "expired" (not < grace), so it must classify rather than Wait.
	TestEqual(TEXT("elapsed == grace is treated as expired"),
		static_cast<uint8>(FVibeUEMcpStatus::DecideAction(true, false, EVibeUEMcpPortProbe::Free, Grace, Grace)),
		static_cast<uint8>(EVibeUEMcpStatusAction::LogFreeError));

	return true;
}

// The probe must distinguish "another socket holds the port" (InUse) from "port free". Occupy a real
// ephemeral loopback port, probe it, then release it and probe again.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeMcpStatusProbeTest, "VibeUE.McpStatus.ProbeLoopbackPort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeMcpStatusProbeTest::RunTest(const FString&)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get();
	if (!SocketSub)
	{
		AddWarning(TEXT("No socket subsystem available; skipping the live probe test."));
		return true;
	}

	// Bind an occupier to 127.0.0.1:0 so the OS hands us a free ephemeral port; reuse-addr OFF so it
	// genuinely holds the port against the probe's own (also reuse-addr OFF) bind.
	const TSharedRef<FInternetAddr> Addr = SocketSub->CreateInternetAddr();
	bool bIpValid = false;
	Addr->SetIp(TEXT("127.0.0.1"), bIpValid);
	Addr->SetPort(0);
	if (!TestTrue(TEXT("127.0.0.1 is a valid address"), bIpValid))
	{
		return true;
	}

	FSocket* Occupier = SocketSub->CreateSocket(NAME_Stream, TEXT("VibeUEMcpStatusTestOccupier"), Addr->GetProtocolType());
	if (!Occupier)
	{
		AddWarning(TEXT("Could not create a test socket; skipping the live probe test."));
		return true;
	}
	Occupier->SetReuseAddr(false);
	if (!Occupier->Bind(*Addr))
	{
		SocketSub->DestroySocket(Occupier);
		AddWarning(TEXT("Could not bind an ephemeral test port; skipping the live probe test."));
		return true;
	}

	// Discover the actual bound port to probe.
	const TSharedRef<FInternetAddr> BoundAddr = SocketSub->CreateInternetAddr();
	Occupier->GetAddress(*BoundAddr);
	const int32 BoundPort = BoundAddr->GetPort();
	if (!TestTrue(TEXT("occupier received a non-zero ephemeral port"), BoundPort > 0))
	{
		Occupier->Close();
		SocketSub->DestroySocket(Occupier);
		return true;
	}

	const uint32 Port = static_cast<uint32>(BoundPort);

	// While the occupier holds the port, the probe must report InUse (not ProbeError, not Free).
	const EVibeUEMcpPortProbe WhileHeld = FVibeUEMcpStatus::ProbeLoopbackPort(Port);
	TestEqual(FString::Printf(TEXT("occupied port %u probes as InUse (got %s)"), Port, ProbeName(WhileHeld)),
		static_cast<uint8>(WhileHeld), static_cast<uint8>(EVibeUEMcpPortProbe::InUse));

	// Release the port. A bound-but-not-listening socket with no connections leaves no TIME_WAIT, so the
	// port is immediately bindable again and must probe Free.
	Occupier->Close();
	SocketSub->DestroySocket(Occupier);

	const EVibeUEMcpPortProbe AfterRelease = FVibeUEMcpStatus::ProbeLoopbackPort(Port);
	TestEqual(FString::Printf(TEXT("released port %u probes as Free (got %s)"), Port, ProbeName(AfterRelease)),
		static_cast<uint8>(AfterRelease), static_cast<uint8>(EVibeUEMcpPortProbe::Free));

	return true;
}

// Shutdown-while-pending: a scheduled grace ticker must cancel cleanly (as Remove() does at teardown)
// so its callback can never fire afterward. Uses port 0 and cancels before any manual tick, so the
// callback never touches the live MCP module and no shared ticker is pumped.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeMcpStatusTickerCancelTest, "VibeUE.McpStatus.StartupGraceTickerCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVibeMcpStatusTickerCancelTest::RunTest(const FString&)
{
	// Start from a known-clean state (the running editor's own startup grace has long since resolved).
	FVibeUEMcpStatus::CancelStartupGraceTicker();
	TestFalse(TEXT("no ticker pending before scheduling"), FVibeUEMcpStatus::IsStartupGraceTickerPending());

	FVibeUEMcpStatus::ScheduleStartupGraceTicker(0);
	TestTrue(TEXT("ticker pending after scheduling"), FVibeUEMcpStatus::IsStartupGraceTickerPending());

	// Scheduling again must not stack a second ticker — a single cancel below must fully clear it.
	FVibeUEMcpStatus::ScheduleStartupGraceTicker(0);
	TestTrue(TEXT("still exactly one ticker pending after a redundant schedule"), FVibeUEMcpStatus::IsStartupGraceTickerPending());

	// Simulate module shutdown / editor pre-exit cancelling the pending retry.
	FVibeUEMcpStatus::CancelStartupGraceTicker();
	TestFalse(TEXT("ticker cancelled by CancelStartupGraceTicker"), FVibeUEMcpStatus::IsStartupGraceTickerPending());

	// Cancelling again must be a safe no-op (Remove() runs at both RegisterToolsets start and shutdown).
	FVibeUEMcpStatus::CancelStartupGraceTicker();
	TestFalse(TEXT("double cancel stays clean"), FVibeUEMcpStatus::IsStartupGraceTickerPending());

	return true;
}

#endif // WITH_AUTOMATION_TESTS
