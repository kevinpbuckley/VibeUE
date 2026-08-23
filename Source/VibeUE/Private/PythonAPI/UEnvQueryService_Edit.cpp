// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UEnvQueryService.h"

#if WITH_VIBEUE_EQS

#include "AIGraphNode.h"
#include "AIGraphTypes.h"
#include "DataProviders/AIDataProvider.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EnvQueryServiceInternal.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EnvironmentQueryGraph.h"
#include "EnvironmentQueryGraphNode.h"
#include "EnvironmentQueryGraphNode_Option.h"
#include "EnvironmentQueryGraphNode_Test.h"
#include "UObject/UnrealType.h"

/**
 * Structural writes to an Environment Query's editor graph.
 *
 * ===========================================================================================
 *  Why nothing here calls BreakAllNodeLinks / DestroyNode / RemoveNode(bBreakAllLinks = true)
 * ===========================================================================================
 *
 * UAIGraphNode::NodeConnectionListChanged() is not a deferred notification. It is:
 *
 *     void UAIGraphNode::NodeConnectionListChanged()
 *     {
 *         Super::NodeConnectionListChanged();
 *         GetAIGraph()->UpdateAsset();          // AIGraphNode.cpp:236-241
 *     }
 *
 * — a synchronous regeneration of UEnvQuery::Options from whatever the graph looks like at that
 * instant. UEdGraphNode::BreakAllNodeLinks() fires it on every node that lost a link, and
 * UEdGraph::RemoveNode() calls BreakAllNodeLinks() by default (EdGraph.cpp:261-281), as does
 * UEdGraphNode::DestroyNode().
 *
 * That matters because UpdateAsset opens with GetOptionsMutable().Reset() and ends in
 * RemoveOrphanedNodes(), which marks every no-longer-referenced UEnvQueryOption / UEnvQueryTest
 * RF_Transient and renames it into the transient package. Run it against a half-finished edit and
 * the loss is already permanent — before CommitGraph, and outside the discard guard, which brackets
 * only its own UpdateAsset() call and can neither see nor undo a regeneration that already
 * happened.
 *
 * The engine's pin-level API does exactly what is needed and notifies nothing:
 *
 *   - UEdGraphPin::MakeLinkTo / BreakAllPinLinks(bNotifyNodes = false) mutate the two LinkedTo
 *     arrays and stop (EdGraphPin.cpp:514, 705). UEnvironmentQueryGraph::SpawnMissingNodes links
 *     its own spawned option nodes with MakeLinkTo for the same reason (:442-445).
 *   - UEdGraph::AddNode and UEdGraph::RemoveNode(..., bBreakAllLinks = false) broadcast
 *     OnGraphChanged — an editor-UI signal with no listeners headlessly — and never touch
 *     UpdateAsset.
 *
 * So every function below rewires pins directly and lets CommitGraph perform the single
 * regeneration, once, over a graph that is valid again.
 *
 * ===========================================================================================
 *  ...and why nothing here calls UAIGraphNode::AddSubNode either
 * ===========================================================================================
 *
 * The same hazard, one class up. The engine attaches a test with
 * `OptionNode->AddSubNode(TestNode, Graph)` (EnvironmentQueryGraph.cpp:463), and AddSubNode ends:
 *
 *     SubNodes.Add(SubNode);
 *     OnSubNodeAdded(SubNode);
 *     ParentGraph->NotifyGraphChanged();
 *     GetAIGraph()->UpdateAsset();          // AIGraphNode.cpp:294-298
 *
 * — the same synchronous regeneration, before CommitGraph, outside the discard guard and outside the
 * sticky-poison check, on a graph that is mid-edit by construction. It also cannot take an index:
 * it always appends, so it could not implement AddTest's Index argument anyway.
 *
 * What the rest of that API does in 5.8, verified rather than assumed, because the BehaviorTree
 * counterparts differ:
 *
 *   - UAIGraphNode::OnSubNodeAdded / OnSubNodeRemoved are empty in the base class
 *     (AIGraphNode.cpp:301-304, 318-321) and UEnvironmentQueryGraphNode_Option overrides NEITHER, so
 *     neither notifies anything here. (UBehaviorTreeGraphNode overrides both.)
 *   - UAIGraphNode::InsertSubNodeAt(SubNode, DropIndex) is `SubNodes.Insert` for DropIndex > -1 and
 *     `SubNodes.Add` otherwise (AIGraphNode.cpp:330-340) — safe, and no packed index: the
 *     three-field packing is UBehaviorTreeGraphNode's override, and EQS has no override at all.
 *   - UAIGraphNode::RemoveSubNode is Modify() + RemoveSingle + OnSubNodeRemoved (:306-312) — also
 *     safe, but it does not clear ParentNode and finds its target by value rather than by the index
 *     the caller resolved.
 *
 * The array is manipulated directly below regardless: it is exactly what those two would do, it
 * cannot be changed out from under this code by a future override, and it lets a resolved index be
 * used as an index.
 *
 * Sub-nodes are deliberately NOT added to Graph->Nodes, and the reason is not the one the sibling
 * BehaviorTree plan records. UEnvironmentQueryGraph::UpdateAsset does not clear ParentNode for
 * entries in Nodes — it never touches ParentNode except to ASSIGN it from a parent option
 * (EnvironmentQueryGraph.cpp:87, :250), verified by grepping the whole file. What is true here is
 * simpler: the engine never lists a sub-node in Nodes (SpawnMissingSubNodes attaches through
 * SubNodes alone, EnvironmentQueryGraph.cpp:449-466), UAIGraph::CollectAllNodeInstances already
 * reaches every sub-node's instance through its parent's SubNodes array (AIGraph.cpp:197-203) so
 * listing it buys no orphan protection, and Nodes is what the graph editor draws — an entry there is
 * a free-floating node a human would see beside the option it is supposed to be inside.
 *
 * ===========================================================================================
 *  Order is position, position is link order
 * ===========================================================================================
 *
 * UpdateAsset sorts the root's links by node X position (FCompareNodeXLocation,
 * EnvironmentQueryGraph.cpp:65) and writes the options out in that order, so option order is node X
 * position and nothing else. CommitGraph runs VibeEQS::ArrangeGraph first, which lays the root's
 * links out left to right in *link* order — which is what turns the LinkedTo array manipulated here
 * into the order the query actually runs. Insert at index 0 without that pass and the new option is
 * merely present; with it, it is first.
 *
 * ===========================================================================================
 *  What a failed commit leaves behind
 * ===========================================================================================
 *
 * Each function refuses up front for every reason it can detect (unknown class, unresolvable path,
 * a removal that would empty the query), so by the time it mutates, the only way the commit fails
 * is a locked graph or a failed package save. The in-memory graph then holds the edit while the
 * file does not, and it is not rolled back: nothing was written, the next successful commit writes
 * it, and reopening the asset (OpenWriteGuard -> Initialize -> SpawnMissingNodes) reconciles the two
 * from UEnvQuery::Options. Recorded rather than papered over — an atomic rollback would need to
 * restore link ORDER as well as membership, and an untested rollback path is a worse hazard than a
 * documented one.
 */
namespace VibeEQSEdit
{
	/**
	 * AddOption and AddTest return a path on success, so their failures need a prefix a caller can
	 * test. Every other method here returns empty on success and the reason otherwise, so it needs
	 * no marker.
	 */
	FString OptionError(const FString& Reason)
	{
		return FString::Printf(TEXT("ERROR: %s"), *Reason);
	}

	/** "Option[1]/@test[0]" — the path GetQuery reports for a test and every test method accepts. */
	FString MakeTestPath(const FString& OptionPath, int32 TestIndex)
	{
		return FString::Printf(TEXT("%s/@test[%d]"), *OptionPath, TestIndex);
	}

	/**
	 * The option segment of a test path — "Option[1]" from "Option[1]/@test[0]". Empty if there is
	 * no '/' separator at all, which is how "this is an option path, not a test path" is detected
	 * without duplicating the path grammar ResolveNodePath owns.
	 */
	FString OptionSegmentOf(const FString& TestPath)
	{
		FString OptionSegment;
		FString TestSegment;
		return TestPath.Split(TEXT("/"), &OptionSegment, &TestSegment) ? OptionSegment : FString();
	}

	/**
	 * Resolve a test path to the option node that owns the sub-node AND the index it sits at.
	 *
	 * Both halves are needed by every mutator below, and neither can be derived safely from the
	 * other: the index is what the path means, and the owning option is what the index is an index
	 * into. ParentNode is deliberately not trusted as the route to the option — UpdateAsset assigns
	 * it (EnvironmentQueryGraph.cpp:87) but only for sub-nodes of root-linked options carrying a
	 * generator, so on a graph mid-repair it can be stale or null while the path is still perfectly
	 * well defined.
	 *
	 * False with OutError set for every failure. Resolution goes through VibeEQS::ResolveNodePath so
	 * the grammar, the range checks and the rejection of trailing junk all live in exactly one place.
	 */
	bool ResolveTestLocation(UEnvironmentQueryGraph* Graph, const FString& TestPath,
		const FString& AssetPath, UEnvironmentQueryGraphNode_Option*& OutOptionNode,
		int32& OutTestIndex, FString& OutError)
	{
		OutOptionNode = nullptr;
		OutTestIndex = INDEX_NONE;

		UEnvironmentQueryGraphNode* Node = VibeEQS::ResolveNodePath(Graph, TestPath);
		if (!Node)
		{
			OutError = FString::Printf(
				TEXT("No test at path '%s' in %s. Test paths are index-based and zero-based within "
					 "their option, as in 'Option[0]/@test[1]'; call GetQuery to list what is there."),
				*TestPath, *AssetPath);
			return false;
		}

		const FString OptionPath = OptionSegmentOf(TestPath);
		if (OptionPath.IsEmpty())
		{
			// The path resolved, to an option. Distinguished from "nothing there" for the same reason
			// ResolveOptionNode distinguishes the mirror case: it is the difference between a typo and
			// a caller addressing the wrong kind of node.
			OutError = FString::Printf(
				TEXT("'%s' in %s is an option path, not a test path. This call takes a test path such "
					 "as 'Option[0]/@test[0]'."),
				*TestPath, *AssetPath);
			return false;
		}

		UEnvironmentQueryGraphNode_Option* OptionNode =
			Cast<UEnvironmentQueryGraphNode_Option>(VibeEQS::ResolveNodePath(Graph, OptionPath));
		const int32 TestIndex = OptionNode ? OptionNode->SubNodes.IndexOfByKey(Node) : INDEX_NONE;
		if (TestIndex == INDEX_NONE)
		{
			// Unreachable through ResolveNodePath, which found this very node in that very array a
			// few lines ago. Reported rather than checked away: if it ever fires, the path grammar and
			// the graph have stopped agreeing, and mutating an index derived from that would edit
			// whichever sub-node happens to sit there.
			OutError = FString::Printf(
				TEXT("'%s' in %s resolved to a node its own option does not list as a sub-node"),
				*TestPath, *AssetPath);
			return false;
		}

		OutOptionNode = OptionNode;
		OutTestIndex = TestIndex;
		return true;
	}

	/** The option node at OptionPath, or nullptr with OutError set. */
	UEnvironmentQueryGraphNode_Option* ResolveOptionNode(UEnvironmentQueryGraph* Graph,
		const FString& OptionPath, const FString& AssetPath, FString& OutError)
	{
		UEnvironmentQueryGraphNode* Node = VibeEQS::ResolveNodePath(Graph, OptionPath);
		if (!Node)
		{
			OutError = FString::Printf(
				TEXT("No node at path '%s' in %s. Option paths are index-based and zero-based, as in "
					 "'Option[0]'; call GetQuery to list what is there."),
				*OptionPath, *AssetPath);
			return nullptr;
		}

		UEnvironmentQueryGraphNode_Option* OptionNode = Cast<UEnvironmentQueryGraphNode_Option>(Node);
		if (!OptionNode)
		{
			// Distinct from "nothing there": the path resolved, to a test sub-node. Saying so is the
			// difference between a typo and a caller addressing the wrong kind of node.
			OutError = FString::Printf(
				TEXT("'%s' in %s is not an option (it resolved to a test sub-node). This call takes an "
					 "option path such as 'Option[0]'."),
				*OptionPath, *AssetPath);
			return nullptr;
		}

		return OptionNode;
	}

	/**
	 * Move OptionNode's link so it becomes the TargetIndex'th option the root feeds.
	 *
	 * Operates on the *slots* the option links occupy in RootOut->LinkedTo rather than on the array
	 * as a whole, so anything else linked to that pin — which nothing here creates, but a
	 * hand-edited asset can carry — keeps its absolute position instead of being reordered or
	 * dropped by an edit that was about options.
	 *
	 * Reordering LinkedTo in place is safe: a link is reciprocal membership in two arrays, and
	 * neither the order of either array nor UEdGraphPin's own invariants depend on where in it a
	 * given entry sits. UpdateAsset re-sorts this same array itself.
	 */
	bool ReorderRootOptionLink(UEdGraphPin* RootOut, const UEnvironmentQueryGraphNode_Option* OptionNode,
		int32 TargetIndex)
	{
		if (!RootOut || !OptionNode)
		{
			return false;
		}

		TArray<int32> Slots;
		TArray<UEdGraphPin*> OptionLinks;
		int32 CurrentIndex = INDEX_NONE;
		for (int32 LinkIndex = 0; LinkIndex < RootOut->LinkedTo.Num(); ++LinkIndex)
		{
			UEdGraphPin* Linked = RootOut->LinkedTo[LinkIndex];
			const UEdGraphNode* Owner = Linked ? Linked->GetOwningNode() : nullptr;
			if (!Owner || !Owner->IsA<UEnvironmentQueryGraphNode_Option>())
			{
				continue;
			}

			if (Owner == OptionNode)
			{
				// A duplicated link would make "the" position ambiguous; GetOptionNodes reports such
				// a node once, so the first slot is the one that position refers to.
				if (CurrentIndex != INDEX_NONE)
				{
					continue;
				}
				CurrentIndex = OptionLinks.Num();
			}

			Slots.Add(LinkIndex);
			OptionLinks.Add(Linked);
		}

		if (CurrentIndex == INDEX_NONE)
		{
			return false;
		}

		const int32 ClampedTarget = FMath::Clamp(TargetIndex, 0, OptionLinks.Num() - 1);
		if (ClampedTarget == CurrentIndex)
		{
			return true;
		}

		UEdGraphPin* Moved = OptionLinks[CurrentIndex];
		OptionLinks.RemoveAt(CurrentIndex);
		OptionLinks.Insert(Moved, ClampedTarget);

		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			RootOut->LinkedTo[Slots[Index]] = OptionLinks[Index];
		}
		return true;
	}

	/** A failed write, with the addressed class kept if it was already known. */
	FEQSPropertySetResult PropertyError(const FString& Reason, const FString& ResolvedNodeClass = FString())
	{
		FEQSPropertySetResult Result;
		Result.bSuccess = false;
		Result.Error = Reason;
		Result.ResolvedNodeClass = ResolvedNodeClass;
		return Result;
	}

	/**
	 * The preamble both setters share: open for writing, resolve the node path to the object that
	 * owns the properties, and find the named property on it.
	 *
	 * False with OutResult already populated on any failure — including ResolvedNodeClass whenever the
	 * path resolved, so "no such property" still says which class was asked.
	 */
	bool BeginPropertyWrite(const FString& AssetPath, const FString& NodePath,
		const FString& PropertyName, UEnvQuery*& OutQuery, UEnvironmentQueryGraph*& OutGraph,
		UObject*& OutInstance, FProperty*& OutProperty, FEQSPropertySetResult& OutResult)
	{
		OutInstance = nullptr;
		OutProperty = nullptr;

		const FString OpenError = VibeEQS::OpenWriteGuard(AssetPath, OutQuery, OutGraph);
		if (!OpenError.IsEmpty())
		{
			OutResult = PropertyError(OpenError);
			return false;
		}

		FString ResolveError;
		OutInstance = VibeEQS::ResolvePropertyTarget(OutGraph, NodePath, AssetPath, ResolveError);
		if (!OutInstance)
		{
			OutResult = PropertyError(ResolveError);
			return false;
		}

		const FString ResolvedClass = OutInstance->GetClass()->GetName();

		const FString NameError = VibeEQS::CheckNameLength(PropertyName, TEXT("PropertyName"));
		if (!NameError.IsEmpty())
		{
			OutResult = PropertyError(NameError);
			return false;
		}

		FProperty* Property = OutInstance->GetClass()->FindPropertyByName(FName(*PropertyName));
		if (!VibeEQS::IsAuthorableProperty(Property))
		{
			// Deliberately one message for "absent" and for "present but not authorable". The second
			// case is mostly UEnvQueryTest::TestOrder, and naming it as a property that exists but
			// cannot be set would invite a caller to keep trying: UpdateAsset rewrites it from the
			// sub-node order on every commit, so MoveTest is the only thing that can change it.
			OutResult = PropertyError(FString::Printf(
				TEXT("%s has no authorable property named '%s'. Call GetPropertyNames for the names "
					 "this accepts; test order is not among them and is changed with MoveTest."),
				*ResolvedClass, *PropertyName), ResolvedClass);
			return false;
		}

		if (Property->ArrayDim > 1)
		{
			// A C-style fixed array: the import below would write element 0 and leave the rest, and
			// the read-back would then agree with itself while describing only part of the property.
			// No EQS class ships one, which is exactly why this is a refusal and not a feature.
			OutResult = PropertyError(FString::Printf(
				TEXT("'%s' on %s is a fixed-size array (%d elements); this service writes single "
					 "values only."),
				*PropertyName, *ResolvedClass, Property->ArrayDim), ResolvedClass);
			return false;
		}

		OutProperty = Property;
		OutResult.ResolvedNodeClass = ResolvedClass;
		return true;
	}

	/**
	 * The tail both setters share: commit, then re-read the property from the COMMITTED asset and
	 * check it still says what the import produced.
	 *
	 * ValueAfterImport is the export taken immediately after a successful import, and comparing
	 * against it is the point of this function. UEnvironmentQueryGraph::UpdateAsset runs inside the
	 * commit and rebuilds the option list, each option's Tests and every TestOrder, so a write it
	 * discarded would otherwise be reported as a success carrying the value that was passed in. The
	 * node is re-resolved from the path rather than reusing the instance pointer for the same reason.
	 */
	FEQSPropertySetResult FinishPropertyWrite(UEnvQuery* Query, UEnvironmentQueryGraph* Graph,
		const FString& AssetPath, const FString& NodePath, const FString& PropertyName,
		const FString& ValueAfterImport, const FString& ResolvedNodeClass)
	{
		const FString CommitError = VibeEQS::CommitGraph(Query, Graph);
		if (!CommitError.IsEmpty())
		{
			return PropertyError(CommitError, ResolvedNodeClass);
		}

		FString ResolveError;
		const UObject* Committed =
			VibeEQS::ResolvePropertyTarget(Graph, NodePath, AssetPath, ResolveError);
		const FProperty* Property =
			Committed ? Committed->GetClass()->FindPropertyByName(FName(*PropertyName)) : nullptr;
		if (!Property)
		{
			return PropertyError(FString::Printf(
				TEXT("'%s' was written to '%s' in %s but the committed asset no longer has that "
					 "property there (%s)"),
				*PropertyName, *NodePath, *AssetPath,
				ResolveError.IsEmpty() ? TEXT("the class changed") : *ResolveError),
				ResolvedNodeClass);
		}

		FEQSPropertySetResult Result;
		Result.ResolvedNodeClass = ResolvedNodeClass;
		Result.ValueAfterWrite = VibeEQS::ExportPropertyValue(Property, Committed);
		if (Result.ValueAfterWrite != ValueAfterImport)
		{
			Result.bSuccess = false;
			Result.Error = FString::Printf(
				TEXT("the write to '%s' on '%s' in %s did not survive the commit: it reads %s where "
					 "%s was written"),
				*PropertyName, *NodePath, *AssetPath, *Result.ValueAfterWrite, *ValueAfterImport);
			return Result;
		}

		Result.bSuccess = true;
		return Result;
	}
}

using namespace VibeEQSEdit;

FString UEnvQueryService::AddOption(const FString& AssetPath, const FString& GeneratorClassName,
	int32 Index)
{
	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	const FString OpenError = VibeEQS::OpenWriteGuard(AssetPath, Query, Graph);
	if (!OpenError.IsEmpty())
	{
		return OptionError(OpenError);
	}

	// Resolved before anything is created, so an unknown generator adds nothing at all rather than
	// leaving a blank option node behind — the one shape UpdateAsset silently drops.
	UClass* GeneratorClass =
		VibeEQS::ResolveClass(GeneratorClassName, UEnvQueryGenerator::StaticClass());
	if (!GeneratorClass)
	{
		return OptionError(FString::Printf(
			TEXT("Unknown or ambiguous generator class '%s'. Call GetAvailableGeneratorTypes for the "
				 "names this accepts."),
			*GeneratorClassName));
	}

	UEdGraphPin* RootOut = VibeEQS::FindRootOutputPin(Graph);
	if (!RootOut)
	{
		return OptionError(FString::Printf(
			TEXT("%s has no root node output pin to attach an option to"), *AssetPath));
	}

	const int32 ExistingCount = VibeEQS::GetOptionNodes(Graph).Num();
	const int32 InsertAt = (Index < 0) ? ExistingCount : FMath::Clamp(Index, 0, ExistingCount);

	Graph->Modify();

	// The engine's own idiom (EnvironmentQueryGraph.cpp:426-436). ClassData is set BEFORE Finalize
	// because Finalize runs PostPlacedNewNode, and UEnvironmentQueryGraphNode_Option's override
	// reads ClassData to build the UEnvQueryOption and its generator, outered to the UEnvQuery
	// (EnvironmentQueryGraphNode_Option.cpp:29-49) — which is the outer
	// UEnvironmentQueryGraph::CollectAllNodeInstances protects from RemoveOrphanedNodes.
	FGraphNodeCreator<UEnvironmentQueryGraphNode_Option> NodeBuilder(*Graph);
	UEnvironmentQueryGraphNode_Option* OptionNode = NodeBuilder.CreateNode();
	UAIGraphNode::UpdateNodeClassDataFrom(GeneratorClass, OptionNode->ClassData);
	NodeBuilder.Finalize();

	// Belt and braces, not redundancy: everything downstream — the discard guard, UpdateAsset, the
	// option order — assumes a linked option node carries an instance WITH a generator, and a node
	// that does not is the exact shape that empties a query on save. If PostPlacedNewNode did the
	// work (it does, for a resolvable class), both branches are skipped.
	UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(ToRawPtr(OptionNode->NodeInstance));
	if (!OptionInstance)
	{
		OptionInstance = NewObject<UEnvQueryOption>(Query);
		OptionInstance->SetFlags(RF_Transactional);
		OptionNode->NodeInstance = OptionInstance;
	}
	if (!OptionInstance->Generator)
	{
		UEnvQueryGenerator* Generator = NewObject<UEnvQueryGenerator>(Query, GeneratorClass);
		Generator->SetFlags(RF_Transactional);
		Generator->UpdateNodeVersion();
		OptionInstance->Generator = Generator;
	}

	UEdGraphPin* OptionIn = nullptr;
	for (UEdGraphPin* Pin : OptionNode->Pins)
	{
		if (Pin && Pin->Direction == EGPD_Input)
		{
			OptionIn = Pin;
			break;
		}
	}
	if (!OptionIn)
	{
		// Nothing has been linked yet, so dropping the node here leaves the graph as it was found.
		Graph->RemoveNode(OptionNode, /*bBreakAllLinks*/ false);
		return OptionError(TEXT("the new option node has no input pin to link to the root"));
	}

	RootOut->MakeLinkTo(OptionIn);

	// MakeLinkTo appends, so the node is last until this moves it. Without the move, Index means
	// nothing: every option would land at the end regardless.
	if (!ReorderRootOptionLink(RootOut, OptionNode, InsertAt))
	{
		return OptionError(TEXT("failed to place the new option in the root's link order"));
	}

	const FString CommitError = VibeEQS::CommitGraph(Query, Graph);
	if (!CommitError.IsEmpty())
	{
		return OptionError(CommitError);
	}

	// The path is derived from the committed graph rather than from InsertAt: the commit is what
	// decides the final order, and reporting an index the asset does not agree with would send every
	// following call to the wrong node.
	const int32 FinalIndex = VibeEQS::GetOptionNodes(Graph).IndexOfByKey(OptionNode);
	if (FinalIndex == INDEX_NONE)
	{
		return OptionError(FString::Printf(
			TEXT("the option was added to %s but the commit did not keep it"), *AssetPath));
	}

	return VibeEQS::MakeOptionPath(FinalIndex);
}

FString UEnvQueryService::RemoveOption(const FString& AssetPath, const FString& OptionPath)
{
	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	const FString OpenError = VibeEQS::OpenWriteGuard(AssetPath, Query, Graph);
	if (!OpenError.IsEmpty())
	{
		return OpenError;
	}

	FString ResolveError;
	UEnvironmentQueryGraphNode_Option* OptionNode =
		ResolveOptionNode(Graph, OptionPath, AssetPath, ResolveError);
	if (!OptionNode)
	{
		return ResolveError;
	}

	const TArray<UEnvironmentQueryGraphNode_Option*> OptionNodes = VibeEQS::GetOptionNodes(Graph);

	// Refused before anything is touched, rather than mutating and failing at commit: CommitGraph's
	// discard guard would refuse this commit anyway (a populated option list rebuilding to nothing
	// is indistinguishable from the corruption it exists to catch), and a refusal after the node is
	// already unlinked would leave the graph disagreeing with UEnvQuery::Options until the asset is
	// reopened.
	bool bAnotherOptionSurvives = false;
	for (const UEnvironmentQueryGraphNode_Option* Other : OptionNodes)
	{
		const UEnvQueryOption* Instance =
			Other != OptionNode ? Cast<UEnvQueryOption>(ToRawPtr(Other->NodeInstance)) : nullptr;
		if (Instance && Instance->Generator)
		{
			bAnotherOptionSurvives = true;
			break;
		}
	}
	if (!bAnotherOptionSurvives && Query->GetOptions().Num() > 0)
	{
		return FString::Printf(
			TEXT("Refusing to remove '%s' from %s: it is the only option the query would have left, "
				 "and committing a query whose graph rebuilds no options at all is what "
				 "UEnvironmentQueryGraph::UpdateAsset turns into an empty asset on disk. Replace its "
				 "generator with SetOptionGenerator, or delete the query."),
			*OptionPath, *AssetPath);
	}

	const int32 CountBefore = OptionNodes.Num();

	Graph->Modify();
	OptionNode->Modify();

	// Pin level, and bNotifyNodes = false: the notifying forms reach
	// UAIGraphNode::NodeConnectionListChanged, which runs UpdateAsset() on the spot — see the file
	// header. The option's tests are sub-nodes rather than graph nodes, so they leave with it, and
	// RemoveOrphanedNodes (inside the commit) is what finally detaches their instances.
	for (UEdGraphPin* Pin : OptionNode->Pins)
	{
		if (Pin)
		{
			Pin->BreakAllPinLinks(/*bNotifyNodes*/ false);
		}
	}
	Graph->RemoveNode(OptionNode, /*bBreakAllLinks*/ false);

	const FString CommitError = VibeEQS::CommitGraph(Query, Graph);
	if (!CommitError.IsEmpty())
	{
		return CommitError;
	}

	// Verified by re-reading the committed graph, not by trusting the calls above.
	const int32 CountAfter = VibeEQS::GetOptionNodes(Graph).Num();
	if (CountAfter != CountBefore - 1)
	{
		return FString::Printf(
			TEXT("removing '%s' from %s left %d options where %d were expected"),
			*OptionPath, *AssetPath, CountAfter, CountBefore - 1);
	}

	return FString();
}

FString UEnvQueryService::MoveOption(const FString& AssetPath, const FString& OptionPath,
	int32 NewIndex)
{
	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	const FString OpenError = VibeEQS::OpenWriteGuard(AssetPath, Query, Graph);
	if (!OpenError.IsEmpty())
	{
		return OpenError;
	}

	FString ResolveError;
	UEnvironmentQueryGraphNode_Option* OptionNode =
		ResolveOptionNode(Graph, OptionPath, AssetPath, ResolveError);
	if (!OptionNode)
	{
		return ResolveError;
	}

	const int32 OptionCount = VibeEQS::GetOptionNodes(Graph).Num();
	// NewIndex < 0 means "last", matching AddOption's Index < 0. Clamped rather than refused at the
	// top end for the same reason.
	const int32 TargetIndex =
		(NewIndex < 0) ? OptionCount - 1 : FMath::Clamp(NewIndex, 0, OptionCount - 1);

	UEdGraphPin* RootOut = VibeEQS::FindRootOutputPin(Graph);
	if (!RootOut)
	{
		return FString::Printf(TEXT("%s has no root node output pin"), *AssetPath);
	}

	Graph->Modify();
	if (!ReorderRootOptionLink(RootOut, OptionNode, TargetIndex))
	{
		return FString::Printf(
			TEXT("'%s' in %s is not linked to the root, so it has no position to move"),
			*OptionPath, *AssetPath);
	}

	const FString CommitError = VibeEQS::CommitGraph(Query, Graph);
	if (!CommitError.IsEmpty())
	{
		return CommitError;
	}

	// Read back from the committed graph: link order only becomes option order by way of
	// ArrangeGraph and UpdateAsset's X sort, so "the reorder returned true" is not evidence that the
	// option actually runs in that position.
	const int32 FinalIndex = VibeEQS::GetOptionNodes(Graph).IndexOfByKey(OptionNode);
	if (FinalIndex != TargetIndex)
	{
		return FString::Printf(
			TEXT("moving '%s' in %s put it at index %d instead of %d"),
			*OptionPath, *AssetPath, FinalIndex, TargetIndex);
	}

	return FString();
}

FString UEnvQueryService::SetOptionGenerator(const FString& AssetPath, const FString& OptionPath,
	const FString& GeneratorClassName)
{
	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	const FString OpenError = VibeEQS::OpenWriteGuard(AssetPath, Query, Graph);
	if (!OpenError.IsEmpty())
	{
		return OpenError;
	}

	FString ResolveError;
	UEnvironmentQueryGraphNode_Option* OptionNode =
		ResolveOptionNode(Graph, OptionPath, AssetPath, ResolveError);
	if (!OptionNode)
	{
		return ResolveError;
	}

	UClass* GeneratorClass =
		VibeEQS::ResolveClass(GeneratorClassName, UEnvQueryGenerator::StaticClass());
	if (!GeneratorClass)
	{
		return FString::Printf(
			TEXT("Unknown or ambiguous generator class '%s'. Call GetAvailableGeneratorTypes for the "
				 "names this accepts."),
			*GeneratorClassName);
	}

	// The whole reason this method exists rather than RemoveOption + AddOption: the tests hang off
	// the option, so re-adding it would discard every one of them. Only Generator and the node's
	// displayed ClassData change below; OptionInstance->Tests and the node's SubNodes are not
	// touched, and the commit rebuilds Tests from those same SubNodes.
	UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(ToRawPtr(OptionNode->NodeInstance));
	if (!OptionInstance)
	{
		// A blank option node: no instance at all. Repaired rather than refused — this is the one
		// call whose job is to give an option a generator, and the node is otherwise unusable and
		// would be dropped by the next commit.
		OptionInstance = NewObject<UEnvQueryOption>(Query);
		OptionInstance->SetFlags(RF_Transactional);
		OptionNode->Modify();
		OptionNode->NodeInstance = OptionInstance;
	}

	// Counted from the SUB-NODES, not from OptionInstance->Tests, because those two disagree on
	// exactly the repair path above: a blank node can already carry test sub-nodes while the
	// UEnvQueryOption that was just created for it carries none, and the commit rebuilds Tests from
	// the sub-nodes. Reading 0 there would make the post-commit check below report "left N tests
	// where 0 were expected" for a commit that in fact did the right thing — a false failure after a
	// real success, which is the worst kind to hand an agent that retries on error.
	//
	// Disabled sub-nodes are excluded for the same reason: UpdateAsset only re-adds enabled ones
	// (EnvironmentQueryGraph.cpp:90), so counting them would make the check fail on every option
	// carrying a switched-off test.
	int32 TestCountBefore = 0;
	for (const TObjectPtr<UAIGraphNode>& SubNode : OptionNode->SubNodes)
	{
		const UEnvironmentQueryGraphNode_Test* TestNode =
			Cast<UEnvironmentQueryGraphNode_Test>(ToRawPtr(SubNode));
		if (TestNode && TestNode->bTestEnabled && TestNode->NodeInstance)
		{
			++TestCountBefore;
		}
	}

	UEnvQueryGenerator* Generator = NewObject<UEnvQueryGenerator>(Query, GeneratorClass);
	Generator->SetFlags(RF_Transactional);
	Generator->UpdateNodeVersion();

	OptionInstance->Modify();
	OptionInstance->Generator = Generator;

	// The node's cached class description, which is what the editor draws and what
	// UEnvironmentQueryGraphNode_Option::UpdateNodeClassData would set. Done through the static
	// UAIGraphNode helper rather than that virtual: EnvironmentQueryEditor exports nothing, so only
	// symbols on an exported base are linkable from here.
	//
	// ErrorMessage is deliberately not refreshed from ClassData.GetDeprecatedMessage(): this
	// service never populates deprecation messages (see EnsureGraph), and setting it here alone
	// would make one node's warning state inconsistent with every other node in the same asset.
	OptionNode->Modify();
	UAIGraphNode::UpdateNodeClassDataFrom(GeneratorClass, OptionNode->ClassData);

	const FString CommitError = VibeEQS::CommitGraph(Query, Graph);
	if (!CommitError.IsEmpty())
	{
		return CommitError;
	}

	// Read back after the commit. UpdateAsset regenerates both the option list and each option's
	// Tests array, so "we assigned it" is not evidence either survived.
	const UEnvironmentQueryGraphNode* CommittedNode = VibeEQS::ResolveNodePath(Graph, OptionPath);
	const UEnvQueryOption* CommittedOption = CommittedNode
		? Cast<UEnvQueryOption>(ToRawPtr(CommittedNode->NodeInstance))
		: nullptr;
	if (!CommittedOption || CommittedOption->Generator == nullptr
		|| CommittedOption->Generator->GetClass() != GeneratorClass)
	{
		return FString::Printf(
			TEXT("the generator of '%s' in %s did not survive the commit"), *OptionPath, *AssetPath);
	}
	if (CommittedOption->Tests.Num() != TestCountBefore)
	{
		return FString::Printf(
			TEXT("replacing the generator of '%s' in %s left %d tests where %d were expected"),
			*OptionPath, *AssetPath, CommittedOption->Tests.Num(), TestCountBefore);
	}

	return FString();
}

FString UEnvQueryService::AddTest(const FString& AssetPath, const FString& OptionPath,
	const FString& TestClassName, int32 Index)
{
	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	const FString OpenError = VibeEQS::OpenWriteGuard(AssetPath, Query, Graph);
	if (!OpenError.IsEmpty())
	{
		return OptionError(OpenError);
	}

	FString ResolveError;
	UEnvironmentQueryGraphNode_Option* OptionNode =
		ResolveOptionNode(Graph, OptionPath, AssetPath, ResolveError);
	if (!OptionNode)
	{
		return OptionError(ResolveError);
	}

	// Resolved before anything is created, so an unknown test class attaches nothing at all rather
	// than leaving an instance-less sub-node behind — which GetQuery would report with an empty
	// "class" and UpdateAsset would skip, i.e. a test visible in the editor that never runs.
	UClass* TestClass = VibeEQS::ResolveClass(TestClassName, UEnvQueryTest::StaticClass());
	if (!TestClass)
	{
		return OptionError(FString::Printf(
			TEXT("Unknown or ambiguous test class '%s'. Call GetAvailableTestTypes for the names this "
				 "accepts."),
			*TestClassName));
	}

	const int32 ExistingCount = OptionNode->SubNodes.Num();
	const int32 InsertAt = (Index < 0) ? ExistingCount : FMath::Clamp(Index, 0, ExistingCount);

	Graph->Modify();
	OptionNode->Modify();

	// The engine's own test-node idiom (EnvironmentQueryGraph.cpp:459-465) MINUS its last line, which
	// is the AddSubNode call the file header exists to explain. Everything up to the attachment is
	// reproduced exactly; the attachment itself is the two data-level assignments below.
	UEnvironmentQueryGraphNode_Test* TestNode = NewObject<UEnvironmentQueryGraphNode_Test>(Graph);

	// Outer is the Graph, matching what AddSubNode arranges by renaming the node into it
	// (AIGraphNode.cpp:282) — the sub-node is part of the graph even though it is not in Graph->Nodes,
	// and an outer anywhere else would not be saved with the asset. RF_Transactional likewise mirrors
	// AddSubNode (:280).
	TestNode->SetFlags(RF_Transactional);

	// Not `TestNode->ClassData = FGraphNodeClassData(TestClass, FString())`: that constructor fills in
	// only Class and ClassName, leaving AssetName and GeneratedClassPackage empty, so a
	// Blueprint-derived test would have no route back to its class once the weak Class pointer went
	// stale. UpdateNodeClassDataFrom picks the Blueprint-aware constructor for exactly that case
	// (AIGraphNode.cpp:390-407) and is what AddOption already uses for generators.
	UAIGraphNode::UpdateNodeClassDataFrom(TestClass, TestNode->ClassData);
	TestNode->CreateNewGuid();

	// ParentNode BEFORE PostPlacedNewNode, as AddSubNode does (:284-287): PostPlacedNewNode builds the
	// UEnvQueryTest under the graph's owner and then calls InitializeInstance, whose
	// UEnvironmentQueryGraphNode_Test override reads ParentNode to recalculate the option's displayed
	// weights (EnvironmentQueryGraphNode_Test.cpp:19-31). With it null that recalculation is silently
	// skipped.
	TestNode->ParentNode = OptionNode;
	TestNode->PostPlacedNewNode();

	// AddSubNode also calls AllocateDefaultPins() and AutowireNewNode(nullptr) here. Both are omitted
	// as verified no-ops for this node type rather than by oversight: neither
	// UEnvironmentQueryGraphNode_Test nor UEnvironmentQueryGraphNode overrides AllocateDefaultPins (a
	// test sub-node has no pins — only options and the root do), and AutowireNewNode with a null pin
	// is UEdGraphNode's empty base.

	// ClassData again, now from the instance rather than from the requested class. Not redundant: this
	// is UAIGraphNode::UpdateNodeClassData (:376-383), and it is what makes the node describe what it
	// ACTUALLY holds. If PostPlacedNewNode failed to build an instance it does nothing, which is why
	// the check below is a check and not an assumption.
	TestNode->UpdateNodeClassData();

	if (!TestNode->NodeInstance)
	{
		// Nothing has been attached yet, so returning here leaves the graph exactly as it was found.
		return OptionError(FString::Printf(
			TEXT("the test node for '%s' was created but its %s instance was not"),
			*TestClassName, *TestClass->GetName()));
	}

	OptionNode->SubNodes.Insert(TestNode, InsertAt);

	const FString CommitError = VibeEQS::CommitGraph(Query, Graph);
	if (!CommitError.IsEmpty())
	{
		return OptionError(CommitError);
	}

	// The path is derived from the committed graph rather than from InsertAt, for the same reason
	// AddOption re-derives its own: the commit is what decides the final order, and reporting an index
	// the asset does not agree with sends every following call to the wrong node.
	const int32 FinalIndex = OptionNode->SubNodes.IndexOfByKey(TestNode);
	if (FinalIndex == INDEX_NONE)
	{
		return OptionError(FString::Printf(
			TEXT("the test was added to '%s' in %s but the commit did not keep it"),
			*OptionPath, *AssetPath));
	}

	return MakeTestPath(OptionPath, FinalIndex);
}

FString UEnvQueryService::RemoveTest(const FString& AssetPath, const FString& TestPath)
{
	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	const FString OpenError = VibeEQS::OpenWriteGuard(AssetPath, Query, Graph);
	if (!OpenError.IsEmpty())
	{
		return OpenError;
	}

	UEnvironmentQueryGraphNode_Option* OptionNode = nullptr;
	int32 TestIndex = INDEX_NONE;
	FString ResolveError;
	if (!ResolveTestLocation(Graph, TestPath, AssetPath, OptionNode, TestIndex, ResolveError))
	{
		return ResolveError;
	}

	const int32 CountBefore = OptionNode->SubNodes.Num();

	Graph->Modify();
	OptionNode->Modify();

	// RemoveAt by the resolved index rather than UAIGraphNode::RemoveSubNode, which is RemoveSingle by
	// value: identical on a well-formed graph, but on one where the same node object appears twice in
	// SubNodes, by-value removal takes the FIRST occurrence while the path names a specific position.
	// ParentNode is cleared first so the detached node does not keep a reference to the option it is
	// no longer part of; the UEnvQueryTest instance is left alone, and RemoveOrphanedNodes inside the
	// commit is what marks it transient (it is no longer reachable through any SubNodes array, which
	// is how UAIGraph::CollectAllNodeInstances finds sub-node instances).
	if (UAIGraphNode* TestNode = ToRawPtr(OptionNode->SubNodes[TestIndex]))
	{
		TestNode->Modify();
		TestNode->ParentNode = nullptr;
	}
	OptionNode->SubNodes.RemoveAt(TestIndex);

	const FString CommitError = VibeEQS::CommitGraph(Query, Graph);
	if (!CommitError.IsEmpty())
	{
		return CommitError;
	}

	// Verified by re-reading the committed graph, not by trusting the calls above: Graph->Initialize()
	// inside the commit runs SpawnMissingSubNodes, which re-creates a sub-node for every test still in
	// UEnvQueryOption::Tests, so a removal that UpdateAsset did not see would come straight back.
	const int32 CountAfter = OptionNode->SubNodes.Num();
	if (CountAfter != CountBefore - 1)
	{
		return FString::Printf(
			TEXT("removing '%s' from %s left %d tests on the option where %d were expected"),
			*TestPath, *AssetPath, CountAfter, CountBefore - 1);
	}

	return FString();
}

FString UEnvQueryService::MoveTest(const FString& AssetPath, const FString& TestPath, int32 NewIndex)
{
	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	const FString OpenError = VibeEQS::OpenWriteGuard(AssetPath, Query, Graph);
	if (!OpenError.IsEmpty())
	{
		return OpenError;
	}

	UEnvironmentQueryGraphNode_Option* OptionNode = nullptr;
	int32 TestIndex = INDEX_NONE;
	FString ResolveError;
	if (!ResolveTestLocation(Graph, TestPath, AssetPath, OptionNode, TestIndex, ResolveError))
	{
		return ResolveError;
	}

	// NewIndex < 0 means "last", matching AddTest's and AddOption's Index < 0. Clamped rather than
	// refused at the top end for the same reason.
	const int32 SubNodeCount = OptionNode->SubNodes.Num();
	const int32 TargetIndex =
		(NewIndex < 0) ? SubNodeCount - 1 : FMath::Clamp(NewIndex, 0, SubNodeCount - 1);

	Graph->Modify();
	OptionNode->Modify();

	// Unlike option order — which is node X position, written by ArrangeGraph and sorted by
	// UpdateAsset — test order IS the SubNodes array order, read straight out of it into
	// UEnvQueryTest::TestOrder and UEnvQueryOption::Tests (EnvironmentQueryGraph.cpp:78-98). So there
	// is no layout pass to make this stick, and none is needed.
	TObjectPtr<UAIGraphNode> Moved = OptionNode->SubNodes[TestIndex];
	OptionNode->SubNodes.RemoveAt(TestIndex);
	OptionNode->SubNodes.Insert(Moved, TargetIndex);

	const FString CommitError = VibeEQS::CommitGraph(Query, Graph);
	if (!CommitError.IsEmpty())
	{
		return CommitError;
	}

	const int32 FinalIndex = OptionNode->SubNodes.IndexOfByKey(Moved);
	if (FinalIndex != TargetIndex)
	{
		return FString::Printf(TEXT("moving '%s' in %s put it at index %d instead of %d"),
			*TestPath, *AssetPath, FinalIndex, TargetIndex);
	}

	return FString();
}

FString UEnvQueryService::SetTestEnabled(const FString& AssetPath, const FString& TestPath,
	bool bEnabled)
{
	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	const FString OpenError = VibeEQS::OpenWriteGuard(AssetPath, Query, Graph);
	if (!OpenError.IsEmpty())
	{
		return OpenError;
	}

	UEnvironmentQueryGraphNode_Option* OptionNode = nullptr;
	int32 TestIndex = INDEX_NONE;
	FString ResolveError;
	if (!ResolveTestLocation(Graph, TestPath, AssetPath, OptionNode, TestIndex, ResolveError))
	{
		return ResolveError;
	}

	// bTestEnabled exists only on UEnvironmentQueryGraphNode_Test, so a sub-node of any other kind
	// cannot be switched off — and reporting success for a call that changed nothing would be worse
	// than refusing, because the caller's next read would show the test still running.
	UEnvironmentQueryGraphNode_Test* TestNode =
		Cast<UEnvironmentQueryGraphNode_Test>(ToRawPtr(OptionNode->SubNodes[TestIndex]));
	if (!TestNode)
	{
		return FString::Printf(
			TEXT("'%s' in %s is not a test node, so it has no enabled flag to set"),
			*TestPath, *AssetPath);
	}

	const UObject* TestInstance = ToRawPtr(TestNode->NodeInstance);

	TestNode->Modify();
	TestNode->bTestEnabled = bEnabled ? 1 : 0;

	const FString CommitError = VibeEQS::CommitGraph(Query, Graph);
	if (!CommitError.IsEmpty())
	{
		return CommitError;
	}

	// Both halves are asserted, because they are two different facts and only one of them is the one
	// that matters at runtime: the flag on the graph node (which is what GetQuery reports and what
	// survives to disk) and membership of UEnvQueryOption::Tests (which is what the query actually
	// executes, rebuilt by UpdateAsset from the enabled sub-nodes only, EnvironmentQueryGraph.cpp:90).
	// A commit that wrote the flag but did not regenerate Tests would leave a "disabled" test still
	// running, and nothing else here would notice.
	if ((TestNode->bTestEnabled != 0) != bEnabled)
	{
		return FString::Printf(TEXT("the enabled flag of '%s' in %s did not survive the commit"),
			*TestPath, *AssetPath);
	}

	const UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(ToRawPtr(OptionNode->NodeInstance));
	const bool bInRuntimeTests = OptionInstance && TestInstance
		&& OptionInstance->Tests.ContainsByPredicate(
			[TestInstance](const TObjectPtr<UEnvQueryTest>& Test) { return ToRawPtr(Test) == TestInstance; });
	if (bInRuntimeTests != bEnabled)
	{
		return FString::Printf(
			TEXT("'%s' in %s was set %s but the committed option %s it in its runtime test list"),
			*TestPath, *AssetPath, bEnabled ? TEXT("enabled") : TEXT("disabled"),
			bInRuntimeTests ? TEXT("still lists") : TEXT("does not list"));
	}

	return FString();
}

FEQSPropertySetResult UEnvQueryService::SetPropertyValue(const FString& AssetPath,
	const FString& NodePath, const FString& PropertyName, const FString& Value)
{
	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	UObject* Instance = nullptr;
	FProperty* Property = nullptr;
	FEQSPropertySetResult Result;
	if (!BeginPropertyWrite(AssetPath, NodePath, PropertyName, Query, Graph, Instance, Property,
		Result))
	{
		return Result;
	}

	// Before the import, not after: Modify() is an undo snapshot, and taking it once the value has
	// already changed records the wrong state for any transaction that happens to be open. The cost
	// is that a REFUSED write below still leaves the package marked dirty — stated plainly rather
	// than traded away, because a cosmetic dirty flag is worth less than a correct undo snapshot,
	// and the value itself is restored either way.
	Instance->Modify();

	FString ImportError;
	if (!VibeEQS::ImportPropertyValue(Property, Instance, Instance, Value, ImportError))
	{
		// Nothing was committed and the value is back as it was found, so this refusal writes nothing
		// to disk and leaves the property exactly as it was (the package is dirty — see above). The
		// data-provider hint is here rather than in the generic message because "(DefaultValue=...)"
		// is the single most common reason a perfectly reasonable-looking value is rejected.
		const FString Hint = VibeEQS::IsDataProviderProperty(Property)
			? FString::Printf(
				TEXT(" '%s' is a data-provider value, so its literal form is \"(DefaultValue=%s)\" "
					 "rather than \"%s\" — SetDataProviderValue takes the bare form."),
				*PropertyName, *Value, *Value)
			: FString();
		return PropertyError(FString::Printf(TEXT("%s on %s: %s.%s"), *PropertyName,
			*Result.ResolvedNodeClass, *ImportError, *Hint), Result.ResolvedNodeClass);
	}

	const FString ValueAfterImport = VibeEQS::ExportPropertyValue(Property, Instance);
	return FinishPropertyWrite(Query, Graph, AssetPath, NodePath, PropertyName, ValueAfterImport,
		Result.ResolvedNodeClass);
}

FEQSPropertySetResult UEnvQueryService::SetDataProviderValue(const FString& AssetPath,
	const FString& NodePath, const FString& PropertyName, const FString& Value)
{
	UEnvQuery* Query = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	UObject* Instance = nullptr;
	FProperty* Property = nullptr;
	FEQSPropertySetResult Result;
	if (!BeginPropertyWrite(AssetPath, NodePath, PropertyName, Query, Graph, Instance, Property,
		Result))
	{
		return Result;
	}

	// Refused, not quietly forwarded to SetPropertyValue. A caller reaching this entry point believes
	// the property is a data-provider value; if it is not, that belief is the bug, and a generic write
	// that happened to work would leave it in place to misfire on the next property.
	if (!VibeEQS::IsDataProviderProperty(Property))
	{
		return PropertyError(FString::Printf(
			TEXT("'%s' on %s is a %s, not a data-provider value, so it holds no DefaultValue to set. "
				 "Use SetPropertyValue for it."),
			*PropertyName, *Result.ResolvedNodeClass, *Property->GetCPPType()),
			Result.ResolvedNodeClass);
	}

	const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Property);
	void* StructPtr = StructProperty->ContainerPtrToValuePtr<void>(Instance);

	// Member names taken through GET_MEMBER_NAME_CHECKED so an engine rename is a build error here
	// rather than a setter that silently stops finding anything. DefaultValue is declared on each
	// concrete derived struct rather than on FAIDataProviderValue — Int, Float and Bool all spell it
	// identically, so the float one stands in for the name of all three.
	static const FName DataBindingName = GET_MEMBER_NAME_CHECKED(FAIDataProviderValue, DataBinding);
	static const FName DefaultValueName =
		GET_MEMBER_NAME_CHECKED(FAIDataProviderFloatValue, DefaultValue);

	const FObjectPropertyBase* BindingProperty =
		CastField<FObjectPropertyBase>(StructProperty->Struct->FindPropertyByName(DataBindingName));
	if (BindingProperty && BindingProperty->GetObjectPropertyValue_InContainer(StructPtr) != nullptr)
	{
		// The refusal this call exists for. A bound value reads its number out of the provider's
		// field at query time and never looks at DefaultValue (FAIDataProviderValue::GetRawValuePtr),
		// so writing one here would report success and change nothing at runtime — while destroying
		// the binding is the only alternative, and doing that silently is worse.
		return PropertyError(FString::Printf(
			TEXT("'%s' on %s is bound to a data provider, so its literal DefaultValue is never read. "
				 "It currently holds %s. Clear or replace the binding deliberately with "
				 "SetPropertyValue and a full struct literal if that is what you meant."),
			*PropertyName, *Result.ResolvedNodeClass,
			*VibeEQS::ExportPropertyValue(Property, Instance)),
			Result.ResolvedNodeClass);
	}

	const FProperty* DefaultValueProperty = StructProperty->Struct->FindPropertyByName(DefaultValueName);
	if (!DefaultValueProperty)
	{
		// FAIDataProviderValue / FAIDataProviderTypedValue / FAIDataProviderStructValue carry a
		// binding but no literal at all. Nothing on a stock EQS class is one of those, so this is a
		// guard against a custom struct rather than a path that is expected to run.
		return PropertyError(FString::Printf(
			TEXT("'%s' on %s is a %s, which carries no DefaultValue member — it can only be bound to "
				 "a data provider, not given a literal."),
			*PropertyName, *Result.ResolvedNodeClass, *Property->GetCPPType()),
			Result.ResolvedNodeClass);
	}

	// Before the import, for the undo-snapshot reason SetPropertyValue records. Every refusal that
	// this call makes on its own account — not a provider value, bound, no DefaultValue member — sits
	// ABOVE this line, so only a malformed literal can reach it and dirty the package.
	Instance->Modify();

	// The struct is the container and DefaultValue is the property: everything else in the struct
	// (DataField, and the binding just verified to be absent) is left exactly as it was, which a
	// whole-struct import of "(DefaultValue=...)" would not guarantee.
	FString ImportError;
	if (!VibeEQS::ImportPropertyValue(DefaultValueProperty, StructPtr, Instance, Value, ImportError))
	{
		return PropertyError(FString::Printf(TEXT("%s.DefaultValue on %s: %s"), *PropertyName,
			*Result.ResolvedNodeClass, *ImportError), Result.ResolvedNodeClass);
	}

	// Exported and compared as the WHOLE provider struct, not just DefaultValue: that is what
	// GetPropertyValue returns for this name, and reporting a bare number here would hand callers a
	// ValueAfterWrite that SetPropertyValue would reject if fed back.
	const FString ValueAfterImport = VibeEQS::ExportPropertyValue(Property, Instance);
	return FinishPropertyWrite(Query, Graph, AssetPath, NodePath, PropertyName, ValueAfterImport,
		Result.ResolvedNodeClass);
}

DEFINE_LOG_CATEGORY_STATIC(LogVibeEQSRepair, Log, All);

FString UEnvQueryService::RepairGraphFromOptions(const FString& AssetPath)
{
	if (AssetPath.IsEmpty())
	{
		return TEXT("AssetPath is empty");
	}

	// Read first, and deliberately NOT through OpenWriteGuard: the guard's EnsureGraph runs
	// UEnvironmentQueryGraph::Initialize(), which performs the very reconstruction this call is
	// about. Both refusals below have to be decided against the state as found, or "the graph
	// already has option nodes" would be true by the time it was asked and this call could never
	// do anything.
	UEnvQuery* Query =
		LoadObject<UEnvQuery>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Query)
	{
		return FString::Printf(TEXT("Environment Query not found: %s"), *AssetPath);
	}

	const int32 OptionCount = Query->GetOptions().Num();
	if (OptionCount == 0)
	{
		return FString::Printf(
			TEXT("Nothing to repair in %s: it has no options, so there is nothing to rebuild the "
				 "graph from. A repair here would produce the same empty graph it started with, and "
				 "reporting success for that would suggest options had been recovered."),
			*AssetPath);
	}

	UEnvironmentQueryGraph* ExistingGraph = Cast<UEnvironmentQueryGraph>(Query->EdGraph);
	const int32 OptionNodesBefore =
		ExistingGraph ? VibeEQS::GetOptionNodes(ExistingGraph).Num() : 0;
	if (OptionNodesBefore > 0)
	{
		return FString::Printf(
			TEXT("Nothing to repair in %s: its graph's root already feeds %d option node(s). "
				 "Rebuilding is refused rather than repeated — SpawnMissingNodes skips options that "
				 "already have a node, so a second run is a no-op that would still report success "
				 "and still save. Use CompileAndSave to recommit a healthy query."),
			*AssetPath, OptionNodesBefore);
	}

	// A graph with no root, which ValidateQuery already reports first-class as "no root node".
	// Without this the repair DAMAGES the asset it was asked to fix: EnsureGraph only creates a root
	// when EdGraph is null, so an existing rootless graph keeps none; SpawnMissingNodes finds
	// MyRootNode == nullptr and spawns option nodes with no RootOutPin to link them to
	// (EnvironmentQueryGraph.cpp:415, 441-445); the UpdateAsset at the end of Initialize() then
	// rebuilds the option list from a graph whose root feeds nothing — zero — and EnsureGraph
	// refuses AND marks this copy permanently un-writable. Asking to repair would be the thing that
	// broke it. A null ExistingGraph is a different case and is allowed through: EnsureGraph builds
	// the graph and its root from scratch there.
	if (ExistingGraph && !VibeEQS::FindRootNode(ExistingGraph))
	{
		return FString::Printf(
			TEXT("Refusing to repair %s: its editor graph has no root node, which is the shape "
				 "ValidateQuery reports as 'no root node'. SpawnMissingNodes has nothing to link the "
				 "rebuilt options to, and the commit that follows would rebuild an EMPTY option list "
				 "over this query's %d — so proceeding would destroy in memory exactly what the "
				 "repair was asked to recover. Nothing was touched. Recreate the root in the "
				 "Environment Query editor, or create the query again with CreateQuery."),
			*AssetPath, OptionCount);
	}

	const int32 GraphNodesBefore = ExistingGraph ? ExistingGraph->Nodes.Num() : 0;

	// THE reconstruction. UEnvironmentQueryGraph::SpawnMissingNodes() is a non-virtual member of a
	// module that exports nothing, so it cannot be called from here at all (LNK2019); the only
	// reachable route is Initialize(), a UAIGraph virtual, which is what EnsureGraph — and therefore
	// OpenWriteGuard — runs. Every refusal the guard makes (open editor, play session, locked graph,
	// poisoned copy, a reconstruction that would DROP options) applies unchanged, which is the point:
	// a repair is still a write.
	UEnvQuery* OpenedQuery = nullptr;
	UEnvironmentQueryGraph* Graph = nullptr;
	const FString OpenError = VibeEQS::OpenWriteGuard(AssetPath, OpenedQuery, Graph);
	if (!OpenError.IsEmpty())
	{
		return OpenError;
	}

	const int32 GraphNodesAfter = Graph->Nodes.Num();
	const int32 OptionNodesAfter = VibeEQS::GetOptionNodes(Graph).Num();
	if (OptionNodesAfter == 0)
	{
		// Reached when the options cannot be represented at all — every one of them lacking a
		// generator, say. Refused before the commit rather than left to the discard guard, so the
		// message names the repair that failed instead of a commit the caller never asked for.
		return FString::Printf(
			TEXT("Repair of %s produced no option nodes: UEnvironmentQueryGraph::SpawnMissingNodes "
				 "rebuilds a node only for an option carrying a generator "
				 "(EnvironmentQueryGraph.cpp:421), and none of its %d option(s) does. Nothing was "
				 "committed and the asset on disk is unchanged."),
			*AssetPath, OptionCount);
	}

	const FString CommitError = VibeEQS::CommitGraph(OpenedQuery, Graph);
	if (!CommitError.IsEmpty())
	{
		return CommitError;
	}

	UE_LOG(LogVibeEQSRepair, Log,
		TEXT("RepairGraphFromOptions(%s): rebuilt from %d option(s); graph nodes %d -> %d, option "
			 "nodes %d -> %d, options after commit %d."),
		*AssetPath, OptionCount, GraphNodesBefore, GraphNodesAfter, OptionNodesBefore,
		OptionNodesAfter, OpenedQuery->GetOptions().Num());

	return FString();
}

#else  // WITH_VIBEUE_EQS

namespace
{
	const TCHAR* const GEQSEditUnavailable =
		TEXT("EQS authoring is unavailable: the EnvironmentQueryEditor plugin is not enabled in this "
			 "build (WITH_VIBEUE_EQS=0).");
}

FString UEnvQueryService::AddOption(const FString&, const FString&, int32)
{
	return FString::Printf(TEXT("ERROR: %s"), GEQSEditUnavailable);
}

FString UEnvQueryService::RemoveOption(const FString&, const FString&)
{
	return GEQSEditUnavailable;
}

FString UEnvQueryService::MoveOption(const FString&, const FString&, int32)
{
	return GEQSEditUnavailable;
}

FString UEnvQueryService::SetOptionGenerator(const FString&, const FString&, const FString&)
{
	return GEQSEditUnavailable;
}

FString UEnvQueryService::AddTest(const FString&, const FString&, const FString&, int32)
{
	return FString::Printf(TEXT("ERROR: %s"), GEQSEditUnavailable);
}

FString UEnvQueryService::RemoveTest(const FString&, const FString&)
{
	return GEQSEditUnavailable;
}

FString UEnvQueryService::MoveTest(const FString&, const FString&, int32)
{
	return GEQSEditUnavailable;
}

FString UEnvQueryService::SetTestEnabled(const FString&, const FString&, bool)
{
	return GEQSEditUnavailable;
}

FEQSPropertySetResult UEnvQueryService::SetPropertyValue(const FString&, const FString&,
	const FString&, const FString&)
{
	FEQSPropertySetResult Result;
	Result.Error = GEQSEditUnavailable;
	return Result;
}

FEQSPropertySetResult UEnvQueryService::SetDataProviderValue(const FString&, const FString&,
	const FString&, const FString&)
{
	FEQSPropertySetResult Result;
	Result.Error = GEQSEditUnavailable;
	return Result;
}

FString UEnvQueryService::RepairGraphFromOptions(const FString&)
{
	return GEQSEditUnavailable;
}

#endif // WITH_VIBEUE_EQS
