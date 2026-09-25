// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UCustomizableObjectService.h"

#include "CustomizableObjectServiceInternal.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "HAL/PlatformTime.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "VibeUECustomizableObjectService"

namespace
{
	const TCHAR* const ObjectGroupClassPath = TEXT("/Script/CustomizableObjectEditor.CustomizableObjectNodeObjectGroup");

	/** "<NodeId>/<PinName>", for Notes. */
	FString DescribeEnd(const UEdGraphPin* Pin)
	{
		return Pin ? VibeCO::NodeIdOf(Pin->GetOwningNodeUnchecked()) + TEXT("/") + Pin->PinName.ToString() : FString();
	}

	/** Property text from a JSON value: strings verbatim, numbers and booleans converted. */
	bool JsonValueToText(const TSharedPtr<FJsonValue>& Value, FString& OutText)
	{
		switch (Value.IsValid() ? Value->Type : EJson::None)
		{
		case EJson::String:
			OutText = Value->AsString();
			return true;
		case EJson::Number:
		{
			const double Number = Value->AsNumber();
			OutText = FMath::IsNearlyEqual(Number, FMath::RoundToDouble(Number))
				? FString::Printf(TEXT("%lld"), static_cast<int64>(FMath::RoundToDouble(Number)))
				: FString::SanitizeFloat(Number);
			return true;
		}
		case EJson::Boolean:
			OutText = Value->AsBool() ? TEXT("True") : TEXT("False");
			return true;
		default:
			return false;
		}
	}

	/** Discard a node that never made it into the graph. */
	void DiscardUnplacedNode(UEdGraphNode* Node)
	{
		if (Node)
		{
			Node->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			Node->MarkAsGarbage();
		}
	}
}

FVibeCOEditResult UCustomizableObjectService::AddNode(const FString& AssetPath, const FString& NodeClass,
	int32 PosX, int32 PosY, const FString& PropertiesJson)
{
	FVibeCOEditResult Result;
	UObject* Object = nullptr;
	UEdGraph* Graph = nullptr;
	Result.Error = VibeCO::OpenForWrite(AssetPath, Object, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}

	UClass* Class = VibeCO::ResolveNodeClass(NodeClass, Result.Error);
	if (!Class)
	{
		return Result;
	}
	Result.Error = VibeCO::WhyNotPlaceable(Class);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}

	TSharedPtr<FJsonObject> Properties;
	if (!PropertiesJson.TrimStartAndEnd().IsEmpty()
		&& (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PropertiesJson), Properties) || !Properties.IsValid()))
	{
		Result.Error = TEXT("PropertiesJson must be a JSON object of property name -> value, e.g. {\"PinType\": \"material\"}");
		return Result;
	}

	FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Customizable Object Node"));
	Graph->Modify();
	UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, Class, NAME_None, RF_Transactional);

	// Applied before the node exists in the graph and before its pins are built, the way the palette
	// configures its node templates: Switch, Expose/External pin nodes and arithmetic nodes read
	// these while constructing pins, and changing them afterwards does not rebuild them.
	if (Properties.IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Properties->Values)
		{
			FString Text;
			FString PropertyError;
			FProperty* Property = VibeCO::FindNodeProperty(Node, Entry.Key, PropertyError);
			if (Property && !JsonValueToText(Entry.Value, Text))
			{
				PropertyError = FString::Printf(TEXT("PropertiesJson.%s must be a string, number or boolean; "
					"write structs and arrays in Unreal text format, e.g. \"(Name=\\\"A\\\")\""), *Entry.Key);
			}
			if (PropertyError.IsEmpty())
			{
				PropertyError = VibeCO::ImportValue(Property, Node, Text);
			}
			if (!PropertyError.IsEmpty())
			{
				DiscardUnplacedNode(Node);
				Transaction.Cancel();
				Result.Error = PropertyError;
				return Result;
			}
		}
	}

	VibeCO::FinishNewNode(Graph, Node, PosX, PosY);

	Result.NodeId = VibeCO::NodeIdOf(Node);
	Result.Pins = VibeCO::DescribePins(Node);
	Result.Error = VibeCO::CommitEdit(Object, Graph);
	Result.bSuccess = Result.Error.IsEmpty();
	return Result;
}

FVibeCOEditResult UCustomizableObjectService::RemoveNode(const FString& AssetPath, const FString& NodeId)
{
	FVibeCOEditResult Result;
	UObject* Object = nullptr;
	UEdGraph* Graph = nullptr;
	Result.Error = VibeCO::OpenForWrite(AssetPath, Object, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}
	UEdGraphNode* Node = VibeCO::FindNode(Graph, NodeId, Result.Error);
	if (!Node)
	{
		return Result;
	}
	Result.NodeId = VibeCO::NodeIdOf(Node);
	if (!Node->CanUserDeleteNode())
	{
		Result.Error = FString::Printf(TEXT("%s (%s) cannot be deleted: every Customizable Object keeps its "
			"Base Object node"), *Result.NodeId, *Node->GetClass()->GetName());
		return Result;
	}

	FScopedTransaction Transaction(LOCTEXT("RemoveNode", "Remove Customizable Object Node"));
	Graph->Modify();
	Node->Modify();
	// Through the schema, so connected nodes are told their links changed (a Switch rebuilds its
	// option pins when its enum goes away), the same as the editor's Delete.
	if (const UEdGraphSchema* Schema = Graph->GetSchema())
	{
		Schema->BreakNodeLinks(*Node);
	}
	else
	{
		Node->BreakAllNodeLinks();
	}
	Node->DestroyNode();

	Result.Error = VibeCO::CommitEdit(Object, Graph);
	Result.bSuccess = Result.Error.IsEmpty();
	return Result;
}

FVibeCOEditResult UCustomizableObjectService::SetNodePosition(const FString& AssetPath, const FString& NodeId,
	int32 PosX, int32 PosY)
{
	FVibeCOEditResult Result;
	UObject* Object = nullptr;
	UEdGraph* Graph = nullptr;
	Result.Error = VibeCO::OpenForWrite(AssetPath, Object, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}
	UEdGraphNode* Node = VibeCO::FindNode(Graph, NodeId, Result.Error);
	if (!Node)
	{
		return Result;
	}

	FScopedTransaction Transaction(LOCTEXT("MoveNode", "Move Customizable Object Node"));
	Node->Modify();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;

	Result.NodeId = VibeCO::NodeIdOf(Node);
	Result.Error = VibeCO::CommitEdit(Object, Graph);
	Result.bSuccess = Result.Error.IsEmpty();
	return Result;
}

FVibeCOEditResult UCustomizableObjectService::ConnectPins(const FString& AssetPath, const FString& FromNodeId,
	const FString& FromPin, const FString& ToNodeId, const FString& ToPin)
{
	FVibeCOEditResult Result;
	UObject* Object = nullptr;
	UEdGraph* Graph = nullptr;
	Result.Error = VibeCO::OpenForWrite(AssetPath, Object, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}
	UEdGraphNode* FromNode = VibeCO::FindNode(Graph, FromNodeId, Result.Error);
	if (!FromNode)
	{
		return Result;
	}
	UEdGraphNode* ToNode = VibeCO::FindNode(Graph, ToNodeId, Result.Error);
	if (!ToNode)
	{
		return Result;
	}
	// The Mutable schema CastChecks the nodes it is asked about; a comment node would assert.
	if (!VibeCO::IsMutableNode(FromNode) || !VibeCO::IsMutableNode(ToNode))
	{
		Result.Error = TEXT("Both nodes must be Customizable Object nodes (comments and other nodes have no pins to link)");
		return Result;
	}

	// The natural reading first (From = output, To = input), then the reverse, so callers may name
	// the two ends in either order without a pin name shared by an input and an output being ambiguous.
	FString OutputError;
	FString InputError;
	UEdGraphPin* Output = VibeCO::FindPin(FromNode, FromPin, EGPD_Output, OutputError);
	UEdGraphPin* Input = VibeCO::FindPin(ToNode, ToPin, EGPD_Input, InputError);
	if (!Output || !Input)
	{
		FString Ignored;
		UEdGraphPin* ReverseInput = VibeCO::FindPin(FromNode, FromPin, EGPD_Input, Ignored);
		UEdGraphPin* ReverseOutput = VibeCO::FindPin(ToNode, ToPin, EGPD_Output, Ignored);
		if (ReverseInput && ReverseOutput)
		{
			Output = ReverseOutput;
			Input = ReverseInput;
		}
		else
		{
			Result.Error = !OutputError.IsEmpty() ? OutputError : InputError;
			return Result;
		}
	}

	const UEdGraphSchema* Schema = Graph->GetSchema();
	const FPinConnectionResponse Response = Schema->CanCreateConnection(Output, Input);
	if (Response.Response == CONNECT_RESPONSE_DISALLOW)
	{
		Result.Error = FString::Printf(TEXT("Cannot connect %s (%s) -> %s (%s): %s"),
			*DescribeEnd(Output), *Output->PinType.PinCategory.ToString(),
			*DescribeEnd(Input), *Input->PinType.PinCategory.ToString(), *Response.Message.ToString());
		return Result;
	}

	// Remembered so the links the schema drops to honour single-link pins can be reported.
	const TArray<UEdGraphPin*> OutputLinksBefore = Output->LinkedTo;
	const TArray<UEdGraphPin*> InputLinksBefore = Input->LinkedTo;

	FScopedTransaction Transaction(LOCTEXT("ConnectPins", "Connect Customizable Object Pins"));
	FromNode->Modify();
	ToNode->Modify();
	const FString OutputEnd = DescribeEnd(Output);
	const FString InputEnd = DescribeEnd(Input);
	UEdGraphNode* const InputNode = Input->GetOwningNode();
	if (!Schema->TryCreateConnection(Output, Input))
	{
		Transaction.Cancel();
		Result.Error = FString::Printf(TEXT("The schema refused to connect %s -> %s"), *OutputEnd, *InputEnd);
		return Result;
	}

	for (UEdGraphPin* Previous : InputLinksBefore)
	{
		if (Previous != Output && !Input->LinkedTo.Contains(Previous))
		{
			Result.Notes.Add(FString::Printf(TEXT("Replaced link %s -> %s"), *DescribeEnd(Previous), *InputEnd));
		}
	}
	for (UEdGraphPin* Previous : OutputLinksBefore)
	{
		if (Previous != Input && !Output->LinkedTo.Contains(Previous))
		{
			Result.Notes.Add(FString::Printf(TEXT("Broke link %s -> %s (the output allows one link)"),
				*OutputEnd, *DescribeEnd(Previous)));
		}
	}

	// The input node may rebuild its pins in reaction (a Switch fed a new enum), so it is described
	// after the connection, and the id is reported for the side that received the link.
	Result.NodeId = VibeCO::NodeIdOf(InputNode);
	Result.Pins = VibeCO::DescribePins(InputNode);
	Result.Error = VibeCO::CommitEdit(Object, Graph);
	Result.bSuccess = Result.Error.IsEmpty();
	return Result;
}

FVibeCOEditResult UCustomizableObjectService::DisconnectPins(const FString& AssetPath, const FString& NodeId,
	const FString& PinName, const FString& OtherNodeId, const FString& OtherPin)
{
	FVibeCOEditResult Result;
	UObject* Object = nullptr;
	UEdGraph* Graph = nullptr;
	Result.Error = VibeCO::OpenForWrite(AssetPath, Object, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}
	UEdGraphNode* Node = VibeCO::FindNode(Graph, NodeId, Result.Error);
	if (!Node)
	{
		return Result;
	}
	UEdGraphPin* Pin = VibeCO::FindPin(Node, PinName, EGPD_MAX, Result.Error);
	if (!Pin)
	{
		return Result;
	}
	Result.NodeId = VibeCO::NodeIdOf(Node);

	const UEdGraphSchema* Schema = Graph->GetSchema();
	const bool bOneLink = !OtherNodeId.TrimStartAndEnd().IsEmpty() || !OtherPin.TrimStartAndEnd().IsEmpty();
	UEdGraphPin* Other = nullptr;
	if (bOneLink)
	{
		UEdGraphNode* OtherNode = VibeCO::FindNode(Graph, OtherNodeId, Result.Error);
		if (!OtherNode)
		{
			return Result;
		}
		Other = VibeCO::FindPin(OtherNode, OtherPin,
			Pin->Direction == EGPD_Input ? EGPD_Output : EGPD_Input, Result.Error);
		if (!Other)
		{
			return Result;
		}
		if (!Pin->LinkedTo.Contains(Other))
		{
			Result.Error = FString::Printf(TEXT("%s is not linked to %s"), *DescribeEnd(Pin), *DescribeEnd(Other));
			return Result;
		}
	}
	else if (Pin->LinkedTo.Num() == 0)
	{
		// Already in the requested state: nothing to change, nothing to save.
		Result.Notes.Add(FString::Printf(TEXT("%s had no links"), *DescribeEnd(Pin)));
		Result.bSuccess = true;
		return Result;
	}

	FScopedTransaction Transaction(LOCTEXT("DisconnectPins", "Disconnect Customizable Object Pins"));
	Node->Modify();
	if (Other)
	{
		Other->GetOwningNode()->Modify();
		Result.Notes.Add(FString::Printf(TEXT("Broke link %s - %s"), *DescribeEnd(Pin), *DescribeEnd(Other)));
		Schema->BreakSinglePinLink(Pin, Other);
	}
	else
	{
		for (const UEdGraphPin* Linked : Pin->LinkedTo)
		{
			Result.Notes.Add(FString::Printf(TEXT("Broke link %s - %s"), *DescribeEnd(Pin), *DescribeEnd(Linked)));
		}
		Schema->BreakPinLinks(*Pin, /*bSendsNodeNotification*/ true);
	}

	Result.Pins = VibeCO::DescribePins(Node);
	Result.Error = VibeCO::CommitEdit(Object, Graph);
	Result.bSuccess = Result.Error.IsEmpty();
	return Result;
}

FVibeCOEditResult UCustomizableObjectService::SetNodeProperty(const FString& AssetPath, const FString& NodeId,
	const FString& PropertyName, const FString& Value)
{
	FVibeCOEditResult Result;
	UObject* Object = nullptr;
	UEdGraph* Graph = nullptr;
	Result.Error = VibeCO::OpenForWrite(AssetPath, Object, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}
	UEdGraphNode* Node = VibeCO::FindNode(Graph, NodeId, Result.Error);
	if (!Node)
	{
		return Result;
	}
	Result.NodeId = VibeCO::NodeIdOf(Node);
	FProperty* Property = VibeCO::FindNodeProperty(Node, PropertyName, Result.Error);
	if (!Property)
	{
		return Result;
	}

	FScopedTransaction Transaction(LOCTEXT("SetNodeProperty", "Set Customizable Object Node Property"));
	Node->Modify();
	Node->PreEditChange(Property);
	const FString ImportError = VibeCO::ImportValue(Property, Node, Value);

	// PostEditChangeProperty runs even for a rolled-back write, to balance PreEditChange. It is what
	// makes the node react like a details-panel edit: Skeletal Mesh / Section / Table / Enum nodes
	// rebuild their pins here.
	FPropertyChangedEvent ChangedEvent(Property, EPropertyChangeType::ValueSet);
	Node->PostEditChangeProperty(ChangedEvent);
	Result.ValueAfterWrite = VibeCO::ExportValue(Property, Node);
	if (!ImportError.IsEmpty())
	{
		Transaction.Cancel();
		Result.Error = ImportError;
		return Result;
	}

	VibeCO::RefreshDependentSwitches(Node);

	Result.Pins = VibeCO::DescribePins(Node);
	Result.Error = VibeCO::CommitEdit(Object, Graph);
	Result.bSuccess = Result.Error.IsEmpty();
	return Result;
}

FVibeCOEditResult UCustomizableObjectService::RefreshNode(const FString& AssetPath, const FString& NodeId)
{
	FVibeCOEditResult Result;
	UObject* Object = nullptr;
	UEdGraph* Graph = nullptr;
	Result.Error = VibeCO::OpenForWrite(AssetPath, Object, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}
	UEdGraphNode* Node = VibeCO::FindNode(Graph, NodeId, Result.Error);
	if (!Node)
	{
		return Result;
	}

	FScopedTransaction Transaction(LOCTEXT("RefreshNode", "Refresh Customizable Object Node"));
	Node->Modify();
	Node->ReconstructNode();

	Result.NodeId = VibeCO::NodeIdOf(Node);
	Result.Pins = VibeCO::DescribePins(Node);
	Result.Error = VibeCO::CommitEdit(Object, Graph);
	Result.bSuccess = Result.Error.IsEmpty();
	return Result;
}

FVibeCOEditResult UCustomizableObjectService::SetParentObject(const FString& ChildAssetPath,
	const FString& ParentAssetPath, const FString& GroupNodeId)
{
	FVibeCOEditResult Result;
	UObject* Child = nullptr;
	UEdGraph* ChildGraph = nullptr;
	Result.Error = VibeCO::OpenForWrite(ChildAssetPath, Child, ChildGraph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}
	UEdGraphNode* Base = VibeCO::FindBaseObjectNode(ChildGraph);
	if (!Base)
	{
		Result.Error = FString::Printf(TEXT("%s has no Base Object node"), *ChildAssetPath);
		return Result;
	}
	Result.NodeId = VibeCO::NodeIdOf(Base);

	FObjectPropertyBase* ParentProperty = CastField<FObjectPropertyBase>(VibeCO::FindNodeProperty(Base, TEXT("ParentObject"), Result.Error));
	FStructProperty* GroupProperty = CastField<FStructProperty>(VibeCO::FindNodeProperty(Base, TEXT("ParentObjectGroupId"), Result.Error));
	if (!ParentProperty || !GroupProperty || GroupProperty->Struct != TBaseStructure<FGuid>::Get())
	{
		Result.Error = TEXT("The Base Object node has no ParentObject / ParentObjectGroupId in this Mutable version");
		return Result;
	}

	UObject* Parent = nullptr;
	FGuid GroupId;
	if (!ParentAssetPath.TrimStartAndEnd().IsEmpty())
	{
		UEdGraph* ParentGraph = nullptr;
		Result.Error = VibeCO::OpenObject(ParentAssetPath, Parent, ParentGraph);
		if (!Result.Error.IsEmpty())
		{
			return Result;
		}

		// Walk the prospective parent's own chain: an object that is (transitively) its own parent
		// has no root, and the compiler resolves every child through its root.
		UObject* Cursor = Parent;
		for (int32 Depth = 0; Cursor && Depth < 64; ++Depth)
		{
			if (Cursor == Child)
			{
				Result.Error = FString::Printf(TEXT("%s is %s's own ancestor; parenting would make a cycle"),
					*ChildAssetPath, *ParentAssetPath);
				return Result;
			}
			const FString Next = VibeCO::ReadObjectPath(VibeCO::FindBaseObjectNode(VibeCO::GetSourceGraph(Cursor)), TEXT("ParentObject"));
			Cursor = Next.IsEmpty() ? nullptr : LoadObject<UObject>(nullptr, *Next, nullptr, LOAD_NoWarn | LOAD_Quiet);
		}

		UEdGraphNode* GroupNode = VibeCO::FindNode(ParentGraph, GroupNodeId, Result.Error);
		if (!GroupNode)
		{
			return Result;
		}
		UClass* GroupClass = FindObject<UClass>(nullptr, ObjectGroupClassPath);
		if (!GroupClass || !GroupNode->IsA(GroupClass))
		{
			Result.Error = FString::Printf(TEXT("%s in %s is a %s, not a Group node (CustomizableObjectNodeObjectGroup). "
				"Child objects attach to a Group."), *GroupNodeId, *ParentAssetPath, *GroupNode->GetClass()->GetName());
			return Result;
		}
		GroupId = GroupNode->NodeGuid;
	}

	FScopedTransaction Transaction(LOCTEXT("SetParentObject", "Set Customizable Object Parent"));
	Base->Modify();
	Base->PreEditChange(ParentProperty);
	ParentProperty->SetObjectPropertyValue_InContainer(Base, Parent);
	*GroupProperty->ContainerPtrToValuePtr<FGuid>(Base) = GroupId;
	// The Object node derives bIsChildObject from ParentObject here.
	FPropertyChangedEvent ChangedEvent(ParentProperty, EPropertyChangeType::ValueSet);
	Base->PostEditChangeProperty(ChangedEvent);

	Result.Pins = VibeCO::DescribePins(Base);
	Result.Error = VibeCO::CommitEdit(Child, ChildGraph);
	Result.bSuccess = Result.Error.IsEmpty();
	return Result;
}

FVibeCOCompileResult UCustomizableObjectService::CompileObject(const FString& AssetPath,
	const FString& OptimizationLevel, const FString& TextureCompression)
{
	FVibeCOCompileResult Result;
	Result.State = TEXT("Failed");
	UObject* Object = nullptr;
	UEdGraph* Graph = nullptr;
	Result.Error = VibeCO::OpenObject(AssetPath, Object, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}

	// Unsaved edits made outside this service (a human in the graph editor with the asset editor
	// closed) would compile under a VersionId that already has cached data.
	if (Object->GetOutermost()->IsDirty())
	{
		VibeCO::BumpVersionId(Object);
	}

	const double Start = FPlatformTime::Seconds();
	VibeCO::Compile(Object, OptimizationLevel.TrimStartAndEnd(), TextureCompression.TrimStartAndEnd(), Result);
	Result.DurationSeconds = static_cast<float>(FPlatformTime::Seconds() - Start);
	return Result;
}

#undef LOCTEXT_NAMESPACE
