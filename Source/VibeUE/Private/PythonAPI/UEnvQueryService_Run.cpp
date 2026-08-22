// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "PythonAPI/UEnvQueryService.h"

#if WITH_VIBEUE_EQS

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "GameFramework/Actor.h"

namespace
{
	/**
	 * Uniquely named rather than left to collide: these are file-local helpers in an anonymous
	 * namespace, and a unity/jumbo build concatenates this translation unit with its siblings, where
	 * a second "ParseRunMode" would be an ODR clash rather than a shadow.
	 */

	/** Every accepted RunMode string, in the order the refusal message lists them. */
	const TCHAR* const GVibeEQSRunModeNames[] =
	{
		TEXT("SingleResult"),
		TEXT("RandomBest5Pct"),
		TEXT("RandomBest25Pct"),
		TEXT("AllMatching"),
	};

	const EEnvQueryRunMode::Type GVibeEQSRunModeValues[] =
	{
		EEnvQueryRunMode::SingleResult,
		EEnvQueryRunMode::RandomBest5Pct,
		EEnvQueryRunMode::RandomBest25Pct,
		EEnvQueryRunMode::AllMatching,
	};

	static_assert(UE_ARRAY_COUNT(GVibeEQSRunModeNames) == UE_ARRAY_COUNT(GVibeEQSRunModeValues),
		"RunMode name and value tables must stay index-aligned");

	/**
	 * Map a RunMode string onto the enum. False when unrecognised — never a default.
	 *
	 * Matching is case-insensitive because the caller is typing a mode name, not addressing a node;
	 * "allmatching" is unambiguous and refusing it would be pedantry. Anything that is not one of the
	 * four IS refused, which is the point: the modes return different numbers of items and two of
	 * them pick randomly, so falling back to AllMatching on a typo would answer a different question
	 * and return a plausible-looking result for it.
	 */
	bool VibeEQSParseRunMode(const FString& Text, EEnvQueryRunMode::Type& OutMode)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(GVibeEQSRunModeNames); ++Index)
		{
			if (Text.Equals(GVibeEQSRunModeNames[Index], ESearchCase::IgnoreCase))
			{
				OutMode = GVibeEQSRunModeValues[Index];
				return true;
			}
		}
		return false;
	}

	/** The valid RunMode set, quoted, for a refusal message. */
	FString VibeEQSRunModeList()
	{
		TArray<FString> Quoted;
		for (const TCHAR* const Name : GVibeEQSRunModeNames)
		{
			Quoted.Add(FString::Printf(TEXT("\"%s\""), Name));
		}
		return FString::Join(Quoted, TEXT(", "));
	}

	/**
	 * The engine's own name for a query status.
	 *
	 * Written out by hand as a CHOICE, not out of necessity, and the distinction matters enough to
	 * record: EEnvQueryStatus IS reflected — EnvQueryTypes.h:168 is `UENUM(BlueprintType)` above the
	 * namespace — so StaticEnum<EEnvQueryStatus::Type>()->GetNameStringByValue() works and returns
	 * these same six short names (UEnum::GetNameStringByIndex strips the namespace from a
	 * non-Regular CppForm, Enum.cpp:779-785). The engine does exactly that for the sibling run-mode
	 * enum at EnvQueryManager.cpp:806.
	 *
	 * Two reasons to keep the table anyway:
	 *   - these strings are this service's PUBLIC CONTRACT. A caller compares Status against
	 *     "Failed"; sourcing them from reflection would let an engine-side enum rename silently
	 *     change what this API returns, which is a break we would not see until someone's script
	 *     stopped matching.
	 *   - GetNameStringByValue returns the EMPTY string for a value not in the enum
	 *     (GetIndexByValue -> INDEX_NONE -> GetNameStringByIndex's `return FString()`,
	 *     Enum.cpp:787-793), and empty is the value FEQSRunResult::Status uses to mean "the query
	 *     never ran". The switch's explicit "Unknown" cannot collide with that.
	 *
	 * The six names below were diffed against EnvQueryTypes.h:171-179 and match it exactly.
	 */
	const TCHAR* VibeEQSStatusName(EEnvQueryStatus::Type Status)
	{
		switch (Status)
		{
		case EEnvQueryStatus::Processing:   return TEXT("Processing");
		case EEnvQueryStatus::Success:      return TEXT("Success");
		case EEnvQueryStatus::Failed:       return TEXT("Failed");
		case EEnvQueryStatus::Aborted:      return TEXT("Aborted");
		case EEnvQueryStatus::OwnerLost:    return TEXT("OwnerLost");
		case EEnvQueryStatus::MissingParam: return TEXT("MissingParam");
		default:                            return TEXT("Unknown");
		}
	}

	/**
	 * The actor in World whose object name or editor label is NameOrLabel, or nullptr.
	 *
	 * Name is preferred over label because only one of the two is unique: a UObject name is unique
	 * within its outer, an actor label is a display string several actors may share. Matching label
	 * first would let a duplicate label shadow the exact actor the caller named.
	 *
	 * On a miss, OutSearched receives what WAS there — capped, because a play world holds hundreds
	 * of actors and a refusal that dumps all of them is unreadable.
	 */
	AActor* VibeEQSFindQuerier(UWorld* World, const FString& NameOrLabel, FString& OutSearched)
	{
		AActor* LabelMatch = nullptr;
		TArray<FString> Seen;
		int32 TotalActors = 0;

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!Actor)
			{
				continue;
			}
			++TotalActors;

			const FString Name = Actor->GetName();
			if (Name.Equals(NameOrLabel, ESearchCase::IgnoreCase))
			{
				return Actor;
			}

#if WITH_EDITOR
			const FString Label = Actor->GetActorLabel();
#else
			const FString Label = Name;
#endif
			if (!LabelMatch && Label.Equals(NameOrLabel, ESearchCase::IgnoreCase))
			{
				LabelMatch = Actor;
			}

			// Collected for the refusal only. The cap is on what is REPORTED, not on what is
			// searched: the loop always visits every actor, so a match at index 500 is still found.
			constexpr int32 MaxReported = 20;
			if (Seen.Num() < MaxReported)
			{
				Seen.Add(Label.Equals(Name) ? Name : FString::Printf(TEXT("%s (%s)"), *Label, *Name));
			}
		}

		if (LabelMatch)
		{
			return LabelMatch;
		}

		OutSearched = FString::Printf(TEXT("%d actor(s): %s%s"),
			TotalActors,
			*FString::Join(Seen, TEXT(", ")),
			TotalActors > Seen.Num() ? TEXT(", ...") : TEXT(""));
		return nullptr;
	}

	/** A refusal: no Status, no items, and the reason. */
	FEQSRunResult VibeEQSRunRefusal(FString Reason)
	{
		FEQSRunResult Result;
		Result.Error = MoveTemp(Reason);
		return Result;
	}
}

FEQSRunResult UEnvQueryService::RunQuery(const FString& AssetPath,
	const FString& QuerierActorNameOrLabel, const FString& RunMode)
{
	// Argument validation first, and specifically BEFORE the play-session check. Not an arbitrary
	// order: a malformed RunMode is wrong about the call itself and is worth reporting whether or not
	// a world happens to be running, and checking it second would make the mode refusal unreachable
	// outside PIE — including from the headless suite, which is the only place it can be tested.
	EEnvQueryRunMode::Type Mode = EEnvQueryRunMode::AllMatching;
	if (!VibeEQSParseRunMode(RunMode, Mode))
	{
		return VibeEQSRunRefusal(FString::Printf(
			TEXT("Unknown RunMode \"%s\". Valid values are %s. Refused rather than defaulted: the "
				 "modes differ in how many items are returned and in whether the pick is random, so "
				 "a fallback would answer a different question and look like a result."),
			*RunMode, *VibeEQSRunModeList()));
	}

	// RunQuery is the one method here that REQUIRES a play session rather than refusing during one.
	// It reads world state and writes nothing, so the commit-time guard that protects the others does
	// not apply — but there is no world to query without PIE, and an empty result set is the wrong
	// answer to "the editor is not playing".
	if (!GEditor || !GEditor->PlayWorld)
	{
		return VibeEQSRunRefusal(FString::Printf(
			TEXT("No Play In Editor session is running; start one and retry running %s. EQS executes "
				 "against a live world — its generators trace, overlap and iterate actors — so there "
				 "is nothing to query from the editor world. Note that a game running in a SEPARATE "
				 "PROCESS (\"Standalone Game\", \"Launch\") is not a PIE session and is invisible to "
				 "this check, so this is also what you get while staring at a running game: use Play "
				 "In Editor (selected viewport, or new editor window) instead."),
			*AssetPath));
	}

	UWorld* PlayWorld = GEditor->PlayWorld;

	// Read-only load, deliberately not OpenWriteGuard: that path calls EnsureGraph, which creates the
	// editor graph, spawns nodes and regenerates the option list. Running a query must not modify the
	// asset it runs — and EnsureGraph is refused during PIE anyway, which is exactly when this runs.
	UEnvQuery* Query = LoadObject<UEnvQuery>(nullptr, *AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Query)
	{
		return VibeEQSRunRefusal(
			FString::Printf(TEXT("Environment Query not found: %s"), *AssetPath));
	}

	if (QuerierActorNameOrLabel.IsEmpty())
	{
		return VibeEQSRunRefusal(
			TEXT("QuerierActorNameOrLabel is empty. The querier is the actor the query runs on behalf "
				 "of — UEnvQueryContext_Querier resolves to it and every distance-to-querier test "
				 "scores against it — so there is no sensible default."));
	}

	FString Searched;
	AActor* Querier = VibeEQSFindQuerier(PlayWorld, QuerierActorNameOrLabel, Searched);
	if (!Querier)
	{
		return VibeEQSRunRefusal(FString::Printf(
			TEXT("No actor named or labelled \"%s\" in the play world. Searched object names and "
				 "editor labels of %s"),
			*QuerierActorNameOrLabel, *Searched));
	}

	// GetCurrent returns the UAISystem's manager, and null when the world has no AI system
	// (EnvQueryManager.cpp:223-227) — which is a real state for a world whose WorldSettings disables
	// it, not a defensive check.
	UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(PlayWorld);
	if (!Manager)
	{
		return VibeEQSRunRefusal(FString::Printf(
			TEXT("The play world has no Environment Query manager: UAISystem is absent, so nothing "
				 "can execute %s. A world whose AISystemClass is unset, or whose WorldSettings turn "
				 "the AI system off, has no EQS at all."),
			*AssetPath));
	}

	FEnvQueryRequest Request(Query, Querier);

	// Documented as "set world (for accessing query manager) when owner can't provide it", and only
	// FEnvQueryRequest::Execute reads it (EnvQueryManager.cpp:153) — the manager is already in hand
	// here, so this changes nothing on this path. Set anyway so the request is not carrying a null
	// world if it is ever handed to Execute.
	Request.SetWorldOverride(PlayWorld);

	// Synchronous: RunInstantQuery steps the instance to completion before returning
	// (EnvQueryManager.cpp:305-316). Null means PrepareQueryInstance could not build an instance, and
	// with a non-null template there is exactly one cause — no options (EnvQueryManager.cpp:782).
	const TSharedPtr<FEnvQueryResult> Result = Manager->RunInstantQuery(Request, Mode);
	if (!Result.IsValid())
	{
		return VibeEQSRunRefusal(FString::Printf(
			TEXT("%s produced no query instance. UEnvQueryManager::CreateQueryInstance refuses a "
				 "template with no options (EnvQueryManager.cpp:782); check GetQueryInfo's "
				 "OptionCount, and ValidateQuery if it is zero on a query that should have some."),
			*AssetPath));
	}

	FEQSRunResult Out;
	Out.Status = VibeEQSStatusName(Result->GetRawStatus());
	Out.bSuccess = Result->IsSuccessful();

	Out.Items.Reserve(Result->Items.Num());
	for (int32 Index = 0; Index < Result->Items.Num(); ++Index)
	{
		FEQSResultItem Item;
		Item.Score = Result->GetItemScore(Index);
		Item.bPassed = Result->Items[Index].IsValid();

		// Both accessors self-check the item type and return a zero/null on a mismatch
		// (EnvQueryTypes.cpp:29-55), so an actor query fills both — UEnvQueryItemType_ActorBase
		// derives from UEnvQueryItemType_VectorBase — and a point query fills only the location.
		Item.Location = Result->GetItemAsLocation(Index);
		if (const AActor* ItemActor = Result->GetItemAsActor(Index))
		{
			Item.ActorName = ItemActor->GetName();
		}

		Out.Items.Add(MoveTemp(Item));
	}
	Out.ItemCount = Out.Items.Num();

	if (!Out.bSuccess)
	{
		Out.Error = FString::Printf(
			TEXT("%s ran but finished with status %s and %d item(s). \"Failed\" is the engine's word "
				 "for a run that produced no valid items (EnvQueryInstance.cpp:877), not for a "
				 "broken query: every generated item was filtered out by a test."),
			*AssetPath, *Out.Status, Out.ItemCount);
	}

	return Out;
}

#else  // WITH_VIBEUE_EQS

FEQSRunResult UEnvQueryService::RunQuery(const FString&, const FString&, const FString&)
{
	FEQSRunResult Result;
	Result.Error = TEXT("EQS authoring is unavailable: the EnvironmentQueryEditor plugin is not "
						"enabled in this build (WITH_VIBEUE_EQS=0).");
	return Result;
}

#endif // WITH_VIBEUE_EQS
