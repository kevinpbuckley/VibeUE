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

TResult<TArray<FNodeInfo>> FBlueprintNodeService::FindNodes(UBlueprint* Blueprint, const FNodeSearchCriteria& Criteria)
{
    if (!Blueprint)
    {
        return TResult<TArray<FNodeInfo>>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }
    
    // Validate that we have a node type to search for
    auto ValidationResult = ValidateNotEmpty(Criteria.NodeType, TEXT("NodeType"));
    if (ValidationResult.IsError())
    {
        return TResult<TArray<FNodeInfo>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }
    
    // Resolve target graph(s)
    FString Error;
    UEdGraph* TargetGraph = ResolveTargetGraph(Blueprint, Criteria.GraphScope, Error);
    TArray<UEdGraph*> Graphs;
    if (TargetGraph)
    {
        Graphs.Add(TargetGraph);
    }
    else
    {
        GatherCandidateGraphs(Blueprint, nullptr, Graphs);
    }
    
    // Use reflection-based node type resolution
    UClass* TargetNodeClass = FBlueprintReflection::ResolveNodeClass(Criteria.NodeType);
    if (!TargetNodeClass)
    {
        return TResult<TArray<FNodeInfo>>::Error(
            VibeUE::ErrorCodes::PARAM_INVALID,
            FString::Printf(TEXT("Unknown node type '%s' - reflection system could not resolve this node type"), *Criteria.NodeType)
        );
    }
    
    UE_LOG(LogBlueprintNodeService, Log, TEXT("FindNodes - Resolved node class via reflection: %s"), *TargetNodeClass->GetName());
    
    // Search through nodes using reflection-based type matching
    TArray<FNodeInfo> FoundNodes;
    FString LowerNamePattern = Criteria.NamePattern.ToLower();
    
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph) continue;
        
        UE_LOG(LogBlueprintNodeService, Verbose, TEXT("FindNodes - Searching %d nodes in graph '%s'"), Graph->Nodes.Num(), *Graph->GetName());
        
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node || !Node->IsA(TargetNodeClass))
            {
                continue;
            }
            
            // Apply name pattern filter if specified
            if (!Criteria.NamePattern.IsEmpty())
            {
                FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().ToLower();
                FString NodeName = Node->GetName().ToLower();
                
                if (!NodeTitle.Contains(LowerNamePattern) && !NodeName.Contains(LowerNamePattern))
                {
                    continue;
                }
            }
            
            // Build node info
            FNodeInfo Info;
            Info.NodeId = Node->NodeGuid.ToString();
            Info.NodeType = Node->GetClass()->GetName();
            Info.DisplayName = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
            Info.Position = FVector2D(Node->NodePosX, Node->NodePosY);
            Info.GraphName = Graph->GetName();
            
            FoundNodes.Add(Info);
            
            UE_LOG(LogBlueprintNodeService, Verbose, TEXT("FindNodes - Found matching node: %s"), *Node->NodeGuid.ToString());
        }
    }
    
    UE_LOG(LogBlueprintNodeService, Log, TEXT("FindNodes - Found %d matching nodes for type: %s"), FoundNodes.Num(), *Criteria.NodeType);
    
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

TResult<FNodeDetails> FBlueprintNodeService::GetNodeDetailsExtended(UBlueprint* Blueprint, const FString& NodeId, 
                                                                     bool bIncludePins, bool bIncludeConnections)
{
    if (!Blueprint)
    {
        return TResult<FNodeDetails>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }
    
    auto ValidationResult = ValidateNotEmpty(NodeId, TEXT("NodeId"));
    if (ValidationResult.IsError())
    {
        return TResult<FNodeDetails>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }
    
    // Find the node
    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
    
    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph))
    {
        return TResult<FNodeDetails>::Error(VibeUE::ErrorCodes::NODE_NOT_FOUND, 
            FString::Printf(TEXT("Node '%s' not found"), *NodeId));
    }
    
    // Build detailed node information
    FNodeDetails Details;
    Details.NodeId = Node->NodeGuid.ToString();
    Details.NodeType = Node->GetClass()->GetName();
    Details.DisplayName = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
    Details.Position = FVector2D(Node->NodePosX, Node->NodePosY);
    Details.GraphName = NodeGraph ? NodeGraph->GetName() : TEXT("");
    Details.bCanUserDeleteNode = Node->CanUserDeleteNode();
    
    // Get K2Node-specific metadata
    if (UK2Node* K2Node = Cast<UK2Node>(Node))
    {
        Details.Category = K2Node->GetMenuCategory().ToString();
        Details.Tooltip = K2Node->GetTooltipText().ToString();
        Details.Keywords = K2Node->GetKeywords().ToString();
    }
    
    // Gather pin details if requested
    if (bIncludePins)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin)
            {
                continue;
            }
            
            FPinDetail PinDetail;
            PinDetail.PinName = Pin->PinName.ToString();
            PinDetail.PinType = Pin->PinType.PinCategory.ToString();
            PinDetail.Direction = (Pin->Direction == EGPD_Input) ? TEXT("Input") : TEXT("Output");
            PinDetail.bIsHidden = Pin->bHidden;
            PinDetail.bIsConnected = Pin->LinkedTo.Num() > 0;
            PinDetail.DefaultValue = Pin->DefaultValue;
            PinDetail.DefaultObjectName = Pin->DefaultObject ? Pin->DefaultObject->GetName() : TEXT("");
            PinDetail.DefaultTextValue = Pin->DefaultTextValue.ToString();
            PinDetail.bIsArray = Pin->PinType.IsArray();
            PinDetail.bIsReference = Pin->PinType.bIsReference;
            
            // Gather connection details if requested
            if (bIncludeConnections && Pin->LinkedTo.Num() > 0)
            {
                for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                {
                    if (LinkedPin && LinkedPin->GetOwningNode())
                    {
                        FPinConnectionInfo ConnInfo;
                        ConnInfo.SourceNodeId = Details.NodeId;
                        ConnInfo.SourcePinName = Pin->PinName.ToString();
                        ConnInfo.TargetNodeId = LinkedPin->GetOwningNode()->NodeGuid.ToString();
                        ConnInfo.TargetPinName = LinkedPin->PinName.ToString();
                        ConnInfo.PinType = Pin->PinType.PinCategory.ToString();
                        PinDetail.Connections.Add(ConnInfo);
                    }
                }
            }
            
            // Add to appropriate collection
            if (Pin->Direction == EGPD_Input)
            {
                Details.InputPins.Add(PinDetail);
            }
            else
            {
                Details.OutputPins.Add(PinDetail);
            }
        }
    }
    
    return TResult<FNodeDetails>::Success(Details);
}
TResult<FDetailedNodeInfo> FBlueprintNodeService::DescribeNode(UBlueprint* Blueprint, const FString& NodeId, bool bIncludePins, bool bIncludeInternalPins)
{
    if (!Blueprint)
    {
        return TResult<FDetailedNodeInfo>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }
    
    auto ValidationResult = ValidateNotEmpty(NodeId, TEXT("NodeId"));
    if (ValidationResult.IsError())
    {
        return TResult<FDetailedNodeInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }

    TArray<UEdGraph*> CandidateGraphs;
    GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
    
    UEdGraphNode* Node = nullptr;
    UEdGraph* NodeGraph = nullptr;
    if (!ResolveNodeIdentifier(NodeId, CandidateGraphs, Node, NodeGraph))
    {
        return TResult<FDetailedNodeInfo>::Error(VibeUE::ErrorCodes::NODE_NOT_FOUND, 
            FString::Printf(TEXT("Node with ID '%s' not found"), *NodeId));
    }

    FDetailedNodeInfo Info;
    Info.NodeId = NormalizeGuid(Node->NodeGuid);
    Info.DisplayName = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
    Info.ClassPath = Node->GetClass()->GetPathName();
    Info.GraphScope = DescribeGraphScope(Blueprint, NodeGraph);
    Info.GraphName = NodeGraph ? NodeGraph->GetName() : TEXT("");
    Info.GraphGuid = NodeGraph ? NormalizeGuid(NodeGraph->GraphGuid) : TEXT("");
    Info.Position = FVector2D(Node->NodePosX, Node->NodePosY);
    Info.Comment = Node->NodeComment;
    Info.bIsPure = IsPureK2Node(Node);
    Info.ExecState = DescribeExecState(Node);

    // Extract node descriptor if it's a K2Node
    if (UK2Node* AsK2Node = Cast<UK2Node>(Node))
    {
        TSharedPtr<FJsonObject> NodeParams;
        FString DerivedSpawnerKey;
        
        // Extract descriptor information using BlueprintReflection system
        if (UK2Node_CallFunction* FuncNode = Cast<UK2Node_CallFunction>(AsK2Node))
        {
            if (UFunction* TargetFunction = FuncNode->GetTargetFunction())
            {
                TStrongObjectPtr<UBlueprintFunctionNodeSpawner> TempSpawner(UBlueprintFunctionNodeSpawner::Create(TargetFunction));
                if (TempSpawner.IsValid())
                {
                    auto Descriptor = FBlueprintReflection::ExtractDescriptorFromSpawner(TempSpawner.Get(), Blueprint);
                    if (!Descriptor.SpawnerKey.IsEmpty())
                    {
                        Info.SpawnerKey = Descriptor.SpawnerKey;
                    }
                    Info.NodeDescriptor = Descriptor.ToJson();
                    
                    NodeParams = MakeShared<FJsonObject>();
                    NodeParams->SetStringField(TEXT("spawner_key"), Descriptor.SpawnerKey);
                    NodeParams->SetStringField(TEXT("function_name"), Descriptor.FunctionName);
                    if (!Descriptor.FunctionClassPath.IsEmpty())
                    {
                        NodeParams->SetStringField(TEXT("function_class"), Descriptor.FunctionClassPath);
                    }
                    NodeParams->SetBoolField(TEXT("is_static"), Descriptor.bIsStatic);
                    Info.NodeParams = NodeParams;
                    
                    if (Info.NodeDescriptor.IsValid() && Info.NodeDescriptor->HasField(TEXT("function_metadata")))
                    {
                        Info.FunctionMetadata = Info.NodeDescriptor->GetObjectField(TEXT("function_metadata"));
                    }
                }
            }
        }
        else if (UK2Node_VariableGet* VarGetNode = Cast<UK2Node_VariableGet>(AsK2Node))
        {
            const FName VariableName = VarGetNode->GetVarName();
            if (!VariableName.IsNone())
            {
                using FDescriptor = FBlueprintReflection::FNodeSpawnerDescriptor;
                FDescriptor Descriptor;
                Descriptor.NodeType = TEXT("variable_get");
                Descriptor.DisplayName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
                Descriptor.NodeClassName = Node->GetClass()->GetName();
                Descriptor.NodeClassPath = Node->GetClass()->GetPathName();
                Descriptor.VariableName = VariableName.ToString();
                Descriptor.SpawnerKey = FString::Printf(TEXT("GET %s"), *Descriptor.VariableName);
                
                if (UEdGraphPin* ValuePin = VarGetNode->GetValuePin())
                {
                    Descriptor.VariableType = ValuePin->PinType.PinCategory.ToString();
                    if (ValuePin->PinType.PinSubCategoryObject.IsValid())
                    {
                        Descriptor.VariableTypePath = ValuePin->PinType.PinSubCategoryObject->GetPathName();
                    }
                }
                
                FBlueprintReflection::ExtractPinDescriptorsFromNode(VarGetNode, Descriptor.Pins);
                Descriptor.ExpectedPinCount = Descriptor.Pins.Num();
                
                Info.SpawnerKey = Descriptor.SpawnerKey;
                Info.NodeDescriptor = Descriptor.ToJson();
                
                NodeParams = MakeShared<FJsonObject>();
                NodeParams->SetStringField(TEXT("variable_name"), Descriptor.VariableName);
                NodeParams->SetStringField(TEXT("operation"), TEXT("get"));
                Info.NodeParams = NodeParams;
                
                if (Info.NodeDescriptor.IsValid() && Info.NodeDescriptor->HasField(TEXT("variable_metadata")))
                {
                    Info.VariableMetadata = Info.NodeDescriptor->GetObjectField(TEXT("variable_metadata"));
                }
            }
        }
        else if (UK2Node_VariableSet* VarSetNode = Cast<UK2Node_VariableSet>(AsK2Node))
        {
            const FName VariableName = VarSetNode->GetVarName();
            if (!VariableName.IsNone())
            {
                using FDescriptor = FBlueprintReflection::FNodeSpawnerDescriptor;
                FDescriptor Descriptor;
                Descriptor.NodeType = TEXT("variable_set");
                Descriptor.DisplayName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
                Descriptor.NodeClassName = Node->GetClass()->GetName();
                Descriptor.NodeClassPath = Node->GetClass()->GetPathName();
                Descriptor.VariableName = VariableName.ToString();
                Descriptor.SpawnerKey = FString::Printf(TEXT("SET %s"), *Descriptor.VariableName);
                
                UEdGraphPin* ValuePin = VarSetNode->FindPin(VariableName, EGPD_Input);
                if (!ValuePin)
                {
                    for (UEdGraphPin* Pin : VarSetNode->Pins)
                    {
                        if (Pin && Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                        {
                            ValuePin = Pin;
                            break;
                        }
                    }
                }
                
                if (ValuePin)
                {
                    Descriptor.VariableType = ValuePin->PinType.PinCategory.ToString();
                    if (ValuePin->PinType.PinSubCategoryObject.IsValid())
                    {
                        Descriptor.VariableTypePath = ValuePin->PinType.PinSubCategoryObject->GetPathName();
                    }
                }
                
                FBlueprintReflection::ExtractPinDescriptorsFromNode(VarSetNode, Descriptor.Pins);
                Descriptor.ExpectedPinCount = Descriptor.Pins.Num();
                
                Info.SpawnerKey = Descriptor.SpawnerKey;
                Info.NodeDescriptor = Descriptor.ToJson();
                
                NodeParams = MakeShared<FJsonObject>();
                NodeParams->SetStringField(TEXT("variable_name"), Descriptor.VariableName);
                NodeParams->SetStringField(TEXT("operation"), TEXT("set"));
                Info.NodeParams = NodeParams;
                
                if (Info.NodeDescriptor.IsValid() && Info.NodeDescriptor->HasField(TEXT("variable_metadata")))
                {
                    Info.VariableMetadata = Info.NodeDescriptor->GetObjectField(TEXT("variable_metadata"));
                }
            }
        }
        else if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(AsK2Node))
        {
            if (UClass* TargetClass = CastNode->TargetType)
            {
                using FDescriptor = FBlueprintReflection::FNodeSpawnerDescriptor;
                FDescriptor Descriptor;
                Descriptor.NodeType = TEXT("dynamic_cast");
                Descriptor.DisplayName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
                Descriptor.NodeClassName = Node->GetClass()->GetName();
                Descriptor.NodeClassPath = Node->GetClass()->GetPathName();
                Descriptor.TargetClassName = TargetClass->GetName();
                Descriptor.TargetClassPath = TargetClass->GetPathName();
                Descriptor.SpawnerKey = FString::Printf(TEXT("Cast To %s"), *Descriptor.TargetClassName);
                
                FBlueprintReflection::ExtractPinDescriptorsFromNode(CastNode, Descriptor.Pins);
                Descriptor.ExpectedPinCount = Descriptor.Pins.Num();
                
                Info.SpawnerKey = Descriptor.SpawnerKey;
                Info.NodeDescriptor = Descriptor.ToJson();
                
                NodeParams = MakeShared<FJsonObject>();
                NodeParams->SetStringField(TEXT("cast_target"), Descriptor.TargetClassPath);
                Info.NodeParams = NodeParams;
                
                if (Info.NodeDescriptor.IsValid() && Info.NodeDescriptor->HasField(TEXT("cast_metadata")))
                {
                    Info.CastMetadata = Info.NodeDescriptor->GetObjectField(TEXT("cast_metadata"));
                }
            }
        }
    }

    // Build pin information
    if (bIncludePins)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin)
            {
                continue;
            }

            if (!bIncludeInternalPins && (Pin->bHidden || Pin->bAdvancedView))
            {
                continue;
            }

            FPinInfo PinInfo;
            PinInfo.PinId = BuildPinIdentifier(Node, Pin);
            PinInfo.Name = Pin->PinName.ToString();
            PinInfo.Direction = DescribePinDirection(Pin->Direction);
            PinInfo.Category = Pin->PinType.PinCategory.ToString();
            PinInfo.SubCategory = Pin->PinType.PinSubCategory.ToString();
            if (Pin->PinType.PinSubCategoryObject.IsValid())
            {
                PinInfo.PinTypePath = Pin->PinType.PinSubCategoryObject->GetPathName();
            }
            PinInfo.Container = DescribeContainerType(Pin->PinType.ContainerType);
            PinInfo.bIsConst = Pin->PinType.bIsConst;
            PinInfo.bIsReference = Pin->PinType.bIsReference;
            PinInfo.bIsArray = Pin->PinType.ContainerType == EPinContainerType::Array;
            PinInfo.bIsSet = Pin->PinType.ContainerType == EPinContainerType::Set;
            PinInfo.bIsMap = Pin->PinType.ContainerType == EPinContainerType::Map;
            PinInfo.bIsHidden = Pin->bHidden;
            PinInfo.bIsAdvanced = Pin->bAdvancedView;
            PinInfo.bIsConnected = Pin->LinkedTo.Num() > 0;
            PinInfo.Tooltip = Pin->PinToolTip;
            PinInfo.DefaultValue = Pin->DefaultValue;
            PinInfo.DefaultText = Pin->DefaultTextValue.ToString();
            if (Pin->DefaultObject)
            {
                PinInfo.DefaultObjectPath = Pin->DefaultObject->GetPathName();
            }
            PinInfo.DefaultValueJson = BuildDefaultValueJson(Pin);

            // Build links
            for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
            {
                if (!LinkedPin)
                {
                    continue;
                }

                const UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode();
                TSharedPtr<FJsonObject> Link = MakeShared<FJsonObject>();
                if (LinkedNode)
                {
                    Link->SetStringField(TEXT("node_id"), NormalizeGuid(LinkedNode->NodeGuid));
                }
                Link->SetStringField(TEXT("pin_id"), BuildPinIdentifier(LinkedNode, LinkedPin));
                Link->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
                PinInfo.Links.Add(Link);
            }

            Info.Pins.Add(PinInfo);
        }
    }

    // Build metadata
    TSharedPtr<FJsonObject> Metadata = MakeShared<FJsonObject>();
    Metadata->SetStringField(TEXT("guid"), NormalizeGuid(Node->NodeGuid));
    Metadata->SetNumberField(TEXT("node_flags"), static_cast<int64>(Node->GetFlags()));
    Metadata->SetBoolField(TEXT("has_compiler_message"), Node->bHasCompilerMessage);
    if (Node->bHasCompilerMessage)
    {
        Metadata->SetNumberField(TEXT("compiler_message_type"), Node->ErrorType);
        Metadata->SetStringField(TEXT("compiler_message"), Node->ErrorMsg);
    }
    Metadata->SetBoolField(TEXT("blueprint_has_breakpoints"), FKismetDebugUtilities::BlueprintHasBreakpoints(Blueprint));
    Info.Metadata = Metadata;

    return TResult<FDetailedNodeInfo>::Success(Info);
}

TResult<TArray<FDetailedNodeInfo>> FBlueprintNodeService::DescribeAllNodes(UBlueprint* Blueprint, const FString& GraphScope, 
                                                                           bool bIncludePins, bool bIncludeInternalPins,
                                                                           int32 Offset, int32 Limit)
{
    if (!Blueprint)
    {
        return TResult<TArray<FDetailedNodeInfo>>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    // Resolve target graphs based on scope
    TArray<UEdGraph*> CandidateGraphs;
    FString Error;
    
    bool bAllGraphs = GraphScope.Equals(TEXT("all"), ESearchCase::IgnoreCase);
    if (!bAllGraphs && !GraphScope.IsEmpty())
    {
        UEdGraph* PreferredGraph = ResolveTargetGraph(Blueprint, GraphScope, Error);
        if (!PreferredGraph && !Error.IsEmpty())
        {
            return TResult<TArray<FDetailedNodeInfo>>::Error(VibeUE::ErrorCodes::GRAPH_NOT_FOUND, Error);
        }
        if (PreferredGraph)
        {
            GatherCandidateGraphs(Blueprint, PreferredGraph, CandidateGraphs);
        }
    }
    
    if (CandidateGraphs.Num() == 0)
    {
        GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
    }

    TArray<FDetailedNodeInfo> Results;
    int32 Skipped = 0;
    int32 Collected = 0;

    for (UEdGraph* Graph : CandidateGraphs)
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

            if (Skipped < Offset)
            {
                ++Skipped;
                continue;
            }

            if (Limit >= 0 && Collected >= Limit)
            {
                break;
            }

            // Describe this node
            auto DescribeResult = DescribeNode(Blueprint, Node->NodeGuid.ToString(), bIncludePins, bIncludeInternalPins);
            if (DescribeResult.IsSuccess())
            {
                Results.Add(DescribeResult.GetValue());
                ++Collected;
            }
            else
            {
                // Log failure for diagnostics but continue processing other nodes
                UE_LOG(LogBlueprintNodeService, Warning, TEXT("Failed to describe node %s: %s"), 
                    *Node->NodeGuid.ToString(), *DescribeResult.GetErrorMessage());
            }
        }

        if (Limit >= 0 && Collected >= Limit)
        {
            break;
        }
    }

    return TResult<TArray<FDetailedNodeInfo>>::Success(Results);
}

TSharedPtr<FJsonObject> FBlueprintNodeService::ConvertPinInfoToJson(const FPinInfo& PinInfo)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    PinObject->SetStringField(TEXT("pin_id"), PinInfo.PinId);
    PinObject->SetStringField(TEXT("name"), PinInfo.Name);
    PinObject->SetStringField(TEXT("direction"), PinInfo.Direction);
    PinObject->SetStringField(TEXT("category"), PinInfo.Category);
    PinObject->SetStringField(TEXT("subcategory"), PinInfo.SubCategory);
    if (!PinInfo.PinTypePath.IsEmpty())
    {
        PinObject->SetStringField(TEXT("pin_type_path"), PinInfo.PinTypePath);
    }
    PinObject->SetStringField(TEXT("container"), PinInfo.Container);
    PinObject->SetBoolField(TEXT("is_const"), PinInfo.bIsConst);
    PinObject->SetBoolField(TEXT("is_reference"), PinInfo.bIsReference);
    PinObject->SetBoolField(TEXT("is_array"), PinInfo.bIsArray);
    PinObject->SetBoolField(TEXT("is_set"), PinInfo.bIsSet);
    PinObject->SetBoolField(TEXT("is_map"), PinInfo.bIsMap);
    PinObject->SetBoolField(TEXT("is_hidden"), PinInfo.bIsHidden);
    PinObject->SetBoolField(TEXT("is_advanced"), PinInfo.bIsAdvanced);
    PinObject->SetBoolField(TEXT("is_connected"), PinInfo.bIsConnected);

    if (!PinInfo.Tooltip.IsEmpty())
    {
        PinObject->SetStringField(TEXT("tooltip"), PinInfo.Tooltip);
    }

    if (!PinInfo.DefaultValue.IsEmpty())
    {
        PinObject->SetStringField(TEXT("default_value"), PinInfo.DefaultValue);
    }
    if (!PinInfo.DefaultText.IsEmpty())
    {
        PinObject->SetStringField(TEXT("default_text"), PinInfo.DefaultText);
    }
    if (!PinInfo.DefaultObjectPath.IsEmpty())
    {
        PinObject->SetStringField(TEXT("default_object_path"), PinInfo.DefaultObjectPath);
    }

    if (PinInfo.DefaultValueJson.IsValid())
    {
        PinObject->SetField(TEXT("default_value_json"), PinInfo.DefaultValueJson);
    }

    TArray<TSharedPtr<FJsonValue>> LinksArray;
    for (const TSharedPtr<FJsonObject>& Link : PinInfo.Links)
    {
        LinksArray.Add(MakeShared<FJsonValueObject>(Link));
    }
    PinObject->SetArrayField(TEXT("links"), LinksArray);

    return PinObject;
}

TSharedPtr<FJsonObject> FBlueprintNodeService::ConvertNodeInfoToJson(const FDetailedNodeInfo& NodeInfo, bool bIncludePins)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    NodeObject->SetStringField(TEXT("node_id"), NodeInfo.NodeId);
    NodeObject->SetStringField(TEXT("display_name"), NodeInfo.DisplayName);
    NodeObject->SetStringField(TEXT("class_path"), NodeInfo.ClassPath);
    NodeObject->SetStringField(TEXT("graph_scope"), NodeInfo.GraphScope);
    NodeObject->SetStringField(TEXT("graph_name"), NodeInfo.GraphName);
    NodeObject->SetStringField(TEXT("graph_guid"), NodeInfo.GraphGuid);

    TSharedPtr<FJsonObject> Position = MakeShared<FJsonObject>();
    Position->SetNumberField(TEXT("x"), NodeInfo.Position.X);
    Position->SetNumberField(TEXT("y"), NodeInfo.Position.Y);
    NodeObject->SetObjectField(TEXT("position"), Position);

    if (!NodeInfo.Comment.IsEmpty())
    {
        NodeObject->SetStringField(TEXT("comment"), NodeInfo.Comment);
    }

    NodeObject->SetBoolField(TEXT("is_pure"), NodeInfo.bIsPure);
    NodeObject->SetStringField(TEXT("exec_state"), NodeInfo.ExecState);

    if (NodeInfo.NodeDescriptor.IsValid())
    {
        NodeObject->SetObjectField(TEXT("node_descriptor"), NodeInfo.NodeDescriptor);
    }

    if (!NodeInfo.SpawnerKey.IsEmpty())
    {
        NodeObject->SetStringField(TEXT("spawner_key"), NodeInfo.SpawnerKey);
    }

    if (NodeInfo.NodeParams.IsValid())
    {
        NodeObject->SetObjectField(TEXT("node_params"), NodeInfo.NodeParams);
    }

    if (NodeInfo.FunctionMetadata.IsValid())
    {
        NodeObject->SetObjectField(TEXT("function_metadata"), NodeInfo.FunctionMetadata);
    }

    if (NodeInfo.VariableMetadata.IsValid())
    {
        NodeObject->SetObjectField(TEXT("variable_metadata"), NodeInfo.VariableMetadata);
    }

    if (NodeInfo.CastMetadata.IsValid())
    {
        NodeObject->SetObjectField(TEXT("cast_metadata"), NodeInfo.CastMetadata);
    }

    if (bIncludePins)
    {
        TArray<TSharedPtr<FJsonValue>> PinsArray;
        for (const FPinInfo& PinInfo : NodeInfo.Pins)
        {
            PinsArray.Add(MakeShared<FJsonValueObject>(ConvertPinInfoToJson(PinInfo)));
        }
        NodeObject->SetArrayField(TEXT("pins"), PinsArray);
    }

    if (NodeInfo.Metadata.IsValid())
    {
        NodeObject->SetObjectField(TEXT("metadata"), NodeInfo.Metadata);
    }

    return NodeObject;
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
    
    // Resolve target graph (defaults to EventGraph if Config.GraphName is empty)
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
