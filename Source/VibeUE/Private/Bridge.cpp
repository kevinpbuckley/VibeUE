// Copyright Kevin Buckley 2025 All Rights Reserved.

#include "Bridge.h"
#include "MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
// Add Blueprint related includes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
// UE5.5 correct includes
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
// Blueprint Graph specific includes
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
// Include our new command handler classes
#include "Commands/BlueprintCommands.h"
#include "Commands/BlueprintNodeCommands.h"
#include "Commands/BlueprintComponentReflection.h"
#include "Commands/CommonUtils.h"
#include "Commands/UMGCommands.h"
#include "Commands/UMGReflectionCommands.h"
#include "Commands/AssetCommands.h"
#include "Commands/EnhancedInputCommands.h"
#include "Commands/LevelActorCommands.h"
#include "Commands/MaterialCommands.h"
#include "Commands/MaterialNodeCommands.h"
// Include service architecture
#include "Core/ServiceContext.h"
#include "Core/ErrorCodes.h"

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

UBridge::UBridge()
{
    // Create service context (shared across all services and command handlers)
    ServiceContext = MakeShared<FServiceContext>();
    
    // Initialize command handlers
    // Note: Some handlers create their own ServiceContext internally (AssetCommands, UMGCommands, BlueprintNodeCommands)
    // TODO(Issue #38-40): Update remaining handlers to accept ServiceContext when refactored
    BlueprintCommands = MakeShared<FBlueprintCommands>();
    BlueprintNodeCommands = MakeShared<FBlueprintNodeCommands>();
    BlueprintComponentReflection = MakeShared<FBlueprintComponentReflection>();
    UMGCommands = MakeShared<FUMGCommands>(ServiceContext);
    UMGReflectionCommands = MakeShared<FUMGReflectionCommands>();
    AssetCommands = MakeShared<FAssetCommands>();
    EnhancedInputCommands = MakeShared<FEnhancedInputCommands>();
    LevelActorCommands = MakeShared<FLevelActorCommands>();
    MaterialCommands = MakeShared<FMaterialCommands>();
    MaterialNodeCommands = MakeShared<FMaterialNodeCommands>();
}

UBridge::~UBridge()
{
    BlueprintCommands.Reset();
    BlueprintNodeCommands.Reset();
    BlueprintComponentReflection.Reset();
    UMGCommands.Reset();
    UMGReflectionCommands.Reset();
    AssetCommands.Reset();
    EnhancedInputCommands.Reset();
    LevelActorCommands.Reset();
    MaterialCommands.Reset();
    MaterialNodeCommands.Reset();
    
    // Defensive cleanup - Deinitialize() should have been called by UEditorSubsystem,
    // but ensure ServiceContext is cleaned up even if lifecycle was abnormal
    if (ServiceContext.IsValid())
    {
        ServiceContext.Reset();
    }
}

// Initialize subsystem
void UBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Initializing with service architecture"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT;
    FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

    // Log service context initialization
    if (ServiceContext.IsValid())
    {
        UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: ServiceContext initialized successfully"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("VibeUEBridge: Failed to initialize ServiceContext"));
        return;
    }

    // Start the server automatically
    StartServer();
}

// Clean up resources when subsystem is destroyed
void UBridge::Deinitialize()
{
    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Graceful shutdown initiated"));
    StopServer();
    
    // Clean up service context
    if (ServiceContext.IsValid())
    {
        UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: ServiceContext cleaned up"));
        ServiceContext.Reset();
    }
    
    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Shutdown complete"));
}

// Start the MCP server
void UBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("VibeUEBridge: Server is already running on %s:%d"), *ServerAddress.ToString(), Port);
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Starting server on %s:%d"), *ServerAddress.ToString(), Port);

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("VibeUEBridge: Failed to get socket subsystem - network services unavailable"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("VibeUEListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("VibeUEBridge: Failed to create listener socket - cannot start server"));
        return;
    }

    // Allow address reuse for quick restarts
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("VibeUEBridge: Failed to bind listener socket to %s:%d - address may be in use"), *ServerAddress.ToString(), Port);
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("VibeUEBridge: Failed to start listening on %s:%d"), *ServerAddress.ToString(), Port);
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Server started successfully on %s:%d - ready for connections"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
        TEXT("VibeUEServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("VibeUEBridge: Failed to create server thread - stopping server"));
        StopServer();
        return;
    }
    
    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Server thread created successfully"));
}

// Stop the MCP server
void UBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Stopping server..."));
    bIsRunning = false;

    // Clean up thread
    if (ServerThread)
    {
        UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Terminating server thread"));
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Closing connection socket"));
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Closing listener socket"));
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Server stopped successfully"));
}

// Route command to appropriate handler based on command type
TSharedPtr<FJsonObject> UBridge::RouteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> ResultJson;
    
    // Status and System Commands
    if (CommandType == TEXT("get_system_info"))
    {
        ResultJson = MakeShareable(new FJsonObject);
        ResultJson->SetBoolField(TEXT("success"), true);
        ResultJson->SetStringField(TEXT("unreal_version"), TEXT("5.7"));
        ResultJson->SetStringField(TEXT("plugin_version"), TEXT("1.0"));
        ResultJson->SetStringField(TEXT("server_status"), TEXT("running"));
        ResultJson->SetBoolField(TEXT("editor_connected"), true);
        
        TSharedPtr<FJsonObject> AvailableTools = MakeShareable(new FJsonObject);
        AvailableTools->SetBoolField(TEXT("widget_tools"), true);
        AvailableTools->SetBoolField(TEXT("blueprint_tools"), true);
        AvailableTools->SetBoolField(TEXT("actor_tools"), true);
        AvailableTools->SetBoolField(TEXT("editor_tools"), true);
        ResultJson->SetObjectField(TEXT("available_tools"), AvailableTools);
    }
    // Blueprint Component Reflection Commands
    else if (CommandType == TEXT("get_available_components") ||
             CommandType == TEXT("get_component_info") ||
             CommandType == TEXT("get_property_metadata") ||
             CommandType == TEXT("get_component_hierarchy") ||
             CommandType == TEXT("add_component") ||
             CommandType == TEXT("set_component_property") ||
             CommandType == TEXT("get_component_property") ||
             CommandType == TEXT("get_all_component_properties") ||
             CommandType == TEXT("compare_component_properties") ||
             CommandType == TEXT("reparent_component") ||
             CommandType == TEXT("remove_component") ||
             CommandType == TEXT("reorder_components"))
    {
        ResultJson = BlueprintComponentReflection->HandleCommand(CommandType, Params);
    }
    // Blueprint Commands
    else if (CommandType == TEXT("create_blueprint") || 
             CommandType == TEXT("add_component_to_blueprint") || 
             CommandType == TEXT("set_component_property") || 
             CommandType == TEXT("compile_blueprint") || 
             CommandType == TEXT("get_blueprint_property") || 
             CommandType == TEXT("set_blueprint_property") || 
             CommandType == TEXT("reparent_blueprint") ||
             // Blueprint Variable Commands
             CommandType == TEXT("manage_blueprint_variable") ||
             CommandType == TEXT("add_blueprint_variable") ||
             CommandType == TEXT("get_blueprint_variable") ||
             CommandType == TEXT("delete_blueprint_variable") ||
             CommandType == TEXT("get_available_blueprint_variable_types") ||
             // Reflection-based variable property API (two-method)
             CommandType == TEXT("get_variable_property") ||
             CommandType == TEXT("set_variable_property") ||
             // Comprehensive Blueprint information
             CommandType == TEXT("get_blueprint_info"))
    {
        ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
    }
    // Blueprint Node Commands
    else if (CommandType == TEXT("manage_blueprint_node") ||
             CommandType == TEXT("manage_blueprint_function") ||
             CommandType == TEXT("get_available_blueprint_nodes") ||
             CommandType == TEXT("discover_nodes_with_descriptors"))
    {
        UE_LOG(LogTemp, Warning, TEXT("MCP: Dispatching to BlueprintNodeCommands: %s"), *CommandType);
        ResultJson = BlueprintNodeCommands->HandleCommand(CommandType, Params);
    }
    // UMG Commands (Reflection-based system)
    else if (CommandType == TEXT("create_umg_widget_blueprint") ||
             CommandType == TEXT("delete_widget_blueprint") ||
             // UMG Discovery Commands
             CommandType == TEXT("search_items") ||
             CommandType == TEXT("get_widget_blueprint_info") ||
             CommandType == TEXT("list_widget_components") ||
             CommandType == TEXT("get_widget_component_properties") ||
             CommandType == TEXT("get_available_widget_types") ||
             CommandType == TEXT("validate_widget_hierarchy") ||
             CommandType == TEXT("remove_widget_component") ||
             // UMG Child Management
             CommandType == TEXT("add_child_to_panel") ||
             CommandType == TEXT("remove_umg_component") ||  // Universal component removal
             CommandType == TEXT("set_widget_slot_properties") ||
             // UMG Styling Commands
             CommandType == TEXT("set_widget_property") ||
             CommandType == TEXT("get_widget_property") ||
             CommandType == TEXT("list_widget_properties") ||
             // UMG Event Commands
             CommandType == TEXT("bind_input_events") ||
             CommandType == TEXT("get_available_events"))
    {
        ResultJson = UMGCommands->HandleCommand(CommandType, Params);
    }
    // UMG Reflection Commands
    else if (CommandType == TEXT("get_available_widgets") ||
             CommandType == TEXT("add_widget_component"))
    {
        ResultJson = UMGReflectionCommands->HandleCommand(CommandType, Params);
    }
    // Asset Discovery and Procedural Generation Commands
    else if (CommandType == TEXT("import_texture_asset") ||
             CommandType == TEXT("export_texture_for_analysis") ||
             CommandType == TEXT("delete_asset") ||
             CommandType == TEXT("duplicate_asset") ||
             CommandType == TEXT("save_asset") ||
             CommandType == TEXT("save_all_assets") ||
             CommandType == TEXT("list_references") ||
             CommandType == TEXT("OpenAssetInEditor"))
    {
        ResultJson = AssetCommands->HandleCommand(CommandType, Params);
    }
    // Enhanced Input System Commands
    else if (CommandType == TEXT("manage_enhanced_input"))
    {
        UE_LOG(LogTemp, Display, TEXT("MCP: Dispatching to EnhancedInputCommands: %s"), *CommandType);
        ResultJson = EnhancedInputCommands->HandleCommand(CommandType, Params);
    }
    // Level Actor Commands
    else if (CommandType == TEXT("manage_level_actors"))
    {
        UE_LOG(LogTemp, Display, TEXT("MCP: Dispatching to LevelActorCommands: %s"), *CommandType);
        ResultJson = LevelActorCommands->HandleCommand(CommandType, Params);
    }
    // Material Commands
    else if (CommandType == TEXT("manage_material"))
    {
        UE_LOG(LogTemp, Display, TEXT("MCP: Dispatching to MaterialCommands: %s"), *CommandType);
        ResultJson = MaterialCommands->HandleCommand(CommandType, Params);
    }
    // Material Node Commands
    else if (CommandType == TEXT("manage_material_node"))
    {
        UE_LOG(LogTemp, Display, TEXT("MCP: Dispatching to MaterialNodeCommands: %s"), *CommandType);
        ResultJson = MaterialNodeCommands->HandleCommand(CommandType, Params);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("VibeUEBridge: Unknown command received: %s"), *CommandType);
        ResultJson = MakeShareable(new FJsonObject);
        ResultJson->SetBoolField(TEXT("success"), false);
        ResultJson->SetStringField(TEXT("error_code"), VibeUE::ErrorCodes::UNKNOWN_COMMAND);
        ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));
    }
    
    return ResultJson;
}

// Execute a command received from a client
FString UBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("MCP: VibeUEBridge: Executing command: %s"), *CommandType);
    
    double StartTime = FPlatformTime::Seconds();
    
    // Create a promise to wait for the result
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();
    
    // Queue execution on Game Thread
    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
    {
        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
        
        try
        {
            // Route command to appropriate handler
            TSharedPtr<FJsonObject> ResultJson = RouteCommand(CommandType, Params);
            
            // Check if the result contains an error
            bool bSuccess = true;
            FString ErrorMessage;

            if (ResultJson->HasField(TEXT("success")))
            {
                bSuccess = ResultJson->GetBoolField(TEXT("success"));
                if (!bSuccess)
                {
                    // Prefer explicit 'error', but fall back to 'message' or serialize full object
                    if (ResultJson->HasField(TEXT("error")))
                    {
                        ErrorMessage = ResultJson->GetStringField(TEXT("error"));
                    }
                    else if (ResultJson->HasField(TEXT("message")))
                    {
                        ErrorMessage = ResultJson->GetStringField(TEXT("message"));
                    }
                    else
                    {
                        // Last resort: serialize the inner object for diagnostics
                        FString Tmp;
                        TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Tmp);
                        FJsonSerializer::Serialize(ResultJson.ToSharedRef(), W);
                        ErrorMessage = Tmp;
                    }
                }
            }

            if (bSuccess)
            {
                // Set success status and include the result
                ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
                ResponseJson->SetObjectField(TEXT("result"), ResultJson);
            }
            else
            {
                // Set error status and include the error message and full inner result for debugging
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
                ResponseJson->SetObjectField(TEXT("result"), ResultJson);

                // Optional: surface a machine-readable error code if provided by inner result
                if (ResultJson->HasField(TEXT("code")))
                {
                    ResponseJson->SetField(TEXT("error_code"), ResultJson->TryGetField(TEXT("code")));
                }
                // Also check for error_code field directly
                else if (ResultJson->HasField(TEXT("error_code")))
                {
                    ResponseJson->SetField(TEXT("error_code"), ResultJson->TryGetField(TEXT("error_code")));
                }
            }
        }
        catch (const std::exception& e)
        {
            FString ExceptionMessage = UTF8_TO_TCHAR(e.what());
            UE_LOG(LogTemp, Error, TEXT("VibeUEBridge: C++ exception during command execution: %s"), *ExceptionMessage);
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error_code"), VibeUE::ErrorCodes::CPP_EXCEPTION);
            ResponseJson->SetStringField(TEXT("error"), ExceptionMessage);
        }
        
        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        Promise.SetValue(ResultString);
    });
    
    // Wait for result - returns immediately when ready, timeout is just the max wait
    bool bReady = Future.WaitFor(FTimespan::FromSeconds(15));
    
    double ElapsedTime = FPlatformTime::Seconds() - StartTime;
    
    if (!bReady)
    {
        UE_LOG(LogTemp, Error, TEXT("MCP: VibeUEBridge: Command '%s' timed out after %.1f seconds - game thread may be blocked"), *CommandType, ElapsedTime);
        return CreateErrorResponse(TEXT("TIMEOUT"), FString::Printf(TEXT("Command '%s' timed out after %.1f seconds. The game thread may be busy with level loading or asset compilation."), *CommandType, ElapsedTime));
    }
    
    if (ElapsedTime > 5.0)
    {
        UE_LOG(LogTemp, Warning, TEXT("MCP: VibeUEBridge: Command '%s' took %.1f seconds to complete"), *CommandType, ElapsedTime);
    }
    
    return Future.Get();
}

// Helper to create standardized error response
FString UBridge::CreateErrorResponse(const FString& ErrorCode, const FString& ErrorMessage)
{
    TSharedPtr<FJsonObject> ResponseJson = MakeShared<FJsonObject>();
    ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
    ResponseJson->SetStringField(TEXT("error_code"), ErrorCode);
    
    if (!ErrorMessage.IsEmpty())
    {
        ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
    }
    else
    {
        ResponseJson->SetStringField(TEXT("error"), ErrorCode);
    }
    
    FString ResultString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
    FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
    return ResultString;
}
