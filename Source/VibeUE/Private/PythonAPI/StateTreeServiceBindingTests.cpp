// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "PythonAPI/UStateTreeService.h"
#include "AIServiceTestFixture.h"

/**
 * Binding READER coverage for UStateTreeService (get_bindings and the bIsBound / BoundTo fields of
 * get_task_property_names). Each test builds an in-memory StateTree through the service itself:
 *   Root
 *     Work  (FStateTreeDelayTask, Duration bound to root parameter WaitTime)
 *     Other (no tasks)
 * CreateStateTree never saves, and FScopedFixtureReset purges the package (and any stray .uasset)
 * on entry and exit, so nothing outlives the test.
 */
namespace VibeStateTreeBindingTest
{
	static const EAutomationTestFlags kFlags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

	static const TCHAR* kTestDir = TEXT("/Game/Developers/VibeUEStateTreeBindingTests");
	static const TCHAR* kTaskStruct = TEXT("FStateTreeDelayTask");
	static const TCHAR* kWorkState = TEXT("Root/Work");

	/** Build the fixture tree described above. Returns false (with test errors) when any step fails. */
	static bool BuildFixture(FAutomationTestBase& Test, const FString& AssetPath)
	{
		const TArray<FString> Schemas = UStateTreeService::ListStateTreeSchemas();
		if (!Test.TestTrue(TEXT("at least one StateTree schema class is available"), Schemas.Num() > 0))
		{
			return false;
		}
		const FString Schema = Schemas.Contains(TEXT("StateTreeComponentSchema"))
			? FString(TEXT("StateTreeComponentSchema"))
			: Schemas[0];

		if (!Test.TestTrue(TEXT("fixture StateTree created"), UStateTreeService::CreateStateTree(AssetPath, Schema))
			|| !Test.TestTrue(TEXT("Root subtree added"), UStateTreeService::AddState(AssetPath, TEXT(""), TEXT("Root")))
			|| !Test.TestTrue(TEXT("Root/Work added"), UStateTreeService::AddState(AssetPath, TEXT("Root"), TEXT("Work")))
			|| !Test.TestTrue(TEXT("Root/Other added"), UStateTreeService::AddState(AssetPath, TEXT("Root"), TEXT("Other")))
			|| !Test.TestTrue(TEXT("root parameter WaitTime added"),
				UStateTreeService::AddOrUpdateRootParameter(AssetPath, TEXT("WaitTime"), TEXT("Float"), TEXT("2.5")))
			|| !Test.TestTrue(TEXT("Delay task added to Root/Work"), UStateTreeService::AddTask(AssetPath, kWorkState, kTaskStruct)))
		{
			return false;
		}

		return Test.TestTrue(TEXT("Duration bound to root parameter WaitTime"),
			UStateTreeService::BindTaskPropertyToRootParameter(AssetPath, kWorkState, kTaskStruct,
				TEXT("Duration"), TEXT("WaitTime")));
	}

	static const FStateTreePropertyInfo* FindProperty(const TArray<FStateTreePropertyInfo>& Properties, const TCHAR* Name)
	{
		return Properties.FindByPredicate([Name](const FStateTreePropertyInfo& Info) { return Info.Name == Name; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeStateTreeBindingsRootParameterTest,
	"VibeUE.StateTreeService.Bindings.RootParameterTaskBinding", VibeStateTreeBindingTest::kFlags)
bool FVibeStateTreeBindingsRootParameterTest::RunTest(const FString&)
{
	using namespace VibeStateTreeBindingTest;

	const FString AssetPath = FString(kTestDir) / TEXT("ST_BindingReaderFixture");
	VibeAITest::FScopedFixtureReset Reset(AssetPath);
	if (!BuildFixture(*this, AssetPath))
	{
		return false;
	}

	// Whole asset: exactly the one binding the fixture made, fully described.
	const TArray<FStateTreeBindingInfo> All = UStateTreeService::GetBindings(AssetPath);
	if (!TestEqual(TEXT("whole-asset read returns exactly one binding"), All.Num(), 1))
	{
		return false;
	}
	const FStateTreeBindingInfo& Binding = All[0];
	AddInfo(FString::Printf(TEXT("Binding raw: %s"), *Binding.Raw));
	TestEqual(TEXT("TargetKind"), Binding.TargetKind, FString(TEXT("Task")));
	TestEqual(TEXT("TargetStatePath"), Binding.TargetStatePath, FString(kWorkState));
	TestEqual(TEXT("TargetNodeStruct"), Binding.TargetNodeStruct, FString(TEXT("StateTreeDelayTask")));
	TestFalse(TEXT("TargetNodeName is filled"), Binding.TargetNodeName.IsEmpty());
	TestEqual(TEXT("TargetNodeIndex is the task's array index"), Binding.TargetNodeIndex, 0);
	TestEqual(TEXT("TargetTransitionIndex is unset for a task"), Binding.TargetTransitionIndex, (int32)INDEX_NONE);
	TestEqual(TEXT("TargetPropertyPath"), Binding.TargetPropertyPath, FString(TEXT("Duration")));
	TestEqual(TEXT("SourceKind"), Binding.SourceKind, FString(TEXT("RootParameter")));
	TestTrue(TEXT("SourceStatePath is empty for a root parameter"), Binding.SourceStatePath.IsEmpty());
	TestEqual(TEXT("SourceNodeName"), Binding.SourceNodeName, FString(TEXT("Parameters")));
	TestEqual(TEXT("SourcePropertyPath"), Binding.SourcePropertyPath, FString(TEXT("WaitTime")));
	TestTrue(TEXT("bSourceResolves"), Binding.bSourceResolves);
	// Raw = "<SourceStructID>:<SourcePropertyPath> -> <TargetStructID>:<TargetPropertyPath>" with raw property
	// names. Regression: it once used FPropertyBindingPath::ToString(), which emits the display name "Wait Time".
	TestTrue(TEXT("Raw carries the raw source path, not the display name"), Binding.Raw.Contains(TEXT(":WaitTime -> ")));
	TestFalse(TEXT("Raw has no display-name spelling"), Binding.Raw.Contains(TEXT("Wait Time")));
	TestTrue(TEXT("Raw ends with the raw target path"), Binding.Raw.EndsWith(TEXT(":Duration")));

	// State filter: the owning state sees it, a sibling state does not.
	const TArray<FStateTreeBindingInfo> InWork = UStateTreeService::GetBindings(AssetPath, kWorkState);
	if (TestEqual(TEXT("filtered to Root/Work returns the binding"), InWork.Num(), 1))
	{
		TestEqual(TEXT("filtered binding is the same target path"), InWork[0].TargetPropertyPath, FString(TEXT("Duration")));
	}
	TestEqual(TEXT("filtered to Root/Other returns nothing"),
		UStateTreeService::GetBindings(AssetPath, TEXT("Root/Other")).Num(), 0);
	TestEqual(TEXT("filtered to Root (the parent) returns nothing: the target lives in Root/Work"),
		UStateTreeService::GetBindings(AssetPath, TEXT("Root")).Num(), 0);
	TestFalse(TEXT("a service-made binding is not an output binding"), Binding.bIsOutputBinding);

	// Unbind through the existing writer, then read back.
	TestTrue(TEXT("UnbindTaskProperty succeeds"),
		UStateTreeService::UnbindTaskProperty(AssetPath, kWorkState, kTaskStruct, TEXT("Duration")));
	TestEqual(TEXT("no bindings remain after unbind"), UStateTreeService::GetBindings(AssetPath).Num(), 0);
	TestEqual(TEXT("no bindings remain in Root/Work after unbind"),
		UStateTreeService::GetBindings(AssetPath, kWorkState).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeStateTreeBindingsUnknownStateTest,
	"VibeUE.StateTreeService.Bindings.UnknownStatePathWarnsAndReturnsEmpty", VibeStateTreeBindingTest::kFlags)
bool FVibeStateTreeBindingsUnknownStateTest::RunTest(const FString&)
{
	using namespace VibeStateTreeBindingTest;

	const FString AssetPath = FString(kTestDir) / TEXT("ST_BindingUnknownStateFixture");
	VibeAITest::FScopedFixtureReset Reset(AssetPath);
	if (!BuildFixture(*this, AssetPath))
	{
		return false;
	}

	// The asset HAS a binding, so an empty result can only come from the unknown-state early-out.
	TestEqual(TEXT("the fixture has one binding"), UStateTreeService::GetBindings(AssetPath).Num(), 1);
	AddExpectedMessagePlain(TEXT("GetBindings: State not found: Root/Missing"), ELogVerbosity::Warning,
		EAutomationExpectedMessageFlags::Contains, 1);
	TestEqual(TEXT("an unknown state path returns no bindings"),
		UStateTreeService::GetBindings(AssetPath, TEXT("Root/Missing")).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibeStateTreeBindingsTaskPropertyNamesTest,
	"VibeUE.StateTreeService.Bindings.TaskPropertyNamesReportBound", VibeStateTreeBindingTest::kFlags)
bool FVibeStateTreeBindingsTaskPropertyNamesTest::RunTest(const FString&)
{
	using namespace VibeStateTreeBindingTest;

	const FString AssetPath = FString(kTestDir) / TEXT("ST_BindingPropertyNamesFixture");
	VibeAITest::FScopedFixtureReset Reset(AssetPath);
	if (!BuildFixture(*this, AssetPath))
	{
		return false;
	}

	const TArray<FStateTreePropertyInfo> Bound = UStateTreeService::GetTaskPropertyNames(AssetPath, kWorkState, kTaskStruct);
	const FStateTreePropertyInfo* Duration = FindProperty(Bound, TEXT("Duration"));
	const FStateTreePropertyInfo* Deviation = FindProperty(Bound, TEXT("RandomDeviation"));
	if (!TestNotNull(TEXT("Duration is listed"), Duration) || !TestNotNull(TEXT("RandomDeviation is listed"), Deviation))
	{
		return false;
	}
	TestTrue(TEXT("Duration reports bIsBound"), Duration->bIsBound);
	TestEqual(TEXT("Duration BoundTo names the root parameter"), Duration->BoundTo,
		FString(TEXT("RootParameter:Parameters.WaitTime")));
	TestFalse(TEXT("RandomDeviation is not bound"), Deviation->bIsBound);
	TestTrue(TEXT("RandomDeviation BoundTo is empty"), Deviation->BoundTo.IsEmpty());

	// The existing fields keep their meaning.
	TestEqual(TEXT("Duration type is unchanged"), Duration->Type, FString(TEXT("float")));
	TestFalse(TEXT("Duration CurrentValue is still exported"), Duration->CurrentValue.IsEmpty());

	TestTrue(TEXT("UnbindTaskProperty succeeds"),
		UStateTreeService::UnbindTaskProperty(AssetPath, kWorkState, kTaskStruct, TEXT("Duration")));
	const TArray<FStateTreePropertyInfo> Unbound = UStateTreeService::GetTaskPropertyNames(AssetPath, kWorkState, kTaskStruct);
	const FStateTreePropertyInfo* DurationAfter = FindProperty(Unbound, TEXT("Duration"));
	if (TestNotNull(TEXT("Duration is still listed after unbind"), DurationAfter))
	{
		TestFalse(TEXT("Duration no longer reports bIsBound"), DurationAfter->bIsBound);
		TestTrue(TEXT("Duration BoundTo is empty after unbind"), DurationAfter->BoundTo.IsEmpty());
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
