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

TResult<TSharedPtr<FJsonObject>> FBlueprintNodeService::GetNodeDetailsAdvanced(
	UBlueprint* Blueprint,
	const FString& NodeId,
	const TSharedPtr<FJsonObject>& Params)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	if (auto ValidationResult = ValidateNotEmpty(NodeId, TEXT("NodeId")); ValidationResult.IsError())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	// Extract parameters
	FString GraphScope = TEXT("event");
	FString FunctionName;
	bool bIncludePins = true;
	bool bIncludeProperties = false;
	bool bIncludeConnections = false;
	
	if (Params.IsValid())
	{
		Params->TryGetStringField(TEXT("graph_scope"), GraphScope);
		Params->TryGetStringField(TEXT("function_name"), FunctionName);
		Params->TryGetBoolField(TEXT("include_pins"), bIncludePins);
		Params->TryGetBoolField(TEXT("include_properties"), bIncludeProperties);
		Params->TryGetBoolField(TEXT("include_connections"), bIncludeConnections);
	}
	
	// Resolve target graph
	UEdGraph* TargetGraph = nullptr;
	
	if (GraphScope == TEXT("function") && !FunctionName.IsEmpty())
	{
		// Find function graph by name
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (Graph && Graph->GetName() == FunctionName)
			{
				TargetGraph = Graph;
				break;
			}
		}
		
		if (!TargetGraph)
		{
			return TResult<TSharedPtr<FJsonObject>>::Error(
				VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
				FString::Printf(TEXT("Function graph not found: %s"), *FunctionName)
			);
		}
	}
	else
	{
		// Default to event graph using ResolveTargetGraph
		TargetGraph = ResolveTargetGraph(Blueprint, FString());
		
		if (!TargetGraph)
		{
			return TResult<TSharedPtr<FJsonObject>>::Error(
				VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
				TEXT("Event graph not found")
			);
		}
	}
	
	// Find the node
	UEdGraphNode* Found = nullptr;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node && Node->NodeGuid.ToString() == NodeId)
		{
			Found = Node;
			break;
		}
	}
	
	if (!Found)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::NODE_NOT_FOUND,
			FString::Printf(TEXT("Node not found: %s"), *NodeId)
		);
	}
	
	// Build comprehensive node information
	TSharedPtr<FJsonObject> NodeInfo = MakeShared<FJsonObject>();
	NodeInfo->SetStringField(TEXT("id"), Found->NodeGuid.ToString());
	NodeInfo->SetStringField(TEXT("node_class"), Found->GetClass()->GetName());
	NodeInfo->SetStringField(TEXT("title"), Found->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	
	// Position information
	TArray<TSharedPtr<FJsonValue>> Position;
	Position.Add(MakeShared<FJsonValueNumber>(Found->NodePosX));
	Position.Add(MakeShared<FJsonValueNumber>(Found->NodePosY));
	NodeInfo->SetArrayField(TEXT("position"), Position);
	
	// Add category and metadata for K2 nodes
	if (UK2Node* K2Node = Cast<UK2Node>(Found))
	{
		NodeInfo->SetStringField(TEXT("category"), K2Node->GetMenuCategory().ToString());
		NodeInfo->SetStringField(TEXT("tooltip"), K2Node->GetTooltipText().ToString());
		NodeInfo->SetStringField(TEXT("keywords"), K2Node->GetKeywords().ToString());
	}
	
	// Add node state information
	NodeInfo->SetBoolField(TEXT("can_user_delete_node"), Found->CanUserDeleteNode());
	
	// Include pins if requested
	if (bIncludePins)
	{
		TArray<TSharedPtr<FJsonValue>> InputPins;
		TArray<TSharedPtr<FJsonValue>> OutputPins;
		
		for (UEdGraphPin* Pin : Found->Pins)
		{
			if (!Pin) continue;
			
			TSharedPtr<FJsonObject> PinInfo = MakeShared<FJsonObject>();
			PinInfo->SetStringField(TEXT("name"), Pin->PinName.ToString());
			PinInfo->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());
			PinInfo->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
			PinInfo->SetBoolField(TEXT("is_hidden"), Pin->bHidden);
			PinInfo->SetBoolField(TEXT("is_connected"), Pin->LinkedTo.Num() > 0);
			
			// Add default value information
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
			
			// Add connection information if requested
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
			
			// Add pin type details
			if (!Pin->PinType.PinSubCategory.IsNone())
			{
				PinInfo->SetStringField(TEXT("sub_category"), Pin->PinType.PinSubCategory.ToString());
			}
			if (Pin->PinType.PinSubCategoryObject.IsValid())
			{
				PinInfo->SetStringField(TEXT("sub_category_object"), Pin->PinType.PinSubCategoryObject->GetName());
			}
			
			// Add to appropriate array
			if (Pin->Direction == EGPD_Input)
			{
				InputPins.Add(MakeShared<FJsonValueObject>(PinInfo));
			}
			else
			{
				OutputPins.Add(MakeShared<FJsonValueObject>(PinInfo));
			}
		}
		
		NodeInfo->SetArrayField(TEXT("input_pins"), InputPins);
		NodeInfo->SetArrayField(TEXT("output_pins"), OutputPins);
	}
	
	// Include properties if requested
	if (bIncludeProperties)
	{
		TArray<TSharedPtr<FJsonValue>> Properties;
		
		for (TFieldIterator<FProperty> PropIt(Found->GetClass()); PropIt; ++PropIt)
		{
			FProperty* Prop = *PropIt;
			if (!Prop || Prop->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient))
			{
				continue;
			}
			
			if (Prop->HasAnyPropertyFlags(CPF_Edit | CPF_EditConst))
			{
				TSharedPtr<FJsonObject> PropInfo = MakeShared<FJsonObject>();
				PropInfo->SetStringField(TEXT("name"), Prop->GetName());
				PropInfo->SetStringField(TEXT("type"), Prop->GetCPPType());
				
				// Try to get the property value as a string
				FString ValueStr;
				const void* PropValuePtr = Prop->ContainerPtrToValuePtr<void>(Found);
				if (PropValuePtr)
				{
					Prop->ExportTextItem_Direct(ValueStr, PropValuePtr, nullptr, nullptr, PPF_None);
					PropInfo->SetStringField(TEXT("value"), ValueStr);
				}
				
				Properties.Add(MakeShared<FJsonValueObject>(PropInfo));
			}
		}
		
		NodeInfo->SetArrayField(TEXT("properties"), Properties);
	}
	
	// Build result
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetObjectField(TEXT("node_info"), NodeInfo);
	
	return TResult<TSharedPtr<FJsonObject>>::Success(Result);
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

TResult<TArray<TSharedPtr<FJsonObject>>> FBlueprintNodeService::DescribeNodesAdvanced(
	UBlueprint* Blueprint,
	const TSharedPtr<FJsonObject>& Params,
	bool bIncludePins,
	bool bIncludeInternalPins)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<TArray<TSharedPtr<FJsonObject>>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	if (!Params.IsValid())
	{
		return TResult<TArray<TSharedPtr<FJsonObject>>>::Error(
			VibeUE::ErrorCodes::INVALID_PARAMETER,
			TEXT("Params object is null")
		);
	}

	// Extract pagination parameters
	double OffsetValue = 0.0;
	int32 Offset = 0;
	if (Params->TryGetNumberField(TEXT("offset"), OffsetValue))
	{
		Offset = FMath::Max(0, static_cast<int32>(OffsetValue));
	}

	double LimitValue = -1.0;
	int32 Limit = -1;
	if (Params->TryGetNumberField(TEXT("limit"), LimitValue))
	{
		Limit = static_cast<int32>(LimitValue);
		if (Limit < 0)
		{
			Limit = -1;
		}
	}

	// Extract graph scope
	FString GraphScopeValue;
	Params->TryGetStringField(TEXT("graph_scope"), GraphScopeValue);
	const bool bAllGraphs = GraphScopeValue.Equals(TEXT("all"), ESearchCase::IgnoreCase);

	FString GraphName;
	Params->TryGetStringField(TEXT("function_name"), GraphName);

	// Gather candidate graphs
	TArray<UEdGraph*> CandidateGraphs;
	if (bAllGraphs)
	{
		GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
	}
	else
	{
		UEdGraph* PreferredGraph = ResolveTargetGraph(Blueprint, GraphName);
		GatherCandidateGraphs(Blueprint, PreferredGraph, CandidateGraphs);
		
		if (CandidateGraphs.Num() == 0 && PreferredGraph)
		{
			CandidateGraphs.Add(PreferredGraph);
		}
	}

	if (CandidateGraphs.Num() == 0)
	{
		return TResult<TArray<TSharedPtr<FJsonObject>>>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			TEXT("No graphs available for description")
		);
	}

	// Extract node ID filters
	TSet<FGuid> NodeGuidFilters;
	TSet<FString> NodeStringFilters;
	const TArray<TSharedPtr<FJsonValue>>* NodeIdArray = nullptr;
	if (Params->TryGetArrayField(TEXT("node_ids"), NodeIdArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *NodeIdArray)
		{
			if (!Value.IsValid())
			{
				continue;
			}

			FString RawId = Value->AsString();
			RawId.TrimStartAndEndInline();
			if (RawId.IsEmpty())
			{
				continue;
			}

			FGuid ParsedGuid;
			if (FGuid::Parse(RawId, ParsedGuid))
			{
				NodeGuidFilters.Add(ParsedGuid);
			}
			else
			{
				RawId.ToLowerInline();
				NodeStringFilters.Add(RawId);
			}
		}
	}

	// Extract pin name filters
	TSet<FName> PinNameFilters;
	const TArray<TSharedPtr<FJsonValue>>* PinArray = nullptr;
	if (Params->TryGetArrayField(TEXT("pin_names"), PinArray))
	{
		for (const TSharedPtr<FJsonValue>& Value : *PinArray)
		{
			if (!Value.IsValid())
			{
				continue;
			}

			const FString PinName = Value->AsString();
			if (!PinName.IsEmpty())
			{
				PinNameFilters.Add(FName(*PinName));
			}
		}
	}
	const bool bHasPinFilter = PinNameFilters.Num() > 0;

	// Node matching lambda
	auto NodeMatchesFilters = [&NodeGuidFilters, &NodeStringFilters](UEdGraphNode* Node) -> bool
	{
		if (!Node)
		{
			return false;
		}

		if (NodeGuidFilters.Num() == 0 && NodeStringFilters.Num() == 0)
		{
			return true;
		}

		if (NodeGuidFilters.Contains(Node->NodeGuid))
		{
			return true;
		}

		FString GuidString = Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces);
		GuidString.ToLowerInline();
		if (NodeStringFilters.Contains(GuidString))
		{
			return true;
		}

		FString CompactGuid = Node->NodeGuid.ToString(EGuidFormats::Digits);
		CompactGuid.ToLowerInline();
		if (NodeStringFilters.Contains(CompactGuid))
		{
			return true;
		}

		FString NodeName = Node->GetName();
		NodeName.ToLowerInline();
		if (NodeStringFilters.Contains(NodeName))
		{
			return true;
		}

		return false;
	};

	// Collect and describe nodes
	TArray<TSharedPtr<FJsonObject>> NodesArray;
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
			if (!Node || !NodeMatchesFilters(Node))
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

			// Build node descriptor
			TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
			NodeObject->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			NodeObject->SetStringField(TEXT("display_name"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
			NodeObject->SetStringField(TEXT("class_path"), Node->GetClass()->GetPathName());
			NodeObject->SetStringField(TEXT("graph_name"), Graph->GetName());
			NodeObject->SetStringField(TEXT("graph_guid"), Graph->GraphGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));

			// Position
			TSharedPtr<FJsonObject> Position = MakeShared<FJsonObject>();
			Position->SetNumberField(TEXT("x"), Node->NodePosX);
			Position->SetNumberField(TEXT("y"), Node->NodePosY);
			NodeObject->SetObjectField(TEXT("position"), Position);

			// Comment
			if (!Node->NodeComment.IsEmpty())
			{
				NodeObject->SetStringField(TEXT("comment"), Node->NodeComment);
			}

			// Include pins if requested
			if (bIncludePins)
			{
				TArray<TSharedPtr<FJsonValue>> PinArrayJson;
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

					if (bHasPinFilter && !PinNameFilters.Contains(Pin->PinName))
					{
						continue;
					}

					PinArrayJson.Add(MakeShared<FJsonValueObject>(BuildPinDescriptor(Pin)));
				}
				NodeObject->SetArrayField(TEXT("pins"), PinArrayJson);
			}

			NodesArray.Add(NodeObject);
			++Collected;
		}

		if (Limit >= 0 && Collected >= Limit)
		{
			break;
		}
	}

	LogInfo(FString::Printf(TEXT("Described %d nodes from %d graphs"), NodesArray.Num(), CandidateGraphs.Num()));

	return TResult<TArray<TSharedPtr<FJsonObject>>>::Success(NodesArray);
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

TResult<TSharedPtr<FJsonObject>> FBlueprintNodeService::ConfigureNodeAdvanced(
	UBlueprint* Blueprint,
	const FString& NodeId,
	const TSharedPtr<FJsonObject>& Params)
{
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	if (auto ValidationResult = ValidateNotEmpty(NodeId, TEXT("NodeId")); ValidationResult.IsError())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}
	
	// Helper to collect string values from various field names
	auto CollectStrings = [](const TSharedPtr<FJsonObject>& Source, const TArray<FString>& Fields, TArray<FString>& OutValues)
	{
		if (!Source.IsValid())
		{
			return;
		}
		
		for (const FString& Field : Fields)
		{
			// Try as single string
			FString SingleValue;
			if (Source->TryGetStringField(Field, SingleValue))
			{
				OutValues.AddUnique(SingleValue);
			}
			
			// Try as array
			const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
			if (Source->TryGetArrayField(Field, ArrayValue) && ArrayValue)
			{
				for (const TSharedPtr<FJsonValue>& Value : *ArrayValue)
				{
					if (Value.IsValid() && Value->Type == EJson::String)
					{
						OutValues.AddUnique(Value->AsString());
					}
				}
			}
		}
	};
	
	// Helper to gather pins from pin_operations array
	auto GatherFromOperations = [](const TSharedPtr<FJsonObject>& Source, bool bSplit, TArray<FString>& OutPins)
	{
		if (!Source.IsValid())
		{
			return;
		}
		
		const TArray<TSharedPtr<FJsonValue>>* PinOperations = nullptr;
		if (!Source->TryGetArrayField(TEXT("pin_operations"), PinOperations) || !PinOperations)
		{
			return;
		}
		
		for (const TSharedPtr<FJsonValue>& Value : *PinOperations)
		{
			const TSharedPtr<FJsonObject>* OperationObject = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(OperationObject) || !OperationObject)
			{
				continue;
			}
			
			FString Action;
			if (!(*OperationObject)->TryGetStringField(TEXT("action"), Action))
			{
				continue;
			}
			Action.TrimStartAndEndInline();
			
			const bool bMatches = bSplit
				? Action.Equals(TEXT("split"), ESearchCase::IgnoreCase)
				: (Action.Equals(TEXT("recombine"), ESearchCase::IgnoreCase) || Action.Equals(TEXT("unsplit"), ESearchCase::IgnoreCase));
			
			if (!bMatches)
			{
				continue;
			}
			
			// Collect pin names from various field names
			TArray<FString> PinFields = {TEXT("pin"), TEXT("pin_name"), TEXT("name")};
			for (const FString& Field : PinFields)
			{
				FString PinName;
				if ((*OperationObject)->TryGetStringField(Field, PinName))
				{
					OutPins.AddUnique(PinName);
				}
			}
		}
	};
	
	// Collect pins to split and recombine
	TArray<FString> PinsToSplit;
	TArray<FString> PinsToRecombine;
	
	const TArray<FString> SplitFields = {TEXT("split_pin"), TEXT("split_pins"), TEXT("pins_to_split")};
	const TArray<FString> RecombineFields = {TEXT("recombine_pin"), TEXT("recombine_pins"), TEXT("unsplit_pins"), TEXT("collapse_pins")};
	
	// Gather from main params
	CollectStrings(Params, SplitFields, PinsToSplit);
	CollectStrings(Params, RecombineFields, PinsToRecombine);
	
	// Gather from extra object
	const TSharedPtr<FJsonObject>* Extra = nullptr;
	if (Params->TryGetObjectField(TEXT("extra"), Extra) && Extra)
	{
		CollectStrings(*Extra, SplitFields, PinsToSplit);
		CollectStrings(*Extra, RecombineFields, PinsToRecombine);
		GatherFromOperations(*Extra, true, PinsToSplit);
		GatherFromOperations(*Extra, false, PinsToRecombine);
	}
	
	// Gather from node_config object
	const TSharedPtr<FJsonObject>* NodeConfig = nullptr;
	if (Params->TryGetObjectField(TEXT("node_config"), NodeConfig) && NodeConfig)
	{
		CollectStrings(*NodeConfig, SplitFields, PinsToSplit);
		CollectStrings(*NodeConfig, RecombineFields, PinsToRecombine);
		GatherFromOperations(*NodeConfig, true, PinsToSplit);
		GatherFromOperations(*NodeConfig, false, PinsToRecombine);
	}
	
	if (PinsToSplit.Num() == 0 && PinsToRecombine.Num() == 0)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::INVALID_PARAMETER,
			TEXT("No configuration operations specified")
		);
	}
	
	// Execute split operations
	TArray<TSharedPtr<FJsonValue>> SplitResults;
	int32 SplitSuccessCount = 0;
	
	for (const FString& PinName : PinsToSplit)
	{
		TSharedPtr<FJsonObject> PinResult = MakeShared<FJsonObject>();
		PinResult->SetStringField(TEXT("pin"), PinName);
		PinResult->SetStringField(TEXT("operation"), TEXT("split"));
		
		auto Result = SplitPin(Blueprint, NodeId, PinName);
		if (Result.IsSuccess())
		{
			PinResult->SetBoolField(TEXT("success"), true);
			SplitSuccessCount++;
		}
		else
		{
			PinResult->SetBoolField(TEXT("success"), false);
			PinResult->SetStringField(TEXT("error"), Result.GetErrorMessage());
		}
		
		SplitResults.Add(MakeShared<FJsonValueObject>(PinResult));
	}
	
	// Execute recombine operations
	TArray<TSharedPtr<FJsonValue>> RecombineResults;
	int32 RecombineSuccessCount = 0;
	
	for (const FString& PinName : PinsToRecombine)
	{
		TSharedPtr<FJsonObject> PinResult = MakeShared<FJsonObject>();
		PinResult->SetStringField(TEXT("pin"), PinName);
		PinResult->SetStringField(TEXT("operation"), TEXT("recombine"));
		
		auto Result = RecombinePin(Blueprint, NodeId, PinName);
		if (Result.IsSuccess())
		{
			PinResult->SetBoolField(TEXT("success"), true);
			RecombineSuccessCount++;
		}
		else
		{
			PinResult->SetBoolField(TEXT("success"), false);
			PinResult->SetStringField(TEXT("error"), Result.GetErrorMessage());
		}
		
		RecombineResults.Add(MakeShared<FJsonValueObject>(PinResult));
	}
	
	// Build comprehensive response
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	
	const int32 TotalOperations = PinsToSplit.Num() + PinsToRecombine.Num();
	const int32 TotalSuccess = SplitSuccessCount + RecombineSuccessCount;
	const bool bOverallSuccess = (TotalSuccess == TotalOperations);
	
	Response->SetBoolField(TEXT("success"), bOverallSuccess);
	Response->SetStringField(TEXT("node_id"), NodeId);
	Response->SetNumberField(TEXT("operation_count"), TotalOperations);
	Response->SetNumberField(TEXT("changed_count"), TotalSuccess);
	
	// Combine all pin results
	TArray<TSharedPtr<FJsonValue>> AllPinResults;
	AllPinResults.Append(SplitResults);
	AllPinResults.Append(RecombineResults);
	Response->SetArrayField(TEXT("pins"), AllPinResults);
	
	// Add operation summaries
	TArray<TSharedPtr<FJsonValue>> OperationSummaries;
	
	if (SplitResults.Num() > 0)
	{
		TSharedPtr<FJsonObject> SplitSummary = MakeShared<FJsonObject>();
		SplitSummary->SetStringField(TEXT("operation"), TEXT("split"));
		SplitSummary->SetNumberField(TEXT("total"), PinsToSplit.Num());
		SplitSummary->SetNumberField(TEXT("success"), SplitSuccessCount);
		SplitSummary->SetArrayField(TEXT("pins"), SplitResults);
		OperationSummaries.Add(MakeShared<FJsonValueObject>(SplitSummary));
	}
	
	if (RecombineResults.Num() > 0)
	{
		TSharedPtr<FJsonObject> RecombineSummary = MakeShared<FJsonObject>();
		RecombineSummary->SetStringField(TEXT("operation"), TEXT("recombine"));
		RecombineSummary->SetNumberField(TEXT("total"), PinsToRecombine.Num());
		RecombineSummary->SetNumberField(TEXT("success"), RecombineSuccessCount);
		RecombineSummary->SetArrayField(TEXT("pins"), RecombineResults);
		OperationSummaries.Add(MakeShared<FJsonValueObject>(RecombineSummary));
	}
	
	Response->SetArrayField(TEXT("operations"), OperationSummaries);
	Response->SetStringField(TEXT("message"), 
		bOverallSuccess ? TEXT("Node configuration updated") : TEXT("One or more configuration operations failed"));
	
	return TResult<TSharedPtr<FJsonObject>>::Success(Response);
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
