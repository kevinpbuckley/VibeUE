// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Tools/PythonExecutionService.h"

// Regression guard for the execute_python_code safety-filter false positives (item 12): the
// EditorDialog / show_modal / `while True:` guards used to run against the RAW source, so a comment
// or docstring that merely mentioned a pattern — or a `while True:` that exits via return/raise/
// sys.exit() — was wrongly refused. They now run against comment- and string-literal-stripped code,
// and return/raise/sys.exit() count as loop exits. This test drives the pure guard
// (FPythonExecutionService::ContainsUnsafePattern) so no Python interpreter is required.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVibePythonSafetyFilterTest, "VibeUE.Python.SafetyFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVibePythonSafetyFilterTest::RunTest(const FString&)
{
	using VibeUE::FPythonExecutionService;

	auto IsUnsafe = [](const FString& Code)
	{
		FString Pattern;
		FString Reason;
		return FPythonExecutionService::ContainsUnsafePattern(Code, Pattern, Reason);
	};

	// A docstring / comment that merely NAMES the patterns must pass — it is not calling them.
	TestFalse(TEXT("docstring mentioning EditorDialog and show_modal passes"),
		IsUnsafe(TEXT("\"\"\"This helper avoids EditorDialog and never calls show_modal().\"\"\"\nx = 1\n")));
	TestFalse(TEXT("comment mentioning show_modal passes"),
		IsUnsafe(TEXT("# do not use show_modal here\nresult = 2\n")));
	TestFalse(TEXT("string literal containing while True: passes"),
		IsUnsafe(TEXT("msg = \"never write while True: without a break\"\nprint(msg)\n")));

	// A `while True:` that leaves via return / raise / sys.exit() is not infinite.
	TestFalse(TEXT("while True: with return passes"),
		IsUnsafe(TEXT("def f():\n    while True:\n        if done():\n            return\n")));
	TestFalse(TEXT("while True: with raise passes"),
		IsUnsafe(TEXT("while True:\n    if bad():\n        raise RuntimeError('stop')\n")));
	TestFalse(TEXT("while True: with sys.exit() passes"),
		IsUnsafe(TEXT("import sys\nwhile True:\n    if done():\n        sys.exit(0)\n")));
	TestFalse(TEXT("while True: with break still passes"),
		IsUnsafe(TEXT("while True:\n    if done():\n        break\n")));

	// Real dangerous calls must STILL be refused.
	TestTrue(TEXT("real show_modal( call is refused"),
		IsUnsafe(TEXT("dlg = unreal.SomeThing()\ndlg.show_modal()\n")));
	TestTrue(TEXT("real EditorDialog usage is refused"),
		IsUnsafe(TEXT("unreal.EditorDialog.show_message('t', 'm', unreal.AppMsgType.OK)\n")));
	TestTrue(TEXT("real bare while True: with no exit is refused"),
		IsUnsafe(TEXT("count = 0\nwhile True:\n    count += 1\n")));

	return true;
}

#endif // WITH_AUTOMATION_TESTS
