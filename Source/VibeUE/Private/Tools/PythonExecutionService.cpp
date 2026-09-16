// Copyright Buckley Builds LLC 2026 All Rights Reserved.

#include "Tools/PythonExecutionService.h"
#include "Core/ErrorCodes.h"
#include "Utils/VibeUEPythonResultLog.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Regex.h"

// For SEH exception handling on Windows
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <excpt.h>
#include "Windows/HideWindowsPlatformTypes.h"

// UE5 assert exception code (check() failures raise this via RaiseException)
static constexpr DWORD UE_ASSERT_EXCEPTION_CODE = 0x4000;

// Tracks whether a hard crash (SEH-caught access violation or assertion) has happened during
// Python execution this editor session. An access violation cannot be safely recovered in-process
// — the CPython runtime state is undefined afterwards — so once this is set we tell the caller to
// restart the editor instead of letting every later call fail with the same cryptic error.
// Cleared whenever a command completes without a hard crash (the interpreter is proven alive).
static bool GbPythonInterpreterCrashed = false;

// Mirror UE5's FAssertInfo struct layout (defined in WindowsPlatformCrashContext.cpp)
struct FVibeUEAssertInfo
{
	const TCHAR* ErrorMessage;
	void* ProgramCounter;
};

// Helper struct for SEH-safe Python execution result
// NOTE: Must be POD-like (no C++ destructors) since it's used in __try functions
struct FSEHExecutionResult
{
	bool bSuccess = false;
	bool bCrashed = false;
	DWORD ExceptionCode = 0;
	TCHAR AssertMessage[512]; // Populated for UE assert exceptions (0x4000)
	bool bHasAssertMessage = false;
};

// SEH exception filter that extracts assert info before handling
static LONG WINAPI PythonSEHFilter(LPEXCEPTION_POINTERS ExInfo, FSEHExecutionResult* OutResult)
{
	OutResult->bCrashed = true;
	OutResult->ExceptionCode = ExInfo->ExceptionRecord->ExceptionCode;

	// For UE assert exceptions (check() failures), extract the error message
	if (ExInfo->ExceptionRecord->ExceptionCode == UE_ASSERT_EXCEPTION_CODE &&
		ExInfo->ExceptionRecord->NumberParameters >= 1 &&
		ExInfo->ExceptionRecord->ExceptionInformation[0] != 0)
	{
		const FVibeUEAssertInfo* Info = (const FVibeUEAssertInfo*)ExInfo->ExceptionRecord->ExceptionInformation[0];
		if (Info->ErrorMessage)
		{
			// Safe copy into fixed buffer
			int32 i = 0;
			for (; i < 511 && Info->ErrorMessage[i] != 0; i++)
			{
				OutResult->AssertMessage[i] = Info->ErrorMessage[i];
			}
			OutResult->AssertMessage[i] = 0;
			OutResult->bHasAssertMessage = true;
		}
	}

	return EXCEPTION_EXECUTE_HANDLER;
}

// Separate function for SEH - cannot have C++ objects that need unwinding in __try block
static FSEHExecutionResult ExecutePythonWithSEH(IPythonScriptPlugin* PythonPlugin, FPythonCommandEx* Command)
{
	FSEHExecutionResult Result;
	__try
	{
		Result.bSuccess = PythonPlugin->ExecPythonCommandEx(*Command);
	}
	__except(PythonSEHFilter(GetExceptionInformation(), &Result))
	{
		// Result already populated by PythonSEHFilter
	}
	return Result;
}
#endif

// Strip Python comments (text after an unquoted '#' to end of line) so guards below are not
// defeated by a '#' anywhere in the script. Simple scan: track single/double quotes (honouring
// backslash escapes) and reset at each newline; a '#' seen outside a string ends that line.
static FString StripPythonComments(const FString& Code)
{
	FString Out;
	Out.Reserve(Code.Len());
	bool bInSingle = false;
	bool bInDouble = false;
	bool bInComment = false;
	for (int32 i = 0; i < Code.Len(); ++i)
	{
		const TCHAR C = Code[i];
		if (C == TEXT('\n'))
		{
			bInSingle = false;
			bInDouble = false;
			bInComment = false;
			Out.AppendChar(C);
			continue;
		}
		if (bInComment)
		{
			continue;
		}
		if (bInSingle || bInDouble)
		{
			// Skip an escaped character so a \' or \" does not flip the quote state.
			if (C == TEXT('\\') && (i + 1) < Code.Len() && Code[i + 1] != TEXT('\n'))
			{
				Out.AppendChar(C);
				Out.AppendChar(Code[i + 1]);
				++i;
				continue;
			}
			if (bInSingle && C == TEXT('\'')) { bInSingle = false; }
			else if (bInDouble && C == TEXT('"')) { bInDouble = false; }
		}
		else
		{
			if (C == TEXT('#')) { bInComment = true; continue; }
			if (C == TEXT('\'')) { bInSingle = true; }
			else if (C == TEXT('"')) { bInDouble = true; }
		}
		Out.AppendChar(C);
	}
	return Out;
}

// Blank the CONTENTS of every Python string literal ('...', "...", '''...''', """...""") while
// keeping the surrounding quote characters and the overall line/column shape, so a keyword that
// appears only inside a string or a docstring (e.g. "show_modal" in a doc line, or the word "break"
// in a message) no longer trips the guards below. Comments should be stripped first (this helper
// does not treat '#'). Escapes are honoured so a \' or \" inside a string does not end it early.
static FString StripPythonStringLiterals(const FString& Code)
{
	FString Out;
	Out.Reserve(Code.Len());
	const int32 Len = Code.Len();
	int32 i = 0;
	while (i < Len)
	{
		const TCHAR C = Code[i];

		// Triple-quoted string ('''...''' or """...""") — may span lines.
		if ((C == TEXT('\'') || C == TEXT('"')) && (i + 2) < Len && Code[i + 1] == C && Code[i + 2] == C)
		{
			const TCHAR Q = C;
			Out.AppendChar(Q); Out.AppendChar(Q); Out.AppendChar(Q);
			i += 3;
			while (i < Len)
			{
				if (Code[i] == TEXT('\\') && (i + 1) < Len)
				{
					// Preserve real newlines to keep line counts stable; blank the rest.
					Out.AppendChar(Code[i] == TEXT('\n') ? TEXT('\n') : TEXT(' '));
					Out.AppendChar(Code[i + 1] == TEXT('\n') ? TEXT('\n') : TEXT(' '));
					i += 2;
					continue;
				}
				if ((i + 2) < Len && Code[i] == Q && Code[i + 1] == Q && Code[i + 2] == Q)
				{
					Out.AppendChar(Q); Out.AppendChar(Q); Out.AppendChar(Q);
					i += 3;
					break;
				}
				Out.AppendChar(Code[i] == TEXT('\n') ? TEXT('\n') : TEXT(' '));
				++i;
			}
			continue;
		}

		// Single- or double-quoted string on one line.
		if (C == TEXT('\'') || C == TEXT('"'))
		{
			const TCHAR Q = C;
			Out.AppendChar(Q);
			++i;
			while (i < Len && Code[i] != TEXT('\n'))
			{
				if (Code[i] == TEXT('\\') && (i + 1) < Len && Code[i + 1] != TEXT('\n'))
				{
					Out.AppendChar(TEXT(' '));
					Out.AppendChar(TEXT(' '));
					i += 2;
					continue;
				}
				if (Code[i] == Q)
				{
					Out.AppendChar(Q);
					++i;
					break;
				}
				Out.AppendChar(TEXT(' '));
				++i;
			}
			continue;
		}

		Out.AppendChar(C);
		++i;
	}
	return Out;
}

// Dangerous patterns that can crash the editor. Free helper kept for internal use; the public,
// testable entry point is FPythonExecutionService::ContainsUnsafePattern (declared in the header).
static bool ContainsDangerousPattern(const FString& Code, FString& OutPattern, FString& OutReason)
{
	// EdGraphPinType construction crashes - use BlueprintEditorLibrary.get_basic_type_by_name() instead
	if (Code.Contains(TEXT("EdGraphPinType(")) && Code.Contains(TEXT("pin_category")))
	{
		OutPattern = TEXT("EdGraphPinType(pin_category=...)");
		OutReason = TEXT("EdGraphPinType cannot be constructed with arguments from Python. Use unreal.BlueprintEditorLibrary.get_basic_type_by_name('float') instead.");
		return true;
	}
	
	// (CDO set_editor_property guard removed: the same-line regex guarded nothing — a chained write
	// split across two statements passed it — and it blocked the standard, safe way to set Blueprint
	// defaults from Python, e.g. flipping WaterBodyComponent.affects_landscape before spawning water.)

	// input() blocks the editor indefinitely.
	// Strip comments first so a '#' anywhere in the script no longer defeats this guard.
	// The regex matches a bare input( preceded by a non-identifier char (input(, =input(, (input(),
	// so it already ignores method names ending in _input( such as Enhanced Input's inject_input(.
	const FString CodeNoComments = StripPythonComments(Code);
	static FRegexPattern InputPattern(TEXT("(?:^|[^_a-zA-Z0-9])input\\s*\\("));
	FRegexMatcher InputMatcher(InputPattern, CodeNoComments);
	if (InputMatcher.FindNext())
	{
		OutPattern = TEXT("input()");
		OutReason = TEXT("input() blocks the editor. Use a different approach for user interaction.");
		return true;
	}

	// The remaining guards used to run against the raw source, so a mention of the pattern in a
	// comment or a docstring/string literal produced a false refusal. Run them against code that has
	// had both comments AND string-literal contents blanked, so only real code trips them.
	const FString CodeSanitized = StripPythonStringLiterals(CodeNoComments);

	// Modal dialogs freeze the editor
	if (CodeSanitized.Contains(TEXT("EditorDialog")) || CodeSanitized.Contains(TEXT("show_modal")))
	{
		OutPattern = TEXT("Modal dialogs");
		OutReason = TEXT("Modal dialogs freeze the editor from Python. Use non-blocking alternatives.");
		return true;
	}

	// Infinite loops: a `while True:` with no way out. break is the obvious exit, but return, raise
	// and sys.exit() also leave the loop, so a `while True:` guarded by any of those is not infinite.
	if (CodeSanitized.Contains(TEXT("while True:")) &&
		!CodeSanitized.Contains(TEXT("break")) &&
		!CodeSanitized.Contains(TEXT("return")) &&
		!CodeSanitized.Contains(TEXT("raise")) &&
		!CodeSanitized.Contains(TEXT("sys.exit(")))
	{
		OutPattern = TEXT("while True without break");
		OutReason = TEXT("Infinite loops freeze the editor. Ensure your loop has a break/return/raise/sys.exit().");
		return true;
	}

	return false;
}

namespace VibeUE
{

FPythonExecutionService::FPythonExecutionService(TSharedPtr<FServiceContext> Context)
	: FServiceBase(Context)
{
}

bool FPythonExecutionService::ContainsUnsafePattern(const FString& Code, FString& OutPattern, FString& OutReason)
{
	return ContainsDangerousPattern(Code, OutPattern, OutReason);
}

TResult<FPythonExecutionResult> FPythonExecutionService::ExecuteCode(
	const FString& Code,
	EPythonFileExecutionScope ExecutionScope,
	int32 TimeoutMs)
{
	// Validate Python is available
	auto AvailableResult = IsPythonAvailable();
	if (AvailableResult.IsError())
	{
		return TResult<FPythonExecutionResult>::Error(
			AvailableResult.GetErrorCode(),
			AvailableResult.GetErrorMessage()
		);
	}

	// Validate code is not empty
	if (Code.IsEmpty())
	{
		return TResult<FPythonExecutionResult>::Error(
			ErrorCodes::PARAM_EMPTY,
			TEXT("Python code cannot be empty")
		);
	}

	// Block dangerous patterns that can cause crashes
	FString BlockedPattern;
	FString BlockedReason;
	if (ContainsUnsafePattern(Code, BlockedPattern, BlockedReason))
	{
		return TResult<FPythonExecutionResult>::Error(
			ErrorCodes::PYTHON_UNSAFE_CODE,
			FString::Printf(TEXT("Blocked unsafe Python code: %s. %s"), *BlockedPattern, *BlockedReason)
		);
	}

	// Assign this run its per-process id and wall-clock start now, so the outcome can be persisted
	// (B2) even when the client abandons the call after its timeout while the script keeps running.
	const int64 RunId = FVibeUEPythonResultLog::NextRunId();
	const FDateTime StartedUtc = FDateTime::UtcNow();

	// Setup command
	FPythonCommandEx Command;
	Command.Command = Code;
	Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	Command.FileExecutionScope = ExecutionScope;
	Command.Flags = EPythonCommandFlags::None;

	// Execute with timing and timeout handling
	double StartTime = FPlatformTime::Seconds();
	double ExecutionTimeMs = 0.0; // measured for the main execution (excludes the post-crash probe)
	bool bSuccess = false;
	bool bCrashed = false;
	FString CrashMessage;

	// Get Python plugin first (outside of SEH block)
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		return TResult<FPythonExecutionResult>::Error(
			ErrorCodes::PYTHON_NOT_AVAILABLE,
			TEXT("Python plugin is not initialized")
		);
	}

#if PLATFORM_WINDOWS
	// Use SEH helper to catch access violations that C++ try/catch won't handle
	FSEHExecutionResult SEHResult = ExecutePythonWithSEH(PythonPlugin, &Command);
	ExecutionTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0; // before the probe below
	bSuccess = SEHResult.bSuccess;
	if (SEHResult.bCrashed)
	{
		bCrashed = true;
		if (SEHResult.ExceptionCode == UE_ASSERT_EXCEPTION_CODE && SEHResult.bHasAssertMessage)
		{
			CrashMessage = FString::Printf(TEXT("Python execution caused a UE assertion failure: %s"), SEHResult.AssertMessage);
		}
		else
		{
			CrashMessage = FString::Printf(TEXT("Python execution caused a crash (exception code: 0x%08X). The Python code may have accessed invalid memory."), SEHResult.ExceptionCode);
		}

		// A caught fault often originates in engine C++ called from Python, not in
		// CPython itself — in that case the interpreter (and GIL) are still healthy and
		// the session can keep working. Probe with a no-op through the same SEH wrapper:
		// clean probe → recovered; probe faults too → genuinely wedged (issue #399).
		FPythonCommandEx ProbeCommand;
		ProbeCommand.Command = TEXT("pass");
		ProbeCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteStatement;
		ProbeCommand.Flags = EPythonCommandFlags::None;
		const FSEHExecutionResult ProbeResult = ExecutePythonWithSEH(PythonPlugin, &ProbeCommand);
		if (!ProbeResult.bCrashed && ProbeResult.bSuccess)
		{
			GbPythonInterpreterCrashed = false;
			CrashMessage += TEXT(" NOTE: the interpreter responded to a post-crash probe — the session has been recovered and further Python calls will execute normally. Dirty assets touched by the crashed script may still be corrupt; verify before saving.");
		}
		else if (GbPythonInterpreterCrashed)
		{
			CrashMessage += TEXT(" NOTE: the post-crash probe failed and the interpreter has crashed repeatedly this session — it is unrecoverable in-process; restart the editor (BuildAndLaunch) to restore Python execution.");
			GbPythonInterpreterCrashed = true;
		}
		else
		{
			CrashMessage += TEXT(" NOTE: the post-crash probe failed — the interpreter is unstable; if further commands keep failing identically, restart the editor (BuildAndLaunch).");
			GbPythonInterpreterCrashed = true;
		}

		UE_LOG(LogTemp, Error, TEXT("%s"), *CrashMessage);
	}
	else
	{
		// Completed without a hard crash (a normal Python exception is fine) — interpreter is alive.
		GbPythonInterpreterCrashed = false;
	}
#else
	// Non-Windows platforms - use regular try/catch
	try
	{
		bSuccess = PythonPlugin->ExecPythonCommandEx(Command);
	}
	catch (...)
	{
		bCrashed = true;
		CrashMessage = TEXT("Python execution threw an unhandled exception");
	}
	ExecutionTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
#endif

	const FDateTime FinishedUtc = FDateTime::UtcNow();

	// Build the outcome record once so EVERY completed run is persisted (B2) — including the error
	// paths, because the code has already run in the editor even when we return an error to a client
	// that has since timed out. OutErrorCode empty => success.
	FPythonExecutionResult Result;
	FString OutErrorCode;
	FString OutErrorMessage;

	if (bCrashed)
	{
		Result.bSuccess = false;
		Result.ErrorMessage = CrashMessage;
		Result.ExecutionTimeMs = ExecutionTimeMs;
		OutErrorCode = ErrorCodes::PYTHON_RUNTIME_ERROR;
		OutErrorMessage = CrashMessage;
	}
	else
	{
		Result = ConvertExecutionResult(Command, ExecutionTimeMs);

		if (!bSuccess || !Result.bSuccess)
		{
			OutErrorCode = ErrorCodes::PYTHON_RUNTIME_ERROR;
			OutErrorMessage = Result.ErrorMessage.IsEmpty() ? TEXT("Python execution failed") : Result.ErrorMessage;
		}
		else if (TimeoutMs > 0 && ExecutionTimeMs > TimeoutMs)
		{
			// The script COMPLETED SUCCESSFULLY but ran past the client timeout. This C++ call is
			// synchronous, so the result is always in hand here — returning PYTHON_EXECUTION_TIMEOUT
			// (as this path used to) threw away a good result and left a client that is still waiting
			// (or that retries with a longer budget) with nothing. Return the successful payload
			// instead, flagged timed_out so the caller knows it overran; run_id + the persisted signal
			// path travel with the result so vibeue.last_python_result() can recover it too.
			Result.bTimedOut = true;
			UE_LOG(LogTemp, Warning,
				TEXT("Python run #%lld completed in %.2fms, past the %dms timeout — returning the successful result flagged timed_out."),
				(long long)RunId, ExecutionTimeMs, TimeoutMs);
		}
	}

	Result.RunId = RunId;

	// Persist off the return path. A failure here logs Warning inside Record and never affects the run.
	FVibeUEPythonResultLog::Record(Result, StartedUtc, FinishedUtc);

	if (!OutErrorCode.IsEmpty())
	{
		// The run has already executed and its outcome is persisted, so even on the error path give the
		// caller a handle to it: a client that timed out can recover the full result with
		// vibeue.last_python_result() using the run id and signal file named here.
		const FString SignalPath = FVibeUEPythonResultLog::GetLastResultPathForPid(FPlatformProcess::GetCurrentProcessId());
		OutErrorMessage += FString::Printf(
			TEXT(" (run_id=%lld; result persisted to %s — recover it with vibeue.last_python_result())"),
			(long long)RunId, *SignalPath);
		return TResult<FPythonExecutionResult>::Error(OutErrorCode, OutErrorMessage);
	}

	return TResult<FPythonExecutionResult>::Success(Result);
}

TResult<FPythonExecutionResult> FPythonExecutionService::EvaluateExpression(const FString& Expression)
{
	// Validate Python is available
	auto AvailableResult = IsPythonAvailable();
	if (AvailableResult.IsError())
	{
		return TResult<FPythonExecutionResult>::Error(
			AvailableResult.GetErrorCode(),
			AvailableResult.GetErrorMessage()
		);
	}

	// Validate expression is not empty
	if (Expression.IsEmpty())
	{
		return TResult<FPythonExecutionResult>::Error(
			ErrorCodes::PYTHON_INVALID_EXPRESSION,
			TEXT("Python expression cannot be empty")
		);
	}

	// Setup command for evaluation
	FPythonCommandEx Command;
	Command.Command = Expression;
	Command.ExecutionMode = EPythonCommandExecutionMode::EvaluateStatement;
	Command.FileExecutionScope = EPythonFileExecutionScope::Private;
	Command.Flags = EPythonCommandFlags::None;

	// Execute with timing
	double StartTime = FPlatformTime::Seconds();
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		return TResult<FPythonExecutionResult>::Error(
			ErrorCodes::PYTHON_NOT_AVAILABLE,
			TEXT("Python plugin is not initialized")
		);
	}
	bool bSuccess = PythonPlugin->ExecPythonCommandEx(Command);
	double ExecutionTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

	// Convert result
	FPythonExecutionResult Result = ConvertExecutionResult(Command, ExecutionTimeMs);

	// Check for errors
	if (!bSuccess || !Result.bSuccess)
	{
		return TResult<FPythonExecutionResult>::Error(
			ErrorCodes::PYTHON_RUNTIME_ERROR,
			Result.ErrorMessage.IsEmpty() ? TEXT("Python expression evaluation failed") : Result.ErrorMessage
		);
	}

	return TResult<FPythonExecutionResult>::Success(Result);
}

TResult<FPythonExecutionResult> FPythonExecutionService::ExecuteCodeSafe(
	const FString& Code,
	bool bValidateBeforeExecution)
{
	// Optionally validate code
	if (bValidateBeforeExecution)
	{
		auto ValidationResult = ValidateCode(Code);
		if (ValidationResult.IsError())
		{
			FPythonExecutionResult ErrorResult;
			ErrorResult.bSuccess = false;
			ErrorResult.ErrorMessage = ValidationResult.GetErrorMessage();

			return TResult<FPythonExecutionResult>::Error(
				ValidationResult.GetErrorCode(),
				ValidationResult.GetErrorMessage()
			);
		}
	}

	// Execute code normally
	return ExecuteCode(Code);
}

TResult<bool> FPythonExecutionService::IsPythonAvailable()
{
	// Get Python plugin
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();

	if (!PythonPlugin)
	{
		return TResult<bool>::Error(
			ErrorCodes::PYTHON_NOT_AVAILABLE,
			TEXT("PythonScriptPlugin is not loaded. Enable it in Project Settings -> Plugins -> Scripting -> Python.")
		);
	}

	// Check if Python is initialized
	if (!PythonPlugin->IsPythonAvailable())
	{
		return TResult<bool>::Error(
			ErrorCodes::PYTHON_NOT_AVAILABLE,
			TEXT("Python is not initialized. Check that Python is enabled in project settings.")
		);
	}

	bPythonValidated = true;
	return TResult<bool>::Success(true);
}

TResult<FString> FPythonExecutionService::GetPythonInfo()
{
	// Check Python availability
	auto AvailableResult = IsPythonAvailable();
	if (AvailableResult.IsError())
	{
		return TResult<FString>::Error(
			AvailableResult.GetErrorCode(),
			AvailableResult.GetErrorMessage()
		);
	}

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	FString InterpreterPath = PythonPlugin->GetInterpreterExecutablePath();

	// Get Python version by executing sys.version
	FPythonCommandEx Command;
	Command.Command = TEXT("import sys; print(sys.version)");
	Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	Command.FileExecutionScope = EPythonFileExecutionScope::Private;

	bool bSuccess = PythonPlugin->ExecPythonCommandEx(Command);

	if (bSuccess && Command.LogOutput.Num() > 0)
	{
		FString Version = Command.LogOutput[0].Output.TrimStartAndEnd();
		FString Info = FString::Printf(
			TEXT("Python Version: %s\nInterpreter: %s"),
			*Version,
			*InterpreterPath
		);
		return TResult<FString>::Success(Info);
	}

	return TResult<FString>::Success(
		FString::Printf(TEXT("Interpreter: %s"), *InterpreterPath)
	);
}

FPythonExecutionResult FPythonExecutionService::ConvertExecutionResult(
	const FPythonCommandEx& CommandEx,
	float ExecutionTimeMs)
{
	FPythonExecutionResult Result;
	Result.ExecutionTimeMs = ExecutionTimeMs;

	// Check for errors in log output
	bool bHasError = false;
	for (const FPythonLogOutputEntry& LogEntry : CommandEx.LogOutput)
	{
		FString LogOutput = LogEntry.Output.TrimStartAndEnd();
		if (LogOutput.IsEmpty())
		{
			continue;
		}

		Result.LogMessages.Add(LogOutput);

		if (LogEntry.Type == EPythonLogOutputType::Info)
		{
			if (!Result.Output.IsEmpty())
			{
				Result.Output += TEXT("\n");
			}
			Result.Output += LogOutput;
		}
		else if (LogEntry.Type == EPythonLogOutputType::Warning)
		{
			// Warnings (e.g. DeprecationWarning) must not fail the execution —
			// the code ran. Surface them in the output so callers still see them.
			if (!Result.Output.IsEmpty())
			{
				Result.Output += TEXT("\n");
			}
			Result.Output += FString::Printf(TEXT("[warning] %s"), *LogOutput);
		}
		else if (LogEntry.Type == EPythonLogOutputType::Error)
		{
			bHasError = true;
			if (!Result.ErrorMessage.IsEmpty())
			{
				Result.ErrorMessage += TEXT("\n");
			}
			Result.ErrorMessage += LogOutput;
		}
	}

	// Check command result for errors or return value
	if (!CommandEx.CommandResult.IsEmpty())
	{
		// Check if this is an error (contains "Error" or "Traceback")
		if (CommandEx.CommandResult.Contains(TEXT("Error")) ||
		    CommandEx.CommandResult.Contains(TEXT("Traceback")))
		{
			bHasError = true;
			Result.ErrorMessage = ParsePythonException(CommandEx.CommandResult);
		}
		else
		{
			// This is a return value (from EvaluateStatement)
			Result.Result = CommandEx.CommandResult;
		}
	}

	Result.bSuccess = !bHasError;
	return Result;
}

TResult<void> FPythonExecutionService::ValidateCode(const FString& Code)
{
	// Check for potentially dangerous patterns
	TArray<FString> DangerousPatterns = {
		TEXT("import subprocess"),
		TEXT("import os"),
		TEXT("os.system"),
		TEXT("open("),
		TEXT("__import__"),
		TEXT("eval("),
		TEXT("exec(")
	};

	for (const FString& Pattern : DangerousPatterns)
	{
		if (Code.Contains(Pattern))
		{
			LogWarning(FString::Printf(
				TEXT("Potentially dangerous pattern detected in Python code: %s"),
				*Pattern
			));

			// Could return error here if strict validation is desired
			// return TResult<void>::Error(
			//     ErrorCodes::PYTHON_UNSAFE_CODE,
			//     FString::Printf(TEXT("Unsafe Python pattern detected: %s"), *Pattern)
			// );
		}
	}

	return TResult<void>::Success();
}

FString FPythonExecutionService::ParsePythonException(const FString& Traceback)
{
	// Keep the traceback intact: the line number and source context are what the
	// caller needs to fix the code. Reducing it to the last non-empty line used to
	// produce useless messages like "^" (the caret marker of a SyntaxError).
	TArray<FString> Lines;
	Traceback.ParseIntoArrayLines(Lines);

	// Drop leading/trailing blank lines, cap very deep tracebacks to the tail
	// (the exception line and innermost frames are at the end).
	int32 FirstLine = 0;
	while (FirstLine < Lines.Num() && Lines[FirstLine].TrimStartAndEnd().IsEmpty())
	{
		++FirstLine;
	}
	int32 LastLine = Lines.Num() - 1;
	while (LastLine >= FirstLine && Lines[LastLine].TrimStartAndEnd().IsEmpty())
	{
		--LastLine;
	}

	if (FirstLine > LastLine)
	{
		return Traceback;
	}

	constexpr int32 MaxLines = 40;
	FString ParsedError;
	if (LastLine - FirstLine + 1 > MaxLines)
	{
		ParsedError = TEXT("[traceback truncated]");
		FirstLine = LastLine - MaxLines + 1;
	}

	for (int32 i = FirstLine; i <= LastLine; ++i)
	{
		if (!ParsedError.IsEmpty())
		{
			ParsedError += TEXT("\n");
		}
		ParsedError += Lines[i].TrimEnd();
	}

	return ParsedError;
}

} // namespace VibeUE
