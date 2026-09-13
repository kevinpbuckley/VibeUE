// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Utils/VibeUEMcpStatus.h"

#include "IModelContextProtocolModule.h"
#include "ModelContextProtocolServer.h"
#include "ModelContextProtocolSettings.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

DEFINE_LOG_CATEGORY_STATIC(LogVibeUEMcp, Log, All);

namespace
{
	enum class EPortProbe : uint8
	{
		Free,      // bind succeeded — nobody is listening on the port
		InUse,     // bind failed with address-in-use — something is listening
		ProbeError // could not run the probe (no socket subsystem / socket creation failed)
	};

	// Attempt a throwaway bind on 127.0.0.1:Port. Reuse-addr is forced OFF so an existing listener
	// makes our bind fail; the socket is closed immediately either way.
	EPortProbe ProbeLoopbackPort(uint32 Port)
	{
		ISocketSubsystem* SocketSub = ISocketSubsystem::Get();
		if (!SocketSub)
		{
			return EPortProbe::ProbeError;
		}

		const TSharedRef<FInternetAddr> Addr = SocketSub->CreateInternetAddr();
		bool bIpValid = false;
		Addr->SetIp(TEXT("127.0.0.1"), bIpValid);
		Addr->SetPort(static_cast<int32>(Port));
		if (!bIpValid)
		{
			return EPortProbe::ProbeError;
		}

		FSocket* Socket = SocketSub->CreateSocket(NAME_Stream, TEXT("VibeUEMcpPortProbe"), Addr->GetProtocolType());
		if (!Socket)
		{
			return EPortProbe::ProbeError;
		}

		Socket->SetReuseAddr(false);
		const bool bBound = Socket->Bind(*Addr);

		Socket->Close();
		SocketSub->DestroySocket(Socket);

		return bBound ? EPortProbe::Free : EPortProbe::InUse;
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

void FVibeUEMcpStatus::LogPortContradictionIfAny(uint32 Port, bool bModuleListening)
{
	// "Intends to serve" = auto-start requested (command line or settings) or the module already
	// reports a running server. If this process is not supposed to serve MCP, stay silent — another
	// editor legitimately owning the port is not our problem.
	const bool bIntendsToServe = UE::ModelContextProtocol::ShouldAutoStartServer() || bModuleListening;
	if (!bIntendsToServe)
	{
		return;
	}

	const EPortProbe Probe = ProbeLoopbackPort(Port);
	switch (Probe)
	{
	case EPortProbe::InUse:
		// A listener holds the port. From inside the owning process a bind fails whether WE hold it
		// or another process won the race, so this is the expected healthy case — nothing loud.
		break;

	case EPortProbe::Free:
		UE_LOG(LogVibeUEMcp, Error,
			TEXT("VibeUE: MCP is expected to listen on 127.0.0.1:%u but a socket bind probe found the port FREE ")
			TEXT("— no HTTP listener is bound, so every MCP call to this editor will fail to connect. If a headless ")
			TEXT("UnrealEditor-Cmd and the GUI editor started together, one lost the port-%u fight (see LogHttpListener ")
			TEXT("'unable to bind'); restart the loser."),
			Port, Port);
		break;

	case EPortProbe::ProbeError:
		UE_LOG(LogVibeUEMcp, Error,
			TEXT("VibeUE: MCP is expected to listen on 127.0.0.1:%u but the socket bind probe could not run ")
			TEXT("— unable to confirm a live MCP listener on this process."),
			Port);
		break;
	}
}
