// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UStateTreeService.h"

#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "StructUtils/PropertyBag.h"

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

static bool StructNameMatches(const UScriptStruct* Struct, const FString& ExpectedStructName)
{
	if (!Struct)
	{
		return false;
	}

	if (ExpectedStructName.IsEmpty())
	{
		return true;
	}

	const FString StructName = Struct->GetName();
	if (StructName.Equals(ExpectedStructName, ESearchCase::IgnoreCase))
	{
		return true;
	}

	if (!ExpectedStructName.StartsWith(TEXT("F"))
		&& StructName.Equals(TEXT("F") + ExpectedStructName, ESearchCase::IgnoreCase))
	{
		return true;
	}

	if (ExpectedStructName.StartsWith(TEXT("F")) && ExpectedStructName.Len() > 1
		&& StructName.Equals(ExpectedStructName.RightChop(1), ESearchCase::IgnoreCase))
	{
		return true;
	}

	return false;
}

static FStateTreeEditorNode* FindTaskNodeByStruct(UStateTreeState* State, const FString& TaskStructName, int32 TaskMatchIndex)
{
	if (!State)
	{
		return nullptr;
	}

	TArray<int32> MatchingTaskIndices;
	for (int32 TaskIndex = 0; TaskIndex < State->Tasks.Num(); ++TaskIndex)
	{
		const UScriptStruct* NodeStruct = State->Tasks[TaskIndex].Node.GetScriptStruct();
		if (StructNameMatches(NodeStruct, TaskStructName))
		{
			MatchingTaskIndices.Add(TaskIndex);
		}
	}

	if (MatchingTaskIndices.IsEmpty())
	{
		return nullptr;
	}

	const int32 SelectedMatchIndex = (TaskMatchIndex < 0) ? (MatchingTaskIndices.Num() - 1) : TaskMatchIndex;
	if (!MatchingTaskIndices.IsValidIndex(SelectedMatchIndex))
	{
		return nullptr;
	}

	return &State->Tasks[MatchingTaskIndices[SelectedMatchIndex]];
}

static bool ResolvePropertyPath(const UStruct* RootStruct, void* RootValue, const FString& PropertyPath, FProperty*& OutProperty, void*& OutValuePtr)
{
	OutProperty = nullptr;
	OutValuePtr = nullptr;

	if (!RootStruct || !RootValue || PropertyPath.IsEmpty())
	{
		return false;
	}

	TArray<FString> Segments;
	PropertyPath.ParseIntoArray(Segments, TEXT("."), true);
	if (Segments.IsEmpty())
	{
		return false;
	}

	const UStruct* CurrentStruct = RootStruct;
	void* CurrentValue = RootValue;

	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		const FString& Segment = Segments[SegmentIndex];
		FProperty* Property = FindFProperty<FProperty>(CurrentStruct, *Segment);
		if (!Property)
		{
			return false;
		}

		void* PropertyValue = Property->ContainerPtrToValuePtr<void>(CurrentValue);
		if (SegmentIndex == Segments.Num() - 1)
		{
			OutProperty = Property;
			OutValuePtr = PropertyValue;
			return true;
		}

		FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if (!StructProperty || !StructProperty->Struct)
		{
			return false;
		}

		CurrentStruct = StructProperty->Struct;
		CurrentValue = PropertyValue;
	}

	return false;
}

static bool ParseBoolString(const FString& Value, bool& OutValue)
{
	if (Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Value == TEXT("1"))
	{
		OutValue = true;
		return true;
	}

	if (Value.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Value == TEXT("0"))
	{
		OutValue = false;
		return true;
	}

	return false;
}

static bool SetPropertyValueFromString(FProperty* Property, void* ValuePtr, const FString& Value)
{
	if (!Property || !ValuePtr)
	{
		return false;
	}

	if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		TextProperty->SetPropertyValue(ValuePtr, FText::FromString(Value));
		return true;
	}

	if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
	{
		StringProperty->SetPropertyValue(ValuePtr, Value);
		return true;
	}

	if (FNameProperty* NameProperty = CastField<FNameProperty>(Property))
	{
		NameProperty->SetPropertyValue(ValuePtr, FName(*Value));
		return true;
	}

	if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(Property))
	{
		bool bParsedValue = false;
		if (!ParseBoolString(Value, bParsedValue))
		{
			return false;
		}
		BoolProperty->SetPropertyValue(ValuePtr, bParsedValue);
		return true;
	}

	if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property))
	{
		NumericProperty->SetNumericPropertyValueFromString(ValuePtr, *Value);
		return true;
	}

	return Property->ImportText_Direct(*Value, ValuePtr, nullptr, PPF_None) != nullptr;
}

static bool MakeBindingPath(const FGuid StructID, const FString& PropertyPath, FPropertyBindingPath& OutPath)
{
	OutPath = FPropertyBindingPath(StructID);
	if (PropertyPath.IsEmpty())
	{
		return true;
	}

	return OutPath.FromString(PropertyPath);
}

static bool ResolveContextStructID(const UStateTree* StateTree, const FString& ContextName, FGuid& OutStructID)
{
	OutStructID.Invalidate();

	if (!StateTree || !StateTree->GetSchema())
	{
		return false;
	}

	const TConstArrayView<FStateTreeExternalDataDesc> ContextDescs = StateTree->GetSchema()->GetContextDataDescs();
	if (ContextDescs.IsEmpty())
	{
		return false;
	}

	if (ContextName.IsEmpty())
	{
		OutStructID = ContextDescs[0].ID;
		return OutStructID.IsValid();
	}

	for (const FStateTreeExternalDataDesc& ContextDesc : ContextDescs)
	{
		if (ContextDesc.Name.ToString().Equals(ContextName, ESearchCase::IgnoreCase))
		{
			OutStructID = ContextDesc.ID;
			return OutStructID.IsValid();
		}
	}

	return false;
}

static UClass* ResolveActorClassPath(const FString& ActorClassPath)
{
	if (ActorClassPath.IsEmpty())
	{
		return nullptr;
	}

	// 1. Direct class load — handles native classes and Blueprint generated classes already in memory.
	if (UClass* LoadedClass = StaticLoadClass(AActor::StaticClass(), nullptr, *ActorClassPath))
	{
		return LoadedClass;
	}

	// 2. Path without a dot — assume it's a plain Blueprint asset path; construct the generated class path.
	if (!ActorClassPath.Contains(TEXT(".")))
	{
		const FString AssetName = FPackageName::GetShortName(ActorClassPath);
		const FString GeneratedClassPath = FString::Printf(TEXT("%s.%s_C"), *ActorClassPath, *AssetName);
		if (UClass* LoadedGeneratedClass = StaticLoadClass(AActor::StaticClass(), nullptr, *GeneratedClassPath))
		{
			return LoadedGeneratedClass;
		}
	}

	// 3. Strip any object suffix (e.g. ".BP_Cube_C" or ".BP_Cube") to get the Blueprint asset path,
	//    then load the Blueprint and return its generated class.
	//    This handles paths like "/Game/Foo/BP_Bar.BP_Bar_C" that StaticLoadClass can't resolve
	//    when the generated class hasn't been registered yet.
	FString BlueprintAssetPath = ActorClassPath;
	int32 DotIdx = INDEX_NONE;
	if (BlueprintAssetPath.FindLastChar(TEXT('.'), DotIdx))
	{
		BlueprintAssetPath = BlueprintAssetPath.Left(DotIdx);
	}

	if (UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(BlueprintAssetPath))
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(LoadedAsset))
		{
			if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
			{
				return Blueprint->GeneratedClass;
			}
		}
		// Also handle when the path already pointed directly at the generated class object.
		if (UClass* DirectClass = Cast<UClass>(LoadedAsset))
		{
			if (DirectClass->IsChildOf(AActor::StaticClass()))
			{
				return DirectClass;
			}
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
	if (!StateTree)
	{
		return;
	}

	StateTree->MarkPackageDirty();
	StateTree->Modify();

#if WITH_EDITORONLY_DATA
	if (UStateTreeEditorData* EditorData = GetEditorData(StateTree))
	{
		EditorData->Modify();
		EditorData->PostEditChange();

#if WITH_EDITOR
		FPropertyChangedEvent PropertyChangedEvent(nullptr);
		EditorData->PostEditChangeProperty(PropertyChangedEvent);
#endif
	}
#endif

	StateTree->PostEditChange();

#if WITH_EDITOR
	FPropertyChangedEvent PropertyChangedEvent(nullptr);
	StateTree->PostEditChangeProperty(PropertyChangedEvent);
#endif
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

bool UStateTreeService::SetStateDescription(const FString& AssetPath, const FString& StatePath, const FString& Description)
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
		UE_LOG(LogStateTreeService, Warning, TEXT("SetStateDescription: State not found: %s"), *StatePath);
		return false;
	}

	State->Description = Description;
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::SetStateThemeColor(const FString& AssetPath, const FString& StatePath,
	const FString& ColorName, const FLinearColor& Color)
{
	if (ColorName.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetStateThemeColor: ColorName is empty"));
		return false;
	}

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
		UE_LOG(LogStateTreeService, Warning, TEXT("SetStateThemeColor: State not found: %s"), *StatePath);
		return false;
	}

	FStateTreeEditorColor UpdatedColor;
	bool bFoundExistingColor = false;
	for (const FStateTreeEditorColor& ExistingColor : EditorData->Colors)
	{
		if (ExistingColor.DisplayName.Equals(ColorName, ESearchCase::IgnoreCase))
		{
			UpdatedColor = ExistingColor;
			bFoundExistingColor = true;
			break;
		}
	}

	if (bFoundExistingColor)
	{
		EditorData->Colors.Remove(UpdatedColor);
	}

	UpdatedColor.DisplayName = ColorName;
	UpdatedColor.Color = Color;
	EditorData->Colors.Add(UpdatedColor);
	State->ColorRef = UpdatedColor.ColorRef;

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::RenameThemeColor(const FString& AssetPath, const FString& OldColorName, const FString& NewColorName)
{
	if (OldColorName.IsEmpty() || NewColorName.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RenameThemeColor: OldColorName and NewColorName must not be empty"));
		return false;
	}

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

	FStateTreeEditorColor FoundColor;
	bool bFound = false;
	for (const FStateTreeEditorColor& ExistingColor : EditorData->Colors)
	{
		if (ExistingColor.DisplayName.Equals(OldColorName, ESearchCase::IgnoreCase))
		{
			FoundColor = ExistingColor;
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RenameThemeColor: Color '%s' not found in %s"), *OldColorName, *AssetPath);
		return false;
	}

	// Remove old entry, update display name, re-add — ColorRef UUID is preserved so state references remain valid
	EditorData->Colors.Remove(FoundColor);
	FoundColor.DisplayName = NewColorName;
	EditorData->Colors.Add(FoundColor);

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::SetContextActorClass(const FString& AssetPath, const FString& ActorClassPath)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetContextActorClass: Failed to load StateTree: %s"), *AssetPath);
		return false;
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData || !EditorData->Schema)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetContextActorClass: Missing editor schema for %s"), *AssetPath);
		return false;
	}

	UClass* ActorClass = ResolveActorClassPath(ActorClassPath);
	if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetContextActorClass: Could not resolve actor class path: %s"), *ActorClassPath);
		return false;
	}

	FClassProperty* ContextActorClassProperty = FindFProperty<FClassProperty>(EditorData->Schema->GetClass(), TEXT("ContextActorClass"));
	if (!ContextActorClassProperty)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetContextActorClass: Schema %s has no ContextActorClass property"),
			*EditorData->Schema->GetClass()->GetName());
		return false;
	}

	EditorData->Schema->Modify();
	ContextActorClassProperty->SetPropertyValue_InContainer(EditorData->Schema, ActorClass);

	// Keep first context descriptor in sync for schemas that cache context data entries.
	TConstArrayView<FStateTreeExternalDataDesc> ContextDescs = EditorData->Schema->GetContextDataDescs();
	if (!ContextDescs.IsEmpty())
	{
		FStateTreeExternalDataDesc& MutableDesc = const_cast<FStateTreeExternalDataDesc&>(ContextDescs[0]);
		MutableDesc.Struct = ActorClass;
	}

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::AddOrUpdateRootFloatParameter(const FString& AssetPath, const FString& ParameterName,
	float DefaultValue)
{
	if (ParameterName.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddOrUpdateRootFloatParameter: ParameterName is empty"));
		return false;
	}

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

	FInstancedPropertyBag& RootPropertyBag = const_cast<FInstancedPropertyBag&>(EditorData->GetRootParametersPropertyBag());
	const FName ParameterFName(*ParameterName);

	const FPropertyBagPropertyDesc* ExistingDesc = RootPropertyBag.FindPropertyDescByName(ParameterFName);
	if (!ExistingDesc || ExistingDesc->ValueType != EPropertyBagPropertyType::Float || !ExistingDesc->ContainerTypes.IsEmpty())
	{
		if (RootPropertyBag.AddProperty(ParameterFName, EPropertyBagPropertyType::Float, nullptr, true) != EPropertyBagAlterationResult::Success)
		{
			UE_LOG(LogStateTreeService, Warning, TEXT("AddOrUpdateRootFloatParameter: Failed to add float parameter '%s'"), *ParameterName);
			return false;
		}
	}

	if (RootPropertyBag.SetValueFloat(ParameterFName, DefaultValue) != EPropertyBagResult::Success)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddOrUpdateRootFloatParameter: Failed to set default value for '%s'"), *ParameterName);
		return false;
	}

	MarkStateTreeDirty(StateTree);
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

bool UStateTreeService::SetTaskPropertyValue(const FString& AssetPath, const FString& StatePath,
	const FString& TaskStructName, const FString& PropertyPath, const FString& Value, int32 TaskMatchIndex)
{
	if (PropertyPath.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTaskPropertyValue: PropertyPath is empty"));
		return false;
	}

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
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTaskPropertyValue: State not found: %s"), *StatePath);
		return false;
	}

	FStateTreeEditorNode* TaskNode = FindTaskNodeByStruct(State, TaskStructName, TaskMatchIndex);
	if (!TaskNode)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("SetTaskPropertyValue: Task not found in %s for struct '%s' at match index %d"),
			*StatePath, *TaskStructName, TaskMatchIndex);
		return false;
	}

	if (!TaskNode->Instance.IsValid())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTaskPropertyValue: Task instance is invalid"));
		return false;
	}

	FProperty* Property = nullptr;
	void* PropertyValuePtr = nullptr;
	if (!ResolvePropertyPath(TaskNode->Instance.GetScriptStruct(), TaskNode->Instance.GetMutableMemory(),
		PropertyPath, Property, PropertyValuePtr))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTaskPropertyValue: Could not resolve property path '%s'"), *PropertyPath);
		return false;
	}

	if (!SetPropertyValueFromString(Property, PropertyValuePtr, Value))
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("SetTaskPropertyValue: Failed to set property '%s' with value '%s'"), *PropertyPath, *Value);
		return false;
	}

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::BindTaskPropertyToRootParameter(const FString& AssetPath, const FString& StatePath,
	const FString& TaskStructName, const FString& TaskPropertyPath, const FString& ParameterPath, int32 TaskMatchIndex)
{
	if (TaskPropertyPath.IsEmpty() || ParameterPath.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTaskPropertyToRootParameter: TaskPropertyPath and ParameterPath are required"));
		return false;
	}

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
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTaskPropertyToRootParameter: State not found: %s"), *StatePath);
		return false;
	}

	FStateTreeEditorNode* TaskNode = FindTaskNodeByStruct(State, TaskStructName, TaskMatchIndex);
	if (!TaskNode)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("BindTaskPropertyToRootParameter: Task not found in %s for struct '%s' at match index %d"),
			*StatePath, *TaskStructName, TaskMatchIndex);
		return false;
	}

	FPropertyBindingPath SourcePath;
	if (!MakeBindingPath(EditorData->GetRootParametersGuid(), ParameterPath, SourcePath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTaskPropertyToRootParameter: Invalid parameter path: %s"), *ParameterPath);
		return false;
	}

	FPropertyBindingPath TargetPath;
	if (!MakeBindingPath(TaskNode->ID, TaskPropertyPath, TargetPath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTaskPropertyToRootParameter: Invalid task property path: %s"), *TaskPropertyPath);
		return false;
	}

	EditorData->AddPropertyBinding(SourcePath, TargetPath);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::BindTaskPropertyToContext(const FString& AssetPath, const FString& StatePath,
	const FString& TaskStructName, const FString& TaskPropertyPath, const FString& ContextName,
	const FString& ContextPropertyPath, int32 TaskMatchIndex)
{
	if (TaskPropertyPath.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTaskPropertyToContext: TaskPropertyPath is required"));
		return false;
	}

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
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTaskPropertyToContext: State not found: %s"), *StatePath);
		return false;
	}

	FStateTreeEditorNode* TaskNode = FindTaskNodeByStruct(State, TaskStructName, TaskMatchIndex);
	if (!TaskNode)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("BindTaskPropertyToContext: Task not found in %s for struct '%s' at match index %d"),
			*StatePath, *TaskStructName, TaskMatchIndex);
		return false;
	}

	FGuid ContextStructID;
	if (!ResolveContextStructID(StateTree, ContextName, ContextStructID))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTaskPropertyToContext: Context '%s' not found"), *ContextName);
		return false;
	}

	FPropertyBindingPath SourcePath;
	if (!MakeBindingPath(ContextStructID, ContextPropertyPath, SourcePath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTaskPropertyToContext: Invalid context property path: %s"), *ContextPropertyPath);
		return false;
	}

	FPropertyBindingPath TargetPath;
	if (!MakeBindingPath(TaskNode->ID, TaskPropertyPath, TargetPath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTaskPropertyToContext: Invalid task property path: %s"), *TaskPropertyPath);
		return false;
	}

	EditorData->AddPropertyBinding(SourcePath, TargetPath);
	MarkStateTreeDirty(StateTree);
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
