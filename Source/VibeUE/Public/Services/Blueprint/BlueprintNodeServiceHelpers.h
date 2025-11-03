#pragma once

#include "CoreMinimal.h"

// Forward declarations
class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

/**
 * Helper utilities for BlueprintNodeService
 * Extracted to keep the main service focused and under 450 lines
 */
class VIBEUE_API FBlueprintNodeServiceHelpers
{
public:
    // Graph resolution
    static UEdGraph* ResolveTargetGraph(UBlueprint* Blueprint, const FString& GraphName, FString& OutError);
    static void GatherCandidateGraphs(UBlueprint* Blueprint, UEdGraph* PreferredGraph, TArray<UEdGraph*>& OutGraphs);
    
    // Node resolution
    static bool ResolveNodeIdentifier(const FString& Identifier, const TArray<UEdGraph*>& Graphs, 
                                      UEdGraphNode*& OutNode, UEdGraph*& OutGraph);
    
    // Pin resolution and validation
    static UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName, FString& OutError);
    static bool ValidatePinConnection(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, FString& OutError);
};
