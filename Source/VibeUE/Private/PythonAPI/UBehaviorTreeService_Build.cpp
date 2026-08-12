// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UBehaviorTreeService.h"

#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_SimpleParallel.h"
#include "BehaviorTreeServiceInternal.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraphPin.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * BuildTree: a JSON walk over the primitives, and nothing else.
 *
 * Every write below goes through a public entry point of this service — AddNode, AddDecorator,
 * AddService, RemoveNode, RemoveSubNode, SetNodePropertyValue, SetNodeBlackboardKey. Nothing here
 * touches pins, sub-node arrays or the graph, so the whole engine-hazard argument in the header of
 * UBehaviorTreeService_Edit.cpp (why AddSubNode / BreakAllNodeLinks / DestroyNode / AutoArrange are
 * never called) is inherited rather than restated, and cannot be regressed here by accident.
 *
 * The graph IS read directly, in two places, both read-only: to see whether the target already has a
 * top-level node (and of what class) before deciding what bReplaceExisting means, and to pick the
 * next child to remove while clearing. Reading through GetTree's JSON instead would work and would be
 * slower and less precise; neither read calls Modify(), dirties a package or commits.
 *
 * Three things the shape of the JSON does NOT tell you, and which this therefore works out for
 * itself:
 *
 *  1. "children" is COMPACTED, not slot-indexed. VibeBT::GetChildNodes walks linked pins, so a
 *     SimpleParallel with only a background branch reports it at array position 0 while it occupies
 *     fixed slot 1. Array position is therefore not a child index, and neither is the "[n]" in the
 *     node's path — that counts SAME-NAMED SIBLINGS within the same compacted list
 *     (VibeBT::SegmentFor), so a lone background Wait is "Wait[0]" whatever slot it is in. What
 *     actually resolves it is that an empty background pin does not survive a save:
 *     UBehaviorTreeGraph::SpawnMissingNodesForParallel refills it with a Wait task on every OnSave
 *     (BehaviorTreeGraph.cpp:1015-1074), so a parallel read out of a saved asset has slot 1 occupied
 *     always, and its lone child — when it has one — is the background branch. See ChildSlotFor.
 *
 *  2. Nothing may be replayed onto an INJECTED node. They are the engine's read-only copies of a
 *     RunBehavior subtree's nodes; every mutator refuses them, and UpdateInjectedNodes regenerates
 *     them from the subtree asset anyway. "bInjected" is in the JSON precisely so this can skip them.
 *
 *  3. A path can go stale mid-build, and a stale path silently names a different node. Two rules keep
 *     that from happening here: a node's own properties are written LAST, after its sub-nodes and its
 *     whole subtree, because a NodeName write renames the node and the name IS its path segment; and
 *     the clear loop re-resolves the graph and re-reads the target's GUID through GetNodeInfo before
 *     every RemoveNode, because RemoveNode takes a whole subtree with it and a path that has shifted
 *     by one would take the wrong one.
 */

// Named, not anonymous: this module builds with unity/jumbo enabled, where two anonymous namespaces
// in the same blob collide on identical helper names — the second definition silently wins and the
// other translation unit calls it instead of its own.
namespace VibeBTBuild
{
	/**
	 * Iteration cap on the clear loops. They are already bounded by the fact that each pass removes a
	 * node, but a bound that does not depend on the loop body being correct is what turns "the graph
	 * is malformed in a way nobody anticipated" into an error rather than a hung editor.
	 */
	constexpr int32 kMaxClearIterations = 4096;

	// -------------------------------------------------------------------------------------------
	//  JSON accessors. All of them tolerate a missing or wrongly-typed field: the JSON is an input,
	//  and a hand-written one is a caller error to report, not a crash.
	// -------------------------------------------------------------------------------------------

	FString StringField(const TSharedPtr<FJsonObject>& Node, const TCHAR* Field)
	{
		FString Value;
		return (Node.IsValid() && Node->TryGetStringField(Field, Value)) ? Value : FString();
	}

	FString ClassOf(const TSharedPtr<FJsonObject>& Node)
	{
		return StringField(Node, TEXT("class"));
	}

	FString PathOf(const TSharedPtr<FJsonObject>& Node)
	{
		return StringField(Node, TEXT("path"));
	}

	bool IsInjected(const TSharedPtr<FJsonObject>& Node)
	{
		bool bValue = false;
		return Node.IsValid() && Node->TryGetBoolField(TEXT("bInjected"), bValue) && bValue;
	}

	/** The node objects under Field, in order, skipping any array entry that is not an object. */
	TArray<TSharedPtr<FJsonObject>> NodeArray(const TSharedPtr<FJsonObject>& Node, const TCHAR* Field)
	{
		TArray<TSharedPtr<FJsonObject>> Out;
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!Node.IsValid() || !Node->TryGetArrayField(Field, Array))
		{
			return Out;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Object) && Object && Object->IsValid())
			{
				Out.Add(*Object);
			}
		}
		return Out;
	}

	/**
	 * The field names of a JSON object, sorted.
	 *
	 * Written out rather than TMap::GetKeys because FJsonObject's key type is UE::FSharedString in
	 * this engine version, not FString (JsonObject.h:99), so GetKeys deduces nothing for a
	 * TArray<FString>. Sorted so that two runs of the same build write the same things in the same
	 * order and a failure is reproducible — FJsonObject's map order is not.
	 */
	TArray<FString> SortedFieldNames(const TSharedPtr<FJsonObject>& Object)
	{
		TArray<FString> Names;
		if (Object.IsValid())
		{
			Names.Reserve(Object->Values.Num());
			for (const auto& Pair : Object->Values)
			{
				Names.Add(FString(*Pair.Key));
			}
			Names.Sort();
		}
		return Names;
	}

	/** A node's "properties" map, or an invalid pointer when it has none. */
	TSharedPtr<FJsonObject> PropertiesOf(const TSharedPtr<FJsonObject>& Node)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (Node.IsValid() && Node->TryGetObjectField(TEXT("properties"), Object) && Object)
		{
			return *Object;
		}
		return nullptr;
	}

	TSharedPtr<FJsonObject> ParseJson(const FString& Json)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Object))
		{
			return nullptr;
		}
		return Object;
	}

	// -------------------------------------------------------------------------------------------
	//  Result bookkeeping.
	// -------------------------------------------------------------------------------------------

	/**
	 * Start an entry for Source and return its INDEX.
	 *
	 * An index, never a reference: the walk keeps appending to Result.Nodes while a node is being
	 * built (its sub-nodes, its whole subtree), and a reference into a TArray does not survive the
	 * reallocation that follows.
	 */
	int32 BeginNode(FBTBuildResult& Result, const TSharedPtr<FJsonObject>& Source)
	{
		FBTBuildNodeResult Entry;
		Entry.Path = PathOf(Source);
		Entry.bSuccess = true;
		return Result.Nodes.Add(MoveTemp(Entry));
	}

	/** Fail an entry, accumulating rather than overwriting — one node can fail several properties. */
	void FailNode(FBTBuildResult& Result, int32 Index, const FString& Error)
	{
		FBTBuildNodeResult& Entry = Result.Nodes[Index];
		Entry.bSuccess = false;
		Entry.Error = Entry.Error.IsEmpty() ? Error : Entry.Error + TEXT("; ") + Error;
	}

	/** Explain an entry that succeeded without being built the ordinary way. */
	void NoteNode(FBTBuildResult& Result, int32 Index, const FString& Note)
	{
		Result.Nodes[Index].Error = Note;
	}

	/**
	 * Record Source's descendants as not built, with the reason. Called when a node itself could not
	 * be created: everything under it is unreachable, and saying so per node is the difference between
	 * a report and a silence.
	 *
	 * Depth-bounded like every walk over TreeJson: this one is the cheapest stack overflow in the
	 * file — a hostile dump whose top-level class does not resolve sends the ENTIRE remaining JSON
	 * through here, with no engine work per level to slow the descent.
	 */
	void RecordSubtreeUnbuilt(FBTBuildResult& Result, const TSharedPtr<FJsonObject>& Source,
		const FString& Reason, int32 Depth = 0)
	{
		if (Depth > VibeBT::MaxTreeDepth)
		{
			const int32 Index = BeginNode(Result, Source);
			FailNode(Result, Index, FString::Printf(
				TEXT("report truncated below this node: the JSON nests deeper than %d levels"),
				VibeBT::MaxTreeDepth));
			return;
		}
		for (const TCHAR* Field : { TEXT("decorators"), TEXT("services"), TEXT("children") })
		{
			for (const TSharedPtr<FJsonObject>& Child : NodeArray(Source, Field))
			{
				const int32 Index = BeginNode(Result, Child);
				FailNode(Result, Index, Reason);
				RecordSubtreeUnbuilt(Result, Child, Reason, Depth + 1);
			}
		}
	}

	// -------------------------------------------------------------------------------------------
	//  Properties.
	// -------------------------------------------------------------------------------------------

	/**
	 * The value of a TOP-LEVEL member of a struct literal: ExtractStructMember("(A=1,B=(A=2))", "A")
	 * is "1", never "2". Depth- and quote-aware, so a comma or parenthesis inside a nested struct or
	 * inside a quoted string is not read as a delimiter. Surrounding quotes are stripped from the
	 * result. An empty string means the literal does not name that member.
	 *
	 * Used for exactly one thing: reading SelectedKeyName out of an exported FBlackboardKeySelector,
	 * because SetNodeBlackboardKey takes a key NAME and the JSON carries the whole struct. Importing
	 * the literal into a scratch FBlackboardKeySelector would be the obvious alternative and is worse:
	 * its AllowedTypes are instanced UBlackboardKeyType objects, so a struct import with no owner
	 * object either fabricates objects in the transient package or fails, to read one FName.
	 */
	FString ExtractStructMember(const FString& Literal, const FString& MemberName)
	{
		const TCHAR* const Text = *Literal;
		const int32 Length = Literal.Len();

		int32 Index = 0;
		while (Index < Length && FChar::IsWhitespace(Text[Index]))
		{
			++Index;
		}
		if (Index >= Length || Text[Index] != TCHAR('('))
		{
			return FString();
		}
		++Index;

		while (Index < Length)
		{
			const int32 NameStart = Index;
			while (Index < Length && Text[Index] != TCHAR('=') && Text[Index] != TCHAR(',')
				&& Text[Index] != TCHAR(')'))
			{
				++Index;
			}
			if (Index >= Length || Text[Index] != TCHAR('='))
			{
				return FString();
			}
			const FString Name = Literal.Mid(NameStart, Index - NameStart).TrimStartAndEnd();
			++Index; // '='

			const int32 ValueStart = Index;
			int32 Depth = 0;
			bool bInQuotes = false;
			while (Index < Length)
			{
				const TCHAR Character = Text[Index];
				if (bInQuotes)
				{
					if (Character == TCHAR('\\'))
					{
						++Index; // an escaped character is never a delimiter
					}
					else if (Character == TCHAR('"'))
					{
						bInQuotes = false;
					}
				}
				else if (Character == TCHAR('"'))
				{
					bInQuotes = true;
				}
				else if (Character == TCHAR('('))
				{
					++Depth;
				}
				else if (Character == TCHAR(')'))
				{
					if (Depth == 0)
					{
						break;
					}
					--Depth;
				}
				else if (Character == TCHAR(',') && Depth == 0)
				{
					break;
				}
				++Index;
			}

			if (Name == MemberName)
			{
				FString Value = Literal.Mid(ValueStart, Index - ValueStart).TrimStartAndEnd();
				if (Value.Len() >= 2 && Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\"")))
				{
					Value = Value.Mid(1, Value.Len() - 2);
				}
				return Value;
			}

			if (Index >= Length || Text[Index] == TCHAR(')'))
			{
				return FString();
			}
			++Index; // the ','
		}
		return FString();
	}

	/** Names of NodePath's properties that are blackboard key selectors, i.e. the ones that must not
	 *  go through SetNodePropertyValue. Asked of the TARGET node, because the JSON carries no types. */
	TSet<FString> KeySelectorProperties(const FString& AssetPath, const FString& NodePath)
	{
		TSet<FString> Names;
		for (const FBTPropertyInfo& Info : UBehaviorTreeService::GetNodePropertyNames(AssetPath, NodePath))
		{
			if (Info.bIsBlackboardKeySelector)
			{
				Names.Add(Info.Name);
			}
		}
		return Names;
	}

	/**
	 * Write Source's "properties" onto the node at NodePath, recording every failure on its entry.
	 *
	 * Keys are applied in sorted order rather than in FJsonObject's map order, so two runs of the same
	 * build write the same things in the same sequence and a failure is reproducible — but blackboard
	 * key selectors go FIRST, ahead of the ordinary values, whatever the alphabet says. A node's
	 * remaining properties can depend on what its selectors are bound to (a value is validated, or
	 * re-derived, against the currently bound key); the reverse never happens, because binding a key
	 * only ever reads the selector's own allowed types. Applying them in name order interleaves the
	 * two, so a value could land while the node was still unbound and be rejected — recorded as a
	 * failed entry in FBTBuildResult for a build that was in fact perfectly authorable, and dependent
	 * on nothing but the property's spelling.
	 */
	void ApplyProperties(const FString& AssetPath, const FString& NodePath,
		const TSharedPtr<FJsonObject>& Source, FBTBuildResult& Result, int32 EntryIndex)
	{
		const TSharedPtr<FJsonObject> Properties = PropertiesOf(Source);
		if (!Properties.IsValid() || Properties->Values.Num() == 0)
		{
			return;
		}

		const TSet<FString> Selectors = KeySelectorProperties(AssetPath, NodePath);

		// Partitioned three ways, not sorted twice: within each partition the alphabetical order
		// from SortedFieldNames survives, so the sequence is still fully determined by the JSON.
		//
		// NodeName goes LAST, unconditionally. Writing it retitles the node, and the title is the
		// node's path segment — so every property applied after it through NodePath would resolve
		// against a stale path: loudly ("No node at path") when nothing else matches, or SILENTLY
		// onto a same-titled sibling when something does (a renamed Wait beside the engine-spawned
		// background Wait of a SimpleParallel). Alphabetical order made this bite any renamed node
		// with an edited property sorting after "NodeName" — WaitTime, RandomDeviation — which is
		// most renamed nodes in a real tree. The file-header rule ("a node's own properties are
		// written last") held BETWEEN nodes but not WITHIN this function.
		TArray<FString> Names;
		TArray<FString> ValueNames;
		bool bHasNodeName = false;
		for (const FString& Name : SortedFieldNames(Properties))
		{
			if (Name == TEXT("NodeName"))
			{
				bHasNodeName = true;
				continue;
			}
			(Selectors.Contains(Name) ? Names : ValueNames).Add(Name);
		}
		Names.Append(MoveTemp(ValueNames));
		if (bHasNodeName)
		{
			Names.Add(TEXT("NodeName"));
		}

		for (const FString& Name : Names)
		{
			FString Value;
			if (!Properties->TryGetStringField(Name, Value))
			{
				FailNode(Result, EntryIndex, FString::Printf(
					TEXT("property '%s' is not a string; GetTree emits every property value as a "
						 "string literal"), *Name));
				continue;
			}

			if (Selectors.Contains(Name))
			{
				// A key selector is a name plus a resolved key ID, and only the ID is read at runtime;
				// importing the literal would write the name and leave the node silently inert. See
				// SetNodeBlackboardKey.
				const FString Key = ExtractStructMember(Value, TEXT("SelectedKeyName"));
				if (Key.IsEmpty() || Key == TEXT("None"))
				{
					// An unbound selector: exactly the state the freshly created node is already in.
					continue;
				}
				const FBTPropertySetResult Bind =
					UBehaviorTreeService::SetNodeBlackboardKey(AssetPath, NodePath, Name, Key);
				if (!Bind.bSuccess)
				{
					FailNode(Result, EntryIndex,
						FString::Printf(TEXT("%s -> blackboard key '%s': %s"), *Name, *Key, *Bind.Error));
				}
				continue;
			}

			const FBTPropertySetResult Write =
				UBehaviorTreeService::SetNodePropertyValue(AssetPath, NodePath, Name, Value);
			if (!Write.bSuccess)
			{
				FailNode(Result, EntryIndex,
					FString::Printf(TEXT("%s = '%s': %s"), *Name, *Value, *Write.Error));
			}
		}
	}

	// -------------------------------------------------------------------------------------------
	//  The walk.
	// -------------------------------------------------------------------------------------------

	/** True when ClassName names a SimpleParallel — the one composite whose children occupy fixed
	 *  slots. Subclasses included, which is why this resolves the class rather than comparing names. */
	bool IsSimpleParallel(const FString& ClassName)
	{
		if (ClassName.IsEmpty())
		{
			return false;
		}
		const UClass* Class = VibeBT::ResolveNodeClass(ClassName, UBTNode::StaticClass());
		return Class && Class->IsChildOf(UBTComposite_SimpleParallel::StaticClass());
	}

	/**
	 * The ChildIndex to hand AddNode for the ArrayPosition'th entry of a parent's "children".
	 *
	 * Ordinary composites hold an ordered list, so appending in JSON order reproduces it exactly.
	 * A SimpleParallel has two fixed slots and a compacted children array: two entries are slots
	 * [0] (main task) and [1] (background) in order, and a single entry is the BACKGROUND branch —
	 * see the file header for why slot 1 is the one that is never empty in a saved asset.
	 */
	int32 ChildSlotFor(bool bParentIsSimpleParallel, int32 ChildCount, int32 ArrayPosition)
	{
		if (!bParentIsSimpleParallel)
		{
			return -1;
		}
		return (ChildCount >= 2) ? ArrayPosition : 1;
	}

	void ReplayInto(const FString& AssetPath, const FString& TargetPath,
		const TSharedPtr<FJsonObject>& Source, int32 EntryIndex, FBTBuildResult& Result, int32 Depth);

	/**
	 * Write Source's "comment" — the graph node's comment bubble — onto the node at TargetPath.
	 * An empty comment is not replayed: a fresh node already has none, and SetNodeComment costs a
	 * commit. Runs BEFORE ApplyProperties, while TargetPath is still guaranteed to name the node
	 * (a NodeName replay retitles it).
	 */
	void ApplyComment(const FString& AssetPath, const FString& TargetPath,
		const TSharedPtr<FJsonObject>& Source, FBTBuildResult& Result, int32 EntryIndex)
	{
		const FString Comment = StringField(Source, TEXT("comment"));
		if (Comment.IsEmpty())
		{
			return;
		}
		const FString Error = UBehaviorTreeService::SetNodeComment(AssetPath, TargetPath, Comment);
		if (!Error.IsEmpty())
		{
			FailNode(Result, EntryIndex,
				FString::Printf(TEXT("comment = '%s': %s"), *Comment, *Error));
		}
	}

	/** Attach one of Source's sub-node arrays to the node at TargetPath, in listed order. */
	void ReplaySubNodes(const FString& AssetPath, const FString& TargetPath,
		const TSharedPtr<FJsonObject>& Source, bool bDecorators, FBTBuildResult& Result)
	{
		const TCHAR* const Field = bDecorators ? TEXT("decorators") : TEXT("services");
		for (const TSharedPtr<FJsonObject>& Sub : NodeArray(Source, Field))
		{
			const int32 Index = BeginNode(Result, Sub);

			if (IsInjected(Sub))
			{
				NoteNode(Result, Index,
					TEXT("skipped: injected from a subtree. The engine regenerates injected sub-nodes "
						 "from the subtree asset, and every mutator refuses them."));
				continue;
			}

			const FString ClassName = ClassOf(Sub);
			if (ClassName.IsEmpty())
			{
				FailNode(Result, Index,
					TEXT("carries no class name. A sub-node with no class is a composite "
						 "(logic-operator) decorator — an editor-only sub-graph with no UBTDecorator "
						 "class to name — which BuildTree cannot recreate; author it in the Behavior "
						 "Tree editor."));
				continue;
			}

			// Appended in order, and deliberately not by index: AddDecorator clamps an authored
			// decorator to before the first injected one, so appending is the position it keeps.
			const FString SubPath = bDecorators
				? UBehaviorTreeService::AddDecorator(AssetPath, TargetPath, ClassName, -1)
				: UBehaviorTreeService::AddService(AssetPath, TargetPath, ClassName, -1);
			if (SubPath.StartsWith(TEXT("ERROR")))
			{
				FailNode(Result, Index, SubPath);
				continue;
			}

			ApplyComment(AssetPath, SubPath, Sub, Result, Index);
			ApplyProperties(AssetPath, SubPath, Sub, Result, Index);
		}
	}

	/** Create Source's children under the node at TargetPath and recurse into each. */
	void ReplayChildren(const FString& AssetPath, const FString& TargetPath,
		const TSharedPtr<FJsonObject>& Source, FBTBuildResult& Result, int32 Depth)
	{
		const TArray<TSharedPtr<FJsonObject>> Children = NodeArray(Source, TEXT("children"));
		if (Children.Num() == 0)
		{
			return;
		}

		// The bound turns a hostile or malformed dump's linear ten-thousand-node chain into a
		// per-node failure report instead of a stack overflow. Legitimate GetTree output cannot
		// reach it: GetTree truncates its own serialisation at the same depth.
		if (Depth > VibeBT::MaxTreeDepth)
		{
			for (const TSharedPtr<FJsonObject>& Child : Children)
			{
				const int32 Index = BeginNode(Result, Child);
				FailNode(Result, Index, FString::Printf(
					TEXT("not built: the JSON nests deeper than %d levels, which no legitimate "
						 "Behavior Tree does"), VibeBT::MaxTreeDepth));
				RecordSubtreeUnbuilt(Result, Child, TEXT("not built: below the depth bound"), Depth);
			}
			return;
		}

		const bool bParallel = IsSimpleParallel(ClassOf(Source));

		for (int32 Position = 0; Position < Children.Num(); ++Position)
		{
			const TSharedPtr<FJsonObject>& Child = Children[Position];
			const int32 Index = BeginNode(Result, Child);

			if (IsInjected(Child))
			{
				NoteNode(Result, Index,
					TEXT("skipped: injected from a subtree, along with its subtree. The engine "
						 "regenerates injected nodes from the subtree asset."));
				continue;
			}

			const FString ClassName = ClassOf(Child);
			if (ClassName.IsEmpty())
			{
				FailNode(Result, Index, TEXT("carries no class name, so there is nothing to create"));
				RecordSubtreeUnbuilt(Result, Child,
					FString::Printf(TEXT("not built: its parent '%s' was not created"), *PathOf(Child)));
				continue;
			}

			const FString ChildPath = UBehaviorTreeService::AddNode(AssetPath, TargetPath, ClassName,
				ChildSlotFor(bParallel, Children.Num(), Position));
			if (ChildPath.StartsWith(TEXT("ERROR")))
			{
				FailNode(Result, Index, ChildPath);
				RecordSubtreeUnbuilt(Result, Child,
					FString::Printf(TEXT("not built: its parent '%s' was not created"), *PathOf(Child)));
				continue;
			}

			ReplayInto(AssetPath, ChildPath, Child, Index, Result, Depth + 1);
		}
	}

	/**
	 * Replay Source onto the node that already exists at TargetPath.
	 *
	 * Order is load bearing. Sub-nodes and children are built first and the node's OWN properties are
	 * written last, because one of those properties is UBTNode::NodeName — and the name is the node's
	 * path segment, so writing it makes TargetPath stop resolving to this node (and possibly start
	 * resolving to another). Everything that needs TargetPath therefore happens before anything can
	 * invalidate it. Sub-node paths are unaffected either way: they are addressed by slot
	 * ("@decorator[n]"), not by name.
	 */
	void ReplayInto(const FString& AssetPath, const FString& TargetPath,
		const TSharedPtr<FJsonObject>& Source, int32 EntryIndex, FBTBuildResult& Result, int32 Depth)
	{
		ApplyComment(AssetPath, TargetPath, Source, Result, EntryIndex);
		ReplaySubNodes(AssetPath, TargetPath, Source, /*bDecorators*/ true, Result);
		ReplaySubNodes(AssetPath, TargetPath, Source, /*bDecorators*/ false, Result);
		ReplayChildren(AssetPath, TargetPath, Source, Result, Depth);
		ApplyProperties(AssetPath, TargetPath, Source, Result, EntryIndex);
	}

	// -------------------------------------------------------------------------------------------
	//  Clearing an existing tree (bReplaceExisting).
	// -------------------------------------------------------------------------------------------

	/** True when Child hangs off Parent's second output pin, i.e. it is a SimpleParallel's background
	 *  branch — which RemoveNode refuses, because the engine refills that pin with a Wait on save. */
	bool IsParallelBackground(UBehaviorTreeGraphNode* Parent, UBehaviorTreeGraphNode* Child)
	{
		if (!Parent || !Child || !Parent->IsA<UBehaviorTreeGraphNode_SimpleParallel>())
		{
			return false;
		}
		const UEdGraphPin* ChildIn = Child->GetInputPin();
		const UEdGraphPin* ParentPin =
			(ChildIn && ChildIn->LinkedTo.Num() > 0) ? ChildIn->LinkedTo[0] : nullptr;
		return ParentPin && ParentPin == Parent->GetOutputPin(1);
	}

	/**
	 * Remove everything under the node at TopPath that this service is allowed to remove: every child
	 * subtree, and the node's own authored decorators and services. Returns an empty string on
	 * success, else the error — in which case the asset is left as far as the clear got, which is
	 * why the caller checks whether the build can proceed BEFORE calling this.
	 *
	 * Two things are left behind, both by the engine's design rather than by omission: a node injected
	 * from a subtree (regenerated from the subtree asset, and refused by RemoveNode / RemoveSubNode),
	 * and a SimpleParallel top node's background branch (refilled by SpawnMissingNodesForParallel on
	 * every save; the replay REPLACES it, via AddNode into slot 1).
	 *
	 * Each pass re-resolves the top node and re-reads the victim's GUID through GetNodeInfo before
	 * removing it. That is not ceremony: paths are positional, RemoveNode removes an entire subtree,
	 * and each removal renumbers the siblings after it — so a path computed once and used twice is
	 * exactly how the wrong subtree gets deleted.
	 */
	FString ClearBelow(const FString& AssetPath, UBehaviorTreeGraph* Graph, const FString& TopPath)
	{
		bool bChildrenCleared = false;
		for (int32 Pass = 0; Pass < kMaxClearIterations; ++Pass)
		{
			UBehaviorTreeGraphNode* Top = VibeBT::ResolveNodePath(Graph, TopPath);
			if (!Top)
			{
				return FString::Printf(
					TEXT("the top-level node '%s' stopped resolving while clearing the tree"), *TopPath);
			}

			UBehaviorTreeGraphNode* Victim = nullptr;
			for (UBehaviorTreeGraphNode* Child : VibeBT::GetChildNodes(Top))
			{
				if (Child && !Child->bInjectedNode && !IsParallelBackground(Top, Child))
				{
					Victim = Child;
					break;
				}
			}
			if (!Victim)
			{
				bChildrenCleared = true;
				break;
			}

			const FString VictimPath = VibeBT::GetNodePath(Victim);
			const FString VictimGuid = Victim->NodeGuid.ToString();
			FBTNodeInfo Info;
			if (!UBehaviorTreeService::GetNodeInfo(AssetPath, VictimPath, Info) || Info.Guid != VictimGuid)
			{
				return FString::Printf(
					TEXT("'%s' no longer names the node it was computed for (expected %s, found %s); "
						 "refusing to remove a subtree through a stale path"),
					*VictimPath, *VictimGuid, *Info.Guid);
			}

			const FString RemoveError = UBehaviorTreeService::RemoveNode(AssetPath, VictimPath);
			if (!RemoveError.IsEmpty())
			{
				return RemoveError;
			}
		}

		if (!bChildrenCleared)
		{
			return FString::Printf(TEXT("gave up removing the children of '%s' after %d passes"),
				*TopPath, kMaxClearIterations);
		}

		// Sub-nodes: removing one renumbers every later "@decorator[n]" / "@service[n]" on the same
		// node, so each pass takes the first removable slot and looks again.
		for (int32 Pass = 0; Pass < kMaxClearIterations; ++Pass)
		{
			UBehaviorTreeGraphNode* Top = VibeBT::ResolveNodePath(Graph, TopPath);
			if (!Top)
			{
				return FString::Printf(
					TEXT("the top-level node '%s' stopped resolving while clearing its sub-nodes"),
					*TopPath);
			}

			FString SubNodePath;
			FString SubNodeGuid;
			for (const TCHAR* Kind : { TEXT("@decorator"), TEXT("@service") })
			{
				const TArray<TObjectPtr<UBehaviorTreeGraphNode>>& Slots =
					(FCString::Strcmp(Kind, TEXT("@decorator")) == 0) ? Top->Decorators : Top->Services;
				for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
				{
					const UBehaviorTreeGraphNode* SubNode = ToRawPtr(Slots[Slot]);
					if (SubNode && !SubNode->bInjectedNode)
					{
						SubNodePath = FString::Printf(TEXT("%s/%s[%d]"), *TopPath, Kind, Slot);
						SubNodeGuid = SubNode->NodeGuid.ToString();
						break;
					}
				}
				if (!SubNodePath.IsEmpty())
				{
					break;
				}
			}
			if (SubNodePath.IsEmpty())
			{
				return FString();
			}

			FBTNodeInfo Info;
			if (!UBehaviorTreeService::GetNodeInfo(AssetPath, SubNodePath, Info)
				|| Info.Guid != SubNodeGuid)
			{
				return FString::Printf(
					TEXT("'%s' no longer names the sub-node it was computed for (expected %s, found %s)"),
					*SubNodePath, *SubNodeGuid, *Info.Guid);
			}

			const FString RemoveError = UBehaviorTreeService::RemoveSubNode(AssetPath, SubNodePath);
			if (!RemoveError.IsEmpty())
			{
				return RemoveError;
			}
		}

		return FString::Printf(TEXT("gave up clearing '%s' after %d passes"), *TopPath,
			kMaxClearIterations);
	}

	/**
	 * What the RETAINED top-level node still carries that TreeJson did not ask for, or an empty string
	 * when it carries exactly what was asked for.
	 *
	 * The one place a build can produce a hybrid: bReplaceExisting cannot delete the top-level
	 * composite (RemoveNode refuses the root's only child), so it is reused, and a property it already
	 * had that the JSON does not mention is still on it afterwards. Rather than document that and hope,
	 * the node's edited properties are re-read from the finished asset and compared with the JSON's,
	 * in both directions — extra, missing and differing keys are all divergences.
	 */
	FString RetainedTopDivergence(const FString& AssetPath, const TSharedPtr<FJsonObject>& SourceTop)
	{
		const TSharedPtr<FJsonObject> Dump = ParseJson(UBehaviorTreeService::GetTree(AssetPath));
		const TArray<TSharedPtr<FJsonObject>> Tops = NodeArray(Dump, TEXT("children"));
		if (Tops.Num() != 1)
		{
			return TEXT("could not re-read the built tree to verify the retained top-level node");
		}

		const TSharedPtr<FJsonObject> Built = PropertiesOf(Tops[0]);
		const TSharedPtr<FJsonObject> Wanted = PropertiesOf(SourceTop);

		TSet<FString> Names;
		Names.Append(SortedFieldNames(Built));
		Names.Append(SortedFieldNames(Wanted));

		TArray<FString> Sorted = Names.Array();
		Sorted.Sort();

		TArray<FString> Divergences;
		for (const FString& Name : Sorted)
		{
			const FString BuiltValue = Built.IsValid() ? StringField(Built, *Name) : FString();
			const FString WantedValue = Wanted.IsValid() ? StringField(Wanted, *Name) : FString();
			if (BuiltValue != WantedValue)
			{
				Divergences.Add(FString::Printf(TEXT("%s is '%s' but the JSON asked for '%s'"),
					*Name, *BuiltValue, *WantedValue));
			}
		}
		if (Divergences.Num() == 0)
		{
			return FString();
		}

		return FString::Printf(
			TEXT("retained rather than rebuilt (the tree's top-level node cannot be removed), and it "
				 "does not match what was asked for: %s. Build into a fresh asset for an exact copy."),
			*FString::Join(Divergences, TEXT("; ")));
	}
}

using namespace VibeBTBuild;

FBTBuildResult UBehaviorTreeService::BuildTree(const FString& AssetPath, const FString& TreeJson,
	bool bReplaceExisting)
{
	FBTBuildResult Result;

	// --- The source JSON. ------------------------------------------------------------------------
	const TSharedPtr<FJsonObject> SourceRoot = ParseJson(TreeJson);
	if (!SourceRoot.IsValid())
	{
		Result.Error = TEXT("TreeJson is not a JSON object. Pass what GetTree returns.");
		return Result;
	}

	FString SourceError;
	if (SourceRoot->TryGetStringField(TEXT("error"), SourceError) && !SourceError.IsEmpty())
	{
		// GetTree reports its own failures as a parseable object with an "error" field and an empty
		// "children" array. Building from one would look exactly like building from an empty tree.
		Result.Error = FString::Printf(
			TEXT("TreeJson is a GetTree failure result, not a tree: %s"), *SourceError);
		return Result;
	}
	if (!ClassOf(SourceRoot).IsEmpty())
	{
		Result.Error = FString::Printf(
			TEXT("TreeJson must be the whole object GetTree returns — the graph's Root node, whose "
				 "\"class\" is empty — rather than one node out of it (this one is %s)."),
			*ClassOf(SourceRoot));
		return Result;
	}

	const TArray<TSharedPtr<FJsonObject>> SourceTops = NodeArray(SourceRoot, TEXT("children"));
	if (SourceTops.Num() == 0)
	{
		Result.Error = TEXT("TreeJson's root has no child, so there is no tree to build. An empty tree "
							"is not something this can produce either: the root's only child cannot be "
							"removed, because a Behavior Tree with no runtime root cannot be saved.");
		return Result;
	}
	if (SourceTops.Num() > 1)
	{
		Result.Error = FString::Printf(
			TEXT("TreeJson's root has %d children. A Behavior Tree root takes exactly one, and the "
				 "extra ones could not be linked, so nothing was built."), SourceTops.Num());
		return Result;
	}

	const TSharedPtr<FJsonObject> SourceTop = SourceTops[0];
	const FString TopClassName = ClassOf(SourceTop);
	if (TopClassName.IsEmpty())
	{
		Result.Error = TEXT("TreeJson's top-level node carries no class name, so there is nothing to "
							"create.");
		return Result;
	}

	// --- The target. -----------------------------------------------------------------------------
	// Asked once and up front, so a refusal costs no writes at all. This is the same guard every
	// mutator applies (editor open on the asset, locked graph, missing editor graph); the mutators
	// below will each apply it again, and it is cheap.
	UBehaviorTree* Tree = nullptr;
	UBehaviorTreeGraph* Graph = nullptr;
	Result.Error = VibeBT::OpenWriteGuard(AssetPath, Tree, Graph);
	if (!Result.Error.IsEmpty())
	{
		return Result;
	}

	UBehaviorTreeGraphNode_Root* TargetRoot = VibeBT::FindRootGraphNode(Graph);
	if (!TargetRoot)
	{
		Result.Error = FString::Printf(TEXT("%s has no root node in its editor graph"), *AssetPath);
		return Result;
	}

	const TArray<UBehaviorTreeGraphNode*> ExistingTops = VibeBT::GetChildNodes(TargetRoot);
	FString TopPath;

	if (ExistingTops.Num() == 0)
	{
		const int32 TopIndex = BeginNode(Result, SourceTop);
		TopPath = AddNode(AssetPath, TEXT("Root"), TopClassName, -1);
		if (TopPath.StartsWith(TEXT("ERROR")))
		{
			FailNode(Result, TopIndex, TopPath);
			RecordSubtreeUnbuilt(Result, SourceTop,
				TEXT("not built: the tree's top-level node was not created"));
			Result.Error = TEXT("the tree's top-level node could not be created; nothing below it was "
								"built either");
			return Result;
		}
		ReplayInto(AssetPath, TopPath, SourceTop, TopIndex, Result, 0);
	}
	else
	{
		UBehaviorTreeGraphNode* ExistingTop = ExistingTops[0];
		const UObject* ExistingInstance = ToRawPtr(ExistingTop->NodeInstance);
		const FString ExistingClassName =
			ExistingInstance ? ExistingInstance->GetClass()->GetName() : FString(TEXT("<none>"));

		if (!bReplaceExisting)
		{
			Result.Error = FString::Printf(
				TEXT("%s already has a top-level node (%s) and bReplaceExisting is false, so nothing "
					 "was written. Pass true to rebuild the tree below it, or build into a fresh "
					 "asset."),
				*AssetPath, *ExistingClassName);
			return Result;
		}

		// The class check comes BEFORE the clear, so a build that cannot be completed does not first
		// destroy the tree it was going to replace. There is no ReplaceNode, and RemoveNode refuses
		// the root's only child, so a top-level node of the wrong class is the end of it.
		const UClass* WantedClass = VibeBT::ResolveNodeClass(TopClassName, UBTNode::StaticClass());
		if (!WantedClass)
		{
			Result.Error = FString::Printf(
				TEXT("unknown or ambiguous Behavior Tree node class '%s' for the top-level node; "
					 "nothing was written."), *TopClassName);
			return Result;
		}
		if (!ExistingInstance || ExistingInstance->GetClass() != WantedClass)
		{
			Result.Error = FString::Printf(
				TEXT("%s's top-level node is %s and TreeJson's is %s. The top-level node cannot be "
					 "replaced — RemoveNode refuses the root's only child, because a tree with no "
					 "runtime root cannot be saved, and there is no ReplaceNode — so this build would "
					 "produce a hybrid rather than the tree that was asked for. Nothing was written; "
					 "build into a fresh asset instead."),
				*AssetPath, *ExistingClassName, *TopClassName);
			return Result;
		}

		TopPath = VibeBT::GetNodePath(ExistingTop);
		if (TopPath.IsEmpty())
		{
			Result.Error = FString::Printf(
				TEXT("%s's top-level node has no resolvable path"), *AssetPath);
			return Result;
		}

		// Captured BEFORE the clear, because the clear is destructive to disk one subtree at a
		// time: a build that fails midway has already removed real nodes, and without this dump
		// there is nothing to re-apply. It rides on the result rather than being logged so the
		// caller that hit the failure is the one holding the recovery data.
		Result.PreReplaceSnapshot = GetTree(AssetPath);

		const FString ClearError = ClearBelow(AssetPath, Graph, TopPath);
		if (!ClearError.IsEmpty())
		{
			Result.Error = FString::Printf(
				TEXT("could not clear %s before rebuilding it: %s. The asset HAS been modified — the "
					 "clear removes and saves one subtree at a time — and PreReplaceSnapshot holds the "
					 "tree as it was, so BuildTree(bReplaceExisting=true) with it restores the "
					 "original."),
				*AssetPath, *ClearError);
			return Result;
		}

		const int32 TopIndex = BeginNode(Result, SourceTop);
		ReplayInto(AssetPath, TopPath, SourceTop, TopIndex, Result, 0);

		// The retained node is the only node in the build that was not created from scratch, so it is
		// the only one that can still be carrying something nobody asked for.
		const FString Divergence = RetainedTopDivergence(AssetPath, SourceTop);
		if (!Divergence.IsEmpty())
		{
			FailNode(Result, TopIndex, Divergence);
		}
	}

	int32 Failed = 0;
	for (const FBTBuildNodeResult& Entry : Result.Nodes)
	{
		Failed += Entry.bSuccess ? 0 : 1;
	}
	if (Failed > 0)
	{
		Result.Error = FString::Printf(TEXT("%d of %d nodes failed; see Nodes for each one"), Failed,
			Result.Nodes.Num());
		if (!Result.PreReplaceSnapshot.IsEmpty())
		{
			// A failed replace is not "nothing happened": the target was cleared and partially
			// rebuilt, and each primitive saved as it went. Say so, and point at the way back.
			Result.Error += TEXT(". The target was modified as far as the build got; "
								 "PreReplaceSnapshot holds the tree as it was before the clear.");
		}
		return Result;
	}

	Result.bSuccess = true;
	return Result;
}
