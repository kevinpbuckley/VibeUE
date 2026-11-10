// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

static const TCHAR* const kEQSTestDir = TEXT("/Game/Developers/VibeUEEQSTests");

#include "PythonAPI/UEnvQueryService.h"

static const EAutomationTestFlags kEQSTestFlags =
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

// (VibeUE.EQS.Asset.List lives below, after the fixture helpers it needs.)

#include "EnvQueryServiceInternal.h"

// RunQuery's EXECUTION path cannot be covered here, and that is stated rather than worked around:
// UEnvQueryManager::RunInstantQuery needs a live world and a querier actor in it, and the headless
// automation process has neither — GEditor->PlayWorld is null for the whole run, which the first
// assertion below establishes as a fact instead of assuming it.
//
// What IS testable headless is exactly the two refusals, and they are worth testing precisely
// because they are what a caller hits first:
//   - an unrecognised RunMode is refused naming the valid set, never defaulted;
//   - with no play session the call says so, rather than returning an empty result set that reads
//     like "the query matched nothing".
//
// The rest of the refusals (unknown asset, empty querier, querier not found, no AI system) sit
// BEHIND the play-session check by design, so headless every one of them returns the play-session
// message and none of them can be distinguished here. They are not asserted rather than asserted
// falsely. The real run — items, scores, actor names — was verified manually in a PIE session; see
// the RunQuery section of the PR description for what it measured.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeEQSRunRefusalTest,
	"VibeUE.EQS.Run.Refusals", kEQSTestFlags)
bool FVibeEQSRunRefusalTest::RunTest(const FString&)
{
#if !WITH_VIBEUE_EQS
	AddInfo(TEXT("skipped: EnvironmentQueryEditor plugin not available (WITH_VIBEUE_EQS=0)"));
	return true;
#else
	// The precondition every assertion below rests on. If a future harness ever runs this suite with
	// a play session up, this fails loudly instead of the tests quietly asserting nothing.
	TestTrue(TEXT("no play session in the headless suite"), !GEditor || !GEditor->PlayWorld);

	const FString AnyPath = FString(kEQSTestDir) / TEXT("EQS_RunRefusalNoSuchAsset");

	// 1) An unrecognised RunMode is refused, and refused FIRST — before the play-session check, which
	//    is the only reason this is reachable at all outside PIE.
	const FEQSRunResult BadMode = UEnvQueryService::RunQuery(AnyPath, TEXT("Querier"), TEXT("Best"));
	TestFalse(TEXT("bad run mode is not a success"), BadMode.bSuccess);
	TestTrue(TEXT("bad run mode explains itself"), !BadMode.Error.IsEmpty());
	TestTrue(TEXT("bad run mode names the offending value"), BadMode.Error.Contains(TEXT("\"Best\"")));
	for (const TCHAR* const Valid :
		{ TEXT("SingleResult"), TEXT("RandomBest5Pct"), TEXT("RandomBest25Pct"), TEXT("AllMatching") })
	{
		TestTrue(FString::Printf(TEXT("bad run mode lists %s"), Valid), BadMode.Error.Contains(Valid));
	}

	// A refusal never ran anything, so it carries no engine status and no items — the field that
	// tells a caller "the query did not execute" apart from "it executed and matched nothing".
	TestEqual(TEXT("bad run mode has no engine status"), BadMode.Status, FString());
	TestEqual(TEXT("bad run mode returns no items"), BadMode.ItemCount, 0);
	TestEqual(TEXT("bad run mode's item array agrees"), BadMode.Items.Num(), 0);

	// 2) A valid RunMode gets PAST the parser and is stopped by the play-session check instead. This
	//    is what proves the two refusals discriminate: same call, same asset, different message
	//    depending only on the mode string.
	const FEQSRunResult NoPie = UEnvQueryService::RunQuery(AnyPath, TEXT("Querier"), TEXT("AllMatching"));
	TestFalse(TEXT("no play session is not a success"), NoPie.bSuccess);
	TestTrue(TEXT("no play session explains itself"), !NoPie.Error.IsEmpty());
	TestTrue(TEXT("no play session says so"), NoPie.Error.Contains(TEXT("Play In Editor")));
	TestEqual(TEXT("no play session has no engine status"), NoPie.Status, FString());
	TestEqual(TEXT("no play session returns no items"), NoPie.ItemCount, 0);

	// The two messages are genuinely different, in both directions.
	TestFalse(TEXT("the mode refusal is not the play-session one"),
		BadMode.Error.Contains(TEXT("Play In Editor")));
	TestFalse(TEXT("the play-session refusal is not the mode one"),
		NoPie.Error.Contains(TEXT("Unknown RunMode")));

	// 3) All four documented modes are accepted by the parser — asserted through the same
	//    discrimination, since none of them can execute here. A mode the table forgot would come back
	//    with the RunMode refusal instead.
	for (const TCHAR* const Valid :
		{ TEXT("SingleResult"), TEXT("RandomBest5Pct"), TEXT("RandomBest25Pct"), TEXT("AllMatching") })
	{
		const FEQSRunResult Accepted = UEnvQueryService::RunQuery(AnyPath, TEXT("Querier"), Valid);
		TestTrue(FString::Printf(TEXT("%s is accepted by the parser"), Valid),
			Accepted.Error.Contains(TEXT("Play In Editor")));
	}

	// Case-insensitive, deliberately: the caller is typing a mode name, not addressing a node.
	TestTrue(TEXT("mode matching ignores case"),
		UEnvQueryService::RunQuery(AnyPath, TEXT("Querier"), TEXT("allmatching"))
			.Error.Contains(TEXT("Play In Editor")));

	// An empty RunMode is a typo, not a request for the default, so it is refused like any other
	// unrecognised value — the default lives in the signature and is only applied when the argument
	// is absent.
	TestTrue(TEXT("empty run mode is refused, not defaulted"),
		UEnvQueryService::RunQuery(AnyPath, TEXT("Querier"), FString())
			.Error.Contains(TEXT("Valid values are")));

	AddInfo(FString::Printf(TEXT("mode refusal: %s"), *BadMode.Error));
	AddInfo(FString::Printf(TEXT("play-session refusal: %s"), *NoPie.Error));
	AddInfo(TEXT("NOT covered here: the execution path (RunInstantQuery needs a live world and a "
				 "querier actor). Verified manually in a PIE session instead."));
	return true;
#endif
}


#endif // WITH_AUTOMATION_TESTS
