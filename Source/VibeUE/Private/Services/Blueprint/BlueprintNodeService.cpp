#include "Services/Blueprint/BlueprintNodeService.h"
#include "Core/ErrorCodes.h"
#include "Commands/BlueprintReflection.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Timeline.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_InputAction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

FBlueprintNodeService::FBlueprintNodeService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

// ============================================================================
// Node Discovery
// ============================================================================

TResult<TArray<FString>> FBlueprintNodeService::FindNodes(UBlueprint* Blueprint, const FString& NodeType, const FString& GraphName)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<TArray<FString>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	if (auto ValidationResult = ValidateNotEmpty(NodeType, TEXT("NodeType")); ValidationResult.IsError())
	{
		return TResult<TArray<FString>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	// Resolve target graph
	UEdGraph* TargetGraph = ResolveTargetGraph(Blueprint, GraphName);
	if (!TargetGraph)
	{
		return TResult<TArray<FString>>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			FString::Printf(TEXT("Graph not found: %s"), GraphName.IsEmpty() ? TEXT("EventGraph") : *GraphName)
		);
	}
	
	// Use reflection to resolve node class
	UClass* TargetNodeClass = FBlueprintReflection::ResolveNodeClass(NodeType);
	if (!TargetNodeClass)
	{
		return TResult<TArray<FString>>::Error(
			VibeUE::ErrorCodes::INVALID_NODE_TYPE,
			FString::Printf(TEXT("Unknown node type: %s"), *NodeType)
		);
	}
	
	// Find matching nodes
	TArray<FString> NodeGuids;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node && Node->IsA(TargetNodeClass))
		{
			NodeGuids.Add(Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		}
	}
	
	LogInfo(FString::Printf(TEXT("Found %d nodes of type '%s' in graph '%s'"), 
		NodeGuids.Num(), *NodeType, *TargetGraph->GetName()));
	
	return TResult<TArray<FString>>::Success(NodeGuids);
}

TResult<FNodeInfo> FBlueprintNodeService::GetNodeDetails(UBlueprint* Blueprint, const FString& NodeId, const FString& GraphName)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<FNodeInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	if (auto ValidationResult = ValidateNotEmpty(NodeId, TEXT("NodeId")); ValidationResult.IsError())
	{
		return TResult<FNodeInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	// Gather candidate graphs
	TArray<UEdGraph*> CandidateGraphs;
	UEdGraph* PreferredGraph = ResolveTargetGraph(Blueprint, GraphName);
	GatherCandidateGraphs(Blueprint, PreferredGraph, CandidateGraphs);
	
	if (CandidateGraphs.Num() == 0)
	{
		return TResult<FNodeInfo>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			TEXT("No graphs available to search")
		);
	}
	
	// Find the node
	UEdGraphNode* Node = FindNodeByGuid(CandidateGraphs, NodeId);
	if (!Node)
	{
		return TResult<FNodeInfo>::Error(
			VibeUE::ErrorCodes::NODE_NOT_FOUND,
			FString::Printf(TEXT("Node not found: %s"), *NodeId)
		);
	}
	
	// Build node info
	FNodeInfo Info = BuildNodeInfo(Blueprint, Node);
	
	return TResult<FNodeInfo>::Success(Info);
}

TResult<TArray<FNodeSummary>> FBlueprintNodeService::DescribeNodes(UBlueprint* Blueprint, const TArray<FString>& NodeIds, const FString& GraphName)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<TArray<FNodeSummary>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	if (NodeIds.Num() == 0)
	{
		return TResult<TArray<FNodeSummary>>::Error(
			VibeUE::ErrorCodes::INVALID_PARAMETER,
			TEXT("NodeIds array is empty")
		);
	}
	
	// Gather candidate graphs
	TArray<UEdGraph*> CandidateGraphs;
	UEdGraph* PreferredGraph = ResolveTargetGraph(Blueprint, GraphName);
	GatherCandidateGraphs(Blueprint, PreferredGraph, CandidateGraphs);
	
	if (CandidateGraphs.Num() == 0)
	{
		return TResult<TArray<FNodeSummary>>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			TEXT("No graphs available to search")
		);
	}
	
	// Build summaries for each node
	TArray<FNodeSummary> Summaries;
	for (const FString& NodeId : NodeIds)
	{
		UEdGraphNode* Node = FindNodeByGuid(CandidateGraphs, NodeId);
		if (Node)
		{
			Summaries.Add(BuildNodeSummary(Blueprint, Node));
		}
	}
	
	return TResult<TArray<FNodeSummary>>::Success(Summaries);
}

TResult<TArray<FNodeSummary>> FBlueprintNodeService::ListNodes(UBlueprint* Blueprint, const FString& GraphName)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<TArray<FNodeSummary>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	// Resolve target graph
	UEdGraph* TargetGraph = ResolveTargetGraph(Blueprint, GraphName);
	if (!TargetGraph)
	{
		return TResult<TArray<FNodeSummary>>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			FString::Printf(TEXT("Graph not found: %s"), GraphName.IsEmpty() ? TEXT("EventGraph") : *GraphName)
		);
	}
	
	// Build summaries for all nodes
	TArray<FNodeSummary> Summaries;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node)
		{
			Summaries.Add(BuildNodeSummary(Blueprint, Node));
		}
	}
	
	return TResult<TArray<FNodeSummary>>::Success(Summaries);
}

TResult<TArray<FNodeTypeInfo>> FBlueprintNodeService::DiscoverNodeTypes(const FNodeTypeSearchCriteria& Criteria)
{
	// TODO(Phase 4): Implement node type discovery - will delegate to BlueprintReflectionService
	// This functionality exists in ReflectionCommands and needs to be extracted to ReflectionService
	// See HandleGetAvailableBlueprintNodes and HandleDiscoverNodesWithDescriptors
	return TResult<TArray<FNodeTypeInfo>>::Error(
		VibeUE::ErrorCodes::NOT_IMPLEMENTED,
		TEXT("DiscoverNodeTypes not yet implemented - will delegate to BlueprintReflectionService")
	);
}

// ============================================================================
// Node Creation
// ============================================================================

TResult<FNodeInfo> FBlueprintNodeService::CreateNode(UBlueprint* Blueprint, const FNodeCreationParams& Params)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<FNodeInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	// TODO(Phase 4): Implement node creation - complex logic needs extraction from HandleAddBlueprintNode
	// This involves spawner key resolution, node placement, pin configuration
	// Currently handled by ReflectionCommands->HandleAddBlueprintNode
	return TResult<FNodeInfo>::Error(
		VibeUE::ErrorCodes::NOT_IMPLEMENTED,
		TEXT("CreateNode not yet implemented - complex logic needs extraction from HandleAddBlueprintNode")
	);
}

TResult<FNodeInfo> FBlueprintNodeService::CreateEventNode(UBlueprint* Blueprint, const FString& EventName, const FString& GraphName)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<FNodeInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	if (auto ValidationResult = ValidateNotEmpty(EventName, TEXT("EventName")); ValidationResult.IsError())
	{
		return TResult<FNodeInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	// Resolve target graph
	UEdGraph* TargetGraph = ResolveTargetGraph(Blueprint, GraphName);
	if (!TargetGraph)
	{
		return TResult<FNodeInfo>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			FString::Printf(TEXT("Graph not found: %s"), GraphName.IsEmpty() ? TEXT("EventGraph") : *GraphName)
		);
	}
	
	// Check for existing event node with this name
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
		if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
		{
			LogInfo(FString::Printf(TEXT("Using existing event node with name %s"), *EventName));
			return TResult<FNodeInfo>::Success(BuildNodeInfo(Blueprint, EventNode));
		}
	}
	
	// Find the function to create the event
	UClass* BlueprintClass = Blueprint->GeneratedClass;
	if (!BlueprintClass)
	{
		return TResult<FNodeInfo>::Error(
			VibeUE::ErrorCodes::BLUEPRINT_NOT_COMPILED,
			TEXT("Blueprint has no generated class - compile blueprint first")
		);
	}
	
	UFunction* EventFunction = BlueprintClass->FindFunctionByName(FName(*EventName));
	if (!EventFunction)
	{
		return TResult<FNodeInfo>::Error(
			VibeUE::ErrorCodes::FUNCTION_NOT_FOUND,
			FString::Printf(TEXT("Event function not found: %s"), *EventName)
		);
	}
	
	// Create the event node
	const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "CreateEventNode", "Create Event Node"));
	TargetGraph->Modify();
	
	UK2Node_Event* EventNode = NewObject<UK2Node_Event>(TargetGraph);
	EventNode->EventReference.SetExternalMember(FName(*EventName), BlueprintClass);
	EventNode->NodePosX = 0;
	EventNode->NodePosY = 0;
	TargetGraph->AddNode(EventNode, true);
	EventNode->PostPlacedNewNode();
	EventNode->AllocateDefaultPins();
	
	TargetGraph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	
	LogInfo(FString::Printf(TEXT("Created event node '%s' in Blueprint '%s'"), *EventName, *Blueprint->GetName()));
	
	return TResult<FNodeInfo>::Success(BuildNodeInfo(Blueprint, EventNode));
}

TResult<FNodeInfo> FBlueprintNodeService::CreateInputActionNode(UBlueprint* Blueprint, const FString& ActionName, const FString& GraphName)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<FNodeInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	if (auto ValidationResult = ValidateNotEmpty(ActionName, TEXT("ActionName")); ValidationResult.IsError())
	{
		return TResult<FNodeInfo>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	// Resolve target graph
	UEdGraph* TargetGraph = ResolveTargetGraph(Blueprint, GraphName);
	if (!TargetGraph)
	{
		return TResult<FNodeInfo>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			FString::Printf(TEXT("Graph not found: %s"), GraphName.IsEmpty() ? TEXT("EventGraph") : *GraphName)
		);
	}
	
	// Create the input action node
	const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "CreateInputActionNode", "Create Input Action Node"));
	TargetGraph->Modify();
	
	UK2Node_InputAction* InputActionNode = NewObject<UK2Node_InputAction>(TargetGraph);
	InputActionNode->InputActionName = FName(*ActionName);
	InputActionNode->NodePosX = 0;
	InputActionNode->NodePosY = 0;
	TargetGraph->AddNode(InputActionNode, true);
	InputActionNode->CreateNewGuid();
	InputActionNode->PostPlacedNewNode();
	InputActionNode->AllocateDefaultPins();
	
	TargetGraph->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	
	LogInfo(FString::Printf(TEXT("Created input action node '%s' in Blueprint '%s'"), *ActionName, *Blueprint->GetName()));
	
	return TResult<FNodeInfo>::Success(BuildNodeInfo(Blueprint, InputActionNode));
}

// ============================================================================
// Pin Operations
// ============================================================================

TResult<FPinConnectionResult> FBlueprintNodeService::ConnectPins(UBlueprint* Blueprint, const FPinConnectionRequest& Request)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<FPinConnectionResult>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	return TResult<FPinConnectionResult>::Error(
		VibeUE::ErrorCodes::NOT_IMPLEMENTED,
		TEXT("ConnectPins not yet implemented - complex logic needs extraction from HandleConnectPins")
	);
}

TResult<FPinDisconnectionResult> FBlueprintNodeService::DisconnectPins(UBlueprint* Blueprint, const FString& NodeId, const FString& PinName, const FString& GraphName)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<FPinDisconnectionResult>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	return TResult<FPinDisconnectionResult>::Error(
		VibeUE::ErrorCodes::NOT_IMPLEMENTED,
		TEXT("DisconnectPins not yet implemented - needs extraction from HandleDisconnectPins")
	);
}

TResult<void> FBlueprintNodeService::SplitPin(UBlueprint* Blueprint, const FString& NodeId, const FString& PinName)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	if (auto ValidationResult = ValidateNotEmpty(NodeId, TEXT("NodeId")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	if (auto ValidationResult = ValidateNotEmpty(PinName, TEXT("PinName")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	// Gather candidate graphs
	TArray<UEdGraph*> CandidateGraphs;
	GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
	
	if (CandidateGraphs.Num() == 0)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			TEXT("No graphs available to search")
		);
	}
	
	// Find the node
	UEdGraphNode* Node = FindNodeByGuid(CandidateGraphs, NodeId);
	if (!Node)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::NODE_NOT_FOUND,
			FString::Printf(TEXT("Node not found: %s"), *NodeId)
		);
	}
	
	// Find the pin
	UEdGraphPin* Pin = FindPinByName(Node, PinName);
	if (!Pin)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::PIN_NOT_FOUND,
			FString::Printf(TEXT("Pin not found: %s"), *PinName)
		);
	}
	
	// Check if already split
	if (Pin->SubPins.Num() > 0)
	{
		LogInfo(FString::Printf(TEXT("Pin '%s' already split"), *PinName));
		return TResult<void>::Success();
	}
	
	// Check if can split
	if (!Node->CanSplitPin(Pin))
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::OPERATION_NOT_ALLOWED,
			FString::Printf(TEXT("Pin '%s' cannot be split"), *PinName)
		);
	}
	
	UEdGraph* Graph = Node->GetGraph();
	const UEdGraphSchema_K2* Schema = Graph ? Cast<UEdGraphSchema_K2>(Graph->GetSchema()) : nullptr;
	if (!Schema)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::INVALID_GRAPH_SCHEMA,
			TEXT("Graph schema is not K2")
		);
	}
	
	// Split the pin with transaction
	const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "SplitPin", "Split Blueprint Pin"));
	if (Graph)
	{
		Graph->Modify();
	}
	Node->Modify();
	
	Schema->SplitPin(Pin);
	
	if (Graph)
	{
		Graph->NotifyGraphChanged();
	}
	
	LogInfo(FString::Printf(TEXT("Split pin '%s' on node '%s'"), *PinName, *NodeId));
	
	return TResult<void>::Success();
}

TResult<void> FBlueprintNodeService::RecombinePin(UBlueprint* Blueprint, const FString& NodeId, const FString& PinName)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	if (auto ValidationResult = ValidateNotEmpty(NodeId, TEXT("NodeId")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	if (auto ValidationResult = ValidateNotEmpty(PinName, TEXT("PinName")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	// Gather candidate graphs
	TArray<UEdGraph*> CandidateGraphs;
	GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
	
	if (CandidateGraphs.Num() == 0)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			TEXT("No graphs available to search")
		);
	}
	
	// Find the node
	UEdGraphNode* Node = FindNodeByGuid(CandidateGraphs, NodeId);
	if (!Node)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::NODE_NOT_FOUND,
			FString::Printf(TEXT("Node not found: %s"), *NodeId)
		);
	}
	
	// Find the pin
	UEdGraphPin* Pin = FindPinByName(Node, PinName);
	if (!Pin)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::PIN_NOT_FOUND,
			FString::Printf(TEXT("Pin not found: %s"), *PinName)
		);
	}
	
	// Get parent pin if this is a sub-pin
	UEdGraphPin* ParentPin = Pin->ParentPin ? Pin->ParentPin : Pin;
	
	// Check if already recombined
	if (ParentPin->SubPins.Num() == 0)
	{
		LogInfo(FString::Printf(TEXT("Pin '%s' already recombined"), *PinName));
		return TResult<void>::Success();
	}
	
	UEdGraph* Graph = Node->GetGraph();
	const UEdGraphSchema_K2* Schema = Graph ? Cast<UEdGraphSchema_K2>(Graph->GetSchema()) : nullptr;
	if (!Schema)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::INVALID_GRAPH_SCHEMA,
			TEXT("Graph schema is not K2")
		);
	}
	
	// Recombine the pin with transaction
	const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "RecombinePin", "Recombine Blueprint Pin"));
	if (Graph)
	{
		Graph->Modify();
	}
	Node->Modify();
	
	Schema->RecombinePin(ParentPin);
	
	if (Graph)
	{
		Graph->NotifyGraphChanged();
	}
	
	LogInfo(FString::Printf(TEXT("Recombined pin '%s' on node '%s'"), *PinName, *NodeId));
	
	return TResult<void>::Success();
}

// ============================================================================
// Node Configuration
// ============================================================================

TResult<void> FBlueprintNodeService::SetNodeProperty(UBlueprint* Blueprint, const FString& NodeId, const FString& PropertyName, const FString& Value)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	return TResult<void>::Error(
		VibeUE::ErrorCodes::NOT_IMPLEMENTED,
		TEXT("SetNodeProperty not yet implemented - needs extraction from HandleSetBlueprintNodeProperty")
	);
}

TResult<void> FBlueprintNodeService::ResetPinDefaults(UBlueprint* Blueprint, const FString& NodeId, const TArray<FString>& PinNames)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	return TResult<void>::Error(
		VibeUE::ErrorCodes::NOT_IMPLEMENTED,
		TEXT("ResetPinDefaults not yet implemented - needs extraction from HandleResetPinDefaults")
	);
}

// ============================================================================
// Node Lifecycle
// ============================================================================

TResult<void> FBlueprintNodeService::DeleteNode(UBlueprint* Blueprint, const FString& NodeId, const FString& GraphName)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	if (auto ValidationResult = ValidateNotEmpty(NodeId, TEXT("NodeId")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	// Gather candidate graphs
	TArray<UEdGraph*> CandidateGraphs;
	UEdGraph* PreferredGraph = ResolveTargetGraph(Blueprint, GraphName);
	GatherCandidateGraphs(Blueprint, PreferredGraph, CandidateGraphs);
	
	if (CandidateGraphs.Num() == 0)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			TEXT("No graphs available to search")
		);
	}
	
	// Find the node
	UEdGraphNode* NodeToDelete = FindNodeByGuid(CandidateGraphs, NodeId);
	if (!NodeToDelete)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::NODE_NOT_FOUND,
			FString::Printf(TEXT("Node not found: %s"), *NodeId)
		);
	}
	
	// Check if node can be deleted
	if (!NodeToDelete->CanUserDeleteNode())
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::OPERATION_NOT_ALLOWED,
			FString::Printf(TEXT("Node '%s' cannot be deleted (protected engine node)"), *NodeId)
		);
	}
	
	UEdGraph* NodeGraph = NodeToDelete->GetGraph();
	
	// Disconnect all pins first
	for (UEdGraphPin* Pin : NodeToDelete->Pins)
	{
		if (Pin && Pin->LinkedTo.Num() > 0)
		{
			Pin->BreakAllPinLinks();
		}
	}
	
	// Delete the node with transaction
	const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "DeleteBlueprintNode", "Delete Blueprint Node"));
	
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
	
	LogInfo(FString::Printf(TEXT("Deleted node '%s' from Blueprint '%s'"), *NodeId, *Blueprint->GetName()));
	
	return TResult<void>::Success();
}

TResult<void> FBlueprintNodeService::MoveNode(UBlueprint* Blueprint, const FString& NodeId, int32 PosX, int32 PosY)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	if (auto ValidationResult = ValidateNotEmpty(NodeId, TEXT("NodeId")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	// Gather candidate graphs
	TArray<UEdGraph*> CandidateGraphs;
	GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
	
	if (CandidateGraphs.Num() == 0)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			TEXT("No graphs available to search")
		);
	}
	
	// Find the node
	UEdGraphNode* Node = FindNodeByGuid(CandidateGraphs, NodeId);
	if (!Node)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::NODE_NOT_FOUND,
			FString::Printf(TEXT("Node not found: %s"), *NodeId)
		);
	}
	
	UEdGraph* NodeGraph = Node->GetGraph();
	
	// Move the node with transaction
	const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "MoveBlueprintNode", "Move Blueprint Node"));
	
	if (NodeGraph)
	{
		NodeGraph->Modify();
	}
	Node->Modify();
	
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	
	if (NodeGraph)
	{
		NodeGraph->NotifyGraphChanged();
	}
	
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	
	LogInfo(FString::Printf(TEXT("Moved node '%s' to position (%d, %d) in Blueprint '%s'"), 
		*NodeId, PosX, PosY, *Blueprint->GetName()));
	
	return TResult<void>::Success();
}

TResult<void> FBlueprintNodeService::RefreshNode(UBlueprint* Blueprint, const FString& NodeId)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	if (auto ValidationResult = ValidateNotEmpty(NodeId, TEXT("NodeId")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	// Gather candidate graphs
	TArray<UEdGraph*> CandidateGraphs;
	GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
	
	if (CandidateGraphs.Num() == 0)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			TEXT("No graphs available to search")
		);
	}
	
	// Find the node
	UEdGraphNode* Node = FindNodeByGuid(CandidateGraphs, NodeId);
	if (!Node)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::NODE_NOT_FOUND,
			FString::Printf(TEXT("Node not found: %s"), *NodeId)
		);
	}
	
	UEdGraph* Graph = Node->GetGraph();
	
	// Perform refresh
	const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "RefreshBlueprintNode", "Refresh Blueprint Node"));
	
	Blueprint->Modify();
	if (Graph)
	{
		Graph->Modify();
	}
	Node->Modify();
	Node->ReconstructNode();
	
	if (Graph)
	{
		Graph->NotifyGraphChanged();
	}
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	LogInfo(FString::Printf(TEXT("Refreshed node '%s' in Blueprint '%s'"), *NodeId, *Blueprint->GetName()));
	
	return TResult<void>::Success();
}

TResult<void> FBlueprintNodeService::RefreshAllNodes(UBlueprint* Blueprint)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return ValidationResult;
	}
	
	// Gather all graphs
	TArray<UEdGraph*> Graphs;
	GatherCandidateGraphs(Blueprint, nullptr, Graphs);
	
	if (Graphs.Num() == 0)
	{
		return TResult<void>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			TEXT("No graphs available to refresh")
		);
	}
	
	// Perform refresh
	const FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "RefreshBlueprintNodes", "Refresh Blueprint Nodes"));
	
	Blueprint->Modify();
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph)
		{
			Graph->Modify();
		}
	}
	
	FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
	
	// Notify all graphs of changes
	int32 TotalNodes = 0;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph)
		{
			Graph->NotifyGraphChanged();
			TotalNodes += Graph->Nodes.Num();
		}
	}
	
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	
	LogInfo(FString::Printf(TEXT("Refreshed %d graphs (%d nodes) in Blueprint '%s'"), Graphs.Num(), TotalNodes, *Blueprint->GetName()));
	
	return TResult<void>::Success();
}

// ============================================================================
// Helper Methods
// ============================================================================

UEdGraph* FBlueprintNodeService::ResolveTargetGraph(UBlueprint* Blueprint, const FString& GraphName) const
{
	if (!Blueprint)
	{
		return nullptr;
	}
	
	// If no graph name specified, return event graph
	if (GraphName.IsEmpty())
	{
		if (Blueprint->UbergraphPages.Num() > 0)
		{
			return Blueprint->UbergraphPages[0];
		}
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
	
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			return Graph;
		}
	}
	
	return nullptr;
}

void FBlueprintNodeService::GatherCandidateGraphs(UBlueprint* Blueprint, UEdGraph* PreferredGraph, TArray<UEdGraph*>& OutGraphs) const
{
	if (!Blueprint)
	{
		return;
	}
	
	OutGraphs.Reset();
	
	// If preferred graph is specified, add it first
	if (PreferredGraph)
	{
		OutGraphs.Add(PreferredGraph);
	}
	
	// Add all ubergraph pages (event graphs)
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (Graph && !OutGraphs.Contains(Graph))
		{
			OutGraphs.Add(Graph);
		}
	}
	
	// Add function graphs
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (Graph && !OutGraphs.Contains(Graph))
		{
			OutGraphs.Add(Graph);
		}
	}
	
	// Add macro graphs
	for (UEdGraph* Graph : Blueprint->MacroGraphs)
	{
		if (Graph && !OutGraphs.Contains(Graph))
		{
			OutGraphs.Add(Graph);
		}
	}
}

UEdGraphNode* FBlueprintNodeService::FindNodeByGuid(const TArray<UEdGraph*>& Graphs, const FString& NodeGuid) const
{
	FGuid Guid;
	if (!FGuid::Parse(NodeGuid, Guid))
	{
		return nullptr;
	}
	
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}
		
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == Guid)
			{
				return Node;
			}
		}
	}
	
	return nullptr;
}

UEdGraphPin* FBlueprintNodeService::FindPinByName(UEdGraphNode* Node, const FString& PinName) const
{
	if (!Node)
	{
		return nullptr;
	}
	
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
		{
			return Pin;
		}
	}
	
	return nullptr;
}

FNodeInfo FBlueprintNodeService::BuildNodeInfo(UBlueprint* Blueprint, UEdGraphNode* Node) const
{
	FNodeInfo Info;
	
	if (!Node)
	{
		return Info;
	}
	
	Info.NodeId = Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces);
	Info.NodeClass = Node->GetClass()->GetName();
	Info.Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	Info.PosX = Node->NodePosX;
	Info.PosY = Node->NodePosY;
	Info.NodeType = DetermineNodeType(Node);
	
	// Build pin descriptors
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			Info.Pins.Add(BuildPinDescriptor(Pin));
		}
	}
	
	return Info;
}

FNodeSummary FBlueprintNodeService::BuildNodeSummary(UBlueprint* Blueprint, UEdGraphNode* Node) const
{
	FNodeSummary Summary;
	
	if (!Node)
	{
		return Summary;
	}
	
	Summary.NodeId = Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces);
	Summary.Title = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	Summary.NodeType = DetermineNodeType(Node);
	
	// Build pin descriptors
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			Summary.Pins.Add(BuildPinDescriptor(Pin));
		}
	}
	
	return Summary;
}

FString FBlueprintNodeService::DetermineNodeType(UEdGraphNode* Node) const
{
	if (!Node)
	{
		return TEXT("Unknown");
	}
	
	// Determine node type by casting to known K2Node types
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

TSharedPtr<FJsonObject> FBlueprintNodeService::BuildPinDescriptor(const UEdGraphPin* Pin) const
{
	TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
	
	if (!Pin)
	{
		return PinObj;
	}
	
	PinObj->SetStringField(TEXT("name"), Pin->PinName.ToString());
	PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
	PinObj->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
	
	if (!Pin->DefaultValue.IsEmpty())
	{
		PinObj->SetStringField(TEXT("default"), Pin->DefaultValue);
	}
	
	// Add connections for output pins
	if (Pin->Direction == EGPD_Output && Pin->LinkedTo.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Connections;
		for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
		{
			if (!LinkedPin)
			{
				continue;
			}
			
			TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
			if (UEdGraphNode* LinkedNode = LinkedPin->GetOwningNode())
			{
				ConnObj->SetStringField(TEXT("to_node_id"), LinkedNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			}
			ConnObj->SetStringField(TEXT("to_pin"), LinkedPin->PinName.ToString());
			Connections.Add(MakeShared<FJsonValueObject>(ConnObj));
		}
		
		if (Connections.Num() > 0)
		{
			PinObj->SetArrayField(TEXT("connections"), Connections);
		}
	}
	
	return PinObj;
}

bool FBlueprintNodeService::ValidateNodeGuid(const FString& Guid) const
{
	FGuid ParsedGuid;
	return FGuid::Parse(Guid, ParsedGuid);
}

bool FBlueprintNodeService::ValidatePinDirection(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	if (!SourcePin || !TargetPin)
	{
		return false;
	}
	
	return SourcePin->Direction == EGPD_Output && TargetPin->Direction == EGPD_Input;
}
