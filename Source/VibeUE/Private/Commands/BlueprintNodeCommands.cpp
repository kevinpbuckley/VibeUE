#include "Commands/BlueprintNodeCommands.h"
#include "Commands/BlueprintReflection.h"
#include "Commands/CommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
// #include "K2Node_ForEachLoop.h"  // Commented out - header not found
#include "K2Node_Timeline.h"
#include "K2Node_MacroInstance.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GameFramework/InputSettings.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "EdGraphSchema_K2.h"

// Declare the log category
DEFINE_LOG_CATEGORY_STATIC(LogVibeUE, Log, All);

FBlueprintNodeCommands::FBlueprintNodeCommands()
{
    // Initialize reflection system
    ReflectionCommands = MakeShareable(new FBlueprintReflectionCommands());
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogVibeUE, Warning, TEXT("MCP: BlueprintNodeCommands::HandleCommand called with CommandType: %s"), *CommandType);
    
    if (CommandType == TEXT("connect_blueprint_nodes"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleConnectBlueprintNodes"));
        return HandleConnectBlueprintNodes(Params);
    }
    else if (CommandType == TEXT("add_blueprint_event_node"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleAddBlueprintEvent"));
        return HandleAddBlueprintEvent(Params);
    }
    else if (CommandType == TEXT("add_blueprint_input_action_node"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleAddBlueprintInputActionNode"));
        return HandleAddBlueprintInputActionNode(Params);
    }
    else if (CommandType == TEXT("find_blueprint_nodes"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleFindBlueprintNodes"));
        return HandleFindBlueprintNodes(Params);
    }
    else if (CommandType == TEXT("list_event_graph_nodes"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleListEventGraphNodes"));
        return HandleListEventGraphNodes(Params);
    }
    else if (CommandType == TEXT("get_node_details"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleGetNodeDetails"));
        return HandleGetNodeDetails(Params);
    }
    else if (CommandType == TEXT("list_blueprint_functions"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleListBlueprintFunctions"));
        return HandleListBlueprintFunctions(Params);
    }
    else if (CommandType == TEXT("list_custom_events"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleListCustomEvents"));
        return HandleListCustomEvents(Params);
    }
    // NEW: Reflection-based commands
    else if (CommandType == TEXT("get_available_blueprint_nodes"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleGetAvailableBlueprintNodes (Reflection)"));
        return HandleGetAvailableBlueprintNodes(Params);
    }
    else if (CommandType == TEXT("add_blueprint_node"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleAddBlueprintNode (Reflection)"));
        return HandleAddBlueprintNode(Params);
    }
    else if (CommandType == TEXT("set_blueprint_node_property"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleSetBlueprintNodeProperty (Reflection)"));
        return HandleSetBlueprintNodeProperty(Params);
    }
    else if (CommandType == TEXT("get_blueprint_node_property"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleGetBlueprintNodeProperty (Reflection)"));
        return HandleGetBlueprintNodeProperty(Params);
    }
    // NEW: Deletion commands
    else if (CommandType == TEXT("delete_blueprint_node"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleDeleteBlueprintNode"));
        return HandleDeleteBlueprintNode(Params);
    }
    else if (CommandType == TEXT("delete_blueprint_event_node"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleDeleteBlueprintEventNode"));
        return HandleDeleteBlueprintEventNode(Params);
    }
    
    UE_LOG(LogVibeUE, Error, TEXT("MCP: Unknown blueprint node command: %s"), *CommandType);
    return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown blueprint node command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString SourceNodeId;
    if (!Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'source_node_id' parameter"));
    }

    FString TargetNodeId;
    if (!Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'target_node_id' parameter"));
    }

    FString SourcePinName;
    if (!Params->TryGetStringField(TEXT("source_pin"), SourcePinName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'source_pin' parameter"));
    }

    FString TargetPinName;
    if (!Params->TryGetStringField(TEXT("target_pin"), TargetPinName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'target_pin' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Find the nodes
    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (Node->NodeGuid.ToString() == SourceNodeId)
        {
            SourceNode = Node;
        }
        else if (Node->NodeGuid.ToString() == TargetNodeId)
        {
            TargetNode = Node;
        }
    }

    if (!SourceNode || !TargetNode)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Source or target node not found"));
    }

    // Enhanced connection with reflection-based pin discovery
    TSharedPtr<FJsonObject> ConnectionResult = FCommonUtils::ConnectGraphNodesWithReflection(
        EventGraph, SourceNode, SourcePinName, TargetNode, TargetPinName);
    
    bool bSuccess = false;
    ConnectionResult->TryGetBoolField(TEXT("success"), bSuccess);
    
    if (bSuccess)
    {
        // Mark the blueprint as modified
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        
        // Return the enhanced connection result with detailed information
        return ConnectionResult;
    }
    else
    {
        // Return detailed error with suggestions
        return ConnectionResult;
    }
}


TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleAddBlueprintEvent(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString EventName;
    if (!Params->TryGetStringField(TEXT("event_name"), EventName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create the event node
    UK2Node_Event* EventNode = FCommonUtils::CreateEventNode(EventGraph, EventName, NodePosition);
    if (!EventNode)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to create event node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), EventNode->NodeGuid.ToString());
    return ResultObj;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleAddBlueprintInputActionNode(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActionName;
    if (!Params->TryGetStringField(TEXT("action_name"), ActionName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'action_name' parameter"));
    }

    // Get position parameters (optional)
    FVector2D NodePosition(0.0f, 0.0f);
    if (Params->HasField(TEXT("node_position")))
    {
        NodePosition = FCommonUtils::GetVector2DFromJson(Params, TEXT("node_position"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create the input action node
    UK2Node_InputAction* InputActionNode = FCommonUtils::CreateInputActionNode(EventGraph, ActionName, NodePosition);
    if (!InputActionNode)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to create input action node"));
    }

    // Mark the blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("node_id"), InputActionNode->NodeGuid.ToString());
    return ResultObj;
}


TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleFindBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get the event graph
    UEdGraph* EventGraph = FCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    // Create a JSON array for the node GUIDs
    TArray<TSharedPtr<FJsonValue>> NodeGuidArray;
    
    // Filter nodes by the exact requested type
    if (NodeType == TEXT("Event"))
    {
        FString EventName;
        // Prefer 'event_name', but allow legacy 'event_type' for compatibility
        if (!Params->TryGetStringField(TEXT("event_name"), EventName))
        {
            Params->TryGetStringField(TEXT("event_type"), EventName);
        }
        if (EventName.IsEmpty())
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'event_name' (or legacy 'event_type') parameter for Event node search"));
        }
        
        // Look for nodes with exact event name (e.g., ReceiveBeginPlay)
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
            if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
            {
                UE_LOG(LogTemp, Display, TEXT("Found event node with name %s: %s"), *EventName, *EventNode->NodeGuid.ToString());
                NodeGuidArray.Add(MakeShared<FJsonValueString>(EventNode->NodeGuid.ToString()));
            }
        }
    }
    // Add other node types as needed (InputAction, etc.)
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("node_guids"), NodeGuidArray);
    
    return ResultObj;
} 

static FString GetNodeTypeString(const UEdGraphNode* Node)
{
    if (Cast<UK2Node_Event>(Node)) return TEXT("Event");
    if (Cast<UK2Node_CallFunction>(Node)) return TEXT("FunctionCall");
    if (Cast<UK2Node_VariableGet>(Node)) return TEXT("VariableGet");
    if (Cast<UK2Node_VariableSet>(Node)) return TEXT("VariableSet");
    if (Cast<UK2Node_IfThenElse>(Node)) return TEXT("Branch");
    // if (Cast<UK2Node_ForEachLoop>(Node)) return TEXT("ForEachLoop");  // Commented out - class not available
    if (Cast<UK2Node_Timeline>(Node)) return TEXT("Timeline");
    if (Cast<UK2Node_MacroInstance>(Node)) return TEXT("MacroInstance");
    if (Cast<UK2Node_CustomEvent>(Node)) return TEXT("CustomEvent");
    return Node ? Node->GetClass()->GetName() : TEXT("Unknown");
}

static TSharedPtr<FJsonObject> MakePinJson(const UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
    PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
    PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
    PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
    if (!Pin->DefaultValue.IsEmpty())
    {
        PinObj->SetStringField(TEXT("default"), Pin->DefaultValue);
    }
    // Connections for outputs
    if (Pin->Direction == EGPD_Output)
    {
        TArray<TSharedPtr<FJsonValue>> Conns;
        for (const UEdGraphPin* Linked : Pin->LinkedTo)
        {
            TSharedPtr<FJsonObject> C = MakeShared<FJsonObject>();
            C->SetStringField(TEXT("to_node_id"), Linked->GetOwningNode()->NodeGuid.ToString());
            C->SetStringField(TEXT("to_pin"), Linked->PinName.ToString());
            Conns.Add(MakeShared<FJsonValueObject>(C));
        }
        if (Conns.Num() > 0)
        {
            PinObj->SetArrayField(TEXT("connections"), Conns);
        }
    }
    return PinObj;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleListEventGraphNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    bool bIncludeFunctions = true, bIncludeMacros = true, bIncludeTimeline = true;
    Params->TryGetBoolField(TEXT("include_functions"), bIncludeFunctions);
    Params->TryGetBoolField(TEXT("include_macros"), bIncludeMacros);
    Params->TryGetBoolField(TEXT("include_timeline"), bIncludeTimeline);

    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    UEdGraph* EventGraph = FCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }

    TArray<TSharedPtr<FJsonValue>> NodeArray;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        const FString Type = GetNodeTypeString(Node);
        if (!bIncludeFunctions && Type == TEXT("FunctionCall")) continue;
        if (!bIncludeMacros && Type == TEXT("MacroInstance")) continue;
        if (!bIncludeTimeline && Type == TEXT("Timeline")) continue;

        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
        Obj->SetStringField(TEXT("node_type"), Type);
        Obj->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

        TArray<TSharedPtr<FJsonValue>> Pins;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            Pins.Add(MakeShared<FJsonValueObject>(MakePinJson(Pin)));
        }
        Obj->SetArrayField(TEXT("pins"), Pins);
        NodeArray.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("nodes"), NodeArray);
    return Result;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleGetNodeDetails(const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogVibeUE, Warning, TEXT("MCP: HandleGetNodeDetails called"));
    
    FString BlueprintName, NodeId;
    bool bIncludeProperties = true;
    bool bIncludePins = true;
    bool bIncludeConnections = true;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        UE_LOG(LogVibeUE, Error, TEXT("MCP: HandleGetNodeDetails - Missing blueprint_name parameter"));
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("node_id"), NodeId))
    {
        UE_LOG(LogVibeUE, Error, TEXT("MCP: HandleGetNodeDetails - Missing node_id parameter"));
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }
    
    // Enhanced: Support optional parameters
    Params->TryGetBoolField(TEXT("include_properties"), bIncludeProperties);
    Params->TryGetBoolField(TEXT("include_pins"), bIncludePins);
    Params->TryGetBoolField(TEXT("include_connections"), bIncludeConnections);
    
    UE_LOG(LogVibeUE, Warning, TEXT("MCP: HandleGetNodeDetails - Blueprint: %s, NodeId: %s, Props: %s, Pins: %s, Conns: %s"), 
        *BlueprintName, *NodeId, bIncludeProperties ? TEXT("true") : TEXT("false"), 
        bIncludePins ? TEXT("true") : TEXT("false"), bIncludeConnections ? TEXT("true") : TEXT("false"));
    
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        UE_LOG(LogVibeUE, Error, TEXT("MCP: HandleGetNodeDetails - Blueprint not found: %s"), *BlueprintName);
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }
    UEdGraph* EventGraph = FCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }
    UEdGraphNode* Found = nullptr;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (Node->NodeGuid.ToString() == NodeId)
        {
            Found = Node; break;
        }
    }
    if (!Found)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Node not found"));
    }
    
    // Enhanced: Create comprehensive node information
    TSharedPtr<FJsonObject> NodeInfo = MakeShared<FJsonObject>();
    NodeInfo->SetStringField(TEXT("id"), Found->NodeGuid.ToString());
    NodeInfo->SetStringField(TEXT("node_class"), Found->GetClass()->GetName());
    NodeInfo->SetStringField(TEXT("title"), Found->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    
    // Enhanced: Add position information
    TArray<TSharedPtr<FJsonValue>> Position;
    Position.Add(MakeShared<FJsonValueNumber>(Found->NodePosX));
    Position.Add(MakeShared<FJsonValueNumber>(Found->NodePosY));
    NodeInfo->SetArrayField(TEXT("position"), Position);
    
    // Enhanced: Add category and metadata
    if (UK2Node* K2Node = Cast<UK2Node>(Found))
    {
        NodeInfo->SetStringField(TEXT("category"), K2Node->GetMenuCategory().ToString());
        NodeInfo->SetStringField(TEXT("tooltip"), K2Node->GetTooltipText().ToString());
        NodeInfo->SetStringField(TEXT("keywords"), K2Node->GetKeywords().ToString());
    }
    
    // Enhanced: Add node state information
    NodeInfo->SetBoolField(TEXT("can_user_delete_node"), Found->CanUserDeleteNode());
    // Note: Comment node checking requires specific include - simplified for now
    NodeInfo->SetStringField(TEXT("node_class_simple"), Found->GetClass()->GetName().Contains(TEXT("Comment")) ? TEXT("Comment") : TEXT("Other"));
    
    // Enhanced: Try to get additional node properties safely
    if (UK2Node* K2Node = Cast<UK2Node>(Found))
    {
        // Color information might not be available on all node types
        NodeInfo->SetStringField(TEXT("node_type"), TEXT("Blueprint"));
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetObjectField(TEXT("node_info"), NodeInfo);
    
    // Enhanced: Include detailed pins if requested
    if (bIncludePins)
    {
        TArray<TSharedPtr<FJsonValue>> InputPins;
        TArray<TSharedPtr<FJsonValue>> OutputPins;
        
        for (UEdGraphPin* Pin : Found->Pins)
        {
            TSharedPtr<FJsonObject> PinInfo = MakeShared<FJsonObject>();
            PinInfo->SetStringField(TEXT("name"), Pin->PinName.ToString());
            PinInfo->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
            PinInfo->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
            PinInfo->SetBoolField(TEXT("is_hidden"), Pin->bHidden);
            PinInfo->SetBoolField(TEXT("is_connected"), Pin->LinkedTo.Num() > 0);
            
            // Enhanced: Add default value information
            if (!Pin->DefaultValue.IsEmpty())
            {
                PinInfo->SetStringField(TEXT("default_value"), Pin->DefaultValue);
            }
            if (Pin->DefaultObject)
            {
                PinInfo->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetName());
            }
            if (!Pin->DefaultTextValue.IsEmpty())
            {
                PinInfo->SetStringField(TEXT("default_text"), Pin->DefaultTextValue.ToString());
            }
            
            // Enhanced: Add connection information if requested
            if (bIncludeConnections && Pin->LinkedTo.Num() > 0)
            {
                TArray<TSharedPtr<FJsonValue>> Connections;
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (LinkedPin && LinkedPin->GetOwningNode())
                    {
                        TSharedPtr<FJsonObject> Connection = MakeShared<FJsonObject>();
                        Connection->SetStringField(TEXT("to_node_id"), LinkedPin->GetOwningNode()->NodeGuid.ToString());
                        Connection->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
                        Connections.Add(MakeShared<FJsonValueObject>(Connection));
                    }
                }
                PinInfo->SetArrayField(TEXT("connections"), Connections);
            }
            
            // Enhanced: Add pin type details
            if (Pin->PinType.PinSubCategory.IsNone() == false)
            {
                PinInfo->SetStringField(TEXT("sub_category"), Pin->PinType.PinSubCategory.ToString());
            }
            if (Pin->PinType.PinSubCategoryObject.IsValid())
            {
                PinInfo->SetStringField(TEXT("sub_category_object"), Pin->PinType.PinSubCategoryObject->GetName());
            }
            PinInfo->SetBoolField(TEXT("is_array"), Pin->PinType.IsArray());
            PinInfo->SetBoolField(TEXT("is_reference"), Pin->PinType.bIsReference);
            
            if (Pin->Direction == EGPD_Input)
            {
                InputPins.Add(MakeShared<FJsonValueObject>(PinInfo));
            }
            else
            {
                OutputPins.Add(MakeShared<FJsonValueObject>(PinInfo));
            }
        }
        
        TSharedPtr<FJsonObject> PinsInfo = MakeShared<FJsonObject>();
        PinsInfo->SetArrayField(TEXT("input_pins"), InputPins);
        PinsInfo->SetArrayField(TEXT("output_pins"), OutputPins);
        Result->SetObjectField(TEXT("pins"), PinsInfo);
    }
    
    // Enhanced: Include node properties if requested
    if (bIncludeProperties)
    {
        TArray<TSharedPtr<FJsonValue>> Properties;
        
        // Get reflection-based properties
        UClass* NodeClass = Found->GetClass();
        for (TFieldIterator<FProperty> PropIt(NodeClass); PropIt; ++PropIt)
        {
            FProperty* Property = *PropIt;
            if (!Property || Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
            {
                continue;
            }
            
            // Enhanced: Include editable properties
            if (Property->HasAnyPropertyFlags(CPF_Edit))
            {
                TSharedPtr<FJsonObject> PropInfo = MakeShared<FJsonObject>();
                PropInfo->SetStringField(TEXT("name"), Property->GetName());
                PropInfo->SetStringField(TEXT("type"), Property->GetClass()->GetName());
                PropInfo->SetBoolField(TEXT("editable"), true);
                
                // Enhanced: Try to get current property value as string
                FString PropertyValue;
                if (const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Found))
                {
                    // Use simpler text export method
                    PropertyValue = Property->GetNameCPP();
                    PropInfo->SetStringField(TEXT("current_value"), PropertyValue);
                }
                
                // Enhanced: Add property metadata
                if (Property->HasMetaData(TEXT("Tooltip")))
                {
                    PropInfo->SetStringField(TEXT("tooltip"), Property->GetMetaData(TEXT("Tooltip")));
                }
                if (Property->HasMetaData(TEXT("Category")))
                {
                    PropInfo->SetStringField(TEXT("category"), Property->GetMetaData(TEXT("Category")));
                }
                
                Properties.Add(MakeShared<FJsonValueObject>(PropInfo));
            }
        }
        
        Result->SetArrayField(TEXT("properties"), Properties);
    }
    
    return Result;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleListBlueprintFunctions(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    bool bIncludeOverrides = true;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }
    Params->TryGetBoolField(TEXT("include_overrides"), bIncludeOverrides);

    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    TArray<TSharedPtr<FJsonValue>> Funcs;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!Graph) continue;
        TSharedPtr<FJsonObject> F = MakeShared<FJsonObject>();
        F->SetStringField(TEXT("name"), Graph->GetName());

        // Gather entry pins
        TArray<FString> Inputs, Outputs;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            // TODO: Fix UK2Node_FunctionEntry compilation issue
            /*
            if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
            {
                for (const FEdGraphPinType& PinType : Entry->UserDefinedPins)
                {
                    Inputs.Add(PinType.PinCategory.ToString());
                }
            }
            */
        }
        // Attempt to find result node types
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node->GetClass()->GetName().Contains(TEXT("FunctionResult")))
            {
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin->Direction == EGPD_Input)
                    {
                        Outputs.Add(Pin->PinType.PinCategory.ToString());
                    }
                }
            }
        }
        if (Inputs.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> InVals; for (const FString& S : Inputs) InVals.Add(MakeShared<FJsonValueString>(S));
            F->SetArrayField(TEXT("inputs"), InVals);
        }
        if (Outputs.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> OutVals; for (const FString& S : Outputs) OutVals.Add(MakeShared<FJsonValueString>(S));
            F->SetArrayField(TEXT("outputs"), OutVals);
        }
        Funcs.Add(MakeShared<FJsonValueObject>(F));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("functions"), Funcs);
    return Result;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleListCustomEvents(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }
    UEdGraph* EventGraph = FCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (!EventGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to get event graph"));
    }
    TArray<TSharedPtr<FJsonValue>> Events;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (UK2Node_CustomEvent* CE = Cast<UK2Node_CustomEvent>(Node))
        {
            TSharedPtr<FJsonObject> Evt = MakeShared<FJsonObject>();
            Evt->SetStringField(TEXT("name"), CE->CustomFunctionName.ToString());
            Events.Add(MakeShared<FJsonValueObject>(Evt));
        }
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("events"), Events);
    return Result;
}

// NEW: Reflection-based command implementations
TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleGetAvailableBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    if (ReflectionCommands.IsValid())
    {
        return ReflectionCommands->HandleGetAvailableBlueprintNodes(Params);
    }
    return FCommonUtils::CreateErrorResponse(TEXT("Reflection system not initialized"));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleAddBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    if (ReflectionCommands.IsValid())
    {
        return ReflectionCommands->HandleAddBlueprintNode(Params);
    }
    return FCommonUtils::CreateErrorResponse(TEXT("Reflection system not initialized"));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleSetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
    if (ReflectionCommands.IsValid())
    {
        return ReflectionCommands->HandleSetBlueprintNodeProperty(Params);
    }
    return FCommonUtils::CreateErrorResponse(TEXT("Reflection system not initialized"));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleGetBlueprintNodeProperty(const TSharedPtr<FJsonObject>& Params)
{
    if (ReflectionCommands.IsValid())
    {
        return ReflectionCommands->HandleGetBlueprintNodeProperty(Params);
    }
    return FCommonUtils::CreateErrorResponse(TEXT("Reflection system not initialized"));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleDeleteBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    FString NodeID;
    bool DisconnectPins = true;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing blueprint_name parameter"));
    }
    
    if (!Params->TryGetStringField(TEXT("node_id"), NodeID))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing node_id parameter"));
    }
    
    // Optional parameter with default
    Params->TryGetBoolField(TEXT("disconnect_pins"), DisconnectPins);
    
    // Find the Blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }
    
    // Find the Event Graph
    UEdGraph* EventGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph && Graph->GetFName() == "EventGraph")
        {
            EventGraph = Graph;
            break;
        }
    }
    
    if (!EventGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("EventGraph not found in Blueprint"));
    }
    
    // Find the node by ID
    UEdGraphNode* NodeToDelete = nullptr;
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (Node && Node->GetName() == NodeID)
        {
            NodeToDelete = Node;
            break;
        }
    }
    
    if (!NodeToDelete)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node with ID '%s' not found"), *NodeID));
    }
    
    // Safety check: Can the user delete this node?
    if (!NodeToDelete->CanUserDeleteNode())
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node '%s' cannot be deleted (protected engine node)"), *NodeID));
    }
    
    // Collect information about disconnected pins for response
    TArray<TSharedPtr<FJsonValue>> DisconnectedPins;
    
    if (DisconnectPins)
    {
        // Disconnect all pins before deletion
        for (UEdGraphPin* Pin : NodeToDelete->Pins)
        {
            if (Pin && Pin->LinkedTo.Num() > 0)
            {
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    TSharedPtr<FJsonObject> PinInfo = MakeShareable(new FJsonObject);
                    PinInfo->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
                    PinInfo->SetStringField(TEXT("pin_type"), Pin->PinType.PinCategory.ToString());
                    PinInfo->SetStringField(TEXT("linked_node"), LinkedPin->GetOwningNode()->GetName());
                    PinInfo->SetStringField(TEXT("linked_pin"), LinkedPin->PinName.ToString());
                    DisconnectedPins.Add(MakeShareable(new FJsonValueObject(PinInfo)));
                }
                
                // Break all connections
                Pin->BreakAllPinLinks();
            }
        }
    }
    
    // Store node information before deletion
    FString NodeType = NodeToDelete->GetClass()->GetName();
    
    // Remove the node from the graph
    EventGraph->RemoveNode(NodeToDelete, true);
    
    // Mark Blueprint as modified and recompile
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    
    // Create success response
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Result->SetStringField(TEXT("node_id"), NodeID);
    Result->SetStringField(TEXT("node_type"), NodeType);
    Result->SetArrayField(TEXT("disconnected_pins"), DisconnectedPins);
    Result->SetBoolField(TEXT("pins_disconnected"), DisconnectPins);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Node '%s' successfully deleted from Blueprint '%s'"), *NodeID, *BlueprintName));
    
    TSharedPtr<FJsonObject> SafetyChecks = MakeShareable(new FJsonObject);
    SafetyChecks->SetBoolField(TEXT("can_delete_check_passed"), true);
    SafetyChecks->SetBoolField(TEXT("is_protected_node"), false);
    SafetyChecks->SetNumberField(TEXT("pins_disconnected_count"), DisconnectedPins.Num());
    Result->SetObjectField(TEXT("safety_checks"), SafetyChecks);
    
    return Result;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleDeleteBlueprintEventNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    FString EventName;
    bool RemoveCustomEventsOnly = true;
    
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing blueprint_name parameter"));
    }
    
    if (!Params->TryGetStringField(TEXT("event_name"), EventName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing event_name parameter"));
    }
    
    // Optional parameter with default
    Params->TryGetBoolField(TEXT("remove_custom_events_only"), RemoveCustomEventsOnly);
    
    // Find the Blueprint
    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }
    
    // Find the Event Graph
    UEdGraph* EventGraph = nullptr;
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph && Graph->GetFName() == "EventGraph")
        {
            EventGraph = Graph;
            break;
        }
    }
    
    if (!EventGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("EventGraph not found in Blueprint"));
    }
    
    // Find the event node
    UK2Node_Event* EventNode = nullptr;
    UK2Node_CustomEvent* CustomEventNode = nullptr;
    FString EventType = TEXT("Unknown");
    
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (UK2Node_Event* Event = Cast<UK2Node_Event>(Node))
        {
            FString NodeEventName = Event->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
            if (NodeEventName.Contains(EventName) || Event->EventReference.GetMemberName().ToString() == EventName)
            {
                EventNode = Event;
                
                // Check if it's a custom event
                if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Event))
                {
                    CustomEventNode = CustomEvent;
                    EventType = TEXT("Custom");
                }
                else
                {
                    EventType = TEXT("Engine");
                }
                break;
            }
        }
    }
    
    if (!EventNode)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Event '%s' not found in Blueprint"), *EventName));
    }
    
    // Safety check: Protect engine events if safety is enabled
    if (RemoveCustomEventsOnly && EventType == TEXT("Engine"))
    {
        // Check for protected engine events
        FString EventMemberName = EventNode->EventReference.GetMemberName().ToString();
        if (EventMemberName == TEXT("ReceiveBeginPlay") || 
            EventMemberName == TEXT("ReceiveConstruct") || 
            EventMemberName == TEXT("ReceiveTick") ||
            EventMemberName == TEXT("ReceiveEndPlay") ||
            EventMemberName.StartsWith(TEXT("InputAction")) ||
            EventMemberName.StartsWith(TEXT("InputAxis")))
        {
            return FCommonUtils::CreateErrorResponse(FString::Printf(
                TEXT("Cannot delete protected engine event '%s'. Use remove_custom_events_only=false to override (not recommended)"), 
                *EventName
            ));
        }
    }
    
    // Safety check: Can the user delete this node?
    if (!EventNode->CanUserDeleteNode())
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Event node '%s' cannot be deleted (protected)"), *EventName));
    }
    
    // Collect information about connected nodes
    TArray<TSharedPtr<FJsonValue>> ConnectedNodes;
    
    for (UEdGraphPin* Pin : EventNode->Pins)
    {
        if (Pin && Pin->LinkedTo.Num() > 0)
        {
            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                TSharedPtr<FJsonObject> NodeInfo = MakeShareable(new FJsonObject);
                NodeInfo->SetStringField(TEXT("connected_node"), LinkedPin->GetOwningNode()->GetName());
                NodeInfo->SetStringField(TEXT("connected_node_type"), LinkedPin->GetOwningNode()->GetClass()->GetName());
                NodeInfo->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
                ConnectedNodes.Add(MakeShareable(new FJsonValueObject(NodeInfo)));
            }
            
            // Break all connections
            Pin->BreakAllPinLinks();
        }
    }
    
    // Remove the event node from the graph
    EventGraph->RemoveNode(EventNode, true);
    
    // Mark Blueprint as modified and recompile
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    
    // Create success response
    TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Result->SetStringField(TEXT("event_name"), EventName);
    Result->SetStringField(TEXT("event_type"), EventType);
    Result->SetBoolField(TEXT("protection_active"), RemoveCustomEventsOnly);
    Result->SetArrayField(TEXT("connected_nodes"), ConnectedNodes);
    Result->SetNumberField(TEXT("connected_nodes_count"), ConnectedNodes.Num());
    Result->SetStringField(TEXT("message"), FString::Printf(
        TEXT("%s event '%s' successfully deleted from Blueprint '%s'"), 
        *EventType, *EventName, *BlueprintName
    ));
    
    // Add safety information
    TSharedPtr<FJsonObject> SafetyInfo = MakeShareable(new FJsonObject);
    SafetyInfo->SetBoolField(TEXT("custom_events_only"), RemoveCustomEventsOnly);
    SafetyInfo->SetBoolField(TEXT("is_custom_event"), EventType == TEXT("Custom"));
    SafetyInfo->SetBoolField(TEXT("is_protected_event"), false);  // If we got here, it wasn't protected
    Result->SetObjectField(TEXT("safety_info"), SafetyInfo);
    
    return Result;
}
