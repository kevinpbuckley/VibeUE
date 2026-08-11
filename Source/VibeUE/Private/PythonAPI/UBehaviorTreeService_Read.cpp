// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBehaviorTreeService.h"

#include "AIGraphTypes.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_CompositeDecorator.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeServiceInternal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
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
// namespaces in the same blob collide on identical helper names (docs/gotchas.md).
namespace VibeBTRead
{
	/**
	 * The node's edited state: every property a human could have changed in the details panel whose
	 * value differs from the class default.
	 *
	 * Scoped to CPF_Edit deliberately. The alternative — every UPROPERTY that differs from the CDO —
	 * would report a composite's own Children array, its Services array and its ParentNode back
	 * pointer as "properties", which is structure this JSON already describes as children /
	 * decorators / services and which nothing may set directly. What is left is exactly the set a
	 * later SetNodeProperty could round-trip.
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
			if (!Property->HasAnyPropertyFlags(CPF_Edit)
				|| Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Transient | CPF_Deprecated))
			{
				continue;
			}
			if (Property->Identical_InContainer(Instance, Defaults))
			{
				continue;
			}

			FString Value;
			Property->ExportText_InContainer(0, Value, Instance, Defaults, nullptr, PPF_None);
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
		TSet<const UBehaviorTreeGraphNode*>& Visited)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		WriteCommonFields(Node, Object);
		Object->SetBoolField(TEXT("bHasCompositeDecorator"), HasCompositeDecorator(Node));

		Visited.Add(Node);

		TArray<TSharedPtr<FJsonValue>> Decorators;
		for (const TObjectPtr<UBehaviorTreeGraphNode>& Decorator : Node->Decorators)
		{
			if (const UBehaviorTreeGraphNode* Raw = ToRawPtr(Decorator))
			{
				Decorators.Add(MakeShared<FJsonValueObject>(NodeToJson(Raw, Visited)));
			}
		}
		Object->SetArrayField(TEXT("decorators"), Decorators);

		TArray<TSharedPtr<FJsonValue>> Services;
		for (const TObjectPtr<UBehaviorTreeGraphNode>& Service : Node->Services)
		{
			if (const UBehaviorTreeGraphNode* Raw = ToRawPtr(Service))
			{
				Services.Add(MakeShared<FJsonValueObject>(NodeToJson(Raw, Visited)));
			}
		}
		Object->SetArrayField(TEXT("services"), Services);

		TArray<TSharedPtr<FJsonValue>> Children;
		for (const UBehaviorTreeGraphNode* Child : VibeBT::GetChildNodes(Node))
		{
			if (Child && !Visited.Contains(Child))
			{
				Children.Add(MakeShared<FJsonValueObject>(NodeToJson(Child, Visited)));
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
	return true;
}
