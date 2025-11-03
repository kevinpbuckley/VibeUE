#include "Services/Blueprint/BlueprintGraphService.h"
#include "Core/ErrorCodes.h"
#include "EdGraph/EdGraphNode.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Timeline.h"
#include "K2Node_MacroInstance.h"
#include "K2Node.h"
#include "Kismet2/BlueprintEditorUtils.h"

FBlueprintGraphService::FBlueprintGraphService(TSharedPtr<FServiceContext> Context)
    : FServiceBase(Context)
{
}

FString FBlueprintGraphService::DescribeGraphScope(const UBlueprint* Blueprint, const UEdGraph* Graph) const
{
    if (!Blueprint || !Graph)
    {
        return TEXT("unknown");
    }

    UEdGraph* MutableGraph = const_cast<UEdGraph*>(Graph);
    if (Blueprint->UbergraphPages.Contains(MutableGraph))
    {
        return TEXT("event");
    }
    if (Blueprint->FunctionGraphs.Contains(MutableGraph))
    {
        return TEXT("function");
    }
    if (Blueprint->MacroGraphs.Contains(MutableGraph))
    {
        return TEXT("macro");
    }
    if (Blueprint->IntermediateGeneratedGraphs.Contains(MutableGraph))
    {
        return TEXT("intermediate");
    }

    return TEXT("unknown");
}

FString FBlueprintGraphService::GetNodeTypeString(const UEdGraphNode* Node) const
{
    if (!Node)
    {
        return TEXT("Unknown");
    }

    if (Cast<UK2Node_Event>(Node)) return TEXT("Event");
    if (Cast<UK2Node_CallFunction>(Node)) return TEXT("FunctionCall");
    if (Cast<UK2Node_VariableGet>(Node)) return TEXT("VariableGet");
    if (Cast<UK2Node_VariableSet>(Node)) return TEXT("VariableSet");
    if (Cast<UK2Node_IfThenElse>(Node)) return TEXT("Branch");
    if (Cast<UK2Node_Timeline>(Node)) return TEXT("Timeline");
    if (Cast<UK2Node_MacroInstance>(Node)) return TEXT("MacroInstance");
    if (Cast<UK2Node_CustomEvent>(Node)) return TEXT("CustomEvent");
    
    return Node->GetClass()->GetName();
}

void FBlueprintGraphService::GatherCustomEvents(UEdGraph* Graph, TArray<UK2Node_CustomEvent*>& OutEvents) const
{
    if (!Graph)
    {
        return;
    }

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node))
        {
            OutEvents.Add(CustomEvent);
        }
    }
}

TResult<FString> FBlueprintGraphService::SummarizeEventGraph(UBlueprint* Blueprint, int32 MaxNodes)
{
    if (!Blueprint)
    {
        return TResult<FString>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    TResult<UEdGraph*> EventGraphResult = GetEventGraph(Blueprint);
    if (EventGraphResult.IsError())
    {
        return TResult<FString>::Error(EventGraphResult.GetErrorCode(), EventGraphResult.GetErrorMessage());
    }

    UEdGraph* EventGraph = EventGraphResult.GetValue();
    FString Summary = FString::Printf(TEXT("Event Graph Summary for %s\nTotal Nodes: %d\n"), *Blueprint->GetName(), EventGraph->Nodes.Num());
    
    TMap<FString, int32> NodeTypeCounts;
    int32 NodesProcessed = 0;
    
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        if (NodesProcessed >= MaxNodes)
        {
            Summary += FString::Printf(TEXT("\n(Showing first %d nodes, %d total)"), MaxNodes, EventGraph->Nodes.Num());
            break;
        }
        
        if (Node)
        {
            FString NodeType = GetNodeTypeString(Node);
            int32& Count = NodeTypeCounts.FindOrAdd(NodeType, 0);
            Count++;
            NodesProcessed++;
        }
    }
    
    Summary += TEXT("\nNode Types:\n");
    for (const auto& Pair : NodeTypeCounts)
    {
        Summary += FString::Printf(TEXT("  %s: %d\n"), *Pair.Key, Pair.Value);
    }
    
    return TResult<FString>::Success(Summary);
}

TResult<TArray<FString>> FBlueprintGraphService::ListCustomEvents(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return TResult<TArray<FString>>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    TResult<UEdGraph*> EventGraphResult = GetEventGraph(Blueprint);
    if (EventGraphResult.IsError())
    {
        return TResult<TArray<FString>>::Error(EventGraphResult.GetErrorCode(), EventGraphResult.GetErrorMessage());
    }

    UEdGraph* EventGraph = EventGraphResult.GetValue();
    TArray<UK2Node_CustomEvent*> CustomEvents;
    GatherCustomEvents(EventGraph, CustomEvents);
    
    TArray<FString> EventNames;
    for (UK2Node_CustomEvent* Event : CustomEvents)
    {
        if (Event)
        {
            EventNames.Add(Event->CustomFunctionName.ToString());
        }
    }
    
    return TResult<TArray<FString>>::Success(EventNames);
}

TResult<TArray<FGraphInfo>> FBlueprintGraphService::GetAllGraphs(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return TResult<TArray<FGraphInfo>>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    TArray<FGraphInfo> Graphs;
    TArray<UEdGraph*> AllGraphs;
    AllGraphs.Append(Blueprint->UbergraphPages);
    AllGraphs.Append(Blueprint->FunctionGraphs);
    AllGraphs.Append(Blueprint->MacroGraphs);
    
    for (UEdGraph* Graph : AllGraphs)
    {
        if (Graph)
        {
            FGraphInfo Info;
            Info.Name = Graph->GetName();
            Info.Guid = Graph->GraphGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces);
            Info.GraphType = DescribeGraphScope(Blueprint, Graph);
            Info.NodeCount = Graph->Nodes.Num();
            Graphs.Add(Info);
        }
    }
    
    return TResult<TArray<FGraphInfo>>::Success(Graphs);
}

TResult<FGraphInfo> FBlueprintGraphService::GetGraphInfo(UBlueprint* Blueprint, const FString& GraphName)
{
    if (!Blueprint)
    {
        return TResult<FGraphInfo>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    TResult<UEdGraph*> GraphResult = GetGraph(Blueprint, GraphName);
    if (GraphResult.IsError())
    {
        return TResult<FGraphInfo>::Error(GraphResult.GetErrorCode(), GraphResult.GetErrorMessage());
    }

    UEdGraph* Graph = GraphResult.GetValue();
    FGraphInfo Info;
    Info.Name = Graph->GetName();
    Info.Guid = Graph->GraphGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces);
    Info.GraphType = DescribeGraphScope(Blueprint, Graph);
    Info.NodeCount = Graph->Nodes.Num();
    
    return TResult<FGraphInfo>::Success(Info);
}

TResult<UEdGraph*> FBlueprintGraphService::GetGraph(UBlueprint* Blueprint, const FString& GraphName)
{
    if (!Blueprint)
    {
        return TResult<UEdGraph*>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    if (GraphName.IsEmpty())
    {
        return TResult<UEdGraph*>::Error(VibeUE::ErrorCodes::PARAM_INVALID, TEXT("Graph name is empty"));
    }

    TArray<UEdGraph*> AllGraphs;
    AllGraphs.Append(Blueprint->UbergraphPages);
    AllGraphs.Append(Blueprint->FunctionGraphs);
    AllGraphs.Append(Blueprint->MacroGraphs);
    
    for (UEdGraph* Graph : AllGraphs)
    {
        if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
        {
            return TResult<UEdGraph*>::Success(Graph);
        }
    }
    
    return TResult<UEdGraph*>::Error(VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
        FString::Printf(TEXT("Graph '%s' not found in Blueprint"), *GraphName));
}

TResult<UEdGraph*> FBlueprintGraphService::GetEventGraph(UBlueprint* Blueprint)
{
    if (!Blueprint)
    {
        return TResult<UEdGraph*>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (Graph && Graph->GetFName() == FName(TEXT("EventGraph")))
        {
            return TResult<UEdGraph*>::Success(Graph);
        }
    }
    
    if (Blueprint->UbergraphPages.Num() > 0 && Blueprint->UbergraphPages[0])
    {
        return TResult<UEdGraph*>::Success(Blueprint->UbergraphPages[0]);
    }
    
    return TResult<UEdGraph*>::Error(VibeUE::ErrorCodes::GRAPH_NOT_FOUND, TEXT("No event graph found in Blueprint"));
}

TResult<void> FBlueprintGraphService::ClearGraph(UBlueprint* Blueprint, const FString& GraphName)
{
    if (!Blueprint)
    {
        return TResult<void>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    TResult<UEdGraph*> GraphResult = GetGraph(Blueprint, GraphName);
    if (GraphResult.IsError())
    {
        return TResult<void>::Error(GraphResult.GetErrorCode(), GraphResult.GetErrorMessage());
    }

    UEdGraph* Graph = GraphResult.GetValue();
    TArray<UEdGraphNode*> NodesToRemove = Graph->Nodes;
    for (UEdGraphNode* Node : NodesToRemove)
    {
        if (Node && Node->CanUserDeleteNode())
        {
            Graph->RemoveNode(Node);
        }
    }
    
    Graph->NotifyGraphChanged();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    return TResult<void>::Success();
}

TResult<TArray<FString>> FBlueprintGraphService::ValidateGraph(UBlueprint* Blueprint, const FString& GraphName)
{
    if (!Blueprint)
    {
        return TResult<TArray<FString>>::Error(VibeUE::ErrorCodes::BLUEPRINT_NOT_FOUND, TEXT("Blueprint is null"));
    }

    TResult<UEdGraph*> GraphResult = GetGraph(Blueprint, GraphName);
    if (GraphResult.IsError())
    {
        return TResult<TArray<FString>>::Error(GraphResult.GetErrorCode(), GraphResult.GetErrorMessage());
    }

    UEdGraph* Graph = GraphResult.GetValue();
    TArray<FString> ValidationErrors;
    
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Node)
        {
            ValidationErrors.Add(TEXT("Found null node in graph"));
            continue;
        }
        
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!Pin)
            {
                ValidationErrors.Add(FString::Printf(TEXT("Node '%s' has null pin"), *Node->GetName()));
            }
        }
    }
    
    return TResult<TArray<FString>>::Success(ValidationErrors);
}

TResult<bool> FBlueprintGraphService::IsGraphValid(UBlueprint* Blueprint, const FString& GraphName)
{
    TResult<TArray<FString>> ValidationResult = ValidateGraph(Blueprint, GraphName);
    if (ValidationResult.IsError())
    {
        return TResult<bool>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
    }
    
    bool bIsValid = ValidationResult.GetValue().Num() == 0;
    return TResult<bool>::Success(bIsValid);
}
