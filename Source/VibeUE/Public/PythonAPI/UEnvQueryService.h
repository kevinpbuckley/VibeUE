// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UEnvQueryService.generated.h"

/** One EQS class available for AddOption / AddTest, or referenceable as a context. */
USTRUCT(BlueprintType)
struct FEQSClassInfo
{
	GENERATED_BODY()

	/** Class name as passed to AddOption / AddTest, e.g. "EnvQueryTest_Distance". */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString ClassName;

	/**
	 * For a Blueprint-derived class, its full object path (e.g. "/Game/AI/EQC_Foo.EQC_Foo_C").
	 * For a native class, just the bare class name (e.g. "EnvQueryTest_Distance") — native
	 * classes carry no package here (listing never calls GetClass(), the only route that would
	 * resolve one), so this is NOT a loadable path for them.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString ClassPath;

	/** Author-declared category shown in the node picker. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Category;

	/** True if this is a Blueprint-derived class rather than a native one. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	bool bIsBlueprint = false;
};

/** Summary of an Environment Query asset. */
USTRUCT(BlueprintType)
struct FEQSQueryInfo
{
	GENERATED_BODY()

	/** Number of options (generators) the query runs. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	int32 OptionCount = 0;

	/**
	 * Total ENABLED tests across every option — this counts UEnvQueryOption::Tests, which the commit
	 * rebuilds from the enabled sub-nodes only. GetQuery's "tests" arrays are longer whenever an
	 * option carries a disabled test, and each entry there says which it is via "enabled".
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	int32 TestCount = 0;

	/** False if EdGraph is null — the asset has never been opened or created by this service. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	bool bHasGraph = false;

	/** Whether the graph contains a root node. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	bool bHasRootNode = false;

	/** Populated when the asset could not be read. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Error;
};

/** One authorable property of a generator or test, with its current value. */
USTRUCT(BlueprintType)
struct FEQSPropertyInfo
{
	GENERATED_BODY()

	/** Property name, as SetPropertyValue / SetDataProviderValue accept it. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Name;

	/** C++ type, e.g. "FAIDataProviderFloatValue" or "TEnumAsByte<EEnvTestScoreEquation::Type>". */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Type;

	/** Current value as text, in the same encoding SetPropertyValue accepts. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Value;

	/**
	 * True when the value is an FAIDataProviderValue-derived struct — a literal wrapped in
	 * "(DefaultValue=...)" that may instead be BOUND to a data provider. Set these with
	 * SetDataProviderValue, which writes the literal and refuses to overwrite a binding.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	bool bIsDataProvider = false;
};

/** Outcome of a property write. */
USTRUCT(BlueprintType)
struct FEQSPropertySetResult
{
	GENERATED_BODY()

	/** True only if the value was written AND still held that value after the commit. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	bool bSuccess = false;

	/** Populated whenever bSuccess is false. Empty otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Error;

	/**
	 * The class the node path actually resolved to — the GENERATOR's class for an option path, the
	 * test's class for a test path. Populated as soon as the path resolves, so a later failure still
	 * says which object was addressed.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString ResolvedNodeClass;

	/**
	 * The value read back AFTER the commit, not the value that was passed in. UpdateAsset runs during
	 * the commit and rewrites parts of the asset, so this is the only honest answer to "what does it
	 * hold now".
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString ValueAfterWrite;
};

/** The outcome of ONE node in a BuildQuery walk — one option, or one test. */
USTRUCT(BlueprintType)
struct FEQSBuildNodeResult
{
	GENERATED_BODY()

	/**
	 * Where this node is, and the two forms mean different things.
	 *
	 * For a node that was CREATED it is the node's path in the resulting asset ("Option[1]",
	 * "Option[1]/@test[0]") — the address every other method on this service accepts.
	 *
	 * For a node that was never created — an unknown generator or test class, or a node skipped
	 * because the build had already aborted — it is its position in the SUPPLIED JSON, written
	 * "json:options[1]" / "json:options[1].tests[0]". Those are deliberately not asset paths: a
	 * failed option is absent from the result, so every later option sits one index lower than the
	 * JSON says, and reporting the JSON index as an asset path would name a different node than the
	 * one that failed. The two forms cannot collide.
	 *
	 * One exception to "created means an asset path", and it is deliberate: when the CLEAR phase of a
	 * bReplaceExisting build fails, the option it had just created reports a "json:" path too. That
	 * option exists in the asset, but the clear is exactly the step that establishes where — it is what
	 * makes the survivor "Option[0]" — so a failed clear leaves its committed index genuinely unknowable
	 * and naming one would be a guess. The build aborts there, and FEQSBuildResult::Error says the query
	 * now holds both old and new options.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Path;

	/** True only if every write this node needed succeeded. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	bool bSuccess = false;

	/**
	 * Populated whenever bSuccess is false. A node with several failed property writes carries all
	 * of their reasons, joined — one node result is one node, not one write.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Error;
};

/** The outcome of a whole BuildQuery walk. */
USTRUCT(BlueprintType)
struct FEQSBuildResult
{
	GENERATED_BODY()

	/** False if anything was refused up front, OR if any single node failed. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	bool bSuccess = false;

	/**
	 * The whole-build reason: a malformed QueryJson, a refused target, an aborted clear. Also carries
	 * a one-line count when the build ran but some nodes failed — the reasons themselves stay on the
	 * nodes, because a partial failure that collapses into one string cannot be acted on.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Error;

	/**
	 * One entry per node the JSON described, in JSON order: option, then that option's tests, then
	 * the next option. Empty only when the build was refused before any node was attempted.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	TArray<FEQSBuildNodeResult> Nodes;
};

/** One item a query run produced, with the score it was ranked by. */
USTRUCT(BlueprintType)
struct FEQSResultItem
{
	GENERATED_BODY()

	/**
	 * The item's world location. Populated for every item type derived from
	 * UEnvQueryItemType_VectorBase — which includes the actor types, since
	 * UEnvQueryItemType_ActorBase derives from it. Zero for an item type that is neither.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FVector Location = FVector::ZeroVector;

	/**
	 * The item's actor, by UObject name, or EMPTY when the query generated locations rather than
	 * actors. Empty is therefore not a failure — it is what a point-generator query looks like.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString ActorName;

	/**
	 * The item's final score. Whether it is normalized to 0..1 is decided by the generator's
	 * UEnvQueryGenerator::GetNormalizationOption AND by the RunMode this call was asked for — which
	 * makes it partly this service's argument, not purely the generator's choice.
	 *
	 * FEnvQueryInstance::FinalizeQuery resolves the generator's Default option differently per branch:
	 * under "AllMatching" Default falls through to Normalized (EnvQueryInstance.cpp:865-868), under
	 * every other mode it falls through to Unaltered (:820-822, :837-839). So the same query on the
	 * same generator normalizes at AllMatching and does not at SingleResult — a live PIE run measured
	 * 0.9517..1.0000 at AllMatching and a best score of 1.0508 at SingleResult on one query. Compare
	 * scores within one run of one mode, never across modes and never across queries.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	float Score = 0.0f;

	/**
	 * Whether the item passed every test — FEnvQueryItem::IsValid().
	 *
	 * True for every item under "AllMatching", and that is a fact about the engine rather than about
	 * this service: FEnvQueryInstance::FinalizeQuery truncates Items to NumValidItems there
	 * (EnvQueryInstance.cpp:855). It is NOT true under the three single-pick modes, which never
	 * truncate in an editor build — failed items come back alongside the passing ones. So this is read
	 * from each item rather than assumed, and a caller filtering results must actually test it. See
	 * FEQSRunResult::Items for the mechanism.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	bool bPassed = false;
};

/** The outcome of one RunQuery execution. */
USTRUCT(BlueprintType)
struct FEQSRunResult
{
	GENERATED_BODY()

	/**
	 * True only when the query RAN and finished with EEnvQueryStatus::Success.
	 *
	 * A query that executed correctly and matched nothing is false, with Status "Failed" — that is
	 * the engine's own word for it (FinalizeQuery marks a run with no valid items as failed), and
	 * folding it into success would report an empty result set as a good one.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	bool bSuccess = false;

	/** Populated whenever bSuccess is false. Empty otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Error;

	/**
	 * The engine's EEnvQueryStatus name: "Success", "Failed", "Aborted", "OwnerLost",
	 * "MissingParam", "Processing".
	 *
	 * EMPTY when the query never ran — a rejected RunMode, no play session, an unresolved asset or
	 * querier. Status and Error together therefore say which side of the call failed: a refusal has
	 * an Error and no Status, an executed-but-unsuccessful query has both.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	FString Status;

	/** Items.Num(), reported so a caller can check the count without walking the array. */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	int32 ItemCount = 0;

	/**
	 * The items. Empty for a refusal, and also for a query that ran and matched nothing.
	 *
	 * Only "AllMatching" gives the tidy contract — best first, failed items dropped, scores normalized
	 * unless the generator opted out (EnvQueryInstance.cpp:848-870). The three single-pick modes return
	 * neither one item nor a reliably ranked list, and that surprises everyone exactly once:
	 *
	 *   - FEnvQueryInstance::PickSingleItem keeps the WHOLE item array whenever bStoreDebugInfo is set
	 *     (:773-795). It swaps the pick to index 0 and returns everything else untruncated, failed
	 *     items included. That flag defaults to true in every non-shipping build
	 *     (FEnvQueryInstance::bDebuggingInfoEnabled, :107; USE_EQS_DEBUGGER, EnvQueryTypes.h:33) —
	 *     which is every build this editor-only service can run in.
	 *   - "SingleResult" has a second exit on top of that. When the last test was a pure condition
	 *     (bFoundSingleResult || bPassOnSingleResult) FinalizeQuery skips its whole SingleResult body
	 *     (:811-830): no SortScores, no normalization and no pick at all, so the items come back in
	 *     GENERATION order carrying raw scores. A live PIE run measured exactly that — 27 items with a
	 *     best score of 1.0508, against the same query's 27 items sorted 1.0000..0.9517 at AllMatching.
	 *
	 * So: treat index 0 as the engine's answer and the array as a ranking only under AllMatching.
	 * Under the other three, index 0 is the pick when a pick happened at all, the remaining entries are
	 * the debugger's view of the run rather than runners-up, and bPassed is per item for a reason.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "EQS")
	TArray<FEQSResultItem> Items;
};

/**
 * Read, author, tune and run Environment Query (EQS) assets.
 *
 * All writes go through the editor EdGraph (UEnvQuery::EdGraph) and commit with
 * UEnvironmentQueryGraph::UpdateAsset(). Writing UEnvQuery::Options directly is a trap: UpdateAsset
 * calls GetOptionsMutable().Reset() and regenerates the whole array from the graph, so a direct
 * write reads back correctly and is discarded on the next commit.
 *
 * Requires the EnvironmentQueryEditor plugin. Without it every method reports the feature as
 * unavailable rather than failing to build.
 */
UCLASS(BlueprintType)
class VIBEUE_API UEnvQueryService : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** All Environment Query assets under DirectoryPath, as package paths. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static TArray<FString> ListQueries(const FString& DirectoryPath = TEXT("/Game"));

	/** Generator classes available to AddOption. Includes Blueprint-derived classes. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static TArray<FEQSClassInfo> GetAvailableGeneratorTypes();

	/** Test classes available to AddTest. Includes Blueprint-derived classes. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static TArray<FEQSClassInfo> GetAvailableTestTypes();

	/** Context classes referenceable from generator and test properties. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static TArray<FEQSClassInfo> GetAvailableContextTypes();

	/**
	 * Create an Environment Query asset, its editor graph and its root node. Returns the EMPTY string
	 * on success, else the reason — the same convention as every other FString-returning method here
	 * except AddOption / AddTest, which have a path to return and prefix their failures with "ERROR:".
	 * Creating over an existing asset is refused rather than overwriting it.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString CreateQuery(const FString& AssetPath);

	/** Summary of an Environment Query asset. Returns false if it could not be loaded. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static bool GetQueryInfo(const FString& AssetPath, FEQSQueryInfo& OutInfo);

	/**
	 * The whole query as JSON:
	 * { "options": [ { "path", "guid", "generator", "properties", "tests": [ ... ] } ] }
	 *
	 * Options are listed in execution order, and their "path" is what every edit method below
	 * accepts. Those paths are POSITIONAL, so this must be re-read after every add, remove or move:
	 * the "guid" of an option is stable across such a change, its path is not.
	 *
	 * Each test carries "enabled". A disabled test is still listed, and still addressable at its
	 * "@test[n]" path, but does not run and is not counted by GetQueryInfo's TestCount.
	 *
	 * Read-only: unlike the write path it never creates the editor graph, so an asset that
	 * has never been opened reports that fact instead of being modified by being looked at. On
	 * failure the object carries "error" and an empty "options", so a caller can walk the array
	 * unconditionally.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString GetQuery(const FString& AssetPath);

	/**
	 * Add an option running GeneratorClassName, at Index (appended when Index < 0, clamped when
	 * beyond the end). Returns the new option's path, or "ERROR: <reason>".
	 *
	 * An option IS its generator: there is no such thing as an option without one, which is why the
	 * generator is a required argument rather than something to set afterwards.
	 *
	 * Option paths are positional, so an insert RENUMBERS every option at or after Index: a path
	 * held from before this call may now name a different option. Re-read GetQuery before using
	 * one — nothing downstream can detect the mix-up, it simply edits the wrong option.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString AddOption(const FString& AssetPath, const FString& GeneratorClassName,
		int32 Index = -1);

	/**
	 * Remove the option at OptionPath, and every test hanging off it. Empty on success, else the
	 * reason.
	 *
	 * Removing the LAST remaining option is refused: UpdateAsset would rebuild an empty option list
	 * over a query that still has one, which is the shape CommitGraph's discard guard exists to
	 * refuse. The refusal happens before anything is touched rather than at commit time.
	 *
	 * Option paths are positional, so a removal RENUMBERS every option after it — "Option[2]"
	 * afterwards is the option that used to be "Option[3]". Re-read GetQuery before reusing a path,
	 * and never remove a list of paths in ascending order without doing so.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString RemoveOption(const FString& AssetPath, const FString& OptionPath);

	/**
	 * Move the option at OptionPath to NewIndex (to the end when NewIndex < 0, clamped when beyond
	 * it). Empty on success, else the reason. Option order is execution order: the query runs
	 * options in turn and stops at the first that produces a valid result.
	 *
	 * Which means a move RENUMBERS every option between the old and new positions, including this
	 * one: after MoveOption(p, "Option[2]", 0) the path "Option[2]" names a different option than it
	 * did a moment ago, and the option that moved is at "Option[0]". Re-read GetQuery — this call
	 * silently invalidates every path a caller is holding.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString MoveOption(const FString& AssetPath, const FString& OptionPath, int32 NewIndex);

	/**
	 * Replace the generator of the option at OptionPath, KEEPING its tests. Empty on success, else
	 * the reason.
	 *
	 * This exists so that changing a generator is not remove-plus-add: an option owns its tests, so
	 * re-adding it would silently discard every test authored against it.
	 *
	 * This call does not itself renumber anything, but option paths are positional: one held from
	 * before an intervening AddOption / RemoveOption / MoveOption may now name a different option,
	 * and replacing the wrong option's generator reports success. Re-read GetQuery after any of
	 * those.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString SetOptionGenerator(const FString& AssetPath, const FString& OptionPath,
		const FString& GeneratorClassName);

	/**
	 * Add a test of TestClassName to the option at OptionPath, at Index (appended when Index < 0,
	 * clamped when beyond the end). Returns the new test's path ("Option[1]/@test[0]"), or
	 * "ERROR: <reason>".
	 *
	 * Test order is not cosmetic: it is the order the tests run in, it is what UpdateAsset writes
	 * into each UEnvQueryTest::TestOrder, and a cheap filtering test placed first is the difference
	 * between a query that costs nothing and one that scores every item.
	 *
	 * Test paths are positional within their option, so an insert RENUMBERS every test at or after
	 * Index on that option — "@test[1]" afterwards is the test that used to be "@test[0]" if this
	 * inserted at 0. It also does NOT renumber anything on any other option. Re-read GetQuery.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString AddTest(const FString& AssetPath, const FString& OptionPath,
		const FString& TestClassName, int32 Index = -1);

	/**
	 * Remove the test at TestPath ("Option[0]/@test[2]"). Empty on success, else the reason.
	 *
	 * Unlike RemoveOption there is no last-one refusal: an option with no tests is a perfectly valid
	 * query (it scores every generated item equally), so removing the only test destroys nothing the
	 * commit could not rebuild from the graph.
	 *
	 * Test paths are positional, so a removal RENUMBERS every later test on the SAME option. Re-read
	 * GetQuery before reusing a path, and never remove a list of test paths in ascending order
	 * without doing so.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString RemoveTest(const FString& AssetPath, const FString& TestPath);

	/**
	 * Move the test at TestPath to NewIndex within its own option (to the end when NewIndex < 0,
	 * clamped when beyond it). Empty on success, else the reason. This never moves a test between
	 * options — a test belongs to the option that owns it.
	 *
	 * A move RENUMBERS every test between the old and new positions on that option, including this
	 * one, so every test path a caller is holding for it is invalidated. Re-read GetQuery.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString MoveTest(const FString& AssetPath, const FString& TestPath, int32 NewIndex);

	/**
	 * Switch the test at TestPath on or off. Empty on success, else the reason.
	 *
	 * This is a GRAPH-NODE write, not a property write: bTestEnabled lives on
	 * UEnvironmentQueryGraphNode_Test, not on the UEnvQueryTest instance, so no property setter could
	 * ever reach it — which is why it needs a method of its own rather than being one more name
	 * SetProperty accepts.
	 *
	 * Disabling does NOT remove the test. The sub-node stays in the graph, GetQuery keeps listing it
	 * (with "enabled": false) and its "@test[n]" path keeps working, so nothing renumbers. What
	 * changes is UEnvQueryOption::Tests, which the commit rebuilds from the ENABLED sub-nodes only
	 * (EnvironmentQueryGraph.cpp:90): a disabled test leaves the runtime array, stops running, and
	 * stops being counted by GetQueryInfo's TestCount.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString SetTestEnabled(const FString& AssetPath, const FString& TestPath, bool bEnabled);

	/**
	 * Every authorable property of the object NodePath addresses, with its current value.
	 *
	 * NodePath is an option path ("Option[0]") or a test path ("Option[0]/@test[1]"). An option path
	 * describes its GENERATOR — the same object GetQuery reports properties for under that option,
	 * because a UEnvQueryOption itself exposes nothing tunable.
	 *
	 * This lists EVERY authorable property; GetQuery's "properties" map lists only those differing
	 * from the class default. Both use the same filter and the same value encoding, so a name here is
	 * a name SetPropertyValue accepts, and a value here is a value it round-trips.
	 *
	 * UEnvQueryTest::TestOrder is deliberately absent: it carries no edit specifier and every commit
	 * overwrites it from the sub-node order. Use MoveTest.
	 *
	 * Read-only, so it never creates the editor graph; an asset that has never been opened returns
	 * empty. Paths are POSITIONAL — re-read GetQuery after any add, remove or move.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static TArray<FEQSPropertyInfo> GetPropertyNames(const FString& AssetPath,
		const FString& NodePath);

	/**
	 * The current value of one property as text, or "ERROR: <reason>".
	 *
	 * The encoding is the engine's own property text, exported with the instance as its own delta so
	 * nothing is elided: a data-provider knob reads as
	 * "(DefaultValue=2.500000,DataBinding=None,DataField=\"\")", never as a bare number, and still
	 * carries its literal when every member sits at zero.
	 *
	 * Read-only, and paths are POSITIONAL — see GetPropertyNames.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString GetPropertyValue(const FString& AssetPath, const FString& NodePath,
		const FString& PropertyName);

	/**
	 * Write one property from its text form and save.
	 *
	 * Value is engine property text, exactly what GetPropertyValue returns: "InverseLinear" for an
	 * enum, "true" for a bool, "(DefaultValue=2.5)" for a data-provider knob. For those knobs prefer
	 * SetDataProviderValue, which takes the bare number and refuses to silently discard a binding —
	 * "2.5" alone is not a valid FAIDataProviderFloatValue and is rejected here.
	 *
	 * A failed import changes NOTHING: the value is restored from a by-value pre-image, so a struct
	 * text that applies two members and then fails on the third leaves all three as they were.
	 *
	 * ValueAfterWrite is read back AFTER the commit and compared with what the import produced, so a
	 * write UpdateAsset silently undid is reported as a failure rather than as the value it was
	 * handed. Node paths are POSITIONAL and shift after any AddOption / RemoveOption / MoveOption /
	 * AddTest / RemoveTest / MoveTest — re-read GetQuery first, or this tunes the wrong node and
	 * reports success.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FEQSPropertySetResult SetPropertyValue(const FString& AssetPath, const FString& NodePath,
		const FString& PropertyName, const FString& Value);

	/**
	 * Write the literal held by an FAIDataProviderValue-derived property — ScoringFactor,
	 * FloatValueMin, ScoreClampMax, BoolValue, a generator's GridSize — from a bare value: "2.5",
	 * "true".
	 *
	 * Every scoring knob on a UEnvQueryTest is one of these structs rather than a number
	 * (EnvQueryTest.h:88-139), so SetPropertyValue(..., "ScoringFactor", "2.5") is the write everyone
	 * reaches for and it is not a valid value for the property. This is the call that means what it
	 * looks like: it writes the struct's DefaultValue member and leaves the rest alone.
	 *
	 * It REFUSES when the property is currently bound to a data provider (DataBinding is set), rather
	 * than writing a DefaultValue the query would never read — a bound knob takes its value from the
	 * provider's field, so a "successful" write there is a silent no-op at runtime. Replace a binding
	 * deliberately with SetPropertyValue and a full struct literal.
	 *
	 * It also refuses on a property that is NOT one of these structs, instead of falling back to a
	 * generic write: "InverseLinear" reaching this call means the caller believes ScoringEquation is a
	 * data provider, and quietly doing the right thing would leave that belief in place.
	 *
	 * Same commit-time read-back, same by-value rollback and the same POSITIONAL-path warning as
	 * SetPropertyValue.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FEQSPropertySetResult SetDataProviderValue(const FString& AssetPath,
		const FString& NodePath, const FString& PropertyName, const FString& Value);

	/**
	 * Author a whole query from the JSON GetQuery emits, node by node.
	 *
	 * QueryJson is exactly GetQuery's shape — { "options": [ { "generator", "properties",
	 * "tests": [ { "class", "enabled", "properties" } ] } ] } — so read, edit and write round-trip.
	 * "path" and "guid" are ignored on the way in: both are positional or identity facts of the
	 * SOURCE asset, and the nodes created here get their own.
	 *
	 * This introduces no engine behaviour of its own. It is AddOption / AddTest / SetPropertyValue /
	 * SetDataProviderValue / SetTestEnabled driven from JSON, and every guard, refusal and
	 * commit-time read-back of those calls applies unchanged. In particular each of them commits and
	 * SAVES, so a build of N options and M tests performs one full package save per node plus one per
	 * property write — the deliberate cost of having every setter verify against the COMMITTED asset
	 * rather than against memory.
	 *
	 * Two things in the JSON need translating rather than replaying, and both are why this is not a
	 * loop over SetPropertyValue:
	 *   - a property GetQuery reports as a data-provider value ("(DefaultValue=2.5,DataBinding=None,
	 *     DataField=\"\")") is written with SetDataProviderValue from its DefaultValue alone, and
	 *     provider writes are ordered before plain ones on the same node;
	 *   - "enabled" is a graph-node field, not a property, so a test whose JSON says false gets an
	 *     explicit SetTestEnabled — replaying "properties" alone would silently produce a query with
	 *     every test running.
	 *
	 * A property carrying a live DataBinding is REFUSED, not replayed. A binding exports as an object
	 * reference to an Instanced provider living in the source asset, so importing it into another
	 * asset would cross-link the two rather than copy anything. Bindings are outside authoring scope;
	 * the node reports the refusal by name.
	 *
	 * The JSON must describe AT LEAST ONE option, and an empty "options" array is refused rather than
	 * treated as "clear the query". A query with options can never be committed with none (that is
	 * CommitGraph's discard guard), so an empty build could not clear a populated target and would
	 * write nothing while reporting success against an empty one.
	 *
	 * The target is refused up front on any of ValidateQuery's three STRUCTURAL diagnostics —
	 * "sparse graph:", "no root node:", "orphaned option:" — each of which names a graph no write
	 * could survive. Nothing is written in that case, which is the point: attempting the build would
	 * commit an option list shorter than the query's own and poison the in-memory asset for every
	 * later write.
	 *
	 * bReplaceExisting is the whole-query switch and there is no middle setting: false requires the
	 * target to have no options and refuses otherwise, true removes every existing option first. It
	 * can never produce a hybrid of the old query and the new one.
	 *
	 * The result is per node. bSuccess is false if ANY node failed, and each node says which it was
	 * and why — a build that placed four options and could not resolve the fifth's generator reports
	 * exactly that, rather than one boolean.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FEQSBuildResult BuildQuery(const FString& AssetPath, const FString& QueryJson,
		bool bReplaceExisting = false);

	/** Re-run layout, regenerate the options from the graph, and save. */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString CompileAndSave(const FString& AssetPath);

	/**
	 * Everything wrong with a query, as one human-readable line each. A healthy query returns an
	 * EMPTY array — a validator that always says something is as useless as one that never does.
	 *
	 * Reports:
	 *   - "sparse graph: ..."       the query has options but its graph's root feeds none of them;
	 *   - "no root node: ..."       the graph has no UEnvironmentQueryGraphNode_Root at all, so there
	 *                               is nothing for an option to hang off;
	 *   - "orphaned option: ..."    ONE option whose node exists but is unlinked from the root — the
	 *                               partial form of the sparse shape, which RepairGraphFromOptions
	 *                               cannot fix (SpawnMissingNodes skips an option that has a node);
	 *   - "<path>: no generator"    an option node carrying no generator (the shape the commit drops);
	 *   - "<path>: node class ..."  a node whose class failed to load (UAIGraphNode::HasErrors);
	 *   - "<path>: unresolved ..."  a UEnvQueryContext reference cleared to None where the class
	 *                               default supplies one.
	 *
	 * Plus two input failures that are about the call rather than the asset: "AssetPath is empty" and
	 * "Environment Query not found: ...", each returned alone.
	 *
	 * The first three are STRUCTURAL and are what BuildQuery treats as fatal before it writes anything;
	 * the last three are per-node and are left to surface as the per-node failures they already are.
	 *
	 * READ-ONLY, and that is a hard requirement rather than a convention: it loads with LoadObject
	 * only, and pointedly does NOT route through the write guard, whose EnsureGraph would run
	 * UEnvironmentQueryGraph::Initialize() and repair the very sparse graph this is meant to report.
	 * A validator that heals the asset it diagnoses can never report the illness twice, and would
	 * make a read of a production asset a silent write.
	 *
	 * The sparse shape is therefore only observable here while it is still sparse: any write through
	 * this service repairs it as a side effect of opening (see RepairGraphFromOptions), so what this
	 * reports is an asset no write path has touched yet. Nothing in this service can save that shape
	 * — the commit-time discard guard refuses it — so reaching it from disk requires an asset
	 * written by something else. That route is argued from the guards rather than measured: the
	 * suite exercises the shape in memory only.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static TArray<FString> ValidateQuery(const FString& AssetPath);

	/**
	 * Rebuild the option and test nodes of a sparse graph from UEnvQuery::Options, then lay out,
	 * commit and save. Empty on success, else the reason. This is the cure ValidateQuery's
	 * "sparse graph" diagnostic names.
	 *
	 * The reconstruction is UEnvironmentQueryGraph::SpawnMissingNodes(), reached through
	 * Initialize() — it REUSES the live UEnvQueryOption / UEnvQueryTest objects rather than
	 * recreating them (EnvironmentQueryGraph.cpp:426-446), so a repaired query keeps every property
	 * ever authored on it.
	 *
	 * Refuses when there is nothing to repair — no options, or the graph already has option nodes
	 * below the root — so a second run cannot duplicate anything, and so it is never a way to
	 * "just recompile" (that is CompileAndSave). It also refuses on a graph with no ROOT node, which
	 * is not a repairable shape but a destructive one: there would be nothing to link the rebuilt
	 * options to, and the commit would empty the option list it was asked to recover.
	 *
	 * It cannot repair an option whose node exists in the graph but is unlinked from the root
	 * (ValidateQuery's "orphaned option"): SpawnMissingNodes skips any option that already has a
	 * node, so it is never relinked. That one is a hand fix in the Environment Query editor.
	 *
	 * Honest caveat, and the one thing worth knowing before reaching for this: on EQS the
	 * reconstruction is NOT exclusive to this call, and the sparse shape therefore self-heals.
	 * UEnvironmentQueryGraph::Initialize() IS LockUpdates + SpawnMissingNodes + CalculateAllWeights +
	 * UnlockUpdates (EnvironmentQueryGraph.cpp:168-176), the EQS asset editor calls Initialize() on
	 * every open (EnvironmentQueryEditor.cpp:224), and this service's own EnsureGraph calls it on
	 * every write. So opening a sparse query in the editor, or running any mutator or CompileAndSave
	 * against it, repairs it too.
	 *
	 * This is a genuine divergence from BehaviorTree, and NOT the one usually quoted: both editors
	 * call Initialize() unconditionally, outside their null-EdGraph branch (BehaviorTreeEditor.cpp
	 * :365, EnvironmentQueryEditor.cpp:224). The difference is the body —
	 * UBehaviorTreeGraph::Initialize() is Super::Initialize() + UpdateInjectedNodes()
	 * (BehaviorTreeGraph.cpp:202-206) and rebuilds nothing from the asset.
	 *
	 * What this call adds over CompileAndSave is therefore the refusals, the before/after node
	 * counts in the log, and a name for the operation — not a capability CompileAndSave lacks.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FString RepairGraphFromOptions(const FString& AssetPath);

	/**
	 * Run the query against the live Play In Editor world, from QuerierActorNameOrLabel's position,
	 * and return the scored items. This is the call that turns authoring into a feedback loop:
	 * everything else on this service describes what a query SAYS, this one says what it DOES.
	 *
	 * REQUIRES a play session, and that is the inverse of every other method here. The rest are
	 * refused while PIE runs, because committing a graph renames the live UEnvQueryOption /
	 * UEnvQueryTest instances a running query may be executing. This one writes nothing — it loads
	 * with LoadObject and never opens the editor graph — so the write guard does not apply to it,
	 * and it needs the very world the others refuse to be near.
	 *
	 * The session it requires is specifically an IN-PROCESS one, and that has two blind spots worth
	 * knowing before they cost a debugging cycle. A session launched as a separate process —
	 * "Standalone Game", "Launch", anything with EPlaySessionDestinationType::NewProcess or Launcher
	 * — leaves GEditor->PlayWorld null, so this refuses while a game is visibly running on screen;
	 * use Play In Editor (selected viewport or new editor window) instead. And under net-PIE with
	 * several worlds in the one process, GEditor->PlayWorld names only ONE of them: the query runs
	 * against that world, which is not necessarily the window being watched.
	 *
	 * The querier is matched against each actor's object name AND its editor label, name first (a
	 * name is unique in a world, a label is not). It becomes the request's Owner, which is what
	 * UEnvQueryContext_Querier resolves to — so it is not merely a position, it is the actor the
	 * query is asked ON BEHALF OF, and every "distance to querier" test scores relative to it.
	 *
	 * RunMode is one of "SingleResult", "RandomBest5Pct", "RandomBest25Pct", "AllMatching",
	 * matched case-insensitively. An unrecognised value is REFUSED naming the valid set rather than
	 * defaulting: the modes differ in how many items come back and in whether the pick is random, so
	 * a silent fallback would answer a different question than the one asked and look like a result.
	 * The mode also decides whether the results are sorted, truncated to the passing items, and
	 * normalized — see FEQSRunResult::Items, which is where that stops being obvious.
	 *
	 * Execution is synchronous — UEnvQueryManager::RunInstantQuery steps the instance to completion
	 * before returning (EnvQueryManager.cpp:305-316) — so there is no delegate, no tick and no handle.
	 * The cost is paid on the calling thread: a heavy generator run at AllMatching stalls the editor
	 * for as long as the query takes.
	 *
	 * The query is read from DISK STATE, not from the editor graph: whatever the last commit wrote
	 * into UEnvQuery::Options is what runs. A query edited through this service is already committed
	 * by the time any setter returns, but one edited by hand in an open Environment Query editor tab
	 * and not saved will run as its saved version.
	 */
	UFUNCTION(BlueprintCallable, meta = (AICallable), Category = "VibeUE|EQS")
	static FEQSRunResult RunQuery(const FString& AssetPath, const FString& QuerierActorNameOrLabel,
		const FString& RunMode = TEXT("AllMatching"));
};
