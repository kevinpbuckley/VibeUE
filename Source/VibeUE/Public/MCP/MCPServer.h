// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Networking.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Containers/Queue.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMCPServer, Log, All);

// Forward declarations
class FSocket;
class FTcpListener;
struct FMCPTool;

/**
 * MCP Host Configuration
 * Settings for exposing internal tools to external clients via Streamable HTTP
 * Note: Named FMCPHostConfig to avoid collision with FMCPServerConfig (client connection config)
 */
struct FMCPHostConfig
{
    /** Whether the MCP server is enabled */
    bool bEnabled = true;
    
    /** Port to listen on */
    int32 Port = 8080;
    
    /** API key for authentication (empty = no auth required) */
    FString ApiKey;
};

/**
 * Represents a pending HTTP request to process on the game thread
 */
struct FMCPPendingRequest
{
    FSocket* ClientSocket = nullptr;
    FString Method;
    FString Path;
    TMap<FString, FString> Headers;
    FString Body;
    FString SessionId;
};

/**
 * Represents an active SSE stream connection
 */
struct FMCPSSEConnection
{
    FSocket* ClientSocket = nullptr;
    FString SessionId;
    FDateTime ConnectedAt;
    int32 LastEventId = 0;
    bool bIsActive = true;
};

/**
 * MCP Server - Exposes internal tools to external chat clients
 * 
 * Implements the Model Context Protocol (MCP) Streamable HTTP transport
 * allowing clients like VS Code, Cursor, and Claude Desktop to use
 * VibeUE's internal tools.
 * 
 * @see https://modelcontextprotocol.io/specification/2025-06-18/basic/transports#streamable-http
 */
class VIBEUE_API FMCPServer : public FRunnable
{
public:
    FMCPServer();
    virtual ~FMCPServer();
    
    /** Get singleton instance */
    static FMCPServer& Get();
    
    /** Initialize the server (loads config, optionally starts if enabled) */
    void Initialize();
    
    /** Shutdown the server */
    void Shutdown();
    
    /** Start the HTTP server */
    bool Start();
    
    /** Stop the HTTP server */
    void StopServer();
    
    /** Check if server is running */
    bool IsRunning() const { return bIsRunning; }
    
    /** Get current configuration */
    const FMCPHostConfig& GetConfig() const { return Config; }
    
    /** Get the server URL for display */
    FString GetServerUrl() const;
    
    // ============ FRunnable Interface ============
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override { bShouldStop = true; }
    virtual void Exit() override;
    
    // ============ Config Persistence ============
    
    /** Get MCP Server enabled state from config */
    static bool GetEnabledFromConfig();
    
    /** Save MCP Server enabled state to config */
    static void SaveEnabledToConfig(bool bEnabled);
    
    /** Get MCP Server port from config */
    static int32 GetPortFromConfig();
    
    /** Save MCP Server port to config */
    static void SavePortToConfig(int32 Port);
    
    /** Get MCP Server API key from config */
    static FString GetApiKeyFromConfig();
    
    /** Save MCP Server API key to config */
    static void SaveApiKeyToConfig(const FString& ApiKey);
    
    /** Load all config into Config struct */
    void LoadConfig();
    
    /** Save all config from Config struct */
    void SaveConfig();
    
    /** Process pending requests on game thread (called by tick) */
    void ProcessPendingRequests();

private:
    /** Handle incoming connection */
    bool HandleConnection(FSocket* ClientSocket);
    
    /** Parse HTTP request from socket */
    bool ParseHttpRequest(FSocket* Socket, FString& OutMethod, FString& OutPath, 
                          TMap<FString, FString>& OutHeaders, FString& OutBody);
    
    /** Send HTTP response */
    void SendHttpResponse(FSocket* Socket, int32 StatusCode, const FString& StatusText,
                          const FString& ContentType, const FString& Body,
                          const TMap<FString, FString>& ExtraHeaders = TMap<FString, FString>());
    
    /** Handle MCP JSON-RPC request */
    FString HandleMCPRequest(const FString& JsonBody, FString& InOutSessionId, bool& bOutIsNotification);
    
    /** Handle SSE GET request - opens event stream. bSendEndpointEvent=true for legacy /sse transport (Windsurf). */
    bool HandleSSERequest(FSocket* ClientSocket, const TMap<FString, FString>& Headers, bool bSendEndpointEvent = false);
    
    /** Send SSE event to a client */
    bool SendSSEEvent(FSocket* ClientSocket, const FString& Data, int32 EventId = -1);
    
    /** Send SSE response with Content-Type: text/event-stream */
    void SendSSEResponse(FSocket* Socket, const FString& SessionId);
    
    /** Validate MCP-Protocol-Version header */
    bool ValidateProtocolVersion(const TMap<FString, FString>& Headers) const;
    
    /** Parse Accept header to check for SSE support */
    bool AcceptsSSE(const TMap<FString, FString>& Headers) const;
    
    /** Handle individual JSON-RPC methods */
    FString HandleInitialize(TSharedPtr<FJsonObject> Params, const FString& RequestId);
    FString HandleToolsList(TSharedPtr<FJsonObject> Params, const FString& RequestId);
    FString HandleToolsCall(TSharedPtr<FJsonObject> Params, const FString& RequestId);
    FString HandlePing(const FString& RequestId);
    
    /** Build JSON-RPC response */
    FString BuildJsonRpcResponse(const FString& RequestId, TSharedPtr<FJsonObject> Result);
    FString BuildJsonRpcError(const FString& RequestId, int32 Code, const FString& Message);
    
    /** Validate API key if configured */
    bool ValidateApiKey(const TMap<FString, FString>& Headers) const;
    
    /** Validate Origin header for security */
    bool ValidateOrigin(const TMap<FString, FString>& Headers) const;
    
    /** Generate new session ID */
    FString GenerateSessionId() const;
    
    /** Get internal tools in MCP format */
    TArray<FMCPTool> GetInternalTools() const;
    
    /** Current configuration */
    FMCPHostConfig Config;
    
    /** Whether the server is currently running */
    bool bIsRunning = false;
    
    /** Signal to stop the server thread */
    bool bShouldStop = false;
    
    /** TCP listener socket */
    FSocket* ListenerSocket = nullptr;
    
    /** Server thread */
    FRunnableThread* ServerThread = nullptr;
    
    /** Active sessions */
    TMap<FString, FDateTime> ActiveSessions;
    
    /** Critical section for thread safety */
    FCriticalSection SessionLock;
    
    /** Queue of requests to process on game thread */
    TQueue<FMCPPendingRequest> PendingRequests;
    
    /** Active SSE connections */
    TArray<TSharedPtr<FMCPSSEConnection>> SSEConnections;
    FCriticalSection SSELock;

    /** Pending responses queued before SSE connection opened (legacy SSE transport) */
    TArray<FString> PendingSSEResponses;
    FCriticalSection PendingSSELock;
    
    /** Next SSE event ID for resumability */
    TAtomic<int32> NextEventId;
    
    /** Tick delegate handle */
    FTSTicker::FDelegateHandle TickDelegateHandle;
    
    /** Singleton instance */
    static TSharedPtr<FMCPServer> Instance;
};
