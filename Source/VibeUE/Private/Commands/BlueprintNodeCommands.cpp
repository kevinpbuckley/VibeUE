#include "Commands/BlueprintNodeCommands.h"
#include "Commands/BlueprintReflection.h"
#include "Commands/CommonUtils.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Containers/Set.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
// #include "K2Node_ForEachLoop.h"  // Commented out - header not found
#include "K2Node_Timeline.h"
#include "K2Node_MacroInstance.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "GameFramework/InputSettings.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "EdGraphSchema_K2.h"
#include "ScopedTransaction.h"
#include "Math/UnrealMathUtility.h"
#include "Internationalization/Text.h"

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
    if (CommandType == TEXT("manage_blueprint_node"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleManageBlueprintNode"));
        return HandleManageBlueprintNode(Params);
    }
    if (CommandType == TEXT("manage_blueprint_function"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleManageBlueprintFunction"));
        return HandleManageBlueprintFunction(Params);
    }
    if (CommandType == TEXT("get_available_blueprint_nodes"))
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: Calling HandleGetAvailableBlueprintNodes"));
        return HandleGetAvailableBlueprintNodes(Params);
    }

    const FString ErrorMessage = FString::Printf(TEXT("Unknown command: %s. Use manage_blueprint_node, manage_blueprint_function, or get_available_blueprint_nodes."), *CommandType);
    return FCommonUtils::CreateErrorResponse(ErrorMessage);
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleConnectBlueprintNodes(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    FString SourceNodeId;
    FString TargetNodeId;
    FString SourcePinName;
    FString TargetPinName;

    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'source_node_id' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'target_node_id' parameter"));
    }
    if (!Params->TryGetStringField(TEXT("source_pin"), SourcePinName))
    {
        if (!Params->TryGetStringField(TEXT("source_pin_name"), SourcePinName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'source_pin' parameter"));
        }
    }
    if (!Params->TryGetStringField(TEXT("target_pin"), TargetPinName))
    {
        if (!Params->TryGetStringField(TEXT("target_pin_name"), TargetPinName))
        {
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'target_pin' parameter"));
        }
    }

    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    FString ScopeError;
    UEdGraph* PreferredGraph = ResolveTargetGraph(Blueprint, Params, ScopeError);
    if (!PreferredGraph && !ScopeError.IsEmpty())
    {
        return FCommonUtils::CreateErrorResponse(ScopeError);
    }

    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, PreferredGraph, CandidateGraphs);

    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;
    UEdGraph* SourceNodeGraph = nullptr;
    UEdGraph* TargetNodeGraph = nullptr;

    ResolveNodeIdentifier(SourceNodeId, CandidateGraphs, SourceNode, SourceNodeGraph);
    ResolveNodeIdentifier(TargetNodeId, CandidateGraphs, TargetNode, TargetNodeGraph);

    if (!SourceNode || !TargetNode)
    {
        const FString AvailableNodes = DescribeAvailableNodes(CandidateGraphs);
        UE_LOG(LogVibeUE, Error, TEXT("ConnectBlueprintNodes: Failed to resolve nodes. SourceId=%s TargetId=%s. Available: %s"),
               *SourceNodeId, *TargetNodeId, *AvailableNodes);
        return FCommonUtils::CreateErrorResponse(TEXT("Source or target node not found"));
    }

    if (SourceNodeGraph != TargetNodeGraph)
    {
        const FString SourceGraphName = SourceNodeGraph ? SourceNodeGraph->GetName() : TEXT("<unknown>");
        const FString TargetGraphName = TargetNodeGraph ? TargetNodeGraph->GetName() : TEXT("<unknown>");
        UE_LOG(LogVibeUE, Error, TEXT("ConnectBlueprintNodes: Source and target nodes resolved in different graphs (Source: %s, Target: %s)."),
               *SourceGraphName, *TargetGraphName);
        return FCommonUtils::CreateErrorResponse(TEXT("Source and target nodes are not in the same graph"));
    }

    UEdGraph* TargetGraph = SourceNodeGraph ? SourceNodeGraph : PreferredGraph;
    if (!TargetGraph)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to determine target graph for connection"));
    }

    UE_LOG(LogVibeUE, Warning, TEXT("ConnectBlueprintNodes: Resolved both nodes in graph '%s'."),
           *TargetGraph->GetName());

    TSharedPtr<FJsonObject> ConnectionResult = FCommonUtils::ConnectGraphNodesWithReflection(
        TargetGraph, SourceNode, SourcePinName, TargetNode, TargetPinName);

    bool bSuccess = false;
    ConnectionResult->TryGetBoolField(TEXT("success"), bSuccess);

    if (bSuccess)
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }

    return ConnectionResult;
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

    FString ScopeError; UEdGraph* EventGraph = ResolveTargetGraph(Blueprint, Params, ScopeError);
    if (!EventGraph) return FCommonUtils::CreateErrorResponse(ScopeError);

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

    FString ScopeError; UEdGraph* EventGraph = ResolveTargetGraph(Blueprint, Params, ScopeError);
    if (!EventGraph) return FCommonUtils::CreateErrorResponse(ScopeError);

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

    FString ScopeError; UEdGraph* EventGraph = ResolveTargetGraph(Blueprint, Params, ScopeError);
    if (!EventGraph) return FCommonUtils::CreateErrorResponse(ScopeError);

    // Create a JSON array for the node GUIDs
    TArray<TSharedPtr<FJsonValue>> NodeGuidArray;
    
    UE_LOG(LogVibeUE, Warning, TEXT("MCP: FindBlueprintNodes - Searching for node type: %s"), *NodeType);
    
    // Use pure reflection-based node type resolution
    UClass* TargetNodeClass = FBlueprintReflection::ResolveNodeClass(NodeType);
    
    if (TargetNodeClass)
    {
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: FindBlueprintNodes - Resolved node class via reflection: %s"), *TargetNodeClass->GetName());
        
        // Search through nodes using reflection-based type matching
        UE_LOG(LogVibeUE, Warning, TEXT("MCP: FindBlueprintNodes - Searching %d nodes for type: %s"), EventGraph->Nodes.Num(), *TargetNodeClass->GetName());
        
        for (UEdGraphNode* Node : EventGraph->Nodes)
        {
            if (Node && Node->IsA(TargetNodeClass))
            {
                UE_LOG(LogVibeUE, Warning, TEXT("MCP: FindBlueprintNodes - Found matching node: %s"), *Node->NodeGuid.ToString());
                NodeGuidArray.Add(MakeShared<FJsonValueString>(Node->NodeGuid.ToString()));
            }
        }
    }
    else
    {
        UE_LOG(LogVibeUE, Error, TEXT("MCP: FindBlueprintNodes - Failed to resolve node type via reflection: %s"), *NodeType);
        
        // Since we don't want hardcoded fallbacks, return an error if reflection fails
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown node type '%s' - reflection system could not resolve this node type"), *NodeType));
    }
    
    UE_LOG(LogVibeUE, Warning, TEXT("MCP: FindBlueprintNodes - Found %d matching nodes for type: %s"), NodeGuidArray.Num(), *NodeType);
    
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

    FString ScopeError; UEdGraph* EventGraph = ResolveTargetGraph(Blueprint, Params, ScopeError);
    if (!EventGraph) return FCommonUtils::CreateErrorResponse(ScopeError);

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
    
    // Enhanced: Use ResolveTargetGraph to support function scope
    FString GraphError;
    UEdGraph* TargetGraph = ResolveTargetGraph(Blueprint, Params, GraphError);
    if (!TargetGraph)
    {
        UE_LOG(LogVibeUE, Error, TEXT("MCP: HandleGetNodeDetails - Failed to resolve target graph: %s"), *GraphError);
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to resolve target graph: %s"), *GraphError));
    }
    
    UE_LOG(LogVibeUE, Warning, TEXT("MCP: HandleGetNodeDetails - Using graph: %s"), *TargetGraph->GetName());
    UEdGraphNode* Found = nullptr;
    for (UEdGraphNode* Node : TargetGraph->Nodes)
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

// --- Unified Function Management (Phase 1) ---
TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleManageBlueprintFunction(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName; FString Action;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    if (!Params->TryGetStringField(TEXT("action"), Action))
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'action' parameter"));

    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));

    // Core CRUD
    if (Action.Equals(TEXT("list"), ESearchCase::IgnoreCase))
        return BuildFunctionSummary(Blueprint);
    if (Action.Equals(TEXT("get"), ESearchCase::IgnoreCase))
    {
        FString FunctionName; if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
        return BuildSingleFunctionInfo(Blueprint, FunctionName);
    }
    if (Action.Equals(TEXT("create"), ESearchCase::IgnoreCase))
    {
        FString FunctionName; if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
        return CreateFunctionGraph(Blueprint, FunctionName);
    }
    if (Action.Equals(TEXT("delete"), ESearchCase::IgnoreCase))
    {
        FString FunctionName; if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' parameter"));
        FString Err; if (!RemoveFunctionGraph(Blueprint, FunctionName, Err))
            return FCommonUtils::CreateErrorResponse(Err);
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"), true); R->SetStringField(TEXT("function_name"), FunctionName); return R;
    }

    // Parameter operations
    if (Action.Equals(TEXT("list_params"), ESearchCase::IgnoreCase))
    {
        FString FunctionName; if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name' for list_params"));
        UEdGraph* Graph=nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), true);
        R->SetStringField(TEXT("function_name"), FunctionName);
        R->SetArrayField(TEXT("parameters"), ListFunctionParameters(Blueprint, Graph));
        return R;
    }
    if (Action.Equals(TEXT("add_param"), ESearchCase::IgnoreCase))
    {
        FString FunctionName, ParamName, TypeDesc, Direction;
        if (!Params->TryGetStringField(TEXT("function_name"), FunctionName)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name'"));
        if (!Params->TryGetStringField(TEXT("param_name"), ParamName)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'param_name'"));
        if (!Params->TryGetStringField(TEXT("type"), TypeDesc)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'type'"));
        if (!Params->TryGetStringField(TEXT("direction"), Direction)) Direction = TEXT("input");
        UEdGraph* Graph=nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        return AddFunctionParameter(Blueprint, Graph, ParamName, TypeDesc, Direction);
    }
    if (Action.Equals(TEXT("remove_param"), ESearchCase::IgnoreCase))
    {
        FString FunctionName, ParamName, Direction;
        if (!Params->TryGetStringField(TEXT("function_name"), FunctionName)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name'"));
        if (!Params->TryGetStringField(TEXT("param_name"), ParamName)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'param_name'"));
        if (!Params->TryGetStringField(TEXT("direction"), Direction)) Direction = TEXT("input");
        UEdGraph* Graph=nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        return RemoveFunctionParameter(Blueprint, Graph, ParamName, Direction);
    }
    if (Action.Equals(TEXT("update_param"), ESearchCase::IgnoreCase))
    {
        FString FunctionName, ParamName, Direction, NewType, NewName;
        if (!Params->TryGetStringField(TEXT("function_name"), FunctionName)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name'"));
        if (!Params->TryGetStringField(TEXT("param_name"), ParamName)) return FCommonUtils::CreateErrorResponse(TEXT("Missing 'param_name'"));
        if (!Params->TryGetStringField(TEXT("direction"), Direction)) Direction = TEXT("input");
        Params->TryGetStringField(TEXT("new_type"), NewType);
        Params->TryGetStringField(TEXT("new_name"), NewName);
        UEdGraph* Graph=nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        return UpdateFunctionParameter(Blueprint, Graph, ParamName, Direction, NewType, NewName);
    }
    if (Action.Equals(TEXT("update_properties"), ESearchCase::IgnoreCase))
    {
        FString FunctionName; if (!Params->TryGetStringField(TEXT("function_name"), FunctionName))
            return FCommonUtils::CreateErrorResponse(TEXT("Missing 'function_name'"));
        UEdGraph* Graph=nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
            return FCommonUtils::CreateErrorResponse(TEXT("Function not found"));
        return UpdateFunctionProperties(Blueprint, Graph, Params);
    }

    return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown function action: %s"), *Action));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleManageBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    FString Action;
    if (!Params->TryGetStringField(TEXT("action"), Action))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'action' parameter"));
    }

    const FString NormalizedAction = Action.ToLower();

    if (NormalizedAction == TEXT("list") || NormalizedAction == TEXT("list_nodes") || NormalizedAction == TEXT("enumerate"))
    {
        return HandleListEventGraphNodes(Params);
    }
    if (NormalizedAction == TEXT("find") || NormalizedAction == TEXT("search") || NormalizedAction == TEXT("locate"))
    {
        return HandleFindBlueprintNodes(Params);
    }
    if (NormalizedAction == TEXT("add") || NormalizedAction == TEXT("create") || NormalizedAction == TEXT("spawn"))
    {
        return HandleAddBlueprintNode(Params);
    }
    if (NormalizedAction == TEXT("delete") || NormalizedAction == TEXT("remove") || NormalizedAction == TEXT("destroy"))
    {
        return HandleDeleteBlueprintNode(Params);
    }
    if (NormalizedAction == TEXT("connect") || NormalizedAction == TEXT("link") || NormalizedAction == TEXT("wire"))
    {
        return HandleConnectBlueprintNodes(Params);
    }
    if (NormalizedAction == TEXT("move") || NormalizedAction == TEXT("reposition") || NormalizedAction == TEXT("translate") || NormalizedAction == TEXT("set_position"))
    {
        return HandleMoveBlueprintNode(Params);
    }
    if (NormalizedAction == TEXT("details") || NormalizedAction == TEXT("get") || NormalizedAction == TEXT("info"))
    {
        return HandleGetNodeDetails(Params);
    }
    if (NormalizedAction == TEXT("available") || NormalizedAction == TEXT("catalog") || NormalizedAction == TEXT("palette"))
    {
        return HandleGetAvailableBlueprintNodes(Params);
    }
    if (NormalizedAction == TEXT("set_property") || NormalizedAction == TEXT("update_property"))
    {
        return HandleSetBlueprintNodeProperty(Params);
    }
    if (NormalizedAction == TEXT("get_property") || NormalizedAction == TEXT("property"))
    {
        return HandleGetBlueprintNodeProperty(Params);
    }
    if (NormalizedAction == TEXT("list_custom_events") || NormalizedAction == TEXT("events"))
    {
        return HandleListCustomEvents(Params);
    }

    return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown node action: %s"), *Action));
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::BuildFunctionSummary(UBlueprint* Blueprint)
{
    TArray<TSharedPtr<FJsonValue>> Funcs;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!Graph) continue;
        TSharedPtr<FJsonObject> F = MakeShared<FJsonObject>();
        F->SetStringField(TEXT("name"), Graph->GetName());
        F->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
        Funcs.Add(MakeShared<FJsonValueObject>(F));
    }
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetArrayField(TEXT("functions"), Funcs);
    return Result;
}

bool FBlueprintNodeCommands::FindUserFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName, UEdGraph*& OutGraph) const
{
    OutGraph = nullptr; if (!Blueprint) return false;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetName().Equals(FunctionName, ESearchCase::IgnoreCase)) { OutGraph = Graph; return true; }
    }
    return false;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::BuildSingleFunctionInfo(UBlueprint* Blueprint, const FString& FunctionName)
{
    UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph))
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Function not found: %s"), *FunctionName));
    TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
    Info->SetStringField(TEXT("name"), FunctionName);
    Info->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
    Info->SetStringField(TEXT("graph_guid"), Graph->GraphGuid.ToString());
    return Info;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::CreateFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName)
{
    UEdGraph* Existing = nullptr; if (FindUserFunctionGraph(Blueprint, FunctionName, Existing))
        return FCommonUtils::CreateErrorResponse(TEXT("Function already exists"));
    // Create new graph then add as function
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(*FunctionName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    if (!NewGraph)
        return FCommonUtils::CreateErrorResponse(TEXT("Failed to allocate new function graph"));
    // Use non-templated form: explicitly specify nullptr as UFunction signature (template parameter cannot deduce from nullptr directly)
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, NewGraph, /*bIsUserCreated*/ true, (UFunction*)nullptr);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Res = MakeShared<FJsonObject>();
    Res->SetBoolField(TEXT("success"), true);
    Res->SetStringField(TEXT("function_name"), FunctionName);
    Res->SetStringField(TEXT("graph_guid"), NewGraph->GraphGuid.ToString());
    return Res;
}

bool FBlueprintNodeCommands::RemoveFunctionGraph(UBlueprint* Blueprint, const FString& FunctionName, FString& OutError)
{
    UEdGraph* Graph = nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, Graph)) { OutError = TEXT("Function not found"); return false; }
    FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph, EGraphRemoveFlags::Recompile);
    return true;
}

TArray<TSharedPtr<FJsonValue>> FBlueprintNodeCommands::ListFunctionParameters(UBlueprint* Blueprint, UEdGraph* FunctionGraph) const
{
    TArray<TSharedPtr<FJsonValue>> Result;
    if (!Blueprint || !FunctionGraph) return Result;
    UK2Node_FunctionEntry* EntryNode = nullptr;
    TArray<UK2Node_FunctionResult*> ResultNodes;
    for (UEdGraphNode* Node : FunctionGraph->Nodes)
    {
        if (UK2Node_FunctionEntry* AsEntry = Cast<UK2Node_FunctionEntry>(Node)) { EntryNode = AsEntry; }
        else if (UK2Node_FunctionResult* AsRes = Cast<UK2Node_FunctionResult>(Node)) { ResultNodes.Add(AsRes); }
    }
    if (!EntryNode) return Result; // malformed function graph

    auto SerializePin = [](const UEdGraphPin* Pin, const FString& Dir)->TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
        P->SetStringField(TEXT("name"), Pin->GetFName().ToString());
        P->SetStringField(TEXT("direction"), Dir);
        FString TypeStr = Pin->PinType.PinCategory.ToString();
        if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object && Pin->PinType.PinSubCategoryObject.IsValid())
        { TypeStr = FString::Printf(TEXT("object:%s"), *Pin->PinType.PinSubCategoryObject->GetName()); }
        else if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && Pin->PinType.PinSubCategoryObject.IsValid())
        { TypeStr = FString::Printf(TEXT("struct:%s"), *Pin->PinType.PinSubCategoryObject->GetName()); }
        if (Pin->PinType.ContainerType == EPinContainerType::Array)
        { TypeStr = FString::Printf(TEXT("array<%s>"), *TypeStr); }
        P->SetStringField(TEXT("type"), TypeStr);
        return P;
    };

    // Inputs (entry node outputs)
    for (UEdGraphPin* Pin : EntryNode->Pins)
    {
        if (Pin->Direction == EGPD_Output && Pin->PinName != UEdGraphSchema_K2::PN_Then)
        {
            Result.Add(MakeShared<FJsonValueObject>(SerializePin(Pin, TEXT("input"))));
        }
    }
    // Return / out params (result node inputs)
    for (UK2Node_FunctionResult* RNode : ResultNodes)
    {
        for (UEdGraphPin* Pin : RNode->Pins)
        {
            if (Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Then)
            {
                const bool bIsReturn = (Pin->PinName == UEdGraphSchema_K2::PN_ReturnValue);
                Result.Add(MakeShared<FJsonValueObject>(SerializePin(Pin, bIsReturn ? TEXT("return") : TEXT("out"))));
            }
        }
    }
    return Result;
}

static UK2Node_FunctionEntry* FindFunctionEntry(UEdGraph* Graph)
{
    for (UEdGraphNode* Node : Graph->Nodes)
    { if (UK2Node_FunctionEntry* E = Cast<UK2Node_FunctionEntry>(Node)) return E; }
    return nullptr;
}
static UK2Node_FunctionResult* FindOrCreateResultNode(UBlueprint* Blueprint, UEdGraph* Graph)
{
    for (UEdGraphNode* Node : Graph->Nodes) if (UK2Node_FunctionResult* R = Cast<UK2Node_FunctionResult>(Node)) return R;
    FGraphNodeCreator<UK2Node_FunctionResult> Creator(*Graph); UK2Node_FunctionResult* NewNode = Creator.CreateNode(); Creator.Finalize();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    return NewNode;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::AddFunctionParameter(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& ParamName, const FString& TypeDesc, const FString& Direction)
{
    if (!Blueprint || !FunctionGraph) return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint/graph"));
    FString DirLower = Direction.ToLower();
    if (!(DirLower == TEXT("input") || DirLower == TEXT("out") || DirLower == TEXT("return")))
        return FCommonUtils::CreateErrorResponse(TEXT("Invalid direction (expected input|out|return)"));

    // Check existing
    auto Existing = ListFunctionParameters(Blueprint, FunctionGraph);
    for (auto& V : Existing)
    {
        const TSharedPtr<FJsonObject>* ObjPtr; if (V->TryGetObject(ObjPtr))
        { if ((*ObjPtr)->GetStringField(TEXT("name")).Equals(ParamName, ESearchCase::IgnoreCase)) return FCommonUtils::CreateErrorResponse(TEXT("Parameter already exists")); }
    }

    FEdGraphPinType PinType; FString TypeErr; if (!ParseTypeDescriptor(TypeDesc, PinType, TypeErr)) return FCommonUtils::CreateErrorResponse(TypeErr);

    UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph);
    if (!Entry) return FCommonUtils::CreateErrorResponse(TEXT("Function entry node not found"));

    if (DirLower == TEXT("input"))
    {
        UEdGraphPin* NewPin = Entry->CreateUserDefinedPin(FName(*ParamName), PinType, EGPD_Output, false);
        if (!NewPin) return FCommonUtils::CreateErrorResponse(TEXT("Failed to create input pin"));
    }
    else // out or return
    {
        UK2Node_FunctionResult* ResultNode = FindOrCreateResultNode(Blueprint, FunctionGraph);
        if (!ResultNode) return FCommonUtils::CreateErrorResponse(TEXT("Failed to resolve/create result node"));
        FName NewPinName = (DirLower == TEXT("return")) ? UEdGraphSchema_K2::PN_ReturnValue : FName(*ParamName);
        if (DirLower == TEXT("return"))
        {
            // If return already exists, error
            for (UEdGraphPin* P : ResultNode->Pins)
            { if (P->PinName == UEdGraphSchema_K2::PN_ReturnValue) return FCommonUtils::CreateErrorResponse(TEXT("Return value already exists")); }
        }
        UEdGraphPin* NewPin = ResultNode->CreateUserDefinedPin(NewPinName, PinType, EGPD_Input, false);
        if (!NewPin) return FCommonUtils::CreateErrorResponse(TEXT("Failed to create result pin"));
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetArrayField(TEXT("parameters"), ListFunctionParameters(Blueprint, FunctionGraph));
    return R;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::RemoveFunctionParameter(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& ParamName, const FString& Direction)
{
    if (!Blueprint || !FunctionGraph) return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint/graph"));
    FString DirLower = Direction.ToLower();
    bool bFound = false;
    if (DirLower == TEXT("input"))
    {
        if (UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph))
        {
            for (int32 i=Entry->Pins.Num()-1;i>=0;--i)
            {
                UEdGraphPin* P = Entry->Pins[i];
                if (P->Direction==EGPD_Output && P->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase))
                { P->BreakAllPinLinks(); Entry->Pins.RemoveAt(i); bFound=true; }
            }
        }
    }
    else // out or return
    {
        for (UEdGraphNode* Node : FunctionGraph->Nodes)
        {
            if (UK2Node_FunctionResult* RNode = Cast<UK2Node_FunctionResult>(Node))
            {
                for (int32 i=RNode->Pins.Num()-1;i>=0;--i)
                {
                    UEdGraphPin* P = RNode->Pins[i];
                    if (P->Direction==EGPD_Input)
                    {
                        bool bNameMatch = false;
                        if (DirLower==TEXT("return")) bNameMatch = (P->PinName == UEdGraphSchema_K2::PN_ReturnValue);
                        else bNameMatch = P->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase);
                        if (bNameMatch)
                        { P->BreakAllPinLinks(); RNode->Pins.RemoveAt(i); bFound=true; }
                    }
                }
            }
        }
    }
    if (!bFound) return FCommonUtils::CreateErrorResponse(TEXT("Parameter not found"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), true);
    R->SetArrayField(TEXT("parameters"), ListFunctionParameters(Blueprint, FunctionGraph));
    return R;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::UpdateFunctionParameter(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const FString& ParamName, const FString& Direction, const FString& NewType, const FString& NewName)
{
    if (!Blueprint || !FunctionGraph) return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint/graph"));
    FString DirLower = Direction.ToLower();
    FEdGraphPinType NewPinType; bool bTypeChange=false; if (!NewType.IsEmpty()) { FString Err; if (!ParseTypeDescriptor(NewType, NewPinType, Err)) return FCommonUtils::CreateErrorResponse(Err); bTypeChange=true; }
    bool bModified=false;

    auto ApplyChanges = [&](UEdGraphPin* P)
    {
        if (bTypeChange) P->PinType = NewPinType;
        if (!NewName.IsEmpty() && P->PinName.ToString() != NewName && P->PinName != UEdGraphSchema_K2::PN_ReturnValue)
        { P->PinName = FName(*NewName); }
        bModified=true;
    };

    if (DirLower==TEXT("input"))
    {
        if (UK2Node_FunctionEntry* Entry = FindFunctionEntry(FunctionGraph))
        {
            for (UEdGraphPin* P : Entry->Pins)
            {
                if (P->Direction==EGPD_Output && P->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase)) { ApplyChanges(P); }
            }
        }
    }
    else
    {
        for (UEdGraphNode* Node : FunctionGraph->Nodes)
        {
            if (UK2Node_FunctionResult* RNode = Cast<UK2Node_FunctionResult>(Node))
            {
                for (UEdGraphPin* P : RNode->Pins)
                {
                    if (P->Direction==EGPD_Input)
                    {
                        bool bMatch=false;
                        if (DirLower==TEXT("return")) bMatch = (P->PinName==UEdGraphSchema_K2::PN_ReturnValue);
                        else bMatch = P->PinName.ToString().Equals(ParamName, ESearchCase::IgnoreCase);
                        if (bMatch) { ApplyChanges(P); }
                    }
                }
            }
        }
    }
    if (!bModified) return FCommonUtils::CreateErrorResponse(TEXT("Parameter not found"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetBoolField(TEXT("success"), true); R->SetArrayField(TEXT("parameters"), ListFunctionParameters(Blueprint, FunctionGraph)); return R;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::UpdateFunctionProperties(UBlueprint* Blueprint, UEdGraph* FunctionGraph, const TSharedPtr<FJsonObject>& Params)
{
    if (!Blueprint || !FunctionGraph) return FCommonUtils::CreateErrorResponse(TEXT("Invalid blueprint or function graph"));
    // Only 'is_pure' supported currently; others require metadata API adaptation
    bool bPure=false; bool bHas=false; bHas = Params->TryGetBoolField(TEXT("is_pure"), bPure);
    if (bHas)
    {
        // Pure flag requires locating entry node & adjusting function flags post-compile; stub for now
        return FCommonUtils::CreateErrorResponse(TEXT("Setting is_pure not yet implemented"));
    }
    return FCommonUtils::CreateErrorResponse(TEXT("No supported properties provided"));
}

bool FBlueprintNodeCommands::ParseTypeDescriptor(const FString& TypeDesc, FEdGraphPinType& OutType, FString& OutError) const
{
    FString Lower = TypeDesc.ToLower(); OutType.ResetToDefaults();
    if (Lower.StartsWith(TEXT("array<")) && Lower.EndsWith(TEXT(">")))
    { FString Inner=TypeDesc.Mid(6, TypeDesc.Len()-7); FEdGraphPinType InnerType; FString Err; if (!ParseTypeDescriptor(Inner, InnerType, Err)) { OutError=Err; return false;} OutType=InnerType; OutType.ContainerType=EPinContainerType::Array; return true; }
    if (Lower==TEXT("bool")) { OutType.PinCategory=UEdGraphSchema_K2::PC_Boolean; return true; }
    if (Lower==TEXT("int")||Lower==TEXT("int32")) { OutType.PinCategory=UEdGraphSchema_K2::PC_Int; return true; }
    if (Lower==TEXT("float")) { OutType.PinCategory=UEdGraphSchema_K2::PC_Float; return true; }
    if (Lower==TEXT("string")) { OutType.PinCategory=UEdGraphSchema_K2::PC_String; return true; }
    if (Lower==TEXT("name")) { OutType.PinCategory=UEdGraphSchema_K2::PC_Name; return true; }
    if (Lower==TEXT("vector")) { OutType.PinCategory=UEdGraphSchema_K2::PC_Struct; OutType.PinSubCategoryObject=TBaseStructure<FVector>::Get(); return true; }
    if (Lower==TEXT("rotator")) { OutType.PinCategory=UEdGraphSchema_K2::PC_Struct; OutType.PinSubCategoryObject=TBaseStructure<FRotator>::Get(); return true; }
    if (Lower==TEXT("transform")) { OutType.PinCategory=UEdGraphSchema_K2::PC_Struct; OutType.PinSubCategoryObject=TBaseStructure<FTransform>::Get(); return true; }
    if (Lower.StartsWith(TEXT("object:"))) { FString ClassName=TypeDesc.Mid(7); UClass* C=FindFirstObject<UClass>(*ClassName); if(!C){OutError=FString::Printf(TEXT("Class '%s' not found"),*ClassName);return false;} OutType.PinCategory=UEdGraphSchema_K2::PC_Object; OutType.PinSubCategoryObject=C; return true; }
    if (Lower.StartsWith(TEXT("struct:"))) { FString StructName=TypeDesc.Mid(7); UScriptStruct* S=FindFirstObject<UScriptStruct>(*StructName); if(!S){OutError=FString::Printf(TEXT("Struct '%s' not found"),*StructName);return false;} OutType.PinCategory=UEdGraphSchema_K2::PC_Struct; OutType.PinSubCategoryObject=S; return true; }
    OutError = FString::Printf(TEXT("Unsupported type descriptor '%s'"), *TypeDesc); return false;
}

UEdGraph* FBlueprintNodeCommands::ResolveTargetGraph(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params, FString& OutError) const
{
    OutError.Reset(); if (!Blueprint) { OutError=TEXT("Invalid blueprint"); return nullptr; }
    FString Scope; if (!Params->TryGetStringField(TEXT("graph_scope"), Scope) || Scope.IsEmpty())
        return FCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (Scope.Equals(TEXT("event"), ESearchCase::IgnoreCase)) return FCommonUtils::FindOrCreateEventGraph(Blueprint);
    if (Scope.Equals(TEXT("function"), ESearchCase::IgnoreCase))
    { FString FunctionName; if (!Params->TryGetStringField(TEXT("function_name"), FunctionName)) { OutError=TEXT("Missing 'function_name' for function scope"); return nullptr; } UEdGraph* G=nullptr; if (!FindUserFunctionGraph(Blueprint, FunctionName, G)) { OutError=TEXT("Function not found"); return nullptr; } return G; }
    OutError = FString::Printf(TEXT("Unsupported graph_scope '%s'"), *Scope); return nullptr;
}

void FBlueprintNodeCommands::GatherCandidateGraphs(UBlueprint* Blueprint, UEdGraph* PreferredGraph, TArray<UEdGraph*>& OutGraphs) const
{
    OutGraphs.Reset();

    if (!Blueprint)
    {
        return;
    }

    TSet<UEdGraph*> Seen;
    auto AddGraph = [&OutGraphs, &Seen](UEdGraph* Graph)
    {
        if (Graph && !Seen.Contains(Graph))
        {
            Seen.Add(Graph);
            OutGraphs.Add(Graph);
        }
    };

    AddGraph(PreferredGraph);

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        AddGraph(Graph);
    }
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        AddGraph(Graph);
    }
    for (UEdGraph* Graph : Blueprint->MacroGraphs)
    {
        AddGraph(Graph);
    }
    for (UEdGraph* Graph : Blueprint->IntermediateGeneratedGraphs)
    {
        AddGraph(Graph);
    }
}

bool FBlueprintNodeCommands::ResolveNodeIdentifier(const FString& Identifier, const TArray<UEdGraph*>& Graphs, UEdGraphNode*& OutNode, UEdGraph*& OutGraph) const
{
    OutNode = nullptr;
    OutGraph = nullptr;

    if (Identifier.IsEmpty())
    {
        return false;
    }

    FString NormalizedIdentifier = Identifier;
    NormalizedIdentifier.ReplaceInline(TEXT("{"), TEXT(""));
    NormalizedIdentifier.ReplaceInline(TEXT("}"), TEXT(""));
    FString HyphenlessIdentifier = NormalizedIdentifier;
    HyphenlessIdentifier.ReplaceInline(TEXT("-"), TEXT(""));

    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph)
        {
            continue;
        }

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node)
            {
                continue;
            }

            FString GuidString = Node->NodeGuid.ToString();
            GuidString.ReplaceInline(TEXT("{"), TEXT(""));
            GuidString.ReplaceInline(TEXT("}"), TEXT(""));
            FString HyphenlessGuid = GuidString;
            HyphenlessGuid.ReplaceInline(TEXT("-"), TEXT(""));

            if (GuidString.Equals(NormalizedIdentifier, ESearchCase::IgnoreCase) ||
                HyphenlessGuid.Equals(HyphenlessIdentifier, ESearchCase::IgnoreCase))
            {
                OutNode = Node;
                OutGraph = Graph;
                return true;
            }

            FString LexGuidString = LexToString(Node->NodeGuid);
            FString HyphenlessLexGuid = LexGuidString;
            HyphenlessLexGuid.ReplaceInline(TEXT("-"), TEXT(""));

            if (LexGuidString.Equals(NormalizedIdentifier, ESearchCase::IgnoreCase) ||
                HyphenlessLexGuid.Equals(HyphenlessIdentifier, ESearchCase::IgnoreCase))
            {
                OutNode = Node;
                OutGraph = Graph;
                return true;
            }

            const FString NodeName = Node->GetName();
            if (NodeName.Equals(NormalizedIdentifier, ESearchCase::IgnoreCase))
            {
                OutNode = Node;
                OutGraph = Graph;
                return true;
            }

            const FString UniqueIdString = FString::FromInt(Node->GetUniqueID());
            if (UniqueIdString.Equals(NormalizedIdentifier, ESearchCase::IgnoreCase))
            {
                OutNode = Node;
                OutGraph = Graph;
                return true;
            }

            const FString TitleString = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
            if (TitleString.Equals(NormalizedIdentifier, ESearchCase::IgnoreCase))
            {
                OutNode = Node;
                OutGraph = Graph;
                return true;
            }
        }
    }

    return false;
}

FString FBlueprintNodeCommands::DescribeAvailableNodes(const TArray<UEdGraph*>& Graphs) const
{
    FString Description;

    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph)
        {
            continue;
        }

        const FString GraphName = Graph->GetName();
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node)
            {
                continue;
            }

            if (!Description.IsEmpty())
            {
                Description += TEXT(" | ");
            }

            Description += FString::Printf(TEXT("%s (Graph=%s, Guid=%s, Name=%s, UniqueId=%d)"),
                                           *Node->GetNodeTitle(ENodeTitleType::ListView).ToString(),
                                           *GraphName,
                                           *Node->NodeGuid.ToString(),
                                           *Node->GetName(),
                                           Node->GetUniqueID());
        }
    }

    return Description;
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
    
    FString ScopeError;
    UEdGraph* PreferredGraph = ResolveTargetGraph(Blueprint, Params, ScopeError);
    if (!PreferredGraph && !ScopeError.IsEmpty())
    {
        return FCommonUtils::CreateErrorResponse(ScopeError);
    }

    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, PreferredGraph, CandidateGraphs);

    UEdGraphNode* NodeToDelete = nullptr;
    UEdGraph* NodeGraph = nullptr;
    ResolveNodeIdentifier(NodeID, CandidateGraphs, NodeToDelete, NodeGraph);

    if (!NodeToDelete)
    {
        const FString AvailableNodes = DescribeAvailableNodes(CandidateGraphs);
        UE_LOG(LogVibeUE, Error, TEXT("DeleteBlueprintNode: Node '%s' not found. Candidates: %s"), *NodeID, *AvailableNodes);
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node with ID '%s' not found"), *NodeID));
    }

    if (!NodeToDelete->CanUserDeleteNode())
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node '%s' cannot be deleted (protected engine node)"), *NodeID));
    }

    TArray<TSharedPtr<FJsonValue>> DisconnectedPins;
    if (DisconnectPins)
    {
        for (UEdGraphPin* Pin : NodeToDelete->Pins)
        {
            if (Pin && Pin->LinkedTo.Num() > 0)
            {
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (!LinkedPin)
                    {
                        continue;
                    }

                    TSharedPtr<FJsonObject> PinInfo = MakeShared<FJsonObject>();
                    PinInfo->SetStringField(TEXT("pin_name"), Pin->PinName.ToString());
                    PinInfo->SetStringField(TEXT("pin_type"), Pin->PinType.PinCategory.ToString());
                    PinInfo->SetStringField(TEXT("linked_node"), LinkedPin->GetOwningNode()->GetName());
                    PinInfo->SetStringField(TEXT("linked_pin"), LinkedPin->PinName.ToString());
                    DisconnectedPins.Add(MakeShared<FJsonValueObject>(PinInfo));
                }

                Pin->BreakAllPinLinks();
            }
        }
    }

    const FString NodeType = NodeToDelete->GetClass()->GetName();
    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "DeleteBlueprintNode", "MCP Delete Blueprint Node"));

    if (NodeGraph)
    {
        NodeGraph->Modify();
    }
    NodeToDelete->Modify();

    if (NodeGraph)
    {
        NodeGraph->RemoveNode(NodeToDelete, true);
        NodeGraph->NotifyGraphChanged();
    }
    else
    {
        NodeToDelete->DestroyNode();
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Result->SetStringField(TEXT("node_guid"), NodeID);
    Result->SetStringField(TEXT("node_type"), NodeType);
    Result->SetStringField(TEXT("graph_name"), NodeGraph ? NodeGraph->GetName() : TEXT(""));
    Result->SetArrayField(TEXT("disconnected_pins"), DisconnectedPins);
    Result->SetBoolField(TEXT("pins_disconnected"), DisconnectPins);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Node '%s' successfully deleted from Blueprint '%s'"), *NodeID, *BlueprintName));

    TSharedPtr<FJsonObject> SafetyChecks = MakeShared<FJsonObject>();
    SafetyChecks->SetBoolField(TEXT("can_delete_check_passed"), true);
    SafetyChecks->SetBoolField(TEXT("is_protected_node"), false);
    SafetyChecks->SetNumberField(TEXT("pins_disconnected_count"), DisconnectedPins.Num());
    Result->SetObjectField(TEXT("safety_checks"), SafetyChecks);

    return Result;
}

TSharedPtr<FJsonObject> FBlueprintNodeCommands::HandleMoveBlueprintNode(const TSharedPtr<FJsonObject>& Params)
{
    FString BlueprintName;
    FString NodeID;

    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    if (!Params->TryGetStringField(TEXT("node_id"), NodeID))
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'node_id' parameter"));
    }

    FVector2D NewPosition(0.0f, 0.0f);
    bool bHasPosition = false;

    auto TryLoadPosition = [&](const FString& FieldName) -> bool
    {
        if (Params->HasField(FieldName))
        {
            NewPosition = FCommonUtils::GetVector2DFromJson(Params, FieldName);
            return true;
        }
        return false;
    };

    bHasPosition = TryLoadPosition(TEXT("position")) ||
                   TryLoadPosition(TEXT("node_position")) ||
                   TryLoadPosition(TEXT("new_position"));

    if (!bHasPosition)
    {
        double PosX = 0.0;
        double PosY = 0.0;
        const bool bHasX = Params->TryGetNumberField(TEXT("x"), PosX) || Params->TryGetNumberField(TEXT("pos_x"), PosX);
        const bool bHasY = Params->TryGetNumberField(TEXT("y"), PosY) || Params->TryGetNumberField(TEXT("pos_y"), PosY);

        if (bHasX && bHasY)
        {
            NewPosition.X = static_cast<float>(PosX);
            NewPosition.Y = static_cast<float>(PosY);
            bHasPosition = true;
        }
    }

    if (!bHasPosition)
    {
        return FCommonUtils::CreateErrorResponse(TEXT("Missing 'position' (array) or 'x'/'y' fields for node move"));
    }

    UBlueprint* Blueprint = FCommonUtils::FindBlueprint(BlueprintName);
    if (!Blueprint)
    {
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint '%s' not found"), *BlueprintName));
    }

    FString ScopeError;
    UEdGraph* PreferredGraph = ResolveTargetGraph(Blueprint, Params, ScopeError);
    if (!PreferredGraph && !ScopeError.IsEmpty())
    {
        return FCommonUtils::CreateErrorResponse(ScopeError);
    }

    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, PreferredGraph, CandidateGraphs);

    UEdGraphNode* TargetNode = nullptr;
    UEdGraph* NodeGraph = nullptr;
    ResolveNodeIdentifier(NodeID, CandidateGraphs, TargetNode, NodeGraph);

    if (!TargetNode)
    {
        const FString AvailableNodes = DescribeAvailableNodes(CandidateGraphs);
        UE_LOG(LogVibeUE, Error, TEXT("MoveBlueprintNode: Node '%s' not found. Candidates: %s"), *NodeID, *AvailableNodes);
        return FCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Node with ID '%s' not found"), *NodeID));
    }

    if (!NodeGraph)
    {
        NodeGraph = PreferredGraph;
    }

    const int32 PreviousX = TargetNode->NodePosX;
    const int32 PreviousY = TargetNode->NodePosY;
    const int32 RoundedX = FMath::RoundToInt(NewPosition.X);
    const int32 RoundedY = FMath::RoundToInt(NewPosition.Y);

    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "MoveBlueprintNode", "MCP Move Blueprint Node"));
    if (NodeGraph)
    {
        NodeGraph->Modify();
    }
    TargetNode->Modify();

    TargetNode->NodePosX = RoundedX;
    TargetNode->NodePosY = RoundedY;

    if (NodeGraph)
    {
        NodeGraph->NotifyGraphChanged();
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("blueprint_name"), BlueprintName);
    Result->SetStringField(TEXT("node_id"), NodeID);
    Result->SetStringField(TEXT("graph_name"), NodeGraph ? NodeGraph->GetName() : TEXT(""));
    Result->SetNumberField(TEXT("previous_x"), PreviousX);
    Result->SetNumberField(TEXT("previous_y"), PreviousY);
    Result->SetNumberField(TEXT("new_x"), RoundedX);
    Result->SetNumberField(TEXT("new_y"), RoundedY);
    Result->SetStringField(TEXT("message"), FString::Printf(TEXT("Node '%s' moved to (%d, %d)"), *NodeID, RoundedX, RoundedY));

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
