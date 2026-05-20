// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UAnimGraphService.h"

#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_BlendListByInt.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_TwoBoneIK.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateNode.h"
#include "AnimStateNodeBase.h"
#include "AnimStateConduitNode.h"
#include "AnimStateEntryNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationStateMachineSchema.h"
#include "AnimationStateGraph.h"
#include "AnimationStateGraphSchema.h"
#include "AnimationTransitionGraph.h"
#include "AnimationTransitionSchema.h"
#include "AnimationGraph.h"
#include "AnimGraphNode_Base.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "IAnimationBlueprintEditor.h"
#include "IAnimationBlueprintEditorModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "GraphEditorActions.h"
#include "K2Node_VariableGet.h"
#include "K2Node_CallFunction.h"
#include "EdGraphSchema_K2.h"
#include "Kismet/KismetMathLibrary.h"
#include "AlphaBlend.h"
#include "ScopedTransaction.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

UAnimBlueprint* UAnimGraphService::LoadAnimBlueprint(const FString& AnimBlueprintPath)
{
	if (AnimBlueprintPath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimGraphService::LoadAnimBlueprint: Path is empty"));
		return nullptr;
	}

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(AnimBlueprintPath);
	if (!LoadedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimGraphService::LoadAnimBlueprint: Failed to load: %s"), *AnimBlueprintPath);
		return nullptr;
	}

	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(LoadedObject);
	if (!AnimBlueprint)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimGraphService::LoadAnimBlueprint: Not an AnimBlueprint: %s"), *AnimBlueprintPath);
		return nullptr;
	}

	return AnimBlueprint;
}

UEdGraph* UAnimGraphService::FindAnimGraph(UAnimBlueprint* AnimBlueprint, const FString& GraphName)
{
	if (!AnimBlueprint)
	{
		return nullptr;
	}

	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
		{
			return Graph;
		}
	}

	return nullptr;
}

UAnimGraphNode_StateMachine* UAnimGraphService::FindStateMachineNode(UAnimBlueprint* AnimBlueprint, const FString& MachineName)
{
	if (!AnimBlueprint)
	{
		return nullptr;
	}

	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachine* StateMachineNode = Cast<UAnimGraphNode_StateMachine>(Node);
			if (StateMachineNode)
			{
				FString NodeTitle = StateMachineNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
				if (NodeTitle.Equals(MachineName, ESearchCase::IgnoreCase) ||
					StateMachineNode->EditorStateMachineGraph->GetName().Equals(MachineName, ESearchCase::IgnoreCase))
				{
					return StateMachineNode;
				}
			}
		}
	}

	return nullptr;
}

UAnimStateNodeBase* UAnimGraphService::FindStateNode(UEdGraph* StateMachineGraph, const FString& StateName)
{
	if (!StateMachineGraph)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : StateMachineGraph->Nodes)
	{
		UAnimStateNodeBase* StateNode = Cast<UAnimStateNodeBase>(Node);
		if (StateNode)
		{
			FString NodeTitle = StateNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
			if (NodeTitle.Equals(StateName, ESearchCase::IgnoreCase) ||
				StateNode->GetStateName().Equals(StateName, ESearchCase::IgnoreCase))
			{
				return StateNode;
			}
		}
	}

	return nullptr;
}

UAnimStateTransitionNode* UAnimGraphService::FindTransitionNode(UEdGraph* StateMachineGraph, const FString& SourceState, const FString& DestState)
{
	if (!StateMachineGraph)
	{
		return nullptr;
	}

	for (UEdGraphNode* Node : StateMachineGraph->Nodes)
	{
		UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(Node);
		if (TransitionNode)
		{
			UAnimStateNodeBase* PrevState = TransitionNode->GetPreviousState();
			UAnimStateNodeBase* NextState = TransitionNode->GetNextState();

			if (PrevState && NextState)
			{
				FString PrevName = PrevState->GetStateName();
				FString NextName = NextState->GetStateName();

				if (PrevName.Equals(SourceState, ESearchCase::IgnoreCase) &&
					NextName.Equals(DestState, ESearchCase::IgnoreCase))
				{
					return TransitionNode;
				}
			}
		}
	}

	return nullptr;
}

IAnimationBlueprintEditor* UAnimGraphService::GetAnimBlueprintEditor(UAnimBlueprint* AnimBlueprint)
{
	if (!AnimBlueprint || !GEditor)
	{
		return nullptr;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return nullptr;
	}

	// Try to get existing editor first
	IAssetEditorInstance* ExistingEditor = AssetEditorSubsystem->FindEditorForAsset(AnimBlueprint, false);
	if (!ExistingEditor)
	{
		// Open the editor
		if (!AssetEditorSubsystem->OpenEditorForAsset(AnimBlueprint))
		{
			UE_LOG(LogTemp, Error, TEXT("UAnimGraphService: Failed to open editor for AnimBlueprint"));
			return nullptr;
		}
		ExistingEditor = AssetEditorSubsystem->FindEditorForAsset(AnimBlueprint, false);
	}

	if (!ExistingEditor)
	{
		return nullptr;
	}

	// Cast to the animation blueprint editor interface
	return static_cast<IAnimationBlueprintEditor*>(ExistingEditor);
}

// ============================================================================
// GRAPH NAVIGATION
// ============================================================================

bool UAnimGraphService::OpenAnimGraph(const FString& AnimBlueprintPath, const FString& GraphName)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	UEdGraph* Graph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!Graph)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimGraphService::OpenAnimGraph: Graph '%s' not found"), *GraphName);
		return false;
	}

	IAnimationBlueprintEditor* Editor = GetAnimBlueprintEditor(AnimBlueprint);
	if (!Editor)
	{
		return false;
	}

	// Focus on the graph
	Editor->SetCurrentMode(FName("AnimationBlueprintEditorMode"));

	// Use FocusedGraphEditorChanged or OpenDocument to navigate to the specific graph
	FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Graph, false);

	UE_LOG(LogTemp, Log, TEXT("UAnimGraphService::OpenAnimGraph: Opened graph '%s'"), *GraphName);
	return true;
}

bool UAnimGraphService::OpenAnimState(const FString& AnimBlueprintPath, const FString& StateMachineName, const FString& StateName)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	// Find the state machine
	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimGraphService::OpenAnimState: State machine '%s' not found"), *StateMachineName);
		return false;
	}

	UEdGraph* StateMachineGraph = StateMachineNode->EditorStateMachineGraph;
	if (!StateMachineGraph)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimGraphService::OpenAnimState: Could not get state machine graph"));
		return false;
	}

	// Find the state
	UAnimStateNodeBase* StateNode = FindStateNode(StateMachineGraph, StateName);
	if (!StateNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimGraphService::OpenAnimState: State '%s' not found in '%s'"), *StateName, *StateMachineName);
		return false;
	}

	IAnimationBlueprintEditor* Editor = GetAnimBlueprintEditor(AnimBlueprint);
	if (!Editor)
	{
		return false;
	}

	// Focus on the state node - this should open its internal graph
	FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(StateNode, false);

	// If the state has a bound graph (internal logic), try to open it
	UAnimStateNode* AnimState = Cast<UAnimStateNode>(StateNode);
	if (AnimState && AnimState->BoundGraph)
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(AnimState->BoundGraph, false);
	}

	UE_LOG(LogTemp, Log, TEXT("UAnimGraphService::OpenAnimState: Opened state '%s' in '%s'"), *StateName, *StateMachineName);
	return true;
}

bool UAnimGraphService::OpenTransition(const FString& AnimBlueprintPath, const FString& StateMachineName, const FString& SourceStateName, const FString& DestStateName)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimGraphService::OpenTransition: State machine '%s' not found"), *StateMachineName);
		return false;
	}

	UEdGraph* StateMachineGraph = StateMachineNode->EditorStateMachineGraph;
	if (!StateMachineGraph)
	{
		return false;
	}

	UAnimStateTransitionNode* TransitionNode = FindTransitionNode(StateMachineGraph, SourceStateName, DestStateName);
	if (!TransitionNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimGraphService::OpenTransition: Transition from '%s' to '%s' not found"), *SourceStateName, *DestStateName);
		return false;
	}

	IAnimationBlueprintEditor* Editor = GetAnimBlueprintEditor(AnimBlueprint);
	if (!Editor)
	{
		return false;
	}

	// Focus on the transition node
	FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(TransitionNode, false);

	// Open the transition's bound graph (the transition rule)
	if (TransitionNode->BoundGraph)
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(TransitionNode->BoundGraph, false);
	}

	UE_LOG(LogTemp, Log, TEXT("UAnimGraphService::OpenTransition: Opened transition '%s' -> '%s'"), *SourceStateName, *DestStateName);
	return true;
}

bool UAnimGraphService::FocusNode(const FString& AnimBlueprintPath, const FString& NodeId)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	FGuid SearchGuid;
	if (!FGuid::Parse(NodeId, SearchGuid))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimGraphService::FocusNode: Invalid GUID format: %s"), *NodeId);
		return false;
	}

	// Search all graphs for the node
	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == SearchGuid)
			{
				IAnimationBlueprintEditor* Editor = GetAnimBlueprintEditor(AnimBlueprint);
				if (!Editor)
				{
					return false;
				}

				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node, false);
				UE_LOG(LogTemp, Log, TEXT("UAnimGraphService::FocusNode: Focused on node '%s'"), *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
				return true;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("UAnimGraphService::FocusNode: Node with GUID '%s' not found"), *NodeId);
	return false;
}

// ============================================================================
// GRAPH INTROSPECTION
// ============================================================================

TArray<FAnimGraphInfo> UAnimGraphService::ListGraphs(const FString& AnimBlueprintPath)
{
	TArray<FAnimGraphInfo> GraphInfos;

	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return GraphInfos;
	}

	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}

		FAnimGraphInfo Info;
		Info.GraphName = Graph->GetName();
		Info.NodeCount = Graph->Nodes.Num();

		// Determine graph type
		if (Cast<UAnimationGraph>(Graph))
		{
			Info.GraphType = TEXT("AnimGraph");
		}
		else if (Cast<UAnimationStateMachineGraph>(Graph))
		{
			Info.GraphType = TEXT("StateMachine");
		}
		else if (Graph->GetName().Contains(TEXT("EventGraph")))
		{
			Info.GraphType = TEXT("EventGraph");
		}
		else
		{
			// Check if this is a state's bound graph
			bool bIsStateGraph = false;
			for (UEdGraph* OuterGraph : Graphs)
			{
				if (!OuterGraph)
				{
					continue;
				}

				for (UEdGraphNode* Node : OuterGraph->Nodes)
				{
					UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node);
					if (StateNode && StateNode->BoundGraph == Graph)
					{
						Info.GraphType = TEXT("State");
						Info.ParentGraphName = OuterGraph->GetName();
						bIsStateGraph = true;
						break;
					}

					UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(Node);
					if (TransNode && TransNode->BoundGraph == Graph)
					{
						Info.GraphType = TEXT("Transition");
						Info.ParentGraphName = OuterGraph->GetName();
						bIsStateGraph = true;
						break;
					}
				}

				if (bIsStateGraph)
				{
					break;
				}
			}

			if (!bIsStateGraph)
			{
				Info.GraphType = TEXT("Other");
			}
		}

		GraphInfos.Add(Info);
	}

	return GraphInfos;
}

TArray<FAnimStateMachineInfo> UAnimGraphService::ListStateMachines(const FString& AnimBlueprintPath)
{
	TArray<FAnimStateMachineInfo> Machines;

	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return Machines;
	}

	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_StateMachine* StateMachineNode = Cast<UAnimGraphNode_StateMachine>(Node);
			if (StateMachineNode)
			{
				FAnimStateMachineInfo Info;
				Info.MachineName = StateMachineNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
				Info.NodeId = StateMachineNode->NodeGuid.ToString();
				Info.ParentGraphName = Graph->GetName();

				// Count states
				UEdGraph* SMGraph = StateMachineNode->EditorStateMachineGraph;
				if (SMGraph)
				{
					int32 StateCount = 0;
					for (UEdGraphNode* SMNode : SMGraph->Nodes)
					{
						if (Cast<UAnimStateNodeBase>(SMNode) && !Cast<UAnimStateEntryNode>(SMNode))
						{
							StateCount++;
						}
					}
					Info.StateCount = StateCount;
				}

				Machines.Add(Info);
			}
		}
	}

	return Machines;
}

TArray<FAnimStateInfo> UAnimGraphService::ListStatesInMachine(const FString& AnimBlueprintPath, const FString& StateMachineName)
{
	TArray<FAnimStateInfo> States;

	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return States;
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAnimGraphService::ListStatesInMachine: State machine '%s' not found"), *StateMachineName);
		return States;
	}

	UEdGraph* StateMachineGraph = StateMachineNode->EditorStateMachineGraph;
	if (!StateMachineGraph)
	{
		return States;
	}

	for (UEdGraphNode* Node : StateMachineGraph->Nodes)
	{
		// Skip entry nodes (they don't inherit from UAnimStateNodeBase)
		if (Cast<UAnimStateEntryNode>(Node))
		{
			continue;
		}

		UAnimStateNodeBase* StateNode = Cast<UAnimStateNodeBase>(Node);
		if (!StateNode)
		{
			continue;
		}

		FAnimStateInfo Info;
		Info.StateName = StateNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		Info.NodeId = StateNode->NodeGuid.ToString();
		Info.PosX = StateNode->NodePosX;
		Info.PosY = StateNode->NodePosY;

		// Determine state type
		if (Cast<UAnimStateConduitNode>(StateNode))
		{
			Info.StateType = TEXT("Conduit");
		}
		else if (Cast<UAnimStateNode>(StateNode))
		{
			Info.StateType = TEXT("State");
		}
		else
		{
			Info.StateType = StateNode->GetClass()->GetName();
		}

		// Check if this is an end state (no outgoing transitions)
		bool bHasOutgoingTransitions = false;
		for (UEdGraphNode* OtherNode : StateMachineGraph->Nodes)
		{
			UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(OtherNode);
			if (TransNode && TransNode->GetPreviousState() == StateNode)
			{
				bHasOutgoingTransitions = true;
				break;
			}
		}
		Info.bIsEndState = !bHasOutgoingTransitions;

		States.Add(Info);
	}

	return States;
}

TArray<FAnimTransitionInfo> UAnimGraphService::GetStateTransitions(const FString& AnimBlueprintPath, const FString& StateMachineName, const FString& StateName)
{
	TArray<FAnimTransitionInfo> Transitions;

	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return Transitions;
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode)
	{
		return Transitions;
	}

	UEdGraph* StateMachineGraph = StateMachineNode->EditorStateMachineGraph;
	if (!StateMachineGraph)
	{
		return Transitions;
	}

	for (UEdGraphNode* Node : StateMachineGraph->Nodes)
	{
		UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(Node);
		if (!TransitionNode)
		{
			continue;
		}

		UAnimStateNodeBase* PrevState = TransitionNode->GetPreviousState();
		UAnimStateNodeBase* NextState = TransitionNode->GetNextState();

		if (!PrevState || !NextState)
		{
			continue;
		}

		// Filter by state name if provided
		if (!StateName.IsEmpty())
		{
			FString PrevName = PrevState->GetStateName();
			FString NextName = NextState->GetStateName();

			if (!PrevName.Equals(StateName, ESearchCase::IgnoreCase) &&
				!NextName.Equals(StateName, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		FAnimTransitionInfo Info;
		Info.TransitionName = TransitionNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		Info.NodeId = TransitionNode->NodeGuid.ToString();
		Info.SourceState = PrevState->GetStateName();
		Info.DestState = NextState->GetStateName();
		Info.Priority = TransitionNode->PriorityOrder;
		Info.BlendDuration = TransitionNode->CrossfadeDuration;
		Info.bIsAutomatic = TransitionNode->bAutomaticRuleBasedOnSequencePlayerInState;
		DescribeTransitionRule(TransitionNode, Info);

		Transitions.Add(Info);
	}

	return Transitions;
}

bool UAnimGraphService::GetStateMachineInfo(const FString& AnimBlueprintPath, const FString& StateMachineName, FAnimStateMachineInfo& OutInfo)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode)
	{
		return false;
	}

	OutInfo.MachineName = StateMachineNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	OutInfo.NodeId = StateMachineNode->NodeGuid.ToString();

	// Find parent graph
	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->Nodes.Contains(StateMachineNode))
		{
			OutInfo.ParentGraphName = Graph->GetName();
			break;
		}
	}

	// Count states
	UEdGraph* SMGraph = StateMachineNode->EditorStateMachineGraph;
	if (SMGraph)
	{
		int32 StateCount = 0;
		for (UEdGraphNode* Node : SMGraph->Nodes)
		{
			if (Cast<UAnimStateNodeBase>(Node) && !Cast<UAnimStateEntryNode>(Node))
			{
				StateCount++;
			}
		}
		OutInfo.StateCount = StateCount;
	}

	return true;
}

bool UAnimGraphService::GetStateInfo(const FString& AnimBlueprintPath, const FString& StateMachineName, const FString& StateName, FAnimStateInfo& OutInfo)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode)
	{
		return false;
	}

	UEdGraph* StateMachineGraph = StateMachineNode->EditorStateMachineGraph;
	if (!StateMachineGraph)
	{
		return false;
	}

	UAnimStateNodeBase* StateNode = FindStateNode(StateMachineGraph, StateName);
	if (!StateNode)
	{
		return false;
	}

	OutInfo.StateName = StateNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
	OutInfo.NodeId = StateNode->NodeGuid.ToString();
	OutInfo.PosX = StateNode->NodePosX;
	OutInfo.PosY = StateNode->NodePosY;

	if (Cast<UAnimStateConduitNode>(StateNode))
	{
		OutInfo.StateType = TEXT("Conduit");
	}
	else if (Cast<UAnimStateNode>(StateNode))
	{
		OutInfo.StateType = TEXT("State");
	}
	else
	{
		OutInfo.StateType = StateNode->GetClass()->GetName();
	}

	// Check for outgoing transitions
	bool bHasOutgoing = false;
	for (UEdGraphNode* Node : StateMachineGraph->Nodes)
	{
		UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(Node);
		if (TransNode && TransNode->GetPreviousState() == StateNode)
		{
			bHasOutgoing = true;
			break;
		}
	}
	OutInfo.bIsEndState = !bHasOutgoing;

	return true;
}

// ============================================================================
// ANIMATION ASSET ANALYSIS
// ============================================================================

TArray<FAnimSequenceUsageInfo> UAnimGraphService::GetUsedAnimSequences(const FString& AnimBlueprintPath)
{
	TArray<FAnimSequenceUsageInfo> Sequences;

	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return Sequences;
	}

	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			// Check for sequence player nodes
			UAnimGraphNode_SequencePlayer* SeqPlayer = Cast<UAnimGraphNode_SequencePlayer>(Node);
			if (SeqPlayer)
			{
				// Get the sequence from the node
				UAnimSequenceBase* Sequence = SeqPlayer->Node.GetSequence();
				if (Sequence)
				{
					FAnimSequenceUsageInfo Info;
					Info.SequencePath = Sequence->GetPathName();
					Info.SequenceName = Sequence->GetName();
					Info.UsedInGraph = Graph->GetName();
					Info.UsedByNode = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();

					// Avoid duplicates
					bool bAlreadyAdded = false;
					for (const FAnimSequenceUsageInfo& Existing : Sequences)
					{
						if (Existing.SequencePath == Info.SequencePath && Existing.UsedInGraph == Info.UsedInGraph)
						{
							bAlreadyAdded = true;
							break;
						}
					}

					if (!bAlreadyAdded)
					{
						Sequences.Add(Info);
					}
				}
			}
		}
	}

	return Sequences;
}

FString UAnimGraphService::GetSkeleton(const FString& AnimBlueprintPath)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	if (AnimBlueprint->TargetSkeleton)
	{
		return AnimBlueprint->TargetSkeleton->GetPathName();
	}

	return FString();
}

FString UAnimGraphService::GetPreviewMesh(const FString& AnimBlueprintPath)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	USkeletalMesh* PreviewMesh = AnimBlueprint->GetPreviewMesh();
	if (PreviewMesh)
	{
		return PreviewMesh->GetPathName();
	}

	return FString();
}

// ============================================================================
// UTILITY
// ============================================================================

bool UAnimGraphService::IsAnimBlueprint(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return false;
	}

	UObject* LoadedObject = UEditorAssetLibrary::LoadAsset(AssetPath);
	return Cast<UAnimBlueprint>(LoadedObject) != nullptr;
}

FString UAnimGraphService::GetParentClass(const FString& AnimBlueprintPath)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	if (AnimBlueprint->ParentClass)
	{
		return AnimBlueprint->ParentClass->GetName();
	}

	return FString();
}

// ============================================================================
// ANIMGRAPH NODE CREATION
// ============================================================================

FString UAnimGraphService::AddStateMachine(const FString& AnimBlueprintPath, const FString& MachineName, float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddStateMachine: Failed to load AnimBlueprint"));
		return FString();
	}

	// Find the main AnimGraph
	UAnimationGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : AnimBlueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
		{
			AnimGraph = Cast<UAnimationGraph>(Graph);
			break;
		}
	}

	if (!AnimGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddStateMachine: AnimGraph not found"));
		return FString();
	}

	// Create state machine node - DO NOT set EditorStateMachineGraph before Finalize()
	// PostPlacedNewNode() will create it automatically and asserts that it's null
	FGraphNodeCreator<UAnimGraphNode_StateMachine> NodeCreator(*AnimGraph);
	UAnimGraphNode_StateMachine* NewNode = NodeCreator.CreateNode();
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddStateMachine: Failed to create state machine node"));
		return FString();
	}
	
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;

	// Finalize will call PostPlacedNewNode() which creates EditorStateMachineGraph,
	// entry node, and sets up the schema correctly
	NodeCreator.Finalize();

	// Now rename the graph to the desired name
	if (NewNode->EditorStateMachineGraph)
	{
		TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(NewNode);
		FBlueprintEditorUtils::RenameGraphWithSuggestion(NewNode->EditorStateMachineGraph, NameValidator, MachineName);
	}

	// Mark dirty and compile
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddStateMachine: Created '%s' at (%f, %f)"), *MachineName, PosX, PosY);
	return NewNode->NodeGuid.ToString();
}

FString UAnimGraphService::AddSequencePlayer(const FString& AnimBlueprintPath, const FString& GraphName, 
	const FString& AnimSequencePath, float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddSequencePlayer: Graph '%s' not found"), *GraphName);
		return FString();
	}

	// Create sequence player node
	FGraphNodeCreator<UAnimGraphNode_SequencePlayer> NodeCreator(*TargetGraph);
	UAnimGraphNode_SequencePlayer* NewNode = NodeCreator.CreateNode();
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddSequencePlayer: Failed to create node"));
		return FString();
	}
	
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;

	// Set animation sequence if provided
	if (!AnimSequencePath.IsEmpty())
	{
		UAnimSequence* Sequence = Cast<UAnimSequence>(UEditorAssetLibrary::LoadAsset(AnimSequencePath));
		if (Sequence)
		{
			NewNode->Node.SetSequence(Sequence);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AddSequencePlayer: Could not load sequence '%s'"), *AnimSequencePath);
		}
	}

	NodeCreator.Finalize();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddSequencePlayer: Created in '%s'"), *GraphName);
	return NewNode->NodeGuid.ToString();
}

FString UAnimGraphService::AddBlendSpacePlayer(const FString& AnimBlueprintPath, const FString& GraphName, 
	const FString& BlendSpacePath, float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddBlendSpacePlayer: Graph '%s' not found"), *GraphName);
		return FString();
	}

	// Create blend space player node
	FGraphNodeCreator<UAnimGraphNode_BlendSpacePlayer> NodeCreator(*TargetGraph);
	UAnimGraphNode_BlendSpacePlayer* NewNode = NodeCreator.CreateNode();
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddBlendSpacePlayer: Failed to create node"));
		return FString();
	}
	
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;

	// Set blend space if provided
	if (!BlendSpacePath.IsEmpty())
	{
		UBlendSpace* BlendSpace = Cast<UBlendSpace>(UEditorAssetLibrary::LoadAsset(BlendSpacePath));
		if (BlendSpace)
		{
			NewNode->Node.SetBlendSpace(BlendSpace);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AddBlendSpacePlayer: Could not load blend space '%s'"), *BlendSpacePath);
		}
	}

	NodeCreator.Finalize();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddBlendSpacePlayer: Created in '%s'"), *GraphName);
	return NewNode->NodeGuid.ToString();
}

FString UAnimGraphService::AddBlendByBool(const FString& AnimBlueprintPath, const FString& GraphName, float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddBlendByBool: Graph '%s' not found"), *GraphName);
		return FString();
	}

	FGraphNodeCreator<UAnimGraphNode_BlendListByBool> NodeCreator(*TargetGraph);
	UAnimGraphNode_BlendListByBool* NewNode = NodeCreator.CreateNode();
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddBlendByBool: Failed to create node"));
		return FString();
	}
	
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;
	NodeCreator.Finalize();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddBlendByBool: Created in '%s'"), *GraphName);
	return NewNode->NodeGuid.ToString();
}

FString UAnimGraphService::AddBlendByInt(const FString& AnimBlueprintPath, const FString& GraphName, int32 NumPoses, float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddBlendByInt: Graph '%s' not found"), *GraphName);
		return FString();
	}

	FGraphNodeCreator<UAnimGraphNode_BlendListByInt> NodeCreator(*TargetGraph);
	UAnimGraphNode_BlendListByInt* NewNode = NodeCreator.CreateNode();
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddBlendByInt: Failed to create node"));
		return FString();
	}
	
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;
	NodeCreator.Finalize();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddBlendByInt: Created in '%s' with %d poses"), *GraphName, NumPoses);
	return NewNode->NodeGuid.ToString();
}

FString UAnimGraphService::AddLayeredBlend(const FString& AnimBlueprintPath, const FString& GraphName, float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddLayeredBlend: Graph '%s' not found"), *GraphName);
		return FString();
	}

	FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> NodeCreator(*TargetGraph);
	UAnimGraphNode_LayeredBoneBlend* NewNode = NodeCreator.CreateNode();
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddLayeredBlend: Failed to create node"));
		return FString();
	}
	
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;
	NodeCreator.Finalize();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddLayeredBlend: Created in '%s'"), *GraphName);
	return NewNode->NodeGuid.ToString();
}

FString UAnimGraphService::AddSlotNode(const FString& AnimBlueprintPath, const FString& GraphName, 
	const FString& SlotName, float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddSlotNode: Graph '%s' not found"), *GraphName);
		return FString();
	}

	FGraphNodeCreator<UAnimGraphNode_Slot> NodeCreator(*TargetGraph);
	UAnimGraphNode_Slot* NewNode = NodeCreator.CreateNode();
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddSlotNode: Failed to create node"));
		return FString();
	}
	
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;
	NewNode->Node.SlotName = FName(*SlotName);
	NodeCreator.Finalize();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddSlotNode: Created '%s' in '%s'"), *SlotName, *GraphName);
	return NewNode->NodeGuid.ToString();
}

FString UAnimGraphService::AddSaveCachedPose(const FString& AnimBlueprintPath, const FString& GraphName, 
	const FString& CacheName, float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddSaveCachedPose: Graph '%s' not found"), *GraphName);
		return FString();
	}

	FGraphNodeCreator<UAnimGraphNode_SaveCachedPose> NodeCreator(*TargetGraph);
	UAnimGraphNode_SaveCachedPose* NewNode = NodeCreator.CreateNode();
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddSaveCachedPose: Failed to create node"));
		return FString();
	}
	
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;
	NewNode->CacheName = CacheName;
	NodeCreator.Finalize();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddSaveCachedPose: Created '%s' in '%s'"), *CacheName, *GraphName);
	return NewNode->NodeGuid.ToString();
}

FString UAnimGraphService::AddUseCachedPose(const FString& AnimBlueprintPath, const FString& GraphName, 
	const FString& CacheName, float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddUseCachedPose: Graph '%s' not found"), *GraphName);
		return FString();
	}

	// First, find the corresponding SaveCachedPose node
	UAnimGraphNode_SaveCachedPose* SaveNode = nullptr;
	TArray<UEdGraph*> AllGraphs;
	AnimBlueprint->GetAllGraphs(AllGraphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (!Graph) continue;

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimGraphNode_SaveCachedPose* SaveCached = Cast<UAnimGraphNode_SaveCachedPose>(Node);
			if (SaveCached && SaveCached->CacheName == CacheName)
			{
				SaveNode = SaveCached;
				break;
			}
		}

		if (SaveNode) break;
	}

	if (!SaveNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddUseCachedPose: SaveCachedPose with name '%s' not found. Creating node anyway."), *CacheName);
	}

	FGraphNodeCreator<UAnimGraphNode_UseCachedPose> NodeCreator(*TargetGraph);
	UAnimGraphNode_UseCachedPose* NewNode = NodeCreator.CreateNode();
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddUseCachedPose: Failed to create node"));
		return FString();
	}
	
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;
	NewNode->SaveCachedPoseNode = SaveNode;
	NodeCreator.Finalize();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddUseCachedPose: Created reference to '%s' in '%s'"), *CacheName, *GraphName);
	return NewNode->NodeGuid.ToString();
}

// ============================================================================
// STATE MACHINE MUTATIONS
// ============================================================================

FString UAnimGraphService::AddState(const FString& AnimBlueprintPath, const FString& StateMachineName, 
	const FString& StateName, float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddState: Failed to load AnimBlueprint"));
		return FString();
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddState: State machine node '%s' not found"), *StateMachineName);
		return FString();
	}
	
	if (!StateMachineNode->EditorStateMachineGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddState: State machine '%s' has no graph (may not be fully initialized)"), *StateMachineName);
		return FString();
	}

	UAnimationStateMachineGraph* SMGraph = StateMachineNode->EditorStateMachineGraph;

	// Create state node - DO NOT set BoundGraph before Finalize()
	// PostPlacedNewNode() will create it automatically and asserts that it's null
	FGraphNodeCreator<UAnimStateNode> NodeCreator(*SMGraph);
	UAnimStateNode* NewState = NodeCreator.CreateNode();
	if (!NewState)
	{
		UE_LOG(LogTemp, Error, TEXT("AddState: Failed to create state node for '%s'"), *StateName);
		return FString();
	}
	
	NewState->NodePosX = PosX;
	NewState->NodePosY = PosY;

	// Finalize will call PostPlacedNewNode() which creates BoundGraph and sets up schema
	NodeCreator.Finalize();

	// Now rename the graph to the desired name
	if (NewState->BoundGraph)
	{
		TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(NewState);
		FBlueprintEditorUtils::RenameGraphWithSuggestion(NewState->BoundGraph, NameValidator, StateName);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddState: Created '%s' in '%s'"), *StateName, *StateMachineName);
	return NewState->NodeGuid.ToString();
}

FString UAnimGraphService::AddConduit(const FString& AnimBlueprintPath, const FString& StateMachineName, 
	const FString& ConduitName, float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddConduit: Failed to load AnimBlueprint"));
		return FString();
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddConduit: State machine node '%s' not found"), *StateMachineName);
		return FString();
	}
	
	if (!StateMachineNode->EditorStateMachineGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddConduit: State machine '%s' has no graph"), *StateMachineName);
		return FString();
	}

	UAnimationStateMachineGraph* SMGraph = StateMachineNode->EditorStateMachineGraph;

	// Create conduit node - DO NOT set BoundGraph before Finalize()
	// PostPlacedNewNode() will create it automatically and asserts that it's null
	FGraphNodeCreator<UAnimStateConduitNode> NodeCreator(*SMGraph);
	UAnimStateConduitNode* NewConduit = NodeCreator.CreateNode();
	if (!NewConduit)
	{
		UE_LOG(LogTemp, Error, TEXT("AddConduit: Failed to create conduit node for '%s'"), *ConduitName);
		return FString();
	}
	
	NewConduit->NodePosX = PosX;
	NewConduit->NodePosY = PosY;

	// Finalize will call PostPlacedNewNode() which creates BoundGraph
	NodeCreator.Finalize();

	// Now rename the graph to the desired name
	if (NewConduit->BoundGraph)
	{
		TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(NewConduit);
		FBlueprintEditorUtils::RenameGraphWithSuggestion(NewConduit->BoundGraph, NameValidator, ConduitName);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddConduit: Created '%s' in '%s'"), *ConduitName, *StateMachineName);
	return NewConduit->NodeGuid.ToString();
}

FString UAnimGraphService::AddTransition(const FString& AnimBlueprintPath, const FString& StateMachineName, 
	const FString& SourceStateName, const FString& DestStateName, float BlendDuration)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTransition: Failed to load AnimBlueprint"));
		return FString();
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTransition: State machine node '%s' not found"), *StateMachineName);
		return FString();
	}
	
	if (!StateMachineNode->EditorStateMachineGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTransition: State machine '%s' has no graph"), *StateMachineName);
		return FString();
	}

	UAnimationStateMachineGraph* SMGraph = StateMachineNode->EditorStateMachineGraph;

	// Find source and destination states
	UAnimStateNodeBase* SourceState = FindStateNode(SMGraph, SourceStateName);
	UAnimStateNodeBase* DestState = FindStateNode(SMGraph, DestStateName);

	if (!SourceState)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTransition: Source state '%s' not found"), *SourceStateName);
		return FString();
	}

	if (!DestState)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTransition: Dest state '%s' not found"), *DestStateName);
		return FString();
	}

	// Create transition node - DO NOT set BoundGraph before Finalize()
	// PostPlacedNewNode() will create it via CreateBoundGraph()
	FGraphNodeCreator<UAnimStateTransitionNode> NodeCreator(*SMGraph);
	UAnimStateTransitionNode* Transition = NodeCreator.CreateNode();
	if (!Transition)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTransition: Failed to create transition node"));
		return FString();
	}

	// Position between states
	Transition->NodePosX = (SourceState->NodePosX + DestState->NodePosX) / 2;
	Transition->NodePosY = (SourceState->NodePosY + DestState->NodePosY) / 2;
	Transition->CrossfadeDuration = BlendDuration;

	// Finalize will call PostPlacedNewNode() which creates BoundGraph
	NodeCreator.Finalize();

	// Connect source -> transition -> dest via pins
	UEdGraphPin* SourceOutPin = SourceState->GetOutputPin();
	UEdGraphPin* TransInPin = Transition->GetInputPin();
	UEdGraphPin* TransOutPin = Transition->GetOutputPin();
	UEdGraphPin* DestInPin = DestState->GetInputPin();

	if (SourceOutPin && TransInPin && TransOutPin && DestInPin)
	{
		SourceOutPin->MakeLinkTo(TransInPin);
		TransOutPin->MakeLinkTo(DestInPin);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AddTransition: Failed to connect pins (source_out=%d, trans_in=%d, trans_out=%d, dest_in=%d)"),
			SourceOutPin != nullptr, TransInPin != nullptr, TransOutPin != nullptr, DestInPin != nullptr);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddTransition: Created '%s' -> '%s' with blend %.2fs"), 
		*SourceStateName, *DestStateName, BlendDuration);
	return Transition->NodeGuid.ToString();
}

bool UAnimGraphService::RemoveState(const FString& AnimBlueprintPath, const FString& StateMachineName, 
	const FString& StateName, bool bRemoveTransitions)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode || !StateMachineNode->EditorStateMachineGraph)
	{
		return false;
	}

	UEdGraph* SMGraph = StateMachineNode->EditorStateMachineGraph;
	UAnimStateNodeBase* StateNode = FindStateNode(SMGraph, StateName);

	if (!StateNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveState: State '%s' not found"), *StateName);
		return false;
	}

	// Remove transitions if requested
	if (bRemoveTransitions)
	{
		TArray<UAnimStateTransitionNode*> TransitionsToRemove;
		for (UEdGraphNode* Node : SMGraph->Nodes)
		{
			UAnimStateTransitionNode* TransNode = Cast<UAnimStateTransitionNode>(Node);
			if (TransNode)
			{
				if (TransNode->GetPreviousState() == StateNode || TransNode->GetNextState() == StateNode)
				{
					TransitionsToRemove.Add(TransNode);
				}
			}
		}

		for (UAnimStateTransitionNode* TransNode : TransitionsToRemove)
		{
			SMGraph->RemoveNode(TransNode);
		}
	}

	// Remove the state node
	SMGraph->RemoveNode(StateNode);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("RemoveState: Removed '%s' from '%s'"), *StateName, *StateMachineName);
	return true;
}

bool UAnimGraphService::RemoveTransition(const FString& AnimBlueprintPath, const FString& StateMachineName, 
	const FString& SourceStateName, const FString& DestStateName)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode || !StateMachineNode->EditorStateMachineGraph)
	{
		return false;
	}

	UEdGraph* SMGraph = StateMachineNode->EditorStateMachineGraph;
	UAnimStateTransitionNode* TransitionNode = FindTransitionNode(SMGraph, SourceStateName, DestStateName);

	if (!TransitionNode)
	{
		UE_LOG(LogTemp, Warning, TEXT("RemoveTransition: Transition '%s' -> '%s' not found"), 
			*SourceStateName, *DestStateName);
		return false;
	}

	SMGraph->RemoveNode(TransitionNode);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("RemoveTransition: Removed '%s' -> '%s'"), *SourceStateName, *DestStateName);
	return true;
}

// ============================================================================
// ANIMGRAPH CONNECTIONS
// ============================================================================

bool UAnimGraphService::ConnectAnimNodes(const FString& AnimBlueprintPath, const FString& GraphName, 
	const FString& SourceNodeId, const FString& SourcePinName, const FString& TargetNodeId, const FString& TargetPinName)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectAnimNodes: Graph '%s' not found"), *GraphName);
		return false;
	}

	// Parse GUIDs
	FGuid SourceGuid, TargetGuid;
	if (!FGuid::Parse(SourceNodeId, SourceGuid))
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectAnimNodes: Invalid source GUID"));
		return false;
	}

	if (!FGuid::Parse(TargetNodeId, TargetGuid))
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectAnimNodes: Invalid target GUID"));
		return false;
	}

	// Find nodes
	UEdGraphNode* SourceNode = nullptr;
	UEdGraphNode* TargetNode = nullptr;

	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node)
		{
			if (Node->NodeGuid == SourceGuid) SourceNode = Node;
			if (Node->NodeGuid == TargetGuid) TargetNode = Node;
		}
	}

	if (!SourceNode || !TargetNode)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectAnimNodes: Nodes not found"));
		return false;
	}

	// Find pins
	UEdGraphPin* SourcePin = nullptr;
	UEdGraphPin* TargetPin = nullptr;

	for (UEdGraphPin* Pin : SourceNode->Pins)
	{
		if (Pin && Pin->PinName.ToString() == SourcePinName && Pin->Direction == EGPD_Output)
		{
			SourcePin = Pin;
			break;
		}
	}

	for (UEdGraphPin* Pin : TargetNode->Pins)
	{
		if (Pin && Pin->PinName.ToString() == TargetPinName && Pin->Direction == EGPD_Input)
		{
			TargetPin = Pin;
			break;
		}
	}

	if (!SourcePin || !TargetPin)
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectAnimNodes: Pins not found"));
		return false;
	}

	// Make connection
	SourcePin->MakeLinkTo(TargetPin);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("ConnectAnimNodes: Connected nodes in '%s'"), *GraphName);
	return true;
}

bool UAnimGraphService::ConnectToOutputPose(const FString& AnimBlueprintPath, const FString& GraphName, 
	const FString& SourceNodeId, const FString& SourcePinName)
{
	FString OutputNodeId = GetOutputPoseNodeId(AnimBlueprintPath, GraphName);
	if (OutputNodeId.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("ConnectToOutputPose: Could not find output pose node"));
		return false;
	}

	return ConnectAnimNodes(AnimBlueprintPath, GraphName, SourceNodeId, SourcePinName, OutputNodeId, TEXT("Result"));
}

bool UAnimGraphService::DisconnectAnimNode(const FString& AnimBlueprintPath, const FString& GraphName, 
	const FString& NodeId, const FString& PinName)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		return false;
	}

	FGuid NodeGuid;
	if (!FGuid::Parse(NodeId, NodeGuid))
	{
		return false;
	}

	// Find node
	UEdGraphNode* Node = nullptr;
	for (UEdGraphNode* GraphNode : TargetGraph->Nodes)
	{
		if (GraphNode && GraphNode->NodeGuid == NodeGuid)
		{
			Node = GraphNode;
			break;
		}
	}

	if (!Node)
	{
		return false;
	}

	// Find and disconnect pin
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString() == PinName)
		{
			Pin->BreakAllPinLinks();
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
			return true;
		}
	}

	return false;
}

FString UAnimGraphService::GetOutputPoseNodeId(const FString& AnimBlueprintPath, const FString& GraphName)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		return FString();
	}

	// Look for output pose nodes (Root node in AnimGraph, StateResult in state graphs)
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Cast<UAnimGraphNode_Root>(Node) || Cast<UAnimGraphNode_StateResult>(Node))
		{
			return Node->NodeGuid.ToString();
		}
	}

	return FString();
}

// ============================================================================
// ANIMATION ASSET ASSIGNMENT
// ============================================================================

bool UAnimGraphService::SetSequencePlayerAsset(const FString& AnimBlueprintPath, const FString& GraphName, 
	const FString& NodeId, const FString& AnimSequencePath)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		return false;
	}

	FGuid NodeGuid;
	if (!FGuid::Parse(NodeId, NodeGuid))
	{
		return false;
	}

	// Find node
	UAnimGraphNode_SequencePlayer* SeqPlayer = nullptr;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node && Node->NodeGuid == NodeGuid)
		{
			SeqPlayer = Cast<UAnimGraphNode_SequencePlayer>(Node);
			break;
		}
	}

	if (!SeqPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("SetSequencePlayerAsset: Node is not a sequence player"));
		return false;
	}

	// Load sequence
	UAnimSequence* Sequence = Cast<UAnimSequence>(UEditorAssetLibrary::LoadAsset(AnimSequencePath));
	if (!Sequence)
	{
		UE_LOG(LogTemp, Error, TEXT("SetSequencePlayerAsset: Could not load sequence '%s'"), *AnimSequencePath);
		return false;
	}

	SeqPlayer->Node.SetSequence(Sequence);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("SetSequencePlayerAsset: Set sequence to '%s'"), *AnimSequencePath);
	return true;
}

bool UAnimGraphService::SetBlendSpaceAsset(const FString& AnimBlueprintPath, const FString& GraphName, 
	const FString& NodeId, const FString& BlendSpacePath)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		return false;
	}

	FGuid NodeGuid;
	if (!FGuid::Parse(NodeId, NodeGuid))
	{
		return false;
	}

	// Find node
	UAnimGraphNode_BlendSpacePlayer* BSPlayer = nullptr;
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node && Node->NodeGuid == NodeGuid)
		{
			BSPlayer = Cast<UAnimGraphNode_BlendSpacePlayer>(Node);
			break;
		}
	}

	if (!BSPlayer)
	{
		UE_LOG(LogTemp, Error, TEXT("SetBlendSpaceAsset: Node is not a blend space player"));
		return false;
	}

	// Load blend space
	UBlendSpace* BlendSpace = Cast<UBlendSpace>(UEditorAssetLibrary::LoadAsset(BlendSpacePath));
	if (!BlendSpace)
	{
		UE_LOG(LogTemp, Error, TEXT("SetBlendSpaceAsset: Could not load blend space '%s'"), *BlendSpacePath);
		return false;
	}

	BSPlayer->Node.SetBlendSpace(BlendSpace);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("SetBlendSpaceAsset: Set blend space to '%s'"), *BlendSpacePath);
	return true;
}

FString UAnimGraphService::GetNodeAnimationAsset(const FString& AnimBlueprintPath, const FString& GraphName, 
	const FString& NodeId)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		return FString();
	}

	FGuid NodeGuid;
	if (!FGuid::Parse(NodeId, NodeGuid))
	{
		return FString();
	}

	// Find node
	for (UEdGraphNode* Node : TargetGraph->Nodes)
	{
		if (Node && Node->NodeGuid == NodeGuid)
		{
			// Try sequence player
			if (UAnimGraphNode_SequencePlayer* SeqPlayer = Cast<UAnimGraphNode_SequencePlayer>(Node))
			{
				UAnimSequenceBase* Sequence = SeqPlayer->Node.GetSequence();
				if (Sequence)
				{
					return Sequence->GetPathName();
				}
			}

			// Try blend space player
			if (UAnimGraphNode_BlendSpacePlayer* BSPlayer = Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
			{
				UBlendSpace* BlendSpace = BSPlayer->Node.GetBlendSpace();
				if (BlendSpace)
				{
					return BlendSpace->GetPathName();
				}
			}

			break;
		}
	}

	return FString();
}

// ============================================================================
// ADVANCED ANIMATION NODES
// ============================================================================

FString UAnimGraphService::AddTwoBoneIKNode(const FString& AnimBlueprintPath, const FString& GraphName, 
	float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTwoBoneIKNode: Graph '%s' not found"), *GraphName);
		return FString();
	}

	FGraphNodeCreator<UAnimGraphNode_TwoBoneIK> NodeCreator(*TargetGraph);
	UAnimGraphNode_TwoBoneIK* NewNode = NodeCreator.CreateNode();
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddTwoBoneIKNode: Failed to create node"));
		return FString();
	}
	
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;
	NodeCreator.Finalize();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddTwoBoneIKNode: Created in '%s'"), *GraphName);
	return NewNode->NodeGuid.ToString();
}

FString UAnimGraphService::AddModifyBoneNode(const FString& AnimBlueprintPath, const FString& GraphName, 
	const FString& BoneName, float PosX, float PosY)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UEdGraph* TargetGraph = FindAnimGraph(AnimBlueprint, GraphName);
	if (!TargetGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("AddModifyBoneNode: Graph '%s' not found"), *GraphName);
		return FString();
	}

	FGraphNodeCreator<UAnimGraphNode_ModifyBone> NodeCreator(*TargetGraph);
	UAnimGraphNode_ModifyBone* NewNode = NodeCreator.CreateNode();
	if (!NewNode)
	{
		UE_LOG(LogTemp, Error, TEXT("AddModifyBoneNode: Failed to create node"));
		return FString();
	}
	
	NewNode->NodePosX = PosX;
	NewNode->NodePosY = PosY;

	if (!BoneName.IsEmpty())
	{
		NewNode->Node.BoneToModify.BoneName = FName(*BoneName);
	}

	NodeCreator.Finalize();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);

	UE_LOG(LogTemp, Log, TEXT("AddModifyBoneNode: Created in '%s' for bone '%s'"), *GraphName, *BoneName);
	return NewNode->NodeGuid.ToString();
}

// ============================================================================
// FILE-LOCAL HELPERS (transition rule authoring)
// ============================================================================

namespace
{
	/** Get the single value output pin of a pure variable-get node. */
	UEdGraphPin* GetVariableValuePin(UK2Node_VariableGet* VarNode)
	{
		if (!VarNode)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : VarNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output && Pin->PinName != UEdGraphSchema_K2::PN_Self)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	/** Map a comparison keyword to a KismetMathLibrary function name. Empty string = unknown op. */
	FString ComparisonToFunctionName(const FString& Op)
	{
		const FString S = Op.ToLower();
		if (S == TEXT("greater") || S == TEXT(">")) return TEXT("Greater_DoubleDouble");
		if (S == TEXT("less") || S == TEXT("<")) return TEXT("Less_DoubleDouble");
		if (S == TEXT("greater_equal") || S == TEXT(">=") || S == TEXT("gequal")) return TEXT("GreaterEqual_DoubleDouble");
		if (S == TEXT("less_equal") || S == TEXT("<=") || S == TEXT("lequal")) return TEXT("LessEqual_DoubleDouble");
		if (S == TEXT("equal") || S == TEXT("==")) return TEXT("EqualEqual_DoubleDouble");
		if (S == TEXT("not_equal") || S == TEXT("!=")) return TEXT("NotEqual_DoubleDouble");
		return FString();
	}

	/** Map a KismetMathLibrary comparison function name back to an operator symbol (for introspection). */
	FString FunctionNameToSymbol(const FString& FnName)
	{
		if (FnName == TEXT("Greater_DoubleDouble")) return TEXT(">");
		if (FnName == TEXT("Less_DoubleDouble")) return TEXT("<");
		if (FnName == TEXT("GreaterEqual_DoubleDouble")) return TEXT(">=");
		if (FnName == TEXT("LessEqual_DoubleDouble")) return TEXT("<=");
		if (FnName == TEXT("EqualEqual_DoubleDouble")) return TEXT("==");
		if (FnName == TEXT("NotEqual_DoubleDouble")) return TEXT("!=");
		return FnName;
	}

	/** Parse an alpha-blend keyword into EAlphaBlendOption (defaults to Linear). */
	EAlphaBlendOption ParseBlendOption(const FString& In)
	{
		const FString S = In.ToLower();
		if (S == TEXT("cubic")) return EAlphaBlendOption::Cubic;
		if (S == TEXT("hermitecubic") || S == TEXT("hermite")) return EAlphaBlendOption::HermiteCubic;
		if (S == TEXT("sinusoidal") || S == TEXT("sin")) return EAlphaBlendOption::Sinusoidal;
		if (S == TEXT("quadraticinout") || S == TEXT("quadratic")) return EAlphaBlendOption::QuadraticInOut;
		if (S == TEXT("cubicinout")) return EAlphaBlendOption::CubicInOut;
		if (S == TEXT("expinout") || S == TEXT("exp")) return EAlphaBlendOption::ExpInOut;
		if (S == TEXT("circularin")) return EAlphaBlendOption::CircularIn;
		if (S == TEXT("circularout")) return EAlphaBlendOption::CircularOut;
		if (S == TEXT("circularinout")) return EAlphaBlendOption::CircularInOut;
		return EAlphaBlendOption::Linear;
	}
}

// ============================================================================
// PRIVATE HELPERS (state machine authoring)
// ============================================================================

UAnimStateTransitionNode* UAnimGraphService::ResolveTransition(const FString& AnimBlueprintPath, const FString& StateMachineName,
	const FString& SourceStateName, const FString& DestStateName, UAnimBlueprint*& OutBlueprint)
{
	OutBlueprint = nullptr;
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return nullptr;
	}
	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode || !StateMachineNode->EditorStateMachineGraph)
	{
		return nullptr;
	}
	UAnimStateTransitionNode* Transition = FindTransitionNode(StateMachineNode->EditorStateMachineGraph, SourceStateName, DestStateName);
	if (!Transition)
	{
		return nullptr;
	}
	OutBlueprint = AnimBlueprint;
	return Transition;
}

UAnimGraphNode_TransitionResult* UAnimGraphService::GetTransitionResultNode(UAnimStateTransitionNode* Transition)
{
	if (!Transition)
	{
		return nullptr;
	}
	UAnimationTransitionGraph* TransitionGraph = Cast<UAnimationTransitionGraph>(Transition->BoundGraph);
	if (!TransitionGraph)
	{
		return nullptr;
	}
	return TransitionGraph->GetResultNode();
}

void UAnimGraphService::ResetTransitionRule(UAnimStateTransitionNode* Transition)
{
	if (!Transition)
	{
		return;
	}

	Transition->bAutomaticRuleBasedOnSequencePlayerInState = false;

	UAnimationTransitionGraph* TransitionGraph = Cast<UAnimationTransitionGraph>(Transition->BoundGraph);
	if (!TransitionGraph)
	{
		return;
	}

	UAnimGraphNode_TransitionResult* ResultNode = TransitionGraph->GetResultNode();
	if (ResultNode)
	{
		if (UEdGraphPin* CanEnterPin = ResultNode->FindPin(TEXT("bCanEnterTransition"), EGPD_Input))
		{
			CanEnterPin->BreakAllPinLinks();
			CanEnterPin->DefaultValue = TEXT("false");
		}
	}

	// Remove every rule-logic node, leaving only the result node intact.
	TArray<UEdGraphNode*> NodesToRemove;
	for (UEdGraphNode* Node : TransitionGraph->Nodes)
	{
		if (Node && Node != ResultNode)
		{
			NodesToRemove.Add(Node);
		}
	}
	for (UEdGraphNode* Node : NodesToRemove)
	{
		TransitionGraph->RemoveNode(Node);
	}
}

UAnimGraphNode_Base* UAnimGraphService::FindStateAssetPlayer(UAnimStateNodeBase* StateNode)
{
	if (!StateNode)
	{
		return nullptr;
	}
	UEdGraph* StateGraph = StateNode->GetBoundGraph();
	if (!StateGraph)
	{
		return nullptr;
	}
	for (UEdGraphNode* Node : StateGraph->Nodes)
	{
		if (UAnimGraphNode_SequencePlayer* SeqPlayer = Cast<UAnimGraphNode_SequencePlayer>(Node))
		{
			return SeqPlayer;
		}
	}
	for (UEdGraphNode* Node : StateGraph->Nodes)
	{
		if (UAnimGraphNode_BlendSpacePlayer* BSPlayer = Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
		{
			return BSPlayer;
		}
	}
	return nullptr;
}

void UAnimGraphService::DescribeTransitionRule(UAnimStateTransitionNode* Transition, FAnimTransitionInfo& OutInfo)
{
	OutInfo.RuleType = TEXT("None");
	OutInfo.RuleVariable = TEXT("");
	OutInfo.RuleSummary = TEXT("inert (never fires)");
	OutInfo.bHasRule = false;

	if (!Transition)
	{
		return;
	}

	if (Transition->bAutomaticRuleBasedOnSequencePlayerInState)
	{
		OutInfo.RuleType = TEXT("Automatic");
		OutInfo.RuleSummary = TEXT("auto (asset player time remaining)");
		OutInfo.bHasRule = true;
		return;
	}

	UAnimGraphNode_TransitionResult* ResultNode = GetTransitionResultNode(Transition);
	if (!ResultNode)
	{
		return;
	}
	UEdGraphPin* CanEnterPin = ResultNode->FindPin(TEXT("bCanEnterTransition"), EGPD_Input);
	if (!CanEnterPin)
	{
		return;
	}

	if (CanEnterPin->LinkedTo.Num() == 0)
	{
		if (CanEnterPin->DefaultValue.ToLower() == TEXT("true"))
		{
			OutInfo.RuleType = TEXT("Custom");
			OutInfo.RuleSummary = TEXT("always true");
			OutInfo.bHasRule = true;
		}
		return;
	}

	UEdGraphPin* SourcePin = CanEnterPin->LinkedTo[0];
	UEdGraphNode* SourceNode = SourcePin ? SourcePin->GetOwningNodeUnchecked() : nullptr;

	if (UK2Node_VariableGet* VarGet = Cast<UK2Node_VariableGet>(SourceNode))
	{
		OutInfo.RuleType = TEXT("Bool");
		OutInfo.RuleVariable = VarGet->GetVarName().ToString();
		OutInfo.RuleSummary = FString::Printf(TEXT("%s == true"), *OutInfo.RuleVariable);
		OutInfo.bHasRule = true;
		return;
	}

	if (UK2Node_CallFunction* CallFn = Cast<UK2Node_CallFunction>(SourceNode))
	{
		const FString FnName = CallFn->FunctionReference.GetMemberName().ToString();

		if (FnName == TEXT("Not_PreBool"))
		{
			FString VarName;
			if (UEdGraphPin* APin = CallFn->FindPin(TEXT("A"), EGPD_Input))
			{
				if (APin->LinkedTo.Num() > 0)
				{
					if (UK2Node_VariableGet* VG = Cast<UK2Node_VariableGet>(APin->LinkedTo[0]->GetOwningNodeUnchecked()))
					{
						VarName = VG->GetVarName().ToString();
					}
				}
			}
			OutInfo.RuleType = TEXT("Bool");
			OutInfo.RuleVariable = VarName;
			OutInfo.RuleSummary = VarName.IsEmpty() ? TEXT("NOT (bool)") : FString::Printf(TEXT("%s == false"), *VarName);
			OutInfo.bHasRule = true;
			return;
		}

		// Treat as a numeric comparison.
		FString VarName;
		if (UEdGraphPin* APin = CallFn->FindPin(TEXT("A"), EGPD_Input))
		{
			if (APin->LinkedTo.Num() > 0)
			{
				if (UK2Node_VariableGet* VG = Cast<UK2Node_VariableGet>(APin->LinkedTo[0]->GetOwningNodeUnchecked()))
				{
					VarName = VG->GetVarName().ToString();
				}
			}
		}
		FString BVal;
		if (UEdGraphPin* BPin = CallFn->FindPin(TEXT("B"), EGPD_Input))
		{
			BVal = BPin->DefaultValue;
		}
		OutInfo.RuleType = TEXT("Comparison");
		OutInfo.RuleVariable = VarName;
		OutInfo.RuleSummary = FString::Printf(TEXT("%s %s %s"), *VarName, *FunctionNameToSymbol(FnName), *BVal);
		OutInfo.bHasRule = true;
		return;
	}

	OutInfo.RuleType = TEXT("Custom");
	OutInfo.RuleSummary = TEXT("custom rule graph");
	OutInfo.bHasRule = true;
}

// ============================================================================
// ENTRY STATE / TRANSITION SETTINGS
// ============================================================================

bool UAnimGraphService::SetEntryState(const FString& AnimBlueprintPath, const FString& StateMachineName, const FString& StateName)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return false;
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode || !StateMachineNode->EditorStateMachineGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("SetEntryState: State machine '%s' not found"), *StateMachineName);
		return false;
	}

	UAnimationStateMachineGraph* SMGraph = Cast<UAnimationStateMachineGraph>(StateMachineNode->EditorStateMachineGraph);
	if (!SMGraph || !SMGraph->EntryNode)
	{
		UE_LOG(LogTemp, Error, TEXT("SetEntryState: State machine '%s' has no entry node"), *StateMachineName);
		return false;
	}

	UAnimStateNodeBase* TargetState = FindStateNode(SMGraph, StateName);
	if (!TargetState)
	{
		UE_LOG(LogTemp, Error, TEXT("SetEntryState: State '%s' not found"), *StateName);
		return false;
	}

	UEdGraphPin* EntryOutPin = SMGraph->EntryNode->GetOutputPin();
	UEdGraphPin* StateInPin = TargetState->GetInputPin();
	if (!EntryOutPin || !StateInPin)
	{
		UE_LOG(LogTemp, Error, TEXT("SetEntryState: Could not resolve entry/state pins"));
		return false;
	}

	EntryOutPin->BreakAllPinLinks();
	EntryOutPin->MakeLinkTo(StateInPin);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	UE_LOG(LogTemp, Log, TEXT("SetEntryState: '%s' is now the entry state of '%s'"), *StateName, *StateMachineName);
	return true;
}

bool UAnimGraphService::SetTransitionPriority(const FString& AnimBlueprintPath, const FString& StateMachineName,
	const FString& SourceStateName, const FString& DestStateName, int32 Priority)
{
	UAnimBlueprint* AnimBlueprint = nullptr;
	UAnimStateTransitionNode* Transition = ResolveTransition(AnimBlueprintPath, StateMachineName, SourceStateName, DestStateName, AnimBlueprint);
	if (!Transition || !AnimBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetTransitionPriority: transition '%s'->'%s' not found"), *SourceStateName, *DestStateName);
		return false;
	}

	Transition->PriorityOrder = Priority;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	UE_LOG(LogTemp, Log, TEXT("SetTransitionPriority: '%s'->'%s' priority = %d"), *SourceStateName, *DestStateName, Priority);
	return true;
}

bool UAnimGraphService::SetTransitionBlend(const FString& AnimBlueprintPath, const FString& StateMachineName,
	const FString& SourceStateName, const FString& DestStateName, float BlendDuration, const FString& BlendMode)
{
	UAnimBlueprint* AnimBlueprint = nullptr;
	UAnimStateTransitionNode* Transition = ResolveTransition(AnimBlueprintPath, StateMachineName, SourceStateName, DestStateName, AnimBlueprint);
	if (!Transition || !AnimBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetTransitionBlend: transition '%s'->'%s' not found"), *SourceStateName, *DestStateName);
		return false;
	}

	Transition->CrossfadeDuration = FMath::Max(0.0f, BlendDuration);
	Transition->BlendMode = ParseBlendOption(BlendMode);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	UE_LOG(LogTemp, Log, TEXT("SetTransitionBlend: '%s'->'%s' blend %.2fs (%s)"), *SourceStateName, *DestStateName, BlendDuration, *BlendMode);
	return true;
}

// ============================================================================
// TRANSITION RULES
// ============================================================================

bool UAnimGraphService::SetTransitionRuleFromBool(const FString& AnimBlueprintPath, const FString& StateMachineName,
	const FString& SourceStateName, const FString& DestStateName, const FString& BoolVariableName, bool bInvert)
{
	UAnimBlueprint* AnimBlueprint = nullptr;
	UAnimStateTransitionNode* Transition = ResolveTransition(AnimBlueprintPath, StateMachineName, SourceStateName, DestStateName, AnimBlueprint);
	if (!Transition || !AnimBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetTransitionRuleFromBool: transition '%s'->'%s' not found in '%s'"), *SourceStateName, *DestStateName, *StateMachineName);
		return false;
	}

	UClass* VarClass = AnimBlueprint->SkeletonGeneratedClass ? AnimBlueprint->SkeletonGeneratedClass : AnimBlueprint->GeneratedClass;
	FProperty* Prop = VarClass ? VarClass->FindPropertyByName(FName(*BoolVariableName)) : nullptr;
	if (!Prop || !Prop->IsA<FBoolProperty>())
	{
		UE_LOG(LogTemp, Error, TEXT("SetTransitionRuleFromBool: bool variable '%s' not found on AnimBP (add it and compile first)"), *BoolVariableName);
		return false;
	}

	ResetTransitionRule(Transition);

	UAnimGraphNode_TransitionResult* ResultNode = GetTransitionResultNode(Transition);
	UEdGraph* RuleGraph = Transition->BoundGraph;
	if (!ResultNode || !RuleGraph)
	{
		return false;
	}
	UEdGraphPin* CanEnterPin = ResultNode->FindPin(TEXT("bCanEnterTransition"), EGPD_Input);
	if (!CanEnterPin)
	{
		return false;
	}

	FGraphNodeCreator<UK2Node_VariableGet> VarCreator(*RuleGraph);
	UK2Node_VariableGet* VarNode = VarCreator.CreateNode();
	VarNode->VariableReference.SetSelfMember(FName(*BoolVariableName));
	VarNode->NodePosX = ResultNode->NodePosX - (bInvert ? 500 : 280);
	VarNode->NodePosY = ResultNode->NodePosY;
	VarCreator.Finalize();

	UEdGraphPin* VarOutPin = GetVariableValuePin(VarNode);
	if (!VarOutPin)
	{
		RuleGraph->RemoveNode(VarNode);
		UE_LOG(LogTemp, Error, TEXT("SetTransitionRuleFromBool: failed to resolve value pin for '%s'"), *BoolVariableName);
		return false;
	}

	if (bInvert)
	{
		UFunction* NotFn = UKismetMathLibrary::StaticClass()->FindFunctionByName(FName(TEXT("Not_PreBool")));
		if (!NotFn)
		{
			RuleGraph->RemoveNode(VarNode);
			return false;
		}
		FGraphNodeCreator<UK2Node_CallFunction> FnCreator(*RuleGraph);
		UK2Node_CallFunction* NotNode = FnCreator.CreateNode();
		NotNode->SetFromFunction(NotFn);
		NotNode->NodePosX = ResultNode->NodePosX - 250;
		NotNode->NodePosY = ResultNode->NodePosY;
		FnCreator.Finalize();

		UEdGraphPin* APin = NotNode->FindPin(TEXT("A"), EGPD_Input);
		UEdGraphPin* RetPin = NotNode->GetReturnValuePin();
		if (!APin || !RetPin)
		{
			return false;
		}
		VarOutPin->MakeLinkTo(APin);
		RetPin->MakeLinkTo(CanEnterPin);
	}
	else
	{
		VarOutPin->MakeLinkTo(CanEnterPin);
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	UE_LOG(LogTemp, Log, TEXT("SetTransitionRuleFromBool: '%s'->'%s' driven by %s%s"),
		*SourceStateName, *DestStateName, bInvert ? TEXT("NOT ") : TEXT(""), *BoolVariableName);
	return true;
}

bool UAnimGraphService::SetTransitionRuleComparison(const FString& AnimBlueprintPath, const FString& StateMachineName,
	const FString& SourceStateName, const FString& DestStateName, const FString& FloatVariableName, const FString& Comparison, float Threshold)
{
	const FString FnName = ComparisonToFunctionName(Comparison);
	if (FnName.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("SetTransitionRuleComparison: unknown comparison '%s' (use greater/less/greater_equal/less_equal/equal/not_equal)"), *Comparison);
		return false;
	}

	UAnimBlueprint* AnimBlueprint = nullptr;
	UAnimStateTransitionNode* Transition = ResolveTransition(AnimBlueprintPath, StateMachineName, SourceStateName, DestStateName, AnimBlueprint);
	if (!Transition || !AnimBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetTransitionRuleComparison: transition '%s'->'%s' not found"), *SourceStateName, *DestStateName);
		return false;
	}

	UClass* VarClass = AnimBlueprint->SkeletonGeneratedClass ? AnimBlueprint->SkeletonGeneratedClass : AnimBlueprint->GeneratedClass;
	FProperty* Prop = VarClass ? VarClass->FindPropertyByName(FName(*FloatVariableName)) : nullptr;
	if (!Prop || !Prop->IsA<FNumericProperty>())
	{
		UE_LOG(LogTemp, Error, TEXT("SetTransitionRuleComparison: numeric variable '%s' not found on AnimBP (add it and compile first)"), *FloatVariableName);
		return false;
	}

	UFunction* CmpFn = UKismetMathLibrary::StaticClass()->FindFunctionByName(FName(*FnName));
	if (!CmpFn)
	{
		UE_LOG(LogTemp, Error, TEXT("SetTransitionRuleComparison: math function '%s' not found"), *FnName);
		return false;
	}

	ResetTransitionRule(Transition);

	UAnimGraphNode_TransitionResult* ResultNode = GetTransitionResultNode(Transition);
	UEdGraph* RuleGraph = Transition->BoundGraph;
	if (!ResultNode || !RuleGraph)
	{
		return false;
	}
	UEdGraphPin* CanEnterPin = ResultNode->FindPin(TEXT("bCanEnterTransition"), EGPD_Input);
	if (!CanEnterPin)
	{
		return false;
	}

	FGraphNodeCreator<UK2Node_VariableGet> VarCreator(*RuleGraph);
	UK2Node_VariableGet* VarNode = VarCreator.CreateNode();
	VarNode->VariableReference.SetSelfMember(FName(*FloatVariableName));
	VarNode->NodePosX = ResultNode->NodePosX - 550;
	VarNode->NodePosY = ResultNode->NodePosY;
	VarCreator.Finalize();

	UEdGraphPin* VarOutPin = GetVariableValuePin(VarNode);
	if (!VarOutPin)
	{
		RuleGraph->RemoveNode(VarNode);
		UE_LOG(LogTemp, Error, TEXT("SetTransitionRuleComparison: failed to resolve value pin for '%s'"), *FloatVariableName);
		return false;
	}

	FGraphNodeCreator<UK2Node_CallFunction> FnCreator(*RuleGraph);
	UK2Node_CallFunction* CmpNode = FnCreator.CreateNode();
	CmpNode->SetFromFunction(CmpFn);
	CmpNode->NodePosX = ResultNode->NodePosX - 280;
	CmpNode->NodePosY = ResultNode->NodePosY;
	FnCreator.Finalize();

	UEdGraphPin* APin = CmpNode->FindPin(TEXT("A"), EGPD_Input);
	UEdGraphPin* BPin = CmpNode->FindPin(TEXT("B"), EGPD_Input);
	UEdGraphPin* RetPin = CmpNode->GetReturnValuePin();
	if (!APin || !BPin || !RetPin)
	{
		UE_LOG(LogTemp, Error, TEXT("SetTransitionRuleComparison: comparison node pins missing"));
		return false;
	}

	VarOutPin->MakeLinkTo(APin);
	BPin->DefaultValue = FString::SanitizeFloat(Threshold);
	RetPin->MakeLinkTo(CanEnterPin);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	UE_LOG(LogTemp, Log, TEXT("SetTransitionRuleComparison: '%s'->'%s' rule = %s %s %.3f"),
		*SourceStateName, *DestStateName, *FloatVariableName, *FunctionNameToSymbol(FnName), Threshold);
	return true;
}

bool UAnimGraphService::SetTransitionRuleAutomatic(const FString& AnimBlueprintPath, const FString& StateMachineName,
	const FString& SourceStateName, const FString& DestStateName, float TriggerTime)
{
	UAnimBlueprint* AnimBlueprint = nullptr;
	UAnimStateTransitionNode* Transition = ResolveTransition(AnimBlueprintPath, StateMachineName, SourceStateName, DestStateName, AnimBlueprint);
	if (!Transition || !AnimBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("SetTransitionRuleAutomatic: transition '%s'->'%s' not found"), *SourceStateName, *DestStateName);
		return false;
	}

	ResetTransitionRule(Transition);
	Transition->bAutomaticRuleBasedOnSequencePlayerInState = true;
	Transition->AutomaticRuleTriggerTime = TriggerTime;

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	UE_LOG(LogTemp, Log, TEXT("SetTransitionRuleAutomatic: '%s'->'%s' auto (trigger %.3f)"), *SourceStateName, *DestStateName, TriggerTime);
	return true;
}

bool UAnimGraphService::ClearTransitionRule(const FString& AnimBlueprintPath, const FString& StateMachineName,
	const FString& SourceStateName, const FString& DestStateName)
{
	UAnimBlueprint* AnimBlueprint = nullptr;
	UAnimStateTransitionNode* Transition = ResolveTransition(AnimBlueprintPath, StateMachineName, SourceStateName, DestStateName, AnimBlueprint);
	if (!Transition || !AnimBlueprint)
	{
		UE_LOG(LogTemp, Error, TEXT("ClearTransitionRule: transition '%s'->'%s' not found"), *SourceStateName, *DestStateName);
		return false;
	}

	ResetTransitionRule(Transition);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	UE_LOG(LogTemp, Log, TEXT("ClearTransitionRule: cleared '%s'->'%s'"), *SourceStateName, *DestStateName);
	return true;
}

// ============================================================================
// HIGH-LEVEL STATE AUTHORING
// ============================================================================

FString UAnimGraphService::SetStateAnimation(const FString& AnimBlueprintPath, const FString& StateMachineName,
	const FString& StateName, const FString& AnimSequencePath, bool bLoop, float PlayRate)
{
	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		return FString();
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode || !StateMachineNode->EditorStateMachineGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("SetStateAnimation: state machine '%s' not found"), *StateMachineName);
		return FString();
	}

	UAnimStateNode* StateNode = Cast<UAnimStateNode>(FindStateNode(StateMachineNode->EditorStateMachineGraph, StateName));
	if (!StateNode)
	{
		UE_LOG(LogTemp, Error, TEXT("SetStateAnimation: state '%s' not found (or is a conduit)"), *StateName);
		return FString();
	}

	UEdGraph* StateGraph = StateNode->GetBoundGraph();
	if (!StateGraph)
	{
		UE_LOG(LogTemp, Error, TEXT("SetStateAnimation: state '%s' has no graph"), *StateName);
		return FString();
	}

	UAnimSequence* Sequence = Cast<UAnimSequence>(UEditorAssetLibrary::LoadAsset(AnimSequencePath));
	if (!Sequence)
	{
		UE_LOG(LogTemp, Error, TEXT("SetStateAnimation: could not load sequence '%s'"), *AnimSequencePath);
		return FString();
	}

	UAnimGraphNode_SequencePlayer* SeqPlayer = Cast<UAnimGraphNode_SequencePlayer>(FindStateAssetPlayer(StateNode));
	if (!SeqPlayer)
	{
		FGraphNodeCreator<UAnimGraphNode_SequencePlayer> Creator(*StateGraph);
		SeqPlayer = Creator.CreateNode();
		SeqPlayer->NodePosX = -350;
		SeqPlayer->NodePosY = 0;
		Creator.Finalize();
	}

	SeqPlayer->Node.SetSequence(Sequence);
	SeqPlayer->Node.SetLoopAnimation(bLoop);
	SeqPlayer->Node.SetPlayRate(PlayRate);

	// Wire the player's pose output to the state's Output Pose (result) node.
	UAnimGraphNode_StateResult* ResultNode = nullptr;
	for (UEdGraphNode* Node : StateGraph->Nodes)
	{
		if (UAnimGraphNode_StateResult* Result = Cast<UAnimGraphNode_StateResult>(Node))
		{
			ResultNode = Result;
			break;
		}
	}
	if (ResultNode)
	{
		UEdGraphPin* OutPin = nullptr;
		for (UEdGraphPin* Pin : SeqPlayer->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output)
			{
				OutPin = Pin;
				break;
			}
		}
		UEdGraphPin* InPin = nullptr;
		for (UEdGraphPin* Pin : ResultNode->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Input)
			{
				InPin = Pin;
				break;
			}
		}
		if (OutPin && InPin && !OutPin->LinkedTo.Contains(InPin))
		{
			InPin->BreakAllPinLinks();
			OutPin->MakeLinkTo(InPin);
		}
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	UE_LOG(LogTemp, Log, TEXT("SetStateAnimation: '%s' in '%s' -> %s (loop=%d, rate=%.2f)"),
		*StateName, *StateMachineName, *AnimSequencePath, bLoop ? 1 : 0, PlayRate);
	return SeqPlayer->NodeGuid.ToString();
}

FString UAnimGraphService::BuildStateMachine(const FString& AnimBlueprintPath, const FString& StateMachineName,
	const FString& SpecJson, float PosX, float PosY)
{
	TArray<FString> Errors;
	int32 StatesCreated = 0;
	int32 TransitionsCreated = 0;

	auto MakeReport = [&](bool bSuccess) -> FString
	{
		TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("success"), bSuccess);
		Report->SetStringField(TEXT("machine"), StateMachineName);
		Report->SetNumberField(TEXT("states_created"), StatesCreated);
		Report->SetNumberField(TEXT("transitions_created"), TransitionsCreated);
		TArray<TSharedPtr<FJsonValue>> ErrArr;
		for (const FString& E : Errors)
		{
			ErrArr.Add(MakeShared<FJsonValueString>(E));
		}
		Report->SetArrayField(TEXT("errors"), ErrArr);
		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Report, Writer);
		return Out;
	};

	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		Errors.Add(TEXT("Failed to load AnimBlueprint"));
		return MakeReport(false);
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SpecJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Errors.Add(TEXT("Invalid JSON spec"));
		return MakeReport(false);
	}

	FScopedTransaction Transaction(NSLOCTEXT("VibeUE", "BuildStateMachine", "VibeUE: Build State Machine"));

	if (!FindStateMachineNode(AnimBlueprint, StateMachineName))
	{
		const FString NewMachineId = AddStateMachine(AnimBlueprintPath, StateMachineName, PosX, PosY);
		if (NewMachineId.IsEmpty())
		{
			Errors.Add(FString::Printf(TEXT("Failed to create state machine '%s'"), *StateMachineName));
			return MakeReport(false);
		}
	}

	FString FirstStateName;

	// --- States ---
	const TArray<TSharedPtr<FJsonValue>>* StatesArr = nullptr;
	if (Root->TryGetArrayField(TEXT("states"), StatesArr) && StatesArr)
	{
		for (const TSharedPtr<FJsonValue>& Value : *StatesArr)
		{
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				continue;
			}
			TSharedPtr<FJsonObject> StateObj = Value->AsObject();
			if (!StateObj.IsValid())
			{
				continue;
			}

			FString SName;
			if (!StateObj->TryGetStringField(TEXT("name"), SName) || SName.IsEmpty())
			{
				Errors.Add(TEXT("State entry missing 'name'"));
				continue;
			}
			if (FirstStateName.IsEmpty())
			{
				FirstStateName = SName;
			}

			float SX = PosX;
			float SY = PosY;
			const TArray<TSharedPtr<FJsonValue>>* PosArr = nullptr;
			if (StateObj->TryGetArrayField(TEXT("pos"), PosArr) && PosArr && PosArr->Num() >= 2)
			{
				SX = (float)(*PosArr)[0]->AsNumber();
				SY = (float)(*PosArr)[1]->AsNumber();
			}

			UAnimGraphNode_StateMachine* SMNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
			UEdGraph* SMGraph = SMNode ? SMNode->EditorStateMachineGraph : nullptr;
			if (!SMGraph || !FindStateNode(SMGraph, SName))
			{
				const FString StateId = AddState(AnimBlueprintPath, StateMachineName, SName, SX, SY);
				if (StateId.IsEmpty())
				{
					Errors.Add(FString::Printf(TEXT("Failed to add state '%s'"), *SName));
					continue;
				}
				StatesCreated++;
			}

			FString Anim;
			if (StateObj->TryGetStringField(TEXT("animation"), Anim) && !Anim.IsEmpty())
			{
				bool bLoop = true;
				StateObj->TryGetBoolField(TEXT("loop"), bLoop);
				double Rate = 1.0;
				StateObj->TryGetNumberField(TEXT("play_rate"), Rate);
				if (SetStateAnimation(AnimBlueprintPath, StateMachineName, SName, Anim, bLoop, (float)Rate).IsEmpty())
				{
					Errors.Add(FString::Printf(TEXT("Failed to set animation '%s' on state '%s'"), *Anim, *SName));
				}
			}
		}
	}

	// --- Transitions ---
	const TArray<TSharedPtr<FJsonValue>>* TransArr = nullptr;
	if (Root->TryGetArrayField(TEXT("transitions"), TransArr) && TransArr)
	{
		for (const TSharedPtr<FJsonValue>& Value : *TransArr)
		{
			if (!Value.IsValid() || Value->Type != EJson::Object)
			{
				continue;
			}
			TSharedPtr<FJsonObject> TransObj = Value->AsObject();
			if (!TransObj.IsValid())
			{
				continue;
			}

			FString From;
			FString To;
			if (!TransObj->TryGetStringField(TEXT("from"), From) || !TransObj->TryGetStringField(TEXT("to"), To))
			{
				Errors.Add(TEXT("Transition entry missing 'from'/'to'"));
				continue;
			}

			double Blend = 0.2;
			TransObj->TryGetNumberField(TEXT("blend"), Blend);

			UAnimGraphNode_StateMachine* SMNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
			UEdGraph* SMGraph = SMNode ? SMNode->EditorStateMachineGraph : nullptr;
			if (!SMGraph || !FindTransitionNode(SMGraph, From, To))
			{
				const FString TransId = AddTransition(AnimBlueprintPath, StateMachineName, From, To, (float)Blend);
				if (TransId.IsEmpty())
				{
					Errors.Add(FString::Printf(TEXT("Failed to add transition '%s'->'%s'"), *From, *To));
					continue;
				}
				TransitionsCreated++;
			}
			else
			{
				SetTransitionBlend(AnimBlueprintPath, StateMachineName, From, To, (float)Blend, TEXT("Linear"));
			}

			double PriorityD = 0.0;
			if (TransObj->TryGetNumberField(TEXT("priority"), PriorityD))
			{
				SetTransitionPriority(AnimBlueprintPath, StateMachineName, From, To, (int32)PriorityD);
			}

			const TSharedPtr<FJsonObject>* RuleObjPtr = nullptr;
			if (TransObj->TryGetObjectField(TEXT("rule"), RuleObjPtr) && RuleObjPtr && RuleObjPtr->IsValid())
			{
				const TSharedPtr<FJsonObject> RuleObj = *RuleObjPtr;
				FString RType;
				RuleObj->TryGetStringField(TEXT("type"), RType);
				RType = RType.ToLower();

				if (RType == TEXT("bool"))
				{
					FString Var;
					bool bInv = false;
					RuleObj->TryGetStringField(TEXT("variable"), Var);
					RuleObj->TryGetBoolField(TEXT("invert"), bInv);
					if (!SetTransitionRuleFromBool(AnimBlueprintPath, StateMachineName, From, To, Var, bInv))
					{
						Errors.Add(FString::Printf(TEXT("Failed bool rule on '%s'->'%s' (variable '%s')"), *From, *To, *Var));
					}
				}
				else if (RType == TEXT("comparison"))
				{
					FString Var;
					FString Op;
					double Val = 0.0;
					RuleObj->TryGetStringField(TEXT("variable"), Var);
					RuleObj->TryGetStringField(TEXT("op"), Op);
					RuleObj->TryGetNumberField(TEXT("value"), Val);
					if (!SetTransitionRuleComparison(AnimBlueprintPath, StateMachineName, From, To, Var, Op, (float)Val))
					{
						Errors.Add(FString::Printf(TEXT("Failed comparison rule on '%s'->'%s'"), *From, *To));
					}
				}
				else if (RType == TEXT("automatic"))
				{
					double Trig = -1.0;
					RuleObj->TryGetNumberField(TEXT("trigger_time"), Trig);
					SetTransitionRuleAutomatic(AnimBlueprintPath, StateMachineName, From, To, (float)Trig);
				}
				else if (RType == TEXT("always"))
				{
					UAnimBlueprint* RuleBP = nullptr;
					UAnimStateTransitionNode* RuleTrans = ResolveTransition(AnimBlueprintPath, StateMachineName, From, To, RuleBP);
					if (RuleTrans)
					{
						ResetTransitionRule(RuleTrans);
						if (UAnimGraphNode_TransitionResult* RN = GetTransitionResultNode(RuleTrans))
						{
							if (UEdGraphPin* CEP = RN->FindPin(TEXT("bCanEnterTransition"), EGPD_Input))
							{
								CEP->DefaultValue = TEXT("true");
							}
						}
						if (RuleBP)
						{
							FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(RuleBP);
						}
					}
				}
			}
		}
	}

	// --- Entry ---
	FString Entry;
	if (!Root->TryGetStringField(TEXT("entry"), Entry) || Entry.IsEmpty())
	{
		Entry = FirstStateName;
	}
	if (!Entry.IsEmpty())
	{
		if (!SetEntryState(AnimBlueprintPath, StateMachineName, Entry))
		{
			Errors.Add(FString::Printf(TEXT("Failed to set entry state '%s'"), *Entry));
		}
	}

	FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);

	return MakeReport(Errors.Num() == 0);
}

FAnimStateMachineValidationResult UAnimGraphService::ValidateStateMachine(const FString& AnimBlueprintPath, const FString& StateMachineName)
{
	FAnimStateMachineValidationResult Result;

	UAnimBlueprint* AnimBlueprint = LoadAnimBlueprint(AnimBlueprintPath);
	if (!AnimBlueprint)
	{
		Result.Errors.Add(TEXT("Failed to load AnimBlueprint"));
		return Result;
	}

	UAnimGraphNode_StateMachine* StateMachineNode = FindStateMachineNode(AnimBlueprint, StateMachineName);
	if (!StateMachineNode || !StateMachineNode->EditorStateMachineGraph)
	{
		Result.Errors.Add(FString::Printf(TEXT("State machine '%s' not found"), *StateMachineName));
		return Result;
	}

	UAnimationStateMachineGraph* SMGraph = Cast<UAnimationStateMachineGraph>(StateMachineNode->EditorStateMachineGraph);
	if (!SMGraph)
	{
		Result.Errors.Add(TEXT("State machine graph is invalid"));
		return Result;
	}

	TArray<UAnimStateNode*> States;
	TArray<UAnimStateTransitionNode*> Transitions;
	for (UEdGraphNode* Node : SMGraph->Nodes)
	{
		if (UAnimStateNode* State = Cast<UAnimStateNode>(Node))
		{
			States.Add(State);
		}
		else if (UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(Node))
		{
			Transitions.Add(Transition);
		}
	}
	Result.StateCount = States.Num();
	Result.TransitionCount = Transitions.Num();

	// Entry state
	UAnimStateNodeBase* EntryTarget = nullptr;
	if (SMGraph->EntryNode)
	{
		if (UEdGraphPin* EntryOut = SMGraph->EntryNode->GetOutputPin())
		{
			if (EntryOut->LinkedTo.Num() > 0 && EntryOut->LinkedTo[0])
			{
				EntryTarget = Cast<UAnimStateNodeBase>(EntryOut->LinkedTo[0]->GetOwningNodeUnchecked());
			}
		}
	}
	if (!EntryTarget)
	{
		Result.Errors.Add(TEXT("No entry state set (state machine has no default state, will not run)"));
	}

	// States with no animation/pose
	for (UAnimStateNode* State : States)
	{
		bool bHasPose = false;
		if (UEdGraph* StateGraph = State->GetBoundGraph())
		{
			for (UEdGraphNode* Node : StateGraph->Nodes)
			{
				if (UAnimGraphNode_StateResult* ResultNode = Cast<UAnimGraphNode_StateResult>(Node))
				{
					for (UEdGraphPin* Pin : ResultNode->Pins)
					{
						if (Pin && Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() > 0)
						{
							bHasPose = true;
							break;
						}
					}
				}
			}
		}
		if (!bHasPose)
		{
			Result.Warnings.Add(FString::Printf(TEXT("State '%s' has no animation connected to its Output Pose"), *State->GetStateName()));
		}
	}

	// Inert transitions
	for (UAnimStateTransitionNode* Transition : Transitions)
	{
		FAnimTransitionInfo Info;
		DescribeTransitionRule(Transition, Info);
		if (!Info.bHasRule)
		{
			UAnimStateNodeBase* Prev = Transition->GetPreviousState();
			UAnimStateNodeBase* Next = Transition->GetNextState();
			Result.Errors.Add(FString::Printf(TEXT("Transition '%s'->'%s' is inert (no rule) and will never fire"),
				Prev ? *Prev->GetStateName() : TEXT("?"), Next ? *Next->GetStateName() : TEXT("?")));
		}
	}

	// Reachability from entry
	if (EntryTarget)
	{
		TSet<UAnimStateNodeBase*> Reachable;
		TArray<UAnimStateNodeBase*> Stack;
		Reachable.Add(EntryTarget);
		Stack.Add(EntryTarget);
		while (Stack.Num() > 0)
		{
			UAnimStateNodeBase* Current = Stack.Pop();
			for (UAnimStateTransitionNode* Transition : Transitions)
			{
				if (Transition->GetPreviousState() == Current)
				{
					UAnimStateNodeBase* Next = Transition->GetNextState();
					if (Next && !Reachable.Contains(Next))
					{
						Reachable.Add(Next);
						Stack.Add(Next);
					}
				}
			}
		}
		for (UAnimStateNode* State : States)
		{
			if (!Reachable.Contains(State))
			{
				Result.Warnings.Add(FString::Printf(TEXT("State '%s' is unreachable from the entry state"), *State->GetStateName()));
			}
		}
	}

	Result.bIsValid = (Result.Errors.Num() == 0);
	return Result;
}
