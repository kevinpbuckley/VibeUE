// Copyright Buckley Builds LLC 2025 All Rights Reserved.

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
#include "K2Node_Knot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Helper to suggest alternative search terms when common searches fail
static FString GetSearchSuggestion(const FString& SearchTerm)
{
	FString LowerSearch = SearchTerm.ToLower();
	
	// IMPORTANT: Basic math operators (*, +, -, /) use special K2Node_CommutativeAssociativeBinaryOperator
	// which are NOT available through standard Blueprint action discovery. These are created via
	// the right-click context menu in the Blueprint editor, not programmatically.
	
	// Multiply/times searches - basic float*float is NOT available as a discoverable node!
	if (LowerSearch.Contains(TEXT("multipl")) || LowerSearch.Contains(TEXT("times")) || 
	    LowerSearch.Contains(TEXT("*")) || LowerSearch.Contains(TEXT("float*float")) ||
	    LowerSearch.Contains(TEXT("multiply_float")))
	{
		return TEXT("STOP: Basic multiplication (float * float) is NOT available through node discovery. "
		            "In Unreal Engine, basic math operators (+, -, *, /) use special internal nodes (K2Node_CommutativeAssociativeBinaryOperator) "
		            "that cannot be created programmatically. "
		            "ALTERNATIVES: 1) Use 'MultiplyByPi' (spawner_key: 'KismetMathLibrary::MultiplyByPi') to multiply by Pi, "
		            "2) Use function parameters directly and calculate in C++, "
		            "3) For power/exponent use 'Power' (spawner_key: 'KismetMathLibrary::MultiplyMultiply_FloatFloat'). "
		            "DO NOT keep searching for 'multiply float' - it does not exist as a discoverable node.");
	}
	
	// Add/plus searches
	if (LowerSearch.Contains(TEXT("add")) && LowerSearch.Contains(TEXT("float")) ||
	    LowerSearch.Contains(TEXT("+")) || LowerSearch.Contains(TEXT("plus")))
	{
		return TEXT("STOP: Basic addition (float + float) is NOT available through node discovery. "
		            "Basic math operators use special internal nodes that cannot be created programmatically. "
		            "DO NOT keep searching for 'add float' - it does not exist as a discoverable node.");
	}
	
	// Subtract/minus searches
	if (LowerSearch.Contains(TEXT("subtract")) || LowerSearch.Contains(TEXT("minus")) ||
	    LowerSearch.Contains(TEXT("-")) && LowerSearch.Contains(TEXT("float")))
	{
		return TEXT("STOP: Basic subtraction (float - float) is NOT available through node discovery. "
		            "Basic math operators use special internal nodes that cannot be created programmatically. "
		            "DO NOT keep searching for 'subtract float' - it does not exist as a discoverable node.");
	}
	
	// Divide searches
	if (LowerSearch.Contains(TEXT("divid")) || LowerSearch.Contains(TEXT("/")) ||
	    LowerSearch.Contains(TEXT("divide_float")))
	{
		return TEXT("STOP: Basic division (float / float) is NOT available through node discovery. "
		            "Basic math operators use special internal nodes that cannot be created programmatically. "
		            "DO NOT keep searching for 'divide float' - it does not exist as a discoverable node.");
	}
	
	// Common float/int conversion searches - in UE these are FTrunc, FFloor, FCeil, Round
	if (LowerSearch.Contains(TEXT("float")) && (LowerSearch.Contains(TEXT("int")) || LowerSearch.Contains(TEXT("integer"))))
	{
		return TEXT("For float-to-integer conversion, search for: 'FTrunc' (truncate), 'FFloor' (round down), 'FCeil' (round up), or 'Round'. The spawner_key is 'KismetMathLibrary::FTrunc'.");
	}
	
	// Int to float conversion
	if (LowerSearch.Contains(TEXT("int")) && LowerSearch.Contains(TEXT("float")) && !LowerSearch.Contains(TEXT("toint")))
	{
		return TEXT("For integer-to-float conversion, search for: 'Conv_IntToDouble'. The spawner_key is 'KismetMathLibrary::Conv_IntToDouble'.");
	}
	
	// Print/debug searches
	if (LowerSearch.Contains(TEXT("print")) && LowerSearch.Contains(TEXT("string")))
	{
		return TEXT("For debugging output, search for: 'PrintString' or 'Print String'. The spawner_key is 'KismetSystemLibrary::PrintString'.");
	}
	
	// Branch/if searches
	if (LowerSearch.Contains(TEXT("if")) || LowerSearch.Contains(TEXT("branch")))
	{
		return TEXT("For conditional branching, search for: 'Branch'. The spawner_key is 'Branch'.");
	}
	
	// For loop searches
	if (LowerSearch.Contains(TEXT("for")) && LowerSearch.Contains(TEXT("loop")))
	{
		return TEXT("For loops, search for: 'ForLoop', 'ForLoopWithBreak', or 'ForEachLoop'.");
	}
	
	return FString();  // No suggestion available
}

// Helper to get common valid Blueprint node category names for error messages
static FString GetCommonCategoryNames()
{
	return TEXT("Math, Math|Float, Math|Integer, Math|Vector, Math|Rotator, Math|Transform, "
	            "Flow Control, Utilities, Utilities|String, Utilities|Text, Utilities|Array, "
	            "Development, Development|Editor, Game, Game|Actor, Game|Components, "
	            "Collision, Physics, Rendering, Animation, Audio, Input, AI, "
	            "Variables, Class|Default, Default");
}

// Helper to validate if a category looks valid (contains letters, optional pipes)
static bool IsValidCategoryFormat(const FString& Category)
{
	// Empty is valid (no filter)
	if (Category.IsEmpty())
	{
		return true;
	}
	
	// Categories should contain letters and optionally | for hierarchy
	// Invalid examples: "Math|Float" for multiply, numbers, special chars
	FString Trimmed = Category.TrimStartAndEnd();
	
	// Check for common invalid patterns that indicate AI confusion
	if (Trimmed.Contains(TEXT("KismetMathLibrary")) ||
	    Trimmed.Contains(TEXT("::")) ||
	    Trimmed.Contains(TEXT("Multiply_")) ||
	    Trimmed.Contains(TEXT("_Float")) ||
	    Trimmed.Contains(TEXT("Add_")) ||
	    Trimmed.Contains(TEXT("Subtract_")) ||
	    Trimmed.Contains(TEXT("Divide_")))
	{
		return false;  // This looks like a function name, not a category
	}
	
	return true;
}

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

TResult<TSharedPtr<FJsonObject>> FBlueprintNodeService::ConnectPinsAdvanced(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params)
{
	// Validate Blueprint
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	if (!Params.IsValid())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::INVALID_PARAMETER,
			TEXT("Params object is invalid"));
	}

	// Resolve graph scope
	FString GraphScope = TEXT("event");
	Params->TryGetStringField(TEXT("graph_scope"), GraphScope);
	
	FString FunctionName;
	Params->TryGetStringField(TEXT("function_name"), FunctionName);

	UEdGraph* PreferredGraph = nullptr;
	if (GraphScope.Equals(TEXT("function"), ESearchCase::IgnoreCase) && !FunctionName.IsEmpty())
	{
		PreferredGraph = ResolveTargetGraph(Blueprint, FunctionName);
	}
	else
	{
		PreferredGraph = ResolveTargetGraph(Blueprint, FString());
	}

	// Gather candidate graphs
	TArray<UEdGraph*> CandidateGraphs;
	GatherCandidateGraphs(Blueprint, PreferredGraph, CandidateGraphs);
	if (CandidateGraphs.Num() == 0)
	{
		GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
	}
	if (CandidateGraphs.Num() == 0)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			TEXT("No graphs available for connection"));
	}

	// Extract default connection flags
	bool bAllowConversionDefault = true;
	Params->TryGetBoolField(TEXT("allow_conversion_node"), bAllowConversionDefault);

	bool bAllowPromotionDefault = true;
	if (Params->HasField(TEXT("allow_make_array")))
	{
		Params->TryGetBoolField(TEXT("allow_make_array"), bAllowPromotionDefault);
	}
	if (Params->HasField(TEXT("allow_promotion")))
	{
		Params->TryGetBoolField(TEXT("allow_promotion"), bAllowPromotionDefault);
	}

	bool bBreakExistingDefault = true;
	if (Params->HasField(TEXT("break_existing_links")))
	{
		Params->TryGetBoolField(TEXT("break_existing_links"), bBreakExistingDefault);
	}
	if (Params->HasField(TEXT("break_existing_connections")))
	{
		Params->TryGetBoolField(TEXT("break_existing_connections"), bBreakExistingDefault);
	}

	// Extract connections array from extra parameter
	const TSharedPtr<FJsonObject>* ExtraPtr = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* ConnectionArrayPtr = nullptr;
	
	if (Params->TryGetObjectField(TEXT("extra"), ExtraPtr) && ExtraPtr && (*ExtraPtr).IsValid())
	{
		(*ExtraPtr)->TryGetArrayField(TEXT("connections"), ConnectionArrayPtr);
	}
	
	// Fallback to top-level connections array
	if (!ConnectionArrayPtr)
	{
		Params->TryGetArrayField(TEXT("connections"), ConnectionArrayPtr);
	}

	TArray<TSharedPtr<FJsonValue>> LocalConnections;
	if (!ConnectionArrayPtr || ConnectionArrayPtr->Num() == 0)
	{
		// Create single default connection from top-level parameters
		TSharedPtr<FJsonObject> DefaultConnection = MakeShared<FJsonObject>();
		
		// Copy connection parameters from top-level Params to DefaultConnection
		// Source info - support multiple field name variants
		FString SourceNodeId, SourcePinName;
		if (Params->TryGetStringField(TEXT("source_node_id"), SourceNodeId))
		{
			DefaultConnection->SetStringField(TEXT("source_node_id"), SourceNodeId);
		}
		// Support source_pin, source_pin_name, and source_pin_id as aliases
		if (Params->TryGetStringField(TEXT("source_pin"), SourcePinName) ||
			Params->TryGetStringField(TEXT("source_pin_name"), SourcePinName) ||
			Params->TryGetStringField(TEXT("source_pin_id"), SourcePinName))
		{
			DefaultConnection->SetStringField(TEXT("source_pin_name"), SourcePinName);
		}
		
		// Target info - support multiple field name variants
		FString TargetNodeId, TargetPinName;
		if (Params->TryGetStringField(TEXT("target_node_id"), TargetNodeId))
		{
			DefaultConnection->SetStringField(TEXT("target_node_id"), TargetNodeId);
		}
		// Support target_pin, target_pin_name, and target_pin_id as aliases
		if (Params->TryGetStringField(TEXT("target_pin"), TargetPinName) ||
			Params->TryGetStringField(TEXT("target_pin_name"), TargetPinName) ||
			Params->TryGetStringField(TEXT("target_pin_id"), TargetPinName))
		{
			DefaultConnection->SetStringField(TEXT("target_pin_name"), TargetPinName);
		}
		
		LocalConnections.Add(MakeShared<FJsonValueObject>(DefaultConnection));
		ConnectionArrayPtr = &LocalConnections;
	}

	// Result arrays
	TArray<TSharedPtr<FJsonValue>> Successes;
	TArray<TSharedPtr<FJsonValue>> Failures;
	TSet<UEdGraph*> ModifiedGraphs;
	bool bBlueprintModified = false;

	// Lambda to capture linked pins before operation
	auto CaptureLinkedPins = [](UEdGraphPin* Pin) -> TSet<UEdGraphPin*>
	{
		TSet<UEdGraphPin*> Result;
		if (!Pin)
		{
			return Result;
		}
		for (UEdGraphPin* Linked : Pin->LinkedTo)
		{
			if (Linked)
			{
				Result.Add(Linked);
			}
		}
		return Result;
	};

	// Lambda to summarize link information
	auto SummarizeLinks = [](UEdGraphPin* Pin, const FString& Role) -> TArray<TSharedPtr<FJsonValue>>
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		if (!Pin)
		{
			return Result;
		}

		for (UEdGraphPin* Linked : Pin->LinkedTo)
		{
			if (!Linked)
			{
				continue;
			}

			TSharedPtr<FJsonObject> LinkInfo = MakeShared<FJsonObject>();
			if (UEdGraphNode* LinkedNode = Linked->GetOwningNode())
			{
				LinkInfo->SetStringField(TEXT("other_node_id"), LinkedNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
				LinkInfo->SetStringField(TEXT("other_node_class"), LinkedNode->GetClass()->GetPathName());
			}
			LinkInfo->SetStringField(TEXT("other_pin_name"), Linked->PinName.ToString());
			LinkInfo->SetStringField(TEXT("pin_role"), Role);
			Result.Add(MakeShared<FJsonValueObject>(LinkInfo));
		}

		return Result;
	};

	// Process each connection request
	int32 Index = 0;
	for (const TSharedPtr<FJsonValue>& ConnectionValue : *ConnectionArrayPtr)
	{
		const TSharedPtr<FJsonObject>* ConnectionObjPtr = nullptr;
		if (!ConnectionValue.IsValid() || !ConnectionValue->TryGetObject(ConnectionObjPtr) || !ConnectionObjPtr)
		{
			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("INVALID_REQUEST"));
			Failure->SetStringField(TEXT("message"), TEXT("Connection entry must be an object"));
			Failure->SetNumberField(TEXT("index"), Index);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		const TSharedPtr<FJsonObject>& ConnectionObj = *ConnectionObjPtr;

		// Extract per-connection flags (override defaults)
		bool bAllowConversion = bAllowConversionDefault;
		ConnectionObj->TryGetBoolField(TEXT("allow_conversion_node"), bAllowConversion);

		bool bAllowPromotion = bAllowPromotionDefault;
		ConnectionObj->TryGetBoolField(TEXT("allow_make_array"), bAllowPromotion);
		ConnectionObj->TryGetBoolField(TEXT("allow_promotion"), bAllowPromotion);

		bool bBreakExisting = bBreakExistingDefault;
		ConnectionObj->TryGetBoolField(TEXT("break_existing_links"), bBreakExisting);
		ConnectionObj->TryGetBoolField(TEXT("break_existing_connections"), bBreakExisting);

		// Resolve source and target pins using helper (from handler - will use existing FindPinByName, etc.)
		UEdGraphPin* SourcePin = nullptr;
		UEdGraphPin* TargetPin = nullptr;
		UEdGraphNode* SourceNode = nullptr;
		UEdGraphNode* TargetNode = nullptr;
		UEdGraph* WorkingGraph = nullptr;

		// Extract source pin info - support source_pin, source_pin_name as aliases
		FString SourceNodeId, SourcePinName;
		ConnectionObj->TryGetStringField(TEXT("source_node_id"), SourceNodeId);
		if (!ConnectionObj->TryGetStringField(TEXT("source_pin_name"), SourcePinName))
		{
			ConnectionObj->TryGetStringField(TEXT("source_pin"), SourcePinName);
		}

		// Extract target pin info - support target_pin, target_pin_name as aliases
		FString TargetNodeId, TargetPinName;
		ConnectionObj->TryGetStringField(TEXT("target_node_id"), TargetNodeId);
		if (!ConnectionObj->TryGetStringField(TEXT("target_pin_name"), TargetPinName))
		{
			ConnectionObj->TryGetStringField(TEXT("target_pin"), TargetPinName);
		}

		// Find source node and pin
		if (!SourceNodeId.IsEmpty())
		{
			SourceNode = FindNodeByGuid(CandidateGraphs, SourceNodeId);
			if (SourceNode)
			{
				SourcePin = FindPinByName(SourceNode, SourcePinName);
				if (SourcePin)
				{
					WorkingGraph = SourceNode->GetGraph();
				}
			}
		}

		if (!SourcePin)
		{
			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("SOURCE_PIN_NOT_FOUND"));
			Failure->SetStringField(TEXT("message"), FString::Printf(TEXT("Source pin '%s' not found on node '%s'"), *SourcePinName, *SourceNodeId));
			Failure->SetNumberField(TEXT("index"), Index);
			Failure->SetObjectField(TEXT("request"), ConnectionObj);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		// Find target node and pin
		if (!TargetNodeId.IsEmpty())
		{
			TargetNode = FindNodeByGuid(CandidateGraphs, TargetNodeId);
			if (TargetNode)
			{
				TargetPin = FindPinByName(TargetNode, TargetPinName);
				if (!WorkingGraph && TargetPin)
				{
					WorkingGraph = TargetNode->GetGraph();
				}
			}
		}

		if (!TargetPin)
		{
			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("TARGET_PIN_NOT_FOUND"));
			Failure->SetStringField(TEXT("message"), FString::Printf(TEXT("Target pin '%s' not found on node '%s'"), *TargetPinName, *TargetNodeId));
			Failure->SetNumberField(TEXT("index"), Index);
			Failure->SetObjectField(TEXT("request"), ConnectionObj);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		// Validate pins are in same graph
		if (SourceNode->GetGraph() != TargetNode->GetGraph())
		{
			// Get graph names for debugging
			FString SourceGraphName = SourceNode->GetGraph() ? SourceNode->GetGraph()->GetName() : TEXT("NULL");
			FString TargetGraphName = TargetNode->GetGraph() ? TargetNode->GetGraph()->GetName() : TEXT("NULL");
			FString SourceNodeClass = SourceNode->GetClass()->GetName();
			FString TargetNodeClass = TargetNode->GetClass()->GetName();

			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("DIFFERENT_GRAPHS"));
			Failure->SetStringField(TEXT("message"), FString::Printf(
				TEXT("Source and target pins are not in the same graph. Source: %s (%s) in graph '%s', Target: %s (%s) in graph '%s'"),
				*SourceNodeId, *SourceNodeClass, *SourceGraphName,
				*TargetNodeId, *TargetNodeClass, *TargetGraphName));
			Failure->SetNumberField(TEXT("index"), Index);
			Failure->SetObjectField(TEXT("request"), ConnectionObj);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		// Validate pins are not identical
		if (SourcePin == TargetPin)
		{
			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("IDENTICAL_PINS"));
			Failure->SetStringField(TEXT("message"), TEXT("Source and target pins are identical"));
			Failure->SetNumberField(TEXT("index"), Index);
			Failure->SetObjectField(TEXT("request"), ConnectionObj);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		// Get schema for validation
		const UEdGraphSchema* Schema = WorkingGraph ? WorkingGraph->GetSchema() : nullptr;
		if (!Schema)
		{
			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("SCHEMA_UNAVAILABLE"));
			Failure->SetStringField(TEXT("message"), TEXT("Unable to resolve graph schema for connection"));
			Failure->SetNumberField(TEXT("index"), Index);
			Failure->SetObjectField(TEXT("request"), ConnectionObj);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		// Check schema can create connection
		const FPinConnectionResponse Response = Schema->CanCreateConnection(SourcePin, TargetPin);
		const ECanCreateConnectionResponse ResponseType = Response.Response;
		const FString ResponseMessage = Response.Message.ToString();

		if (ResponseType == CONNECT_RESPONSE_DISALLOW)
		{
			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("CONNECTION_BLOCKED"));
			
			// Enhance error message for float/double type mismatch (common in UE5)
			FString EnhancedMessage = ResponseMessage;
			if (ResponseMessage.Contains(TEXT("single-precision")) && ResponseMessage.Contains(TEXT("double-precision")))
			{
				EnhancedMessage = FString::Printf(TEXT("%s HINT: In UE5, Blueprint 'float' variables are single-precision, but math function pins use 'real' (double-precision). "
				                                       "This is an Unreal type system limitation. Options: 1) Change the variable type in the Blueprint to 'Double', "
				                                       "2) Use a conversion node, or 3) Redesign the function to avoid this mismatch."), *ResponseMessage);
			}
			
			Failure->SetStringField(TEXT("message"), EnhancedMessage.IsEmpty() ? TEXT("Schema disallowed this connection") : EnhancedMessage);
			Failure->SetNumberField(TEXT("index"), Index);
			Failure->SetObjectField(TEXT("request"), ConnectionObj);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		// Check if conversion/promotion needed but disallowed
		if ((ResponseType == CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE && !bAllowConversion) ||
			(ResponseType == CONNECT_RESPONSE_MAKE_WITH_PROMOTION && !bAllowPromotion))
		{
			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("CONVERSION_REQUIRED"));
			Failure->SetStringField(TEXT("message"), ResponseMessage.IsEmpty() ? TEXT("Connection requires conversion") : ResponseMessage);
			Failure->SetNumberField(TEXT("index"), Index);
			Failure->SetObjectField(TEXT("request"), ConnectionObj);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		const bool bRequiresBreakSource = (ResponseType == CONNECT_RESPONSE_BREAK_OTHERS_A || ResponseType == CONNECT_RESPONSE_BREAK_OTHERS_AB);
		const bool bRequiresBreakTarget = (ResponseType == CONNECT_RESPONSE_BREAK_OTHERS_B || ResponseType == CONNECT_RESPONSE_BREAK_OTHERS_AB);

		// Check for reroute nodes (K2Node_Knot) - allow multiple output connections
		bool bIsRerouteOutputPin = false;
		if (SourcePin && SourcePin->Direction == EGPD_Output)
		{
			if (UK2Node_Knot* KnotNode = Cast<UK2Node_Knot>(SourcePin->GetOwningNode()))
			{
				bIsRerouteOutputPin = true;
			}
		}

		// Check if breaking existing links required but disallowed
		if ((bRequiresBreakSource || bRequiresBreakTarget) && !bBreakExisting && !bIsRerouteOutputPin)
		{
			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("WOULD_BREAK_EXISTING"));
			Failure->SetStringField(TEXT("message"), TEXT("Connection requires breaking existing links"));
			Failure->SetNumberField(TEXT("index"), Index);
			Failure->SetObjectField(TEXT("request"), ConnectionObj);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		// Check if already linked
		const bool bAlreadyLinked = SourcePin->LinkedTo.Contains(TargetPin);

		// Capture existing connections before making changes
		TSet<UEdGraphPin*> SourceBefore = CaptureLinkedPins(SourcePin);
		TSet<UEdGraphPin*> TargetBefore = CaptureLinkedPins(TargetPin);
		TArray<TSharedPtr<FJsonValue>> BrokenLinks;

		// Summarize links that will be broken
		if (bRequiresBreakSource)
		{
			TArray<TSharedPtr<FJsonValue>> Links = SummarizeLinks(SourcePin, TEXT("source"));
			BrokenLinks.Append(Links);
		}
		if (bRequiresBreakTarget)
		{
			TArray<TSharedPtr<FJsonValue>> Links = SummarizeLinks(TargetPin, TEXT("target"));
			BrokenLinks.Append(Links);
		}

		// Break existing links if required (except for reroute output pins)
		if (bRequiresBreakSource && !bIsRerouteOutputPin)
		{
			SourcePin->BreakAllPinLinks();
		}
		if (bRequiresBreakTarget)
		{
			TargetPin->BreakAllPinLinks();
		}

		// Additional break for MAKE response with break_existing flag
		if (bBreakExisting && ResponseType == CONNECT_RESPONSE_MAKE)
		{
			const bool bSourceNeedsBreak = SourcePin->LinkedTo.Num() > 0 && SourcePin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec;
			const bool bTargetNeedsBreak = TargetPin->LinkedTo.Num() > 0 && TargetPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec;

			if (bSourceNeedsBreak)
			{
				TArray<TSharedPtr<FJsonValue>> Links = SummarizeLinks(SourcePin, TEXT("source"));
				BrokenLinks.Append(Links);
				SourcePin->BreakAllPinLinks();
			}
			if (bTargetNeedsBreak)
			{
				TArray<TSharedPtr<FJsonValue>> Links = SummarizeLinks(TargetPin, TEXT("target"));
				BrokenLinks.Append(Links);
				TargetPin->BreakAllPinLinks();
			}
		}

		// Create transaction for undo support
		FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "ConnectPins", "MCP Connect Pins"));
		if (WorkingGraph)
		{
			WorkingGraph->Modify();
		}
		if (SourceNode)
		{
			SourceNode->Modify();
		}
		if (TargetNode && TargetNode != SourceNode)
		{
			TargetNode->Modify();
		}
		SourcePin->Modify();
		TargetPin->Modify();

		// Attempt to create connection
		bool bSuccess = bAlreadyLinked;
		if (!bAlreadyLinked)
		{
			bSuccess = Schema->TryCreateConnection(SourcePin, TargetPin);
			if (!bSuccess)
			{
				SourcePin->MakeLinkTo(TargetPin);
				bSuccess = SourcePin->LinkedTo.Contains(TargetPin);
			}
		}

		if (!bSuccess)
		{
			Transaction.Cancel();

			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("CONNECTION_FAILED"));
			Failure->SetStringField(TEXT("message"), ResponseMessage.IsEmpty() ? TEXT("Failed to create connection") : ResponseMessage);
			Failure->SetNumberField(TEXT("index"), Index);
			Failure->SetObjectField(TEXT("request"), ConnectionObj);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		// Track modifications
		ModifiedGraphs.Add(WorkingGraph);
		bBlueprintModified = true;

		// Identify newly created links
		TSet<FString> SeenLinkKeys;
		TArray<TSharedPtr<FJsonValue>> CreatedLinks;

		auto AppendNewLinks = [&](UEdGraphPin* Pin, const TSet<UEdGraphPin*>& BeforeSet, const FString& Role)
		{
			if (!Pin)
			{
				return;
			}
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (!Linked || BeforeSet.Contains(Linked))
				{
					continue;
				}

				FString FromId = Pin->GetOwningNode()->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces) + TEXT(":") + Pin->PinName.ToString();
				FString ToId = Linked->GetOwningNode()->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces) + TEXT(":") + Linked->PinName.ToString();
				const FString LinkKey = FString::Printf(TEXT("%s->%s"), *FromId, *ToId);
				if (SeenLinkKeys.Contains(LinkKey))
				{
					continue;
				}
				SeenLinkKeys.Add(LinkKey);

				TSharedPtr<FJsonObject> LinkInfo = MakeShared<FJsonObject>();
				LinkInfo->SetStringField(TEXT("from_pin_id"), FromId);
				LinkInfo->SetStringField(TEXT("to_pin_id"), ToId);
				LinkInfo->SetStringField(TEXT("from_pin_role"), Role);
				if (UEdGraphNode* OtherNode = Linked->GetOwningNode())
				{
					LinkInfo->SetStringField(TEXT("to_node_id"), OtherNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
					LinkInfo->SetStringField(TEXT("to_node_class"), OtherNode->GetClass()->GetPathName());
				}
				LinkInfo->SetStringField(TEXT("to_pin_name"), Linked->PinName.ToString());
				CreatedLinks.Add(MakeShared<FJsonValueObject>(LinkInfo));
			}
		};

		AppendNewLinks(SourcePin, SourceBefore, TEXT("source"));
		AppendNewLinks(TargetPin, TargetBefore, TEXT("target"));

		// Build success result
		TSharedPtr<FJsonObject> Success = MakeShared<FJsonObject>();
		Success->SetBoolField(TEXT("success"), true);
		Success->SetNumberField(TEXT("index"), Index);
		Success->SetStringField(TEXT("source_node_id"), SourceNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		Success->SetStringField(TEXT("target_node_id"), TargetNode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		Success->SetStringField(TEXT("source_pin_name"), SourcePinName);
		Success->SetStringField(TEXT("target_pin_name"), TargetPinName);
		Success->SetBoolField(TEXT("already_connected"), bAlreadyLinked);
		if (!ResponseMessage.IsEmpty())
		{
			Success->SetStringField(TEXT("schema_response"), ResponseMessage);
		}
		if (BrokenLinks.Num() > 0)
		{
			Success->SetArrayField(TEXT("broken_links"), BrokenLinks);
		}
		if (CreatedLinks.Num() > 0)
		{
			Success->SetArrayField(TEXT("created_links"), CreatedLinks);
		}

		Successes.Add(MakeShared<FJsonValueObject>(Success));
		++Index;
	}

	// Notify graphs of changes
	for (UEdGraph* Graph : ModifiedGraphs)
	{
		if (Graph)
		{
			Graph->NotifyGraphChanged();
		}
	}

	// Mark Blueprint as modified if any connections were made
	if (bBlueprintModified)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	// Build final result
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), Failures.Num() == 0);
	Result->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
	Result->SetNumberField(TEXT("attempted"), ConnectionArrayPtr->Num());
	Result->SetNumberField(TEXT("succeeded"), Successes.Num());
	Result->SetNumberField(TEXT("failed"), Failures.Num());
	Result->SetArrayField(TEXT("connections"), Successes);
	if (Failures.Num() > 0)
	{
		Result->SetArrayField(TEXT("failures"), Failures);
	}

	// Add modified graphs info
	if (ModifiedGraphs.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> GraphArray;
		for (UEdGraph* Graph : ModifiedGraphs)
		{
			if (!Graph)
			{
				continue;
			}

			TSharedPtr<FJsonObject> GraphInfo = MakeShared<FJsonObject>();
			GraphInfo->SetStringField(TEXT("graph_name"), Graph->GetName());
			GraphInfo->SetStringField(TEXT("graph_guid"), Graph->GraphGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			GraphArray.Add(MakeShared<FJsonValueObject>(GraphInfo));
		}
		Result->SetArrayField(TEXT("modified_graphs"), GraphArray);
	}

	return TResult<TSharedPtr<FJsonObject>>::Success(Result);
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

TResult<TSharedPtr<FJsonObject>> FBlueprintNodeService::DisconnectPinsAdvanced(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params)
{
	// Validate Blueprint
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	if (!Params.IsValid())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::INVALID_PARAMETER,
			TEXT("Params object is invalid"));
	}

	// Resolve graph scope
	FString GraphScope = TEXT("event");
	Params->TryGetStringField(TEXT("graph_scope"), GraphScope);
	
	FString FunctionName;
	Params->TryGetStringField(TEXT("function_name"), FunctionName);

	UEdGraph* PreferredGraph = nullptr;
	if (GraphScope.Equals(TEXT("function"), ESearchCase::IgnoreCase) && !FunctionName.IsEmpty())
	{
		PreferredGraph = ResolveTargetGraph(Blueprint, FunctionName);
	}
	else
	{
		PreferredGraph = ResolveTargetGraph(Blueprint, FString());
	}

	// Gather candidate graphs
	TArray<UEdGraph*> CandidateGraphs;
	GatherCandidateGraphs(Blueprint, PreferredGraph, CandidateGraphs);
	if (CandidateGraphs.Num() == 0)
	{
		GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
	}
	if (CandidateGraphs.Num() == 0)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			TEXT("No graphs available for disconnection"));
	}

	// Extract default break_all flag
	bool bBreakAllDefault = true;
	if (Params->HasField(TEXT("break_all")))
	{
		Params->TryGetBoolField(TEXT("break_all"), bBreakAllDefault);
	}
	if (Params->HasField(TEXT("break_all_links")))
	{
		Params->TryGetBoolField(TEXT("break_all_links"), bBreakAllDefault);
	}

	// Extract requests from connections array, pin_ids array, or extra parameter
	const TSharedPtr<FJsonObject>* ExtraPtr = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* ConnectionsArray = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* PinArray = nullptr;
	
	// Try to get from extra parameter first
	if (Params->TryGetObjectField(TEXT("extra"), ExtraPtr) && ExtraPtr && (*ExtraPtr).IsValid())
	{
		(*ExtraPtr)->TryGetArrayField(TEXT("connections"), ConnectionsArray);
		if (!ConnectionsArray)
		{
			(*ExtraPtr)->TryGetArrayField(TEXT("pin_ids"), PinArray);
		}
	}
	
	// Fallback to top-level arrays
	if (!ConnectionsArray)
	{
		Params->TryGetArrayField(TEXT("connections"), ConnectionsArray);
	}
	if (!PinArray)
	{
		Params->TryGetArrayField(TEXT("pin_ids"), PinArray);
	}

	// Build request list
	TArray<TSharedPtr<FJsonValue>> Requests;

	if (ConnectionsArray)
	{
		Requests.Append(*ConnectionsArray);
	}

	if (PinArray)
	{
		for (const TSharedPtr<FJsonValue>& Value : *PinArray)
		{
			if (!Value.IsValid())
			{
				continue;
			}

			TSharedPtr<FJsonObject> PinRequest = MakeShared<FJsonObject>();
			PinRequest->SetStringField(TEXT("pin_id"), Value->AsString());
			Requests.Add(MakeShared<FJsonValueObject>(PinRequest));
		}
	}

	if (Requests.Num() == 0)
	{
		// Create default request from top-level parameters
		TSharedPtr<FJsonObject> DefaultRequest = MakeShared<FJsonObject>();
		DefaultRequest->Values = Params->Values;
		Requests.Add(MakeShared<FJsonValueObject>(DefaultRequest));
	}

	// Lambda to summarize link information
	auto SummarizeLinks = [](UEdGraphPin* Pin, const FString& Role) -> TArray<TSharedPtr<FJsonValue>>
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		if (!Pin)
		{
			return Result;
		}
		for (UEdGraphPin* Linked : Pin->LinkedTo)
		{
			if (!Linked)
			{
				continue;
			}

			TSharedPtr<FJsonObject> LinkInfo = MakeShared<FJsonObject>();
			if (UEdGraphNode* Node = Linked->GetOwningNode())
			{
				LinkInfo->SetStringField(TEXT("other_node_id"), Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
				LinkInfo->SetStringField(TEXT("other_node_class"), Node->GetClass()->GetPathName());
			}
			LinkInfo->SetStringField(TEXT("other_pin_name"), Linked->PinName.ToString());
			LinkInfo->SetStringField(TEXT("pin_role"), Role);
			Result.Add(MakeShared<FJsonValueObject>(LinkInfo));
		}
		return Result;
	};

	// Result arrays
	TArray<TSharedPtr<FJsonValue>> Successes;
	TArray<TSharedPtr<FJsonValue>> Failures;
	TSet<UEdGraph*> ModifiedGraphs;
	bool bBlueprintModified = false;

	// Process each disconnection request
	int32 Index = 0;
	for (const TSharedPtr<FJsonValue>& Value : Requests)
	{
		const TSharedPtr<FJsonObject>* RequestObjPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(RequestObjPtr) || !RequestObjPtr)
		{
			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("INVALID_REQUEST"));
			Failure->SetStringField(TEXT("message"), TEXT("Disconnect entry must be an object"));
			Failure->SetNumberField(TEXT("index"), Index);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		const TSharedPtr<FJsonObject>& RequestObj = *RequestObjPtr;

		// Extract per-request break_all flag
		bool bRequestBreakAll = bBreakAllDefault;
		RequestObj->TryGetBoolField(TEXT("break_all"), bRequestBreakAll);
		RequestObj->TryGetBoolField(TEXT("break_all_links"), bRequestBreakAll);

		// Determine if this is a pin-pair or single-pin request
		const bool bHasTargetHints = RequestObj->HasField(TEXT("target_pin_id")) || RequestObj->HasField(TEXT("target_pin")) ||
									RequestObj->HasField(TEXT("target_pin_name")) || RequestObj->HasField(TEXT("target_node_id")) ||
									RequestObj->HasField(TEXT("to_pin_id")) || RequestObj->HasField(TEXT("to_pin"));
		const bool bHasSourceHints = RequestObj->HasField(TEXT("source_pin_id")) || RequestObj->HasField(TEXT("source_pin")) ||
									RequestObj->HasField(TEXT("source_pin_name")) || RequestObj->HasField(TEXT("source_node_id")) ||
									RequestObj->HasField(TEXT("from_pin_id")) || RequestObj->HasField(TEXT("from_pin"));

		const bool bTreatAsPair = bHasSourceHints && bHasTargetHints;

		if (bTreatAsPair)
		{
			// Pin-pair disconnection: break specific link between two pins
			FString SourceNodeId, SourcePinName, TargetNodeId, TargetPinName;
			RequestObj->TryGetStringField(TEXT("source_node_id"), SourceNodeId);
			RequestObj->TryGetStringField(TEXT("source_pin_name"), SourcePinName);
			RequestObj->TryGetStringField(TEXT("target_node_id"), TargetNodeId);
			RequestObj->TryGetStringField(TEXT("target_pin_name"), TargetPinName);

			// Find source pin
			UEdGraphPin* SourcePin = nullptr;
			UEdGraphNode* SourceNode = nullptr;
			UEdGraph* SourceGraph = nullptr;

			if (!SourceNodeId.IsEmpty())
			{
				SourceNode = FindNodeByGuid(CandidateGraphs, SourceNodeId);
				if (SourceNode)
				{
					SourcePin = FindPinByName(SourceNode, SourcePinName);
					if (SourcePin)
					{
						SourceGraph = SourceNode->GetGraph();
					}
				}
			}

			if (!SourcePin)
			{
				TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
				Failure->SetBoolField(TEXT("success"), false);
				Failure->SetStringField(TEXT("code"), TEXT("SOURCE_PIN_NOT_FOUND"));
				Failure->SetStringField(TEXT("message"), FString::Printf(TEXT("Source pin '%s' not found on node '%s'"), *SourcePinName, *SourceNodeId));
				Failure->SetNumberField(TEXT("index"), Index);
				Failure->SetObjectField(TEXT("request"), RequestObj);
				Failures.Add(MakeShared<FJsonValueObject>(Failure));
				++Index;
				continue;
			}

			// Find target pin
			UEdGraphPin* TargetPin = nullptr;
			UEdGraphNode* TargetNode = nullptr;

			if (!TargetNodeId.IsEmpty())
			{
				TargetNode = FindNodeByGuid(CandidateGraphs, TargetNodeId);
				if (TargetNode)
				{
					TargetPin = FindPinByName(TargetNode, TargetPinName);
				}
			}

			if (!TargetPin)
			{
				TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
				Failure->SetBoolField(TEXT("success"), false);
				Failure->SetStringField(TEXT("code"), TEXT("TARGET_PIN_NOT_FOUND"));
				Failure->SetStringField(TEXT("message"), FString::Printf(TEXT("Target pin '%s' not found on node '%s'"), *TargetPinName, *TargetNodeId));
				Failure->SetNumberField(TEXT("index"), Index);
				Failure->SetObjectField(TEXT("request"), RequestObj);
				Failures.Add(MakeShared<FJsonValueObject>(Failure));
				++Index;
				continue;
			}

			// Validate pins are in same graph
			if (SourceNode->GetGraph() != TargetNode->GetGraph())
			{
				// Get graph names for debugging
				FString SourceGraphName = SourceNode->GetGraph() ? SourceNode->GetGraph()->GetName() : TEXT("NULL");
				FString TargetGraphName = TargetNode->GetGraph() ? TargetNode->GetGraph()->GetName() : TEXT("NULL");
				FString SourceNodeClass = SourceNode->GetClass()->GetName();
				FString TargetNodeClass = TargetNode->GetClass()->GetName();

				TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
				Failure->SetBoolField(TEXT("success"), false);
				Failure->SetStringField(TEXT("code"), TEXT("DIFFERENT_GRAPHS"));
				Failure->SetStringField(TEXT("message"), FString::Printf(
					TEXT("Pins are not in the same graph. Source: %s (%s) in graph '%s', Target: %s (%s) in graph '%s'"),
					*SourceNodeId, *SourceNodeClass, *SourceGraphName,
					*TargetNodeId, *TargetNodeClass, *TargetGraphName));
				Failure->SetNumberField(TEXT("index"), Index);
				Failure->SetObjectField(TEXT("request"), RequestObj);
				Failures.Add(MakeShared<FJsonValueObject>(Failure));
				++Index;
				continue;
			}

			// Check if link exists
			bool bLinkExists = false;
			for (UEdGraphPin* Linked : SourcePin->LinkedTo)
			{
				if (Linked == TargetPin)
				{
					bLinkExists = true;
					break;
				}
			}

			if (!bLinkExists)
			{
				TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
				Failure->SetBoolField(TEXT("success"), false);
				Failure->SetStringField(TEXT("code"), TEXT("LINK_NOT_FOUND"));
				Failure->SetStringField(TEXT("message"), TEXT("No link exists between the specified pins"));
				Failure->SetNumberField(TEXT("index"), Index);
				Failure->SetObjectField(TEXT("request"), RequestObj);
				Failures.Add(MakeShared<FJsonValueObject>(Failure));
				++Index;
				continue;
			}

			// Create transaction and modify
			FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "DisconnectPins", "MCP Disconnect Pins"));
			if (SourceGraph)
			{
				SourceGraph->Modify();
			}
			if (SourceNode)
			{
				SourceNode->Modify();
			}
			if (TargetNode && TargetNode != SourceNode)
			{
				TargetNode->Modify();
			}
			SourcePin->Modify();
			TargetPin->Modify();

			// Summarize links before breaking
			TArray<TSharedPtr<FJsonValue>> BrokenLinks;
			BrokenLinks.Append(SummarizeLinks(SourcePin, TEXT("source")));
			BrokenLinks.Append(SummarizeLinks(TargetPin, TEXT("target")));

			// Break the specific link
			if (const UEdGraphSchema* Schema = SourcePin->GetSchema())
			{
				Schema->BreakSinglePinLink(SourcePin, TargetPin);
			}
			else
			{
				SourcePin->BreakLinkTo(TargetPin);
				TargetPin->BreakLinkTo(SourcePin);
			}

			ModifiedGraphs.Add(SourceGraph);
			bBlueprintModified = true;

			// Build success result
			TSharedPtr<FJsonObject> Success = MakeShared<FJsonObject>();
			Success->SetBoolField(TEXT("success"), true);
			Success->SetNumberField(TEXT("index"), Index);
			Success->SetStringField(TEXT("source_pin_name"), SourcePinName);
			Success->SetStringField(TEXT("target_pin_name"), TargetPinName);
			Success->SetArrayField(TEXT("broken_links"), BrokenLinks);
			Successes.Add(MakeShared<FJsonValueObject>(Success));
			++Index;
			continue;
		}

		// Single-pin disconnection: break all links on a pin
		FString PinNodeId, PinPinName;
		RequestObj->TryGetStringField(TEXT("node_id"), PinNodeId);
		RequestObj->TryGetStringField(TEXT("pin_name"), PinPinName);
		
		// Also try pin_id field
		if (PinNodeId.IsEmpty() || PinPinName.IsEmpty())
		{
			RequestObj->TryGetStringField(TEXT("source_node_id"), PinNodeId);
			RequestObj->TryGetStringField(TEXT("source_pin_name"), PinPinName);
		}

		// Find pin
		UEdGraphPin* Pin = nullptr;
		UEdGraphNode* Node = nullptr;
		UEdGraph* Graph = nullptr;

		if (!PinNodeId.IsEmpty())
		{
			Node = FindNodeByGuid(CandidateGraphs, PinNodeId);
			if (Node)
			{
				Pin = FindPinByName(Node, PinPinName);
				if (Pin)
				{
					Graph = Node->GetGraph();
				}
			}
		}

		if (!Pin)
		{
			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("PIN_NOT_FOUND"));
			Failure->SetStringField(TEXT("message"), FString::Printf(TEXT("Pin '%s' not found on node '%s'"), *PinPinName, *PinNodeId));
			Failure->SetNumberField(TEXT("index"), Index);
			Failure->SetObjectField(TEXT("request"), RequestObj);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		// Check break_all flag
		if (!bRequestBreakAll)
		{
			TSharedPtr<FJsonObject> Failure = MakeShared<FJsonObject>();
			Failure->SetBoolField(TEXT("success"), false);
			Failure->SetStringField(TEXT("code"), TEXT("TARGET_REQUIRED"));
			Failure->SetStringField(TEXT("message"), TEXT("Target pin must be specified when break_all is false"));
			Failure->SetNumberField(TEXT("index"), Index);
			Failure->SetObjectField(TEXT("request"), RequestObj);
			Failures.Add(MakeShared<FJsonValueObject>(Failure));
			++Index;
			continue;
		}

		// Handle case where pin has no links
		if (Pin->LinkedTo.Num() == 0)
		{
			TSharedPtr<FJsonObject> Success = MakeShared<FJsonObject>();
			Success->SetBoolField(TEXT("success"), true);
			Success->SetNumberField(TEXT("index"), Index);
			Success->SetStringField(TEXT("pin_name"), PinPinName);
			Success->SetArrayField(TEXT("broken_links"), {});
			Successes.Add(MakeShared<FJsonValueObject>(Success));
			++Index;
			continue;
		}

		// Create transaction and modify
		FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "DisconnectPins", "MCP Disconnect Pins"));
		if (Graph)
		{
			Graph->Modify();
		}
		if (Node)
		{
			Node->Modify();
		}
		Pin->Modify();

		// Summarize links before breaking
		TArray<TSharedPtr<FJsonValue>> BrokenLinks = SummarizeLinks(Pin, TEXT("pin"));
		
		// Break all links
		Pin->BreakAllPinLinks();

		ModifiedGraphs.Add(Graph);
		bBlueprintModified = true;

		// Build success result
		TSharedPtr<FJsonObject> Success = MakeShared<FJsonObject>();
		Success->SetBoolField(TEXT("success"), true);
		Success->SetNumberField(TEXT("index"), Index);
		Success->SetStringField(TEXT("pin_name"), PinPinName);
		Success->SetArrayField(TEXT("broken_links"), BrokenLinks);
		Successes.Add(MakeShared<FJsonValueObject>(Success));
		++Index;
	}

	// Notify graphs of changes
	for (UEdGraph* Graph : ModifiedGraphs)
	{
		if (Graph)
		{
			Graph->NotifyGraphChanged();
		}
	}

	// Mark Blueprint as modified
	if (bBlueprintModified)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}

	// Build final result
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), Failures.Num() == 0);
	Result->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
	Result->SetNumberField(TEXT("attempted"), Requests.Num());
	Result->SetNumberField(TEXT("succeeded"), Successes.Num());
	Result->SetNumberField(TEXT("failed"), Failures.Num());
	Result->SetArrayField(TEXT("operations"), Successes);
	if (Failures.Num() > 0)
	{
		Result->SetArrayField(TEXT("failures"), Failures);
	}

	// Add modified graphs info
	if (ModifiedGraphs.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> GraphArray;
		for (UEdGraph* Graph : ModifiedGraphs)
		{
			if (!Graph)
			{
				continue;
			}
			TSharedPtr<FJsonObject> GraphInfo = MakeShared<FJsonObject>();
			GraphInfo->SetStringField(TEXT("graph_name"), Graph->GetName());
			GraphInfo->SetStringField(TEXT("graph_guid"), Graph->GraphGuid.ToString(EGuidFormats::DigitsWithHyphensInBraces));
			GraphArray.Add(MakeShared<FJsonValueObject>(GraphInfo));
		}
		Result->SetArrayField(TEXT("modified_graphs"), GraphArray);
	}

	return TResult<TSharedPtr<FJsonObject>>::Success(Result);
}

TResult<TSharedPtr<FJsonObject>> FBlueprintNodeService::GetAvailableNodes(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params)
{
	// Validate Blueprint
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	if (!Params.IsValid())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Invalid parameters object")
		);
	}

	// Extract parameters - accept multiple parameter name variations
	FString SearchTerm;
	if (!Params->TryGetStringField(TEXT("search_term"), SearchTerm))
	{
		if (!Params->TryGetStringField(TEXT("searchTerm"), SearchTerm))
		{
			if (!Params->TryGetStringField(TEXT("search_text"), SearchTerm))
			{
				Params->TryGetStringField(TEXT("searchterm"), SearchTerm);
			}
		}
	}
	SearchTerm.TrimStartAndEndInline();

	FString Category;
	Params->TryGetStringField(TEXT("category"), Category);
	Category.TrimStartAndEndInline();
	
	// Validate category format - catch when AI passes function names as categories
	if (!IsValidCategoryFormat(Category))
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("INVALID CATEGORY: '%s' appears to be a function name, not a category. "
			                     "Categories are high-level groupings like 'Math', 'Math|Float', 'Flow Control', 'Utilities'. "
			                     "DO NOT pass function names (like 'KismetMathLibrary::Multiply_FloatFloat') as category. "
			                     "Valid categories include: %s. "
			                     "To find a specific function, use search_term parameter instead."), 
			                *Category, *GetCommonCategoryNames())
		);
	}

	// Extract filter options
	bool bIncludeFunctions = true;
	bool bIncludeVariables = true;
	bool bIncludeEvents = true;
	Params->TryGetBoolField(TEXT("include_functions"), bIncludeFunctions);
	Params->TryGetBoolField(TEXT("includeFunctions"), bIncludeFunctions);
	Params->TryGetBoolField(TEXT("include_variables"), bIncludeVariables);;
	Params->TryGetBoolField(TEXT("includeVariables"), bIncludeVariables);
	Params->TryGetBoolField(TEXT("include_events"), bIncludeEvents);
	Params->TryGetBoolField(TEXT("includeEvents"), bIncludeEvents);

	// Default max_results reduced to 10 to avoid token blowout
	int32 MaxResults = 10;
	double ParsedMaxResults = 0.0;
	if (Params->TryGetNumberField(TEXT("max_results"), ParsedMaxResults) ||
		Params->TryGetNumberField(TEXT("maxResults"), ParsedMaxResults))
	{
		MaxResults = FMath::Max(1, static_cast<int32>(ParsedMaxResults));
	}
	
	// Compact mode (default: true) returns minimal fields to reduce token usage
	bool bCompact = true;
	Params->TryGetBoolField(TEXT("compact"), bCompact);

	// Use descriptor-based discovery (modern approach)
	TArray<FBlueprintReflection::FNodeSpawnerDescriptor> Descriptors = 
		FBlueprintReflection::DiscoverNodesWithDescriptors(Blueprint, SearchTerm, Category, TEXT(""), MaxResults);

	// Convert descriptors to category-based format with type filtering
	TMap<FString, TArray<TSharedPtr<FJsonValue>>> CategoryMap;
	int32 TotalNodes = 0;

	for (const FBlueprintReflection::FNodeSpawnerDescriptor& Desc : Descriptors)
	{
		// Skip invalid descriptors (empty spawner_key or display_name)
		if (Desc.SpawnerKey.IsEmpty() || Desc.DisplayName.IsEmpty())
		{
			continue;
		}
		
		// Apply type filters
		if (!bIncludeFunctions && Desc.NodeType == TEXT("function_call"))
		{
			continue;
		}
		if (!bIncludeVariables && (Desc.NodeType == TEXT("variable_get") || Desc.NodeType == TEXT("variable_set")))
		{
			continue;
		}
		if (!bIncludeEvents && Desc.NodeType == TEXT("event"))
		{
			continue;
		}

		// Convert descriptor to JSON (compact or full)
		TSharedPtr<FJsonObject> DescriptorJson = bCompact ? Desc.ToJsonCompact() : Desc.ToJson();

		// Organize by category
		FString DescCategory = Desc.Category;
		if (DescCategory.IsEmpty())
		{
			DescCategory = TEXT("Other");
		}

		if (!CategoryMap.Contains(DescCategory))
		{
			CategoryMap.Add(DescCategory, TArray<TSharedPtr<FJsonValue>>());
		}

		CategoryMap[DescCategory].Add(MakeShared<FJsonValueObject>(DescriptorJson));
		TotalNodes++;
	}

	// Build result structure
	TSharedPtr<FJsonObject> Categories = MakeShared<FJsonObject>();
	for (auto& CategoryPair : CategoryMap)
	{
		Categories->SetArrayField(CategoryPair.Key, CategoryPair.Value);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetObjectField(TEXT("categories"), Categories);
	Result->SetNumberField(TEXT("total_nodes"), TotalNodes);
	Result->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
	Result->SetBoolField(TEXT("truncated"), false);
	Result->SetBoolField(TEXT("success"), true);
	Result->SetBoolField(TEXT("with_descriptors"), true);  // Descriptor mode
	
	// Add hints to guide AI - ALWAYS check for suggestions even when results are found
	// This is important because searches like "multiply" return Power/MultiplyByPi but NOT basic multiplication
	FString Suggestion = GetSearchSuggestion(SearchTerm);
	if (!Suggestion.IsEmpty())
	{
		Result->SetStringField(TEXT("hint"), Suggestion);
	}
	else if (TotalNodes == 0 && !SearchTerm.IsEmpty())
	{
		Result->SetStringField(TEXT("hint"), TEXT("No nodes found. Try broader search terms or remove category filter."));
	}
	else if (SearchTerm.IsEmpty() && Category.IsEmpty())
	{
		// Return ERROR for empty search to stop AI loops
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("REQUIRED: search_term parameter must not be empty. Use specific terms like 'multiply', 'print', 'add', 'get health', 'set variable'. Without search_term, results are random and useless. DO NOT retry with empty search_term.")
		);
	}

	return TResult<TSharedPtr<FJsonObject>>::Success(Result);
}

TResult<TSharedPtr<FJsonObject>> FBlueprintNodeService::DiscoverNodesWithDescriptors(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Params)
{
	// Validate Blueprint
	if (auto ValidationResult = ValidateNotNull(Blueprint, TEXT("Blueprint")); ValidationResult.IsError())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(ValidationResult.GetErrorCode(), ValidationResult.GetErrorMessage());
	}

	if (!Params.IsValid())
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("Invalid parameters object")
		);
	}

	// Extract parameters - accept multiple parameter name variations
	FString SearchTerm;
	if (!Params->TryGetStringField(TEXT("search_term"), SearchTerm))
	{
		Params->TryGetStringField(TEXT("search_text"), SearchTerm);
	}

	FString CategoryFilter;
	Params->TryGetStringField(TEXT("category_filter"), CategoryFilter);
	
	// Validate category format - catch when AI passes function names as categories
	if (!IsValidCategoryFormat(CategoryFilter))
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			FString::Printf(TEXT("INVALID CATEGORY FILTER: '%s' appears to be a function name, not a category. "
			                     "Categories are high-level groupings like 'Math', 'Math|Float', 'Flow Control', 'Utilities'. "
			                     "DO NOT pass function names (like 'KismetMathLibrary::Multiply_FloatFloat') as category_filter. "
			                     "Valid categories include: %s. "
			                     "To find a specific function, use search_term parameter instead."), 
			                *CategoryFilter, *GetCommonCategoryNames())
		);
	}

	FString ClassFilter;
	Params->TryGetStringField(TEXT("class_filter"), ClassFilter);

	// Default max_results reduced to 10 to avoid token blowout
	int32 MaxResults = 10;
	double ParsedMaxResults = 0.0;
	if (Params->TryGetNumberField(TEXT("max_results"), ParsedMaxResults))
	{
		MaxResults = FMath::Max(1, static_cast<int32>(ParsedMaxResults));
	}
	
	// Compact mode (default: true) returns minimal fields to reduce token usage
	bool bCompact = true;
	Params->TryGetBoolField(TEXT("compact"), bCompact);

	// Call descriptor-based discovery system
	TArray<FBlueprintReflection::FNodeSpawnerDescriptor> Descriptors = 
		FBlueprintReflection::DiscoverNodesWithDescriptors(Blueprint, SearchTerm, CategoryFilter, ClassFilter, MaxResults);

	// Convert descriptors to JSON (compact or full), filtering out invalid entries
	TArray<TSharedPtr<FJsonValue>> DescriptorJsonArray;
	for (const FBlueprintReflection::FNodeSpawnerDescriptor& Desc : Descriptors)
	{
		// Skip invalid descriptors (empty spawner_key or display_name)
		if (Desc.SpawnerKey.IsEmpty() || Desc.DisplayName.IsEmpty())
		{
			continue;
		}
		TSharedPtr<FJsonObject> DescriptorJson = bCompact ? Desc.ToJsonCompact() : Desc.ToJson();
		DescriptorJsonArray.Add(MakeShared<FJsonValueObject>(DescriptorJson));
	}

	// Build result
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("descriptors"), DescriptorJsonArray);
	Result->SetNumberField(TEXT("count"), DescriptorJsonArray.Num());
	Result->SetStringField(TEXT("blueprint_name"), Blueprint->GetName());
	
	// Add hints to guide AI - ALWAYS check for suggestions even when results are found
	// This is important because searches like "multiply" return Power/MultiplyByPi but NOT basic multiplication
	FString Suggestion = GetSearchSuggestion(SearchTerm);
	if (!Suggestion.IsEmpty())
	{
		Result->SetStringField(TEXT("hint"), Suggestion);
	}
	else if (DescriptorJsonArray.Num() == 0 && !SearchTerm.IsEmpty())
	{
		Result->SetStringField(TEXT("hint"), TEXT("No nodes found. Try broader search terms or check category_filter."));
	}
	else if (SearchTerm.IsEmpty() && CategoryFilter.IsEmpty())
	{
		// Return ERROR for empty search to stop AI loops
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::PARAM_INVALID,
			TEXT("REQUIRED: search_term parameter must not be empty. Use specific terms like 'multiply', 'print', 'add', 'get health', 'set variable'. Without search_term, results are random and useless. DO NOT retry with empty search_term.")
		);
	}

	return TResult<TSharedPtr<FJsonObject>>::Success(Result);
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

TResult<TSharedPtr<FJsonObject>> FBlueprintNodeService::ResetPinDefaultsAdvanced(
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
	
	// Find the node
	TArray<UEdGraph*> CandidateGraphs;
	GatherCandidateGraphs(Blueprint, nullptr, CandidateGraphs);
	
	UEdGraphNode* Node = FindNodeByGuid(CandidateGraphs, NodeId);
	if (!Node)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::NODE_NOT_FOUND,
			FString::Printf(TEXT("Node not found: %s"), *NodeId)
		);
	}
	
	UEdGraph* Graph = Node->GetGraph();
	if (!Graph)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::GRAPH_NOT_FOUND,
			TEXT("Node has no parent graph")
		);
	}
	
	const UEdGraphSchema* GraphSchema = Graph->GetSchema();
	UEdGraphSchema_K2* K2Schema = GraphSchema ? const_cast<UEdGraphSchema_K2*>(Cast<UEdGraphSchema_K2>(GraphSchema)) : nullptr;
	if (!K2Schema)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::INVALID_PARAMETER,
			TEXT("Graph schema does not support K2 pin defaults")
		);
	}
	
	// Helper to collect pin names from various field names
	auto CollectPinNames = [](const TSharedPtr<FJsonObject>& Source, TArray<FString>& OutPins)
	{
		if (!Source.IsValid())
		{
			return;
		}
		
		const TArray<FString> PinFields = {
			TEXT("pin"), TEXT("pin_name"), TEXT("pin_names"), TEXT("pins"), 
			TEXT("pin_display_name"), TEXT("pin_identifier"), TEXT("pin_identifiers"), TEXT("pin_ids")
		};
		
		for (const FString& Field : PinFields)
		{
			// Try as single string
			FString SingleValue;
			if (Source->TryGetStringField(Field, SingleValue))
			{
				OutPins.AddUnique(SingleValue);
			}
			
			// Try as array
			const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
			if (Source->TryGetArrayField(Field, ArrayValue) && ArrayValue)
			{
				for (const TSharedPtr<FJsonValue>& Value : *ArrayValue)
				{
					if (Value.IsValid() && Value->Type == EJson::String)
					{
						OutPins.AddUnique(Value->AsString());
					}
				}
			}
		}
	};
	
	// Helper to evaluate reset_all flag
	auto EvaluateResetAll = [](const TSharedPtr<FJsonObject>& Source) -> bool
	{
		if (!Source.IsValid())
		{
			return false;
		}
		
		const TArray<FString> Fields = {TEXT("reset_all"), TEXT("all_pins"), TEXT("all"), TEXT("reset_defaults")};
		for (const FString& Field : Fields)
		{
			bool BoolValue = false;
			if (Source->TryGetBoolField(Field, BoolValue) && BoolValue)
			{
				return true;
			}
			
			FString StringValue;
			if (Source->TryGetStringField(Field, StringValue))
			{
				StringValue.TrimStartAndEndInline();
				StringValue.ToLowerInline();
				if (StringValue == TEXT("true") || StringValue == TEXT("1") || 
					StringValue == TEXT("all") || StringValue == TEXT("yes"))
				{
					return true;
				}
			}
		}
		
		return false;
	};
	
	// Collect pin names from params and extra/node_config objects
	TArray<FString> PinNames;
	CollectPinNames(Params, PinNames);
	
	const TSharedPtr<FJsonObject>* Extra = nullptr;
	if (Params->TryGetObjectField(TEXT("extra"), Extra) && Extra)
	{
		CollectPinNames(*Extra, PinNames);
	}
	
	const TSharedPtr<FJsonObject>* NodeConfig = nullptr;
	if (Params->TryGetObjectField(TEXT("node_config"), NodeConfig) && NodeConfig)
	{
		CollectPinNames(*NodeConfig, PinNames);
	}
	
	// Check for reset_all flag
	const bool bResetAllPins = EvaluateResetAll(Params) || 
		EvaluateResetAll(Extra ? *Extra : nullptr) || 
		EvaluateResetAll(NodeConfig ? *NodeConfig : nullptr);
	
	if (bResetAllPins)
	{
		PinNames.Empty();
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin)
			{
				PinNames.Add(Pin->PinName.ToString());
			}
		}
	}
	
	// Clean up pin names
	PinNames.RemoveAll([](FString& Name)
	{
		Name.TrimStartAndEndInline();
		return Name.IsEmpty();
	});
	
	if (PinNames.Num() == 0)
	{
		return TResult<TSharedPtr<FJsonObject>>::Error(
			VibeUE::ErrorCodes::INVALID_PARAMETER,
			TEXT("No pin names provided for reset")
		);
	}
	
	// Evaluate compile preference
	auto EvaluateCompilePreference = [](const TSharedPtr<FJsonObject>& Source, bool& bHasValue, bool& bValue)
	{
		if (!Source.IsValid())
		{
			return;
		}
		
		bool CompileFlag = false;
		if (Source->TryGetBoolField(TEXT("compile"), CompileFlag))
		{
			bHasValue = true;
			bValue = CompileFlag;
		}
		
		bool SkipFlag = false;
		if (Source->TryGetBoolField(TEXT("skip_compile"), SkipFlag) && SkipFlag)
		{
			bHasValue = true;
			bValue = false;
		}
	};
	
	bool bCompileExplicit = false;
	bool bCompileValue = false;
	EvaluateCompilePreference(Params, bCompileExplicit, bCompileValue);
	EvaluateCompilePreference(Extra ? *Extra : nullptr, bCompileExplicit, bCompileValue);
	EvaluateCompilePreference(NodeConfig ? *NodeConfig : nullptr, bCompileExplicit, bCompileValue);
	const bool bShouldCompile = bCompileExplicit ? bCompileValue : false;
	
	// Process pins
	TUniquePtr<FScopedTransaction> Transaction;
	TSet<FString> SeenPins;
	TArray<TSharedPtr<FJsonValue>> PinReports;
	int32 FailureCount = 0;
	int32 ChangedCount = 0;
	int32 NoOpCount = 0;
	
	auto EnsureTransaction = [&]()
	{
		if (!Transaction.IsValid())
		{
			Transaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("VibeUE", "ResetPinDefaultsTransaction", "Reset Blueprint Pin Defaults"));
			Graph->Modify();
			Blueprint->Modify();
			Node->Modify();
		}
	};
	
	for (const FString& RawName : PinNames)
	{
		FString PinName = RawName;
		PinName.TrimStartAndEndInline();
		if (PinName.IsEmpty() || SeenPins.Contains(PinName))
		{
			continue;
		}
		SeenPins.Add(PinName);
		
		TSharedPtr<FJsonObject> PinReport = MakeShared<FJsonObject>();
		PinReport->SetStringField(TEXT("pin_name"), PinName);
		
		// Find pin
		UEdGraphPin* Pin = nullptr;
		for (UEdGraphPin* NodePin : Node->Pins)
		{
			if (NodePin && NodePin->PinName.ToString() == PinName)
			{
				Pin = NodePin;
				break;
			}
		}
		
		if (!Pin)
		{
			++FailureCount;
			PinReport->SetStringField(TEXT("status"), TEXT("failed"));
			PinReport->SetStringField(TEXT("message"), TEXT("Pin not found"));
			PinReports.Add(MakeShared<FJsonValueObject>(PinReport));
			continue;
		}
		
		PinReport->SetStringField(TEXT("original_value"), Pin->GetDefaultAsString());
		PinReport->SetStringField(TEXT("autogenerated_value"), Pin->AutogeneratedDefaultValue);
		PinReport->SetBoolField(TEXT("has_connections"), Pin->LinkedTo.Num() > 0);
		
#if WITH_EDITORONLY_DATA
		if (Pin->bDefaultValueIsIgnored)
		{
			PinReport->SetStringField(TEXT("status"), TEXT("ignored"));
			PinReport->SetStringField(TEXT("message"), TEXT("Pin default value is ignored by schema"));
			PinReports.Add(MakeShared<FJsonValueObject>(PinReport));
			continue;
		}
#endif
		
		if (Pin->DoesDefaultValueMatchAutogenerated())
		{
			++NoOpCount;
			PinReport->SetStringField(TEXT("status"), TEXT("noop"));
			PinReport->SetStringField(TEXT("message"), TEXT("Pin already matches autogenerated default"));
			PinReport->SetStringField(TEXT("new_value"), Pin->GetDefaultAsString());
			PinReports.Add(MakeShared<FJsonValueObject>(PinReport));
			continue;
		}
		
		EnsureTransaction();
		Pin->Modify();
		K2Schema->ResetPinToAutogeneratedDefaultValue(Pin);
		++ChangedCount;
		PinReport->SetStringField(TEXT("status"), TEXT("applied"));
		PinReport->SetStringField(TEXT("message"), TEXT("Pin default reset to autogenerated value"));
		PinReport->SetStringField(TEXT("new_value"), Pin->GetDefaultAsString());
		PinReports.Add(MakeShared<FJsonValueObject>(PinReport));
	}
	
	// Mark modified and optionally compile
	if (ChangedCount > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		if (bShouldCompile)
		{
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
		}
	}
	
	// Build result
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	const bool bSuccess = (FailureCount == 0);
	Result->SetBoolField(TEXT("success"), bSuccess);
	Result->SetStringField(TEXT("node_id"), NodeId);
	Result->SetBoolField(TEXT("reset_all"), bResetAllPins);
	Result->SetNumberField(TEXT("requested_count"), PinNames.Num());
	Result->SetNumberField(TEXT("changed_count"), ChangedCount);
	Result->SetNumberField(TEXT("failure_count"), FailureCount);
	Result->SetNumberField(TEXT("noop_count"), NoOpCount);
	Result->SetBoolField(TEXT("compiled"), bShouldCompile && ChangedCount > 0);
	Result->SetArrayField(TEXT("pins"), PinReports);
	
	if (Graph)
	{
		Result->SetStringField(TEXT("graph_name"), Graph->GetName());
	}
	
	FString Message;
	if (FailureCount > 0)
	{
		Message = TEXT("Some pins could not be reset to defaults");
	}
	else if (ChangedCount == 0)
	{
		Message = TEXT("All pins already matched their autogenerated defaults");
	}
	else
	{
		Message = FString::Printf(TEXT("Reset %d pin%s to autogenerated defaults"), 
			ChangedCount, ChangedCount == 1 ? TEXT("") : TEXT("s"));
	}
	
	Result->SetStringField(TEXT("message"), Message);
	
	return TResult<TSharedPtr<FJsonObject>>::Success(Result);
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
