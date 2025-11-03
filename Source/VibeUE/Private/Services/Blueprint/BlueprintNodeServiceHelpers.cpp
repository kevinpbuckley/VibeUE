#include "Services/Blueprint/BlueprintNodeServiceHelpers.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

UEdGraph* FBlueprintNodeServiceHelpers::ResolveTargetGraph(UBlueprint* Blueprint, const FString& GraphName, FString& OutError)
{
    if (!Blueprint)
    {
        OutError = TEXT("Blueprint is null");
        return nullptr;
    }

    if (GraphName.IsEmpty())
    {
        // Use default event graph
        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (Graph && Graph->GetName().Contains(TEXT("EventGraph")))
            {
                return Graph;
            }
        }
        
        // Fall back to first uber graph
        if (Blueprint->UbergraphPages.Num() > 0)
        {
            return Blueprint->UbergraphPages[0];
        }
        
        OutError = TEXT("No graphs found in blueprint");
        return nullptr;
    }

    // Search for named graph
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

    OutError = FString::Printf(TEXT("Graph '%s' not found"), *GraphName);
    return nullptr;
}

void FBlueprintNodeServiceHelpers::GatherCandidateGraphs(UBlueprint* Blueprint, UEdGraph* PreferredGraph, TArray<UEdGraph*>& OutGraphs)
{
    OutGraphs.Empty();

    if (PreferredGraph)
    {
        OutGraphs.Add(PreferredGraph);
    }

    if (Blueprint)
    {
        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (Graph && Graph != PreferredGraph)
            {
                OutGraphs.Add(Graph);
            }
        }

        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph && Graph != PreferredGraph)
            {
                OutGraphs.Add(Graph);
            }
        }
    }
}

bool FBlueprintNodeServiceHelpers::ResolveNodeIdentifier(const FString& Identifier, const TArray<UEdGraph*>& Graphs, 
                                                         UEdGraphNode*& OutNode, UEdGraph*& OutGraph)
{
    OutNode = nullptr;
    OutGraph = nullptr;

    FGuid NodeGuid;
    if (FGuid::Parse(Identifier, NodeGuid))
    {
        // Search by GUID
        for (UEdGraph* Graph : Graphs)
        {
            if (!Graph) continue;

            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (Node && Node->NodeGuid == NodeGuid)
                {
                    OutNode = Node;
                    OutGraph = Graph;
                    return true;
                }
            }
        }
    }

    // Search by name
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph) continue;

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Node->GetName().Equals(Identifier, ESearchCase::IgnoreCase))
            {
                OutNode = Node;
                OutGraph = Graph;
                return true;
            }
        }
    }

    return false;
}

UEdGraphPin* FBlueprintNodeServiceHelpers::FindPin(UEdGraphNode* Node, const FString& PinName, FString& OutError)
{
    if (!Node)
    {
        OutError = TEXT("Node is null");
        return nullptr;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
        {
            return Pin;
        }
    }

    OutError = FString::Printf(TEXT("Pin '%s' not found on node"), *PinName);
    return nullptr;
}

bool FBlueprintNodeServiceHelpers::ValidatePinConnection(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, FString& OutError)
{
    if (!SourcePin || !TargetPin)
    {
        OutError = TEXT("Source or target pin is null");
        return false;
    }

    if (SourcePin->Direction != EGPD_Output)
    {
        OutError = TEXT("Source pin must be an output pin");
        return false;
    }

    if (TargetPin->Direction != EGPD_Input)
    {
        OutError = TEXT("Target pin must be an input pin");
        return false;
    }

    const UEdGraphSchema* Schema = SourcePin->GetSchema();
    if (!Schema)
    {
        OutError = TEXT("Cannot find schema for pin validation");
        return false;
    }

    FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
    if (Response.Response != CONNECT_RESPONSE_MAKE)
    {
        OutError = Response.Message.ToString();
        return false;
    }

    return true;
}
