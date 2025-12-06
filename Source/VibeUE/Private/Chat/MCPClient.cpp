// Copyright 2025 Vibe AI. All Rights Reserved.

#include "Chat/MCPClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Async/Async.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY(LogMCPClient);

FMCPClient::FMCPClient()
{
}

FMCPClient::~FMCPClient()
{
    Shutdown();
}

void FMCPClient::Initialize(bool bInEngineMode)
{
    bEngineMode = bInEngineMode;
    
    UE_LOG(LogMCPClient, Log, TEXT("MCP Client initializing in %s mode"), bEngineMode ? TEXT("Engine") : TEXT("Local"));
    
    // Always look for vibeue.mcp.json - first in plugin folder, then fallback locations
    FString ConfigPath;
    
    // Priority 1: Plugin Config folder
    ConfigPath = FPaths::ProjectPluginsDir() / TEXT("VibeUE") / TEXT("Config") / TEXT("vibeue.mcp.json");
    
    if (!FPaths::FileExists(ConfigPath))
    {
        // Priority 2: Engine plugins (for marketplace installs)
        ConfigPath = FPaths::EnginePluginsDir() / TEXT("Marketplace") / TEXT("VibeUE") / TEXT("Config") / TEXT("vibeue.mcp.json");
    }
    
    if (!FPaths::FileExists(ConfigPath))
    {
        // Priority 3: Project saved folder (user override)
        ConfigPath = FPaths::ProjectSavedDir() / TEXT("VibeUE") / TEXT("vibeue.mcp.json");
    }
    
    if (FPaths::FileExists(ConfigPath))
    {
        if (LoadConfiguration(ConfigPath))
        {
            UE_LOG(LogMCPClient, Log, TEXT("Loaded MCP configuration from %s"), *ConfigPath);
        }
    }
    else
    {
        UE_LOG(LogMCPClient, Warning, TEXT("MCP configuration file not found. Searched: Plugin/Config, Engine/Marketplace, Saved/VibeUE"));
    }
}

void FMCPClient::Shutdown()
{
    // Stop all servers
    TArray<FString> ServerNames;
    ServerStates.GetKeys(ServerNames);
    
    for (const FString& ServerName : ServerNames)
    {
        StopServer(ServerName);
    }
    
    ServerStates.Empty();
    AllTools.Empty();
    
    UE_LOG(LogMCPClient, Log, TEXT("MCP Client shutdown"));
}

bool FMCPClient::LoadConfiguration(const FString& ConfigPath)
{
    FString JsonContent;
    if (!FFileHelper::LoadFileToString(JsonContent, *ConfigPath))
    {
        UE_LOG(LogMCPClient, Error, TEXT("Failed to load MCP config from %s"), *ConfigPath);
        return false;
    }
    
    Configuration = FMCPConfiguration::FromJsonString(JsonContent);
    
    UE_LOG(LogMCPClient, Log, TEXT("Loaded %d MCP server configurations"), Configuration.Servers.Num());
    
    for (const auto& Pair : Configuration.Servers)
    {
        UE_LOG(LogMCPClient, Log, TEXT("  - %s (%s): %s"), *Pair.Key, *Pair.Value.Type, *Pair.Value.Command);
    }
    
    return true;
}

bool FMCPClient::FindVibeUEInMarketplace(FString& OutFolderName)
{
    FString EngineMarketplacePath = FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir() / TEXT("Marketplace"));
    
    if (!FPaths::DirectoryExists(EngineMarketplacePath))
    {
        return false;
    }
    
    IFileManager& FileManager = IFileManager::Get();
    TArray<FString> Directories;
    FileManager.FindFiles(Directories, *(EngineMarketplacePath / TEXT("*")), false, true);
    
    for (const FString& DirName : Directories)
    {
        FString FullPath = EngineMarketplacePath / DirName;
        // Look for the vibe_ue_server.py file which uniquely identifies VibeUE
        FString ServerPyPath = FullPath / TEXT("Content") / TEXT("Python") / TEXT("vibe_ue_server.py");
        if (FPaths::FileExists(ServerPyPath))
        {
            OutFolderName = DirName;
            return true;
        }
    }
    
    return false;
}

FString FMCPClient::GetEngineVibeUEPythonPath()
{
    FString FolderName;
    if (FindVibeUEInMarketplace(FolderName))
    {
        FString EngineMarketplacePath = FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir() / TEXT("Marketplace"));
        return EngineMarketplacePath / FolderName / TEXT("Content") / TEXT("Python");
    }
    return FString();
}

bool FMCPClient::IsLocalModeAvailable()
{
    FString LocalPath = FPaths::ProjectPluginsDir() / TEXT("VibeUE") / TEXT("Content") / TEXT("Python") / TEXT("vibe_ue_server.py");
    return FPaths::FileExists(LocalPath);
}

bool FMCPClient::IsEngineModeAvailable()
{
    FString FolderName;
    return FindVibeUEInMarketplace(FolderName);
}

bool FMCPClient::DetermineDefaultMode(bool& bHasSavedPreference, bool& bSavedEngineMode)
{
    // Check for saved preference
    bHasSavedPreference = GConfig->GetBool(TEXT("VibeUE"), TEXT("MCPEngineMode"), bSavedEngineMode, GEditorPerProjectIni);
    
    bool bLocalAvailable = IsLocalModeAvailable();
    bool bEngineAvailable = IsEngineModeAvailable();
    
    UE_LOG(LogMCPClient, Log, TEXT("Mode detection - Local available: %s, Engine available: %s, Has saved preference: %s"),
        bLocalAvailable ? TEXT("Yes") : TEXT("No"),
        bEngineAvailable ? TEXT("Yes") : TEXT("No"),
        bHasSavedPreference ? TEXT("Yes") : TEXT("No"));
    
    // If user has a saved preference and that mode is available, use it
    if (bHasSavedPreference)
    {
        if (bSavedEngineMode && bEngineAvailable)
        {
            UE_LOG(LogMCPClient, Log, TEXT("Using saved preference: Engine mode"));
            return true; // Engine mode
        }
        else if (!bSavedEngineMode && bLocalAvailable)
        {
            UE_LOG(LogMCPClient, Log, TEXT("Using saved preference: Local mode"));
            return false; // Local mode
        }
        // Saved preference mode not available, fall through to auto-detect
        UE_LOG(LogMCPClient, Warning, TEXT("Saved preference mode not available, auto-detecting..."));
    }
    
    // Auto-detect: prefer Local mode if available, otherwise Engine mode
    if (bLocalAvailable)
    {
        UE_LOG(LogMCPClient, Log, TEXT("Auto-selected: Local mode"));
        return false; // Local mode
    }
    else if (bEngineAvailable)
    {
        UE_LOG(LogMCPClient, Log, TEXT("Auto-selected: Engine mode"));
        return true; // Engine mode
    }
    
    // Neither available - default to Local mode (will show error later)
    UE_LOG(LogMCPClient, Warning, TEXT("No VibeUE installation found! Defaulting to Local mode."));
    return false;
}

FString FMCPClient::ResolveConfigVariables(const FString& Input) const
{
    FString Result = Input;
    
    // Replace ${VibeUE_Instance} based on mode setting
    // - Local mode: Forces use of Project/Plugins/VibeUE (for development)
    // - Engine mode: Scans Engine/Plugins/Marketplace for VibeUE plugin (FAB installs use random folder names)
    FString VibeUEInstance;
    FString VibeUEPluginFolder;
    
    FString ProjectPluginPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());
    FString EngineMarketplacePath = FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir() / TEXT("Marketplace"));
    
    if (bEngineMode)
    {
        // Engine mode: Use the static helper to find VibeUE in marketplace
        FString FoundFolder;
        if (FindVibeUEInMarketplace(FoundFolder))
        {
            VibeUEInstance = EngineMarketplacePath;
            VibeUEPluginFolder = FoundFolder;
            UE_LOG(LogMCPClient, Log, TEXT("Engine Mode - found VibeUE plugin in: %s/%s"), *EngineMarketplacePath, *FoundFolder);
        }
        else
        {
            // Fallback: use the path anyway, user may need to install
            VibeUEInstance = EngineMarketplacePath;
            VibeUEPluginFolder = TEXT("VibeUE");
            UE_LOG(LogMCPClient, Warning, TEXT("Engine Mode enabled but VibeUE not found in %s. Please install VibeUE from FAB."), *EngineMarketplacePath);
        }
    }
    else
    {
        // Local mode: Use project plugins path (for development)
        VibeUEInstance = ProjectPluginPath;
        VibeUEPluginFolder = TEXT("VibeUE");
        UE_LOG(LogMCPClient, Log, TEXT("Local Mode - using project plugins path: %s/VibeUE"), *VibeUEInstance);
        
        // Warn if the plugin doesn't exist there
        if (!FPaths::DirectoryExists(ProjectPluginPath / TEXT("VibeUE")))
        {
            UE_LOG(LogMCPClient, Warning, TEXT("Local Mode enabled but VibeUE not found in %s/VibeUE"), *ProjectPluginPath);
        }
    }
    
    // Replace ${VibeUE_Instance} - this is the parent folder containing the VibeUE plugin
    // The config uses ${VibeUE_Instance}/VibeUE/Content/Python, so we need to handle versioned folder names
    Result = Result.Replace(TEXT("${VibeUE_Instance}"), *VibeUEInstance);
    
    // Also replace the hardcoded "VibeUE" folder name with the actual folder name if different
    if (!VibeUEPluginFolder.IsEmpty() && VibeUEPluginFolder != TEXT("VibeUE"))
    {
        Result = Result.Replace(TEXT("/VibeUE/"), *(TEXT("/") + VibeUEPluginFolder + TEXT("/")));
        Result = Result.Replace(TEXT("\\VibeUE\\"), *(TEXT("\\") + VibeUEPluginFolder + TEXT("\\")));
    }
    
    // Legacy support: Replace ${workspaceFolder} with project directory
    Result = Result.Replace(TEXT("${workspaceFolder}"), *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    
    // Replace ${ProjectDir} with project directory
    Result = Result.Replace(TEXT("${ProjectDir}"), *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    
    // Replace ${EngineDir} with engine directory
    Result = Result.Replace(TEXT("${EngineDir}"), *FPaths::ConvertRelativePathToFull(FPaths::EngineDir()));
    
    UE_LOG(LogMCPClient, Log, TEXT("Resolved path: %s -> %s"), *Input, *Result);
    
    return Result;
}

bool FMCPClient::StartServer(const FString& ServerName)
{
    // Check if already running
    if (TSharedPtr<FMCPServerState>* ExistingState = ServerStates.Find(ServerName))
    {
        if ((*ExistingState)->bInitialized)
        {
            UE_LOG(LogMCPClient, Log, TEXT("Server %s is already running"), *ServerName);
            return true;
        }
    }
    
    // Find server config
    FMCPServerConfig* Config = Configuration.Servers.Find(ServerName);
    if (!Config)
    {
        UE_LOG(LogMCPClient, Error, TEXT("Server %s not found in configuration"), *ServerName);
        return false;
    }
    
    if (Config->Type != TEXT("stdio"))
    {
        UE_LOG(LogMCPClient, Error, TEXT("Server %s uses unsupported transport type: %s"), *ServerName, *Config->Type);
        return false;
    }
    
    // Create server state
    TSharedPtr<FMCPServerState> State = MakeShared<FMCPServerState>();
    State->Config = *Config;
    
    // Resolve variables in config
    FString Command = ResolveConfigVariables(Config->Command);
    FString WorkDir = ResolveConfigVariables(Config->WorkingDirectory);
    
    // Build args string
    FString Args;
    for (const FString& Arg : Config->Args)
    {
        FString ResolvedArg = ResolveConfigVariables(Arg);
        Args += FString::Printf(TEXT(" %s"), *ResolvedArg);
    }
    
    // Build environment
    TMap<FString, FString> Environment;
    for (const auto& EnvPair : Config->Environment)
    {
        Environment.Add(EnvPair.Key, ResolveConfigVariables(EnvPair.Value));
    }
    
    // Set environment variables
    for (const auto& EnvPair : Environment)
    {
        FPlatformMisc::SetEnvironmentVar(*EnvPair.Key, *EnvPair.Value);
        UE_LOG(LogMCPClient, Log, TEXT("  Env: %s=%s"), *EnvPair.Key, *EnvPair.Value);
    }
    
    UE_LOG(LogMCPClient, Log, TEXT("Starting MCP server %s: %s%s"), *ServerName, *Command, *Args);
    UE_LOG(LogMCPClient, Log, TEXT("  Working directory: %s"), *WorkDir);
    
    // Create pipe for reading child's stdout (we read locally, child writes)
    // bWritePipeLocal = false means the write end goes to child
    void* StdOutRead = nullptr;
    void* StdOutWrite = nullptr;
    if (!FPlatformProcess::CreatePipe(StdOutRead, StdOutWrite, false))
    {
        UE_LOG(LogMCPClient, Error, TEXT("Failed to create stdout pipe for %s"), *ServerName);
        return false;
    }
    
    // Create pipe for writing to child's stdin (we write locally, child reads)
    // bWritePipeLocal = true means the write end stays local
    void* StdInRead = nullptr;
    void* StdInWrite = nullptr;
    if (!FPlatformProcess::CreatePipe(StdInRead, StdInWrite, true))
    {
        UE_LOG(LogMCPClient, Error, TEXT("Failed to create stdin pipe for %s"), *ServerName);
        FPlatformProcess::ClosePipe(StdOutRead, StdOutWrite);
        return false;
    }
    
    // Launch the process
    // CreateProc signature: CreateProc(URL, Parms, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, OutProcessID, PriorityModifier, OptionalWorkingDirectory, PipeWriteChild, PipeReadChild)
    // PipeWriteChild = child's stdout goes here (we read from StdOutRead)
    // PipeReadChild = child reads from here (we write to StdInWrite)
    FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
        *Command,
        *Args,
        false,  // bLaunchDetached
        true,   // bLaunchHidden - hide console window
        true,   // bLaunchReallyHidden
        nullptr, // OutProcessID
        0,      // PriorityModifier
        WorkDir.IsEmpty() ? nullptr : *WorkDir,
        StdOutWrite,  // Child writes stdout here
        StdInRead     // Child reads stdin from here
    );
    
    if (!ProcessHandle.IsValid())
    {
#if PLATFORM_WINDOWS
        DWORD LastError = GetLastError();
        UE_LOG(LogMCPClient, Error, TEXT("Failed to launch MCP server %s (Command: %s) - Windows Error: 0x%08X"), *ServerName, *Command, LastError);
#else
        UE_LOG(LogMCPClient, Error, TEXT("Failed to launch MCP server %s (Command: %s)"), *ServerName, *Command);
#endif
        FPlatformProcess::ClosePipe(StdOutRead, StdOutWrite);
        FPlatformProcess::ClosePipe(StdInRead, StdInWrite);
        return false;
    }
    
    // CRITICAL: Close the child-side handles in the parent process!
    // The child has inherited copies of these handles.
    // If we keep them open, the pipes won't properly signal EOF when the child exits.
#if PLATFORM_WINDOWS
    if (StdOutWrite)
    {
        CloseHandle(StdOutWrite);
        StdOutWrite = nullptr;
    }
    if (StdInRead)
    {
        CloseHandle(StdInRead);
        StdInRead = nullptr;
    }
#endif
    
    State->ProcessHandle = ProcessHandle;
    State->ReadPipe = StdOutRead;   // We read from here (child's stdout)
    State->WritePipe = StdInWrite;  // We write to here (child's stdin)
    
    ServerStates.Add(ServerName, State);
    
    // Wait a moment for server to start
    FPlatformProcess::Sleep(1.0f);
    
    // Check if process is still running
    if (!FPlatformProcess::IsProcRunning(ProcessHandle))
    {
        int32 ReturnCode = -1;
        FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);
        UE_LOG(LogMCPClient, Error, TEXT("MCP server %s exited immediately after launch (Return Code: %d)"), *ServerName, ReturnCode);
        
        // Try to read any output from stdout
        if (State->ReadPipe)
        {
            FString Output = FPlatformProcess::ReadPipe(State->ReadPipe);
            if (!Output.IsEmpty())
            {
                UE_LOG(LogMCPClient, Error, TEXT("Server stdout: %s"), *Output);
            }
        }
        
        ServerStates.Remove(ServerName);
        return false;
    }
    
    UE_LOG(LogMCPClient, Log, TEXT("MCP server %s process started, beginning initialization..."), *ServerName);
    
    // Initialize the server (MCP handshake)
    if (!InitializeServer(*State))
    {
        UE_LOG(LogMCPClient, Error, TEXT("Failed to initialize MCP server %s"), *ServerName);
        StopServer(ServerName);
        return false;
    }
    
    UE_LOG(LogMCPClient, Log, TEXT("MCP server %s started and initialized successfully"), *ServerName);
    return true;
}

void FMCPClient::StopServer(const FString& ServerName)
{
    TSharedPtr<FMCPServerState>* StatePtr = ServerStates.Find(ServerName);
    if (!StatePtr || !StatePtr->IsValid())
    {
        return;
    }
    
    TSharedPtr<FMCPServerState> State = *StatePtr;
    
    // Close pipes
    if (State->ReadPipe)
    {
        FPlatformProcess::ClosePipe(State->ReadPipe, nullptr);
        State->ReadPipe = nullptr;
    }
    if (State->WritePipe)
    {
        FPlatformProcess::ClosePipe(nullptr, State->WritePipe);
        State->WritePipe = nullptr;
    }
    
    // Terminate process if still running
    if (State->ProcessHandle.IsValid())
    {
        if (FPlatformProcess::IsProcRunning(State->ProcessHandle))
        {
            FPlatformProcess::TerminateProc(State->ProcessHandle, true);
        }
        FPlatformProcess::CloseProc(State->ProcessHandle);
    }
    
    // Remove tools from this server
    AllTools.RemoveAll([&ServerName](const FMCPTool& Tool) {
        return Tool.ServerName == ServerName;
    });
    
    ServerStates.Remove(ServerName);
    
    UE_LOG(LogMCPClient, Log, TEXT("MCP server %s stopped"), *ServerName);
}

bool FMCPClient::SendRequest(FMCPServerState& State, const FString& Method, TSharedPtr<FJsonObject> Params, TFunction<void(TSharedPtr<FJsonObject>)> OnResponse)
{
    if (!State.WritePipe)
    {
        UE_LOG(LogMCPClient, Error, TEXT("Cannot send request - server pipe not open"));
        return false;
    }
    
    // Check if this is a notification (no id field, no response expected)
    bool bIsNotification = Method.StartsWith(TEXT("notifications/"));
    
    // Build JSON-RPC 2.0 request/notification
    TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
    Request->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
    Request->SetStringField(TEXT("method"), Method);
    
    int32 RequestId = 0;
    if (!bIsNotification)
    {
        RequestId = State.NextRequestId++;
        Request->SetNumberField(TEXT("id"), RequestId);
    }
    
    if (Params.IsValid())
    {
        Request->SetObjectField(TEXT("params"), Params);
    }
    
    // Serialize to compact JSON (MCP requires single-line JSON messages)
    FString RequestJson;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = 
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&RequestJson);
    FJsonSerializer::Serialize(Request.ToSharedRef(), Writer);
    
    // MCP uses JSON-RPC over stdio with newline-delimited messages
    FString Message = RequestJson + TEXT("\n");
    
    UE_LOG(LogMCPClient, Verbose, TEXT("Sending MCP request [%d] method=%s: %s"), RequestId, *Method, *RequestJson.Left(300));
    
    // Write to pipe
    FString MessageUtf8 = Message;
    bool bWritten = FPlatformProcess::WritePipe(State.WritePipe, MessageUtf8);
    
    if (!bWritten)
    {
#if PLATFORM_WINDOWS
        DWORD LastError = GetLastError();
        UE_LOG(LogMCPClient, Error, TEXT("Failed to write to MCP server pipe - Windows Error: 0x%08X"), LastError);
#else
        UE_LOG(LogMCPClient, Error, TEXT("Failed to write to MCP server pipe"));
#endif
        return false;
    }
    
    // Store callback if provided (only for non-notifications)
    if (OnResponse && !bIsNotification)
    {
        State.PendingRequests.Add(RequestId, OnResponse);
    }
    
    return true;
}

bool FMCPClient::ReadResponse(FMCPServerState& State, FString& OutResponse)
{
    if (!State.ReadPipe)
    {
        return false;
    }
    
    // Try to read from pipe
    OutResponse = FPlatformProcess::ReadPipe(State.ReadPipe);
    
    return !OutResponse.IsEmpty();
}

void FMCPClient::ProcessResponse(FMCPServerState& State, const FString& ResponseJson)
{
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);
    TSharedPtr<FJsonObject> Response;
    
    if (!FJsonSerializer::Deserialize(Reader, Response) || !Response.IsValid())
    {
        UE_LOG(LogMCPClient, Warning, TEXT("Failed to parse MCP response: %s"), *ResponseJson);
        return;
    }
    
    // Check for request ID
    int32 RequestId = 0;
    if (Response->TryGetNumberField(TEXT("id"), RequestId))
    {
        UE_LOG(LogMCPClient, Log, TEXT("Processing response for RequestId=%d"), RequestId);
        
        // Find pending callback
        TFunction<void(TSharedPtr<FJsonObject>)>* Callback = State.PendingRequests.Find(RequestId);
        if (Callback && *Callback)
        {
            UE_LOG(LogMCPClient, Log, TEXT("Found callback for RequestId=%d, invoking..."), RequestId);
            (*Callback)(Response);
            State.PendingRequests.Remove(RequestId);
        }
        else
        {
            UE_LOG(LogMCPClient, Warning, TEXT("No callback found for RequestId=%d (pending count=%d)"), RequestId, State.PendingRequests.Num());
        }
    }
    else
    {
        UE_LOG(LogMCPClient, Verbose, TEXT("Response has no ID field (notification or malformed): %s"), *ResponseJson.Left(200));
    }
    
    // Check for error
    const TSharedPtr<FJsonObject>* ErrorObj;
    if (Response->TryGetObjectField(TEXT("error"), ErrorObj))
    {
        FString ErrorMessage;
        (*ErrorObj)->TryGetStringField(TEXT("message"), ErrorMessage);
        UE_LOG(LogMCPClient, Error, TEXT("MCP error response: %s"), *ErrorMessage);
    }
}

bool FMCPClient::InitializeServer(FMCPServerState& State)
{
    // Build initialize params
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("protocolVersion"), TEXT("2024-11-05"));
    
    TSharedPtr<FJsonObject> ClientInfo = MakeShared<FJsonObject>();
    ClientInfo->SetStringField(TEXT("name"), TEXT("VibeUE"));
    ClientInfo->SetStringField(TEXT("version"), TEXT("1.0.0"));
    Params->SetObjectField(TEXT("clientInfo"), ClientInfo);
    
    TSharedPtr<FJsonObject> Capabilities = MakeShared<FJsonObject>();
    Params->SetObjectField(TEXT("capabilities"), Capabilities);
    
    // Send initialize request
    if (!SendRequest(State, TEXT("initialize"), Params))
    {
        UE_LOG(LogMCPClient, Error, TEXT("Failed to send initialize request"));
        return false;
    }
    
    // Wait for response (with timeout)
    double StartTime = FPlatformTime::Seconds();
    double Timeout = 10.0; // 10 second timeout
    
    FString Response;
    while (FPlatformTime::Seconds() - StartTime < Timeout)
    {
        if (ReadResponse(State, Response))
        {
            // Parse response lines (may have multiple JSON objects)
            TArray<FString> Lines;
            Response.ParseIntoArray(Lines, TEXT("\n"));
            
            for (const FString& Line : Lines)
            {
                if (Line.IsEmpty()) continue;
                
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
                TSharedPtr<FJsonObject> JsonResponse;
                
                if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
                {
                    // Check for result (successful init)
                    const TSharedPtr<FJsonObject>* ResultObj;
                    if (JsonResponse->TryGetObjectField(TEXT("result"), ResultObj))
                    {
                        UE_LOG(LogMCPClient, Log, TEXT("MCP server initialized successfully"));
                        
                        // Send initialized notification
                        SendRequest(State, TEXT("notifications/initialized"));
                        
                        State.bInitialized = true;
                        
                        // Request tools list
                        RequestToolsList(State);
                        
                        return true;
                    }
                    
                    // Check for error
                    const TSharedPtr<FJsonObject>* ErrorObj;
                    if (JsonResponse->TryGetObjectField(TEXT("error"), ErrorObj))
                    {
                        FString ErrorMessage;
                        (*ErrorObj)->TryGetStringField(TEXT("message"), ErrorMessage);
                        UE_LOG(LogMCPClient, Error, TEXT("MCP initialize error: %s"), *ErrorMessage);
                        return false;
                    }
                }
            }
        }
        
        FPlatformProcess::Sleep(0.1f);
    }
    
    UE_LOG(LogMCPClient, Error, TEXT("MCP initialize timed out"));
    return false;
}

void FMCPClient::RequestToolsList(FMCPServerState& State)
{
    // MCP tools/list requires empty params object
    TSharedPtr<FJsonObject> EmptyParams = MakeShared<FJsonObject>();
    
    SendRequest(State, TEXT("tools/list"), EmptyParams, [this, &State](TSharedPtr<FJsonObject> Response)
    {
        const TSharedPtr<FJsonObject>* ResultObj;
        if (Response->TryGetObjectField(TEXT("result"), ResultObj))
        {
            const TArray<TSharedPtr<FJsonValue>>* ToolsArray;
            if ((*ResultObj)->TryGetArrayField(TEXT("tools"), ToolsArray))
            {
                for (const auto& ToolValue : *ToolsArray)
                {
                    TSharedPtr<FJsonObject>* ToolObj = nullptr;
                    const TSharedPtr<FJsonObject>& ToolObjRef = ToolValue->AsObject();
                    if (ToolObjRef.IsValid())
                    {
                        FMCPTool Tool = FMCPTool::FromJson(ToolObjRef, State.Config.Name);
                        State.Tools.Add(Tool);
                        AllTools.Add(Tool);
                        
                        UE_LOG(LogMCPClient, Log, TEXT("Discovered tool: %s from %s"), *Tool.Name, *State.Config.Name);
                    }
                }
            }
        }
    });
    
    // Wait for tools response
    double StartTime = FPlatformTime::Seconds();
    double Timeout = 5.0;
    
    FString Response;
    while (FPlatformTime::Seconds() - StartTime < Timeout)
    {
        if (ReadResponse(State, Response))
        {
            TArray<FString> Lines;
            Response.ParseIntoArray(Lines, TEXT("\n"));
            
            for (const FString& Line : Lines)
            {
                if (!Line.IsEmpty())
                {
                    ProcessResponse(State, Line);
                }
            }
            
            // Check if we got tools
            if (State.Tools.Num() > 0)
            {
                break;
            }
        }
        
        FPlatformProcess::Sleep(0.1f);
    }
    
    UE_LOG(LogMCPClient, Log, TEXT("Discovered %d tools from %s"), State.Tools.Num(), *State.Config.Name);
}

void FMCPClient::DiscoverTools(FOnToolsDiscovered OnComplete)
{
    // Start all configured servers
    for (const auto& Pair : Configuration.Servers)
    {
        if (Pair.Value.bEnabled)
        {
            StartServer(Pair.Key);
        }
    }
    
    // Return all discovered tools
    OnComplete.ExecuteIfBound(AllTools.Num() > 0, AllTools);
}

void FMCPClient::ExecuteTool(const FMCPToolCall& ToolCall, FOnToolExecuted OnComplete)
{
    // Find which server provides this tool
    FMCPServerState* State = FindServerForTool(ToolCall.ToolName);
    if (!State)
    {
        FMCPToolResult Result;
        Result.ToolCallId = ToolCall.Id;
        Result.bSuccess = false;
        Result.ErrorMessage = FString::Printf(TEXT("Tool '%s' not found"), *ToolCall.ToolName);
        OnComplete.ExecuteIfBound(false, Result);
        return;
    }
    
    // Build call params
    TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
    Params->SetStringField(TEXT("name"), ToolCall.ToolName);
    
    if (ToolCall.Arguments.IsValid())
    {
        Params->SetObjectField(TEXT("arguments"), ToolCall.Arguments);
    }
    else
    {
        Params->SetObjectField(TEXT("arguments"), MakeShared<FJsonObject>());
    }
    
    // Capture the request ID before sending (it will be incremented in SendRequest)
    int32 RequestId = State->NextRequestId;
    
    UE_LOG(LogMCPClient, Log, TEXT("Executing tool: %s (RequestId=%d)"), *ToolCall.ToolName, RequestId);
    
    // Variable to track if callback was called (atomic for thread safety)
    TSharedPtr<FThreadSafeBool> bCallbackCalled = MakeShared<FThreadSafeBool>(false);
    
    // Send tools/call request with callback
    SendRequest(*State, TEXT("tools/call"), Params, [this, ToolCall, OnComplete, bCallbackCalled](TSharedPtr<FJsonObject> Response)
    {
        if (*bCallbackCalled)
        {
            return; // Already called (e.g., from timeout)
        }
        *bCallbackCalled = true;
        
        FMCPToolResult Result;
        Result.ToolCallId = ToolCall.Id;
        
        const TSharedPtr<FJsonObject>* ResultObj;
        if (Response->TryGetObjectField(TEXT("result"), ResultObj))
        {
            // Get content from result
            const TArray<TSharedPtr<FJsonValue>>* ContentArray;
            if ((*ResultObj)->TryGetArrayField(TEXT("content"), ContentArray))
            {
                for (const auto& ContentValue : *ContentArray)
                {
                    const TSharedPtr<FJsonObject>& ContentObj = ContentValue->AsObject();
                    if (ContentObj.IsValid())
                    {
                        FString ContentType;
                        ContentObj->TryGetStringField(TEXT("type"), ContentType);
                        
                        if (ContentType == TEXT("text"))
                        {
                            FString Text;
                            ContentObj->TryGetStringField(TEXT("text"), Text);
                            Result.Content += Text;
                        }
                    }
                }
            }
            
            // Check for isError flag
            bool bIsError = false;
            (*ResultObj)->TryGetBoolField(TEXT("isError"), bIsError);
            
            Result.bSuccess = !bIsError;
            if (bIsError)
            {
                Result.ErrorMessage = Result.Content;
            }
        }
        else
        {
            // Check for error
            const TSharedPtr<FJsonObject>* ErrorObj;
            if (Response->TryGetObjectField(TEXT("error"), ErrorObj))
            {
                Result.bSuccess = false;
                (*ErrorObj)->TryGetStringField(TEXT("message"), Result.ErrorMessage);
            }
        }
        
        UE_LOG(LogMCPClient, Log, TEXT("Tool %s completed, success=%d"), *ToolCall.ToolName, Result.bSuccess);
        
        // Execute callback on game thread
        AsyncTask(ENamedThreads::GameThread, [OnComplete, Result]()
        {
            OnComplete.ExecuteIfBound(Result.bSuccess, Result);
        });
    });
    
    // Capture weak reference for async task
    TWeakPtr<FMCPClient> WeakClient = AsShared();
    FString ServerName = State->Config.Command; // Capture server identifier
    
    UE_LOG(LogMCPClient, Log, TEXT("Starting async polling for tool response (RequestId=%d)..."), RequestId);
    
    // Run polling on a background thread to avoid blocking game thread
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakClient, ToolCall, OnComplete, RequestId, bCallbackCalled, ServerName]()
    {
        double StartTime = FPlatformTime::Seconds();
        double Timeout = 30.0; // 30 second timeout for tool execution
        
        while (FPlatformTime::Seconds() - StartTime < Timeout)
        {
            // Check if callback was already called
            if (*bCallbackCalled)
            {
                UE_LOG(LogMCPClient, Log, TEXT("Tool request %d completed (callback called)"), RequestId);
                return;
            }
            
            // Check if client is still valid
            TSharedPtr<FMCPClient> Client = WeakClient.Pin();
            if (!Client.IsValid())
            {
                UE_LOG(LogMCPClient, Warning, TEXT("MCP client destroyed while waiting for tool response"));
                return;
            }
            
            // Find the server state again (in case it changed)
            FMCPServerState* State = Client->FindServerForTool(ToolCall.ToolName);
            if (!State)
            {
                UE_LOG(LogMCPClient, Warning, TEXT("Server state not found while polling"));
                break;
            }
            
            // Read response from pipe
            FString Response;
            if (Client->ReadResponse(*State, Response))
            {
                UE_LOG(LogMCPClient, Verbose, TEXT("Received response data: %s"), *Response.Left(500));
                
                TArray<FString> Lines;
                Response.ParseIntoArray(Lines, TEXT("\n"));
                
                for (const FString& Line : Lines)
                {
                    if (!Line.IsEmpty())
                    {
                        UE_LOG(LogMCPClient, Verbose, TEXT("Processing line: %s"), *Line.Left(300));
                        Client->ProcessResponse(*State, Line);
                    }
                }
                
                // Check if callback was invoked (request removed from pending)
                if (!State->PendingRequests.Contains(RequestId))
                {
                    UE_LOG(LogMCPClient, Log, TEXT("Tool request %d completed (pending request removed)"), RequestId);
                    return;
                }
            }
            
            FPlatformProcess::Sleep(0.05f);
        }
        
        // Timeout - call callback with error if not already called
        if (!*bCallbackCalled)
        {
            *bCallbackCalled = true;
            
            UE_LOG(LogMCPClient, Error, TEXT("Tool execution timed out after %.1f seconds (RequestId=%d)"), Timeout, RequestId);
            
            FMCPToolResult Result;
            Result.ToolCallId = ToolCall.Id;
            Result.bSuccess = false;
            Result.ErrorMessage = TEXT("Tool execution timed out");
            
            // Execute callback on game thread
            AsyncTask(ENamedThreads::GameThread, [OnComplete, Result]()
            {
                OnComplete.ExecuteIfBound(Result.bSuccess, Result);
            });
        }
    });
}

FMCPServerState* FMCPClient::FindServerForTool(const FString& ToolName)
{
    for (auto& Pair : ServerStates)
    {
        FMCPServerState& State = *Pair.Value;
        for (const FMCPTool& Tool : State.Tools)
        {
            if (Tool.Name == ToolName)
            {
                return &State;
            }
        }
    }
    return nullptr;
}

int32 FMCPClient::GetConnectedServerCount() const
{
    int32 Count = 0;
    for (const auto& Pair : ServerStates)
    {
        if (Pair.Value->bInitialized)
        {
            Count++;
        }
    }
    return Count;
}

bool FMCPClient::IsServerConnected(const FString& ServerName) const
{
    const TSharedPtr<FMCPServerState>* StatePtr = ServerStates.Find(ServerName);
    return StatePtr && (*StatePtr)->bInitialized;
}
