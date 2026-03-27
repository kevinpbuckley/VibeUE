// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UStateTreeService.h"

#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Misc/PackageName.h"
#include "StructUtils/PropertyBag.h"

// StateTree core
#include "StateTree.h"
#include "StateTreeTypes.h"
#include "StateTreeReference.h"
#include "StateTreeTaskBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeConditionBase.h"

// StateTree editor
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StateTreeSchema.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeViewModel.h"
#include "Editor.h"

// For class discovery
#include "UObject/UObjectIterator.h"

#include "GameplayTagContainer.h"
#include "PythonAPI/UActorService.h"

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
	if (UStateTree* Found = FindObject<UStateTree>(nullptr, *FullObjectPath))
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

/**
 * Check if an editor node is a StateTreeBlueprintTaskWrapper whose TaskClass
 * or display name matches the user-provided name.  This lets callers pass
 * "STT_Rotate_C", "STT_Rotate", or the display name instead of the raw
 * wrapper struct name.
 */
static bool BlueprintWrapperMatchesName(const FStateTreeEditorNode& EditorNode, const FString& ExpectedName)
{
	const UScriptStruct* NodeStruct = EditorNode.Node.GetScriptStruct();
	if (!NodeStruct || !NodeStruct->GetName().Equals(TEXT("StateTreeBlueprintTaskWrapper"), ESearchCase::IgnoreCase))
	{
		return false;
	}

	// Check display name (e.g. "STT Rotate")
	const FString DisplayName = EditorNode.GetName().ToString();
	if (DisplayName.Equals(ExpectedName, ESearchCase::IgnoreCase)
		|| DisplayName.Replace(TEXT(" "), TEXT("_")).Equals(ExpectedName, ESearchCase::IgnoreCase))
	{
		return true;
	}

	// Read the TaskClass property to get the Blueprint class path
	const void* NodeMemory = EditorNode.Node.GetMemory();
	if (!NodeMemory)
	{
		return false;
	}

	const FProperty* TaskClassProp = FindFProperty<FProperty>(NodeStruct, TEXT("TaskClass"));
	if (!TaskClassProp)
	{
		return false;
	}

	FString TaskClassStr;
	TaskClassProp->ExportTextItem_Direct(TaskClassStr, TaskClassProp->ContainerPtrToValuePtr<void>(NodeMemory), nullptr, nullptr, 0);
	if (TaskClassStr.IsEmpty())
	{
		return false;
	}

	// TaskClassStr is something like "/Script/Engine.BlueprintGeneratedClass'/Game/StateTree/STT_Rotate.STT_Rotate_C'"
	// Extract the class name after the last dot or slash
	FString ClassName;
	if (TaskClassStr.Split(TEXT("."), nullptr, &ClassName, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		ClassName.RemoveFromEnd(TEXT("'"));
	}
	else
	{
		ClassName = TaskClassStr;
	}

	// Match against "STT_Rotate_C" or "STT_Rotate" (without _C suffix)
	if (ClassName.Equals(ExpectedName, ESearchCase::IgnoreCase))
	{
		return true;
	}
	FString ClassNameNoSuffix = ClassName;
	ClassNameNoSuffix.RemoveFromEnd(TEXT("_C"));
	if (ClassNameNoSuffix.Equals(ExpectedName, ESearchCase::IgnoreCase))
	{
		return true;
	}

	return false;
}

static FStateTreeEditorNode* FindTaskNodeByStruct(UStateTreeState* State, const FString& TaskStructName,
	int32 TaskMatchIndex, int32* OutResolvedTaskMatchIndex = nullptr)
{
	if (!State)
	{
		return nullptr;
	}

	TArray<int32> MatchingTaskIndices;
	for (int32 TaskIndex = 0; TaskIndex < State->Tasks.Num(); ++TaskIndex)
	{
		const UScriptStruct* NodeStruct = State->Tasks[TaskIndex].Node.GetScriptStruct();
		if (StructNameMatches(NodeStruct, TaskStructName)
			|| BlueprintWrapperMatchesName(State->Tasks[TaskIndex], TaskStructName))
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

	if (OutResolvedTaskMatchIndex)
	{
		*OutResolvedTaskMatchIndex = SelectedMatchIndex;
	}

	return &State->Tasks[MatchingTaskIndices[SelectedMatchIndex]];
}

static void AppendPropertyInfo(FProperty* Property, void* ContainerValue, const FString& Prefix,
	TArray<FStateTreePropertyInfo>& OutProperties, TSet<FString>* SeenPropertyPaths = nullptr)
{
	if (!Property || !ContainerValue)
	{
		return;
	}

	void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerValue);
	if (!PropertyValuePtr)
	{
		return;
	}

	const FString PropertyPath = Prefix.IsEmpty() ? Property->GetName() : Prefix + TEXT(".") + Property->GetName();
	if (SeenPropertyPaths && SeenPropertyPaths->Contains(PropertyPath))
	{
		return;
	}

	FStateTreePropertyInfo Info;
	Info.Name = PropertyPath;
	Info.Type = Property->GetCPPType();
	Property->ExportText_Direct(Info.CurrentValue, PropertyValuePtr, PropertyValuePtr, nullptr, PPF_None);
	OutProperties.Add(MoveTemp(Info));
	if (SeenPropertyPaths)
	{
		SeenPropertyPaths->Add(PropertyPath);
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (const UScriptStruct* Struct = StructProperty->Struct)
		{
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				FProperty* ChildProperty = *It;
				if (!ChildProperty)
				{
					continue;
				}

				AppendPropertyInfo(ChildProperty, PropertyValuePtr, PropertyPath, OutProperties, SeenPropertyPaths);
			}
		}
	}
}

static void AppendEditableProperties(const UStruct* RootStruct, void* RootValue,
	TArray<FStateTreePropertyInfo>& OutProperties, TSet<FString>* SeenPropertyPaths = nullptr)
{
	if (!RootStruct || !RootValue)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(RootStruct, EFieldIterationFlags::IncludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property || Property->GetOwnerStruct() != RootStruct)
		{
			continue;
		}

		AppendPropertyInfo(Property, RootValue, TEXT(""), OutProperties, SeenPropertyPaths);
	}
}

static FString GetBindablePropertySegmentName(const FProperty* Property)
{
	if (!Property)
	{
		return TEXT("");
	}

	const FString AuthoredName = Property->GetAuthoredName();
	return AuthoredName.IsEmpty() ? Property->GetName() : AuthoredName;
}

static bool FindBindablePropertyBySegment(const UStruct* OwnerStruct, const FString& RequestedSegment, FProperty*& OutProperty)
{
	OutProperty = nullptr;
	if (!OwnerStruct || RequestedSegment.IsEmpty())
	{
		return false;
	}

	for (TFieldIterator<FProperty> It(OwnerStruct, EFieldIterationFlags::IncludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property || Property->GetOwnerStruct() != OwnerStruct)
		{
			continue;
		}

		const FString RawName = Property->GetName();
		const FString AuthoredName = GetBindablePropertySegmentName(Property);
		if (RawName.Equals(RequestedSegment, ESearchCase::IgnoreCase)
			|| AuthoredName.Equals(RequestedSegment, ESearchCase::IgnoreCase))
		{
			OutProperty = Property;
			return true;
		}

		int32 UnderscoreIndex = INDEX_NONE;
		if (RawName.FindChar(TEXT('_'), UnderscoreIndex))
		{
			const FString BaseName = RawName.Left(UnderscoreIndex);
			if (!BaseName.IsEmpty() && BaseName.Equals(RequestedSegment, ESearchCase::IgnoreCase))
			{
				OutProperty = Property;
				return true;
			}
		}
	}

	return false;
}

static bool ResolveBindablePropertyPathAgainstStruct(const UStruct* RootStruct, const FString& RequestedPath, FString& OutResolvedPath)
{
	OutResolvedPath = RequestedPath;
	if (!RootStruct)
	{
		return false;
	}

	if (RequestedPath.IsEmpty())
	{
		OutResolvedPath = TEXT("");
		return true;
	}

	TArray<FString> RequestedSegments;
	RequestedPath.ParseIntoArray(RequestedSegments, TEXT("."), true);
	if (RequestedSegments.IsEmpty())
	{
		return false;
	}

	const UStruct* CurrentStruct = RootStruct;
	TArray<FString> ResolvedSegments;
	ResolvedSegments.Reserve(RequestedSegments.Num());

	for (int32 Index = 0; Index < RequestedSegments.Num(); ++Index)
	{
		FProperty* MatchingProperty = nullptr;
		if (!FindBindablePropertyBySegment(CurrentStruct, RequestedSegments[Index], MatchingProperty) || !MatchingProperty)
		{
			return false;
		}

		ResolvedSegments.Add(MatchingProperty->GetName());
		if (Index == RequestedSegments.Num() - 1)
		{
			continue;
		}

		const FStructProperty* StructProperty = CastField<FStructProperty>(MatchingProperty);
		if (!StructProperty || !StructProperty->Struct)
		{
			return false;
		}

		CurrentStruct = StructProperty->Struct;
	}

	OutResolvedPath = FString::Join(ResolvedSegments, TEXT("."));
	return true;
}

static bool ResolveEventPayloadBindingPath(const UScriptStruct* PayloadStruct, const FString& RequestedPayloadPath, FString& OutResolvedPath)
{
	OutResolvedPath = RequestedPayloadPath;
	if (!PayloadStruct)
	{
		return false;
	}

	FString ResolvedPayloadPath;
	if (!ResolveBindablePropertyPathAgainstStruct(PayloadStruct, RequestedPayloadPath, ResolvedPayloadPath))
	{
		return false;
	}

	OutResolvedPath = FString::Printf(TEXT("Payload.%s"), *ResolvedPayloadPath);
	return true;
}

static void AppendBindablePropertyInfo(FProperty* Property, void* ContainerValue, const FString& Prefix,
	TArray<FStateTreePropertyInfo>& OutProperties, TSet<FString>* SeenPropertyPaths = nullptr)
{
	if (!Property || !ContainerValue)
	{
		return;
	}

	void* PropertyValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerValue);
	if (!PropertyValuePtr)
	{
		return;
	}

	const FString PropertySegment = GetBindablePropertySegmentName(Property);
	const FString PropertyPath = Prefix.IsEmpty() ? PropertySegment : Prefix + TEXT(".") + PropertySegment;
	if (SeenPropertyPaths && SeenPropertyPaths->Contains(PropertyPath))
	{
		return;
	}

	FStateTreePropertyInfo Info;
	Info.Name = PropertyPath;
	Info.Type = Property->GetCPPType();
	Property->ExportText_Direct(Info.CurrentValue, PropertyValuePtr, PropertyValuePtr, nullptr, PPF_None);
	OutProperties.Add(MoveTemp(Info));
	if (SeenPropertyPaths)
	{
		SeenPropertyPaths->Add(PropertyPath);
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
	{
		if (const UScriptStruct* Struct = StructProperty->Struct)
		{
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				FProperty* ChildProperty = *It;
				if (!ChildProperty)
				{
					continue;
				}

				AppendBindablePropertyInfo(ChildProperty, PropertyValuePtr, PropertyPath, OutProperties, SeenPropertyPaths);
			}
		}
	}
}

static void AppendBindableEditableProperties(const UStruct* RootStruct, void* RootValue,
	TArray<FStateTreePropertyInfo>& OutProperties, TSet<FString>* SeenPropertyPaths = nullptr)
{
	if (!RootStruct || !RootValue)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(RootStruct, EFieldIterationFlags::IncludeSuper); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property || Property->GetOwnerStruct() != RootStruct)
		{
			continue;
		}

		AppendBindablePropertyInfo(Property, RootValue, TEXT(""), OutProperties, SeenPropertyPaths);
	}
}

static bool GetTaskNodeData(FStateTreeEditorNode* TaskNode, const UStruct*& OutStruct, void*& OutMemory)
{
	if (!TaskNode || !TaskNode->Node.IsValid())
	{
		return false;
	}

	OutStruct = TaskNode->Node.GetScriptStruct();
	OutMemory = TaskNode->Node.GetMutableMemory();
	return OutStruct != nullptr && OutMemory != nullptr;
}

/**
 * Resolve the instance data for a task node, handling both struct-based (FInstancedStruct)
 * and UObject-based (InstanceObject) instance data patterns.
 * UE5.5+ tasks may use either — we must handle both.
 */
static bool GetTaskInstanceData(FStateTreeEditorNode* TaskNode, const UStruct*& OutStruct, void*& OutMemory)
{
	if (!TaskNode)
	{
		return false;
	}

	// Struct-based instance data (classic pattern, UE5.4 and earlier tasks)
	if (TaskNode->Instance.IsValid())
	{
		OutStruct = TaskNode->Instance.GetScriptStruct();
		OutMemory = TaskNode->Instance.GetMutableMemory();
		return OutStruct != nullptr && OutMemory != nullptr;
	}

	// UObject-based instance data (UE5.5+ pattern)
	if (IsValid(TaskNode->InstanceObject))
	{
		OutStruct = TaskNode->InstanceObject->GetClass();
		OutMemory = TaskNode->InstanceObject;
		return OutStruct != nullptr && OutMemory != nullptr;
	}

	return false;
}

static bool ResolvePropertyPath(const UStruct* RootStruct, void* RootValue, const FString& PropertyPath,
	FProperty*& OutProperty, void*& OutValuePtr);

static bool ResolveTaskPropertyPath(FStateTreeEditorNode* TaskNode, const FString& PropertyPath,
	FProperty*& OutProperty, void*& OutValuePtr)
{
	const UStruct* NodeStruct = nullptr;
	void* NodeMemory = nullptr;
	if (GetTaskNodeData(TaskNode, NodeStruct, NodeMemory)
		&& ResolvePropertyPath(NodeStruct, NodeMemory, PropertyPath, OutProperty, OutValuePtr))
	{
		return true;
	}

	const UStruct* InstanceStruct = nullptr;
	void* InstanceMemory = nullptr;
	if (GetTaskInstanceData(TaskNode, InstanceStruct, InstanceMemory)
		&& ResolvePropertyPath(InstanceStruct, InstanceMemory, PropertyPath, OutProperty, OutValuePtr))
	{
		return true;
	}

	OutProperty = nullptr;
	OutValuePtr = nullptr;
	return false;
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

	if (!StateTree)
	{
		return false;
	}

	// Try runtime schema first, then fall back to editor schema (for uncompiled trees)
	const UStateTreeSchema* Schema = StateTree->GetSchema();
#if WITH_EDITORONLY_DATA
	if (!Schema)
	{
		if (const UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData))
		{
			Schema = EditorData->Schema;
		}
	}
#endif

	if (!Schema)
	{
		return false;
	}

	const TConstArrayView<FStateTreeExternalDataDesc> ContextDescs = Schema->GetContextDataDescs();
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

static bool IsValidBlueprintTaskClass(UClass* InClass)
{
	const UClass* BlueprintTaskBaseClass = UStateTreeTaskBlueprintBase::StaticClass();
	if (!InClass || !BlueprintTaskBaseClass)
	{
		return false;
	}

	if (!InClass->IsChildOf(BlueprintTaskBaseClass) || InClass == BlueprintTaskBaseClass)
	{
		return false;
	}

	if (InClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return false;
	}

	return true;
}

static UClass* TryResolveBlueprintTaskClassFromObjectPath(const FString& ObjectPath)
{
	if (ObjectPath.IsEmpty())
	{
		return nullptr;
	}

	if (UClass* LoadedClass = LoadObject<UClass>(nullptr, *ObjectPath))
	{
		if (IsValidBlueprintTaskClass(LoadedClass))
		{
			return LoadedClass;
		}
	}

	if (UClass* FoundClass = FindFirstObject<UClass>(*ObjectPath, EFindFirstObjectOptions::None))
	{
		if (IsValidBlueprintTaskClass(FoundClass))
		{
			return FoundClass;
		}
	}

	return nullptr;
}

static UClass* TryResolveBlueprintTaskClassFromAssetPath(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return nullptr;
	}

	if (UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath))
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(LoadedAsset))
		{
			if (IsValidBlueprintTaskClass(Blueprint->GeneratedClass))
			{
				return Blueprint->GeneratedClass;
			}
		}

		if (UClass* LoadedClass = Cast<UClass>(LoadedAsset))
		{
			if (IsValidBlueprintTaskClass(LoadedClass))
			{
				return LoadedClass;
			}
		}
	}

	const FString AssetName = FPackageName::GetShortName(AssetPath);
	if (!AssetName.IsEmpty())
	{
		const FString GeneratedClassPath = FString::Printf(TEXT("%s.%s_C"), *AssetPath, *AssetName);
		if (UClass* GeneratedClass = TryResolveBlueprintTaskClassFromObjectPath(GeneratedClassPath))
		{
			return GeneratedClass;
		}
	}

	return nullptr;
}

static UClass* ResolveBlueprintTaskClass(const FString& TaskIdentifier)
{
	FString Identifier = TaskIdentifier;
	Identifier.TrimStartAndEndInline();
	if (Identifier.IsEmpty())
	{
		return nullptr;
	}

	if (!Identifier.Contains(TEXT("/")))
	{
		if (UClass* LoadedClass = FindFirstObject<UClass>(*Identifier, EFindFirstObjectOptions::None))
		{
			if (IsValidBlueprintTaskClass(LoadedClass))
			{
				return LoadedClass;
			}
		}

		if (!Identifier.EndsWith(TEXT("_C")))
		{
			if (UClass* LoadedClass = FindFirstObject<UClass>(*(Identifier + TEXT("_C")), EFindFirstObjectOptions::None))
			{
				if (IsValidBlueprintTaskClass(LoadedClass))
				{
					return LoadedClass;
				}
			}
		}
	}

	if (Identifier.StartsWith(TEXT("/")))
	{
		if (Identifier.Contains(TEXT(".")))
		{
			if (UClass* TaskClass = TryResolveBlueprintTaskClassFromObjectPath(Identifier))
			{
				return TaskClass;
			}

			FString PackagePath;
			FString ObjectName;
			if (Identifier.Split(TEXT("."), &PackagePath, &ObjectName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
			{
				if (UClass* TaskClass = TryResolveBlueprintTaskClassFromAssetPath(PackagePath))
				{
					return TaskClass;
				}

				if (!ObjectName.EndsWith(TEXT("_C")))
				{
					if (UClass* TaskClass = TryResolveBlueprintTaskClassFromObjectPath(PackagePath + TEXT(".") + ObjectName + TEXT("_C")))
					{
						return TaskClass;
					}
				}
			}
		}
		else
		{
			if (UClass* TaskClass = TryResolveBlueprintTaskClassFromAssetPath(Identifier))
			{
				return TaskClass;
			}
		}
	}

	FString TargetBlueprintName = Identifier;
	if (TargetBlueprintName.Contains(TEXT(".")))
	{
		FString LeftPart;
		FString RightPart;
		if (TargetBlueprintName.Split(TEXT("."), &LeftPart, &RightPart, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			TargetBlueprintName = RightPart;
		}
	}
	if (TargetBlueprintName.Contains(TEXT("/")))
	{
		TargetBlueprintName = FPackageName::GetShortName(TargetBlueprintName);
	}
	if (TargetBlueprintName.EndsWith(TEXT("_C")))
	{
		TargetBlueprintName.LeftChopInline(2);
	}

	if (TargetBlueprintName.IsEmpty())
	{
		return nullptr;
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(TEXT("/Game")));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.Get().GetAssets(Filter, BlueprintAssets);

	for (const FAssetData& AssetData : BlueprintAssets)
	{
		if (!AssetData.AssetName.ToString().Equals(TargetBlueprintName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		if (UClass* TaskClass = TryResolveBlueprintTaskClassFromAssetPath(AssetData.PackageName.ToString()))
		{
			return TaskClass;
		}

		const FAssetTagValueRef GeneratedClassTag = AssetData.TagsAndValues.FindTag(TEXT("GeneratedClass"));
		if (GeneratedClassTag.IsSet())
		{
			const FString GeneratedClassObjectPath = FPackageName::ExportTextPathToObjectPath(GeneratedClassTag.GetValue());
			if (UClass* TaskClass = TryResolveBlueprintTaskClassFromObjectPath(GeneratedClassObjectPath))
			{
				return TaskClass;
			}
		}
	}

	return nullptr;
}

static bool SetBlueprintTaskClassOnWrapperNode(FStateTreeEditorNode& InOutNode, UClass* BlueprintTaskClass, UObject* Outer)
{
	if (!BlueprintTaskClass || !IsValidBlueprintTaskClass(BlueprintTaskClass))
	{
		return false;
	}

	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	const UScriptStruct* NodeStruct = InOutNode.Node.GetScriptStruct();
	void* NodeMemory = InOutNode.Node.GetMutableMemory();
	if (!NodeStruct || !NodeMemory)
	{
		return false;
	}

	FProperty* TaskClassProperty = FindFProperty<FProperty>(NodeStruct, TEXT("TaskClass"));
	if (!TaskClassProperty)
	{
		return false;
	}

	void* TaskClassValuePtr = TaskClassProperty->ContainerPtrToValuePtr<void>(NodeMemory);
	if (!TaskClassValuePtr)
	{
		return false;
	}

	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(TaskClassProperty))
	{
		ObjectProperty->SetObjectPropertyValue(TaskClassValuePtr, BlueprintTaskClass);
	}
	else if (!SetPropertyValueFromString(TaskClassProperty, TaskClassValuePtr, BlueprintTaskClass->GetPathName()))
	{
		return false;
	}

	// TaskClass controls wrapper instance type. Refresh instance containers after assignment.
	InOutNode.Instance.Reset();
	InOutNode.InstanceObject = nullptr;
	if (const FStateTreeNodeBase* NodeBase = InOutNode.Node.GetPtr<FStateTreeNodeBase>())
	{
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(NodeBase->GetInstanceDataType()))
		{
			InOutNode.Instance.InitializeAs(InstanceType);
		}
		else if (const UClass* InstanceClass = Cast<const UClass>(NodeBase->GetInstanceDataType()))
		{
			InOutNode.InstanceObject = NewObject<UObject>(Outer, InstanceClass);
		}
	}

	return InOutNode.Instance.IsValid() || InOutNode.InstanceObject != nullptr;
}

static void AppendBlueprintTaskTypes(TArray<FString>& InOutResults)
{
	TSet<FString> UniqueTypes(InOutResults);
	const UClass* BlueprintTaskBaseClass = UStateTreeTaskBlueprintBase::StaticClass();
	if (!BlueprintTaskBaseClass)
	{
		return;
	}

	auto AddTypeNameIfBlueprintTask = [&UniqueTypes](UClass* CandidateClass)
	{
		if (IsValidBlueprintTaskClass(CandidateClass))
		{
			UniqueTypes.Add(CandidateClass->GetName());
		}
	};

	for (TObjectIterator<UClass> It; It; ++It)
	{
		AddTypeNameIfBlueprintTask(*It);
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(TEXT("/Game")));
	Filter.bRecursivePaths = true;

	TArray<FAssetData> BlueprintAssets;
	AssetRegistry.Get().GetAssets(Filter, BlueprintAssets);

	for (const FAssetData& AssetData : BlueprintAssets)
	{
		const FAssetTagValueRef GeneratedClassTag = AssetData.TagsAndValues.FindTag(TEXT("GeneratedClass"));
		if (!GeneratedClassTag.IsSet())
		{
			continue;
		}

		const FString GeneratedClassObjectPath = FPackageName::ExportTextPathToObjectPath(GeneratedClassTag.GetValue());
		if (GeneratedClassObjectPath.IsEmpty())
		{
			continue;
		}

		if (UClass* GeneratedClass = LoadObject<UClass>(nullptr, *GeneratedClassObjectPath))
		{
			AddTypeNameIfBlueprintTask(GeneratedClass);
		}
	}

	InOutResults = UniqueTypes.Array();
	InOutResults.Sort();
}

/**
 * Initialize an FStateTreeEditorNode from an FStateTreeNodeBase-derived struct type.
 */
static bool InitEditorNodeFromStruct(FStateTreeEditorNode& OutNode, UScriptStruct* NodeStruct, UObject* Outer = nullptr)
{
	if (!NodeStruct)
	{
		return false;
	}

	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
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
		else if (const UClass* InstanceClass = Cast<const UClass>(NodeBase->GetInstanceDataType()))
		{
			OutNode.InstanceObject = NewObject<UObject>(Outer, InstanceClass);
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

static FString OperandToString(EStateTreeExpressionOperand Operand);
static FString TasksCompletionTypeToString(EStateTreeTaskCompletionType Type);

static FStateTreeNodeInfo NodeInfoFromEditorNode(const FStateTreeEditorNode& EditorNode)
{
	FStateTreeNodeInfo Info;
	Info.Name = EditorNode.GetName().ToString();
	Info.Operand = OperandToString(EditorNode.ExpressionOperand);
	if (EditorNode.Node.IsValid())
	{
		Info.StructType = EditorNode.Node.GetScriptStruct()->GetName();
		if (const FStateTreeTaskBase* TaskBase = EditorNode.Node.GetPtr<FStateTreeTaskBase>())
		{
			Info.bEnabled = TaskBase->bTaskEnabled;
#if WITH_EDITORONLY_DATA
			Info.bConsideredForCompletion = TaskBase->bConsideredForCompletion;
#endif
		}
	}
	return Info;
}

static FStateTreeTransitionInfo TransitionInfoFromTransition(const FStateTreeTransition& Transition, int32 Index)
{
	FStateTreeTransitionInfo Info;
	Info.Index = Index;
	Info.Trigger = TransitionTriggerToString(Transition.Trigger);
	Info.Priority = PriorityToString(Transition.Priority);
	Info.bEnabled = Transition.bTransitionEnabled;
	Info.bDelayTransition = Transition.bDelayTransition;
	Info.DelayDuration = Transition.DelayDuration;
	Info.DelayRandomVariance = Transition.DelayRandomVariance;

	if (Transition.RequiredEvent.Tag.IsValid())
	{
		Info.RequiredEventTag = Transition.RequiredEvent.Tag.ToString();
	}
	if (Transition.RequiredEvent.PayloadStruct)
	{
		Info.EventPayloadStruct = Transition.RequiredEvent.PayloadStruct->GetName();
	}

#if WITH_EDITORONLY_DATA
	Info.TransitionType = TransitionTypeToString(Transition.State.LinkType);
	Info.TargetStateName = Transition.State.Name.ToString();
	Info.TargetStatePath = Transition.State.Name.ToString();

	for (int32 i = 0; i < Transition.Conditions.Num(); ++i)
	{
		Info.Conditions.Add(NodeInfoFromEditorNode(Transition.Conditions[i]));
		Info.ConditionOperands.Add(OperandToString(Transition.Conditions[i].ExpressionOperand));
	}
#endif

	return Info;
}

static void CollectStateInfo(const UStateTreeState* State, TArray<FStateTreeStateInfo>& OutStates,
                            const UStateTreeEditorData* EditorData = nullptr)
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
	Info.bExpanded = State->bExpanded;
	Info.Description = State->Description;
	Info.Tag = State->Tag.ToString();
	Info.Weight = State->Weight;
	Info.TasksCompletion = TasksCompletionTypeToString(State->TasksCompletion);
	Info.bHasCustomTickRate = State->bHasCustomTickRate;
	Info.CustomTickRate = State->CustomTickRate;
	if (State->RequiredEventToEnter.Tag.IsValid())
	{
		Info.RequiredEventTag = State->RequiredEventToEnter.Tag.ToString();
	}

	// Resolve theme color display name from ColorRef
	if (EditorData && State->ColorRef.ID.IsValid())
	{
		if (const FStateTreeEditorColor* FoundColor = EditorData->FindColor(State->ColorRef))
		{
			Info.ThemeColor = FoundColor->DisplayName;
		}
	}

	for (const FStateTreeEditorNode& TaskNode : State->Tasks)
	{
		Info.Tasks.Add(NodeInfoFromEditorNode(TaskNode));
	}

	for (const FStateTreeEditorNode& CondNode : State->EnterConditions)
	{
		Info.EnterConditions.Add(NodeInfoFromEditorNode(CondNode));
	}

	for (const FStateTreeEditorNode& CondNode : State->EnterConditions)
	{
		Info.EnterConditionOperands.Add(OperandToString(CondNode.ExpressionOperand));
	}

	for (int32 TransIdx = 0; TransIdx < State->Transitions.Num(); ++TransIdx)
	{
		Info.Transitions.Add(TransitionInfoFromTransition(State->Transitions[TransIdx], TransIdx));
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
		CollectStateInfo(Child, OutStates, EditorData);
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

static EStateTreeStateSelectionBehavior StringToSelectionBehavior(const FString& Str)
{
	if (Str == TEXT("None"))                                            return EStateTreeStateSelectionBehavior::None;
	if (Str == TEXT("TryEnterState"))                                   return EStateTreeStateSelectionBehavior::TryEnterState;
	if (Str == TEXT("TrySelectChildrenInOrder"))                        return EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;
	if (Str == TEXT("TrySelectChildrenAtRandom"))                       return EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandom;
	if (Str == TEXT("TrySelectChildrenWithHighestUtility"))             return EStateTreeStateSelectionBehavior::TrySelectChildrenWithHighestUtility;
	if (Str == TEXT("TrySelectChildrenAtRandomWeightedByUtility"))      return EStateTreeStateSelectionBehavior::TrySelectChildrenAtRandomWeightedByUtility;
	if (Str == TEXT("TryFollowTransitions"))                            return EStateTreeStateSelectionBehavior::TryFollowTransitions;
	return EStateTreeStateSelectionBehavior::TrySelectChildrenInOrder;
}

static FString TasksCompletionTypeToString(EStateTreeTaskCompletionType Type)
{
	switch (Type)
	{
	case EStateTreeTaskCompletionType::Any: return TEXT("Any");
	case EStateTreeTaskCompletionType::All: return TEXT("All");
	default:                                return TEXT("Any");
	}
}

static EStateTreeTaskCompletionType StringToTasksCompletionType(const FString& Str)
{
	if (Str == TEXT("All")) return EStateTreeTaskCompletionType::All;
	return EStateTreeTaskCompletionType::Any;
}

static FString OperandToString(EStateTreeExpressionOperand Operand)
{
	switch (Operand)
	{
	case EStateTreeExpressionOperand::Copy:     return TEXT("Copy");
	case EStateTreeExpressionOperand::And:      return TEXT("And");
	case EStateTreeExpressionOperand::Or:       return TEXT("Or");
	default:                                     return TEXT("And");
	}
}

static EStateTreeExpressionOperand StringToOperand(const FString& Str)
{
	if (Str == TEXT("Copy")) return EStateTreeExpressionOperand::Copy;
	if (Str == TEXT("Or"))   return EStateTreeExpressionOperand::Or;
	return EStateTreeExpressionOperand::And;
}

static FString PropertyBagTypeToString(EPropertyBagPropertyType Type)
{
	switch (Type)
	{
	case EPropertyBagPropertyType::Bool:       return TEXT("Bool");
	case EPropertyBagPropertyType::Byte:       return TEXT("Byte");
	case EPropertyBagPropertyType::Int32:      return TEXT("Int32");
	case EPropertyBagPropertyType::Int64:      return TEXT("Int64");
	case EPropertyBagPropertyType::Float:      return TEXT("Float");
	case EPropertyBagPropertyType::Double:     return TEXT("Double");
	case EPropertyBagPropertyType::Name:       return TEXT("Name");
	case EPropertyBagPropertyType::String:     return TEXT("String");
	case EPropertyBagPropertyType::Text:       return TEXT("Text");
	case EPropertyBagPropertyType::Enum:       return TEXT("Enum");
	case EPropertyBagPropertyType::Struct:     return TEXT("Struct");
	case EPropertyBagPropertyType::Object:     return TEXT("Object");
	case EPropertyBagPropertyType::SoftObject: return TEXT("SoftObject");
	case EPropertyBagPropertyType::Class:      return TEXT("Class");
	case EPropertyBagPropertyType::SoftClass:  return TEXT("SoftClass");
	default:                                    return TEXT("None");
	}
}

static EPropertyBagPropertyType StringToPropertyBagType(const FString& TypeStr)
{
	if (TypeStr == TEXT("Bool"))       return EPropertyBagPropertyType::Bool;
	if (TypeStr == TEXT("Byte"))       return EPropertyBagPropertyType::Byte;
	if (TypeStr == TEXT("Int32"))      return EPropertyBagPropertyType::Int32;
	if (TypeStr == TEXT("Int64"))      return EPropertyBagPropertyType::Int64;
	if (TypeStr == TEXT("Float"))      return EPropertyBagPropertyType::Float;
	if (TypeStr == TEXT("Double"))     return EPropertyBagPropertyType::Double;
	if (TypeStr == TEXT("Name"))       return EPropertyBagPropertyType::Name;
	if (TypeStr == TEXT("String"))     return EPropertyBagPropertyType::String;
	if (TypeStr == TEXT("Text"))       return EPropertyBagPropertyType::Text;
	if (TypeStr == TEXT("Enum"))       return EPropertyBagPropertyType::Enum;
	if (TypeStr == TEXT("Struct"))     return EPropertyBagPropertyType::Struct;
	if (TypeStr == TEXT("Object"))     return EPropertyBagPropertyType::Object;
	if (TypeStr == TEXT("SoftObject")) return EPropertyBagPropertyType::SoftObject;
	if (TypeStr == TEXT("Class"))      return EPropertyBagPropertyType::Class;
	if (TypeStr == TEXT("SoftClass"))  return EPropertyBagPropertyType::SoftClass;
	return EPropertyBagPropertyType::None;
}

/** Generic: find a node by struct name in any TArray<FStateTreeEditorNode>. */
static FStateTreeEditorNode* FindEditorNodeByStructInArray(TArray<FStateTreeEditorNode>& Nodes,
	const FString& StructName, int32 MatchIndex, int32* OutResolvedMatchIndex = nullptr)
{
	TArray<int32> MatchingIndices;
	for (int32 i = 0; i < Nodes.Num(); ++i)
	{
		const UScriptStruct* NodeStruct = Nodes[i].Node.GetScriptStruct();
		if (StructNameMatches(NodeStruct, StructName)
			|| BlueprintWrapperMatchesName(Nodes[i], StructName))
		{
			MatchingIndices.Add(i);
		}
	}

	if (MatchingIndices.IsEmpty())
	{
		return nullptr;
	}

	const int32 SelectedMatchIndex = (MatchIndex < 0) ? (MatchingIndices.Num() - 1) : MatchIndex;
	if (!MatchingIndices.IsValidIndex(SelectedMatchIndex))
	{
		return nullptr;
	}

	if (OutResolvedMatchIndex)
	{
		*OutResolvedMatchIndex = SelectedMatchIndex;
	}
	return &Nodes[MatchingIndices[SelectedMatchIndex]];
}

/** Get and set default value from FInstancedPropertyBag by property name string. */
static FString ExportPropertyBagValue(const FInstancedPropertyBag& Bag, const FName& PropName)
{
	if (!Bag.IsValid())
	{
		return FString();
	}
	const UPropertyBag* BagStruct = Bag.GetPropertyBagStruct();
	if (!BagStruct)
	{
		return FString();
	}
	FProperty* Prop = BagStruct->FindPropertyByName(PropName);
	if (!Prop)
	{
		return FString();
	}
	FConstStructView View = Bag.GetValue();
	const void* Memory = View.GetMemory();
	if (!Memory)
	{
		return FString();
	}
	const void* PropValue = Prop->ContainerPtrToValuePtr<void>(const_cast<void*>(static_cast<const void*>(Memory)));
	FString Exported;
	Prop->ExportText_Direct(Exported, PropValue, PropValue, nullptr, PPF_None);
	return Exported;
}

static bool SetPropertyBagValueFromString(FInstancedPropertyBag& Bag, const FName& PropName,
	EPropertyBagPropertyType BagType, const FString& Value)
{
	if (Value.IsEmpty())
	{
		return true; // nothing to set
	}

	switch (BagType)
	{
	case EPropertyBagPropertyType::Bool:
		{
			bool bVal = false;
			if (!ParseBoolString(Value, bVal)) return false;
			return Bag.SetValueBool(PropName, bVal) == EPropertyBagResult::Success;
		}
	case EPropertyBagPropertyType::Int32:
		return Bag.SetValueInt32(PropName, FCString::Atoi(*Value)) == EPropertyBagResult::Success;
	case EPropertyBagPropertyType::Int64:
		return Bag.SetValueInt64(PropName, FCString::Atoi64(*Value)) == EPropertyBagResult::Success;
	case EPropertyBagPropertyType::Float:
		return Bag.SetValueFloat(PropName, FCString::Atof(*Value)) == EPropertyBagResult::Success;
	case EPropertyBagPropertyType::Double:
		return Bag.SetValueDouble(PropName, FCString::Atod(*Value)) == EPropertyBagResult::Success;
	case EPropertyBagPropertyType::Name:
		return Bag.SetValueName(PropName, FName(*Value)) == EPropertyBagResult::Success;
	case EPropertyBagPropertyType::String:
		return Bag.SetValueString(PropName, Value) == EPropertyBagResult::Success;
	case EPropertyBagPropertyType::Text:
		return Bag.SetValueText(PropName, FText::FromString(Value)) == EPropertyBagResult::Success;
	default:
		{
			// For other types, try raw property import
			if (!Bag.IsValid()) return false;
			const UPropertyBag* BagStruct = Bag.GetPropertyBagStruct();
			if (!BagStruct) return false;
			FProperty* Prop = BagStruct->FindPropertyByName(PropName);
			if (!Prop) return false;
			// Need mutable memory — const_cast is safe for writing
			FConstStructView View = Bag.GetValue();
			void* Memory = const_cast<void*>(static_cast<const void*>(View.GetMemory()));
			if (!Memory) return false;
			void* PropValue = Prop->ContainerPtrToValuePtr<void>(Memory);
			return SetPropertyValueFromString(Prop, PropValue, Value);
		}
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

		// Report context actor class so the AI can see if it needs to be set
		FClassProperty* CtxProp = FindFProperty<FClassProperty>(Schema->GetClass(), TEXT("ContextActorClass"));
		if (CtxProp)
		{
			UClass* CtxClass = Cast<UClass>(CtxProp->GetPropertyValue_InContainer(Schema));
			if (CtxClass)
			{
				OutInfo.ContextActorClass = CtxClass->GetPathName();
			}
		}
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);

	// If runtime schema was null (uncompiled tree), try the editor schema instead
	if (OutInfo.SchemaClass.IsEmpty() && EditorData && EditorData->Schema)
	{
		const UStateTreeSchema* EdSchema = EditorData->Schema;
		OutInfo.SchemaClass = EdSchema->GetClass()->GetName();

		FClassProperty* CtxProp = FindFProperty<FClassProperty>(EdSchema->GetClass(), TEXT("ContextActorClass"));
		if (CtxProp)
		{
			UClass* CtxClass = Cast<UClass>(CtxProp->GetPropertyValue_InContainer(EdSchema));
			if (CtxClass)
			{
				OutInfo.ContextActorClass = CtxClass->GetPathName();
			}
		}
	}

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
			CollectStateInfo(SubTree, OutInfo.AllStates, EditorData);
		}

		// Populate root parameters
		const FInstancedPropertyBag& RootBag = EditorData->GetRootParametersPropertyBag();
		if (const UPropertyBag* BagStruct = RootBag.GetPropertyBagStruct())
		for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
		{
			FStateTreeParameterInfo ParamInfo;
			ParamInfo.Name = Desc.Name.ToString();
			ParamInfo.Type = PropertyBagTypeToString(Desc.ValueType);
			ParamInfo.DefaultValue = ExportPropertyBagValue(RootBag, Desc.Name);
			OutInfo.RootParameters.Add(ParamInfo);
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

	AppendBlueprintTaskTypes(Results);
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
		if (It->IsChildOf(UStateTreeSchema::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
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
			if (It->IsChildOf(UStateTreeSchema::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
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

TArray<FStateTreeThemeColorInfo> UStateTreeService::GetThemeColors(const FString& AssetPath)
{
	TArray<FStateTreeThemeColorInfo> Result;

	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("GetThemeColors: Failed to load StateTree: %s"), *AssetPath);
		return Result;
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData)
	{
		return Result;
	}

	// Build result array from the global color table
	TMap<FGuid, int32> ColorRefToIndex;
	for (const FStateTreeEditorColor& ColorEntry : EditorData->Colors)
	{
		FStateTreeThemeColorInfo Info;
		Info.DisplayName = ColorEntry.DisplayName;
		Info.Color = ColorEntry.Color;
		int32 Idx = Result.Add(Info);
		ColorRefToIndex.Add(ColorEntry.ColorRef.ID, Idx);
	}

	// Walk all states to populate UsedByStates
	TArray<FStateTreeStateInfo> AllStates;
	for (const UStateTreeState* SubTree : EditorData->SubTrees)
	{
		CollectStateInfo(SubTree, AllStates, EditorData);
	}
	for (const FStateTreeStateInfo& StateInfo : AllStates)
	{
		if (!StateInfo.ThemeColor.IsEmpty())
		{
			for (int32 i = 0; i < Result.Num(); ++i)
			{
				if (Result[i].DisplayName.Equals(StateInfo.ThemeColor, ESearchCase::IgnoreCase))
				{
					Result[i].UsedByStates.Add(StateInfo.Path);
					break;
				}
			}
		}
	}
#endif

	return Result;
}

bool UStateTreeService::SetStateExpanded(const FString& AssetPath, const FString& StatePath, bool bExpanded)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetStateExpanded: Failed to load StateTree: %s"), *AssetPath);
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
		UE_LOG(LogStateTreeService, Warning, TEXT("SetStateExpanded: State not found: %s"), *StatePath);
		return false;
	}

	State->bExpanded = bExpanded;
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::SelectState(const FString& AssetPath, const FString& StatePath)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SelectState: Failed to load StateTree: %s"), *AssetPath);
		return false;
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SelectState: No editor data for %s"), *AssetPath);
		return false;
	}

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SelectState: State not found: %s"), *StatePath);
		return false;
	}

	UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>();
	if (!EditingSubsystem)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SelectState: UStateTreeEditingSubsystem not available"));
		return false;
	}

	TSharedRef<FStateTreeViewModel> ViewModel = EditingSubsystem->FindOrAddViewModel(StateTree);
	ViewModel->SetSelection(State);
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

	// Notify schema about the property change so it rebuilds context data descriptors.
	// This mirrors what the editor Details panel does — PostEditChangeChainProperty
	// triggers the schema to sync ContextDataDescs[0].Struct from ContextActorClass.
#if WITH_EDITOR
	{
		FEditPropertyChain PropertyChain;
		PropertyChain.AddHead(ContextActorClassProperty);
		FPropertyChangedEvent InnerEvent(ContextActorClassProperty, EPropertyChangeType::ValueSet);
		FPropertyChangedChainEvent ChainEvent(PropertyChain, InnerEvent);
		EditorData->Schema->PostEditChangeChainProperty(ChainEvent);
	}
#else
	// Fallback: manual sync for non-editor builds
	TConstArrayView<FStateTreeExternalDataDesc> ContextDescs = EditorData->Schema->GetContextDataDescs();
	if (!ContextDescs.IsEmpty())
	{
		FStateTreeExternalDataDesc& MutableDesc = const_cast<FStateTreeExternalDataDesc&>(ContextDescs[0]);
		MutableDesc.Struct = ActorClass;
	}
#endif

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

TArray<FStateTreeParameterInfo> UStateTreeService::GetRootParameters(const FString& AssetPath)
{
	TArray<FStateTreeParameterInfo> Result;

#if WITH_EDITORONLY_DATA
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return Result; }

	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return Result; }

	const FInstancedPropertyBag& RootBag = EditorData->GetRootParametersPropertyBag();
	if (const UPropertyBag* BagStruct = RootBag.GetPropertyBagStruct())
	for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
	{
		FStateTreeParameterInfo ParamInfo;
		ParamInfo.Name = Desc.Name.ToString();
		ParamInfo.Type = PropertyBagTypeToString(Desc.ValueType);
		ParamInfo.DefaultValue = ExportPropertyBagValue(RootBag, Desc.Name);
		Result.Add(ParamInfo);
	}
#endif

	return Result;
}

bool UStateTreeService::AddOrUpdateRootParameter(const FString& AssetPath, const FString& Name,
	const FString& Type, const FString& DefaultValue)
{
	if (Name.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddOrUpdateRootParameter: Name is empty"));
		return false;
	}

	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	const EPropertyBagPropertyType BagType = StringToPropertyBagType(Type);
	if (BagType == EPropertyBagPropertyType::None)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddOrUpdateRootParameter: Unknown type '%s'"), *Type);
		return false;
	}

	FInstancedPropertyBag& Bag = const_cast<FInstancedPropertyBag&>(EditorData->GetRootParametersPropertyBag());
	const FName ParamFName(*Name);

	const FPropertyBagPropertyDesc* ExistingDesc = Bag.FindPropertyDescByName(ParamFName);
	if (!ExistingDesc || ExistingDesc->ValueType != BagType)
	{
		if (Bag.AddProperty(ParamFName, BagType, nullptr, true) != EPropertyBagAlterationResult::Success)
		{
			UE_LOG(LogStateTreeService, Warning, TEXT("AddOrUpdateRootParameter: Failed to add parameter '%s'"), *Name);
			return false;
		}
	}

	SetPropertyBagValueFromString(Bag, ParamFName, BagType, DefaultValue);

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::RemoveRootParameter(const FString& AssetPath, const FString& Name)
{
	if (Name.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveRootParameter: Name is empty"));
		return false;
	}

	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	FInstancedPropertyBag& Bag = const_cast<FInstancedPropertyBag&>(EditorData->GetRootParametersPropertyBag());
	const FName ParamFName(*Name);

	if (!Bag.FindPropertyDescByName(ParamFName))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveRootParameter: Parameter '%s' not found"), *Name);
		return false;
	}

	if (Bag.RemovePropertyByName(ParamFName) != EPropertyBagAlterationResult::Success)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveRootParameter: Failed to remove parameter '%s'"), *Name);
		return false;
	}

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::RenameRootParameter(const FString& AssetPath, const FString& OldName, const FString& NewName)
{
	if (OldName.IsEmpty() || NewName.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RenameRootParameter: OldName and NewName must not be empty"));
		return false;
	}

	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	FInstancedPropertyBag& Bag = const_cast<FInstancedPropertyBag&>(EditorData->GetRootParametersPropertyBag());
	const FName OldFName(*OldName);
	const FName NewFName(*NewName);

	const FPropertyBagPropertyDesc* ExistingDesc = Bag.FindPropertyDescByName(OldFName);
	if (!ExistingDesc)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RenameRootParameter: Parameter '%s' not found"), *OldName);
		return false;
	}

	// Rename by reading the type + value, removing the old, adding under new name
	const EPropertyBagPropertyType BagType = ExistingDesc->ValueType;
	const FString CurrentValue = ExportPropertyBagValue(Bag, OldFName);

	if (Bag.RemovePropertyByName(OldFName) != EPropertyBagAlterationResult::Success)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RenameRootParameter: Failed to remove old parameter '%s'"), *OldName);
		return false;
	}

	if (Bag.AddProperty(NewFName, BagType, nullptr, true) != EPropertyBagAlterationResult::Success)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RenameRootParameter: Failed to add renamed parameter '%s'"), *NewName);
		return false;
	}

	SetPropertyBagValueFromString(Bag, NewFName, BagType, CurrentValue);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

// ============================================================
// Component-level parameter overrides (level actor instances)
// ============================================================

namespace
{
	/** Find UStateTreeComponent class dynamically — avoids requiring GameplayStateTreeModule as a hard header dep. */
	static UClass* GetStateTreeComponentClass()
	{
		static UClass* Cached = nullptr;
		if (!Cached)
		{
			Cached = FindObject<UClass>(nullptr, TEXT("/Script/GameplayStateTreeModule.StateTreeComponent"));
		}
		return Cached;
	}

	/** Access the FStateTreeReference on a component via reflection (the property is protected). */
	static FStateTreeReference* GetStateTreeRefFromComp(UActorComponent* Comp)
	{
		FStructProperty* Prop = FindFProperty<FStructProperty>(Comp->GetClass(), TEXT("StateTreeRef"));
		if (!Prop || Prop->Struct != FStateTreeReference::StaticStruct())
		{
			return nullptr;
		}
		return Prop->ContainerPtrToValuePtr<FStateTreeReference>(Comp);
	}
} // anonymous namespace

FString UStateTreeService::GetComponentStateTreePath(const FString& ActorNameOrLabel)
{
	AActor* Actor = UActorService::FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor) { return FString(); }

	UClass* STCompClass = GetStateTreeComponentClass();
	if (!STCompClass) { return FString(); }

	UActorComponent* Comp = Actor->GetComponentByClass(STCompClass);
	if (!Comp) { return FString(); }

	FStateTreeReference* STRef = GetStateTreeRefFromComp(Comp);
	if (!STRef || !STRef->IsValid()) { return FString(); }

	const UStateTree* Tree = STRef->GetStateTree();
	if (!Tree) { return FString(); }

	// Convert object path to game content path (strip _C suffix, convert /Game/... format)
	FString PackagePath = Tree->GetPackage()->GetName();
	return PackagePath;
}

TArray<FStateTreeParameterInfo> UStateTreeService::GetComponentParameterOverrides(const FString& ActorNameOrLabel)
{
	TArray<FStateTreeParameterInfo> Result;

	AActor* Actor = UActorService::FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("GetComponentParameterOverrides: Actor not found: %s"), *ActorNameOrLabel);
		return Result;
	}

	UClass* STCompClass = GetStateTreeComponentClass();
	if (!STCompClass)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("GetComponentParameterOverrides: StateTreeComponent class not available"));
		return Result;
	}

	UActorComponent* Comp = Actor->GetComponentByClass(STCompClass);
	if (!Comp)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("GetComponentParameterOverrides: Actor '%s' has no StateTreeComponent"), *ActorNameOrLabel);
		return Result;
	}

	FStateTreeReference* STRef = GetStateTreeRefFromComp(Comp);
	if (!STRef || !STRef->IsValid())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("GetComponentParameterOverrides: StateTreeRef not set on actor '%s'"), *ActorNameOrLabel);
		return Result;
	}

	const FInstancedPropertyBag& Bag = STRef->GetParameters();
	const UPropertyBag* BagStruct = Bag.GetPropertyBagStruct();
	if (!BagStruct)
	{
		return Result;
	}

	using namespace UStateTreeServiceHelpers;
	for (const FPropertyBagPropertyDesc& Desc : BagStruct->GetPropertyDescs())
	{
		FStateTreeParameterInfo Info;
		Info.Name = Desc.Name.ToString();
		Info.Type = PropertyBagTypeToString(Desc.ValueType);
		Info.DefaultValue = ExportPropertyBagValue(Bag, Desc.Name);
		Result.Add(Info);
	}
	return Result;
}

bool UStateTreeService::SetComponentParameterOverride(const FString& ActorNameOrLabel,
	const FString& ParameterName, const FString& Value)
{
	if (ParameterName.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetComponentParameterOverride: ParameterName is empty"));
		return false;
	}

	AActor* Actor = UActorService::FindActorByIdentifier(ActorNameOrLabel);
	if (!Actor)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetComponentParameterOverride: Actor not found: %s"), *ActorNameOrLabel);
		return false;
	}

	UClass* STCompClass = GetStateTreeComponentClass();
	if (!STCompClass)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetComponentParameterOverride: StateTreeComponent class not available"));
		return false;
	}

	UActorComponent* Comp = Actor->GetComponentByClass(STCompClass);
	if (!Comp)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetComponentParameterOverride: Actor '%s' has no StateTreeComponent"), *ActorNameOrLabel);
		return false;
	}

	FStateTreeReference* STRef = GetStateTreeRefFromComp(Comp);
	if (!STRef || !STRef->IsValid())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetComponentParameterOverride: StateTreeRef not set on actor '%s'"), *ActorNameOrLabel);
		return false;
	}

#if WITH_EDITORONLY_DATA
	// Resolve the parameter type from the linked StateTree asset's schema
	UStateTree* LinkedTree = STRef->GetMutableStateTree();
	UStateTreeEditorData* EditorData = GetEditorData(LinkedTree);
	if (!EditorData)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetComponentParameterOverride: Could not get editor data for linked StateTree on actor '%s'"), *ActorNameOrLabel);
		return false;
	}

	const FInstancedPropertyBag& AssetBag = EditorData->GetRootParametersPropertyBag();
	const FName ParamFName(*ParameterName);
	const FPropertyBagPropertyDesc* AssetDesc = AssetBag.FindPropertyDescByName(ParamFName);
	if (!AssetDesc)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetComponentParameterOverride: Parameter '%s' not found in linked StateTree '%s'"),
			*ParameterName, *LinkedTree->GetName());
		return false;
	}

	const EPropertyBagPropertyType BagType = AssetDesc->ValueType;
	const FGuid ParamGuid = AssetDesc->ID;  // Needed to mark as overridden

	// Get (or lazily sync) the instance-level parameter bag
	FInstancedPropertyBag& InstanceBag = STRef->GetMutableParameters();

	// Ensure the instance bag has the property — sync with asset schema if stale
	if (!InstanceBag.FindPropertyDescByName(ParamFName))
	{
		STRef->SetStateTree(LinkedTree); // triggers SyncParameters()
		if (!InstanceBag.FindPropertyDescByName(ParamFName))
		{
			UE_LOG(LogStateTreeService, Warning, TEXT("SetComponentParameterOverride: Parameter '%s' not in instance bag after sync"), *ParameterName);
			return false;
		}
	}

	using namespace UStateTreeServiceHelpers;
	if (!SetPropertyBagValueFromString(InstanceBag, ParamFName, BagType, Value))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetComponentParameterOverride: Failed to set '%s' = '%s' on actor '%s'"),
			*ParameterName, *Value, *ActorNameOrLabel);
		return false;
	}

	// IMPORTANT: Mark this parameter as overridden so the instance value takes effect at runtime.
	// Non-overridden parameters inherit from the StateTree asset defaults and ignore the bag value.
	STRef->SetPropertyOverridden(ParamGuid, true);

	// Mark dirty so the override is persisted when the level is saved
	Comp->Modify();
	Actor->Modify();

	UE_LOG(LogStateTreeService, Log, TEXT("SetComponentParameterOverride: '%s' = '%s' on actor '%s'"),
		*ParameterName, *Value, *ActorNameOrLabel);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::SetSelectionBehavior(const FString& AssetPath, const FString& StatePath, const FString& Behavior)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetSelectionBehavior: State not found: %s"), *StatePath);
		return false;
	}

	State->SelectionBehavior = StringToSelectionBehavior(Behavior);
	MarkStateTreeDirty(StateTree);
	UE_LOG(LogStateTreeService, Log, TEXT("SetSelectionBehavior: %s -> '%s' in %s"), *StatePath, *Behavior, *AssetPath);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::SetTasksCompletion(const FString& AssetPath, const FString& StatePath, const FString& Completion)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTasksCompletion: State not found: %s"), *StatePath);
		return false;
	}

	State->TasksCompletion = StringToTasksCompletionType(Completion);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::RenameState(const FString& AssetPath, const FString& StatePath, const FString& NewName)
{
	if (NewName.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RenameState: NewName is empty"));
		return false;
	}

	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RenameState: State not found: %s"), *StatePath);
		return false;
	}

	State->Name = FName(*NewName);
	MarkStateTreeDirty(StateTree);
	UE_LOG(LogStateTreeService, Log, TEXT("RenameState: '%s' renamed to '%s' in %s"), *StatePath, *NewName, *AssetPath);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::SetStateTag(const FString& AssetPath, const FString& StatePath, const FString& GameplayTag)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetStateTag: State not found: %s"), *StatePath);
		return false;
	}

	if (GameplayTag.IsEmpty())
	{
		State->Tag = FGameplayTag();
	}
	else
	{
		State->Tag = FGameplayTag::RequestGameplayTag(FName(*GameplayTag), false);
		if (!State->Tag.IsValid())
		{
			UE_LOG(LogStateTreeService, Warning, TEXT("SetStateTag: Gameplay tag '%s' not found in tag table"), *GameplayTag);
		}
	}

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::SetStateWeight(const FString& AssetPath, const FString& StatePath, float Weight)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetStateWeight: State not found: %s"), *StatePath);
		return false;
	}

	State->Weight = FMath::Max(0.0f, Weight);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::SetTaskConsideredForCompletion(const FString& AssetPath, const FString& StatePath,
	const FString& TaskStructName, int32 TaskMatchIndex, bool bConsideredForCompletion)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTaskConsideredForCompletion: State not found: %s"), *StatePath);
		return false;
	}

	FStateTreeEditorNode* TaskNode = FindTaskNodeByStruct(State, TaskStructName, TaskMatchIndex);
	if (!TaskNode)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTaskConsideredForCompletion: Task '%s' not found in state '%s'"),
			*TaskStructName, *StatePath);
		return false;
	}

	FStateTreeTaskBase* TaskBase = TaskNode->Node.GetMutablePtr<FStateTreeTaskBase>();
	if (!TaskBase)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTaskConsideredForCompletion: Task node is not a FStateTreeTaskBase"));
		return false;
	}

#if WITH_EDITORONLY_DATA
	TaskBase->bConsideredForCompletion = bConsideredForCompletion;
#endif
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::UpdateTransition(const FString& AssetPath, const FString& StatePath, int32 TransitionIndex,
	const FString& Trigger, const FString& TransitionType, const FString& TargetPath, const FString& Priority,
	const FString& EventTag,
	const FString& EventPayloadStruct,
	bool bSetEnabled, bool bEnabled, bool bSetDelay, bool bDelayTransition, float DelayDuration, float DelayRandomVariance)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("UpdateTransition: State not found: %s"), *StatePath);
		return false;
	}

	if (!State->Transitions.IsValidIndex(TransitionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("UpdateTransition: TransitionIndex %d out of range (%d transitions) on state '%s'"),
			TransitionIndex, State->Transitions.Num(), *StatePath);
		return false;
	}

	FStateTreeTransition& Trans = State->Transitions[TransitionIndex];

	if (!Trigger.IsEmpty())
	{
		// Validate trigger string — StringToTransitionTrigger silently falls back to OnStateCompleted
		// for unknown values, so we validate explicitly to avoid silent no-ops.
		static const TSet<FString> ValidTriggers = {
			TEXT("OnStateCompleted"), TEXT("OnStateSucceeded"), TEXT("OnStateFailed"),
			TEXT("OnTick"), TEXT("OnEvent"), TEXT("OnDelegate")
		};
		if (!ValidTriggers.Contains(Trigger))
		{
			UE_LOG(LogStateTreeService, Warning,
				TEXT("UpdateTransition: Unknown trigger '%s'. Valid values: OnStateCompleted, OnStateSucceeded, OnStateFailed, OnTick, OnEvent, OnDelegate"),
				*Trigger);
			return false;
		}
		Trans.Trigger = StringToTransitionTrigger(Trigger);
	}
	if (!TransitionType.IsEmpty())
	{
		const EStateTreeTransitionType NewType = StringToTransitionType(TransitionType);
		Trans.State.LinkType = NewType;

		if (NewType == EStateTreeTransitionType::GotoState && !TargetPath.IsEmpty())
		{
			const UStateTreeState* TargetState = FindStateByPath(EditorData, TargetPath);
			if (!TargetState)
			{
				UE_LOG(LogStateTreeService, Warning, TEXT("UpdateTransition: Target state not found: %s"), *TargetPath);
				return false;
			}
			Trans.State.Name = TargetState->Name;
			Trans.State.ID = TargetState->ID;
		}
	}
	else if (!TargetPath.IsEmpty())
	{
		// target_path provided without transition_type — update the target on an existing GotoState transition
		if (Trans.State.LinkType == EStateTreeTransitionType::GotoState)
		{
			const UStateTreeState* TargetState = FindStateByPath(EditorData, TargetPath);
			if (!TargetState)
			{
				UE_LOG(LogStateTreeService, Warning, TEXT("UpdateTransition: Target state not found: %s"), *TargetPath);
				return false;
			}
			Trans.State.Name = TargetState->Name;
			Trans.State.ID = TargetState->ID;
		}
		else
		{
			UE_LOG(LogStateTreeService, Warning, TEXT("UpdateTransition: target_path provided but transition %d on '%s' is not GotoState — target_path ignored"), TransitionIndex, *StatePath);
		}
	}
	if (!Priority.IsEmpty())
	{
		Trans.Priority = StringToPriority(Priority);
	}
	if (bSetEnabled)
	{
		Trans.bTransitionEnabled = bEnabled;
	}
	if (bSetDelay)
	{
		Trans.bDelayTransition = bDelayTransition;
		Trans.DelayDuration = DelayDuration;
		Trans.DelayRandomVariance = DelayRandomVariance;
	}
	if (!EventTag.IsEmpty())
	{
		const FGameplayTag ParsedTag = FGameplayTag::RequestGameplayTag(FName(*EventTag), /*bErrorIfNotFound=*/false);
		Trans.RequiredEvent.Tag = ParsedTag;
	}
	if (!EventPayloadStruct.IsEmpty())
	{
		if (EventPayloadStruct.Equals(TEXT("None"), ESearchCase::IgnoreCase))
		{
			Trans.RequiredEvent.PayloadStruct = nullptr;
		}
		else
		{
			UScriptStruct* FoundStruct = FindNodeStruct(EventPayloadStruct);
			if (!FoundStruct)
			{
				UE_LOG(LogStateTreeService, Warning, TEXT("UpdateTransition: EventPayloadStruct not found: %s"), *EventPayloadStruct);
				return false;
			}
			Trans.RequiredEvent.PayloadStruct = FoundStruct;
		}
	}

	MarkStateTreeDirty(StateTree);
	UE_LOG(LogStateTreeService, Log, TEXT("UpdateTransition: Updated transition %d on state '%s' in %s"),
		TransitionIndex, *StatePath, *AssetPath);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::BindTransitionToDelegate(const FString& AssetPath, const FString& StatePath,
	int32 TransitionIndex, const FString& TaskStructName, const FString& DispatcherPropertyName, int32 TaskMatchIndex)
{
	if (DispatcherPropertyName.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionToDelegate: DispatcherPropertyName is required"));
		return false;
	}

	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionToDelegate: State not found: %s"), *StatePath);
		return false;
	}

	if (!State->Transitions.IsValidIndex(TransitionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionToDelegate: TransitionIndex %d out of range (%d transitions) on state '%s'"),
			TransitionIndex, State->Transitions.Num(), *StatePath);
		return false;
	}

	FStateTreeTransition& Trans = State->Transitions[TransitionIndex];
	if (Trans.Trigger != EStateTreeTransitionTrigger::OnDelegate)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("BindTransitionToDelegate: Transition %d trigger is '%s', not 'OnDelegate'. Call update_transition() with trigger='OnDelegate' first."),
			TransitionIndex, *TransitionTriggerToString(Trans.Trigger));
		return false;
	}

	FStateTreeEditorNode* TaskNode = FindTaskNodeByStruct(State, TaskStructName, TaskMatchIndex);
	if (!TaskNode)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionToDelegate: Task '%s' not found in state '%s' (match index %d)"),
			*TaskStructName, *StatePath, TaskMatchIndex);
		return false;
	}

	// Source: the FStateTreeDelegateDispatcher property on the task node
	FPropertyBindingPath SourcePath;
	if (!MakeBindingPath(TaskNode->ID, DispatcherPropertyName, SourcePath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionToDelegate: Invalid dispatcher property name: %s"), *DispatcherPropertyName);
		return false;
	}

	// Target: FStateTreeTransition.DelegateListener — the transition's delegate listener slot
	FPropertyBindingPath TargetPath;
	if (!MakeBindingPath(Trans.ID, TEXT("DelegateListener"), TargetPath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionToDelegate: Failed to create DelegateListener path for transition %d"), TransitionIndex);
		return false;
	}

	EditorData->AddPropertyBinding(SourcePath, TargetPath);
	MarkStateTreeDirty(StateTree);
	UE_LOG(LogStateTreeService, Log, TEXT("BindTransitionToDelegate: Bound transition %d on '%s' to '%s.%s'"),
		TransitionIndex, *StatePath, *TaskStructName, *DispatcherPropertyName);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::RemoveTransition(const FString& AssetPath, const FString& StatePath, int32 TransitionIndex)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveTransition: State not found: %s"), *StatePath);
		return false;
	}

	if (!State->Transitions.IsValidIndex(TransitionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveTransition: TransitionIndex %d out of range"), TransitionIndex);
		return false;
	}

	State->Transitions.RemoveAt(TransitionIndex);
	MarkStateTreeDirty(StateTree);
	UE_LOG(LogStateTreeService, Log, TEXT("RemoveTransition: Removed transition %d from state '%s' in %s"),
		TransitionIndex, *StatePath, *AssetPath);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::MoveTransition(const FString& AssetPath, const FString& StatePath, int32 FromIndex, int32 ToIndex)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("MoveTransition: State not found: %s"), *StatePath);
		return false;
	}

	if (!State->Transitions.IsValidIndex(FromIndex) || !State->Transitions.IsValidIndex(ToIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("MoveTransition: FromIndex %d or ToIndex %d out of range (%d transitions)"),
			FromIndex, ToIndex, State->Transitions.Num());
		return false;
	}

	if (FromIndex == ToIndex) { return true; }

	FStateTreeTransition Moved = State->Transitions[FromIndex];
	State->Transitions.RemoveAt(FromIndex);
	const int32 AdjustedToIndex = (ToIndex > FromIndex) ? ToIndex - 1 : ToIndex;
	State->Transitions.Insert(MoveTemp(Moved), AdjustedToIndex);

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
	UClass* BlueprintTaskClass = nullptr;

	if (TaskStruct)
	{
		// Verify struct-backed task derives from FStateTreeTaskBase
		if (!TaskStruct->IsChildOf(FStateTreeTaskBase::StaticStruct()))
		{
			UE_LOG(LogStateTreeService, Warning, TEXT("AddTask: Struct '%s' is not a FStateTreeTaskBase"), *TaskStructName);
			return false;
		}
	}
	else
	{
		BlueprintTaskClass = ResolveBlueprintTaskClass(TaskStructName);
		if (!BlueprintTaskClass)
		{
			UE_LOG(LogStateTreeService, Warning, TEXT("AddTask: Task struct/class not found: %s"), *TaskStructName);
			return false;
		}

		TaskStruct = FindNodeStruct(TEXT("StateTreeBlueprintTaskWrapper"));
		if (!TaskStruct)
		{
			UE_LOG(LogStateTreeService, Warning, TEXT("AddTask: StateTreeBlueprintTaskWrapper struct not found while adding blueprint task '%s'"), *TaskStructName);
			return false;
		}
	}

	FStateTreeEditorNode& NewNode = State->Tasks.AddDefaulted_GetRef();
	if (!InitEditorNodeFromStruct(NewNode, TaskStruct, EditorData))
	{
		State->Tasks.RemoveAt(State->Tasks.Num() - 1);
		return false;
	}

	if (BlueprintTaskClass)
	{
		if (!SetBlueprintTaskClassOnWrapperNode(NewNode, BlueprintTaskClass, EditorData))
		{
			State->Tasks.RemoveAt(State->Tasks.Num() - 1);
			UE_LOG(LogStateTreeService, Warning, TEXT("AddTask: Failed to set blueprint task class '%s' on wrapper"), *BlueprintTaskClass->GetName());
			return false;
		}
	}

	MarkStateTreeDirty(StateTree);
	if (BlueprintTaskClass)
	{
		UE_LOG(LogStateTreeService, Log, TEXT("AddTask: Added blueprint task '%s' to state '%s' in %s"),
		       *BlueprintTaskClass->GetName(), *StatePath, *AssetPath);
	}
	else
	{
		UE_LOG(LogStateTreeService, Log, TEXT("AddTask: Added '%s' to state '%s' in %s"),
		       *TaskStructName, *StatePath, *AssetPath);
	}
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::RemoveTask(const FString& AssetPath, const FString& StatePath,
	const FString& TaskStructName, int32 TaskMatchIndex)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveTask: State not found: %s"), *StatePath);
		return false;
	}

	int32 ResolvedIndex = INDEX_NONE;
	FStateTreeEditorNode* TaskNode = FindTaskNodeByStruct(State, TaskStructName, TaskMatchIndex, &ResolvedIndex);
	if (!TaskNode)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveTask: Task '%s' not found in state '%s'"),
			*TaskStructName, *StatePath);
		return false;
	}

	// Find the actual array index of this node (FindTaskNodeByStruct returns pointer into array)
	const int32 ArrayIndex = static_cast<int32>(TaskNode - State->Tasks.GetData());
	State->Tasks.RemoveAt(ArrayIndex);

	MarkStateTreeDirty(StateTree);
	UE_LOG(LogStateTreeService, Log, TEXT("RemoveTask: Removed '%s' from state '%s' in %s"),
		*TaskStructName, *StatePath, *AssetPath);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::MoveTask(const FString& AssetPath, const FString& StatePath,
	const FString& TaskStructName, int32 TaskMatchIndex, int32 NewIndex)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("MoveTask: State not found: %s"), *StatePath);
		return false;
	}

	FStateTreeEditorNode* TaskNode = FindTaskNodeByStruct(State, TaskStructName, TaskMatchIndex);
	if (!TaskNode)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("MoveTask: Task '%s' not found in state '%s'"),
			*TaskStructName, *StatePath);
		return false;
	}

	const int32 CurrentIndex = static_cast<int32>(TaskNode - State->Tasks.GetData());
	if (!State->Tasks.IsValidIndex(NewIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("MoveTask: NewIndex %d out of range (%d tasks)"),
			NewIndex, State->Tasks.Num());
		return false;
	}

	if (CurrentIndex == NewIndex) { return true; }

	FStateTreeEditorNode Moved = State->Tasks[CurrentIndex];
	State->Tasks.RemoveAt(CurrentIndex);
	const int32 AdjustedIndex = (NewIndex > CurrentIndex) ? NewIndex - 1 : NewIndex;
	State->Tasks.Insert(MoveTemp(Moved), AdjustedIndex);

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::SetTaskEnabled(const FString& AssetPath, const FString& StatePath,
	const FString& TaskStructName, int32 TaskMatchIndex, bool bEnabled)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTaskEnabled: State not found: %s"), *StatePath);
		return false;
	}

	FStateTreeEditorNode* TaskNode = FindTaskNodeByStruct(State, TaskStructName, TaskMatchIndex);
	if (!TaskNode)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTaskEnabled: Task '%s' not found in state '%s'"),
			*TaskStructName, *StatePath);
		return false;
	}

	if (FStateTreeTaskBase* TaskBase = TaskNode->Node.GetMutablePtr<FStateTreeTaskBase>())
	{
		TaskBase->bTaskEnabled = bEnabled;
	}
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::SetTaskPropertyValue(const FString& AssetPath, const FString& StatePath,
	const FString& TaskStructName, const FString& PropertyPath, const FString& Value, int32 TaskMatchIndex)
{
	return SetTaskPropertyValueDetailed(AssetPath, StatePath, TaskStructName, PropertyPath, Value, TaskMatchIndex).bSuccess;
}

FStateTreeTaskPropertySetResult UStateTreeService::SetTaskPropertyValueDetailed(const FString& AssetPath,
	const FString& StatePath, const FString& TaskStructName, const FString& PropertyPath,
	const FString& Value, int32 TaskMatchIndex)
{
	FStateTreeTaskPropertySetResult Result;

	auto Fail = [&Result](const FString& Message, int32 ResolvedTaskMatchIndex = INDEX_NONE)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = Message;
		Result.ResolvedTaskMatchIndex = ResolvedTaskMatchIndex;
		return Result;
	};

	if (PropertyPath.IsEmpty())
	{
		const FString Message = TEXT("SetTaskPropertyValueDetailed: PropertyPath is empty");
		UE_LOG(LogStateTreeService, Warning, TEXT("%s"), *Message);
		return Fail(Message);
	}

	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree)
	{
		return Fail(FString::Printf(TEXT("SetTaskPropertyValueDetailed: StateTree asset not found: %s"), *AssetPath));
	}

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData)
	{
		return Fail(FString::Printf(TEXT("SetTaskPropertyValueDetailed: Editor data unavailable for %s"), *AssetPath));
	}

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		const FString Message = FString::Printf(TEXT("SetTaskPropertyValueDetailed: State not found: %s"), *StatePath);
		UE_LOG(LogStateTreeService, Warning, TEXT("%s"), *Message);
		return Fail(Message);
	}

	int32 ResolvedTaskMatchIndex = INDEX_NONE;
	FStateTreeEditorNode* TaskNode = FindTaskNodeByStruct(State, TaskStructName, TaskMatchIndex, &ResolvedTaskMatchIndex);
	if (!TaskNode)
	{
		const FString Message = FString::Printf(
			TEXT("SetTaskPropertyValueDetailed: Task not found in %s for struct '%s' at match index %d"),
			*StatePath, *TaskStructName, TaskMatchIndex);
		UE_LOG(LogStateTreeService, Warning, TEXT("%s"), *Message);
		return Fail(Message);
	}

	Result.ResolvedTaskMatchIndex = ResolvedTaskMatchIndex;

	FProperty* Property = nullptr;
	void* PropertyValuePtr = nullptr;
	if (!ResolveTaskPropertyPath(TaskNode, PropertyPath, Property, PropertyValuePtr))
	{
		const FString Message = FString::Printf(
			TEXT("SetTaskPropertyValueDetailed: Could not resolve property path '%s' on '%s'. The property may live on the task node struct or its instance data. Call get_task_property_names to inspect valid paths."),
			*PropertyPath, *TaskStructName);
		UE_LOG(LogStateTreeService, Warning, TEXT("%s"), *Message);
		return Fail(Message, ResolvedTaskMatchIndex);
	}

	Result.PropertyType = Property->GetCPPType();
	Property->ExportText_Direct(Result.PreviousValue, PropertyValuePtr, PropertyValuePtr, nullptr, PPF_None);

	if (!SetPropertyValueFromString(Property, PropertyValuePtr, Value))
	{
		const FString Message = FString::Printf(
			TEXT("SetTaskPropertyValueDetailed: Failed to set property '%s' with value '%s'"),
			*PropertyPath, *Value);
		UE_LOG(LogStateTreeService, Warning, TEXT("%s"), *Message);
		return Fail(Message, ResolvedTaskMatchIndex);
	}

	Property->ExportText_Direct(Result.NewValue, PropertyValuePtr, PropertyValuePtr, nullptr, PPF_None);
	MarkStateTreeDirty(StateTree);
	Result.bSuccess = true;
	return Result;
#else
	return Fail(TEXT("SetTaskPropertyValueDetailed: StateTree editing requires WITH_EDITORONLY_DATA"));
#endif
}

TArray<FStateTreePropertyInfo> UStateTreeService::GetTaskPropertyNames(const FString& AssetPath,
	const FString& StatePath, const FString& TaskStructName, int32 TaskMatchIndex)
{
	TArray<FStateTreePropertyInfo> Result;

#if WITH_EDITORONLY_DATA
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return Result; }

	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return Result; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("GetTaskPropertyNames: State not found: %s"), *StatePath);
		return Result;
	}

	FStateTreeEditorNode* TaskNode = FindTaskNodeByStruct(State, TaskStructName, TaskMatchIndex);
	if (!TaskNode)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("GetTaskPropertyNames: Task '%s' not found in state '%s'"),
			*TaskStructName, *StatePath);
		return Result;
	}

	TSet<FString> SeenPropertyPaths;

	const UStruct* NodeStruct = nullptr;
	void* NodeMemory = nullptr;
	if (GetTaskNodeData(TaskNode, NodeStruct, NodeMemory))
	{
		AppendEditableProperties(NodeStruct, NodeMemory, Result, &SeenPropertyPaths);
	}

	const UStruct* InstanceStruct = nullptr;
	void* InstanceMemory = nullptr;
	if (GetTaskInstanceData(TaskNode, InstanceStruct, InstanceMemory))
	{
		AppendEditableProperties(InstanceStruct, InstanceMemory, Result, &SeenPropertyPaths);
	}

	UE_LOG(LogStateTreeService, Log, TEXT("GetTaskPropertyNames: Found %d editable properties on task '%s'"),
		Result.Num(), *TaskStructName);
#endif

	return Result;
}

FString UStateTreeService::GetTaskPropertyValue(const FString& AssetPath, const FString& StatePath,
	const FString& TaskStructName, const FString& PropertyPath, int32 TaskMatchIndex)
{
#if WITH_EDITORONLY_DATA
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return FString(); }

	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return FString(); }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State) { return FString(); }

	FStateTreeEditorNode* TaskNode = FindTaskNodeByStruct(State, TaskStructName, TaskMatchIndex);
	if (!TaskNode) { return FString(); }

	FProperty* Property = nullptr;
	void* PropertyValuePtr = nullptr;
	if (!ResolveTaskPropertyPath(TaskNode, PropertyPath, Property, PropertyValuePtr))
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("GetTaskPropertyValue: Could not resolve property path '%s'"), *PropertyPath);
		return FString();
	}

	FString ExportedValue;
	Property->ExportText_Direct(ExportedValue, PropertyValuePtr, PropertyValuePtr, nullptr, PPF_None);
	return ExportedValue;
#else
	return FString();
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
		const TConstArrayView<FStateTreeExternalDataDesc> CtxDescs = StateTree->GetSchema()->GetContextDataDescs();
		if (CtxDescs.IsEmpty())
		{
			UE_LOG(LogStateTreeService, Warning,
				TEXT("BindTaskPropertyToContext: Context '%s' not found — this StateTree has NO context actor class set. "
				     "Call set_context_actor_class() first to assign one, then retry the bind."),
				*ContextName);
		}
		else
		{
			FString Available;
			for (const FStateTreeExternalDataDesc& Desc : CtxDescs)
			{
				if (!Available.IsEmpty()) Available += TEXT(", ");
				Available += Desc.Name.ToString();
			}
			UE_LOG(LogStateTreeService, Warning,
				TEXT("BindTaskPropertyToContext: Context '%s' not found. Available contexts: [%s]"),
				*ContextName, *Available);
		}
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

bool UStateTreeService::UnbindTaskProperty(const FString& AssetPath, const FString& StatePath,
	const FString& TaskStructName, const FString& TaskPropertyPath, int32 TaskMatchIndex)
{
	if (TaskPropertyPath.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("UnbindTaskProperty: TaskPropertyPath is required"));
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
		UE_LOG(LogStateTreeService, Warning, TEXT("UnbindTaskProperty: State not found: %s"), *StatePath);
		return false;
	}

	FStateTreeEditorNode* TaskNode = FindTaskNodeByStruct(State, TaskStructName, TaskMatchIndex);
	if (!TaskNode)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("UnbindTaskProperty: Task not found in %s for struct '%s' at match index %d"),
			*StatePath, *TaskStructName, TaskMatchIndex);
		return false;
	}

	FPropertyBindingPath TargetPath;
	if (!MakeBindingPath(TaskNode->ID, TaskPropertyPath, TargetPath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("UnbindTaskProperty: Invalid task property path: %s"), *TaskPropertyPath);
		return false;
	}

	FStateTreeEditorPropertyBindings* Bindings = EditorData->GetPropertyEditorBindings();
	if (!Bindings)
	{
		return false;
	}

	Bindings->RemoveBindings(TargetPath);
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

TArray<FString> UStateTreeService::GetAvailableConditionTypes()
{
	TArray<FString> Results;

	UScriptStruct* CondBaseStruct = FStateTreeConditionBase::StaticStruct();
	if (!CondBaseStruct) { return Results; }

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (Struct && Struct != CondBaseStruct && Struct->IsChildOf(CondBaseStruct))
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

bool UStateTreeService::AddEnterCondition(const FString& AssetPath, const FString& StatePath, const FString& ConditionStructName)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddEnterCondition: State not found: %s"), *StatePath);
		return false;
	}

	UScriptStruct* CondStruct = FindNodeStruct(ConditionStructName);
	if (!CondStruct)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddEnterCondition: Struct not found: %s"), *ConditionStructName);
		return false;
	}

	if (!CondStruct->IsChildOf(FStateTreeConditionBase::StaticStruct()))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddEnterCondition: Struct '%s' is not a FStateTreeConditionBase"), *ConditionStructName);
		return false;
	}

	FStateTreeEditorNode& NewNode = State->EnterConditions.AddDefaulted_GetRef();
	if (!InitEditorNodeFromStruct(NewNode, CondStruct))
	{
		State->EnterConditions.RemoveAt(State->EnterConditions.Num() - 1);
		return false;
	}

	// First condition is Copy, subsequent are And
	NewNode.ExpressionOperand = (State->EnterConditions.Num() == 1)
		? EStateTreeExpressionOperand::Copy
		: EStateTreeExpressionOperand::And;

	MarkStateTreeDirty(StateTree);
	UE_LOG(LogStateTreeService, Log, TEXT("AddEnterCondition: Added '%s' to state '%s' in %s"),
		*ConditionStructName, *StatePath, *AssetPath);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::RemoveEnterCondition(const FString& AssetPath, const FString& StatePath, int32 ConditionIndex)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveEnterCondition: State not found: %s"), *StatePath);
		return false;
	}

	if (!State->EnterConditions.IsValidIndex(ConditionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveEnterCondition: ConditionIndex %d out of range"), ConditionIndex);
		return false;
	}

	State->EnterConditions.RemoveAt(ConditionIndex);

	// Fix up first condition operand to Copy if needed
	if (!State->EnterConditions.IsEmpty())
	{
		State->EnterConditions[0].ExpressionOperand = EStateTreeExpressionOperand::Copy;
	}

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::SetEnterConditionOperand(const FString& AssetPath, const FString& StatePath,
	int32 ConditionIndex, const FString& Operand)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetEnterConditionOperand: State not found: %s"), *StatePath);
		return false;
	}

	if (!State->EnterConditions.IsValidIndex(ConditionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetEnterConditionOperand: ConditionIndex %d out of range"), ConditionIndex);
		return false;
	}

	State->EnterConditions[ConditionIndex].ExpressionOperand = StringToOperand(Operand);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

TArray<FStateTreePropertyInfo> UStateTreeService::GetEnterConditionPropertyNames(const FString& AssetPath,
	const FString& StatePath, const FString& ConditionStructName, int32 ConditionMatchIndex)
{
	TArray<FStateTreePropertyInfo> Result;

#if WITH_EDITORONLY_DATA
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return Result; }

	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return Result; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State) { return Result; }

	FStateTreeEditorNode* CondNode = FindEditorNodeByStructInArray(State->EnterConditions, ConditionStructName, ConditionMatchIndex);
	if (!CondNode) { return Result; }

	TSet<FString> SeenPropertyPaths;
	const UStruct* NodeStruct = nullptr;
	void* NodeMemory = nullptr;
	if (GetTaskNodeData(CondNode, NodeStruct, NodeMemory))
	{
		AppendEditableProperties(NodeStruct, NodeMemory, Result, &SeenPropertyPaths);
	}
	const UStruct* InstanceStruct = nullptr;
	void* InstanceMemory = nullptr;
	if (GetTaskInstanceData(CondNode, InstanceStruct, InstanceMemory))
	{
		AppendEditableProperties(InstanceStruct, InstanceMemory, Result, &SeenPropertyPaths);
	}
#endif

	return Result;
}

bool UStateTreeService::SetEnterConditionPropertyValue(const FString& AssetPath, const FString& StatePath,
	const FString& ConditionStructName, const FString& PropertyPath, const FString& Value, int32 ConditionMatchIndex)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetEnterConditionPropertyValue: State not found: %s"), *StatePath);
		return false;
	}

	FStateTreeEditorNode* CondNode = FindEditorNodeByStructInArray(State->EnterConditions, ConditionStructName, ConditionMatchIndex);
	if (!CondNode)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetEnterConditionPropertyValue: Condition '%s' not found in state '%s'"),
			*ConditionStructName, *StatePath);
		return false;
	}

	FProperty* Property = nullptr;
	void* PropertyValuePtr = nullptr;
	if (!ResolveTaskPropertyPath(CondNode, PropertyPath, Property, PropertyValuePtr))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetEnterConditionPropertyValue: Could not resolve property path '%s'"), *PropertyPath);
		return false;
	}

	if (!SetPropertyValueFromString(Property, PropertyValuePtr, Value))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetEnterConditionPropertyValue: Failed to set property '%s'"), *PropertyPath);
		return false;
	}

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::BindEnterConditionPropertyToContext(const FString& AssetPath, const FString& StatePath,
	const FString& ConditionStructName, const FString& ConditionPropertyPath, const FString& ContextName,
	const FString& ContextPropertyPath, int32 ConditionMatchIndex)
{
	if (ConditionPropertyPath.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindEnterConditionPropertyToContext: ConditionPropertyPath is required"));
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
		UE_LOG(LogStateTreeService, Warning, TEXT("BindEnterConditionPropertyToContext: State not found: %s"), *StatePath);
		return false;
	}

	FStateTreeEditorNode* CondNode = FindEditorNodeByStructInArray(State->EnterConditions, ConditionStructName, ConditionMatchIndex);
	if (!CondNode)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("BindEnterConditionPropertyToContext: Condition '%s' not found in state '%s'"),
			*ConditionStructName, *StatePath);
		return false;
	}

	FGuid ContextStructID;
	if (!ResolveContextStructID(StateTree, ContextName, ContextStructID))
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("BindEnterConditionPropertyToContext: Context '%s' not found. Ensure context actor class is set."),
			*ContextName);
		return false;
	}

	FPropertyBindingPath SourcePath;
	if (!MakeBindingPath(ContextStructID, ContextPropertyPath, SourcePath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindEnterConditionPropertyToContext: Invalid context property path: %s"), *ContextPropertyPath);
		return false;
	}

	FPropertyBindingPath TargetPath;
	if (!MakeBindingPath(CondNode->ID, ConditionPropertyPath, TargetPath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindEnterConditionPropertyToContext: Invalid condition property path: %s"), *ConditionPropertyPath);
		return false;
	}

	EditorData->AddPropertyBinding(SourcePath, TargetPath);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::BindEnterConditionPropertyToRootParameter(const FString& AssetPath, const FString& StatePath,
	const FString& ConditionStructName, const FString& ConditionPropertyPath, const FString& ParameterPath, int32 ConditionMatchIndex)
{
	if (ConditionPropertyPath.IsEmpty() || ParameterPath.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindEnterConditionPropertyToRootParameter: ConditionPropertyPath and ParameterPath are required"));
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
		UE_LOG(LogStateTreeService, Warning, TEXT("BindEnterConditionPropertyToRootParameter: State not found: %s"), *StatePath);
		return false;
	}

	FStateTreeEditorNode* CondNode = FindEditorNodeByStructInArray(State->EnterConditions, ConditionStructName, ConditionMatchIndex);
	if (!CondNode)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("BindEnterConditionPropertyToRootParameter: Condition '%s' not found in state '%s'"),
			*ConditionStructName, *StatePath);
		return false;
	}

	FPropertyBindingPath SourcePath;
	if (!MakeBindingPath(EditorData->GetRootParametersGuid(), ParameterPath, SourcePath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindEnterConditionPropertyToRootParameter: Invalid parameter path: %s"), *ParameterPath);
		return false;
	}

	FPropertyBindingPath TargetPath;
	if (!MakeBindingPath(CondNode->ID, ConditionPropertyPath, TargetPath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindEnterConditionPropertyToRootParameter: Invalid condition property path: %s"), *ConditionPropertyPath);
		return false;
	}

	EditorData->AddPropertyBinding(SourcePath, TargetPath);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::UnbindEnterConditionProperty(const FString& AssetPath, const FString& StatePath,
	const FString& ConditionStructName, const FString& ConditionPropertyPath, int32 ConditionMatchIndex)
{
	if (ConditionPropertyPath.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("UnbindEnterConditionProperty: ConditionPropertyPath is required"));
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
		UE_LOG(LogStateTreeService, Warning, TEXT("UnbindEnterConditionProperty: State not found: %s"), *StatePath);
		return false;
	}

	FStateTreeEditorNode* CondNode = FindEditorNodeByStructInArray(State->EnterConditions, ConditionStructName, ConditionMatchIndex);
	if (!CondNode)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("UnbindEnterConditionProperty: Condition '%s' not found in state '%s'"),
			*ConditionStructName, *StatePath);
		return false;
	}

	FPropertyBindingPath TargetPath;
	if (!MakeBindingPath(CondNode->ID, ConditionPropertyPath, TargetPath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("UnbindEnterConditionProperty: Invalid condition property path: %s"), *ConditionPropertyPath);
		return false;
	}

	FStateTreeEditorPropertyBindings* Bindings = EditorData->GetPropertyEditorBindings();
	if (!Bindings)
	{
		return false;
	}

	Bindings->RemoveBindings(TargetPath);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::AddTransitionCondition(const FString& AssetPath, const FString& StatePath,
	int32 TransitionIndex, const FString& ConditionStructName)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddTransitionCondition: State not found: %s"), *StatePath);
		return false;
	}

	if (!State->Transitions.IsValidIndex(TransitionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddTransitionCondition: TransitionIndex %d out of range"), TransitionIndex);
		return false;
	}

	UScriptStruct* CondStruct = FindNodeStruct(ConditionStructName);
	if (!CondStruct)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddTransitionCondition: Struct not found: %s"), *ConditionStructName);
		return false;
	}

	if (!CondStruct->IsChildOf(FStateTreeConditionBase::StaticStruct()))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("AddTransitionCondition: Struct '%s' is not a FStateTreeConditionBase"), *ConditionStructName);
		return false;
	}

	FStateTreeTransition& Trans = State->Transitions[TransitionIndex];
	FStateTreeEditorNode& NewNode = Trans.Conditions.AddDefaulted_GetRef();
	if (!InitEditorNodeFromStruct(NewNode, CondStruct))
	{
		Trans.Conditions.RemoveAt(Trans.Conditions.Num() - 1);
		return false;
	}

	NewNode.ExpressionOperand = (Trans.Conditions.Num() == 1)
		? EStateTreeExpressionOperand::Copy
		: EStateTreeExpressionOperand::And;

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::RemoveTransitionCondition(const FString& AssetPath, const FString& StatePath,
	int32 TransitionIndex, int32 ConditionIndex)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveTransitionCondition: State not found: %s"), *StatePath);
		return false;
	}

	if (!State->Transitions.IsValidIndex(TransitionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveTransitionCondition: TransitionIndex %d out of range"), TransitionIndex);
		return false;
	}

	FStateTreeTransition& Trans = State->Transitions[TransitionIndex];
	if (!Trans.Conditions.IsValidIndex(ConditionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveTransitionCondition: ConditionIndex %d out of range"), ConditionIndex);
		return false;
	}

	Trans.Conditions.RemoveAt(ConditionIndex);
	if (!Trans.Conditions.IsEmpty())
	{
		Trans.Conditions[0].ExpressionOperand = EStateTreeExpressionOperand::Copy;
	}

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::SetTransitionConditionOperand(const FString& AssetPath, const FString& StatePath,
	int32 TransitionIndex, int32 ConditionIndex, const FString& Operand)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTransitionConditionOperand: State not found: %s"), *StatePath);
		return false;
	}

	if (!State->Transitions.IsValidIndex(TransitionIndex) ||
		!State->Transitions[TransitionIndex].Conditions.IsValidIndex(ConditionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTransitionConditionOperand: Invalid transition or condition index"));
		return false;
	}

	State->Transitions[TransitionIndex].Conditions[ConditionIndex].ExpressionOperand = StringToOperand(Operand);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

TArray<FStateTreePropertyInfo> UStateTreeService::GetTransitionConditionPropertyNames(const FString& AssetPath,
	const FString& StatePath, int32 TransitionIndex, const FString& ConditionStructName, int32 ConditionMatchIndex)
{
	TArray<FStateTreePropertyInfo> Result;

#if WITH_EDITORONLY_DATA
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return Result; }

	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return Result; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State || !State->Transitions.IsValidIndex(TransitionIndex)) { return Result; }

	FStateTreeEditorNode* CondNode = FindEditorNodeByStructInArray(
		State->Transitions[TransitionIndex].Conditions, ConditionStructName, ConditionMatchIndex);
	if (!CondNode) { return Result; }

	TSet<FString> SeenPropertyPaths;
	const UStruct* NodeStruct = nullptr;
	void* NodeMemory = nullptr;
	if (GetTaskNodeData(CondNode, NodeStruct, NodeMemory))
	{
		AppendEditableProperties(NodeStruct, NodeMemory, Result, &SeenPropertyPaths);
	}
	const UStruct* InstanceStruct = nullptr;
	void* InstanceMemory = nullptr;
	if (GetTaskInstanceData(CondNode, InstanceStruct, InstanceMemory))
	{
		AppendEditableProperties(InstanceStruct, InstanceMemory, Result, &SeenPropertyPaths);
	}
#endif

	return Result;
}

TArray<FStateTreePropertyInfo> UStateTreeService::GetTransitionEventPayloadPropertyNames(const FString& AssetPath,
	const FString& StatePath, int32 TransitionIndex)
{
	TArray<FStateTreePropertyInfo> Result;

#if WITH_EDITORONLY_DATA
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return Result; }

	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return Result; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State || !State->Transitions.IsValidIndex(TransitionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("GetTransitionEventPayloadPropertyNames: Invalid state or transition index"));
		return Result;
	}

	FStateTreeTransition& Trans = State->Transitions[TransitionIndex];
	if (!Trans.RequiredEvent.PayloadStruct)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("GetTransitionEventPayloadPropertyNames: Transition %d on state '%s' has no event payload struct set"),
			TransitionIndex, *StatePath);
		return Result;
	}

	FStructOnScope PayloadScope(Trans.RequiredEvent.PayloadStruct);
	void* PayloadMemory = PayloadScope.GetStructMemory();
	TSet<FString> SeenPropertyPaths;
	AppendBindableEditableProperties(Trans.RequiredEvent.PayloadStruct, PayloadMemory, Result, &SeenPropertyPaths);
#endif

	return Result;
}

bool UStateTreeService::SetTransitionConditionPropertyValue(const FString& AssetPath, const FString& StatePath,
	int32 TransitionIndex, const FString& ConditionStructName, const FString& PropertyPath,
	const FString& Value, int32 ConditionMatchIndex)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	UStateTreeState* State = FindStateByPath(EditorData, StatePath);
	if (!State || !State->Transitions.IsValidIndex(TransitionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTransitionConditionPropertyValue: Invalid state or transition index"));
		return false;
	}

	FStateTreeEditorNode* CondNode = FindEditorNodeByStructInArray(
		State->Transitions[TransitionIndex].Conditions, ConditionStructName, ConditionMatchIndex);
	if (!CondNode)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTransitionConditionPropertyValue: Condition '%s' not found"), *ConditionStructName);
		return false;
	}

	FProperty* Property = nullptr;
	void* PropertyValuePtr = nullptr;
	if (!ResolveTaskPropertyPath(CondNode, PropertyPath, Property, PropertyValuePtr))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetTransitionConditionPropertyValue: Could not resolve property path '%s'"), *PropertyPath);
		return false;
	}

	if (!SetPropertyValueFromString(Property, PropertyValuePtr, Value))
	{
		return false;
	}

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::BindTransitionConditionPropertyToContext(const FString& AssetPath, const FString& StatePath,
	int32 TransitionIndex, const FString& ConditionStructName, const FString& ConditionPropertyPath,
	const FString& ContextName, const FString& ContextPropertyPath, int32 ConditionMatchIndex)
{
	if (ConditionPropertyPath.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionConditionPropertyToContext: ConditionPropertyPath is required"));
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
	if (!State || !State->Transitions.IsValidIndex(TransitionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionConditionPropertyToContext: Invalid state or transition index"));
		return false;
	}

	FStateTreeEditorNode* CondNode = FindEditorNodeByStructInArray(
		State->Transitions[TransitionIndex].Conditions, ConditionStructName, ConditionMatchIndex);
	if (!CondNode)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("BindTransitionConditionPropertyToContext: Condition '%s' not found on transition %d of state '%s'"),
			*ConditionStructName, TransitionIndex, *StatePath);
		return false;
	}

	FGuid ContextStructID;
	if (!ResolveContextStructID(StateTree, ContextName, ContextStructID))
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("BindTransitionConditionPropertyToContext: Context '%s' not found. Ensure context actor class is set."),
			*ContextName);
		return false;
	}

	FPropertyBindingPath SourcePath;
	if (!MakeBindingPath(ContextStructID, ContextPropertyPath, SourcePath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionConditionPropertyToContext: Invalid context property path: %s"), *ContextPropertyPath);
		return false;
	}

	FPropertyBindingPath TargetPath;
	if (!MakeBindingPath(CondNode->ID, ConditionPropertyPath, TargetPath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionConditionPropertyToContext: Invalid condition property path: %s"), *ConditionPropertyPath);
		return false;
	}

	EditorData->AddPropertyBinding(SourcePath, TargetPath);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::BindTransitionConditionPropertyToEventPayload(const FString& AssetPath, const FString& StatePath,
	int32 TransitionIndex, const FString& ConditionStructName, const FString& ConditionPropertyPath,
	const FString& PayloadPropertyPath, int32 ConditionMatchIndex)
{
	if (ConditionPropertyPath.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionConditionPropertyToEventPayload: ConditionPropertyPath is required"));
		return false;
	}

	if (PayloadPropertyPath.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionConditionPropertyToEventPayload: PayloadPropertyPath is required"));
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
	if (!State || !State->Transitions.IsValidIndex(TransitionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionConditionPropertyToEventPayload: Invalid state or transition index"));
		return false;
	}

	FStateTreeTransition& Trans = State->Transitions[TransitionIndex];

	if (!Trans.RequiredEvent.PayloadStruct)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("BindTransitionConditionPropertyToEventPayload: Transition %d on state '%s' has no event payload struct set"),
			TransitionIndex, *StatePath);
		return false;
	}

	FStateTreeEditorNode* CondNode = FindEditorNodeByStructInArray(
		Trans.Conditions, ConditionStructName, ConditionMatchIndex);
	if (!CondNode)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("BindTransitionConditionPropertyToEventPayload: Condition '%s' not found on transition %d of state '%s'"),
			*ConditionStructName, TransitionIndex, *StatePath);
		return false;
	}

	// The event payload's binding ID is derived from the transition's ID
	const FGuid EventPayloadID = Trans.GetEventID();
	FString ResolvedPayloadPropertyPath;
	if (!ResolveEventPayloadBindingPath(Trans.RequiredEvent.PayloadStruct, PayloadPropertyPath, ResolvedPayloadPropertyPath))
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("BindTransitionConditionPropertyToEventPayload: Could not resolve payload property path '%s' on struct '%s'. Call GetTransitionEventPayloadPropertyNames() to inspect valid bindable paths."),
			*PayloadPropertyPath, *Trans.RequiredEvent.PayloadStruct->GetName());
		return false;
	}

	FPropertyBindingPath SourcePath;
	if (!MakeBindingPath(EventPayloadID, ResolvedPayloadPropertyPath, SourcePath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionConditionPropertyToEventPayload: Invalid payload property path: %s"), *ResolvedPayloadPropertyPath);
		return false;
	}

	FPropertyBindingPath TargetPath;
	if (!MakeBindingPath(CondNode->ID, ConditionPropertyPath, TargetPath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("BindTransitionConditionPropertyToEventPayload: Invalid condition property path: %s"), *ConditionPropertyPath);
		return false;
	}

	EditorData->AddPropertyBinding(SourcePath, TargetPath);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::UnbindTransitionConditionProperty(const FString& AssetPath, const FString& StatePath,
	int32 TransitionIndex, const FString& ConditionStructName, const FString& ConditionPropertyPath,
	int32 ConditionMatchIndex)
{
	if (ConditionPropertyPath.IsEmpty())
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("UnbindTransitionConditionProperty: ConditionPropertyPath is required"));
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
	if (!State || !State->Transitions.IsValidIndex(TransitionIndex))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("UnbindTransitionConditionProperty: Invalid state or transition index"));
		return false;
	}

	FStateTreeTransition& Trans = State->Transitions[TransitionIndex];
	FStateTreeEditorNode* CondNode = FindEditorNodeByStructInArray(Trans.Conditions, ConditionStructName, ConditionMatchIndex);
	if (!CondNode)
	{
		UE_LOG(LogStateTreeService, Warning,
			TEXT("UnbindTransitionConditionProperty: Condition '%s' not found on transition %d of state '%s'"),
			*ConditionStructName, TransitionIndex, *StatePath);
		return false;
	}

	FPropertyBindingPath TargetPath;
	if (!MakeBindingPath(CondNode->ID, ConditionPropertyPath, TargetPath))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("UnbindTransitionConditionProperty: Invalid condition property path: %s"), *ConditionPropertyPath);
		return false;
	}

	FStateTreeEditorPropertyBindings* Bindings = EditorData->GetPropertyEditorBindings();
	if (!Bindings)
	{
		return false;
	}

	Bindings->RemoveBindings(TargetPath);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::RemoveEvaluator(const FString& AssetPath, const FString& EvaluatorStructName, int32 MatchIndex)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	FStateTreeEditorNode* Node = FindEditorNodeByStructInArray(EditorData->Evaluators, EvaluatorStructName, MatchIndex);
	if (!Node)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveEvaluator: Evaluator '%s' not found"), *EvaluatorStructName);
		return false;
	}

	const int32 ArrayIndex = static_cast<int32>(Node - EditorData->Evaluators.GetData());
	EditorData->Evaluators.RemoveAt(ArrayIndex);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

TArray<FStateTreePropertyInfo> UStateTreeService::GetEvaluatorPropertyNames(const FString& AssetPath,
	const FString& EvaluatorStructName, int32 MatchIndex)
{
	TArray<FStateTreePropertyInfo> Result;

#if WITH_EDITORONLY_DATA
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return Result; }

	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return Result; }

	FStateTreeEditorNode* Node = FindEditorNodeByStructInArray(EditorData->Evaluators, EvaluatorStructName, MatchIndex);
	if (!Node) { return Result; }

	TSet<FString> SeenPropertyPaths;
	const UStruct* NodeStruct = nullptr;
	void* NodeMemory = nullptr;
	if (GetTaskNodeData(Node, NodeStruct, NodeMemory))
	{
		AppendEditableProperties(NodeStruct, NodeMemory, Result, &SeenPropertyPaths);
	}
	const UStruct* InstanceStruct = nullptr;
	void* InstanceMemory = nullptr;
	if (GetTaskInstanceData(Node, InstanceStruct, InstanceMemory))
	{
		AppendEditableProperties(InstanceStruct, InstanceMemory, Result, &SeenPropertyPaths);
	}
#endif

	return Result;
}

bool UStateTreeService::SetEvaluatorPropertyValue(const FString& AssetPath, const FString& EvaluatorStructName,
	const FString& PropertyPath, const FString& Value, int32 MatchIndex)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	FStateTreeEditorNode* Node = FindEditorNodeByStructInArray(EditorData->Evaluators, EvaluatorStructName, MatchIndex);
	if (!Node)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetEvaluatorPropertyValue: Evaluator '%s' not found"), *EvaluatorStructName);
		return false;
	}

	FProperty* Property = nullptr;
	void* PropertyValuePtr = nullptr;
	if (!ResolveTaskPropertyPath(Node, PropertyPath, Property, PropertyValuePtr))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetEvaluatorPropertyValue: Could not resolve property path '%s'"), *PropertyPath);
		return false;
	}

	if (!SetPropertyValueFromString(Property, PropertyValuePtr, Value))
	{
		return false;
	}

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::RemoveGlobalTask(const FString& AssetPath, const FString& TaskStructName, int32 MatchIndex)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	FStateTreeEditorNode* Node = FindEditorNodeByStructInArray(EditorData->GlobalTasks, TaskStructName, MatchIndex);
	if (!Node)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("RemoveGlobalTask: Global task '%s' not found"), *TaskStructName);
		return false;
	}

	const int32 ArrayIndex = static_cast<int32>(Node - EditorData->GlobalTasks.GetData());
	EditorData->GlobalTasks.RemoveAt(ArrayIndex);
	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

TArray<FStateTreePropertyInfo> UStateTreeService::GetGlobalTaskPropertyNames(const FString& AssetPath,
	const FString& TaskStructName, int32 MatchIndex)
{
	TArray<FStateTreePropertyInfo> Result;

#if WITH_EDITORONLY_DATA
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return Result; }

	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return Result; }

	FStateTreeEditorNode* Node = FindEditorNodeByStructInArray(EditorData->GlobalTasks, TaskStructName, MatchIndex);
	if (!Node) { return Result; }

	TSet<FString> SeenPropertyPaths;
	const UStruct* NodeStruct = nullptr;
	void* NodeMemory = nullptr;
	if (GetTaskNodeData(Node, NodeStruct, NodeMemory))
	{
		AppendEditableProperties(NodeStruct, NodeMemory, Result, &SeenPropertyPaths);
	}
	const UStruct* InstanceStruct = nullptr;
	void* InstanceMemory = nullptr;
	if (GetTaskInstanceData(Node, InstanceStruct, InstanceMemory))
	{
		AppendEditableProperties(InstanceStruct, InstanceMemory, Result, &SeenPropertyPaths);
	}
#endif

	return Result;
}

bool UStateTreeService::SetGlobalTaskPropertyValue(const FString& AssetPath, const FString& TaskStructName,
	const FString& PropertyPath, const FString& Value, int32 MatchIndex)
{
	UStateTree* StateTree = LoadStateTree(AssetPath);
	if (!StateTree) { return false; }

#if WITH_EDITORONLY_DATA
	UStateTreeEditorData* EditorData = GetEditorData(StateTree);
	if (!EditorData) { return false; }

	FStateTreeEditorNode* Node = FindEditorNodeByStructInArray(EditorData->GlobalTasks, TaskStructName, MatchIndex);
	if (!Node)
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetGlobalTaskPropertyValue: Global task '%s' not found"), *TaskStructName);
		return false;
	}

	FProperty* Property = nullptr;
	void* PropertyValuePtr = nullptr;
	if (!ResolveTaskPropertyPath(Node, PropertyPath, Property, PropertyValuePtr))
	{
		UE_LOG(LogStateTreeService, Warning, TEXT("SetGlobalTaskPropertyValue: Could not resolve property path '%s'"), *PropertyPath);
		return false;
	}

	if (!SetPropertyValueFromString(Property, PropertyValuePtr, Value))
	{
		return false;
	}

	MarkStateTreeDirty(StateTree);
	return true;
#else
	return false;
#endif
}

bool UStateTreeService::AddTransition(const FString& AssetPath, const FString& StatePath,
                                       const FString& Trigger, const FString& TransitionType,
                                       const FString& TargetPath, const FString& Priority,
                                       const FString& EventTag)
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

	// Check for duplicate transition (same trigger, type, and target)
	for (const FStateTreeTransition& Existing : State->Transitions)
	{
		if (Existing.Trigger == ParsedTrigger && Existing.State.LinkType == ParsedType)
		{
			// For GotoState, also match the target state
			if (ParsedType == EStateTreeTransitionType::GotoState)
			{
				if (TargetState && Existing.State.ID == TargetState->ID)
				{
					UE_LOG(LogStateTreeService, Warning, TEXT("AddTransition: Duplicate transition '%s' -> '%s' (target '%s') already exists on state '%s'. Skipping."),
						*Trigger, *TransitionType, *TargetPath, *StatePath);
					return false;
				}
			}
			else
			{
				// Non-GotoState types (NextSelectableState, etc.) match on trigger+type alone
				UE_LOG(LogStateTreeService, Warning, TEXT("AddTransition: Duplicate transition '%s' -> '%s' already exists on state '%s'. Skipping."),
					*Trigger, *TransitionType, *StatePath);
				return false;
			}
		}
	}

	FStateTreeTransition& NewTransition = State->AddTransition(ParsedTrigger, ParsedType, TargetState);
	NewTransition.Priority = ParsedPriority;
	if (!EventTag.IsEmpty())
	{
		const FGameplayTag ParsedTag = FGameplayTag::RequestGameplayTag(FName(*EventTag), /*bErrorIfNotFound=*/false);
		NewTransition.RequiredEvent.Tag = ParsedTag;
	}

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
