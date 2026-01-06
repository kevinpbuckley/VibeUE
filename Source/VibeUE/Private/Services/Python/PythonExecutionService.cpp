// Copyright Buckley Builds LLC 2025 All Rights Reserved.

#include "Services/Python/PythonExecutionService.h"
#include "Core/ErrorCodes.h"
#include "Misc/DateTime.h"

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

	// Setup command
	FPythonCommandEx Command;
	Command.Command = Code;
	Command.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	Command.FileExecutionScope = ExecutionScope;
	Command.Flags = EPythonCommandFlags::None;

	// Execute with timing and timeout handling
	double StartTime = FPlatformTime::Seconds();
	bool bSuccess = false;
	bool bTimedOut = false;

	// Use a simple timeout approach - wrap in try/catch for any exceptions
	try
	{
		// For now, we execute synchronously but with error handling
		// TODO: Implement proper async execution with thread interruption
		bSuccess = IPythonScriptPlugin::Get()->ExecPythonCommandEx(Command);
	}
	catch (...)
	{
		return TResult<FPythonExecutionResult>::Error(
			ErrorCodes::PYTHON_RUNTIME_ERROR,
			TEXT("Python execution threw an exception")
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
	bool bSuccess = IPythonScriptPlugin::Get()->ExecPythonCommandEx(Command);
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
