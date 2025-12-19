// Copyright 2025 Vibe AI. All Rights Reserved.

#include "MCP/MCPServer.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Guid.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Core/ToolRegistry.h"
#include "Chat/MCPTypes.h"
#include "Async/Async.h"

DEFINE_LOG_CATEGORY(LogMCPServer);

TSharedPtr<FMCPServer> FMCPServer::Instance;

// MCP Protocol version we support
static const FString MCP_PROTOCOL_VERSION = TEXT("2025-06-18");
static const FString MCP_SERVER_NAME = TEXT("VibeUE");
static const FString MCP_SERVER_VERSION = TEXT("1.0.0");

FMCPServer::FMCPServer()
{
}

FMCPServer::~FMCPServer()
{
    Shutdown();
}

FMCPServer& FMCPServer::Get()
{
    if (!Instance.IsValid())
    {
        Instance = MakeShared<FMCPServer>();
    }
    return *Instance;
}

void FMCPServer::Initialize()
{
    LoadConfig();
    
    UE_LOG(LogMCPServer, Log, TEXT("MCP Server initialized - Enabled: %s, Port: %d, API Key: %s"),
        Config.bEnabled ? TEXT("Yes") : TEXT("No"),
        Config.Port,
        Config.ApiKey.IsEmpty() ? TEXT("(none)") : TEXT("(set)"));
    
    // Auto-start if enabled
    if (Config.bEnabled)
    {
        Start();
    }
}

void FMCPServer::Shutdown()
{
    StopServer();
    UE_LOG(LogMCPServer, Log, TEXT("MCP Server shutdown"));
}

bool FMCPServer::Start()
{
    if (bIsRunning)
    {
        UE_LOG(LogMCPServer, Warning, TEXT("MCP Server already running"));
        return true;
    }
    
    UE_LOG(LogMCPServer, Log, TEXT("Starting MCP Server on port %d..."), Config.Port);
    
    // Create TCP listener socket
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogMCPServer, Error, TEXT("Failed to get socket subsystem"));
        return false;
    }
    
    // Bind to localhost only for security (prevent DNS rebinding attacks)
    FIPv4Address LocalAddress;
    FIPv4Address::Parse(TEXT("127.0.0.1"), LocalAddress);
    
    TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
    Addr->SetIp(LocalAddress.Value);
    Addr->SetPort(Config.Port);
    
    ListenerSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("MCP Server"), false);
    if (!ListenerSocket)
    {
        UE_LOG(LogMCPServer, Error, TEXT("Failed to create listener socket"));
        return false;
    }
    
    // Set socket options
    ListenerSocket->SetReuseAddr(true);
    ListenerSocket->SetNonBlocking(true);
    
    // Bind and listen
    if (!ListenerSocket->Bind(*Addr))
    {
        UE_LOG(LogMCPServer, Error, TEXT("Failed to bind to port %d - is another process using it?"), Config.Port);
        SocketSubsystem->DestroySocket(ListenerSocket);
        ListenerSocket = nullptr;
        return false;
    }
    
    if (!ListenerSocket->Listen(8))
    {
        UE_LOG(LogMCPServer, Error, TEXT("Failed to listen on socket"));
        SocketSubsystem->DestroySocket(ListenerSocket);
        ListenerSocket = nullptr;
        return false;
    }
    
    bIsRunning = true;
    bShouldStop = false;
    
    // Start server thread
    ServerThread = FRunnableThread::Create(this, TEXT("MCPServerThread"), 0, TPri_Normal);
    if (!ServerThread)
    {
        UE_LOG(LogMCPServer, Error, TEXT("Failed to create server thread"));
        SocketSubsystem->DestroySocket(ListenerSocket);
        ListenerSocket = nullptr;
        bIsRunning = false;
        return false;
    }
    
    // Register tick delegate for processing requests on game thread
    TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda([this](float DeltaTime) -> bool
        {
            ProcessPendingRequests();
            return true; // Keep ticking
        }),
        0.016f // ~60 Hz
    );
    
    UE_LOG(LogMCPServer, Log, TEXT("MCP Server started at %s"), *GetServerUrl());
    
    return true;
}

void FMCPServer::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }
    
    UE_LOG(LogMCPServer, Log, TEXT("Stopping MCP Server..."));
    
    bShouldStop = true;
    bIsRunning = false;
    
    // Remove tick delegate
    if (TickDelegateHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
        TickDelegateHandle.Reset();
    }
    
    // Close listener socket FIRST to unblock the WaitForPendingConnection call in the thread
    if (ListenerSocket)
    {
        ListenerSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket);
        ListenerSocket = nullptr;
    }
    
    // Now wait for thread to finish (it should exit quickly since socket is closed)
    if (ServerThread)
    {
        ServerThread->WaitForCompletion();
        delete ServerThread;
        ServerThread = nullptr;
    }
    
    // Clear sessions
    FScopeLock Lock(&SessionLock);
    ActiveSessions.Empty();
    
    UE_LOG(LogMCPServer, Log, TEXT("MCP Server stopped"));
}

FString FMCPServer::GetServerUrl() const
{
    return FString::Printf(TEXT("http://127.0.0.1:%d/mcp"), Config.Port);
}

// ============ FRunnable Interface ============

bool FMCPServer::Init()
{
    return true;
}

uint32 FMCPServer::Run()
{
    UE_LOG(LogMCPServer, Log, TEXT("MCP Server thread started"));
    
    while (!bShouldStop)
    {
        // Check if socket is still valid (may have been closed during shutdown)
        if (!ListenerSocket)
        {
            break;
        }
        
        bool bHasPendingConnection = false;
        if (ListenerSocket->WaitForPendingConnection(bHasPendingConnection, FTimespan::FromMilliseconds(100)))
        {
            if (bHasPendingConnection && !bShouldStop)
            {
                FSocket* ClientSocket = ListenerSocket->Accept(TEXT("MCP Client"));
                if (ClientSocket)
                {
                    HandleConnection(ClientSocket);
                }
            }
        }
        
        // Small sleep to prevent busy-waiting
        FPlatformProcess::Sleep(0.001f);
    }
    
    UE_LOG(LogMCPServer, Log, TEXT("MCP Server thread exiting"));
    return 0;
}

void FMCPServer::Exit()
{
    // Nothing to cleanup here - Stop() handles it
}

// ============ HTTP Handling ============

bool FMCPServer::HandleConnection(FSocket* ClientSocket)
{
    if (!ClientSocket)
    {
        return false;
    }
    
    FString Method, Path;
    TMap<FString, FString> Headers;
    FString Body;
    
    if (!ParseHttpRequest(ClientSocket, Method, Path, Headers, Body))
    {
        SendHttpResponse(ClientSocket, 400, TEXT("Bad Request"), TEXT("text/plain"), TEXT("Invalid HTTP request"));
        ClientSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
        return false;
    }
    
    UE_LOG(LogMCPServer, Verbose, TEXT("MCP Request: %s %s"), *Method, *Path);
    
    // Only handle /mcp endpoint
    if (!Path.StartsWith(TEXT("/mcp")))
    {
        SendHttpResponse(ClientSocket, 404, TEXT("Not Found"), TEXT("text/plain"), TEXT("Not Found"));
        ClientSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
        return false;
    }
    
    // Validate Origin header for security
    if (!ValidateOrigin(Headers))
    {
        SendHttpResponse(ClientSocket, 403, TEXT("Forbidden"), TEXT("text/plain"), TEXT("Invalid Origin"));
        ClientSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
        return false;
    }
    
    // Validate API key if configured
    if (!ValidateApiKey(Headers))
    {
        SendHttpResponse(ClientSocket, 401, TEXT("Unauthorized"), TEXT("text/plain"), TEXT("Invalid or missing API key"));
        ClientSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
        return false;
    }
    
    // Handle different HTTP methods per MCP spec
    if (Method == TEXT("POST"))
    {
        // Get session ID from header
        FString SessionId = Headers.FindRef(TEXT("mcp-session-id"));
        
        // Process JSON-RPC request
        bool bIsNotification = false;
        FString Response = HandleMCPRequest(Body, SessionId, bIsNotification);
        
        if (bIsNotification)
        {
            // Notifications return 202 Accepted with no body
            SendHttpResponse(ClientSocket, 202, TEXT("Accepted"), TEXT(""), TEXT(""));
        }
        else
        {
            // Build response headers
            TMap<FString, FString> ResponseHeaders;
            
            // If this was an initialize request and we generated a session ID, include it
            if (!SessionId.IsEmpty())
            {
                ResponseHeaders.Add(TEXT("Mcp-Session-Id"), SessionId);
            }
            
            // Add CORS headers
            ResponseHeaders.Add(TEXT("Access-Control-Allow-Origin"), TEXT("*"));
            ResponseHeaders.Add(TEXT("Access-Control-Allow-Headers"), TEXT("Content-Type, Authorization, Mcp-Session-Id, MCP-Protocol-Version"));
            
            SendHttpResponse(ClientSocket, 200, TEXT("OK"), TEXT("application/json"), Response, ResponseHeaders);
        }
    }
    else if (Method == TEXT("GET"))
    {
        // GET can be used to open SSE stream - for now we return 405
        SendHttpResponse(ClientSocket, 405, TEXT("Method Not Allowed"), TEXT("text/plain"), 
            TEXT("SSE streams not yet supported. Use POST for requests."));
    }
    else if (Method == TEXT("DELETE"))
    {
        // Session termination
        FString SessionId = Headers.FindRef(TEXT("mcp-session-id"));
        if (!SessionId.IsEmpty())
        {
            FScopeLock Lock(&SessionLock);
            ActiveSessions.Remove(SessionId);
            UE_LOG(LogMCPServer, Log, TEXT("Session terminated: %s"), *SessionId);
        }
        SendHttpResponse(ClientSocket, 200, TEXT("OK"), TEXT("text/plain"), TEXT("Session terminated"));
    }
    else if (Method == TEXT("OPTIONS"))
    {
        // CORS preflight
        TMap<FString, FString> CorsHeaders;
        CorsHeaders.Add(TEXT("Access-Control-Allow-Origin"), TEXT("*"));
        CorsHeaders.Add(TEXT("Access-Control-Allow-Methods"), TEXT("GET, POST, DELETE, OPTIONS"));
        CorsHeaders.Add(TEXT("Access-Control-Allow-Headers"), TEXT("Content-Type, Authorization, Mcp-Session-Id, MCP-Protocol-Version, Accept"));
        CorsHeaders.Add(TEXT("Access-Control-Max-Age"), TEXT("86400"));
        SendHttpResponse(ClientSocket, 204, TEXT("No Content"), TEXT(""), TEXT(""), CorsHeaders);
    }
    else
    {
        SendHttpResponse(ClientSocket, 405, TEXT("Method Not Allowed"), TEXT("text/plain"), TEXT("Method not allowed"));
    }
    
    ClientSocket->Close();
    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
    return true;
}

bool FMCPServer::ParseHttpRequest(FSocket* Socket, FString& OutMethod, FString& OutPath,
                                   TMap<FString, FString>& OutHeaders, FString& OutBody)
{
    if (!Socket)
    {
        return false;
    }
    
    // Read request with timeout
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(8192);
    
    int32 BytesRead = 0;
    FString RequestData;
    
    // Wait for data with timeout
    Socket->SetNonBlocking(false);
    Socket->SetReceiveBufferSize(8192, BytesRead);
    
    // Read in chunks until we have the full request
    int32 TotalRead = 0;
    const int32 MaxSize = 1024 * 1024; // 1MB max request
    
    while (TotalRead < MaxSize)
    {
        Socket->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromSeconds(5.0));
        
        BytesRead = 0;
        if (Socket->Recv(Buffer.GetData(), Buffer.Num() - 1, BytesRead))
        {
            if (BytesRead > 0)
            {
                Buffer[BytesRead] = 0;
                RequestData += UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData()));
                TotalRead += BytesRead;
                
                // Check if we have complete headers
                int32 HeaderEnd = RequestData.Find(TEXT("\r\n\r\n"));
                if (HeaderEnd != INDEX_NONE)
                {
                    // Parse Content-Length to know if we need more body
                    FString HeaderPart = RequestData.Left(HeaderEnd);
                    int32 ContentLength = 0;
                    
                    TArray<FString> HeaderLines;
                    HeaderPart.ParseIntoArray(HeaderLines, TEXT("\r\n"));
                    
                    for (const FString& Line : HeaderLines)
                    {
                        if (Line.StartsWith(TEXT("Content-Length:"), ESearchCase::IgnoreCase))
                        {
                            FString LengthStr = Line.Mid(15).TrimStartAndEnd();
                            ContentLength = FCString::Atoi(*LengthStr);
                            break;
                        }
                    }
                    
                    // Check if we have the full body
                    int32 BodyStart = HeaderEnd + 4;
                    int32 BodyLength = RequestData.Len() - BodyStart;
                    
                    if (BodyLength >= ContentLength)
                    {
                        break; // We have everything
                    }
                }
            }
            else
            {
                // Connection closed or no more data
                break;
            }
        }
        else
        {
            break;
        }
    }
    
    if (RequestData.IsEmpty())
    {
        return false;
    }
    
    // Parse request line
    int32 FirstLineEnd = RequestData.Find(TEXT("\r\n"));
    if (FirstLineEnd == INDEX_NONE)
    {
        return false;
    }
    
    FString RequestLine = RequestData.Left(FirstLineEnd);
    TArray<FString> RequestParts;
    RequestLine.ParseIntoArrayWS(RequestParts);
    
    if (RequestParts.Num() < 2)
    {
        return false;
    }
    
    OutMethod = RequestParts[0];
    OutPath = RequestParts[1];
    
    // Parse headers
    int32 HeaderEnd = RequestData.Find(TEXT("\r\n\r\n"));
    if (HeaderEnd == INDEX_NONE)
    {
        return false;
    }
    
    FString HeaderSection = RequestData.Mid(FirstLineEnd + 2, HeaderEnd - FirstLineEnd - 2);
    TArray<FString> HeaderLines;
    HeaderSection.ParseIntoArray(HeaderLines, TEXT("\r\n"));
    
    for (const FString& Line : HeaderLines)
    {
        int32 ColonPos;
        if (Line.FindChar(':', ColonPos))
        {
            FString Key = Line.Left(ColonPos).TrimStartAndEnd().ToLower();
            FString Value = Line.Mid(ColonPos + 1).TrimStartAndEnd();
            OutHeaders.Add(Key, Value);
        }
    }
    
    // Extract body
    OutBody = RequestData.Mid(HeaderEnd + 4);
    
    return true;
}

void FMCPServer::SendHttpResponse(FSocket* Socket, int32 StatusCode, const FString& StatusText,
                                   const FString& ContentType, const FString& Body,
                                   const TMap<FString, FString>& ExtraHeaders)
{
    if (!Socket)
    {
        return;
    }
    
    FString Response = FString::Printf(TEXT("HTTP/1.1 %d %s\r\n"), StatusCode, *StatusText);
    
    if (!ContentType.IsEmpty())
    {
        Response += FString::Printf(TEXT("Content-Type: %s\r\n"), *ContentType);
    }
    
    Response += FString::Printf(TEXT("Content-Length: %d\r\n"), Body.Len());
    Response += TEXT("Connection: close\r\n");
    
    // Add extra headers
    for (const auto& Header : ExtraHeaders)
    {
        Response += FString::Printf(TEXT("%s: %s\r\n"), *Header.Key, *Header.Value);
    }
    
    Response += TEXT("\r\n");
    Response += Body;
    
    FTCHARToUTF8 Converter(*Response);
    int32 BytesSent = 0;
    Socket->Send(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length(), BytesSent);
}

// ============ MCP Protocol Handling ============

FString FMCPServer::HandleMCPRequest(const FString& JsonBody, const FString& SessionId, bool& bOutIsNotification)
{
    bOutIsNotification = false;
    
    TSharedPtr<FJsonObject> RequestObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
    
    if (!FJsonSerializer::Deserialize(Reader, RequestObj) || !RequestObj.IsValid())
    {
        return BuildJsonRpcError(TEXT(""), -32700, TEXT("Parse error"));
    }
    
    // Check JSON-RPC version
    FString JsonRpc;
    if (!RequestObj->TryGetStringField(TEXT("jsonrpc"), JsonRpc) || JsonRpc != TEXT("2.0"))
    {
        return BuildJsonRpcError(TEXT(""), -32600, TEXT("Invalid Request - missing or invalid jsonrpc version"));
    }
    
    // Get method
    FString Method;
    if (!RequestObj->TryGetStringField(TEXT("method"), Method))
    {
        return BuildJsonRpcError(TEXT(""), -32600, TEXT("Invalid Request - missing method"));
    }
    
    // Get request ID (if missing, it's a notification)
    FString RequestId;
    const TSharedPtr<FJsonValue>* IdValue = RequestObj->Values.Find(TEXT("id"));
    if (IdValue && (*IdValue)->Type != EJson::Null)
    {
        if ((*IdValue)->Type == EJson::String)
        {
            RequestId = (*IdValue)->AsString();
        }
        else if ((*IdValue)->Type == EJson::Number)
        {
            RequestId = FString::Printf(TEXT("%d"), (int32)(*IdValue)->AsNumber());
        }
    }
    else
    {
        bOutIsNotification = true;
    }
    
    // Get params
    TSharedPtr<FJsonObject> Params;
    const TSharedPtr<FJsonObject>* ParamsPtr;
    if (RequestObj->TryGetObjectField(TEXT("params"), ParamsPtr))
    {
        Params = *ParamsPtr;
    }
    
    UE_LOG(LogMCPServer, Log, TEXT("MCP Method: %s (id: %s)"), *Method, RequestId.IsEmpty() ? TEXT("<notification>") : *RequestId);
    
    // Route to handler
    if (Method == TEXT("initialize"))
    {
        return HandleInitialize(Params, RequestId);
    }
    else if (Method == TEXT("initialized"))
    {
        // Client acknowledgment - nothing to return
        bOutIsNotification = true;
        return TEXT("");
    }
    else if (Method == TEXT("tools/list"))
    {
        return HandleToolsList(Params, RequestId);
    }
    else if (Method == TEXT("tools/call"))
    {
        return HandleToolsCall(Params, RequestId);
    }
    else if (Method == TEXT("ping"))
    {
        return HandlePing(RequestId);
    }
    else if (Method == TEXT("notifications/cancelled"))
    {
        // Cancellation notification - just acknowledge
        bOutIsNotification = true;
        return TEXT("");
    }
    else
    {
        return BuildJsonRpcError(RequestId, -32601, FString::Printf(TEXT("Method not found: %s"), *Method));
    }
}

FString FMCPServer::HandleInitialize(TSharedPtr<FJsonObject> Params, const FString& RequestId)
{
    // Generate session ID
    FString NewSessionId = GenerateSessionId();
    
    // Store session
    {
        FScopeLock Lock(&SessionLock);
        ActiveSessions.Add(NewSessionId, FDateTime::UtcNow());
    }
    
    // Build capabilities
    TSharedPtr<FJsonObject> Capabilities = MakeShared<FJsonObject>();
    
    // We support tools
    TSharedPtr<FJsonObject> ToolsCap = MakeShared<FJsonObject>();
    Capabilities->SetObjectField(TEXT("tools"), ToolsCap);
    
    // Build server info
    TSharedPtr<FJsonObject> ServerInfo = MakeShared<FJsonObject>();
    ServerInfo->SetStringField(TEXT("name"), MCP_SERVER_NAME);
    ServerInfo->SetStringField(TEXT("version"), MCP_SERVER_VERSION);
    
    // Build result
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("protocolVersion"), MCP_PROTOCOL_VERSION);
    Result->SetObjectField(TEXT("capabilities"), Capabilities);
    Result->SetObjectField(TEXT("serverInfo"), ServerInfo);
    
    UE_LOG(LogMCPServer, Log, TEXT("MCP Initialize - Session: %s"), *NewSessionId);
    
    // Build response with session ID in a special way (caller adds header)
    FString Response = BuildJsonRpcResponse(RequestId, Result);
    
    // Note: The session ID should be added as Mcp-Session-Id header by the caller
    return Response;
}

FString FMCPServer::HandleToolsList(TSharedPtr<FJsonObject> Params, const FString& RequestId)
{
    // Get internal tools
    TArray<FMCPTool> Tools = GetInternalTools();
    
    // Build tools array
    TArray<TSharedPtr<FJsonValue>> ToolsArray;
    
    for (const FMCPTool& Tool : Tools)
    {
        TSharedPtr<FJsonObject> ToolObj = MakeShared<FJsonObject>();
        ToolObj->SetStringField(TEXT("name"), Tool.Name);
        ToolObj->SetStringField(TEXT("description"), Tool.Description);
        
        // Convert input schema
        if (Tool.InputSchema.IsValid())
        {
            ToolObj->SetObjectField(TEXT("inputSchema"), Tool.InputSchema);
        }
        else
        {
            // Default empty schema
            TSharedPtr<FJsonObject> EmptySchema = MakeShared<FJsonObject>();
            EmptySchema->SetStringField(TEXT("type"), TEXT("object"));
            EmptySchema->SetObjectField(TEXT("properties"), MakeShared<FJsonObject>());
            ToolObj->SetObjectField(TEXT("inputSchema"), EmptySchema);
        }
        
        ToolsArray.Add(MakeShared<FJsonValueObject>(ToolObj));
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("tools"), ToolsArray);
    
    UE_LOG(LogMCPServer, Log, TEXT("MCP tools/list - Returning %d tools"), Tools.Num());
    
    return BuildJsonRpcResponse(RequestId, Result);
}

FString FMCPServer::HandleToolsCall(TSharedPtr<FJsonObject> Params, const FString& RequestId)
{
    if (!Params.IsValid())
    {
        return BuildJsonRpcError(RequestId, -32602, TEXT("Invalid params"));
    }
    
    FString ToolName;
    if (!Params->TryGetStringField(TEXT("name"), ToolName))
    {
        return BuildJsonRpcError(RequestId, -32602, TEXT("Missing tool name"));
    }
    
    // Get arguments
    TMap<FString, FString> Arguments;
    const TSharedPtr<FJsonObject>* ArgsObj;
    if (Params->TryGetObjectField(TEXT("arguments"), ArgsObj))
    {
        for (const auto& Pair : (*ArgsObj)->Values)
        {
            if (Pair.Value->Type == EJson::String)
            {
                Arguments.Add(Pair.Key, Pair.Value->AsString());
            }
            else
            {
                // Convert non-string values to JSON string
                FString JsonStr;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
                FJsonSerializer::Serialize(Pair.Value.ToSharedRef(), TEXT(""), Writer);
                Arguments.Add(Pair.Key, JsonStr);
            }
        }
    }
    
    UE_LOG(LogMCPServer, Log, TEXT("MCP tools/call - Tool: %s"), *ToolName);
    
    // Check if tool exists (can be done on any thread)
    FToolRegistry& Registry = FToolRegistry::Get();
    const FToolMetadata* ToolMeta = Registry.FindTool(ToolName);
    if (!ToolMeta)
    {
        return BuildJsonRpcError(RequestId, -32602, FString::Printf(TEXT("Unknown tool: %s"), *ToolName));
    }
    
    // Execute tool on game thread - REQUIRED because many UE operations are not thread-safe
    FString ToolResult;
    FEvent* CompletionEvent = FPlatformProcess::GetSynchEventFromPool(false);
    
    AsyncTask(ENamedThreads::GameThread, [&Registry, ToolName, Arguments, &ToolResult, CompletionEvent]()
    {
        ToolResult = Registry.ExecuteTool(ToolName, Arguments);
        CompletionEvent->Trigger();
    });
    
    // Wait for completion with timeout (30 seconds max for tool execution)
    bool bCompleted = CompletionEvent->Wait(FTimespan::FromSeconds(30.0));
    FPlatformProcess::ReturnSynchEventToPool(CompletionEvent);
    
    if (!bCompleted)
    {
        UE_LOG(LogMCPServer, Error, TEXT("Tool execution timed out: %s"), *ToolName);
        return BuildJsonRpcError(RequestId, -32000, TEXT("Tool execution timed out"));
    }
    
    // Build MCP result
    TArray<TSharedPtr<FJsonValue>> ContentArray;
    
    TSharedPtr<FJsonObject> TextContent = MakeShared<FJsonObject>();
    TextContent->SetStringField(TEXT("type"), TEXT("text"));
    TextContent->SetStringField(TEXT("text"), ToolResult);
    ContentArray.Add(MakeShared<FJsonValueObject>(TextContent));
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("content"), ContentArray);
    Result->SetBoolField(TEXT("isError"), ToolResult.Contains(TEXT("\"error\"")));
    
    return BuildJsonRpcResponse(RequestId, Result);
}

FString FMCPServer::HandlePing(const FString& RequestId)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    return BuildJsonRpcResponse(RequestId, Result);
}

// ============ JSON-RPC Helpers ============

FString FMCPServer::BuildJsonRpcResponse(const FString& RequestId, TSharedPtr<FJsonObject> Result)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
    
    if (!RequestId.IsEmpty())
    {
        // Try to preserve numeric ID
        int32 NumericId;
        if (LexTryParseString(NumericId, *RequestId))
        {
            Response->SetNumberField(TEXT("id"), NumericId);
        }
        else
        {
            Response->SetStringField(TEXT("id"), RequestId);
        }
    }
    else
    {
        Response->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
    }
    
    Response->SetObjectField(TEXT("result"), Result);
    
    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    
    return Output;
}

FString FMCPServer::BuildJsonRpcError(const FString& RequestId, int32 Code, const FString& Message)
{
    TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetStringField(TEXT("jsonrpc"), TEXT("2.0"));
    
    if (!RequestId.IsEmpty())
    {
        int32 NumericId;
        if (LexTryParseString(NumericId, *RequestId))
        {
            Response->SetNumberField(TEXT("id"), NumericId);
        }
        else
        {
            Response->SetStringField(TEXT("id"), RequestId);
        }
    }
    else
    {
        Response->SetField(TEXT("id"), MakeShared<FJsonValueNull>());
    }
    
    TSharedPtr<FJsonObject> Error = MakeShared<FJsonObject>();
    Error->SetNumberField(TEXT("code"), Code);
    Error->SetStringField(TEXT("message"), Message);
    Response->SetObjectField(TEXT("error"), Error);
    
    FString Output;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Response.ToSharedRef(), Writer);
    
    return Output;
}

// ============ Security ============

bool FMCPServer::ValidateApiKey(const TMap<FString, FString>& Headers) const
{
    if (Config.ApiKey.IsEmpty())
    {
        return true; // No auth required
    }
    
    // Check Authorization header
    const FString* AuthHeader = Headers.Find(TEXT("authorization"));
    if (!AuthHeader)
    {
        return false;
    }
    
    // Support "Bearer <key>" format
    if (AuthHeader->StartsWith(TEXT("Bearer "), ESearchCase::IgnoreCase))
    {
        FString ProvidedKey = AuthHeader->Mid(7);
        return ProvidedKey == Config.ApiKey;
    }
    
    // Also support raw API key
    return *AuthHeader == Config.ApiKey;
}

bool FMCPServer::ValidateOrigin(const TMap<FString, FString>& Headers) const
{
    // For localhost server, we're more permissive but still check Origin
    const FString* Origin = Headers.Find(TEXT("origin"));
    
    if (!Origin)
    {
        // No origin header - likely a non-browser client like curl or an IDE
        return true;
    }
    
    // Allow localhost origins
    if (Origin->Contains(TEXT("localhost")) || 
        Origin->Contains(TEXT("127.0.0.1")) ||
        Origin->StartsWith(TEXT("vscode-webview://")) ||
        Origin->StartsWith(TEXT("file://")))
    {
        return true;
    }
    
    UE_LOG(LogMCPServer, Warning, TEXT("Rejected request with Origin: %s"), **Origin);
    return false;
}

FString FMCPServer::GenerateSessionId() const
{
    return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
}

// ============ Tool Access ============

TArray<FMCPTool> FMCPServer::GetInternalTools() const
{
    TArray<FMCPTool> Result;
    
    FToolRegistry& Registry = FToolRegistry::Get();
    if (!Registry.IsInitialized())
    {
        UE_LOG(LogMCPServer, Warning, TEXT("ToolRegistry not initialized"));
        return Result;
    }
    
    TArray<FToolMetadata> EnabledTools = Registry.GetEnabledTools();
    Result.Reserve(EnabledTools.Num());
    
    for (const FToolMetadata& Tool : EnabledTools)
    {
        FMCPTool MCPTool;
        MCPTool.Name = Tool.Name;
        MCPTool.Description = Tool.Description;
        MCPTool.ServerName = TEXT("VibeUE-Internal");
        
        // Build input schema
        TSharedPtr<FJsonObject> InputSchema = MakeShared<FJsonObject>();
        InputSchema->SetStringField(TEXT("type"), TEXT("object"));
        
        TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> RequiredArray;
        
        for (const FToolParameter& Param : Tool.Parameters)
        {
            TSharedPtr<FJsonObject> ParamSchema = MakeShared<FJsonObject>();
            ParamSchema->SetStringField(TEXT("type"), Param.Type);
            ParamSchema->SetStringField(TEXT("description"), Param.Description);
            
            Properties->SetObjectField(Param.Name, ParamSchema);
            
            if (Param.bRequired)
            {
                RequiredArray.Add(MakeShared<FJsonValueString>(Param.Name));
            }
        }
        
        InputSchema->SetObjectField(TEXT("properties"), Properties);
        if (RequiredArray.Num() > 0)
        {
            InputSchema->SetArrayField(TEXT("required"), RequiredArray);
        }
        
        MCPTool.InputSchema = InputSchema;
        Result.Add(MCPTool);
    }
    
    return Result;
}

void FMCPServer::ProcessPendingRequests()
{
    // Process any requests that need to run on game thread
    // (Currently not used since tool execution happens synchronously)
}

// ============ Config Persistence ============

void FMCPServer::LoadConfig()
{
    Config.bEnabled = GetEnabledFromConfig();
    Config.Port = GetPortFromConfig();
    Config.ApiKey = GetApiKeyFromConfig();
}

void FMCPServer::SaveConfig()
{
    SaveEnabledToConfig(Config.bEnabled);
    SavePortToConfig(Config.Port);
    SaveApiKeyToConfig(Config.ApiKey);
}

bool FMCPServer::GetEnabledFromConfig()
{
    bool bEnabled = true; // Default to enabled
    GConfig->GetBool(TEXT("VibeUE.MCPServer"), TEXT("Enabled"), bEnabled, GEditorPerProjectIni);
    return bEnabled;
}

void FMCPServer::SaveEnabledToConfig(bool bEnabled)
{
    GConfig->SetBool(TEXT("VibeUE.MCPServer"), TEXT("Enabled"), bEnabled, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

int32 FMCPServer::GetPortFromConfig()
{
    int32 Port = 8080; // Default port
    GConfig->GetInt(TEXT("VibeUE.MCPServer"), TEXT("Port"), Port, GEditorPerProjectIni);
    return Port;
}

void FMCPServer::SavePortToConfig(int32 Port)
{
    GConfig->SetInt(TEXT("VibeUE.MCPServer"), TEXT("Port"), Port, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}

FString FMCPServer::GetApiKeyFromConfig()
{
    FString ApiKey;
    GConfig->GetString(TEXT("VibeUE.MCPServer"), TEXT("ApiKey"), ApiKey, GEditorPerProjectIni);
    return ApiKey;
}

void FMCPServer::SaveApiKeyToConfig(const FString& ApiKey)
{
    GConfig->SetString(TEXT("VibeUE.MCPServer"), TEXT("ApiKey"), *ApiKey, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}
