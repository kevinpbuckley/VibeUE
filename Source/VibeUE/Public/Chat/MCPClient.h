// Copyright 2025 Vibe AI. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chat/MCPTypes.h"
#include "HAL/PlatformProcess.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMCPClient, Log, All);

/**
 * Delegate called when tools are discovered from MCP server
 */
DECLARE_DELEGATE_TwoParams(FOnToolsDiscovered, bool /* bSuccess */, const TArray<FMCPTool>& /* Tools */);

/**
 * Delegate called when a tool execution completes
 */
DECLARE_DELEGATE_TwoParams(FOnToolExecuted, bool /* bSuccess */, const FMCPToolResult& /* Result */);

/**
 * State of a single MCP server connection
 */
struct FMCPServerState
{
    /** Server configuration */
    FMCPServerConfig Config;
    
    /** Process handle if running */
    FProcHandle ProcessHandle;
    
    /** Pipe handles for stdio communication */
    void* ReadPipe = nullptr;
    void* WritePipe = nullptr;
    
    /** Whether server is initialized and ready */
    bool bInitialized = false;
    
    /** Available tools from this server */
    TArray<FMCPTool> Tools;
    
    /** Next request ID */
    int32 NextRequestId = 1;
    
    /** Pending requests awaiting responses */
    TMap<int32, TFunction<void(TSharedPtr<FJsonObject>)>> PendingRequests;
};

/**
 * MCP Client for communicating with MCP servers
 * 
 * Supports stdio transport for subprocess-based servers.
 * Handles JSON-RPC 2.0 protocol for MCP communication.
 */
class VIBEUE_API FMCPClient : public TSharedFromThis<FMCPClient>
{
public:
    FMCPClient();
    ~FMCPClient();
    
    /**
     * Initialize the MCP client
     * @param bEngineMode True for Engine (FAB) install, False for Local (open source) install
     */
    void Initialize(bool bEngineMode);
    
    /** Shutdown all server connections */
    void Shutdown();
    
    /**
     * Load MCP configuration from file
     * @param ConfigPath Path to mcp.json file
     * @return True if configuration was loaded successfully
     */
    bool LoadConfiguration(const FString& ConfigPath);
    
    /**
     * Start a specific MCP server
     * @param ServerName Name of the server to start
     * @return True if server started successfully
     */
    bool StartServer(const FString& ServerName);
    
    /**
     * Stop a specific MCP server
     * @param ServerName Name of the server to stop
     */
    void StopServer(const FString& ServerName);
    
    /**
     * Discover available tools from all connected servers
     * @param OnComplete Callback when discovery completes
     */
    void DiscoverTools(FOnToolsDiscovered OnComplete);
    
    /**
     * Execute a tool call
     * @param ToolCall The tool call to execute
     * @param OnComplete Callback when execution completes
     */
    void ExecuteTool(const FMCPToolCall& ToolCall, FOnToolExecuted OnComplete);
    
    /** Get all available tools */
    const TArray<FMCPTool>& GetAvailableTools() const { return AllTools; }
    
    /** Get count of available tools */
    int32 GetToolCount() const { return AllTools.Num(); }
    
    /** Get count of connected servers */
    int32 GetConnectedServerCount() const;
    
    /** Check if a server is connected and ready */
    bool IsServerConnected(const FString& ServerName) const;
    
    /** Get the loaded configuration */
    const FMCPConfiguration& GetConfiguration() const { return Configuration; }
    
    /** Get current mode (Engine or Local) */
    bool IsEngineMode() const { return bEngineMode; }
    
    /**
     * Find the VibeUE plugin folder in the Engine Marketplace directory
     * FAB installs use random folder names, so we scan for the vibe_ue_server.py file
     * @param OutFolderName Returns the folder name if found
     * @return True if VibeUE was found in the Marketplace
     */
    static bool FindVibeUEInMarketplace(FString& OutFolderName);
    
    /**
     * Get the full path to the VibeUE Python content folder for Engine mode
     * @return Full path if found, empty string if not found
     */
    static FString GetEngineVibeUEPythonPath();
    
private:
    /** MCP configuration loaded from mcp.json */
    FMCPConfiguration Configuration;
    
    /** State for each server */
    TMap<FString, TSharedPtr<FMCPServerState>> ServerStates;
    
    /** All available tools from all servers */
    TArray<FMCPTool> AllTools;
    
    /** Whether running in Engine mode (FAB install) vs Local mode */
    bool bEngineMode = false;
    
    /** Resolve variable substitutions in config strings */
    FString ResolveConfigVariables(const FString& Input) const;
    
    /** Send a JSON-RPC request to a server */
    bool SendRequest(FMCPServerState& State, const FString& Method, TSharedPtr<FJsonObject> Params = nullptr, TFunction<void(TSharedPtr<FJsonObject>)> OnResponse = nullptr);
    
    /** Read response from server pipe */
    bool ReadResponse(FMCPServerState& State, FString& OutResponse);
    
    /** Process a JSON-RPC response */
    void ProcessResponse(FMCPServerState& State, const FString& ResponseJson);
    
    /** Perform MCP initialize handshake */
    bool InitializeServer(FMCPServerState& State);
    
    /** Request tools list from server */
    void RequestToolsList(FMCPServerState& State);
    
    /** Find which server provides a tool */
    FMCPServerState* FindServerForTool(const FString& ToolName);
};
