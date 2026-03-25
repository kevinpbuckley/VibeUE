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
