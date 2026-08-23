// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FProperty;
class UEdGraphPin;
class UEnvQuery;
class UEnvironmentQueryGraph;
class UEnvironmentQueryGraphNode;
class UEnvironmentQueryGraphNode_Option;
class UEnvironmentQueryGraphNode_Root;

// Real type is Editor/AIGraph's AIGraphTypes.h (global scope, not namespaced). Forward-declared
// here at global scope so the elaborated-type-specifier below resolves to it rather than
// injecting a distinct, incomplete VibeEQS::FGraphNodeClassHelper (BehaviorTree counterpart:
// VibeBT's identical forward-declare in BehaviorTreeServiceInternal.h).
struct FGraphNodeClassHelper;

/**
 * Shared internals for UEnvQueryService. Private to the module.
 *
 * Deliberately duplicated from BehaviorTreeServiceInternal rather than shared: extracting a common
 * AIGraph layer would refactor files sitting in an open upstream PR. Each guard below names its
 * BehaviorTree counterpart so a fix in one is greppable from the other.
 */
namespace VibeEQS
{
	/** Horizontal gap between option columns, matching the engine's own spawn spacing. */
	constexpr int32 OptionSpacingX = 300;

	/** Vertical offset of the option row below the root node. */
	constexpr int32 OptionRowY = 100;

	/**
	 * Positions for OptionCount options laid out in one row under RootPos.
	 *
	 * X is strictly increasing so UpdateAsset's FCompareNodeXLocation sort reproduces the
	 * intended order; equal X values would sort arbitrarily and scramble the query.
	 * Results are index-aligned with option order.
	 */
	TArray<FIntPoint> ComputeOptionLayout(int32 OptionCount, FIntPoint RootPos);

	/**
	 * Apply ComputeOptionLayout to a live graph, walking the root's output pin in current
	 * LinkedTo order and writing NodePosX / NodePosY back.
	 *
	 * EQS has no AutoArrange to fall back on (unlike BehaviorTree, whose AutoArrange exists but
	 * crashes without an open editor tab).
	 *
	 * BT counterpart: VibeBT::ArrangeGraph.
	 */
	void ArrangeGraph(UEnvironmentQueryGraph* Graph);

	/**
	 * The graph's root node, or nullptr. A linear sweep of Nodes is the whole search: an EQS graph
	 * holds its tests as SubNodes of the option nodes, never as entries in Nodes, so nothing is
	 * nested out of reach. Shared because the root is needed in three places across two
	 * translation units (layout, the commit-time discard guard, and GetQueryInfo).
	 */
	UEnvironmentQueryGraphNode_Root* FindRootNode(UEnvironmentQueryGraph* Graph);

	/**
	 * The root's output pin — the one and only pin option nodes hang off.
	 *
	 * "First output pin", not "Pins[0]", but the two are the same pin on a root node
	 * (UEnvironmentQueryGraphNode_Root::AllocateDefaultPins creates exactly one, an output). Written
	 * as a direction search anyway so a malformed root with something else at index 0 is skipped
	 * rather than silently treated as the option pin. UpdateAsset itself indexes Pins[0]
	 * (EnvironmentQueryGraph.cpp:60-62), so anything hanging off a *second* output pin would be
	 * invisible to the commit — which is precisely why nothing here ever creates one.
	 */
	UEdGraphPin* FindRootOutputPin(UEnvironmentQueryGraph* Graph);

	/**
	 * The option nodes the root feeds, in link order, de-duplicated by pointer.
	 *
	 * This is the option order — the same walk UpdateAsset makes, and after any commit through this
	 * service the same order it wrote into UEnvQuery::Options (CommitGraph runs ArrangeGraph, which
	 * turns this link order into strictly increasing X positions, and UpdateAsset then sorts the
	 * links by X). Index N here is Option[N] in the path grammar and entry N in GetQuery's JSON.
	 *
	 * Reports the GRAPH, not UEnvQuery::Options, and the two can differ mid-edit in one direction:
	 * a root-linked option node carrying no instance or no generator appears here and would NOT
	 * appear in Options, because UpdateAsset drops it (EnvironmentQueryGraph.cpp:75). Reporting it
	 * is deliberate — it is a node a caller can address, repair with SetOptionGenerator or remove,
	 * and hiding it would renumber every path after it relative to what the caller sees in the
	 * editor.
	 */
	TArray<UEnvironmentQueryGraphNode_Option*> GetOptionNodes(UEnvironmentQueryGraph* Graph);

	/** "Option[3]" — the path GetQuery reports and every edit method accepts. */
	FString MakeOptionPath(int32 OptionIndex);

	/**
	 * The node at Path, or nullptr.
	 *
	 * Grammar (index-based, no names): "Option[2]", or "Option[2]/@test[0]" for a test sub-node.
	 * Options are inherently ordered and frequently share a generator name, so an index is the only
	 * unambiguous address; @test[m] indexes the option node's SubNodes array, which is the order
	 * tests run in.
	 *
	 * nullptr is returned for every failure — malformed segment, out-of-range index, trailing
	 * junk — and it is never a success value: there is no node this function is allowed to find
	 * "nothing" at. Callers must turn it into an error naming the path rather than proceeding, which
	 * is why an out-of-range index is not silently clamped to the nearest node.
	 *
	 * BT counterpart: VibeBT::ResolveNodePath (a different path grammar over the same contract).
	 */
	UEnvironmentQueryGraphNode* ResolveNodePath(UEnvironmentQueryGraph* Graph, const FString& Path);

	/**
	 * Create the editor graph and its root node if the asset has none, then bring the graph up to
	 * date, mirroring what FEnvironmentQueryEditor does lazily on first open
	 * (EnvironmentQueryEditor.cpp:207-224). Empty on success, else the reason.
	 *
	 * Called only from write paths. Read paths must not: it mutates.
	 *
	 * Its own refusal is not side-effect-free. Bringing the graph up to date runs an UpdateAsset()
	 * internally, so an asset whose graph cannot represent every option has already lost them from
	 * memory by the time that is detectable. Such a query is marked, and every subsequent write to
	 * that same in-memory object is refused until it is genuinely reloaded — retrying would commit
	 * the loss. Nothing reaches disk either way.
	 *
	 * BT counterpart: VibeBT::EnsureGraph. Genuinely divergent bodies — UEnvironmentQueryGraph
	 * ::Initialize() rebuilds nodes from the asset and UBehaviorTreeGraph::Initialize() does not — so
	 * a fix in one is a question for the other rather than a copy.
	 */
	FString EnsureGraph(UEnvQuery* Query);

	/**
	 * Load AssetPath for writing, or refuse. Empty on success (OutQuery and OutGraph are then both
	 * non-null); otherwise the reason, with both outputs left null and the asset untouched.
	 *
	 * The play-session, open-editor and graph-lock refusals are all decided before EnsureGraph runs,
	 * so those leave the asset untouched in memory as well as on disk. EnsureGraph can refuse too,
	 * and that one is not side-effect-free — see its own comment. No refusal ever writes.
	 *
	 * BT counterpart: VibeBT::OpenWriteGuard.
	 */
	FString OpenWriteGuard(const FString& AssetPath, UEnvQuery*& OutQuery,
		UEnvironmentQueryGraph*& OutGraph);

	/**
	 * Lay the graph out, regenerate UEnvQuery::Options from it, recompute the displayed weights and
	 * save the package. Empty on success, else the reason.
	 *
	 * Every write in this service funnels through here, because UEnvironmentQueryGraph::UpdateAsset
	 * is destructive by design: it opens with GetOptionsMutable().Reset() and rebuilds the array
	 * from the graph alone (EnvironmentQueryGraph.cpp:59).
	 *
	 * BT counterpart: VibeBT::CommitGraph.
	 */
	FString CommitGraph(UEnvQuery* Query, UEnvironmentQueryGraph* Graph);

	/**
	 * Resolve an EQS class by short name, generated-class name ("_C" suffix) or full object path,
	 * and verify it derives from RequiredBase. Returns nullptr if unresolved, ambiguous, or of the
	 * wrong base.
	 */
	UClass* ResolveClass(const FString& ClassName, UClass* RequiredBase);

	/**
	 * The object whose PROPERTIES a node path addresses, or nullptr with OutError set.
	 *
	 * For a test sub-node that is the UEnvQueryTest instance. For an option it is the option's
	 * GENERATOR, not the UEnvQueryOption — deliberately, and for the same reason GetQuery reports the
	 * generator's properties under an option (see OptionToJson): an option IS a generator plus its
	 * tests, and UEnvQueryOption's own two UPROPERTYs (Generator, Tests) carry no CPF_Edit and are
	 * regenerated by every commit, so the option object exposes nothing a caller could tune. Pointing
	 * "Option[0]" at the generator is what makes GetPropertyNames and GetQuery describe the same set
	 * of properties for the same path.
	 *
	 * An option carrying no generator is an error rather than an empty property list: it is the one
	 * shape UpdateAsset drops, and SetOptionGenerator is what repairs it.
	 */
	UObject* ResolvePropertyTarget(UEnvironmentQueryGraph* Graph, const FString& NodePath,
		const FString& AssetPath, FString& OutError);

	/**
	 * A property a human could have edited in the details panel: CPF_Edit (EditAnywhere /
	 * EditDefaultsOnly / EditInstanceOnly) and none of EditConst / Transient / Deprecated. The single
	 * filter behind GetQuery's "properties", GetPropertyNames, and what the setters accept.
	 *
	 * It is also what keeps UEnvQueryTest::TestOrder unreachable, which is a requirement rather than
	 * an accident: TestOrder is a bare UPROPERTY() with no edit specifier, and
	 * UEnvironmentQueryGraph::UpdateAsset overwrites it unconditionally from the sub-node order on
	 * every commit (EnvironmentQueryGraph.cpp:95). A settable TestOrder would report success, revert
	 * at the next commit, and contradict MoveTest — the only call that can actually change it.
	 *
	 * BT counterpart: VibeBT::IsAuthorableProperty, which excludes the regenerated ExecutionIndex /
	 * MemoryOffset family for the same reason this one excludes TestOrder.
	 */
	bool IsAuthorableProperty(const FProperty* Property);

	/**
	 * True when Property is an FStructProperty whose struct derives from FAIDataProviderValue.
	 *
	 * Which is nearly every scoring knob on a UEnvQueryTest — ScoringFactor, FloatValueMin,
	 * FloatValueMax, ScoreClampMin, ScoreClampMax, ReferenceValue and BoolValue are all
	 * FAIDataProviderValue-derived (EnvQueryTest.h:88-139), as are a generator's GridSize /
	 * SpaceBetween and friends. Their literal form is "(DefaultValue=2.5)", never "2.5", and they may
	 * instead be BOUND to a UAIDataProvider field, in which case DefaultValue is dead data. That is
	 * the whole reason SetDataProviderValue exists as a separate call.
	 */
	bool IsDataProviderProperty(const FProperty* Property);

	/**
	 * Property's current value on Instance as text.
	 *
	 * Instance is passed as its OWN delta container, which makes FProperty::ExportText_Direct take
	 * its `Data == Delta` branch (Property.cpp:1024) and export every member of a struct rather than
	 * a delta. This is load-bearing, not defensive: with a null delta each member is compared against
	 * its ZERO value instead (TProperty_Numeric::Identical, UnrealType.h:1760-1775) and every member
	 * left at zero is omitted. Measured, not assumed — switching this one argument to nullptr turns
	 * an FAIDataProviderBoolValue holding false into the string "()" and a ScoringEquation holding
	 * Linear into the EMPTY string, both of which replay as whatever the CDO happens to hold.
	 * Passing the CDO as the delta loses the same members for the same reason.
	 *
	 * BT counterpart: VibeBT::ExportPropertyValue — note the SIGNATURE has drifted. BT returns void and
	 * writes through an FString& out-param; this returns the string. Same semantics, so a fix ports,
	 * but it will not apply as a patch and a mechanical copy will not compile.
	 */
	FString ExportPropertyValue(const FProperty* Property, const UObject* Instance);

	/**
	 * Import ValueText into Property inside Container (a UObject or a struct), rolling the value back
	 * BY VALUE if the import fails. True on success; false with OutError set, and the value then
	 * exactly as it was found.
	 *
	 * The pre-image is a Malloc + InitializeValue + CopyCompleteValue snapshot, not the property's
	 * exported text, because a struct import is applied MEMBER BY MEMBER and can fail halfway:
	 * "(DefaultValue=9.0" writes DefaultValue and then returns nullptr on the missing parenthesis
	 * (Class.cpp:3514-3518), so something has to put the earlier members back.
	 *
	 * A text pre-image is the tempting alternative and it is NOT equivalent, though not for the
	 * reason usually given. Taken through ExportPropertyValue it is a self-delta export, so it does
	 * NOT drop members sitting at their default — that failure belongs to a pre-image taken against
	 * the CDO. What it drops unconditionally is every member FProperty::ShouldPort refuses to export:
	 * CPF_Deprecated and CPF_Transient UPROPERTYs, which CopyCompleteValue copies and text cannot
	 * carry. FAIDataProviderTypedValue::PropertyType_DEPRECATED is exactly one of those.
	 *
	 * Raw C++ members are a weaker argument than they look and are deliberately not the rationale
	 * here: UScriptStruct::CopyScriptStruct only preserves them when the struct sets STRUCT_CopyNative
	 * and uses its C++ copy assignment (Class.cpp:3706-3716) — true for the FAIDataProviderValue
	 * family, since WithCopy defaults to !TIsPODType and these carry a vtable, but false for a POD
	 * struct, which falls to a property-by-property loop (:3723-3728) that cannot see them either.
	 *
	 * OwnerObject is the UObject the container belongs to, needed by import to outer any instanced
	 * sub-object it constructs.
	 */
	bool ImportPropertyValue(const FProperty* Property, void* Container, UObject* OwnerObject,
		const FString& ValueText, FString& OutError);

	/**
	 * Cached FGraphNodeClassHelper for one base class. GatherClasses()/GetClass() are expensive
	 * (the latter loads the class), so one helper per base class is built and reused for the
	 * module's lifetime rather than rebuilt on every discovery/resolution call.
	 *
	 * Also calls AddObservedBlueprintClasses(Base) + UpdateAvailableBlueprintClasses() — matching
	 * the engine's own usage (EnvironmentQueryEditorModule::CreateEnvironmentQueryEditor) — but per
	 * AIGraphTypes.cpp that pair only populates the static BlueprintClassCount map read by
	 * GetObservedBlueprintClassCount, a UI stat helper this codebase never calls. It does NOT gate
	 * whether GatherClasses() reports Blueprint-derived classes: BuildClassGraph() sweeps the asset
	 * registry for every UBlueprint unconditionally whenever bGatherBlueprints is true (its
	 * default, never toggled here). Kept because it is harmless and mirrors the engine's pattern,
	 * not because dropping it would hide any class. (BehaviorTree counterpart: VibeBT::GetClassHelper,
	 * which carries the same now-corrected comment as a recorded follow-up.)
	 */
	TSharedPtr<struct FGraphNodeClassHelper> GetClassHelper(UClass* BaseClass);

	/**
	 * Release the module-lifetime FGraphNodeClassHelper cache. Called from the module's
	 * ShutdownModule: ~FGraphNodeClassHelper unhooks FModuleManager and asset-registry delegates,
	 * which must happen while those systems still exist, not at static teardown.
	 * (BehaviorTree counterpart: VibeBT::ShutdownClassHelperCache.)
	 */
	void ShutdownClassHelperCache();

	/**
	 * Guards the one input the engine turns fatal rather than erroneous: an over-long name. FName
	 * construction past NAME_SIZE is a Fatal log, not a return value, so every entry point that
	 * turns caller input into an FName -- or into an object-path lookup, which builds an FName per
	 * segment -- checks this first. Returns an error naming What, or empty when Value is safe.
	 * (BehaviorTree counterpart: VibeBT::CheckNameLength.)
	 */
	FString CheckNameLength(const FString& Value, const TCHAR* What);

	/**
	 * Whether AssetPath is a package path these services may WRITE. Rejects an over-long or
	 * malformed path, and the roots that are not the project's to edit.
	 * (BehaviorTree counterpart: VibeBT::CheckWritableAssetPath.)
	 */
	FString CheckWritableAssetPath(const FString& AssetPath);

	/**
	 * Put the in-memory package back the way disk has it, and describe what happened as a clause to
	 * append to a refusal. A write that did not reach disk must not linger: a dirty package left
	 * behind is shipped, silently, by the next successful save of the same asset -- a human's Save
	 * All included -- and a stale copy turns a caller's natural retry into a write of exactly the
	 * state a guard refused.
	 *
	 * Used ONLY where the asset itself is still healthy and merely unsaved. The dropped-options
	 * refusal in EnsureGraph deliberately does NOT reload -- see GEQSPoisonedQueries, where the
	 * damaged objects are already RF_Transient and a reload would dangle raw pointers held
	 * elsewhere; that path marks instead.
	 * (BehaviorTree counterpart: VibeBT::DiscardDirtyStateFromDisk.)
	 */
	FString DiscardDirtyStateFromDisk(class UPackage* Package);
}
