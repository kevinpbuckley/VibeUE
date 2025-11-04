#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Http.h"
#include "Json.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Commands/BlueprintCommands.h"
#include "Commands/BlueprintNodeCommands.h"
#include "Commands/BlueprintComponentReflection.h"
#include "Commands/UMGCommands.h"
#include "Commands/UMGReflectionCommands.h"
#include "Commands/AssetCommands.h"
#include "Core/ServiceContext.h"
#include "Bridge.generated.h"

class FMCPServerRunnable;

/**
 * Editor subsystem for MCP Bridge
 * Handles communication between external tools and the Unreal Editor
 * through a TCP socket connection. Commands are received as JSON and
 * routed to appropriate command handlers.
 */
UCLASS()
class VIBEUE_API UBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UBridge();
	virtual ~UBridge();

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
	// Route command to appropriate handler
	TSharedPtr<FJsonObject> RouteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);
	// Server state
	bool bIsRunning;
	TSharedPtr<FSocket> ListenerSocket;
	TSharedPtr<FSocket> ConnectionSocket;
	FRunnableThread* ServerThread;

	// Server configuration
	FIPv4Address ServerAddress;
	uint16 Port;

	// Service context (shared across all services)
	TSharedPtr<FServiceContext> ServiceContext;

	// Command handler instances
	TSharedPtr<FBlueprintCommands> BlueprintCommands;
	TSharedPtr<FBlueprintNodeCommands> BlueprintNodeCommands;
	TSharedPtr<FBlueprintComponentReflection> BlueprintComponentReflection;
	TSharedPtr<FUMGCommands> UMGCommands;
	TSharedPtr<FUMGReflectionCommands> UMGReflectionCommands;
	TSharedPtr<FAssetCommands> AssetCommands;

	// Helper to create error response
	FString CreateErrorResponse(const FString& ErrorCode, const FString& ErrorMessage = FString());
}; 
