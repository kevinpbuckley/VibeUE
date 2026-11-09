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
