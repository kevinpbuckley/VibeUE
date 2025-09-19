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
#include "Commands/CommonUtils.h"
#include "Commands/UMGCommands.h"
#include "Commands/AssetCommands.h"

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

UBridge::UBridge()
{
    BlueprintCommands = MakeShared<FBlueprintCommands>();
    BlueprintNodeCommands = MakeShared<FBlueprintNodeCommands>();
    UMGCommands = MakeShared<FUMGCommands>();
    AssetCommands = MakeShared<FAssetCommands>();
}

UBridge::~UBridge()
{
    BlueprintCommands.Reset();
    BlueprintNodeCommands.Reset();
    UMGCommands.Reset();
    AssetCommands.Reset();
}

// Initialize subsystem
void UBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Initializing"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT;
    FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

    // Start the server automatically
    StartServer();
}

// Clean up resources when subsystem is destroyed
void UBridge::Deinitialize()
{
    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Shutting down"));
    StopServer();
}

// Start the MCP server
void UBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("VibeUEBridge: Server is already running"));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("VibeUEBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("VibeUEListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("VibeUEBridge: Failed to create listener socket"));
        return;
    }

    // Allow address reuse for quick restarts
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("VibeUEBridge: Failed to bind listener socket to %s:%d"), *ServerAddress.ToString(), Port);
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("VibeUEBridge: Failed to start listening"));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
        TEXT("VibeUEServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("VibeUEBridge: Failed to create server thread"));
        StopServer();
        return;
    }
}

// Stop the MCP server
void UBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;

    // Clean up thread
    if (ServerThread)
    {
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogTemp, Display, TEXT("VibeUEBridge: Server stopped"));
}

// Execute a command received from a client
FString UBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("MCP: VibeUEBridge: Executing command: %s"), *CommandType);
    
    // Create a promise to wait for the result
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();
    
    // Queue execution on Game Thread
    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
    {
        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
        
        try
        {
            TSharedPtr<FJsonObject> ResultJson;
            
            // Status and System Commands
            if (CommandType == TEXT("get_system_info"))
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("unreal_version"), TEXT("5.6"));
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
            // Blueprint Commands
            else if (CommandType == TEXT("create_blueprint") || 
                     CommandType == TEXT("add_component_to_blueprint") || 
                     CommandType == TEXT("set_component_property") || 
                     CommandType == TEXT("set_physics_properties") || 
                     CommandType == TEXT("compile_blueprint") || 
                     CommandType == TEXT("set_blueprint_property") || 
                     CommandType == TEXT("set_static_mesh_properties") ||
                     CommandType == TEXT("reparent_blueprint"))
            {
                ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Node Commands
            else if (CommandType == TEXT("connect_blueprint_nodes") || 
                     CommandType == TEXT("add_blueprint_get_self_component_reference") ||
                     CommandType == TEXT("add_blueprint_self_reference") ||
                     CommandType == TEXT("find_blueprint_nodes") ||
                     CommandType == TEXT("list_event_graph_nodes") ||
                     CommandType == TEXT("get_node_details") ||
                     CommandType == TEXT("set_blueprint_node_property") ||
                     CommandType == TEXT("get_blueprint_node_property") ||
                     CommandType == TEXT("list_blueprint_functions") ||
                     CommandType == TEXT("list_custom_events") ||
                     CommandType == TEXT("add_blueprint_event_node") ||
                     CommandType == TEXT("add_blueprint_input_action_node") ||
                     CommandType == TEXT("add_blueprint_function_node") ||
                     CommandType == TEXT("add_blueprint_get_component_node") ||
                     CommandType == TEXT("add_blueprint_variable") ||
                     CommandType == TEXT("get_available_blueprint_nodes") ||
                     CommandType == TEXT("add_blueprint_node"))
            {
                UE_LOG(LogTemp, Warning, TEXT("MCP: Dispatching to BlueprintNodeCommands: %s"), *CommandType);
                ResultJson = BlueprintNodeCommands->HandleCommand(CommandType, Params);
            }
            // UMG Commands
            else if (CommandType == TEXT("create_umg_widget_blueprint") ||
                     CommandType == TEXT("add_text_block_to_widget") ||
                     CommandType == TEXT("add_button_to_widget") ||
                     // UMG Discovery Commands
                     CommandType == TEXT("search_items") ||
                     CommandType == TEXT("get_widget_blueprint_info") ||
                     CommandType == TEXT("list_widget_components") ||
                     CommandType == TEXT("get_widget_component_properties") ||
                     CommandType == TEXT("get_available_widget_types") ||
                     CommandType == TEXT("validate_widget_hierarchy") ||
                     // UMG Component Commands
                     CommandType == TEXT("add_editable_text") ||
                     CommandType == TEXT("add_editable_text_box") ||
                     CommandType == TEXT("add_rich_text_block") ||
                     CommandType == TEXT("add_check_box") ||
                     CommandType == TEXT("add_slider") ||
                     CommandType == TEXT("add_progress_bar") ||
                     CommandType == TEXT("add_image") ||
                     CommandType == TEXT("add_spacer") ||
                     CommandType == TEXT("remove_widget_component") ||
                     // UMG Layout Commands
                     CommandType == TEXT("add_canvas_panel") ||
                     CommandType == TEXT("add_overlay") ||
                     CommandType == TEXT("add_horizontal_box") ||
                     CommandType == TEXT("add_vertical_box") ||
                     CommandType == TEXT("add_scroll_box") ||
                     CommandType == TEXT("add_grid_panel") ||
                     CommandType == TEXT("add_child_to_panel") ||
                     CommandType == TEXT("remove_child_from_panel") ||
                     CommandType == TEXT("set_widget_slot_properties") ||
                     // UMG Styling Commands
                     CommandType == TEXT("set_widget_property") ||
                     CommandType == TEXT("get_widget_property") ||
                     CommandType == TEXT("list_widget_properties") ||
                     CommandType == TEXT("set_widget_transform") ||
                     CommandType == TEXT("set_widget_visibility") ||
                     CommandType == TEXT("set_widget_z_order") ||
                     // UMG Event Commands
                     CommandType == TEXT("bind_input_events") ||
                     CommandType == TEXT("get_available_events") ||
                     // UMG Data Binding Commands
                     CommandType == TEXT("add_list_view") ||
                     CommandType == TEXT("add_tile_view") ||
                     CommandType == TEXT("add_tree_view") ||
                     // Widget Switcher Commands (relocated from animation)
                     CommandType == TEXT("add_widget_switcher") ||
                     CommandType == TEXT("add_widget_switcher_slot"))
            {
                ResultJson = UMGCommands->HandleCommand(CommandType, Params);
            }
            // Asset Discovery and Procedural Generation Commands
            else if (CommandType == TEXT("import_texture_asset") ||
                     CommandType == TEXT("export_texture_for_analysis") ||
                     CommandType == TEXT("OpenAssetInEditor"))
            {
                ResultJson = AssetCommands->HandleCommand(CommandType, Params);
            }
            else
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetBoolField(TEXT("success"), false);
                ResultJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));
            }
            
            // Check if the result contains an error
            bool bSuccess = true;
            FString ErrorMessage;
            
            if (ResultJson->HasField(TEXT("success")))
            {
                bSuccess = ResultJson->GetBoolField(TEXT("success"));
                if (!bSuccess && ResultJson->HasField(TEXT("error")))
                {
                    ErrorMessage = ResultJson->GetStringField(TEXT("error"));
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
                // Set error status and include the error message
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
            }
        }
        catch (const std::exception& e)
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
        }
        
        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        Promise.SetValue(ResultString);
    });
    
    return Future.Get();
}
