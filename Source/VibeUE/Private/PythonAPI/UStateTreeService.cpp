// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UStateTreeService.h"

#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

// StateTree core
#include "StateTree.h"
#include "StateTreeTypes.h"
#include "StateTreeTaskBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeConditionBase.h"

// StateTree editor
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StateTreeSchema.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"

// For class discovery
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogStateTreeService, Log, All);

// ============================================================
// Internal helpers
// ============================================================

namespace UStateTreeServiceHelpers
{

static UStateTree* LoadStateTree(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return nullptr;
	}

	// Try to find in memory first — handles newly created assets that haven't been saved yet.
	// UEditorAssetLibrary::LoadAsset only finds assets on disk, so a fresh CreateAsset call
	// leaves the object in memory but unreachable by LoadAsset until it's saved.
	const FString ShortName = FPackageName::GetShortName(AssetPath);
	const FString FullObjectPath = AssetPath + TEXT(".") + ShortName;
	if (UStateTree* Found = FindObject<UStateTree>(nullptr, *FullObjectPath, EFindObjectFlags::None))
	{
		return Found;
	}

	// Fall back to loading from disk
	UObject* Obj = UEditorAssetLibrary::LoadAsset(AssetPath);
	return Cast<UStateTree>(Obj);
}

static UStateTreeEditorData* GetEditorData(UStateTree* StateTree)
{
	if (!StateTree)
	{
		return nullptr;
	}
#if WITH_EDITORONLY_DATA
	return Cast<UStateTreeEditorData>(StateTree->EditorData);
#else
	return nullptr;
#endif
}

/** Split a state path like "Root/Walking/Idle" into segments. */
static TArray<FString> SplitPath(const FString& Path)
{
	TArray<FString> Segments;
	Path.ParseIntoArray(Segments, TEXT("/"), true);
	return Segments;
}

/** Build the full path for a state by traversing up to the root. */
static FString BuildStatePath(const UStateTreeState* State)
{
	if (!State)
	{
		return FString();
	}
	TArray<FString> Parts;
	const UStateTreeState* Current = State;
	while (Current)
	{
		Parts.Insert(Current->Name.ToString(), 0);
		Current = Current->Parent;
	}
	return FString::Join(Parts, TEXT("/"));
}

/**
 * Find a state by slash-separated path starting from the subtree roots.
 * e.g. "Root" → SubTrees[x] named "Root"
 *      "Root/Walking" → SubTrees[x].Children[y] named "Walking"
 */
static UStateTreeState* FindStateByPath(UStateTreeEditorData* EditorData, const FString& StatePath)
{
	if (!EditorData || StatePath.IsEmpty())
	{
		return nullptr;
	}

	TArray<FString> Segments = SplitPath(StatePath);
	if (Segments.IsEmpty())
	{
		return nullptr;
	}

	// Find the matching top-level subtree
	UStateTreeState* Current = nullptr;
	for (UStateTreeState* SubTree : EditorData->SubTrees)
	{
		if (SubTree && SubTree->Name.ToString() == Segments[0])
		{
			Current = SubTree;
			break;
		}
	}

	if (!Current)
	{
		return nullptr;
	}

	// Navigate through children for remaining segments
	for (int32 i = 1; i < Segments.Num(); ++i)
	{
		UStateTreeState* Found = nullptr;
		for (UStateTreeState* Child : Current->Children)
		{
			if (Child && Child->Name.ToString() == Segments[i])
			{
				Found = Child;
				break;
			}
		}
		if (!Found)
		{
			return nullptr;
		}
		Current = Found;
	}

	return Current;
}

/**
 * Find a UScriptStruct by name. Tries with and without the "F" prefix.
 * Searches across all packages.
 */
static UScriptStruct* FindNodeStruct(const FString& StructName)
{
	if (StructName.IsEmpty())
	{
		return nullptr;
	}

	// Try the name as given
	UScriptStruct* Found = FindFirstObject<UScriptStruct>(*StructName, EFindFirstObjectOptions::None);
	if (Found)
	{
		return Found;
	}

	// Try with "F" prefix stripped (user may have passed it without prefix)
	if (!StructName.StartsWith(TEXT("F")))
	{
		Found = FindFirstObject<UScriptStruct>(*(TEXT("F") + StructName), EFindFirstObjectOptions::None);
		if (Found)
		{
			return Found;
		}
	}

	// Try without "F" prefix if user passed it with prefix
	if (StructName.StartsWith(TEXT("F")) && StructName.Len() > 1)
	{
		Found = FindFirstObject<UScriptStruct>(*StructName.RightChop(1), EFindFirstObjectOptions::None);
		if (Found)
		{
			return Found;
		}
	}

	return nullptr;
}

/**
 * Initialize an FStateTreeEditorNode from an FStateTreeNodeBase-derived struct type.
 */
static bool InitEditorNodeFromStruct(FStateTreeEditorNode& OutNode, UScriptStruct* NodeStruct)
{
	if (!NodeStruct)
	{
		return false;
	}

	OutNode.Reset();
	OutNode.ID = FGuid::NewGuid();
	OutNode.Node.InitializeAs(NodeStruct);

	// Set up instance data from the node's declared instance type
	if (const FStateTreeNodeBase* NodeBase = OutNode.Node.GetPtr<FStateTreeNodeBase>())
	{
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(NodeBase->GetInstanceDataType()))
		{
			OutNode.Instance.InitializeAs(InstanceType);
		}
		if (const UScriptStruct* RuntimeType = Cast<const UScriptStruct>(NodeBase->GetExecutionRuntimeDataType()))
		{
			OutNode.ExecutionRuntimeData.InitializeAs(RuntimeType);
		}
	}

	return true;
}

static FString StateTypeToString(EStateTreeStateType Type)
{
	switch (Type)
	{
	case EStateTreeStateType::State:       return TEXT("State");
	case EStateTreeStateType::Group:       return TEXT("Group");
	case EStateTreeStateType::Linked:      return TEXT("Linked");
	case EStateTreeStateType::LinkedAsset: return TEXT("LinkedAsset");
	case EStateTreeStateType::Subtree:     return TEXT("Subtree");
	default:                               return TEXT("State");
	}
}

static EStateTreeStateType StringToStateType(const FString& TypeStr)
{
	if (TypeStr == TEXT("Group"))       return EStateTreeStateType::Group;
	if (TypeStr == TEXT("Linked"))      return EStateTreeStateType::Linked;
	if (TypeStr == TEXT("LinkedAsset")) return EStateTreeStateType::LinkedAsset;
	if (TypeStr == TEXT("Subtree"))     return EStateTreeStateType::Subtree;
	return EStateTreeStateType::State;
}

static FString SelectionBehaviorToString(EStateTreeStateSelectionBehavior Behavior)
{
	switch (Behavior)
	{
	case EStateTreeStateSelectionBehavior::None:                              return TEXT("None");
	case EStateTreeStateSelectionBehavior::TryEnterState:                    return TEXT("TryEnterState");
	case EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder:         return TEXT("TrySelectChildrenInOrder");
	case EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandom:        return TEXT("TrySelectChildrenAtRandom");
	case EStateTreeStateSelectionBehavior::TrySelectChildrenWithHighestUtility: return TEXT("TrySelectChildrenWithHighestUtility");
	case EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandomWeightedByUtility: return TEXT("TrySelectChildrenAtRandomWeightedByUtility");
	case EStateTreeStateSelectionBehavior::TryFollowTransitions:             return TEXT("TryFollowTransitions");
	default:                                                                  return TEXT("TrySelectChildrenInOrder");
	}
}

static FString TransitionTriggerToString(EStateTreeTransitionTrigger Trigger)
{
	switch (Trigger)
	{
	case EStateTreeTransitionTrigger::OnStateCompleted:  return TEXT("OnStateCompleted");
	case EStateTreeTransitionTrigger::OnStateSucceeded:  return TEXT("OnStateSucceeded");
	case EStateTreeTransitionTrigger::OnStateFailed:     return TEXT("OnStateFailed");
	case EStateTreeTransitionTrigger::OnTick:            return TEXT("OnTick");
	case EStateTreeTransitionTrigger::OnEvent:           return TEXT("OnEvent");
	case EStateTreeTransitionTrigger::OnDelegate:        return TEXT("OnDelegate");
	default:                                              return TEXT("OnStateCompleted");
	}
}

static EStateTreeTransitionTrigger StringToTransitionTrigger(const FString& TriggerStr)
{
	if (TriggerStr == TEXT("OnStateSucceeded")) return EStateTreeTransitionTrigger::OnStateSucceeded;
	if (TriggerStr == TEXT("OnStateFailed"))    return EStateTreeTransitionTrigger::OnStateFailed;
	if (TriggerStr == TEXT("OnTick"))           return EStateTreeTransitionTrigger::OnTick;
	if (TriggerStr == TEXT("OnEvent"))          return EStateTreeTransitionTrigger::OnEvent;
	if (TriggerStr == TEXT("OnDelegate"))       return EStateTreeTransitionTrigger::OnDelegate;
	return EStateTreeTransitionTrigger::OnStateCompleted;
}

static FString TransitionTypeToString(EStateTreeTransitionType Type)
{
	switch (Type)
	{
	case EStateTreeTransitionType::None:                  return TEXT("None");
	case EStateTreeTransitionType::Succeeded:             return TEXT("Succeeded");
	case EStateTreeTransitionType::Failed:                return TEXT("Failed");
	case EStateTreeTransitionType::GotoState:             return TEXT("GotoState");
	case EStateTreeTransitionType::NextState:             return TEXT("NextState");
	case EStateTreeTransitionType::NextSelectableState:   return TEXT("NextSelectableState");
	default:                                               return TEXT("GotoState");
	}
}

static EStateTreeTransitionType StringToTransitionType(const FString& TypeStr)
{
	if (TypeStr == TEXT("Succeeded"))           return EStateTreeTransitionType::Succeeded;
	if (TypeStr == TEXT("Failed"))              return EStateTreeTransitionType::Failed;
	if (TypeStr == TEXT("NextState"))           return EStateTreeTransitionType::NextState;
	if (TypeStr == TEXT("NextSelectableState")) return EStateTreeTransitionType::NextSelectableState;
	if (TypeStr == TEXT("None"))                return EStateTreeTransitionType::None;
	return EStateTreeTransitionType::GotoState;
}

static FString PriorityToString(EStateTreeTransitionPriority Priority)
{
	switch (Priority)
	{
	case EStateTreeTransitionPriority::Low:      return TEXT("Low");
	case EStateTreeTransitionPriority::Normal:   return TEXT("Normal");
	case EStateTreeTransitionPriority::Medium:   return TEXT("Medium");
	case EStateTreeTransitionPriority::High:     return TEXT("High");
	case EStateTreeTransitionPriority::Critical: return TEXT("Critical");
	default:                                      return TEXT("Normal");
	}
}

static EStateTreeTransitionPriority StringToPriority(const FString& PriorityStr)
{
	if (PriorityStr == TEXT("Low"))      return EStateTreeTransitionPriority::Low;
	if (PriorityStr == TEXT("Medium"))   return EStateTreeTransitionPriority::Medium;
	if (PriorityStr == TEXT("High"))     return EStateTreeTransitionPriority::High;
	if (PriorityStr == TEXT("Critical")) return EStateTreeTransitionPriority::Critical;
	return EStateTreeTransitionPriority::Normal;
}

static FStateTreeNodeInfo NodeInfoFromEditorNode(const FStateTreeEditorNode& EditorNode)
{
	FStateTreeNodeInfo Info;
	Info.Name = EditorNode.GetName().ToString();
	if (EditorNode.Node.IsValid())
	{
		Info.StructType = EditorNode.Node.GetScriptStruct()->GetName();
	}
	return Info;
}

static FStateTreeTransitionInfo TransitionInfoFromTransition(const FStateTreeTransition& Transition)
{
	FStateTreeTransitionInfo Info;
	Info.Trigger = TransitionTriggerToString(Transition.Trigger);
	Info.Priority = PriorityToString(Transition.Priority);
	Info.bEnabled = Transition.bTransitionEnabled;

#if WITH_EDITORONLY_DATA
	Info.TransitionType = TransitionTypeToString(Transition.State.LinkType);
	Info.TargetStateName = Transition.State.Name.ToString();
	// Build path from name only; full path requires hierarchy traversal
	Info.TargetStatePath = Transition.State.Name.ToString();
#endif

	return Info;
}

static void CollectStateInfo(const UStateTreeState* State, TArray<FStateTreeStateInfo>& OutStates)
{
	if (!State)
	{
		return;
	}

	FStateTreeStateInfo Info;
	Info.Name = State->Name.ToString();
	Info.Path = BuildStatePath(State);
	Info.StateType = StateTypeToString(State->Type);
	Info.SelectionBehavior = SelectionBehaviorToString(State->SelectionBehavior);
	Info.bEnabled = State->bEnabled;

	for (const FStateTreeEditorNode& TaskNode : State->Tasks)
	{
		Info.Tasks.Add(NodeInfoFromEditorNode(TaskNode));
	}

	for (const FStateTreeEditorNode& CondNode : State->EnterConditions)
	{
		Info.EnterConditions.Add(NodeInfoFromEditorNode(CondNode));
	}

	for (const FStateTreeTransition& Trans : State->Transitions)
	{
		Info.Transitions.Add(TransitionInfoFromTransition(Trans));
	}

	for (const UStateTreeState* Child : State->Children)
	{
		if (Child)
		{
			Info.ChildPaths.Add(BuildStatePath(Child));
		}
	}

	OutStates.Add(Info);

	// Recurse into children
	for (const UStateTreeState* Child : State->Children)
	{
		CollectStateInfo(Child, OutStates);
	}
}

static void MarkStateTreeDirty(UStateTree* StateTree)
{
	if (StateTree && StateTree->GetPackage())
	{
		StateTree->GetPackage()->SetDirtyFlag(true);
		StateTree->Modify();
	}
}

} // namespace UStateTreeServiceHelpers

using namespace UStateTreeServiceHelpers;

// ============================================================
// UStateTreeService implementation
// ============================================================

TArray<FString> UStateTreeService::ListStateTrees(const FString& DirectoryPath)
{
	TArray<FString> Results;

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter Filter;
	Filter.ClassPaths.Add(UStateTree::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(*DirectoryPath));

	TArray<FAssetData> AssetData;
	AssetRegistry.Get().GetAssets(Filter, AssetData);

	for (const FAssetData& Asset : AssetData)
	{
		Results.Add(Asset.GetObjectPathString());
	}

	return Results;
}

bool UStateTreeService::GetStateTreeInfo(const FString& AssetPath, FStateTreeInfo& OutInfo)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("GetStateTreeInfo: Failed to load StateTree: %s"), *AssetPath);
		return false;
	}

	OutInfo.AssetPath = AssetPath;
	OutInfo.AssetName = StateTree->GetName();
	OutInfo.bIsCompiled = StateTree->IsReadyToRun();
	OutInfo.LastCompileStatus = StateTree->IsReadyToRun() ? TEXT("Compiled") : TEXT("Not compiled or compile failed");

	if (const UStateTreeSchema* Schema = StateTree->GetSchema())
	{
		OutInfo.SchemaClass = Schema->GetClass()->GetName();
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (EditorData)
	{
		for (const FStateTreeEditorNode& EvalNode : EditorData->Evaluators)
		{
			OutInfo.Evaluators.Add(NodeInfoFromEditorNode(EvalNode));
		}

		for (const FStateTreeEditorNode& GlobalTaskNode : EditorData->GlobalTasks)
		{
			OutInfo.GlobalTasks.Add(NodeInfoFromEditorNode(GlobalTaskNode));
		}

		for (const UStateTreeState* SubTree : EditorData->SubTrees)
		{
			CollectStateInfo(SubTree, OutInfo.AllStates);
		}
	}
#endif

	return true;
}

TArray<FString> UStateTreeService::GetAvailableTaskTypes()
{
	TArray<FString> Results;

	UScriptStruct* TaskBaseStruct = FStateTreeTaskBase::StaticStruct();
	if (!TaskBaseStruct)
	{
		return Results;
	}

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (Struct && Struct != TaskBaseStruct && Struct->IsChildOf(TaskBaseStruct))
		{
			// Skip abstract or internal structs
			if (!Struct->HasMetaData(TEXT("Hidden")))
			{
				Results.Add(Struct->GetName());
			}
		}
	}

	Results.Sort();
	return Results;
}

TArray<FString> UStateTreeService::GetAvailableEvaluatorTypes()
{
	TArray<FString> Results;

	UScriptStruct* EvalBaseStruct = FStateTreeEvaluatorBase::StaticStruct();
	if (!EvalBaseStruct)
	{
		return Results;
	}

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (Struct && Struct != EvalBaseStruct && Struct->IsChildOf(EvalBaseStruct))
		{
			if (!Struct->HasMetaData(TEXT("Hidden")))
			{
				Results.Add(Struct->GetName());
			}
		}
	}

	Results.Sort();
	return Results;
}

bool UStateTreeService::CreateStateTree(const FString& AssetPath, const FString& SchemaClassName)
{
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("CreateStateTree: AssetPath is empty"));
		return false;
	}

	// Split into package path and asset name
	FString PackagePath;
	FString AssetName;
	if (!AssetPath.Split(TEXT("/"), &PackagePath, &AssetName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("CreateStateTree: Invalid asset path: %s"), *AssetPath);
		return false;
	}
	PackagePath = AssetPath.Left(AssetPath.Len() - AssetName.Len() - 1);

	// Resolve the schema class — default to StateTreeComponentSchema if none specified
	FString SchemaName = SchemaClassName;
	if (SchemaName.IsEmpty())
	{
		SchemaName = TEXT("StateTreeComponentSchema");
	}

	// Allow shorthand names (e.g. "Component" → "StateTreeComponentSchema")
	if (!SchemaName.Contains(TEXT("Schema")))
	{
		SchemaName = TEXT("StateTree") + SchemaName + TEXT("Schema");
	}

	UClass* SchemaClass = nullptr;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (*It && It->IsChildOf(UStateTreeSchema::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
		{
			if (It->GetName() == SchemaName)
			{
				SchemaClass = *It;
				break;
			}
		}
	}

	if (!SchemaClass)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("CreateStateTree: Schema class not found: %s. Available schemas:"), *SchemaName);
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (*It && It->IsChildOf(UStateTreeSchema::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
			{
				UE_LOG(LogStateTreeService, Warning, TEXT("  - %s"), *It->GetName());
			}
		}
		return false;
	}

	// Create the package and asset directly to avoid the factory's schema-picker dialog
	const FString PackageName = PackagePath / AssetName;
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("CreateStateTree: Failed to create package: %s"), *PackageName);
		return false;
	}

	UStateTree* NewStateTree = NewObject<UStateTree>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!NewStateTree)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("CreateStateTree: Failed to create UStateTree object"));
		return false;
	}

	// Create editor data and set the schema on it (the compiler propagates it to UStateTree during compilation)
#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = NewObject<UStateTreeEditorData>(NewStateTree, NAME_None, RF_Transactional);
	EditorData->Schema = NewObject<UStateTreeSchema>(EditorData, SchemaClass);
	NewStateTree->EditorData = EditorData;
#endif

	// Notify the asset registry
	FAssetRegistryModule::AssetCreated(NewStateTree);
	Package->SetDirtyFlag(true);
	NewStateTree->Modify();

	UE_LOG(LogStateTreeService, Log, TEXT("CreateStateTree: Created StateTree at %s (schema: %s)"),
	       *AssetPath, *SchemaClass->GetName());
	return true;
}

bool UStateTreeService::AddState(const FString& AssetPath, const FString& ParentPath,
                                  const FString& StateName, const FString& StateType)
{
	if (StateName.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddState: StateName is empty"));
		return false;
	}

	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddState: Failed to load StateTree: %s"), *AssetPath);
		return false;
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddState: No editor data for: %s"), *AssetPath);
		return false;
	}

	const EStateTreeStateType ParsedType = StringToStateType(StateType);
	const FName NewStateName(*StateName);

	if (ParentPath.IsEmpty())
	{
		// Add a new top-level subtree
		EditorData->AddSubTree(NewStateName);
		UE_LOG(LogStateTreeService, Log, TEXT("AddState: Added subtree '%s' to %s"), *StateName, *AssetPath);
	}
	else
	{
		UStateTreeState* ParentState = FindStateByPath(EditorData, ParentPath);
		if (!ParentState)
		{
			UE_LOG(LogStateTreeService, Warning, TEXT("AddState: Parent state not found: %s"), *ParentPath);
			return false;
		}
		ParentState->AddChildState(NewStateName, ParsedType);
		UE_LOG(LogStateTreeService, Log, TEXT("AddState: Added state '%s' under '%s' in %s"),
		       *StateName, *ParentPath, *AssetPath);
	}

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::RemoveState(const FString& AssetPath, const FString& StatePath)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData)
	{
		return false;
	}

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveState: State not found: %s"), *StatePath);
		return false;
	}

	UStateTreeState* Parent = State->Parent;
	if (Parent)
	{
		const int32 Removed = Parent->Children.Remove(State);
		if (Removed > 0)
		{
			MarkStateTreeDirty(StateTree);
			UE_LOG(LogStateTreeService, Log, TEXT("RemoveState: Removed '%s' from %s"), *StatePath, *AssetPath);
			return true;
		}
	}
	else
	{
		// It's a top-level subtree
		const int32 Removed = EditorData->SubTrees.Remove(State);
		if (Removed > 0)
		{
			MarkStateTreeDirty(StateTree);
			UE_LOG(LogStateTreeService, Log, TEXT("RemoveState: Removed subtree '%s' from %s"), *StatePath, *AssetPath);
			return true;
		}
	}
#endif

	return false;
}

bool UStateTreeService::SetStateEnabled(const FString& AssetPath, const FString& StatePath, bool bEnabled)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData)
	{
		return false;
	}

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetStateEnabled: State not found: %s"), *StatePath);
		return false;
	}

	State->bEnabled = bEnabled;
	MarkStateTreeDirty(StateTree);
	UE_LOG(LogStateTreeService, Log, TEXT("SetStateEnabled: %s -> %s in %s"),
	       *StatePath, bEnabled ? TEXT("enabled") : TEXT("disabled"), *AssetPath);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::AddTask(const FString& AssetPath, const FString& StatePath,
                                 const FString& TaskStructName)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddTask: Failed to load StateTree: %s"), *AssetPath);
		return false;
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData)
	{
		return false;
	}

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddTask: State not found: %s"), *StatePath);
		return false;
	}

	UScriptStruct* TaskStruct = FindNodeStruct(TaskStructName);
	if (!TaskStruct)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddTask: Task struct not found: %s"), *TaskStructName);
		return false;
	}

	// Verify it derives from FStateTreeTaskBase
	if (!TaskStruct->IsChildOf(FStateTreeTaskBase::StaticStruct()))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddTask: Struct '%s' is not a FStateTreeTaskBase"), *TaskStructName);
		return false;
	}

	FStateTreeEditorNode& NewNode = State->Tasks.AddDefaulted_GetRef();
	if (!InitEditorNodeFromStruct(NewNode, TaskStruct))
	{
		State->Tasks.RemoveAt(State->Tasks.Num() - 1);
		return false;
	}

	MarkStateTreeDirty(StateTree);
	UE_LOG(LogStateTreeService, Log, TEXT("AddTask: Added '%s' to state '%s' in %s"),
	       *TaskStructName, *StatePath, *AssetPath);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::AddEvaluator(const FString& AssetPath, const FString& EvaluatorStructName)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddEvaluator: Failed to load StateTree: %s"), *AssetPath);
		return false;
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData)
	{
		return false;
	}

	UScriptStruct* EvalStruct = FindNodeStruct(EvaluatorStructName);
	if (!EvalStruct)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddEvaluator: Struct not found: %s"), *EvaluatorStructName);
		return false;
	}

	if (!EvalStruct->IsChildOf(FStateTreeEvaluatorBase::StaticStruct()))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddEvaluator: Struct '%s' is not a FStateTreeEvaluatorBase"), *EvaluatorStructName);
		return false;
	}

	FStateTreeEditorNode& NewNode = EditorData->Evaluators.AddDefaulted_GetRef();
	if (!InitEditorNodeFromStruct(NewNode, EvalStruct))
	{
		EditorData->Evaluators.RemoveAt(EditorData->Evaluators.Num() - 1);
		return false;
	}

	MarkStateTreeDirty(StateTree);
	UE_LOG(LogStateTreeService, Log, TEXT("AddEvaluator: Added '%s' to %s"), *EvaluatorStructName, *AssetPath);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::AddGlobalTask(const FString& AssetPath, const FString& TaskStructName)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddGlobalTask: Failed to load StateTree: %s"), *AssetPath);
		return false;
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData)
	{
		return false;
	}

	UScriptStruct* TaskStruct = FindNodeStruct(TaskStructName);
	if (!TaskStruct)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddGlobalTask: Struct not found: %s"), *TaskStructName);
		return false;
	}

	if (!TaskStruct->IsChildOf(FStateTreeTaskBase::StaticStruct()))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddGlobalTask: Struct '%s' is not a FStateTreeTaskBase"), *TaskStructName);
		return false;
	}

	FStateTreeEditorNode& NewNode = EditorData->GlobalTasks.AddDefaulted_GetRef();
	if (!InitEditorNodeFromStruct(NewNode, TaskStruct))
	{
		EditorData->GlobalTasks.RemoveAt(EditorData->GlobalTasks.Num() - 1);
		return false;
	}

	MarkStateTreeDirty(StateTree);
	UE_LOG(LogStateTreeService, Log, TEXT("AddGlobalTask: Added '%s' to %s"), *TaskStructName, *AssetPath);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::AddTransition(const FString& AssetPath, const FString& StatePath,
                                       const FString& Trigger, const FString& TransitionType,
                                       const FString& TargetPath, const FString& Priority)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddTransition: Failed to load StateTree: %s"), *AssetPath);
		return false;
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData)
	{
		return false;
	}

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddTransition: Source state not found: %s"), *StatePath);
		return false;
	}

	const EStateTreeTransitionTrigger ParsedTrigger = StringToTransitionTrigger(Trigger);
	const EStateTreeTransitionType ParsedType = StringToTransitionType(TransitionType);
	const EStateTreeTransitionPriority ParsedPriority = StringToPriority(Priority);

	// For GotoState, find the target state
	const UStateTreeState* TargetState = nullptr;
	if (ParsedType == EStateTreeTransitionType::GotoState && !TargetPath.IsEmpty())
	{
		TargetState = FindStateByPath(EditorData, TargetPath);
		if (!TargetState)
		{
			UE_LOG(LogStateTreeService, Warning, TEXT("AddTransition: Target state not found: %s"), *TargetPath);
			return false;
		}
	}

	FStateTreeTransition& NewTransition = State->AddTransition(ParsedTrigger, ParsedType, TargetState);
	NewTransition.Priority = ParsedPriority;

	MarkStateTreeDirty(StateTree);
	UE_LOG(LogStateTreeService, Log, TEXT("AddTransition: Added '%s' -> '%s' to state '%s' in %s"),
	       *Trigger, *TransitionType, *StatePath, *AssetPath);
	return true;
#else
	return false;
#endif
}

FStateTreeCompileResult UStateTreeService::CompileStateTree(const FString& AssetPath)
{
	FStateTreeCompileResult Result;

	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		Result.Errors.Add(FString::Printf(TEXT("Failed to load StateTree: %s"), *AssetPath));
		return Result;
	}

#if WITH_EDITORONLY_DATA
	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);

	const bool bSuccess = Compiler.Compile(*StateTree);
	Result.bSuccess = bSuccess;

	// Extract messages from log via tokenized messages
	TArray<TSharedRef<FTokenizedMessage>> TokenizedMessages = Log.ToTokenizedMessages();
	for (const TSharedRef<FTokenizedMessage>& Msg : TokenizedMessages)
	{
		const FString MsgText = Msg->ToText().ToString();
		if (Msg->GetSeverity() == EMessageSeverity::Error)
		{
			Result.Errors.Add(MsgText);
		}
		else if (Msg->GetSeverity() == EMessageSeverity::Warning || Msg->GetSeverity() == EMessageSeverity::PerformanceWarning)
		{
			Result.Warnings.Add(MsgText);
		}
	}

	if (bSuccess)
	{
		MarkStateTreeDirty(StateTree);
		UE_LOG(LogStateTreeService, Log, TEXT("CompileStateTree: Successfully compiled %s"), *AssetPath);
	}
	else
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("CompileStateTree: Compilation failed for %s (%d errors)"),
		       *AssetPath, Result.Errors.Num());
	}
#else
	Result.Errors.Add(TEXT("StateTree compilation requires an editor build"));
#endif

	return Result;
}

bool UStateTreeService::SaveStateTree(const FString& AssetPath)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SaveStateTree: Failed to load StateTree: %s"), *AssetPath);
		return false;
	}

	const bool bSaved = UEditorAssetLibrary::SaveAsset(AssetPath, false);
	if (bSaved)
	{
		UE_LOG(LogStateTreeService, Log, TEXT("SaveStateTree: Saved %s"), *AssetPath);
	}
	else
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SaveStateTree: Failed to save %s"), *AssetPath);
	}
	return bSaved;
}
