// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UEnvQueryService.h"

#if WITH_VIBEUE_EQS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EnvQueryServiceInternal.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * BuildQuery: a JSON walk over the primitives, and nothing else.
 *
 * ===========================================================================================
 *  Why every primitive still commits, and what that costs
 * ===========================================================================================
 *
 * Each call below (AddOption, AddTest, SetPropertyValue, SetDataProviderValue, SetTestEnabled) runs
 * VibeEQS::OpenWriteGuard + VibeEQS::CommitGraph, and CommitGraph ends in
 * UEditorLoadingAndSavingUtils::SavePackages(..., bOnlyDirty = false). So a build of N options with
 * M tests performs N + M full package saves plus one per property write — for the suite's
 * round-trip fixture (2 options, 3 tests, 5 property writes) that is 10 saves for one BuildQuery.
 *
 * That is a disclosed deviation from "commit once", taken deliberately and for one reason: every
 * setter verifies its write against the COMMITTED state. UEnvironmentQueryGraph::UpdateAsset
 * regenerates UEnvQuery::Options, each option's Tests and every TestOrder on each commit, and
 * FinishPropertyWrite exists precisely to catch a write that regeneration discarded. A batch that
 * deferred the commit would have to either drop that verification — turning "the value was written"
 * into an assumption for exactly the calls a batch makes most of — or re-implement each setter
 * against a private uncommitted path, duplicating every guard in the service. The saves are the
 * price of the read-back, and the read-back is the only thing that makes a per-node result honest.
 *
 * (The sibling BehaviorTree batch layer made the same call for the same reason; recorded here so a
 * future "optimise the saves" pass finds the argument rather than rediscovering it.)
 *
 * ===========================================================================================
 *  The clear phase: why a wholesale replace is add-one-then-clear
 * ===========================================================================================
 *
 * bReplaceExisting = true must leave nothing of the old query. The obvious sequence — remove every
 * option, then build — cannot be run at all:
 *
 *   - RemoveOption refuses the LAST remaining option, and
 *   - the refusal is not arbitrary: CommitGraph's discard guard refuses any commit where a query
 *     that still has options would rebuild none of them, because UpdateAsset opens with
 *     GetOptionsMutable().Reset() and that shape is indistinguishable from the corruption the guard
 *     exists to catch.
 *
 * So a query that has options can never be committed with zero. The clear is therefore interleaved:
 * the FIRST new option is added (appended, so the old options keep indices 0..K-1), and only then
 * are the old K removed one at a time at "Option[0]" — each of those removals is allowed because the
 * new option survives it. When the clear finishes exactly one option remains, so its path is
 * "Option[0]" by construction rather than by arithmetic, and it is verified by GUID against the
 * option that was just added before the walk continues.
 *
 * The clear runs after the first SUCCESSFUL add, not before the walk: if no option in the JSON can
 * be created at all, the existing query is left exactly as it was rather than destroyed on behalf of
 * a build that produced nothing.
 *
 * Every later option is appended, and appending renumbers nothing, so the path each AddOption
 * returns stays valid for that option's own property and test writes. No path is ever computed from
 * a position in the source JSON.
 *
 * ===========================================================================================
 *  What this deliberately does NOT do
 * ===========================================================================================
 *
 * It does not call RepairGraphFromOptions as a prologue. That call refuses whenever the graph's root
 * already feeds option nodes — i.e. on every healthy query — so using it defensively would make a
 * guaranteed error the normal path. The health check is ValidateQuery's two structural diagnostics
 * instead ("sparse graph:", "no root node:"), which are read-only and name shapes no write could
 * survive. Every other ValidateQuery diagnostic is left to surface as the per-node failure it
 * already produces.
 */
namespace VibeEQSBuild
{
	/** "json:options[2]" — where a node the build could not create sits in the SUPPLIED JSON. */
	FString SourceOptionPath(int32 OptionIndex)
	{
		return FString::Printf(TEXT("json:options[%d]"), OptionIndex);
	}

	/** "json:options[2].tests[0]" — the same, for a test. */
	FString SourceTestPath(int32 OptionIndex, int32 TestIndex)
	{
		return FString::Printf(TEXT("json:options[%d].tests[%d]"), OptionIndex, TestIndex);
	}

	/**
	 * Split a struct literal — "(DefaultValue=2.500000,DataBinding=None,DataField=\"\")" — into its
	 * top-level members. False if the text is not a parenthesised struct at all.
	 *
	 * Depth- and quote-aware, so a nested struct or a string containing a comma cannot end a member
	 * early. This is not a general property-text parser and does not try to be: it is used only to
	 * read DefaultValue and DataBinding out of an FAIDataProviderValue, and any text it cannot make
	 * sense of is handed on verbatim rather than guessed at.
	 */
	bool SplitStructMembers(const FString& StructText, TMap<FString, FString>& OutMembers)
	{
		const FString Trimmed = StructText.TrimStartAndEnd();
		if (Trimmed.Len() < 2 || !Trimmed.StartsWith(TEXT("(")) || !Trimmed.EndsWith(TEXT(")")))
		{
			return false;
		}

		const FString Body = Trimmed.Mid(1, Trimmed.Len() - 2);

		TArray<FString> Segments;
		int32 Depth = 0;
		bool bInQuotes = false;
		int32 SegmentStart = 0;
		for (int32 Index = 0; Index < Body.Len(); ++Index)
		{
			const TCHAR Char = Body[Index];
			if (bInQuotes)
			{
				if (Char == TEXT('\\'))
				{
					++Index;
				}
				else if (Char == TEXT('"'))
				{
					bInQuotes = false;
				}
				continue;
			}

			if (Char == TEXT('"'))
			{
				bInQuotes = true;
			}
			else if (Char == TEXT('(') || Char == TEXT('['))
			{
				++Depth;
			}
			else if (Char == TEXT(')') || Char == TEXT(']'))
			{
				--Depth;
			}
			else if (Char == TEXT(',') && Depth == 0)
			{
				Segments.Add(Body.Mid(SegmentStart, Index - SegmentStart));
				SegmentStart = Index + 1;
			}
		}
		Segments.Add(Body.Mid(SegmentStart));

		for (const FString& Segment : Segments)
		{
			FString Key;
			FString Value;
			if (Segment.Split(TEXT("="), &Key, &Value))
			{
				OutMembers.Add(Key.TrimStartAndEnd(), Value.TrimStartAndEnd());
			}
		}
		return true;
	}

	/** The option "guid" strings GetQuery reports for AssetPath, in order. */
	TArray<FString> OptionGuids(const FString& QueryJson)
	{
		TArray<FString> Guids;

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(QueryJson);
		const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
			|| !Root->TryGetArrayField(TEXT("options"), Options) || Options == nullptr)
		{
			return Guids;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Options)
		{
			const TSharedPtr<FJsonObject>* Option = nullptr;
			FString Guid;
			if (Value.IsValid() && Value->TryGetObject(Option) && Option && Option->IsValid()
				&& (*Option)->TryGetStringField(TEXT("guid"), Guid))
			{
				Guids.Add(Guid);
			}
		}
		return Guids;
	}

	/**
	 * Remove every option except the one carrying KeepGuid, which must be the LAST one — the option
	 * this build just appended. True on success. See the file header for why the sequence is
	 * add-then-clear rather than clear-then-add.
	 *
	 * Re-reads GetQuery on every iteration rather than counting the removals up front: option paths
	 * are positional and each removal renumbers, so "Option[0]" is re-derived from the committed
	 * asset each time and the survivor is checked by GUID. Reads perform no writes and no saves.
	 */
	bool ClearOptionsExcept(const FString& AssetPath, const FString& KeepGuid, FString& OutError)
	{
		// One more than any option list this could legitimately face, so a RemoveOption that reported
		// success without removing anything ends as an error rather than as a hang.
		constexpr int32 MaxRemovals = 4096;

		for (int32 Removals = 0; Removals <= MaxRemovals; ++Removals)
		{
			const TArray<FString> Guids = OptionGuids(UEnvQueryService::GetQuery(AssetPath));

			if (Guids.Num() == 1)
			{
				if (Guids[0] != KeepGuid)
				{
					OutError = FString::Printf(
						TEXT("clearing %s left an option this build did not create (guid %s, expected "
							 "%s); the query is now a hybrid of the old and the new one and nothing "
							 "further was written"),
						*AssetPath, *Guids[0], *KeepGuid);
					return false;
				}
				return true;
			}

			if (Guids.Num() == 0)
			{
				OutError = FString::Printf(
					TEXT("clearing %s left no options at all, so the option this build had just added "
						 "is gone too"),
					*AssetPath);
				return false;
			}

			if (Guids[0] == KeepGuid)
			{
				// The kept option was appended, so it can only be at index 0 once it is the only one.
				// Reaching here means option order stopped agreeing with the order options were added
				// in, and removing "Option[0]" would delete the new option instead of an old one.
				OutError = FString::Printf(
					TEXT("clearing %s found the option this build just added at index 0 while %d "
						 "options remain; it was appended and should be last. Nothing further was "
						 "written."),
					*AssetPath, Guids.Num());
				return false;
			}

			const FString RemoveError =
				UEnvQueryService::RemoveOption(AssetPath, VibeEQS::MakeOptionPath(0));
			if (!RemoveError.IsEmpty())
			{
				OutError = FString::Printf(
					TEXT("clearing %s failed while removing 'Option[0]' with %d option(s) left: %s. The "
						 "query now holds both old and new options; nothing further was written."),
					*AssetPath, Guids.Num(), *RemoveError);
				return false;
			}
		}

		OutError = FString::Printf(
			TEXT("clearing %s did not terminate after %d removals"), *AssetPath, MaxRemovals);
		return false;
	}

	/** One property write, resolved to the setter that can perform it. */
	struct FPendingWrite
	{
		FString Name;
		FString Value;
	};

	/**
	 * Replay one node's "properties" map. Empty on success, else every reason, joined.
	 *
	 * Which names are data-provider values is asked of the node that was just CREATED
	 * (GetPropertyNames reports bIsDataProvider from the FProperty), never inferred from the shape of
	 * the JSON text: a plain FString property whose value happens to start with "(" would otherwise
	 * be routed to SetDataProviderValue and refused, and a provider written as a bare "2.5" by hand
	 * would be routed to SetPropertyValue and refused. The class is the authority.
	 *
	 * Provider writes are ordered before plain ones. Within each group the
	 * order is alphabetical rather than the JSON's own map order, so two runs of the same JSON
	 * perform the same writes in the same sequence (FJsonObject::Values is a TMap and does not
	 * preserve document order). No EQS property is known to depend on another being written first —
	 * this ordering is defensive, and matches the sibling BehaviorTree layer, where a key selector
	 * genuinely must precede the values resolved against it.
	 */
	FString ApplyProperties(const FString& AssetPath, const FString& NodePath,
		const TSharedPtr<FJsonObject>& Properties)
	{
		if (!Properties.IsValid() || Properties->Values.Num() == 0)
		{
			return FString();
		}

		TMap<FString, bool> ProviderFlags;
		for (const FEQSPropertyInfo& Info : UEnvQueryService::GetPropertyNames(AssetPath, NodePath))
		{
			ProviderFlags.Add(Info.Name, Info.bIsDataProvider);
		}

		TArray<FPendingWrite> ProviderWrites;
		TArray<FPendingWrite> PlainWrites;
		TArray<FString> Errors;

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Properties->Values)
		{
			FString ValueText;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(ValueText))
			{
				Errors.Add(FString::Printf(
					TEXT("'%s' is not a string; GetQuery encodes every property value as engine "
						 "property text"),
					*Pair.Key));
				continue;
			}

			// An unknown name is deliberately NOT rejected here: SetPropertyValue already refuses it
			// with the message that names the class and points at GetPropertyNames, and having one
			// wording for that failure is worth more than catching it a call earlier.
			const bool* IsProvider = ProviderFlags.Find(Pair.Key);
			FPendingWrite Write{ Pair.Key, ValueText };
			if (IsProvider && *IsProvider)
			{
				ProviderWrites.Add(MoveTemp(Write));
			}
			else
			{
				PlainWrites.Add(MoveTemp(Write));
			}
		}

		const auto ByName = [](const FPendingWrite& A, const FPendingWrite& B)
		{
			return A.Name < B.Name;
		};
		ProviderWrites.Sort(ByName);
		PlainWrites.Sort(ByName);

		for (const FPendingWrite& Write : ProviderWrites)
		{
			// SetDataProviderValue takes the BARE literal, while GetQuery emits the whole struct, so
			// the DefaultValue member is what gets replayed. Text that is not a struct literal at all
			// is passed straight through: that is a hand-written "2.5", which is exactly what the
			// setter wants.
			FString BareValue = Write.Value;
			TMap<FString, FString> Members;
			if (SplitStructMembers(Write.Value, Members))
			{
				const FString* Binding = Members.Find(TEXT("DataBinding"));
				if (Binding && !Binding->IsEmpty() && *Binding != TEXT("None"))
				{
					// A binding is an object reference to an Instanced UAIDataProvider living in the
					// SOURCE asset. Importing that text here would point this asset's test at the other
					// asset's provider — a cross-link, not a copy — and the value would then change
					// under both. Bindings are outside authoring scope by spec, so this is reported
					// rather than replayed, and the node is failed rather than quietly skipped.
					Errors.Add(FString::Printf(
						TEXT("'%s' carries a live data-provider binding (DataBinding=%s). A binding is "
							 "an object reference into the asset it came from, so replaying it here "
							 "would cross-link the two assets rather than copy the value. Bindings are "
							 "outside this call's authoring scope: set it by hand in the Environment "
							 "Query editor, or remove it from the JSON."),
						*Write.Name, **Binding));
					continue;
				}

				const FString* DefaultValue = Members.Find(TEXT("DefaultValue"));
				if (!DefaultValue)
				{
					Errors.Add(FString::Printf(
						TEXT("'%s' is a data-provider value but its text (%s) carries no DefaultValue "
							 "member, so there is no literal to replay"),
						*Write.Name, *Write.Value));
					continue;
				}
				BareValue = *DefaultValue;
			}

			const FEQSPropertySetResult SetResult =
				UEnvQueryService::SetDataProviderValue(AssetPath, NodePath, Write.Name, BareValue);
			if (!SetResult.bSuccess)
			{
				Errors.Add(SetResult.Error);
			}
		}

		for (const FPendingWrite& Write : PlainWrites)
		{
			const FEQSPropertySetResult SetResult =
				UEnvQueryService::SetPropertyValue(AssetPath, NodePath, Write.Name, Write.Value);
			if (!SetResult.bSuccess)
			{
				Errors.Add(SetResult.Error);
			}
		}

		return Errors.Num() ? FString::Join(Errors, TEXT("; ")) : FString();
	}
}

FEQSBuildResult UEnvQueryService::BuildQuery(const FString& AssetPath, const FString& QueryJson,
	bool bReplaceExisting)
{
	using namespace VibeEQSBuild;

	FEQSBuildResult Result;

	const auto Record = [&Result](const FString& Path, const FString& Error)
	{
		FEQSBuildNodeResult Node;
		Node.Path = Path;
		Node.bSuccess = Error.IsEmpty();
		Node.Error = Error;
		Result.Nodes.Add(MoveTemp(Node));
	};

	// --- The JSON ---------------------------------------------------------------------------------
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(QueryJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Result.Error = TEXT("QueryJson is not a JSON object. It must be the shape GetQuery emits: "
							"{ \"options\": [ { \"generator\": ..., \"tests\": [ ... ] } ] }.");
		return Result;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonOptions = nullptr;
	if (!Root->TryGetArrayField(TEXT("options"), JsonOptions) || JsonOptions == nullptr)
	{
		Result.Error = TEXT("QueryJson has no \"options\" array. GetQuery always emits one, even for a "
							"query it could not read.");
		return Result;
	}

	if (JsonOptions->Num() == 0)
	{
		// Not merely useless — unbuildable. A query with options can never be committed with none
		// (CommitGraph's discard guard), so an empty build against a populated target could not clear
		// it, and against an empty target it would write nothing while reporting success.
		Result.Error = TEXT("QueryJson describes no options. A query needs at least one: an option IS "
							"a generator, and UEnvironmentQueryGraph::UpdateAsset cannot commit a query "
							"whose graph rebuilds none.");
		return Result;
	}

	// --- The target -------------------------------------------------------------------------------
	FEQSQueryInfo Info;
	if (!GetQueryInfo(AssetPath, Info))
	{
		Result.Error = Info.Error;
		return Result;
	}

	// The health check, and pointedly not RepairGraphFromOptions — see the file header. Only the three
	// STRUCTURAL diagnostics are fatal: each names a graph no write could survive, and all are reported
	// by a read-only call that does not heal what it looks at.
	//
	// "orphaned option:" belongs here for a reason the other two make obvious only in hindsight. That
	// shape is an option present in UEnvQuery::Options whose graph node is unlinked from the root, so
	// the very first AddOption this build performs commits an option list SHORTER than the query's own,
	// EnsureGraph refuses it, and the asset is marked un-writable for the rest of the process — a
	// permanent in-memory poisoning triggered by a call that looked healthy going in. Refusing before
	// the first write turns that into a clean, repeatable refusal that names the offending option.
	for (const FString& Issue : ValidateQuery(AssetPath))
	{
		if (Issue.StartsWith(TEXT("sparse graph:")) || Issue.StartsWith(TEXT("no root node:"))
			|| Issue.StartsWith(TEXT("orphaned option:")))
		{
			Result.Error = FString::Printf(
				TEXT("Refusing to build into %s: %s Nothing was written."), *AssetPath, *Issue);
			return Result;
		}
	}

	// Both counts, because they can disagree in one direction: an option node carrying no generator
	// is addressable in the graph and absent from UEnvQuery::Options, and "the target has no options"
	// has to be false for either of them.
	const TArray<FString> ExistingGuids = OptionGuids(GetQuery(AssetPath));
	const int32 ExistingOptions = FMath::Max(Info.OptionCount, ExistingGuids.Num());

	if (ExistingOptions > 0 && !bReplaceExisting)
	{
		Result.Error = FString::Printf(
			TEXT("Refusing to build into %s: it already has %d option(s) and bReplaceExisting is "
				 "false. Pass bReplaceExisting = true to replace the query wholesale — there is no "
				 "setting that merges, because a half-replaced query is a shape nothing downstream "
				 "could tell from an intended one. Nothing was written."),
			*AssetPath, ExistingOptions);
		return Result;
	}

	// --- The walk ---------------------------------------------------------------------------------
	// bCleared is "the target holds nothing of its former self". Already true when it was empty; when
	// it was not, it becomes true after the first successful AddOption clears the old options out from
	// under it (see the file header for why that order).
	bool bCleared = (ExistingOptions == 0);
	FString AbortReason;

	for (int32 OptionIndex = 0; OptionIndex < JsonOptions->Num(); ++OptionIndex)
	{
		const TSharedPtr<FJsonObject>* OptionObject = nullptr;
		const bool bParsedOption = (*JsonOptions)[OptionIndex].IsValid()
			&& (*JsonOptions)[OptionIndex]->TryGetObject(OptionObject)
			&& OptionObject != nullptr && OptionObject->IsValid();

		const TArray<TSharedPtr<FJsonValue>>* JsonTests = nullptr;
		if (bParsedOption)
		{
			(*OptionObject)->TryGetArrayField(TEXT("tests"), JsonTests);
		}

		// Everything from here that gives up on this option reports its tests too, so a caller can
		// walk Nodes and find an entry for every node it asked for.
		const auto FailOptionAndItsTests = [&](const FString& OptionError, const FString& TestError)
		{
			Record(SourceOptionPath(OptionIndex), OptionError);
			if (JsonTests)
			{
				for (int32 TestIndex = 0; TestIndex < JsonTests->Num(); ++TestIndex)
				{
					Record(SourceTestPath(OptionIndex, TestIndex), TestError);
				}
			}
		};

		if (!AbortReason.IsEmpty())
		{
			const FString Skipped =
				FString::Printf(TEXT("not attempted: the build stopped earlier (%s)"), *AbortReason);
			FailOptionAndItsTests(Skipped, Skipped);
			continue;
		}

		if (!bParsedOption)
		{
			FailOptionAndItsTests(TEXT("not a JSON object"), TEXT("its option is not a JSON object"));
			continue;
		}

		FString GeneratorClass;
		(*OptionObject)->TryGetStringField(TEXT("generator"), GeneratorClass);
		if (GeneratorClass.IsEmpty())
		{
			// GetQuery emits an empty "generator" for an option node carrying none — the one shape the
			// commit drops. Replaying it would build a query that empties itself on the next save.
			FailOptionAndItsTests(
				TEXT("no \"generator\": an option IS its generator, and an option node without one is "
					 "dropped by the next commit"),
				TEXT("its option has no generator"));
			continue;
		}

		const FString AddResult = AddOption(AssetPath, GeneratorClass, /*Index*/ -1);
		if (AddResult.StartsWith(TEXT("ERROR")))
		{
			FailOptionAndItsTests(AddResult,
				FString::Printf(TEXT("not attempted: its option (%s) could not be created"),
					*GeneratorClass));
			continue;
		}

		FString OptionPath = AddResult;

		if (!bCleared)
		{
			// The option just added is the only thing standing between the old options and the discard
			// guard, so its guid is read here and checked by the clear at every step.
			const TArray<FString> Guids = OptionGuids(GetQuery(AssetPath));
			FString ClearError;
			if (Guids.Num() == 0)
			{
				ClearError = FString::Printf(
					TEXT("the option just added to %s is not readable back, so the existing options "
						 "cannot be cleared safely"),
					*AssetPath);
			}
			// Guids.Last() is the option this build just added only if AddOption really appended it —
			// and AddOption returned the path it created, so that is checkable rather than assumed.
			// If it disagrees, keeping the last guid would preserve a PRE-EXISTING option and remove
			// the new one: precisely the silent hybrid ClearOptionsExcept's own guid check exists to
			// refuse, arrived at one step earlier and without having destroyed anything yet.
			else if (AddResult != VibeEQS::MakeOptionPath(Guids.Num() - 1))
			{
				ClearError = FString::Printf(
					TEXT("AddOption reported '%s' for the option just added to %s, but the committed "
						 "query holds %d option(s), so the appended option is not the last one. "
						 "Refusing to clear: the survivor would be an option this build did not create. "
						 "Nothing further was written."),
					*AddResult, *AssetPath, Guids.Num());
			}
			else if (!ClearOptionsExcept(AssetPath, Guids.Last(), ClearError) && ClearError.IsEmpty())
			{
				ClearError = FString::Printf(
					TEXT("clearing the existing options of %s failed without a reason"), *AssetPath);
			}

			if (!ClearError.IsEmpty())
			{
				AbortReason = ClearError;
				Result.Error = ClearError;
				FailOptionAndItsTests(ClearError,
					FString::Printf(TEXT("not attempted: the build stopped earlier (%s)"), *ClearError));
				continue;
			}

			// Exactly one option remains and the clear verified it is this one, so this is the path by
			// construction rather than by arithmetic over what was removed.
			OptionPath = VibeEQS::MakeOptionPath(0);
			bCleared = true;
		}

		const TSharedPtr<FJsonObject>* OptionProperties = nullptr;
		(*OptionObject)->TryGetObjectField(TEXT("properties"), OptionProperties);
		Record(OptionPath, ApplyProperties(AssetPath, OptionPath,
			OptionProperties ? *OptionProperties : nullptr));

		if (!JsonTests)
		{
			continue;
		}

		for (int32 TestIndex = 0; TestIndex < JsonTests->Num(); ++TestIndex)
		{
			const TSharedPtr<FJsonObject>* TestObject = nullptr;
			if (!(*JsonTests)[TestIndex].IsValid()
				|| !(*JsonTests)[TestIndex]->TryGetObject(TestObject)
				|| TestObject == nullptr || !TestObject->IsValid())
			{
				Record(SourceTestPath(OptionIndex, TestIndex), TEXT("not a JSON object"));
				continue;
			}

			FString TestClass;
			(*TestObject)->TryGetStringField(TEXT("class"), TestClass);
			if (TestClass.IsEmpty())
			{
				Record(SourceTestPath(OptionIndex, TestIndex),
					TEXT("no \"class\": GetQuery emits an empty class for a sub-node carrying no "
						 "instance, which the commit skips"));
				continue;
			}

			const FString AddTestResult = AddTest(AssetPath, OptionPath, TestClass, /*Index*/ -1);
			if (AddTestResult.StartsWith(TEXT("ERROR")))
			{
				Record(SourceTestPath(OptionIndex, TestIndex), AddTestResult);
				continue;
			}

			const FString TestPath = AddTestResult;
			TArray<FString> TestErrors;

			const TSharedPtr<FJsonObject>* TestProperties = nullptr;
			(*TestObject)->TryGetObjectField(TEXT("properties"), TestProperties);
			const FString PropertyError = ApplyProperties(AssetPath, TestPath,
				TestProperties ? *TestProperties : nullptr);
			if (!PropertyError.IsEmpty())
			{
				TestErrors.Add(PropertyError);
			}

			// After the properties, and never through the property map: bTestEnabled lives on
			// UEnvironmentQueryGraphNode_Test, not on the UEnvQueryTest, so no property write can reach
			// it. Without this call a round-trip silently produces a query with every test running —
			// the properties would all match and the query would do something else.
			bool bEnabled = true;
			(*TestObject)->TryGetBoolField(TEXT("enabled"), bEnabled);
			if (!bEnabled)
			{
				const FString EnabledError = SetTestEnabled(AssetPath, TestPath, false);
				if (!EnabledError.IsEmpty())
				{
					TestErrors.Add(EnabledError);
				}
			}

			Record(TestPath, TestErrors.Num() ? FString::Join(TestErrors, TEXT("; ")) : FString());
		}
	}

	int32 FailedNodes = 0;
	for (const FEQSBuildNodeResult& Node : Result.Nodes)
	{
		FailedNodes += Node.bSuccess ? 0 : 1;
	}

	Result.bSuccess = (FailedNodes == 0) && Result.Error.IsEmpty();
	if (FailedNodes > 0 && Result.Error.IsEmpty())
	{
		// A count and a pointer, not a cause: the causes are on the nodes, one each, and folding them
		// into this string is exactly the collapse a per-node result exists to prevent.
		Result.Error = FString::Printf(
			TEXT("%d of %d node(s) failed; see Nodes for the reason of each."),
			FailedNodes, Result.Nodes.Num());
	}

	return Result;
}

#else  // WITH_VIBEUE_EQS

FEQSBuildResult UEnvQueryService::BuildQuery(const FString&, const FString&, bool)
{
	FEQSBuildResult Result;
	Result.Error = TEXT("EQS authoring is unavailable: the EnvironmentQueryEditor plugin is not "
						"enabled in this build (WITH_VIBEUE_EQS=0).");
	return Result;
}

#endif // WITH_VIBEUE_EQS
