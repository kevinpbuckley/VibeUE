// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "EnvQueryServiceInternal.h"

#if WITH_VIBEUE_EQS

#include "AIGraphTypes.h"
#include "CoreGlobals.h"
#include "DataProviders/AIDataProvider.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_EnvironmentQuery.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQueryGraph.h"
#include "EnvironmentQueryGraphNode.h"
#include "EnvironmentQueryGraphNode_Option.h"
#include "EnvironmentQueryGraphNode_Root.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/ScopeExit.h"
#include "PackageTools.h"
#include "Misc/StringOutputDevice.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/ObjectKey.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace VibeEQS
{
	TArray<FIntPoint> ComputeOptionLayout(int32 OptionCount, FIntPoint RootPos)
	{
		TArray<FIntPoint> Positions;
		Positions.Reserve(FMath::Max(0, OptionCount));
		for (int32 Index = 0; Index < OptionCount; ++Index)
		{
			Positions.Add(FIntPoint(RootPos.X + Index * OptionSpacingX, RootPos.Y + OptionRowY));
		}
		return Positions;
	}

	UEnvironmentQueryGraphNode_Root* FindRootNode(UEnvironmentQueryGraph* Graph)
	{
		if (!Graph)
		{
			return nullptr;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UEnvironmentQueryGraphNode_Root* Root = Cast<UEnvironmentQueryGraphNode_Root>(Node))
			{
				return Root;
			}
		}
		return nullptr;
	}

	UEdGraphPin* FindRootOutputPin(UEnvironmentQueryGraph* Graph)
	{
		UEnvironmentQueryGraphNode_Root* Root = FindRootNode(Graph);
		if (!Root)
		{
			return nullptr;
		}

		for (UEdGraphPin* Pin : Root->Pins)
		{
			if (Pin && Pin->Direction == EGPD_Output)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	TArray<UEnvironmentQueryGraphNode_Option*> GetOptionNodes(UEnvironmentQueryGraph* Graph)
	{
		TArray<UEnvironmentQueryGraphNode_Option*> Options;

		const UEdGraphPin* RootOut = FindRootOutputPin(Graph);
		if (!RootOut)
		{
			return Options;
		}

		// De-duplicated by pointer for the same reason ArrangeGraph does it: a malformed graph with
		// the same option linked twice must not appear as two addressable options, because the
		// second copy has no independent existence and every path after it would be off by one.
		TSet<UEnvironmentQueryGraphNode_Option*> Seen;
		for (const UEdGraphPin* Linked : RootOut->LinkedTo)
		{
			UEnvironmentQueryGraphNode_Option* OptionNode = Linked
				? Cast<UEnvironmentQueryGraphNode_Option>(Linked->GetOwningNode())
				: nullptr;
			if (OptionNode && !Seen.Contains(OptionNode))
			{
				Seen.Add(OptionNode);
				Options.Add(OptionNode);
			}
		}
		return Options;
	}

	FString MakeOptionPath(int32 OptionIndex)
	{
		return FString::Printf(TEXT("Option[%d]"), OptionIndex);
	}

	namespace
	{
		/**
		 * Parse "<Prefix><digits>]" into OutIndex. False for anything else.
		 *
		 * Digits are checked one by one rather than with FString::IsNumeric, which accepts "-1",
		 * "+1" and "1.5" — the first would resolve a negative index, and the last would silently
		 * become 1 through Atoi. A path that does not mean exactly one node must not resolve to a
		 * node at all.
		 */
		bool ParseIndexedSegment(const FString& Segment, const TCHAR* Prefix, int32& OutIndex)
		{
			OutIndex = INDEX_NONE;

			const int32 PrefixLen = FCString::Strlen(Prefix);
			if (!Segment.StartsWith(Prefix, ESearchCase::IgnoreCase) || !Segment.EndsWith(TEXT("]")))
			{
				return false;
			}

			const FString Digits = Segment.Mid(PrefixLen, Segment.Len() - PrefixLen - 1);
			if (Digits.IsEmpty())
			{
				return false;
			}
			for (const TCHAR Char : Digits)
			{
				if (!FChar::IsDigit(Char))
				{
					return false;
				}
			}

			OutIndex = FCString::Atoi(*Digits);
			return OutIndex >= 0;
		}
	}

	UEnvironmentQueryGraphNode* ResolveNodePath(UEnvironmentQueryGraph* Graph, const FString& Path)
	{
		if (!Graph || Path.IsEmpty())
		{
			return nullptr;
		}

		FString OptionSegment = Path;
		FString TestSegment;
		Path.Split(TEXT("/"), &OptionSegment, &TestSegment);

		int32 OptionIndex = INDEX_NONE;
		if (!ParseIndexedSegment(OptionSegment, TEXT("Option["), OptionIndex))
		{
			return nullptr;
		}

		const TArray<UEnvironmentQueryGraphNode_Option*> Options = GetOptionNodes(Graph);
		if (!Options.IsValidIndex(OptionIndex))
		{
			return nullptr;
		}

		UEnvironmentQueryGraphNode_Option* OptionNode = Options[OptionIndex];
		if (TestSegment.IsEmpty())
		{
			return OptionNode;
		}

		// A third segment ends up inside TestSegment ("@test[0]/x") and fails the ']' terminator
		// check below, so trailing junk is rejected rather than ignored.
		int32 TestIndex = INDEX_NONE;
		if (!ParseIndexedSegment(TestSegment, TEXT("@test["), TestIndex)
			|| !OptionNode->SubNodes.IsValidIndex(TestIndex))
		{
			return nullptr;
		}

		return Cast<UEnvironmentQueryGraphNode>(ToRawPtr(OptionNode->SubNodes[TestIndex]));
	}

	void ArrangeGraph(UEnvironmentQueryGraph* Graph)
	{
		UEnvironmentQueryGraphNode_Root* Root = FindRootNode(Graph);
		if (!Root || Root->Pins.Num() == 0)
		{
			return;
		}

		// Gather the options the root actually feeds, in current link order, de-duplicated by
		// pointer so a malformed graph with a doubled link cannot desynchronise the position
		// array from the node list.
		TArray<UEdGraphNode*> Options;
		TSet<UEdGraphNode*> Seen;
		for (UEdGraphPin* Pin : Root->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output)
			{
				continue;
			}
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				UEdGraphNode* Owner = Linked ? Linked->GetOwningNode() : nullptr;
				if (Owner && !Seen.Contains(Owner))
				{
					Seen.Add(Owner);
					Options.Add(Owner);
				}
			}
		}

		const TArray<FIntPoint> Positions =
			ComputeOptionLayout(Options.Num(), FIntPoint(Root->NodePosX, Root->NodePosY));
		check(Positions.Num() == Options.Num());

		for (int32 Index = 0; Index < Options.Num(); ++Index)
		{
			Options[Index]->Modify();
			Options[Index]->NodePosX = Positions[Index].X;
			Options[Index]->NodePosY = Positions[Index].Y;
		}
	}

	namespace
	{
		/**
		 * Queries whose in-memory copy has been damaged by a refused reconstruction (see
		 * EnsureGraph). Keyed by FObjectKey, i.e. by *this* UObject instance: a genuine reload
		 * produces a different object and is therefore not poisoned, so the mark lifts exactly when
		 * the damage is undone and never before.
		 *
		 * Bounded by the number of damaged queries in a session, which is zero in normal use.
		 */
		TSet<FObjectKey> GEQSPoisonedQueries;

		FString PoisonRefusal(const UEnvQuery* Query)
		{
			return FString::Printf(
				TEXT("Refusing to write %s: this in-memory copy was damaged by an earlier refused "
					 "reconstruction and its option list is no longer trustworthy. Retrying would "
					 "commit the loss, because the options it is meant to protect are already gone "
					 "from memory. Reload the asset (close and reopen the editor, or force a package "
					 "reload) before writing it again — the file on disk is still intact."),
				*Query->GetPathName());
		}
	}

	FString EnsureGraph(UEnvQuery* Query)
	{
		if (!Query)
		{
			return TEXT("EnsureGraph: null query");
		}

		if (GEQSPoisonedQueries.Contains(FObjectKey(Query)))
		{
			return PoisonRefusal(Query);
		}

		if (!Query->EdGraph)
		{
			// The factory does not create the graph; FEnvironmentQueryEditor does, lazily, on first
			// open (EnvironmentQueryEditor.cpp:207-217). Replicate that so a headlessly-created
			// asset is immediately writable.
			Query->Modify();
			Query->EdGraph = FBlueprintEditorUtils::CreateNewGraph(
				Query, TEXT("QueryGraph"), UEnvironmentQueryGraph::StaticClass(),
				UEdGraphSchema_EnvironmentQuery::StaticClass());
			if (!Query->EdGraph)
			{
				return TEXT("failed to create EdGraph");
			}

			// CreateDefaultNodesForGraph is what spawns the root, via
			// FGraphNodeCreator<UEnvironmentQueryGraphNode_Root>
			// (EdGraphSchema_EnvironmentQuery.cpp:24-30).
			const UEdGraphSchema* Schema = Query->EdGraph->GetSchema();
			if (!Schema)
			{
				return TEXT("Environment Query graph has no schema");
			}
			Schema->CreateDefaultNodesForGraph(*Query->EdGraph);

			// The editor also calls OnCreated() here (EnvironmentQueryEditor.cpp:217). Omitted
			// because it is a verified no-op on this path: UAIGraph::OnCreated() is only
			// MarkVersion(), and the Initialize() below reaches the identical end state — a fresh
			// graph has GraphVersion 0, so UEnvironmentQueryGraph::UpdateVersion() runs its three
			// upgrade passes (all no-ops on a graph holding nothing but a root), then sets
			// GraphVersion = Latest and Modify()s, exactly as MarkVersion would have.
		}

		UEnvironmentQueryGraph* Graph = Cast<UEnvironmentQueryGraph>(Query->EdGraph);
		if (!Graph)
		{
			return FString::Printf(TEXT("EdGraph is a %s, not a UEnvironmentQueryGraph"),
				*Query->EdGraph->GetClass()->GetName());
		}

		// When the graph already existed, the editor calls OnLoaded() here
		// (EnvironmentQueryEditor.cpp:221). Omitted: UAIGraph::OnLoaded() (AIGraph.cpp:39-43) runs
		// UpdateUnknownNodeClasses() -> UAIGraphNode::RefreshNodeClass(), which genuinely mutates
		// nodes, and rewriting an unrelated part of an asset because someone asked to write this
		// part of it is the kind of silent side effect this service exists not to have.
		// (UEnvironmentQueryGraph::OnLoaded also calls UpdateDeprecatedGeneratorClasses(), whose
		// name oversells it: EnvironmentQueryGraph.cpp:355-366 only assigns each option node's
		// ErrorMessage from FGraphNodeClassHelper::GetDeprecationMessage — it rewrites no classes.)
		// Known consequence: deprecation ErrorMessages are never populated on this path, so a node
		// whose generator class is deprecated carries no warning until a human opens the asset.
		//
		// Initialize() is not the passive setup call its name suggests. It is LockUpdates() +
		// SpawnMissingNodes() + CalculateAllWeights() + UnlockUpdates() (EnvironmentQueryGraph.cpp
		// :168-176), and UAIGraph::UnlockUpdates() ends in UpdateAsset() (AIGraph.cpp:273-277) —
		// so merely opening an asset for writing already regenerates UEnvQuery::Options from the
		// graph, in memory, before any guard in CommitGraph gets a look.
		//
		// On a healthy asset that is harmless and in fact required: SpawnMissingNodes() runs first
		// and rebuilds an option node (and its test sub-nodes) for every option the graph is
		// missing, laying them out at Root.X + Idx*300 / Root.Y + 100 so the X sort in UpdateAsset
		// reproduces the original order. Options in, identical options out.
		//
		// It is not harmless when the reconstruction cannot represent an option — one carrying no
		// generator, or one whose node exists but is not linked to the root. Those are dropped, and
		// because they are dropped *here*, CommitGraph's discard guard would later look at an
		// already-emptied array and see nothing wrong. So the loss is caught at the only point it
		// is still visible.
		//
		// The refusal must also be STICKY, and that is not a nicety. By the time the drop is
		// detectable this object is already damaged, and the damage is not undoable by putting the
		// array back: UpdateAsset ends in RemoveOrphanedNodes (AIGraph.cpp:215-236), which has
		// already set RF_Transient on each dropped UEnvQueryOption and renamed it into the transient
		// package, so a restored array would hold objects that serialise as null. Without the mark,
		// a caller that retries on error — which the intended caller, an agent, does — gets
		// LoadObject handing back this same emptied object, sees OptionsBefore == OptionsAfter == 0,
		// sails past CommitGraph's discard guard (Num() > 0 is now false) and saves the option-less
		// query over the intact file, reporting success. The retry is the data loss.
		//
		// Marked rather than force-reloaded: purging and re-loading the package here would
		// invalidate every raw UEnvQuery/UEnvQueryOption pointer any other subsystem — or the
		// caller, mid-batch — is holding, turning a contained refusal into dangling references
		// elsewhere. The mark is inert, costs nothing, and lifts by itself the moment a genuine
		// reload produces a new UObject (see GEQSPoisonedQueries).
		const int32 OptionsBefore = Query->GetOptionsMutable().Num();
		Graph->Initialize();
		const int32 OptionsAfter = Query->GetOptionsMutable().Num();
		if (OptionsAfter < OptionsBefore)
		{
			GEQSPoisonedQueries.Add(FObjectKey(Query));
			return FString::Printf(
				TEXT("Refusing to write %s: rebuilding its editor graph dropped %d of %d options "
					 "(UEnvironmentQueryGraph::Initialize -> UnlockUpdates -> UpdateAsset). An option "
					 "is dropped when it has no generator, or when its graph node is not linked to "
					 "the root. Saving would have persisted that loss. The asset on disk is "
					 "unchanged, but this in-memory copy is now damaged and every further write to it "
					 "will be refused until the asset is genuinely reloaded — retrying without a "
					 "reload would commit the loss."),
				*Query->GetPathName(), OptionsBefore - OptionsAfter, OptionsBefore);
		}

		return FString();
	}

	namespace
	{
		/**
		 * How many options UpdateAsset would put back into UEnvQuery::Options.
		 *
		 * Models its own acceptance test exactly (EnvironmentQueryGraph.cpp:60-101): the root's
		 * *first* pin, each of its links, the owning node cast to an option node, and — the part
		 * that matters — a live UEnvQueryOption instance carrying a non-null Generator
		 * (`if (OptionInstance && OptionInstance->Generator)`, :75).
		 *
		 * Counting links to *any* option node is not good enough, and the difference is a real
		 * data-loss path rather than a technicality: a caller that replaces a query's option nodes
		 * and links one blank, instance-less node satisfies the loose test while UpdateAsset re-adds
		 * nothing, so every existing option is reset away and saved. That is precisely the shape
		 * SetOptionGenerator produces mid-edit.
		 *
		 * BT counterpart: VibeBT::GraphWouldRebuildARuntimeTree — same job (would the commit rebuild
		 * anything?), answered as a bool there because a tree has one root and a count here because a
		 * query has N options and the guard compares against the query's current count.
		 */
		int32 CountOptionsUpdateAssetWouldKeep(UEnvironmentQueryGraph* Graph)
		{
			const UEnvironmentQueryGraphNode_Root* Root = FindRootNode(Graph);
			if (!Root || Root->Pins.Num() == 0 || !Root->Pins[0])
			{
				return 0;
			}

			int32 Count = 0;
			for (const UEdGraphPin* Linked : Root->Pins[0]->LinkedTo)
			{
				const UEnvironmentQueryGraphNode_Option* OptionNode = Linked
					? Cast<UEnvironmentQueryGraphNode_Option>(Linked->GetOwningNode())
					: nullptr;
				if (!OptionNode)
				{
					continue;
				}

				const UEnvQueryOption* Instance = Cast<UEnvQueryOption>(OptionNode->NodeInstance);
				if (Instance && Instance->Generator)
				{
					++Count;
				}
			}
			return Count;
		}
	}

	FString OpenWriteGuard(const FString& AssetPath, UEnvQuery*& OutQuery,
		UEnvironmentQueryGraph*& OutGraph)
	{
		OutQuery = nullptr;
		OutGraph = nullptr;

		// Before LoadObject: resolving an object path builds an FName per segment, so an over-long
		// path is fatal at the lookup, not an error this function could return.
		const FString PathError = CheckWritableAssetPath(AssetPath);
		if (!PathError.IsEmpty())
		{
			return PathError;
		}

		// LOAD_NoWarn | LOAD_Quiet: "not found" is a value this function returns to its caller, not
		// an incident worth engine warnings in the log.
		UEnvQuery* Query =
			LoadObject<UEnvQuery>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Query)
		{
			return FString::Printf(TEXT("Environment Query not found: %s"), *AssetPath);
		}

		// All three refusals are decided before EnsureGraph, so a refused write leaves the asset
		// exactly as it was — which is not cosmetic here, because EnsureGraph's Graph->Initialize()
		// regenerates UEnvQuery::Options in memory on the way through (see the comment there).
		//
		// The play session is checked first: cheapest test, worst consequence.
		if (GEditor && GEditor->PlayWorld)
		{
			return FString::Printf(
				TEXT("A Play In Editor session is running; stop it and retry writing %s. Committing "
					 "runs UEnvironmentQueryGraph::UpdateAsset(), which ends in "
					 "UAIGraph::RemoveOrphanedNodes(): every UEnvQueryOption / UEnvQueryTest instance "
					 "the graph no longer references is marked RF_Transient and renamed into the "
					 "transient package — possibly while a live query is executing it. The asset on "
					 "disk is unchanged."),
				*AssetPath);
		}

		if (GEditor)
		{
			if (UAssetEditorSubsystem* AssetEditorSubsystem =
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				if (AssetEditorSubsystem->FindEditorsForAsset(Query).Num() > 0)
				{
					return FString::Printf(
						TEXT("An Environment Query editor is open on %s; close it and retry. The open "
							 "editor holds its own copy of the graph, would not show this change, and "
							 "would overwrite it on the next save from the editor."),
						*AssetPath);
				}
			}
		}

		// Only an existing graph can be locked: EnsureGraph's freshly created one has bLockUpdates
		// clear by construction, and Initialize() leaves it clear on the way out.
		if (const UEnvironmentQueryGraph* ExistingGraph = Cast<UEnvironmentQueryGraph>(Query->EdGraph))
		{
			if (ExistingGraph->IsLocked())
			{
				return FString::Printf(
					TEXT("Graph updates are locked on %s (bLockUpdates). "
						 "UEnvironmentQueryGraph::UpdateAsset() early-returns while it is set "
						 "(EnvironmentQueryGraph.cpp:42), so the write would save a stale option list "
						 "and still report success."),
					*AssetPath);
			}
		}

		const FString GraphError = EnsureGraph(Query);
		if (!GraphError.IsEmpty())
		{
			return GraphError;
		}

		UEnvironmentQueryGraph* Graph = Cast<UEnvironmentQueryGraph>(Query->EdGraph);
		if (!Graph)
		{
			return FString::Printf(TEXT("%s has no Environment Query graph"), *AssetPath);
		}

		OutQuery = Query;
		OutGraph = Graph;
		return FString();
	}

	FString CommitGraph(UEnvQuery* Query, UEnvironmentQueryGraph* Graph)
	{
		if (!Query || !Graph)
		{
			return TEXT("CommitGraph: null query or graph");
		}

		// Also checked here, not only in EnsureGraph: CommitGraph is callable directly with a
		// handle obtained before the damage, and this is the call that would persist it.
		if (GEQSPoisonedQueries.Contains(FObjectKey(Query)))
		{
			return PoisonRefusal(Query);
		}

		// Re-asserted rather than assumed: OpenWriteGuard checked this, but anything the caller did
		// in between could have locked the graph, and a locked UpdateAsset() below does nothing at
		// all — silently — so an entire batch of edits would be saved as a no-op reporting success.
		if (Graph->IsLocked())
		{
			return TEXT("Graph updates are locked (bLockUpdates); "
						"UEnvironmentQueryGraph::UpdateAsset() would silently do nothing");
		}

		// Layout first, because it is an input to the commit rather than cosmetics: UpdateAsset
		// sorts the root's links by node X position (FCompareNodeXLocation,
		// EnvironmentQueryGraph.cpp:65) and writes the options out in that order. There is no
		// AutoArrange() on UEnvironmentQueryGraph to fall back on.
		ArrangeGraph(Graph);

		// Refuse to trade a populated option list for a graph that would rebuild none of it.
		// UpdateAsset opens with Query->GetOptionsMutable().Reset() (EnvironmentQueryGraph.cpp:59)
		// and only refills it from root-linked option nodes carrying a generator (:60-101), so
		// committing when nothing would survive destroys every option — on disk, with no undo.
		//
		// The test is "nothing survives", not "fewer survive than before". A commit that keeps 2 of
		// 3 options is indistinguishable from a correct RemoveOption, so refusing on any decrease
		// would make removing an option impossible. That leaves a deliberate gap — a *partial*
		// accidental loss is not caught here — recorded rather than papered over.
		const int32 SurvivingOptions = CountOptionsUpdateAssetWouldKeep(Graph);
		if (Query->GetOptionsMutable().Num() > 0 && SurvivingOptions == 0)
		{
			return FString::Printf(
				TEXT("Refusing to commit %s: its graph would rebuild 0 options while the query still "
					 "has %d. UEnvironmentQueryGraph::UpdateAsset() rebuilds the option list from the "
					 "graph alone, keeping only root-linked option nodes that carry a generator, so "
					 "saving would discard all of them. The asset on disk is unchanged."),
				*Query->GetPathName(), Query->GetOptionsMutable().Num());
		}

		// UpdateAsset() directly, not OnSave(): unlike UBehaviorTreeGraph, UEnvironmentQueryGraph
		// declares no OnSave() (its BT counterpart existed only to run SpawnMissingNodesForParallel,
		// and EQS has no parallel equivalent). This is the call that regenerates UEnvQuery::Options
		// from the graph, and it must run FIRST — see below.
		Graph->UpdateAsset();

		// Then the weights the editor displays, which UpdateAsset does not touch: without this a
		// human opening the asset next sees stale per-option numbers.
		//
		// Not Graph->CalculateAllWeights(), which does not link. The EnvironmentQueryEditor module
		// exports nothing — there is no ENVIRONMENTQUERYEDITOR_API or UE_API anywhere in its public
		// headers — so only virtuals declared on an exported base are reachable from here, through
		// the vtable. CalculateAllWeights is neither virtual nor exported and fails with LNK2019;
		// Initialize() is a UAIGraph virtual (UE_API) and runs it
		// (EnvironmentQueryGraph.cpp:168-176).
		//
		// The UpdateAsset() above is therefore load-bearing rather than redundant. Initialize()
		// begins with SpawnMissingNodes(), which re-creates a graph node for every entry still in
		// UEnvQuery::Options — so running it against a stale option list would resurrect the very
		// option nodes a caller had just deleted. Regenerating Options from the graph first leaves
		// SpawnMissingNodes with nothing to find. (Initialize() ends in UnlockUpdates(), i.e. a
		// second UpdateAsset(); it is idempotent on an already-updated graph.)
		Graph->Initialize();

		UPackage* Package = Query->GetOutermost();
		if (!Package)
		{
			return TEXT("Environment Query has no package");
		}

		// SavePackages(..., bOnlyDirty = true) silently skips a clean package, and neither
		// UpdateAsset() nor a node-position-only change necessarily dirties one — hence both the
		// explicit Modify/MarkPackageDirty and bOnlyDirty = false. The return value is checked
		// rather than discarded: a save that wrote nothing must not report success.
		Query->Modify();
		Query->MarkPackageDirty();
		if (!UEditorLoadingAndSavingUtils::SavePackages({ Package }, /*bOnlyDirty*/ false))
		{
			// The mutation is applied in memory but did not reach disk (read-only file, held file
			// lock). Left as-is, the package sits dirty and this "failed" edit ships silently with the
			// next successful save of the same asset - a human's Save All included - so the dirty
			// state is discarded back to what disk holds. Safe to reload here, unlike the
			// dropped-options refusal in EnsureGraph: nothing has been marked transient, the asset is
			// merely unsaved.
			return FString::Printf(TEXT("Failed to save package %s (read-only file or a held file "
										"lock?). The edit did not reach disk%s"),
				*Package->GetName(), *DiscardDirtyStateFromDisk(Package));
		}

		return FString();
	}

	FString CheckNameLength(const FString& Value, const TCHAR* What)
	{
		// NAME_SIZE is the engine's own bound (UnrealNames.h); construction past it is Fatal, not
		// an error, so this must run before any FName / object-path lookup sees the string.
		if (Value.Len() >= NAME_SIZE)
		{
			return FString::Printf(
				TEXT("%s is %d characters long; names are limited to %d (FName construction past "
					 "that is a fatal engine error)"),
				What, Value.Len(), NAME_SIZE - 1);
		}
		return FString();
	}

	FString CheckWritableAssetPath(const FString& AssetPath)
	{
		if (AssetPath.IsEmpty())
		{
			return TEXT("AssetPath is empty");
		}
		const FString LengthError = CheckNameLength(AssetPath, TEXT("AssetPath"));
		if (!LengthError.IsEmpty())
		{
			return LengthError;
		}
		if (!FPackageName::IsValidLongPackageName(AssetPath))
		{
			return FString::Printf(TEXT("Not a valid asset path: %s"), *AssetPath);
		}
		for (const TCHAR* Forbidden : { TEXT("/Engine/"), TEXT("/Script/"), TEXT("/Temp/") })
		{
			if (AssetPath.StartsWith(Forbidden))
			{
				return FString::Printf(
					TEXT("%s is under %s, which these services do not write to: engine content is "
						 "not the project's to edit, script packages are not assets, and /Temp does "
						 "not survive the session. Create the asset under /Game (or a plugin's "
						 "content root)."),
					*AssetPath, Forbidden);
			}
		}
		return FString();
	}

	FString DiscardDirtyStateFromDisk(UPackage* Package)
	{
		if (!Package)
		{
			return TEXT(", and there was no package to reload.");
		}
		FText ReloadError;
		if (UPackageTools::ReloadPackages({ Package }, ReloadError,
			EReloadPackagesInteractionMode::AssumePositive))
		{
			return TEXT(", and this in-memory copy has been reloaded from disk, so it is safe "
						"to retry after fixing the cause.");
		}
		return FString::Printf(
			TEXT(", but reloading the in-memory copy from disk failed (%s) - it is STALE and "
				 "must not be written again until the editor reloads it."),
			*ReloadError.ToString());
	}

	namespace
	{
		/**
		 * One FGraphNodeClassHelper per base class. GatherClasses/GetClass() are expensive (the
		 * latter loads the class), so a single primed helper is reused for the module's lifetime
		 * rather than rebuilt on every discovery/resolution call.
		 */
		TMap<UClass*, TSharedPtr<FGraphNodeClassHelper>> GClassHelperCache;
	}

	TSharedPtr<FGraphNodeClassHelper> GetClassHelper(UClass* BaseClass)
	{
		if (!BaseClass)
		{
			return nullptr;
		}

		if (const TSharedPtr<FGraphNodeClassHelper>* Existing = GClassHelperCache.Find(BaseClass))
		{
			return *Existing;
		}

		TSharedPtr<FGraphNodeClassHelper> Helper = MakeShared<FGraphNodeClassHelper>(BaseClass);

		// NOT what makes Blueprint-derived classes visible to GatherClasses(): that happens
		// unconditionally, because FGraphNodeClassHelper::BuildClassGraph() sweeps the asset
		// registry for every UBlueprint whenever bGatherBlueprints is true (its default, never
		// toggled here) and links matches into the class tree by parent-class name — verified by
		// reading AIGraphTypes.cpp. AddObservedBlueprintClasses/UpdateAvailableBlueprintClasses
		// only populate the static BlueprintClassCount map, read solely by
		// GetObservedBlueprintClassCount (a UI stat helper this codebase never calls); it is never
		// consulted by GatherClasses or FindAllSubClasses. Kept anyway because it is harmless and
		// matches the engine's own usage (EnvironmentQueryEditorModule::CreateEnvironmentQueryEditor
		// calls the same pair, priming only UEnvQueryGenerator_BlueprintBase, and that single call
		// still leaves Blueprint tests and contexts visible too — further evidence the sweep, not
		// this priming, is what does the work).
		FGraphNodeClassHelper::AddObservedBlueprintClasses(BaseClass);
		Helper->UpdateAvailableBlueprintClasses();

		GClassHelperCache.Add(BaseClass, Helper);
		return Helper;
	}

	void ShutdownClassHelperCache()
	{
		// ~FGraphNodeClassHelper unhooks FModuleManager and asset-registry delegates. Letting the
		// static TMap destroy the helpers at process teardown runs that after those systems are
		// already gone, which is UB. Released here, while the module is still alive.
		GClassHelperCache.Empty();
	}

	UClass* ResolveClass(const FString& ClassName, UClass* RequiredBase)
	{
		if (ClassName.IsEmpty() || !RequiredBase)
		{
			return nullptr;
		}

		// Deliberately not FindObject/FindFirstObject: those are in-memory-only hash lookups that
		// never load from disk, so a not-yet-resident Blueprint class would come back nullptr,
		// indistinguishable from "no such class". Matching against the primed, RequiredBase-scoped
		// class list below is the only route that can load such a class, and it keeps matches
		// scoped to RequiredBase's own hierarchy so a same-named class under a different EQS
		// family can never be the accidental winner.
		const TSharedPtr<FGraphNodeClassHelper> Helper = GetClassHelper(RequiredBase);
		if (!Helper.IsValid())
		{
			return nullptr;
		}

		TArray<FGraphNodeClassData> ClassData;
		Helper->GatherClasses(RequiredBase, ClassData);

		const FString GeneratedName =
			ClassName.EndsWith(TEXT("_C")) ? ClassName : ClassName + TEXT("_C");

		FGraphNodeClassData* Match = nullptr;
		int32 MatchCount = 0;
		for (FGraphNodeClassData& Data : ClassData)
		{
			const FString CandidateName = Data.GetClassName();
			bool bNameMatches = (CandidateName == ClassName);
			if (!bNameMatches && Data.IsBlueprint())
			{
				bNameMatches = (CandidateName == GeneratedName) ||
					(Data.GetPackageName() + TEXT(".") + CandidateName == ClassName);
			}

			if (bNameMatches)
			{
				++MatchCount;
				Match = &Data;
			}
		}

		if (MatchCount != 1)
		{
			// Zero: unresolved. More than one: two classes under RequiredBase share this short
			// name in different packages, and silently picking one would resolve to the wrong
			// class while reporting success. Refuse instead of guessing.
			return nullptr;
		}

		// Only now, on the single surviving candidate, does GetClass() run — a load for a
		// not-yet-resident Blueprint, a pointer read for an already-resident native class.
		UClass* Resolved = Match->GetClass(/*bSilent=*/true);
		return (Resolved && Resolved->IsChildOf(RequiredBase)) ? Resolved : nullptr;
	}

	UObject* ResolvePropertyTarget(UEnvironmentQueryGraph* Graph, const FString& NodePath,
		const FString& AssetPath, FString& OutError)
	{
		UEnvironmentQueryGraphNode* Node = ResolveNodePath(Graph, NodePath);
		if (!Node)
		{
			OutError = FString::Printf(
				TEXT("No node at path '%s' in %s. Paths are index-based and zero-based, as in "
					 "'Option[0]' or 'Option[0]/@test[1]'; call GetQuery to list what is there."),
				*NodePath, *AssetPath);
			return nullptr;
		}

		if (const UEnvironmentQueryGraphNode_Option* OptionNode =
			Cast<UEnvironmentQueryGraphNode_Option>(Node))
		{
			UEnvQueryOption* OptionInstance = Cast<UEnvQueryOption>(ToRawPtr(OptionNode->NodeInstance));
			UEnvQueryGenerator* Generator =
				OptionInstance ? ToRawPtr(OptionInstance->Generator) : nullptr;
			if (!Generator)
			{
				OutError = FString::Printf(
					TEXT("'%s' in %s carries no generator, so it has no properties to read or write. "
						 "Give it one with SetOptionGenerator (the next commit would otherwise drop the "
						 "option entirely)."),
					*NodePath, *AssetPath);
				return nullptr;
			}
			return Generator;
		}

		UObject* Instance = ToRawPtr(Node->NodeInstance);
		if (!Instance)
		{
			OutError = FString::Printf(
				TEXT("'%s' in %s resolved to a node with no instance, so it has no properties"),
				*NodePath, *AssetPath);
			return nullptr;
		}
		return Instance;
	}

	bool IsAuthorableProperty(const FProperty* Property)
	{
		return Property
			&& Property->HasAnyPropertyFlags(CPF_Edit)
			&& !Property->HasAnyPropertyFlags(CPF_EditConst | CPF_Transient | CPF_Deprecated);
	}

	bool IsDataProviderProperty(const FProperty* Property)
	{
		const FStructProperty* StructProperty = CastField<const FStructProperty>(Property);
		return StructProperty != nullptr
			&& StructProperty->Struct != nullptr
			&& StructProperty->Struct->IsChildOf(FAIDataProviderValue::StaticStruct());
	}

	FString ExportPropertyValue(const FProperty* Property, const UObject* Instance)
	{
		if (!Property || !Instance)
		{
			return FString();
		}

		FString Value;
		// Delta == Data, on purpose. See the header: any other delta silently drops struct members
		// that happen to sit at their zero value, which for an FAIDataProviderFloatValue holding 0.0
		// is the entire payload.
		Property->ExportText_InContainer(0, Value, Instance, /*Delta*/ Instance,
			const_cast<UObject*>(Instance), PPF_None);
		return Value;
	}

	bool ImportPropertyValue(const FProperty* Property, void* Container, UObject* OwnerObject,
		const FString& ValueText, FString& OutError)
	{
		if (!Property || !Container)
		{
			OutError = TEXT("ImportPropertyValue: null property or container");
			return false;
		}

		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Container);

		// The pre-image, by value. FMemory::Malloc + InitializeValue + CopyCompleteValue is the
		// engine's own idiom for a standalone property value (UnrealType.h:1436); DestroyValue before
		// Free is what releases anything the copy owns (FString members, and the copy's share of any
		// UObject reference).
		void* PreImage = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
		Property->InitializeValue(PreImage);
		ON_SCOPE_EXIT
		{
			Property->DestroyValue(PreImage);
			FMemory::Free(PreImage);
		};
		Property->CopyCompleteValue(PreImage, ValuePtr);

		// Collected rather than sent to GWarn: an import failure is a value this function returns to
		// a caller, not log noise, and GWarn would additionally surface it as a warning in whatever
		// automation run happens to be executing.
		FStringOutputDevice ImportErrors;

		// FProperty::ImportSingleProperty has TWO recoverable-refusal branches, and only one of them
		// is audible by default. "Cannot perform text import on property" logs through
		// UE_SUPPRESS(LogExec, Warning, ...) (Property.cpp:1666) and arrives. "Unknown property in
		// <struct>" — a MISSPELLED member name — logs through UE_SUPPRESS(LogExec, Verbose, ...)
		// (Property.cpp:1660), and LogExec's default runtime verbosity is Warning
		// (CoreGlobals.h:46), so UE_SUPPRESS's IsSuppressed check (LogMacros.h:379) skips the Logf
		// entirely. The member is dropped in total silence: the import returns non-null, this device
		// stays empty, and "(DefultValue=2.5)" would be reported as a successful write of nothing.
		//
		// Raising the category for the duration of the import is what makes that branch observable.
		// It is not a blanket "log more": the only UE_SUPPRESS(LogExec, Verbose) sites reachable from
		// an import are that one and TopLevelAssetPath.cpp:295/318, which are the same
		// unknown-member message for a different container — failures for the same reason. Everything
		// else under LogExec already logs at Warning.
		//
		// Deliberately NOT done by comparing the exported value before and after: a legitimate
		// idempotent write (setting a property to the value it already holds) is indistinguishable
		// from a dropped one that way, and refusing it would break replaying a desired state.
		const ELogVerbosity::Type PreviousExecVerbosity = LogExec.GetVerbosity();
		LogExec.SetVerbosity(ELogVerbosity::Verbose);
		ON_SCOPE_EXIT
		{
			LogExec.SetVerbosity(PreviousExecVerbosity);
		};

		const TCHAR* Consumed = Property->ImportText_InContainer(*ValueText, Container, OwnerObject,
			PPF_None, &ImportErrors);

		// Both halves are failures. nullptr is the hard one (bad syntax, unknown enum value), but a
		// non-null return with a logged error is a PARTIAL import — ImportSingleProperty refuses an
		// individual member and keeps going (Property.cpp:1656-1667) — and accepting that would
		// report success for a write that half happened, or in the misspelled-member case never
		// happened at all.
		if (Consumed == nullptr || !ImportErrors.IsEmpty())
		{
			Property->CopyCompleteValue(ValuePtr, PreImage);
			OutError = FString::Printf(TEXT("'%s' is not a valid value for %s (%s)%s%s"),
				*ValueText, *Property->GetName(), *Property->GetCPPType(),
				ImportErrors.IsEmpty() ? TEXT("") : TEXT(": "), *ImportErrors);
			return false;
		}

		return true;
	}
}

#endif // WITH_VIBEUE_EQS
