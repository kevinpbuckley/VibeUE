#include "Services/Blueprint/BlueprintNodeService.h"
#include "Services/Blueprint/BlueprintGraphService.h"
#include "Core/ErrorCodes.h"
#include "Commands/CommonUtils.h"
#include "Commands/BlueprintReflection.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node.h"
#include "K2Node_InputAction.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "Json.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlueprintNodeService, Log, All);

FBlueprintNodeService::FBlueprintNodeService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

UEdGraph* FBlueprintNodeService::ResolveTargetGraph(UBlueprint* Blueprint, const FString& GraphName, FString& OutError)
{
    OutError.Reset();
    if (!Blueprint)
    {
        OutError = TEXT("Invalid blueprint");
        return nullptr;
    }

    if (GraphName.IsEmpty())
    {
        return FCommonUtils::FindOrCreateEventGraph(Blueprint);
    }

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
        {
            return Graph;
        }
    }

    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
        {
            return Graph;
        }
    }

    for (UEdGraph* Graph : Blueprint->MacroGraphs)
    {
        if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
        {
            return Graph;
        }
    }

    OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
    return nullptr;
}

void FBlueprintNodeService::GatherCandidateGraphs(UBlueprint* Blueprint, UEdGraph* PreferredGraph, TArray<UEdGraph*>& OutGraphs)
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
    for (UEdGraph* Graph : Blueprint->UbergraphPages) { AddGraph(Graph); }
    for (UEdGraph* Graph : Blueprint->FunctionGraphs) { AddGraph(Graph); }
    for (UEdGraph* Graph : Blueprint->MacroGraphs) { AddGraph(Graph); }
    for (UEdGraph* Graph : Blueprint->IntermediateGeneratedGraphs) { AddGraph(Graph); }
}

bool FBlueprintNodeService::ResolveNodeIdentifier(const FString& Identifier, const TArray<UEdGraph*>& Graphs, UEdGraphNode*& OutNode, UEdGraph*& OutGraph)
{
    OutNode = nullptr;
    OutGraph = nullptr;
    if (Identifier.IsEmpty()) { return false; }

    FString NormalizedIdentifier = Identifier;
    NormalizedIdentifier.ReplaceInline(TEXT("{"), TEXT(""));
    NormalizedIdentifier.ReplaceInline(TEXT("}"), TEXT(""));
    FString HyphenlessIdentifier = NormalizedIdentifier;
    HyphenlessIdentifier.ReplaceInline(TEXT("-"), TEXT(""));

    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph) { continue; }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node) { continue; }
            FString GuidString = Node->NodeGuid.ToString();
            GuidString.ReplaceInline(TEXT("{"), TEXT(""));
            GuidString.ReplaceInline(TEXT("}"), TEXT(""));
            FString HyphenlessGuid = GuidString;
            HyphenlessGuid.ReplaceInline(TEXT("-"), TEXT(""));

            if (GuidString.Equals(NormalizedIdentifier, ESearchCase::IgnoreCase) || HyphenlessGuid.Equals(HyphenlessIdentifier, ESearchCase::IgnoreCase))
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

UEdGraphPin* FBlueprintNodeService::FindPin(UEdGraphNode* Node, const FString& PinName)
{
    if (!Node || PinName.IsEmpty()) { return nullptr; }
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase)) { return Pin; }
    }
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->GetDisplayName().ToString().Equals(PinName, ESearchCase::IgnoreCase)) { return Pin; }
    }
    return nullptr;
}

TResult<UK2Node*> FBlueprintNodeService::CreateNode(UBlueprint* Blueprint, const FString& GraphName, const FString& NodeType, const FVector2D& Position)
{
    if (!Blueprint) { return TResult<UK2Node*>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null")); }
    auto ValidationResult = ValidateNotEmpty(NodeType, TEXT("NodeType"));
    if (ValidationResult.IsError()) { return TResult<UK2Node*>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage()); }

    FString Error;
    UEdGraph* TargetGraph = ResolveTargetGraph(Blueprint, GraphName, Error);
    if (!TargetGraph) { return TResult<UK2Node*>::Error(VibeUE::ErrorCodes::GRAPH_NOT_FOUND, Error.IsEmpty() ? TEXT("Failed to resolve target graph") : Error); }

    TSharedPtr<FJsonObject> NodeParams = MakeShared<FJsonObject>();
    NodeParams->SetNumberField(TEXT("x"), Position.X);
    NodeParams->SetNumberField(TEXT("y"), Position.Y);
    
    UK2Node* NewNode = FBlueprintReflection::CreateNodeFromIdentifier(Blueprint, NodeType, NodeParams);
    if (!NewNode) { return TResult<UK2Node*>::Error(VibeUE::ErrorCodes::NODE_CREATE_FAILED, FString::Printf(TEXT("Failed to create node of type '%s'"), *NodeType)); }

    NewNode->NodePosX = FMath::RoundToInt(Position.X);
    NewNode->NodePosY = FMath::RoundToInt(Position.Y);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    return TResult<UK2Node*>::Success(NewNode);
}

TResult<FNodeDeletionInfo> FBlueprintNodeService::DeleteNode(UBlueprint* Blueprint, const FString& NodeId, bool bDisconnectPins)
{
    if (!Blueprint)
    {
        return TResult<FNodeDeletionInfo>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }
    
    auto ValidationResult = ValidateNotEmpty(NodeId, TEXT("NodeId"));
    if (ValidationResult.IsError())
    {
        return TResult<FNodeDeletionInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
    
    UEdGraphNode* NodeToDelete = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!ResolveNodeIdentifier(NodeId, CandidateGraphs, NodeToDelete, NodeGraph))
    {
        return TResult<FNodeDeletionInfo>::Error(VibeUE::ErrorCodes::NODE_NOT_FOUND, 
            FString::Printf(TEXT("Node with ID '%s' not found"), *NodeId));
    }
    
    if (!NodeToDelete->CanUserDeleteNode())
    {
        return TResult<FNodeDeletionInfo>::Error(VibeUE::ErrorCodes::NODE_DELETE_FAILED, 
            FString::Printf(TEXT("Node '%s' cannot be deleted (protected engine node)"), *NodeId));
    }

    // Gather deletion info before deleting
    FNodeDeletionInfo DeletionInfo;
    DeletionInfo.NodeId = NodeToDelete->NodeGuid.ToString();
    DeletionInfo.NodeType = NodeToDelete->GetClass()->GetName();
    DeletionInfo.GraphName = NodeGraph ? NodeGraph->GetName() : TEXT("");
    DeletionInfo.bWasProtected = false;

    // Disconnect pins if requested and collect info
    if (bDisconnectPins)
    {
        for (UEdGraphPin* Pin : NodeToDelete->Pins)
        {
            if (Pin && Pin->LinkedTo.Num() > 0)
            {
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (LinkedPin && LinkedPin->GetOwningNode())
                    {
                        FPinConnectionInfo ConnInfo;
                        ConnInfo.SourceNodeId = DeletionInfo.NodeId;
                        ConnInfo.SourcePinName = Pin->PinName.ToString();
                        ConnInfo.TargetNodeId = LinkedPin->GetOwningNode()->NodeGuid.ToString();
                        ConnInfo.TargetPinName = LinkedPin->PinName.ToString();
                        ConnInfo.PinType = Pin->PinType.PinCategory.ToString();
                        DeletionInfo.DisconnectedPins.Add(ConnInfo);
                    }
                }
                Pin->BreakAllPinLinks();
            }
        }
    }

    // Delete the node
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
    
    return TResult<FNodeDeletionInfo>::Success(DeletionInfo);
}

TResult<void> FBlueprintNodeService::MoveNode(UBlueprint* Blueprint, const FString& NodeId, const FVector2D& Position)
{
    if (!Blueprint) { return TResult<void>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null")); }
    auto ValidationResult = ValidateNotEmpty(NodeId, TEXT("NodeId"));
    if (ValidationResult.IsError()) { return TResult<void>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage()); }

    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
    UEdGraphNode* TargetNode = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!ResolveNodeIdentifier(NodeId, CandidateGraphs, TargetNode, NodeGraph)) { return TResult<void>::Error(VibeUE::ErrorCodes::NODE_NOT_FOUND, FString::Printf(TEXT("Node with ID '%s' not found"), *NodeId)); }

    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "MoveBlueprintNode", "Move Blueprint Node"));
    if (NodeGraph) { NodeGraph->Modify(); }
    TargetNode->Modify();
    TargetNode->NodePosX = FMath::RoundToInt(Position.X);
    TargetNode->NodePosY = FMath::RoundToInt(Position.Y);
    if (NodeGraph) { NodeGraph->NotifyGraphChanged(); }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    return TResult<void>::Success();
}


TResult<TArray<FPinConnectionInfo>> FBlueprintNodeService::GetPinConnections(UBlueprint* Blueprint, const FString& NodeId)
{
    if (!Blueprint) { return TResult<TArray<FPinConnectionInfo>>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null")); }
    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph)) { return TResult<TArray<FPinConnectionInfo>>::Error(VibeUE::ErrorCodes::NODE_NOT_FOUND, FString::Printf(TEXT("Node '%s' not found"), *NodeId)); }
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

TResult<void> FBlueprintNodeService::SetPinDefaultValue(UBlueprint* Blueprint, const FString& NodeId, const FString& PinName, const FString& Value)
{
    if (!Blueprint) { return TResult<void>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null")); }
    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph)) { return TResult<void>::Error(VibeUE::ErrorCodes::NODE_NOT_FOUND, FString::Printf(TEXT("Node '%s' not found"), *NodeId)); }
    UEdGraphPin* Pin = FindPin(Node, PinName);
    if (!Pin) { return TResult<void>::Error(VibeUE::ErrorCodes::PIN_NOT_FOUND, FString::Printf(TEXT("Pin '%s' not found on node '%s'"), *PinName, *NodeId)); }
    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "SetPinDefaultValue", "Set Pin Default Value"));
    if (NodeGraph) { NodeGraph->Modify(); }
    Node->Modify();
    Pin->DefaultValue = Value;
    if (NodeGraph) { NodeGraph->NotifyGraphChanged(); }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    return TResult<void>::Success();
}

TResult<FString> FBlueprintNodeService::GetPinDefaultValue(UBlueprint* Blueprint, const FString& NodeId, const FString& PinName)
{
    if (!Blueprint) { return TResult<FString>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null")); }
    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph)) { return TResult<FString>::Error(VibeUE::ErrorCodes::NODE_NOT_FOUND, FString::Printf(TEXT("Node '%s' not found"), *NodeId)); }
    UEdGraphPin* Pin = FindPin(Node, PinName);
    if (!Pin) { return TResult<FString>::Error(VibeUE::ErrorCodes::PIN_NOT_FOUND, FString::Printf(TEXT("Pin '%s' not found on node '%s'"), *PinName, *NodeId)); }
    return TResult<FString>::Success(Pin->DefaultValue);
}

TResult<void> FBlueprintNodeService::ConfigureNode(UBlueprint* Blueprint, const FString& NodeId, const TMap<FString, FString>& Config)
{
    if (!Blueprint) { return TResult<void>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null")); }
    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph)) { return TResult<void>::Error(VibeUE::ErrorCodes::NODE_NOT_FOUND, FString::Printf(TEXT("Node '%s' not found"), *NodeId)); }
    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "ConfigureNode", "Configure Node"));
    if (NodeGraph) { NodeGraph->Modify(); }
    Node->Modify();
    for (const auto& Pair : Config)
    {
        UEdGraphPin* Pin = FindPin(Node, Pair.Key);
        if (Pin) { Pin->DefaultValue = Pair.Value; }
    }
    if (NodeGraph) { NodeGraph->NotifyGraphChanged(); }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    return TResult<void>::Success();
}

TResult<TArray<FNodeDescriptor>> FBlueprintNodeService::DiscoverAvailableNodes(UBlueprint* Blueprint, const FString& SearchTerm)
{
    if (!Blueprint) { return TResult<TArray<FNodeDescriptor>>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null")); }
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
                            if (KeywordValue.IsValid() && KeywordValue->TryGetString(Keyword)) { Desc.Keywords.Add(Keyword); }
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
    if (!Blueprint) { return TResult<FNodeInfo>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null")); }
    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph)) { return TResult<FNodeInfo>::Error(VibeUE::ErrorCodes::NODE_NOT_FOUND, FString::Printf(TEXT("Node '%s' not found"), *NodeId)); }
    FNodeInfo Info;
    Info.NodeId = Node->NodeGuid.ToString();
    Info.NodeType = Node->GetClass()->GetName();
    Info.DisplayName = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
    Info.Position = FVector2D(Node->NodePosX, Node->NodePosY);
    Info.GraphName = NodeGraph ? NodeGraph->GetName() : TEXT("");
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
    if (!Blueprint) { return TResult<TArray<FString>>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null")); }
    FString Error;
    UEdGraph* TargetGraph = ResolveTargetGraph(Blueprint, GraphName, Error);
    TArray<UEdGraph*> Graphs;
    if (TargetGraph) { Graphs.Add(TargetGraph); }
    else { GatherCandidateGraphs(Blueprint, nullptr, Graphs); }
    TArray<FString> NodeIds;
    for (UEdGraph* Graph : Graphs)
    {
        if (Graph)
        {
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (Node) { NodeIds.Add(Node->NodeGuid.ToString()); }
            }
        }
    }
    return TResult<TArray<FString>>::Success(NodeIds);
}

TResult<TArray<FNodeInfo>> FBlueprintNodeService::FindNodes(UBlueprint* Blueprint, const FString& GraphName, const FString& SearchTerm)
{
    if (!Blueprint)
    {
        return TResult<TArray<FNodeInfo>>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }
    
    FString Error;
    UEdGraph* TargetGraph = ResolveTargetGraph(Blueprint, GraphName, Error);
    TArray<UEdGraph*> Graphs;
    if (TargetGraph)
    {
        Graphs.Add(TargetGraph);
    }
    else
    {
        GatherCandidateGraphs(Blueprint, nullptr, Graphs);
    }
    
    TArray<FNodeInfo> FoundNodes;
    FString LowerSearchTerm = SearchTerm.ToLower();
    
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph) continue;
        
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node) continue;
            
            // Search in node title, type, or GUID
            FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().ToLower();
            FString NodeType = Node->GetClass()->GetName().ToLower();
            FString NodeGuid = Node->NodeGuid.ToString().ToLower();
            
            if (SearchTerm.IsEmpty() || 
                NodeTitle.Contains(LowerSearchTerm) ||
                NodeType.Contains(LowerSearchTerm) ||
                NodeGuid.Contains(LowerSearchTerm))
            {
                FNodeInfo Info;
                Info.NodeId = Node->NodeGuid.ToString();
                Info.NodeType = Node->GetClass()->GetName();
                Info.DisplayName = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
                Info.Position = FVector2D(Node->NodePosX, Node->NodePosY);
                Info.GraphName = Graph->GetName();
                FoundNodes.Add(Info);
            }
        }
    }
    
    return TResult<TArray<FNodeInfo>>::Success(FoundNodes);
}

TResult<void> FBlueprintNodeService::RefreshNode(UBlueprint* Blueprint, const FString& NodeId, bool bCompile)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }
    
    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
    
    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph))
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::NODE_NOT_FOUND, 
            FString::Printf(TEXT("Node '%s' not found"), *NodeId));
    }
    
    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "RefreshNode", "Refresh Blueprint Node"));
    
    if (Blueprint) Blueprint->Modify();
    if (NodeGraph) NodeGraph->Modify();
    if (Node)
    {
        Node->Modify();
        Node->ReconstructNode();
    }
    
    if (NodeGraph) NodeGraph->NotifyGraphChanged();
    
    if (Blueprint)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        if (bCompile)
        {
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
        }
    }
    
    return TResult<void>::Success();
}

TResult<TArray<FGraphInfo>> FBlueprintNodeService::RefreshAllNodes(UBlueprint* Blueprint, bool bCompile)
{
    if (!Blueprint)
    {
        return TResult<TArray<FGraphInfo>>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }
    
    TArray<UEdGraph*> Graphs;
    GatherCandidateGraphs(Blueprint, nullptr, Graphs);
    
    const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "RefreshAllNodes", "Refresh All Blueprint Nodes"));
    
    Blueprint->Modify();
    for (UEdGraph* Graph : Graphs)
    {
        if (Graph) Graph->Modify();
    }
    
    FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
    
    TArray<FGraphInfo> GraphInfos;
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph) continue;
        
        Graph->NotifyGraphChanged();
        
        FGraphInfo Info;
        Info.Name = Graph->GetName();
        Info.Guid = Graph->GraphGuid.ToString();
        Info.NodeCount = Graph->Nodes.Num();
        
        // Determine graph type
        if (Blueprint->UbergraphPages.Contains(Graph))
        {
            Info.GraphType = TEXT("event");
        }
        else if (Blueprint->FunctionGraphs.Contains(Graph))
        {
            Info.GraphType = TEXT("function");
        }
        else if (Blueprint->MacroGraphs.Contains(Graph))
        {
            Info.GraphType = TEXT("macro");
        }
        else
        {
            Info.GraphType = TEXT("other");
        }
        
        GraphInfos.Add(Info);
    }
    
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if (bCompile)
    {
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
    }
    
    return TResult<TArray<FGraphInfo>>::Success(GraphInfos);
}

TResult<FString> FBlueprintNodeService::CreateInputActionNode(UBlueprint* Blueprint, const FInputActionNodeParams& Params)
{
    if (!Blueprint)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }
    
    auto ValidationResult = ValidateNotEmpty(Params.ActionName, TEXT("ActionName"));
    if (ValidationResult.IsError())
    {
        return TResult<FString>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }
    
    // Resolve target graph (defaults to EventGraph)
    FString Error;
    UEdGraph* TargetGraph = ResolveTargetGraph(Blueprint, TEXT(""), Error);
    if (!TargetGraph)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::GRAPH_NOT_FOUND, 
            Error.IsEmpty() ? TEXT("Failed to resolve target graph") : Error);
    }
    
    // Create input action node
    // Uses CommonUtils which handles graph modification, GUID generation, and pin allocation
    UK2Node_InputAction* InputActionNode = FCommonUtils::CreateInputActionNode(TargetGraph, Params.ActionName, Params.Position);
    if (!InputActionNode)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::NODE_CREATE_FAILED, 
            FString::Printf(TEXT("Failed to create input action node for action '%s'"), *Params.ActionName));
    }
    
    // Mark blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    
    // Return node ID
    return TResult<FString>::Success(InputActionNode->NodeGuid.ToString());
}

TResult<FString> FBlueprintNodeService::AddEvent(UBlueprint* Blueprint, const FEventConfiguration& Config)
{
    if (!Blueprint)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }
    
    auto ValidationResult = ValidateNotEmpty(Config.EventName, TEXT("EventName"));
    if (ValidationResult.IsError())
    {
        return TResult<FString>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }
    
    // Resolve target graph (use specified graph or default to EventGraph)
    FString Error;
    UEdGraph* TargetGraph = ResolveTargetGraph(Blueprint, Config.GraphName, Error);
    if (!TargetGraph)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::GRAPH_NOT_FOUND, 
            Error.IsEmpty() ? TEXT("Failed to resolve target graph") : Error);
    }
    
    // Create event node
    // Uses CommonUtils which handles graph modification, GUID generation, and pin allocation
    UK2Node_Event* EventNode = FCommonUtils::CreateEventNode(TargetGraph, Config.EventName, Config.Position);
    if (!EventNode)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::NODE_CREATE_FAILED, 
            FString::Printf(TEXT("Failed to create event node for event '%s'"), *Config.EventName));
    }
    
    // Mark blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    
    // Return node ID
    return TResult<FString>::Success(EventNode->NodeGuid.ToString());
}
