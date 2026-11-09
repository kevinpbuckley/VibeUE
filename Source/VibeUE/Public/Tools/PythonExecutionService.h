// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/ServiceBase.h"
#include "Tools/PythonTypes.h"
#include "Core/Result.h"
#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"

namespace VibeUE
{

/**
 * Service for executing Python code in Unreal Engine
 *
 * Wraps IPythonScriptPlugin for safe execution with output capture.
 * Provides in-memory code execution without disk-based scripts.
 */
class VIBEUE_API FPythonExecutionService : public FServiceBase
{
public:
	/**
	 * Constructor
	 */
	explicit FPythonExecutionService(TSharedPtr<FServiceContext> Context);

	/**
	 * Get service name
	 */
	virtual FString GetServiceName() const override { return TEXT("PythonExecutionService"); }

	/**
	 * Execute Python code as a multi-line script
	 *
	 * @param Code Python code to execute
	 * @param ExecutionScope Private (isolated) or Public (shared console state)
	 * @param TimeoutMs Maximum execution time in milliseconds (0 = no timeout)
	 * @param bFreshNamespace Run in a new namespace instead of the globals every call shares
	 * @return Execution result with output, errors, and timing
	 */
	TResult<FPythonExecutionResult> ExecuteCode(
		const FString& Code,
		EPythonFileExecutionScope ExecutionScope = EPythonFileExecutionScope::Private,
		int32 TimeoutMs = 30000,
		bool bFreshNamespace = false
	);

	/**
	 * Run one Python command under the crash guard. After a caught crash, probes whether the interpreter
	 * survived and says so in CrashMessage.
	 *
	 * @param Command The command to run; holds its output and result afterwards
	 * @param ExecutionTimeMs How long the command took, not counting the probe
	 * @param bCrashed Set when the guard caught a crash
	 * @param CrashMessage What crashed, when bCrashed is set
	 * @return What the Python plugin returned for the command
	 */
	bool RunGuardedCommand(FPythonCommandEx& Command, double& ExecutionTimeMs, bool& bCrashed, FString& CrashMessage);

	/**
	 * Turn what a finished script did into its result, and persist that. Persisted on the error paths too,
	 * because the code has already run in the editor even when the caller has since timed out.
	 *
	 * @param Command The script's output and result: one command's, or several steps' put together
	 * @param bSuccess What the Python plugin returned
	 * @param bCrashed Whether the crash guard caught a crash
	 * @param CrashMessage What crashed
	 * @param ExecutionTimeMs How long the script took
	 * @param RunId The run's id, from FVibeUEPythonResultLog::NextRunId
	 * @param StartedUtc When the script started
	 * @param TimeoutMs The client timeout to flag an overrun against (0 = none)
	 * @param bCancelled Whether the script ended because it was cancelled
	 */
	TResult<FPythonExecutionResult> RecordRunOutcome(
		const FPythonCommandEx& Command,
		bool bSuccess,
		bool bCrashed,
		const FString& CrashMessage,
		double ExecutionTimeMs,
		int64 RunId,
		const FDateTime& StartedUtc,
		int32 TimeoutMs,
		bool bCancelled
	);

	/**
	 * Evaluate a Python expression and return the result
	 *
	 * @param Expression Single Python expression (e.g., "2 + 2", "unreal.load_asset('/Game/Test')")
	 * @return Execution result with expression value
	 */
	TResult<FPythonExecutionResult> EvaluateExpression(const FString& Expression);

	/**
	 * Execute code with validation checks
	 *
	 * Optionally validates for dangerous operations before execution.
	 *
	 * @param Code Python code to execute
	 * @param bValidateBeforeExecution If true, validates code for dangerous patterns
	 * @return Execution result
	 */
	TResult<FPythonExecutionResult> ExecuteCodeSafe(
		const FString& Code,
		bool bValidateBeforeExecution = false
	);

	/**
	 * Check if Python is available and initialized
	 *
	 * @return True if Python is ready to use
	 */
	TResult<bool> IsPythonAvailable();

	/**
	 * Get Python interpreter version and path
	 *
	 * @return Python version information
	 */
	TResult<FString> GetPythonInfo();

	/**
	 * Check whether Code contains a pattern that would crash or hang the editor (input(), modal
	 * dialogs, an infinite `while True:`). Comments and string-literal contents are ignored, so a
	 * mention of the pattern in a comment or docstring does not count. Exposed static so the guard
	 * can be unit-tested without actually executing Python.
	 *
	 * @param Code       Python source to inspect
	 * @param OutPattern  Set to the matched pattern name when true is returned
	 * @param OutReason   Set to a human-readable reason when true is returned
	 * @return True if an unsafe pattern was found
	 */
	static bool ContainsUnsafePattern(const FString& Code, FString& OutPattern, FString& OutReason);

	/**
	 * The command to hand the Python plugin so that Code always runs as code. In ExecuteFile mode the
	 * plugin takes a command whose first ".py" is followed by whitespace or the end (or, when it starts
	 * with a quote, by a closing quote) as a file path to run, so code that merely mentions a .py file
	 * in a comment or string failed "Could not load Python file". Code that mentions ".py" is passed
	 * base64-encoded (no '.' in its alphabet) to a one-line exec(compile(...)) instead, which runs it in
	 * the same globals, under the same "<string>" name and with the same line numbers; a traceback
	 * gains one outer frame. Any other code is returned unchanged.
	 *
	 * This also means a bare script path passed as code (e.g. "C:/scripts/build.py"), which the plugin
	 * used to run as a file, is now compiled as code and fails. That is deliberate: execute_python_code
	 * takes source, and its "code" parameter description tells callers to run a file with
	 * runpy.run_path(...) or exec(open(...).read()).
	 *
	 * @param Code Python source to run
	 * @return The command text for FPythonCommandEx::Command
	 */
	static FString MakeCodeCommand(const FString& Code);

	/** As MakeCodeCommand, always base64-encoded and in a new namespace rather than the shared globals. */
	static FString MakeFreshNamespaceCommand(const FString& Code);

	/** Expression that evaluates to whether Code needs a top-level await run (Content/Python/vibeue_await.py). */
	static FString MakeNeedsAwaitCommand(const FString& Code);

	/** Statement that starts Code as an await run, in the shared globals unless bFreshNamespace. */
	static FString MakeAwaitBeginCommand(int64 RunId, const FString& Code, bool bFreshNamespace);

	/** Expression that calls Function (step, cancel, discard) on an await run. */
	static FString MakeAwaitRunCommand(const TCHAR* Function, int64 RunId);

private:
	/**
	 * Convert FPythonCommandEx result to our result structure
	 *
	 * @param CommandEx UE Python command result
	 * @param ExecutionTimeMs Execution time in milliseconds
	 * @return Converted execution result
	 */
	FPythonExecutionResult ConvertExecutionResult(
		const FPythonCommandEx& CommandEx,
		float ExecutionTimeMs
	);

	/**
	 * Validate code for potentially dangerous operations
	 *
	 * Checks for subprocess, os.system, file writes, etc.
	 *
	 * @param Code Python code to validate
	 * @return Error if dangerous patterns detected, success otherwise
	 */
	TResult<void> ValidateCode(const FString& Code);

	/**
	 * Parse Python exception traceback into structured format
	 *
	 * @param Traceback Raw Python traceback string
	 * @return Formatted error message
	 */
	FString ParsePythonException(const FString& Traceback);

	/** Track if we've validated Python availability */
	bool bPythonValidated = false;
};

} // namespace VibeUE
