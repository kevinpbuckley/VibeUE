// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBehaviorTreeService.h"

#include "AIGraphTypes.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/ValueOrBBKey.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_CompositeDecorator.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeServiceInternal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Templates/Function.h"
#include "UObject/UnrealType.h"

TArray<FBTNodeClassInfo> UBehaviorTreeService::GetAvailableNodeTypes(const FString& Category)
{
	UClass* BaseClass = nullptr;
	if (Category == TEXT("Composite"))
	{
		BaseClass = UBTCompositeNode::StaticClass();
	}
	else if (Category == TEXT("Task"))
	{
		BaseClass = UBTTaskNode::StaticClass();
	}
	else if (Category == TEXT("Decorator"))
	{
		BaseClass = UBTDecorator::StaticClass();
	}
	else if (Category == TEXT("Service"))
	{
		BaseClass = UBTService::StaticClass();
	}

	TArray<FBTNodeClassInfo> Result;
	if (!BaseClass)
	{
		return Result;
	}

	const TSharedPtr<FGraphNodeClassHelper> Helper = VibeBT::GetClassHelper(BaseClass);
	if (!Helper.IsValid())
	{
		return Result;
	}

	TArray<FGraphNodeClassData> ClassData;
	Helper->GatherClasses(BaseClass, ClassData);

	Result.Reserve(ClassData.Num());
	for (FGraphNodeClassData& Data : ClassData)
	{
		FBTNodeClassInfo Info;
		Info.ClassName = Data.GetClassName();
		Info.Category = Data.GetCategory().ToString();

		// IsBlueprint() is a free flag check (AssetName.Len() > 0 — set at construction from
		// asset-registry data). Deliberately NOT using GetClass()->ClassGeneratedBy here: that
		// forces a LoadPackage + FullyLoad per Blueprint entry, on every listing call, whose
		// result is then discarded — dozens of blocking loads to answer a yes/no question.
		Info.bIsBlueprint = Data.IsBlueprint();

		if (Info.bIsBlueprint)
		{
			// Full object path built from asset-registry strings alone, so listing performs
			// no loads at all: "<package>.<GeneratedClassName>", e.g.
			// "/Game/AI/BTT_Foo.BTT_Foo_C".
			Info.ClassPath = Data.GetPackageName() + TEXT(".") + Info.ClassName;
		}
		else
		{
			// Native entries were resolved from a TObjectIterator at graph-build time, so the
			// UClass is already resident — GetClass() here is a pointer read, never a load.
			UClass* ResolvedClass = Data.GetClass(/*bSilent=*/true);
			Info.ClassPath = ResolvedClass
				? ResolvedClass->GetPathName()
				: Data.GetPackageName() + TEXT(".") + Info.ClassName;
		}

		Result.Add(MoveTemp(Info));
	}
	return Result;
}

// Named, not anonymous: this module builds with unity/jumbo enabled, where two anonymous
// namespaces in the same blob collide on identical helper names — the second definition silently
// wins and the other translation unit calls it instead of its own.
namespace VibeBTRead
{
	/**
	 * The node's edited state: every property a human could have changed in the details panel whose
	 * value differs from the class default.
	 *
	 * Scoped to VibeBT::IsAuthorableProperty deliberately — the same definition GetNodePropertyNames
	 * and SetNodePropertyValue use. The alternative — every UPROPERTY that differs from the CDO —
	 * would report a composite's own Children array, its Services array and its ParentNode back
	 * pointer as "properties", which is structure this JSON already describes as children /
	 * decorators / services and which nothing may set directly. What is left is exactly the set
	 * SetNodePropertyValue round-trips.
	 *
	 * The CDO is what decides *whether* a property is reported, and is deliberately not used to
	 * export it: an export against defaults is a delta, and a delta of an array is "(3=...)", which
	 * is not a value SetNodePropertyValue could read back. See VibeBT::ExportPropertyValue.
	 */
	TSharedPtr<FJsonObject> CollectEditedProperties(const UObject* Instance)
	{
		TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
		if (!Instance)
		{
			return Properties;
		}

		UClass* Class = Instance->GetClass();
		const UObject* Defaults = Class ? Class->GetDefaultObject() : nullptr;
		if (!Defaults)
		{
			return Properties;
		}

		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			const FProperty* Property = *It;
			if (!VibeBT::IsAuthorableProperty(Property))
			{
				continue;
			}
			if (Property->Identical_InContainer(Instance, Defaults))
			{
				continue;
			}

			FString Value;
			VibeBT::ExportPropertyValue(Property, Instance, Value);
			Properties->SetStringField(Property->GetName(), Value);
		}

		return Properties;
	}

	/** Node fields shared by tree nodes and sub-nodes. Never recurses. */
	void WriteCommonFields(const UBehaviorTreeGraphNode* Node, const TSharedRef<FJsonObject>& Out)
	{
		Out->SetStringField(TEXT("path"), VibeBT::GetNodePath(Node));
		Out->SetStringField(TEXT("guid"), Node->NodeGuid.ToString());
		Out->SetStringField(TEXT("class"),
			Node->NodeInstance ? Node->NodeInstance->GetClass()->GetName() : FString());
		Out->SetStringField(TEXT("name"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
		// The graph node's comment bubble (SetNodeComment), not the instance's name: annotation,
		// carried so a round-trip through BuildTree preserves it.
		Out->SetStringField(TEXT("comment"), Node->NodeComment);
		Out->SetObjectField(TEXT("properties"), CollectEditedProperties(ToRawPtr(Node->NodeInstance)));
		Out->SetBoolField(TEXT("bInjected"), Node->bInjectedNode != 0);
	}

	/** True when any decorator on Node is a composite (logic-operator) decorator. */
	bool HasCompositeDecorator(const UBehaviorTreeGraphNode* Node)
	{
		for (const TObjectPtr<UBehaviorTreeGraphNode>& Decorator : Node->Decorators)
		{
			if (Cast<UBehaviorTreeGraphNode_CompositeDecorator>(ToRawPtr(Decorator)))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * One node as JSON, with its sub-nodes and its subtree.
	 *
	 * Deliberately the only node serialiser, used for decorators and services as well as for tree
	 * nodes, so every node object in the result carries the same ten fields. Nothing is asserted
	 * about a sub-node to make that true: a decorator has no pins, so GetChildNodes finds nothing,
	 * and its own Decorators / Services arrays are empty because nothing in the engine or in this
	 * service ever fills them. Reporting them by the same code path rather than hardcoding "empty"
	 * means that if that ever stops being true, this says so instead of hiding it.
	 *
	 * Visited is carried so a graph with a back edge — which the layout pass already tolerates —
	 * is reported as a finite tree instead of hanging the caller.
	 */
	TSharedRef<FJsonObject> NodeToJson(const UBehaviorTreeGraphNode* Node,
		TSet<const UBehaviorTreeGraphNode*>& Visited, int32 Depth = 0)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		WriteCommonFields(Node, Object);
		Object->SetBoolField(TEXT("bHasCompositeDecorator"), HasCompositeDecorator(Node));

		Visited.Add(Node);

		// The Visited set makes a CYCLIC graph finite; a merely LINEAR chain of tens of thousands of
		// nodes never revisits anything and would still overflow the stack. Past the bound the node
		// reports itself truncated — with the standard arrays present but empty, so consumers that
		// walk "children" unconditionally still can — rather than letting a read crash the editor.
		if (Depth > VibeBT::MaxTreeDepth)
		{
			Object->SetStringField(TEXT("error"), FString::Printf(
				TEXT("subtree truncated: deeper than %d levels, which no legitimate Behavior Tree "
					 "is"), VibeBT::MaxTreeDepth));
			Object->SetArrayField(TEXT("decorators"), TArray<TSharedPtr<FJsonValue>>());
			Object->SetArrayField(TEXT("services"), TArray<TSharedPtr<FJsonValue>>());
			Object->SetArrayField(TEXT("children"), TArray<TSharedPtr<FJsonValue>>());
			return Object;
		}

		// The Visited test is on all three arms, not just "children". These two arms recurse — the
		// serialiser they replaced did not — so without it a node listed among its own Decorators, or
		// two nodes each carrying the other, recurse until the stack overflows and take the process
		// with them. Nothing this service writes can produce that shape (AttachSubNode always creates
		// a fresh node), but this function's contract is to report a malformed graph as a finite tree
		// rather than hang the caller, and a hand-edited or corrupted asset is exactly what it is for.
		TArray<TSharedPtr<FJsonValue>> Decorators;
		for (const TObjectPtr<UBehaviorTreeGraphNode>& Decorator : Node->Decorators)
		{
			const UBehaviorTreeGraphNode* Raw = ToRawPtr(Decorator);
			if (Raw && !Visited.Contains(Raw))
			{
				Decorators.Add(MakeShared<FJsonValueObject>(NodeToJson(Raw, Visited, Depth + 1)));
			}
		}
		Object->SetArrayField(TEXT("decorators"), Decorators);

		TArray<TSharedPtr<FJsonValue>> Services;
		for (const TObjectPtr<UBehaviorTreeGraphNode>& Service : Node->Services)
		{
			const UBehaviorTreeGraphNode* Raw = ToRawPtr(Service);
			if (Raw && !Visited.Contains(Raw))
			{
				Services.Add(MakeShared<FJsonValueObject>(NodeToJson(Raw, Visited, Depth + 1)));
			}
		}
		Object->SetArrayField(TEXT("services"), Services);

		TArray<TSharedPtr<FJsonValue>> Children;
		for (const UBehaviorTreeGraphNode* Child : VibeBT::GetChildNodes(Node))
		{
			if (Child && !Visited.Contains(Child))
			{
				Children.Add(MakeShared<FJsonValueObject>(NodeToJson(Child, Visited, Depth + 1)));
			}
		}
		Object->SetArrayField(TEXT("children"), Children);

		return Object;
	}

	FString SerialiseJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	/**
	 * A parseable failure. Callers walk "children" without checking anything else, so an error
	 * result carries an empty one rather than omitting the field and turning a handled failure
	 * into a crash in the caller.
	 */
	FString TreeError(const FString& Message)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("error"), Message);
		Object->SetArrayField(TEXT("children"), TArray<TSharedPtr<FJsonValue>>());
		return SerialiseJson(Object);
	}
}

using namespace VibeBTRead;

FString UBehaviorTreeService::GetTree(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return TreeError(TEXT("AssetPath is empty"));
	}
	const FString LengthError = VibeBT::CheckNameLength(AssetPath, TEXT("AssetPath"));
	if (!LengthError.IsEmpty())
	{
		return TreeError(LengthError);
	}

	UBehaviorTree* Tree =
		LoadObject<UBehaviorTree>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Tree)
	{
		return TreeError(FString::Printf(TEXT("Behavior Tree not found: %s"), *AssetPath));
	}

	// No EnsureGraph: this is the read path, and creating the graph here would make every read a
	// write. An asset the factory made and nobody has opened reports that fact instead.
	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
	if (!Graph)
	{
		return TreeError(FString::Printf(
			TEXT("%s has no editor graph. It has never been opened in the Behavior Tree editor, and "
				 "reading is not allowed to create one. Run CompileAndSave (or "
				 "RepairGraphFromRuntimeTree, if it has a runtime tree) to build it."),
			*AssetPath));
	}

	const UBehaviorTreeGraphNode* Root = VibeBT::FindRootGraphNode(Graph);
	if (!Root)
	{
		return TreeError(FString::Printf(TEXT("%s has no root node in its editor graph"), *AssetPath));
	}

	TSet<const UBehaviorTreeGraphNode*> Visited;
	return SerialiseJson(NodeToJson(Root, Visited));
}

bool UBehaviorTreeService::GetNodeInfo(const FString& AssetPath, const FString& NodePath,
	FBTNodeInfo& OutInfo)
{
	OutInfo = FBTNodeInfo();

	OutInfo.Error = VibeBT::CheckNameLength(AssetPath, TEXT("AssetPath"));
	if (!OutInfo.Error.IsEmpty())
	{
		return false;
	}

	UBehaviorTree* Tree =
		LoadObject<UBehaviorTree>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Tree)
	{
		OutInfo.Error = FString::Printf(TEXT("Behavior Tree not found: %s"), *AssetPath);
		return false;
	}

	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
	if (!Graph)
	{
		OutInfo.Error = FString::Printf(TEXT("%s has no editor graph"), *AssetPath);
		return false;
	}

	const UBehaviorTreeGraphNode* Node = VibeBT::ResolveNodePath(Graph, NodePath);
	if (!Node)
	{
		// Deliberately not "false with an empty struct": a default-constructed FBTNodeInfo reads
		// exactly like a real, empty node, so the reason has to travel with the failure.
		OutInfo.Error = FString::Printf(
			TEXT("No node at path '%s' in %s. Either nothing is there, or the path is ambiguous — "
				 "a segment naming several same-named siblings needs an index, as in 'Sequence[1]'."),
			*NodePath, *AssetPath);
		return false;
	}

	OutInfo.Path = VibeBT::GetNodePath(Node);
	OutInfo.Guid = Node->NodeGuid.ToString();
	OutInfo.ClassName = Node->NodeInstance ? Node->NodeInstance->GetClass()->GetName() : FString();
	OutInfo.NodeName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
	OutInfo.ChildCount = VibeBT::GetChildNodes(Node).Num();
	OutInfo.DecoratorCount = Node->Decorators.Num();
	OutInfo.ServiceCount = Node->Services.Num();
	OutInfo.bInjected = Node->bInjectedNode != 0;
	OutInfo.bHasCompositeDecorator = HasCompositeDecorator(Node);
	OutInfo.Comment = Node->NodeComment;
	return true;
}

namespace VibeBTRead
{
	/**
	 * The node instance at NodePath, loaded read-only, or null with OutError set.
	 *
	 * No EnsureGraph, for the same reason GetTree has none: creating the editor graph here would
	 * make reading a property a write to the asset.
	 */
	UObject* ResolveInstanceForRead(const FString& AssetPath, const FString& NodePath, FString& OutError)
	{
		// Before the load: an object-path lookup builds an FName per segment, which is a fatal
		// engine error past NAME_SIZE — reachable from any MCP caller's AssetPath.
		OutError = VibeBT::CheckNameLength(AssetPath, TEXT("AssetPath"));
		if (!OutError.IsEmpty())
		{
			return nullptr;
		}

		UBehaviorTree* Tree =
			LoadObject<UBehaviorTree>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Tree)
		{
			OutError = FString::Printf(TEXT("Behavior Tree not found: %s"), *AssetPath);
			return nullptr;
		}

		UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
		if (!Graph)
		{
			OutError = FString::Printf(TEXT("%s has no editor graph"), *AssetPath);
			return nullptr;
		}

		const UBehaviorTreeGraphNode* Node = VibeBT::ResolveNodePath(Graph, NodePath);
		if (!Node)
		{
			OutError = FString::Printf(
				TEXT("No node at path '%s' in %s (nothing there, or the path is ambiguous and needs an "
					 "index, as in 'Sequence[1]')"),
				*NodePath, *AssetPath);
			return nullptr;
		}

		UObject* Instance = ToRawPtr(Node->NodeInstance);
		if (!Instance)
		{
			OutError = FString::Printf(
				TEXT("'%s' carries no Behavior Tree node instance, so it has no properties (the graph's "
					 "Root node is always in this state)"),
				*NodePath);
			return nullptr;
		}
		return Instance;
	}
}

TArray<FBTPropertyInfo> UBehaviorTreeService::GetNodePropertyNames(const FString& AssetPath,
	const FString& NodePath)
{
	TArray<FBTPropertyInfo> Result;

	FString Error;
	const UObject* Instance = ResolveInstanceForRead(AssetPath, NodePath, Error);
	if (!Instance)
	{
		// No error channel on this signature; the empty array is documented as "nothing addressable
		// there", and GetNodeInfo is what distinguishes the cases.
		return Result;
	}

	for (TFieldIterator<FProperty> It(Instance->GetClass()); It; ++It)
	{
		const FProperty* Property = *It;
		if (!VibeBT::IsAuthorableProperty(Property))
		{
			continue;
		}

		FBTPropertyInfo Info;
		Info.Name = Property->GetName();
		Info.Type = Property->GetCPPType();
		VibeBT::ExportPropertyValue(Property, Instance, Info.Value);

		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		Info.bIsBlackboardKeySelector =
			StructProperty && StructProperty->Struct == FBlackboardKeySelector::StaticStruct();

		Result.Add(MoveTemp(Info));
	}

	return Result;
}

FString UBehaviorTreeService::GetNodePropertyValue(const FString& AssetPath, const FString& NodePath,
	const FString& PropertyName)
{
	FString Error;
	const UObject* Instance = ResolveInstanceForRead(AssetPath, NodePath, Error);
	if (!Instance)
	{
		return FString::Printf(TEXT("ERROR: %s"), *Error);
	}

	const FProperty* Property = VibeBT::FindAuthorableProperty(Instance, PropertyName, Error);
	if (!Property)
	{
		return FString::Printf(TEXT("ERROR: %s"), *Error);
	}

	FString Value;
	VibeBT::ExportPropertyValue(Property, Instance, Value);
	return Value;
}

namespace VibeBTRead
{
	/** Node's path if reachable from Root, or a stand-in identity for one that is not — an orphan,
	 *  by definition, has no path VibeBT::GetNodePath's grammar can produce. */
	FString NodeLabel(const UBehaviorTreeGraphNode* Node)
	{
		const FString Path = VibeBT::GetNodePath(Node);
		if (!Path.IsEmpty())
		{
			return Path;
		}
		return FString::Printf(TEXT("<unreachable:%s#%s>"),
			*Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), *Node->NodeGuid.ToString());
	}

	// Depth cap, not a visited-struct-type guard: no struct reachable from a BT node's own properties
	// contains itself, so an infinite loop is not a real risk here the way a node/pin cycle is
	// elsewhere in this service — but a cap is cheap insurance against a pathological or hand-edited
	// struct layout, in the same spirit as NodeToJson's Visited set (report a malformed graph as
	// finite rather than hang the caller).
	constexpr int32 kMaxBindableRecursionDepth = 8;

	void WalkBlackboardBindables(const UStruct* StructType, const void* ContainerPtr,
		const FString& PathPrefix, int32 Depth,
		const TFunctionRef<void(const FString&, const FBlackboardKeySelector&)>& OnSelector,
		const TFunctionRef<void(const FString&, const FValueOrBlackboardKeyBase&)>& OnValueKey);

	/**
	 * Struct is itself a blackboard-bindable type: report it directly. Otherwise it is some other
	 * struct (e.g. FEQSParametrizedQueryExecutionRequest) that might carry one of its own — recurse
	 * into its members rather than assume a struct property is inert.
	 */
	void VisitBindableStruct(const UScriptStruct* Struct, const void* ValuePtr, const FString& Label,
		int32 Depth,
		const TFunctionRef<void(const FString&, const FBlackboardKeySelector&)>& OnSelector,
		const TFunctionRef<void(const FString&, const FValueOrBlackboardKeyBase&)>& OnValueKey)
	{
		if (!Struct || !ValuePtr)
		{
			return;
		}
		if (Struct == FBlackboardKeySelector::StaticStruct())
		{
			OnSelector(Label, *static_cast<const FBlackboardKeySelector*>(ValuePtr));
			return;
		}
		if (Struct->IsChildOf(FValueOrBlackboardKeyBase::StaticStruct()))
		{
			OnValueKey(Label, *static_cast<const FValueOrBlackboardKeyBase*>(ValuePtr));
			return;
		}
		WalkBlackboardBindables(Struct, ValuePtr, Label, Depth + 1, OnSelector, OnValueKey);
	}

	/**
	 * Every FBlackboardKeySelector and FValueOrBlackboardKeyBase reachable from ContainerPtr (an
	 * instance of StructType), at any nesting depth — struct properties, and struct elements of
	 * array properties — not just StructType's own top-level fields.
	 *
	 * This is deliberately more thorough than UBTNode::PreSave (BTNode.cpp:231-254), which only ever
	 * iterates TFieldIterator<FStructProperty> over the node class itself: a selector or
	 * value-or-key binding sitting inside another struct (UBTTask_RunEQSQuery::EQSRequest is an
	 * FEQSParametrizedQueryExecutionRequest whose QueryConfig is a TArray<FAIDynamicParam>, and
	 * FAIDynamicParam::BBKey is exactly such a selector) is invisible to the engine's own save-time
	 * check, so a mistyped key there is never reset and never logged — this is the only place it is
	 * ever looked at.
	 *
	 * Only struct-valued properties are walked; object-reference properties are not followed, so this
	 * never reaches into another UObject's (or another asset's) memory.
	 */
	void WalkBlackboardBindables(const UStruct* StructType, const void* ContainerPtr,
		const FString& PathPrefix, int32 Depth,
		const TFunctionRef<void(const FString&, const FBlackboardKeySelector&)>& OnSelector,
		const TFunctionRef<void(const FString&, const FValueOrBlackboardKeyBase&)>& OnValueKey)
	{
		if (!StructType || !ContainerPtr || Depth > kMaxBindableRecursionDepth)
		{
			return;
		}

		for (TFieldIterator<FProperty> It(StructType); It; ++It)
		{
			const FProperty* Property = *It;
			const FString Label = PathPrefix.IsEmpty()
				? Property->GetName()
				: PathPrefix + TEXT(".") + Property->GetName();

			if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
			{
				const void* ValuePtr = StructProp->ContainerPtrToValuePtr<uint8>(ContainerPtr);
				VisitBindableStruct(StructProp->Struct, ValuePtr, Label, Depth, OnSelector, OnValueKey);
			}
			else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
			{
				if (const FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner))
				{
					const void* ArrayPtr = ArrayProp->ContainerPtrToValuePtr<uint8>(ContainerPtr);
					FScriptArrayHelper Helper(ArrayProp, ArrayPtr);
					for (int32 Index = 0; Index < Helper.Num(); ++Index)
					{
						VisitBindableStruct(InnerStruct->Struct, Helper.GetRawPtr(Index),
							FString::Printf(TEXT("%s[%d]"), *Label, Index), Depth, OnSelector, OnValueKey);
					}
				}
			}
		}
	}

	/**
	 * True when KeyName cannot resolve against Blackboard — either there is no blackboard to resolve
	 * against, or it has no key by that name. Read-only: this is exactly the ID
	 * FBlackboardKeySelector::ResolveSelectedKey would assign (BlackboardAsset.GetKeyID(KeyName)) in
	 * the branch that runs whenever a name has actually been set, computed as a read against the
	 * current board rather than by mutating the live property's cached ID.
	 */
	bool KeyIsUnresolved(const UBlackboardData* Blackboard, FName KeyName)
	{
		return !Blackboard || Blackboard->GetKeyID(KeyName) == FBlackboard::InvalidKey;
	}

	/**
	 * The two checks that apply to any node CARRYING an instance — a tree node (composite/task) or a
	 * sub-node (decorator/service) alike: failed-to-load, and the blackboard-bindable walk. Shared
	 * rather than duplicated, because decorators and services are BT node instances in their own
	 * right (UBTDecorator_Blackboard::BlackboardKey is exactly the kind of top-level selector this
	 * function exists to catch) and live outside Graph->Nodes — the walk in ValidateTree that finds
	 * composites and tasks never reaches them on its own.
	 *
	 * bIsRoot excludes the graph's Root node from the failed-to-load check: its NodeInstance is null
	 * by design, not a load failure. No sub-node is ever the Root, so callers checking a decorator or
	 * service always pass false.
	 */
	void CheckNodeInstance(const UBehaviorTreeGraphNode* Node, bool bIsRoot, const FString& Label,
		const UBlackboardData* Blackboard, TArray<FString>& Diagnostics)
	{
		if (!bIsRoot && Node->HasErrors())
		{
			Diagnostics.Add(Node->ErrorMessage.IsEmpty()
				? FString::Printf(TEXT("%s: node instance failed to load (class unresolved)"), *Label)
				: FString::Printf(TEXT("%s: node instance failed to load: %s"), *Label, *Node->ErrorMessage));
		}

		UObject* Instance = ToRawPtr(Node->NodeInstance);
		if (!Instance)
		{
			return;
		}

		auto OnSelector = [&Diagnostics, &Label, Blackboard, Instance](
			const FString& PropLabel, const FBlackboardKeySelector& Selector)
		{
			if (Selector.SelectedKeyName.IsNone())
			{
				return; // Intentionally unbound — not a mistyped binding.
			}
			if (KeyIsUnresolved(Blackboard, Selector.SelectedKeyName))
			{
				Diagnostics.Add(FString::Printf(
					TEXT("%s: %s.%s is bound to blackboard key '%s', which %s"),
					*Label, *Instance->GetClass()->GetName(), *PropLabel,
					*Selector.SelectedKeyName.ToString(),
					Blackboard ? TEXT("does not exist on the tree's blackboard")
							   : TEXT("cannot resolve: the tree has no blackboard assigned")));
			}
		};
		auto OnValueKey = [&Diagnostics, &Label, Blackboard, Instance](
			const FString& PropLabel, const FValueOrBlackboardKeyBase& ValueKey)
		{
			if (ValueKey.GetKey().IsNone())
			{
				return;
			}
			if (KeyIsUnresolved(Blackboard, ValueKey.GetKey()))
			{
				Diagnostics.Add(FString::Printf(
					TEXT("%s: %s.%s is bound to blackboard key '%s', which %s"),
					*Label, *Instance->GetClass()->GetName(), *PropLabel,
					*ValueKey.GetKey().ToString(),
					Blackboard ? TEXT("does not exist on the tree's blackboard")
							   : TEXT("cannot resolve: the tree has no blackboard assigned")));
			}
		};
		WalkBlackboardBindables(Instance->GetClass(), Instance, FString(), 0, OnSelector, OnValueKey);
	}
}

TArray<FString> UBehaviorTreeService::ValidateTree(const FString& AssetPath)
{
	using namespace VibeBTRead;

	TArray<FString> Diagnostics;

	if (AssetPath.IsEmpty())
	{
		Diagnostics.Add(TEXT("ERROR: AssetPath is empty"));
		return Diagnostics;
	}
	const FString LengthError = VibeBT::CheckNameLength(AssetPath, TEXT("AssetPath"));
	if (!LengthError.IsEmpty())
	{
		Diagnostics.Add(FString::Printf(TEXT("ERROR: %s"), *LengthError));
		return Diagnostics;
	}

	// Read-only load, exactly GetTree's path: LOAD_NoWarn | LOAD_Quiet, and no EnsureGraph — creating
	// the editor graph here would make running a diagnostic a write to the asset. Nothing below calls
	// Modify(), MarkPackageDirty() or CommitGraph either.
	UBehaviorTree* Tree =
		LoadObject<UBehaviorTree>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Tree)
	{
		Diagnostics.Add(FString::Printf(TEXT("ERROR: Behavior Tree not found: %s"), *AssetPath));
		return Diagnostics;
	}

	UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(Tree->BTGraph);
	if (!Graph)
	{
		Diagnostics.Add(FString::Printf(
			TEXT("ERROR: %s has no editor graph. It has never been opened in the Behavior Tree "
				 "editor, and validating is not allowed to create one."),
			*AssetPath));
		return Diagnostics;
	}

	UBehaviorTreeGraphNode_Root* Root = VibeBT::FindRootGraphNode(Graph);
	if (!Root)
	{
		Diagnostics.Add(
			FString::Printf(TEXT("ERROR: %s has no root node in its editor graph"), *AssetPath));
		return Diagnostics;
	}

	// The whole-asset defect: a populated runtime tree behind a graph that would not rebuild one.
	// Reported here because it is exactly the shape every mutator on this service refuses to write
	// (VibeBT::CommitGraph's discard guard, same predicate) — without it, the one asset the service
	// will not touch is the one ValidateTree calls healthy, and the caller is told to fix nothing.
	// It is a property of the graph as a whole, not of any one node, so it precedes the node walk.
	if (Tree->RootNode && !VibeBT::GraphWouldRebuildARuntimeTree(Root))
	{
		Diagnostics.Add(FString::Printf(
			TEXT("%s: the editor graph would not rebuild a runtime tree (its root node leads to "
				 "nothing, or to a node carrying no composite instance) while the asset still has a "
				 "populated runtime node tree. Every write to this asset is refused, because "
				 "committing would rebuild an empty tree over the populated one. Run "
				 "RepairGraphFromRuntimeTree to rebuild the graph from the runtime tree first."),
			*AssetPath));
	}

	const UBlackboardData* Blackboard = ToRawPtr(Tree->BlackboardAsset);

	for (UEdGraphNode* EdNode : Graph->Nodes)
	{
		UBehaviorTreeGraphNode* Node = Cast<UBehaviorTreeGraphNode>(EdNode);
		if (!Node)
		{
			continue;
		}

		const bool bIsRoot = Node->IsA<UBehaviorTreeGraphNode_Root>();
		const FString Label = NodeLabel(Node);

		// Orphans: checked for every node, injected or not. An injected node is always linked in by
		// construction (it was copied from a linked subtree node), so this can never misfire on one.
		if (!bIsRoot && VibeBT::GetNodePath(Node).IsEmpty())
		{
			Diagnostics.Add(FString::Printf(TEXT("%s: orphaned, unreachable from Root"), *Label));
		}

		if (!Node->bInjectedNode)
		{
			// Failed-to-load class, and blackboard bindings at any nesting depth — see
			// CheckNodeInstance. A read-only copy of another asset's node, resolved against that
			// asset's own blackboard, is skipped: none of this applies to it (see header comment).
			CheckNodeInstance(Node, bIsRoot, Label, Blackboard, Diagnostics);

			UObject* Instance = ToRawPtr(Node->NodeInstance);

			// Composite with no children. Scoped to composites: a leaf task legitimately has none,
			// and GetChildNodes already spans both of a SimpleParallel's output pins.
			if (Cast<UBTCompositeNode>(Instance) && VibeBT::GetChildNodes(Node).IsEmpty())
			{
				Diagnostics.Add(FString::Printf(TEXT("%s: composite has no children"), *Label));
			}

			// Decorators/services on a node that cannot carry them into the runtime tree.
			const int32 SubNodeCount = Node->Decorators.Num() + Node->Services.Num();
			if (SubNodeCount > 0)
			{
				if (bIsRoot)
				{
					Diagnostics.Add(FString::Printf(
						TEXT("%s: carries %d decorator(s)/service(s) on the graph Root, which never "
							 "run (CreateBTFromGraph reads root-level decorators off the top "
							 "composite, never off the Root graph node)"),
						*Label, SubNodeCount));
				}
				else if (Instance && !Cast<UBTCompositeNode>(Instance) && !Cast<UBTTaskNode>(Instance))
				{
					Diagnostics.Add(FString::Printf(
						TEXT("%s: carries %d decorator(s)/service(s) but its instance (%s) is "
							 "neither a composite nor a task"),
						*Label, SubNodeCount, *Instance->GetClass()->GetName()));
				}
			}
		}

		// Decorators and services: BT node instances in their own right, living outside Graph->Nodes
		// (AIGraphNode.cpp, UAIGraphNode::AddSubNode), so the loop above never reaches them on its
		// own. Each sub-node's OWN bInjectedNode is what gates it here — an injected decorator can
		// sit on a perfectly ordinary, non-injected owner (a RunBehavior task), so the owner's flag
		// checked above says nothing about whether ITS sub-nodes are injected.
		for (const TObjectPtr<UBehaviorTreeGraphNode>& SubNode : Node->Decorators)
		{
			if (UBehaviorTreeGraphNode* Raw = ToRawPtr(SubNode); Raw && !Raw->bInjectedNode)
			{
				CheckNodeInstance(Raw, /*bIsRoot*/ false, NodeLabel(Raw), Blackboard, Diagnostics);
			}
		}
		for (const TObjectPtr<UBehaviorTreeGraphNode>& SubNode : Node->Services)
		{
			if (UBehaviorTreeGraphNode* Raw = ToRawPtr(SubNode); Raw && !Raw->bInjectedNode)
			{
				CheckNodeInstance(Raw, /*bIsRoot*/ false, NodeLabel(Raw), Blackboard, Diagnostics);
			}
		}
	}

	return Diagnostics;
}
