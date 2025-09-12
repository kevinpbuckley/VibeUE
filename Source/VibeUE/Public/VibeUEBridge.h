#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Http.h"
#include "Json.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Commands/VibeUEBlueprintCommands.h"
#include "Commands/VibeUEBlueprintNodeCommands.h"
#include "Commands/VibeUEUMGCommands.h"
#include "Commands/VibeUEAssetCommands.h"
#include "VibeUEBridge.generated.h"

class FMCPServerRunnable;

/**
 * Editor subsystem for MCP Bridge
 * Handles communication between external tools and the Unreal Editor
 * through a TCP socket connection. Commands are received as JSON and
 * routed to appropriate command handlers.
 */
UCLASS()
class VIBEUE_API UVibeUEBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UVibeUEBridge();
	virtual ~UVibeUEBridge();

	// UEditorSubsystem implementation
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Server functions
	void StartServer();
	void StopServer();
	bool IsRunning() const { return bIsRunning; }

	// Command execution
	FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

private:
	// Server state
	bool bIsRunning;
	TSharedPtr<FSocket> ListenerSocket;
	TSharedPtr<FSocket> ConnectionSocket;
	FRunnableThread* ServerThread;

	// Server configuration
	FIPv4Address ServerAddress;
	uint16 Port;

	// Command handler instances
	TSharedPtr<FVibeUEBlueprintCommands> BlueprintCommands;
	TSharedPtr<FVibeUEBlueprintNodeCommands> BlueprintNodeCommands;
	TSharedPtr<FVibeUEUMGCommands> UMGCommands;
	TSharedPtr<FVibeUEAssetCommands> AssetCommands;
}; 