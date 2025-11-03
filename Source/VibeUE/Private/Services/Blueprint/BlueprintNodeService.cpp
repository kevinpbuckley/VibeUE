#include "Services/Blueprint/BlueprintNodeService.h"
#include "Services/Blueprint/BlueprintNodeServiceHelpers.h"
#include "Core/ErrorCodes.h"
#include "Commands/CommonUtils.h"
#include "Commands/BlueprintReflection.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintNodeService, Log, All);

FBlueprintNodeService::FBlueprintNodeService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

// Node lifecycle
TResult<UK2Node*> FBlueprintNodeService::CreateNode(UBlueprint* Blueprint, const FString& GraphName, 
                                                     const FString& NodeType, const FVector2D& Position)
{
    if (!Blueprint)
    {
        return TResult<UK2Node*>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null")
        );
    }

    auto ValidationResult = ValidateString(NodeType, TEXT("NodeType"));
    if (ValidationResult.IsError())
    {
        return TResult<UK2Node*>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    // Find or use default graph
    FString Error;
    UEdGraph* TargetGraph = FBlueprintNodeServiceHelpers::ResolveTargetGraph(Blueprint, GraphName, Error);
    if (!TargetGraph)
    {
        return TResult<UK2Node*>::Error(
            VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
            Error.IsEmpty() ? TEXT("Failed to resolve target graph") : Error
        );
    }

    // Use reflection system to create the node
    TSharedPtr<FJsonObject> NodeParams = MakeShared<FJsonObject>();
    NodeParams->SetNumberField(TEXT("x"), Position.X);
    NodeParams->SetNumberField(TEXT("y"), Position.Y);
    
    UK2Node* NewNode = FBlueprintReflection::CreateNodeFromIdentifier(Blueprint, NodeType, NodeParams);
    if (!NewNode)
    {
        return TResult<UK2Node*>::Error(
            VibeUE::ErrorCodes::NODE_CREATE_FAILED,
            FString::Printf(TEXT("Failed to create node of type '%s'"), *NodeType)
        );
    }

    // Set position
    NewNode->NodePosX = FMath::RoundToInt(Position.X);
    NewNode->NodePosY = FMath::RoundToInt(Position.Y);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    
    return TResult<UK2Node*>::Success(NewNode);
}

TResult<void> FBlueprintNodeService::DeleteNode(UBlueprint* Blueprint, const FString& NodeId)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null")
        );
    }

    auto ValidationResult = ValidateString(NodeId, TEXT("NodeId"));
    if (ValidationResult.IsError())
    {
        return TResult<void>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    // Gather all graphs
    TArray<UEdGraph*> CandidateGraphs;
    FBlueprintNodeServiceHelpers::GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);

    UEdGraphNode* NodeToDelete = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!FBlueprintNodeServiceHelpers::ResolveNodeIdentifier(NodeId, CandidateGraphs, NodeToDelete, NodeGraph))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::NODE_NOT_FOUND,
            FString::Printf(TEXT("Node with ID '%s' not found"), *NodeId)
        );
    }

    if (!NodeToDelete->CanUserDeleteNode())
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::NODE_DELETE_FAILED,
            FString::Printf(TEXT("Node '%s' cannot be deleted (protected engine node)"), *NodeId)
        );
    }

    // Disconnect all pins
    for (UEdGraphPin* Pin : NodeToDelete->Pins)
    {
        if (Pin && Pin->LinkedTo.Num() > 0)
        {
            Pin->BreakAllPinLinks();
        }
    }

    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "DeleteBlueprintNode", "Delete Blueprint Node"));

    if (NodeGraph)
    {
        NodeGraph->Modify();
        NodeGraph->RemoveNode(NodeToDelete, true);
        NodeGraph->NotifyGraphChanged();
    }
    else
    {
        NodeToDelete->Modify();
        NodeToDelete->DestroyNode();
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    
    return TResult<void>::Success();
}

TResult<void> FBlueprintNodeService::MoveNode(UBlueprint* Blueprint, const FString& NodeId, const FVector2D& Position)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null")
        );
    }

    auto ValidationResult = ValidateString(NodeId, TEXT("NodeId"));
    if (ValidationResult.IsError())
    {
        return TResult<void>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    // Gather all graphs
    TArray<UEdGraph*> CandidateGraphs;
    FBlueprintNodeServiceHelpers::GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);

    UEdGraphNode* TargetNode = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!FBlueprintNodeServiceHelpers::ResolveNodeIdentifier(NodeId, CandidateGraphs, TargetNode, NodeGraph))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::NODE_NOT_FOUND,
            FString::Printf(TEXT("Node with ID '%s' not found"), *NodeId)
        );
    }

    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "MoveBlueprintNode", "Move Blueprint Node"));
    
    if (NodeGraph)
    {
        NodeGraph->Modify();
    }
    TargetNode->Modify();

    TargetNode->NodePosX = FMath::RoundToInt(Position.X);
    TargetNode->NodePosY = FMath::RoundToInt(Position.Y);

    if (NodeGraph)
    {
        NodeGraph->NotifyGraphChanged();
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    
    return TResult<void>::Success();
}

// Pin connections
TResult<void> FBlueprintNodeService::ConnectPins(UBlueprint* Blueprint, const FString& SourceNodeId, 
                                                 const FString& SourcePinName, const FString& TargetNodeId, 
                                                 const FString& TargetPinName)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null")
        );
    }

    // Gather all graphs
    TArray<UEdGraph*> CandidateGraphs;
    FBlueprintNodeServiceHelpers::GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);

    // Find source node
    UEdGraphNode* SourceNode = nullptr;
    UEdGraph* SourceGraph = nullptr;
    if (!FBlueprintNodeServiceHelpers::ResolveNodeIdentifier(SourceNodeId, CandidateGraphs, SourceNode, SourceGraph))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::NODE_NOT_FOUND,
            FString::Printf(TEXT("Source node '%s' not found"), *SourceNodeId)
        );
    }

    // Find target node
    UEdGraphNode* TargetNode = nullptr;
    UEdGraph* TargetGraph = nullptr;
    if (!FBlueprintNodeServiceHelpers::ResolveNodeIdentifier(TargetNodeId, CandidateGraphs, TargetNode, TargetGraph))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::NODE_NOT_FOUND,
            FString::Printf(TEXT("Target node '%s' not found"), *TargetNodeId)
        );
    }

    // Find pins
    FString Error;
    UEdGraphPin* SourcePin = FBlueprintNodeServiceHelpers::FindPin(SourceNode, SourcePinName, Error);
    if (!SourcePin)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::PIN_NOT_FOUND, Error);
    }

    UEdGraphPin* TargetPin = FBlueprintNodeServiceHelpers::FindPin(TargetNode, TargetPinName, Error);
    if (!TargetPin)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::PIN_NOT_FOUND, Error);
    }

    // Validate connection
    if (!FBlueprintNodeServiceHelpers::ValidatePinConnection(SourcePin, TargetPin, Error))
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::PIN_TYPE_INCOMPATIBLE, Error);
    }

    // Make connection
    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "ConnectPins", "Connect Pins"));
    
    if (SourceGraph)
    {
        SourceGraph->Modify();
    }

    SourcePin->MakeLinkTo(TargetPin);

    if (SourceGraph)
    {
        SourceGraph->NotifyGraphChanged();
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    
    return TResult<void>::Success();
}

TResult<void> FBlueprintNodeService::DisconnectPins(UBlueprint* Blueprint, const FString& SourceNodeId,
                                                    const FString& SourcePinName)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null")
        );
    }

    // Gather all graphs
    TArray<UEdGraph*> CandidateGraphs;
    FBlueprintNodeServiceHelpers::GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);

    // Find source node
    UEdGraphNode* SourceNode = nullptr;
    UEdGraph* SourceGraph = nullptr;
    if (!FBlueprintNodeServiceHelpers::ResolveNodeIdentifier(SourceNodeId, CandidateGraphs, SourceNode, SourceGraph))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::NODE_NOT_FOUND,
            FString::Printf(TEXT("Source node '%s' not found"), *SourceNodeId)
        );
    }

    // Find pin
    FString Error;
    UEdGraphPin* SourcePin = FBlueprintNodeServiceHelpers::FindPin(SourceNode, SourcePinName, Error);
    if (!SourcePin)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::PIN_NOT_FOUND, Error);
    }

    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "DisconnectPins", "Disconnect Pins"));
    
    if (SourceGraph)
    {
        SourceGraph->Modify();
    }

    SourcePin->BreakAllPinLinks();

    if (SourceGraph)
    {
        SourceGraph->NotifyGraphChanged();
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    
    return TResult<void>::Success();
}

TResult<TArray<FPinConnectionInfo>> FBlueprintNodeService::GetPinConnections(UBlueprint* Blueprint, const FString& NodeId)
{
    if (!Blueprint)
    {
        return TResult<TArray<FPinConnectionInfo>>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null")
        );
    }

    // Gather all graphs
    TArray<UEdGraph*> CandidateGraphs;
    FBlueprintNodeServiceHelpers::GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);

    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!FBlueprintNodeServiceHelpers::ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph))
    {
        return TResult<TArray<FPinConnectionInfo>>::Error(
            VibeUE::ErrorCodes::NODE_NOT_FOUND,
            FString::Printf(TEXT("Node '%s' not found"), *NodeId)
        );
    }

    TArray<FPinConnectionInfo> Connections;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->LinkedTo.Num() > 0)
        {
            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (LinkedPin && LinkedPin->GetOwningNode())
                {
                    FPinConnectionInfo Info;
                    Info.SourceNodeId = NodeId;
                    Info.SourcePinName = Pin->PinName.ToString();
                    Info.TargetNodeId = LinkedPin->GetOwningNode()->NodeGuid.ToString();
                    Info.TargetPinName = LinkedPin->PinName.ToString();
                    Info.PinType = Pin->PinType.PinCategory.ToString();
                    Connections.Add(Info);
                }
            }
        }
    }

    return TResult<TArray<FPinConnectionInfo>>::Success(Connections);
}

// Node configuration
TResult<void> FBlueprintNodeService::SetPinDefaultValue(UBlueprint* Blueprint, const FString& NodeId,
                                                        const FString& PinName, const FString& Value)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null")
        );
    }

    // Gather all graphs
    TArray<UEdGraph*> CandidateGraphs;
    FBlueprintNodeServiceHelpers::GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);

    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!FBlueprintNodeServiceHelpers::ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::NODE_NOT_FOUND,
            FString::Printf(TEXT("Node '%s' not found"), *NodeId)
        );
    }

    FString Error;
    UEdGraphPin* Pin = FBlueprintNodeServiceHelpers::FindPin(Node, PinName, Error);
    if (!Pin)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::PIN_NOT_FOUND, Error);
    }

    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "SetPinDefaultValue", "Set Pin Default Value"));
    
    if (NodeGraph)
    {
        NodeGraph->Modify();
    }
    Node->Modify();

    Pin->DefaultValue = Value;

    if (NodeGraph)
    {
        NodeGraph->NotifyGraphChanged();
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    
    return TResult<void>::Success();
}

TResult<FString> FBlueprintNodeService::GetPinDefaultValue(UBlueprint* Blueprint, const FString& NodeId,
                                                           const FString& PinName)
{
    if (!Blueprint)
    {
        return TResult<FString>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null")
        );
    }

    // Gather all graphs
    TArray<UEdGraph*> CandidateGraphs;
    FBlueprintNodeServiceHelpers::GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);

    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!FBlueprintNodeServiceHelpers::ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph))
    {
        return TResult<FString>::Error(
            VibeUE::ErrorCodes::NODE_NOT_FOUND,
            FString::Printf(TEXT("Node '%s' not found"), *NodeId)
        );
    }

    FString Error;
    UEdGraphPin* Pin = FBlueprintNodeServiceHelpers::FindPin(Node, PinName, Error);
    if (!Pin)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::PIN_NOT_FOUND, Error);
    }

    return TResult<FString>::Success(Pin->DefaultValue);
}

TResult<void> FBlueprintNodeService::ConfigureNode(UBlueprint* Blueprint, const FString& NodeId,
                                                   const TMap<FString, FString>& Config)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null")
        );
    }

    // Gather all graphs
    TArray<UEdGraph*> CandidateGraphs;
    FBlueprintNodeServiceHelpers::GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);

    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!FBlueprintNodeServiceHelpers::ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph))
    {
        return TResult<void>::Error(
            VibeUE::ErrorCodes::NODE_NOT_FOUND,
            FString::Printf(TEXT("Node '%s' not found"), *NodeId)
        );
    }

    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "ConfigureNode", "Configure Node"));
    
    if (NodeGraph)
    {
        NodeGraph->Modify();
    }
    Node->Modify();

    // Apply configuration
    for (const auto& Pair : Config)
    {
        FString Error;
        UEdGraphPin* Pin = FBlueprintNodeServiceHelpers::FindPin(Node, Pair.Key, Error);
        if (Pin)
        {
            Pin->DefaultValue = Pair.Value;
        }
    }

    if (NodeGraph)
    {
        NodeGraph->NotifyGraphChanged();
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    
    return TResult<void>::Success();
}

// Node discovery
TResult<TArray<FNodeDescriptor>> FBlueprintNodeService::DiscoverAvailableNodes(UBlueprint* Blueprint, 
                                                                               const FString& SearchTerm)
{
    if (!Blueprint)
    {
        return TResult<TArray<FNodeDescriptor>>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null")
        );
    }

    TSharedPtr<FJsonObject> Result = FBlueprintReflection::GetAvailableBlueprintNodes(Blueprint, TEXT(""), SearchTerm, TEXT(""));
    
    TArray<FNodeDescriptor> Descriptors;
    
    if (Result.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>>* NodesArrayPtr = nullptr;
        if (Result->TryGetArrayField(TEXT("nodes"), NodesArrayPtr) && NodesArrayPtr)
        {
            for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArrayPtr)
            {
                const TSharedPtr<FJsonObject>* NodeObjPtr = nullptr;
                if (NodeValue.IsValid() && NodeValue->TryGetObject(NodeObjPtr) && NodeObjPtr)
                {
                    const TSharedPtr<FJsonObject>& NodeObj = *NodeObjPtr;
                    FNodeDescriptor Desc;
                    NodeObj->TryGetStringField(TEXT("node_type"), Desc.NodeType);
                    NodeObj->TryGetStringField(TEXT("display_name"), Desc.DisplayName);
                    NodeObj->TryGetStringField(TEXT("category"), Desc.Category);
                    NodeObj->TryGetStringField(TEXT("description"), Desc.Description);
                    NodeObj->TryGetStringField(TEXT("spawner_key"), Desc.SpawnerKey);
                    
                    const TArray<TSharedPtr<FJsonValue>>* KeywordsArrayPtr = nullptr;
                    if (NodeObj->TryGetArrayField(TEXT("keywords"), KeywordsArrayPtr) && KeywordsArrayPtr)
                    {
                        for (const TSharedPtr<FJsonValue>& KeywordValue : *KeywordsArrayPtr)
                        {
                            FString Keyword;
                            if (KeywordValue.IsValid() && KeywordValue->TryGetString(Keyword))
                            {
                                Desc.Keywords.Add(Keyword);
                            }
                        }
                    }
                    
                    Descriptors.Add(Desc);
                }
            }
        }
    }

    return TResult<TArray<FNodeDescriptor>>::Success(Descriptors);
}

TResult<FNodeInfo> FBlueprintNodeService::GetNodeDetails(UBlueprint* Blueprint, const FString& NodeId)
{
    if (!Blueprint)
    {
        return TResult<FNodeInfo>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null")
        );
    }

    // Gather all graphs
    TArray<UEdGraph*> CandidateGraphs;
    FBlueprintNodeServiceHelpers::GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);

    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!FBlueprintNodeServiceHelpers::ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph))
    {
        return TResult<FNodeInfo>::Error(
            VibeUE::ErrorCodes::NODE_NOT_FOUND,
            FString::Printf(TEXT("Node '%s' not found"), *NodeId)
        );
    }

    FNodeInfo Info;
    Info.NodeId = Node->NodeGuid.ToString();
    Info.NodeType = Node->GetClass()->GetName();
    Info.DisplayName = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
    Info.Position = FVector2D(Node->NodePosX, Node->NodePosY);
    Info.GraphName = NodeGraph ? NodeGraph->GetName() : TEXT("");

    // Gather connections
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->LinkedTo.Num() > 0)
        {
            for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (LinkedPin && LinkedPin->GetOwningNode())
                {
                    FPinConnectionInfo ConnInfo;
                    ConnInfo.SourceNodeId = Info.NodeId;
                    ConnInfo.SourcePinName = Pin->PinName.ToString();
                    ConnInfo.TargetNodeId = LinkedPin->GetOwningNode()->NodeGuid.ToString();
                    ConnInfo.TargetPinName = LinkedPin->PinName.ToString();
                    ConnInfo.PinType = Pin->PinType.PinCategory.ToString();
                    Info.Connections.Add(ConnInfo);
                }
            }
        }
    }

    return TResult<FNodeInfo>::Success(Info);
}

TResult<TArray<FString>> FBlueprintNodeService::ListNodes(UBlueprint* Blueprint, const FString& GraphName)
{
    if (!Blueprint)
    {
        return TResult<TArray<FString>>::Error(
            VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND,
            TEXT("Blueprint is null")
        );
    }

    FString Error;
    UEdGraph* TargetGraph = FBlueprintNodeServiceHelpers::ResolveTargetGraph(Blueprint, GraphName, Error);
    
    TArray<UEdGraph*> Graphs;
    if (TargetGraph)
    {
        Graphs.Add(TargetGraph);
    }
    else
    {
        FBlueprintNodeServiceHelpers::GatherCandidateGraphs(Blueprint, nullptr, Graphs);
    }

    TArray<FString> NodeIds;
    for (UEdGraph* Graph : Graphs)
    {
        if (Graph)
        {
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (Node)
                {
                    NodeIds.Add(Node->NodeGuid.ToString());
                }
            }
        }
    }

    return TResult<TArray<FString>>::Success(NodeIds);
}
