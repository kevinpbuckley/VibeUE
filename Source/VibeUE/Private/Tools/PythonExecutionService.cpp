// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Tools/PythonExecutionService.h"
#include "Core/ErrorCodes.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformMisc.h"
#include "Internationalization/Regex.h"

// For SEH exception handling on Windows
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <excpt.h>
#include "Windows/HideWindowsPlatformTypes.h"

// Helper struct for SEH-safe Python execution result
struct FSEHExecutionResult
{
	bool bSuccess = false;
	bool bCrashed = false;
	DWORD ExceptionCode = 0;
};

// Separate function for SEH - cannot have C++ objects that need unwinding
static FSEHExecutionResult ExecutePythonWithSEH(IPythonScriptPlugin* PythonPlugin, FPythonCommandEx* Command)
{
	FSEHExecutionResult Result;
	__try
	{
		Result.bSuccess = PythonPlugin->ExecPythonCommandEx(*Command);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		Result.bCrashed = true;
		Result.ExceptionCode = GetExceptionCode();
	}
	return Result;
}
#endif

// Dangerous patterns that can crash the editor
static bool ContainsDangerousPattern(const FString& Code, FString& OutPattern, FString& OutReason)
{
	// EdGraphPinType construction crashes - use BlueprintEditorLibrary.get_basic_type_by_name() instead
	if (Code.Contains(TEXT("EdGraphPinType(")) && Code.Contains(TEXT("pin_category")))
	{
		OutPattern = TEXT("EdGraphPinType(pin_category=...)");
		OutReason = TEXT("EdGraphPinType cannot be constructed with arguments from Python. Use unreal.BlueprintEditorLibrary.get_basic_type_by_name('float') instead.");
		return true;
	}
	
	// Direct CDO modification causes crashes
	if (Code.Contains(TEXT("get_default_object")) && (Code.Contains(TEXT("set_editor_property")) || Code.Contains(TEXT("="))))
	{
		OutPattern = TEXT("get_default_object() modification");
		OutReason = TEXT("Modifying Class Default Objects (CDOs) from Python causes crashes. Modify instances instead.");
		return true;
	}
	
	// input() blocks the editor indefinitely
	// Use regex to match standalone input( calls, not method names like add_function_input(
	// Pattern matches: input(, =input(, (input(, but NOT _input( or identifier_input(
	static FRegexPattern InputPattern(TEXT("(?:^|[^_a-zA-Z0-9])input\\s*\\("));
	FRegexMatcher InputMatcher(InputPattern, Code);
	if (InputMatcher.FindNext() && !Code.Contains(TEXT("#")) && !Code.Contains(TEXT("Enhanced")))
	{
		OutPattern = TEXT("input()");
		OutReason = TEXT("input() blocks the editor. Use a different approach for user interaction.");
		return true;
	}
	
	// Modal dialogs freeze the editor
	if (Code.Contains(TEXT("EditorDialog")) || Code.Contains(TEXT("show_modal")))
	{
		OutPattern = TEXT("Modal dialogs");
		OutReason = TEXT("Modal dialogs freeze the editor from Python. Use non-blocking alternatives.");
		return true;
	}
	
	// Infinite loops
	if (Code.Contains(TEXT("while True:")) && !Code.Contains(TEXT("break")))
	{
		OutPattern = TEXT("while True without break");
		OutReason = TEXT("Infinite loops freeze the editor. Ensure your loop has a break condition.");
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
	if (ContainsDangerousPattern(Code, BlockedPattern, BlockedReason))
	{
		return TResult<FPythonExecutionResult>::Error(
			ErrorCodes::PYTHON_UNSAFE_CODE,
			FString::Printf(TEXT("Blocked unsafe Python code: %s. %s"), *BlockedPattern, *BlockedReason)
		);
	}

	// Setup command
	FPythonCommandEx Command;
	Command.Command = Code;
	Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	Command.FileExecutionScope = ExecutionScope;
	Command.Flags = EPythonCommandFlags::None;

	// Execute with timing and timeout handling
	double StartTime = FPlatformTime::Seconds();
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
	bSuccess = SEHResult.bSuccess;
	if (SEHResult.bCrashed)
	{
		bCrashed = true;
		CrashMessage = FString::Printf(TEXT("Python execution caused a crash (exception code: 0x%08X). The Python code may have accessed invalid memory."), SEHResult.ExceptionCode);
		UE_LOG(LogTemp, Error, TEXT("%s"), *CrashMessage);
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
#endif

	if (bCrashed)
	{
		return TResult<FPythonExecutionResult>::Error(
			ErrorCodes::PYTHON_RUNTIME_ERROR,
			CrashMessage
		);
	}

	double ExecutionTimeMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

	// Check if execution took too long (post-execution check)
	if (TimeoutMs > 0 && ExecutionTimeMs > TimeoutMs)
	{
		return TResult<FPythonExecutionResult>::Error(
			ErrorCodes::PYTHON_EXECUTION_TIMEOUT,
			FString::Printf(TEXT("Python execution exceeded %dms timeout (took %.2fms)"),
				TimeoutMs, ExecutionTimeMs)
		);
	}

	// Convert result
	FPythonExecutionResult Result = ConvertExecutionResult(Command, ExecutionTimeMs);

	// Check for errors in result
	if (!bSuccess || !Result.bSuccess)
	{
		return TResult<FPythonExecutionResult>::Error(
			ErrorCodes::PYTHON_RUNTIME_ERROR,
			Result.ErrorMessage.IsEmpty() ? TEXT("Python execution failed") : Result.ErrorMessage
		);
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
		else if (LogEntry.Type == EPythonLogOutputType::Error || LogEntry.Type == EPythonLogOutputType::Warning)
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
	// Simple traceback parsing - extract the most relevant error info
	TArray<FString> Lines;
	Traceback.ParseIntoArrayLines(Lines);

	FString ParsedError;

	// Look for the actual error line (usually the last non-empty line)
	for (int32 i = Lines.Num() - 1; i >= 0; --i)
	{
		FString Line = Lines[i].TrimStartAndEnd();
		if (!Line.IsEmpty())
		{
			ParsedError = Line;
			break;
		}
	}

	// If we couldn't parse it, return the full traceback
	if (ParsedError.IsEmpty())
	{
		return Traceback;
	}

	// Add context if we found an error
	if (Lines.Num() > 2)
	{
		FString LastLine = Lines.Last().TrimStartAndEnd();
		if (!LastLine.IsEmpty() && LastLine != ParsedError)
		{
			ParsedError = FString::Printf(TEXT("%s\n%s"), *ParsedError, *LastLine);
		}
	}

	return ParsedError;
}

} // namespace VibeUE
